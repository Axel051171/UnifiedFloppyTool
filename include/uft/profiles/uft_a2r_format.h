/**
 * @file uft_a2r_format.h
 * @brief A2R (Applesauce) format profile - Modern Apple II flux preservation format
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * A2R is the flux-level disk image format created for the Applesauce project.
 * It captures raw flux transitions for Apple II 5.25" and 3.5" disks with
 * precise timing information.
 *
 * Key features:
 * - Flux transition timing data
 * - Multiple capture passes per track
 * - Metadata and creator information
 * - Support for 5.25" and 3.5" disks
 *
 * Format specification: https://applesaucefdc.com/a2r/
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_A2R_FORMAT_H
#define UFT_A2R_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * A2R Format Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief A2R v2 signature "A2R2" */
#define UFT_A2R_SIGNATURE_V2        "A2R2"

/** @brief A2R v3 signature "A2R3" */
#define UFT_A2R_SIGNATURE_V3        "A2R3"

/** @brief A2R signature length */
#define UFT_A2R_SIGNATURE_LEN       4

/** @brief A2R header terminator (0xFF 0x0A) */
#define UFT_A2R_HEADER_TERM1        0xFF
#define UFT_A2R_HEADER_TERM2        0x0A

/** @brief A2R file header size */
#define UFT_A2R_HEADER_SIZE         8

/** @brief A2R chunk header size */
#define UFT_A2R_CHUNK_HEADER_SIZE   8

/** @brief Maximum tracks (35 tracks × 4 quarter tracks × 2 sides) */
#define UFT_A2R_MAX_TRACKS          280

/* ═══════════════════════════════════════════════════════════════════════════
 * A2R Chunk Types
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief INFO chunk identifier */
#define UFT_A2R_CHUNK_INFO          "INFO"

/** @brief STRM chunk identifier (v3) */
#define UFT_A2R_CHUNK_STRM          "STRM"

/** @brief META chunk identifier */
#define UFT_A2R_CHUNK_META          "META"

/** @brief RWCP chunk identifier (v2 raw capture) */
#define UFT_A2R_CHUNK_RWCP          "RWCP"

/** @brief SLVD chunk identifier (v2 solved) */
#define UFT_A2R_CHUNK_SLVD          "SLVD"

/* ═══════════════════════════════════════════════════════════════════════════
 * A2R Disk Types
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief A2R disk type identifiers
 */
typedef enum {
    UFT_A2R_DISK_525_SS      = 1,   /**< 5.25" single-sided */
    UFT_A2R_DISK_35_SS_400K  = 2,   /**< 3.5" single-sided 400K */
    UFT_A2R_DISK_35_DS_800K  = 3,   /**< 3.5" double-sided 800K */
    UFT_A2R_DISK_35_DS_1440K = 4    /**< 3.5" double-sided 1.44MB */
} uft_a2r_disk_type_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * A2R Capture Types
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief A2R capture type identifiers (v3)
 */
typedef enum {
    UFT_A2R_CAP_TIMING       = 1,   /**< Timing data capture */
    UFT_A2R_CAP_BITS         = 2,   /**< Bit data capture */
    UFT_A2R_CAP_XTIMING      = 3    /**< Extended timing capture */
} uft_a2r_capture_type_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * A2R Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief A2R file header (8 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t signature[4];           /**< "A2R2" or "A2R3" */
    uint8_t ff_byte;                /**< 0xFF byte */
    uint8_t lf_byte1;               /**< 0x0A (LF) */
    uint8_t lf_byte2;               /**< 0x0D (CR) */
    uint8_t lf_byte3;               /**< 0x0A (LF) */
} uft_a2r_header_t;
#pragma pack(pop)

/**
 * @brief A2R chunk header (8 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t chunk_id[4];            /**< Chunk type identifier */
    uint32_t chunk_size;            /**< Chunk data size (little-endian) */
} uft_a2r_chunk_header_t;
#pragma pack(pop)

/**
 * @brief A2R INFO chunk (v2, 36 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t version;                /**< INFO chunk version */
    uint8_t creator[32];            /**< Creator string (UTF-8) */
    uint8_t disk_type;              /**< Disk type */
    uint8_t write_protected;        /**< Write protected flag */
    uint8_t synchronized;           /**< Synchronized flag */
} uft_a2r_info_v2_t;
#pragma pack(pop)

