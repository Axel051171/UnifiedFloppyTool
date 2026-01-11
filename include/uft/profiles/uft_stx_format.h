/**
 * @file uft_stx_format.h
 * @brief STX (Pasti) format profile - Atari ST copy-protected disk format
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * STX (Pasti) is an advanced disk image format for Atari ST that captures
 * low-level track information including timing, weak bits, and fuzzy sectors.
 * It's essential for preserving copy-protected Atari ST software.
 *
 * Key features:
 * - Precise timing information
 * - Fuzzy/weak bit support
 * - Multiple sector copies
 * - Track timing profiles
 *
 * Format specification: http://info-coach.fr/atari/documents/_mydoc/Pasti-documentation.pdf
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_STX_FORMAT_H
#define UFT_STX_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * STX Format Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief STX signature "RSY\0" */
#define UFT_STX_SIGNATURE           "RSY"

/** @brief STX signature length */
#define UFT_STX_SIGNATURE_LEN       3

/** @brief STX file header size */
#define UFT_STX_HEADER_SIZE         16

/** @brief STX track header size */
#define UFT_STX_TRACK_HEADER_SIZE   16

/** @brief STX sector header size */
#define UFT_STX_SECTOR_HEADER_SIZE  16

/** @brief Maximum tracks in STX */
#define UFT_STX_MAX_TRACKS          86

/** @brief Maximum sectors per track */
#define UFT_STX_MAX_SECTORS         26

/* ═══════════════════════════════════════════════════════════════════════════
 * STX Version Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief STX version 3.0 (final) */
#define UFT_STX_VERSION_3           3

/* ═══════════════════════════════════════════════════════════════════════════
 * STX Track Flags
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief STX track flag bits
 */
#define UFT_STX_TRK_SECT_DESC       0x0001  /**< Track has sector descriptors */
#define UFT_STX_TRK_TIMING          0x0002  /**< Track has timing descriptor */
#define UFT_STX_TRK_TRACK_IMAGE     0x0040  /**< Track image follows */
#define UFT_STX_TRK_TRACK_IMAGE_SYNC 0x0080 /**< Track image has sync offset */

/* ═══════════════════════════════════════════════════════════════════════════
 * STX Sector Flags
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief STX sector flag bits
 */
#define UFT_STX_SECT_RNF            0x01    /**< Record Not Found */
#define UFT_STX_SECT_CRC_ERR        0x02    /**< CRC Error (ID or data) */
#define UFT_STX_SECT_DELETED        0x20    /**< Deleted Data Mark */
#define UFT_STX_SECT_FUZZY          0x80    /**< Fuzzy bits present */

/* ═══════════════════════════════════════════════════════════════════════════
 * Atari ST Disk Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Atari ST standard sector size */
#define UFT_STX_SECTOR_SIZE         512

/** @brief Atari ST tracks per side */
#define UFT_STX_TRACKS_PER_SIDE     80

/** @brief Atari ST sides */
#define UFT_STX_SIDES               2

/** @brief DD sectors per track (9 sectors) */
#define UFT_STX_SECTORS_DD          9

/** @brief HD sectors per track (18 sectors) */
#define UFT_STX_SECTORS_HD          18

/** @brief ED sectors per track (36 sectors) */
#define UFT_STX_SECTORS_ED          36

/* ═══════════════════════════════════════════════════════════════════════════
 * STX Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief STX file header (16 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t signature[4];           /**< "RSY\0" */
    uint16_t version;               /**< Format version */
    uint16_t tool;                  /**< Tool ID */
    uint16_t reserved1;             /**< Reserved */
    uint8_t track_count;            /**< Number of tracks */
    uint8_t revision;               /**< Revision number */
    uint32_t reserved2;             /**< Reserved */
} uft_stx_header_t;
#pragma pack(pop)

