UFT Protection Analyzer Module
=============================

Files
-----
- protection_analyzer.h / protection_analyzer.c
    The analyzer "device" with the required 5-function interface:
      floppy_open / floppy_close / floppy_read / floppy_write / floppy_analyze_protection

- uft_protection_test.c
    CLI test tool that opens an image, analyzes, and exports IMD or ATX stub.

Reality check (no sugarcoating)
-------------------------------
1) Pure sector dumps (.IMG/.D64) do NOT contain analog copy-protection effects.
   So "weak bits" and real CRC faults usually cannot be proven from the dump.

2) This module therefore works with SIGNATURES + HEURISTICS:
   - string markers (VMAX / RAPIDLOK / SafeDisc / etc.)
   - suspicious filler sectors (0xF6/0x00/0xFF on protection tracks)
   - simple weak-bit pattern heuristics for Atari (very limited)

3) Export:
   - IMD: marks "bad CRC" sectors, which is actually supported in the IMD spec.
   - ATX: emitted as a UFT stub container (magic + JSON + raw payload),
          because correct ATX track encoding needs flux-level input.

Integration tip
---------------
Because this module matches your uniform interface, you can plug it into the
same function-pointer table as your other formats.

Later, your Greaseweazle writer can:
- read UFTATX1 JSON weakRegions
- for each weak region: call generate_flux_pattern() to synthesize flux timings
- perform a real flux write

Build & run
-----------
make
./uft_protection_test input.atr out.imd
./uft_protection_test --atx input.atr out.atx
