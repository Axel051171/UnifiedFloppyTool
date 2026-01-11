/**
 * @file uft_g64_format.h
 * @brief G64 format profile - Commodore 64/1541 GCR preservation format
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * G64 is the native GCR (Group Code Recording) disk image format for
 * Commodore 64/1541 disk preservation. Unlike D64, G64 preserves the raw
 * GCR-encoded track data including timing information, making it suitable
 * for copy-protected disks.
 *
 * Key features:
 * - Raw GCR track data (no decoding)
 * - Per-track speed zone information
 * - Variable track length support
 * - Supports 35-42 tracks
 *
 * Format specification: https://vice-emu.sourceforge.io/vice_17.html#SEC330
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_G64_FORMAT_H
#define UFT_G64_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * G64 Format Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief G64 signature "GCR-1541" */
#define UFT_G64_SIGNATURE           "GCR-1541"

/** @brief G64 signature length */
#define UFT_G64_SIGNATURE_LEN       8

/** @brief G64 header size */
#define UFT_G64_HEADER_SIZE         12

/** @brief Maximum tracks in G64 file */
#define UFT_G64_MAX_TRACKS          84

/** @brief Standard track count (35 tracks × 2 halftracks) */
#define UFT_G64_STD_TRACKS          70

/** @brief Extended track count (42 tracks × 2 halftracks) */
#define UFT_G64_EXT_TRACKS          84

/** @brief Maximum track size in bytes */
#define UFT_G64_MAX_TRACK_SIZE      7928

/** @brief Standard maximum track size */
#define UFT_G64_STD_MAX_TRACK       7692

/* ═══════════════════════════════════════════════════════════════════════════
 * 1541 Speed Zone Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief 1541 disk speed zones
 * 
 * The 1541 uses variable speed zones to maintain constant bit density:
 * - Zone 3: Tracks 1-17   (21 sectors, ~7692 GCR bytes, 300 RPM @ 4 µs)
 * - Zone 2: Tracks 18-24  (19 sectors, ~7142 GCR bytes, 300 RPM @ 4 µs)
 * - Zone 1: Tracks 25-30  (18 sectors, ~6666 GCR bytes, 300 RPM @ 4 µs)
 * - Zone 0: Tracks 31-35+ (17 sectors, ~6250 GCR bytes, 300 RPM @ 4 µs)
 */
typedef enum {
    UFT_G64_ZONE_0 = 0,     /**< Slowest zone (tracks 31-35+) */
    UFT_G64_ZONE_1 = 1,     /**< Zone 1 (tracks 25-30) */
    UFT_G64_ZONE_2 = 2,     /**< Zone 2 (tracks 18-24) */
    UFT_G64_ZONE_3 = 3      /**< Fastest zone (tracks 1-17) */
} uft_g64_speed_zone_t;

/**
 * @brief GCR bytes per track for each speed zone
 */
static const uint16_t UFT_G64_ZONE_TRACK_SIZE[] = {
    6250,   /* Zone 0: 17 sectors */
    6666,   /* Zone 1: 18 sectors */
    7142,   /* Zone 2: 19 sectors */
    7692    /* Zone 3: 21 sectors */
};

/**
 * @brief Sectors per track for each speed zone
 */
static const uint8_t UFT_G64_ZONE_SECTORS[] = {
    17,     /* Zone 0: tracks 31-35+ */
    18,     /* Zone 1: tracks 25-30 */
    19,     /* Zone 2: tracks 18-24 */
    21      /* Zone 3: tracks 1-17 */
};

/**
 * @brief Bit rate for each speed zone (bits per second)
 */
static const uint32_t UFT_G64_ZONE_BITRATE[] = {
    250000, /* Zone 0 */
    266667, /* Zone 1 */
    285714, /* Zone 2 */
    307692  /* Zone 3 */
};

/* ═══════════════════════════════════════════════════════════════════════════
 * GCR Encoding Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief GCR sync mark (10 consecutive 1 bits) */
#define UFT_G64_SYNC_MARK           0xFF

/** @brief Number of sync bytes before each sector */
#define UFT_G64_SYNC_LENGTH         5

/** @brief GCR header block ID */
#define UFT_G64_HEADER_ID           0x08

/** @brief GCR data block ID */
#define UFT_G64_DATA_ID             0x07

/** @brief GCR block header size (decoded) */
#define UFT_G64_BLOCK_HEADER_SIZE   8

/** @brief GCR data block size (decoded) */
#define UFT_G64_BLOCK_DATA_SIZE     260

/** @brief GCR encoded header size */
#define UFT_G64_GCR_HEADER_SIZE     10

/** @brief GCR encoded data block size */
#define UFT_G64_GCR_DATA_SIZE       325

/** @brief Gap bytes between sectors */
#define UFT_G64_GAP_BYTE            0x55

