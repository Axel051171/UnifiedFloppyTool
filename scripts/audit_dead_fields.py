#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
Felder, die in einem oeffentlichen Header stehen und NIRGENDS geschrieben
werden (MF-831).

── Warum es dieses Tor gibt ────────────────────────────────────────────────

Dieselbe Fehlerklasse ist in drei Tagen dreimal aufgetreten:

  MF-829  `uft_fat_detect_t.fat_mismatch` — Warnfeld, nie gesetzt; ein Test
          sicherte zu, dass es `false` ist, und war gruen, WEIL die Pruefung
          fehlte.
  MF-830  `ipf_track_t.block_count` gegen `actual_blocks` — beide Zahlen
          lagen vor, verglichen hat sie niemand.
  MF-831  `uft_sector_t.id_offset` — „Bit offset of ID field", im ganzen
          Baum **null** Schreibstellen. Immer 0.

Ein nie geschriebenes Feld ist schlimmer als ein fehlendes: es sieht wie
eine Zusage aus. Wer `sector.id_offset` liest, bekommt 0 — und 0 ist eine
gueltige Bitposition.

── Was dieses Tor kann und was NICHT ───────────────────────────────────────

Gemeldet werden nur Felder, deren Name im ganzen Baum an **keiner** Stelle
auf der linken Seite einer Zuweisung steht.

**BERICHTIGT MF-1163.** Hier stand: „Das ist eindeutig und kann nicht falsch
anschlagen." Beide Haelften trugen nicht, und beide sind gemessen:

  1. Es hat falsch angeschlagen — mit Namen, die es im Baum gar nicht gibt.
     Die Regel `DEKL` durfte den Trenner zwischen Typ und Name leer lassen
     und zerlegte `unsigned populated;` zu `popula` + `ted`. Elf erfundene
     Feldnamen, hinter jedem ein Feld, das wirklich geschrieben wird.
     Einzelheiten stehen am Muster selbst.

  2. Es schlaegt weiter falsch an, und diese Luecke bleibt offen: ein Feld,
     das als AUS-PARAMETER gefuellt wird (`&out->checksum_stored`), steht
     nirgends links von einem `=`. Gemessen sind **79** der 1161 gemeldeten
     Felder irgendwo als `&x->feld` uebergeben. Abgezogen wird davon
     nichts — eine Adressuebergabe beweist keinen Schreibvorgang —, aber
     die Zahl wird seit MF-1163 getrennt ausgewiesen, damit niemand die
     Hauptzahl fuer schaerfer haelt, als sie ist.

Die Lehre ist nicht die Regel, sondern der Satz darueber: **eine Zusage,
ein Tor koenne nicht falsch anschlagen, ist selbst eine ungemessene Zahl.**
Sie hat hier dazu gefuehrt, dass niemand die gemeldeten Namen nachgesehen
hat — `ted`, `de0`, `unt`, `ull` standen jahrelang in der Ausgabe.

NICHT gefunden werden Felder, die zwar irgendwo geschrieben werden, aber
auf einer ANDEREN Struktur mit gleichnamigem Feld. Beispiel aus dem Baum:
`uft_sector_t.gap_before` hat genau einen Treffer — und der steht in
`uft_atarist_macrodos.c` auf einer eigenen Struktur. Das Feld der
kanonischen Struktur ist trotzdem tot; dieses Tor sieht es nicht.

Diese Grenze steht hier ausdruecklich, weil „0 gefunden" in diesem Baum
schon einmal als Entwarnung gelesen wurde und keine war. Wer sie aufheben
will, braucht eine Typanalyse, kein weiteres Muster.

── Dateimenge ──────────────────────────────────────────────────────────────

Aus `git ls-files`, nie aus einer gepflegten Verzeichnisliste (Grundsatz
MF-636).
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
GRUNDLINIE = WURZEL / "docs" / "dead_fields_baseline.txt"

