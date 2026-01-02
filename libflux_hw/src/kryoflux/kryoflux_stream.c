// SPDX-License-Identifier: MIT
/*
 * kryoflux_stream.c - KryoFlux Stream Decoder
 * 
 * Professional stream decoder for KryoFlux STREAM format.
 * Based on analysis of disk-utilities by Keir Fraser.
 * 
 * Stream Format Analysis:
 *   - Opcodes 0x00-0x0D for flux timing and control
 *   - Out-of-Band (OOB) data for index pulses and metadata
 *   - Timing conversion from 2µs ticks to nanoseconds
 * 
 * @version 2.7.2
 * @date 2024-12-25
 */

#include "../include/kryoflux_hw.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/*============================================================================*
 * TIMING CONSTANTS (From KryoFlux Hardware Specs)
 *============================================================================*/

/* Master clock frequency: (18.432 MHz * 73) / 14 / 2 */
#define MCK_FREQ ((18432000ULL * 73) / 14 / 2)

/* Sample clock frequency: MCK / 2 */
#define SCK_FREQ (MCK_FREQ / 2)

/* Picoseconds per sample clock tick */
#define SCK_PS_PER_TICK (1000000000ULL / (SCK_FREQ / 1000))

/*============================================================================*
 * STREAM OPCODES (Extracted from analysis)
 *============================================================================*/

typedef enum {
    /* Flux samples (0x00-0x07): 2-byte values */
    KF_OPCODE_FLUX2_0   = 0x00,
    KF_OPCODE_FLUX2_1   = 0x01,
    KF_OPCODE_FLUX2_2   = 0x02,
    KF_OPCODE_FLUX2_3   = 0x03,
    KF_OPCODE_FLUX2_4   = 0x04,
    KF_OPCODE_FLUX2_5   = 0x05,
    KF_OPCODE_FLUX2_6   = 0x06,
    KF_OPCODE_FLUX2_7   = 0x07,
    
    /* NOPs (padding) */
    KF_OPCODE_NOP1      = 0x08,  /**< 1-byte NOP */
    KF_OPCODE_NOP2      = 0x09,  /**< 2-byte NOP */
    KF_OPCODE_NOP3      = 0x0A,  /**< 3-byte NOP */
    
    /* Special values */
    KF_OPCODE_OVERFLOW  = 0x0B,  /**< Add 0x10000 to value */
    KF_OPCODE_VALUE16   = 0x0C,  /**< Force 2-byte sample */
    
    /* Out-of-Band data */
    KF_OPCODE_OOB       = 0x0D   /**< OOB block follows */
} kf_stream_opcode_t;

/*============================================================================*
 * OOB DATA TYPES
 *============================================================================*/

typedef enum {
    KF_OOB_STREAM_READ  = 0x01,  /**< Stream position marker */
    KF_OOB_INDEX        = 0x02,  /**< Index pulse position */
    KF_OOB_STREAM_END   = 0x03,  /**< End of stream block */
    KF_OOB_KF_INFO      = 0x04,  /**< KryoFlux device info */
    KF_OOB_EOF          = 0x0D   /**< End of file */
} kf_oob_type_t;

/*============================================================================*
 * INTERNAL STRUCTURES
 *============================================================================*/

/**
 * @brief Stream decoder state
 */
typedef struct {
    uint8_t *data;              /**< Raw stream data */
    size_t data_size;           /**< Size of data */
    size_t data_idx;            /**< Current position in data */
    size_t stream_idx;          /**< Position in non-OOB data */
    
    uint32_t *index_positions;  /**< Index pulse positions */
    size_t index_count;         /**< Number of indices */
    size_t index_idx;           /**< Current index */
    
    uint64_t flux_ns;           /**< Cumulative flux in ns */
} stream_decoder_t;

/*============================================================================*
 * HELPER FUNCTIONS
 *============================================================================*/

/**
 * @brief Read 16-bit little-endian value
 */
