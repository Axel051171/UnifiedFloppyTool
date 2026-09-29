# uft_capsimg_helper — IPF and CT-Raw through capsimg, outside UFT

A small separate program that reads an IPF or CT-Raw image through the
**SPS capsimg library** and answers in UFT's helper protocol
([`docs/specs/capsimg-helper/PROTOCOL.md`](../../docs/specs/capsimg-helper/PROTOCOL.md),
version 1). UFT finds it through the environment variable `UFT_IPF_HELPER`
and runs it as its own process.

## Why a separate program

Owner decision 2026-09-29: capsimg is used under its own licence, the
**SPS DECODER LIBRARY licence v1.02** (non-commercial;
`github.com/simonowen/capsimage`, `LICENCE.txt`). That licence does not fit
UFT's GPL-2.0-or-later, so UFT never links capsimg:

- this helper is **MIT**, UFT's side stays GPL;
- the helper **loads** `CAPSImg.dll` / `libcapsimage.so` at run time from a
  path the user provides — nothing of capsimg is shipped with UFT or with
  this helper, and nothing of it is compiled in;
- the two programs talk through files only (index + blob), never through
  a shared address space.

The capsimg declarations in the source (types, field order, function names,
flag bits) are interface facts from capsimg's public header `CapsAPI.h`,
declared anew — no capsimg code.

## Build

```
gcc -std=c11 -O2 -Wall -Wextra -o uft_capsimg_helper uft_capsimg_helper.c        # Windows (MinGW)
gcc -std=c11 -O2 -Wall -Wextra -o uft_capsimg_helper uft_capsimg_helper.c -ldl   # Linux
```

## Use

```
UFT_CAPSIMG_LIB = <path to CAPSImg.dll or libcapsimage.so.5>   (optional:
                  default is CAPSImg.dll next to the helper / system search)
UFT_IPF_HELPER  = <path to uft_capsimg_helper[.exe]>
```

UFT then opens `.ipf`, `.ct` and `.ctr` images through the helper first and
falls back to its own IPF reader when the helper is not set up.

## Measured (2026-09-29, MF-1613)

With the `CAPSImg.dll` shipped with greaseweazle 1.23 (win64):

| Image | Result |
|---|---|
| `tests/corpus_free/disk_analyse_uftk_amiga.ipf` (CAPS encoder, cyl 0..3) | rc 0, 84x2, 8 tracks with data |
| same source, 80 cylinders (local) | rc 0, 160 tracks with data |
| `sps_lethalxcess_a/b.ipf` (SPS encoder, local) | rc 0, 82x2, 160 tracks with data each |
| `tests/corpus_free/hxcfe_ibmdd.ipf` (the known hollow IPF, P3-358) | rc 5, `ERROR capsimg rejects the image (CAPSLockImage 14)`, no track (H3) |
| three CT-Raw images from archive.org (`flux_floppies`, local only) | rc 0, 84x2, 168 of 168 tracks with data |
| the same CT-Raw twice (H2) | index and blob byte-identical |

Cross-check against UFT's own IPF reader, per track: **160 of 160** tracks
carry the same cell count for `sps_lethalxcess_a.ipf` and for the
80-cylinder disk-analyse IPF — the writer (SPS / disk-analyse), capsimg
and UFT are three hands.

H4 (an IPF cut to 20 000 bytes): capsimg keeps every track header and
answers `CAPSLockTrack` rc 2 for 163 of 164 tracks. The helper therefore
treats a lock ERROR as a damaged image — rc 5, `ERROR track 0/1:
CAPSLockTrack 2 (image damaged or truncated)`, no TRACK line — while a
genuinely unformatted track (rc 0, no cells; cylinders 4..83 of the
4-cylinder file) stays listed with length 0.

H5 (blob path not writable): rc 6, `ERROR blob file not writable`, no
TRACK line.

Observed, not explained: capsimg sets the "flakey" flag on every track of
all three CT-Raw images; the helper passes it on unchanged (flags bit 0).

Not measured: the Linux build against `libcapsimage.so` (it compiles; it has
not run against a real library).
