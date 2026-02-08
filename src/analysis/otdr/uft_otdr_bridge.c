/**
 * @file uft_otdr_bridge.c
 * @brief UFT <-> OTDR Integration Bridge Implementation
 *
 * Connects UFT flux format parsers (KryoFlux, SCP, Greaseweazle)
 * to the OTDR signal analysis engine.
 */

#include "uft/analysis/uft_otdr_bridge.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ======================================================================
 * KryoFlux Stream Decoder (raw stream -> flux intervals)
 * ====================================================================== */

/** KryoFlux sample clock = 18.432 MHz x 73/56 ~ 24.027428 MHz */
#define KF_SAMPLE_CLOCK  24027428.5714285

static uint32_t decode_kryoflux_stream(const uint8_t *stream, uint32_t length,
                                        uint32_t *flux_out, uint32_t max_flux,
                                        uint32_t *index_pos, uint32_t max_index,
                                        uint32_t *n_index) {
    uint32_t pos = 0, flux_count = 0, overflow = 0, idx_count = 0;

    while (pos < length && flux_count < max_flux) {
        uint8_t b = stream[pos];

        if (b == 0x0D) {
            /* OOB block */
            if (pos + 4 > length) break;
            uint8_t oob_type = stream[pos + 1];
            uint16_t oob_size = stream[pos + 2] | ((uint16_t)stream[pos + 3] << 8);
            if (oob_type == 0x02 && oob_size >= 8 && index_pos && idx_count < max_index)
                index_pos[idx_count++] = flux_count;
            else if (oob_type == 0x03 || oob_type == 0x0D)
                break;
            pos += 4 + oob_size;
            continue;
        }

        if (b <= 0x07) {
            /* Flux2: 2-byte */
            if (pos + 2 > length) break;
            uint32_t val = ((uint32_t)(b & 0x07) << 8) | stream[pos + 1];
            val += overflow; overflow = 0;
            flux_out[flux_count++] = (uint32_t)((double)val * 1e9 / KF_SAMPLE_CLOCK + 0.5);
            pos += 2;
        } else if (b == 0x08) { pos += 1; }
          else if (b == 0x09) { pos += 2; }
          else if (b == 0x0A) { pos += 3; }
          else if (b == 0x0B) { overflow += 0x10000; pos += 1; }
          else if (b == 0x0C) {
            /* Flux3 */
            if (pos + 3 > length) break;
            uint32_t val = ((uint32_t)stream[pos+1] << 8) | stream[pos+2];
            val += overflow; overflow = 0;
            flux_out[flux_count++] = (uint32_t)((double)val * 1e9 / KF_SAMPLE_CLOCK + 0.5);
            pos += 3;
        } else {
            /* Flux1: 0x0E-0xFF */
            uint32_t val = (uint32_t)b + overflow; overflow = 0;
            flux_out[flux_count++] = (uint32_t)((double)val * 1e9 / KF_SAMPLE_CLOCK + 0.5);
            pos += 1;
        }
    }
    if (n_index) *n_index = idx_count;
    return flux_count;
}

/* ======================================================================
 * SCP Track Decoder (25 MHz clock = 40ns/tick)
 * ====================================================================== */

static uint32_t decode_scp_revolution(const uint8_t *data, uint32_t n_words,
                                       uint32_t *flux_out, uint32_t max_flux) {
    uint32_t count = 0, overflow = 0;
    for (uint32_t i = 0; i < n_words && count < max_flux; i++) {
        uint16_t val = ((uint16_t)data[i*2] << 8) | data[i*2+1];
        if (val == 0) { overflow += 65536; }
        else {
            flux_out[count++] = (uint32_t)((val + overflow) * 40.0 + 0.5);
            overflow = 0;
        }
    }
    return count;
}

/* ======================================================================
 * Greaseweazle Decoder (72 MHz clock)
 * ====================================================================== */

static uint32_t decode_greaseweazle(const uint8_t *data, uint32_t length,
                                     uint32_t *flux_out, uint32_t max_flux) {
    uint32_t pos = 0, count = 0, overflow = 0;
    while (pos < length && count < max_flux) {
        uint8_t b = data[pos++];
        if (b == 255) { overflow += 249*255; }
        else if (b == 250) {
            if (pos+2 > length) break;
            uint32_t val = data[pos] | ((uint32_t)data[pos+1]<<8);
            pos += 2; val += overflow; overflow = 0;
            flux_out[count++] = (uint32_t)((double)val * 1e9 / 72000000.0 + 0.5);
        } else if (b == 0) { break; }
        else {
            uint32_t val = (uint32_t)b + overflow; overflow = 0;
            flux_out[count++] = (uint32_t)((double)val * 1e9 / 72000000.0 + 0.5);
        }
    }
    return count;
}

