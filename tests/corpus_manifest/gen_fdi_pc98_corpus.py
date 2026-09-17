#!/usr/bin/env python3
"""PC-98-FDI von FREMDER Hand — das Rezept zu MF-1224.

`fdi_pc98` stand seit MF-1026 auf T2. Der Leser war dort **richtig**:
Kopffelder, beide Konsistenzbedingungen aus MAMEs `identify()`,
Versatzformel und 1-basierte Sektornummern stimmten ueberein. Gefehlt hat
ein Abbild von fremder Hand.

── Die Lizenzlage, und sie ist eine Eigentuemerentscheidung ──────────

Das Paket `pc98-disk-tools` hat **keine Lizenzdatei**, und im ganzen
README (3549 Byte, vollstaendig gelesen) steht **kein Lizenzwort**.
Gefuehrt als `P3-356`. Eigentuemer-Entscheidung vom 2026-09-17, woertlich:

    „Lizenzentscheidung (fdi_pc98, nfd) ja wir machen das"

So gehandhabt: AUSFUEHREN ja (wie `dtc`, wie `epstool` — verglichen wird
die Ausgabe), WEITERGEBEN nein (nichts aus dem Paket wandert in den Baum;
es liegt gitignoriert unter `tools/uft-scout/work/`). Das ERZEUGNIS darf
in `tests/corpus_free/`, weil kein fremder Inhalt mitfaehrt — die 4064
Kopfbytes hinter den Feldern sind alle Null und es gibt keine ASCII-Kette
im Kopf; `test_fdi_pc98_gegen_pc98tools.c` prueft beides nach.

**Und sie deckt `nfd` NICHT ab, obwohl sie es nennt:** das Paket erwaehnt
NFD in keiner seiner 14 Dateien. Dort fehlt ein SCHREIBER, nicht eine
Lizenz.

── Was dieser Beleg traegt, und was nicht ────────────────────────────

Er ist SCHWAECHER als MF-1222/1223, und das gehoert vorneweg.
`hdm_to_fdi.py` stellt der Nutzlast einen Kopf voran und laesst die Bytes
sonst unberuehrt:

    fdi_header = pack('<8L4064x', dummy, fddtype, headersize, fdd_size,
                      sector_size, sector_count, surfaces, cylinders)
    full_fdi_image = fdi_header + hdm_blob

Kein Modellierungsschritt wie der MFM-Zellstrom (MF-1222) oder die
FSK-Wellenform (MF-1223), und die Geometriewerte sind im Skript FEST
VERDRAHTET (1024/8/2/77), nicht aus der Eingabe abgeleitet. Belegt ist
die **Behaelter-Zerlegung** — dieselben acht Dwords an denselben acht
Versaetzen —, nicht die Geometrie-Herleitung.

Mit nur EINER fremden Hand waere das nahe an der „Gleichheit ohne
Aussage", die MF-1039 abgelehnt hat. Deshalb verlangt dieses Rezept die
DRITTE Hand und liefert sonst nichts aus: `hxcfe` muss das Erzeugnis mit
`NEC_FDI` laden (nicht mit seinem ZX-FDI-Lader — beide beanspruchen die
Endung `.fdi`) und daraus eine **IMD** schreiben, die je Sektor Zylinder,
Kopf und Nummer AUSDRUECKLICH nennt. Erst der sektorweise Abgleich gegen
die selbstbenennenden Marken macht die Zerlegung pruefbar, ohne UFT zu
befragen — dasselbe Muster wie MF-1037 bei `dim`.

Aufruf:
    python tests/corpus_manifest/gen_fdi_pc98_corpus.py \
        <hdm_to_fdi.py> [<hxcfe.exe>]

Beschaffung des Erzeugers: `neue-ideen/pc98-disk-tools-master.zip`
(sha256 90505a65d15ef707702be6ec2c8e99b1e4861079fe07650e7db525e8cdf23a58),
entpacken nach `tools/uft-scout/work/`. Reines Python 3, kein Bau.
"""
from __future__ import annotations

import hashlib
import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ZYLINDER = 77
KOEPFE = 2
SPT = 8
SEKTORGROESSE = 1024
HDM_GROESSE = ZYLINDER * KOEPFE * SPT * SEKTORGROESSE   # 1 261 568
KOPF_LEN = 4096
FDI_GROESSE = KOPF_LEN + HDM_GROESSE                    # 1 265 664

