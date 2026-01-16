/**
 * @file uft_cw_raw.c
 * @brief Catweasel Raw Flux Format Implementation
 * 
 * Based on qbarnes/catweasel-cw and Tim Mann's cw2dmk documentation.
 */

#include "uft/formats/uft_cw_raw.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

uft_cw_image_t* uft_cw_create(int cylinders, int heads, uft_cw_model_t model)
{
    uft_cw_image_t *img = calloc(1, sizeof(uft_cw_image_t));
    if (!img) return NULL;
    
    img->model = model;
    img->cylinders = cylinders;
    img->heads = heads;
    img->track_count = cylinders * heads;
    
    switch (model) {
    case UFT_CW_MODEL_MK3:
        img->sample_clock = UFT_CW_MK3_CLOCK;
        break;
    case UFT_CW_MODEL_MK4:
    case UFT_CW_MODEL_MK4PLUS:
    default:
        img->sample_clock = UFT_CW_MK4_CLOCK;
        break;
    }
    
    img->tracks = calloc(img->track_count, sizeof(uft_cw_track_t));
    if (!img->tracks) {
        free(img);
        return NULL;
    }
    
    return img;
}

void uft_cw_free(uft_cw_image_t *img)
{
    if (!img) return;
    
    if (img->tracks) {
        for (int i = 0; i < img->track_count; i++) {
            uft_cw_track_free(&img->tracks[i]);
        }
        free(img->tracks);
    }
    
    free(img);
}

int uft_cw_track_init(uft_cw_track_t *track, int cyl, int head, size_t max_len)
{
    if (!track) return -1;
    
    memset(track, 0, sizeof(*track));
    track->cylinder = cyl;
    track->head = head;
    track->sample_clock = UFT_CW_DEFAULT_CLOCK;
    
    if (max_len > 0) {
        track->data = malloc(max_len);
        if (!track->data) return -1;
    }
    
    return 0;
}

void uft_cw_track_free(uft_cw_track_t *track)
{
    if (!track) return;
    free(track->data);
    track->data = NULL;
    track->length = 0;
}

/*===========================================================================
 * I/O
 *===========================================================================*/

int uft_cw_read_track(const char *filename, uft_cw_track_t *track)
{
    if (!filename || !track) return -1;
    
    FILE *f = fopen(filename, "rb");
    if (!f) return -1;
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0 || size > UFT_CW_MAX_TRACK_LEN) {
        fclose(f);
        return -1;
    }
    
    /* Allocate and read */
    track->data = malloc(size);
    if (!track->data) {
        fclose(f);
        return -1;
    }
    
    if (fread(track->data, 1, size, f) != (size_t)size) {
        free(track->data);
        track->data = NULL;
        fclose(f);
        return -1;
    }
    
    track->length = size;
    track->sample_clock = UFT_CW_DEFAULT_CLOCK;
    
    fclose(f);
    return 0;
}

int uft_cw_write_track(const char *filename, const uft_cw_track_t *track)
{
    if (!filename || !track || !track->data) return -1;
    
    FILE *f = fopen(filename, "wb");
    if (!f) return -1;
    
    size_t written = fwrite(track->data, 1, track->length, f);
    fclose(f);
    
    return (written == track->length) ? 0 : -1;
}

int uft_cw_read_image(const char *filename, uft_cw_image_t *img, int tracks)
{
    if (!filename || !img) return -1;
    
    FILE *f = fopen(filename, "rb");
    if (!f) return -1;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0) {
        fclose(f);
        return -1;
    }
    
    /* Read all data */
    uint8_t *data = malloc(size);
    if (!data || fread(data, 1, size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return -1;
    }
    fclose(f);
    
    /* If track count specified, divide evenly */
    if (tracks > 0) {
        size_t track_size = size / tracks;
        
        img->track_count = tracks;
        img->tracks = calloc(tracks, sizeof(uft_cw_track_t));
        
        for (int t = 0; t < tracks; t++) {
            img->tracks[t].data = malloc(track_size);
            if (img->tracks[t].data) {
                memcpy(img->tracks[t].data, data + t * track_size, track_size);
                img->tracks[t].length = track_size;
                img->tracks[t].cylinder = t / 2;
                img->tracks[t].head = t % 2;
                img->tracks[t].sample_clock = img->sample_clock;
            }
        }
    } else {
        /* Single track mode */
        img->track_count = 1;
        img->tracks = calloc(1, sizeof(uft_cw_track_t));
        img->tracks[0].data = data;
        img->tracks[0].length = size;
        img->tracks[0].sample_clock = img->sample_clock;
        data = NULL; /* Ownership transferred */
    }
    
    free(data);
    return 0;
}

