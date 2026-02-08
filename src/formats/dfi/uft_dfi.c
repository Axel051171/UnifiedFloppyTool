/**
 * @file uft_dfi.c
 * @brief DFI (DiscFerret Image) format implementation
 * @version 3.9.0
 * 
 * DiscFerret raw flux capture format.
 * Reference: DiscFerret documentation
 */

#include "uft/formats/uft_dfi.h"
#include "uft/uft_format_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static uint32_t read_le32(const uint8_t *p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read_le16(const uint8_t *p) {
    return p[0] | (p[1] << 8);
}

static void write_le32(uint8_t *p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

static void write_le16(uint8_t *p, uint16_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

/* ============================================================================
 * Initialization Functions
 * ============================================================================ */

void uft_dfi_image_init(dfi_image_t *image) {
    if (!image) return;
    memset(image, 0, sizeof(*image));
    image->sample_rate = DFI_DEFAULT_SAMPLE_RATE;
}

void uft_dfi_image_free(dfi_image_t *image) {
    if (!image) return;
    
    if (image->tracks) {
        for (size_t i = 0; i < image->track_count; i++) {
            free(image->tracks[i].flux_times);
            free(image->tracks[i].index_times);
        }
        free(image->tracks);
    }
    
    memset(image, 0, sizeof(*image));
}

void uft_dfi_read_options_init(dfi_read_options_t *opts) {
    if (!opts) return;
    memset(opts, 0, sizeof(*opts));
    opts->decode_flux = false;
    opts->revolution = 0;
}

void uft_dfi_write_options_init(dfi_write_options_t *opts) {
    if (!opts) return;
    memset(opts, 0, sizeof(*opts));
    opts->sample_rate = DFI_DEFAULT_SAMPLE_RATE;
    opts->include_index = true;
}

/* ============================================================================
 * Header Validation
 * ============================================================================ */

bool uft_dfi_validate_header(const dfi_file_header_t *header) {
    if (!header) return false;
    return memcmp(header->magic, DFI_MAGIC, DFI_MAGIC_LEN) == 0;
}

bool uft_dfi_probe(const uint8_t *data, size_t size, int *confidence) {
    if (!data || size < DFI_HEADER_SIZE) return false;
    
    const dfi_file_header_t *header = (const dfi_file_header_t *)data;
    if (uft_dfi_validate_header(header)) {
        if (confidence) *confidence = 95;
        return true;
    }
    
    return false;
}

/* ============================================================================
 * Track Data Parsing
 * ============================================================================ */

static uft_error_t parse_track_data(const uint8_t *data, size_t size,
                                    dfi_track_data_t *track,
                                    uint32_t sample_rate) {
    if (!data || !track || size == 0) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    track->sample_rate = sample_rate;
    
    /* Count flux transitions and index pulses */
    size_t flux_count = 0;
    size_t index_count = 0;
    
    size_t pos = 0;
    while (pos < size) {
        uint8_t byte = data[pos++];
        
        if (byte == DFI_DATA_EXTENDED) {
            if (pos + 2 > size) break;
            pos += 2;  /* Extended value is 2 bytes */
            flux_count++;
        } else if (byte & DFI_DATA_INDEX) {
            index_count++;
        } else {
            flux_count++;
        }
    }
    
    /* Allocate arrays */
    if (flux_count > 0) {
        track->flux_times = malloc(flux_count * sizeof(uint32_t));
        if (!track->flux_times) return UFT_ERR_MEMORY;
    }
    
    if (index_count > 0) {
        track->index_times = malloc(index_count * sizeof(uint32_t));
        if (!track->index_times) {
            free(track->flux_times);
            track->flux_times = NULL;
            return UFT_ERR_MEMORY;
        }
    }
    
    /* Parse flux data */
    pos = 0;
    size_t fi = 0, ii = 0;
    uint32_t current_time = 0;
    
    while (pos < size) {
        uint8_t byte = data[pos++];
        
        if (byte == DFI_DATA_EXTENDED) {
            if (pos + 2 > size) break;
            uint16_t ext_time = read_le16(data + pos);
            pos += 2;
            current_time += ext_time;
            if (fi < flux_count) {
                track->flux_times[fi++] = current_time;
            }
        } else if (byte & DFI_DATA_INDEX) {
            /* Index pulse */
            if (ii < index_count) {
                track->index_times[ii++] = current_time;
            }
        } else {
            /* Delta time */
            current_time += byte;
            if (fi < flux_count) {
                track->flux_times[fi++] = current_time;
            }
        }
    }
    
    track->flux_count = fi;
    track->index_count = ii;
    track->total_time = current_time;
    
    return UFT_OK;
}

/* ============================================================================
 * Read Implementation
 * ============================================================================ */

uft_error_t uft_dfi_read_mem(const uint8_t *data, size_t size,
                             dfi_image_t *image,
                             const dfi_read_options_t *opts,
                             dfi_read_result_t *result) {
    if (!data || !image || size < DFI_HEADER_SIZE) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Initialize result */
    if (result) {
        memset(result, 0, sizeof(*result));
        result->image_size = size;
    }
    
    /* Validate header */
    const dfi_file_header_t *header = (const dfi_file_header_t *)data;
    if (!uft_dfi_validate_header(header)) {
        if (result) {
            result->error = UFT_ERR_FORMAT;
            result->error_detail = "Invalid DFI signature";
        }
        return UFT_ERR_FORMAT;
    }
    
    uft_dfi_image_init(image);
    memcpy(&image->header, header, sizeof(*header));
    
    uint32_t sample_rate = (opts && opts->sample_rate) ? 
                           opts->sample_rate : DFI_DEFAULT_SAMPLE_RATE;
    image->sample_rate = sample_rate;
    
    /* Count tracks */
    size_t track_count = 0;
    size_t pos = DFI_HEADER_SIZE;
    
    while (pos + sizeof(dfi_track_header_t) <= size) {
        if (memcmp(data + pos, DFI_TRACK_MAGIC, DFI_TRACK_MAGIC_LEN) != 0) {
            break;
        }
        
        uint32_t track_len = read_le32(data + pos + 4);
        if (pos + sizeof(dfi_track_header_t) + track_len > size) {
            break;
        }
        
        track_count++;
        pos += sizeof(dfi_track_header_t) + track_len;
    }
    
    if (track_count == 0) {
        if (result) {
            result->error = UFT_ERR_FORMAT;
            result->error_detail = "No tracks found in DFI file";
        }
        return UFT_ERR_FORMAT;
    }
    
    /* Allocate tracks */
    image->tracks = calloc(track_count, sizeof(dfi_track_data_t));
    if (!image->tracks) {
        return UFT_ERR_MEMORY;
    }
    image->track_count = track_count;
    
    /* Parse tracks */
    pos = DFI_HEADER_SIZE;
    size_t ti = 0;
    uint8_t max_cyl = 0, max_head = 0;
    
    while (pos + sizeof(dfi_track_header_t) <= size && ti < track_count) {
        if (memcmp(data + pos, DFI_TRACK_MAGIC, DFI_TRACK_MAGIC_LEN) != 0) {
            break;
        }
        
        uint32_t track_len = read_le32(data + pos + 4);
        const uint8_t *track_data = data + pos + sizeof(dfi_track_header_t);
        
        dfi_track_data_t *track = &image->tracks[ti];
        
        /* Derive cylinder/head from track index */
        /* DFI typically stores tracks in cylinder order, alternating heads */
        track->cylinder = ti / 2;
        track->head = ti % 2;
        
        if (track->cylinder > max_cyl) max_cyl = track->cylinder;
        if (track->head > max_head) max_head = track->head;
        
        /* Parse track flux data */
        parse_track_data(track_data, track_len, track, sample_rate);
        
        if (result) {
            result->total_flux_count += track->flux_count;
            result->total_index_count += track->index_count;
        }
        
        pos += sizeof(dfi_track_header_t) + track_len;
        ti++;
    }
    
    image->cylinders = max_cyl + 1;
    image->heads = max_head + 1;
    
    if (result) {
        result->success = true;
        result->cylinders = image->cylinders;
        result->heads = image->heads;
        result->sample_rate = sample_rate;
        result->track_count = track_count;
    }
    
    return UFT_OK;
}

uft_error_t uft_dfi_read(const char *path,
                         dfi_image_t *image,
                         const dfi_read_options_t *opts,
                         dfi_read_result_t *result) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return UFT_ERR_IO;
    }
    
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(fp);
        return UFT_ERR_MEMORY;
    }
    
    if (fread(data, 1, size, fp) != size) {
        free(data);
        fclose(fp);
        return UFT_ERR_IO;
    }
    fclose(fp);
    
    uft_error_t err = uft_dfi_read_mem(data, size, image, opts, result);
    free(data);
    
    return err;
}

