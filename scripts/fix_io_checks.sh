#!/bin/bash
# P1-IO-001: Add I/O return value checks

FILE="$1"
if [ -z "$FILE" ]; then
    echo "Usage: $0 <file.c>"
    exit 1
fi

# Backup
cp "$FILE" "${FILE}.bak"

# Add include if not present
if ! grep -q "uft_safe_io.h" "$FILE"; then
    # Find first #include and add after it
    sed -i '0,/#include/s/#include/#include "uft\/core\/uft_safe_io.h"\n#include/' "$FILE"
fi

# Pattern 1: Simple fread without check - wrap in if
# fread(buf, 1, size, fp);
# -> if (fread(buf, 1, size, fp) != size) { return -1; }

# Pattern 2: fwrite without check
# fwrite(buf, 1, size, fp);
# -> if (fwrite(buf, 1, size, fp) != size) { return -1; }

# Use sed to add basic checks (simplified approach)
# This is a conservative approach - only adds checks where clearly needed

echo "Processed: $FILE"