int uft_cw_write_image(const char *filename, const uft_cw_image_t *img)
{
    if (!filename || !img) return -1;
    
    FILE *f = fopen(filename, "wb");
    if (!f) return -1;
    
    for (int i = 0; i < img->track_count; i++) {
        if (img->tracks[i].data && img->tracks[i].length > 0) {
            fwrite(img->tracks[i].data, 1, img->tracks[i].length, f);
        }
    }
    
    fclose(f);
    return 0;
}

/*===========================================================================
 * Conversion
 *===========================================================================*/

int uft_cw_raw_to_flux(const uint8_t *raw, size_t raw_len,
                       uint32_t *flux, size_t *flux_count, size_t max_flux)
{
    if (!raw || !flux || !flux_count) return -1;
    
    *flux_count = 0;
    uint32_t accum = 0;
    
    for (size_t i = 0; i < raw_len && *flux_count < max_flux; i++) {
        uint8_t val = raw[i];
        
        if (val == UFT_CW_OVERFLOW) {
            /* 0x00 means add 256 to next value */
            accum += 256;
        } else {
            flux[(*flux_count)++] = accum + val;
            accum = 0;
        }
    }
    
    return 0;
}

int uft_cw_flux_to_raw(const uint32_t *flux, size_t flux_count,
                       uint8_t *raw, size_t *raw_len, size_t max_raw)
{
    if (!flux || !raw || !raw_len) return -1;
    
    *raw_len = 0;
    
    for (size_t i = 0; i < flux_count && *raw_len < max_raw; i++) {
        uint32_t val = flux[i];
        
        /* Insert overflow markers for large values */
        while (val > 255 && *raw_len < max_raw - 1) {
            raw[(*raw_len)++] = UFT_CW_OVERFLOW;
            val -= 256;
        }
        
        if (*raw_len < max_raw) {
            raw[(*raw_len)++] = (uint8_t)val;
        }
    }
    
    return 0;
}

/*===========================================================================
 * Analysis
 *===========================================================================*/

uft_cw_model_t uft_cw_detect_model(const uft_cw_track_t *track)
{
    if (!track || !track->data || track->length == 0) {
        return UFT_CW_MODEL_UNKNOWN;
    }
    
    /* Analyze timing distribution to detect MK3 vs MK4 */
    /* MK4 values should be roughly 2x MK3 for same disk */
    
    uint32_t sum = 0;
    uint32_t count = 0;
    uint32_t accum = 0;
    
    for (size_t i = 0; i < track->length; i++) {
        if (track->data[i] == 0) {
            accum += 256;
        } else {
            sum += accum + track->data[i];
            count++;
            accum = 0;
        }
    }
    
    if (count == 0) return UFT_CW_MODEL_UNKNOWN;
    
    double mean = (double)sum / count;
    
    /* MK3 at 14.16 MHz: 2µs pulse = ~28 ticks
     * MK4 at 28.32 MHz: 2µs pulse = ~56 ticks
     * Typical MFM 1T values: ~50 for MK3, ~100 for MK4
     */
    
    if (mean < 80) {
        return UFT_CW_MODEL_MK3;
    } else {
        return UFT_CW_MODEL_MK4;
    }
}