# Eine Feld-Deklaration in einem Header: Typ, Name, Semikolon.
#
# MF-1163: der Trenner zwischen Typ und Name ist PFLICHT — Leerzeichen oder
# Stern. Er stand als `\s*\**\s*` und durfte damit LEER sein, und das war
# der ganze Fehler: bei `unsigned populated;` verbraucht der optionale
# Praefix `unsigned ` bereits, danach muss der Typteil in den Namen
# hineinfressen. Eine Regel gibt beim Zuruecksetzen den SPAETEREN Quantor
# zuerst auf, also fand sie `popula` + `ted` und war fertig, bevor sie den
# optionalen Praefix je in Frage stellte.
#
# Gemessen: 19 Zeilen dieser Gestalt liegen in `include/`, elf davon traten
# als ERFUNDENER toter Feldname an die Oberflaeche — `ted` (populated),
# `de0`/`de1` (populated_side0/1), `irs` (aliased_pairs), `nge`
# (out_of_range), `dht` (below_tdht), `unt` (head_count), `ead`
# (first_head), `ers` (cylinders), `ull`/`ins` (max_null/max_eins in
# `uft_zellregel.h`). Hinter JEDEM der elf liegt ein Feld, das wirklich
# geschrieben wird — 11 Fehlalarme, 0 verdeckte echte Befunde.
#
# Das ist die Gestalt von MF-867 (dort fehlten `++`/`--`) mit umgekehrtem
# Vorzeichen: dort meldete das Tor Fehlalarm, weil es eine Schreibweise
# nicht kannte, hier, weil es einen Namen falsch LAS. Ein Tor, das Namen
# nennt, die es im Baum gar nicht gibt, ist an der teuersten Stelle
# ungenau — man sucht dann nach dem falschen Feld.
#
# Was diese Fassung ausdruecklich NICHT repariert: `volatile` fehlt im
# Praefix (gemessen genau eine Fundstelle, `volatile bool* cancel;`, von
# beiden Fassungen schlicht uebersehen — kein Phantom), und Namen unter
# drei Zeichen bleiben draussen. Beides ist gemessen und nicht blind
# mitgeweitet.
DEKL = re.compile(
    r"^\s*(?:const\s+)?(?:struct\s+|union\s+|enum\s+|unsigned\s+|signed\s+)?"
    r"[A-Za-z_][A-Za-z0-9_]*(?:\s*\*+\s*|\s+)"
    r"([a-z_][a-z0-9_]{2,})\s*(?:\[[^\]]*\])?\s*;"
)
# Was keine Felddeklaration ist.
KEIN_FELD = re.compile(r"\b(return|typedef|extern|static|void|\(|\))")


def dateien(muster: str) -> list[Path]:
    try:
        aus = subprocess.run(
            ["git", "ls-files", "--cached", "--others", "--exclude-standard",
             muster],
            cwd=WURZEL, capture_output=True, text=True, timeout=120)
        if aus.returncode != 0:
            raise RuntimeError(aus.stderr)
        return [WURZEL / z for z in aus.stdout.splitlines() if z.strip()]
    except Exception as e:                                  # noqa: BLE001
        # Grundsatz MF-636: lieber alles durchlassen UND es sagen.
        print(f"  WARNUNG: git nicht befragbar ({e}) — Tor laesst durch",
              file=sys.stderr)
        return []


def felder_aus_headern(hdr: list[Path]) -> dict[str, list[str]]:
    """Feldname -> Fundstellen."""
    gefunden: dict[str, list[str]] = {}
    for p in hdr:
        try:
            zeilen = p.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError:
            continue
        in_struct = 0
        for nr, z in enumerate(zeilen, 1):
            roh = z.split("/*")[0].split("//")[0]
            if re.search(r"\b(struct|union)\b[^;]*\{", roh):
                in_struct += 1
            if in_struct and "}" in roh:
                in_struct = max(0, in_struct - roh.count("}"))
                continue
            if not in_struct or KEIN_FELD.search(roh):
                continue
            m = DEKL.match(roh)
            if m:
                try:
                    wo = p.relative_to(WURZEL).as_posix()
                except ValueError:
                    wo = p.name          # Selbsttest laeuft ausserhalb
                gefunden.setdefault(m.group(1), []).append(f"{wo}:{nr}")
    return gefunden


