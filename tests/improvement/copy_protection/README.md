# improvement/copy_protection/

Tests that prove UFT detects and *names* historical copy-protection
schemes — `gw` captures flux faithfully but has no protection-analysis
stage at all, so it can never say "this track carries scheme X".

**Form: core-library C test, not pytest.** Same reasoning as the other
improvement categories — UFT is GUI-only with no Python bindings; the
test lives in `tests/`.

| Test | Proves | gw fails because |
|------|--------|------------------|
| `tests/test_protection_detection.c` :: longtrack (PROTEC, Silmarils, Protoscan) | `uft_longtrack_detect()` names each Amiga longtrack scheme from a track synthesised to that scheme's documented sync word / length / fill / signature | gw does not classify protection |
| `tests/test_protection_detection.c` :: longtrack negative | a normal-length track is **not** flagged — no invented protection | — (this guards the forensic mandate, not a gw gap) |
| `tests/test_protection_detection.c` :: Dungeon Master fuzzy bits | `uft_detect_dm_fuzzy_pattern()` recognises DM's ambiguous-timing + compensating-pair signature, and rejects a clean 4µs MFM track | gw does not classify protection |
| `tests/test_protection_detection.c` :: DM serial CRC | `uft_extract_dm_serial()` + `uft_calc_dm_serial_crc()` recover the DM sector-7 serial and report CRC validity — including flagging a corrupted CRC instead of silently "fixing" it | gw does not parse protection payloads |

Each fixture is synthesised in-process, deterministically, from the
published scheme parameters in `uft_longtrack.h` / `uft_fuzzy_bits.h`.
They are not captured disks and never claim to be.

## Scope note — the planned "Lenslok" case was dropped

`docs/TESTER_STRATEGY.md` §3 originally listed a `test_protection_lenslok`
case. Lenslok is a *physical lens-overlay* protection — there is nothing
on the disk flux to detect, and UFT has no Lenslok detector to exercise.
It was replaced by the longtrack-negative and DM-serial-CRC cases, which
test real UFT protection code against real signatures.

## Bug found while writing this test

Wiring the longtrack detector surfaced a genuine product bug
(fixed alongside the test, MF-227): `uft_longtrack_type_name()` /
`uft_longtrack_get_def()` were declared in `uft_longtrack.h` but never
defined (link error for any external caller), and the internal helpers
indexed `uft_longtrack_defs[type]` even though that table is packed
0-based while the enum starts at `UFT_LONGTRACK_UNKNOWN = 0` — so every
scheme was named as its neighbour (and `uft_longtrack_detect()`'s own
info string was mislabelled). Now resolved by `.type`-field search.
