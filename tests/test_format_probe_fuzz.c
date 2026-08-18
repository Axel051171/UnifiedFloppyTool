/**
 * @file test_format_probe_fuzz.c
 * @brief Adversarial input testing for every linked format probe (MF-392).
 *
 * `docs/KNOWN_ISSUES.md` FMT-7 records that all format probes bounds-check
 * their input — established by READING them. This file establishes it by
 * running them: every probe is fed random, truncated and near-miss input and
 * must survive it. Under the project's ASan/UBSan CI jobs that turns any
 * out-of-bounds read or undefined shift into a hard failure.
 *
 * Two entry points share one core:
 *   - LLVMFuzzerTestOneInput(), the standard libFuzzer signature, so this can
 *     be handed to clang -fsanitize=fuzzer or OSS-Fuzz unchanged. Compile with
 *     -DUFT_FUZZ_LIBFUZZER to drop the main() below.
 *   - a deterministic driver (default) that runs a fixed, reproducible set of
 *     inputs so the same coverage runs in ordinary ctest on any toolchain.
 *     No clang is required for that path.
 *
 * Three invariants are checked, all of them derived from documented contracts
 * rather than invented here:
 *   1. No crash and no undefined behaviour (enforced by the sanitizer jobs).
 *   2. `uft_format_plugin.h` documents confidence as 0-100; a probe that
 *      reports a match must stay inside that range.
 *   3. A probe must not modify the buffer it inspects. It takes a const
 *      pointer, and DESIGN_PRINCIPLES forbids silent alteration of data, so
 *      this is checked with a byte-exact copy rather than trusted.
 *
 * The header documents `size` as "min. 512". Inputs smaller than that are
 * therefore fed in a separate pass whose findings are REPORTED, not asserted —
 * a probe that assumes the documented minimum is not violating its contract.
 */

#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_d64;
extern const uft_format_plugin_t uft_format_plugin_d71;
extern const uft_format_plugin_t uft_format_plugin_d81;
extern const uft_format_plugin_t uft_format_plugin_g64;
extern const uft_format_plugin_t uft_format_plugin_hfe;
extern const uft_format_plugin_t uft_format_plugin_atr;
extern const uft_format_plugin_t uft_format_plugin_d88;
extern const uft_format_plugin_t uft_format_plugin_dc42;
extern const uft_format_plugin_t uft_format_plugin_imd;
extern const uft_format_plugin_t uft_format_plugin_atx;
extern const uft_format_plugin_t uft_format_plugin_cqm;
extern const uft_format_plugin_t uft_format_plugin_jv1;
extern const uft_format_plugin_t uft_format_plugin_xfd;
extern const uft_format_plugin_t uft_format_plugin_edsk;
extern const uft_format_plugin_t uft_format_plugin_msa;
extern const uft_format_plugin_t uft_format_plugin_nib;
extern const uft_format_plugin_t uft_format_plugin_woz;
extern const uft_format_plugin_t uft_format_plugin_do;
extern const uft_format_plugin_t uft_format_plugin_po;
extern const uft_format_plugin_t uft_format_plugin_img;
extern const uft_format_plugin_t uft_format_plugin_td0;
extern const uft_format_plugin_t uft_format_plugin_fdi;
extern const uft_format_plugin_t uft_format_plugin_ipf;
extern const uft_format_plugin_t uft_format_plugin_scp;
extern const uft_format_plugin_t uft_format_plugin_stx;
extern const uft_format_plugin_t uft_format_plugin_2img;
extern const uft_format_plugin_t uft_format_plugin_dsk_cpc;
extern const uft_format_plugin_t uft_format_plugin_adf;

static const uft_format_plugin_t *const PLUGINS[] = {
    &uft_format_plugin_d64,  &uft_format_plugin_d71,  &uft_format_plugin_d81,
    &uft_format_plugin_g64,  &uft_format_plugin_hfe,  &uft_format_plugin_atr,
    &uft_format_plugin_d88,  &uft_format_plugin_dc42, &uft_format_plugin_imd,
    &uft_format_plugin_atx,  &uft_format_plugin_cqm,  &uft_format_plugin_jv1,
    &uft_format_plugin_xfd,  &uft_format_plugin_edsk, &uft_format_plugin_msa,
    &uft_format_plugin_nib,  &uft_format_plugin_woz,  &uft_format_plugin_do,
    &uft_format_plugin_po,   &uft_format_plugin_img,  &uft_format_plugin_td0,
    &uft_format_plugin_fdi,  &uft_format_plugin_ipf,  &uft_format_plugin_scp,
    &uft_format_plugin_stx,  &uft_format_plugin_2img, &uft_format_plugin_dsk_cpc,
    &uft_format_plugin_adf,
};
#define PLUGIN_COUNT (sizeof(PLUGINS) / sizeof(PLUGINS[0]))

/* Findings are collected rather than aborting, so one run reports everything. */
static int g_violations = 0;
static int g_short_input_notes = 0;

static void check_one(const uft_format_plugin_t *p, const uint8_t *data,
                      size_t size, size_t file_size, int contract_size)
{
    if (!p || !p->probe) return;

    /* Invariant 3: the probe must leave the buffer untouched. */
    uint8_t *copy = (uint8_t *)malloc(size ? size : 1);
    if (!copy) return;
    memcpy(copy, data, size);

    int confidence = -12345;
    bool hit = p->probe(data, size, file_size, &confidence);

    if (memcmp(copy, data, size) != 0) {
        printf("  VIOLATION: %s modified its input buffer (size=%zu)\n",
               p->name ? p->name : "?", size);
        g_violations++;
    }

    if (hit) {
        if (confidence < 0 || confidence > 100) {
            if (contract_size) {
                printf("  VIOLATION: %s reported confidence %d, contract is 0-100"
                       " (size=%zu file_size=%zu)\n",
                       p->name ? p->name : "?", confidence, size, file_size);
                g_violations++;
            } else {
                g_short_input_notes++;
            }
        }
    }

    free(copy);
}

