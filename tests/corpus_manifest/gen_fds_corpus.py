#!/usr/bin/env python3
"""Famicom-Disketten-Abbild von FREMDER Hand — das Rezept zu MF-1226.

`fds` stand seit MF-1038 auf T2. Der Leser war dort abgenommen (zwei
Seiten nesdev-Wiki als *Spec*, MAMEs `nes_dsk.cpp` als zweite Hand) und
vier Befunde behoben, darunter die **36 erfundenen Byte je Seite**: der
letzte virtuelle Sektor traegt 65 500 - 127 x 512 = **476** Byte, gemeldet
wurden 512. Die Begruendung fuer T2 war benannt: „im Baum liest oder
schreibt KEIN Werkzeug FDS".

── Zwei Werkzeuge, zwei Lizenzlagen, und eine Berichtigung ───────────

  1. `fdtc` + `bintofdf` aus `segaloco/fdtc` — **BSD-3-Clause**,
     „Copyright 2024 Matthew Gilmore", am `COPYING` IM PAKET gemessen
     (1457 Byte). `bintofdf(1)` setzt einen Typ-3-Dateikopf auf ein
     Typ-4-Blob, `fdtc(1)` baut daraus ein Abbild mit Typ-1- und
     Typ-2-Kopf. **Der ganze Behaelter kommt vom Werkzeug.**
  2. `fdstool` (`rhester72/fdstool`) — **KEINE Lizenz**. Eigentuemer-
     Entscheidung vom 2026-09-17: „fds auch, ja machen". Gehandhabt wie
     `dtc`/`epstool`: ausfuehren ja, weitergeben nein. Hier setzt es den
     16-Byte-fwNES-Kopf und liest zur Gegenprobe.

  **BERICHTIGT:** `P3-474` sagte „alle drei ohne jede Lizenz". Das war
  falsch — gemessen war das **API-Feld** (`license` der
  Projekteinstellung), nicht die **Datei**. Dieselbe Falle wie LisaEms
  „NOASSERTION". Wer eine Lizenz feststellt, liest die Datei.

── Abstammung: nicht dieselbe Hand (MF-644) ──────────────────────────

Weder `fdtc` noch `fdstool` stammen aus dem nesdev-Wiki oder MAME. Das
ist NICHT die Falle aus MF-1135 (`dms`), wo UFTs Leser und hxcfes Leser
beide aus xDMS kamen.

── Was belegt ist — und was nicht ────────────────────────────────────

`fdtc` modelliert wirklich: Blockkennungen 1/2/3/4, der 14-Byte-Text
`*NINTENDO-HVC*`, Showa-Datum, Dateizahl, je Datei Nummer, Name,
Ladeadresse, Groesse und Art. Die Rechnung geht auf:
56 + 2 + 2 x (17 + 8192) = **16 476** Byte Bloecke.

**Die Polsterung auf 65 500 Byte ist UFT-eigen** (Nullbytes) — aber die
ZAHL ist nicht geraten: `fdstool.c:19` definiert `FDS_LENGTH 65500`, und
`fdstool` WEIST die ungepolsterte 16 476-Byte-Datei ab („not in qd/fds
format"). Zwei unabhaengige Umsetzungen, dieselbe Zahl.

── Ein Befund am Orakel, gemessen am Quelltext ───────────────────────

`fdstool` meldet beide Dateinamen als „UFT", im Abbild stehen `UFTK0`
und `UFTK1`. Grund: `fdstool.c:641` laeuft `for (x = 0; x < 3; x++)`
ueber das Namensfeld, das **8 Byte** hat — die Schranke 3 ist die des
DISKETTEN-Namens. Alle Strukturzahlen stimmen; nur die Namensanzeige ist
kaputt. Ein Orakel ist eine Referenz, kein Beweis (MF-1015), und der
Test prueft die Namen deshalb selbst.

── Eine Umgebungshilfe, und warum sie das Werkzeug nicht anfasst ─────

`fdtc.sh`/`bintofdf.sh` brauchen `bc`, und diese Umgebung hat keines
(gemessen: `command -v bc` leer). Das Werkzeug zu PATCHEN waere die
falsche Antwort — dann kaeme das Erzeugnis von einer veraenderten
Fassung, und die „fremde Hand" waere gemischt. Dieses Skript legt
deshalb einen **winzigen `bc`-Ersatz** an, der AUSSCHLIESSLICH
`obase=16;N` und `ibase=16;X` kennt und alles andere mit rc 2 abweist.
Er beruehrt nur die Zahlenbasis (Dateizahl im Typ-2-Block, Ladeadresse
im Typ-3-Kopf); die FDS-Struktur sieht er nie.

Aufruf:
    python tests/corpus_manifest/gen_fds_corpus.py \
        <fdtc.sh> <bintofdf.sh> <fdstool.exe>

Beschaffung:
    git clone https://gitlab.com/segaloco/fdtc      # BSD-3-Clause
    git clone https://github.com/rhester72/fdstool  # keine Lizenz
    gcc -O2 -o fdstool fdstool/fdstool.c            # rc 0, zwei Warnungen
"""
from __future__ import annotations

