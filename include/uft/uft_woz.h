/**
 * @file uft_woz.h
 * @brief UnifiedFloppyTool - WOZ Format and Apple Nibble Encoding
 * @version 3.1.4.006
 *
 * Complete WOZ 1.0/2.0 format support for Apple II 5.25" and 3.5" disks.
 * Includes 4-and-4, 5-and-3, and 6-and-2 nibble encoding/decoding.
 *
 * Sources analyzed:
 * - dsk2woz2.c (Ben Zotto): DSK to WOZ2 conversion
 * - DskToWoz2 (Christopher Mosher): nibblize_6_2.c, nibblize_5_3.c
 * - DiskBrowser (ByteZone): WozFile.java, ByteTranslator6and2.java
 * - Applesauce FDC WOZ Reference 2.0
 *
 * Key features:
 * - WOZ1/WOZ2 file format parsing and generation
 * - DSK/PO to WOZ conversion
 * - 4-and-4 encoding (address marks)
 * - 5-and-3 encoding (DOS 3.2, 13-sector)
 * - 6-and-2 encoding (DOS 3.3/ProDOS, 16-sector)
 * - 3.5" GCR encoding (Mac/Apple IIgs)
 * - Track bit stream manipulation
 * - WRIT chunk support for write images
 */

#ifndef UFT_WOZ_H
#define UFT_WOZ_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * WOZ Format Constants
 *============================================================================*/

/** WOZ1 magic number ('WOZ1') */
#define UFT_WOZ1_MAGIC              0x315A4F57

/** WOZ2 magic number ('WOZ2') */
#define UFT_WOZ2_MAGIC              0x325A4F57

/** WOZ header size */
#define UFT_WOZ_HEADER_SIZE         12

/** WOZ track count (5.25" max) */
#define UFT_WOZ_TRACK_COUNT_525     40

/** WOZ track count (3.5" max) */
#define UFT_WOZ_TRACK_COUNT_35      160

/** WOZ block size */
#define UFT_WOZ_BLOCK_SIZE          512

/** WOZ1 track size */
#define UFT_WOZ1_TRACK_SIZE         0x1A00

/** WOZ2 bits blocks per track */
#define UFT_WOZ2_BITS_BLOCKS        13

/** WOZ2 bits track size */
#define UFT_WOZ2_BITS_TRACK_SIZE    (UFT_WOZ2_BITS_BLOCKS * UFT_WOZ_BLOCK_SIZE)

/*============================================================================
 * Apple Disk Constants
 *============================================================================*/

/** Tracks per disk (5.25") */
#define UFT_APPLE_TRACKS_525        35

/** Sectors per track (13-sector DOS 3.2) */
#define UFT_APPLE_SECTORS_13        13

/** Sectors per track (16-sector DOS 3.3/ProDOS) */
#define UFT_APPLE_SECTORS_16        16

/** Bytes per sector */
#define UFT_APPLE_SECTOR_SIZE       256

/** DSK image size (140K) */
#define UFT_DSK_IMAGE_SIZE          (UFT_APPLE_TRACKS_525 * UFT_APPLE_SECTORS_16 * UFT_APPLE_SECTOR_SIZE)

/** DOS 3.2 image size (116K) */
#define UFT_DOS32_IMAGE_SIZE        (UFT_APPLE_TRACKS_525 * UFT_APPLE_SECTORS_13 * UFT_APPLE_SECTOR_SIZE)

/** Default DOS volume number */
#define UFT_APPLE_VOLUME_DEFAULT    254

/** Sync bytes at track start */
#define UFT_APPLE_TRACK_LEADER      64

/** Sync bytes between sectors */
#define UFT_APPLE_SECTOR_GAP        16

/** 6-and-2 encoded sector size (with checksum) */
#define UFT_NIBBLE_62_SIZE          343

/** 5-and-3 encoded sector size (with checksum) */
#define UFT_NIBBLE_53_SIZE          411

/*============================================================================
 * Apple Address/Data Field Markers
 *============================================================================*/