/**
 * @brief STX track descriptor (16 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t size;                  /**< Track record size */
    uint32_t fuzzy_count;           /**< Number of fuzzy bytes */
    uint16_t sector_count;          /**< Number of sectors */
    uint16_t flags;                 /**< Track flags */
    uint16_t track_length;          /**< MFM track length */
    uint8_t track_number;           /**< Track number */
    uint8_t track_type;             /**< Track type */
} uft_stx_track_desc_t;
#pragma pack(pop)

/**
 * @brief STX sector descriptor (16 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t data_offset;           /**< Offset to sector data */
    uint16_t bit_position;          /**< Bit position in track */
    uint16_t read_time;             /**< Read time (timing) */
    uint8_t track;                  /**< ID field: track (C) */
    uint8_t head;                   /**< ID field: head (H) */
    uint8_t sector;                 /**< ID field: sector (R) */
    uint8_t size;                   /**< ID field: size (N) */
    uint8_t crc[2];                 /**< ID CRC bytes */
    uint8_t fdc_status;             /**< FDC status flags */
    uint8_t reserved;               /**< Reserved */
} uft_stx_sector_desc_t;
#pragma pack(pop)

/**
 * @brief STX timing descriptor
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t flags;                 /**< Timing flags */
    uint16_t size;                  /**< Size of timing data */
    /* Timing data follows */
} uft_stx_timing_desc_t;
#pragma pack(pop)

/**
 * @brief Parsed STX information
 */
