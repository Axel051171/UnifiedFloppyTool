#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Tor 70: sagt ein Plugin eine Faehigkeit zu, zu der kein Weg fuehrt? (MF-1200)

Aufrufer/Importeure: `scripts/check_consistency.py` ruft `check(repo)` als
eine weitere Kategorie; sonst niemand. Beruehrte API: keine — das Tor LIEST
`src/formats/**/*.c` und `docs/VERIFICATION_TIERS.md`. Abgelegtes Schema: nur
mit `--json <datei>` (Aufbau unten unter "Ausgabe fuer die Oberflaeche").
Woertlicher Auftrag des Eigentuemers: "bazes" — bau den Faehigkeits-Audit,
der je Format und Einstellung die vier Zustaende declared/wired/tested/
verified trennt, statt eines `supported=true`.

── Warum es dieses Tor gibt ─────────────────────────────────────────────

Der Entwurf des Eigentuemers nennt den Fehlerfall so:

    capability.write = true
    plugin->write == NULL

Gemessen gibt es diesen Fall in diesem Baum **kein einziges Mal** — und das
ist keine Entwarnung, sondern die Aufforderung, eine Ebene tiefer zu sehen.
`src/formats/cqm/uft_cqm.c:328` sagt ausdruecklich, warum:

    "`write_track` bleibt GESETZT statt NULL: ein Nullzeiger gaebe dem
     Aufrufer keinen Grund."

Ein gesetzter Rueckruf ist also **Absicht auch dann, wenn die Faehigkeit
fehlt** — der Rumpf sagt mit `UFT_ERROR_NOT_SUPPORTED` benannt ab, statt den
Aufrufer in einen Nullzeiger laufen zu lassen. Die Frage "ist die Zusage
gedeckt" laesst sich damit an der Zuweisung NICHT beantworten. Sie wird hier
am **Weg** beantwortet: fuehrt vom Rueckruf aus, transitiv innerhalb der
Datei, ein Pfad zu einer Funktion mit einem Erfolgsausgang?

── Drei eigene Fehlalarme, die zu dieser Bauform gefuehrt haben ─────────

Die Vormessung hat drei Fassungen gebraucht, und jede Verwerfung steht hier,
weil sie die naechste Fassung erklaert:

  1. "jedes `return` ist ein Fehlercode"  -> `uft_fdi_plugin.c` und
     `uft_nfd_plugin.c` geben `return rc;` zurueck, eine VARIABLE. Beide
     schreiben wirklich (`nfd_write_track` oeffnet mit `fopen(…, "r+b")`).
     Zwei Fehlalarme.
  2. dieselbe Zeile per `sed` berichtigt -> aus dem Wortgrenzen-Ausdruck
     wurde ein blosses `b`, der Ausdruck konnte nie zutreffen, und **alle
     78** galten als Absager. Klasse `heredoc_zerstoert_escapes`:
     Ausdruecke mit Backslash gehoeren nicht durch `sed`.
  3. "der Rumpf des Rueckrufs nennt `UFT_OK`" -> `g64_read_track` DELEGIERT
     an `g64_read_slot`; sein eigener Rumpf nennt nichts. G64 ist eines der
     sieben verlustfrei gemessenen Wandlungsziele — ein absagender G64-Leser
     waere ein absurdes Ergebnis gewesen. Dritter Fehlalarm derselben Wurzel.

Erst (3) hat die Bauform erzwungen: **Fixpunkt ueber die Aufrufkanten
innerhalb der Datei**, uebernommen von `_erreicht_schreiber()` aus Tor 57
(`audit_schreibzusage.py`, MF-930). Dort war die Frage "erreicht `close` den
Schreiber"; hier ist sie "erreicht der Rueckruf einen Erfolgsausgang".

