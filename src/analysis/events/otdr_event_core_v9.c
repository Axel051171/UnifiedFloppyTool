/*
 * otdr_event_core_v9.c — Signal Integrity: Dropout/Saturation/Stuck/Dead-Zone
 * ==============================================================================
 */

#include "otdr_event_core_v9.h"
#include "otdr_common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ════════════════════════════════════════════════════════════════════
 * Helpers
 * ════════════════════════════════════════════════════════════════════ */

/* Robust sigma via MAD on slice [a..b] inclusive */
static float mad_sigma(const float *x, size_t a, size_t b, float scale) {
    size_t n = b - a + 1;
    if (n < 3) return 0.0f;
    float *tmp = (float *)malloc(n * sizeof(float));
    if (!tmp) return 0.0f;

    for (size_t i = 0; i < n; i++) tmp[i] = x[a + i];
    qsort(tmp, n, sizeof(float), otdr_cmp_float_asc);
    float med = (n & 1u) ? tmp[n / 2] : 0.5f * (tmp[n / 2 - 1] + tmp[n / 2]);

    for (size_t i = 0; i < n; i++) tmp[i] = fabsf(tmp[i] - med);
    qsort(tmp, n, sizeof(float), otdr_cmp_float_asc);
    float mad = (n & 1u) ? tmp[n / 2] : 0.5f * (tmp[n / 2 - 1] + tmp[n / 2]);

    free(tmp);
    float s = scale * mad;
    return (s > 1e-15f) ? s : 1e-15f;
}

/* Emit a region into the output array */
static int emit_region(otdr9_region_t *regions, size_t *count, size_t max,
                       otdr9_anomaly_t type, size_t start, size_t end,
                       float severity, float mean_val, float stuck_val,
                       float snr_db) {
    if (*count >= max) return 0;
    otdr9_region_t *r = &regions[*count];
    r->type        = type;
    r->start       = start;
    r->end         = end;
    r->severity    = severity;
    r->mean_value  = mean_val;
    r->stuck_value = stuck_val;
    r->snr_db      = snr_db;
    (*count)++;
    return 1;
}

/* ════════════════════════════════════════════════════════════════════
 * Defaults
 * ════════════════════════════════════════════════════════════════════ */

otdr9_config_t otdr9_default_config(void) {
    otdr9_config_t c;
    memset(&c, 0, sizeof(c));

    c.dropout_threshold = 1e-4f;
    c.dropout_min_run   = 3;

    c.clip_high       = 0.99f;
    c.clip_low        = -0.99f;
    c.clip_min_run    = 2;
    c.clip_auto_range = 0.0f;

    c.stuck_max_delta = 1e-6f;
    c.stuck_min_run   = 5;

    c.deadzone_snr_db   = 3.0f;
    c.deadzone_min_run  = 64;
    c.deadzone_sigma_win= 1024;

    c.auto_repair   = 0;
    c.mark_exclude  = 1;
    c.mad_scale     = 1.4826f;

    return c;
}

/* ════════════════════════════════════════════════════════════════════
 * Scanner passes
 * ════════════════════════════════════════════════════════════════════ */

/* Pass 1: Dropout detection */
static void scan_dropouts(const float *amp, size_t n,
                           const otdr9_config_t *cfg,
                           uint8_t *flags,
                           otdr9_region_t *regions, size_t *rcnt, size_t rmax) {
    float thr = cfg->dropout_threshold;
    size_t min_run = cfg->dropout_min_run;
    if (min_run < 1) min_run = 1;

    size_t run_start = 0;
    int in_run = 0;

    for (size_t i = 0; i <= n; i++) {
        int is_drop = (i < n) && (fabsf(amp[i]) < thr);

        if (is_drop && !in_run) {
            run_start = i;
            in_run = 1;
        } else if (!is_drop && in_run) {
            size_t run_len = i - run_start;
            if (run_len >= min_run) {
                /* Flag samples */
                for (size_t j = run_start; j < i; j++)
                    flags[j] |= OTDR9_FLAG_DROPOUT;

                /* Severity: longer = worse, capped at 1.0 */
                float sev = (float)run_len / (float)(min_run * 20);
                if (sev > 1.0f) sev = 1.0f;

                /* Mean value in region */
                double sum = 0.0;
                for (size_t j = run_start; j < i; j++) sum += (double)amp[j];
                float mean = (float)(sum / (double)run_len);

                emit_region(regions, rcnt, rmax, OTDR9_ANOM_DROPOUT,
                            run_start, i - 1, sev, mean, 0.0f, 0.0f);
            }
            in_run = 0;
        }
    }
}

