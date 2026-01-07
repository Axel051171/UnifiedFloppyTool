/**
 * @file uft_hfe_v3_parser.h
 * @brief UFT HFE v3 Parser API - UFT HFE Format Format with HDDD A2 Support
 * @version 3.3.7
 * @date 2026-01-03
 *
 * PUBLIC API:
 * - hfe_open()              - Open HFE file
 * - hfe_close()             - Close context
 * - hfe_read_track()        - Read track data
 * - hfe_free_track()        - Free track data
 * - hfe_get_info()          - Get disk info
 * - hfe_get_stats()         - Get statistics
 * - hfe_is_valid_file()     - Check if file is valid HFE
 * - hfe_get_file_version()  - Get HFE version
 * - hfe_decode_hddd_a2_track() - Decode Apple II HDDD A2 track
 *
 * SUPPORTED FORMATS:
 * - HFE v1 (HXCPICFE)
 * - HFE v3 (HXCHFEV3) with opcodes
 * - HDDD A2 variant (Apple II GCR)
 *
 * @author UFT Team
 * @copyright UFT Project 2025-2026
 */

#ifndef UFT_HFE_V3_PARSER_H
#define UFT_HFE_V3_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*============================================================================
 * CONSTANTS
 *============================================================================*/

/* Maximum values */
#define UFT_HFE_MAX_TRACKS      84
#define UFT_HFE_MAX_SIDES       2

/* HFE v3 Opcodes */
#define UFT_HFE_NOP_OPCODE      0xF0
#define UFT_HFE_SETINDEX_OPCODE 0xF1
#define UFT_HFE_SETBITRATE_OPCODE 0xF2
#define UFT_HFE_SKIPBITS_OPCODE 0xF3
#define UFT_HFE_RAND_OPCODE     0xF4

/* Track encoding types */
#define UFT_HFE_ENCODING_ISOIBM_MFM     0
#define UFT_HFE_ENCODING_AMIGA_MFM      1
#define UFT_HFE_ENCODING_ISOIBM_FM      2
#define UFT_HFE_ENCODING_EMU_FM         3
#define UFT_HFE_ENCODING_APPLE_GCR1     7
#define UFT_HFE_ENCODING_APPLE_GCR2     8
#define UFT_HFE_ENCODING_HDDD_A2_GCR1   0x87
#define UFT_HFE_ENCODING_HDDD_A2_GCR2   0x88

/* Interface modes */
#define UFT_HFE_IFMODE_IBMPC_DD         0
#define UFT_HFE_IFMODE_IBMPC_HD         1
#define UFT_HFE_IFMODE_ATARIST_DD       2
#define UFT_HFE_IFMODE_ATARIST_HD       3
#define UFT_HFE_IFMODE_AMIGA_DD         4
#define UFT_HFE_IFMODE_AMIGA_HD         5
#define UFT_HFE_IFMODE_CPC_DD           6
#define UFT_HFE_IFMODE_SHUGART_DD       7
#define UFT_HFE_IFMODE_IBMPC_ED         8
#define UFT_HFE_IFMODE_MSX2_DD          9
#define UFT_HFE_IFMODE_C64_DD           10
#define UFT_HFE_IFMODE_EMU_SHUGART      11

/*============================================================================
 * ERROR CODES
 *============================================================================*/

typedef enum {
    UFT_HFE_OK = 0,
    UFT_HFE_ERR_NULL_PARAM,
    UFT_HFE_ERR_FILE_OPEN,
    UFT_HFE_ERR_FILE_READ,
    UFT_HFE_ERR_FILE_WRITE,
    UFT_HFE_ERR_BAD_SIGNATURE,
    UFT_HFE_ERR_BAD_VERSION,
    UFT_HFE_ERR_TRUNCATED,
    UFT_HFE_ERR_BAD_TRACK,
    UFT_HFE_ERR_ALLOC,
    UFT_HFE_ERR_INVALID_DATA,
    UFT_HFE_ERR_NOT_SUPPORTED,
    UFT_HFE_ERR_COUNT
} uft_hfe_error_t;

/*============================================================================
 * TYPES
 *============================================================================*/

/* Opaque context */
typedef struct hfe_ctx hfe_ctx_t;

