/**
 * @file uft_uft_kf_parser.h
 * @version 1.0.0
 * @date 2026-01-06
 *
 * a variable-length encoding for flux transitions.
 *
 * File naming: trackXX.Y.raw where XX=track (00-83), Y=side (0-1)
 *
 * Stream consists of flux blocks with opcodes:
 * - 0x00-0x07: Flux2 (2-byte flux value)
 * - 0x08: Nop1 (skip 1 byte)
 * - 0x09: Nop2 (skip 2 bytes)
 * - 0x0A: Nop3 (skip 3 bytes)
 * - 0x0B: Ovl16 (overflow, add 0x10000 to next value)
 * - 0x0C: Flux3 (3-byte flux value)
 * - 0x0D: OOB (Out-of-band block follows)
 */

#ifndef UFT_UFT_UFT_KF_PARSER_H
#define UFT_UFT_UFT_KF_PARSER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

#define UFT_UFT_KF_MAX_TRACKS       168
#define UFT_UFT_KF_MAX_REVOLUTIONS  10
#define UFT_UFT_KF_SAMPLE_CLOCK     (18432000.0 * 73.0 / 14.0 / 2.0)  /* ~48.054MHz */
#define UFT_UFT_KF_INDEX_CLOCK      (18432000.0 / 16.0)               /* 1.152MHz */

/*============================================================================
 * Stream Opcodes
 *============================================================================*/

/* Flux opcodes (0x00-0x07 = Flux2 with high bits) */
#define UFT_UFT_KF_OP_FLUX2_BASE    0x00  /* 0x00-0x07 */
#define UFT_UFT_KF_OP_NOP1          0x08
#define UFT_UFT_KF_OP_NOP2          0x09
#define UFT_UFT_KF_OP_NOP3          0x0A
#define UFT_UFT_KF_OP_OVL16         0x0B  /* Overflow +65536 */
#define UFT_UFT_KF_OP_FLUX3         0x0C  /* 3-byte flux */
#define UFT_UFT_KF_OP_OOB           0x0D  /* Out-of-band */

/*============================================================================
 * OOB Block Types
 *============================================================================*/

#define UFT_UFT_KF_OOB_INVALID      0x00
#define UFT_UFT_KF_OOB_STREAMINFO   0x01
#define UFT_UFT_KF_OOB_INDEX        0x02
#define UFT_UFT_KF_OOB_STREAMEND    0x03
#define UFT_UFT_KF_OOB_KFINFO       0x04
#define UFT_UFT_KF_OOB_EOF          0x0D

/*============================================================================
 * Structures
 *============================================================================*/

#pragma pack(push, 1)

/**
 * @brief OOB block header
 */
typedef struct {
    uint8_t  type;
    uint16_t size;
} uft_kf_oob_header_t;

/**
 * @brief Stream info block
 */
typedef struct {
    uint32_t stream_pos;    /**< Position in stream */
    uint32_t xfer_time;     /**< Transfer time */
} uft_kf_stream_info_t;

/**
 * @brief Index block
 */
typedef struct {
    uint32_t stream_pos;    /**< Stream position at index */
    uint32_t sample_counter;/**< Sample counter at index */
    uint32_t index_counter; /**< Index timer value */
} uft_kf_index_t;

/**
 * @brief Stream end block
 */
typedef struct {
    uint32_t stream_pos;    /**< Final stream position */
    uint32_t result_code;   /**< Hardware result */
} uft_kf_stream_end_t;

#pragma pack(pop)

/**
 * @brief Revolution info
 */
typedef struct {
    uint32_t start_pos;         /**< Start position in stream */
    uint32_t end_pos;           /**< End position in stream */
    uint32_t sample_counter;    /**< Sample counter at index */
    uint32_t index_counter;     /**< Index counter value */
    double   index_time_us;     /**< Index time in microseconds */
    uint32_t flux_count;        /**< Number of flux transitions */
    uint32_t* flux_data;        /**< Flux data in sample ticks */
} uft_kf_revolution_t;