import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

DATEIEN = 2
BROCKEN = 16
SS = 512
NUTZ_LEN = BROCKEN * SS          # 8192
SEITE = 65500
KOPF_LEN = 16
GROESSE = KOPF_LEN + SEITE       # 65 516

ZIEL = "fdtc_fdstool_uftk.fds"

BC_ERSATZ = '''#!/usr/bin/env python
"""Ersatz fuer die zwei `bc`-Aufrufe von fdtc/bintofdf — nur Zahlenbasis.

Kennt AUSSCHLIESSLICH `obase=16;N` und `ibase=16;X`; alles andere wird
mit rc 2 abgewiesen statt geraten. Damit laeuft das BSD-3-Werkzeug
unveraendert, und die FDS-Struktur sieht dieser Ersatz nie.
"""
import re, sys
O = re.compile(r"^\\s*obase\\s*=\\s*16\\s*;\\s*(-?\\d+)\\s*$")
I = re.compile(r"^\\s*ibase\\s*=\\s*16\\s*;\\s*([0-9A-Fa-f]+)\\s*$")
for z in sys.stdin.read().splitlines():
    if not z.strip():
        continue
    m = O.match(z)
    if m:
        print(format(int(m.group(1)), "X")); continue
    m = I.match(z)
    if m:
        print(int(m.group(1), 16)); continue
    sys.stderr.write("bc-Ersatz: nicht unterstuetzt: %r\\n" % z)
    raise SystemExit(2)
'''


def marke(f: int, c: int) -> bytes:
    return ("UFT-K FDS F%d C%03d " % (f, c)).encode("ascii")


