/**
 * @file uft_dart.h
 * @brief DART (Disk Archive/Retrieval Tool) format parser
 *
 * Apple Macintosh compressed disk archive format. DART was a popular
 * utility for archiving Mac floppy disks, supporting RLE compression.
 *
 * Header structure (84 bytes):
 *   - Bytes  0-63:  Source disk name (Pascal string)
 *   - Bytes 64-67:  Disk size in 512-byte blocks (big-endian)
 *   - Bytes 68-71:  Checksum (big-endian)
 *   - Byte  72:     Data type (0=uncompressed, 1=RLE)
 *   - Byte  73:     Disk type (0=400K, 1=800K, 2=720K, 3=1440K)
 *   - Bytes 74-83:  Reserved
 *
 * @author UFT Project
 * @date 2026-04-10
 */

#ifndef UFT_DART_H
#define UFT_DART_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** DART header size */
#define DART_HEADER_SIZE        84

/** Sector size */
#define DART_SECTOR_SIZE        512

/** Maximum volume name length (Pascal string) */
#define DART_MAX_NAME_LEN       63

/** RLE escape byte */
#define DART_RLE_ESCAPE         0x90

/* ============================================================================
 * Error Codes
 * ============================================================================ */

typedef enum {
    UFT_DART_OK              =  0,
    UFT_DART_ERR_NULL        = -1,
    UFT_DART_ERR_OPEN        = -2,
    UFT_DART_ERR_READ        = -3,
    UFT_DART_ERR_TRUNCATED   = -4,
    UFT_DART_ERR_INVALID     = -5,
    UFT_DART_ERR_MEMORY      = -6,
    UFT_DART_ERR_DECOMPRESS  = -7,
    UFT_DART_ERR_SIZE        = -8,
    UFT_DART_ERR_CHECKSUM    = -9,
} uft_dart_error_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/** DART compression type */
typedef enum {
    DART_DATA_UNCOMPRESSED  = 0,    /**< Raw sector data */
    DART_DATA_RLE           = 1,    /**< RLE compressed */
} dart_data_type_t;

/** DART disk type */
typedef enum {
    DART_DISK_400K          = 0,    /**< Mac 400K GCR */
    DART_DISK_800K          = 1,    /**< Mac 800K GCR */
    DART_DISK_720K          = 2,    /**< PC 720K MFM */
    DART_DISK_1440K         = 3,    /**< Mac/PC 1.44MB MFM HD */
} dart_disk_type_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

#pragma pack(push, 1)

/** DART file header (84 bytes, big-endian integers) */
typedef struct {
    uint8_t     disk_name[64];      /**< Pascal string: length byte + name */
    uint32_t    block_count;        /**< Disk size in 512-byte blocks (BE) */
    uint32_t    checksum;           /**< Data checksum (BE) */
    uint8_t     data_type;          /**< 0 = uncompressed, 1 = RLE */
    uint8_t     disk_type;          /**< dart_disk_type_t */
    uint8_t     reserved[10];       /**< Reserved, should be 0 */
} dart_header_t;

#pragma pack(pop)

/** DART parsed context */
typedef struct {
    /* Header fields */
    char                volume_name[DART_MAX_NAME_LEN + 1];
    dart_data_type_t    data_type;
    dart_disk_type_t    disk_type;
    bool                is_valid;

    /* Disk geometry */
    uint32_t            block_count;    /**< Number of 512-byte blocks */
    uint32_t            data_size;      /**< Total uncompressed data size */
    uint32_t            sector_count;
    uint32_t            sector_size;

    /* Checksum */
    uint32_t            stored_checksum;
    uint32_t            calculated_checksum;
    bool                checksum_valid;

    /* Offsets */
    uint32_t            data_offset;    /**< Offset to compressed/raw data */

    /* Format details */
    char                description[64];
} uft_dart_context_t;

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Detect if data is a DART image
 * @param data      File data
 * @param size      Size of file data
 * @param filename  Filename for extension check, can be NULL
 * @return true if this looks like a DART image
 */
bool uft_dart_detect(const uint8_t *data, size_t size, const char *filename);

/**
 * @brief Parse DART image header
 * @param data  File data (complete file in memory)
 * @param size  Size of data
 * @param ctx   Output: parsed context (caller-allocated)
 * @return UFT_DART_OK on success, negative error code on failure
 */
uft_dart_error_t uft_dart_parse(const uint8_t *data, size_t size,
                                  uft_dart_context_t *ctx);

/**
 * @brief Extract raw disk data from DART image
 *
 * Handles both uncompressed and RLE-compressed data.
 *
 * @param data          Source file data
 * @param size          Size of source data
 * @param ctx           Previously parsed context
 * @param output        Output buffer for raw sector data
 * @param output_size   Size of output buffer
 * @return Number of bytes written, or negative error code
 */
int uft_dart_extract(const uint8_t *data, size_t size,
                      const uft_dart_context_t *ctx,
                      uint8_t *output, size_t output_size);

/**
 * @brief Decompress DART RLE data
 * @param input         Compressed data
 * @param input_size    Size of compressed data
 * @param output        Output buffer
 * @param output_size   Size of output buffer
 * @return Number of decompressed bytes, or negative on error
 */
int dart_rle_decompress(const uint8_t *input, size_t input_size,
                         uint8_t *output, size_t output_size);

/**
 * @brief Calculate DART checksum over data
 * @param data  Data to checksum
 * @param size  Size of data
 * @return Calculated checksum
 */
uint32_t dart_calculate_checksum(const uint8_t *data, size_t size);

/**
 * @brief Get disk type description string
 * @param disk_type DART disk type
 * @return Static description string
 */
const char *uft_dart_disk_type_string(dart_disk_type_t disk_type);

/**
 * @brief Get error string for DART error code
 * @param err Error code
 * @return Static error description
 */
const char *uft_dart_strerror(uft_dart_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DART_H */
