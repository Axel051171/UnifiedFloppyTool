/**
 * @file test_corpus_protection_copylock.c
 * @brief First protection-scheme test against REAL protected code (MF-377).
 *
 * Corpus: three loader/program extracts from real 1988/89 Atari ST releases,
 * preserved in the dec0de project (github.com/orionfuzion/dec0de, commit
 * 54d7efd9, samples/ROBN88 + ROBN89, documented per-file in samples/README.txt).
 * Game code is copyrighted -> LOCAL-ONLY in tests/corpus/ (gitignored);
 * sha256 + reproducible download command in tests/corpus_manifest/manifest.json.
 *
 * Ground truth pinned by independent python inspection BEFORE any UFT run:
 *   dec0de_RAINBISL.BIN (Rainbow Islands, Series 2 variant A):
 *     init1 pattern @28, trampoline @2214, magic32 = 0x6B1D1929
 *   dec0de_WARLOCK.BIN  (Warlock, Series 2 variant B):
 *     init2 pattern hit, trampoline @1960, magic32 = 0x0
 *   dec0de_RICKD.BIN    (Rick Dangerous, Series 1 1988):
 *     BRA.S prefix present, but keydisk pattern 50F9 0000 043E absent in
 *     cleartext (lives inside the TVD-encrypted region) -> current Series-1
 *     detection CANNOT fire on real data. Pinned as negative; the independent
 *     scan found the same for ALL 16 real Series-1 samples in dec0de
 *     (see docs/KNOWN_ISSUES.md PROT-1).
 *
 * SKIPS (exit 77) when the local corpus files are absent (e.g. CI).
 */

#include "uft/protection/uft_atarist_copylock.h"

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

TEST(rick_dangerous_series1_undetectable) {
    /* Pins CURRENT behavior, not desired behavior: Series-1 keydisk/serial
     * patterns only exist after TVD decryption, so detection on the raw
     * protected code returns "not CopyLock". Documented as PROT-1. */
    size_t len = 0;
    uint8_t *d = load_corpus("dec0de_RICKD.BIN", &len);
    ASSERT(d != NULL && len == 4096);
    uft_copylock_st_result_t r;
    ASSERT(uft_copylock_st_analyze(d, len, &r) == 1);   /* "not CopyLock" */
    ASSERT(!r.detected);
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
    RUN(rick_dangerous_series1_undetectable);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
