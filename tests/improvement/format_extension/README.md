# improvement/format_extension/

Tests that prove UFT decodes formats outside `gw`'s set.

**Form: core-library C test, not pytest.** Same reasoning as the other
improvement categories — UFT is GUI-only with no Python bindings; the
test lives in `tests/`.

| Test | Proves | gw fails because |
|------|--------|------------------|
| `tests/test_format_extension.c` :: `test_rdb_buffer_parse` | `uft_hdf_parse_rdb()` decodes an Amiga Rigid Disk Block — geometry, block lists, vendor strings — and rejects a non-RDSK / undersized buffer | gw is a floppy-flux tool; an Amiga hard-disk RDB is not in its world |
| `tests/test_format_extension.c` :: `test_partition_buffer_parse` | `uft_hdf_parse_partition()` decodes a PART block — name, block size, cylinder range, DOS type, bootable flag | gw has no partition-table concept |
| `tests/test_format_extension.c` :: `test_fs_type_names` | `uft_hdf_fs_type_name()` maps the DOS\x identifiers (OFS/FFS/FFS-INTL/…) | — (table coverage) |
| `tests/test_format_extension.c` :: `test_hdf_file_end_to_end` | `uft_hdf_parse()` walks a full synthetic HDF file: finds the RDB, follows the partition list, returns geometry + the partition | gw cannot open an HDF at all |

The fixture is a synthetic HDF built in-process, byte-exact to the
documented big-endian RDB / PART block layout. It is not a captured
disk image and never claims to be. There is no `gw` side to diff
against — the assertion is purely on UFT's decode of a known-good
fixture, which is exactly the point of a format-extension test.

## Scope vs. the original TESTER_STRATEGY plan

`docs/TESTER_STRATEGY.md` §3 named IPF (SPS/CAPS), Atari STX, KryoFlux
stream and Japanese (D88/D77/NFD/…) decode. HDF/RDB is delivered first
because it has a real, self-contained, **buffer-based** parser
(`src/formats/uft_hdf_parser.c`) that can be exercised honestly from a
synthetic fixture with no file-format reverse-engineering risk. The
remaining four are still open:

- **D88/D77** — `src/formats/d88/uft_d88_parser_v2.c` is a real parser
  but a standalone program (`main()`, file-based `d88_open()`), not a
  library TU; it needs a small refactor before it is test-linkable.
- **IPF / STX / KryoFlux-stream** — parsers exist under `src/formats/`
  but were not yet audited for a clean, synthesisable entry point.

Adding any of these is a follow-up increment in this same category.
