#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
Erzeugt pathologische SCP-Varianten aus einer sauberen Aufnahme.

Zweck: Testmaterial fuer uft_scp_integrity und fuer den SCP-Parser. Die
Varianten bilden Faelle nach, die in freier Wildbahn vorkommen und die UFT
heute stillschweigend akzeptiert.

    python3 make_scp_pathological.py sauber.scp -o tests/corpus_free/

Erzeugte Dateien:

  <name>.sideb-only.scp     Seite-0-Eintraege geleert, Kopfbyte 0x0A = 2.
                            Eine EHRLICHE einseitige Rueckseitenaufnahme.
                            Pruefsumme wird korrekt neu gebildet.

  <name>.scpmod.scp         Die Umformung von SCPmodSideB.py: Seite-B-Offset
                            in beide Steckplaetze, Pruefsumme NICHT erneuert.
                            Das ist die Datei, die luegt.

  <name>.sideb-undeclared.scp  Wie sideb-only, aber Kopfbyte 0x0A bleibt 0.
                            Kopf und Tabelle widersprechen sich.

  <name>.badsum.scp         Ein Byte der Nutzlast gekippt, Pruefsumme
                            unveraendert. Einfachster Manipulationsnachweis.

  <name>.nosum.scp          Pruefsummenfeld auf 0. Darf KEINEN Fehlalarm
                            ausloesen.

