/**
 * @file floppy_otdr.c
 * @brief OTDR-Style Floppy Disk Signal Analysis — Implementation
 *
 * Core analysis engine: PLL simulation, jitter measurement,
 * quality profiling, event detection, weak-bit analysis,
 * copy protection detection, and export functions.
 */

#include "uft/analysis/floppy_otdr.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

/* ═══════════════════════════════════════════════════════════════════════
 * Internal helpers
 * ═══════════════════════════════════════════════════════════════════════ */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/** Comparison function for qsort (float, ascending) */
static int cmp_float(const void *a, const void *b) {
    float fa = *(const float *)a;
    float fb = *(const float *)b;
    return (fa > fb) - (fa < fb);
}

/** Clamp float to range */
static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/** Gaussian weight for window position */
static inline float gaussian_weight(int pos, int center, float sigma) {
    float d = (float)(pos - center);
    return expf(-(d * d) / (2.0f * sigma * sigma));
}

/** Get nominal interval for encoding */
static uint32_t nominal_2t_ns(otdr_encoding_t enc) {
    switch (enc) {
        case OTDR_ENC_MFM_DD:
        case OTDR_ENC_AMIGA_DD:
            return OTDR_MFM_2US_NS;
        case OTDR_ENC_MFM_HD:
            return OTDR_MFM_HD_2T_NS;
        case OTDR_ENC_FM_SD:
            return OTDR_FM_SHORT_NS;
        default:
            return OTDR_MFM_2US_NS;
    }
}

