/**
 * @file uft_kryoflux_v9.c
 * @brief KryoFlux flux stream processing
 * @version 3.8.0
 */
/**
 * @file uft_kryoflux.c
 * 
 * Based on Aufit by Jean Louis-Guerin / Software Preservation Society
 */

#include "uft_kryoflux.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*===========================================================================
 * Initialization / Cleanup
 *===========================================================================*/

uft_kf_status_t uft_kf_init(uft_kf_stream_t *stream) {
    if (!stream) return UFT_UFT_KF_STATUS_READ_ERROR;
    
    memset(stream, 0, sizeof(*stream));
    
    /* Default clock values */
    stream->sample_clock = UFT_UFT_KF_SAMPLE_CLOCK;
    stream->index_clock = UFT_UFT_KF_INDEX_CLOCK;
    
    /* Initial allocation */
    stream->flux_capacity = 65536;
    stream->flux_values = malloc(stream->flux_capacity * sizeof(uint32_t));
    stream->flux_positions = malloc(stream->flux_capacity * sizeof(uint32_t));
    
    if (!stream->flux_values || !stream->flux_positions) {
        uft_kf_free(stream);
        return UFT_UFT_KF_STATUS_READ_ERROR;
    }
    
    stream->indexes = malloc(UFT_UFT_KF_MAX_INDEX * sizeof(uft_kf_index_t));
    stream->index_internal = malloc(UFT_UFT_KF_MAX_INDEX * sizeof(uft_kf_index_internal_t));
    
    if (!stream->indexes || !stream->index_internal) {
        uft_kf_free(stream);
        return UFT_UFT_KF_STATUS_READ_ERROR;
    }
    
    return UFT_UFT_KF_STATUS_OK;
}

void uft_kf_free(uft_kf_stream_t *stream) {
    if (!stream) return;
    
    free(stream->flux_values);
    free(stream->flux_positions);
    free(stream->indexes);
    free(stream->index_internal);
    
    memset(stream, 0, sizeof(*stream));
}

void uft_kf_reset(uft_kf_stream_t *stream) {
    if (!stream) return;
    
    stream->flux_count = 0;
    stream->index_count = 0;
    stream->info_string[0] = '\0';
    stream->data_count = 0;
    stream->data_time = 0;
    memset(&stream->stats, 0, sizeof(stream->stats));
}

/*===========================================================================
 * Internal Helpers
 *===========================================================================*/

static uft_kf_status_t ensure_flux_capacity(uft_kf_stream_t *stream, uint32_t needed) {
    if (needed <= stream->flux_capacity) {
        return UFT_UFT_KF_STATUS_OK;
    }
    
    uint32_t new_cap = stream->flux_capacity * 2;
    while (new_cap < needed) {
        new_cap *= 2;
    }
    
    uint32_t *new_values = realloc(stream->flux_values, new_cap * sizeof(uint32_t));
    uint32_t *new_positions = realloc(stream->flux_positions, new_cap * sizeof(uint32_t));
    
    if (!new_values || !new_positions) {
        return UFT_UFT_KF_STATUS_READ_ERROR;
    }
    
    stream->flux_values = new_values;
    stream->flux_positions = new_positions;
    stream->flux_capacity = new_cap;
    
    return UFT_UFT_KF_STATUS_OK;
}

static void emit_flux(uft_kf_stream_t *stream, uint32_t value, uint32_t stream_pos) {
    if (ensure_flux_capacity(stream, stream->flux_count + 1) != UFT_UFT_KF_STATUS_OK) {
        return;
    }
    
    stream->flux_values[stream->flux_count] = value;
    stream->flux_positions[stream->flux_count] = stream_pos;
    stream->flux_count++;
}

/*===========================================================================
 * OOB Block Handling
 *===========================================================================*/