typedef struct {
    uint16_t version;               /**< Format version */
    uint8_t track_count;            /**< Number of tracks */
    uint8_t revision;               /**< Revision */
    uint32_t total_sectors;         /**< Total sector count */
    uint32_t fuzzy_bytes;           /**< Total fuzzy byte count */
    bool has_timing;                /**< Has timing information */
    bool has_fuzzy;                 /**< Has fuzzy sectors */
    bool has_errors;                /**< Has error sectors */
    bool has_deleted;               /**< Has deleted sectors */
    uint8_t sides;                  /**< Detected sides */
    uint8_t sectors_per_track;      /**< Typical sectors per track */
} uft_stx_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Size Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_stx_header_t) == 16, "STX header must be 16 bytes");
_Static_assert(sizeof(uft_stx_track_desc_t) == 16, "STX track desc must be 16 bytes");
_Static_assert(sizeof(uft_stx_sector_desc_t) == 16, "STX sector desc must be 16 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Inline Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Convert sector size code to bytes
 * @param size_code Size code (N value)
 * @return Size in bytes
 */
static inline uint32_t uft_stx_size_to_bytes(uint8_t size_code) {
    if (size_code > 6) return 0;
    return 128U << size_code;
}

/**
 * @brief Check if sector has fuzzy bits
 * @param status FDC status
 * @return true if fuzzy
 */
static inline bool uft_stx_is_fuzzy(uint8_t status) {
    return (status & UFT_STX_SECT_FUZZY) != 0;
}

/**
 * @brief Check if sector has CRC error
 * @param status FDC status
 * @return true if CRC error
 */
static inline bool uft_stx_has_crc_error(uint8_t status) {
    return (status & UFT_STX_SECT_CRC_ERR) != 0;
}

/**
 * @brief Check if sector is deleted
 * @param status FDC status
 * @return true if deleted
 */
static inline bool uft_stx_is_deleted(uint8_t status) {
    return (status & UFT_STX_SECT_DELETED) != 0;
}

/**
 * @brief Check if sector was not found
 * @param status FDC status
 * @return true if RNF
 */
static inline bool uft_stx_is_rnf(uint8_t status) {
    return (status & UFT_STX_SECT_RNF) != 0;
}

/**
 * @brief Check if track has sector descriptors
 * @param flags Track flags
 * @return true if has descriptors
 */
static inline bool uft_stx_track_has_sectors(uint16_t flags) {
    return (flags & UFT_STX_TRK_SECT_DESC) != 0;
}

/**
 * @brief Check if track has timing data
 * @param flags Track flags
 * @return true if has timing
 */
static inline bool uft_stx_track_has_timing(uint16_t flags) {
    return (flags & UFT_STX_TRK_TIMING) != 0;
}

/**
 * @brief Check if track has image data
 * @param flags Track flags
 * @return true if has image
 */
static inline bool uft_stx_track_has_image(uint16_t flags) {
    return (flags & UFT_STX_TRK_TRACK_IMAGE) != 0;
}

/**
 * @brief Describe sector status
 * @param status FDC status
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Pointer to buffer
 */
static inline char* uft_stx_describe_status(uint8_t status, char* buffer, size_t size) {
    if (!buffer || size == 0) return NULL;
    buffer[0] = '\0';
    
    if (status == 0) {
        snprintf(buffer, size, "OK");
        return buffer;
    }
    
    size_t pos = 0;
    if (status & UFT_STX_SECT_RNF) pos += snprintf(buffer + pos, size - pos, "RNF ");
    if (status & UFT_STX_SECT_CRC_ERR) pos += snprintf(buffer + pos, size - pos, "CRC ");
    if (status & UFT_STX_SECT_DELETED) pos += snprintf(buffer + pos, size - pos, "DEL ");
    if (status & UFT_STX_SECT_FUZZY) pos += snprintf(buffer + pos, size - pos, "FUZZY ");
    
    if (pos > 0 && buffer[pos - 1] == ' ') buffer[pos - 1] = '\0';
    
    return buffer;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Header Validation and Parsing
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Validate STX signature
 * @param data File data
 * @param size File size
 * @return true if valid signature
 */
static inline bool uft_stx_validate_signature(const uint8_t* data, size_t size) {
    if (!data || size < UFT_STX_HEADER_SIZE) return false;
    return (memcmp(data, UFT_STX_SIGNATURE, UFT_STX_SIGNATURE_LEN) == 0 &&
            data[3] == 0);
}

/**
 * @brief Validate STX header
 * @param header STX header
 * @return true if valid
 */
static inline bool uft_stx_validate_header(const uft_stx_header_t* header) {
    if (!header) return false;
    
    /* Check signature */
    if (memcmp(header->signature, UFT_STX_SIGNATURE, UFT_STX_SIGNATURE_LEN) != 0) {
        return false;
    }
    if (header->signature[3] != 0) return false;
    
    /* Check version */
    if (header->version != UFT_STX_VERSION_3) return false;
    
    /* Check track count */
    if (header->track_count == 0 || header->track_count > UFT_STX_MAX_TRACKS * 2) {
        return false;
    }
    
    return true;
}

/**
 * @brief Parse STX file into info structure
 * @param data File data
 * @param size File size
 * @param info Output info structure
 * @return true if successfully parsed
 */
static inline bool uft_stx_parse(const uint8_t* data, size_t size,
                                 uft_stx_info_t* info) {
    if (!data || !info || size < UFT_STX_HEADER_SIZE) return false;
    
    const uft_stx_header_t* header = (const uft_stx_header_t*)data;
    if (!uft_stx_validate_header(header)) return false;
    
    memset(info, 0, sizeof(*info));
    
    info->version = header->version;
    info->track_count = header->track_count;
    info->revision = header->revision;
    
    /* Determine sides from track count */
    if (header->track_count > 80) {
        info->sides = 2;
    } else {
        info->sides = 1;
    }
    
    /* Parse track descriptors */
    size_t offset = UFT_STX_HEADER_SIZE;
    
    for (uint8_t t = 0; t < header->track_count && offset + UFT_STX_TRACK_HEADER_SIZE <= size; t++) {
        const uft_stx_track_desc_t* track = (const uft_stx_track_desc_t*)(data + offset);
        
        if (track->fuzzy_count > 0) {
            info->has_fuzzy = true;
            info->fuzzy_bytes += track->fuzzy_count;
        }
        
        if (uft_stx_track_has_timing(track->flags)) {
            info->has_timing = true;
        }
        
        info->total_sectors += track->sector_count;
        
        /* Remember typical sector count */
        if (track->sector_count > 0 && info->sectors_per_track == 0) {
            info->sectors_per_track = track->sector_count;
        }
        
        /* Parse sector descriptors if present */
        if (uft_stx_track_has_sectors(track->flags)) {
            size_t sect_offset = offset + UFT_STX_TRACK_HEADER_SIZE;
            
            for (uint16_t s = 0; s < track->sector_count && 
                 sect_offset + UFT_STX_SECTOR_HEADER_SIZE <= size; s++) {
                
                const uft_stx_sector_desc_t* sect = 
                    (const uft_stx_sector_desc_t*)(data + sect_offset);
                
                if (uft_stx_has_crc_error(sect->fdc_status)) info->has_errors = true;
                if (uft_stx_is_deleted(sect->fdc_status)) info->has_deleted = true;
                if (uft_stx_is_fuzzy(sect->fdc_status)) info->has_fuzzy = true;
                
                sect_offset += UFT_STX_SECTOR_HEADER_SIZE;
            }
        }
        
        offset += track->size;
    }
    
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Probe and Detection
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Probe data to determine if it's an STX file
 * @param data File data
 * @param size File size
 * @return Confidence score 0-100
 */
static inline int uft_stx_probe(const uint8_t* data, size_t size) {
    if (!data || size < UFT_STX_HEADER_SIZE) return 0;
    
    /* Check signature */
    if (!uft_stx_validate_signature(data, size)) return 0;
    
    int score = 60;  /* Base score for signature */
    
    const uft_stx_header_t* header = (const uft_stx_header_t*)data;
    
    /* Check version */
    if (header->version == UFT_STX_VERSION_3) {
        score += 20;
    }
    
    /* Check track count */
    if (header->track_count > 0 && header->track_count <= 168) {
        score += 10;
    }
    
    /* Check first track descriptor */
    if (size >= UFT_STX_HEADER_SIZE + UFT_STX_TRACK_HEADER_SIZE) {
        const uft_stx_track_desc_t* track = 
            (const uft_stx_track_desc_t*)(data + UFT_STX_HEADER_SIZE);
        
        /* Track size should be reasonable */
        if (track->size >= UFT_STX_TRACK_HEADER_SIZE && track->size < 100000) {
            score += 10;
        }
    }
    
    return (score > 100) ? 100 : score;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Creation Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize STX header
 * @param header Output header
 * @param track_count Number of tracks
 */
static inline void uft_stx_create_header(uft_stx_header_t* header,
                                         uint8_t track_count) {
    if (!header) return;
    
    memset(header, 0, sizeof(*header));
    memcpy(header->signature, UFT_STX_SIGNATURE, UFT_STX_SIGNATURE_LEN);
    header->signature[3] = 0;
    header->version = UFT_STX_VERSION_3;
    header->track_count = track_count;
}

/**
 * @brief Initialize STX track descriptor
 * @param track Output track descriptor
 * @param track_number Track number
 * @param sector_count Number of sectors
 * @param flags Track flags
 */
static inline void uft_stx_create_track_desc(uft_stx_track_desc_t* track,
                                             uint8_t track_number,
                                             uint16_t sector_count,
                                             uint16_t flags) {
    if (!track) return;
    
    memset(track, 0, sizeof(*track));
    track->track_number = track_number;
    track->sector_count = sector_count;
    track->flags = flags;
}

/**
 * @brief Initialize STX sector descriptor
 * @param sector Output sector descriptor
 * @param c Cylinder
 * @param h Head
 * @param r Sector number
 * @param n Size code
 */
static inline void uft_stx_create_sector_desc(uft_stx_sector_desc_t* sector,
                                              uint8_t c, uint8_t h,
                                              uint8_t r, uint8_t n) {
    if (!sector) return;
    
    memset(sector, 0, sizeof(*sector));
    sector->track = c;
    sector->head = h;
    sector->sector = r;
    sector->size = n;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_STX_FORMAT_H */
