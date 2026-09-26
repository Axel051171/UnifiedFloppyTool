/**
 * @file uft_diag_gw.c
 * @brief Surface-scan read adapter over a Greaseweazle (door stage 4).
 *
 * Contract and limits: include/uft/diag/uft_diag_gw.h.
 *
 * Only READ-side functions of the Greaseweazle driver are called here
 * (uft_gw_get_info, uft_gw_read_track, uft_gw_flux_free).
 * src/hal/uft_greaseweazle_full.c is a protected file and is used as it is.
 */
#include "uft/diag/uft_diag_gw.h"
#include "uft/hal/uft_greaseweazle_full.h"

#include <stdlib.h>
#include <string.h>

int uft_diag_gw_init(uft_diag_gw_t *a, struct uft_gw_device *dev,
                     flux_encoding_t encoding, uint32_t bitcell_ns,
                     uint8_t revolutions)
{
    if (!a) return -1;
    memset(a, 0, sizeof(*a));
    if (!dev || revolutions < 1) return -1;
    /* The caller names the encoding. AUTO would let each capture decide
     * what the disk is — geometry from a read, which the scan must not do. */
    if (encoding != FLUX_ENC_MFM && encoding != FLUX_ENC_FM) return -1;

    /* The sample clock, asked from the device itself (GET_INFO, read-only).
     *
     * Measured against the emulator (MF-1346): a device opened through
     * uft_gw_open_stream() never had its info filled (only uft_gw_open()
     * called GET_INFO), so every capture on it reported sample_freq = 0.
     * Since P3-551 (MF-1356) both open paths run the same handshake and
     * uft_gw_get_info() refuses 0 Hz instead of substituting 72 MHz; the
     * question stays here anyway, because this adapter takes any device
     * and a clock it did not see is a clock it cannot vouch for. Without
     * a clock the flux cannot be timed, and a fixed 72 MHz would be an
     * invented number on an F7-Plus (84 MHz). */
    uft_gw_info_t info;
    memset(&info, 0, sizeof info);
    if (uft_gw_get_info((uft_gw_device_t *)dev, &info) != UFT_GW_OK ||
        info.sample_freq == 0)
        return -1;

    a->track_cache = (flux_decoded_track_t *)calloc(1, sizeof(*a->track_cache));
    if (!a->track_cache) return -1;
    a->dev         = dev;
    a->encoding    = encoding;
    a->bitcell_ns  = bitcell_ns;
    a->revolutions = revolutions;
    a->sample_freq = info.sample_freq;
    return 0;
}

void uft_diag_gw_free(uft_diag_gw_t *a)
{
    if (!a) return;
    if (a->track_cache) {
        flux_decoded_track_free(a->track_cache);
        free(a->track_cache);
    }
    memset(a, 0, sizeof(*a));
}

/* GW samples are intervals in ticks; the decoder wants CUMULATIVE times
 * (flux_raw_data_t, MF-438). The driver's index_times are durations too
 * (first: stream start -> first index; then index -> index, as
 * uft_gw_decode_flux_index_times() builds them), the decoder wants
 * positions — both are summed up here. */
static int flux_to_raw(const uft_gw_flux_data_t *fx, uint32_t sample_freq,
                       flux_raw_data_t *raw)
{
    memset(raw, 0, sizeof(*raw));
    if (!fx->samples || fx->sample_count == 0 || sample_freq == 0) return -1;

    uint32_t *t = (uint32_t *)malloc((size_t)fx->sample_count * sizeof(uint32_t));
    if (!t) return -1;
    uint64_t cum = 0;
    size_t n = 0;
    for (uint32_t i = 0; i < fx->sample_count; i++) {
        if (fx->samples[i] == 0) continue;
        cum += fx->samples[i];
        if (cum > UINT32_MAX) break;          /* > 59 s at 72 MHz: stop, do not wrap */
        t[n++] = (uint32_t)cum;
    }
    if (n == 0) { free(t); return -1; }
    raw->transitions      = t;
    raw->transition_count = n;
    raw->sample_rate      = sample_freq;

    if (fx->index_times && fx->index_count > 0) {
        uint32_t *ix = (uint32_t *)malloc((size_t)fx->index_count * sizeof(uint32_t));
        if (!ix) { free(t); memset(raw, 0, sizeof(*raw)); return -1; }
        uint64_t pos = 0;
        size_t k = 0;
        for (uint8_t i = 0; i < fx->index_count; i++) {
            pos += fx->index_times[i];
            if (pos > UINT32_MAX) break;
            ix[k++] = (uint32_t)pos;
        }
        raw->index_times = ix;
        raw->index_count = k;
    }
    return 0;
}