── Was das Tor NICHT sieht, und das ist wichtig ─────────────────────────

  * **Ueber Dateigrenzen wird nicht verfolgt.** Delegiert ein Rueckruf an
    eine Funktion in einer anderen Uebersetzungseinheit, gilt er hier als
    nicht aufgeloest und wird als `unentscheidbar` gefuehrt, nicht als
    Absage. Dieselbe benannte Unschaerfe wie in Tor 57.
  * **VERIFY ist ueberwiegend unentscheidbar.** Gemessen tragen 84 Plugins
    `.verify_track`, dessen Ziel in 84 Faellen nicht in derselben Datei
    definiert ist (gemeinsamer Helfer). Das Tor sagt das, statt 84 Befunde
    zu erfinden.
  * **Ein Erfolgsausgang ist kein Beweis, dass die Faehigkeit TRAEGT.** Er
    beweist, dass es einen Weg gibt, der nicht absagt. Ob am Ende Bytes in
    der Datei stehen, misst Tor 57 (Schreibzusage) und beantworten die
    Stufen T1/T1b in `docs/VERIFICATION_TIERS.md`. Die drei Fragen sind
    ausdruecklich getrennt — das ist der ganze Zweck der vier Zustaende.
  * **`tested` und `verified` werden BERICHTET, nicht bewacht.** Die
    Zuordnung Datei -> Formatname ist nicht eindeutig (`uft_dsk_generic.c`
    traegt 49 Formate), und eine Grundlinie auf einer unsicheren Zuordnung
    waere eine Zahl ohne Messung. Sie stehen in der JSON-Ausgabe; rot wird
    das Tor allein an `declared` gegen `wired`.

── Die vier Zustaende ───────────────────────────────────────────────────

    declared   Bitflagge `UFT_FORMAT_CAP_*` oder Merkmalstafel sagt zu.
               (Widersprueche ZWISCHEN den beiden gehoeren Tor 69,
                `audit_faehigkeitsaussage.py` — hier nicht doppelt gemessen,
                MF-1177.)
    wired      Vom Rueckruf aus ist transitiv in derselben Datei ein
               Erfolgsausgang erreichbar.
    tested     Der Formatname kommt in `tests/` vor.        (berichtet)
    verified   Stufe aus `docs/VERIFICATION_TIERS.md`.      (berichtet)