/* ======================================================================
 * Analog Zero-Crossing Detector
 * ====================================================================== */

static uint32_t analog_to_flux(const int16_t *samples, uint32_t count,
                                float sample_rate_hz,
                                uint32_t *flux_out, uint32_t max_flux) {
    uint32_t flux_count = 0;
    double ns_per_sample = 1e9 / sample_rate_hz;
    double last_crossing = 0.0;
    int had_crossing = 0;

    for (uint32_t i = 1; i < count && flux_count < max_flux; i++) {
        if (samples[i-1] <= 0 && samples[i] > 0) {
            double frac = (double)(-samples[i-1]) / (double)(samples[i]-samples[i-1]);
            double crossing = ((double)i - 1.0 + frac) * ns_per_sample;
            if (had_crossing) {
                double interval = crossing - last_crossing;
                if (interval > 100.0 && interval < 500000.0)
                    flux_out[flux_count++] = (uint32_t)(interval + 0.5);
            }
            last_crossing = crossing;
            had_crossing = 1;
        }
    }
    return flux_count;
}

/* ======================================================================
 * Public API
 * ====================================================================== */

int uft_otdr_init(uft_otdr_context_t *ctx, const char *platform) {
    if (!ctx) return -1;
    memset(ctx, 0, sizeof(*ctx));
    if (platform) otdr_config_for_platform(&ctx->config, platform);
    else otdr_config_defaults(&ctx->config);
    ctx->tdfc_env_window = 512;
    ctx->tdfc_snr_window = 256;
    ctx->tdfc_step = 64;
    ctx->max_cylinders = 80;
    ctx->max_heads = 2;
    ctx->disk = otdr_disk_create(ctx->max_cylinders, ctx->max_heads);
    return ctx->disk ? 0 : -2;
}

void uft_otdr_set_geometry(uft_otdr_context_t *ctx, uint8_t cyl, uint8_t heads) {
    if (!ctx) return;
    if (ctx->disk && (cyl != ctx->max_cylinders || heads != ctx->max_heads)) {
        otdr_disk_free(ctx->disk);
        ctx->max_cylinders = cyl;
        ctx->max_heads = heads;
        ctx->disk = otdr_disk_create(cyl, heads);
    }
}

int uft_otdr_feed_flux_ns(uft_otdr_context_t *ctx,
                          const uint32_t *flux_ns, uint32_t count,
                          uint8_t cyl, uint8_t head, uint8_t rev) {
    if (!ctx || !ctx->disk || !flux_ns || count == 0) return -1;
    if (cyl >= ctx->max_cylinders || head >= ctx->max_heads) return -2;
    if (rev >= UFT_OTDR_MAX_REVOLUTIONS) return -3;
    uint32_t idx = (uint32_t)cyl * ctx->max_heads + head;
    if (idx >= ctx->disk->track_count) return -4;
    otdr_track_load_flux(&ctx->disk->tracks[idx], flux_ns, count, rev);
    ctx->analyzed = false;
    return 0;
}

int uft_otdr_feed_kryoflux(uft_otdr_context_t *ctx,
                           const uint8_t *stream, uint32_t length,
                           uint8_t cyl, uint8_t head) {
    if (!ctx || !stream || length == 0) return -1;
    uint32_t *flux = (uint32_t *)malloc(length * sizeof(uint32_t));
    if (!flux) return -2;
    uint32_t index_pos[16]; uint32_t n_index = 0;
    uint32_t fc = decode_kryoflux_stream(stream, length, flux, length,
                                          index_pos, 16, &n_index);
    if (fc == 0) { free(flux); return -3; }
    if (n_index >= 2) {
        for (uint32_t r = 0; r < n_index-1 && r < UFT_OTDR_MAX_REVOLUTIONS; r++) {
            uint32_t s = index_pos[r], e = index_pos[r+1];
            if (s < e && e <= fc)
                uft_otdr_feed_flux_ns(ctx, &flux[s], e-s, cyl, head, (uint8_t)r);
        }
    } else {
        uft_otdr_feed_flux_ns(ctx, flux, fc, cyl, head, 0);
    }
    free(flux);
    return 0;
}

int uft_otdr_feed_scp(uft_otdr_context_t *ctx,
                      const uint8_t *scp_data, uint32_t length,
                      uint8_t cyl, uint8_t head, uint8_t revolutions) {
    if (!ctx || !scp_data || length == 0) return -1;
    if (revolutions * 12 > length) return -2;
    uint32_t *flux = (uint32_t *)malloc(200000 * sizeof(uint32_t));
    if (!flux) return -3;

    for (uint8_t rev = 0; rev < revolutions && rev < UFT_OTDR_MAX_REVOLUTIONS; rev++) {
        uint32_t ho = (uint32_t)rev * 12;
        uint32_t nbc = scp_data[ho+4] | ((uint32_t)scp_data[ho+5]<<8) |
                       ((uint32_t)scp_data[ho+6]<<16) | ((uint32_t)scp_data[ho+7]<<24);
        uint32_t doff = scp_data[ho+8] | ((uint32_t)scp_data[ho+9]<<8) |
                        ((uint32_t)scp_data[ho+10]<<16) | ((uint32_t)scp_data[ho+11]<<24);
        if (doff + nbc*2 > length) continue;
        uint32_t n = decode_scp_revolution(&scp_data[doff], nbc, flux, 200000);
        if (n > 0) uft_otdr_feed_flux_ns(ctx, flux, n, cyl, head, rev);
    }
    free(flux);
    return 0;
}

