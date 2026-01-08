/**
 * @file uft_ipf.h
 * @brief IPF Container: Read / Write / Validate (C11)
 * 
 * Implements robust parsing for IPF/CAPS-style container files.
 * Based on publicly available information about the IPF format.
 * 
 * IPF (Interchangeable Preservation Format) was developed by the
 * Software Preservation Society (SPS) for preserving copy-protected
 * floppy disk images, particularly Amiga software.
 * 
 * This module handles:
 * - Container structure (record/chunk parsing)
 * - CRC32 validation
 * - Basic record type identification
 * - Forensic analysis and diagnostics
 * 
 * Note: Full track decoding requires the CapsImg library from SPS.
 * 
 * @version 2.0.0 - Enhanced with known record types
 * @date 2025-01-08
 */

#ifndef UFT_IPF_H
#define UFT_IPF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Version
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_IPF_VERSION_MAJOR   2
#define UFT_IPF_VERSION_MINOR   0
#define UFT_IPF_VERSION_PATCH   0
#define UFT_IPF_VERSION_STRING  "2.0.0"

/* ═══════════════════════════════════════════════════════════════════════════════
 * Error Codes
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum uft_ipf_err {
    UFT_IPF_OK              =  0,   /**< Success */
    UFT_IPF_EINVAL          = -1,   /**< Invalid argument */
    UFT_IPF_EIO             = -2,   /**< I/O error */
    UFT_IPF_EFORMAT         = -3,   /**< Invalid format / not an IPF */
    UFT_IPF_ESHORT          = -4,   /**< File too short */
    UFT_IPF_EBOUNDS         = -5,   /**< Record out of bounds */
    UFT_IPF_EOVERLAP        = -6,   /**< Records overlap */
    UFT_IPF_ECRC            = -7,   /**< CRC mismatch */
    UFT_IPF_ENOMEM          = -8,   /**< Out of memory */
    UFT_IPF_ENOTSUP         = -9,   /**< Not supported */
    UFT_IPF_EVERSION        = -10,  /**< Unsupported version */
    UFT_IPF_ETRUNCATED      = -11,  /**< Truncated record */
    UFT_IPF_EMAGIC          = -12   /**< Invalid magic */
} uft_ipf_err_t;

/** Get human-readable error string */
const char *uft_ipf_strerror(uft_ipf_err_t err);

/* ═══════════════════════════════════════════════════════════════════════════════
 * IPF Record Types (Known from public documentation)
 * 
 * IPF files consist of records, each with a 12-byte header:
 *   [type:4][length:4][crc:4][data:length]
 * 
 * All values are big-endian.
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum uft_ipf_record_type {
    /* Core record types */
    UFT_IPF_REC_CAPS        = 0x43415053,  /**< 'CAPS' - File header/magic */
    UFT_IPF_REC_INFO        = 0x494E464F,  /**< 'INFO' - Image info record */
    UFT_IPF_REC_IMGE        = 0x494D4745,  /**< 'IMGE' - Image descriptor */
    UFT_IPF_REC_DATA        = 0x44415441,  /**< 'DATA' - Track data */
    UFT_IPF_REC_TRCK        = 0x5452434B,  /**< 'TRCK' - Track descriptor */
    UFT_IPF_REC_CTEI        = 0x43544549,  /**< 'CTEI' - CT Editor Info */
    UFT_IPF_REC_CTEX        = 0x43544558,  /**< 'CTEX' - CT Extension */
    UFT_IPF_REC_DUMP        = 0x44554D50,  /**< 'DUMP' - Raw dump data */
    
    /* Extended/optional types */
    UFT_IPF_REC_COMM        = 0x434F4D4D,  /**< 'COMM' - Comment */
    UFT_IPF_REC_TEXT        = 0x54455854,  /**< 'TEXT' - Text data */
    UFT_IPF_REC_USER        = 0x55534552,  /**< 'USER' - User-defined */
    
    UFT_IPF_REC_UNKNOWN     = 0x00000000   /**< Unknown type */
} uft_ipf_record_type_t;

