/**
 * @file uft_td0_format.h
 * @brief TD0 (Teledisk) format profile - Sydex's historical disk archiving format
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * Teledisk (TD0) was created by Sydex in 1985 as one of the first disk imaging
 * formats. It supports optional LZSS compression (indicated by "td" vs "TD"
 * signature) and can preserve sector-level information including copy protection.
 *
 * The format was widely used for BBS distribution and software archival.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_TD0_FORMAT_H
#define UFT_TD0_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * TD0 Format Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief TD0 normal signature (uncompressed) */
#define UFT_TD0_SIGNATURE_NORMAL    "TD"

/** @brief TD0 advanced signature (LZSS compressed) */
#define UFT_TD0_SIGNATURE_ADVANCED  "td"

/** @brief TD0 signature length */
#define UFT_TD0_SIGNATURE_LEN       2

/** @brief TD0 main header size */
#define UFT_TD0_HEADER_SIZE         12

/** @brief TD0 comment header size */
#define UFT_TD0_COMMENT_HEADER_SIZE 10

/** @brief TD0 track header size */
#define UFT_TD0_TRACK_HEADER_SIZE   4

/** @brief TD0 sector header size */
#define UFT_TD0_SECTOR_HEADER_SIZE  6

/** @brief TD0 end of file marker (track with 0xFF sectors) */
#define UFT_TD0_EOF_MARKER          0xFF

/** @brief Maximum comment length */
#define UFT_TD0_MAX_COMMENT         8192

/* ═══════════════════════════════════════════════════════════════════════════
 * TD0 Version Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief TD0 version 1.0 */
#define UFT_TD0_VERSION_10          10

/** @brief TD0 version 1.1 */
#define UFT_TD0_VERSION_11          11

/** @brief TD0 version 1.5 */
#define UFT_TD0_VERSION_15          15

/** @brief TD0 version 2.0 */
#define UFT_TD0_VERSION_20          20

/** @brief TD0 version 2.1 */
#define UFT_TD0_VERSION_21          21

/* ═══════════════════════════════════════════════════════════════════════════
 * TD0 Data Rate / Drive Type
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief TD0 data rate values (in header byte)
 */
typedef enum {
    UFT_TD0_RATE_250K   = 0,    /**< 250 kbps (DD) */
    UFT_TD0_RATE_300K   = 1,    /**< 300 kbps (DD in HD drive) */
    UFT_TD0_RATE_500K   = 2,    /**< 500 kbps (HD) */
    UFT_TD0_RATE_250K_FM = 0x80 /**< 250 kbps FM (bit 7 = FM) */
} uft_td0_data_rate_t;

/**
 * @brief TD0 drive type values
 */
typedef enum {
    UFT_TD0_DRIVE_525_96TPI  = 1,   /**< 5.25" 96 TPI (1.2MB) */
    UFT_TD0_DRIVE_525_48TPI  = 2,   /**< 5.25" 48 TPI (360KB) */
    UFT_TD0_DRIVE_35_135TPI  = 3,   /**< 3.5" 135 TPI */
    UFT_TD0_DRIVE_35_HD      = 4,   /**< 3.5" HD (1.44MB) */
    UFT_TD0_DRIVE_8_INCH     = 5,   /**< 8" drive */
    UFT_TD0_DRIVE_35_ED      = 6    /**< 3.5" ED (2.88MB) */
} uft_td0_drive_type_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * TD0 Stepping Mode
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief TD0 track stepping modes
 */
typedef enum {
    UFT_TD0_STEP_SINGLE = 0,   /**< Single step (normal) */
    UFT_TD0_STEP_DOUBLE = 1,   /**< Double step (48 TPI disk in 96 TPI drive) */
    UFT_TD0_STEP_EXTRA  = 2    /**< Extra step (for verification) */
} uft_td0_stepping_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * TD0 Sector Flags
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief TD0 sector flag bits
 */
