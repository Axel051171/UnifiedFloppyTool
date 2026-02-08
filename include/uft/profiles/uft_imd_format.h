/**
 * @file uft_imd_format.h
 * @brief IMD (ImageDisk) format profile - Dave Dunfield's PC preservation standard
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * IMD is a sector-level disk image format created by Dave Dunfield for his
 * ImageDisk utility. It preserves sector data along with metadata about
 * data rates, encoding, and sector status. Widely used for PC/DOS disk
 * preservation and supports FM/MFM encoding at various data rates.
 *
 * Format specification: http://dunfield.classiccmp.org/img/index.htm
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_IMD_FORMAT_H
#define UFT_IMD_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * IMD Format Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief IMD signature string (always starts file) */
#define UFT_IMD_SIGNATURE       "IMD "

/** @brief IMD signature length */
#define UFT_IMD_SIGNATURE_LEN   4

/** @brief IMD header terminator (ASCII 0x1A = EOF) */
#define UFT_IMD_HEADER_END      0x1A

/** @brief Maximum comment length (practical limit) */
#define UFT_IMD_MAX_COMMENT     8192

/** @brief Maximum tracks per disk */
#define UFT_IMD_MAX_TRACKS      255

/** @brief Maximum heads per disk */
#define UFT_IMD_MAX_HEADS       2

/** @brief Maximum sectors per track */
#define UFT_IMD_MAX_SECTORS     255

/* ═══════════════════════════════════════════════════════════════════════════
 * IMD Mode Byte (Data Rate + Encoding)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief IMD track mode values
 * 
 * Mode = (data_rate * 2) + (mfm ? 1 : 0)
 * 
 * Data rates:
 *   0 = 500 kbps (HD)
 *   1 = 300 kbps (DD in HD drive)
 *   2 = 250 kbps (DD)
 *   3 = 500 kbps (??? - rare)
 *   4 = 300 kbps (??? - rare)
 *   5 = 250 kbps (??? - rare)
 */
typedef enum {
    UFT_IMD_MODE_500K_FM    = 0,    /**< 500 kbps FM encoding */
    UFT_IMD_MODE_500K_MFM   = 1,    /**< 500 kbps MFM encoding */
    UFT_IMD_MODE_300K_FM    = 2,    /**< 300 kbps FM encoding */
    UFT_IMD_MODE_300K_MFM   = 3,    /**< 300 kbps MFM encoding */
    UFT_IMD_MODE_250K_FM    = 4,    /**< 250 kbps FM encoding */
    UFT_IMD_MODE_250K_MFM   = 5,    /**< 250 kbps MFM encoding */
    UFT_IMD_MODE_INVALID    = 0xFF  /**< Invalid/unknown mode */
} uft_imd_mode_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * IMD Sector Status Flags
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief IMD sector data type indicators
 * 
 * These values appear before sector data and indicate the type of data:
 *   0x00 = Sector data unavailable
 *   0x01 = Normal data
 *   0x02 = Normal data with compression (all bytes same)
 *   0x03 = Deleted data
 *   0x04 = Deleted data with compression
 *   0x05 = Normal data with CRC error
 *   0x06 = Normal data with CRC error, compressed
 *   0x07 = Deleted data with CRC error
 *   0x08 = Deleted data with CRC error, compressed
 */
typedef enum {
    UFT_IMD_SECT_UNAVAILABLE        = 0x00, /**< Sector data not available */
    UFT_IMD_SECT_NORMAL             = 0x01, /**< Normal sector data */
    UFT_IMD_SECT_NORMAL_COMPRESSED  = 0x02, /**< Normal data, all bytes same */
    UFT_IMD_SECT_DELETED            = 0x03, /**< Deleted sector data */
    UFT_IMD_SECT_DELETED_COMPRESSED = 0x04, /**< Deleted data, all bytes same */
    UFT_IMD_SECT_CRC_ERROR          = 0x05, /**< Normal data with CRC error */
    UFT_IMD_SECT_CRC_COMPRESSED     = 0x06, /**< CRC error, all bytes same */
    UFT_IMD_SECT_DEL_CRC_ERROR      = 0x07, /**< Deleted with CRC error */
    UFT_IMD_SECT_DEL_CRC_COMPRESSED = 0x08  /**< Deleted CRC error, compressed */
} uft_imd_sector_type_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * IMD Sector Size Codes
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief IMD sector size codes (same as FDC N field)
 */