def geschriebene_namen(quellen: list[Path]) -> set[str]:
    """Jeder Name, der irgendwo links von '=' hinter '.' oder '->' steht.

    MF-867: `++` und `--` gehoeren dazu. Sie fehlten, und der Fall ist
    lehrreich — ein ZAEHLER wird typischerweise genau so geschrieben.
    Aufgefallen an `pll->clamp_hits++` (MF-866): das Feld galt als tot,
    obwohl es an der einzigen sinnvollen Stelle beschrieben wird.

    Ein Tor, das die haeufigste Schreibweise fuer die haeufigste Art
    toter Kandidaten nicht kennt, meldet genau dort Fehlalarm, wo man
    ihm glauben moechte.
    """
    zuw = re.compile(r"(?:\.|->)\s*([a-z_][a-z0-9_]*)\s*(?:\[[^\]]*\])?\s*"
                     r"(?:=[^=]|\+=|-=|\|=|&=|\^=|\+\+|--)")
    # Auch designierte Initialisierer: { .feld = ... }
    des = re.compile(r"\.\s*([a-z_][a-z0-9_]*)\s*=")
    namen: set[str] = set()
    for p in quellen:
        try:
            t = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        namen.update(zuw.findall(t))
        namen.update(des.findall(t))
    return namen


def adressierte_namen(quellen: list[Path]) -> set[str]:
    """Jeder Name, der irgendwo als `&x->name` oder `&x.name` auftaucht.

    MF-1163. Ein Feld kann als AUS-PARAMETER gefuellt werden, und dann
    steht sein Name nirgends links von einem `=`:

        uft_scp_check_integrity(..., &out->checksum_stored, ...)

    Fuer `geschriebene_namen()` ist das Feld tot, obwohl es an der einzigen
    sinnvollen Stelle gefuellt wird. Dieselbe Gestalt wie MF-866/MF-867,
    wo `++` fehlte — nur laesst sich diese Luecke NICHT einfach schliessen:
    eine Adressuebergabe beweist keinen Schreibvorgang, die Funktion
    dahinter darf auch nur lesen.

    Deshalb wird hier nichts abgezogen. Die Zahl wird GETRENNT
    ausgewiesen, genau wie die `_`-praefigierte Fuellung — sie benennt die
    Groesse der Unsicherheit in der Hauptzahl, statt sie stillschweigend
    zu verrechnen. Gemessen am 2026-09-15: 79 der 1161.
    """
    adr = re.compile(r"&\s*[A-Za-z_][A-Za-z0-9_]*\s*(?:->|\.)\s*"
                     r"([a-z_][a-z0-9_]*)")
    namen: set[str] = set()
    for p in quellen:
        try:
            namen.update(adr.findall(
                p.read_text(encoding="utf-8", errors="replace")))
        except OSError:
            continue
    return namen


def messen() -> tuple[list[tuple[str, str]], int, set[str]]:
    hdr = dateien("include/*.h")
    quell = (dateien("src/*.c") + dateien("src/*.cpp")
             + dateien("tests/*.c") + dateien("tests/*.cpp")
             + dateien("include/*.h"))          # inline-Setter in Headern
    if not hdr:
        return [], 0, set()
    deklariert = felder_aus_headern(hdr)
    geschrieben = geschriebene_namen(quell)
    adressiert = adressierte_namen(quell)
    tot = sorted((n, s[0]) for n, s in deklariert.items()
                 if n not in geschrieben)
    return tot, len(deklariert), adressiert


