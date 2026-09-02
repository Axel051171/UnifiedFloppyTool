#!/usr/bin/env python3
"""Der DPB einer CP/M-Definition ist gegen SICH SELBST prüfbar (MF-813).

── Warum dieses Tor ohne Referenz auskommt ──────────────────────────────

`src/formats/cpm/uft_cpm_diskdefs.c` trägt im Kopf:

    Reference: libdsk diskdefs, cpmtools diskdefs (libdsk: LGPL-2.0-or-later,
      Fassung 1.5.12 geprueft; cpmtools NICHT gemessen -- Lizenz offen, LIZ-1)

Eine der beiden Quellen ist geprüft, die andere ausdrücklich nicht — und
aus dieser Ehrlichkeit ist nie eine Folge gezogen worden.

Die Pointe: **für diese Prüfung braucht es überhaupt keine Referenz.**
Der Disk Parameter Block ist kein freies Feld. Seine Werte hängen
arithmetisch voneinander ab, dokumentiert im *CP/M 2.2 Alteration Guide*
und in jeder Referenz seither. Wer die Regeln über die Tabelle laufen
lässt, findet Widersprüche ohne ein einziges Referenzabbild.

── Die vier Regeln ──────────────────────────────────────────────────────

1. **BLM = 2^BSH − 1.** Blockgröße ist 128 << BSH; BLM ist die Maske
   dazu. Rein mechanisch.

2. **BLS/DSM-Kombination zulässig.** Ab `DSM >= 256` werden die
   Blockzeiger im Verzeichniseintrag 16-bittig, es passen nur noch acht
   statt sechzehn hinein. Bei BLS = 1024 fasst ein Extent damit 8 KB —
   und CP/M erlaubt keine Formate, bei denen ein Extent weniger als
   16 KB fasst. Die cpmtools-Dokumentation nennt genau diesen Fall als
   Beispiel. **Das ist kein ungewöhnliches Format, sondern ein
   unmögliches.**

3. **AL0/AL1 passen zu DRM.** Das Verzeichnis braucht
   `ceil((DRM+1) * 32 / BLS)` Blöcke; genau so viele Bits müssen in
   AL0/AL1 gesetzt sein, von oben.

4. **Kapazität.** `(DSM+1) * BLS` plus die Systemspuren darf das
   Medium — aus der im selben Eintrag deklarierten Geometrie — nicht
   überschreiten.

── Was das Tor NICHT tut: EXM erzwingen ─────────────────────────────────

EXM hat einen Tabellenwert aus BLS und DSM, aber eine Abweichung ist
**nicht automatisch falsch**. Chuck Guzis erklärt im CP/M-Forum
(vcfed.org, Thread „Sisyphus CP/M 2.2 and 22DISK"), dass EXM eine
**negative Suchmaske** ist — 0 heißt „alle Bits des Extent-Felds
auswerten", 1 heißt „das unterste ignorieren" — und dass man bewusst vom
Tabellenwert abweicht, um mit älteren CP/M-1.4-Programmen kompatibel zu
bleiben, die eigenes Random-I/O machen und darauf bauen, dass ein Extent
128 Records fasst.

Deshalb ist EXM hier eine **Liste**, kein Tor — dieselbe Form wie bei
den Attributionen (MF-636). Verlangt wird nur, dass eine Abweichung
**begründet dasteht**: ein Kommentar in denselben oder den zwei Zeilen
davor. Ohne ihn ist ein absichtlicher Kompatibilitätswert von einem
Tippfehler nicht mehr zu unterscheiden, und **das** ist der Mangel,
unabhängig davon, wie viele Abweichungen am Ende richtig sind.

── Selbsttest ───────────────────────────────────────────────────────────

Läuft vor jedem Nenner (`--selftest` ist implizit). Ein Tor, dessen
eigene Rechnung niemand prüft, ist eine Behauptung — dieser Baum hat
das bei `tuersucher.py` einmal bezahlt, wo „Selbsttest 3/3" gemeldet und
0/3 geliefert wurde.

Aufruf:
    python scripts/audit_cpm_dpb.py            # Bericht
    python scripts/audit_cpm_dpb.py --strict   # Rückgabe 1 bei hartem Verstoß
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
QUELLE = WURZEL / "src/formats/cpm/uft_cpm_diskdefs.c"

FELDER = ("spt", "bsh", "blm", "exm", "dsm", "drm", "al0", "al1", "off",
          "cylinders", "heads", "sectors", "sector_size")


def zahl(s: str) -> int | None:
    s = s.strip().rstrip(",").strip()
    try:
        return int(s, 0)
    except ValueError:
        return None


def lies_definitionen(text: str):
    """Jede `const cpm_diskdef_t cpm_X = { ... };` als Wörterbuch."""
    for m in re.finditer(
            r"^const\s+cpm_diskdef_t\s+(\w+)\s*=\s*\{(.*?)^\};",
            text, re.S | re.M):
        symbol, koerper = m.group(1), m.group(2)
        zeile0 = text[:m.start()].count("\n") + 1
        d = {"symbol": symbol, "zeile": zeile0, "roh": koerper}
        for f in FELDER:
            mm = re.search(r"\.%s\s*=\s*([^,\n]+)" % f, koerper)
            if mm:
                v = zahl(mm.group(1))
                if v is not None:
                    d[f] = v
        nm = re.search(r'\.name\s*=\s*"([^"]*)"', koerper)
        d["name"] = nm.group(1) if nm else symbol
        yield d


def exm_soll(bls: int, dsm: int) -> int | None:
    """Tabellenwert nach dem CP/M 2.2 Alteration Guide."""
    klein = dsm < 256
    tab = {1024: (0, None), 2048: (1, 0), 4096: (3, 1),
           8192: (7, 3), 16384: (15, 7)}
    if bls not in tab:
        return None
    return tab[bls][0] if klein else tab[bls][1]


def pruefe(d: dict):
    """(harte Befunde, EXM-Liste) für eine Definition."""
    hart, liste = [], []
    if "bsh" not in d or "dsm" not in d:
        return hart, liste
    bls = 128 << d["bsh"]

    # Regel 1 — BLM = 2^BSH - 1
    if "blm" in d and d["blm"] != (1 << d["bsh"]) - 1:
        hart.append("BLM=%d, aus BSH=%d folgt %d"
                    % (d["blm"], d["bsh"], (1 << d["bsh"]) - 1))

    # Regel 2 — BLS/DSM zulaessig
    if d["dsm"] >= 256 and bls == 1024:
        hart.append("BLS=1024 mit DSM=%d: ab DSM>=256 sind die Blockzeiger "
                    "16-bittig, ein Extent fasst dann 8 KB — CP/M verlangt "
                    "mindestens 16 KB. Diese Kombination ist unmoeglich, "
                    "nicht bloss ungewoehnlich" % d["dsm"])

    # Regel 3 — AL0/AL1 gegen DRM
    if "drm" in d and "al0" in d and "al1" in d:
        noetig = -(-((d["drm"] + 1) * 32) // bls)
        gesetzt = bin(d["al0"]).count("1") + bin(d["al1"]).count("1")
        if gesetzt != noetig:
            hart.append("AL0/AL1 setzen %d Bit, DRM=%d braucht bei BLS=%d "
                        "genau %d" % (gesetzt, d["drm"], bls, noetig))

    # Regel 4 — Kapazitaet
    if all(k in d for k in ("cylinders", "heads", "sectors", "sector_size")):
        medium = d["cylinders"] * d["heads"] * d["sectors"] * d["sector_size"]
        daten = (d["dsm"] + 1) * bls
        if medium > 0 and daten > medium:
            hart.append("Datenbereich %d B ueberschreitet das Medium %d B "
                        "(%.2fx) — %d/%d/%d/%d"
                        % (daten, medium, daten / medium, d["cylinders"],
                           d["heads"], d["sectors"], d["sector_size"]))

    # EXM — Liste, kein Tor
    if "exm" in d:
        soll = exm_soll(bls, d["dsm"])
        if soll is not None and d["exm"] != soll:
            begruendet = bool(re.search(r"/\*|//", d["roh"].split("\n")[0])) \
                or "exm" in d["roh"].lower().split(".exm")[0][-160:]
            liste.append(("EXM=%d, Tabelle sagt %d bei BLS=%d/DSM=%d"
                          % (d["exm"], soll, bls, d["dsm"]), begruendet))
    return hart, liste


# ── Selbsttest ───────────────────────────────────────────────────────────
def selftest() -> bool:
    faelle = [
        # (Beschreibung, Definition, muss hart auffallen)
        ("BLM passt nicht zu BSH",
         dict(bsh=3, blm=6, dsm=100, drm=63, al0=0xC0, al1=0), True),
        ("BLS=1024 mit DSM>=256 ist unmoeglich",
         dict(bsh=3, blm=7, dsm=300, drm=63, al0=0xC0, al1=0), True),
        ("AL0/AL1 passen nicht zu DRM",
         dict(bsh=3, blm=7, dsm=100, drm=63, al0=0x80, al1=0), True),
        ("Kapazitaet ueberschritten",
         dict(bsh=4, blm=15, dsm=400, drm=63, al0=0xC0, al1=0,
              cylinders=40, heads=1, sectors=9, sector_size=512), True),
        # Gegenprobe: eine saubere Definition darf NICHT auffallen.
        ("saubere Definition bleibt still",
         dict(bsh=3, blm=7, dsm=242, drm=63, al0=0xC0, al1=0,
              cylinders=77, heads=1, sectors=26, sector_size=128), False),
    ]
    ok = 0
    for text, d, soll_hart in faelle:
        d["roh"] = ""
        hart, _ = pruefe(d)
        traf = bool(hart)
        if traf == soll_hart:
            ok += 1
        else:
            print("  SELBSTTEST FEHLGESCHLAGEN: %s -> %s"
                  % (text, hart or "(still)"))
    print("  Selbsttest %d/%d" % (ok, len(faelle)))
    return ok == len(faelle)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--strict", action="store_true",
                    help="Rueckgabe 1, wenn ein harter Verstoss vorliegt")
    args = ap.parse_args()

    print("CP/M-DPB-Pruefung (MF-813) — arithmetisch, ohne Referenz")
    if not selftest():
        print("ABBRUCH: der Pruefer besteht seinen eigenen Test nicht.")
        return 2

    if not QUELLE.exists():
        print("  %s fehlt" % QUELLE)
        return 0
    text = QUELLE.read_text(encoding="utf-8", errors="replace")
    defs = list(lies_definitionen(text))

    hart_gesamt, exm_gesamt, exm_ohne_grund = 0, 0, 0
    for d in defs:
        hart, liste = pruefe(d)
        for h in hart:
            hart_gesamt += 1
            print("  [HART] %s (Z. %d): %s" % (d["symbol"], d["zeile"], h))
        for text_, begruendet in liste:
            exm_gesamt += 1
            if not begruendet:
                exm_ohne_grund += 1
                print("  [EXM ] %s (Z. %d): %s — ohne Begruendung"
                      % (d["symbol"], d["zeile"], text_))

    print("\n  Definitionen        : %d" % len(defs))
    print("  harte Verstoesse    : %d" % hart_gesamt)
    print("  EXM-Abweichungen    : %d, davon %d ohne Begruendung"
          % (exm_gesamt, exm_ohne_grund))
    print("\n  EXM ist bewusst eine LISTE, kein Tor: eine Abweichung kann")
    print("  absichtlich sein (CP/M-1.4-Kompatibilitaet). Verlangt ist nur,")
    print("  dass sie BEGRUENDET dasteht — sonst ist Absicht von Tippfehler")
    print("  nicht mehr zu unterscheiden.")
    return 1 if (args.strict and hart_gesamt) else 0


if __name__ == "__main__":
    raise SystemExit(main())


# ── Als Tor, mit eingefrorenem Bestand (MF-813) ──────────────────────────
#
# 13 harte Verstoesse stehen im Baum. Sie sofort zum Tor zu machen hiesse,
# die CI rot zu lassen, bis jemand 13 Definitionen nachrechnet — und ein
# dauerrotes Tor wird abgeschaltet. Deshalb dasselbe Muster wie beim
# Format-Freeze (`scripts/format_freeze_baseline.json`): der BESTAND ist
# eingefroren, ein VIERZEHNTER faellt sofort auf.
#
# Die eingefrorene Zahl ist keine Absolution. Sie steht hier, damit
# sichtbar bleibt, wie viele es sind — und damit sie nur nach unten
# gehen kann.
BESTAND_HART = 13

def check(repo: Path | None = None):
    """Schnittstelle fuer scripts/check_consistency.py."""
    wurzel = Path(repo) if repo else WURZEL
    quelle = wurzel / "src/formats/cpm/uft_cpm_diskdefs.c"
    if not quelle.exists():
        return []
    text = quelle.read_text(encoding="utf-8", errors="replace")
    hart = []
    for d in lies_definitionen(text):
        h, _ = pruefe(d)
        for x in h:
            hart.append("%s (%s:%d): %s"
                        % (d["symbol"], quelle.name, d["zeile"], x))
    if len(hart) > BESTAND_HART:
        neu = len(hart) - BESTAND_HART
        return hart[-neu:] + [
            "%d harte DPB-Verstoesse, eingefroren waren %d. Der DPB ist "
            "gegen sich selbst pruefbar — eine neue Definition muss "
            "rechnen, bevor sie hereinkommt." % (len(hart), BESTAND_HART)]
    if len(hart) < BESTAND_HART:
        return ["BESTAND_HART in scripts/audit_cpm_dpb.py steht auf %d, "
                "gemessen sind nur noch %d. Bitte die Zahl senken — sie "
                "darf nur nach unten." % (BESTAND_HART, len(hart))]
    return []