/* ============================================================================
 * Write Implementation
 * ============================================================================ */

uft_error_t uft_dfi_write(const dfi_image_t *image,
                          const char *path,
                          const dfi_write_options_t *opts) {
    if (!image || !path) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Default options */
    dfi_write_options_t default_opts;
    if (!opts) {
        uft_dfi_write_options_init(&default_opts);
        opts = &default_opts;
    }
    
    /* Calculate output size */
    size_t output_size = DFI_HEADER_SIZE;
    
    for (size_t t = 0; t < image->track_count; t++) {
        dfi_track_data_t *track = &image->tracks[t];
        /* Estimate track data size (flux transitions + some overhead) */
        output_size += sizeof(dfi_track_header_t);
        output_size += track->flux_count * 3;  /* Worst case: extended format */
        if (opts->include_index) {
            output_size += track->index_count;
        }
    }
    
    uint8_t *output = malloc(output_size);
    if (!output) {
        return UFT_ERR_MEMORY;
    }
    memset(output, 0, output_size);
    
    /* Write header */
    dfi_file_header_t *header = (dfi_file_header_t *)output;
    memcpy(header->magic, DFI_MAGIC, DFI_MAGIC_LEN);
    write_le16((uint8_t*)&header->version, 2);
    header->flags = 0;
    
    size_t pos = DFI_HEADER_SIZE;
    
    /* Write tracks */
    for (size_t t = 0; t < image->track_count; t++) {
        dfi_track_data_t *track = &image->tracks[t];
        
        /* Track header placeholder */
        size_t track_start = pos;
        memcpy(output + pos, DFI_TRACK_MAGIC, DFI_TRACK_MAGIC_LEN);
        pos += sizeof(dfi_track_header_t);
        
        size_t track_data_start = pos;
        
        /* Encode flux transitions */
        uint32_t prev_time = 0;
        size_t idx_pos = 0;
        
        for (size_t f = 0; f < track->flux_count; f++) {
            /* Check for index pulse */
            if (opts->include_index && idx_pos < track->index_count) {
                if (track->index_times[idx_pos] <= track->flux_times[f]) {
                    output[pos++] = DFI_DATA_INDEX;
                    idx_pos++;
                }
            }
            
            uint32_t delta = track->flux_times[f] - prev_time;
            prev_time = track->flux_times[f];
            
            if (delta < 127) {
                output[pos++] = (uint8_t)delta;
            } else {
                /* Extended format */
                output[pos++] = DFI_DATA_EXTENDED;
                write_le16(output + pos, delta > 65535 ? 65535 : delta);
                pos += 2;
            }
        }
        
        /* Update track length */
        uint32_t track_len = pos - track_data_start;
        write_le32(output + track_start + 4, track_len);
    }
    
    /* Write file */
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        free(output);
        return UFT_ERR_IO;
    }
    
    size_t written = fwrite(output, 1, pos, fp);
    fclose(fp);
    free(output);
    
    if (written != pos) {
        return UFT_ERR_IO;
    }
    
    return UFT_OK;
}

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

