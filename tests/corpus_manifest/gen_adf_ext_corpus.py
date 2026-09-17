#!/usr/bin/env python3
"""Extended ADF (UAE-1ADF) von FREMDER Hand — das Rezept zu MF-1222.

`adf_ext` stand auf T2, weil sein einziger Test seine Pruefdatei SELBST
baut: `test_adf_ext_plugin` schreibt den Kopf, die Spurtafel und die
Spurdaten und liest sie dann zurueck. Schreiber und Leser sind dieselbe
Hand — genau die Gestalt, in der `apridisk` (MF-1009) und `qrst`
(MF-1028) gruen durch einen ERFUNDENEN Aufbau gelaufen sind.

── Warum es lange keinen Erzeuger gab, und warum es jetzt einen gibt ──

`docs/ERZEUGER_ZENSUS.md` fuehrte fuer `adf_ext` keinen Kanal, und
MF-1220 hat das vierfach bestaetigt. Der Zensus ordnet Werkzeuge aber
ueber ENDUNGEN zu (`d["ext"] & exts` in `gen_erzeuger_zensus.py`), und
`adf_ext` teilt seine Endung `adf` mit `adf` — dem gewoehnlichen
AmigaDOS-Abbild, das seit MF-1081 auf T1b steht. Direkt nach
MODULNAMEN gefragt sagen die drei gebauten Werkzeugfamilien dasselbe:

    hxcfe    AMIGA_EXTADF;R          nur LESEN
    floptool kein Modul
    libdsk   keiner seiner 27 Typen

Der Erzeuger lag daneben, im Baum, ungebaut: `neue-ideen/
disk-utilities-master.zip`. `libdisk/container/eadf.c` darin schreibt in
`eadf_close()` woertlich `memcpy(dhdr.sig, "UAE-1ADF", ...)` — genau die
Kennung, die `uft_adf_ext.c` verlangt. Lizenz: **Unlicense / public
domain**, am mitgelieferten `COPYING` gemessen (1211 Byte), nicht nur am
Ursprung nachgelesen; das mitgelieferte `eadf.c` ist mit dem Ursprung
byteidentisch (5593 Byte).

── Drei Haende, dieselbe Diskette ──────────────────────────────────────

  1. DIESES Skript legt eine ADF, in der sich JEDER Sektor selbst
     benennt (`UFT-K Ccc Hh Sss ` alle 17 Byte). Ohne das waere eine
     Pruefdatei aus Fuellbytes moeglich, die Erfolg meldet und nichts
     belegt (MF-1021, `v9t9`: 184 320 Byte, zu 100 % 0xF6).
  2. `disk-analyse` kodiert jede Spur nach AmigaDOS-MFM. Das ist kein
     Durchreichen (MF-1084): der Zwischenschritt ist ein ZELLSTROM, und
     wer ihn schreibt, muss Sync, Kopffeld, Pruefsummen und Anordnung
     wirklich modellieren. `eadf_close()` setzt `thdr.type = htobe16(1)`
     einmal vor der Schleife — es schreibt AUSSCHLIESSLICH rohe
     MFM-Spuren.
  3. `hxcfe` (`AMIGA_EXTADF`, GPL-2, unabhaengige Umsetzung) liest das
     Ergebnis zurueck nach ADF. **Nur wenn das byteidentisch mit (1)
     ist, wird die Datei ueberhaupt ausgeliefert.** Eine von einer Hand
     erzeugte Datei belegt allein nichts — dieselbe Regel wie in
     `gen_container_corpus.py`.

Erst danach liest UFT sie mit seinem EIGENEN Dekoder
(`flux_decode_amiga_bits`) und muss 1760 von 1760 Sektoren an ihrer
eigenen Ortsmarke finden: `tests/test_adf_ext_gegen_disk_analyse.c`.

── Warum die Datei `.adf` heisst und nicht `.eadf` ─────────────────────

Gemessen, nicht gewaehlt: hxcfe waehlt seinen Lader ueber die Endung und
sagt zu `uftk.eadf` „No loader support the file" (rc 127), zu derselben
Datei als `.adf` „File loader found : AMIGA_EXTADF" (rc 0). Dazu fuehrt
`gen_verification_tiers.py` fuer `adf_ext` die Endung `adf` — eine
`.eadf` waere fuer `scripts/audit_korpus_regal.py` (MF-1220) unsichtbar.
`disk-analyse` dagegen waehlt seinen SCHREIBER ueber die Endung und
braucht `.eadf`; deshalb schreibt dieses Skript `.eadf` und benennt um.
Zwei Werkzeuge, zwei Konventionen, eine Datei — der Schritt gehoert
mechanisiert, nicht in eine Anleitung.

Aufruf:
    python tests/corpus_manifest/gen_adf_ext_corpus.py \
        <disk-analyse.exe> <disk-analyse/formats> [<hxcfe.exe>]

Bau des Erzeugers (nicht der dokumentierte Weg, deshalb notiert —
Klasse MF-1028, wo libdsks Bau aufgeschrieben werden musste):
    unzip neue-ideen/disk-utilities-master.zip
    cd disk-utilities-master && mingw32-make SHARED_LIB=n
    -> disk-analyse/disk-analyse.exe, gcc 13.1.0, erster Versuch rc 0
"""
from __future__ import annotations

import hashlib
import subprocess
import sys
import tempfile
from pathlib import Path

ZYLINDER = 80
KOEPFE = 2
SEKTOREN = 11
SEKTORGROESSE = 512
ADF_GROESSE = ZYLINDER * KOEPFE * SEKTOREN * SEKTORGROESSE   # 901 120

