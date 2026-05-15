#!/usr/bin/env python3
"""
extract_ref.py — the official Applesauce serial-protocol reference.

Emits the same JSON shape as extract_uft.py so diff.py can compare them
key-by-key.

PROVENANCE: grade = "recalled" for the well-established figures,
"needs-source" for the line-protocol command vocabulary.

The Applesauce floppy controller (Evolution Interactive / John Googin)
speaks an ASCII line protocol over a USB-CDC serial connection. The
protocol is documented at wiki.applesaucefdc.com. UFT's V1 provider
(applesaucehardwareprovider.cpp, since deleted in P1.18) was written
against that spec; the V2 provider's doc-comments reproduce the command
vocabulary and the single-char status responses.

Reference layers:

  1. Sample clock — grade "recalled". The Applesauce samples flux at
     8 MHz (125 ns per tick). This is a well-known, frequently-cited
     figure (it is in the Applesauce documentation and in every
     description of the hardware). Not vendored.

  2. Response status characters ('.' OK, '!' error, '+' on, '-' off,
     '?' unknown, 'v' no power) — grade "recalled". Reproduced from the
     V2 provider's own audit doc-comment (applesauce_provider_v2.h:11-16,
     which summarises the V1 implementation written against the spec)
     and from general knowledge of the protocol. Not vendored from
     wiki.applesaucefdc.com.

  3. Serial line parameters (115200 8N1) — grade "recalled".

  4. Device buffer sizes (163840 standard / 430080 for Applesauce+) —
     grade "recalled" (from the V1 detectDrive() implementation summary).

  5. Command-string vocabulary (sync:on, disk:read, head:track N, ...) —
     grade "needs-source". The exact command grammar can only be
     established from wiki.applesaucefdc.com, which is NOT vendored. The
     audit verifies that the command tokens UFT's doc-comments use are
     internally consistent and plausible, but cannot byte-diff them
     against an official grammar. The command_vocab section is therefore
     graded UNVERIFIED in the diff.

Upgrading: vendor a snapshot of the wiki.applesaucefdc.com protocol page
(or the Applesauce SDK if one is published) to lift the command vocab
to "vendored" and the rest from "recalled" to "vendored".
"""

import json
import sys

PROVENANCE = {
    "grade": "mixed",
    "sample_clock": "recalled",
    "response_chars": "recalled",
    "serial_params": "recalled",
    "buffers": "recalled",
    "command_vocab": "needs-source",
    "project": "Applesauce — Evolution Interactive / John Googin",
    "files_recalled_from": ["applesauce_provider_v2.h doc-comment "
                            "(V1 implementation summary)",
                            "general Applesauce hardware documentation"],
    "files_needed": ["wiki.applesaucefdc.com protocol page snapshot "
                     "(command-string grammar)"],
    "note": "8 MHz clock + status chars + 115200-8N1 are well-known; "
            "the line-protocol command grammar is needs-source. "
            "Not vendored — see audit/README.md.",
}

# Response status characters — recalled.
RESPONSE_CHARS = {
    "RESP_OK":    ord("."),   # '.' = command OK
    "RESP_ERROR": ord("!"),   # '!' = error
    "RESP_ON":    ord("+"),   # '+' = on / write-protect active
}

# Sample clock — recalled (8 MHz, 125 ns/tick).
TIMING = {
    "AS_SAMPLE_CLOCK_HZ": 8000000,
    "AS_SAMPLE_NS_derived": 125.0,            # 1e9 / 8e6
    "UFT_AS_SAMPLE_CLOCK_hal": 8000000,       # C-HAL #define must agree
}

# Device buffer sizes — recalled.
BUFFERS = {
    "AS_BUFFER_STANDARD": 163840,   # 160K — standard Applesauce
    "AS_BUFFER_PLUS":     430080,   # 420K — Applesauce+
}

# Serial line parameters — recalled.
SERIAL_PARAMS = {
    "baud": 115200,
    "framing_8N1_documented": 1,    # 8 data bits, no parity, 1 stop
}

# Command-string vocabulary — needs-source. The reference here is only
# the *set of expected tokens*; the exact grammar is not vendored, so
# diff.py marks every command_vocab row UNVERIFIED. Presence of the
# token in UFT (value 1) is recorded for completeness.
COMMAND_VOCAB = {
    tok: {"ref": 1, "kind": "needs-source"}
    for tok in (
        "sync:on", "sync:off", "disk:read", "disk:readx", "data:?size",
        "disk:?write", "data:clear", "disk:write", "motor:on", "motor:off",
        "head:track", "head:side", "head:zero", "sync:?speed", "?kind",
        "data:?max", "psu:?5v", "psu:?12v",
    )
}


def main():
    json.dump({
        "_provenance": PROVENANCE,
        "response_chars": RESPONSE_CHARS,
        "timing": TIMING,
        "buffers": BUFFERS,
        "serial_params": SERIAL_PARAMS,
        "command_vocab": COMMAND_VOCAB,
    }, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