/** Feed one blob to every probe, with several plausible file_size values. */
static void run_all_probes(const uint8_t *data, size_t size)
{
    const int contract_size = (size >= 512);
    const size_t file_sizes[] = {
        size,          /* the honest case: buffer is the whole file */
        0,             /* empty file with a non-empty buffer        */
        174848,        /* a real D64 length                          */
        819200,        /* a real D81 length                          */
        143360,        /* DO / PO                                    */
        (size_t)-1,    /* absurd, must not be trusted for indexing   */
    };

    for (size_t i = 0; i < PLUGIN_COUNT; i++) {
        for (size_t f = 0; f < sizeof(file_sizes) / sizeof(file_sizes[0]); f++) {
            check_one(PLUGINS[i], data, size, file_sizes[f], contract_size);
        }
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size == 0) return 0;
    run_all_probes(data, size);
    return 0;
}

#ifndef UFT_FUZZ_LIBFUZZER

/* Deterministic 64-bit PRNG (xorshift64*), so a failing run is reproducible
 * from the seed printed below — no reliance on rand() or on time(). */
static uint64_t g_state = 0x9E3779B97F4A7C15ull;
static uint64_t next_rand(void)
{
    uint64_t x = g_state;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    g_state = x;
    return x * 0x2545F4914F6CDD1Dull;
}

/* Real signatures, so mutation starts from inputs that actually reach the
 * interesting branches instead of being rejected at byte 0. */
static const char *const SEEDS[] = {
    "GCR-1541", "HXCPICFE", "HXCHFEV3", "SCP", "AT8X", "TD", "td", "CAPS",
    "RSY", "2IMG", "EXTENDED CPC DSK", "MV - CPC", "IMD ", "FDI", "WOZ1",
    "WOZ2", "CQ", "\x96\x02", "\x0e\x0f",
};
#define SEED_COUNT (sizeof(SEEDS) / sizeof(SEEDS[0]))

#define BUF_MAX 4096

int main(void)
{
    printf("=== Adversarial probe testing, %zu plugins (MF-392) ===\n",
           PLUGIN_COUNT);
    printf("    seed 0x9E3779B97F4A7C15, deterministic\n");

    uint8_t buf[BUF_MAX];
    size_t iterations = 0;

    /* Pass 1 — pure random bytes across the whole size range. */
    for (int i = 0; i < 400; i++) {
        size_t size = (size_t)(next_rand() % BUF_MAX) + 1;
        for (size_t k = 0; k < size; k++) buf[k] = (uint8_t)(next_rand() & 0xFF);
        run_all_probes(buf, size);
        iterations++;
    }

    /* Pass 2 — real signature, then random noise, then random single-byte
     * corruption of the signature itself (the near-miss case). */
    for (size_t s = 0; s < SEED_COUNT; s++) {
        size_t sl = strlen(SEEDS[s]);
        if (sl == 0) sl = 2;
        for (int i = 0; i < 40; i++) {
            size_t size = 512 + (size_t)(next_rand() % (BUF_MAX - 512));
            memset(buf, 0, size);
            memcpy(buf, SEEDS[s], sl < size ? sl : size);
            for (size_t k = sl; k < size; k++)
                buf[k] = (uint8_t)(next_rand() & 0xFF);
            if (i % 3 == 0 && sl > 0)
                buf[next_rand() % sl] ^= (uint8_t)(1u << (next_rand() % 8));
            run_all_probes(buf, size);
            iterations++;
        }
    }

    /* Pass 3 — truncation: a valid signature cut to every length from 0 up.
     * This is where a probe that trusts file_size instead of size breaks. */
    for (size_t s = 0; s < SEED_COUNT; s++) {
        size_t sl = strlen(SEEDS[s]);
        memset(buf, 0, 600);
        memcpy(buf, SEEDS[s], sl);
        for (size_t cut = 1; cut <= 600; cut += 7) {
            run_all_probes(buf, cut);
            iterations++;
        }
    }

    /* Pass 4 — degenerate buffers: all zero, all 0xFF, alternating. */
    for (int variant = 0; variant < 3; variant++) {
        for (size_t size = 1; size <= BUF_MAX; size = size * 2 + 1) {
            uint8_t fill = (variant == 0) ? 0x00 : (variant == 1) ? 0xFF : 0xAA;
            memset(buf, fill, size);
            run_all_probes(buf, size);
            iterations++;
        }
    }

    printf("\n%zu input blobs x %zu plugins x 6 file_size values = %zu probe calls\n",
           iterations, PLUGIN_COUNT, iterations * PLUGIN_COUNT * 6);
    if (g_short_input_notes) {
        printf("note: %d out-of-range confidences occurred only below the "
               "documented 512-byte minimum — reported, not counted as failures\n",
               g_short_input_notes);
    }
    if (g_violations) {
        printf("\nFAILED: %d contract violations\n", g_violations);
        return 1;
    }
    printf("\nNo crash, no input mutation, confidence in range. OK\n");
    return 0;
}

#endif /* UFT_FUZZ_LIBFUZZER */