/** 16-sector address prologue (D5 AA 96) */
#define UFT_ADDR_PROLOGUE_16        0xD5AA96

/** 13-sector address prologue (D5 AA B5) */
#define UFT_ADDR_PROLOGUE_13        0xD5AAB5

/** Data field prologue (D5 AA AD) */
#define UFT_DATA_PROLOGUE           0xD5AAAD

/** Epilogue (DE AA EB) */
#define UFT_EPILOGUE                0xDEAAEB

/** Sync byte value */
#define UFT_SYNC_BYTE               0xFF

/*============================================================================
 * WOZ Chunk IDs
 *============================================================================*/

/** INFO chunk ID */
#define UFT_WOZ_CHUNK_INFO          0x4F464E49  /* 'INFO' */

/** TMAP chunk ID */
#define UFT_WOZ_CHUNK_TMAP          0x50414D54  /* 'TMAP' */

/** TRKS chunk ID */
#define UFT_WOZ_CHUNK_TRKS          0x534B5254  /* 'TRKS' */

/** WRIT chunk ID */
#define UFT_WOZ_CHUNK_WRIT          0x54495257  /* 'WRIT' */

/** META chunk ID */
#define UFT_WOZ_CHUNK_META          0x4154454D  /* 'META' */

/*============================================================================
 * WOZ Structures
 *============================================================================*/

/**
 * @brief WOZ file header
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t magic;           /**< 'WOZ1' or 'WOZ2' */
    uint8_t  high_bits;       /**< 0xFF (verify high bits preserved) */
    uint8_t  lf;              /**< 0x0A (LF) */
    uint8_t  cr;              /**< 0x0D (CR) */
    uint8_t  lf2;             /**< 0x0A (LF) */
    uint32_t crc32;           /**< CRC32 of all data after header */
} uft_woz_header_t;
#pragma pack(pop)

/**
 * @brief WOZ chunk header
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t id;              /**< Chunk ID (4 chars) */
    uint32_t size;            /**< Chunk data size */
} uft_woz_chunk_header_t;
#pragma pack(pop)

/**
 * @brief WOZ INFO chunk (60 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  version;         /**< INFO chunk version (1 or 2) */
    uint8_t  disk_type;       /**< 1=5.25", 2=3.5" */
    uint8_t  write_protected; /**< 0=no, 1=yes */
    uint8_t  synchronized;    /**< 0=no, 1=yes */
    uint8_t  cleaned;         /**< 0=no, 1=yes */
    char     creator[32];     /**< Creating software */
    
    /* WOZ2 only fields */
    uint8_t  disk_sides;      /**< 1 or 2 */
    uint8_t  boot_sector_format; /**< 0=unknown, 1=16-sector, 2=13-sector, 3=both */
    uint8_t  optimal_bit_timing; /**< Optimal bit timing in 125ns units */
    uint16_t compatible_hardware; /**< Compatible hardware bitmask */
    uint16_t required_ram;    /**< Required RAM in KB */
    uint16_t largest_track;   /**< Largest track block count */
    uint16_t flux_block;      /**< Starting block for FLUX chunk */
    uint16_t largest_flux;    /**< Largest flux track block count */
    uint8_t  reserved[10];    /**< Reserved for future use */
} uft_woz_info_t;
#pragma pack(pop)

/**
 * @brief WOZ1 track entry (in TRKS chunk)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  data[6646];      /**< Track bit data */
    uint16_t bytes_used;      /**< Bytes of valid data */
    uint16_t bit_count;       /**< Number of valid bits */
    uint16_t splice_point;    /**< Bit index of track splice */
    uint8_t  splice_nibble;   /**< Nibble at splice point */
    uint8_t  splice_bit_count;/**< Bits in splice nibble */
    uint16_t reserved;        /**< Reserved */
} uft_woz1_track_t;
#pragma pack(pop)