/* ═══════════════════════════════════════════════════════════════════════════
 * G64 Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief G64 file header (12 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t signature[8];       /**< "GCR-1541" signature */
    uint8_t version;            /**< Format version (0) */
    uint8_t track_count;        /**< Number of tracks × 2 (half-tracks) */
    uint16_t track_size;        /**< Maximum track size in bytes */
} uft_g64_header_t;
#pragma pack(pop)

/**
 * @brief G64 track table entry structure
 * 
 * Following the header are two tables:
 * 1. Track offset table: track_count × uint32_t (offset to track data)
 * 2. Speed zone table: track_count × uint32_t (speed zone for track)
 */
typedef struct {
    uint32_t* track_offsets;    /**< Array of track data offsets */
    uint32_t* speed_zones;      /**< Array of speed zones */
    uint8_t track_count;        /**< Number of entries */
} uft_g64_track_table_t;

/**
 * @brief G64 track data structure
 * 
 * Each track starts with a 2-byte length followed by GCR data
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t length;            /**< Track data length in bytes */
    uint8_t data[];             /**< GCR track data (variable length) */
} uft_g64_track_data_t;
#pragma pack(pop)

/**
 * @brief Parsed G64 file information
 */
typedef struct {
    uint8_t version;            /**< Format version */
    uint8_t track_count;        /**< Number of half-tracks */
    uint16_t max_track_size;    /**< Maximum track size */
    uint32_t file_size;         /**< Total file size */
    uint8_t actual_tracks;      /**< Number of non-empty tracks */
    bool has_half_tracks;       /**< Contains half-track data */
    bool is_extended;           /**< Extended format (>35 tracks) */
} uft_g64_info_t;

/**
 * @brief Standard D64 geometry for reference
 */
typedef struct {
    uint8_t track;              /**< Track number (1-based) */
    uint8_t sectors;            /**< Sectors on this track */
    uint8_t speed_zone;         /**< Speed zone */
    uint16_t gcr_size;          /**< GCR data size */
} uft_g64_track_geometry_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Size Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_g64_header_t) == 12, "G64 header must be 12 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * GCR Encoding/Decoding Tables
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief GCR encoding table (4-bit nibble to 5-bit GCR)
 */
static const uint8_t UFT_G64_GCR_ENCODE[] = {
    0x0A, 0x0B, 0x12, 0x13,  /* 0-3 */
    0x0E, 0x0F, 0x16, 0x17,  /* 4-7 */
    0x09, 0x19, 0x1A, 0x1B,  /* 8-11 */
    0x0D, 0x1D, 0x1E, 0x15   /* 12-15 */
};

/**
 * @brief GCR decoding table (5-bit GCR to 4-bit nibble, 0xFF = invalid)
 */
static const uint8_t UFT_G64_GCR_DECODE[] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x00-0x07 */
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,  /* 0x08-0x0F */
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,  /* 0x10-0x17 */
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF   /* 0x18-0x1F */
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Inline Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get speed zone for a given track number
 * @param track Track number (1-based)
 * @return Speed zone (0-3)
 */
static inline uint8_t uft_g64_track_speed_zone(uint8_t track) {
    if (track == 0) return UFT_G64_ZONE_0;
    if (track <= 17) return UFT_G64_ZONE_3;
    if (track <= 24) return UFT_G64_ZONE_2;
    if (track <= 30) return UFT_G64_ZONE_1;
    return UFT_G64_ZONE_0;
}

/**
 * @brief Get sectors per track for a given track number
 * @param track Track number (1-based)
 * @return Number of sectors on track
 */
static inline uint8_t uft_g64_track_sectors(uint8_t track) {
    return UFT_G64_ZONE_SECTORS[uft_g64_track_speed_zone(track)];
}

/**
 * @brief Get expected GCR track size for a given track number
 * @param track Track number (1-based)
 * @return Expected GCR track size in bytes
 */
static inline uint16_t uft_g64_track_gcr_size(uint8_t track) {
    return UFT_G64_ZONE_TRACK_SIZE[uft_g64_track_speed_zone(track)];
}

/**
 * @brief Convert track number to half-track index
 * @param track Track number (1-based)
 * @param half Half track (0 or 1)
 * @return Half-track index (0-based)
 */
static inline uint8_t uft_g64_halftrack_index(uint8_t track, uint8_t half) {
    return ((track - 1) * 2) + (half ? 1 : 0);
}

/**
 * @brief Convert half-track index to track number
 * @param index Half-track index (0-based)
 * @return Track number (1-based)
 */
static inline uint8_t uft_g64_index_to_track(uint8_t index) {
    return (index / 2) + 1;
}

/**
 * @brief Check if half-track index is a half track (odd index)
 * @param index Half-track index
 * @return true if half track
 */
