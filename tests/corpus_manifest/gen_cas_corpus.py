#!/usr/bin/env python3
"""MSX-Kassettenabbild von FREMDER Hand — das Rezept zu MF-1223.

`cas` stand seit MF-1040 auf T2, und die Begruendung war benannt: MAMEs
Lader wandelt CAS in WAV-Abtastwerte und gibt keine Bloecke zurueck — es
gab **keinen fremden Erzeuger**. Der Leser selbst war abgenommen (gegen
MAMEs `fmsx_cas.cpp`) und drei stille Verluste behoben.

── Warum nicht MAMEs `imgtool` ────────────────────────────────────────

`imgtool` kennt ein `fmsx_cas`-Modul und koennte schreiben. Es ist
trotzdem ungeeignet: sein Code IST die Referenz, gegen die UFTs Leser
abgenommen wurde. Das ist die Falle aus MF-1135 (`dms`) — dort stammten
UFTs Leser UND hxcfes Leser aus xDMS, und jeder Abgleich war derselbe
Kreis mit zwei Namen.

`joyrex2001/castools` ist unabhaengig, und das ist am Quelltext gemessen:
`cas2wav.c:69` deklariert die Kennung `1F A6 DE BA CC 13 7D 74` selbst,
und `wav2cas.c` nennt die MSX-Blocktypen (0xD0/0xD3/0xEA) **0 Mal** — es
ist ein reiner Signaldekoder und deutet keinen Inhalt. Lizenz **GPL-2**,
am `COPYING` IM PAKET gemessen (18 009 Byte).

── Der Umweg ist der Beleg (Klasse MF-1084) ──────────────────────────

Ein Werkzeug, das `.cas` nach `.cas` schreibt, koennte die Bytes
durchreichen. Dieses Rezept fuehrt sie durch eine AUDIO-WELLENFORM:

    1. selbstbenennende Eingabe        1 584 Byte
    2. cas2wav   ->  FSK-Wellenform    1 853 036 Byte
    3. wav2cas   ->  das Erzeugnis     1 584 Byte, 0 abweichend

Wer das kann, modelliert 1200/2400-Hz-FSK, die 11-Bit-Rahmung und die
Blockkoepfe wirklich. Und die Eingabe benennt sich selbst (`UFT-K CAS
Bnn ` alle 14 Byte), damit ein Leseergebnis nicht nur sagt, DASS etwas
kam, sondern ob die richtige Stelle getroffen wurde (MF-1020/MF-1021).

── Drei Haende, kein Selbstgespraech (MF-1028) ───────────────────────

Dass die Eingabe WOHLGEFORMT ist, sagt nicht UFT, sondern `casdir` — das
dritte Werkzeug des Pakets mit eigenem Leser. Es muss den Kopfblock als
`UFTKOR  binary` erkennen; sonst liefert dieses Skript nichts aus.
Dieselbe Regel wie in `gen_container_corpus.py` und
`gen_adf_ext_corpus.py`.

Aufruf:
    python tests/corpus_manifest/gen_cas_corpus.py \
        <cas2wav.exe> <wav2cas.exe> [<casdir.exe>]

Bau des Erzeugers:
    curl -L -o castools.tar.gz \\
        https://codeload.github.com/joyrex2001/castools/tar.gz/refs/heads/master
    tar xf castools.tar.gz && cd castools-master && mingw32-make
    -> cas2wav.exe, wav2cas.exe, casdir.exe, cpu.exe
       gcc 13.1.0, erster Versuch rc 0
"""
from __future__ import annotations

import hashlib
import subprocess
import sys
import tempfile
from pathlib import Path

SYNC = bytes([0x1F, 0xA6, 0xDE, 0xBA, 0xCC, 0x13, 0x7D, 0x74])
NAME = b"UFTKOR"          # 6 Zeichen, MSX-Konvention
DATENBLOECKE = 3
BLOCKGROESSE = 512

ZIEL = "castools_wav2cas_uftk.cas"


def marke(b: int) -> bytes:
    return ("UFT-K CAS B%02d " % b).encode("ascii")