dfi_track_data_t* uft_dfi_get_track(dfi_image_t *image, uint8_t cyl, uint8_t head) {
    if (!image || !image->tracks) return NULL;
    
    for (size_t i = 0; i < image->track_count; i++) {
        if (image->tracks[i].cylinder == cyl && image->tracks[i].head == head) {
            return &image->tracks[i];
        }
    }
    
    return NULL;
}

uint32_t uft_dfi_calc_bitrate(const dfi_track_data_t *track) {
    if (!track || track->flux_count < 2) return 0;
    
    /* Calculate average flux interval */
    uint64_t total_interval = track->total_time;
    uint32_t avg_interval = total_interval / track->flux_count;
    
    /* Convert to bit rate based on sample rate */
    if (avg_interval == 0) return 0;
    
    /* For MFM, each flux represents approximately 2 bit cells */
    uint32_t bit_rate = (track->sample_rate / avg_interval) * 2;
    
    return bit_rate;
}

uft_encoding_t uft_dfi_detect_encoding(const dfi_track_data_t *track) {
    if (!track || track->flux_count < 100) return UFT_ENC_MFM;
    
    uint32_t bitrate = uft_dfi_calc_bitrate(track);
    
    /* FM typically runs at 125-250 kbps, MFM at 250-500 kbps */
    if (bitrate < 200000) {
        return UFT_ENC_FM;
    }
    
    return UFT_ENC_MFM;
}