static inline bool uft_g64_is_half_track(uint8_t index) {
    return (index & 1) != 0;
}

/**
 * @brief Encode a nibble to GCR
 * @param nibble 4-bit value (0-15)
 * @return 5-bit GCR code
 */
static inline uint8_t uft_g64_gcr_encode_nibble(uint8_t nibble) {
    return UFT_G64_GCR_ENCODE[nibble & 0x0F];
}

/**
 * @brief Decode a GCR code to nibble
 * @param gcr 5-bit GCR code
 * @return 4-bit value, or 0xFF if invalid
 */
static inline uint8_t uft_g64_gcr_decode_nibble(uint8_t gcr) {
    return (gcr < 32) ? UFT_G64_GCR_DECODE[gcr] : 0xFF;
}

/**
 * @brief Check if GCR code is valid
 * @param gcr 5-bit GCR code
 * @return true if valid
 */
static inline bool uft_g64_gcr_is_valid(uint8_t gcr) {
    return (gcr < 32) && (UFT_G64_GCR_DECODE[gcr] != 0xFF);
}

/**
 * @brief Get speed zone name
 * @param zone Speed zone (0-3)
 * @return Zone description
 */
static inline const char* uft_g64_zone_name(uint8_t zone) {
    switch (zone) {
        case UFT_G64_ZONE_0: return "Zone 0 (slowest)";
        case UFT_G64_ZONE_1: return "Zone 1";
        case UFT_G64_ZONE_2: return "Zone 2";
        case UFT_G64_ZONE_3: return "Zone 3 (fastest)";
        default: return "Unknown";
    }
}

/**
 * @brief Calculate offset to track table in G64 file
 * @return Offset in bytes
 */
static inline size_t uft_g64_track_table_offset(void) {
    return UFT_G64_HEADER_SIZE;
}

/**
 * @brief Calculate offset to speed zone table in G64 file
 * @param track_count Number of tracks
 * @return Offset in bytes
 */
static inline size_t uft_g64_speed_table_offset(uint8_t track_count) {
    return UFT_G64_HEADER_SIZE + (track_count * sizeof(uint32_t));
}

/**
 * @brief Calculate minimum file size for given track count
 * @param track_count Number of tracks
 * @return Minimum file size in bytes
 */