ZIEL = "disk_analyse_extadf_raw160.adf"


def marke(c: int, h: int, s: int) -> bytes:
    return ("UFT-K C%02d H%d S%02d " % (c, h, s)).encode("ascii")


def selbstbenennende_adf(pfad: Path) -> None:
    """Jeder Sektor nennt seinen Platz — 17 Byte, ab Versatz 0 wiederholt.

    512 = 30 x 17 + 2, die Phase wandert also im Sektor. Das ist
    ABSICHT: ein Vergleich, der die Marke am Sektoranfang erwartet,
    faellt bei jedem Versatzfehler; ein Muster, das 512 teilt, koennte
    einen halben Sektor Versatz ueberdecken (Lehre aus MF-1149).
    """
    puffer = bytearray()
    for c in range(ZYLINDER):
        for h in range(KOEPFE):
            for s in range(SEKTOREN):
                m = marke(c, h, s)
                sektor = (m * ((SEKTORGROESSE // len(m)) + 1))[:SEKTORGROESSE]
                assert len(sektor) == SEKTORGROESSE and sektor.startswith(m)
                puffer += sektor
    if len(puffer) != ADF_GROESSE:
        raise SystemExit(f"ABBRUCH: {len(puffer)} statt {ADF_GROESSE} Byte")
    pfad.write_bytes(bytes(puffer))


def lauf(argv: list[str], cwd: str | None = None) -> str:
    ergebnis = subprocess.run(argv, capture_output=True, text=True, cwd=cwd)
    if ergebnis.returncode != 0:
        raise SystemExit(
            f"ABBRUCH: {argv[0]} endete mit {ergebnis.returncode}\n"
            f"{ergebnis.stdout}\n{ergebnis.stderr}")
    return ergebnis.stdout


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    da = Path(sys.argv[1]).resolve()
    formate = Path(sys.argv[2]).resolve()
    hxc = Path(sys.argv[3]).resolve() if len(sys.argv) > 3 else None
    for p in (da, formate):
        if not p.exists():
            raise SystemExit(f"ABBRUCH: nicht gefunden: {p}")

    # Arbeitsdateien NICHT in den Baum: `scripts/repo_scope.py` liefert
    # auch unverfolgte Dateien (MF-636), ein Erzeuger, der Zwischenstaende
    # neben sich ablegt, laeuft also in jedes Tor hinein.
    arbeit = Path(tempfile.mkdtemp(prefix="uft_adf_ext_"))
    print(f"0. Arbeitsverzeichnis (ausserhalb des Baums): {arbeit}")
    quelle = arbeit / "uftk_selbstbenennend.adf"
    zwischen = arbeit / "uftk.eadf"        # disk-analyse waehlt per Endung
    ziel = arbeit / ZIEL

    selbstbenennende_adf(quelle)
    print(f"1. selbstbenennende ADF: {quelle.stat().st_size} Byte")

    # `-c` bekommt den BLOSSEN Dateinamen, und gelaufen wird im Verzeichnis
    # des Deskriptors. Gemessen, nicht vorsichtshalber: `config.c::open_file()`
    # entscheidet mit `if (name[0] != '/')` ueber „absolut" — ein
    # Windows-Pfad beginnt mit `C:`, gilt damit als relativ und wird hinter
    # `getcwd()` gehaengt. Absolut uebergeben endet der Lauf mit rc 1 und
    # „could not open config file", waehrend die Datei existiert; relativ
    # mit rc 0. Ein- und Ausgabe duerfen absolut sein, die gehen durch
    # gewoehnliches `fopen`.
    ausgabe = lauf([str(da), "-c", formate.name, str(quelle), str(zwischen)],
                   cwd=str(formate.parent))
    print(f"2. disk-analyse: {zwischen.stat().st_size} Byte")
    for z in ausgabe.splitlines():
        if z.strip():
            print(f"   {z.strip()}")
    ziel.write_bytes(zwischen.read_bytes())

    if hxc and hxc.exists():
        zurueck = arbeit / "hxc_zurueck.adf"
        lauf([str(hxc), f"-finput:{ziel}", "-conv:AMIGA_ADF",
              f"-foutput:{zurueck}"])
        a, b = quelle.read_bytes(), zurueck.read_bytes()
        abweichend = sum(1 for x, y in zip(a, b) if x != y)
        if len(a) != len(b) or abweichend:
            raise SystemExit(
                "ABBRUCH: die ZWEITE Hand liest etwas anderes zurueck — "
                f"{len(a)} gegen {len(b)} Byte, {abweichend} abweichend. "
                "Die Datei wird NICHT ausgeliefert (Regel aus "
                "gen_container_corpus.py).")
        print(f"3. hxcfe (AMIGA_EXTADF) liest zurueck: "
              f"0 von {len(a)} Byte abweichend")
    else:
        print("3. hxcfe NICHT uebergeben — die Zwei-Haende-Pruefung ist "
              "AUSGEFALLEN, nicht bestanden. Die Datei ist damit ein "
              "Einzelzeugnis (MF-1028).")

    roh = ziel.read_bytes()
    print()
    print(f"{ZIEL}")
    print(f"  Groesse : {len(roh)}")
    print(f"  sha256  : {hashlib.sha256(roh).hexdigest()}")
    print("  -> nach tests/corpus_free/ kopieren. Die Summe im Manifest wird")
    print("     gegen das GIT-OBJEKT gebildet (MF-1096 Sperre 3):")
    print(f"     git hash-object -t blob tests/corpus_free/{ZIEL}")
    print("     und `git check-attr text` muss `unset` melden.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