/**
 * @brief WOZ2 track entry (8 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t starting_block;  /**< Starting 512-byte block */
    uint16_t block_count;     /**< Number of blocks */
    uint32_t bit_count;       /**< Number of valid bits */
} uft_woz2_track_entry_t;
#pragma pack(pop)

/**
 * @brief WOZ WRIT chunk entry (per track)
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t track_start_block;   /**< Track data start block */
    uint16_t track_block_count;   /**< Track block count */
    uint32_t track_bit_count;     /**< Track bit count */
} uft_woz_writ_entry_t;
#pragma pack(pop)

/*============================================================================
 * Sector Format Types
 *============================================================================*/

/**
 * @brief DSK sector ordering format
 */
typedef enum {
    UFT_SECTOR_FORMAT_DOS33  = 0,   /**< DOS 3.3 sector ordering */
    UFT_SECTOR_FORMAT_PRODOS = 1,   /**< ProDOS sector ordering */
    UFT_SECTOR_FORMAT_DOS32  = 2,   /**< DOS 3.2 (13-sector) */
    UFT_SECTOR_FORMAT_LINEAR = 3    /**< Linear/physical ordering */
} uft_sector_format_t;

/*============================================================================
 * 6-and-2 Nibble Encoding (DOS 3.3/ProDOS, 16-sector)
 *============================================================================*/

/**
 * @brief 6-and-2 translation table (64 valid disk bytes)
 *
 * Maps 6-bit values (0-63) to valid disk nibbles (0x96-0xFF).
 * Excludes 0xAA and 0xD5 which are used as markers.
 */
