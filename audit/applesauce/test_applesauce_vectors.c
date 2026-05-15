/*
 * test_applesauce_vectors.c — D1 wire-protocol vector check (native backend).
 *
 * Compile-time assertion that UFT's Applesauce protocol constants equal
 * the official Applesauce reference values.
 *
 * SCOPE NOTE — the Applesauce protocol is an ASCII line protocol; its
 * "wire constants" are the single-char status responses ('.', '!', '+')
 * and the 8 MHz sample clock. The status-char constants live as
 * `static constexpr char` in applesauce_provider_v2.cpp (a C++ TU,
 * file-local) — they are NOT in a header and cannot be included here.
 * They are covered by diff.py (textual parse of the .cpp) instead.
 * THIS file covers the pure-C surface: include/uft/hal/uft_applesauce.h
 * — the UFT_AS_SAMPLE_CLOCK #define and the uft_as_format_t enum.
 *
 * It is the C twin of diff.py for the C-HAL header: if a future edit
 * changes the sample clock, the build breaks here.
 *
 * Reproduce:
 *   gcc -std=c11 -I../../include -fsyntax-only test_applesauce_vectors.c
 *
 * Reference provenance: "recalled" — the 8 MHz / 125 ns Applesauce
 * sample clock is a well-known, frequently-cited figure (Applesauce
 * hardware documentation). Not vendored. See
 * audit/applesauce/extract_ref.py PROVENANCE.
 */
#include "uft/hal/uft_applesauce.h"

/* -- Sample clock ----------------------------------------------------- *
 * The Applesauce samples flux at 8 MHz => 125 ns per tick. Both the
 * C-HAL #define and the C++ provider's AS_SAMPLE_CLOCK_HZ constexpr
 * (which diff.py validates) must equal this. If they drift apart, flux
 * timing is mis-scaled — exactly the silent error this gate catches.
 */
_Static_assert(UFT_AS_SAMPLE_CLOCK == 8000000,
    "Applesauce sample clock must be 8 MHz (125 ns per tick)");

/* 1e9 / 8e6 == 125 ns. Integer-exact, so assertable directly. */
_Static_assert(1000000000 / UFT_AS_SAMPLE_CLOCK == 125,
    "Applesauce tick resolution must be 125 ns (1e9 / 8 MHz)");

/* -- uft_as_format_t enum ordinal contract ---------------------------- *
 * UFT_AS_FMT_AUTO must be 0 (zero-init / auto-detect default). The
 * remaining members are consecutive. The provider does not pin a
 * specific non-zero default, so only AUTO carries a hard contract;
 * the rest are spot-checked for monotonicity.
 */
_Static_assert(UFT_AS_FMT_AUTO == 0,
    "uft_as_format_t: AUTO must be ordinal 0 (zero-init default)");
_Static_assert(UFT_AS_FMT_DOS32 == 1,
    "uft_as_format_t: DOS32 follows AUTO");
_Static_assert(UFT_AS_FMT_DOS33 == 2,
    "uft_as_format_t: DOS33 follows DOS32");
_Static_assert(UFT_AS_FMT_PRODOS == 3,
    "uft_as_format_t: PRODOS follows DOS33");
_Static_assert(UFT_AS_FMT_RAW > UFT_AS_FMT_AUTO,
    "uft_as_format_t: RAW is a real (non-AUTO) format code");

/* -- uft_as_drive_t enum --------------------------------------------- */
_Static_assert(UFT_AS_DRIVE_525 == 0,
    "uft_as_drive_t: 5.25\" drive is ordinal 0");
_Static_assert(UFT_AS_DRIVE_35 == 1,
    "uft_as_drive_t: 3.5\" drive is ordinal 1");

/* Translation unit needs one external symbol to be non-empty under -c. */
int uft_audit_applesauce_vectors_ok(void) { return 0; }
