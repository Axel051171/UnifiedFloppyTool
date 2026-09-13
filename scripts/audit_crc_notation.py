#!/usr/bin/env python3
"""Ein CRC-Polynom in der falschen Schreibweise ist ein anderes Polynom (MF-1089).

── Warum dieses Tor ──────────────────────────────────────────────────────

Philip Koopman nennt die Notationsverwechslung die **haeufigste
Fehlerquelle** bei CRCs, und sie ist die unangenehmste Sorte Fehler:
nichts warnt, nichts bricht ab, die Pruefsumme rechnet weiter — nur eben
eine andere.

Dasselbe Polynom erscheint in vier Schreibweisen:

    (0x8810; 0x11021)  <=>  (0x8408; 0x10811)
     └Koopman  └explizit     └reflektiert └refl. explizit

  * **normal**       `0x1021`  — explizit ohne fuehrendes Bit, MSB-first
  * **reflektiert**  `0x8408`  — bitweise gespiegelt, LSB-first
  * **Koopman**      `0x8810`  — impliziter +1-Term, hoechstes Bit weg

Mechanisch entscheidbar ist daran genau eine Zusage, und die prueft
dieses Tor:

    ein Schritt mit `crc << 1` verlangt ein **normales** Polynom
    ein Schritt mit `crc >> 1` verlangt ein **reflektiertes**
    eine **Koopman**-Konstante gehoert in keinen von beiden

Eine Koopman-Zahl ist nur die Eingabe fuer `hdlen`; in einer
Implementierung ist sie immer falsch.

── Was das Tor NICHT sieht, und das gehoert hierher ──────────────────────

* **Tabellengestuetzte Rechner.** Steht das Polynom nur im
  Tabellengenerator, faellt die Tabelle selbst nicht auf. Der Generator
  wird erfasst, die 256 Werte nicht.
* **Init, Reflexion der Ein-/Ausgabe, XorOut.** Das ist der
  Parametersatz, und den entscheidet nicht die Schreibweise des
  Polynoms, sondern der RevEng-Katalog (P3-370).
* **Die Spanne.** Ob die drei `A1`-Synchronbytes mitgerechnet werden,
  ist die zweite klassische Falle — sie sitzt nicht in der Konstanten.
  Gemessen MF-1089: `src/flux/uft_mfm_sector_parser.c:149` und
  `src/flux/uft_flux_decoder.c:1107-1113` machen es richtig und sagen
  den Unterschied zwischen FM und MFM ausdruecklich.
* **Fremdcode im Baum.** `src/samdisk/` und `src/a8rawconv/` sind fremde
  Baeume; ein Befund dort waere keiner von uns. Sie werden ausgenommen,
  und die Ausnahme steht hier statt in einer stillen Liste.

Aufruf:
    python scripts/audit_crc_notation.py            # prueft
    python scripts/audit_crc_notation.py --selftest # Selbsttest
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

WURZEL = Path(__file__).resolve().parent.parent

# Fremde Baeume im eigenen Baum — siehe Kopf.
FREMD = ("src/samdisk/", "src/a8rawconv/")

# Bekannte Polynome je Breite. Quelle fuer die Zuordnung:
# Philip Koopman, "Best CRC Polynomials", CMU,
# https://users.ece.cmu.edu/~koopman/crc/, Abruf 2026-09-13, CC BY 4.0.
# Die Umrechnung ist unten nachgerechnet, nicht abgeschrieben.
NORMAL = {
    16: {0x1021: "CCITT-16", 0x8005: "CRC-16-IBM/ARC", 0x3D65: "DNP/EN-13757"},
    32: {0x04C11DB7: "CRC-32", 0x1EDC6F41: "CRC-32C"},
}


def spiegel(v: int, breite: int) -> int:
    r = 0
    for _ in range(breite):
        r = (r << 1) | (v & 1)
        v >>= 1
    return r


def koopman(v: int, breite: int) -> int:
    """normal -> Koopman: hoechstes Bit setzen, dann einmal nach rechts."""
    return ((v | (1 << breite)) >> 1) & ((1 << breite) - 1)


REFLEKTIERT = {b: {spiegel(p, b): n for p, n in d.items()}
               for b, d in NORMAL.items()}
KOOPMAN = {b: {koopman(p, b): n for p, n in d.items()}
           for b, d in NORMAL.items()}

# Nachgerechnet statt behauptet — faellt sofort auf, wenn jemand die
# Tafel oben anfasst.
assert REFLEKTIERT[16][0x8408] == "CCITT-16"
assert REFLEKTIERT[16][0xA001] == "CRC-16-IBM/ARC"
assert REFLEKTIERT[32][0xEDB88320] == "CRC-32"
assert REFLEKTIERT[32][0x82F63B78] == "CRC-32C"
assert KOOPMAN[16][0x8810] == "CCITT-16"
assert KOOPMAN[32][0x82608EDB] == "CRC-32"

# Ein bitweiser CRC-Schritt: irgendwo eine Verschiebung, irgendwo ein XOR
# mit einer Hex-Konstante. Beides in derselben Zeile ist die Bauform, die
# in diesem Baum 36-mal vorkommt (P3-78).
LINKS = re.compile(r"<<\s*1\b")
RECHTS = re.compile(r">>\s*1\b")
XOR_HEX = re.compile(r"\^\s*\(?\s*(0[xX][0-9a-fA-F]{3,8})[uU]?[lL]*")


def pruefe_zeile(z: str):
    """Gibt (richtung, konstante, name, erwartet) zurueck oder None."""
    m = XOR_HEX.search(z)
    if not m:
        return None
    wert = int(m.group(1), 16)
    breite = 16 if wert <= 0xFFFF else 32
    links, rechts = bool(LINKS.search(z)), bool(RECHTS.search(z))
    if links == rechts:          # beides oder keines: nicht entscheidbar
        return None
    richtung = "<<" if links else ">>"

    if richtung == "<<":
        if wert in NORMAL.get(breite, {}):
            return None
        if wert in REFLEKTIERT.get(breite, {}):
            return (richtung, m.group(1), REFLEKTIERT[breite][wert],
                    "normal (MSB-first verlangt die normale Schreibweise)")
        if wert in KOOPMAN.get(breite, {}):
            return (richtung, m.group(1), KOOPMAN[breite][wert],
                    "normal (Koopman-Notation gehoert nur in `hdlen`)")
    else:
        if wert in REFLEKTIERT.get(breite, {}):
            return None
        if wert in NORMAL.get(breite, {}):
            return (richtung, m.group(1), NORMAL[breite][wert],
                    "reflektiert (LSB-first verlangt die gespiegelte Form)")
        if wert in KOOPMAN.get(breite, {}):
            return (richtung, m.group(1), KOOPMAN[breite][wert],
                    "reflektiert (Koopman-Notation gehoert nur in `hdlen`)")
    return None


def _ist_crc_schritt(z: str) -> bool:
    """Eine Zeile, die ein BEKANNTES Polynom in eindeutiger Richtung
    verschiebt — richtig oder falsch herum."""
    m = XOR_HEX.search(z)
    if not m:
        return False
    wert = int(m.group(1), 16)
    breite = 16 if wert <= 0xFFFF else 32
    if bool(LINKS.search(z)) == bool(RECHTS.search(z)):
        return False
    return any(wert in tafel.get(breite, {})
               for tafel in (NORMAL, REFLEKTIERT, KOOPMAN))


def dateien(wurzel: Path) -> list[Path]:
    import subprocess
    aus = subprocess.run(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard"],
        cwd=wurzel, capture_output=True, text=True)
    if aus.returncode != 0:
        print("HINWEIS: git nicht befragbar — es wird ALLES geprueft",
              file=sys.stderr)
        namen = [str(p.relative_to(wurzel).as_posix())
                 for p in wurzel.rglob("*") if p.is_file()]
    else:
        namen = [z.strip() for z in aus.stdout.split("\n") if z.strip()]
    raus = []
    for rel in namen:
        if not rel.endswith((".c", ".h", ".cpp", ".hpp")):
            continue
        if any(rel.startswith(f) for f in FREMD):
            continue
        raus.append(wurzel / rel)
    return raus


def check(wurzel: Path, zaehler: dict | None = None) -> list[str]:
    """`zaehler` nimmt, wenn gegeben, die Zahl der GEPRUEFTEN Schritte
    auf. Ohne sie waere „0 Befunde" nicht von „nichts angesehen" zu
    unterscheiden — genau die Lage aus MF-1000 / Tor 64."""
    befunde = []
    for p in dateien(wurzel):
        try:
            text = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for nr, z in enumerate(text.splitlines(), 1):
            if zaehler is not None and _ist_crc_schritt(z):
                zaehler["schritte"] = zaehler.get("schritte", 0) + 1
            treffer = pruefe_zeile(z)
            if not treffer:
                continue
            richtung, konst, name, erwartet = treffer
            try:
                rel = p.relative_to(wurzel).as_posix()
            except ValueError:
                rel = str(p)
            befunde.append(
                f"{rel}:{nr}: `{konst}` ist {name} in der falschen "
                f"Schreibweise — der Schritt verschiebt `{richtung} 1`, "
                f"erwartet ist {erwartet}")
    return befunde


# ── Selbsttest ──────────────────────────────────────────────────────────
#
# Ein Tor, das nicht feuert, beweist nichts (MF-1000). Jede Probe pflanzt
# genau einen Fall; die stillen Faelle sind die Gegenprobe.

_FAELLE = [
    ("MSB-first mit normalem Polynom -> still",
     "crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);", False),
    ("LSB-first mit reflektiertem Polynom -> still",
     "crc = (crc & 1) ? ((crc >> 1) ^ 0x8408) : (crc >> 1);", False),
    ("MSB-first mit REFLEKTIERTEM Polynom",
     "crc = (crc & 0x8000) ? (crc << 1) ^ 0x8408 : (crc << 1);", True),
    ("LSB-first mit NORMALEM Polynom",
     "crc = (crc & 1) ? ((crc >> 1) ^ 0x1021) : (crc >> 1);", True),
    ("MSB-first mit KOOPMAN-Notation",
     "crc = (crc & 0x8000) ? (crc << 1) ^ 0x8810 : (crc << 1);", True),
    ("CRC-32 LSB-first, reflektiert -> still",
     "crc = (crc >> 1) ^ (0xEDB88320u & mask);", False),
    ("CRC-32 LSB-first mit normalem Polynom",
     "crc = (crc >> 1) ^ 0x04C11DB7u;", True),
    ("CRC-32C MSB-first mit reflektiertem Polynom",
     "crc = (crc << 1) ^ 0x82F63B78;", True),
    ("Tabellenzugriff ohne Verschiebungsrichtung -> still",
     "crc = tab[(crc >> 8) ^ *buf++] ^ (crc << 8);", False),
    ("fremde Konstante -> still",
     "hash = (hash << 1) ^ 0xDEADBEEF;", False),
]


def _selbsttest() -> int:
    ok = 0
    for titel, zeile, soll in _FAELLE:
        feuert = pruefe_zeile(zeile) is not None
        gut = feuert == soll
        ok += gut
        print("  %-46s %-7s%s" % (
            titel, "feuert" if feuert else "still",
            "" if gut else "  <- erwartet: " + ("feuert" if soll else "still")))
    print("Selbsttest %d/%d" % (ok, len(_FAELLE)))
    return 0 if ok == len(_FAELLE) else 1


def main() -> int:
    if "--selftest" in sys.argv:
        return _selbsttest()
    zaehler: dict = {}
    befunde = check(WURZEL, zaehler)
    for b in befunde:
        print("  " + b)
    print("CRC-Schreibweise: %d Befunde bei %d geprueften Schritten"
          % (len(befunde), zaehler.get("schritte", 0)))
    return 1 if befunde else 0


if __name__ == "__main__":
    raise SystemExit(main())
