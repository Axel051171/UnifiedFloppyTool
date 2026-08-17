/**
 * @file test_corpus_protection_copylock.c
 * @brief Protection-scheme tests against REAL protected code (MF-377/379/380).
 *
 * Corpus: loader/program extracts from real 1988/89 Atari ST releases,
 * preserved in the dec0de project (github.com/orionfuzion/dec0de, commit
 * 54d7efd9, samples/ROBN88 + ROBN89, documented per-file in samples/README.txt).
 * Game code is copyrighted -> LOCAL-ONLY in tests/corpus/ (gitignored);
 * sha256 + reproducible download command in tests/corpus_manifest/manifest.json.
 *
 * Ground truth pinned by independent python inspection BEFORE any UFT run:
 *   dec0de_RAINBISL.BIN (Rainbow Islands, Series 2 variant a):
 *     init1 pattern @28, trampoline @2214, magic32 = 0x6B1D1929
 *   dec0de_WARLOCK.BIN  (Warlock, Series 2 variant b):
 *     init2 pattern hit, trampoline @1960, magic32 = 0x0
 *   dec0de_XENON2.BIN   (Xenon 2, Series 1 variant e):
 *     TVD prolog @362, keydisk (decoded) @918, serial @598
 *   dec0de_COSMIC.BIN   (Cosmic Pirate, Series 1 variant e):
 *     TVD prolog @356, keydisk @534, serial @510
 *   dec0de_RICKD.BIN    (Rick Dangerous, Series 1 variant a):
 *     TVD prolog @420, NO keydisk instruction at all
 *
 * Coverage of the underlying detector over the full dec0de sample set:
 * Series 1 16/16, Series 2 14/14, with 0 false positives across the 15
 * non-CopyLock protections. See docs/KNOWN_ISSUES.md PROT-1.
 *
 * SKIPS (exit 77) when the local corpus files are absent (e.g. CI).
 */

#include "uft/protection/uft_atarist_copylock.h"
#include "uft/protection/uft_atarist_dec0de.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef UFT_CORPUS_RESTRICTED_DIR
#error "UFT_CORPUS_RESTRICTED_DIR must be defined by the build"
#endif

#define SKIP_EXIT 77

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-30s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

static uint8_t *load_corpus(const char *name, size_t *len) {
    char p[512];
    snprintf(p, sizeof(p), "%s/%s", UFT_CORPUS_RESTRICTED_DIR, name);
    FILE *f = fopen(p, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz <= 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc((size_t)sz);
    if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf); fclose(f); return NULL;
    }
    fclose(f);
    *len = (size_t)sz;
    return buf;
}

TEST(rainbow_islands_series2_variant_a) {
    size_t len = 0;
    uint8_t *d = load_corpus("dec0de_RAINBISL.BIN", &len);
    ASSERT(d != NULL && len == 2426);
    uft_copylock_st_result_t r;
    ASSERT(uft_copylock_st_analyze(d, len, &r) == 0);
    ASSERT(r.detected);
    ASSERT(r.series == UFT_COPYLOCK_SERIES_2_1989);
    ASSERT(r.variant == UFT_COPYLOCK_VARIANT_A);
    ASSERT(r.start_off == 2214);           /* pinned trampoline offset */
    ASSERT(r.magic32 == 0x6B1D1929u);      /* pinned TVD magic value  */
    free(d);
}

TEST(warlock_series2_variant_b) {
    size_t len = 0;
    uint8_t *d = load_corpus("dec0de_WARLOCK.BIN", &len);
    ASSERT(d != NULL && len == 2164);
    uft_copylock_st_result_t r;
    ASSERT(uft_copylock_st_analyze(d, len, &r) == 0);
    ASSERT(r.detected);
    ASSERT(r.series == UFT_COPYLOCK_SERIES_2_1989);
    ASSERT(r.variant == UFT_COPYLOCK_VARIANT_B);
    ASSERT(r.start_off == 1960);           /* pinned trampoline offset */
    ASSERT(r.magic32 == 0x0u);             /* pinned: plain XOR-chain  */
    free(d);
}

/* MF-379: Series-1 detection now matches THROUGH the TVD encryption, so the
 * keydisk instruction is found where a raw byte search saw nothing. Offsets
 * pinned by independent python inspection before the C code was touched. */