/**
 * @brief A2R INFO chunk (v3, 52 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t version;                /**< INFO chunk version (3) */
    uint8_t creator[32];            /**< Creator string (UTF-8) */
    uint8_t disk_type;              /**< Disk type */
    uint8_t write_protected;        /**< Write protected flag */
    uint8_t synchronized;           /**< Synchronized flag */
    uint8_t hard_sector_count;      /**< Hard sector count (0=soft) */
    uint8_t require_ram;            /**< Requires 128K RAM */
    uint8_t largest_track;          /**< Largest track number */
    uint16_t flux_block;            /**< Flux block size (125ns units) */
    uint16_t bit_timing;            /**< Bit timing (125ns units) */
    uint16_t compatible_hardware;   /**< Compatible hardware flags */
    uint8_t ram_required;           /**< RAM required in KB */
    uint8_t largest_flux_track;     /**< Largest flux track number */
    uint8_t reserved[6];            /**< Reserved */
} uft_a2r_info_v3_t;
#pragma pack(pop)

/**
 * @brief A2R STRM chunk track header (v3, 10 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t location;               /**< Track location (quarter track) */
    uint8_t capture_type;           /**< Capture type */
    uint32_t data_size;             /**< Data size in bytes */
    uint32_t estimated_loop;        /**< Estimated loop point */
} uft_a2r_strm_track_t;
#pragma pack(pop)

/**
 * @brief A2R RWCP chunk header (v2)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t version;                /**< RWCP version */
    /* Track data follows */
} uft_a2r_rwcp_header_t;
#pragma pack(pop)

/**
 * @brief A2R RWCP track header (v2)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t track_number;           /**< Track number (0-159) */
    uint8_t capture_count;          /**< Number of captures */
    /* Capture data follows */
} uft_a2r_rwcp_track_t;
#pragma pack(pop)

/**
 * @brief Parsed A2R information
 */