ZIEL = "pc98tools_hdm2fdi_uftk.fdi"
GROESSEN_KODE = {0: 128, 1: 256, 2: 512, 3: 1024, 4: 2048, 5: 4096, 6: 8192}


def marke(c: int, h: int, s: int) -> bytes:
    return ("UFT-K C%02d H%d S%02d " % (c, h, s)).encode("ascii")


def selbstbenennende_hdm(pfad: Path) -> None:
    """1232 Sektoren, jeder nennt seinen Platz.

    1024 = 60 x 17 + 4, die Phase wandert also im Sektor. Absicht: ein
    Vergleich, der die Marke am Sektoranfang erwartet, faellt bei jedem
    Versatzfehler (Lehre aus MF-1149).
    """
    puffer = bytearray()
    for c in range(ZYLINDER):
        for h in range(KOEPFE):
            for s in range(SPT):
                m = marke(c, h, s)
                sek = (m * ((SEKTORGROESSE // len(m)) + 1))[:SEKTORGROESSE]
                assert len(sek) == SEKTORGROESSE and sek.startswith(m)
                puffer += sek
    if len(puffer) != HDM_GROESSE:
        raise SystemExit(f"ABBRUCH: {len(puffer)} statt {HDM_GROESSE} Byte")
    pfad.write_bytes(bytes(puffer))


def lauf(argv: list[str]) -> str:
    e = subprocess.run(argv, capture_output=True, text=True)
    if e.returncode != 0:
        raise SystemExit(f"ABBRUCH: {argv[0]} endete mit {e.returncode}\n"
                         f"{e.stdout}\n{e.stderr}")
    return e.stdout + e.stderr


def imd_abgleich(pfad: Path) -> tuple[int, int, int, int]:
    """Sektorweiser Abgleich einer IMD gegen die Marken.

    Die IMD nennt Zylinder, Kopf und Nummer je Sektor ausdruecklich —
    anders als das flache FDI, in dem die Lage nur aus einer Rechnung
    folgt. Gibt (spuren_mit_sektoren, leer, getroffen, abweichend).
    """
    d = pfad.read_bytes()
    p = d.find(0x1A)
    if p < 0:
        raise SystemExit("ABBRUCH: kein 0x1A im IMD-Kopf")
    p += 1
    voll = leer = getroffen = abweichend = 0
    while p + 5 <= len(d):
        _mode, cyl, head, nsec, sc = d[p], d[p + 1], d[p + 2], d[p + 3], d[p + 4]
        p += 5
        zk, kk = bool(head & 0x80), bool(head & 0x40)
        head &= 0x3F
        nsize = GROESSEN_KODE.get(sc)
        if nsize is None:
            raise SystemExit(f"ABBRUCH: unbekannter Groessenkode {sc}")
        nummern = list(d[p:p + nsec]); p += nsec
        zylinder = list(d[p:p + nsec]) if zk else [cyl] * nsec
        if zk:
            p += nsec
        koepfe = list(d[p:p + nsec]) if kk else [head] * nsec
        if kk:
            p += nsec
        if nsec == 0:
            leer += 1
        else:
            voll += 1
        for i in range(nsec):
            typ = d[p]; p += 1
            if typ == 0x00:
                abweichend += 1
                continue
            if typ in (0x01, 0x03, 0x05, 0x07):
                daten = d[p:p + nsize]; p += nsize
            elif typ in (0x02, 0x04, 0x06, 0x08):
                daten = bytes([d[p]]) * nsize; p += 1
            else:
                raise SystemExit(f"ABBRUCH: unbekannter Sektortyp {typ:#04x}")
            # Die IMD nennt 1-basierte Nummern, die Marke ist 0-basiert.
            if daten.startswith(marke(zylinder[i], koepfe[i], nummern[i] - 1)):
                getroffen += 1
            else:
                abweichend += 1
    return voll, leer, getroffen, abweichend


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    hdm2fdi = Path(sys.argv[1]).resolve()
    hxc = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else None
    if not hdm2fdi.exists():
        raise SystemExit(f"ABBRUCH: nicht gefunden: {hdm2fdi}")

    # Arbeitsdateien NICHT in den Baum (MF-636: `repo_scope` liefert auch
    # unverfolgte Dateien).
    arbeit = Path(tempfile.mkdtemp(prefix="uft_fdi_pc98_"))
    print(f"0. Arbeitsverzeichnis (ausserhalb des Baums): {arbeit}")

    hdm = arbeit / "uftk_pc98.hdm"
    selbstbenennende_hdm(hdm)
    print(f"1. selbstbenennende HDM: {hdm.stat().st_size} Byte "
          f"({ZYLINDER} x {KOEPFE} x {SPT} x {SEKTORGROESSE}), "
          f"{ZYLINDER * KOEPFE * SPT} Sektoren")

    ausgabe = lauf([sys.executable, str(hdm2fdi), str(hdm)])
    for z in ausgabe.splitlines():
        if z.strip():
            print(f"   {z.strip()}")
    fdi = hdm.with_suffix(".fdi")
    if not fdi.exists():
        raise SystemExit(f"ABBRUCH: {fdi} nicht entstanden")
    roh = fdi.read_bytes()
    print(f"2. hdm_to_fdi.py: {len(roh)} Byte")
    if len(roh) != FDI_GROESSE:
        raise SystemExit(f"ABBRUCH: {len(roh)} statt {FDI_GROESSE} Byte")

    felder = struct.unpack("<8L", roh[:32])
    namen = ("dummy", "fddtype", "headersize", "fddsize", "sectorsize",
             "sectors", "surfaces", "cylinders")
    soll = (0, 0x90, KOPF_LEN, HDM_GROESSE, SEKTORGROESSE, SPT, KOEPFE,
            ZYLINDER)
    for n, ist, s in zip(namen, felder, soll):
        if ist != s:
            raise SystemExit(f"ABBRUCH: Kopffeld {n} = {ist}, erwartet {s}")
    print("   Kopffelder: " + ", ".join(f"{n}={v}"
                                        for n, v in zip(namen, felder)))

    # Die Bedingung, unter der das Erzeugnis ueberhaupt verteilt werden
    # darf: kein fremder Inhalt im Kopf.
    if set(roh[32:KOPF_LEN]) != {0}:
        raise SystemExit("ABBRUCH: die 4064 Kopfbytes hinter den Feldern "
                         "sind nicht alle Null — es koennte fremder Inhalt "
                         "mitfahren, und dann darf die Datei nicht in "
                         "tests/corpus_free/")
    ketten = [m.group(0) for m in re.finditer(rb"[ -~]{4,}", roh[:KOPF_LEN])]
    if ketten:
        raise SystemExit(f"ABBRUCH: ASCII-Ketten im Kopf: {ketten[:4]}")
    print("   Kopf sauber: 4064 Byte Null, keine ASCII-Kette")

    if hxc and hxc.exists():
        imd = arbeit / "uftk_pc98.imd"
        liste = lauf([str(hxc), f"-finput:{fdi}", "-conv:IMD_IMG",
                      f"-foutput:{imd}"])
        if "NEC_FDI" not in liste:
            raise SystemExit(
                "ABBRUCH: hxcfe hat NICHT den NEC-Lader genommen. Die "
                "Endung `.fdi` beanspruchen zwei seiner Module "
                "(ZXSPECTRUM_FDI und NEC_FDI) — ohne NEC_FDI sagt der "
                "Abgleich nichts ueber PC-98.")
        voll, leer, getroffen, abweichend = imd_abgleich(imd)
        print(f"3. hxcfe (NEC_FDI) -> IMD: {voll} Spursaetze mit Sektoren, "
              f"{leer} leere")
        print(f"   an ihrer Marke: {getroffen}, abweichend: {abweichend}")
        if abweichend or getroffen != ZYLINDER * KOEPFE * SPT:
            raise SystemExit(
                "ABBRUCH: die DRITTE Hand zerlegt anders — "
                f"{getroffen} getroffen, {abweichend} abweichend. Die "
                "Datei wird NICHT ausgeliefert (MF-1028).")
        if leer:
            print(f"   Hinweis: hxcfe polstert auf {(voll + leer) // 2} "
                  f"Zylinder; die {leer} zusaetzlichen Spursaetze sind "
                  "leer und stehen NICHT im FDI.")
    else:
        print("3. hxcfe NICHT uebergeben — die dritte Hand fehlt, und damit "
              "ist die Zerlegung ein EINZELZEUGNIS (MF-1028/MF-1039). Die "
              "Datei sollte so nicht in den Korpus.")

    ziel = arbeit / ZIEL
    ziel.write_bytes(roh)
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