def selbsttest() -> bool:
    """Vor dem Nenner. Bricht bei roter Abnahme ab (Muster uft-innendienst)."""
    ok = 0
    # Die beiden `unsigned`-Felder sind der Rotbeweis zu MF-1163: vor der
    # Korrektur las die Regel sie als `ird` und `abc` — Namen, die es im
    # Baum nicht gibt.
    hdr_text = (
        "typedef struct {\n"
        "    int  wird_gesetzt;\n"
        "    bool nie_gesetzt_xyz;\n"
        "    unsigned gezaehlt_wird;\n"
        "    unsigned nie_gezaehlt_abc;\n"
        "} probe_t;\n")
    src_text = ("void f(probe_t *p) { p->wird_gesetzt = 1;"
                " p->gezaehlt_wird++; }\n")

    import tempfile
    with tempfile.TemporaryDirectory() as d:
        h = Path(d) / "p.h"; h.write_text(hdr_text, encoding="utf-8")
        c = Path(d) / "p.c"; c.write_text(src_text, encoding="utf-8")
        deklariert = felder_aus_headern([h])
        geschrieben = geschriebene_namen([c])

        # 1: das gesetzte Feld wird als Deklaration erkannt
        if "wird_gesetzt" in deklariert:
            ok += 1
        else:
            print("  SELBSTTEST 1 ROT: Deklaration nicht erkannt")
        # 2: das gesetzte Feld gilt als geschrieben
        if "wird_gesetzt" in geschrieben:
            ok += 1
        else:
            print("  SELBSTTEST 1 ROT: Zuweisung nicht erkannt")
        # 3: GEGENBEWEIS — das ungesetzte Feld darf NICHT als
        #    geschrieben gelten, sonst meldet das Tor nie etwas
        if "nie_gesetzt_xyz" in deklariert and "nie_gesetzt_xyz" not in geschrieben:
            ok += 1
        else:
            print("  SELBSTTEST 3 ROT: totes Feld faelschlich als "
                  "geschrieben gewertet — das Tor waere blind")
        # 4: MF-1163 — `unsigned <name>;` wird als EIN Name gelesen, nicht
        #    in Typ und Bruchstueck zerlegt. Vor der Korrektur stand hier
        #    `ird` statt `gezaehlt_wird`; die Abwesenheit des Bruchstuecks
        #    gehoert mitgeprueft, sonst faellt eine Rueckkehr nicht auf.
        if "gezaehlt_wird" in deklariert and "ird" not in deklariert:
            ok += 1
        else:
            print("  SELBSTTEST 4 ROT: `unsigned <name>;` falsch zerlegt — "
                  f"gemeldet wurde {sorted(deklariert)}")
        # 5: GEGENBEWEIS zu 4 — die Korrektur darf das Tor fuer
        #    `unsigned`-Felder nicht blind machen. Ein nie geschriebenes
        #    muss weiterhin als tot gelten.
        if ("nie_gezaehlt_abc" in deklariert
                and "nie_gezaehlt_abc" not in geschrieben):
            ok += 1
        else:
            print("  SELBSTTEST 5 ROT: totes `unsigned`-Feld nicht mehr "
                  "gemeldet — die Korrektur hat das Tor verengt")
    print(f"  Selbsttest {ok}/5")
    return ok == 5


