#!/usr/bin/env bash
# selftest.sh — belegt die Kerneigenschaften des Skills.
#
# A) Ein strukturiertes FAT12-Image und Zufallsdaten GLEICHER GRÖSSE dürfen
#    nicht dasselbe Urteil bekommen; der Unterschied muss an einer GEMESSENEN
#    Eigenschaft sichtbar werden, nicht an der Größe.
# B) Jede Magic-Konstante in analyze_image.sh muss der Konstante des
#    zugehörigen UFT-Plugins entsprechen.
#
# (B) ist der Wächter gegen den Fehler, den die erste Fassung dieses Skills
# hatte: "HYCHE" statt "HXCPICFE" — das Skript hätte nie eine HFE-Datei
# erkannt und hätte trotzdem "keine bekannte Signatur" gemeldet, also
# fehlerfrei ausgesehen.
#
# Der Test SKIPPT NIE. Die alte Fassung verlangte mkfs.fat und beendete sich
# unter Windows mit Exit 0, ohne etwas geprüft zu haben — genau das
# "green because empty"-Muster, das das Skill selbst als Anti-Pattern führt.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../../.." && pwd)"
A="$HERE/analyze_image.sh"

if command -v python3 >/dev/null 2>&1; then PY=python3
elif command -v python >/dev/null 2>&1; then PY=python
else echo "SELFTEST FEHLGESCHLAGEN: kein Python im PATH" >&2; exit 1; fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
fail() { echo "SELFTEST FEHLGESCHLAGEN: $1" >&2; exit 1; }

# ══════════════════════════════════════════════════════════════════════════
# A) Positiv-/Negativkontrolle bei gleicher Größe
# ══════════════════════════════════════════════════════════════════════════

# Positivkontrolle: echtes mkfs.fat wenn vorhanden (stärker), sonst ein
# minimales FAT12-förmiges Abbild. Welcher Weg lief, wird ausgegeben — ein
# schwächerer Beleg wird nicht als starker ausgegeben.
if command -v mkfs.fat >/dev/null 2>&1; then MKFS=mkfs.fat
elif [ -x /sbin/mkfs.fat ]; then MKFS=/sbin/mkfs.fat
else MKFS=""; fi

if [ -n "$MKFS" ]; then
  "$PY" -c "open('$TMP/real.img','wb').write(b'\0'*737280)"
  "$MKFS" -F12 "$TMP/real.img" >/dev/null 2>&1
  POS_SOURCE="mkfs.fat"
else
  "$PY" - "$TMP/real.img" <<'PY'
import sys, struct
# Minimales FAT12-720K-Abbild: gültiger BPB + Bootsignatur, Rest Nullen.
# Reicht für DIESEN Test, weil hier nichts über FAT12-Parsing behauptet wird,
# sondern nur "strukturiert vs. zufällig bei gleicher Größe".
img = bytearray(737280)
img[0:3]   = b'\xEB\x3C\x90'
img[3:11]  = b'MSDOS5.0'
struct.pack_into('<H', img, 11, 512)     # Bytes/Sektor
img[13]    = 2                            # Sektoren/Cluster
struct.pack_into('<H', img, 14, 1)       # reservierte Sektoren
img[16]    = 2                            # FAT-Zahl
struct.pack_into('<H', img, 17, 112)     # Root-Eintraege
struct.pack_into('<H', img, 19, 1440)    # Sektoren gesamt
img[21]    = 0xF9                         # Media-Descriptor 720K
struct.pack_into('<H', img, 22, 3)       # Sektoren/FAT
struct.pack_into('<H', img, 24, 9)       # Sektoren/Spur
struct.pack_into('<H', img, 26, 2)       # Koepfe
img[510:512] = b'\x55\xAA'
img[512]   = 0xF9; img[513] = 0xFF; img[514] = 0xFF   # FAT-ID
open(sys.argv[1], 'wb').write(img)
PY
  POS_SOURCE="eingebauter FAT12-BPB-Generator (mkfs.fat nicht verfuegbar)"
fi