static uft_kf_status_t handle_oob(uft_kf_stream_t *stream,
                                  const uint8_t *data, size_t pos,
                                  uint8_t type, uint16_t size,
                                  uint32_t stream_pos) {
    switch (type) {
        case UFT_UFT_KF_OOB_STREAM_INFO:
            /* Parse stream info string */
            if (size > 0 && size < sizeof(stream->info_string)) {
                memcpy(stream->info_string, data + pos, size);
                stream->info_string[size] = '\0';
            }
            break;
            
        case UFT_UFT_KF_OOB_INDEX:
            if (size >= 12 && stream->index_count < UFT_UFT_KF_MAX_INDEX) {
                uft_kf_index_internal_t *idx = &stream->index_internal[stream->index_count];
                idx->stream_pos = uft_kf_read_u32(data + pos);
                idx->sample_counter = uft_kf_read_u32(data + pos + 4);
                idx->index_counter = uft_kf_read_u32(data + pos + 8);
                stream->index_count++;
            }
            break;
            
        case UFT_UFT_KF_OOB_STREAM_END:
            if (size >= 8) {
                uint32_t result = uft_kf_read_u32(data + pos + 4);
                if (result == UFT_UFT_KF_RESULT_BUFFERING) {
                    return UFT_UFT_KF_STATUS_DEV_BUFFER;
                }
                if (result == UFT_UFT_KF_RESULT_NO_INDEX) {
                    return UFT_UFT_KF_STATUS_DEV_INDEX;
                }
            }
            break;
            
        case UFT_UFT_KF_OOB_UFT_KF_INFO:
            /* Additional hardware info - append to info string */
            break;
            
        case UFT_UFT_KF_OOB_EOF:
            /* End of file marker */
            break;
            
        default:
            return UFT_UFT_KF_STATUS_INVALID_OOB;
    }
    
    return UFT_UFT_KF_STATUS_OK;
}

/*===========================================================================
 * Main Decoder
 *===========================================================================*/

uft_kf_status_t uft_kf_decode(uft_kf_stream_t *stream,
                              const uint8_t *data, size_t len) {
    if (!stream || !data || len == 0) {
        return UFT_UFT_KF_STATUS_READ_ERROR;
    }
    
    uft_kf_reset(stream);
    
    size_t pos = 0;
    uint32_t flux = 0;
    uint32_t stream_pos = 0;
    bool eof_found = false;
    
    while (pos < len && !eof_found) {
        uint8_t code = data[pos++];
        stream_pos++;
        
        if (code <= UFT_UFT_KF_FLUX1_MAX) {
            /* Flux1: short value (0-7) */
            flux += code;
            emit_flux(stream, flux, stream_pos);
            flux = 0;
        }
        else if (code == UFT_UFT_KF_FLUX2) {
            /* Flux2: medium value */
            if (pos >= len) return UFT_UFT_KF_STATUS_MISSING_DATA;
            flux += data[pos++];
            stream_pos++;
            emit_flux(stream, flux, stream_pos);
            flux = 0;
        }
        else if (code == UFT_UFT_KF_FLUX3 || code == UFT_UFT_KF_FLUX3_ALT) {
            /* Flux3: long value */
            if (pos + 1 >= len) return UFT_UFT_KF_STATUS_MISSING_DATA;
            flux += uft_kf_read_u16(data + pos);
            pos += 2;
            stream_pos += 2;
            emit_flux(stream, flux, stream_pos);
            flux = 0;
        }
        else if (code == UFT_UFT_KF_OVERFLOW) {
            /* Overflow: add 65536 */
            flux += 65536;
        }
        else if (code == UFT_UFT_KF_NOP1) {
            /* NOP1: skip 1 byte */
            if (pos >= len) return UFT_UFT_KF_STATUS_MISSING_DATA;
            pos++;
            stream_pos++;
        }
        else if (code == UFT_UFT_KF_OOB) {
            /* OOB block */
            if (pos + 2 >= len) return UFT_UFT_KF_STATUS_MISSING_DATA;
            
            uint8_t oob_type = data[pos++];
            uint16_t oob_size = uft_kf_read_u16(data + pos);
            pos += 2;
            stream_pos += 3;
            
            if (pos + oob_size > len) return UFT_UFT_KF_STATUS_MISSING_DATA;
            
            uft_kf_status_t status = handle_oob(stream, data, pos, 
                                                oob_type, oob_size, stream_pos);
            if (status != UFT_UFT_KF_STATUS_OK) {
                return status;
            }
            
            if (oob_type == UFT_UFT_KF_OOB_EOF) {
                eof_found = true;
            }
            
            pos += oob_size;
            stream_pos += oob_size;
        }
        else if (code == UFT_UFT_KF_NOP3) {
            /* NOP3: skip 3 bytes */
            if (pos + 2 >= len) return UFT_UFT_KF_STATUS_MISSING_DATA;
            pos += 3;
            stream_pos += 3;
        }
        else {
            return UFT_UFT_KF_STATUS_INVALID_CODE;
        }
    }
    
    if (!eof_found) {
        return UFT_UFT_KF_STATUS_MISSING_END;
    }
    
    /* Process index data */
    uft_kf_status_t status = uft_kf_process_indexes(stream);
    if (status != UFT_UFT_KF_STATUS_OK) {
        return status;
    }
    
    /* Calculate statistics */
    uft_uft_kf_calc_stats(stream);
    
    return UFT_UFT_KF_STATUS_OK;
}