/** Add event to track event list */
static void add_event(otdr_track_t *track, otdr_event_type_t type,
                       otdr_severity_t severity, uint32_t position,
                       uint32_t flux_idx, uint32_t length,
                       float magnitude, const char *desc) {
    if (track->event_count >= OTDR_MAX_EVENTS)
        return;
    otdr_event_t *evt = &track->events[track->event_count++];
    memset(evt, 0, sizeof(*evt));
    evt->type = type;
    evt->severity = severity;
    evt->position = position;
    evt->flux_index = flux_idx;
    evt->length = length;
    evt->magnitude = magnitude;
    evt->loss_db = otdr_quality_to_db(magnitude);
    evt->sector_id = -1;
    if (desc)
        snprintf(evt->desc, sizeof(evt->desc), "%s", desc);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Configuration
 * ═══════════════════════════════════════════════════════════════════════ */

void otdr_config_defaults(otdr_config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->encoding = OTDR_ENC_AUTO;
    cfg->rpm = 300;
    cfg->expected_sectors = 0; /* auto-detect */

    cfg->pll_bandwidth = 0.04;
    cfg->pll_damping = 0.707;  /* critically damped */
    cfg->pll_lock_threshold = 10.0;

    cfg->detect_weak_bits = true;
    cfg->detect_protection = true;
    cfg->generate_heatmap = true;
    cfg->heatmap_resolution = 1024;

    cfg->smooth_window = OTDR_WINDOW_SIZE;
    cfg->use_gaussian = true;

    cfg->noflux_threshold = OTDR_NOFLUX_THRESHOLD;
    cfg->weak_bit_cv = OTDR_WEAK_BIT_CV;
    cfg->jitter_spike_threshold = 25.0;
}

void otdr_config_for_platform(otdr_config_t *cfg, const char *platform) {
    otdr_config_defaults(cfg);

    if (!platform)
        return;

    if (strcmp(platform, "atari_st") == 0) {
        cfg->encoding = OTDR_ENC_MFM_DD;
        cfg->rpm = 300;
        cfg->expected_sectors = 9;
    } else if (strcmp(platform, "atari_st_11") == 0) {
        cfg->encoding = OTDR_ENC_MFM_DD;
        cfg->rpm = 300;
        cfg->expected_sectors = 11; /* extended format */
    } else if (strcmp(platform, "atari_falcon_hd") == 0) {
        cfg->encoding = OTDR_ENC_MFM_HD;
        cfg->rpm = 300;
        cfg->expected_sectors = 18;
    } else if (strcmp(platform, "amiga") == 0) {
        cfg->encoding = OTDR_ENC_AMIGA_DD;
        cfg->rpm = 300;
        cfg->expected_sectors = 11;
    } else if (strcmp(platform, "pc_dd") == 0) {
        cfg->encoding = OTDR_ENC_MFM_DD;
        cfg->rpm = 300;
        cfg->expected_sectors = 9;
    } else if (strcmp(platform, "pc_hd") == 0) {
        cfg->encoding = OTDR_ENC_MFM_HD;
        cfg->rpm = 300;
        cfg->expected_sectors = 18;
    } else if (strcmp(platform, "c64") == 0) {
        cfg->encoding = OTDR_ENC_GCR_C64;
        cfg->rpm = 300;
        cfg->expected_sectors = 0; /* varies by zone */
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * PLL (Phase-Locked Loop) Simulation
 * ═══════════════════════════════════════════════════════════════════════ */

void otdr_pll_init(otdr_pll_state_t *pll, double initial_freq,
                   double bandwidth, double damping) {
    memset(pll, 0, sizeof(*pll));
    pll->frequency = initial_freq;
    pll->bandwidth = bandwidth;
    pll->damping = damping;
    pll->locked = false;
    pll->freq_min = initial_freq;
    pll->freq_max = initial_freq;
}

void otdr_pll_reset(otdr_pll_state_t *pll) {
    double freq = pll->frequency;
    double bw = pll->bandwidth;
    double damp = pll->damping;
    otdr_pll_init(pll, freq, bw, damp);
}

uint8_t otdr_pll_feed(otdr_pll_state_t *pll, uint32_t flux_ns,
                      double *phase_error_out) {
    /*
     * 2nd-order PLL model:
     * 1. Compute expected interval from current frequency
     * 2. Determine how many bitcells this flux interval spans
     * 3. Compute phase error
     * 4. Adjust frequency (proportional + integral)
     */

    double period_ns = 1e9 / pll->frequency; /* ns per bitcell */
    double flux = (double)flux_ns;

    /* Determine number of bitcells: round to nearest integer */
    double ratio = flux / period_ns;
    uint8_t bitcells = (uint8_t)(ratio + 0.5);
    if (bitcells < 1) bitcells = 1;
    if (bitcells > 8) bitcells = 8;

    /* Expected interval for this number of bitcells */
    double expected = bitcells * period_ns;

    /* Phase error */
    double error = flux - expected;
    pll->phase_error = error;

    if (phase_error_out)
        *phase_error_out = error;

    /* PI controller */
    double omega_n = 2.0 * M_PI * pll->bandwidth * pll->frequency;
    double kp = 2.0 * pll->damping * omega_n;
    double ki = omega_n * omega_n;

    pll->phase_integral += error;

    /* Frequency adjustment */
    double freq_adj = (kp * error + ki * pll->phase_integral * 1e-9) * 1e-9;
    pll->frequency += freq_adj;

    /* Clamp frequency to reasonable range (±30% of nominal).
     * Use running midpoint after stabilization, or initial frequency
     * for the first samples. */
    double nom_freq;
    if (pll->total_samples > 16 && pll->freq_min > 0) {
        nom_freq = (pll->freq_min + pll->freq_max) / 2.0;
    } else {
        nom_freq = pll->frequency; /* trust initial frequency */
    }
    double clamp_lo = nom_freq * 0.7;
    double clamp_hi = nom_freq * 1.3;
    /* Absolute bounds: 100 kHz to 2 MHz (covers FM SD to MFM HD) */
    if (clamp_lo < 100000.0) clamp_lo = 100000.0;
    if (clamp_hi > 2000000.0) clamp_hi = 2000000.0;
    if (pll->frequency < clamp_lo) pll->frequency = clamp_lo;
    if (pll->frequency > clamp_hi) pll->frequency = clamp_hi;

    /* Track min/max */
    if (pll->frequency < pll->freq_min) pll->freq_min = pll->frequency;
    if (pll->frequency > pll->freq_max) pll->freq_max = pll->frequency;

    /* Lock detection */
    double error_pct = fabs(error / expected) * 100.0;
    if (error_pct < 10.0) { /* within 10% = locked */
        pll->lock_count++;
        if (pll->lock_count > 8)
            pll->locked = true;
    } else {
        if (pll->locked) {
            pll->lock_lost_count++;
            pll->last_lock_pos = pll->total_samples;
        }
        pll->locked = false;
        pll->lock_count = 0;
    }

    pll->total_samples++;
    return bitcells;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Track — Allocation & Data Loading
 * ═══════════════════════════════════════════════════════════════════════ */

otdr_track_t *otdr_track_create(uint8_t cylinder, uint8_t head) {
    otdr_track_t *t = calloc(1, sizeof(otdr_track_t));
    if (!t) return NULL;
    t->cylinder = cylinder;
    t->head = head;
    t->track_num = cylinder * 2 + head;
    return t;
}

void otdr_track_free(otdr_track_t *track) {
    if (!track) return;
    free(track->flux_ns);
    for (int i = 0; i < OTDR_MAX_REVOLUTIONS; i++)
        free(track->flux_multi[i]);
    free(track->samples);
    free(track->quality_profile);
    free(track->quality_smoothed);
    free(track);
}

void otdr_track_load_flux(otdr_track_t *track, const uint32_t *flux_ns,
                          uint32_t count, uint8_t rev) {
    if (!track || !flux_ns || count == 0) return;

    if (rev == 0) {
        /* Primary flux data */
        free(track->flux_ns);
        track->flux_ns = malloc(count * sizeof(uint32_t));
        if (!track->flux_ns) return;
        memcpy(track->flux_ns, flux_ns, count * sizeof(uint32_t));
        track->flux_count = count;

        /* Compute revolution time */
        track->revolution_ns = 0;
        for (uint32_t i = 0; i < count; i++)
            track->revolution_ns += flux_ns[i];
    }

    /* Multi-read storage */
    if (rev < OTDR_MAX_REVOLUTIONS) {
        free(track->flux_multi[rev]);
        track->flux_multi[rev] = malloc(count * sizeof(uint32_t));
        if (track->flux_multi[rev]) {
            memcpy(track->flux_multi[rev], flux_ns, count * sizeof(uint32_t));
            track->flux_multi_count[rev] = count;
            if (rev >= track->num_revolutions)
                track->num_revolutions = rev + 1;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * Timing Histogram
 * ═══════════════════════════════════════════════════════════════════════ */

void otdr_track_histogram(otdr_track_t *track) {
    if (!track || !track->flux_ns) return;

    memset(track->histogram.bins, 0, sizeof(track->histogram.bins));

    for (uint32_t i = 0; i < track->flux_count; i++) {
        uint32_t ns = track->flux_ns[i];
        uint32_t bin = ns / 100; /* 100ns per bin */
        if (bin >= 256) bin = 255;
        track->histogram.bins[bin]++;
    }

    /* Find peaks (local maxima with minimum separation) */
    uint32_t peaks[8];
    uint32_t peak_vals[8];
    int npeak = 0;

    for (int b = 2; b < 254 && npeak < 8; b++) {
        uint32_t v = track->histogram.bins[b];
        if (v > track->histogram.bins[b-1] &&
            v > track->histogram.bins[b-2] &&
            v > track->histogram.bins[b+1] &&
            v > track->histogram.bins[b+2] &&
            v > track->flux_count / 50) { /* at least 2% of transitions */

            /* Ensure minimum separation from previous peak */
            if (npeak == 0 || b - peaks[npeak-1] > 8) {
                peaks[npeak] = b;
                peak_vals[npeak] = v;
                npeak++;
            } else if (v > peak_vals[npeak-1]) {
                /* Replace previous peak if this one is stronger */
                peaks[npeak-1] = b;
                peak_vals[npeak-1] = v;
            }
        }
    }

    if (npeak >= 1) track->histogram.peak_2t = peaks[0] * 100;
    if (npeak >= 2) track->histogram.peak_3t = peaks[1] * 100;
    if (npeak >= 3) track->histogram.peak_4t = peaks[2] * 100;

    if (npeak >= 2 && peaks[0] > 0)
        track->histogram.peak_separation =
            (float)peaks[1] / (float)peaks[0];
}

otdr_encoding_t otdr_track_detect_encoding(const otdr_track_t *track) {
    if (!track) return OTDR_ENC_MFM_DD;

    float sep = track->histogram.peak_separation;
    uint32_t p2t = track->histogram.peak_2t;

    /* MFM: peaks at ~1.5 ratio (2T:3T:4T = 4:6:8µs) */
    if (sep >= 1.3f && sep <= 1.7f) {
        if (p2t < 3000)
            return OTDR_ENC_MFM_HD;
        else
            return OTDR_ENC_MFM_DD;
    }

    /* FM: peaks at ~2.0 ratio */
    if (sep >= 1.8f && sep <= 2.2f)
        return OTDR_ENC_FM_SD;

    /* GCR: different pattern — use narrower peak separation */
    if (p2t < 3500 && sep < 1.3f)
        return OTDR_ENC_GCR_C64;

    return OTDR_ENC_MFM_DD; /* fallback */
}

/* ═══════════════════════════════════════════════════════════════════════
 * Quality Profile — The "OTDR Trace"
 * ═══════════════════════════════════════════════════════════════════════ */

void otdr_track_quality_profile(otdr_track_t *track,
                                const otdr_config_t *cfg) {
    if (!track || !track->flux_ns || track->flux_count == 0) return;

    /* Allocate samples */
    free(track->samples);
    track->samples = calloc(track->flux_count, sizeof(otdr_sample_t));
    if (!track->samples) return;
    track->sample_count = track->flux_count;

    /* Initialize PLL */
    otdr_encoding_t enc = (cfg->encoding == OTDR_ENC_AUTO)
        ? otdr_track_detect_encoding(track) : cfg->encoding;
    track->encoding = enc;

    double base_freq = 2.0e9 / (double)nominal_2t_ns(enc); /* bitcell (1T) freq */
    otdr_pll_init(&track->pll, base_freq, cfg->pll_bandwidth, cfg->pll_damping);

    /* Pass 1: PLL tracking + per-sample analysis */
    uint32_t bitcell_pos = 0;
    uint32_t t2 = nominal_2t_ns(enc);

    for (uint32_t i = 0; i < track->flux_count; i++) {
        otdr_sample_t *s = &track->samples[i];
        s->raw_ns = track->flux_ns[i];

        double phase_err;
        s->bitcells = otdr_pll_feed(&track->pll, s->raw_ns, &phase_err);

        /* Nominal interval from PLL */
        s->nominal_ns = (uint32_t)(s->bitcells * (1e9 / track->pll.frequency));
        s->deviation_ns = (int32_t)s->raw_ns - (int32_t)s->nominal_ns;

        /* Percentage deviation */
        if (s->nominal_ns > 0)
            s->deviation_pct = fabsf((float)s->deviation_ns /
                                      (float)s->nominal_ns) * 100.0f;

        /* Classify MFM pattern */
        if (s->raw_ns < t2 * 5 / 4)
            s->decoded_pattern = 2; /* 2T */
        else if (s->raw_ns < t2 * 7 / 4)
            s->decoded_pattern = 3; /* 3T */
        else if (s->raw_ns < t2 * 9 / 4)
            s->decoded_pattern = 4; /* 4T */
        else
            s->decoded_pattern = (uint8_t)(s->raw_ns / (t2 / 2) + 0.5);

        /* Quality in dB */
        s->quality_db = otdr_quality_to_db(s->deviation_pct);
        s->quality = otdr_db_to_quality(s->quality_db);
        s->is_stable = true; /* default, updated by weak-bit analysis */

        bitcell_pos += s->bitcells;
    }
    track->bitcell_count = bitcell_pos;

    /* Pass 2: Sliding-window RMS jitter */
    uint32_t win = cfg->smooth_window;
    if (win < 4) win = 4;

    for (uint32_t i = 0; i < track->flux_count; i++) {
        uint32_t start = (i >= win / 2) ? (i - win / 2) : 0;
        uint32_t end = (i + win / 2 < track->flux_count)
                          ? (i + win / 2) : track->flux_count;

        float sum_sq = 0.0f;
        uint32_t n = 0;
        for (uint32_t j = start; j < end; j++) {
            float d = track->samples[j].deviation_pct;
            sum_sq += d * d;
            n++;
        }
        track->samples[i].jitter_rms = (n > 0) ? sqrtf(sum_sq / n) : 0.0f;
    }

    /* Build bitcell-resolution quality profile */
    free(track->quality_profile);
    track->quality_profile = calloc(bitcell_pos, sizeof(float));
    if (!track->quality_profile) return;

    uint32_t bc = 0;
    for (uint32_t i = 0; i < track->flux_count; i++) {
        float q = track->samples[i].quality_db;
        for (uint8_t b = 0; b < track->samples[i].bitcells && bc < bitcell_pos; b++) {
            track->quality_profile[bc++] = q;
        }
    }
}

void otdr_track_smooth_profile(otdr_track_t *track, uint32_t window_size,
                               bool gaussian) {
    if (!track || !track->quality_profile || track->bitcell_count == 0)
        return;

    uint32_t n = track->bitcell_count;
    free(track->quality_smoothed);
    track->quality_smoothed = calloc(n, sizeof(float));
    if (!track->quality_smoothed) return;

    int half = (int)(window_size / 2);
    float sigma = (float)half / 2.5f;

    for (uint32_t i = 0; i < n; i++) {
        float sum = 0.0f;
        float wsum = 0.0f;

        int start = (int)i - half;
        int end = (int)i + half;
        if (start < 0) start = 0;
        if (end >= (int)n) end = (int)n - 1;

        for (int j = start; j <= end; j++) {
            float w = gaussian
                ? gaussian_weight(j, (int)i, sigma)
                : 1.0f;
            sum += w * track->quality_profile[j];
            wsum += w;
        }

        track->quality_smoothed[i] = (wsum > 0) ? (sum / wsum) : 0.0f;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * Event Detection
 * ═══════════════════════════════════════════════════════════════════════ */

void otdr_track_detect_events(otdr_track_t *track, const otdr_config_t *cfg) {
    if (!track || !track->samples) return;

    track->event_count = 0;
    uint32_t t2 = nominal_2t_ns(track->encoding);
    uint32_t noflux_ns = (uint32_t)(t2 * cfg->noflux_threshold);
    uint32_t bitcell_pos = 0;

    /* Previous jitter for spike detection */
    float prev_jitter = 0.0f;

    /* Track PLL lock state for re-lock events */
    bool was_locked = false;

    /* Running average for drift detection */
    float drift_sum = 0.0f;
    uint32_t drift_count = 0;
    uint32_t drift_start = 0;
    bool in_drift = false;

    for (uint32_t i = 0; i < track->flux_count; i++) {
        otdr_sample_t *s = &track->samples[i];

        /* --- No-Flux Area --- */
        if (s->raw_ns > noflux_ns) {
            uint32_t gap_bitcells = s->raw_ns / (t2 / 2);
            char desc[80];
            snprintf(desc, sizeof(desc),
                     "No-flux area: %u ns (%u bitcells)", s->raw_ns, gap_bitcells);
            add_event(track, OTDR_EVT_NOFLUX_AREA, OTDR_SEV_CRITICAL,
                      bitcell_pos, i, gap_bitcells, 100.0f, desc);
        }

        /* --- Jitter Spike --- */
        if (i > 0 && s->jitter_rms > cfg->jitter_spike_threshold &&
            s->jitter_rms > prev_jitter * 2.5f) {
            char desc[80];
            snprintf(desc, sizeof(desc),
                     "Jitter spike: %.1f%% (prev %.1f%%)",
                     s->jitter_rms, prev_jitter);
            add_event(track, OTDR_EVT_JITTER_SPIKE, OTDR_SEV_WARNING,
                      bitcell_pos, i, s->bitcells, s->jitter_rms, desc);
        }

        /* --- Jitter Drift (gradual degradation) --- */
        if (s->deviation_pct > OTDR_QUALITY_GOOD) {
            if (!in_drift) {
                in_drift = true;
                drift_start = bitcell_pos;
                drift_sum = 0.0f;
                drift_count = 0;
            }
            drift_sum += s->deviation_pct;
            drift_count++;
        } else if (in_drift) {
            if (drift_count > 32) { /* significant drift region */
                char desc[80];
                snprintf(desc, sizeof(desc),
                         "Jitter drift: avg %.1f%% over %u bitcells",
                         drift_sum / drift_count, drift_count);
                add_event(track, OTDR_EVT_JITTER_DRIFT, OTDR_SEV_MINOR,
                          drift_start, i - drift_count, drift_count,
                          drift_sum / drift_count, desc);
            }
            in_drift = false;
        }

        /* --- PLL Re-lock --- */
        bool now_locked = track->pll.locked; /* simplified: use per-sample */
        if (s->deviation_pct < cfg->pll_lock_threshold)
            now_locked = true;
        else
            now_locked = false;

        if (now_locked && !was_locked && i > 16) {
            add_event(track, OTDR_EVT_PLL_RELOCK, OTDR_SEV_WARNING,
                      bitcell_pos, i, 1, s->deviation_pct,
                      "PLL re-acquired lock");
        }
        was_locked = now_locked;

        /* --- Encoding Error (invalid pattern for MFM) --- */
        if (track->encoding == OTDR_ENC_MFM_DD ||
            track->encoding == OTDR_ENC_MFM_HD) {
            if (s->decoded_pattern < 2 || s->decoded_pattern > 4) {
                if (s->raw_ns < noflux_ns) { /* not already a no-flux */
                    char desc[80];
                    snprintf(desc, sizeof(desc),
                             "Invalid MFM pattern: %uT (%u ns)",
                             s->decoded_pattern, s->raw_ns);
                    add_event(track, OTDR_EVT_ENCODING_ERROR, OTDR_SEV_ERROR,
                              bitcell_pos, i, s->bitcells,
                              s->deviation_pct, desc);
                }
            }
        }

        prev_jitter = s->jitter_rms;
        bitcell_pos += s->bitcells;
    }

    /* --- Track-level events --- */
    /* Expected revolution time for RPM */
    uint32_t expected_rev_ns = (uint32_t)(60e9 / 300.0); /* ~200ms at 300 RPM */
    float rev_deviation = fabsf((float)track->revolution_ns -
                                (float)expected_rev_ns) /
                          (float)expected_rev_ns * 100.0f;

    if (rev_deviation > 2.0f) {
        char desc[80];
        snprintf(desc, sizeof(desc),
                 "Track length %.1f%% %s than nominal (%u vs %u ns)",
                 rev_deviation,
                 (track->revolution_ns > expected_rev_ns) ? "longer" : "shorter",
                 track->revolution_ns, expected_rev_ns);

        if (track->revolution_ns > expected_rev_ns * 1.02)
            add_event(track, OTDR_EVT_PROT_LONG_TRACK, OTDR_SEV_INFO,
                      0, 0, track->bitcell_count, rev_deviation, desc);
        else if (track->revolution_ns < expected_rev_ns * 0.98)
            add_event(track, OTDR_EVT_PROT_SHORT_TRACK, OTDR_SEV_INFO,
                      0, 0, track->bitcell_count, rev_deviation, desc);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * Weak Bit / Multi-Read Analysis
 * ═══════════════════════════════════════════════════════════════════════ */

void otdr_track_weak_bit_analysis(otdr_track_t *track,
                                  const otdr_config_t *cfg) {
    if (!track || track->num_revolutions < 2) return;

    /*
     * Strategy: Compare decoded bit patterns across revolutions.
     * Where the decoded pattern varies, we have weak/fuzzy bits.
     *
     * For timing-based analysis (no decoded data), compare flux intervals:
     * - Align by cumulative position
     * - Compute coefficient of variation at each position
     * - CV > threshold → weak bit
     */

    /* Use the shortest revolution as reference */
    uint32_t min_count = UINT32_MAX;
    for (uint8_t r = 0; r < track->num_revolutions; r++) {
        if (track->flux_multi_count[r] < min_count)
            min_count = track->flux_multi_count[r];
    }
    if (min_count == 0) return;

    /* Compare corresponding flux intervals across revolutions */
    uint32_t weak_regions = 0;
    uint32_t weak_start = 0;
    bool in_weak = false;
    uint32_t bitcell_pos = 0;
    uint32_t t2 = nominal_2t_ns(track->encoding);

    for (uint32_t i = 0; i < min_count; i++) {
        /* Gather this flux interval across all revolutions */
        float values[OTDR_MAX_REVOLUTIONS];
        float sum = 0.0f;
        uint8_t nrev = track->num_revolutions;

        for (uint8_t r = 0; r < nrev; r++) {
            values[r] = (float)track->flux_multi[r][i];
            sum += values[r];
        }

        float mean = sum / nrev;
        float var_sum = 0.0f;
        for (uint8_t r = 0; r < nrev; r++) {
            float d = values[r] - mean;
            var_sum += d * d;
        }
        float stddev = sqrtf(var_sum / nrev);
        float cv = (mean > 0) ? (stddev / mean) : 0.0f;

        /* Check if decoded pattern differs across revolutions */
        bool pattern_varies = false;
        uint8_t ref_pattern = 2;
        if (mean < t2 * 5 / 4) ref_pattern = 2;
        else if (mean < t2 * 7 / 4) ref_pattern = 3;
        else ref_pattern = 4;

        for (uint8_t r = 0; r < nrev; r++) {
            uint8_t p;
            if (values[r] < t2 * 5 / 4) p = 2;
            else if (values[r] < t2 * 7 / 4) p = 3;
            else p = 4;
            if (p != ref_pattern) {
                pattern_varies = true;
                break;
            }
        }

        bool is_weak = (cv > cfg->weak_bit_cv) || pattern_varies;

        /* Mark in primary samples if available */
        if (is_weak && i < track->sample_count)
            track->samples[i].is_stable = false;

        /* Track weak regions */
        if (is_weak && !in_weak) {
            in_weak = true;
            weak_start = bitcell_pos;
        } else if (!is_weak && in_weak) {
            uint32_t weak_len = bitcell_pos - weak_start;
            if (weak_len > 2) { /* at least 3 bitcells to be significant */
                char desc[80];
                snprintf(desc, sizeof(desc),
                         "Weak/fuzzy bits: %u bitcells (CV up to %.1f%%)",
                         weak_len, cv * 100.0f);

                otdr_severity_t sev = (weak_len > 32)
                    ? OTDR_SEV_WARNING : OTDR_SEV_MINOR;

                /* Determine if this looks intentional (copy protection) */
                otdr_event_type_t type = (weak_len > 8 && weak_len < 128)
                    ? OTDR_EVT_FUZZY_BITS : OTDR_EVT_WEAK_BITS;

                add_event(track, type, sev, weak_start,
                          i - weak_len, weak_len, cv * 100.0f, desc);
                weak_regions++;
            }
            in_weak = false;
        }

        /* Approximate bitcell position */
        bitcell_pos += (uint32_t)(mean / (t2 / 2.0f) + 0.5f);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * Track Statistics
 * ═══════════════════════════════════════════════════════════════════════ */

void otdr_track_compute_stats(otdr_track_t *track) {
    if (!track || !track->samples || track->sample_count == 0) return;

    float *devs = malloc(track->sample_count * sizeof(float));
    if (!devs) return;

    float sum = 0.0f, sum_sq = 0.0f, peak = 0.0f;
    float quality_sum = 0.0f, quality_min = 0.0f;
    uint32_t good = 0, weak = 0, bad = 0, noflux = 0;

    for (uint32_t i = 0; i < track->sample_count; i++) {
        otdr_sample_t *s = &track->samples[i];
        float d = s->deviation_pct;
        devs[i] = d;
        sum += d;
        sum_sq += d * d;
        if (d > peak) peak = d;
        quality_sum += s->quality_db;
        if (s->quality_db < quality_min) quality_min = s->quality_db;

        if (!s->is_stable)
            weak += s->bitcells;
        else if (s->quality <= OTDR_QUAL_GOOD)
            good += s->bitcells;
        else if (s->quality >= OTDR_QUAL_UNREADABLE)
            noflux += s->bitcells;
        else
            bad += s->bitcells;
    }

    uint32_t n = track->sample_count;
    track->stats.jitter_mean = sum / n;
    track->stats.jitter_rms = sqrtf(sum_sq / n);
    track->stats.jitter_peak = peak;

    /* 95th percentile */
    qsort(devs, n, sizeof(float), cmp_float);
    track->stats.jitter_p95 = devs[(uint32_t)(n * 0.95)];

    track->stats.quality_mean_db = quality_sum / n;
    track->stats.quality_min_db = quality_min;

    /* SNR estimate: ratio of nominal signal to jitter noise */
    if (track->stats.jitter_rms > 0)
        track->stats.snr_estimate = 20.0f * log10f(100.0f / track->stats.jitter_rms);
    else
        track->stats.snr_estimate = 60.0f; /* excellent */

    /* Speed variation from PLL frequency range */
    if (track->pll.freq_min > 0)
        track->stats.speed_variation =
            (float)((track->pll.freq_max - track->pll.freq_min) /
                    track->pll.freq_min * 100.0);

    track->stats.total_bitcells = track->bitcell_count;
    track->stats.good_bitcells = good;
    track->stats.weak_bitcells = weak;
    track->stats.bad_bitcells = bad;
    track->stats.noflux_bitcells = noflux;

    /* Count specific events */
    track->stats.crc_errors = 0;
    track->stats.missing_sectors = 0;
    track->stats.pll_relocks = 0;
    for (uint32_t i = 0; i < track->event_count; i++) {
        switch (track->events[i].type) {
            case OTDR_EVT_CRC_ERROR:     track->stats.crc_errors++; break;
            case OTDR_EVT_MISSING_SECTOR: track->stats.missing_sectors++; break;
            case OTDR_EVT_PLL_RELOCK:    track->stats.pll_relocks++; break;
            default: break;
        }
    }

    /* Overall quality classification */
    if (track->stats.jitter_rms < OTDR_QUALITY_EXCELLENT)
        track->stats.overall = OTDR_QUAL_EXCELLENT;
    else if (track->stats.jitter_rms < OTDR_QUALITY_GOOD)
        track->stats.overall = OTDR_QUAL_GOOD;
    else if (track->stats.jitter_rms < OTDR_QUALITY_FAIR)
        track->stats.overall = OTDR_QUAL_FAIR;
    else if (track->stats.jitter_rms < OTDR_QUALITY_POOR)
        track->stats.overall = OTDR_QUAL_POOR;
    else if (track->stats.jitter_rms < OTDR_QUALITY_CRITICAL)
        track->stats.overall = OTDR_QUAL_CRITICAL;
    else
        track->stats.overall = OTDR_QUAL_UNREADABLE;

    /* Override if CRC errors present */
    if (track->stats.crc_errors > 0 && track->stats.overall < OTDR_QUAL_POOR)
        track->stats.overall = OTDR_QUAL_POOR;

    free(devs);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Full Track Analysis (orchestrator)
 * ═══════════════════════════════════════════════════════════════════════ */

int otdr_track_analyze(otdr_track_t *track, const otdr_config_t *cfg) {
    if (!track || !track->flux_ns || track->flux_count == 0)
        return -1;

    /* Step 1: Build timing histogram */
    otdr_track_histogram(track);

    /* Step 2: Detect encoding if auto */
    if (cfg->encoding == OTDR_ENC_AUTO)
        track->encoding = otdr_track_detect_encoding(track);
    else
        track->encoding = cfg->encoding;

    /* Step 3: Run PLL + build quality profile */
    otdr_track_quality_profile(track, cfg);

    /* Step 4: Smooth profile */
    otdr_track_smooth_profile(track, cfg->smooth_window, cfg->use_gaussian);

    /* Step 5: Detect events */
    otdr_track_detect_events(track, cfg);

    /* Step 6: Weak bit analysis (if multi-read data available) */
    if (cfg->detect_weak_bits && track->num_revolutions >= 2)
        otdr_track_weak_bit_analysis(track, cfg);

    /* Step 7: Compute statistics */
    otdr_track_compute_stats(track);

    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Disk-Level Analysis
 * ═══════════════════════════════════════════════════════════════════════ */

otdr_disk_t *otdr_disk_create(uint8_t cylinders, uint8_t heads) {
    otdr_disk_t *d = calloc(1, sizeof(otdr_disk_t));
    if (!d) return NULL;

    d->num_cylinders = cylinders;
    d->num_heads = heads;
    d->track_count = cylinders * heads;
    d->tracks = calloc(d->track_count, sizeof(otdr_track_t));
    if (!d->tracks) {
        free(d);
        return NULL;
    }

    /* Initialize track identification */
    for (uint16_t i = 0; i < d->track_count; i++) {
        d->tracks[i].cylinder = i / heads;
        d->tracks[i].head = i % heads;
        d->tracks[i].track_num = i;
    }

    return d;
}

void otdr_disk_free(otdr_disk_t *disk) {
    if (!disk) return;
    if (disk->tracks) {
        for (uint16_t i = 0; i < disk->track_count; i++) {
            free(disk->tracks[i].flux_ns);
            for (int r = 0; r < OTDR_MAX_REVOLUTIONS; r++)
                free(disk->tracks[i].flux_multi[r]);
            free(disk->tracks[i].samples);
            free(disk->tracks[i].quality_profile);
            free(disk->tracks[i].quality_smoothed);
        }
        free(disk->tracks);
    }
    free(disk->heatmap);
    free(disk);
}

int otdr_disk_analyze(otdr_disk_t *disk, const otdr_config_t *cfg) {
    if (!disk || !disk->tracks) return -1;

    disk->encoding = cfg->encoding;

    for (uint16_t i = 0; i < disk->track_count; i++) {
        if (disk->tracks[i].flux_ns && disk->tracks[i].flux_count > 0) {
            otdr_track_analyze(&disk->tracks[i], cfg);
        }
    }

    if (cfg->generate_heatmap)
        otdr_disk_generate_heatmap(disk, cfg->heatmap_resolution);

    if (cfg->detect_protection)
        otdr_disk_detect_protection(disk);

    otdr_disk_compute_stats(disk);
    return 0;
}

void otdr_disk_generate_heatmap(otdr_disk_t *disk, uint32_t resolution) {
    if (!disk || !disk->tracks) return;

    free(disk->heatmap);
    disk->heatmap_cols = resolution;
    disk->heatmap_rows = disk->track_count;
    disk->heatmap = calloc((size_t)resolution * disk->track_count, sizeof(float));
    if (!disk->heatmap) return;

    for (uint16_t t = 0; t < disk->track_count; t++) {
        otdr_track_t *trk = &disk->tracks[t];
        float *row = &disk->heatmap[t * resolution];

        if (!trk->quality_profile || trk->bitcell_count == 0) {
            for (uint32_t c = 0; c < resolution; c++)
                row[c] = -60.0f; /* no data = worst */
            continue;
        }

        /* Resample quality profile to heatmap resolution */
        for (uint32_t c = 0; c < resolution; c++) {
            uint32_t bc_start = (uint32_t)((uint64_t)c * trk->bitcell_count / resolution);
            uint32_t bc_end = (uint32_t)((uint64_t)(c + 1) * trk->bitcell_count / resolution);
            if (bc_end > trk->bitcell_count) bc_end = trk->bitcell_count;

            float sum = 0.0f;
            uint32_t n = 0;
            for (uint32_t bc = bc_start; bc < bc_end; bc++) {
                sum += trk->quality_profile[bc];
                n++;
            }
            row[c] = (n > 0) ? (sum / n) : -60.0f;
        }
    }
}

void otdr_disk_compute_stats(otdr_disk_t *disk) {
    if (!disk) return;

    float quality_sum = 0.0f;
    float worst = 0.0f;
    uint8_t worst_track = 0;
    uint32_t total_sectors = 0, good_sectors = 0, bad_sectors = 0;
    uint32_t total_events = 0, critical_events = 0;
    uint16_t analyzed = 0;

    for (uint16_t t = 0; t < disk->track_count; t++) {
        otdr_track_t *trk = &disk->tracks[t];
        if (trk->sample_count == 0) continue;

        quality_sum += trk->stats.jitter_rms;
        analyzed++;

        if (trk->stats.jitter_rms > worst) {
            worst = trk->stats.jitter_rms;
            worst_track = trk->track_num;
        }

        total_sectors += trk->sector_count;
        for (uint8_t s = 0; s < trk->sector_count; s++) {
            if (trk->sectors[s].data_ok)
                good_sectors++;
            else
                bad_sectors++;
        }

        total_events += trk->event_count;
        for (uint32_t e = 0; e < trk->event_count; e++) {
            if (trk->events[e].severity >= OTDR_SEV_ERROR)
                critical_events++;
        }
    }

    disk->stats.quality_mean = (analyzed > 0) ? (quality_sum / analyzed) : 0;
    disk->stats.quality_worst_track = worst;
    disk->stats.worst_track_num = worst_track;
    disk->stats.total_sectors = total_sectors;
    disk->stats.good_sectors = good_sectors;
    disk->stats.bad_sectors = bad_sectors;
    disk->stats.total_events = total_events;
    disk->stats.critical_events = critical_events;

    /* Overall disk quality */
    if (disk->stats.quality_mean < OTDR_QUALITY_EXCELLENT)
        disk->stats.overall = OTDR_QUAL_EXCELLENT;
    else if (disk->stats.quality_mean < OTDR_QUALITY_GOOD)
        disk->stats.overall = OTDR_QUAL_GOOD;
    else if (disk->stats.quality_mean < OTDR_QUALITY_FAIR)
        disk->stats.overall = OTDR_QUAL_FAIR;
    else if (disk->stats.quality_mean < OTDR_QUALITY_POOR)
        disk->stats.overall = OTDR_QUAL_POOR;
    else
        disk->stats.overall = OTDR_QUAL_CRITICAL;

    if (bad_sectors > 0 && disk->stats.overall < OTDR_QUAL_POOR)
        disk->stats.overall = OTDR_QUAL_POOR;
}

void otdr_disk_detect_protection(otdr_disk_t *disk) {
    if (!disk) return;

    uint32_t prot_tracks = 0;
    bool has_fuzzy = false;
    bool has_long = false;
    bool has_noflux = false;

    for (uint16_t t = 0; t < disk->track_count; t++) {
        otdr_track_t *trk = &disk->tracks[t];
        bool track_protected = false;

        for (uint32_t e = 0; e < trk->event_count; e++) {
            switch (trk->events[e].type) {
                case OTDR_EVT_FUZZY_BITS:
                    has_fuzzy = true;
                    track_protected = true;
                    break;
                case OTDR_EVT_PROT_LONG_TRACK:
                    has_long = true;
                    track_protected = true;
                    break;
                case OTDR_EVT_NOFLUX_AREA:
                    /* Large no-flux areas often intentional */
                    if (trk->events[e].length > 64) {
                        has_noflux = true;
                        track_protected = true;
                    }
                    break;
                case OTDR_EVT_PROT_SHORT_TRACK:
                case OTDR_EVT_PROT_OVERLAP:
                case OTDR_EVT_PROT_DESYNC:
                    track_protected = true;
                    break;
                default:
                    break;
            }
        }
        if (track_protected) prot_tracks++;
    }

    disk->stats.has_copy_protection = (prot_tracks > 0);
    disk->stats.protected_tracks = prot_tracks;

    if (has_fuzzy && has_long)
        snprintf(disk->stats.protection_type,
                 sizeof(disk->stats.protection_type),
                 "Fuzzy bits + long tracks");
    else if (has_fuzzy)
        snprintf(disk->stats.protection_type,
                 sizeof(disk->stats.protection_type),
                 "Fuzzy/weak bits");
    else if (has_long)
        snprintf(disk->stats.protection_type,
                 sizeof(disk->stats.protection_type),
                 "Long/short tracks");
    else if (has_noflux)
        snprintf(disk->stats.protection_type,
                 sizeof(disk->stats.protection_type),
                 "No-flux areas");
    else if (prot_tracks > 0)
        snprintf(disk->stats.protection_type,
                 sizeof(disk->stats.protection_type),
                 "Unknown protection");
    else
        disk->stats.protection_type[0] = '\0';
}

/* ═══════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════ */

float otdr_quality_to_db(float deviation_pct) {
    /*
     * Maps jitter percentage to a dB-like quality scale:
     *   0% deviation → 0 dB (perfect)
     *   5% deviation → -3 dB
     *  10% deviation → -6 dB
     *  25% deviation → -14 dB
     *  50% deviation → -20 dB
     * 100% deviation → -40 dB
     *
     * Formula: dB = -20 * log10(1 + deviation/10)
     * Gives intuitive OTDR-like trace where lower = worse.
     */
    if (deviation_pct <= 0.0f) return 0.0f;
    return -20.0f * log10f(1.0f + deviation_pct / 10.0f);
}

otdr_quality_t otdr_db_to_quality(float db) {
    if (db >= -3.0f)  return OTDR_QUAL_EXCELLENT;
    if (db >= -6.0f)  return OTDR_QUAL_GOOD;
    if (db >= -10.0f) return OTDR_QUAL_FAIR;
    if (db >= -16.0f) return OTDR_QUAL_POOR;
    if (db >= -25.0f) return OTDR_QUAL_CRITICAL;
    return OTDR_QUAL_UNREADABLE;
}

const char *otdr_quality_name(otdr_quality_t q) {
    switch (q) {
        case OTDR_QUAL_EXCELLENT:  return "Excellent";
        case OTDR_QUAL_GOOD:       return "Good";
        case OTDR_QUAL_FAIR:       return "Fair";
        case OTDR_QUAL_POOR:       return "Poor";
        case OTDR_QUAL_CRITICAL:   return "Critical";
        case OTDR_QUAL_UNREADABLE: return "Unreadable";
    }
    return "Unknown";
}

const char *otdr_event_type_name(otdr_event_type_t type) {
    switch (type) {
        case OTDR_EVT_SECTOR_HEADER:  return "Sector Header";
        case OTDR_EVT_SECTOR_DATA:    return "Sector Data";
        case OTDR_EVT_INDEX_MARK:     return "Index Mark";
        case OTDR_EVT_TRACK_GAP:      return "Track Gap";
        case OTDR_EVT_JITTER_SPIKE:   return "Jitter Spike";
        case OTDR_EVT_JITTER_DRIFT:   return "Jitter Drift";
        case OTDR_EVT_PLL_RELOCK:     return "PLL Re-lock";
        case OTDR_EVT_TIMING_SHIFT:   return "Timing Shift";
        case OTDR_EVT_CRC_ERROR:      return "CRC Error";
        case OTDR_EVT_NOFLUX_AREA:    return "No-Flux Area";
        case OTDR_EVT_WEAK_BITS:      return "Weak Bits";
        case OTDR_EVT_FUZZY_BITS:     return "Fuzzy Bits (CP)";
        case OTDR_EVT_EXTRA_SECTOR:   return "Extra Sector";
        case OTDR_EVT_MISSING_SECTOR: return "Missing Sector";
        case OTDR_EVT_ENCODING_ERROR: return "Encoding Error";
        case OTDR_EVT_DENSITY_CHANGE: return "Density Change";
        case OTDR_EVT_PROT_LONG_TRACK:  return "Long Track (CP)";
        case OTDR_EVT_PROT_SHORT_TRACK: return "Short Track (CP)";
        case OTDR_EVT_PROT_OVERLAP:     return "Track Overlap (CP)";
        case OTDR_EVT_PROT_DESYNC:      return "Desync Pattern (CP)";
        case OTDR_EVT_PROT_SIGNATURE:   return "Protection Signature";
    }
    return "Unknown";
}

const char *otdr_severity_name(otdr_severity_t sev) {
    switch (sev) {
        case OTDR_SEV_INFO:     return "Info";
        case OTDR_SEV_MINOR:    return "Minor";
        case OTDR_SEV_WARNING:  return "Warning";
        case OTDR_SEV_ERROR:    return "Error";
        case OTDR_SEV_CRITICAL: return "Critical";
    }
    return "Unknown";
}

void otdr_quality_color(otdr_quality_t q, uint8_t *r, uint8_t *g, uint8_t *b) {
    switch (q) {
        case OTDR_QUAL_EXCELLENT:  *r = 0;   *g = 200; *b = 0;   break;
        case OTDR_QUAL_GOOD:       *r = 100; *g = 220; *b = 0;   break;
        case OTDR_QUAL_FAIR:       *r = 220; *g = 220; *b = 0;   break;
        case OTDR_QUAL_POOR:       *r = 255; *g = 140; *b = 0;   break;
        case OTDR_QUAL_CRITICAL:   *r = 255; *g = 40;  *b = 0;   break;
        case OTDR_QUAL_UNREADABLE: *r = 180; *g = 0;   *b = 0;   break;
        default:                   *r = 128; *g = 128; *b = 128; break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * Export Functions
 * ═══════════════════════════════════════════════════════════════════════ */

int otdr_track_export_csv(const otdr_track_t *track, const char *filename) {
    if (!track || !track->samples || !filename) return -1;

    FILE *f = fopen(filename, "w");
    if (!f) return -1;

    fprintf(f, "index,raw_ns,nominal_ns,deviation_ns,deviation_pct,"
               "jitter_rms,quality_db,pattern,bitcells,quality,stable\n");

    for (uint32_t i = 0; i < track->sample_count; i++) {
        const otdr_sample_t *s = &track->samples[i];
        fprintf(f, "%u,%u,%u,%d,%.2f,%.2f,%.2f,%u,%u,%s,%d\n",
                i, s->raw_ns, s->nominal_ns, s->deviation_ns,
                s->deviation_pct, s->jitter_rms, s->quality_db,
                s->decoded_pattern, s->bitcells,
                otdr_quality_name(s->quality), s->is_stable);
    }

    fclose(f);
    return 0;
}

int otdr_track_export_events_csv(const otdr_track_t *track,
                                  const char *filename) {
    if (!track || !filename) return -1;

    FILE *f = fopen(filename, "w");
    if (!f) return -1;

    fprintf(f, "type,severity,position,flux_index,length,"
               "magnitude,loss_db,sector,description\n");

    for (uint32_t i = 0; i < track->event_count; i++) {
        const otdr_event_t *e = &track->events[i];
        fprintf(f, "%s,%s,%u,%u,%u,%.2f,%.2f,%d,\"%s\"\n",
                otdr_event_type_name(e->type),
                otdr_severity_name(e->severity),
                e->position, e->flux_index, e->length,
                e->magnitude, e->loss_db, e->sector_id, e->desc);
    }

    fclose(f);
    return 0;
}

int otdr_disk_export_heatmap_pgm(const otdr_disk_t *disk,
                                  const char *filename) {
    if (!disk || !disk->heatmap || !filename) return -1;

    FILE *f = fopen(filename, "wb");
    if (!f) return -1;

    uint32_t w = disk->heatmap_cols;
    uint16_t h = disk->heatmap_rows;

    fprintf(f, "P5\n%u %u\n255\n", w, h);

    for (uint16_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            float db = disk->heatmap[y * w + x];
            /* Map -40dB..0dB to 0..255 */
            float normalized = (db + 40.0f) / 40.0f;
            normalized = clampf(normalized, 0.0f, 1.0f);
            uint8_t pixel = (uint8_t)(normalized * 255.0f);
            fwrite(&pixel, 1, 1, f);
        }
    }

    fclose(f);
    return 0;
}

int otdr_disk_export_report(const otdr_disk_t *disk, const char *filename) {
    if (!disk || !filename) return -1;

    FILE *f = fopen(filename, "w");
    if (!f) return -1;

    fprintf(f, "╔══════════════════════════════════════════════════════════╗\n");
    fprintf(f, "║       Floppy OTDR — Disk Analysis Report                ║\n");
    fprintf(f, "╚══════════════════════════════════════════════════════════╝\n\n");

    if (disk->label[0])
        fprintf(f, "  Label:     %s\n", disk->label);
    if (disk->source_file[0])
        fprintf(f, "  Source:    %s\n", disk->source_file);

    fprintf(f, "  Geometry:  %u cylinders × %u heads = %u tracks\n",
            disk->num_cylinders, disk->num_heads, disk->track_count);
    fprintf(f, "  Encoding:  %s\n",
            disk->encoding == OTDR_ENC_MFM_DD ? "MFM DD" :
            disk->encoding == OTDR_ENC_MFM_HD ? "MFM HD" :
            disk->encoding == OTDR_ENC_FM_SD  ? "FM SD"  : "Auto");
    fprintf(f, "  RPM:       %u\n\n", disk->rpm);

    fprintf(f, "── Overall Assessment ──────────────────────────────────────\n\n");
    fprintf(f, "  Quality:        %s (avg jitter %.1f%%)\n",
            otdr_quality_name(disk->stats.overall), disk->stats.quality_mean);
    fprintf(f, "  Worst Track:    %u (jitter %.1f%%)\n",
            disk->stats.worst_track_num, disk->stats.quality_worst_track);
    fprintf(f, "  Sectors:        %u total, %u good, %u bad\n",
            disk->stats.total_sectors, disk->stats.good_sectors,
            disk->stats.bad_sectors);
    fprintf(f, "  Events:         %u total, %u critical\n",
            disk->stats.total_events, disk->stats.critical_events);

    if (disk->stats.has_copy_protection) {
        fprintf(f, "\n  ⚠ Copy Protection: %s (%u tracks affected)\n",
                disk->stats.protection_type, disk->stats.protected_tracks);
    }

    fprintf(f, "\n── Per-Track Summary ───────────────────────────────────────\n\n");
    fprintf(f, "  Track  Cyl:Hd  Jitter%%  Quality   Events  Sectors  Notes\n");
    fprintf(f, "  ─────  ──────  ───────  ────────  ──────  ───────  ─────\n");

    for (uint16_t t = 0; t < disk->track_count; t++) {
        const otdr_track_t *trk = &disk->tracks[t];
        if (trk->sample_count == 0) continue;

        /* Build notes string */
        char notes[128] = "";
        if (trk->stats.crc_errors > 0)
            snprintf(notes + strlen(notes), sizeof(notes) - strlen(notes),
                     "CRC×%u ", trk->stats.crc_errors);
        if (trk->stats.weak_bitcells > 0)
            snprintf(notes + strlen(notes), sizeof(notes) - strlen(notes),
                     "Weak:%u ", trk->stats.weak_bitcells);
        if (trk->stats.pll_relocks > 0)
            snprintf(notes + strlen(notes), sizeof(notes) - strlen(notes),
                     "PLL×%u ", trk->stats.pll_relocks);

        fprintf(f, "  %5u  %2u:%u    %5.1f    %-9s %5u   %4u     %s\n",
                t, trk->cylinder, trk->head,
                trk->stats.jitter_rms,
                otdr_quality_name(trk->stats.overall),
                trk->event_count, trk->sector_count, notes);
    }

    fprintf(f, "\n── Event Details ──────────────────────────────────────────\n\n");

    for (uint16_t t = 0; t < disk->track_count; t++) {
        const otdr_track_t *trk = &disk->tracks[t];
        if (trk->event_count == 0) continue;

        fprintf(f, "  Track %u (Cyl %u, Head %u):\n", t, trk->cylinder, trk->head);
        for (uint32_t e = 0; e < trk->event_count; e++) {
            const otdr_event_t *evt = &trk->events[e];
            fprintf(f, "    [%s] @%u +%u: %s (%.1f%%, %.1f dB)\n",
                    otdr_severity_name(evt->severity),
                    evt->position, evt->length,
                    evt->desc, evt->magnitude, evt->loss_db);
        }
        fprintf(f, "\n");
    }

    fclose(f);
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * TDFC Integration — Matched Filter & Advanced Signal Analysis
 *
 * Merged from Time-Domain Flux Characterization (TDFC) library.
 * Domain-adapted for floppy flux analysis.
 * ═══════════════════════════════════════════════════════════════════════ */

/* ── Built-in MFM/FM Sync Templates ─────────────────────────────────── */

/*
 * MFM A1 sync byte (with missing clock bit):
 * Normal A1 = 10100001 → MFM = 0100010010101001
 * With missing clock:     0100010010001001
 *
 * As flux intervals (DD, 4µs = 2T):
 * 2T, 3T, 2T, 2T, 3T, 3T, 2T  (7 transitions)
 */
otdr_template_t otdr_template_mfm_sync_a1(otdr_encoding_t enc) {
    otdr_template_t t;
    memset(&t, 0, sizeof(t));
    snprintf(t.name, sizeof(t.name), "MFM Sync A1");
    t.threshold = 0.85f;

    uint32_t base = (enc == OTDR_ENC_MFM_HD) ? OTDR_MFM_HD_2T_NS : OTDR_MFM_2US_NS;

    float pattern[] = {
        (float)(base * 1.0f),   /* 2T */
        (float)(base * 1.5f),   /* 3T */
        (float)(base * 1.0f),   /* 2T */
        (float)(base * 1.0f),   /* 2T */
        (float)(base * 1.5f),   /* 3T — missing clock */
        (float)(base * 1.5f),   /* 3T */
        (float)(base * 1.0f),   /* 2T */
    };
    t.length = 7;
    t.pattern = (float *)malloc(t.length * sizeof(float));
    if (t.pattern)
        memcpy(t.pattern, pattern, t.length * sizeof(float));

    return t;
}

otdr_template_t otdr_template_mfm_iam_c2(otdr_encoding_t enc) {
    otdr_template_t t;
    memset(&t, 0, sizeof(t));
    snprintf(t.name, sizeof(t.name), "MFM IAM C2");
    t.threshold = 0.85f;

    uint32_t base = (enc == OTDR_ENC_MFM_HD) ? OTDR_MFM_HD_2T_NS : OTDR_MFM_2US_NS;

    float pattern[] = {
        (float)(base * 1.0f),   /* 2T */
        (float)(base * 1.0f),   /* 2T */
        (float)(base * 1.5f),   /* 3T */
        (float)(base * 2.0f),   /* 4T — missing clock */
        (float)(base * 1.0f),   /* 2T */
        (float)(base * 1.5f),   /* 3T */
    };
    t.length = 6;
    t.pattern = (float *)malloc(t.length * sizeof(float));
    if (t.pattern)
        memcpy(t.pattern, pattern, t.length * sizeof(float));

    return t;
}

otdr_template_t otdr_template_fm_sync(void) {
    otdr_template_t t;
    memset(&t, 0, sizeof(t));
    snprintf(t.name, sizeof(t.name), "FM Sync");
    t.threshold = 0.80f;

    float pattern[] = {
        (float)(OTDR_FM_SHORT_NS),
        (float)(OTDR_FM_LONG_NS),
        (float)(OTDR_FM_SHORT_NS),
        (float)(OTDR_FM_LONG_NS),
    };
    t.length = 4;
    t.pattern = (float *)malloc(t.length * sizeof(float));
    if (t.pattern)
        memcpy(t.pattern, pattern, t.length * sizeof(float));

    return t;
}

otdr_template_t otdr_template_from_flux(const uint32_t *flux_ns,
                                         uint32_t count,
                                         const char *name,
                                         float threshold) {
    otdr_template_t t;
    memset(&t, 0, sizeof(t));
    if (name)
        snprintf(t.name, sizeof(t.name), "%s", name);
    t.threshold = threshold;
    t.length = count;
    t.pattern = (float *)malloc(count * sizeof(float));
    if (t.pattern) {
        for (uint32_t i = 0; i < count; i++)
            t.pattern[i] = (float)flux_ns[i];
    }
    return t;
}

void otdr_template_free(otdr_template_t *tmpl) {
    if (!tmpl) return;
    free(tmpl->pattern);
    tmpl->pattern = NULL;
    tmpl->length = 0;
}

/* ── Matched Filter ──────────────────────────────────────────────────── */

static float norm_corr_at_flux(const uint32_t *flux, uint32_t flux_count,
                                const float *tmpl, uint32_t tmpl_len,
                                uint32_t center) {
    if (!flux || !tmpl || flux_count == 0 || tmpl_len == 0)
        return 0.0f;
    if (center + 1 < tmpl_len) return 0.0f;

    uint32_t start = center + 1 - tmpl_len;
    if (start + tmpl_len > flux_count) return 0.0f;

    double dot = 0.0, nx = 0.0, nt = 0.0;
    for (uint32_t i = 0; i < tmpl_len; i++) {
        double xv = (double)flux[start + i];
        double tv = (double)tmpl[i];
        dot += xv * tv;
        nx += xv * xv;
        nt += tv * tv;
    }

    double denom = sqrt(nx) * sqrt(nt);
    if (denom < 1e-18) return 0.0f;
    return (float)(dot / denom);
}

int otdr_track_match_template(const otdr_track_t *track,
                               const otdr_template_t *tmpl,
                               otdr_match_result_t *result) {
    if (!track || !track->flux_ns || !tmpl || !tmpl->pattern || !result)
        return -1;

    memset(result, 0, sizeof(*result));
    uint32_t n = track->flux_count;
    uint32_t tlen = tmpl->length;
    if (tlen > n) return -2;

    result->corr_count = n;
    result->correlation = (float *)calloc(n, sizeof(float));
    result->match_positions = (uint32_t *)malloc(n * sizeof(uint32_t));
    if (!result->correlation || !result->match_positions) {
        otdr_match_result_free(result);
        return -3;
    }

    float peak = -1.0f;
    uint32_t peak_pos = 0;
    uint32_t matches = 0;

    for (uint32_t i = tlen - 1; i < n; i++) {
        float c = norm_corr_at_flux(track->flux_ns, n,
                                     tmpl->pattern, tlen, i);
        result->correlation[i] = c;

        if (c > peak) {
            peak = c;
            peak_pos = i;
        }

        if (c >= tmpl->threshold) {
            if (matches == 0 || i - result->match_positions[matches - 1] >= tlen) {
                result->match_positions[matches++] = i;
            }
        }
    }

    result->match_count = matches;
    result->peak_corr = peak;
    result->peak_position = peak_pos;

    if (matches > 0 && matches < n) {
        uint32_t *shrunk = (uint32_t *)realloc(result->match_positions,
                                                 matches * sizeof(uint32_t));
        if (shrunk) result->match_positions = shrunk;
    } else if (matches == 0) {
        free(result->match_positions);
        result->match_positions = NULL;
    }

    return 0;
}

void otdr_match_result_free(otdr_match_result_t *r) {
    if (!r) return;
    free(r->correlation);
    free(r->match_positions);
    memset(r, 0, sizeof(*r));
}

/* ── CUSUM Change-Point Detection ────────────────────────────────────── */

otdr_cusum_config_t otdr_cusum_defaults(void) {
    otdr_cusum_config_t c;
    c.drift_k = 0.05f;
    c.threshold_h = 6.0f;
    return c;
}

int otdr_cusum_analyze(const float *series, uint32_t n,
                        const otdr_cusum_config_t *cfg,
                        otdr_changepoints_t *result) {
    if (!series || n == 0 || !cfg || !result)
        return -1;

    memset(result, 0, sizeof(*result));
    result->capacity = 256;
    result->positions = (uint32_t *)malloc(result->capacity * sizeof(uint32_t));
    result->magnitudes = (float *)malloc(result->capacity * sizeof(float));
    if (!result->positions || !result->magnitudes) {
        otdr_changepoints_free(result);
        return -2;
    }

    double mean = 0.0;
    for (uint32_t i = 0; i < n; i++) mean += (double)series[i];
    mean /= (double)n;

    double gp = 0.0, gn = 0.0;
    float k = cfg->drift_k;
    float h = cfg->threshold_h;

    for (uint32_t i = 0; i < n; i++) {
        double x = (double)series[i] - mean;
        gp = fmax(0.0, gp + x - k);
        gn = fmin(0.0, gn + x + k);

        if (gp > h || gn < -h) {
            if (result->count >= result->capacity) {
                result->capacity *= 2;
                uint32_t *np = (uint32_t *)realloc(result->positions,
                    result->capacity * sizeof(uint32_t));
                float *nm = (float *)realloc(result->magnitudes,
                    result->capacity * sizeof(float));
                if (!np || !nm) {
                    if (np) result->positions = np;
                    if (nm) result->magnitudes = nm;
                    return -3;
                }
                result->positions = np;
                result->magnitudes = nm;
            }

            result->positions[result->count] = i;
            result->magnitudes[result->count] = (float)(gp > h ? gp : -gn);
            result->count++;
            gp = 0.0;
            gn = 0.0;
        }
    }

    return 0;
}

int otdr_track_cusum(const otdr_track_t *track,
                      const otdr_cusum_config_t *cfg,
                      otdr_changepoints_t *result) {
    if (!track) return -1;

    const float *series = NULL;
    uint32_t n = 0;

    if (track->quality_smoothed && track->bitcell_count > 0) {
        series = track->quality_smoothed;
        n = track->bitcell_count;
    } else if (track->quality_profile && track->bitcell_count > 0) {
        series = track->quality_profile;
        n = track->bitcell_count;
    } else if (track->samples && track->sample_count > 0) {
        float *tmp = (float *)malloc(track->sample_count * sizeof(float));
        if (!tmp) return -2;
        for (uint32_t i = 0; i < track->sample_count; i++)
            tmp[i] = track->samples[i].deviation_pct;
        int rc = otdr_cusum_analyze(tmp, track->sample_count, cfg, result);
        free(tmp);
        return rc;
    } else {
        return -3;
    }

    return otdr_cusum_analyze(series, n, cfg, result);
}

void otdr_changepoints_free(otdr_changepoints_t *cp) {
    if (!cp) return;
    free(cp->positions);
    free(cp->magnitudes);
    memset(cp, 0, sizeof(*cp));
}

/* ── Amplitude Envelope Profiling (from TDFC) ────────────────────────── */

int otdr_track_envelope(const otdr_track_t *track,
                         uint32_t window, uint32_t step,
                         otdr_envelope_t *result) {
    if (!track || !track->flux_ns || track->flux_count == 0 || !result)
        return -1;
    if (window == 0) window = 512;
    if (step == 0) step = 64;

    memset(result, 0, sizeof(*result));

    uint32_t n = track->flux_count;
    uint32_t n_points = (n > 0) ? ((n - 1) / step + 1) : 0;
    if (n_points == 0) return -2;

    result->n_points = n_points;
    result->step = step;
    result->envelope_rms = (float *)calloc(n_points, sizeof(float));
    result->snr_db = (float *)calloc(n_points, sizeof(float));
    if (!result->envelope_rms || !result->snr_db) {
        otdr_envelope_free(result);
        return -3;
    }

    float *signal = (float *)malloc(n * sizeof(float));
    if (!signal) { otdr_envelope_free(result); return -4; }
    for (uint32_t i = 0; i < n; i++)
        signal[i] = (float)track->flux_ns[i];

    /* Global stats */
    double gsum = 0.0;
    for (uint32_t i = 0; i < n; i++) gsum += signal[i];
    float gmean = (float)(gsum / n);
    double gvar = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        double d = signal[i] - gmean;
        gvar += d * d;
    }
    result->global_mean = gmean;
    result->global_std = (n > 1) ? (float)sqrt(gvar / (n - 1)) : 0.0f;

    /* Sliding RMS envelope (O(n) running sum-of-squares) */
    double sumsq = 0.0;
    uint32_t wa = 0, wb = 0;
    for (uint32_t p = 0; p < n_points; p++) {
        uint32_t idx = p * step;
        if (idx >= n) idx = n - 1;

        uint32_t end = idx + 1;
        uint32_t start = (end > window) ? (end - window) : 0;

        while (wb < end && wb < n) { double v = signal[wb]; sumsq += v * v; wb++; }
        while (wa < start) { double v = signal[wa]; sumsq -= v * v; wa++; }

        uint32_t cur = (wb > wa) ? (wb - wa) : 1;
        result->envelope_rms[p] = (float)sqrt(sumsq / cur);
    }

    /* Sliding SNR (mean/std on |signal|) */
    double sum_abs = 0.0, sumsq_abs = 0.0;
    wa = 0; wb = 0;
    for (uint32_t p = 0; p < n_points; p++) {
        uint32_t idx = p * step;
        if (idx >= n) idx = n - 1;

        uint32_t end = idx + 1;
        uint32_t start = (end > window) ? (end - window) : 0;

        while (wb < end && wb < n) {
            double v = fabs(signal[wb]);
            sum_abs += v; sumsq_abs += v * v; wb++;
        }
        while (wa < start) {
            double v = fabs(signal[wa]);
            sum_abs -= v; sumsq_abs -= v * v; wa++;
        }

        uint32_t cur = (wb > wa) ? (wb - wa) : 1;
        double mn = sum_abs / cur;
        double var = (cur > 1) ?
            (sumsq_abs - sum_abs * sum_abs / cur) / (cur - 1) : 0.0;
        if (var < 0.0) var = 0.0;
        double sd = sqrt(var);

        double ratio = (sd > 1e-12) ? (mn / sd) : 0.0;
        double db = (ratio > 1e-12) ? (20.0 * log10(ratio)) : -120.0;
        result->snr_db[p] = (float)db;
    }

    free(signal);
    result->health_score = otdr_envelope_health_score(result);
    return 0;
}

int otdr_envelope_health_score(const otdr_envelope_t *env) {
    if (!env || env->n_points == 0 || !env->snr_db) return 0;

    uint32_t n = env->n_points;
    float *tmp = (float *)malloc(n * sizeof(float));
    if (!tmp) return 0;
    memcpy(tmp, env->snr_db, n * sizeof(float));
    qsort(tmp, n, sizeof(float), cmp_float);

    uint32_t lo = n / 10;
    uint32_t hi = n - n / 10;
    if (hi <= lo) { lo = 0; hi = n; }

    double sum = 0.0;
    uint32_t cnt = 0;
    for (uint32_t i = lo; i < hi; i++) { sum += tmp[i]; cnt++; }
    free(tmp);

    double snr = cnt ? (sum / cnt) : -120.0;
    double score = (snr + 5.0) / 25.0;
    if (score < 0.0) score = 0.0;
    if (score > 1.0) score = 1.0;
    return (int)(score * 100.0 + 0.5);
}

void otdr_envelope_free(otdr_envelope_t *env) {
    if (!env) return;
    free(env->envelope_rms);
    free(env->snr_db);
    memset(env, 0, sizeof(*env));
}

/* ── Spectral Flatness (Wiener Entropy) ──────────────────────────────── */

int otdr_track_spectral_flatness(const otdr_track_t *track,
                                  uint32_t window,
                                  float **flatness, uint32_t *count) {
    if (!track || !track->flux_ns || track->flux_count == 0 ||
        !flatness || !count)
        return -1;

    if (window < 4) window = 64;
    uint32_t n = track->flux_count;
    uint32_t n_out = (n >= window) ? (n - window + 1) : 0;
    if (n_out == 0) return -2;

    *flatness = (float *)malloc(n_out * sizeof(float));
    if (!*flatness) return -3;
    *count = n_out;

    for (uint32_t i = 0; i < n_out; i++) {
        double log_sum = 0.0;
        double arith_sum = 0.0;

        for (uint32_t j = 0; j < window; j++) {
            double v = (double)track->flux_ns[i + j];
            if (v < 1.0) v = 1.0;
            log_sum += log(v);
            arith_sum += v;
        }

        double geo_mean = exp(log_sum / window);
        double ari_mean = arith_sum / window;

        (*flatness)[i] = (ari_mean > 1e-12)
            ? (float)(geo_mean / ari_mean) : 0.0f;
    }

    return 0;
}