def nutzdaten(pfad: Path, f: int) -> None:
    """16 Brocken zu 512 Byte, jeder nennt Datei und Brocken.

    512 = 28 x 18 + 8, die Phase wandert also im Brocken. Absicht: ein
    Vergleich, der nur den Anfang prueft, koennte einen Versatz
    ueberdecken (Lehre aus MF-1149) — der Test vergleicht den ganzen
    Brocken.
    """
    buf = bytearray()
    for c in range(BROCKEN):
        m = marke(f, c)
        brocken = (m * ((SS // len(m)) + 1))[:SS]
        assert len(brocken) == SS and brocken.startswith(m)
        buf += brocken
    assert len(buf) == NUTZ_LEN
    pfad.write_bytes(bytes(buf))


def lauf(argv: list[str], eingabe: str = "", cwd: str | None = None,
         umgebung: dict | None = None, rohaus: Path | None = None) -> str:
    aus = subprocess.run(argv, input=eingabe.encode("ascii"),
                         capture_output=True, cwd=cwd, env=umgebung)
    if aus.returncode != 0:
        raise SystemExit(f"ABBRUCH: {argv[0]} endete mit {aus.returncode}\n"
                         f"{aus.stdout[:400]!r}\n{aus.stderr[:400]!r}")
    if rohaus is not None:
        rohaus.write_bytes(aus.stdout)
    return aus.stderr.decode("latin-1")


def main() -> int:
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    fdtc = Path(sys.argv[1]).resolve()
    bintofdf = Path(sys.argv[2]).resolve()
    fdstool = Path(sys.argv[3]).resolve()
    for p in (fdtc, bintofdf, fdstool):
        if not p.exists():
            raise SystemExit(f"ABBRUCH: nicht gefunden: {p}")

    arbeit = Path(tempfile.mkdtemp(prefix="uft_fds_"))
    print(f"0. Arbeitsverzeichnis (ausserhalb des Baums): {arbeit}")

    # Der `bc`-Ersatz, damit das Werkzeug UNVERAENDERT laufen kann.
    bcdir = arbeit / "bcshim"
    bcdir.mkdir()
    (bcdir / "bc").write_text(BC_ERSATZ, encoding="utf-8")
    os.chmod(bcdir / "bc", 0o755)
    umgebung = dict(os.environ)
    umgebung["PATH"] = f"{bcdir}{os.pathsep}" + umgebung.get("PATH", "")
    if shutil.which("bc") is None:
        print("   `bc` fehlt in dieser Umgebung — Ersatz bereitgestellt "
              "(nur Zahlenbasis, siehe Kopf)")

    sh = shutil.which("sh")
    if not sh:
        raise SystemExit("ABBRUCH: keine POSIX-Shell gefunden (`sh`)")

    # 1. Nutzdaten + Typ-3-Kopf je Datei
    fdfs = []
    for f in range(DATEIEN):
        bin_ = arbeit / f"UFTK{f}.bin"
        nutzdaten(bin_, f)
        fdf = arbeit / f"UFTK{f}.fdf"
        lauf([sh, str(bintofdf), str(bin_)], eingabe="00 8000\n",
             cwd=str(arbeit), umgebung=umgebung, rohaus=fdf)
        erwartet = 17 + NUTZ_LEN
        if fdf.stat().st_size != erwartet:
            raise SystemExit(f"ABBRUCH: {fdf.name} hat "
                             f"{fdf.stat().st_size} statt {erwartet} Byte")
        fdfs.append(str(fdf))
    print(f"1. bintofdf: {DATEIEN} x {17 + NUTZ_LEN} Byte "
          f"(Typ-3-Kopf 16 + 0x04 + {NUTZ_LEN} Nutzdaten)")

    # 2. Behaelter mit Typ-1- und Typ-2-Block
    roh = arbeit / "uftk_roh.fds"
    lauf([sh, str(fdtc)] + fdfs, eingabe="00 UFT 20 00 A 00 00 00 J Y 00\n",
         cwd=str(arbeit), umgebung=umgebung, rohaus=roh)
    bloecke = 56 + 2 + DATEIEN * (17 + NUTZ_LEN)
    if roh.stat().st_size != bloecke:
        raise SystemExit(f"ABBRUCH: fdtc lieferte {roh.stat().st_size} statt "
                         f"{bloecke} Byte")
    print(f"2. fdtc: {roh.stat().st_size} Byte = 56 (Typ 1) + 2 (Typ 2) + "
          f"{DATEIEN} x {17 + NUTZ_LEN}")
    d = roh.read_bytes()
    if d[0] != 0x01 or d[1:15] != b"*NINTENDO-HVC*":
        raise SystemExit("ABBRUCH: Typ-1-Block ohne `*NINTENDO-HVC*`")
    if d[56] != 0x02 or d[57] != DATEIEN:
        raise SystemExit(f"ABBRUCH: Typ-2-Block sagt {d[57]} Dateien")

    # 3. Polstern auf die Seitengroesse. UFT-eigen, aber die ZAHL ist von
    #    fdstool bestaetigt: `FDS_LENGTH 65500`, und es weist die
    #    ungepolsterte Datei ab.
    seite = arbeit / "uftk_seite.fds"
    seite.write_bytes(d + b"\x00" * (SEITE - len(d)))
    print(f"3. gepolstert: {len(d)} -> {SEITE} Byte "
          f"({SEITE - len(d)} Nullbytes)")

    # 4. fwNES-Kopf von fdstool, und dieselbe Hand liest zur Gegenprobe.
    ziel = arbeit / ZIEL
    subprocess.run([str(fdstool), "-a", "-o", str(seite), str(ziel)],
                   capture_output=True)
    if not ziel.exists() or ziel.stat().st_size != GROESSE:
        gr = ziel.stat().st_size if ziel.exists() else 0
        raise SystemExit(f"ABBRUCH: fdstool -a lieferte {gr} statt "
                         f"{GROESSE} Byte")
    k = ziel.read_bytes()[:KOPF_LEN]
    if k[:4] != b"FDS\x1a" or k[4] != 1 or set(k[5:]) != {0}:
        raise SystemExit(f"ABBRUCH: fwNES-Kopf falsch: {k!r}")
    print(f"4. fdstool -a: {ziel.stat().st_size} Byte, Kopf "
          "FDS\\x1a + 1 Seite + 11 Nullbytes")

    liste = subprocess.run([str(fdstool), str(ziel)], capture_output=True,
                           text=True).stdout
    for pflicht in ("Found FDS header with 1 side", "File amount: 2",
                    "File address: $8000", "File size: 8192 bytes"):
        if pflicht not in liste:
            raise SystemExit(
                f"ABBRUCH: die ZWEITE Hand bestaetigt nicht `{pflicht}` — "
                "die Datei wird NICHT ausgeliefert (MF-1028).")
    print("5. fdstool liest zurueck: 1 Seite, 2 Dateien, $8000, 8192 Byte")
    print("   HINWEIS: es meldet beide Namen als `UFT` statt `UFTK0`/`UFTK1` "
          "— Defekt im Orakel (fdstool.c:641 laeuft ueber 3 statt 8 "
          "Namensbytes), nicht im Abbild.")

    inhalt = ziel.read_bytes()
    print()
    print(ZIEL)
    print(f"  Groesse : {len(inhalt)}")
    print(f"  sha256  : {hashlib.sha256(inhalt).hexdigest()}")
    print("  -> nach tests/corpus_free/ kopieren. Die Summe im Manifest wird")
    print("     gegen das GIT-OBJEKT gebildet (MF-1096 Sperre 3):")
    print(f"     git hash-object -t blob tests/corpus_free/{ZIEL}")
    print("     und `git check-attr text` muss `unset` melden.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