/* Pass 2: Saturation / clipping detection */
static void scan_clipping(const float *amp, size_t n,
                           const otdr9_config_t *cfg,
                           uint8_t *flags,
                           otdr9_region_t *regions, size_t *rcnt, size_t rmax) {
    float hi = cfg->clip_high;
    float lo = cfg->clip_low;
    size_t min_run = cfg->clip_min_run;
    if (min_run < 1) min_run = 1;

    /* Auto-detect rails if configured */
    if (cfg->clip_auto_range > 0.0f && n > 100) {
        float *sorted = (float *)malloc(n * sizeof(float));
        if (sorted) {
            memcpy(sorted, amp, n * sizeof(float));
            qsort(sorted, n, sizeof(float), otdr_cmp_float_asc);
            size_t pct_idx = (size_t)((double)n * (double)cfg->clip_auto_range);
            if (pct_idx >= n) pct_idx = n - 1;
            hi = sorted[pct_idx];
            lo = sorted[n - 1 - pct_idx];
            free(sorted);
        }
    }

    /* Scan for high clips */
    size_t run_start = 0;
    int in_run = 0;
    for (size_t i = 0; i <= n; i++) {
        int clipped = (i < n) && (amp[i] >= hi);
        if (clipped && !in_run) { run_start = i; in_run = 1; }
        else if (!clipped && in_run) {
            size_t run_len = i - run_start;
            if (run_len >= min_run) {
                for (size_t j = run_start; j < i; j++)
                    flags[j] |= OTDR9_FLAG_CLIPPED_HIGH;
                float sev = (float)run_len / (float)(min_run * 10);
                if (sev > 1.0f) sev = 1.0f;
                emit_region(regions, rcnt, rmax, OTDR9_ANOM_SATURATED,
                            run_start, i - 1, sev, hi, 0.0f, 0.0f);
            }
            in_run = 0;
        }
    }

    /* Scan for low clips */
    in_run = 0;
    for (size_t i = 0; i <= n; i++) {
        int clipped = (i < n) && (amp[i] <= lo);
        if (clipped && !in_run) { run_start = i; in_run = 1; }
        else if (!clipped && in_run) {
            size_t run_len = i - run_start;
            if (run_len >= min_run) {
                for (size_t j = run_start; j < i; j++)
                    flags[j] |= OTDR9_FLAG_CLIPPED_LOW;
                float sev = (float)run_len / (float)(min_run * 10);
                if (sev > 1.0f) sev = 1.0f;
                emit_region(regions, rcnt, rmax, OTDR9_ANOM_SATURATED,
                            run_start, i - 1, sev, lo, 0.0f, 0.0f);
            }
            in_run = 0;
        }
    }
}

/* Pass 3: Stuck-at detection */
static void scan_stuck(const float *amp, size_t n,
                        const otdr9_config_t *cfg,
                        uint8_t *flags,
                        otdr9_region_t *regions, size_t *rcnt, size_t rmax) {
    float max_delta = cfg->stuck_max_delta;
    size_t min_run = cfg->stuck_min_run;
    if (min_run < 2) min_run = 2;

    size_t run_start = 0;
    int in_run = 0;
    float stuck_val = 0.0f;

    for (size_t i = 1; i <= n; i++) {
        int is_stuck = (i < n) && (fabsf(amp[i] - amp[i - 1]) <= max_delta);

        if (is_stuck && !in_run) {
            run_start = i - 1;
            stuck_val = amp[i - 1];
            in_run = 1;
        } else if (is_stuck && in_run) {
            /* continue run */
        } else if (!is_stuck && in_run) {
            size_t run_len = i - run_start;
            if (run_len >= min_run) {
                /* Don't flag if already flagged as dropout or clipped */
                int dominated = 0;
                for (size_t j = run_start; j < i; j++) {
                    if (flags[j] & (OTDR9_FLAG_DROPOUT | OTDR9_FLAG_CLIPPED_HIGH | OTDR9_FLAG_CLIPPED_LOW)) {
                        dominated = 1; break;
                    }
                }
                if (!dominated) {
                    for (size_t j = run_start; j < i; j++)
                        flags[j] |= OTDR9_FLAG_STUCK;
                    float sev = (float)run_len / (float)(min_run * 20);
                    if (sev > 1.0f) sev = 1.0f;
                    emit_region(regions, rcnt, rmax, OTDR9_ANOM_STUCK,
                                run_start, i - 1, sev, stuck_val, stuck_val, 0.0f);
                }
            }
            in_run = 0;
        }
    }
}