def selbstbenennende_cas(pfad: Path) -> None:
    """Ein Binaerkopf plus drei selbstbenennende Datenbloecke.

    512 ist KEIN Vielfaches von 14, die Phase wandert also im Block. Das
    ist Absicht: ein Vergleich, der die Marke am Blockanfang erwartet,
    faellt bei jedem Versatzfehler (Lehre aus MF-1149).
    """
    bloecke = [SYNC + bytes([0xD0] * 10) + NAME]
    for b in range(DATENBLOECKE):
        m = marke(b)
        nutz = (m * ((BLOCKGROESSE // len(m)) + 1))[:BLOCKGROESSE]
        assert len(nutz) == BLOCKGROESSE and nutz.startswith(m)
        bloecke.append(SYNC + nutz)
    pfad.write_bytes(b"".join(bloecke))


def lauf(argv: list[str]) -> str:
    e = subprocess.run(argv, capture_output=True, text=True)
    if e.returncode != 0:
        raise SystemExit(f"ABBRUCH: {argv[0]} endete mit {e.returncode}\n"
                         f"{e.stdout}\n{e.stderr}")
    return e.stdout + e.stderr


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    cas2wav = Path(sys.argv[1]).resolve()
    wav2cas = Path(sys.argv[2]).resolve()
    casdir = Path(sys.argv[3]).resolve() if len(sys.argv) > 3 else None
    for p in (cas2wav, wav2cas):
        if not p.exists():
            raise SystemExit(f"ABBRUCH: nicht gefunden: {p}")

    # Arbeitsdateien NICHT in den Baum: `scripts/repo_scope.py` liefert
    # auch unverfolgte Dateien (MF-636).
    arbeit = Path(tempfile.mkdtemp(prefix="uft_cas_"))
    print(f"0. Arbeitsverzeichnis (ausserhalb des Baums): {arbeit}")

    eingabe = arbeit / "uftk_eingabe.cas"
    welle = arbeit / "uftk.wav"
    ziel = arbeit / ZIEL

    selbstbenennende_cas(eingabe)
    print(f"1. selbstbenennende Eingabe: {eingabe.stat().st_size} Byte, "
          f"{1 + DATENBLOECKE} Bloecke")

    # `cas2wav` will Ein- UND Ausgabedatei; ohne die zweite druckt es nur
    # seine Gebrauchsanweisung und endet mit rc 1.
    lauf([str(cas2wav), str(eingabe), str(welle)])
    print(f"2. cas2wav: {welle.stat().st_size} Byte FSK-Wellenform")

    ausgabe = lauf([str(wav2cas), str(welle), str(ziel)])
    print(f"3. wav2cas: {ziel.stat().st_size} Byte")
    for z in ausgabe.splitlines():
        if z.strip():
            print(f"   {z.strip()}")

    a, b = eingabe.read_bytes(), ziel.read_bytes()
    abweichend = sum(1 for x, y in zip(a, b) if x != y)
    if len(a) != len(b) or abweichend:
        raise SystemExit(
            "ABBRUCH: der Rundlauf durch die Wellenform verliert Bytes — "
            f"{len(a)} gegen {len(b)} Byte, {abweichend} abweichend. Die "
            "Datei wird NICHT ausgeliefert: ein Erzeugnis, dessen Inhalt "
            "nicht nachweisbar ist, belegt nichts (MF-1021).")
    print(f"   -> 0 von {len(a)} Byte abweichend")

    if casdir and casdir.exists():
        liste = lauf([str(casdir), str(ziel)])
        if NAME.decode("ascii") not in liste:
            raise SystemExit(
                "ABBRUCH: die DRITTE Hand erkennt den Kopfblock nicht — "
                f"`casdir` nennt {NAME!r} nicht. Damit ist nicht belegt, "
                "dass die Datei wohlgeformtes MSX-CAS ist (MF-1028).")
        print("4. casdir (dritte Hand):")
        for z in liste.splitlines():
            if z.strip():
                print(f"   {z.strip()}")
    else:
        print("4. casdir NICHT uebergeben — die Wohlgeformtheit ist damit "
              "UNGEPRUEFT, nicht bestanden (MF-1028).")

    roh = ziel.read_bytes()
    print()
    print(ZIEL)
    print(f"  Groesse : {len(roh)}")
    print(f"  sha256  : {hashlib.sha256(roh).hexdigest()}")
    print("  -> nach tests/corpus_free/ kopieren. Die Summe im Manifest wird")
    print("     gegen das GIT-OBJEKT gebildet (MF-1096 Sperre 3):")
    print(f"     git hash-object -t blob tests/corpus_free/{ZIEL}")
    print("     und `git check-attr text` muss `unset` melden.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