/* ============================================================================
 * Format Plugin Registration
 * ============================================================================ */

static bool dfi_probe_plugin(const uint8_t *data, size_t size,
                             size_t file_size, int *confidence) {
    (void)file_size;
    return uft_dfi_probe(data, size, confidence);
}

static uft_error_t dfi_open(uft_disk_t *disk, const char *path, bool read_only) {
    (void)read_only;
    
    dfi_image_t *dfi_image = malloc(sizeof(dfi_image_t));
    if (!dfi_image) return UFT_ERR_MEMORY;
    
    uft_dfi_image_init(dfi_image);
    
    uft_error_t err = uft_dfi_read(path, dfi_image, NULL, NULL);
    if (err != UFT_OK) {
        uft_dfi_image_free(dfi_image);
        free(dfi_image);
        return err;
    }
    
    disk->plugin_data = dfi_image;
    disk->geometry.cylinders = dfi_image->cylinders;
    disk->geometry.heads = dfi_image->heads;
    disk->geometry.sectors = 0;  /* Flux data doesn't have fixed sectors */
    disk->geometry.sector_size = 0;
    
    return UFT_OK;
}

static void dfi_close(uft_disk_t *disk) {
    if (disk && disk->plugin_data) {
        dfi_image_t *dfi_image = (dfi_image_t*)disk->plugin_data;
        uft_dfi_image_free(dfi_image);
        free(dfi_image);
        disk->plugin_data = NULL;
    }
}

static uft_error_t dfi_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track) {
    (void)disk;
    (void)cyl;
    (void)head;
    (void)track;
    
    /* DFI stores raw flux, not sectors - would need flux decoder */
    return UFT_ERR_NOT_SUPPORTED;
}

const uft_format_plugin_t uft_format_plugin_dfi = {
    .name = "DFI",
    .description = "DiscFerret Raw Flux Image",
    .extensions = "dfi",
    .format = UFT_FORMAT_FLUX,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_FLUX,
    .probe = dfi_probe_plugin,
    .open = dfi_open,
    .close = dfi_close,
    .read_track = dfi_read_track,
};

UFT_REGISTER_FORMAT_PLUGIN(dfi)