static inline size_t uft_g64_min_file_size(uint8_t track_count) {
    /* Header + track offsets + speed zones */
    return UFT_G64_HEADER_SIZE + (track_count * sizeof(uint32_t) * 2);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Header Validation and Parsing
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Validate G64 file signature
 * @param data Pointer to file data
 * @param size Size of data buffer
 * @return true if valid G64 signature
 */
static inline bool uft_g64_validate_signature(const uint8_t* data, size_t size) {
    if (!data || size < UFT_G64_SIGNATURE_LEN) return false;
    return (memcmp(data, UFT_G64_SIGNATURE, UFT_G64_SIGNATURE_LEN) == 0);
}

/**
 * @brief Validate G64 header
 * @param header Pointer to G64 header
 * @return true if header is valid
 */
static inline bool uft_g64_validate_header(const uft_g64_header_t* header) {
    if (!header) return false;
    
    /* Check signature */
    if (memcmp(header->signature, UFT_G64_SIGNATURE, UFT_G64_SIGNATURE_LEN) != 0) {
        return false;
    }
    
    /* Check version */
    if (header->version != 0) {
        return false;  /* Only version 0 is defined */
    }
    
    /* Check track count */
    if (header->track_count == 0 || header->track_count > UFT_G64_MAX_TRACKS) {
        return false;
    }
    
    /* Check max track size */
    if (header->track_size > UFT_G64_MAX_TRACK_SIZE) {
        return false;
    }
    
    return true;
}

/**
 * @brief Parse G64 header into info structure
 * @param data Pointer to file data
 * @param size Size of data buffer
 * @param info Output info structure
 * @return true if successfully parsed
 */
static inline bool uft_g64_parse_header(const uint8_t* data, size_t size,
                                        uft_g64_info_t* info) {
    if (!data || !info || size < UFT_G64_HEADER_SIZE) return false;
    
    const uft_g64_header_t* header = (const uft_g64_header_t*)data;
    if (!uft_g64_validate_header(header)) return false;
    
    memset(info, 0, sizeof(*info));
    
    info->version = header->version;
    info->track_count = header->track_count;
    info->max_track_size = header->track_size;
    info->file_size = size;
    
    /* Check if extended format */
    info->is_extended = (header->track_count > UFT_G64_STD_TRACKS);
    
    /* Count actual tracks and check for half-tracks */
    size_t min_size = uft_g64_min_file_size(header->track_count);
    if (size >= min_size) {
        const uint32_t* offsets = (const uint32_t*)(data + UFT_G64_HEADER_SIZE);
        
        for (uint8_t i = 0; i < header->track_count; i++) {
            if (offsets[i] != 0) {
                info->actual_tracks++;
                if (uft_g64_is_half_track(i)) {
                    info->has_half_tracks = true;
                }
            }
        }
    }
    
    return true;
}

/**
 * @brief Get track data offset from G64 file
 * @param data File data
 * @param size File size
 * @param half_track Half-track index (0-based)
 * @return Offset to track data, or 0 if not present
 */
static inline uint32_t uft_g64_get_track_offset(const uint8_t* data, size_t size,
                                                uint8_t half_track) {
    if (!data || size < UFT_G64_HEADER_SIZE) return 0;
    
    const uft_g64_header_t* header = (const uft_g64_header_t*)data;
    if (half_track >= header->track_count) return 0;
    
    size_t offset_pos = UFT_G64_HEADER_SIZE + (half_track * sizeof(uint32_t));
    if (offset_pos + sizeof(uint32_t) > size) return 0;
    
    uint32_t offset;
    memcpy(&offset, data + offset_pos, sizeof(offset));
    return offset;
}

/**
 * @brief Get track speed zone from G64 file
 * @param data File data
 * @param size File size
 * @param half_track Half-track index (0-based)
 * @return Speed zone, or 0xFF on error
 */
static inline uint8_t uft_g64_get_track_speed(const uint8_t* data, size_t size,
                                              uint8_t half_track) {
    if (!data || size < UFT_G64_HEADER_SIZE) return 0xFF;
    
    const uft_g64_header_t* header = (const uft_g64_header_t*)data;
    if (half_track >= header->track_count) return 0xFF;
    
    size_t speed_pos = uft_g64_speed_table_offset(header->track_count) +
                       (half_track * sizeof(uint32_t));
    if (speed_pos + sizeof(uint32_t) > size) return 0xFF;
    
    uint32_t speed;
    memcpy(&speed, data + speed_pos, sizeof(speed));
    return speed & 0x03;  /* Only lower 2 bits used */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Probe and Detection
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Probe data to determine if it's a G64 file
 * @param data Pointer to file data
 * @param size Size of data buffer
 * @return Confidence score 0-100 (0 = not G64, 100 = definitely G64)
 */
static inline int uft_g64_probe(const uint8_t* data, size_t size) {
    if (!data || size < UFT_G64_HEADER_SIZE) return 0;
    
    /* Check signature */
    if (!uft_g64_validate_signature(data, size)) return 0;
    
    int score = 60;  /* Base score for signature match */
    
    const uft_g64_header_t* header = (const uft_g64_header_t*)data;
    
    /* Check version */
    if (header->version == 0) {
        score += 15;
    }
    
    /* Check track count is reasonable */
    if (header->track_count >= 70 && header->track_count <= 84) {
        score += 10;
    }
    
    /* Check max track size is reasonable */
    if (header->track_size <= UFT_G64_MAX_TRACK_SIZE &&
        header->track_size >= 6000) {
        score += 10;
    }
    
    /* Check file size is sufficient */
    if (size >= uft_g64_min_file_size(header->track_count)) {
        score += 5;
    }
    
    return (score > 100) ? 100 : score;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Creation Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize a G64 header
 * @param header Output header structure
 * @param track_count Number of half-tracks (typically 84)
 * @param max_track_size Maximum track size (typically 7928)
 */
static inline void uft_g64_create_header(uft_g64_header_t* header,
                                         uint8_t track_count,
                                         uint16_t max_track_size) {
    if (!header) return;
    
    memset(header, 0, sizeof(*header));
    memcpy(header->signature, UFT_G64_SIGNATURE, UFT_G64_SIGNATURE_LEN);
    header->version = 0;
    header->track_count = track_count;
    header->track_size = max_track_size;
}

/**
 * @brief Create standard 35-track G64 header
 * @param header Output header structure
 */
static inline void uft_g64_create_standard_header(uft_g64_header_t* header) {
    uft_g64_create_header(header, UFT_G64_STD_TRACKS, UFT_G64_STD_MAX_TRACK);
}

/**
 * @brief Create extended 42-track G64 header
 * @param header Output header structure
 */
static inline void uft_g64_create_extended_header(uft_g64_header_t* header) {
    uft_g64_create_header(header, UFT_G64_EXT_TRACKS, UFT_G64_MAX_TRACK_SIZE);
}

/**
 * @brief Calculate total D64 file size for track count
 * @param tracks Number of tracks (35, 40, or 42)
 * @return D64 file size in bytes
 */
static inline uint32_t uft_g64_d64_size(uint8_t tracks) {
    uint32_t size = 0;
    for (uint8_t t = 1; t <= tracks; t++) {
        size += uft_g64_track_sectors(t) * 256;
    }
    return size;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_G64_FORMAT_H */
