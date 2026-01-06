/**
 * @file uft_sbx.h
 * @brief SeqBox (SBX) and ECSBX Format Structures
 * 
 * Based on blockyarchive
 * SBX provides recoverable block-based archiving with Reed-Solomon FEC
 */

#ifndef UFT_SBX_H
#define UFT_SBX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** SBX signature: "SBx" */
#define UFT_SBX_SIGNATURE           "SBx"
#define UFT_SBX_SIGNATURE_LEN       3

/** Block header size (common for all versions) */
#define UFT_SBX_HEADER_SIZE         16

/** SBX versions (block sizes) */
#define UFT_SBX_VERSION_1           1       /**< 512 bytes */
#define UFT_SBX_VERSION_2           2       /**< 128 bytes */
#define UFT_SBX_VERSION_3           3       /**< 4096 bytes */

/** ECSBX versions (error-correcting) */
#define UFT_ECSBX_VERSION_17        17      /**< 512 bytes + RS */
#define UFT_ECSBX_VERSION_18        18      /**< 128 bytes + RS */
#define UFT_ECSBX_VERSION_19        19      /**< 4096 bytes + RS */

/** Block sizes by version */
#define UFT_SBX_BLOCK_SIZE_128      128
#define UFT_SBX_BLOCK_SIZE_512      512
#define UFT_SBX_BLOCK_SIZE_4096     4096

/** UID size */
#define UFT_SBX_UID_SIZE            6

/** Metadata padding byte */
#define UFT_SBX_PADDING_BYTE        0x1A

/** CRC-16-CCITT polynomial */
#define UFT_SBX_CRC16_POLY          0x1021
#define UFT_SBX_CRC16_INIT          0x0000

/*===========================================================================
 * Metadata IDs
 *===========================================================================*/

/** 3-character metadata field IDs */
#define UFT_SBX_META_FNM            "FNM"   /**< Filename (UTF-8) */
#define UFT_SBX_META_SNM            "SNM"   /**< SBX filename (UTF-8) */
#define UFT_SBX_META_FSZ            "FSZ"   /**< File size (8 bytes BE) */
#define UFT_SBX_META_FDT            "FDT"   /**< File date (8 bytes BE, Unix epoch) */
#define UFT_SBX_META_SDT            "SDT"   /**< SBX date (8 bytes BE) */
#define UFT_SBX_META_HSH            "HSH"   /**< Hash (Multihash format) */
#define UFT_SBX_META_PID            "PID"   /**< Parent UID */
#define UFT_SBX_META_RSD            "RSD"   /**< RS data shards (1 byte) */
#define UFT_SBX_META_RSP            "RSP"   /**< RS parity shards (1 byte) */

/*===========================================================================
 * Multihash Types (for HSH field)
 *===========================================================================*/

#define UFT_MULTIHASH_SHA1          0x11
#define UFT_MULTIHASH_SHA256        0x12
#define UFT_MULTIHASH_SHA512        0x13
#define UFT_MULTIHASH_BLAKE2B_512   0xB240

/*===========================================================================
 * Structures
 *===========================================================================*/

/**
 * @brief SBX block header (16 bytes, big-endian)
 * 
 * Layout:
 *   [0-2]   Signature "SBx"
 *   [3]     Version byte
 *   [4-5]   CRC-16-CCITT (of bytes 6-15, with version as initial value)
 *   [6-11]  File UID (6 bytes)
 *   [12-15] Block sequence number (big-endian)
 */
typedef struct __attribute__((packed)) {
    uint8_t signature[3];           /**< "SBx" */
    uint8_t version;                /**< Version (1, 2, 3, 17, 18, 19) */
    uint16_t crc16;                 /**< CRC-16-CCITT (big-endian) */
    uint8_t uid[UFT_SBX_UID_SIZE];  /**< File UID */
    uint32_t seq_num;               /**< Sequence number (big-endian) */
} uft_sbx_header_t;

/**
 * @brief Metadata entry (variable length)
 */
typedef struct {
    char id[4];                     /**< 3-char ID + null */
    uint8_t length;                 /**< Data length */
    uint8_t *data;                  /**< Data bytes */
} uft_sbx_meta_entry_t;

/**
 * @brief SBX file context
 */
typedef struct {
    uint8_t version;
    size_t block_size;
    uint8_t uid[UFT_SBX_UID_SIZE];
    
    /* Metadata */
    char *filename;
    uint64_t file_size;
    int64_t file_date;
    uint8_t *hash;
    size_t hash_len;
    uint16_t hash_type;
    
    /* ECSBX only */
    bool is_ecsbx;
    uint8_t data_shards;
    uint8_t parity_shards;
    uint32_t burst_resistance;
} uft_sbx_ctx_t;

