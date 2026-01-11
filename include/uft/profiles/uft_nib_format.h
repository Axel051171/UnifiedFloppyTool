/**
 * @file uft_nib_format.h
 * @brief NIB (Apple II Nibble) format profile - Raw Apple II disk format
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * NIB is a raw nibble-level disk image format for Apple II. It stores the
 * raw GCR-encoded nibbles as they appear on disk, including sync bytes
 * and self-sync patterns. This preserves more disk structure than DSK/DO/PO
 * formats but uses more space.
 *
 * Key features:
 * - Raw GCR nibble data
 * - Fixed 6656 bytes per track
 * - 35 tracks standard
 * - Preserves disk structure
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_NIB_FORMAT_H
#define UFT_NIB_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * NIB Format Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief NIB bytes per track (fixed) */
#define UFT_NIB_TRACK_SIZE          6656

/** @brief NIB standard track count */
#define UFT_NIB_STANDARD_TRACKS     35

/** @brief NIB extended track count */
#define UFT_NIB_EXTENDED_TRACKS     40

/** @brief NIB standard file size (35 tracks) */
#define UFT_NIB_FILE_SIZE_35        (UFT_NIB_TRACK_SIZE * 35)

/** @brief NIB extended file size (40 tracks) */
#define UFT_NIB_FILE_SIZE_40        (UFT_NIB_TRACK_SIZE * 40)

/** @brief Sectors per track (DOS 3.3) */
#define UFT_NIB_SECTORS_PER_TRACK   16

/** @brief Bytes per sector (decoded) */
#define UFT_NIB_SECTOR_SIZE         256

/* ═══════════════════════════════════════════════════════════════════════════
 * Apple II GCR Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Sync byte value */
#define UFT_NIB_SYNC_BYTE           0xFF

/** @brief Address prologue byte 1 */
#define UFT_NIB_ADDR_PROLOGUE_1     0xD5

/** @brief Address prologue byte 2 */
#define UFT_NIB_ADDR_PROLOGUE_2     0xAA

/** @brief Address prologue byte 3 */
#define UFT_NIB_ADDR_PROLOGUE_3     0x96

/** @brief Data prologue byte 1 */
#define UFT_NIB_DATA_PROLOGUE_1     0xD5

/** @brief Data prologue byte 2 */
#define UFT_NIB_DATA_PROLOGUE_2     0xAA

/** @brief Data prologue byte 3 */
#define UFT_NIB_DATA_PROLOGUE_3     0xAD

/** @brief Epilogue byte 1 */
#define UFT_NIB_EPILOGUE_1          0xDE

/** @brief Epilogue byte 2 */
#define UFT_NIB_EPILOGUE_2          0xAA

/** @brief Epilogue byte 3 */
#define UFT_NIB_EPILOGUE_3          0xEB

/** @brief DOS 3.2 sectors per track */
#define UFT_NIB_DOS32_SECTORS       13

/** @brief DOS 3.3/ProDOS sectors per track */
#define UFT_NIB_DOS33_SECTORS       16

/* ═══════════════════════════════════════════════════════════════════════════
 * GCR Encoding Tables
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief 6-and-2 GCR encoding table (64 entries)
 * Maps 6-bit values to 8-bit disk bytes
 */
static const uint8_t UFT_NIB_GCR_ENCODE_62[] = {
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
 * @brief 6-and-2 GCR decoding table (256 entries)
 * Maps 8-bit disk bytes to 6-bit values (0xFF = invalid)
 */
static const uint8_t UFT_NIB_GCR_DECODE_62[] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x00-0x07 */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x08-0x0F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x10-0x17 */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x18-0x1F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x20-0x27 */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x28-0x2F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x30-0x37 */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x38-0x3F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x40-0x47 */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x48-0x4F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x50-0x57 */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x58-0x5F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x60-0x67 */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x68-0x6F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x70-0x77 */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x78-0x7F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x80-0x87 */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x88-0x8F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x01,  /* 0x90-0x97 */
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x04, 0x05, 0x06,  /* 0x98-0x9F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x07, 0x08,  /* 0xA0-0xA7 */
    0xFF, 0xFF, 0xFF, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,  /* 0xA8-0xAF */
    0xFF, 0xFF, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13,  /* 0xB0-0xB7 */
    0xFF, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A,  /* 0xB8-0xBF */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0xC0-0xC7 */
    0xFF, 0xFF, 0xFF, 0x1B, 0xFF, 0x1C, 0x1D, 0x1E,  /* 0xC8-0xCF */
    0xFF, 0xFF, 0xFF, 0x1F, 0xFF, 0xFF, 0x20, 0x21,  /* 0xD0-0xD7 */
    0xFF, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,  /* 0xD8-0xDF */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x29, 0x2A, 0x2B,  /* 0xE0-0xE7 */
    0xFF, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32,  /* 0xE8-0xEF */
    0xFF, 0xFF, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,  /* 0xF0-0xF7 */
    0xFF, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F   /* 0xF8-0xFF */
};