int uft_otdr_feed_greaseweazle(uft_otdr_context_t *ctx,
                               const uint8_t *gw_data, uint32_t length,
                               uint8_t cyl, uint8_t head) {
    if (!ctx || !gw_data || length == 0) return -1;
    uint32_t *flux = (uint32_t *)malloc(200000 * sizeof(uint32_t));
    if (!flux) return -2;
    uint32_t n = decode_greaseweazle(gw_data, length, flux, 200000);
    if (n == 0) { free(flux); return -3; }
    uft_otdr_feed_flux_ns(ctx, flux, n, cyl, head, 0);
    free(flux);
    return 0;
}

int uft_otdr_feed_analog(uft_otdr_context_t *ctx,
                         const int16_t *samples, uint32_t count,
                         float sample_rate_hz, uint8_t cyl, uint8_t head) {
    if (!ctx || !samples || count == 0 || sample_rate_hz <= 0) return -1;
    uint32_t *flux = (uint32_t *)malloc(count * sizeof(uint32_t));
    if (!flux) return -2;
    uint32_t n = analog_to_flux(samples, count, sample_rate_hz, flux, count);
    if (n == 0) { free(flux); return -3; }
    uft_otdr_feed_flux_ns(ctx, flux, n, cyl, head, 0);
    free(flux);
    return 0;
}

int uft_otdr_analyze(uft_otdr_context_t *ctx) {
    if (!ctx || !ctx->disk) return -1;
    int rc = otdr_disk_analyze(ctx->disk, &ctx->config);
    if (rc == 0) ctx->analyzed = true;
    return rc;
}

uft_otdr_report_t uft_otdr_get_report(const uft_otdr_context_t *ctx) {
    uft_otdr_report_t rpt;
    memset(&rpt, 0, sizeof(rpt));
    if (!ctx || !ctx->disk || !ctx->analyzed) return rpt;

    const otdr_disk_t *d = ctx->disk;
    rpt.overall_quality = d->stats.overall;
    rpt.overall_jitter_pct = d->stats.quality_mean;
    rpt.total_tracks = d->track_count;
    rpt.has_protection = (d->stats.protected_tracks > 0);
    rpt.protected_tracks = d->stats.protected_tracks;
    if (d->stats.protection_type[0]) {
        memcpy(rpt.protection_type, d->stats.protection_type, sizeof(rpt.protection_type)-1);
        rpt.protection_type[sizeof(rpt.protection_type)-1] = '\0';
    }

    rpt.tracks = (uft_otdr_track_summary_t *)calloc(d->track_count, sizeof(uft_otdr_track_summary_t));
    uint32_t analyzed = 0;
    uint32_t total_evt = 0, crit_evt = 0;
    float worst_j = 0;
    int health_sum = 0;

    for (uint32_t t = 0; t < d->track_count; t++) {
        const otdr_track_t *tk = &d->tracks[t];
        if (tk->flux_count == 0) continue;
        analyzed++;

        uft_otdr_track_summary_t *s = &rpt.tracks[rpt.track_count++];
        s->cylinder = tk->cylinder;
        s->head = tk->head;
        s->quality = tk->stats.overall;
        s->jitter_rms_pct = tk->stats.jitter_rms;
        s->snr_db = tk->stats.snr_estimate;
        s->event_count = tk->event_count;

        for (uint32_t e = 0; e < tk->event_count; e++) {
            if (tk->events[e].type == OTDR_EVT_WEAK_BITS || tk->events[e].type == OTDR_EVT_FUZZY_BITS)
                s->weak_bitcells += tk->events[e].length;
            if (tk->events[e].severity == OTDR_SEV_CRITICAL) crit_evt++;
            if (tk->events[e].type >= OTDR_EVT_PROT_LONG_TRACK && tk->events[e].type <= OTDR_EVT_PROT_SIGNATURE)
                s->has_protection = true;
        }
        total_evt += tk->event_count;

        otdr_envelope_t env; memset(&env, 0, sizeof(env));
        if (otdr_track_envelope(tk, 512, 64, &env) == 0) {
            s->health_score = env.health_score;
            health_sum += env.health_score;
            otdr_envelope_free(&env);
        }

        if (tk->stats.jitter_rms > worst_j) {
            worst_j = tk->stats.jitter_rms;
            rpt.worst_track_cyl = tk->cylinder;
            rpt.worst_track_head = tk->head;
            rpt.worst_track_jitter = worst_j;
        }
    }

    rpt.analyzed_tracks = analyzed;
    rpt.total_events = total_evt;
    rpt.critical_events = crit_evt;
    rpt.health_score = analyzed > 0 ? health_sum / (int)analyzed : 0;
    rpt.total_sectors = d->stats.total_sectors;
    rpt.good_sectors = d->stats.good_sectors;
    rpt.bad_sectors = d->stats.total_sectors - d->stats.good_sectors;
    return rpt;
}