/*===========================================================================
 * CRC-16-CCITT
 *===========================================================================*/

/**
 * @brief CRC-16-CCITT lookup table
 */
extern const uint16_t uft_sbx_crc16_table[256];

/**
 * @brief Compute CRC-16-CCITT
 * @param init Initial value (version byte for SBX)
 * @param data Data to checksum
 * @param len Data length
 * @return CRC-16 value
 */
static inline uint16_t uft_sbx_crc16(uint16_t init, const uint8_t *data, size_t len)
{
    uint16_t crc = init;
    for (size_t i = 0; i < len; i++) {
        crc = (crc << 8) ^ uft_sbx_crc16_table[((crc >> 8) ^ data[i]) & 0xFF];
    }
    return crc;
}

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

/**
 * @brief Get block size for version
 */
static inline size_t uft_sbx_version_to_blocksize(uint8_t version)
{
    switch (version) {
        case UFT_SBX_VERSION_2:
        case UFT_ECSBX_VERSION_18:
            return UFT_SBX_BLOCK_SIZE_128;
        case UFT_SBX_VERSION_1:
        case UFT_ECSBX_VERSION_17:
            return UFT_SBX_BLOCK_SIZE_512;
        case UFT_SBX_VERSION_3:
        case UFT_ECSBX_VERSION_19:
            return UFT_SBX_BLOCK_SIZE_4096;
        default:
            return 0;
    }
}

/**
 * @brief Check if version uses Reed-Solomon
 */
static inline bool uft_sbx_version_uses_rs(uint8_t version)
{
    return version == UFT_ECSBX_VERSION_17 ||
           version == UFT_ECSBX_VERSION_18 ||
           version == UFT_ECSBX_VERSION_19;
}

/**
 * @brief Read big-endian uint16
 */
static inline uint16_t uft_sbx_read_be16(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

/**
 * @brief Read big-endian uint32
 */
static inline uint32_t uft_sbx_read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

/**
 * @brief Read big-endian uint64
 */
static inline uint64_t uft_sbx_read_be64(const uint8_t *p)
{
    return ((uint64_t)uft_sbx_read_be32(p) << 32) | uft_sbx_read_be32(p + 4);
}

/**
 * @brief Write big-endian uint16
 */
static inline void uft_sbx_write_be16(uint8_t *p, uint16_t val)
{
    p[0] = (val >> 8) & 0xFF;
    p[1] = val & 0xFF;
}

/**
 * @brief Write big-endian uint32
 */
static inline void uft_sbx_write_be32(uint8_t *p, uint32_t val)
{
    p[0] = (val >> 24) & 0xFF;
    p[1] = (val >> 16) & 0xFF;
    p[2] = (val >> 8) & 0xFF;
    p[3] = val & 0xFF;
}

/*===========================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Check if buffer contains SBX signature
 */
static inline bool uft_sbx_check_signature(const uint8_t *data)
{
    return data[0] == 'S' && data[1] == 'B' && data[2] == 'x';
}

/**
 * @brief Parse SBX block header
 * @param hdr Output header structure
 * @param data Raw block data (at least 16 bytes)
 * @return 0 on success, -1 on invalid signature
 */
int uft_sbx_parse_header(uft_sbx_header_t *hdr, const uint8_t *data);

/**
 * @brief Verify block CRC
 * @param data Raw block data
 * @param block_size Block size
 * @return true if CRC matches
 */
bool uft_sbx_verify_crc(const uint8_t *data, size_t block_size);

/**
 * @brief Parse metadata block (block 0)
 * @param ctx Output context
 * @param data Block data (after header)
 * @param len Data length
 * @return 0 on success
 */
int uft_sbx_parse_metadata(uft_sbx_ctx_t *ctx, const uint8_t *data, size_t len);

/**
 * @brief Create block header
 * @param buf Output buffer (at least 16 bytes)
 * @param version SBX version
 * @param uid File UID
 * @param seq_num Sequence number
 */
void uft_sbx_create_header(uint8_t *buf, uint8_t version,
                           const uint8_t uid[UFT_SBX_UID_SIZE],
                           uint32_t seq_num);

/**
 * @brief Update block CRC
 * @param data Block data (modifies CRC field)
 * @param block_size Block size
 */
void uft_sbx_update_crc(uint8_t *data, size_t block_size);

/**
 * @brief Scan for SBX blocks in data
 * @param data Data buffer
 * @param len Data length
 * @param found_offsets Output array of found block offsets
 * @param max_found Maximum entries in found_offsets
 * @return Number of blocks found
 */
size_t uft_sbx_scan_blocks(const uint8_t *data, size_t len,
                           size_t *found_offsets, size_t max_found);

/**
 * @brief Free context resources
 */
void uft_sbx_ctx_free(uft_sbx_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SBX_H */
