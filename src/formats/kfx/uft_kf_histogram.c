/**
 * @file uft_kf_histogram.c
 * @brief KryoFlux Flux Histogram Analysis (from Aufit/KFStreamInfo)
 * @version 1.0.0
 *
 * Flux histogram generation and analysis for copy protection detection.
 * Based on DrCoolZic's KFStreamInfo.cs histogram feature from Aufit.
 *
 * A flux histogram shows the distribution of flux transition timing values.
 * Standard MFM encoding produces three characteristic peaks at 4µs, 6µs, 8µs
 * (for DD) or 2µs, 3µs, 4µs (for HD). Deviations indicate:
 *   - Copy protection (fuzzy bits, long tracks, weak sectors)
 *   - Damaged media (broadened peaks, noise floor)
 *   - Non-standard encoding (GCR, FM, custom schemes)
 *
 * Build standalone test:
 *   gcc -DKFHIST_SELF_TEST -Wall -Wextra -O2 -o test_kfhist \
 *       uft_kf_histogram.c -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

/*============================================================================
 * PUBLIC TYPES
 *============================================================================*/

/** Maximum flux value tracked in histogram (anything above is clamped) */
#define KF_HIST_MAX_FLUX  1024

/** Histogram bin for a single flux timing value */
typedef struct {
    uint32_t count;         /**< Number of occurrences */
    double   time_us;       /**< Time in microseconds */
} kf_hist_bin_t;

/** Peak detected in histogram */
typedef struct {
    int      flux_val;      /**< Flux value in sck ticks */
    double   time_us;       /**< Peak center in microseconds */
    uint32_t count;         /**< Count at peak */
    double   percentage;    /**< Percentage of total flux transitions */
    int      width;         /**< Peak width (FWHM in bins) */
} kf_hist_peak_t;

/** Encoding type detected from histogram analysis */
typedef enum {
    KF_ENC_UNKNOWN = 0,
    KF_ENC_MFM_DD,          /**< Standard MFM DD (250 kbit/s) */
    KF_ENC_MFM_HD,          /**< Standard MFM HD (500 kbit/s) */
    KF_ENC_MFM_ED,          /**< MFM ED (1 Mbit/s) */
    KF_ENC_FM_SD,           /**< FM SD (125 kbit/s) */
    KF_ENC_GCR,             /**< GCR encoding (C64/Apple) */
    KF_ENC_AMIGA_DD,        /**< Amiga MFM DD (variable) */
    KF_ENC_PROTECTED,       /**< Copy-protected (anomalous) */
    KF_ENC_EMPTY            /**< Unformatted/empty track */
} kf_encoding_t;

/** Protection anomaly flags */
#define KF_PROT_NONE        0x0000
#define KF_PROT_FUZZY_BITS  0x0001  /**< Bimodal distribution within expected peaks */
#define KF_PROT_LONG_TRACK  0x0002  /**< More flux transitions than standard */
#define KF_PROT_WEAK_SECTOR 0x0004  /**< Abnormally weak (scattered) region */
#define KF_PROT_NOISE_FLOOR 0x0008  /**< Elevated noise between peaks */
#define KF_PROT_EXTRA_PEAKS 0x0010  /**< Unexpected peaks outside MFM range */
#define KF_PROT_TIMING_SKEW 0x0020  /**< Peaks shifted from standard positions */

/** Complete histogram analysis result */
typedef struct {
    /* Raw histogram */
    kf_hist_bin_t bins[KF_HIST_MAX_FLUX];
    int           bin_count;        /**< Highest non-zero bin */
    uint32_t      total_flux;       /**< Total flux transitions */
    int           flux_min;         /**< Minimum flux value seen */
    int           flux_max;         /**< Maximum flux value seen */

    /* Peak detection */
    kf_hist_peak_t peaks[8];        /**< Detected peaks (max 8) */
    int            peak_count;

    /* Analysis */
    kf_encoding_t  encoding;        /**< Detected encoding type */
    uint32_t       prot_flags;      /**< Protection anomaly flags */
    double         noise_ratio;     /**< Ratio of inter-peak to peak counts */
    double         sck;             /**< Sample clock used for time conversion */
} kf_histogram_t;

/*============================================================================
 * HISTOGRAM COMPUTATION
 *============================================================================*/

/**
 * Build histogram from flux transition array.
 * Direct port of KFStreamInfo.cs histogram generation.
 *
 * @param flux_values  Array of flux timing values (in sck ticks)
 * @param flux_count   Number of entries
 * @param sck          Sample clock frequency (Hz)
 * @param hist         Output histogram structure
 */
