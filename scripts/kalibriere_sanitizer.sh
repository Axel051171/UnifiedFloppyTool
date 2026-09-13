#!/usr/bin/env bash
# Eicht die Sanitizer-Stufen: koennen sie ueberhaupt rot werden? (MF-1102)
#
# Laeuft auf Linux oder in WSL. Auf der MinGW-Werkzeugkette dieses
# Rechners gibt es weder `libasan` noch `libubsan` ("cannot find -lasan"),
# deshalb sagt das Skript dort ab, statt eine Messung vorzutaeuschen.
#
# Bestanden heisst: BEIDE Faelle enden mit einem Kode ungleich 0 UND
# haben eine Diagnose auf stderr geschrieben. Der zweite Teil ist
# noetig, weil ein Abbruch aus einem anderen Grund (fehlende
# Bibliothek, Signal beim Start) sonst als Erfolg durchginge.
set -uo pipefail

WURZEL="$(cd "$(dirname "$0")/.." && pwd)"
QUELLE="$WURZEL/tests/kalibrierung/sanitizer_kann_rot.c"
CC="${CC:-gcc}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

[ -f "$QUELLE" ] || { echo "FEHLT: $QUELLE"; exit 2; }
command -v "$CC" >/dev/null 2>&1 || { echo "kein $CC"; exit 2; }

gut=0
gesamt=0

pruefe() {
    local name="$1" bin="$2" opts="$3"
    gesamt=$((gesamt + 1))
    ( cd "$TMP" && env $opts "./$bin" >/dev/null 2>"$TMP/err" )
    local kode=$?
    local bytes
    bytes=$(wc -c < "$TMP/err" | tr -d ' ')
    if [ "$kode" -ne 0 ] && [ "$bytes" -gt 0 ]; then
        gut=$((gut + 1))
        printf '  [GRUEN] %-44s Exit=%-4s Diagnose=%s Byte\n' \
               "$name" "$kode" "$bytes"
    else
        printf '  [ROT]   %-44s Exit=%-4s Diagnose=%s Byte\n' \
               "$name" "$kode" "$bytes"
        [ "$kode" -eq 0 ] && \
            echo "          -> die Stufe MELDET und laeuft weiter: gruen ohne Aussage"
        [ "$bytes" -eq 0 ] && \
            echo "          -> Abbruch OHNE Diagnose: ein anderer Grund, keine Messung"
    fi
}

echo "Eichung der Sanitizer-Stufen ($CC)"
echo

# Die Flags sind die aus .github/workflows/sanitizers.yml. Aendert sich
# der Workflow, gehoert diese Datei mit geaendert — sonst eicht sie
# etwas anderes als das, was gatet.
if "$CC" -g -O1 -fsanitize=address -fno-omit-frame-pointer \
        "$QUELLE" -o "$TMP/asan" 2>"$TMP/bau.err"; then
    pruefe "ASan: heap-buffer-overflow" "asan" \
           "ASAN_OPTIONS=detect_leaks=0:halt_on_error=0:exitcode=1:print_stacktrace=1"
else
    echo "  [UEBERSPRUNGEN] ASan nicht baubar:"
    sed 's/^/          /' "$TMP/bau.err" | head -3
fi

if "$CC" -g -O1 -DUFT_KALIB_UBSAN -fsanitize=undefined \
        -fno-sanitize-recover=all -fno-omit-frame-pointer \
        "$QUELLE" -o "$TMP/ubsan" 2>"$TMP/bau.err"; then
    pruefe "UBSan: signed integer overflow" "ubsan" \
           "UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1:exitcode=1"
else
    echo "  [UEBERSPRUNGEN] UBSan nicht baubar:"
    sed 's/^/          /' "$TMP/bau.err" | head -3
fi

# Gegenprobe zur Gegenprobe: OHNE -fno-sanitize-recover und mit
# halt_on_error=0 MUSS UBSan durchlaufen. Tut es das nicht, misst dieses
# Skript nicht das, was es zu messen glaubt.
if "$CC" -g -O1 -DUFT_KALIB_UBSAN -fsanitize=undefined \
        -fno-omit-frame-pointer "$QUELLE" -o "$TMP/ubsan_lax" 2>/dev/null; then
    gesamt=$((gesamt + 1))
    ( cd "$TMP" && \
      UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=0:exitcode=1 \
      ./ubsan_lax >/dev/null 2>"$TMP/err2" )
    k=$?
    b=$(wc -c < "$TMP/err2" | tr -d ' ')
    if [ "$k" -eq 0 ] && [ "$b" -gt 0 ]; then
        gut=$((gut + 1))
        printf '  [GRUEN] %-44s Exit=%-4s Diagnose=%s Byte\n' \
               "Gegenprobe: lax MUSS durchlaufen" "$k" "$b"
    else
        printf '  [ROT]   %-44s Exit=%-4s Diagnose=%s Byte\n' \
               "Gegenprobe: lax MUSS durchlaufen" "$k" "$b"
        echo "          -> der gemessene Unterschied stimmt so nicht mehr;"
        echo "             die Begruendung im Workflow gehoert nachgemessen"
    fi
fi

echo
echo "Eichung $gut/$gesamt"
[ "$gut" -eq "$gesamt" ] && [ "$gesamt" -gt 0 ] && exit 0
exit 1