/* ═══════════════════════════════════════════════════════════════════════════
 * NIB Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief NIB address field (decoded)
 */
typedef struct {
    uint8_t volume;                 /**< Volume number */
    uint8_t track;                  /**< Track number */
    uint8_t sector;                 /**< Sector number */
    uint8_t checksum;               /**< XOR checksum */
    bool valid;                     /**< Address field valid */
} uft_nib_address_t;

/**
 * @brief NIB sector information
 */
typedef struct {
    uft_nib_address_t address;      /**< Address field */
    size_t address_offset;          /**< Offset to address field */
    size_t data_offset;             /**< Offset to data field */
    bool has_data;                  /**< Data field found */
    bool data_valid;                /**< Data checksum valid */
    uint8_t data[256];              /**< Decoded sector data */
} uft_nib_sector_t;

/**
 * @brief Parsed NIB file information
 */
typedef struct {
    uint8_t track_count;            /**< Number of tracks */
    uint32_t file_size;             /**< File size */
    uint8_t volume;                 /**< Detected volume number */
    bool is_dos32;                  /**< DOS 3.2 format (13 sectors) */
    bool is_dos33;                  /**< DOS 3.3/ProDOS format (16 sectors) */
    uint32_t valid_sectors;         /**< Count of valid sectors */
    uint32_t total_sectors;         /**< Total sectors found */
    uint32_t bad_checksums;         /**< Sectors with bad checksums */
} uft_nib_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Inline Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Encode nibble using 6-and-2 GCR
 * @param value 6-bit value (0-63)
 * @return Encoded disk byte
 */
static inline uint8_t uft_nib_gcr_encode(uint8_t value) {
    return (value < 64) ? UFT_NIB_GCR_ENCODE_62[value] : 0;
}

/**
 * @brief Decode disk byte using 6-and-2 GCR
 * @param byte Encoded disk byte
 * @return 6-bit value, or 0xFF if invalid
 */
static inline uint8_t uft_nib_gcr_decode(uint8_t byte) {
    return UFT_NIB_GCR_DECODE_62[byte];
}

/**
 * @brief Check if byte is valid disk byte
 * @param byte Byte to check
 * @return true if valid (high bit set, not two consecutive zeros)
 */
static inline bool uft_nib_is_valid_byte(uint8_t byte) {
    return (byte & 0x80) && UFT_NIB_GCR_DECODE_62[byte] != 0xFF;
}

/**
 * @brief Check if byte is sync byte
 * @param byte Byte to check
 * @return true if sync (0xFF)
 */
static inline bool uft_nib_is_sync(uint8_t byte) {
    return byte == UFT_NIB_SYNC_BYTE;
}

/**
 * @brief Get track count from file size
 * @param size File size
 * @return Number of tracks, or 0 if invalid
 */
static inline uint8_t uft_nib_tracks_from_size(size_t size) {
    if (size == UFT_NIB_FILE_SIZE_35) return 35;
    if (size == UFT_NIB_FILE_SIZE_40) return 40;
    if (size % UFT_NIB_TRACK_SIZE == 0) {
        return size / UFT_NIB_TRACK_SIZE;
    }
    return 0;
}

/**
 * @brief Calculate track offset in file
 * @param track Track number (0-based)
 * @return Byte offset
 */
static inline size_t uft_nib_track_offset(uint8_t track) {
    return track * UFT_NIB_TRACK_SIZE;
}

/**
 * @brief Decode 4-and-4 encoded byte pair
 * @param odd Odd byte (high bits)
 * @param even Even byte (low bits)
 * @return Decoded byte
 */
static inline uint8_t uft_nib_decode_44(uint8_t odd, uint8_t even) {
    return ((odd << 1) | 1) & even;
}

/**
 * @brief Encode byte as 4-and-4 pair
 * @param byte Byte to encode
 * @param odd Output odd byte
 * @param even Output even byte
 */
