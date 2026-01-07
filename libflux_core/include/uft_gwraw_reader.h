/**
 * @file uft_gwraw_reader.h
 * 
 * 
 * Format Specification:
 * - Stream of opcodes with flux timing data
 * - Opcodes: FLUX1, FLUX2, FLUX3, INDEX, etc.
 * - Variable-length encoding
 * 
 * 
 * @version 2.11.0
 * @date 2024-12-27
 */

#ifndef UFT_GWRAW_READER_H
#define UFT_GWRAW_READER_H

#include "uft/uft_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief GWRAW opcodes
 */
typedef enum {
    GWRAW_OP_INDEX = 0x01,      /**< Index pulse */
    GWRAW_OP_FLUX1 = 0x02,      /**< 1-byte flux */
    GWRAW_OP_FLUX2 = 0x03,      /**< 2-byte flux */
    GWRAW_OP_FLUX3 = 0x04,      /**< 3-byte flux */
    GWRAW_OP_SPACE = 0x05,      /**< Extended space */
    GWRAW_OP_ASTABLE = 0x06,    /**< Astable timing */
    GWRAW_OP_EOF = 0xFF         /**< End of stream */
} gwraw_opcode_t;

/**
 */
typedef struct {
    FILE* fp;
    
    /* Current track position */
    long track_start_pos;
    uint32_t current_track;
    uint32_t current_head;
    
    /* Statistics */
    uint32_t tracks_read;
    uint32_t total_flux_transitions;
    uint32_t index_pulses_found;
    
    /* Timing */
    uint32_t sample_freq;       /**< Sampling frequency (Hz), default 72MHz */
    
    /* Error context */
    uft_error_ctx_t error;
    
} uft_gwraw_ctx_t;

/**
 * 
 * @param[in] path Path to .raw file
 * @param[out] ctx Pointer to receive context
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_gwraw_open(const char* path, uft_gwraw_ctx_t** ctx);

/**
 * @brief Close GWRAW file
 * 
 * @param[in,out] ctx Context to close
 */
void uft_gwraw_close(uft_gwraw_ctx_t** ctx);

/**
 * @brief Read track flux data
 * 
 * Reads flux transition data for current track position.
 * File is sequential - must read tracks in order or seek manually.
 * 
 * @param[in] ctx GWRAW context
 * @param[out] flux_ns Buffer to receive flux times (malloc'd, caller must free)
 * @param[out] count Number of flux transitions
 * @param[out] has_index Set to true if index pulse found
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_gwraw_read_track(
    uft_gwraw_ctx_t* ctx,
    uint32_t** flux_ns,
    uint32_t* count,
    bool* has_index
);

/**
 * @brief Seek to track start
 * 
 * For sequential files, this rewinds to beginning.
 * Use track header markers to skip to specific track.
 * 
 * @param[in] ctx GWRAW context
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_gwraw_rewind(uft_gwraw_ctx_t* ctx);

/**
 * @brief Set sampling frequency
 * 
 * Adjust if using different capture hardware.
 * 
 * @param[in] ctx GWRAW context
 * @param freq_hz Sampling frequency in Hz
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_gwraw_set_freq(uft_gwraw_ctx_t* ctx, uint32_t freq_hz);

#ifdef __cplusplus
}
#endif

#endif /* UFT_GWRAW_READER_H */