#define UFT_TD0_SECT_DUP_WITHIN     0x01    /**< Duplicate sector within track */
#define UFT_TD0_SECT_CRC_ERROR      0x02    /**< CRC error in data field */
#define UFT_TD0_SECT_DELETED        0x04    /**< Deleted data address mark */
#define UFT_TD0_SECT_SKIPPED        0x10    /**< Sector was skipped (no data) */
#define UFT_TD0_SECT_NO_DAM         0x20    /**< No data address mark */
#define UFT_TD0_SECT_NO_ID          0x40    /**< ID field was not found */

/* ═══════════════════════════════════════════════════════════════════════════
 * TD0 Sector Encoding Methods
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief TD0 sector data encoding methods
 */
typedef enum {
    UFT_TD0_ENC_RAW         = 0,   /**< Raw sector data */
    UFT_TD0_ENC_REPEATED    = 1,   /**< 2-byte repeated pattern */
    UFT_TD0_ENC_RLE         = 2    /**< Run-length encoded */
} uft_td0_encoding_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * TD0 Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief TD0 main file header (12 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t signature[2];       /**< "TD" or "td" */
    uint8_t sequence;           /**< Volume sequence (0 for single) */
    uint8_t check_sig;          /**< Check signature (should match) */
    uint8_t version;            /**< TD0 version (10, 11, 15, 20, 21) */
    uint8_t data_rate;          /**< Data rate + FM flag */
    uint8_t drive_type;         /**< Source drive type */
    uint8_t stepping;           /**< Track stepping mode */
    uint8_t dos_alloc;          /**< DOS allocation flag */
    uint8_t sides;              /**< Number of sides (1 or 2) */
    uint16_t crc;               /**< CRC of header (bytes 0-9) */
} uft_td0_header_t;
#pragma pack(pop)

/**
 * @brief TD0 comment header (10 bytes, optional)
 * 
 * Present if version >= 20 and preceded by comment flag
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t crc;               /**< CRC of comment data */
    uint16_t length;            /**< Comment data length */
    uint8_t year;               /**< Year - 1900 */
    uint8_t month;              /**< Month (1-12) */
    uint8_t day;                /**< Day (1-31) */
    uint8_t hour;               /**< Hour (0-23) */
    uint8_t minute;             /**< Minute (0-59) */
    uint8_t second;             /**< Second (0-59) */
} uft_td0_comment_header_t;
#pragma pack(pop)

/**
 * @brief TD0 track header (4 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t sector_count;       /**< Number of sectors (0xFF = EOF) */
    uint8_t cylinder;           /**< Physical cylinder number */
    uint8_t head;               /**< Head number */
    uint8_t crc;                /**< CRC of track header */
} uft_td0_track_header_t;
#pragma pack(pop)

/**
 * @brief TD0 sector header (6 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t cylinder;           /**< Sector ID cylinder */
    uint8_t head;               /**< Sector ID head */
    uint8_t sector;             /**< Sector ID number */
    uint8_t size_code;          /**< Sector size code (N) */
    uint8_t flags;              /**< Sector flags */
    uint8_t crc;                /**< CRC of sector header */
} uft_td0_sector_header_t;
#pragma pack(pop)

/**
 * @brief TD0 sector data header (3 bytes, precedes sector data)
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t data_size;         /**< Size of following data */
    uint8_t encoding;           /**< Encoding method */
} uft_td0_sector_data_t;
#pragma pack(pop)

/**
 * @brief TD0 parsed file information
 */