static inline void uft_nib_encode_44(uint8_t byte, uint8_t* odd, uint8_t* even) {
    *odd = (byte >> 1) | 0xAA;
    *even = byte | 0xAA;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Address Field Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Find address prologue in track data
 * @param data Track data
 * @param size Track size
 * @param start Starting offset
 * @return Offset of prologue, or size if not found
 */
static inline size_t uft_nib_find_address_prologue(const uint8_t* data, size_t size,
                                                   size_t start) {
    for (size_t i = start; i + 2 < size; i++) {
        if (data[i] == UFT_NIB_ADDR_PROLOGUE_1 &&
            data[i + 1] == UFT_NIB_ADDR_PROLOGUE_2 &&
            data[i + 2] == UFT_NIB_ADDR_PROLOGUE_3) {
            return i;
        }
    }
    return size;
}

/**
 * @brief Find data prologue in track data
 * @param data Track data
 * @param size Track size
 * @param start Starting offset
 * @return Offset of prologue, or size if not found
 */
static inline size_t uft_nib_find_data_prologue(const uint8_t* data, size_t size,
                                                size_t start) {
    for (size_t i = start; i + 2 < size; i++) {
        if (data[i] == UFT_NIB_DATA_PROLOGUE_1 &&
            data[i + 1] == UFT_NIB_DATA_PROLOGUE_2 &&
            data[i + 2] == UFT_NIB_DATA_PROLOGUE_3) {
            return i;
        }
    }
    return size;
}

/**
 * @brief Parse address field from track data
 * @param data Track data (pointing to prologue byte 1)
 * @param size Available data
 * @param addr Output address structure
 * @return true if valid address field
 */
static inline bool uft_nib_parse_address(const uint8_t* data, size_t size,
                                         uft_nib_address_t* addr) {
    if (!data || !addr || size < 14) return false;
    
    /* Verify prologue */
    if (data[0] != UFT_NIB_ADDR_PROLOGUE_1 ||
        data[1] != UFT_NIB_ADDR_PROLOGUE_2 ||
        data[2] != UFT_NIB_ADDR_PROLOGUE_3) {
        return false;
    }
    
    /* Decode 4-and-4 encoded fields */
    addr->volume = uft_nib_decode_44(data[3], data[4]);
    addr->track = uft_nib_decode_44(data[5], data[6]);
    addr->sector = uft_nib_decode_44(data[7], data[8]);
    addr->checksum = uft_nib_decode_44(data[9], data[10]);
    
    /* Verify checksum */
    uint8_t calc_checksum = addr->volume ^ addr->track ^ addr->sector;
    addr->valid = (calc_checksum == addr->checksum);
    
    return addr->valid;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Probe and Detection
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Probe data to determine if it's a NIB file
 * @param data File data
 * @param size File size
 * @return Confidence score 0-100
 */
static inline int uft_nib_probe(const uint8_t* data, size_t size) {
    /* Check file size */
    uint8_t tracks = uft_nib_tracks_from_size(size);
    if (tracks == 0) return 0;
    
    int score = 30;  /* Base score for valid size */
    
    if (tracks == 35) score += 20;
    else if (tracks == 40) score += 15;
    
    if (!data) return score;
    
    /* Check first track for valid Apple II structure */
    size_t addr_pos = uft_nib_find_address_prologue(data, UFT_NIB_TRACK_SIZE, 0);
    if (addr_pos < UFT_NIB_TRACK_SIZE) {
        score += 20;
        
        /* Try to parse address field */
        uft_nib_address_t addr;
        if (uft_nib_parse_address(data + addr_pos, UFT_NIB_TRACK_SIZE - addr_pos, &addr)) {
            score += 15;
            
            /* Check for expected track 0 */
            if (addr.track == 0) {
                score += 10;
            }
        }
    }
    
    /* Check for data prologue */
    size_t data_pos = uft_nib_find_data_prologue(data, UFT_NIB_TRACK_SIZE, 0);
    if (data_pos < UFT_NIB_TRACK_SIZE) {
        score += 5;
    }
    
    return (score > 100) ? 100 : score;
}

/**
 * @brief Parse NIB file into info structure
 * @param data File data
 * @param size File size
 * @param info Output info structure
 * @return true if successfully parsed
 */
static inline bool uft_nib_parse(const uint8_t* data, size_t size,
                                 uft_nib_info_t* info) {
    if (!info) return false;
    
    memset(info, 0, sizeof(*info));
    
    info->track_count = uft_nib_tracks_from_size(size);
    if (info->track_count == 0) return false;
    
    info->file_size = size;
    
    if (!data) return true;
    
    /* Analyze tracks */
    for (uint8_t t = 0; t < info->track_count; t++) {
        const uint8_t* track_data = data + uft_nib_track_offset(t);
        uint8_t sectors_found = 0;
        size_t pos = 0;
        
        while (pos < UFT_NIB_TRACK_SIZE) {
            size_t addr_pos = uft_nib_find_address_prologue(track_data, 
                                                           UFT_NIB_TRACK_SIZE, pos);
            if (addr_pos >= UFT_NIB_TRACK_SIZE) break;
            
            uft_nib_address_t addr;
            if (uft_nib_parse_address(track_data + addr_pos, 
                                      UFT_NIB_TRACK_SIZE - addr_pos, &addr)) {
                info->total_sectors++;
                sectors_found++;
                
                if (addr.valid) {
                    info->valid_sectors++;
                    
                    /* Remember volume number */
                    if (info->volume == 0) {
                        info->volume = addr.volume;
                    }
                } else {
                    info->bad_checksums++;
                }
            }
            
            pos = addr_pos + 14;  /* Move past address field */
        }
        
        /* First track determines format */
        if (t == 0) {
            if (sectors_found == 13) info->is_dos32 = true;
            else if (sectors_found >= 16) info->is_dos33 = true;
        }
    }
    
    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_NIB_FORMAT_H */