/* Decoded track side data */
typedef struct {
    uint8_t* data;                 /* Track data */
    uint32_t data_len;             /* Data length in bits */
    uint8_t* flakybitmap;          /* Weak/flaky bit map (NULL if none) */
    uint8_t* indexbitmap;          /* Index pulse bitmap (NULL if none) */
    uint32_t* timing;              /* Per-byte timing (bitrate) */
    uint32_t tracklen_bytes;       /* Length in bytes */
    int encoding;                  /* Track encoding type */
} uft_hfe_track_side_t;

/* Complete track with both sides */
typedef struct {
    int track_number;
    int number_of_sides;
    uft_hfe_track_side_t sides[UFT_HFE_MAX_SIDES];
    uint16_t rpm;                  /* Track RPM */
    bool valid;
} uft_hfe_track_t;

/*============================================================================
 * PUBLIC API
 *============================================================================*/

/**
 * @brief Open HFE file for reading
 * @param path Path to HFE file
 * @return Context pointer or NULL on error
 */
hfe_ctx_t* hfe_open(const char* path);

/**
 * @brief Close HFE context and free resources
 * @param ctx Pointer to context pointer (set to NULL on return)
 */
void hfe_close(hfe_ctx_t** ctx);

/**
 * @brief Read track from HFE file
 * @param ctx Context
 * @param track_num Track number (0-based)
 * @param track_out Output track pointer (caller must free with hfe_free_track)
 * @return UFT_HFE_OK on success, error code otherwise
 */
int hfe_read_track(hfe_ctx_t* ctx, int track_num, uft_hfe_track_t** track_out);

/**
 * @brief Free track data
 * @param track Pointer to track pointer (set to NULL on return)
 */
void hfe_free_track(uft_hfe_track_t** track);

/**
 * @brief Get disk info from HFE header
 * @param ctx Context
 * @param tracks Output: number of tracks (may be NULL)
 * @param sides Output: number of sides (may be NULL)
 * @param encoding Output: track encoding type (may be NULL)
 * @param interface_mode Output: floppy interface mode (may be NULL)
 * @param version Output: HFE version (1, 2, or 3) (may be NULL)
 * @param is_hddd_a2 Output: true if HDDD A2 variant (may be NULL)
 */
void hfe_get_info(
    hfe_ctx_t* ctx,
    int* tracks,
    int* sides,
    int* encoding,
    int* interface_mode,
    int* version,
    bool* is_hddd_a2
);

/**
 * @brief Get decoder statistics
 * @param ctx Context
 * @param tracks_read Output: number of tracks read (may be NULL)
 * @param weak_bits Output: weak bits detected (may be NULL)
 * @param index_marks Output: index marks found (may be NULL)
 */
void hfe_get_stats(
    hfe_ctx_t* ctx,
    uint32_t* tracks_read,
    uint32_t* weak_bits,
    uint32_t* index_marks
);

/**
 * @brief Check if file is a valid HFE file
 * @param path Path to file
 * @return true if valid HFE signature found
 */
bool hfe_is_valid_file(const char* path);

/**
 * @brief Get HFE version from file without full parsing
 * @param path Path to file
 * @return HFE version (1 or 3), or 0 if invalid/error
 */
int hfe_get_file_version(const char* path);

/**
 * @brief Get error string
 * @param err Error code
 * @return Human-readable error string
 */
const char* hfe_error_string(uft_hfe_error_t err);

/**
 * @brief Get encoding name
 * @param encoding Encoding code
 * @return Human-readable encoding name
 */
const char* hfe_encoding_name(int encoding);

/**
 * @brief Get interface mode name
 * @param mode Interface mode code
 * @return Human-readable interface mode name
 */
const char* hfe_interface_name(int mode);

/*============================================================================
 * HDDD A2 (APPLE II) SPECIFIC
 *============================================================================*/

/**
 * @brief Decode HDDD A2 GCR track to Apple II nibbles
 *
 * HDDD A2 stores Apple II GCR data with FM clock bits inserted.
 * This function extracts the original nibble data suitable for
 * Apple II emulators or disk utilities.
 *
 * @param side Track side data from hfe_read_track
 * @param nibbles_out Output buffer for nibbles (must be side->tracklen_bytes / 2 bytes)
 * @param nibbles_len Output: number of nibbles written
 * @return UFT_HFE_OK on success, error code otherwise
 */
int hfe_decode_hddd_a2_track(
    const uft_hfe_track_side_t* side,
    uint8_t* nibbles_out,
    uint32_t* nibbles_len
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_HFE_V3_PARSER_H */