static inline uint16_t le16toh_inline(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/**
 * @brief Read 32-bit little-endian value
 */
static inline uint32_t le32toh_inline(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/*============================================================================*
 * INDEX DETECTION
 *============================================================================*/

/**
 * @brief Parse stream to extract index pulse positions
 * 
 * Algorithm extracted from disk-utilities analysis.
 * Scans stream for OOB blocks containing index markers.
 * 
 * @param data Stream data
 * @param size Data size
 * @param index_count_out Number of indices found (output)
 * @return Array of index positions (caller must free)
 */
static uint32_t* decode_index_positions(
    const uint8_t *data,
    size_t size,
    size_t *index_count_out
) {
    #define MAX_INDEX 128
    
    uint32_t *positions = malloc((MAX_INDEX + 1) * sizeof(uint32_t));
    if (!positions) {
        return NULL;
    }
    
    size_t idx_count = 0;
    size_t i = 0;
    
    while (i < size) {
        uint8_t opcode = data[i];
        
        if (opcode == KF_OPCODE_OOB) {
            /* OOB block: 4 bytes header + variable data */
            if (i + 4 > size) break;
            
            uint8_t oob_type = data[i + 1];
            uint16_t oob_size = le16toh_inline(&data[i + 2]);
            
            i += 4;
            
            if (oob_type == KF_OOB_INDEX) {
                /* Index pulse OOB */
                if (oob_size >= 4 && i + 4 <= size) {
                    uint32_t pos = le32toh_inline(&data[i]);
                    
                    if (idx_count < MAX_INDEX) {
                        positions[idx_count++] = pos;
                    }
                }
            } else if (oob_type == KF_OOB_EOF) {
                /* EOF marker */
                break;
            }
            
            i += oob_size;
        } else if (opcode >= KF_OPCODE_FLUX2_0 && opcode <= KF_OPCODE_FLUX2_7) {
            /* 2-byte flux sample */
            i += 2;
        } else if (opcode == KF_OPCODE_NOP1 || opcode == KF_OPCODE_OVERFLOW) {
            i += 1;
        } else if (opcode == KF_OPCODE_NOP2) {
            i += 2;
        } else if (opcode == KF_OPCODE_NOP3 || opcode == KF_OPCODE_VALUE16) {
            i += (opcode == KF_OPCODE_NOP3) ? 3 : 1;
            if (opcode == KF_OPCODE_VALUE16 && i + 2 <= size) {
                i += 2;  /* VALUE16 forces 2-byte sample */
            }
        } else {
            /* 1-byte flux sample (default) */
            i += 1;
        }
    }
    
    positions[idx_count] = 0xFFFFFFFF;  /* Terminator */
    *index_count_out = idx_count;
    
    return positions;
    
    #undef MAX_INDEX
}

/*============================================================================*
 * STREAM DECODER
 *============================================================================*/

/**
 * @brief Decode next flux transition from stream
 * 
 * Core decoder algorithm based on KryoFlux stream format analysis.
 * Implements opcode parsing and timing conversion.
 * 
 * @param decoder Decoder state
 * @param flux_ns_out Flux timing in nanoseconds (output)
 * @param is_index_out True if at index pulse (output)
 * @return 0 on success, -1 on end of stream
 */
static int decode_next_flux(
    stream_decoder_t *decoder,
    uint32_t *flux_ns_out,
    bool *is_index_out
) {
    uint32_t flux_value = 0;
    bool done = false;
    
    /* Check if at index pulse */
    *is_index_out = false;
    if (decoder->index_idx < decoder->index_count) {
        if (decoder->stream_idx >= decoder->index_positions[decoder->index_idx]) {
            *is_index_out = true;
            decoder->index_idx++;
        }
    }
    
    /* Decode flux value from stream */
    while (!done && decoder->data_idx < decoder->data_size) {
        uint8_t opcode = decoder->data[decoder->data_idx];
        
        if (opcode >= KF_OPCODE_FLUX2_0 && opcode <= KF_OPCODE_FLUX2_7) {
            /* 2-byte flux sample */
            if (decoder->data_idx + 2 > decoder->data_size) break;
            
            flux_value += ((uint32_t)opcode << 8) + decoder->data[decoder->data_idx + 1];
            decoder->data_idx += 2;
            decoder->stream_idx += 2;
            done = true;
            
        } else if (opcode == KF_OPCODE_NOP1) {
            decoder->data_idx += 1;
            decoder->stream_idx += 1;
            
        } else if (opcode == KF_OPCODE_NOP2) {
            decoder->data_idx += 2;
            decoder->stream_idx += 2;
            
        } else if (opcode == KF_OPCODE_NOP3) {
            decoder->data_idx += 3;
            decoder->stream_idx += 3;
            
        } else if (opcode == KF_OPCODE_OVERFLOW) {
            flux_value += 0x10000;
            decoder->data_idx += 1;
            decoder->stream_idx += 1;
            
        } else if (opcode == KF_OPCODE_VALUE16) {
            /* VALUE16: force 2-byte sample */
            decoder->data_idx += 1;
            decoder->stream_idx += 1;
            
            if (decoder->data_idx + 2 > decoder->data_size) break;
            
            uint8_t next = decoder->data[decoder->data_idx];
            flux_value += ((uint32_t)next << 8) + decoder->data[decoder->data_idx + 1];
            decoder->data_idx += 2;
            decoder->stream_idx += 2;
            done = true;
            
        } else if (opcode == KF_OPCODE_OOB) {
            /* OOB block */
            if (decoder->data_idx + 4 > decoder->data_size) break;
            
            uint8_t oob_type = decoder->data[decoder->data_idx + 1];
            uint16_t oob_size = le16toh_inline(&decoder->data[decoder->data_idx + 2]);
            
            decoder->data_idx += 4;
            
            /* Handle different OOB types */
            if (oob_type == KF_OOB_STREAM_READ || oob_type == KF_OOB_STREAM_END) {
                /* Verify stream position */
                if (oob_size >= 4 && decoder->data_idx + 4 <= decoder->data_size) {
                    uint32_t pos = le32toh_inline(&decoder->data[decoder->data_idx]);
                    if (pos != decoder->stream_idx) {
                        fprintf(stderr, "Stream position mismatch: expected %u, got %u\n",
                                decoder->stream_idx, pos);
                    }
                }
            } else if (oob_type == KF_OOB_EOF) {
                decoder->data_idx = decoder->data_size;
                break;
            }
            
            decoder->data_idx += oob_size;
            
        } else {
            /* 1-byte flux sample (default case) */
            flux_value += opcode;
            decoder->data_idx += 1;
            decoder->stream_idx += 1;
            done = true;
        }
    }
    
    if (!done) {
        return -1;  /* End of stream */
    }
    
    /* Convert to nanoseconds */
    /* Formula: (value * SCK_PS_PER_TICK) / 1000 */
    uint64_t flux_ps = (uint64_t)flux_value * SCK_PS_PER_TICK;
    *flux_ns_out = (uint32_t)(flux_ps / 1000);
    
    return 0;
}

/*============================================================================*
 * PUBLIC API IMPLEMENTATION
 *============================================================================*/

/**
 * @brief Decode KryoFlux stream file
 * 
 * Professional stream decoder with full error handling.
 * 
 * @param filename Stream file path (e.g., "track00.0.raw")
 * @param result_out Decoded stream (output, allocated by this function)
 * @return 0 on success, negative on error
 */
int kryoflux_decode_stream_file(
    const char *filename,
    kf_stream_result_t *result_out
) {
    if (!filename || !result_out) {
        return -1;
    }
    
    memset(result_out, 0, sizeof(*result_out));
    
    /* Read file */
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror("fopen");
        return -1;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0) {
        fclose(f);
        return -1;
    }
    
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(f);
        return -1;
    }
    
    if (fread(data, 1, size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return -1;
    }
    fclose(f);
    
    /* Decode index positions */
    size_t index_count;
    uint32_t *indices = decode_index_positions(data, size, &index_count);
    if (!indices) {
        free(data);
        return -1;
    }
    
    /* Initialize decoder */
    stream_decoder_t decoder = {
        .data = data,
        .data_size = size,
        .data_idx = 0,
        .stream_idx = 0,
        .index_positions = indices,
        .index_count = index_count,
        .index_idx = 0,
        .flux_ns = 0
    };
    
    /* Decode all flux transitions */
    #define MAX_TRANSITIONS 500000
    kf_flux_transition_t *transitions = malloc(MAX_TRANSITIONS * sizeof(*transitions));
    if (!transitions) {
        free(indices);
        free(data);
        return -1;
    }
    
    size_t trans_count = 0;
    while (trans_count < MAX_TRANSITIONS) {
        uint32_t flux_ns;
        bool is_index;
        
        if (decode_next_flux(&decoder, &flux_ns, &is_index) < 0) {
            break;  /* End of stream */
        }
        
        decoder.flux_ns += flux_ns;
        
        transitions[trans_count].timing_ns = flux_ns;
        transitions[trans_count].is_index = is_index;
        trans_count++;
    }
    
    /* Fill result */
    result_out->transitions = transitions;
    result_out->transition_count = trans_count;
    result_out->index_positions = indices;
    result_out->index_count = index_count;
    result_out->total_time_ns = decoder.flux_ns;
    
    /* Calculate RPM if we have at least 2 indices */
    if (index_count >= 2 && indices[0] < indices[1]) {
        /* Time per revolution in nanoseconds */
        uint64_t rev_time_ns = 0;
        for (size_t i = 0; i < trans_count; i++) {
            if (transitions[i].is_index) {
                break;
            }
            rev_time_ns += transitions[i].timing_ns;
        }
        
        if (rev_time_ns > 0) {
            /* RPM = 60 * 1e9 / rev_time_ns */
            result_out->rpm = (uint32_t)(60000000000ULL / rev_time_ns);
        }
    }
    
    free(data);
    
    return 0;
    
    #undef MAX_TRANSITIONS
}

/**
 * @brief Free stream result
 */
void kryoflux_free_stream_result(kf_stream_result_t *result)
{
    if (!result) return;
    
    free(result->transitions);
    free(result->index_positions);
    
    memset(result, 0, sizeof(*result));
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