typedef struct {
    bool is_compressed;         /**< File uses LZSS compression */
    uint8_t version;            /**< TD0 version number */
    uint8_t data_rate;          /**< Data rate */
    bool is_fm;                 /**< FM encoding (vs MFM) */
    uint8_t drive_type;         /**< Source drive type */
    uint8_t stepping;           /**< Track stepping */
    uint8_t sides;              /**< Number of sides */
    bool has_comment;           /**< Comment block present */
    char comment[UFT_TD0_MAX_COMMENT]; /**< Comment text */
    uint16_t comment_length;    /**< Comment length */
    uint16_t year;              /**< Comment year */
    uint8_t month;              /**< Comment month */
    uint8_t day;                /**< Comment day */
    uint8_t hour;               /**< Comment hour */
    uint8_t minute;             /**< Comment minute */
    uint8_t second;             /**< Comment second */
    uint32_t data_offset;       /**< Offset to track data */
    uint32_t track_count;       /**< Number of tracks */
    uint32_t total_sectors;     /**< Total sectors in image */
} uft_td0_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Size Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_td0_header_t) == 12, "TD0 header must be 12 bytes");
_Static_assert(sizeof(uft_td0_comment_header_t) == 10, "TD0 comment header must be 10 bytes");
_Static_assert(sizeof(uft_td0_track_header_t) == 4, "TD0 track header must be 4 bytes");
_Static_assert(sizeof(uft_td0_sector_header_t) == 6, "TD0 sector header must be 6 bytes");
_Static_assert(sizeof(uft_td0_sector_data_t) == 3, "TD0 sector data header must be 3 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Inline Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Convert sector size code to bytes
 * @param size_code Sector size code (0-6)
 * @return Sector size in bytes, or 0 if invalid
 */
static inline uint32_t uft_td0_size_code_to_bytes(uint8_t size_code) {
    if (size_code > 6) return 0;
    return 128U << size_code;
}

/**
 * @brief Check if data rate indicates FM encoding
 * @param data_rate Data rate byte from header
 * @return true if FM encoding
 */
static inline bool uft_td0_is_fm(uint8_t data_rate) {
    return (data_rate & 0x80) != 0;
}

/**
 * @brief Get actual data rate in kbps
 * @param data_rate Data rate byte from header
 * @return Data rate in kbps
 */
static inline uint32_t uft_td0_get_data_rate_kbps(uint8_t data_rate) {
    switch (data_rate & 0x7F) {
        case UFT_TD0_RATE_250K: return 250;
        case UFT_TD0_RATE_300K: return 300;
        case UFT_TD0_RATE_500K: return 500;
        default: return 250;
    }
}

/**
 * @brief Get data rate name
 * @param data_rate Data rate byte
 * @return Data rate description
 */
static inline const char* uft_td0_data_rate_name(uint8_t data_rate) {
    bool fm = uft_td0_is_fm(data_rate);
    switch (data_rate & 0x7F) {
        case UFT_TD0_RATE_250K: return fm ? "250 kbps FM" : "250 kbps MFM";
        case UFT_TD0_RATE_300K: return fm ? "300 kbps FM" : "300 kbps MFM";
        case UFT_TD0_RATE_500K: return fm ? "500 kbps FM" : "500 kbps MFM";
        default: return "Unknown";
    }
}

/**
 * @brief Get drive type name
 * @param drive_type Drive type code
 * @return Drive type description
 */
static inline const char* uft_td0_drive_type_name(uint8_t drive_type) {
    switch (drive_type) {
        case UFT_TD0_DRIVE_525_96TPI: return "5.25\" 96 TPI (1.2MB)";
        case UFT_TD0_DRIVE_525_48TPI: return "5.25\" 48 TPI (360KB)";
        case UFT_TD0_DRIVE_35_135TPI: return "3.5\" 135 TPI";
        case UFT_TD0_DRIVE_35_HD:     return "3.5\" HD (1.44MB)";
        case UFT_TD0_DRIVE_8_INCH:    return "8\"";
        case UFT_TD0_DRIVE_35_ED:     return "3.5\" ED (2.88MB)";
        default: return "Unknown";
    }
}

/**
 * @brief Get stepping mode name
 * @param stepping Stepping mode code
 * @return Stepping mode description
 */
static inline const char* uft_td0_stepping_name(uint8_t stepping) {
    switch (stepping) {
        case UFT_TD0_STEP_SINGLE: return "Single step";
        case UFT_TD0_STEP_DOUBLE: return "Double step";
        case UFT_TD0_STEP_EXTRA:  return "Extra step";
        default: return "Unknown";
    }
}

/**
 * @brief Get encoding method name
 * @param encoding Encoding method code
 * @return Encoding method description
 */
static inline const char* uft_td0_encoding_name(uint8_t encoding) {
    switch (encoding) {
        case UFT_TD0_ENC_RAW:      return "Raw";
        case UFT_TD0_ENC_REPEATED: return "Repeated";
        case UFT_TD0_ENC_RLE:      return "RLE";
        default: return "Unknown";
    }
}

/**
 * @brief Get version string
 * @param version Version byte
 * @return Version string
 */
static inline const char* uft_td0_version_name(uint8_t version) {
    switch (version) {
        case UFT_TD0_VERSION_10: return "1.0";
        case UFT_TD0_VERSION_11: return "1.1";
        case UFT_TD0_VERSION_15: return "1.5";
        case UFT_TD0_VERSION_20: return "2.0";
        case UFT_TD0_VERSION_21: return "2.1";
        default: return "Unknown";
    }
}

/**
 * @brief Check if sector flags indicate valid data
 * @param flags Sector flags
 * @return true if sector has valid data
 */
static inline bool uft_td0_sector_has_data(uint8_t flags) {
    return (flags & (UFT_TD0_SECT_SKIPPED | UFT_TD0_SECT_NO_DAM | UFT_TD0_SECT_NO_ID)) == 0;
}

/**
 * @brief Describe sector flags
 * @param flags Sector flags
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Pointer to buffer
 */
static inline char* uft_td0_describe_sector_flags(uint8_t flags, char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) return NULL;
    buffer[0] = '\0';
    
    if (flags == 0) {
        snprintf(buffer, buffer_size, "Normal");
        return buffer;
    }
    
    size_t pos = 0;
    if (flags & UFT_TD0_SECT_DUP_WITHIN) {
        pos += snprintf(buffer + pos, buffer_size - pos, "Duplicate ");
    }
    if (flags & UFT_TD0_SECT_CRC_ERROR) {
        pos += snprintf(buffer + pos, buffer_size - pos, "CRC-Error ");
    }
    if (flags & UFT_TD0_SECT_DELETED) {
        pos += snprintf(buffer + pos, buffer_size - pos, "Deleted ");
    }
    if (flags & UFT_TD0_SECT_SKIPPED) {
        pos += snprintf(buffer + pos, buffer_size - pos, "Skipped ");
    }
    if (flags & UFT_TD0_SECT_NO_DAM) {
        pos += snprintf(buffer + pos, buffer_size - pos, "No-DAM ");
    }
    if (flags & UFT_TD0_SECT_NO_ID) {
        pos += snprintf(buffer + pos, buffer_size - pos, "No-ID ");
    }
    
    /* Remove trailing space */
    if (pos > 0 && buffer[pos - 1] == ' ') {
        buffer[pos - 1] = '\0';
    }
    
    return buffer;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CRC Calculation
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Calculate TD0 CRC (simple checksum)
 * @param data Data to checksum
 * @param length Data length
 * @return CRC value
 */
static inline uint16_t uft_td0_calc_crc(const uint8_t* data, size_t length) {
    uint16_t crc = 0;
    for (size_t i = 0; i < length; i++) {
        crc += data[i];
    }
    return crc;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Header Validation and Parsing
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Validate TD0 file signature
 * @param data Pointer to file data
 * @param size Size of data buffer
 * @return true if valid TD0 signature
 */
static inline bool uft_td0_validate_signature(const uint8_t* data, size_t size) {
    if (!data || size < UFT_TD0_SIGNATURE_LEN) return false;
    return (memcmp(data, UFT_TD0_SIGNATURE_NORMAL, UFT_TD0_SIGNATURE_LEN) == 0 ||
            memcmp(data, UFT_TD0_SIGNATURE_ADVANCED, UFT_TD0_SIGNATURE_LEN) == 0);
}

/**
 * @brief Check if TD0 file is compressed
 * @param data Pointer to file data
 * @param size Size of data buffer
 * @return true if compressed ("td" signature)
 */
static inline bool uft_td0_is_compressed(const uint8_t* data, size_t size) {
    if (!data || size < UFT_TD0_SIGNATURE_LEN) return false;
    return (memcmp(data, UFT_TD0_SIGNATURE_ADVANCED, UFT_TD0_SIGNATURE_LEN) == 0);
}

/**
 * @brief Validate TD0 header
 * @param header Pointer to TD0 header
 * @return true if header is valid
 */
static inline bool uft_td0_validate_header(const uft_td0_header_t* header) {
    if (!header) return false;
    
    /* Check signature */
    if (memcmp(header->signature, UFT_TD0_SIGNATURE_NORMAL, 2) != 0 &&
        memcmp(header->signature, UFT_TD0_SIGNATURE_ADVANCED, 2) != 0) {
        return false;
    }
    
    /* Check version */
    if (header->version != UFT_TD0_VERSION_10 &&
        header->version != UFT_TD0_VERSION_11 &&
        header->version != UFT_TD0_VERSION_15 &&
        header->version != UFT_TD0_VERSION_20 &&
        header->version != UFT_TD0_VERSION_21) {
        return false;
    }
    
    /* Check sides */
    if (header->sides < 1 || header->sides > 2) {
        return false;
    }
    
    /* Verify CRC */
    uint16_t calc_crc = uft_td0_calc_crc((const uint8_t*)header, 10);
    if (calc_crc != header->crc) {
        return false;  /* CRC mismatch */
    }
    
    return true;
}

/**
 * @brief Parse TD0 header into info structure
 * @param data Pointer to file data
 * @param size Size of data buffer
 * @param info Output info structure
 * @return true if successfully parsed
 */
static inline bool uft_td0_parse_header(const uint8_t* data, size_t size,
                                        uft_td0_info_t* info) {
    if (!data || !info || size < UFT_TD0_HEADER_SIZE) return false;
    
    const uft_td0_header_t* header = (const uft_td0_header_t*)data;
    if (!uft_td0_validate_header(header)) return false;
    
    memset(info, 0, sizeof(*info));
    
    info->is_compressed = (header->signature[0] == 't');
    info->version = header->version;
    info->data_rate = header->data_rate & 0x7F;
    info->is_fm = uft_td0_is_fm(header->data_rate);
    info->drive_type = header->drive_type;
    info->stepping = header->stepping;
    info->sides = header->sides;
    
    size_t offset = UFT_TD0_HEADER_SIZE;
    
    /* Check for comment block (version >= 20) */
    if (header->version >= UFT_TD0_VERSION_20 && offset + UFT_TD0_COMMENT_HEADER_SIZE <= size) {
        /* Check for comment flag (first byte after header is non-zero) */
        if (data[offset] != 0) {
            info->has_comment = true;
            const uft_td0_comment_header_t* comment = 
                (const uft_td0_comment_header_t*)(data + offset);
            
            info->comment_length = comment->length;
            info->year = 1900 + comment->year;
            info->month = comment->month;
            info->day = comment->day;
            info->hour = comment->hour;
            info->minute = comment->minute;
            info->second = comment->second;
            
            offset += UFT_TD0_COMMENT_HEADER_SIZE;
            
            /* Copy comment text */
            if (info->comment_length > 0 && offset + info->comment_length <= size) {
                size_t copy_len = (info->comment_length < UFT_TD0_MAX_COMMENT - 1) ?
                                  info->comment_length : UFT_TD0_MAX_COMMENT - 1;
                memcpy(info->comment, data + offset, copy_len);
                info->comment[copy_len] = '\0';
                offset += info->comment_length;
            }
        }
    }
    
    info->data_offset = offset;
    
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Probe and Detection
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Probe data to determine if it's a TD0 file
 * @param data Pointer to file data
 * @param size Size of data buffer
 * @return Confidence score 0-100 (0 = not TD0, 100 = definitely TD0)
 */
static inline int uft_td0_probe(const uint8_t* data, size_t size) {
    if (!data || size < UFT_TD0_HEADER_SIZE) return 0;
    
    /* Check signature */
    if (!uft_td0_validate_signature(data, size)) return 0;
    
    int score = 50;  /* Base score for signature match */
    
    const uft_td0_header_t* header = (const uft_td0_header_t*)data;
    
    /* Check valid version */
    if (header->version == UFT_TD0_VERSION_10 ||
        header->version == UFT_TD0_VERSION_11 ||
        header->version == UFT_TD0_VERSION_15 ||
        header->version == UFT_TD0_VERSION_20 ||
        header->version == UFT_TD0_VERSION_21) {
        score += 15;
    }
    
    /* Check valid sides */
    if (header->sides == 1 || header->sides == 2) {
        score += 10;
    }
    
    /* Check valid drive type */
    if (header->drive_type >= 1 && header->drive_type <= 6) {
        score += 10;
    }
    
    /* Verify header CRC */
    uint16_t calc_crc = uft_td0_calc_crc(data, 10);
    if (calc_crc == header->crc) {
        score += 15;
    }
    
    return (score > 100) ? 100 : score;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Creation Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize a TD0 header
 * @param header Output header structure
 * @param compressed Use LZSS compression
 * @param drive_type Source drive type
 * @param sides Number of sides (1 or 2)
 * @param data_rate Data rate code
 */
static inline void uft_td0_create_header(uft_td0_header_t* header,
                                         bool compressed,
                                         uint8_t drive_type,
                                         uint8_t sides,
                                         uint8_t data_rate) {
    if (!header) return;
    
    memset(header, 0, sizeof(*header));
    
    if (compressed) {
        memcpy(header->signature, UFT_TD0_SIGNATURE_ADVANCED, 2);
    } else {
        memcpy(header->signature, UFT_TD0_SIGNATURE_NORMAL, 2);
    }
    
    header->sequence = 0;
    header->check_sig = 0;
    header->version = UFT_TD0_VERSION_21;
    header->data_rate = data_rate;
    header->drive_type = drive_type;
    header->stepping = UFT_TD0_STEP_SINGLE;
    header->dos_alloc = 0;
    header->sides = sides;
    
    /* Calculate CRC */
    header->crc = uft_td0_calc_crc((const uint8_t*)header, 10);
}

/**
 * @brief Initialize a TD0 track header
 * @param header Output track header
 * @param cylinder Cylinder number
 * @param head Head number
 * @param sector_count Number of sectors
 */
static inline void uft_td0_create_track_header(uft_td0_track_header_t* header,
                                               uint8_t cylinder,
                                               uint8_t head,
                                               uint8_t sector_count) {
    if (!header) return;
    
    header->sector_count = sector_count;
    header->cylinder = cylinder;
    header->head = head;
    header->crc = sector_count + cylinder + head;
}

/**
 * @brief Initialize a TD0 sector header
 * @param header Output sector header
 * @param cylinder ID cylinder
 * @param head ID head
 * @param sector Sector number
 * @param size_code Sector size code
 * @param flags Sector flags
 */
static inline void uft_td0_create_sector_header(uft_td0_sector_header_t* header,
                                                uint8_t cylinder,
                                                uint8_t head,
                                                uint8_t sector,
                                                uint8_t size_code,
                                                uint8_t flags) {
    if (!header) return;
    
    header->cylinder = cylinder;
    header->head = head;
    header->sector = sector;
    header->size_code = size_code;
    header->flags = flags;
    header->crc = cylinder + head + sector + size_code + flags;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_TD0_FORMAT_H */