static void kf_hist_build(const int* flux_values, int flux_count,
                          double sck, kf_histogram_t* hist)
{
    memset(hist, 0, sizeof(*hist));
    hist->sck = sck;
    hist->total_flux = (uint32_t)flux_count;
    hist->flux_min = 0x7FFFFFFF;
    hist->flux_max = 0;

    for (int i = 0; i < flux_count; i++) {
        int v = flux_values[i];
        if (v < 0) continue;

        if (v < hist->flux_min) hist->flux_min = v;
        if (v > hist->flux_max) hist->flux_max = v;

        int bin = (v < KF_HIST_MAX_FLUX) ? v : KF_HIST_MAX_FLUX - 1;
        hist->bins[bin].count++;
    }

    /* Fill time values and find highest bin */
    hist->bin_count = 0;
    for (int i = 0; i < KF_HIST_MAX_FLUX; i++) {
        hist->bins[i].time_us = (double)i / sck * 1e6;
        if (hist->bins[i].count > 0)
            hist->bin_count = i + 1;
    }
}

/*============================================================================
 * PEAK DETECTION
 *============================================================================*/

/**
 * Detect peaks in histogram using simple local-maximum with smoothing.
 * Peaks must have count > 1% of total and be local maximum over ±3 bins.
 */
static void kf_hist_find_peaks(kf_histogram_t* hist)
{
    hist->peak_count = 0;

    uint32_t threshold = hist->total_flux / 100;  /* 1% minimum */
    if (threshold < 10) threshold = 10;

    /* Smoothed values for peak detection */
    for (int i = 3; i < hist->bin_count - 3 && hist->peak_count < 8; i++) {
        /* 7-point average */
        uint32_t sum = 0;
        for (int j = -3; j <= 3; j++)
            sum += hist->bins[i + j].count;
        uint32_t avg = sum / 7;

        if (avg < threshold) continue;

        /* Check local maximum (raw values) */
        uint32_t center = hist->bins[i].count;
        int is_peak = 1;
        for (int j = -3; j <= 3; j++) {
            if (j == 0) continue;
            if (hist->bins[i + j].count > center) { is_peak = 0; break; }
        }
        if (!is_peak) continue;

        /* Compute FWHM */
        uint32_t half = center / 2;
        int left = i, right = i;
        while (left > 0 && hist->bins[left].count > half) left--;
        while (right < hist->bin_count - 1 && hist->bins[right].count > half) right++;

        kf_hist_peak_t* p = &hist->peaks[hist->peak_count];
        p->flux_val = i;
        p->time_us = hist->bins[i].time_us;
        p->count = center;
        p->percentage = (double)center * 100.0 / (double)hist->total_flux;
        p->width = right - left;
        hist->peak_count++;

        /* Skip past this peak */
        i = right + 2;
    }
}

/*============================================================================
 * ENCODING DETECTION
 *============================================================================*/

/**
 * Analyze peaks to determine encoding type and protection anomalies.
 *
 * Standard MFM DD peaks (300 RPM, sck ≈ 24 MHz):
 *   4µs → ~96 sck ticks  (bit cell)
 *   6µs → ~144 sck ticks (1.5× cell)
 *   8µs → ~192 sck ticks (2× cell)
 *
 * Standard MFM HD peaks:
 *   2µs → ~48 ticks
 *   3µs → ~72 ticks
 *   4µs → ~96 ticks
 */