/** Get record type name as string */
const char *uft_ipf_record_type_name(uint32_t type);

/** Check if record type is known */
bool uft_ipf_record_type_known(uint32_t type);

/* ═══════════════════════════════════════════════════════════════════════════════
 * IPF INFO Record Structure (parsed)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct uft_ipf_info {
    uint32_t media_type;      /**< Media type (0=unknown, 1=floppy, etc.) */
    uint32_t encoder_type;    /**< Encoder used */
    uint32_t encoder_rev;     /**< Encoder revision */
    uint32_t file_key;        /**< Unique file identifier */
    uint32_t file_rev;        /**< File revision */
    uint32_t origin;          /**< CRC of original file */
    uint32_t min_track;       /**< Minimum track number */
    uint32_t max_track;       /**< Maximum track number */
    uint32_t min_side;        /**< Minimum side (0 or 1) */
    uint32_t max_side;        /**< Maximum side (0 or 1) */
    uint32_t creation_date;   /**< Creation date (DOS format) */
    uint32_t creation_time;   /**< Creation time (DOS format) */
    uint32_t platforms;       /**< Platform flags */
    uint32_t disk_number;     /**< Disk number (for multi-disk) */
    uint32_t creator_id;      /**< Creator ID */
    uint32_t reserved[3];     /**< Reserved fields */
    bool     parsed;          /**< True if successfully parsed */
} uft_ipf_info_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * IPF IMGE Record Structure (parsed)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct uft_ipf_imge {
    uint32_t track;           /**< Track number */
    uint32_t side;            /**< Side (0 or 1) */
    uint32_t density;         /**< Density type */
    uint32_t signal_type;     /**< Signal type (cell, sample) */
    uint32_t track_bytes;     /**< Track size in bytes */
    uint32_t start_byte_pos;  /**< Start position */
    uint32_t start_bit_pos;   /**< Start bit position */
    uint32_t data_bits;       /**< Data bits count */
    uint32_t gap_bits;        /**< Gap bits count */
    uint32_t track_bits;      /**< Total track bits */
    uint32_t block_count;     /**< Number of blocks */
    uint32_t encoder_process; /**< Encoder process used */
    uint32_t flags;           /**< Image flags */
    uint32_t data_key;        /**< Key to DATA record */
    bool     parsed;          /**< True if successfully parsed */
} uft_ipf_imge_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Platform Flags (for INFO record)
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_IPF_PLATFORM_AMIGA_OCS      (1 << 0)
#define UFT_IPF_PLATFORM_AMIGA_ECS      (1 << 1)
#define UFT_IPF_PLATFORM_AMIGA_AGA      (1 << 2)
#define UFT_IPF_PLATFORM_ATARI_ST       (1 << 3)
#define UFT_IPF_PLATFORM_ATARI_STE      (1 << 4)
#define UFT_IPF_PLATFORM_PC_DOS         (1 << 5)
#define UFT_IPF_PLATFORM_PC_WINDOWS     (1 << 6)
#define UFT_IPF_PLATFORM_AMSTRAD_CPC    (1 << 7)
#define UFT_IPF_PLATFORM_SPECTRUM       (1 << 8)
#define UFT_IPF_PLATFORM_SAM_COUPE      (1 << 9)
#define UFT_IPF_PLATFORM_ARCHIMEDES     (1 << 10)
#define UFT_IPF_PLATFORM_C64            (1 << 11)
#define UFT_IPF_PLATFORM_C128           (1 << 12)