/*===========================================================================
 * Index Processing
 *===========================================================================*/

static uft_kf_status_t uft_kf_process_indexes(uft_kf_stream_t *stream) {
    if (stream->index_count == 0 || stream->flux_count == 0) {
        return UFT_UFT_KF_STATUS_OK;
    }
    
    uint32_t iidx = 0;  /* Index counter */
    uint32_t itime = 0; /* Accumulated time */
    uint32_t next_stream_pos = stream->index_internal[0].stream_pos;
    
    for (uint32_t fidx = 0; fidx < stream->flux_count; fidx++) {
        itime += stream->flux_values[fidx];
        
        uint32_t next_fidx = fidx + 1;
        
        /* Check if next flux position reaches index point */
        if (stream->flux_positions[next_fidx] < next_stream_pos) {
            continue;
        }
        
        /* Handle edge case: first flux has index */
        if (fidx == 0 && stream->flux_positions[0] >= next_stream_pos) {
            next_fidx = 0;
        }
        
        if (iidx < stream->index_count) {
            uft_kf_index_t *idx = &stream->indexes[iidx];
            uft_kf_index_internal_t *iint = &stream->index_internal[iidx];
            
            idx->flux_position = next_fidx;
            
            /* Calculate pre-index time */
            uint32_t iftime = stream->flux_values[next_fidx];
            uint32_t sample_ctr = iint->sample_counter;
            
            if (sample_ctr == 0) {
                sample_ctr = iftime & 0xFFFF;
            }
            
            uint32_t overflow_count = iftime >> 16;
            uint32_t pre_overflow = stream->flux_positions[next_fidx] - next_stream_pos;
            
            if (overflow_count < pre_overflow) {
                return UFT_UFT_KF_STATUS_MISSING_INDEX;
            }
            
            uint32_t pre_index = ((overflow_count - pre_overflow) << 16) + sample_ctr;
            idx->pre_index_time = pre_index;
            
            /* Calculate rotation time */
            if (iidx > 0) {
                itime -= stream->indexes[iidx - 1].pre_index_time;
            }
            idx->rotation_time = (next_fidx != 0 ? itime : 0) + pre_index;
            
            /* Next index */
            iidx++;
            if (iidx < stream->index_count) {
                next_stream_pos = stream->index_internal[iidx].stream_pos;
            }
            
            if (next_fidx != 0) {
                itime = 0;
            }
        }
    }
    
    if (iidx < stream->index_count) {
        return UFT_UFT_KF_STATUS_MISSING_INDEX;
    }
    
    return UFT_UFT_KF_STATUS_OK;
}

/*===========================================================================
 * Statistics
 *===========================================================================*/