/* One capture of (track, side), decoded into a->track_cache. */
static int capture(uft_diag_gw_t *a, int track, int side)
{
    flux_decoded_track_free(a->track_cache);          /* leaves it zeroed */
    a->cache_valid = false;
    memset(a->asked, 0, sizeof(a->asked));

    uft_gw_flux_data_t *fx = NULL;
    a->reads++;
    int rc = uft_gw_read_track((uft_gw_device_t *)a->dev, (uint8_t)track,
                               (uint8_t)side, a->revolutions, &fx);
    if (rc != UFT_GW_OK || !fx) {
        a->last_gw_error = (rc != UFT_GW_OK) ? rc : UFT_GW_ERR_IO;
        if (fx) uft_gw_flux_free(fx);
        return -1;
    }

    /* The capture's own clock wins when it has one; it must then agree
     * with what the device said at init — two clocks for one capture
     * would make every interval a guess. */
    uint32_t freq = fx->sample_freq ? fx->sample_freq : a->sample_freq;
    if (fx->sample_freq && fx->sample_freq != a->sample_freq) {
        uft_gw_flux_free(fx);
        return -1;
    }

    flux_raw_data_t raw;
    int ok = flux_to_raw(fx, freq, &raw);
    uft_gw_flux_free(fx);                 /* frees the struct itself too */
    if (ok != 0) return -1;

    flux_decoder_options_t opts;
    flux_decoder_options_init(&opts);
    opts.encoding   = a->encoding;
    opts.bitcell_ns = a->bitcell_ns;
    /* The status is not the verdict: a track with one bad CRC still
     * carries its good sectors. Each sector is judged on its own flags. */
    (void)flux_decode_track(&raw, a->track_cache, &opts);
    flux_raw_free(&raw);

    a->cache_track = track;
    a->cache_side  = side;
    a->cache_valid = true;
    return 0;
}

int uft_diag_gw_read(int track, int side, int sector,
                     uint8_t *buffer, size_t sector_size, void *user_data)
{
    uft_diag_gw_t *a = (uft_diag_gw_t *)user_data;
    if (!a || !a->dev || !a->track_cache || !buffer || sector_size == 0) return -1;
    if (track < 0 || track > 255 || side < 0 || side > 1) return -1;
    if (sector < 0 || sector > 255) return -1;

    const uint8_t bit = (uint8_t)(1u << (sector & 7));
    const int need_capture = !a->cache_valid || a->cache_track != track ||
                             a->cache_side != side ||
                             (a->asked[sector >> 3] & bit);   /* a retry */
    if (need_capture && capture(a, track, side) != 0) return -1;
    a->asked[sector >> 3] |= bit;

    const flux_decoded_track_t *d = a->track_cache;
    for (size_t i = 0; i < d->sector_count && i < FLUX_MAX_SECTORS; i++) {
        const flux_decoded_sector_t *s = &d->sectors[i];
        if (s->sector != (uint8_t)sector) continue;
        if (s->cylinder != (uint8_t)track || s->head != (uint8_t)side) continue;
        if (!s->id_crc_ok || !s->data_crc_ok) continue;
        if (!s->data || s->data_size != sector_size) continue;
        memcpy(buffer, s->data, sector_size);
        return 0;
    }
    return -1;
}