def main() -> int:
    print("audit_dead_fields (MF-831)")
    if not selbsttest():
        print("  ABBRUCH: Selbsttest rot — kein Nenner ohne Abnahme")
        return 2

    tot, gesamt, adressiert = messen()
    # `_pad`/`_reserved` heissen konventionell so, WEIL sie nie geschrieben
    # werden. Das ist eine Regel, keine Aufzaehlung von Faellen — deshalb
    # wird sie getrennt ausgewiesen und nicht stillschweigend abgezogen.
    fuellend = [t for t in tot if t[0].startswith("_")]
    print(f"  Felder in oeffentlichen Headern : {gesamt}")
    print(f"  davon NIRGENDS geschrieben      : {len(tot)}")
    print(f"    darunter _-praefigiert (Fuellung/reserviert): {len(fuellend)}")
    print(f"    verbleibend, also echte Zusagen ohne Einloesung: "
          f"{len(tot) - len(fuellend)}")
    # MF-1163: zweite getrennt ausgewiesene Klasse, nach demselben Grundsatz
    # wie die Fuellung darueber — benennen, nicht abziehen.
    aus_param = [t for t in tot if t[0] in adressiert]
    print(f"    darunter irgendwo als &x->feld uebergeben "
          f"(Aus-Parameter moeglich, NICHT abgezogen): {len(aus_param)}")

    grenze = None
    if GRUNDLINIE.exists():
        for z in GRUNDLINIE.read_text(encoding="utf-8").splitlines():
            z = z.split("#")[0].strip()
            if z.isdigit():
                grenze = int(z)
                break

    if grenze is None:
        print(f"  keine Grundlinie in {GRUNDLINIE.name} — nur Bericht")
        for n, wo in tot[:40]:
            print(f"    {n:<28} {wo}")
        return 0

    print(f"  Grundlinie                      : {grenze}")
    if len(tot) > grenze:
        print(f"  FEHLER: {len(tot)} > {grenze} — die Zahl ist gestiegen.")
        # MF-1163: hier stand "neue tote Felder", und die Liste darunter
        # hiess `neu`. Sie war es nicht. Die Grundlinie ist eine ZAHL,
        # keine Namensliste — WELCHE Felder neu sind, kann dieses Tor
        # ueberhaupt nicht sagen, und die Aufzaehlung war die alphabetisch
        # erste. Das hat einmal eine halbe Sitzung gekostet: gesucht wurde
        # nach `_magic`, `_pad0`, `_pad1`, `_reserved1`, `_reserved2`, die
        # seit Jahren im Baum stehen, waehrend der Zuwachs aus einem ganz
        # anderen Header kam. Eine Beschriftung, die mehr behauptet als die
        # Messung traegt, schickt den Leser an die falsche Stelle.
        print(f"  (die ersten 40 von {len(tot)}, ALPHABETISCH — nicht die "
              f"neuen; die Grundlinie ist eine Zahl)")
        for n, wo in tot[:40]:
            print(f"    {n} ({wo})")
        return 1
    if len(tot) < grenze:
        print(f"  Hinweis: Grundlinie auf {len(tot)} senken.")
    print("  OK")
    return 0


def check(repo=None):
    """Schnittstelle fuer scripts/check_consistency.py.

    Meldet nur den ANSTIEG ueber die Grundlinie — der Bestand selbst ist
    kein Fehler, sondern eine Obergrenze fuer Vertrauen (siehe
    docs/dead_fields_baseline.txt)."""
    global WURZEL, GRUNDLINIE
    if repo:
        WURZEL = Path(repo)
        GRUNDLINIE = WURZEL / "docs" / "dead_fields_baseline.txt"
    if not GRUNDLINIE.exists():
        return []
    grenze = None
    for z in GRUNDLINIE.read_text(encoding="utf-8").splitlines():
        z = z.split("#")[0].strip()
        if z.isdigit():
            grenze = int(z)
            break
    if grenze is None:
        return []
    tot, _, _ = messen()
    if len(tot) <= grenze:
        return []
    # MF-1163: hier stand "neu u.a." — dieselbe Falschbeschriftung wie in
    # main(). Dieses Tor kennt nur eine Zahl als Grundlinie und kann die
    # neuen Namen nicht benennen; es sagt das jetzt.
    return ["%d tote Felder > Grundlinie %d (Zuwachs %+d); erste fuenf "
            "ALPHABETISCH, nicht die neuen: %s"
            % (len(tot), grenze, len(tot) - grenze,
               ", ".join(n for n, _ in tot[:5]))]


if __name__ == "__main__":
    sys.exit(main())