Beide Grundlinien duerfen nur SINKEN (Bauform Tor 57).
"""
from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

import c_lex   # MF-1247: in TOKEN denken statt in Zeichen

# ── Achsen mit einem eigenen Rueckruf ────────────────────────────────────
# Nur diese vier sind ueber einen Rueckruf ueberhaupt entscheidbar.
# FLUX/TIMING/WEAK_BITS/MULTI_REV sind Aussagen ueber die DATEN, die
# `read_track` liefert, und haben keinen eigenen Zeiger — sie stehen in der
# JSON-Ausgabe als `unentscheidbar`, damit niemand sie fuer geprueft haelt.
ACHSEN = {
    "READ":   ("read_track",),
    "WRITE":  ("write_track",),
    "VERIFY": ("verify_track", "verify_sector"),
    "CREATE": ("create",),
}
OHNE_RUECKRUF = ("FLUX", "TIMING", "WEAK_BITS", "MULTI_REV", "STREAMING")

# Merkmalstafel-Namen je Achse (freie Zeichenketten — Aufzaehlung hier
# unvermeidbar, wie in Tor 69 benannt).
TAFEL = {
    "READ": ("Read",), "WRITE": ("Write",),
    "VERIFY": ("Verify",), "CREATE": ("Create",),
}

RE_BLOCK = re.compile(r"/\*.*?\*/", re.DOTALL)
RE_ZEILE = re.compile(r"//[^\n]*")
RE_ERFOLG = re.compile(r"\b(UFT_OK|UFT_SUCCESS)\b")
RE_FEAT = re.compile(r'\{\s*"([^"]+)"\s*,\s*UFT_FEATURE_([A-Z_]+)')
FUNK_KOPF = re.compile(r"^[A-Za-z_][\w \t\*]*?\b(\w+)\s*\([^;{]*\)\s*\{", re.M)

# ── Grundlinien (gemessen; duerfen nur sinken) ───────────────────────────
# Zusage vorhanden, aber kein Weg zu einem Erfolgsausgang.
ZUSAGE_OHNE_WEG: dict[str, str] = {}
# Weg vorhanden, aber keine Zusage. Gemessen 2026-09-16.
WEG_OHNE_ZUSAGE: dict[str, str] = {
    "src/formats/udi/uft_udi_plugin.c::WRITE":
        "STRUKTURELL, und die Absage ist die ehrliche Antwort. "
        "`udi_write_track` schreibt wirklich bis in die Datei — aber nur "
        "bei EXAKT gleicher Spurlaenge, und die Taktmarken der "
        "ueberschriebenen Spur bleiben unangetastet, waehrend die Daten "
        "neu kodiert werden. Die Merkmalstafel sagt deshalb "
        "`{ \"Write\", UFT_FEATURE_UNSUPPORTED, \"MF-1015: write_track "
        "schreibt bis in die Datei, aber nur bei exakt gleicher "
        "Spurlaenge und ohne die Taktmarken nachzuziehen\" }`. "
        "Derselbe Wortschatz-Mangel wie bei den vier PARTIAL-Faellen in "
        "Tor 69: weder Bitflagge noch Merkmalstafel koennen "
        "\"nur unter dieser Bedingung\" ausdruecken. Der Weg heraus ist "
        "eine Entwurfsentscheidung, keine Korrektur.",
}


def _ohne_kommentare(text: str) -> str:
    """Ein Flaggenname in einem Kommentar ist keine Flagge (Klasse MF-767).

    SEIT MF-1247 nur noch fuer `_rumpfe()` und `.name = "..."` — die
    Bitflagge und die Merkmalstafel kommen aus dem Zerteiler `c_lex`.
    Der Grund: eine Zeichenkette, deren Text die Flagge VERNEINT
    ("beansprucht kein UFT_FORMAT_CAP_WRITE" in `uft_woz_plugin.c` und
    `uft_td0.c`), galt hier als Zusage und ergab zwei falsche Befunde.

    Diese zwei Regex bleiben stehen und sind zugleich das Belegstueck:
    sie halten ein `/*` INNERHALB einer Zeichenkette fuer einen
    Kommentaranfang und fressen bis zum naechsten `*/`. Ob das
    `_rumpfe()` heute schadet, ist NICHT gemessen (P3-500).
    """
    return RE_ZEILE.sub(" ", RE_BLOCK.sub(" ", text))


def _rumpfe(t: str) -> dict:
    """{Funktionsname: Rumpftext} einer entkernten Uebersetzungseinheit.

    Bauform uebernommen aus Tor 57 (`audit_schreibzusage.py::_rumpfe`).
    """
    aus: dict[str, str] = {}
    for m in FUNK_KOPF.finditer(t):
        tiefe, i = 0, m.end() - 1
        while i < len(t):
            if t[i] == "{":
                tiefe += 1
            elif t[i] == "}":
                tiefe -= 1
                if tiefe == 0:
                    aus[m.group(1)] = t[m.end():i]
                    break
            i += 1
    return aus


def _erreicht_erfolg(rumpfe: dict) -> set:
    """Funktionen, von denen aus ein Erfolgsausgang erreichbar ist.

    Transitiv INNERHALB der Datei: erst die, die selbst einen Erfolgsausgang
    nennen, dann solange deren Aufrufer dazu, bis sich nichts mehr aendert.
    Ueber Dateigrenzen wird nicht verfolgt — die benannte Unschaerfe.
    """
    erreicht = {n for n, r in rumpfe.items() if RE_ERFOLG.search(r)}
    geaendert = True
    while geaendert:
        geaendert = False
        for n, r in rumpfe.items():
            if n in erreicht:
                continue
            for z in erreicht:
                if re.search(r"\b%s\s*\(" % re.escape(z), r):
                    erreicht.add(n)
                    geaendert = True
                    break
    return erreicht


def _dateien(repo: Path) -> list[str]:
    """Dateimenge aus git, nie aus einer gepflegten Liste (MF-636)."""
    try:
        roh = subprocess.run(["git", "ls-files", "src/formats"], cwd=repo,
                             capture_output=True, text=True, timeout=120)
        namen = roh.stdout.split() if roh.returncode == 0 else []
    except Exception:
        namen = []
    if not namen:
        basis = repo / "src" / "formats"
        namen = ([str(p.relative_to(repo)).replace("\\", "/")
                  for p in basis.rglob("*.c")] if basis.is_dir() else [])
    return sorted({n for n in namen if n.endswith(".c")})


def _lage(text: str):
    """-> {achse: (declared, zustand)} fuer eine Plugin-Datei.

    zustand in {"wired", "sagt ab", "unentscheidbar", "kein Rueckruf"}
    """
    # MF-1247: die Bitflagge ist ein BEZEICHNER, der Merkmalsname eine
    # ZEICHENKETTE — derselbe Fehler wie im Nachbartor, dieselben zwei
    # falschen Befunde („zugesagt, aber kein Rueckruf" fuer
    # `uft_woz_plugin.c` und `uft_td0.c`, die beide kein
    # UFT_FORMAT_CAP_WRITE setzen und in einer Zeichenkette genau das
    # SAGEN). Gefragt wird deshalb der Zerteiler `c_lex`, nicht der
    # Text.
    #
    # `_rumpfe()` bleibt absichtlich auf der alten Textlesart, damit
    # sich sein Verhalten hier nicht ungemessen aendert; ob eine
    # geschweifte Klammer in einer Zeichenkette seine Rumpfsuche stoert,
    # ist eine eigene, ungemessene Frage (P3-500).
    t = _ohne_kommentare(text)
    # `lex_mit_makros`: die 49 Plugins von `uft_dsk_generic.c` stehen
    # im Rumpf eines `#define`, das `lex` als EIN Token ausgibt.
    toks = c_lex.lex_mit_makros(text)
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
            tafel.setdefault(a.value, c.text[len("UFT_FEATURE_"):])

    rumpfe = _rumpfe(t)
    erfolg = _erreicht_erfolg(rumpfe)

    aus = {}
    for achse, namen in ACHSEN.items():
        sagt_tafel = any(tafel.get(n) == "SUPPORTED" for n in TAFEL[achse])
        declared = (achse in flaggen) or sagt_tafel

        ruf = None
        for n in namen:
            m = re.search(r"\." + n + r"\s*=\s*(?!NULL\b|0\b)([A-Za-z_]\w*)", t)
            if m:
                ruf = m.group(1)
                break
        if ruf is None:
            zustand = "kein Rueckruf"
        elif ruf not in rumpfe:
            zustand = "unentscheidbar"      # Ziel in anderer Datei
        elif ruf in erfolg:
            zustand = "wired"
        else:
            zustand = "sagt ab"
        aus[achse] = (declared, zustand)
    return aus


def _plugin_dateien(repo: Path):
    for rel in _dateien(repo):
        try:
            roh = (repo / rel).read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        if ".capabilities" not in roh:
            continue
        yield rel, roh


def check(repo: Path) -> list[str]:
    befunde: list[str] = []
    gesehen_z: set[str] = set()
    gesehen_w: set[str] = set()

    for rel, roh in _plugin_dateien(repo):
        for achse, (declared, zustand) in _lage(roh).items():
            key = f"{rel}::{achse}"
            if declared and zustand in ("sagt ab", "kein Rueckruf"):
                gesehen_z.add(key)
                if key not in ZUSAGE_OHNE_WEG:
                    befunde.append(
                        f"{key}: zugesagt, aber {zustand} — kein Weg zu einem "
                        f"Erfolgsausgang. Neu, nicht in der Grundlinie.")
            elif not declared and zustand == "wired":
                gesehen_w.add(key)
                if key not in WEG_OHNE_ZUSAGE:
                    befunde.append(
                        f"{key}: Weg vorhanden, aber keine Zusage — die "
                        f"Faehigkeit ist da und wird verschwiegen. Neu.")

    for key in ZUSAGE_OHNE_WEG:
        if key not in gesehen_z:
            befunde.append(f"{key}: Grundlinien-Fall ist weg — bitte aus "
                           f"ZUSAGE_OHNE_WEG entfernen.")
    for key in WEG_OHNE_ZUSAGE:
        if key not in gesehen_w:
            befunde.append(f"{key}: Grundlinien-Fall ist weg — bitte aus "
                           f"WEG_OHNE_ZUSAGE entfernen.")
    return befunde


# ── Ausgabe fuer die Oberflaeche ─────────────────────────────────────────

def matrix(repo: Path) -> dict:
    """{datei: {name, achsen: {achse: {declared, wired, zustand}}}}."""
    aus: dict[str, dict] = {}
    for rel, roh in _plugin_dateien(repo):
        t = _ohne_kommentare(roh)
        nm = re.search(r'\.name\s*=\s*"([^"]*)"', t)
        eintrag = {"name": nm.group(1) if nm else None, "achsen": {}}
        for achse, (declared, zustand) in _lage(roh).items():
            eintrag["achsen"][achse] = {
                "declared": declared,
                "wired": zustand == "wired",
                "zustand": zustand,
            }
        flaggen = set(re.findall(r"UFT_FORMAT_CAP_([A-Z_]+)", t))
        for achse in OHNE_RUECKRUF:
            eintrag["achsen"][achse] = {
                "declared": achse in flaggen,
                "wired": None,
                "zustand": "unentscheidbar — kein eigener Rueckruf",
            }
        aus[rel] = eintrag
    return aus


def selbsttest() -> bool:
    """Gepflanzte Faelle; bei roter Abnahme bricht das Tor ab."""
    faelle = [
        # (Quelltext, Achse, erwartet_declared, erwartet_zustand)
        ('static uft_error_t w(void){ return UFT_OK; }\n'
         'x = { .capabilities = UFT_FORMAT_CAP_WRITE, .write_track = w };',
         "WRITE", True, "wired"),
        # Absager: Rumpf nennt keinen Erfolgsausgang.
        ('static uft_error_t w(void){ return UFT_ERROR_NOT_SUPPORTED; }\n'
         'x = { .capabilities = UFT_FORMAT_CAP_WRITE, .write_track = w };',
         "WRITE", True, "sagt ab"),
        # Delegation: der Rueckruf selbst nennt nichts (Fehlalarm 3).
        ('static uft_error_t hilf(void){ return UFT_OK; }\n'
         'static uft_error_t w(void){ return hilf(); }\n'
         'x = { .capabilities = UFT_FORMAT_CAP_WRITE, .write_track = w };',
         "WRITE", True, "wired"),
        # Rueckgabe einer Variablen, nicht des Literals (Fehlalarm 1).
        ('static uft_error_t w(void){ uft_error_t rc = UFT_OK; return rc; }\n'
         'x = { .capabilities = UFT_FORMAT_CAP_WRITE, .write_track = w };',
         "WRITE", True, "wired"),
        # NULL zugewiesen -> kein Rueckruf.
        ('x = { .capabilities = UFT_FORMAT_CAP_WRITE, .write_track = NULL };',
         "WRITE", True, "kein Rueckruf"),
        # Ziel in anderer Datei -> unentscheidbar, NICHT "sagt ab".
        ('x = { .capabilities = UFT_FORMAT_CAP_WRITE, .write_track = fremd };',
         "WRITE", True, "unentscheidbar"),
        # Flagge nur im Kommentar zaehlt nicht (Klasse MF-767).
        ('/* UFT_FORMAT_CAP_WRITE entfernt */\n'
         'static uft_error_t w(void){ return UFT_OK; }\n'
         'x = { .capabilities = UFT_FORMAT_CAP_READ, .write_track = w };',
         "WRITE", False, "wired"),
        # Erfolgsausgang nur im Kommentar zaehlt nicht.
        ('static uft_error_t w(void){ /* liefert UFT_OK */\n'
         '  return UFT_ERROR_NOT_SUPPORTED; }\n'
         'x = { .capabilities = UFT_FORMAT_CAP_WRITE, .write_track = w };',
         "WRITE", True, "sagt ab"),
        # Merkmalstafel allein genuegt als Zusage.
        ('static uft_error_t c(void){ return UFT_ERROR_NOT_SUPPORTED; }\n'
         'f[] = { { "Create", UFT_FEATURE_SUPPORTED, "" } };\n'
         'x = { .capabilities = 0, .create = c };',
         "CREATE", True, "sagt ab"),
        # Tafel sagt UNSUPPORTED und keine Flagge -> keine Zusage.
        ('static uft_error_t c(void){ return UFT_OK; }\n'
         'f[] = { { "Create", UFT_FEATURE_UNSUPPORTED, "" } };\n'
         'x = { .capabilities = 0, .create = c };',
         "CREATE", False, "wired"),
    ]
    rot = 0
    for i, (text, achse, erw_d, erw_z) in enumerate(faelle, 1):
        d, z = _lage(text)[achse]
        if d != erw_d or z != erw_z:
            print(f"  [ROT] Fall {i}: erwartet ({erw_d}, {erw_z}), "
                  f"gemessen ({d}, {z})")
            rot += 1
    print(f"  Selbsttest {len(faelle) - rot}/{len(faelle)}")
    return rot == 0


def main() -> int:
    repo = Path(__file__).resolve().parent.parent
    if "--selbsttest" in sys.argv:
        return 0 if selbsttest() else 1
    if not selbsttest():
        print("Selbsttest ROT — das Tor misst nicht, was es behauptet.")
        return 1

    if "--json" in sys.argv:
        ziel = Path(sys.argv[sys.argv.index("--json") + 1])
        ziel.write_text(json.dumps(matrix(repo), ensure_ascii=False, indent=1),
                        encoding="utf-8")
        print(f"Matrix -> {ziel}")

    befunde = check(repo)
    for b in befunde:
        print("  " + b)
    print(f"\nTor 70: {len(befunde)} Befunde "
          f"(Grundlinie {len(ZUSAGE_OHNE_WEG)} Zusage-ohne-Weg, "
          f"{len(WEG_OHNE_ZUSAGE)} Weg-ohne-Zusage)")
    return 1 if befunde else 0


if __name__ == "__main__":
    sys.exit(main())
