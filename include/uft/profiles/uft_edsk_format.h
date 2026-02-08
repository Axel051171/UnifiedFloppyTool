/**
 * @file uft_edsk_format.h
 * @brief EDSK (Extended DSK) format profile - Amstrad CPC/ZX Spectrum +3 format
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * EDSK is the extended disk image format for Amstrad CPC and ZX Spectrum +3.
 * It extends the original DSK format with support for variable sector sizes,
 * weak/random sectors, and copy protection schemes.
 *
 * Key features:
 * - Variable sector sizes per track
 * - FDC status bytes for copy protection
 * - Weak/random data sector support
 * - Multiple data copies for weak sectors
 *
 * Format specification: http://www.cpcwiki.eu/index.php/Format:DSK_disk_image_file_format
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_EDSK_FORMAT_H
#define UFT_EDSK_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * DSK/EDSK Format Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Standard DSK signature */
#define UFT_DSK_SIGNATURE           "MV - CPC"

/** @brief Extended DSK signature */
#define UFT_EDSK_SIGNATURE          "EXTENDED CPC DSK File\r\nDisk-Info\r\n"

/** @brief Standard DSK signature length */
#define UFT_DSK_SIGNATURE_LEN       8

/** @brief Extended DSK signature length */
#define UFT_EDSK_SIGNATURE_LEN      34

/** @brief Disk information block size */
#define UFT_EDSK_DISK_INFO_SIZE     256

/** @brief Track information block size */
#define UFT_EDSK_TRACK_INFO_SIZE    256

/** @brief Maximum tracks per side */
#define UFT_EDSK_MAX_TRACKS         85

/** @brief Maximum sides */
#define UFT_EDSK_MAX_SIDES          2

/** @brief Maximum sectors per track */
#define UFT_EDSK_MAX_SECTORS        29

/** @brief Track-Info signature */
#define UFT_EDSK_TRACK_SIGNATURE    "Track-Info\r\n"

/** @brief Track signature length */
#define UFT_EDSK_TRACK_SIG_LEN      12

/* ═══════════════════════════════════════════════════════════════════════════
 * FDC Status Bits (for copy protection)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief FDC Status Register 1 bits
 */
#define UFT_EDSK_ST1_MA             0x01    /**< Missing Address Mark */
#define UFT_EDSK_ST1_NW             0x02    /**< Not Writable */
#define UFT_EDSK_ST1_ND             0x04    /**< No Data */
#define UFT_EDSK_ST1_OR             0x10    /**< Overrun */
#define UFT_EDSK_ST1_DE             0x20    /**< Data Error (CRC) */
#define UFT_EDSK_ST1_EN             0x80    /**< End of Cylinder */

/**
 * @brief FDC Status Register 2 bits
 */
#define UFT_EDSK_ST2_MD             0x01    /**< Missing Address Mark in Data */
#define UFT_EDSK_ST2_BC             0x02    /**< Bad Cylinder */
#define UFT_EDSK_ST2_SN             0x04    /**< Scan Not Satisfied */
#define UFT_EDSK_ST2_SH             0x08    /**< Scan Equal Hit */
#define UFT_EDSK_ST2_WC             0x10    /**< Wrong Cylinder */
#define UFT_EDSK_ST2_DD             0x20    /**< Data Error in Data Field */
#define UFT_EDSK_ST2_CM             0x40    /**< Control Mark (deleted data) */

/* ═══════════════════════════════════════════════════════════════════════════
 * Sector Size Codes
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Sector size codes (N value)
 */
typedef enum {
    UFT_EDSK_SIZE_128   = 0,    /**< 128 bytes */
    UFT_EDSK_SIZE_256   = 1,    /**< 256 bytes */
    UFT_EDSK_SIZE_512   = 2,    /**< 512 bytes */
    UFT_EDSK_SIZE_1024  = 3,    /**< 1024 bytes */
    UFT_EDSK_SIZE_2048  = 4,    /**< 2048 bytes */
    UFT_EDSK_SIZE_4096  = 5,    /**< 4096 bytes */
    UFT_EDSK_SIZE_8192  = 6     /**< 8192 bytes */
} uft_edsk_size_code_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Standard CPC Disk Formats
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief CPC disk format types
 */