const otdr_disk_t *uft_otdr_get_disk(const uft_otdr_context_t *ctx) {
    return ctx ? ctx->disk : NULL;
}

const otdr_track_t *uft_otdr_get_track(const uft_otdr_context_t *ctx,
                                        uint8_t cyl, uint8_t head) {
    if (!ctx || !ctx->disk) return NULL;
    uint32_t idx = (uint32_t)cyl * ctx->max_heads + head;
    if (idx >= ctx->disk->track_count) return NULL;
    return &ctx->disk->tracks[idx];
}

int uft_otdr_snr_weights(const uft_otdr_context_t *ctx,
                         uint8_t cyl, uint8_t head,
                         float *weights, uint8_t *n_weights) {
    if (!ctx || !weights || !n_weights) return -1;
    const otdr_track_t *tk = uft_otdr_get_track(ctx, cyl, head);
    if (!tk || tk->num_revolutions <= 1) return -2;
    *n_weights = tk->num_revolutions;

    float total = 0;
    for (uint8_t r = 0; r < tk->num_revolutions; r++) {
        const uint32_t *rf = tk->flux_multi[r];
        uint32_t rc = tk->flux_multi_count[r];
        if (!rf || rc < 100) { weights[r] = 0.01f; total += 0.01f; continue; }
        double sum = 0, sum2 = 0;
        for (uint32_t i = 0; i < rc; i++) {
            double v = (double)rf[i]; sum += v; sum2 += v*v;
        }
        double mean = sum / rc;
        double var = sum2/rc - mean*mean;
        if (var < 1.0) var = 1.0;
        weights[r] = (float)(mean*mean / var);
        total += weights[r];
    }
    if (total > 0) for (uint8_t r = 0; r < *n_weights; r++) weights[r] /= total;
    return 0;
}

int uft_otdr_region_snr(const uft_otdr_context_t *ctx,
                        uint8_t cyl, uint8_t head,
                        uint32_t n_regions, float *snr_out) {
    if (!ctx || !snr_out || n_regions == 0) return -1;
    const otdr_track_t *tk = uft_otdr_get_track(ctx, cyl, head);
    if (!tk || !tk->quality_profile || tk->bitcell_count == 0) return -2;

    uint32_t rsz = tk->bitcell_count / n_regions;
    if (rsz == 0) rsz = 1;
    const float *prof = tk->quality_smoothed ? tk->quality_smoothed : tk->quality_profile;

    for (uint32_t r = 0; r < n_regions; r++) {
        uint32_t s = r * rsz, e = (r+1) * rsz;
        if (e > tk->bitcell_count) e = tk->bitcell_count;
        double sum = 0; uint32_t cnt = 0;
        for (uint32_t i = s; i < e; i++) { sum += (double)prof[i]; cnt++; }
        snr_out[r] = cnt > 0 ? (float)(sum / cnt) : -60.0f;
    }
    return 0;
}

int uft_otdr_export_report(const uft_otdr_context_t *ctx, const char *path) {
    return (ctx && ctx->disk) ? otdr_disk_export_report(ctx->disk, path) : -1;
}

int uft_otdr_export_heatmap(const uft_otdr_context_t *ctx, const char *path) {
    return (ctx && ctx->disk) ? otdr_disk_export_heatmap_pgm(ctx->disk, path) : -1;
}

int uft_otdr_export_track_csv(const uft_otdr_context_t *ctx,
                              uint8_t cyl, uint8_t head, const char *path) {
    const otdr_track_t *tk = uft_otdr_get_track(ctx, cyl, head);
    return tk ? otdr_track_export_csv(tk, path) : -1;
}

void uft_otdr_free(uft_otdr_context_t *ctx) {
    if (!ctx) return;
    if (ctx->disk) { otdr_disk_free(ctx->disk); ctx->disk = NULL; }
    ctx->analyzed = false;
}

void uft_otdr_report_free(uft_otdr_report_t *rpt) {
    if (!rpt) return;
    free(rpt->tracks); rpt->tracks = NULL; rpt->track_count = 0;
}