void uft_uft_kf_calc_stats(uft_kf_stream_t *stream) {
    if (!stream || stream->flux_count == 0) return;
    
    /* Find min/max flux values */
    stream->stats.min_flux = stream->flux_values[0];
    stream->stats.max_flux = stream->flux_values[0];
    
    for (uint32_t i = 0; i < stream->flux_count; i++) {
        if (stream->flux_values[i] < stream->stats.min_flux) {
            stream->stats.min_flux = stream->flux_values[i];
        }
        if (stream->flux_values[i] > stream->stats.max_flux) {
            stream->stats.max_flux = stream->flux_values[i];
        }
    }
    
    /* Calculate RPM from indexes */
    if (stream->index_count > 1) {
        uint64_t sum = 0;
        uint32_t min_rot = UINT32_MAX;
        uint32_t max_rot = 0;
        
        /* Skip first index (incomplete revolution) */
        for (uint32_t i = 1; i < stream->index_count; i++) {
            uint32_t rot = stream->indexes[i].rotation_time;
            sum += rot;
            if (rot < min_rot) min_rot = rot;
            if (rot > max_rot) max_rot = rot;
        }
        
        uint32_t count = stream->index_count - 1;
        stream->stats.avg_rpm = (stream->sample_clock * count * 60.0) / (double)sum;
        stream->stats.min_rpm = (stream->sample_clock * 60.0) / (double)max_rot;
        stream->stats.max_rpm = (stream->sample_clock * 60.0) / (double)min_rot;
        
        /* Average flux per revolution */
        if (stream->index_count >= 3) {
            stream->stats.flux_per_rev = stream->indexes[2].flux_position - 
                                         stream->indexes[1].flux_position;
        }
    }
    
    /* Transfer rate */
    if (stream->data_time > 0) {
        stream->stats.avg_bps = (double)stream->data_count * 1000.0 / 
                                (double)stream->data_time;
    }
}

/*===========================================================================
 * File I/O
 *===========================================================================*/

uft_kf_status_t uft_kf_decode_file(uft_kf_stream_t *stream, const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        return UFT_UFT_KF_STATUS_READ_ERROR;
    }
    
    /* Get file size */
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    long size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    if (size <= 0 || size > 100 * 1024 * 1024) {  /* 100MB limit */
        fclose(f);
        return UFT_UFT_KF_STATUS_READ_ERROR;
    }
    
    /* Read entire file */
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(f);
        return UFT_UFT_KF_STATUS_READ_ERROR;
    }
    
    if (fread(data, 1, size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return UFT_UFT_KF_STATUS_READ_ERROR;
    }
    fclose(f);
    
    /* Decode */
    uft_kf_status_t status = uft_kf_decode(stream, data, size);
    
    free(data);
    return status;
}

/*===========================================================================
 * Histogram Analysis
 *===========================================================================*/

void uft_uft_kf_build_histogram(const uft_kf_stream_t *stream,
                            uint32_t *histogram, uint32_t max_value) {
    if (!stream || !histogram) return;
    
    memset(histogram, 0, (max_value + 1) * sizeof(uint32_t));
    
    for (uint32_t i = 0; i < stream->flux_count; i++) {
        uint32_t val = stream->flux_values[i];
        if (val <= max_value) {
            histogram[val]++;
        }
    }
}

int uft_kf_find_histogram_peaks(const uint32_t *histogram, size_t len,
                                uint32_t *peaks, int max_peaks) {
    if (!histogram || !peaks || max_peaks <= 0) return 0;
    
    int peak_count = 0;
    
    /* Simple peak detection: local maxima with minimum prominence */
    const int window = 10;  /* Minimum distance between peaks */
    const double min_prominence = 0.1;  /* Minimum relative height */
    
    /* Find maximum value for threshold */
    uint32_t max_val = 0;
    for (size_t i = 0; i < len; i++) {
        if (histogram[i] > max_val) {
            max_val = histogram[i];
        }
    }
    
    uint32_t threshold = (uint32_t)(max_val * min_prominence);
    
    for (size_t i = window; i + window < len && peak_count < max_peaks; i++) {
        if (histogram[i] < threshold) continue;
        
        /* Check if local maximum */
        bool is_peak = true;
        for (int j = -window; j <= window && is_peak; j++) {
            if (j != 0 && histogram[i + j] >= histogram[i]) {
                is_peak = false;
            }
        }
        
        if (is_peak) {
            peaks[peak_count++] = (uint32_t)i;
        }
    }
    
    return peak_count;
}