TEST(xenon2_series1_detected) {
    size_t len = 0;
    uint8_t *d = load_corpus("dec0de_XENON2.BIN", &len);
    ASSERT(d != NULL && len == 1724);
    uft_copylock_st_result_t r;
    ASSERT(uft_copylock_st_analyze(d, len, &r) == 0);
    ASSERT(r.detected);
    ASSERT(r.series == UFT_COPYLOCK_SERIES_1_1988);
    ASSERT(r.type == UFT_COPYLOCK_TYPE_INTERNAL);
    ASSERT(r.variant == UFT_COPYLOCK_VARIANT_E);   /* tvd1, no switch prolog */
    ASSERT(r.start_off == 362);            /* pinned TVD prolog          */
    ASSERT(r.keydisk_off == 918);          /* pinned: st $43e.l, decoded */
    ASSERT(r.serial_off == 598);           /* pinned: move.l d0,$1c(a0)  */
    free(d);
}

TEST(cosmic_pirate_series1_detected) {
    size_t len = 0;
    uint8_t *d = load_corpus("dec0de_COSMIC.BIN", &len);
    ASSERT(d != NULL && len == 1070);
    uft_copylock_st_result_t r;
    ASSERT(uft_copylock_st_analyze(d, len, &r) == 0);
    ASSERT(r.detected);
    ASSERT(r.series == UFT_COPYLOCK_SERIES_1_1988);
    ASSERT(r.variant == UFT_COPYLOCK_VARIANT_E);
    ASSERT(r.start_off == 356);
    ASSERT(r.keydisk_off == 534);
    ASSERT(r.serial_off == 510);
    free(d);
}

/* MF-380: this case was an honest NEGATIVE in MF-377/379 — Rick Dangerous
 * carries no keydisk instruction, so neither a raw nor a decrypted search for
 * it could fire. Detection now runs on the cleartext TVD prolog, which every
 * Series-1 loader must carry, so the assert flipped from "not detected" to
 * "detected, variant a". That is the intended lifecycle of a pinned negative:
 * it goes red when someone fixes the gap, and then it gets turned around. */
TEST(rick_dangerous_series1_detected_via_prolog) {
    size_t len = 0;
    uint8_t *d = load_corpus("dec0de_RICKD.BIN", &len);
    ASSERT(d != NULL && len == 4096);
    uft_copylock_st_result_t r;
    ASSERT(uft_copylock_st_analyze(d, len, &r) == 0);
    ASSERT(r.detected);
    ASSERT(r.series == UFT_COPYLOCK_SERIES_1_1988);
    ASSERT(r.variant == UFT_COPYLOCK_VARIANT_A);   /* switchsupill + tvd1 */
    ASSERT(r.start_off == 420);                    /* pinned TVD prolog */
    ASSERT(r.keydisk_off < 0);                     /* genuinely absent here */
    free(d);
}

/* The header-only dec0de port (uft_atarist_dec0de.h) carries a SECOND,
 * independent implementation of the Series-2 trampoline search. Verify it
 * against the same pinned ground truth — two implementations agreeing on real
 * data is stronger evidence than either alone. (That module's .c file held a
 * fabricated detector, uft_dec0de_detect(), which got 0 of 34 real samples
 * right; it was deleted in MF-379 — see docs/KNOWN_ISSUES.md PROT-3. Only the
 * header primitives survived, and this is one of them.) */
TEST(header_port_finds_same_trampolines) {
    size_t len = 0;
    uint32_t magic = 0;
    ssize_t prog_off = -1;

    uint8_t *d = load_corpus("dec0de_RAINBISL.BIN", &len);
    ASSERT(d != NULL);
    ASSERT(uft_robn89_find_start(d, len, &magic, &prog_off) == 2214);
    ASSERT(magic == 0x6B1D1929u);
    free(d);

    d = load_corpus("dec0de_WARLOCK.BIN", &len);
    ASSERT(d != NULL);
    magic = 0xFFFFFFFFu;
    ASSERT(uft_robn89_find_start(d, len, &magic, &prog_off) == 1960);
    ASSERT(magic == 0x0u);
    free(d);
}

int main(void) {
    size_t len = 0;
    uint8_t *probe = load_corpus("dec0de_RAINBISL.BIN", &len);
    if (!probe) {
        printf("SKIP: local protection corpus absent (%s/dec0de_*.BIN)\n"
               "      re-fetch per tests/corpus_manifest/manifest.json\n",
               UFT_CORPUS_RESTRICTED_DIR);
        return SKIP_EXIT;
    }
    free(probe);
    printf("=== Real-corpus protection: Rob Northen CopyLock ST (MF-377) ===\n");
    RUN(rainbow_islands_series2_variant_a);
    RUN(warlock_series2_variant_b);
    RUN(xenon2_series1_detected);
    RUN(cosmic_pirate_series1_detected);
    RUN(rick_dangerous_series1_detected_via_prolog);
    RUN(header_port_finds_same_trampolines);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
