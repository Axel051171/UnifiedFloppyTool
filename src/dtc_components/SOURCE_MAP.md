# Source map from the supplied ARM64 material

The supplied decompiler output exposed 116 named functions. The reusable
technical areas were mapped as follows:

| Recovered symbol family | Independent module |
|---|---|
| `CBitBuffer::*` | `src/bitbuffer.c` |
| `MakeCRCTable`, `CalcCRC*` | `src/crc.c` |
| `CDiskEncoding::InitFM/InitMFM` | `src/encoding.c` |
| `InitGCRCBM`, `InitGCRApple*` | `src/encoding.c` |
| `InitGCRVorpal*`, `InitGCRVMax`, `InitGCR4Bit` | `src/encoding.c` |
| `CDiskEncoding::FindViolation` | `src/bitbuffer.c`, `src/protection.c` |
| `CCTRawCodec` lifecycle/info functions | `src/ctraw.c` independent container |
| `CDiskFile`, `CMemoryFile` | Standard C `FILE*` and caller-owned buffers |

No complete high-level IPF codec, CT Raw codec, disk-format detector,
protection database, weak-bit analyzer, or flux decoder was present among the
116 C functions. Corresponding package modules are conservative independent
implementations, not line-by-line translations or compatibility claims.

Decompiler defects deliberately removed include guessed global objects,
overlapping anonymous structures, invalid C++ member calls, bad signatures,
rotates standing in for shifts, and truncated stores such as the recovered
`WriteBitLE32` accepting only an 8-bit value.
