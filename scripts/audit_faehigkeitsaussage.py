#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Tor 69: zwei Aussagen ueber dieselbe Faehigkeit, im selben Plugin (MF-1196).

── Warum es dieses Tor gibt ─────────────────────────────────────────────

`uft_format_plugin_t` fuehrt die Faehigkeiten eines Formats ZWEIMAL:

    uint32_t                     capabilities;   /* UFT_FORMAT_CAP_*      */
    const uft_plugin_feature_t*  features;       /* { "Flux", UFT_FEATURE_*,
                                                     "Grund" }            */

Gemessen tragen **89** Plugins Bitflaggen und **88** eine Merkmalstafel.
Dieselbe Aussage, zwei Rechnungen - das ist MF-1177 in der Formatschicht
("eine Groesse, eine Rechnung; wer sie zweimal rechnet, hat sie nicht
gemessen").

Der schaerfste gemessene Fall: `src/formats/atari/uft_pro_plugin.c:312`
setzt `UFT_FORMAT_CAP_WEAK_BITS`, waehrend die Merkmalstafel derselben
Datei sagt:

    { "Weak Bits", UFT_FEATURE_UNSUPPORTED,
      "PRO speichert keine schwachen Bits. Sein Kopierschutz sind
       Phantomsektoren; die werden gelesen und als Duplikate auf
       derselben Spur ausgegeben (MF-1054)." }

Die Tafel hat eine Messung, die Flagge hat nichts. Klasse MF-880/883:
eine Zusage, die eine andere Stelle im selben Plugin widerlegt.

── Was das Tor NICHT behauptet ──────────────────────────────────────────

Vier der fuenf Grundlinien-Widersprueche sind **strukturell, kein
Versaeumnis**: `g71`, `kfx`, `logical` und `mfi` setzen `CAP_READ`,
waehrend ihre Tafel `PARTIAL` sagt. Eine Bitflagge KANN "teilweise" nicht
ausdruecken. Sie stehen trotzdem in der Grundlinie, weil der Widerspruch
echt ist - nur ist der Weg heraus eine Entwurfsentscheidung (dritter
Zustand, oder Verzicht auf eine der beiden Quellen), keine Korrektur.

── Warum die drei vorhandenen Faehigkeits-Tore es nicht sehen ───────────

`audit_faehigkeitszusage.py` (MF-985) prueft Zusagen ohne Vorbehalt.
`audit_faehigkeitszusage_gedeckt.py` (Tor 67, MF-1045) prueft Absagen
gegen `docs/CAPABILITIES.md`. `write_gate_caps_gate.py` (MF-491) prueft
die Signaturtabelle des Schreib-Tors.

Keines stellt die Frage: sagen die ZWEI Felder DESSELBEN Plugins dasselbe?

── Bekannte Grenzen, hier benannt statt verschwiegen ────────────────────

  * Gemessen wird dateiweit, nicht je Struktur-Initialisierer. Eine Datei
    mit mehreren Plugins vermengt deren Flaggen. Im Baum trifft das
    `dsk_generic` (49 Makro-Ausweitungen); es steht deshalb in EINSEITIG.
  * Die Zuordnung Flaggenname -> Merkmalsname steht in ACHSEN und ist
    gepflegt, nicht abgeleitet. Ein Merkmal unter neuem Namen entgeht dem
    Tor. Das ist die Klasse "Aufzaehlung statt Messung" - hier
    unvermeidbar, weil die Merkmalsnamen freie Zeichenketten sind.
  * Kommentare werden entfernt, bevor gesucht wird. Ohne das zaehlte
    `uft_pro_plugin.c:307` - "MF-880: UFT_FORMAT_CAP_WRITE entfernt",
    also die Dokumentation einer ENTFERNUNG - als gesetzte Flagge und
    erzeugte einen sechsten, erfundenen Widerspruch (Klasse MF-767).

Beide Grundlinien duerfen nur SINKEN (Bauform Tor 57).
"""
from __future__ import annotations

import re
import subprocess
import sys
import tempfile
from pathlib import Path

import c_lex   # MF-1247: in TOKEN denken statt in Zeichen

# Flaggenname -> moegliche Merkmalsnamen in der Tafel.
ACHSEN = {
    "FLUX": ("Flux",),
    "WRITE": ("Write",),
    "READ": ("Read",),
    "TIMING": ("Timing",),
    "WEAK_BITS": ("Weak Bits", "WeakBits", "Weak bits"),
    "MULTI_REV": ("Multi-Rev", "Multi Revolution", "MultiRev"),
}

# Flagge gesetzt, Tafel widerspricht. Gemessen 2026-09-16.
WIDERSPRUCH: dict[str, str] = {
    "src/formats/atari/uft_pro_plugin.c::WEAK_BITS":
        "ECHT: Flagge ja, Tafel UNSUPPORTED mit Messung (MF-1054 - PRO "
        "speichert keine schwachen Bits, sein Schutz sind Phantomsektoren).",
    "src/formats/g71/uft_g71.c::READ":
        "STRUKTURELL: Tafel sagt PARTIAL, eine Bitflagge kann das nicht.",
    "src/formats/kfx/uft_kfx.c::READ": "STRUKTURELL: Tafel PARTIAL.",
    "src/formats/logical/uft_logical.c::READ": "STRUKTURELL: Tafel PARTIAL.",
    "src/formats/mfi/uft_mfi.c::READ": "STRUKTURELL: Tafel PARTIAL.",
}

# Flagge gesetzt, Tafel schweigt zu dieser Achse. Gemessen 2026-09-16.
EINSEITIG: dict[str, str] = {
    "src/formats/adf/uft_adf_plugin.c::READ": "Tafel ohne Read-Eintrag.",
    "src/formats/adf/uft_adf_plugin.c::WRITE": "Tafel ohne Write-Eintrag.",
    "src/formats/adf_ext/uft_adf_ext.c::READ": "Tafel ohne Read-Eintrag.",
    "src/formats/apple/uft_woz_plugin.c::READ": "Tafel ohne Read-Eintrag.",
    "src/formats/dsk_generic/uft_dsk_generic.c::READ":
        "49 Makro-Ausweitungen in EINER Datei; die Tafel gilt nicht je Format.",
    "src/formats/dsk_generic/uft_dsk_generic.c::WRITE": "wie READ.",
    "src/formats/hfe/uft_hfe.c::READ": "Tafel ohne Read-Eintrag.",
    "src/formats/hfe/uft_hfe.c::WRITE": "Tafel ohne Write-Eintrag.",
    "src/formats/hfe/uft_hfe.c::TIMING":
        "TIMING gesetzt, Tafel schweigt - und HFE ist ein Zellstrom, kein "
        "Fluss; CAP_FLUX ist dort bewusst NICHT gesetzt.",
    "src/formats/ipf/uft_ipf_plugin.c::FLUX":
        "FLUX gesetzt, Tafel schweigt. Seit MF-1079 auf T1.",
    "src/formats/ipf/uft_ipf_plugin.c::READ": "Tafel ohne Read-Eintrag.",
    "src/formats/lisa/uft_lisa_twiggy.c::READ": "Tafel ohne Read-Eintrag.",
    "src/formats/lisa/uft_lisa_twiggy.c::WRITE": "Tafel ohne Write-Eintrag.",
    "src/formats/stx/uft_stx_plugin.c::READ": "Tafel ohne Read-Eintrag.",
    "src/formats/stx/uft_stx_plugin.c::TIMING": "Tafel ohne Timing-Eintrag.",
}

RE_FEAT = re.compile(r'\{\s*"([^"]+)"\s*,\s*UFT_FEATURE_([A-Z_]+)')
RE_BLOCK = re.compile(r"/\*.*?\*/", re.DOTALL)
RE_ZEILE = re.compile(r"//[^\n]*")


def _ohne_kommentare(text: str) -> str:
    """Ein Flaggenname in einem Kommentar ist keine Flagge (Klasse MF-767).

    SEIT MF-1247 NICHT MEHR AUF DEM URTEILSPFAD. `_paare()` fragt den
    Zerteiler `c_lex`, weil dieselbe Ersetzung zwei verschiedene Fragen
    bedienen musste und dabei falsch lag: die Bitflagge ist ein
    BEZEICHNER, der Merkmalsname eine ZEICHENKETTE. Eine Zeichenkette,
    die den Flaggennamen nennt, ERWAEHNT ihn nur — in
    `uft_woz_plugin.c` verneint sie ihn sogar.

    Die Funktion bleibt stehen (nicht entfernen, weiter erweitern) und
    ist zugleich das Belegstueck: diese zwei Regex halten ein `/*`
    INNERHALB einer Zeichenkette fuer einen Kommentaranfang und fressen
    bis zum naechsten `*/` — gemessen, wenn irgendwo spaeter eines
    folgt. `c_lex` kennt den Unterschied.
    """
    return RE_ZEILE.sub(" ", RE_BLOCK.sub(" ", text))


def _dateien(repo: Path) -> list[str]:
    try:
        roh = subprocess.run(["git", "ls-files", "src/formats"], cwd=repo,
                             capture_output=True, text=True, timeout=120)
        namen = roh.stdout.split()
    except Exception:
        namen = []
    if not namen:
        basis = repo / "src" / "formats"
        namen = [str(p.relative_to(repo)).replace("\\", "/")
                 for p in basis.rglob("*.c")] if basis.is_dir() else []
    return sorted({n for n in namen if n.endswith(".c")})


def _paare(repo: Path):
    """Liefert (rel, achse, hat_flagge, tafel_status|None) je Aussage."""
    for rel in _dateien(repo):
        p = repo / rel
        try:
            t = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        if ".capabilities" not in t and "uft_plugin_feature_t" not in t:
            continue
        # MF-1247: in TOKEN denken statt in Zeichen. Vorher lief beides
        # ueber denselben kommentarfreien TEXT, und damit galt jede
        # Zeichenkette, die den Flaggennamen nennt, als Flagge —
        # `uft_woz_plugin.c` und `uft_td0.c` tragen eine, deren Satz die
        # Flagge ausdruecklich VERNEINT ("beansprucht kein
        # UFT_FORMAT_CAP_WRITE"), waehrend daneben `.capabilities =
        # UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_VERIFY` steht. Zwei
        # falsche Befunde, und im Nachbartor dieselben zwei.
        #
        # Die Trennung ist jetzt die des Zerteilers und nicht mehr die
        # einer Ersetzung: die Bitflagge ist ein BEZEICHNER, der
        # Merkmalsname ist eine ZEICHENKETTE. `c_lex` (MF-1171,
        # A-027/MF-1228) wirft Kommentare von sich aus weg und liefert
        # bei `str` schon den entschaerften Inhalt — also genau das,
        # was `RE_FEAT` muehsam nachgebaut hat.
        # `lex_mit_makros`, nicht `lex`: `uft_dsk_generic.c` definiert
        # 49 Plugins ueber EIN `#define DSK_PLUGIN(...)`, und alle drei
        # Flaggen stehen in dessen Rumpf. Ueber `lex` allein waeren sie
        # unsichtbar — das waere derselbe Fehler in die andere
        # Richtung, gemessen als zwei verschwundene Grundlinien.
        toks = c_lex.lex_mit_makros(t)
        flaggen = {tk.text[len("UFT_FORMAT_CAP_"):]
                   for tk in toks
                   if tk.kind == c_lex.KIND_ID
                   and tk.text.startswith("UFT_FORMAT_CAP_")}
        tafel: dict[str, str] = {}
        for a, b, c in zip(toks, toks[1:], toks[2:]):
            if (a.kind == c_lex.KIND_STR
                    and b.kind == c_lex.KIND_PUNCT and b.text == ","
                    and c.kind == c_lex.KIND_ID
                    and c.text.startswith("UFT_FEATURE_")):
                tafel.setdefault(a.value,
                                 c.text[len("UFT_FEATURE_"):])
        for achse, namen in ACHSEN.items():
            hat = achse in flaggen
            status = next((tafel[n] for n in namen if n in tafel), None)
            if status is None and not hat:
                continue
            yield rel, achse, hat, status


def check(repo: Path) -> list[str]:
    befunde: list[str] = []
    gesehen_w: set[str] = set()
    gesehen_e: set[str] = set()

    for rel, achse, hat, status in _paare(repo):
        key = f"{rel}::{achse}"
        if status is None:
            if hat:
                gesehen_e.add(key)
                if key not in EINSEITIG:
                    befunde.append(
                        f"{key}: Bitflagge gesetzt, Merkmalstafel schweigt "
                        f"- neu, nicht in der Grundlinie.")
            continue
        sagt_ja = status == "SUPPORTED"
        if hat == sagt_ja:
            continue
        gesehen_w.add(key)
        if key not in WIDERSPRUCH:
            befunde.append(
                f"{key}: Flagge={'ja' if hat else 'nein'}, Tafel={status} "
                f"- neuer Widerspruch, nicht in der Grundlinie.")

    for key in WIDERSPRUCH:
        if key not in gesehen_w:
            befunde.append(f"{key}: Grundlinien-Widerspruch ist weg - "
                           f"bitte aus WIDERSPRUCH entfernen.")
    for key in EINSEITIG:
        if key not in gesehen_e:
            befunde.append(f"{key}: einseitige Aussage ist weg - "
                           f"bitte aus EINSEITIG entfernen.")
    return befunde


def selbsttest() -> bool:
    """Gepflanzte Faelle; bei roter Abnahme bricht das Tor ab."""
    global WIDERSPRUCH, EINSEITIG

    faelle = [
        ("einig_ja",
         '.capabilities = UFT_FORMAT_CAP_FLUX,\n'
         '{ "Flux", UFT_FEATURE_SUPPORTED, NULL },\n', False),
        ("einig_nein",
         '.capabilities = UFT_FORMAT_CAP_READ,\n'
         '{ "Flux", UFT_FEATURE_UNSUPPORTED, NULL },\n'
         '{ "Read", UFT_FEATURE_SUPPORTED, NULL },\n', False),
        ("flagge_ja_tafel_nein",
         '.capabilities = UFT_FORMAT_CAP_FLUX,\n'
         '{ "Flux", UFT_FEATURE_UNSUPPORTED, NULL },\n', True),
        ("flagge_nein_tafel_ja",
         '.capabilities = UFT_FORMAT_CAP_READ,\n'
         '{ "Flux", UFT_FEATURE_SUPPORTED, NULL },\n'
         '{ "Read", UFT_FEATURE_SUPPORTED, NULL },\n', True),
        ("nur_flagge",
         '.capabilities = UFT_FORMAT_CAP_FLUX,\n'
         'const uft_plugin_feature_t f[] = '
         '{ { "Read", UFT_FEATURE_SUPPORTED, NULL } };\n', True),
        # Klasse MF-767: die Flagge steht nur in einem Blockkommentar.
        ("flagge_im_blockkommentar",
         '/* MF-880: UFT_FORMAT_CAP_FLUX entfernt. */\n'
         '.capabilities = UFT_FORMAT_CAP_READ,\n'
         '{ "Flux", UFT_FEATURE_UNSUPPORTED, NULL },\n'
         '{ "Read", UFT_FEATURE_SUPPORTED, NULL },\n', False),
        ("flagge_im_zeilenkommentar",
         '// UFT_FORMAT_CAP_FLUX war hier\n'
         '.capabilities = UFT_FORMAT_CAP_READ,\n'
         '{ "Flux", UFT_FEATURE_UNSUPPORTED, NULL },\n'
         '{ "Read", UFT_FEATURE_SUPPORTED, NULL },\n', False),
    ]

    ok = 0
    with tempfile.TemporaryDirectory() as td:
        repo = Path(td)
        ziel = repo / "src" / "formats" / "probe"
        ziel.mkdir(parents=True)
        for name, rumpf, erwartet in faelle:
            p = ziel / f"{name}.c"
            p.write_text(rumpf, encoding="utf-8")
            alt_w, alt_e = WIDERSPRUCH, EINSEITIG
            WIDERSPRUCH, EINSEITIG = {}, {}
            try:
                traf = any(name in b for b in check(repo))
            finally:
                WIDERSPRUCH, EINSEITIG = alt_w, alt_e
            if traf == erwartet:
                ok += 1
            else:
                print(f"  SELBSTTEST FEHLER: {name} erwartet "
                      f"{'Befund' if erwartet else 'sauber'}")
            p.unlink()

    print(f"  Selbsttest: {ok}/{len(faelle)}")
    return ok == len(faelle)


def main() -> int:
    if "--selbsttest" in sys.argv:
        return 0 if selbsttest() else 1

    repo = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    if not selbsttest():
        print("Tor bricht ab: eigener Selbsttest rot.")
        return 2

    errs = check(repo)
    print(f"Faehigkeitsaussagen, zwei Quellen (root={repo}):")
    print(f"  Grundlinie Widersprueche : {len(WIDERSPRUCH)}")
    print(f"  Grundlinie einseitig     : {len(EINSEITIG)}")
    print(f"  Befunde                  : {len(errs)}")
    for e in errs[:40]:
        print(f"    {e}")
    if len(errs) > 40:
        print(f"    ... und {len(errs) - 40} weitere")
    return 1 if errs else 0


if __name__ == "__main__":
    sys.exit(main())