/* Pass 4: Dead zone detection (local SNR) */
static void scan_deadzone(const float *amp, size_t n,
                           const otdr9_config_t *cfg,
                           uint8_t *flags,
                           otdr9_region_t *regions, size_t *rcnt, size_t rmax) {
    float snr_thr = cfg->deadzone_snr_db;
    size_t min_run = cfg->deadzone_min_run;
    size_t sig_win = (size_t)cfg->deadzone_sigma_win;
    if (min_run < 4) min_run = 4;
    if (sig_win < 32) sig_win = 32;

    /* Compute delta trace */
    float *delta = (float *)calloc(n, sizeof(float));
    if (!delta) return;
    for (size_t i = 1; i < n; i++) delta[i] = amp[i] - amp[i - 1];

    /* Compute local SNR (simplified: |delta| / sigma) */
    float *snr = (float *)calloc(n, sizeof(float));
    if (!snr) { free(delta); return; }

    /* Compute sigma in blocks */
    size_t stride = sig_win / 4;
    if (stride < 64) stride = 64;
    float cur_sigma = 1e-15f;

    for (size_t i = 0; i < n; i++) {
        if (i % stride == 0 || i == 0) {
            size_t blk_start = (i + 1 > sig_win) ? (i + 1 - sig_win) : 0;
            cur_sigma = mad_sigma(delta, blk_start, i, cfg->mad_scale);
        }
        float d = fabsf(delta[i]);
        if (d < 1e-20f) d = 1e-20f;
        snr[i] = 20.0f * log10f(d / cur_sigma);
    }

    /* Find low-SNR runs */
    size_t run_start = 0;
    int in_run = 0;

    for (size_t i = 0; i <= n; i++) {
        int is_low = (i < n) && (snr[i] < snr_thr) && !(flags[i] & 0x0F);  /* not already flagged */

        if (is_low && !in_run) { run_start = i; in_run = 1; }
        else if (!is_low && in_run) {
            size_t run_len = i - run_start;
            if (run_len >= min_run) {
                double snr_sum = 0.0;
                for (size_t j = run_start; j < i; j++) {
                    flags[j] |= OTDR9_FLAG_DEADZONE;
                    snr_sum += (double)snr[j];
                }
                float mean_snr = (float)(snr_sum / (double)run_len);
                float sev = 1.0f - (mean_snr / snr_thr);
                if (sev < 0.0f) sev = 0.0f;
                if (sev > 1.0f) sev = 1.0f;

                emit_region(regions, rcnt, rmax, OTDR9_ANOM_DEADZONE,
                            run_start, i - 1, sev, 0.0f, 0.0f, mean_snr);
            }
            in_run = 0;
        }
    }

    free(delta);
    free(snr);
}

/* ════════════════════════════════════════════════════════════════════
 * Public: Scan
 * ════════════════════════════════════════════════════════════════════ */

