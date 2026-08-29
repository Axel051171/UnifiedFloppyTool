#!/usr/bin/env python3
"""Kein Bedienelement ohne Lesestelle (MF-669).

    python scripts/audit_setting_wiring.py [--liste]

── Warum es dieses Tor gibt ─────────────────────────────────────────────

Ein Einstellungsfeld, das **geschrieben** und nie **gelesen** wird, ist
das Oberflaechen-Gegenstueck zum Test, der nicht scheitern kann. Es sieht
aus wie Faehigkeit, verhaelt sich wie Faehigkeit, und tut nichts. Der
Benutzer stellt etwas ein, es wird gemerkt, es wird ihm beim naechsten
Oeffnen wieder gezeigt — und es wirkt sich auf nichts aus.

In diesem Baum ist das siebenmal durchgerutscht. Beim letzten Mal
(MF-668) standen 38 Bedienelemente in drei Dialogen, deren vollstaendiger
Weg lautete: Dialog -> Mitglied -> Dialog. Gefunden wurde das nicht von
einem Tor, sondern weil jemand zufaellig hinsah.

Dieses Tor sieht hin, jedes Mal.

── Warum es nicht `grep -c` ist ─────────────────────────────────────────

Der naheliegende Weg — den Feldnamen zaehlen — ist nachweislich falsch.
Beim Messen von Hand ergab er dreimal ein Ergebnis, das nicht stimmte:

  * `tolerance` schien Lesestellen zu haben. Zwei davon lagen in
    `otdr_event_core_v12.c` (`g->tolerance`) und in `advanceddialogs.cpp`
    (`p.tolerance`) — **andere Strukturen**, gleicher Feldname.
  * `revolution` schien lebendig. Die Treffer waren `d->revolution` aus
    einer SCP-Diagnosestruktur.

Feldnamen sind nicht eindeutig; ein Zaehler, der das ignoriert, spricht
tote Felder frei. Darum geht dieses Tor ueber **Variablen des jeweiligen
Typs**: es sucht Deklarationen (`flux_decoder_options_t opts;`,
`const uft_convert_options_t *o`) und zaehlt nur `opts.feld` /
`o->feld` fuer genau diese Namen.

── Was dieses Tor NICHT sehen kann ──────────────────────────────────────

Ehrlich benannt, weil eine Messung, die ihre Grenze verschweigt, als
Entwarnung missverstanden wird:

  * Zugriff ueber einen Zeiger, dessen Deklaration anders aussieht als
    die erkannten Muster (Gussformen, `void*`-Umwege, Makros).
  * Felder, die nur ueber `memcpy`/`memset` als Ganzes beruehrt werden.
  * Lesen in generiertem oder nicht eingebundenem Code.

Ein Feld, das dieses Tor als tot meldet, ist darum ein **Befund zum
Nachsehen**, kein Urteil — aber ein Befund, den bisher niemand hatte.
Umgekehrt gilt: was es als lebendig zaehlt, hat mindestens eine echte
Lesestelle, denn dafuer braucht es einen Treffer, keine Abwesenheit.

── Ausnahmen ────────────────────────────────────────────────────────────

`ERLAUBT_UNGELESEN` traegt Felder, die absichtlich nur geschrieben
werden — mit Grund und, wo es einen gibt, mit Plananker. Die Liste ist
bewusst kurz zu halten: sie ist eine Aufzaehlung bekannter Faelle, und
solche Listen veralten in diesem Baum (viermal belegt, siehe
`scripts/repo_scope.py`). Wer etwas eintraegt, schreibt dazu, wann es
wieder verschwindet.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(WURZEL / "scripts"))

from repo_scope import make_filter  # noqa: E402

# Welche Strukturen sind Einstellungen? Eine Struktur gehoert hierher,
# wenn ein Mensch ihre Felder waehlt — nicht, wenn sie Messwerte traegt.
GEPRUEFT = [
    ("include/uft/flux/uft_flux_decoder.h", "flux_decoder_options_t"),
    ("include/uft/uft_types.h",             "uft_convert_options_t"),
    ("include/uft/uft_format_convert.h",    "uft_convert_options_ext_t"),
]

# Feld -> Grund. Wer hier eintraegt, nennt auch das Ende.
ERLAUBT_UNGELESEN: dict[str, str] = {}

DEKL = [
    # flux_decoder_options_t opts;   /   const flux_decoder_options_t *opts
    r"\b{typ}\s+\*?\s*([A-Za-z_]\w*)\s*[;,)=]",
    r"\bconst\s+{typ}\s*\*\s*([A-Za-z_]\w*)",
]


def struktur_koerper(text: str, typ: str) -> str | None:
    """Der Rumpf GENAU dieser Struktur.

    Der erste Versuch war `typedef struct[^{]*\\{(.*?)\\}\\s*typ;` mit
    sparsamem `.*?`. Das ist falsch, und zwar auf die teuerste Art: der
    Ausdruck beginnt beim ERSTEN `typedef struct` der Datei und laeuft bis
    zur schliessenden Klammer der gesuchten — alle Strukturen dazwischen
    landen im Rumpf. Gemessen: das Tor meldete `transitions`,
    `sample_rate`, `good_sectors` und neun weitere als tote Felder von
    `flux_decoder_options_t`. Keines davon steht dort drin.

    Ein Tor, das falsche Befunde meldet, ist schlimmer als kein Tor — es
    erzieht dazu, rote Tore zu uebergehen. Darum hier kein Ausdruck ueber
    verschachtelte Klammern, sondern Zaehlen: die schliessende Klammer
    suchen, die den Namen traegt, und von dort rueckwaerts die zugehoerige
    oeffnende finden.
    """
    m = re.search(r"\}\s*" + re.escape(typ) + r"\s*;", text)
    if m:
        zu = text.rindex("}", 0, m.end())
        tiefe = 0
        for i in range(zu, -1, -1):
            if text[i] == "}":
                tiefe += 1
            elif text[i] == "{":
                tiefe -= 1
                if tiefe == 0:
                    return text[i + 1:zu]
        return None
    # `struct typ { ... };`
    m = re.search(r"struct\s+" + re.escape(typ) + r"\s*\{", text)
    if not m:
        return None
    auf = m.end() - 1
    tiefe = 0
    for i in range(auf, len(text)):
        if text[i] == "{":
            tiefe += 1
        elif text[i] == "}":
            tiefe -= 1
            if tiefe == 0:
                return text[auf + 1:i]
    return None


def felder(header: Path, typ: str) -> list[str]:
    """Die Feldnamen der Struktur, in Reihenfolge."""
    text = header.read_text(encoding="utf-8", errors="replace")
    koerper = struktur_koerper(text, typ)
    if koerper is None:
        return []
    # Verschachtelte Strukturen/Unionen zaehlen nicht als Felder dieser
    # Ebene; ihr eigener Name wird unten ueber den Rest der Zeile erfasst.
    while True:
        neu = re.sub(r"\{[^{}]*\}", " ", koerper)
        if neu == koerper:
            break
        koerper = neu
    koerper = re.sub(r"/\*.*?\*/", " ", koerper, flags=re.S)
    koerper = re.sub(r"//[^\n]*", " ", koerper)
    namen = []
    for zeile in koerper.split(";"):
        zeile = zeile.strip()
        if not zeile or "(" in zeile:      # Funktionszeiger uebergehen
            continue
        t = re.match(r"^[A-Za-z_][\w \t\*]*?([A-Za-z_]\w*)\s*(\[[^\]]*\])?$",
                     zeile.replace("\n", " "))
        if t:
            namen.append(t.group(1))
    return namen


def quellen(repo: Path) -> list[tuple[str, str]]:
    behalten, warnung = make_filter(repo)
    if warnung:
        print("HINWEIS: " + warnung)
    aus = []
    for p in list(repo.glob("src/**/*.c")) + list(repo.glob("src/**/*.cpp")) \
            + list(repo.glob("src/**/*.h")) + list(repo.glob("src/**/*.hpp")):
        if not behalten(p):
            continue
        try:
            aus.append((p.relative_to(repo).as_posix(),
                        p.read_text(encoding="utf-8", errors="replace")))
        except OSError:
            pass
    return aus


def zaehle(typ: str, feld: str, dateien) -> tuple[int, int, list[str]]:
    """(Schreibstellen, Lesestellen, Fundorte der Lesestellen)."""
    schreib = lies = 0
    orte: list[str] = []
    for pfad, text in dateien:
        vars_ = set()
        for muster in DEKL:
            vars_ |= set(re.findall(muster.format(typ=re.escape(typ)), text))
        # Benannte Initialisierer  .feld = ...  gelten als Schreiben, auch
        # ohne Variable — sonst faellt jede `= { .feld = x }`-Vorbelegung
        # durch und ein nur so gesetztes Feld gaelte als voellig unbenutzt.
        if re.search(r"\.\s*" + re.escape(feld) + r"\s*=", text):
            if re.search(re.escape(typ), text):
                schreib += 1
        for v in vars_:
            for zug in (r"\b" + re.escape(v) + r"\s*->\s*" + re.escape(feld) + r"\b",
                        r"\b" + re.escape(v) + r"\s*\.\s*" + re.escape(feld) + r"\b"):
                for m in re.finditer(zug, text):
                    rest = text[m.end():m.end() + 40]
                    # `x = ` ist Schreiben, `x == ` / `x != ` ist Lesen.
                    if re.match(r"\s*=(?!=)", rest):
                        schreib += 1
                    else:
                        lies += 1
                        zeile = text[:m.start()].count("\n") + 1
                        if len(orte) < 3:
                            orte.append(f"{pfad}:{zeile}")
    return schreib, lies, orte


# Trichter: Stellen, an denen eine Oberflaeche in eine Auswertung geht,
# und die dabei eine Konfiguration mitgeben. Genau EINE Aufrufstelle je
# Datei — sonst traegt jede eine eigene Kopie der Zuweisung, und eine
# davon veraltet still.
#
# Belegt: bis MF-671 rief `uft_otdr_panel.cpp` viermal
# `otdr_track_analyze()` auf, jedes Mal mit einer handkopierten
# Konfigurations-Zuweisung davor. Eine der vier war bereits
# unvollstaendig — sie setzte das Glaettungsfenster, aber nicht die
# Kodierung. Wer eine Kodierung waehlte und DANN lud, bekam die erste
# Spur mit "Auto" ausgewertet, waehrend der Kasten etwas anderes zeigte.
#
# Nachzaehlen, ob vor jeder Auswertung der Anwender steht, faellt erst
# auf, NACHDEM jemand es vergessen hat. Ein einziger Weg kann nicht
# vergessen werden; dieses Tor haelt fest, dass es einer bleibt.
TRICHTER = [
    ("src/gui/uft_otdr_panel.cpp", "otdr_track_analyze("),
]


def pruefe_trichter(repo: Path) -> list[str]:
    befunde = []
    for rel, ruf in TRICHTER:
        p = repo / rel
        if not p.is_file():
            befunde.append(f"{rel}: Datei fehlt — Trichter-Regel kann nicht "
                           f"geprueft werden (Eintrag veraltet?)")
            continue
        n = p.read_text(encoding="utf-8", errors="replace").count(ruf)
        if n == 0:
            befunde.append(f"{rel}: `{ruf}` kommt nicht mehr vor — der "
                           f"Trichter-Eintrag ist veraltet und gehoert "
                           f"entfernt oder korrigiert")
        elif n > 1:
            befunde.append(
                f"{rel}: `{ruf}` {n}-mal aufgerufen, erlaubt ist 1. Jede "
                f"weitere Stelle traegt ihre eigene Kopie der "
                f"Konfigurations-Zuweisung; genau so veraltete eine von "
                f"vieren still (MF-671).")
    return befunde


def check(repo: Path) -> list[str]:
    """Schnittstelle fuer `check_consistency.py`: eine Zeile je Befund."""
    dateien = quellen(repo)
    if not dateien:
        return ["audit_setting_wiring: keine Quelldateien im Blick — "
                "das Tor kann nichts sagen"]
    befunde = pruefe_trichter(repo)
    for hdr, typ in GEPRUEFT:
        namen = felder(repo / hdr, typ)
        if not namen:
            befunde.append(f"audit_setting_wiring: {typ} nicht lesbar "
                           f"({hdr}) — Fehler im Tor, nicht leere Struktur")
            continue
        for f in namen:
            s, l, _ = zaehle(typ, f, dateien)
            if s > 0 and l == 0 and f not in ERLAUBT_UNGELESEN:
                befunde.append(
                    f"{typ}.{f}: {s} Schreibstelle(n), 0 Lesestellen — "
                    f"eine Einstellung ohne Wirkung")
    return befunde


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--liste", action="store_true",
                    help="alle Felder zeigen, nicht nur die toten")
    a = ap.parse_args()

    dateien = quellen(WURZEL)
    if not dateien:
        print("FEHLER: keine Quelldateien im Blick — Tor kann nichts sagen.")
        return 2

    for z in pruefe_trichter(WURZEL):
        print("!! Trichter: " + z)

    tot: list[tuple[str, str, int]] = []
    unbenutzt: list[tuple[str, str]] = []
    gesamt = 0

    for hdr, typ in GEPRUEFT:
        pfad = WURZEL / hdr
        namen = felder(pfad, typ)
        if not namen:
            print(f"FEHLER: keine Felder in {typ} gefunden ({hdr}).")
            print("Das Tor kann eine Struktur nicht lesen — das ist ein")
            print("Fehler im Tor, nicht eine leere Struktur.")
            return 2
        print(f"\n{typ}  ({hdr}) — {len(namen)} Felder")
        for f in namen:
            gesamt += 1
            s, l, orte = zaehle(typ, f, dateien)
            marke = "  "
            if s > 0 and l == 0 and f not in ERLAUBT_UNGELESEN:
                marke = "!!"
                tot.append((typ, f, s))
            elif s == 0 and l == 0:
                unbenutzt.append((typ, f))
                marke = " ?"
            if a.liste or marke != "  ":
                wo = ("  " + ", ".join(orte)) if orte else ""
                print(f" {marke} {f:<28} schreib={s:<3} lies={l:<3}{wo}")

    print("\n" + "=" * 70)
    print(f"{gesamt} Felder geprueft.")
    if unbenutzt:
        print(f"{len(unbenutzt)} weder geschrieben noch gelesen "
              f"(kein Fehler, aber sehenswert):")
        for typ, f in unbenutzt:
            print(f"    {typ}.{f}")
    if not tot:
        print("Geschrieben-aber-nie-gelesen: 0")
        return 0

    print(f"\nROT: {len(tot)} Feld(er) werden gesetzt und nie gelesen.")
    for typ, f, s in tot:
        print(f"    {typ}.{f}  ({s} Schreibstelle(n), 0 Lesestellen)")
    print("""
Ein solches Feld ist eine Zusage ohne Deckung. Drei Wege, kein vierter:

  * Es hat einen Mechanismus unter anderem Namen -> Feld loeschen, den
    Regler auf den echten Mechanismus zeigen lassen.
  * Es soll einen bekommen -> Mechanismus zuerst (Rotbeweis, benannte
    Referenz), Feld danach.
  * Es hat keinen und soll keinen -> loeschen.

Absicht? Dann mit Grund und Ende nach ERLAUBT_UNGELESEN in dieser Datei.
""")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