static const uint8_t uft_nibble_62_encode[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

/**
 * @brief Build 6-and-2 decode table
 * @param table Output decode table (256 bytes, 0xFF = invalid)
 */
static inline void uft_nibble_62_build_decode(uint8_t table[256])
{
    for (int i = 0; i < 256; i++) {
        table[i] = 0xFF;  /* Mark as invalid */
    }
    for (int i = 0; i < 64; i++) {
        table[uft_nibble_62_encode[i]] = i;
    }
}

/**
 * @brief Encode 256-byte sector using 6-and-2 encoding
 * @param src Source sector data (256 bytes)
 * @param dst Destination encoded data (343 bytes)
 *
 * Based on dsk2woz2.c encode_6_and_2() and CiderPress algorithms.
 */
static inline void uft_nibble_62_encode_sector(const uint8_t src[256], uint8_t dst[343])
{
    /* Bit reversal table for 2-bit values */
    static const uint8_t bit_reverse[4] = {0, 2, 1, 3};
    
    /* Build the auxiliary buffer (first 86 bytes) from bottom 2 bits */
    uint8_t aux[86];
    for (int c = 0; c < 84; c++) {
        aux[c] = bit_reverse[src[c] & 3] |
                 (bit_reverse[src[c + 86] & 3] << 2) |
                 (bit_reverse[src[c + 172] & 3] << 4);
    }
    aux[84] = (bit_reverse[src[84] & 3]) |
              (bit_reverse[src[170] & 3] << 2);
    aux[85] = (bit_reverse[src[85] & 3]) |
              (bit_reverse[src[171] & 3] << 2);
    
    /* Build main data buffer (top 6 bits) */
    uint8_t data[256];
    for (int c = 0; c < 256; c++) {
        data[c] = src[c] >> 2;
    }
    
    /* XOR encode auxiliary bytes */
    uint8_t checksum = 0;
    for (int i = 85; i >= 0; i--) {
        dst[i] = uft_nibble_62_encode[aux[i] ^ checksum];
        checksum = aux[i];
    }
    
    /* XOR encode data bytes */
    for (int i = 0; i < 256; i++) {
        dst[86 + i] = uft_nibble_62_encode[data[i] ^ checksum];
        checksum = data[i];
    }
    
    /* Final checksum byte */
    dst[342] = uft_nibble_62_encode[checksum];
}

/**
 * @brief Decode 6-and-2 encoded sector
 * @param src Source encoded data (343 bytes)
 * @param dst Destination sector data (256 bytes)
 * @param decode_table Pre-built decode table
 * @return 0 on success, -1 on checksum error, -2 on invalid nibble
 */
static inline int uft_nibble_62_decode_sector(const uint8_t src[343], uint8_t dst[256],
                                              const uint8_t decode_table[256])
{
    uint8_t aux[86];
    uint8_t data[256];
    uint8_t checksum = 0;
    
    /* Decode and XOR auxiliary bytes */
    for (int i = 85; i >= 0; i--) {
        uint8_t val = decode_table[src[i]];
        if (val == 0xFF) return -2;
        checksum ^= val;
        aux[i] = checksum;
    }
    
    /* Decode and XOR data bytes */
    for (int i = 0; i < 256; i++) {
        uint8_t val = decode_table[src[86 + i]];
        if (val == 0xFF) return -2;
        checksum ^= val;
        data[i] = checksum << 2;  /* Shift to top 6 bits */
    }
    
    /* Verify checksum */
    uint8_t final_check = decode_table[src[342]];
    if (final_check == 0xFF) return -2;
    if ((checksum ^ final_check) != 0) return -1;
    
    /* Reconstruct bottom 2 bits from auxiliary buffer */
    static const uint8_t bit_reverse[4] = {0, 2, 1, 3};
    
    for (int i = 0; i < 86; i++) {
        int j = i + 86;
        int k = i + 172;
        
        dst[i] = data[i] | bit_reverse[(aux[i] >> 0) & 3];
        dst[j] = data[j] | bit_reverse[(aux[i] >> 2) & 3];
        if (k < 256) {
            dst[k] = data[k] | bit_reverse[(aux[i] >> 4) & 3];
        }
    }
    
    return 0;
}

/*============================================================================
 * 5-and-3 Nibble Encoding (DOS 3.2, 13-sector)
 *============================================================================*/

/**
 * @brief 5-and-3 translation table (32 valid disk bytes)
 */
static const uint8_t uft_nibble_53_encode[32] = {
    0xAB, 0xAD, 0xAE, 0xAF, 0xB5, 0xB6, 0xB7, 0xBA,
    0xBB, 0xBD, 0xBE, 0xBF, 0xD6, 0xD7, 0xDA, 0xDB,
    0xDD, 0xDE, 0xDF, 0xEA, 0xEB, 0xED, 0xEE, 0xEF,
    0xF5, 0xF6, 0xF7, 0xFA, 0xFB, 0xFD, 0xFE, 0xFF
};

/**
 * @brief Build 5-and-3 decode table
 * @param table Output decode table (256 bytes, 0xFF = invalid)
 */
static inline void uft_nibble_53_build_decode(uint8_t table[256])
{
    for (int i = 0; i < 256; i++) {
        table[i] = 0xFF;
    }
    for (int i = 0; i < 32; i++) {
        table[uft_nibble_53_encode[i]] = i;
    }
}

/**
 * @brief Encode 256-byte sector using 5-and-3 encoding
 * @param src Source sector data (256 bytes)
 * @param dst Destination encoded data (411 bytes)
 *
 * Based on DskToWoz2 nibblize_5_3.c
 */
static inline void uft_nibble_53_encode_sector(const uint8_t src[256], uint8_t dst[411])
{
    #define GRP_53 51
    uint8_t top[256];
    uint8_t thr[154];  /* 3 * 51 + 1 = 154 */
    uint8_t checksum = 0;
    
    /* Split bytes into 5-bit top and 3-bit bottom parts */
    for (int i = GRP_53 - 1; i >= 0; i--) {
        uint8_t b1 = src[i * 5 + 0];
        uint8_t b2 = src[i * 5 + 1];
        uint8_t b3 = src[i * 5 + 2];
        uint8_t b4 = src[i * 5 + 3];
        uint8_t b5 = src[i * 5 + 4];
        
        top[i + 0 * GRP_53] = b1 >> 3;
        top[i + 1 * GRP_53] = b2 >> 3;
        top[i + 2 * GRP_53] = b3 >> 3;
        top[i + 3 * GRP_53] = b4 >> 3;
        top[i + 4 * GRP_53] = b5 >> 3;
        
        thr[i + 0 * GRP_53] = ((b1 & 7) << 2) | ((b4 & 4) >> 1) | ((b5 & 4) >> 2);
        thr[i + 1 * GRP_53] = ((b2 & 7) << 2) | ((b4 & 2))      | ((b5 & 2) >> 1);
        thr[i + 2 * GRP_53] = ((b3 & 7) << 2) | ((b4 & 1) << 1) | ((b5 & 1));
    }
    
    /* Handle last byte */
    uint8_t last = src[255];
    top[5 * GRP_53] = last >> 3;
    thr[3 * GRP_53] = last & 7;
    
    /* Encode with XOR checksum */
    int out_idx = 0;
    for (int i = 153; i >= 0; i--) {
        dst[out_idx++] = uft_nibble_53_encode[thr[i] ^ checksum];
        checksum = thr[i];
    }
    for (int i = 0; i < 256; i++) {
        dst[out_idx++] = uft_nibble_53_encode[top[i] ^ checksum];
        checksum = top[i];
    }
    dst[out_idx] = uft_nibble_53_encode[checksum];
    
    #undef GRP_53
}

/*============================================================================
 * 4-and-4 Encoding (Address Fields)
 *============================================================================*/

/**
 * @brief Encode byte using 4-and-4 encoding
 * @param value Byte to encode
 * @param dst Output buffer (2 bytes)
 *
 * Splits byte into two nibbles, each with MSB set:
 * byte 0xAB becomes 0xAA|0x55 and 0xAA|0x55 pattern
 */
static inline void uft_nibble_44_encode(uint8_t value, uint8_t dst[2])
{
    dst[0] = 0xAA | ((value >> 1) & 0x55);
    dst[1] = 0xAA | (value & 0x55);
}

/**
 * @brief Decode 4-and-4 encoded byte
 * @param src Source buffer (2 bytes)
 * @return Decoded byte
 */
static inline uint8_t uft_nibble_44_decode(const uint8_t src[2])
{
    return ((src[0] << 1) | 0x01) & src[1];
}

/*============================================================================
 * DOS 3.3 Sector Interleave
 *============================================================================*/

/**
 * @brief DOS 3.3 physical-to-logical sector mapping
 *
 * Maps physical sector number to logical sector number.
 * Physical sectors 0-15 map to logical in this order.
 */
static const uint8_t uft_interleave_dos33[16] = {
    0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15
};

/**
 * @brief ProDOS physical-to-logical sector mapping
 */
static const uint8_t uft_interleave_prodos[16] = {
    0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15
};

/**
 * @brief DOS 3.2 physical-to-logical sector mapping (13-sector)
 */
static const uint8_t uft_interleave_dos32[13] = {
    0, 10, 7, 4, 1, 11, 8, 5, 2, 12, 9, 6, 3
};

/**
 * @brief Get logical sector from physical sector
 * @param physical Physical sector number
 * @param format Sector format
 * @return Logical sector number
 */
static inline int uft_logical_sector(int physical, uft_sector_format_t format)
{
    switch (format) {
        case UFT_SECTOR_FORMAT_DOS33:
            return (physical == 15) ? 15 : (physical * 7) % 15;
        case UFT_SECTOR_FORMAT_PRODOS:
            return (physical == 15) ? 15 : (physical * 8) % 15;
        case UFT_SECTOR_FORMAT_DOS32:
            return (physical < 13) ? uft_interleave_dos32[physical] : -1;
        case UFT_SECTOR_FORMAT_LINEAR:
        default:
            return physical;
    }
}

/*============================================================================
 * Track Bit Stream Functions
 *============================================================================*/

/**
 * @brief Write a byte to bit stream
 * @param buffer Bit buffer
 * @param bit_index Current bit index (updated)
 * @param value Byte to write
 */
static inline void uft_bits_write_byte(uint8_t *buffer, size_t *bit_index, uint8_t value)
{
    for (int i = 7; i >= 0; i--) {
        size_t byte_idx = *bit_index / 8;
        size_t bit_pos = 7 - (*bit_index % 8);
        
        if (value & (1 << i)) {
            buffer[byte_idx] |= (1 << bit_pos);
        } else {
            buffer[byte_idx] &= ~(1 << bit_pos);
        }
        (*bit_index)++;
    }
}

/**
 * @brief Write sync byte (0xFF with 2 extra 0 bits = 10-bit sync)
 * @param buffer Bit buffer
 * @param bit_index Current bit index (updated)
 */
static inline void uft_bits_write_sync(uint8_t *buffer, size_t *bit_index)
{
    uft_bits_write_byte(buffer, bit_index, 0xFF);
    *bit_index += 2;  /* Skip 2 bits (leave as 0) */
}

/**
 * @brief Write 4-and-4 encoded value
 * @param buffer Bit buffer
 * @param bit_index Current bit index (updated)
 * @param value Value to encode
 */
static inline void uft_bits_write_44(uint8_t *buffer, size_t *bit_index, uint8_t value)
{
    uint8_t encoded[2];
    uft_nibble_44_encode(value, encoded);
    uft_bits_write_byte(buffer, bit_index, encoded[0]);
    uft_bits_write_byte(buffer, bit_index, encoded[1]);
}

/*============================================================================
 * WOZ Track Generation
 *============================================================================*/

/**
 * @brief Encode a full track from sector data
 * @param sector_data 16 sectors of 256 bytes each (4096 bytes total)
 * @param track_num Track number (0-34)
 * @param volume Volume number
 * @param format Sector format
 * @param output Output bit buffer (6656 bytes max for WOZ2)
 * @return Number of valid bits in track
 */
static inline size_t uft_encode_track_525(const uint8_t *sector_data, int track_num,
                                          uint8_t volume, uft_sector_format_t format,
                                          uint8_t *output)
{
    size_t bit_index = 0;
    int sectors = (format == UFT_SECTOR_FORMAT_DOS32) ? 13 : 16;
    
    /* Clear output buffer */
    for (int i = 0; i < UFT_WOZ2_BITS_TRACK_SIZE; i++) {
        output[i] = 0;
    }
    
    /* Write track leader (64 sync bytes) */
    for (int i = 0; i < UFT_APPLE_TRACK_LEADER; i++) {
        uft_bits_write_sync(output, &bit_index);
    }
    
    /* Write each sector */
    for (int s = 0; s < sectors; s++) {
        /* Address field prologue */
        uft_bits_write_byte(output, &bit_index, 0xD5);
        uft_bits_write_byte(output, &bit_index, 0xAA);
        uft_bits_write_byte(output, &bit_index, (sectors == 13) ? 0xB5 : 0x96);
        
        /* Address field: volume, track, sector, checksum (4-and-4) */
        uft_bits_write_44(output, &bit_index, volume);
        uft_bits_write_44(output, &bit_index, track_num);
        uft_bits_write_44(output, &bit_index, s);
        uft_bits_write_44(output, &bit_index, volume ^ track_num ^ s);
        
        /* Address field epilogue */
        uft_bits_write_byte(output, &bit_index, 0xDE);
        uft_bits_write_byte(output, &bit_index, 0xAA);
        uft_bits_write_byte(output, &bit_index, 0xEB);
        
        /* Gap between address and data (7 sync bytes) */
        for (int i = 0; i < 7; i++) {
            uft_bits_write_sync(output, &bit_index);
        }
        
        /* Data field prologue */
        uft_bits_write_byte(output, &bit_index, 0xD5);
        uft_bits_write_byte(output, &bit_index, 0xAA);
        uft_bits_write_byte(output, &bit_index, 0xAD);
        
        /* Get logical sector and encode data */
        int logical = uft_logical_sector(s, format);
        const uint8_t *src = &sector_data[logical * UFT_APPLE_SECTOR_SIZE];
        
        if (sectors == 16) {
            uint8_t encoded[343];
            uft_nibble_62_encode_sector(src, encoded);
            for (int i = 0; i < 343; i++) {
                uft_bits_write_byte(output, &bit_index, encoded[i]);
            }
        } else {
            uint8_t encoded[411];
            uft_nibble_53_encode_sector(src, encoded);
            for (int i = 0; i < 411; i++) {
                uft_bits_write_byte(output, &bit_index, encoded[i]);
            }
        }
        
        /* Data field epilogue */
        uft_bits_write_byte(output, &bit_index, 0xDE);
        uft_bits_write_byte(output, &bit_index, 0xAA);
        uft_bits_write_byte(output, &bit_index, 0xEB);
        
        /* Inter-sector gap (16 sync bytes, except after last sector) */
        if (s < sectors - 1) {
            for (int i = 0; i < UFT_APPLE_SECTOR_GAP; i++) {
                uft_bits_write_sync(output, &bit_index);
            }
        } else {
            uft_bits_write_byte(output, &bit_index, 0xFF);
        }
    }
    
    return bit_index;
}

/*============================================================================
 * WOZ CRC32 Calculation
 *============================================================================*/

/**
 * @brief CRC32 lookup table (standard polynomial 0xEDB88320)
 */
extern const uint32_t uft_woz_crc32_table[256];

/**
 * @brief Calculate CRC32 for WOZ file
 * @param crc Initial CRC value
 * @param data Data buffer
 * @param size Data size
 * @return Updated CRC32
 */
static inline uint32_t uft_woz_crc32(uint32_t crc, const void *data, size_t size)
{
    const uint8_t *p = (const uint8_t *)data;
    crc = crc ^ 0xFFFFFFFF;
    while (size--) {
        crc = uft_woz_crc32_table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

/*============================================================================
 * WOZ File I/O Functions
 *============================================================================*/

/**
 * @brief WOZ file context
 */
typedef struct {
    uft_woz_header_t header;
    uft_woz_info_t info;
    uint8_t tmap[160];        /**< Track map */
    void *tracks;             /**< Track data (WOZ1 or WOZ2 format) */
    char *meta;               /**< Metadata string */
    size_t meta_len;          /**< Metadata length */
    int version;              /**< 1 or 2 */
} uft_woz_file_t;

/**
 * @brief Read WOZ file from buffer
 * @param data File data
 * @param size Data size
 * @param woz Output WOZ structure
 * @return 0 on success, negative on error
 */
int uft_woz_read(const uint8_t *data, size_t size, uft_woz_file_t *woz);

/**
 * @brief Write WOZ2 file to buffer
 * @param woz WOZ structure
 * @param output Output buffer (pre-allocated)
 * @param max_size Maximum output size
 * @return Bytes written, or negative on error
 */
int uft_woz_write(const uft_woz_file_t *woz, uint8_t *output, size_t max_size);

/**
 * @brief Convert DSK/PO to WOZ2
 * @param dsk DSK image data (143360 bytes)
 * @param format Sector format (DOS33 or ProDOS)
 * @param output Output buffer
 * @param max_size Maximum output size
 * @return Bytes written, or negative on error
 */
int uft_dsk_to_woz2(const uint8_t *dsk, uft_sector_format_t format,
                    uint8_t *output, size_t max_size);

/**
 * @brief Convert WOZ to DSK
 * @param woz WOZ file
 * @param format Target sector format
 * @param output Output buffer (143360 bytes)
 * @return 0 on success, negative on error
 */
int uft_woz_to_dsk(const uft_woz_file_t *woz, uft_sector_format_t format,
                   uint8_t *output);

/**
 * @brief Free WOZ file structure
 * @param woz WOZ structure to free
 */
void uft_woz_free(uft_woz_file_t *woz);

#ifdef __cplusplus
}
#endif

#endif /* UFT_WOZ_H */