static void kf_hist_analyze(kf_histogram_t* hist)
{
    hist->encoding = KF_ENC_UNKNOWN;
    hist->prot_flags = KF_PROT_NONE;
    hist->noise_ratio = 0.0;

    if (hist->peak_count == 0) {
        hist->encoding = KF_ENC_EMPTY;
        return;
    }

    /* Compute noise ratio: flux counts between peaks vs in peaks */
    uint32_t peak_total = 0, noise_total = 0;
    for (int i = 0; i < hist->bin_count; i++) {
        int in_peak = 0;
        for (int p = 0; p < hist->peak_count; p++) {
            int half_w = hist->peaks[p].width / 2 + 1;
            if (i >= hist->peaks[p].flux_val - half_w &&
                i <= hist->peaks[p].flux_val + half_w) {
                in_peak = 1;
                break;
            }
        }
        if (in_peak) peak_total += hist->bins[i].count;
        else noise_total += hist->bins[i].count;
    }
    if (peak_total > 0)
        hist->noise_ratio = (double)noise_total / (double)peak_total;

    /* Check peak positions against known encodings.
     * For robustness, find the best 3-peak match among all detected peaks,
     * not just the first three. Extra peaks may indicate protection. */
    if (hist->peak_count >= 3) {
        /* Try all combinations of 3 peaks from detected set */
        int mfm_dd_match = 0, mfm_hd_match = 0;

        for (int i = 0; i < hist->peak_count - 2 && !mfm_dd_match; i++) {
            for (int j = i + 1; j < hist->peak_count - 1 && !mfm_dd_match; j++) {
                for (int k = j + 1; k < hist->peak_count && !mfm_dd_match; k++) {
                    double p0 = hist->peaks[i].time_us;
                    double p1 = hist->peaks[j].time_us;
                    double p2 = hist->peaks[k].time_us;

                    /* MFM DD: peaks near 4/6/8 µs */
                    if (p0 > 3.3 && p0 < 4.7 &&
                        p1 > 5.3 && p1 < 6.7 &&
                        p2 > 7.3 && p2 < 8.7) {
                        mfm_dd_match = 1;
                    }
                }
            }
        }

        for (int i = 0; i < hist->peak_count - 2 && !mfm_hd_match; i++) {
            for (int j = i + 1; j < hist->peak_count - 1 && !mfm_hd_match; j++) {
                for (int k = j + 1; k < hist->peak_count && !mfm_hd_match; k++) {
                    double p0 = hist->peaks[i].time_us;
                    double p1 = hist->peaks[j].time_us;
                    double p2 = hist->peaks[k].time_us;

                    /* MFM HD: peaks near 2/3/4 µs */
                    if (p0 > 1.5 && p0 < 2.5 &&
                        p1 > 2.5 && p1 < 3.5 &&
                        p2 > 3.5 && p2 < 4.5) {
                        mfm_hd_match = 1;
                    }
                }
            }
        }

        if (mfm_dd_match) hist->encoding = KF_ENC_MFM_DD;
        else if (mfm_hd_match) hist->encoding = KF_ENC_MFM_HD;
        else {
            /* Amiga DD: looser match */
            double p0 = hist->peaks[0].time_us;
            double p1 = hist->peaks[1].time_us;
            double p2 = hist->peaks[2].time_us;
            if (p0 > 3.0 && p0 < 4.8 && p1 > 5.0 && p1 < 7.0 && p2 > 7.0 && p2 < 9.0)
                hist->encoding = KF_ENC_AMIGA_DD;
        }
    }
    else if (hist->peak_count == 2) {
        double p0 = hist->peaks[0].time_us;
        double p1 = hist->peaks[1].time_us;

        /* FM SD: peaks near 4/8 µs (only 2 peaks) */
        if (p0 > 3.5 && p0 < 4.5 && p1 > 7.5 && p1 < 8.5) {
            hist->encoding = KF_ENC_FM_SD;
        }
    }

    /* Protection anomaly detection */

    /* Noise floor check - 25% threshold (some scatter is normal) */
    if (hist->noise_ratio > 0.25) {
        hist->prot_flags |= KF_PROT_NOISE_FLOOR;
    }

    /* Extra peaks beyond expected 3 for MFM */
    if ((hist->encoding == KF_ENC_MFM_DD || hist->encoding == KF_ENC_MFM_HD)
        && hist->peak_count > 3) {
        hist->prot_flags |= KF_PROT_EXTRA_PEAKS;
    }

    /* Wide peaks suggest fuzzy/weak bits */
    for (int p = 0; p < hist->peak_count; p++) {
        if (hist->peaks[p].width > 12) {
            hist->prot_flags |= KF_PROT_FUZZY_BITS;
            break;
        }
    }

    /* Peak position shift from ideal */
    if (hist->encoding == KF_ENC_MFM_DD && hist->peak_count >= 3) {
        double ideal_ratio = hist->peaks[1].time_us / hist->peaks[0].time_us;
        if (fabs(ideal_ratio - 1.5) > 0.15) {
            hist->prot_flags |= KF_PROT_TIMING_SKEW;
        }
    }

    /* Only override encoding to PROTECTED if strong anomalies detected */
    if ((hist->prot_flags & (KF_PROT_EXTRA_PEAKS | KF_PROT_FUZZY_BITS | KF_PROT_TIMING_SKEW))
        && hist->encoding != KF_ENC_UNKNOWN) {
        hist->encoding = KF_ENC_PROTECTED;
    }
}

/*============================================================================
 * TEXT OUTPUT (like KFStreamInfo -h)
 *============================================================================*/

/**
 * Print histogram in KFStreamInfo format: flux_val, time_µs, count
 * Only non-zero entries are printed.
 */