typedef enum {
    UFT_IMD_SIZE_128    = 0,    /**< 128 bytes per sector */
    UFT_IMD_SIZE_256    = 1,    /**< 256 bytes per sector */
    UFT_IMD_SIZE_512    = 2,    /**< 512 bytes per sector */
    UFT_IMD_SIZE_1024   = 3,    /**< 1024 bytes per sector */
    UFT_IMD_SIZE_2048   = 4,    /**< 2048 bytes per sector */
    UFT_IMD_SIZE_4096   = 5,    /**< 4096 bytes per sector */
    UFT_IMD_SIZE_8192   = 6     /**< 8192 bytes per sector */
} uft_imd_size_code_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * IMD Head Flags
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief IMD head byte flags
 * 
 * Lower 4 bits = head number (0 or 1)
 * Bit 6 (0x40) = Sector cylinder map present
 * Bit 7 (0x80) = Sector head map present
 */
#define UFT_IMD_HEAD_MASK           0x0F    /**< Head number mask */
#define UFT_IMD_HEAD_CYL_MAP        0x40    /**< Sector cylinder map present */
#define UFT_IMD_HEAD_HEAD_MAP       0x80    /**< Sector head map present */

/* ═══════════════════════════════════════════════════════════════════════════
 * IMD Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief IMD file header information
 * 
 * IMD files start with "IMD x.xx: DD/MM/YYYY HH:MM:SS\r\n"
 * followed by optional comment text ending with 0x1A
 */
typedef struct {
    uint8_t version_major;          /**< Major version (e.g., 1) */
    uint8_t version_minor;          /**< Minor version (e.g., 18) */
    uint8_t day;                    /**< Day of creation (1-31) */
    uint8_t month;                  /**< Month of creation (1-12) */
    uint16_t year;                  /**< Year of creation */
    uint8_t hour;                   /**< Hour of creation (0-23) */
    uint8_t minute;                 /**< Minute of creation (0-59) */
    uint8_t second;                 /**< Second of creation (0-59) */
    char comment[UFT_IMD_MAX_COMMENT]; /**< Optional comment (null-terminated) */
    uint32_t comment_length;        /**< Length of comment */
    uint32_t data_offset;           /**< Offset to first track data */
} uft_imd_header_t;

/**
 * @brief IMD track header (parsed from file)
 */
typedef struct {
    uint8_t mode;                   /**< Data rate + encoding */
    uint8_t cylinder;               /**< Physical cylinder number */
    uint8_t head;                   /**< Head number + flags */
    uint8_t sector_count;           /**< Number of sectors in track */
    uint8_t sector_size;            /**< Sector size code (N value) */
    bool has_cylinder_map;          /**< Sector cylinder map present */
    bool has_head_map;              /**< Sector head map present */
} uft_imd_track_header_t;

/**
 * @brief IMD sector information
 */
typedef struct {
    uint8_t cylinder;               /**< Sector cylinder (from ID or map) */
    uint8_t head;                   /**< Sector head (from ID or map) */
    uint8_t sector;                 /**< Sector number */
    uint8_t size_code;              /**< Sector size code */
    uint8_t type;                   /**< Sector type/status */
    bool is_compressed;             /**< Data is compressed (single byte) */
    bool is_deleted;                /**< Deleted data mark */
    bool has_crc_error;             /**< CRC error in sector */
    uint32_t data_size;             /**< Size of sector data in bytes */
    uint32_t file_offset;           /**< Offset to sector data in file */
} uft_imd_sector_info_t;

