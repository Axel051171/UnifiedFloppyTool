#!/bin/bash
# UFT Security Fix Script - sprintf → snprintf Migration
# 110% Haftungsmodus
# Datum: 2026-01-04

set -e

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║  UFT Security Fix: sprintf → snprintf Migration              ║"
echo "║  110% Haftungsmodus - Industrial Grade                       ║"
echo "╚═══════════════════════════════════════════════════════════════╝"

TARGET_FILE="src/visualization/display_track.c"
BACKUP="${TARGET_FILE}.bak"

if [ ! -f "$TARGET_FILE" ]; then
    echo "FEHLER: $TARGET_FILE nicht gefunden!"
    exit 1
fi

# Backup erstellen
cp "$TARGET_FILE" "$BACKUP"
echo "✓ Backup erstellt: $BACKUP"

# Zähle vorher
BEFORE=$(grep -c "sprintf(" "$TARGET_FILE" || true)
echo "➤ sprintf Aufrufe vorher: $BEFORE"

# Fix 1: tmp_str (256 bytes) - einfache Fälle
sed -i 's/sprintf(tmp_str,/snprintf(tmp_str, sizeof(tmp_str),/g' "$TARGET_FILE"

# Fix 2: tempstr (512 bytes) - einfache Fälle
sed -i 's/sprintf(tempstr,/snprintf(tempstr, sizeof(tempstr),/g' "$TARGET_FILE"

# Fix 3: tempstr2 - falls vorhanden
sed -i 's/sprintf(tempstr2,/snprintf(tempstr2, sizeof(tempstr2),/g' "$TARGET_FILE"

# Zähle nachher
AFTER=$(grep -c "sprintf(" "$TARGET_FILE" || true)
echo "➤ sprintf Aufrufe nachher: $AFTER"
echo "➤ Fixes angewendet: $((BEFORE - AFTER))"

if [ "$AFTER" -gt 0 ]; then
    echo ""
    echo "⚠️  Verbleibende sprintf Aufrufe (manuelle Prüfung empfohlen):"
    grep -n "sprintf(" "$TARGET_FILE" | head -10
fi

echo ""
echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║  ✅ Security Fix abgeschlossen                                ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