static void kf_hist_print(const kf_histogram_t* hist, FILE* out)
{
    fprintf(out, "***** Histogram Information (non-null entries) *****\n");
    int entries = 0;
    for (int i = 0; i < hist->bin_count; i++) {
        if (hist->bins[i].count > 0) {
            fprintf(out, "histogram[%4d %10.3f] --> %u\n",
                    i, hist->bins[i].time_us, hist->bins[i].count);
            entries++;
        }
    }
    fprintf(out, "Table entries = %d\n\n", entries);
}

/**
 * Print analysis summary with encoding and protection info
 */
static void kf_hist_print_analysis(const kf_histogram_t* hist, FILE* out)
{
    static const char* enc_names[] = {
        "Unknown", "MFM DD", "MFM HD", "MFM ED", "FM SD",
        "GCR", "Amiga DD", "PROTECTED", "Empty/Unformatted"
    };

    fprintf(out, "***** Histogram Analysis *****\n");
    fprintf(out, "Encoding type: %s\n", enc_names[hist->encoding]);
    fprintf(out, "Peaks detected: %d\n", hist->peak_count);

    for (int i = 0; i < hist->peak_count; i++) {
        const kf_hist_peak_t* p = &hist->peaks[i];
        fprintf(out, "  Peak %d: %d sck (%.3f µs) count=%u (%.1f%%) width=%d\n",
                i, p->flux_val, p->time_us, p->count, p->percentage, p->width);
    }

    fprintf(out, "Noise ratio: %.3f\n", hist->noise_ratio);

    if (hist->prot_flags) {
        fprintf(out, "Protection flags:");
        if (hist->prot_flags & KF_PROT_FUZZY_BITS)  fprintf(out, " FUZZY");
        if (hist->prot_flags & KF_PROT_LONG_TRACK)  fprintf(out, " LONG_TRACK");
        if (hist->prot_flags & KF_PROT_WEAK_SECTOR)  fprintf(out, " WEAK");
        if (hist->prot_flags & KF_PROT_NOISE_FLOOR)  fprintf(out, " NOISY");
        if (hist->prot_flags & KF_PROT_EXTRA_PEAKS)  fprintf(out, " EXTRA_PEAKS");
        if (hist->prot_flags & KF_PROT_TIMING_SKEW)  fprintf(out, " SKEWED");
        fprintf(out, "\n");
    }
    fprintf(out, "\n");
}

/*============================================================================
 * CONVENIENCE: Full analysis pipeline
 *============================================================================*/

/**
 * Run complete histogram analysis on flux data.
 *
 * @param flux_values  Array of flux timing values (sck ticks)
 * @param flux_count   Number of entries
 * @param sck          Sample clock frequency (default: 24027428.57)
 * @param hist         Output histogram (caller allocated)
 */
static void kf_hist_analyze_flux(const int* flux_values, int flux_count,
                                 double sck, kf_histogram_t* hist)
{
    kf_hist_build(flux_values, flux_count, sck, hist);
    kf_hist_find_peaks(hist);
    kf_hist_analyze(hist);
}

/*============================================================================
 * SELF-TEST
 *============================================================================*/

#ifdef KFHIST_SELF_TEST

static int test_failures = 0;
#define TEST_ASSERT(cond, name) do { \
    if (!(cond)) { printf("  %-50s [FAIL]\n", name); test_failures++; } \
    else { printf("  %-50s [PASS]\n", name); } \
} while(0)

/* Generate synthetic MFM DD flux data (peaks at ~96, ~144, ~192 sck) */
static void gen_mfm_dd(int* flux, int* count) {
    int n = 0;
    /* 4µs peak (~96 sck) - largest */
    for (int i = 0; i < 15000; i++)
        flux[n++] = 93 + (i % 7);   /* 93-99, centered ~96 */
    /* 6µs peak (~144 sck) */
    for (int i = 0; i < 10000; i++)
        flux[n++] = 141 + (i % 7);  /* 141-147, centered ~144 */
    /* 8µs peak (~192 sck) */
    for (int i = 0; i < 5000; i++)
        flux[n++] = 189 + (i % 7);  /* 189-195, centered ~192 */
    /* Small amount of noise */
    for (int i = 0; i < 200; i++)
        flux[n++] = 50 + (i % 250);
    *count = n;
}

