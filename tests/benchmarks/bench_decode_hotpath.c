/**
 * @file bench_decode_hotpath.c
 * @brief Baseline for the two per-transition decode hot paths (MF-435).
 *
 * The tree had no benchmark of any kind — only a uft-benchmark skill that had
 * never been used. That is the wrong order: `uft_pll_process_flux_mfm()` does
 * double arithmetic once per flux transition and `flux_find_sync()` walks a
 * bitstream one bit at a time, and whether either matters was unknown because
 * nobody had measured. This establishes the numbers so a later change can be
 * judged instead of believed.
 *
 * It optimises nothing. A baseline that nobody can reproduce is worth less
 * than no baseline, so the rules from .claude/skills/uft-benchmark are
 * followed literally: fixed workload, seeded generator, three warmups,
 * eleven measurements, min/median/max (never mean), CLOCK_MONOTONIC, and
 * volatile sinks so the optimiser cannot delete the work.
 *
 * Workload sizing, from the medium rather than from a round number:
 *   A 5.25" DD disk turns at 300 rpm — 200 ms per revolution. At a 4 µs MFM
 *   bit cell that is 50000 bit cells, and a typical density gives roughly
 *   100000 flux transitions per revolution per side. WORKLOAD_TRANSITIONS is
 *   that, so one iteration equals one revolution of one track.
 *
 * The sync-search workload deserves a note. Random bytes contain a given
 * 16-bit pattern about every 65536 bits, so a naive random buffer would make
 * flux_find_sync() return after ~1.3 % of the data and measure almost
 * nothing. The buffer here is built so the pattern occurs ONLY in the last
 * 16 bits: the number is then the full-scan cost, which is both the worst
 * case and the one that matters when a track has no readable sync at all —
 * exactly the damaged-media case this tool exists for.
 */

#include "uft/uft_flux_pll.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* flux_find_sync lives in src/flux/uft_flux_decoder.c; its header pulls in a
 * larger dependency set than a benchmark needs. */
int flux_find_sync(const uint8_t *bits, size_t bit_count,
                   uint16_t pattern, size_t start_pos);

#define WORKLOAD_TRANSITIONS  100000u   /* one DD revolution, one side */
#define WORKLOAD_BITS         500000u   /* generous full-track bitstream */
#define MFM_SYNC_PATTERN      0x4489u
#define WARMUP_ITERATIONS     3
#define MEASURE_ITERATIONS    11

static uint64_t now_ns(void)
{
#if defined(CLOCK_MONOTONIC)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#else
#error "CLOCK_MONOTONIC required — clock() measures CPU time, not elapsed time"
#endif
}

static int cmp_u64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

/* Deterministic LCG. Never /dev/urandom: a benchmark that cannot be rerun
 * with the same input cannot show a regression. */
static uint32_t lcg(uint32_t *s)
{
    *s = *s * 1103515245u + 12345u;
    return *s >> 16;
}

/* --------------------------------------------------------------------------
 * Workload 1: flux transitions for the PLL
 *
 * MFM at a 4 µs cell has three legal intervals: 4, 6 and 8 µs
 * (UFT_FLUX_SHORT/MEDIUM/LONG_TIME). Real captures never hit them exactly, so
 * a small deterministic jitter is applied — without it the PLL's correction
 * branches never run and the measurement would flatter the code.
 * -------------------------------------------------------------------------- */
static void build_flux(double *out, size_t count)
{
    static const double interval[3] = {
        UFT_FLUX_SHORT_TIME, UFT_FLUX_MEDIUM_TIME, UFT_FLUX_LONG_TIME
    };
    uint32_t seed = 0x4E554654u;               /* "NUFT" */
    for (size_t i = 0; i < count; i++) {
        uint32_t r = lcg(&seed);
        /* ±2 % jitter, well inside UFT_FLUX_TOLERANCE (±12.5 %) */
        double jitter = 1.0 + ((double)(r & 0xFF) - 128.0) / 6400.0;
        out[i] = interval[r % 3u] * jitter;
    }
}

/* --------------------------------------------------------------------------
 * Workload 2: a bitstream whose only sync word sits at the very end
 * -------------------------------------------------------------------------- */
static void build_bits_sync_at_end(uint8_t *buf, size_t bit_count)
{
    size_t bytes = (bit_count + 7) / 8;
    uint32_t seed = 0x55465421u;               /* "UFT!" */
    for (size_t i = 0; i < bytes; i++)
        buf[i] = (uint8_t)lcg(&seed);

    /* Scrub every accidental occurrence of the pattern, then plant one. */
    uint16_t window = 0;
    for (size_t i = 0; i < bit_count; i++) {
        window = (uint16_t)((window << 1) | ((buf[i / 8] >> (7 - (i % 8))) & 1u));
        if (i >= 15 && window == MFM_SYNC_PATTERN) {
            buf[i / 8] ^= (uint8_t)(1u << (7 - (i % 8)));   /* break it */
            window ^= 1u;
        }
    }
    /* Plant the pattern in the final 16 bits so the scan runs to the end. */
    size_t at = bit_count - 16;
    for (int b = 0; b < 16; b++) {
        size_t p = at + (size_t)b;
        int bit = (MFM_SYNC_PATTERN >> (15 - b)) & 1;
        uint8_t mask = (uint8_t)(1u << (7 - (p % 8)));
        if (bit) buf[p / 8] |= mask; else buf[p / 8] &= (uint8_t)~mask;
    }
}