typedef enum {
    UFT_EDSK_FMT_DATA       = 0,    /**< DATA format (41 tracks, 9 sectors) */
    UFT_EDSK_FMT_SYSTEM     = 1,    /**< SYSTEM format (40 tracks, 9 sectors) */
    UFT_EDSK_FMT_IBM        = 2,    /**< IBM format (40 tracks, 8 sectors) */
    UFT_EDSK_FMT_PARADOS    = 3,    /**< PARADOS (80 tracks, 10 sectors) */
    UFT_EDSK_FMT_CUSTOM     = 255   /**< Custom/unknown format */
} uft_edsk_format_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * EDSK Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Disk Information Block (256 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t signature[34];          /**< "EXTENDED CPC DSK File\r\nDisk-Info\r\n" or "MV - CPC" */
    uint8_t creator[14];            /**< Creator name */
    uint8_t tracks;                 /**< Number of tracks */
    uint8_t sides;                  /**< Number of sides */
    uint16_t track_size;            /**< Track size (standard DSK only, unused in EDSK) */
    uint8_t track_sizes[204];       /**< Track size table (EDSK: MSB of size/256) */
} uft_edsk_disk_info_t;
#pragma pack(pop)

/**
 * @brief Sector Information Block (8 bytes per sector)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t track;                  /**< Track (C) from ID */
    uint8_t side;                   /**< Side (H) from ID */
    uint8_t sector;                 /**< Sector (R) from ID */
    uint8_t size;                   /**< Size (N) from ID */
    uint8_t st1;                    /**< FDC Status Register 1 */
    uint8_t st2;                    /**< FDC Status Register 2 */
    uint16_t data_length;           /**< Actual data length (EDSK) */
} uft_edsk_sector_info_t;
#pragma pack(pop)

/**
 * @brief Track Information Block (256 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t signature[12];          /**< "Track-Info\r\n" */
    uint8_t unused1[4];             /**< Unused */
    uint8_t track;                  /**< Track number */
    uint8_t side;                   /**< Side number */
    uint8_t unused2[2];             /**< Unused */
    uint8_t sector_size;            /**< Sector size code (N) */
    uint8_t sector_count;           /**< Number of sectors */
    uint8_t gap3_length;            /**< GAP#3 length */
    uint8_t filler_byte;            /**< Filler byte */
    uft_edsk_sector_info_t sectors[29]; /**< Sector info (max 29 sectors) */
} uft_edsk_track_info_t;
#pragma pack(pop)

/**
 * @brief Parsed EDSK information
 */