/* Generate protected track (extra peaks + high noise) */
static void gen_protected(int* flux, int* count) {
    int n = 0;
    /* Normal MFM DD peaks */
    for (int i = 0; i < 10000; i++)
        flux[n++] = 93 + (i % 7);
    for (int i = 0; i < 7000; i++)
        flux[n++] = 141 + (i % 7);
    for (int i = 0; i < 3500; i++)
        flux[n++] = 189 + (i % 7);
    /* Extra protection peak at ~120 sck (5µs — between standard peaks) */
    for (int i = 0; i < 2000; i++)
        flux[n++] = 117 + (i % 7);
    /* Lots of noise/scatter (fuzzy bits) */
    for (int i = 0; i < 5000; i++)
        flux[n++] = 60 + (i % 200);
    *count = n;
}

/* Generate empty track */
static void gen_empty(int* flux, int* count) {
    /* Very few random flux transitions */
    int n = 0;
    for (int i = 0; i < 20; i++)
        flux[n++] = 200 + (i * 17) % 300;
    *count = n;
}

int main(void) {
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║    KryoFlux Flux Histogram Self-Test (from Aufit)          ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    double sck = 24027428.5714285;  /* Standard KryoFlux sck */
    int* flux = (int*)malloc(50000 * sizeof(int));
    kf_histogram_t* hist = (kf_histogram_t*)malloc(sizeof(kf_histogram_t));
    int n;

    /* Test 1: Standard MFM DD */
    printf("--- MFM DD Detection ---\n");
    gen_mfm_dd(flux, &n);
    kf_hist_analyze_flux(flux, n, sck, hist);

    TEST_ASSERT(hist->total_flux == (uint32_t)n, "MFM DD: correct flux count");
    TEST_ASSERT(hist->peak_count == 3, "MFM DD: 3 peaks detected");
    TEST_ASSERT(hist->encoding == KF_ENC_MFM_DD, "MFM DD: encoding detected");
    TEST_ASSERT(hist->prot_flags == KF_PROT_NONE, "MFM DD: no protection flags");
    TEST_ASSERT(hist->noise_ratio < 0.20, "MFM DD: low noise ratio (<0.20)");

    if (hist->peak_count >= 3) {
        TEST_ASSERT(fabs(hist->peaks[0].time_us - 4.0) < 0.5,
                    "MFM DD: peak 0 near 4µs");
        TEST_ASSERT(fabs(hist->peaks[1].time_us - 6.0) < 0.5,
                    "MFM DD: peak 1 near 6µs");
        TEST_ASSERT(fabs(hist->peaks[2].time_us - 8.0) < 0.5,
                    "MFM DD: peak 2 near 8µs");
        TEST_ASSERT(hist->peaks[0].count > hist->peaks[1].count,
                    "MFM DD: peak 0 > peak 1 (expected)");
    }

    printf("\n  Full histogram output:\n");
    kf_hist_print_analysis(hist, stdout);

    /* Test 2: Protected track */
    printf("--- Protected Track Detection ---\n");
    gen_protected(flux, &n);
    kf_hist_analyze_flux(flux, n, sck, hist);

    TEST_ASSERT(hist->peak_count >= 4, "Protected: 4+ peaks detected");
    TEST_ASSERT(hist->encoding == KF_ENC_PROTECTED, "Protected: encoding=PROTECTED");
    TEST_ASSERT(hist->prot_flags & KF_PROT_EXTRA_PEAKS, "Protected: EXTRA_PEAKS flag");
    TEST_ASSERT(hist->noise_ratio > 0.10, "Protected: elevated noise ratio");

    kf_hist_print_analysis(hist, stdout);

    /* Test 3: Empty track */
    printf("--- Empty Track Detection ---\n");
    gen_empty(flux, &n);
    kf_hist_analyze_flux(flux, n, sck, hist);

    TEST_ASSERT(hist->peak_count == 0, "Empty: no peaks");
    TEST_ASSERT(hist->encoding == KF_ENC_EMPTY, "Empty: encoding=EMPTY");

    /* Test 4: Time conversion accuracy */
    printf("\n--- Time Conversion ---\n");
    int test_flux[] = {96};
    kf_hist_build(test_flux, 1, sck, hist);
    TEST_ASSERT(fabs(hist->bins[96].time_us - (96.0/sck*1e6)) < 0.001,
                "Time conversion: 96 sck → µs accurate");

    /* Cleanup */
    free(flux);
    free(hist);

    printf("\n══════════════════════════════════════════════════════════════\n");
    if (test_failures == 0) printf("  ALL TESTS PASSED\n");
    else printf("  %d TEST(S) FAILED\n", test_failures);
    printf("══════════════════════════════════════════════════════════════\n");

    return test_failures;
}

#endif /* KFHIST_SELF_TEST */