Jede Variante bekommt eine .txt mit dem erwarteten Befund daneben, damit der
Testlauf gegen Sollwerte prueft statt gegen sich selbst.
"""

import argparse
import os
import sys

HEADER_SIZE = 16
MAX_TRACKS = 168
TDHT_OFFSET = HEADER_SIZE
TDHT_SIZE = MAX_TRACKS * 4
MIN_SIZE = TDHT_OFFSET + TDHT_SIZE

OFF_HEADS = 0x0A
OFF_CHECKSUM = 0x0C


def compute_checksum(buf: bytearray) -> int:
    """32-Bit-Summe ueber alle Bytes ab 0x10 bis Dateiende, mit Ueberlauf."""
    return sum(buf[HEADER_SIZE:]) & 0xFFFFFFFF


def read_u32(buf, off):
    return int.from_bytes(buf[off:off + 4], "little")


def write_u32(buf, off, val):
    buf[off:off + 4] = (val & 0xFFFFFFFF).to_bytes(4, "little")


def seal(buf: bytearray) -> bytearray:
    write_u32(buf, OFF_CHECKSUM, compute_checksum(buf))
    return buf


def load(path: str) -> bytearray:
    with open(path, "rb") as fh:
        buf = bytearray(fh.read())
    if len(buf) < MIN_SIZE:
        sys.exit(f"{path}: zu klein fuer Kopf und Spurtabelle ({len(buf)} Byte)")
    if buf[:3] != b"SCP":
        sys.exit(f"{path}: keine SCP-Signatur")
    return buf


def tdht_slot(cyl, head):
    return TDHT_OFFSET + (cyl * 2 + head) * 4


# ───────────────────────────── Varianten ─────────────────────────────────

def var_sideb_only(src: bytearray) -> bytearray:
    """Ehrliche einseitige Rueckseitenaufnahme."""
    out = bytearray(src)
    for cyl in range(MAX_TRACKS // 2):
        write_u32(out, tdht_slot(cyl, 0), 0)
    out[OFF_HEADS] = 2          # nur Seite 1
    return seal(out)


def var_sideb_undeclared(src: bytearray) -> bytearray:
    """Wie oben, aber der Kopf behauptet weiterhin 'beide Seiten'."""
    out = bytearray(src)
    for cyl in range(MAX_TRACKS // 2):
        write_u32(out, tdht_slot(cyl, 0), 0)
    out[OFF_HEADS] = 0          # luegt, aber nur ungenau
    return seal(out)


def var_scpmod(src: bytearray) -> bytearray:
    """Exakt die Umformung von SCPmodSideB.py — Pruefsumme bleibt stehen."""
    out = bytearray(src)
    for cyl in range(MAX_TRACKS // 2):
        b = read_u32(out, tdht_slot(cyl, 1))
        write_u32(out, tdht_slot(cyl, 0), b)
    return out                  # bewusst KEIN seal()


def var_badsum(src: bytearray) -> bytearray:
    """Ein Byte der Nutzlast gekippt, Pruefsumme unveraendert."""
    out = bytearray(src)
    if len(out) > MIN_SIZE:
        out[-1] ^= 0xFF
    return out                  # bewusst KEIN seal()


def var_nosum(src: bytearray) -> bytearray:
    """Pruefsummenfeld geleert — gueltiger Zustand, kein Befund."""
    out = bytearray(src)
    write_u32(out, OFF_CHECKSUM, 0)
    return out


VARIANTS = [
    ("sideb-only", var_sideb_only,
     "Pruefsumme OK; nur Seite 1 belegt; Kopfbyte 0x0A = 2; kein Widerspruch; "
     "keine Manipulation."),
    ("sideb-undeclared", var_sideb_undeclared,
     "Pruefsumme OK; nur Seite 1 belegt; Kopfbyte 0x0A = 0; Widerspruch "
     "Kopf/Tabelle MUSS gemeldet werden; keine Manipulation."),
    ("scpmod", var_scpmod,
     "Pruefsumme FALSCH; Tabelle systematisch aliasiert; Datei gilt als "
     "veraendert. Beide Detektoren muessen unabhaengig anschlagen."),
    ("badsum", var_badsum,
     "Pruefsumme FALSCH; Tabelle sauber; Datei gilt als veraendert."),
    ("nosum", var_nosum,
     "Pruefsummenfeld 0 = nicht gebildet; KEIN Fehlalarm; keine Manipulation."),
]


def describe(buf: bytearray) -> str:
    pop0 = sum(1 for c in range(MAX_TRACKS // 2)
               if read_u32(buf, tdht_slot(c, 0)))
    pop1 = sum(1 for c in range(MAX_TRACKS // 2)
               if read_u32(buf, tdht_slot(c, 1)))
    alias = sum(1 for c in range(MAX_TRACKS // 2)
                if read_u32(buf, tdht_slot(c, 0))
                and read_u32(buf, tdht_slot(c, 0)) == read_u32(buf, tdht_slot(c, 1)))
    stored = read_u32(buf, OFF_CHECKSUM)
    actual = compute_checksum(buf)
    return (f"Groesse:            {len(buf)} Byte\n"
            f"Kopfbyte 0x0A:      {buf[OFF_HEADS]}\n"
            f"Spurbereich:        {buf[0x06]}..{buf[0x07]}\n"
            f"Belegt Seite 0:     {pop0}\n"
            f"Belegt Seite 1:     {pop1}\n"
            f"Aliasierte Zylinder:{alias}\n"
            f"Pruefsumme gespeich.:0x{stored:08X}\n"
            f"Pruefsumme berechnet:0x{actual:08X}\n"
            f"Pruefsumme stimmt:  {'ja' if stored == actual else 'NEIN'}"
            f"{' (Feld ist 0 = nicht gebildet)' if stored == 0 else ''}\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("source", help="saubere SCP-Aufnahme")
    ap.add_argument("-o", "--outdir", default=".", help="Zielverzeichnis")
    args = ap.parse_args()

    src = load(args.source)
    base = os.path.splitext(os.path.basename(args.source))[0]
    os.makedirs(args.outdir, exist_ok=True)

    stored = read_u32(src, OFF_CHECKSUM)
    actual = compute_checksum(src)
    if stored not in (0, actual):
        print(f"Hinweis: die Quelldatei traegt bereits eine falsche Pruefsumme "
              f"(0x{stored:08X} statt 0x{actual:08X}).", file=sys.stderr)

    for name, fn, expected in VARIANTS:
        out = fn(src)
        path = os.path.join(args.outdir, f"{base}.{name}.scp")
        with open(path, "wb") as fh:
            fh.write(out)

        note = os.path.join(args.outdir, f"{base}.{name}.expected.txt")
        with open(note, "w", encoding="utf-8") as fh:
            fh.write(f"# Erwarteter Befund fuer {os.path.basename(path)}\n")
            fh.write(f"# Erzeugt von make_scp_pathological.py aus "
                     f"{os.path.basename(args.source)}\n#\n")
            fh.write(f"# {expected}\n\n")
            fh.write(describe(out))

        print(f"{path}  ({len(out)} Byte)")


if __name__ == "__main__":
    main()