/**
 * @brief Track data
 */
typedef struct {
    uint8_t  track_number;
    uint8_t  side;
    int      revolution_count;
    bool     valid;
    char     filename[256];
    uft_kf_revolution_t revolutions[UFT_UFT_KF_MAX_REVOLUTIONS];
} uft_kf_track_data_t;

/**
 * @brief Parser context
 */
typedef struct {
    /* Stream data */
    uint8_t* stream_data;
    size_t   stream_size;
    size_t   stream_pos;
    
    /* Parsed indices */
    uft_kf_index_t indices[UFT_UFT_KF_MAX_REVOLUTIONS + 1];
    int      index_count;
    
    /* Stream info */
    bool     has_stream_info;
    uint32_t stream_info_pos;
    uint32_t xfer_time;
    
    /* Status */
    int      last_error;
} uft_kf_ctx_t;

/*============================================================================
 * Error Codes
 *============================================================================*/

#define UFT_UFT_KF_OK               0
#define UFT_UFT_KF_ERR_NULLPTR      -1
#define UFT_UFT_KF_ERR_OPEN         -2
#define UFT_UFT_KF_ERR_READ         -3
#define UFT_UFT_KF_ERR_FORMAT       -4
#define UFT_UFT_KF_ERR_MEMORY       -5
#define UFT_UFT_KF_ERR_OVERFLOW     -6
#define UFT_UFT_KF_ERR_NO_INDEX     -7

/*============================================================================
 * API Functions
 *============================================================================*/

/**
 * @return Context or NULL
 */
uft_kf_ctx_t* uft_kf_create(void);

/**
 * @brief Destroy parser context
 * @param ctx Context
 */
void uft_kf_destroy(uft_kf_ctx_t* ctx);

/**
 * @brief Load stream file
 * @param ctx Context
 * @param filename Path to .raw file
 * @return UFT_UFT_KF_OK on success
 */
int uft_kf_load_file(uft_kf_ctx_t* ctx, const char* filename);

/**
 * @brief Load stream from memory
 * @param ctx Context
 * @param data Stream data
 * @param size Data size
 * @return UFT_UFT_KF_OK on success
 */
int uft_kf_load_memory(uft_kf_ctx_t* ctx, const uint8_t* data, size_t size);

/**
 * @brief Parse stream to extract revolutions
 * @param ctx Context
 * @param track Output track data
 * @return UFT_UFT_KF_OK on success
 */
int uft_kf_parse_stream(uft_kf_ctx_t* ctx, uft_kf_track_data_t* track);

/**
 * @brief Free track data
 * @param track Track to free
 */
void uft_kf_free_track(uft_kf_track_data_t* track);

/**
 * @brief Get index count
 * @param ctx Context
 * @return Number of indices found
 */
int uft_kf_get_index_count(uft_kf_ctx_t* ctx);

/**
 * @brief Parse track/side from filename
 * @param filename Filename (e.g., "track00.0.raw")
 * @param track Output track number
 * @param side Output side number
 * @return true if parsed successfully
 */
bool uft_kf_parse_filename(const char* filename, int* track, int* side);

/**
 * @brief Convert sample ticks to nanoseconds
 * @param ticks Sample clock ticks
 * @return Time in nanoseconds
 */
uint32_t uft_kf_ticks_to_ns(uint32_t ticks);

/**
 * @brief Convert index ticks to microseconds
 * @param ticks Index clock ticks
 * @return Time in microseconds
 */
double uft_kf_index_to_us(uint32_t ticks);

/**
 * @brief Calculate RPM from index time
 * @param index_time_us Index time in microseconds
 * @return RPM value
 */
uint32_t uft_uft_kf_calculate_rpm(double index_time_us);

#ifdef __cplusplus
}
#endif

#endif /* UFT_UFT_UFT_KF_PARSER_H */
