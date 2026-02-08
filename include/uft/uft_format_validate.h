/**
 * @file uft_format_validate.h
 * @brief Format Validation Framework
 * 
 * VALIDIERUNGSSTUFEN:
 * 1. Strukturelle Integrität (Header, Offsets)
 * 2. Checksummen (CRC, einfache Summen)
 * 3. Logische Konsistenz (BAM, Directory, Sector Maps)
 * 4. Plausibilität (Track-Zonen, Dichte, Timing)
 */

#ifndef UFT_FORMAT_VALIDATE_H
#define UFT_FORMAT_VALIDATE_H

#include "uft_types.h"
#include "uft_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Forward declaration - actual definition in uft_format_registry.h */
typedef uint32_t uft_format_id_t;

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Validation Levels
// ============================================================================

typedef enum uft_validation_level {
    UFT_VALIDATE_QUICK,     // Basic structure only
    UFT_VALIDATE_STANDARD,  // + Checksums
    UFT_VALIDATE_THOROUGH,  // + Logical consistency
    UFT_VALIDATE_FORENSIC,  // + All plausibility checks
} uft_validation_level_t;

// ============================================================================
// Validation Result
// ============================================================================

typedef struct uft_validation_issue {
    int             severity;       // 0=info, 1=warning, 2=error, 3=critical
    int             offset;         // -1 if N/A
    int             track;          // -1 if N/A
    int             sector;         // -1 if N/A
    const char*     category;       // "structure", "checksum", "logic", etc.
    char            message[256];
} uft_validation_issue_t;

typedef struct uft_validation_result {
    bool            valid;          // Overall validity
    int             score;          // 0-100
    
    int             issue_count;
    uft_validation_issue_t issues[64];
    
    // Statistics
    int             total_sectors;
    int             bad_sectors;
    int             empty_sectors;
    int             checksum_errors;
    
    // Format-specific
    union {
        struct {  // D64
            bool    bam_valid;
            int     used_blocks;
            int     free_blocks;
            int     directory_entries;
        } d64;
        
        struct {  // ADF
            bool    bootblock_valid;
            bool    rootblock_valid;
            int     used_blocks;
            int     free_blocks;
        } adf;
        
        struct {  // IMG/FAT
            bool    bpb_valid;
            bool    fat_consistent;
            int     clusters_used;
        } fat;
        
        struct {  // SCP
            int     revolutions;
            int     tracks;
            double  avg_track_length;
        } scp;
    };
} uft_validation_result_t;

// ============================================================================
// Validation API
// ============================================================================

uft_error_t uft_validate_format(const uint8_t* data, size_t size,
                                 uft_format_t format,
                                 uft_validation_level_t level,
                                 uft_validation_result_t* result);

// Format-specific validators
uft_error_t uft_validate_d64(const uint8_t* data, size_t size,
                              uft_validation_level_t level,
                              uft_validation_result_t* result);

uft_error_t uft_validate_adf(const uint8_t* data, size_t size,
                              uft_validation_level_t level,
                              uft_validation_result_t* result);

uft_error_t uft_validate_scp(const uint8_t* data, size_t size,
                              uft_validation_level_t level,
                              uft_validation_result_t* result);

uft_error_t uft_validate_g64(const uint8_t* data, size_t size,
                              uft_validation_level_t level,
                              uft_validation_result_t* result);

// ============================================================================
// Checksum Functions
// ============================================================================

uint32_t uft_crc32(const uint8_t* data, size_t size);
uint16_t uft_crc16_ccitt(const uint8_t* data, size_t size);
uint8_t uft_checksum_xor(const uint8_t* data, size_t size);
uint32_t uft_checksum_amiga(const uint8_t* data, size_t size);
uint8_t uft_checksum_gcr(const uint8_t* data, size_t size);

#ifdef __cplusplus
}
#endif

#endif // UFT_FORMAT_VALIDATE_H

// ============================================================================
// Format-Encoding Compatibility Validation (P1-GUI-004)
// ============================================================================

/**
 * @brief Format-Encoding compatibility entry
 */
typedef struct uft_format_encoding_rule {
    uft_format_id_t     format;
    uft_encoding_t      valid_encodings[8];     /**< Null-terminated list */
    const char*         format_name;
} uft_format_encoding_rule_t;

/**
 * @brief Validate format-encoding combination
 * @param format Format ID
 * @param encoding Encoding type
 * @param error_msg Output buffer for error message (can be NULL)
 * @param msg_size Size of error_msg buffer
 * @return true if valid combination, false otherwise
 */
bool uft_validate_format_encoding(
    uft_format_id_t format,
    uft_encoding_t encoding,
    char* error_msg,
    size_t msg_size
);

/**
 * @brief Get valid encodings for a format
 * @param format Format ID
 * @param encodings Output array (must hold at least 8 entries)
 * @return Number of valid encodings
 */
int uft_get_valid_encodings(uft_format_id_t format, uft_encoding_t* encodings);

/**
 * @brief Get default encoding for a format
 * @param format Format ID
 * @return Default encoding or UFT_ENC_UNKNOWN
 */
uft_encoding_t uft_get_default_encoding(uft_format_id_t format);