/**
 * @brief Standard disk geometries for IMD
 */
typedef struct {
    const char* name;               /**< Geometry name */
    uint8_t cylinders;              /**< Number of cylinders */
    uint8_t heads;                  /**< Number of heads */
    uint8_t sectors;                /**< Sectors per track */
    uint8_t size_code;              /**< Sector size code */
    uint8_t mode;                   /**< Track mode */
    uint32_t total_size;            /**< Total disk size in bytes */
} uft_imd_geometry_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Standard Geometries
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Common IMD disk geometries */
static const uft_imd_geometry_t UFT_IMD_GEOMETRIES[] = {
    /* 5.25" Double Density */
    { "5.25\" SSDD 160KB",  40, 1,  8, 2, UFT_IMD_MODE_250K_MFM,  163840 },
    { "5.25\" SSDD 180KB",  40, 1,  9, 2, UFT_IMD_MODE_250K_MFM,  184320 },
    { "5.25\" DSDD 320KB",  40, 2,  8, 2, UFT_IMD_MODE_250K_MFM,  327680 },
    { "5.25\" DSDD 360KB",  40, 2,  9, 2, UFT_IMD_MODE_250K_MFM,  368640 },
    
    /* 5.25" High Density */
    { "5.25\" DSHD 1.2MB",  80, 2, 15, 2, UFT_IMD_MODE_500K_MFM, 1228800 },
    
    /* 3.5" Double Density */
    { "3.5\" DSDD 720KB",   80, 2,  9, 2, UFT_IMD_MODE_250K_MFM,  737280 },
    
    /* 3.5" High Density */
    { "3.5\" DSHD 1.44MB",  80, 2, 18, 2, UFT_IMD_MODE_500K_MFM, 1474560 },
    
    /* 3.5" Extended Density */
    { "3.5\" DSED 2.88MB",  80, 2, 36, 2, UFT_IMD_MODE_500K_MFM, 2949120 },
    
    /* 8" Single Density */
    { "8\" SSSD 250KB",     77, 1, 26, 0, UFT_IMD_MODE_500K_FM,   256256 },
    { "8\" DSSD 500KB",     77, 2, 26, 0, UFT_IMD_MODE_500K_FM,   512512 },
    
    /* 8" Double Density */
    { "8\" SSDD 500KB",     77, 1, 26, 1, UFT_IMD_MODE_500K_MFM,  512512 },
    { "8\" DSDD 1MB",       77, 2, 26, 1, UFT_IMD_MODE_500K_MFM, 1025024 },
    
    { NULL, 0, 0, 0, 0, 0, 0 }  /* Terminator */
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Inline Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Convert sector size code to bytes
 * @param size_code Sector size code (0-6)
 * @return Sector size in bytes, or 0 if invalid
 */
static inline uint32_t uft_imd_size_code_to_bytes(uint8_t size_code) {
    if (size_code > 6) return 0;
    return 128U << size_code;  /* 128, 256, 512, 1024, 2048, 4096, 8192 */
}

/**
 * @brief Convert bytes to sector size code
 * @param bytes Sector size in bytes
 * @return Size code (0-6), or 0xFF if invalid
 */
static inline uint8_t uft_imd_bytes_to_size_code(uint32_t bytes) {
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
 * @brief Check if mode uses MFM encoding
 * @param mode Track mode byte
 * @return true if MFM, false if FM
 */
static inline bool uft_imd_mode_is_mfm(uint8_t mode) {
    return (mode & 0x01) != 0;
}

/**
 * @brief Get data rate in kbps from mode
 * @param mode Track mode byte
 * @return Data rate in kbps (500, 300, or 250)
 */
static inline uint32_t uft_imd_mode_data_rate(uint8_t mode) {
    switch (mode >> 1) {
        case 0: return 500;
        case 1: return 300;
        case 2: return 250;
        default: return 0;
    }
}

/**
 * @brief Get encoding name from mode
 * @param mode Track mode byte
 * @return Encoding name string
 */
static inline const char* uft_imd_mode_encoding_name(uint8_t mode) {
    return uft_imd_mode_is_mfm(mode) ? "MFM" : "FM";
}

/**
 * @brief Get full mode description
 * @param mode Track mode byte
 * @return Mode description string
 */
static inline const char* uft_imd_mode_name(uint8_t mode) {
    switch (mode) {
        case UFT_IMD_MODE_500K_FM:  return "500 kbps FM";
        case UFT_IMD_MODE_500K_MFM: return "500 kbps MFM";
        case UFT_IMD_MODE_300K_FM:  return "300 kbps FM";
        case UFT_IMD_MODE_300K_MFM: return "300 kbps MFM";
        case UFT_IMD_MODE_250K_FM:  return "250 kbps FM";
        case UFT_IMD_MODE_250K_MFM: return "250 kbps MFM";
        default: return "Unknown";
    }
}

/**
 * @brief Get sector type description
 * @param type Sector type byte
 * @return Type description string
 */
static inline const char* uft_imd_sector_type_name(uint8_t type) {
    switch (type) {
        case UFT_IMD_SECT_UNAVAILABLE:        return "Unavailable";
        case UFT_IMD_SECT_NORMAL:             return "Normal";
        case UFT_IMD_SECT_NORMAL_COMPRESSED:  return "Normal (compressed)";
        case UFT_IMD_SECT_DELETED:            return "Deleted";
        case UFT_IMD_SECT_DELETED_COMPRESSED: return "Deleted (compressed)";
        case UFT_IMD_SECT_CRC_ERROR:          return "CRC Error";
        case UFT_IMD_SECT_CRC_COMPRESSED:     return "CRC Error (compressed)";
        case UFT_IMD_SECT_DEL_CRC_ERROR:      return "Deleted + CRC Error";
        case UFT_IMD_SECT_DEL_CRC_COMPRESSED: return "Deleted + CRC Error (compressed)";
        default: return "Unknown";
    }
}

/**
 * @brief Check if sector type indicates compressed data
 * @param type Sector type byte
 * @return true if compressed (single byte fill)
 */
static inline bool uft_imd_sector_is_compressed(uint8_t type) {
    return (type == UFT_IMD_SECT_NORMAL_COMPRESSED ||
            type == UFT_IMD_SECT_DELETED_COMPRESSED ||
            type == UFT_IMD_SECT_CRC_COMPRESSED ||
            type == UFT_IMD_SECT_DEL_CRC_COMPRESSED);
}

/**
 * @brief Check if sector type indicates deleted data
 * @param type Sector type byte
 * @return true if deleted
 */
static inline bool uft_imd_sector_is_deleted(uint8_t type) {
    return (type == UFT_IMD_SECT_DELETED ||
            type == UFT_IMD_SECT_DELETED_COMPRESSED ||
            type == UFT_IMD_SECT_DEL_CRC_ERROR ||
            type == UFT_IMD_SECT_DEL_CRC_COMPRESSED);
}

/**
 * @brief Check if sector type indicates CRC error
 * @param type Sector type byte
 * @return true if CRC error
 */
static inline bool uft_imd_sector_has_crc_error(uint8_t type) {
    return (type >= UFT_IMD_SECT_CRC_ERROR);
}

/**
 * @brief Check if sector has data available
 * @param type Sector type byte
 * @return true if data is available
 */
static inline bool uft_imd_sector_has_data(uint8_t type) {
    return (type != UFT_IMD_SECT_UNAVAILABLE);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Header Parsing Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Validate IMD file signature
 * @param data Pointer to file data
 * @param size Size of data buffer
 * @return true if valid IMD signature
 */
static inline bool uft_imd_validate_signature(const uint8_t* data, size_t size) {
    if (!data || size < UFT_IMD_SIGNATURE_LEN) return false;
    return (memcmp(data, UFT_IMD_SIGNATURE, UFT_IMD_SIGNATURE_LEN) == 0);
}

/**
 * @brief Parse IMD header from file data
 * @param data Pointer to file data
 * @param size Size of data buffer
 * @param header Output header structure
 * @return true if successfully parsed
 */
static inline bool uft_imd_parse_header(const uint8_t* data, size_t size,
                                        uft_imd_header_t* header) {
    if (!data || !header || size < 32) return false;
    if (!uft_imd_validate_signature(data, size)) return false;
    
    memset(header, 0, sizeof(*header));
    
    /* Parse "IMD x.xx: DD/MM/YYYY HH:MM:SS\r\n" */
    /* Minimum: "IMD 1.18: 01/01/2000 00:00:00\r\n" = 32 bytes */
    
    /* Version */
    if (data[4] >= '0' && data[4] <= '9') {
        header->version_major = data[4] - '0';
    }
    if (data[6] >= '0' && data[6] <= '9' && data[7] >= '0' && data[7] <= '9') {
        header->version_minor = (data[6] - '0') * 10 + (data[7] - '0');
    }
    
    /* Date: DD/MM/YYYY starting at offset 10 */
    if (size >= 20) {
        header->day = (data[10] - '0') * 10 + (data[11] - '0');
        header->month = (data[13] - '0') * 10 + (data[14] - '0');
        header->year = (data[16] - '0') * 1000 + (data[17] - '0') * 100 +
                       (data[18] - '0') * 10 + (data[19] - '0');
    }
    
    /* Time: HH:MM:SS starting at offset 21 */
    if (size >= 29) {
        header->hour = (data[21] - '0') * 10 + (data[22] - '0');
        header->minute = (data[24] - '0') * 10 + (data[25] - '0');
        header->second = (data[27] - '0') * 10 + (data[28] - '0');
    }
    
    /* Find end of header line (CR LF) and start of comment */
    size_t pos = UFT_IMD_SIGNATURE_LEN;
    while (pos < size && data[pos] != '\n') pos++;
    if (pos < size) pos++;  /* Skip LF */
    
    /* Read comment until 0x1A */
    size_t comment_start = pos;
    while (pos < size && data[pos] != UFT_IMD_HEADER_END) {
        if (header->comment_length < UFT_IMD_MAX_COMMENT - 1) {
            header->comment[header->comment_length++] = data[pos];
        }
        pos++;
    }
    header->comment[header->comment_length] = '\0';
    
    /* Data starts after 0x1A */
    if (pos < size && data[pos] == UFT_IMD_HEADER_END) {
        header->data_offset = pos + 1;
    } else {
        header->data_offset = pos;
    }
    
    return true;
}

/**
 * @brief Parse track header from file data
 * @param data Pointer to track data
 * @param size Size of available data
 * @param track Output track header structure
 * @return Number of bytes consumed, or 0 on error
 */
static inline size_t uft_imd_parse_track_header(const uint8_t* data, size_t size,
                                                uft_imd_track_header_t* track) {
    if (!data || !track || size < 5) return 0;
    
    memset(track, 0, sizeof(*track));
    
    track->mode = data[0];
    track->cylinder = data[1];
    track->head = data[2] & UFT_IMD_HEAD_MASK;
    track->has_cylinder_map = (data[2] & UFT_IMD_HEAD_CYL_MAP) != 0;
    track->has_head_map = (data[2] & UFT_IMD_HEAD_HEAD_MAP) != 0;
    track->sector_count = data[3];
    track->sector_size = data[4];
    
    /* Validate */
    if (track->mode > 5) return 0;  /* Invalid mode */
    if (track->sector_size > 6) return 0;  /* Invalid size code */
    
    return 5;  /* Bytes consumed */
}

/**
 * @brief Calculate track data size (sector numbering map + optional maps + sector data)
 * @param track Track header
 * @param data Pointer to data after track header
 * @param size Available data size
 * @return Total track data size, or 0 on error
 */
static inline size_t uft_imd_calc_track_size(const uft_imd_track_header_t* track,
                                             const uint8_t* data, size_t size) {
    if (!track || !data) return 0;
    
    size_t pos = 0;
    uint32_t sector_bytes = uft_imd_size_code_to_bytes(track->sector_size);
    
    /* Sector numbering map */
    pos += track->sector_count;
    
    /* Optional cylinder map */
    if (track->has_cylinder_map) {
        pos += track->sector_count;
    }
    
    /* Optional head map */
    if (track->has_head_map) {
        pos += track->sector_count;
    }
    
    /* Sector data */
    for (uint8_t s = 0; s < track->sector_count && pos < size; s++) {
        uint8_t type = data[pos++];
        if (type == UFT_IMD_SECT_UNAVAILABLE) {
            continue;  /* No data */
        } else if (uft_imd_sector_is_compressed(type)) {
            pos += 1;  /* Single fill byte */
        } else {
            pos += sector_bytes;  /* Full sector data */
        }
    }
    
    return pos;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Probe and Detection
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Probe data to determine if it's an IMD file
 * @param data Pointer to file data
 * @param size Size of data buffer
 * @return Confidence score 0-100 (0 = not IMD, 100 = definitely IMD)
 */
static inline int uft_imd_probe(const uint8_t* data, size_t size) {
    if (!data || size < 32) return 0;
    
    /* Check signature */
    if (!uft_imd_validate_signature(data, size)) return 0;
    
    int score = 50;  /* Base score for signature match */
    
    /* Check version format "IMD x.xx:" */
    if (data[5] == '.' && data[8] == ':') {
        score += 20;
    }
    
    /* Check for date format */
    if (data[12] == '/' && data[15] == '/') {
        score += 15;
    }
    
    /* Check for time format */
    if (size >= 27 && data[23] == ':' && data[26] == ':') {
        score += 10;
    }
    
    /* Look for 0x1A terminator */
    for (size_t i = 30; i < size && i < 1024; i++) {
        if (data[i] == UFT_IMD_HEADER_END) {
            score += 5;
            break;
        }
    }
    
    return (score > 100) ? 100 : score;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Creation Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create IMD header string
 * @param buffer Output buffer (must be at least 64 bytes)
 * @param buffer_size Size of buffer
 * @param comment Optional comment (may be NULL)
 * @return Number of bytes written, or 0 on error
 */
static inline size_t uft_imd_create_header(char* buffer, size_t buffer_size,
                                           const char* comment) {
    if (!buffer || buffer_size < 64) return 0;
    
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    
    int len = snprintf(buffer, buffer_size,
                       "IMD 1.18: %02d/%02d/%04d %02d:%02d:%02d\r\n",
                       tm_info->tm_mday, tm_info->tm_mon + 1, tm_info->tm_year + 1900,
                       tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    
    if (len < 0 || (size_t)len >= buffer_size) return 0;
    
    /* Add comment if provided */
    if (comment && *comment) {
        size_t comment_len = strlen(comment);
        if (len + comment_len + 2 < buffer_size) {
            memcpy(buffer + len, comment, comment_len);
            len += comment_len;
        }
    }
    
    /* Add terminator */
    if ((size_t)len + 1 < buffer_size) {
        buffer[len++] = UFT_IMD_HEADER_END;
    }
    
    return len;
}

/**
 * @brief Create track header bytes
 * @param track Track information
 * @param buffer Output buffer (must be at least 5 bytes)
 * @return Number of bytes written (always 5)
 */
static inline size_t uft_imd_create_track_header(const uft_imd_track_header_t* track,
                                                 uint8_t* buffer) {
    if (!track || !buffer) return 0;
    
    buffer[0] = track->mode;
    buffer[1] = track->cylinder;
    buffer[2] = track->head;
    if (track->has_cylinder_map) buffer[2] |= UFT_IMD_HEAD_CYL_MAP;
    if (track->has_head_map) buffer[2] |= UFT_IMD_HEAD_HEAD_MAP;
    buffer[3] = track->sector_count;
    buffer[4] = track->sector_size;
    
    return 5;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_IMD_FORMAT_H */
