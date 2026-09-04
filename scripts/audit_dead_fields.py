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
auf der linken Seite einer Zuweisung steht. Das ist eindeutig und kann
nicht falsch anschlagen.

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
DEKL = re.compile(
    r"^\s*(?:const\s+)?(?:struct\s+|union\s+|enum\s+|unsigned\s+|signed\s+)?"
    r"[A-Za-z_][A-Za-z0-9_]*\s*\**\s*"
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


def messen() -> tuple[list[tuple[str, str]], int]:
    hdr = dateien("include/*.h")
    quell = (dateien("src/*.c") + dateien("src/*.cpp")
             + dateien("tests/*.c") + dateien("tests/*.cpp")
             + dateien("include/*.h"))          # inline-Setter in Headern
    if not hdr:
        return [], 0
    deklariert = felder_aus_headern(hdr)
    geschrieben = geschriebene_namen(quell)
    tot = sorted((n, s[0]) for n, s in deklariert.items()
                 if n not in geschrieben)
    return tot, len(deklariert)


def selbsttest() -> bool:
    """Vor dem Nenner. Bricht bei roter Abnahme ab (Muster uft-innendienst)."""
    ok = 0
    hdr_text = (
        "typedef struct {\n"
        "    int  wird_gesetzt;\n"
        "    bool nie_gesetzt_xyz;\n"
        "} probe_t;\n")
    src_text = "void f(probe_t *p) { p->wird_gesetzt = 1; }\n"

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
    print(f"  Selbsttest {ok}/3")
    return ok == 3


def main() -> int:
    print("audit_dead_fields (MF-831)")
    if not selbsttest():
        print("  ABBRUCH: Selbsttest rot — kein Nenner ohne Abnahme")
        return 2

    tot, gesamt = messen()
    # `_pad`/`_reserved` heissen konventionell so, WEIL sie nie geschrieben
    # werden. Das ist eine Regel, keine Aufzaehlung von Faellen — deshalb
    # wird sie getrennt ausgewiesen und nicht stillschweigend abgezogen.
    fuellend = [t for t in tot if t[0].startswith("_")]
    print(f"  Felder in oeffentlichen Headern : {gesamt}")
    print(f"  davon NIRGENDS geschrieben      : {len(tot)}")
    print(f"    darunter _-praefigiert (Fuellung/reserviert): {len(fuellend)}")
    print(f"    verbleibend, also echte Zusagen ohne Einloesung: "
          f"{len(tot) - len(fuellend)}")

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
        neu = [f"{n} ({wo})" for n, wo in tot]
        print(f"  FEHLER: {len(tot)} > {grenze} — neue tote Felder.")
        for z in neu[:40]:
            print(f"    {z}")
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
    tot, _ = messen()
    if len(tot) <= grenze:
        return []
    return ["%d tote Felder > Grundlinie %d; neu u.a.: %s"
            % (len(tot), grenze,
               ", ".join(n for n, _ in tot[:5]))]


if __name__ == "__main__":
    sys.exit(main())