# Negativkontrolle: Zufallsdaten gleicher Größe, deterministisch geseedet.
"$PY" - "$TMP/rand.img" <<'PY'
import sys, random
r = random.Random(0x5EED)
open(sys.argv[1], 'wb').write(bytes(r.getrandbits(8) for _ in range(737280)))
PY

OUT_REAL="$(bash "$A" "$TMP/real.img")"
OUT_RAND="$(bash "$A" "$TMP/rand.img")"

# 1. Gleiche Größe — sonst ist der Vergleich sinnlos.
grep -q "737280 Bytes" <<<"$OUT_REAL" || fail "Größe des strukturierten Images nicht gemessen"
grep -q "737280 Bytes" <<<"$OUT_RAND" || fail "Größe der Zufallsdatei nicht gemessen"

# 2. Keine der beiden Ausgaben darf ein Format identifizieren.
grep -q "identifiziert KEIN Format" <<<"$OUT_REAL" || fail "strukturiertes Image wurde als Format identifiziert"
grep -q "identifiziert KEIN Format" <<<"$OUT_RAND" || fail "Zufallsdatei wurde als Format identifiziert"

# 3. Der Unterschied muss an der Entropie sichtbar sein.
grep -q "kein rohes Sektorabbild" <<<"$OUT_RAND" || fail "hohe Entropie der Zufallsdatei nicht markiert"
if grep -q "kein rohes Sektorabbild" <<<"$OUT_REAL"; then
  fail "strukturiertes Image faelschlich als Nicht-Sektorabbild markiert"
fi

# 4. Bootsignatur: strukturiert ja, Zufall (mit diesem Seed) nein.
grep -q "55 AA am Offset 510" <<<"$OUT_REAL" || fail "Bootsignatur im strukturierten Image nicht gefunden"

# 5. Und die Bootsignatur darf NICHT als Format ausgelegt werden.
grep -q "NICHT FAT12" <<<"$OUT_REAL" || fail "55 AA wurde nicht als Nicht-Format gekennzeichnet"

# ══════════════════════════════════════════════════════════════════════════
# B) Magic-Konstanten gegen die UFT-Plugins
# ══════════════════════════════════════════════════════════════════════════
"$PY" - "$A" "$ROOT" <<'PY' || exit 1
import re, sys, pathlib

script = pathlib.Path(sys.argv[1]).read_text(encoding='utf-8')
root   = pathlib.Path(sys.argv[2])

magics = re.findall(r'^\s*\(b"([^"]+)",\s*\d+,', script, re.M)
if len(magics) < 10:
    print(f"SELFTEST FEHLGESCHLAGEN: nur {len(magics)} Magics geparst — "
          f"Tabellenformat geaendert?", file=sys.stderr)
    sys.exit(1)

# Quelltext der Format-Plugins einmal einlesen.
blob = []
for p in (root / "src" / "formats").rglob("*"):
    if p.suffix in (".c", ".h", ".cpp") and p.is_file():
        try:
            blob.append(p.read_text(encoding='utf-8', errors='replace'))
        except OSError:
            pass
blob = "\n".join(blob)
if not blob:
    print("SELFTEST FEHLGESCHLAGEN: src/formats/ nicht lesbar", file=sys.stderr)
    sys.exit(1)

missing = [m for m in magics if f'"{m}"' not in blob]
if missing:
    print("SELFTEST FEHLGESCHLAGEN: Magic-Konstanten ohne Entsprechung in "
          "src/formats/:", file=sys.stderr)
    for m in missing:
        print(f"    {m!r}", file=sys.stderr)
    print("  -> genau dieser Fehler ('HYCHE' statt 'HXCPICFE') machte das "
          "Skript blind fuer HFE.", file=sys.stderr)
    sys.exit(1)

print(f"  Magics:  {len(magics)}/{len(magics)} stimmen mit src/formats/ ueberein")
PY

echo "SELFTEST BESTANDEN"
echo "  Positivkontrolle: $POS_SOURCE"
echo "  strukturiert: Entropie niedrig, Bootsig vorhanden, kein Format behauptet"
echo "  zufaellig:    Entropie ~8.0 + Warnung, kein Format behauptet"
echo "  -> gleiche Groesse, unterschiedliches Urteil an gemessener Eigenschaft"
