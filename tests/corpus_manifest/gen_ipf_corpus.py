#!/usr/bin/env python3
"""IPF von FREMDER Hand — das Rezept zu MF-1372 (P3-360 Teil 1, P3-361).

Bis MF-1372 lag im freien Korpus genau EINE IPF, `hxcfe_ibmdd.ipf`, und
die ist hohl: 168 IMGE-Saetze, alle mit trackbytes=0 (MF-1073). Die zwei
echten SPS-Abbilder liegen im EINGESCHRAENKTEN Korpus und sind SPS-kodiert.
Eine Sektorebene liess sich damit nirgends abnehmen.

── Der Erzeuger ─────────────────────────────────────────────────────────

`disk-analyse` aus Keir Frasers disk-utilities (github.com/keirf/
disk-utilities, Quellstand 5e690f3a; Lizenz Unlicense / public domain, am
`COPYING` im Klon gemessen). Sein IPF-Schreiber `libdisk/container/ipf.c`
(„Written in 2011 by Keir Fraser“) ist unabhaengig von AIR, aus dem
`uft_ipf_air.c` portiert ist — eine fremde Hand im Sinne von MF-1135. Er
schreibt standardmaessig den CAPS-Kodierer (encoder 1) und stempelt das
INFO-Feld mit `crc32("User IPF")` (0x843265bb), damit solche Dateien nie
mit SPS-Erhaltungen verwechselt werden — das Rezept laesst das Feld, wie
es ist.

Bau unter Linux: `make` im Klon (gcc, erster Versuch rc 0). Aufruf mit
`LD_LIBRARY_PATH=<klon>/libdisk`.

── Die Eingabe ──────────────────────────────────────────────────────────

UFT-eigen und selbstbenennend: jeder Sektor traegt `UFT-K Ccc Hh Sss `
alle 17 Byte (dieselbe Marke wie gen_adf_ext_corpus.py). Ohne Inhalt waere
ein Abbild moeglich, das Erfolg meldet und nichts belegt (MF-1021).

  Amiga  : 80 x 2 x 11 x 512, AmigaDOS, Sektoren ab 0
  PC 720K: 80 x 2 x  9 x 512, IBM-MFM (Format `ibm_pc_dd`), Sektoren ab 1

Eingecheckt werden nur die Zylinder 0..3 (`-e 3`, je ~70 KB); die
Messung an den vollen 80 Zylindern macht `--voll` und der Test mit
`test_ipf_sektorebene <verzeichnis> 80`.

── Determinismus ────────────────────────────────────────────────────────

Gemessen an zwei Laeufen: die Ausgaben unterscheiden sich in genau zwoelf
Byte — Datum und Uhrzeit im INFO-Satz (Versatz 64..71) und dessen CRC
(20..23). `--vergleich` prueft alles uebrige byteweise gegen die
eingecheckte Datei.

Aufruf:
    python tests/corpus_manifest/gen_ipf_corpus.py <disk-utilities-klon> <ausgabe> [--voll] [--vergleich]
"""
from __future__ import annotations

import hashlib
import os
import subprocess
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parents[2]
KORPUS = WURZEL / "tests" / "corpus_free"
ZEITFELDER = set(range(20, 24)) | set(range(64, 72))


def marke(c: int, h: int, s: int) -> bytes:
    return ("UFT-K C%02d H%d S%02d " % (c, h, s)).encode("ascii")


def sektor(c: int, h: int, s: int) -> bytes:
    m = marke(c, h, s)
    return (m * (512 // len(m) + 1))[:512]


def abbild(zylinder: int, spt: int, erste: int) -> bytes:
    return b"".join(sektor(c, h, s) for c in range(zylinder) for h in range(2)
                    for s in range(erste, erste + spt))


def lauf(klon: Path, argv: list[str]) -> None:
    env = dict(os.environ, LD_LIBRARY_PATH=str(klon / "libdisk"))
    da = klon / "disk-analyse" / "disk-analyse"
    formate = klon / "disk-analyse" / "formats"
    r = subprocess.run([str(da), "-c", str(formate)] + argv, env=env,
                       capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(f"ABBRUCH: disk-analyse rc {r.returncode}\n{r.stderr}")


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    klon = Path(sys.argv[1]).resolve()
    aus = Path(sys.argv[2]).resolve()
    voll = "--voll" in sys.argv
    vergleich = "--vergleich" in sys.argv
    aus.mkdir(parents=True, exist_ok=True)
    zyl = 80 if voll else 4

    adf = aus / "uftk_amiga.adf"
    img = aus / "uftk_pcdd.img"
    adf.write_bytes(abbild(80, 11, 0))
    img.write_bytes(abbild(zyl, 9, 1))

    ziele = {
        "disk_analyse_uftk_amiga.ipf": ["-e", str(zyl - 1), str(adf)],
        "disk_analyse_uftk_pcdd.ipf": ["-e", str(zyl - 1), "-f", "ibm_pc_dd",
                                       str(img)],
    }
    fehler = 0
    for name, argv in ziele.items():
        ziel = aus / name
        lauf(klon, argv + [str(ziel)])
        roh = ziel.read_bytes()
        print(f"{name}: {len(roh)} Byte, sha256 {hashlib.sha256(roh).hexdigest()}")
        if vergleich and not voll:
            ein = (KORPUS / name).read_bytes()
            abw = [i for i in range(max(len(roh), len(ein)))
                   if i >= len(roh) or i >= len(ein) or roh[i] != ein[i]]
            rest = [i for i in abw if i not in ZEITFELDER]
            print(f"  gegen den Korpus: {len(abw)} Byte verschieden, davon "
                  f"{len(rest)} ausserhalb Datum/Uhrzeit/INFO-CRC")
            fehler += 1 if rest else 0
    return 1 if fehler else 0


if __name__ == "__main__":
    sys.exit(main())