uint32_t uft_cw_find_index(const uft_cw_track_t *track)
{
    if (!track || !track->data) return 0;
    
    /* Index pulse often appears as a long gap in the data
     * Look for an unusually large timing value */
    
    uint32_t max_val = 0;
    uint32_t max_pos = 0;
    uint32_t accum = 0;
    uint32_t pos = 0;
    
    for (size_t i = 0; i < track->length; i++) {
        if (track->data[i] == 0) {
            accum += 256;
        } else {
            uint32_t val = accum + track->data[i];
            if (val > max_val) {
                max_val = val;
                max_pos = pos;
            }
            accum = 0;
            pos++;
        }
    }
    
    return max_pos;
}

double uft_cw_rotation_time(const uft_cw_track_t *track)
{
    if (!track || !track->data || track->length == 0) return 0;
    
    /* Sum all timing values */
    uint64_t total = 0;
    uint32_t accum = 0;
    
    for (size_t i = 0; i < track->length; i++) {
        if (track->data[i] == 0) {
            accum += 256;
        } else {
            total += accum + track->data[i];
            accum = 0;
        }
    }
    
    /* Convert to microseconds */
    double us = (double)total * 1000000.0 / track->sample_clock;
    return us;
}

uint32_t uft_cw_estimate_datarate(const uft_cw_track_t *track)
{
    if (!track || !track->data || track->length == 0) return 0;
    
    /* Build simple histogram of timing values */
    uint32_t hist[512] = {0};
    uint32_t accum = 0;
    
    for (size_t i = 0; i < track->length; i++) {
        if (track->data[i] == 0) {
            accum += 256;
        } else {
            uint32_t val = accum + track->data[i];
            if (val < 512) hist[val]++;
            accum = 0;
        }
    }
    
    /* Find first significant peak (1T cell time) */
    uint32_t max_count = 0;
    uint32_t peak_pos = 0;
    
    for (int i = 10; i < 200; i++) {
        if (hist[i] > max_count) {
            max_count = hist[i];
            peak_pos = i;
        }
    }
    
    if (peak_pos == 0) return 0;
    
    /* Convert to data rate */
    double cell_ns = uft_cw_to_ns(peak_pos, track->sample_clock);
    if (cell_ns <= 0) return 0;
    
    return (uint32_t)(1000000000.0 / cell_ns);
}

/*===========================================================================
 * Format Conversion
 *===========================================================================*/

int uft_cw_track_to_scp(const uft_cw_track_t *track, 
                        uint16_t *scp_data, size_t *scp_len,
                        uint32_t scp_clock)
{
    if (!track || !track->data || !scp_data || !scp_len) return -1;
    
    /* Convert CW timing to SCP 16-bit format */
    *scp_len = 0;
    uint32_t accum = 0;
    double scale = (double)scp_clock / track->sample_clock;
    
    for (size_t i = 0; i < track->length; i++) {
        if (track->data[i] == 0) {
            accum += 256;
        } else {
            uint32_t cw_val = accum + track->data[i];
            uint32_t scp_val = (uint32_t)(cw_val * scale + 0.5);
            
            /* SCP uses 0x0000 for overflow */
            while (scp_val >= 65536) {
                scp_data[(*scp_len)++] = 0;
                scp_val -= 65536;
            }
            scp_data[(*scp_len)++] = (uint16_t)scp_val;
            
            accum = 0;
        }
    }
    
    return 0;
}

int uft_cw_track_from_scp(uft_cw_track_t *track,
                          const uint16_t *scp_data, size_t scp_len,
                          uint32_t scp_clock)
{
    if (!track || !scp_data) return -1;
    
    /* Estimate output size */
    size_t max_raw = scp_len * 4; /* Worst case with overflows */
    track->data = malloc(max_raw);
    if (!track->data) return -1;
    
    double scale = (double)track->sample_clock / scp_clock;
    track->length = 0;
    uint32_t accum = 0;
    
    for (size_t i = 0; i < scp_len; i++) {
        uint16_t val = scp_data[i];
        
        if (val == 0) {
            accum += 65536;
        } else {
            uint32_t cw_val = (uint32_t)((accum + val) * scale + 0.5);
            
            /* Encode with overflow markers */
            while (cw_val > 255) {
                track->data[track->length++] = 0;
                cw_val -= 256;
            }
            track->data[track->length++] = (uint8_t)cw_val;
            
            accum = 0;
        }
    }
    
    return 0;
}