int otdr9_scan(const float *amp, size_t n,
               const otdr9_config_t *cfg,
               uint8_t *flags_out,
               otdr9_region_t *regions, size_t max_regions,
               otdr9_summary_t *summary) {
    if (!amp || !flags_out || !regions || n == 0 || max_regions == 0)
        return -1;

    otdr9_config_t c = cfg ? *cfg : otdr9_default_config();

    memset(flags_out, 0, n);
    size_t rcnt = 0;

    /* Run all passes in priority order */
    scan_dropouts(amp, n, &c, flags_out, regions, &rcnt, max_regions);
    scan_clipping(amp, n, &c, flags_out, regions, &rcnt, max_regions);
    scan_stuck(amp, n, &c, flags_out, regions, &rcnt, max_regions);
    scan_deadzone(amp, n, &c, flags_out, regions, &rcnt, max_regions);

    /* Mark EXCLUDE on all flagged samples if configured */
    if (c.mark_exclude) {
        for (size_t i = 0; i < n; i++) {
            if (flags_out[i] & 0x1F)  /* any anomaly flag */
                flags_out[i] |= OTDR9_FLAG_EXCLUDE;
        }
    }

    /* Build summary */
    if (summary) {
        memset(summary, 0, sizeof(*summary));
        summary->samples_analyzed = n;
        summary->total_regions = rcnt;

        for (size_t i = 0; i < rcnt; i++) {
            size_t len = regions[i].end - regions[i].start + 1;
            switch (regions[i].type) {
            case OTDR9_ANOM_DROPOUT:
                summary->dropout_count++;
                summary->dropout_samples += len;
                break;
            case OTDR9_ANOM_SATURATED:
                summary->saturated_count++;
                summary->saturated_samples += len;
                break;
            case OTDR9_ANOM_STUCK:
                summary->stuck_count++;
                summary->stuck_samples += len;
                break;
            case OTDR9_ANOM_DEADZONE:
                summary->deadzone_count++;
                summary->deadzone_samples += len;
                break;
            default: break;
            }
        }

        for (size_t i = 0; i < n; i++) {
            if (flags_out[i] & 0x1F)
                summary->flagged_samples++;
        }
        summary->flagged_fraction = (float)summary->flagged_samples / (float)n;

        /* Integrity score: 1.0 = perfect */
        float frac_ok = 1.0f - summary->flagged_fraction;
        float region_penalty = (float)rcnt * 0.02f;
        if (region_penalty > 0.5f) region_penalty = 0.5f;
        summary->integrity_score = frac_ok - region_penalty;
        if (summary->integrity_score < 0.0f) summary->integrity_score = 0.0f;
        if (summary->integrity_score > 1.0f) summary->integrity_score = 1.0f;
    }

    /* Auto-repair if configured */
    if (c.auto_repair && summary) {
        /* Need a mutable copy — not done here (caller should use otdr9_repair) */
    }

    return (int)rcnt;
}

/* ════════════════════════════════════════════════════════════════════
 * Public: Repair
 * ════════════════════════════════════════════════════════════════════ */

size_t otdr9_repair(float *amp, size_t n, uint8_t *flags) {
    if (!amp || !flags || n < 2) return 0;

    size_t repaired = 0;

    /* Find runs of DROPOUT or STUCK and interpolate */
    size_t i = 0;
    while (i < n) {
        if (flags[i] & (OTDR9_FLAG_DROPOUT | OTDR9_FLAG_STUCK)) {
            size_t start = i;
            while (i < n && (flags[i] & (OTDR9_FLAG_DROPOUT | OTDR9_FLAG_STUCK)))
                i++;
            size_t end = i;  /* exclusive */

            /* Get boundary values for interpolation */
            float v_left  = (start > 0) ? amp[start - 1] : amp[end < n ? end : start];
            float v_right = (end < n)   ? amp[end]        : v_left;

            /* Linear interpolation */
            size_t len = end - start;
            for (size_t j = 0; j < len; j++) {
                float t = (len > 1) ? (float)j / (float)(len - 1) : 0.5f;
                amp[start + j] = v_left + t * (v_right - v_left);
                flags[start + j] |= OTDR9_FLAG_REPAIRED;
                flags[start + j] &= (uint8_t)~OTDR9_FLAG_EXCLUDE;  /* unmark exclude */
                repaired++;
            }
        } else {
            i++;
        }
    }
    return repaired;
}

/* ════════════════════════════════════════════════════════════════════
 * String helpers
 * ════════════════════════════════════════════════════════════════════ */

const char *otdr9_anomaly_str(otdr9_anomaly_t a) {
    switch (a) {
    case OTDR9_ANOM_NONE:      return "NONE";
    case OTDR9_ANOM_DROPOUT:   return "DROPOUT";
    case OTDR9_ANOM_SATURATED: return "SATURATED";
    case OTDR9_ANOM_STUCK:     return "STUCK";
    case OTDR9_ANOM_DEADZONE:  return "DEADZONE";
    default:                   return "UNKNOWN";
    }
}

const char *otdr9_flag_str(uint8_t flag) {
    if (flag & OTDR9_FLAG_DROPOUT)      return "DROPOUT";
    if (flag & OTDR9_FLAG_CLIPPED_HIGH) return "CLIPPED_HIGH";
    if (flag & OTDR9_FLAG_CLIPPED_LOW)  return "CLIPPED_LOW";
    if (flag & OTDR9_FLAG_STUCK)        return "STUCK";
    if (flag & OTDR9_FLAG_DEADZONE)     return "DEADZONE";
    if (flag & OTDR9_FLAG_REPAIRED)     return "REPAIRED";
    if (flag & OTDR9_FLAG_EXCLUDE)      return "EXCLUDE";
    return "OK";
}
