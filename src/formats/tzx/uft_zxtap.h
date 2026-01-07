/**
 * @file uft_zxtap.h
 * @brief ZX Spectrum TAP Format Support
 * 
 * TAP is the simplest tape format for ZX Spectrum:
 * - Just data blocks with 2-byte length prefix
 * - No timing information (unlike TZX)
 * - Easy to convert to/from TZX Block 0x10
 * 
 * TAP Block Structure:
 *   Offset  Size  Description
 *   0x00    2     Block length (N)
 *   0x02    1     Flag byte (0x00=header, 0xFF=data)
 *   0x03    N-2   Data bytes
 *   0x03+N-2  1   Checksum (XOR of flag + data)
 * 
 * @author UFT Team
 * @version 1.0.0
 */

#ifndef UFT_ZXTAP_H
#define UFT_ZXTAP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Block flag bytes */
#define ZXTAP_FLAG_HEADER   0x00
#define ZXTAP_FLAG_DATA     0xFF

/** Header types (first byte after flag in header block) */
#define ZXTAP_TYPE_PROGRAM  0x00    /**< BASIC program */
#define ZXTAP_TYPE_NUMARRAY 0x01    /**< Number array */
#define ZXTAP_TYPE_CHARARRAY 0x02   /**< Character array */
#define ZXTAP_TYPE_CODE     0x03    /**< Bytes/CODE */

/** Header block is always 19 bytes (17 + flag + checksum) */
#define ZXTAP_HEADER_SIZE   17

/* ═══════════════════════════════════════════════════════════════════════════
 * Types
 * ═══════════════════════════════════════════════════════════════════════════ */

/** TAP block */
typedef struct {
    uint16_t    length;         /**< Block length (including flag and checksum) */
    uint8_t     flag;           /**< Flag byte */
    uint8_t*    data;           /**< Block data (excluding flag, including checksum) */
    uint8_t     checksum;       /**< Calculated checksum */
    bool        checksum_ok;    /**< Checksum valid? */
} zxtap_block_t;

/** Parsed header block */
typedef struct {
    uint8_t     type;           /**< 0=Program, 1=NumArray, 2=CharArray, 3=Code */
    char        name[11];       /**< Filename (10 chars + null) */
    uint16_t    length;         /**< Data length */
    uint16_t    param1;         /**< Type-specific: autostart/varname/start */
    uint16_t    param2;         /**< Type-specific: length/unused */
} zxtap_header_t;

/** TAP file */
typedef struct {
    zxtap_block_t*  blocks;     /**< Array of blocks */
    size_t          block_count;
    size_t          capacity;
} zxtap_file_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * TAP File Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Create empty TAP file
 */
zxtap_file_t* zxtap_file_create(void);

/**
 * Free TAP file
 */
void zxtap_file_free(zxtap_file_t* tap);

/**
 * Read TAP file from disk
 */
zxtap_file_t* zxtap_file_read(const char* filename);

/**
 * Read TAP from memory
 */
zxtap_file_t* zxtap_file_parse(const uint8_t* data, size_t size);

/**
 * Write TAP file to disk
 */
bool zxtap_file_write(const zxtap_file_t* tap, const char* filename);

/**
 * Add block to TAP file
 * @param flag Flag byte (ZXTAP_FLAG_HEADER or ZXTAP_FLAG_DATA)
 * @param data Block data (without flag, will copy)
 * @param length Data length
 */
bool zxtap_file_add_block(zxtap_file_t* tap, uint8_t flag,
                          const uint8_t* data, size_t length);

/* ═══════════════════════════════════════════════════════════════════════════
 * Header Parsing
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Parse header block into structured form
 * @param block Block to parse (must be flag=0x00 header)
 * @param header Output header structure
 * @return true if valid header
 */
bool zxtap_parse_header(const zxtap_block_t* block, zxtap_header_t* header);

/**
 * Get human-readable type name
 */
const char* zxtap_type_name(uint8_t type);

/* ═══════════════════════════════════════════════════════════════════════════
 * Checksum Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Calculate XOR checksum for block
 */
uint8_t zxtap_checksum(uint8_t flag, const uint8_t* data, size_t length);

/**
 * Verify block checksum
 */
bool zxtap_verify_checksum(const zxtap_block_t* block);

/* ═══════════════════════════════════════════════════════════════════════════
 * TZX Conversion Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Convert TZX file to TAP
 * Only extracts standard speed blocks (0x10)
 * @return New TAP file or NULL on error
 */
zxtap_file_t* zxtap_from_tzx(const uint8_t* tzx_data, size_t tzx_size);

/**
 * Convert TAP file to TZX
 * Creates TZX with standard speed blocks (0x10)
 * @param tap TAP file to convert
 * @param out_size Output: size of TZX data
 * @return TZX data (caller must free) or NULL on error
 */
uint8_t* zxtap_to_tzx(const zxtap_file_t* tap, size_t* out_size);

/**
 * Convert TZX file to TAP file (disk to disk)
 */
bool zxtap_tzx_to_tap_file(const char* tzx_filename, const char* tap_filename);

/**
 * Convert TAP file to TZX file (disk to disk)
 */
bool zxtap_tap_to_tzx_file(const char* tap_filename, const char* tzx_filename);

/* ═══════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Print TAP file info
 */
void zxtap_print_info(const zxtap_file_t* tap, FILE* out);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ZXTAP_H */
