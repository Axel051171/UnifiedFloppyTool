#!/usr/bin/env bash
# analyze_image.sh — Erste Bestandsaufnahme einer Diskettendatei.
#
# Kennzeichnet jede Aussage als [MEASURED] (aus der Datei gelesen) oder
# [INFERRED] (aus Größe/Form geraten). Nennt NIE ein Format mit Sicherheit
# allein aus der Größe — eine Größenübereinstimmung ist ein Kandidat.
#
# Kein Schreibzugriff, keine Netzwerkaktivität. Reine Lesung.
#
# HERKUNFT DER MAGIC-BYTES: jede Signatur unten stammt aus der Konstante des
# zugehörigen UFT-Plugins, nicht aus dem Gedächtnis. selftest.sh prüft das
# maschinell gegen src/ — die erste Fassung dieses Skripts hatte "HYCHE" statt
# "HXCPICFE" und hätte nie eine HFE-Datei erkannt.
#
# Läuft unter Git-Bash (Windows) genauso wie unter Linux/macOS.
set -euo pipefail

if [ $# -lt 1 ]; then
  echo "usage: $0 <image-datei>" >&2
  exit 2
fi
F="$1"
if [ ! -f "$F" ]; then
  echo "FEHLER: Datei nicht gefunden: $F" >&2
  exit 2
fi

# Windows-Git-Bash hat oft nur `python`, Linux oft nur `python3`.
if command -v python3 >/dev/null 2>&1; then PY=python3
elif command -v python >/dev/null 2>&1; then PY=python
else echo "FEHLER: weder python3 noch python im PATH" >&2; exit 2
fi

SIZE=$("$PY" -c "import os,sys; print(os.path.getsize(sys.argv[1]))" "$F")
echo "Größe:      $SIZE Bytes            [MEASURED]"

if command -v sha256sum >/dev/null 2>&1; then
  echo "SHA-256:    $(sha256sum "$F" | cut -d' ' -f1)  [MEASURED]"
else
  echo "SHA-256:    $("$PY" -c "
import hashlib,sys
h=hashlib.sha256()
with open(sys.argv[1],'rb') as fh:
    for b in iter(lambda: fh.read(1<<20), b''): h.update(b)
print(h.hexdigest())" "$F")  [MEASURED]"
fi

# --- Magic, Bootsignatur, Entropie: alles MEASURED, in einem Durchlauf ---
"$PY" - "$F" <<'PY'
import sys, math, collections

path = sys.argv[1]
with open(path, 'rb') as fh:
    head = fh.read(64)

# Signatur -> (Offset, Plugin-Quelle im Baum). Nur was UFT wirklich kennt.
MAGIC = [
    (b"GCR-1541",         0, "g64   src/formats/commodore/g64.c"),
    (b"HXCPICFE",         0, "hfe   src/formats/hfe/uft_hfe.c:39"),
    (b"HXCHFEV3",         0, "hfe   src/formats/hfe/uft_hfe.c:40 (v3)"),
    (b"SCP",              0, "scp   src/formats/flux/scp.c:62"),
    (b"WOZ1",             0, "woz   src/formats/apple/"),
    (b"WOZ2",             0, "woz   src/formats/apple/"),
    (b"MOOF",             0, "moof  src/formats/apple/"),
    (b"A2R2",             0, "a2r   src/formats/apple/"),
    (b"2IMG",             0, "2img  src/formats/apple/"),
    (b"CAPS",             0, "ipf   src/formats/ipf/"),
    (b"IMD ",             0, "imd   src/formats/pc/"),
    (b"RSY",              0, "cqm   src/formats/pc/"),
    (b"FDI",              0, "fdi   src/formats/fdi/"),
    (b"MV - CPC",         0, "dsk_cpc"),
    (b"EXTENDED CPC DSK", 0, "edsk"),
]

hits = [(m, off, src) for m, off, src in MAGIC if head[off:off+len(m)] == m]
if hits:
    for m, off, src in hits:
        try:
            shown = m.decode('ascii')
        except UnicodeDecodeError:
            shown = m.hex()
        print(f"Magic:      '{shown}' @ Offset {off}   [MEASURED]  -> {src}")
else:
    print("Magic:      keine bekannte UFT-Signatur in den ersten 64 Bytes [MEASURED]")

# 55 AA: gemessen, aber ausdruecklich KEIN Format.
with open(path, 'rb') as fh:
    fh.seek(510)
    sig = fh.read(2)
if sig == b"\x55\xAA":
    print("Bootsig:    55 AA am Offset 510  [MEASURED]")
    print("            -> bedeutet x86-bootfaehig, NICHT FAT12.")
    print("               Format aus dem BPB bestimmen (Bytes/Sektor, Sektoren/Cluster, FAT-Zahl).")

with open(path, 'rb') as fh:
    data = fh.read()
if data:
    c = collections.Counter(data); n = len(data)
    ent = -sum((v/n)*math.log2(v/n) for v in c.values())
else:
    ent = 0.0
print(f"Entropie:   {ent:.2f} bits/byte        [MEASURED]")
if ent > 7.5:
    print("            -> hoch: moeglicherweise komprimiert/verschluesselt,")
    print("               kein rohes Sektorabbild")
PY

# --- Geometrie-KANDIDATEN aus der Größe: ausdrücklich INFERRED ---
echo
echo "Größen-Kandidaten            [INFERRED — Größe ist ein Kandidat, keine Identifikation]:"
case "$SIZE" in
  368640)  echo "  368640  = PC 360K DD (5.25\", 40 Spuren)  — die Kodierung bestätigt es, nicht die Größe" ;;
  737280)  echo "  737280  = PC 720K DD (3.5\")              — dito" ;;
  1228800) echo "  1228800 = PC 1.2M HD (5.25\")             — dito" ;;
  1474560) echo "  1474560 = PC 1.44M HD (3.5\")             — dito" ;;
  901120)  echo "  901120  = Amiga ADF DD (880K)            — die MFM-Kodierung bestätigt es" ;;
  1802240) echo "  1802240 = Amiga ADF HD                   — dito" ;;
  174848)  echo "  174848  = C64 D64 (35 Spuren)            — die GCR-Kodierung bestätigt es" ;;
  196608)  echo "  196608  = C64 D64 (40 Spuren)            — dito" ;;
  349696)  echo "  349696  = C64 D71 (70 Spuren)            — dito" ;;
  819200)  echo "  819200  = C64 D81 (80 Spuren)            — dito" ;;
  92160)   echo "  92160   = Atari ATR/XFD 90K SD           — dito" ;;
  *)       echo "  $SIZE passt auf keine der hier hinterlegten Standardgrößen" ;;
esac

echo
echo "Nächster Schritt: UFT selbst parsen lassen und BEIDE Achsen nachschlagen"
echo "  python scripts/gen_verification_tiers.py --md | grep -i <format>"
echo "     Tier T1/T1b/T2/T3 = wurde der PARSER gegen Realität geprüft"
echo "     spec_status       = wie gut ist das FORMAT spezifiziert"
echo
echo "Diese Ausgabe identifiziert KEIN Format — sie sammelt Kandidaten und Messwerte."