typedef struct {
    uint8_t version;                /**< A2R format version (2 or 3) */
    char creator[33];               /**< Creator string (null-terminated) */
    uft_a2r_disk_type_t disk_type;  /**< Disk type */
    bool write_protected;           /**< Write protected */
    bool synchronized;              /**< Synchronized */
    uint8_t hard_sectors;           /**< Hard sector count */
    uint8_t largest_track;          /**< Largest track number */
    uint32_t track_count;           /**< Number of tracks */
    uint32_t total_captures;        /**< Total capture passes */
    bool has_timing;                /**< Has timing data */
    bool has_bits;                  /**< Has bit data */
    bool has_metadata;              /**< Has metadata chunk */
} uft_a2r_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Size Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_a2r_header_t) == 8, "A2R header must be 8 bytes");
_Static_assert(sizeof(uft_a2r_chunk_header_t) == 8, "A2R chunk header must be 8 bytes");
_Static_assert(sizeof(uft_a2r_info_v2_t) == 36, "A2R info v2 must be 36 bytes");
_Static_assert(sizeof(uft_a2r_strm_track_t) == 10, "A2R STRM track must be 10 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Inline Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get disk type name
 * @param type Disk type
 * @return Type description
 */
static inline const char* uft_a2r_disk_type_name(uint8_t type) {
    switch (type) {
        case UFT_A2R_DISK_525_SS:      return "5.25\" SS";
        case UFT_A2R_DISK_35_SS_400K:  return "3.5\" SS 400K";
        case UFT_A2R_DISK_35_DS_800K:  return "3.5\" DS 800K";
        case UFT_A2R_DISK_35_DS_1440K: return "3.5\" DS 1.44MB";
        default: return "Unknown";
    }
}

/**
 * @brief Get capture type name
 * @param type Capture type
 * @return Type description
 */
static inline const char* uft_a2r_capture_type_name(uint8_t type) {
    switch (type) {
        case UFT_A2R_CAP_TIMING:  return "Timing";
        case UFT_A2R_CAP_BITS:    return "Bits";
        case UFT_A2R_CAP_XTIMING: return "Extended Timing";
        default: return "Unknown";
    }
}

/**
 * @brief Convert quarter-track location to track number
 * @param location Quarter-track location
 * @return Track number (floating point for half/quarter tracks)
 */
static inline float uft_a2r_location_to_track(uint8_t location) {
    return location / 4.0f;
}

/**
 * @brief Convert track number to quarter-track location
 * @param track Track number
 * @param quarter Quarter offset (0-3)
 * @return Quarter-track location
 */
static inline uint8_t uft_a2r_track_to_location(uint8_t track, uint8_t quarter) {
    return (track * 4) + (quarter & 3);
}

/**
 * @brief Get standard track count for disk type
 * @param type Disk type
 * @return Expected track count
 */
static inline uint8_t uft_a2r_standard_tracks(uint8_t type) {
    switch (type) {
        case UFT_A2R_DISK_525_SS:      return 35;  /* Or 40 for DOS 3.3+ */
        case UFT_A2R_DISK_35_SS_400K:  return 80;
        case UFT_A2R_DISK_35_DS_800K:  return 80;
        case UFT_A2R_DISK_35_DS_1440K: return 80;
        default: return 35;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Header Validation and Parsing
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Check if A2R v2 signature
 * @param data File data
 * @param size File size
 * @return true if A2R v2
 */
static inline bool uft_a2r_is_v2(const uint8_t* data, size_t size) {
    if (!data || size < UFT_A2R_HEADER_SIZE) return false;
    return (memcmp(data, UFT_A2R_SIGNATURE_V2, UFT_A2R_SIGNATURE_LEN) == 0);
}

/**
 * @brief Check if A2R v3 signature
 * @param data File data
 * @param size File size
 * @return true if A2R v3
 */
static inline bool uft_a2r_is_v3(const uint8_t* data, size_t size) {
    if (!data || size < UFT_A2R_HEADER_SIZE) return false;
    return (memcmp(data, UFT_A2R_SIGNATURE_V3, UFT_A2R_SIGNATURE_LEN) == 0);
}

/**
 * @brief Validate A2R signature
 * @param data File data
 * @param size File size
 * @return true if valid A2R signature
 */
static inline bool uft_a2r_validate_signature(const uint8_t* data, size_t size) {
    if (!data || size < UFT_A2R_HEADER_SIZE) return false;
    
    /* Check signature */
    if (!uft_a2r_is_v2(data, size) && !uft_a2r_is_v3(data, size)) {
        return false;
    }
    
    /* Check header terminators */
    if (data[4] != 0xFF || data[5] != 0x0A) {
        return false;
    }
    
    return true;
}

/**
 * @brief Parse A2R file into info structure
 * @param data File data
 * @param size File size
 * @param info Output info structure
 * @return true if successfully parsed
 */
static inline bool uft_a2r_parse(const uint8_t* data, size_t size,
                                 uft_a2r_info_t* info) {
    if (!data || !info || !uft_a2r_validate_signature(data, size)) return false;
    
    memset(info, 0, sizeof(*info));
    
    /* Determine version */
    info->version = uft_a2r_is_v3(data, size) ? 3 : 2;
    
    /* Parse chunks */
    size_t offset = UFT_A2R_HEADER_SIZE;
    
    while (offset + UFT_A2R_CHUNK_HEADER_SIZE <= size) {
        const uft_a2r_chunk_header_t* chunk = 
            (const uft_a2r_chunk_header_t*)(data + offset);
        
        uint32_t chunk_size = chunk->chunk_size;
        if (offset + UFT_A2R_CHUNK_HEADER_SIZE + chunk_size > size) break;
        
        const uint8_t* chunk_data = data + offset + UFT_A2R_CHUNK_HEADER_SIZE;
        
        if (memcmp(chunk->chunk_id, UFT_A2R_CHUNK_INFO, 4) == 0) {
            /* Parse INFO chunk */
            if (info->version == 3 && chunk_size >= 52) {
                const uft_a2r_info_v3_t* inf = (const uft_a2r_info_v3_t*)chunk_data;
                memcpy(info->creator, inf->creator, 32);
                info->creator[32] = '\0';
                info->disk_type = (uft_a2r_disk_type_t)inf->disk_type;
                info->write_protected = (inf->write_protected != 0);
                info->synchronized = (inf->synchronized != 0);
                info->hard_sectors = inf->hard_sector_count;
                info->largest_track = inf->largest_track;
            } else if (chunk_size >= 36) {
                const uft_a2r_info_v2_t* inf = (const uft_a2r_info_v2_t*)chunk_data;
                memcpy(info->creator, inf->creator, 32);
                info->creator[32] = '\0';
                info->disk_type = (uft_a2r_disk_type_t)inf->disk_type;
                info->write_protected = (inf->write_protected != 0);
                info->synchronized = (inf->synchronized != 0);
            }
        } else if (memcmp(chunk->chunk_id, UFT_A2R_CHUNK_STRM, 4) == 0) {
            /* Parse STRM chunk (v3) - count tracks */
            size_t pos = 0;
            while (pos + sizeof(uft_a2r_strm_track_t) <= chunk_size) {
                const uft_a2r_strm_track_t* track = 
                    (const uft_a2r_strm_track_t*)(chunk_data + pos);
                
                info->track_count++;
                
                if (track->capture_type == UFT_A2R_CAP_TIMING ||
                    track->capture_type == UFT_A2R_CAP_XTIMING) {
                    info->has_timing = true;
                }
                if (track->capture_type == UFT_A2R_CAP_BITS) {
                    info->has_bits = true;
                }
                
                pos += sizeof(uft_a2r_strm_track_t) + track->data_size;
            }
        } else if (memcmp(chunk->chunk_id, UFT_A2R_CHUNK_RWCP, 4) == 0) {
            /* Parse RWCP chunk (v2) */
            info->has_timing = true;
            /* Count tracks/captures */
            size_t pos = 1;  /* Skip version byte */
            while (pos + 2 <= chunk_size) {
                uint8_t captures = chunk_data[pos + 1];
                info->track_count++;
                info->total_captures += captures;
                
                /* Skip capture data */
                pos += 2;
                for (uint8_t c = 0; c < captures && pos + 4 <= chunk_size; c++) {
                    uint32_t cap_size = *(const uint32_t*)(chunk_data + pos);
                    pos += 4 + cap_size;
                }
            }
        } else if (memcmp(chunk->chunk_id, UFT_A2R_CHUNK_META, 4) == 0) {
            info->has_metadata = true;
        }
        
        offset += UFT_A2R_CHUNK_HEADER_SIZE + chunk_size;
    }
    
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Probe and Detection
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Probe data to determine if it's an A2R file
 * @param data File data
 * @param size File size
 * @return Confidence score 0-100
 */
static inline int uft_a2r_probe(const uint8_t* data, size_t size) {
    if (!data || size < UFT_A2R_HEADER_SIZE) return 0;
    
    /* Check signature */
    if (!uft_a2r_validate_signature(data, size)) return 0;
    
    int score = 70;  /* Base score for valid signature */
    
    /* Check header terminators */
    if (data[6] == 0x0D && data[7] == 0x0A) {
        score += 10;
    }
    
    /* Look for INFO chunk */
    if (size >= UFT_A2R_HEADER_SIZE + UFT_A2R_CHUNK_HEADER_SIZE) {
        if (memcmp(data + UFT_A2R_HEADER_SIZE, UFT_A2R_CHUNK_INFO, 4) == 0) {
            score += 20;
        }
    }
    
    return (score > 100) ? 100 : score;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Creation Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize A2R v3 header
 * @param header Output header
 */
static inline void uft_a2r_create_header_v3(uft_a2r_header_t* header) {
    if (!header) return;
    
    memcpy(header->signature, UFT_A2R_SIGNATURE_V3, 4);
    header->ff_byte = 0xFF;
    header->lf_byte1 = 0x0A;
    header->lf_byte2 = 0x0D;
    header->lf_byte3 = 0x0A;
}

/**
 * @brief Initialize A2R v2 header
 * @param header Output header
 */
static inline void uft_a2r_create_header_v2(uft_a2r_header_t* header) {
    if (!header) return;
    
    memcpy(header->signature, UFT_A2R_SIGNATURE_V2, 4);
    header->ff_byte = 0xFF;
    header->lf_byte1 = 0x0A;
    header->lf_byte2 = 0x0D;
    header->lf_byte3 = 0x0A;
}

/**
 * @brief Initialize chunk header
 * @param chunk Output chunk header
 * @param id Chunk ID (4 chars)
 * @param size Chunk data size
 */
static inline void uft_a2r_create_chunk_header(uft_a2r_chunk_header_t* chunk,
                                               const char* id, uint32_t size) {
    if (!chunk || !id) return;
    
    memcpy(chunk->chunk_id, id, 4);
    chunk->chunk_size = size;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_A2R_FORMAT_H */
