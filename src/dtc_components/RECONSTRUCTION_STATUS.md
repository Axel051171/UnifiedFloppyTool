# Reconstruction status

| Requested area | Status |
|---|---|
| Disk encoding algorithms | Implemented core FM/MFM/GCR |
| CRC routines | Implemented and test-vector checked |
| Bit-buffer functions | Implemented |
| Flux timing | Implemented statistics and bit-cell conversion |
| Automatic image detection | Implemented for common containers |
| Sector/track analysis | Basic IBM MFM IDAM and Amiga sync scanning |
| Weak bits | Multi-revolution disagreement detector |
| Track matching | Shift-tolerant bit comparison |
| Protection detection | Long-track and illegal-run heuristics |
| Apple/Vorpal/V-Max | Encoding tables exposed; generic GCR engine |
| CT Raw | Safe independent container; proprietary compatibility unverified |
| IPF | Header/chunk inspection only |
| Complete IPF generation/decoding | Not recoverable from supplied C subset |
| Complete DTC format/protection database | Not present in supplied C subset |
| Original decompiled C repair | Replaced with typed C11 modules; original output is structurally uncompilable |

The supplied decompiler output recovers 116 named routines from a much larger
ARM64 text section. Several expressions are demonstrably mistranslated (types,
rotates, member calls and byte stores). Copying it wholesale would produce an
unsafe program, not a repaired source tree.

