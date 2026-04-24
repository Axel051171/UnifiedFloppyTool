# XDF Restore — Deferred Files

Two v3.7.0 XDF files were intentionally NOT restored in this pass
because they depend on modules that don't exist in v4.1.x or break
Windows/MinGW portability.

## `uft_xdf_adapter.c` (256 LOC) — deferred

Depends on `include/uft/core/uft_error_codes.h` (14 KB) and its
companion `.c` (13 KB). Restoring those would pull a parallel error-
code system into the tree, conflicting with the SSOT-error work
closed by MF-003 in M1 (`data/errors.tsv` → `include/uft/uft_error.h`).

Re-implementing the adapter against the current SSOT error system
is a separate task — scope: ~50 LOC of error-code remapping.

The adapter's header `include/uft/xdf/uft_xdf_adapter.h` is restored
so consumers can see the API surface; they'll just get unresolved
references until the impl lands.

## `uft_xdf_api_impl.c` (1088 LOC) — deferred

Uses `<fnmatch.h>` (POSIX) for filename-pattern matching at
`uft_xdf_api.c:103` (`fnmatch(pattern, entry->d_name, 0)`). MinGW/
Windows does not provide this header. Restoration requires either:

  (a) A portable fnmatch replacement (small, ~80 LOC glob matcher)
  (b) `#ifdef _WIN32` guards around the fnmatch branches
  (c) A conditional include that falls back to our own globbing

The other ~1000 LOC of this file is portable and is the main
catalog-of-disk-images implementation. Well worth a follow-up.

## Restored successfully

- `uft_xdf_core.c` (875 LOC) — XDF file-format reader/writer core
- `uft_xdf_api.c` (910 LOC) — public API + probe functions for 6
  source formats (ADF, D64, IMG, ST, TRD, XDF)

Both compile clean under `-Wall -Wextra -Werror` after minor
unused-parameter fixes that v3.7 shipped with.