/** Get platform name string */
const char *uft_ipf_platform_name(uint32_t platform);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Media Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum uft_ipf_media_type {
    UFT_IPF_MEDIA_UNKNOWN       = 0,
    UFT_IPF_MEDIA_FLOPPY_DD     = 1,    /**< Double density floppy */
    UFT_IPF_MEDIA_FLOPPY_HD     = 2,    /**< High density floppy */
    UFT_IPF_MEDIA_FLOPPY_ED     = 3     /**< Extended density floppy */
} uft_ipf_media_type_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Density Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum uft_ipf_density {
    UFT_IPF_DENSITY_UNKNOWN     = 0,
    UFT_IPF_DENSITY_NOISE       = 1,
    UFT_IPF_DENSITY_AUTO        = 2,
    UFT_IPF_DENSITY_AMIGA_DD    = 3,    /**< Amiga DD (2µs cells) */
    UFT_IPF_DENSITY_AMIGA_HD    = 4,    /**< Amiga HD (1µs cells) */
    UFT_IPF_DENSITY_ATARI_DD    = 5,    /**< Atari ST DD */
    UFT_IPF_DENSITY_PC_DD       = 6,    /**< PC DD (300 RPM) */
    UFT_IPF_DENSITY_PC_HD       = 7,    /**< PC HD (360 RPM) */
    UFT_IPF_DENSITY_C64         = 8,    /**< Commodore 64 GCR */
    UFT_IPF_DENSITY_APPLE_GCR   = 9     /**< Apple GCR */
} uft_ipf_density_t;

/** Get density name string */
const char *uft_ipf_density_name(uint32_t density);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Record Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct uft_ipf_record {
    uint32_t type;            /**< Record type (FourCC as uint32) */
    uint32_t length;          /**< Data length (not including header) */
    uint32_t crc;             /**< CRC32 of data */
    uint64_t header_offset;   /**< Offset of record header in file */
    uint64_t data_offset;     /**< Offset of data in file */
    uint32_t index;           /**< Record index in file */
    bool     crc_valid;       /**< True if CRC was verified */
    bool     crc_checked;     /**< True if CRC was checked */
} uft_ipf_record_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Container Handle
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct uft_ipf {
    /* File info */
    FILE            *fp;              /**< File handle */
    uint64_t         file_size;       /**< Total file size */
    char             path[260];       /**< File path */
    
    /* Format detection */
    bool             is_valid_ipf;    /**< True if valid IPF detected */
    uint32_t         format_version;  /**< Detected format version */
    
    /* Records */
    uft_ipf_record_t *records;        /**< Parsed records */
    size_t            record_count;   /**< Number of records */
    
    /* Parsed structures */
    uft_ipf_info_t   info;            /**< Parsed INFO record */
    uft_ipf_imge_t  *images;          /**< Parsed IMGE records */
    size_t           image_count;     /**< Number of IMGE records */
    
    /* Statistics */
    size_t           data_records;    /**< Count of DATA records */
    size_t           track_records;   /**< Count of TRCK records */
    size_t           unknown_records; /**< Count of unknown records */
    uint64_t         total_data_size; /**< Total data bytes */
    
    /* Diagnostics */
    uint32_t         warnings;        /**< Warning flags */
    char             last_error[256]; /**< Last error message */
} uft_ipf_t;

/* Warning flags */
#define UFT_IPF_WARN_CRC_MISMATCH       (1 << 0)
#define UFT_IPF_WARN_TRUNCATED          (1 << 1)
#define UFT_IPF_WARN_UNKNOWN_RECORDS    (1 << 2)
#define UFT_IPF_WARN_MISSING_INFO       (1 << 3)
#define UFT_IPF_WARN_MISSING_IMGE       (1 << 4)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Reader API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Open and parse IPF file
 * @param ctx Context to initialize
 * @param path File path
 * @return UFT_IPF_OK on success
 */
uft_ipf_err_t uft_ipf_open(uft_ipf_t *ctx, const char *path);

/**
 * @brief Close IPF file and free resources
 */
void uft_ipf_close(uft_ipf_t *ctx);

/**
 * @brief Validate container integrity
 * @param ctx Opened context
 * @param check_crc Also verify CRC32 values
 * @return UFT_IPF_OK if valid
 */
uft_ipf_err_t uft_ipf_validate(uft_ipf_t *ctx, bool check_crc);

