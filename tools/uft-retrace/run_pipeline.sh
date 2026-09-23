#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
    echo "Aufruf: $0 EINGABE.zip ARBEITSVERZEICHNIS" >&2
    exit 2
fi

python3 -m uft_retrace inventory "$1" -o "$2"
python3 -m uft_retrace analyze "$2"
python3 -m uft_retrace report "$2" -o "$2/REPORT.md"
echo "Fertig: $2/REPORT.md"