typedef struct {
    bool is_extended;               /**< Extended format (vs standard DSK) */
    char creator[15];               /**< Creator string (null-terminated) */
    uint8_t tracks;                 /**< Number of tracks */
    uint8_t sides;                  /**< Number of sides */
    uint16_t track_size;            /**< Standard track size (DSK only) */
    uint32_t total_sectors;         /**< Total sector count */
    uint32_t total_size;            /**< Total data size */
    uft_edsk_format_t format;       /**< Detected disk format */
    bool has_weak_sectors;          /**< Contains weak/random sectors */
    bool has_errors;                /**< Contains sectors with errors */
    bool has_deleted;               /**< Contains deleted data sectors */
} uft_edsk_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Size Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_edsk_disk_info_t) == 256, "EDSK disk info must be 256 bytes");
_Static_assert(sizeof(uft_edsk_sector_info_t) == 8, "EDSK sector info must be 8 bytes");
_Static_assert(sizeof(uft_edsk_track_info_t) == 256, "EDSK track info must be 256 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Inline Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Convert sector size code to bytes
 * @param size_code Sector size code (N value, 0-6)
 * @return Size in bytes
 */
static inline uint32_t uft_edsk_size_to_bytes(uint8_t size_code) {
    if (size_code > 6) return 0;
    return 128U << size_code;
}

/**
 * @brief Convert bytes to sector size code
 * @param bytes Size in bytes
 * @return Size code (0-6), or 0xFF if invalid
 */
static inline uint8_t uft_edsk_bytes_to_size(uint32_t bytes) {
    switch (bytes) {
        case 128:  return 0;
        case 256:  return 1;
        case 512:  return 2;
        case 1024: return 3;
        case 2048: return 4;
        case 4096: return 5;
        case 8192: return 6;
        default:   return 0xFF;
    }
}

/**
 * @brief Check if sector has CRC error
 * @param st1 Status register 1
 * @param st2 Status register 2
 * @return true if CRC error present
 */
static inline bool uft_edsk_has_crc_error(uint8_t st1, uint8_t st2) {
    return ((st1 & UFT_EDSK_ST1_DE) != 0) || ((st2 & UFT_EDSK_ST2_DD) != 0);
}

/**
 * @brief Check if sector has deleted data mark
 * @param st2 Status register 2
 * @return true if deleted data
 */
static inline bool uft_edsk_is_deleted(uint8_t st2) {
    return (st2 & UFT_EDSK_ST2_CM) != 0;
}

/**
 * @brief Check if sector is weak/random
 * @param info Sector info
 * @return true if weak sector (multiple copies)
 */
static inline bool uft_edsk_is_weak_sector(const uft_edsk_sector_info_t* info) {
    if (!info) return false;
    uint32_t nominal = uft_edsk_size_to_bytes(info->size);
    return (info->data_length > nominal);
}

/**
 * @brief Get number of copies for weak sector
 * @param info Sector info
 * @return Number of copies (1 = normal, >1 = weak)
 */
static inline uint8_t uft_edsk_weak_copies(const uft_edsk_sector_info_t* info) {
    if (!info) return 0;
    uint32_t nominal = uft_edsk_size_to_bytes(info->size);
    if (nominal == 0 || info->data_length == 0) return 0;
    return (info->data_length + nominal - 1) / nominal;
}

/**
 * @brief Get format name
 * @param format Format type
 * @return Format description
 */
static inline const char* uft_edsk_format_name(uft_edsk_format_t format) {
    switch (format) {
        case UFT_EDSK_FMT_DATA:    return "DATA format (41T/9S)";
        case UFT_EDSK_FMT_SYSTEM:  return "SYSTEM format (40T/9S)";
        case UFT_EDSK_FMT_IBM:     return "IBM format (40T/8S)";
        case UFT_EDSK_FMT_PARADOS: return "PARADOS (80T/10S)";
        default: return "Custom/Unknown";
    }
}

/**
 * @brief Describe FDC status
 * @param st1 Status register 1
 * @param st2 Status register 2
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Pointer to buffer
 */
static inline char* uft_edsk_describe_status(uint8_t st1, uint8_t st2,
                                             char* buffer, size_t size) {
    if (!buffer || size == 0) return NULL;
    buffer[0] = '\0';
    
    if (st1 == 0 && st2 == 0) {
        snprintf(buffer, size, "OK");
        return buffer;
    }
    
    size_t pos = 0;
    if (st1 & UFT_EDSK_ST1_DE) pos += snprintf(buffer + pos, size - pos, "ID-CRC ");
    if (st1 & UFT_EDSK_ST1_MA) pos += snprintf(buffer + pos, size - pos, "No-IDAM ");
    if (st1 & UFT_EDSK_ST1_ND) pos += snprintf(buffer + pos, size - pos, "No-Data ");
    if (st2 & UFT_EDSK_ST2_DD) pos += snprintf(buffer + pos, size - pos, "Data-CRC ");
    if (st2 & UFT_EDSK_ST2_CM) pos += snprintf(buffer + pos, size - pos, "Deleted ");
    if (st2 & UFT_EDSK_ST2_MD) pos += snprintf(buffer + pos, size - pos, "No-DAM ");
    
    if (pos > 0 && buffer[pos - 1] == ' ') buffer[pos - 1] = '\0';
    
    return buffer;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Header Validation and Parsing
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Check if data is standard DSK format
 * @param data File data
 * @param size File size
 * @return true if standard DSK
 */
static inline bool uft_edsk_is_standard_dsk(const uint8_t* data, size_t size) {
    if (!data || size < UFT_EDSK_DISK_INFO_SIZE) return false;
    return (memcmp(data, UFT_DSK_SIGNATURE, UFT_DSK_SIGNATURE_LEN) == 0);
}

/**
 * @brief Check if data is extended DSK format
 * @param data File data
 * @param size File size
 * @return true if extended DSK
 */
static inline bool uft_edsk_is_extended_dsk(const uint8_t* data, size_t size) {
    if (!data || size < UFT_EDSK_DISK_INFO_SIZE) return false;
    return (memcmp(data, UFT_EDSK_SIGNATURE, UFT_EDSK_SIGNATURE_LEN) == 0);
}

/**
 * @brief Validate DSK/EDSK signature
 * @param data File data
 * @param size File size
 * @return true if valid signature
 */
static inline bool uft_edsk_validate_signature(const uint8_t* data, size_t size) {
    return uft_edsk_is_standard_dsk(data, size) || uft_edsk_is_extended_dsk(data, size);
}

/**
 * @brief Get track size from EDSK track size table
 * @param disk_info Disk information block
 * @param track Track number
 * @param side Side number
 * @return Track size in bytes
 */
static inline uint32_t uft_edsk_get_track_size(const uft_edsk_disk_info_t* disk_info,
                                               uint8_t track, uint8_t side) {
    if (!disk_info) return 0;
    
    uint8_t index = (track * disk_info->sides) + side;
    if (index >= 204) return 0;
    
    /* Track size is stored as MSB (multiply by 256) */
    return (uint32_t)disk_info->track_sizes[index] * 256;
}

/**
 * @brief Calculate track offset in file
 * @param disk_info Disk information block
 * @param track Track number
 * @param side Side number
 * @return Offset in bytes from start of file
 */
static inline size_t uft_edsk_calc_track_offset(const uft_edsk_disk_info_t* disk_info,
                                                uint8_t track, uint8_t side) {
    if (!disk_info) return 0;
    
    size_t offset = UFT_EDSK_DISK_INFO_SIZE;  /* Skip disk info */
    
    uint8_t target = (track * disk_info->sides) + side;
    
    for (uint8_t i = 0; i < target && i < 204; i++) {
        offset += (size_t)disk_info->track_sizes[i] * 256;
    }
    
    return offset;
}

/**
 * @brief Parse EDSK file into info structure
 * @param data File data
 * @param size File size
 * @param info Output info structure
 * @return true if successfully parsed
 */
static inline bool uft_edsk_parse(const uint8_t* data, size_t size,
                                  uft_edsk_info_t* info) {
    if (!data || !info || size < UFT_EDSK_DISK_INFO_SIZE) return false;
    if (!uft_edsk_validate_signature(data, size)) return false;
    
    memset(info, 0, sizeof(*info));
    
    const uft_edsk_disk_info_t* disk = (const uft_edsk_disk_info_t*)data;
    
    info->is_extended = uft_edsk_is_extended_dsk(data, size);
    info->tracks = disk->tracks;
    info->sides = disk->sides;
    
    /* Extract creator */
    memcpy(info->creator, disk->creator, 14);
    info->creator[14] = '\0';
    
    /* For standard DSK, use fixed track size */
    if (!info->is_extended) {
        info->track_size = disk->track_size;
    }
    
    /* Calculate total size and analyze tracks */
    size_t offset = UFT_EDSK_DISK_INFO_SIZE;
    
    for (uint8_t t = 0; t < disk->tracks; t++) {
        for (uint8_t s = 0; s < disk->sides; s++) {
            uint32_t track_size;
            
            if (info->is_extended) {
                uint8_t idx = (t * disk->sides) + s;
                track_size = (uint32_t)disk->track_sizes[idx] * 256;
            } else {
                track_size = disk->track_size;
            }
            
            if (track_size == 0) continue;  /* Empty track */
            
            if (offset + track_size > size) break;
            
            /* Parse track info */
            const uft_edsk_track_info_t* track_info = 
                (const uft_edsk_track_info_t*)(data + offset);
            
            if (memcmp(track_info->signature, UFT_EDSK_TRACK_SIGNATURE, 
                       UFT_EDSK_TRACK_SIG_LEN) == 0) {
                
                info->total_sectors += track_info->sector_count;
                
                /* Check sectors for special conditions */
                for (uint8_t sec = 0; sec < track_info->sector_count; sec++) {
                    const uft_edsk_sector_info_t* sect = &track_info->sectors[sec];
                    
                    if (info->is_extended && uft_edsk_is_weak_sector(sect)) {
                        info->has_weak_sectors = true;
                    }
                    if (uft_edsk_has_crc_error(sect->st1, sect->st2)) {
                        info->has_errors = true;
                    }
                    if (uft_edsk_is_deleted(sect->st2)) {
                        info->has_deleted = true;
                    }
                }
            }
            
            info->total_size += track_size;
            offset += track_size;
        }
    }
    
    /* Detect disk format */
    if (disk->tracks == 40 && disk->sides == 1) {
        info->format = UFT_EDSK_FMT_SYSTEM;
    } else if (disk->tracks == 41 && disk->sides == 1) {
        info->format = UFT_EDSK_FMT_DATA;
    } else if (disk->tracks == 80) {
        info->format = UFT_EDSK_FMT_PARADOS;
    } else {
        info->format = UFT_EDSK_FMT_CUSTOM;
    }
    
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Probe and Detection
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Probe data to determine if it's an EDSK/DSK file
 * @param data File data
 * @param size File size
 * @return Confidence score 0-100
 */
static inline int uft_edsk_probe(const uint8_t* data, size_t size) {
    if (!data || size < UFT_EDSK_DISK_INFO_SIZE) return 0;
    
    int score = 0;
    
    /* Check signature */
    if (uft_edsk_is_extended_dsk(data, size)) {
        score = 70;  /* Extended DSK has unique signature */
    } else if (uft_edsk_is_standard_dsk(data, size)) {
        score = 60;  /* Standard DSK signature */
    } else {
        return 0;
    }
    
    const uft_edsk_disk_info_t* disk = (const uft_edsk_disk_info_t*)data;
    
    /* Check reasonable track/side counts */
    if (disk->tracks > 0 && disk->tracks <= UFT_EDSK_MAX_TRACKS) {
        score += 10;
    }
    if (disk->sides >= 1 && disk->sides <= UFT_EDSK_MAX_SIDES) {
        score += 10;
    }
    
    /* Check for valid track info block */
    size_t track_offset = UFT_EDSK_DISK_INFO_SIZE;
    if (size >= track_offset + UFT_EDSK_TRACK_INFO_SIZE) {
        if (memcmp(data + track_offset, UFT_EDSK_TRACK_SIGNATURE, 
                   UFT_EDSK_TRACK_SIG_LEN) == 0) {
            score += 10;
        }
    }
    
    return (score > 100) ? 100 : score;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Creation Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize EDSK disk info block
 * @param disk Output disk info
 * @param creator Creator string
 * @param tracks Number of tracks
 * @param sides Number of sides
 */
static inline void uft_edsk_create_disk_info(uft_edsk_disk_info_t* disk,
                                             const char* creator,
                                             uint8_t tracks, uint8_t sides) {
    if (!disk) return;
    
    memset(disk, 0, sizeof(*disk));
    memcpy(disk->signature, UFT_EDSK_SIGNATURE, UFT_EDSK_SIGNATURE_LEN);
    
    if (creator) {
        size_t len = strlen(creator);
        if (len > 14) len = 14;
        memcpy(disk->creator, creator, len);
    }
    
    disk->tracks = tracks;
    disk->sides = sides;
}

/**
 * @brief Initialize EDSK track info block
 * @param track Output track info
 * @param track_num Track number
 * @param side Side number
 * @param sector_size Sector size code (N)
 * @param sector_count Number of sectors
 * @param gap3 GAP#3 length
 * @param filler Filler byte
 */
static inline void uft_edsk_create_track_info(uft_edsk_track_info_t* track,
                                              uint8_t track_num, uint8_t side,
                                              uint8_t sector_size, uint8_t sector_count,
                                              uint8_t gap3, uint8_t filler) {
    if (!track) return;
    
    memset(track, 0, sizeof(*track));
    memcpy(track->signature, UFT_EDSK_TRACK_SIGNATURE, UFT_EDSK_TRACK_SIG_LEN);
    
    track->track = track_num;
    track->side = side;
    track->sector_size = sector_size;
    track->sector_count = sector_count;
    track->gap3_length = gap3;
    track->filler_byte = filler;
}

/**
 * @brief Initialize sector info entry
 * @param sector Output sector info
 * @param c Cylinder (track)
 * @param h Head (side)
 * @param r Sector number
 * @param n Size code
 */
static inline void uft_edsk_create_sector_info(uft_edsk_sector_info_t* sector,
                                               uint8_t c, uint8_t h,
                                               uint8_t r, uint8_t n) {
    if (!sector) return;
    
    sector->track = c;
    sector->side = h;
    sector->sector = r;
    sector->size = n;
    sector->st1 = 0;
    sector->st2 = 0;
    sector->data_length = uft_edsk_size_to_bytes(n);
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_EDSK_FORMAT_H */
