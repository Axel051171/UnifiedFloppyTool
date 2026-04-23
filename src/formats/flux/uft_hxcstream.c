/**
 * @file uft_hxcstream.c
 * @brief HxC Floppy Emulator native flux stream format parser
 *
 * Reads HxCStream files containing raw flux timing intervals.
 * Converts uint16 flux intervals to UFT internal flux format.
 *
 * @version 1.0.0
 * @date 2026-04-10
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "uft/uft_error.h"
#include "uft/formats/flux/uft_hxcstream.h"

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static uint16_t read_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* ============================================================================
 * API Implementation
 * ============================================================================ */

void uft_hxcstream_image_init(uft_hxcstream_image_t *image)
{
    if (!image) return;
    memset(image, 0, sizeof(*image));
}

void uft_hxcstream_image_free(uft_hxcstream_image_t *image)
{
    if (!image) return;

    if (image->tracks) {
        for (size_t t = 0; t < image->track_count; t++) {
            free(image->tracks[t].flux_intervals);
            free(image->tracks[t].index_positions);
        }
        free(image->tracks);
    }
    memset(image, 0, sizeof(*image));
}

bool uft_hxcstream_probe(const uint8_t *data, size_t size, int *confidence)
{
    if (!data || size < UFT_HXCSTREAM_MAGIC_LEN) return false;

    if (memcmp(data, UFT_HXCSTREAM_MAGIC, UFT_HXCSTREAM_MAGIC_LEN) == 0) {
        if (confidence) *confidence = 98;
        return true;
    }
    return false;
}