static void report(const char *name, uint64_t *s, double work, const char *unit)
{
    qsort(s, MEASURE_ITERATIONS, sizeof(s[0]), cmp_u64);
    uint64_t lo = s[0], med = s[MEASURE_ITERATIONS / 2], hi = s[MEASURE_ITERATIONS - 1];
    double spread = med ? (double)(hi - lo) * 100.0 / (double)med : 0.0;
    printf("  %-26s min %7.3f ms   median %7.3f ms   max %7.3f ms\n",
           name, lo / 1e6, med / 1e6, hi / 1e6);
    printf("  %-26s %.1f M%s/s at the median, spread %.0f %% of median%s\n",
           "", work / (med / 1e9) / 1e6, unit, spread,
           spread > 20.0 ? "  <-- NOISY, do not publish" : "");
}

int main(void)
{
    printf("=== Decode hot-path baseline (MF-435) ===\n");
    printf("workload: %u flux transitions = one DD revolution; "
           "%u bits full-track scan\n\n", WORKLOAD_TRANSITIONS, WORKLOAD_BITS);

    /* ---- 1. PLL, one call per flux transition --------------------------- */
    double *flux = malloc(WORKLOAD_TRANSITIONS * sizeof(double));
    /* Worst case the PLL can emit: every interval the longest, 4 cells each. */
    uint8_t *bits_out = malloc(WORKLOAD_TRANSITIONS);
    if (!flux || !bits_out) { fprintf(stderr, "alloc failed\n"); return 1; }
    build_flux(flux, WORKLOAD_TRANSITIONS);

    uint64_t samples[MEASURE_ITERATIONS];
    volatile int sink = 0;

    for (int it = 0; it < WARMUP_ITERATIONS + MEASURE_ITERATIONS; it++) {
        uft_pll_t pll;
        uft_pll_init_mfm_250k(&pll);
        size_t bit_pos = 0;
        memset(bits_out, 0, WORKLOAD_TRANSITIONS);

        uint64_t t0 = now_ns();
        int emitted = 0;
        for (size_t i = 0; i < WORKLOAD_TRANSITIONS; i++) {
            /* Stop before the output buffer would overflow — the point is the
             * per-transition cost, not how many bits fit. */
            if (bit_pos + 8 >= WORKLOAD_TRANSITIONS * 8u) break;
            emitted += uft_pll_process_flux_mfm(&pll, flux[i], bits_out, &bit_pos);
        }
        uint64_t t1 = now_ns();

        sink ^= emitted;
        if (it >= WARMUP_ITERATIONS) samples[it - WARMUP_ITERATIONS] = t1 - t0;
    }
    report("uft_pll_process_flux_mfm", samples,
           (double)WORKLOAD_TRANSITIONS, "transitions");

    /* ---- 2. sync search, full scan -------------------------------------- */
    size_t bytes = (WORKLOAD_BITS + 7) / 8;
    uint8_t *bits = malloc(bytes);
    if (!bits) { fprintf(stderr, "alloc failed\n"); return 1; }
    build_bits_sync_at_end(bits, WORKLOAD_BITS);

    /* The buffer must behave as advertised, or the number is meaningless. */
    int found = flux_find_sync(bits, WORKLOAD_BITS, MFM_SYNC_PATTERN, 0);
    if (found != (int)(WORKLOAD_BITS - 16)) {
        fprintf(stderr, "workload invalid: sync at %d, expected %u\n",
                found, WORKLOAD_BITS - 16);
        return 1;
    }

    for (int it = 0; it < WARMUP_ITERATIONS + MEASURE_ITERATIONS; it++) {
        uint64_t t0 = now_ns();
        int r = flux_find_sync(bits, WORKLOAD_BITS, MFM_SYNC_PATTERN, 0);
        uint64_t t1 = now_ns();
        sink ^= r;
        if (it >= WARMUP_ITERATIONS) samples[it - WARMUP_ITERATIONS] = t1 - t0;
    }
    report("flux_find_sync (full scan)", samples, (double)WORKLOAD_BITS, "bits");

    printf("\nsink=%d (keeps the optimiser honest)\n", sink);
    printf("Report as a RANGE with workload and environment — never a bare\n"
           "factor. See .claude/skills/uft-benchmark, Konfidenz-Regel.\n");

    free(flux); free(bits_out); free(bits);
    return 0;
}