/**
 * @brief Get number of records
 */
size_t uft_ipf_record_count(const uft_ipf_t *ctx);

/**
 * @brief Get record at index
 * @return NULL if out of bounds
 */
const uft_ipf_record_t *uft_ipf_record_at(const uft_ipf_t *ctx, size_t idx);

/**
 * @brief Find first record by type
 * @return Record index or SIZE_MAX if not found
 */
size_t uft_ipf_find_record(const uft_ipf_t *ctx, uint32_t type);

/**
 * @brief Find next record by type after given index
 */
size_t uft_ipf_find_next_record(const uft_ipf_t *ctx, uint32_t type, size_t after);

/**
 * @brief Read record data into buffer
 * @param ctx Context
 * @param idx Record index
 * @param buf Output buffer
 * @param buf_sz Buffer size
 * @param out_read Bytes actually read (optional)
 * @return UFT_IPF_OK on success
 */
uft_ipf_err_t uft_ipf_read_record(const uft_ipf_t *ctx, size_t idx,
                                   void *buf, size_t buf_sz, size_t *out_read);

/**
 * @brief Get parsed INFO record
 * @return NULL if not available
 */
const uft_ipf_info_t *uft_ipf_get_info(const uft_ipf_t *ctx);

/**
 * @brief Get parsed IMGE record for track/side
 * @return NULL if not found
 */
const uft_ipf_imge_t *uft_ipf_get_image(const uft_ipf_t *ctx, 
                                         uint32_t track, uint32_t side);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Writer API
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct uft_ipf_writer {
    FILE        *fp;
    uint64_t     bytes_written;
    uint32_t     record_count;
    bool         header_written;
} uft_ipf_writer_t;

uft_ipf_err_t uft_ipf_writer_open(uft_ipf_writer_t *w, const char *path);
uft_ipf_err_t uft_ipf_writer_write_header(uft_ipf_writer_t *w);
uft_ipf_err_t uft_ipf_writer_add_info(uft_ipf_writer_t *w, const uft_ipf_info_t *info);
uft_ipf_err_t uft_ipf_writer_add_record(uft_ipf_writer_t *w, uint32_t type,
                                         const void *data, uint32_t length);
uft_ipf_err_t uft_ipf_writer_close(uft_ipf_writer_t *w);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** Calculate CRC32 (IEEE polynomial) */
uint32_t uft_ipf_crc32(const void *data, size_t len);

/** Verify CRC32 of record */
bool uft_ipf_verify_record_crc(const uft_ipf_t *ctx, size_t idx);

/** Dump container info to file */
void uft_ipf_dump(const uft_ipf_t *ctx, FILE *out, bool verbose);

/** Get summary statistics */
void uft_ipf_get_stats(const uft_ipf_t *ctx, 
                       size_t *total_records,
                       size_t *data_records,
                       size_t *track_records,
                       uint64_t *total_bytes);

/** Format record type as string (static buffer) */
const char *uft_ipf_type_to_string(uint32_t type);

/** Parse type string to uint32 */
uint32_t uft_ipf_string_to_type(const char *str);

/** Check if file looks like IPF (quick probe) */
bool uft_ipf_probe(const char *path);

/** Check if file handle looks like IPF */
bool uft_ipf_probe_fp(FILE *fp);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Diagnostic Callback
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum uft_ipf_diag_level {
    UFT_IPF_DIAG_DEBUG   = 0,
    UFT_IPF_DIAG_INFO    = 1,
    UFT_IPF_DIAG_WARNING = 2,
    UFT_IPF_DIAG_ERROR   = 3
} uft_ipf_diag_level_t;

typedef void (*uft_ipf_diag_callback_t)(uft_ipf_diag_level_t level, 
                                         const char *message, 
                                         void *user_data);

/** Set diagnostic callback */
void uft_ipf_set_diag_callback(uft_ipf_diag_callback_t cb, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* UFT_IPF_H */