int uft_hxcstream_read(const char *path, uft_hxcstream_image_t *image,
                       uft_hxcstream_read_result_t *result)
{
    if (!path || !image) return UFT_E_INVALID_ARG;

    uft_hxcstream_image_init(image);

    FILE *fp = fopen(path, "rb");
    if (!fp) return UFT_E_FILE_NOT_FOUND;

    /* Get file size */
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return UFT_E_IO;
    }
    long file_size = ftell(fp);
    if (file_size < 0 ||
        (size_t)file_size < sizeof(uft_hxcstream_file_header_t)) {
        fclose(fp);
        return UFT_E_FILE_TOO_SMALL;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return UFT_E_IO;
    }

    /* Read file header */
    uint8_t hdr_buf[32];
    size_t hdr_size = sizeof(uft_hxcstream_file_header_t);
    if (fread(hdr_buf, 1, hdr_size, fp) != hdr_size) {
        fclose(fp);
        return UFT_E_FILE_READ;
    }

    /* Verify magic */
    if (memcmp(hdr_buf, UFT_HXCSTREAM_MAGIC, UFT_HXCSTREAM_MAGIC_LEN) != 0) {
        fclose(fp);
        return UFT_E_MAGIC;
    }

    /* Parse header */
    memcpy(image->header.magic, hdr_buf, UFT_HXCSTREAM_MAGIC_LEN);
    image->header.version = hdr_buf[9];
    image->header.num_tracks = hdr_buf[10];
    image->header.num_sides = hdr_buf[11];
    image->header.sample_rate = read_le32(&hdr_buf[12]);
    image->header.flags = hdr_buf[16];

    /* Validate */
    uint8_t num_tracks = image->header.num_tracks;
    uint8_t num_sides = image->header.num_sides;
    if (num_tracks == 0 || num_sides == 0 ||
        num_sides > 2 || num_tracks > 84) {
        fclose(fp);
        return UFT_E_FORMAT_INVALID;
    }

    image->sample_rate = image->header.sample_rate;
    if (image->sample_rate == 0) {
        image->sample_rate = UFT_HXCSTREAM_DEFAULT_RATE;
    }
    image->cylinders = num_tracks;
    image->heads = num_sides;

    size_t total_tracks = (size_t)num_tracks * num_sides;
    if (total_tracks > UFT_HXCSTREAM_MAX_TRACKS) {
        fclose(fp);
        return UFT_E_FORMAT_INVALID;
    }

    /* Read track offset table */
    uft_hxcstream_track_entry_t *entries = calloc(
        total_tracks, sizeof(uft_hxcstream_track_entry_t));
    if (!entries) {
        fclose(fp);
        return UFT_E_MEMORY;
    }

    for (size_t i = 0; i < total_tracks; i++) {
        uint8_t entry_buf[12];
        if (fread(entry_buf, 1, sizeof(entry_buf), fp) != sizeof(entry_buf)) {
            free(entries);
            fclose(fp);
            return UFT_E_FILE_READ;
        }
        entries[i].offset = read_le32(&entry_buf[0]);
        entries[i].length = read_le32(&entry_buf[4]);
        entries[i].revolutions = entry_buf[8];
        entries[i].flags = entry_buf[9];
    }

    /* Allocate tracks */
    image->tracks = calloc(total_tracks, sizeof(uft_hxcstream_track_t));
    if (!image->tracks) {
        free(entries);
        fclose(fp);
        return UFT_E_MEMORY;
    }
    image->track_count = total_tracks;

    size_t total_flux = 0;

    /* Read each track */
    for (size_t i = 0; i < total_tracks; i++) {
        if (entries[i].offset == 0 || entries[i].length == 0) continue;
        if ((long)entries[i].offset >= file_size) continue;

        if (fseek(fp, (long)entries[i].offset, SEEK_SET) != 0) continue;

        /* Read track header */
        uint8_t th_buf[16];
        size_t th_size = sizeof(uft_hxcstream_track_header_t);
        if (fread(th_buf, 1, th_size, fp) != th_size) continue;

        uft_hxcstream_track_t *track = &image->tracks[i];
        track->cylinder = th_buf[0];
        track->head = th_buf[1];
        uint32_t flux_count = read_le32(&th_buf[2]);
        /* index_offset at offset 6, encoding_hint at offset 10 */
        track->encoding_hint = th_buf[10];

        /* Clamp flux count */
        if (flux_count > UFT_HXCSTREAM_MAX_FLUX) {
            flux_count = UFT_HXCSTREAM_MAX_FLUX;
        }
        if (flux_count == 0) continue;

        /* Read flux intervals (uint16 each, little-endian) */
        size_t data_bytes = (size_t)flux_count * 2;
        uint8_t *raw = malloc(data_bytes);
        if (!raw) continue;

        size_t rd = fread(raw, 1, data_bytes, fp);
        size_t actual_flux = rd / 2;

        if (actual_flux > 0) {
            track->flux_intervals = malloc(actual_flux * sizeof(uint16_t));
            if (track->flux_intervals) {
                for (size_t f = 0; f < actual_flux; f++) {
                    track->flux_intervals[f] = read_le16(&raw[f * 2]);
                }
                track->flux_count = actual_flux;
                total_flux += actual_flux;
            }
        }
        free(raw);

        /* Parse index positions from flags if multi-rev */
        if (entries[i].flags & UFT_HXCSTREAM_TF_INDEX) {
            /* Index positions follow flux data as uint32 array */
            uint8_t nrev = entries[i].revolutions;
            if (nrev > 0 && nrev <= UFT_HXCSTREAM_MAX_REVS) {
                track->index_positions = calloc(nrev, sizeof(uint32_t));
                if (track->index_positions) {
                    for (uint8_t r = 0; r < nrev; r++) {
                        uint8_t idx_buf[4];
                        if (fread(idx_buf, 1, 4, fp) == 4) {
                            track->index_positions[r] = read_le32(idx_buf);
                        }
                    }
                    track->index_count = nrev;
                }
            }
        }
    }

    free(entries);
    fclose(fp);

    image->is_open = true;

    /* Fill result */
    if (result) {
        memset(result, 0, sizeof(*result));
        result->success = true;
        result->error_code = 0;
        result->cylinders = image->cylinders;
        result->heads = image->heads;
        result->sample_rate = image->sample_rate;
        result->track_count = image->track_count;
        result->total_flux_count = total_flux;
    }

    return 0;
}

const uft_hxcstream_track_t *uft_hxcstream_get_track(
    const uft_hxcstream_image_t *image, uint8_t cyl, uint8_t head)
{
    if (!image || !image->tracks) return NULL;
    if (cyl >= image->cylinders || head >= image->heads) return NULL;

    size_t idx = (size_t)cyl * image->heads + head;
    if (idx >= image->track_count) return NULL;

    return &image->tracks[idx];
}

size_t uft_hxcstream_flux_to_ns(const uft_hxcstream_image_t *image,
                                const uft_hxcstream_track_t *track,
                                uint32_t *ns_out, size_t max_count)
{
    if (!image || !track || !ns_out || max_count == 0) return 0;
    if (!track->flux_intervals || track->flux_count == 0) return 0;

    uint32_t sample_rate = image->sample_rate;
    if (sample_rate == 0) sample_rate = UFT_HXCSTREAM_DEFAULT_RATE;

    /* ns = ticks * 1_000_000_000 / sample_rate
     * To avoid overflow with 64-bit: ns = ticks * (1e9 / rate)
     * For 72MHz: factor = 1e9/72e6 = 13.888... ~ 125/9 */
    double ns_per_tick = 1000000000.0 / (double)sample_rate;

    size_t count = track->flux_count;
    if (count > max_count) count = max_count;

    for (size_t i = 0; i < count; i++) {
        ns_out[i] = (uint32_t)((double)track->flux_intervals[i] * ns_per_tick
                                + 0.5);
    }

    return count;
}
