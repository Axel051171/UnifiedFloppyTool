/**
 * @file uft_unified_types.h
 * @brief Unified data types for all UFT modules
 * 
 * P0-001: Unified Sector-ID Struct
 * P0-002: Unified Track-Struct
 * P0-004: API compatibility layer
 * 
 * This header defines the canonical data structures that ALL modules
 * must use. Legacy structures are deprecated and will be removed.
 * 
 * MIGRATION: Use UFT_COMPAT_LEGACY_TYPES to enable compatibility macros
 */

#ifndef UFT_UNIFIED_TYPES_H
#define UFT_UNIFIED_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "uft/uft_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Version of this type system */
#define UFT_TYPES_VERSION 0x0100
#define UFT_TYPES_VERSION_STR "1.0"

/* ============================================================================
 * Error Codes — delegated to the canonical definition.
 *
 * PREVIOUSLY (pre Must-Fix-Hunter F1): this file defined its own
 * uft_error_t enum with POSITIVE hex values (UFT_ERR_IO = 0x80,
 * UFT_ERR_MEMORY = 0x81, UFT_ERR_INTERNAL = 0xFF, …) while the sister
 * file include/uft/uft_error.h defined the same identifiers with
 * NEGATIVE decimal values (UFT_ERR_IO = -10, UFT_ERR_MEMORY = -30,
 * UFT_ERR_INTERNAL = -90, …). Both blocks were guarded by the same
 * UFT_ERROR_ENUM_DEFINED sentinel — whichever header was included
 * first won, and downstream hex-alias macros clobbered enum members
 * in the other TUs. Silent ABI divergence depending on include order.
 *
 * FIX: the canonical enum lives in uft/uft_error.h. This header just
 * pulls it in so callers that include only unified_types.h still get
 * a consistent uft_error_t. Extension names that don't exist in
 * uft_error.h (FILE_OPEN, INVALID_STATE, OUT_OF_MEMORY, the UFT_ERROR_*
 * mixed-case aliases, the SYNC_LOST / WEAK_BITS / PROTECTION families)
 * are mapped to the closest uft_error.h enum member using the enum
 * identifier — NOT a hex number — so they resolve to the canonical
 * value regardless of include order.
 * ============================================================================ */

#include "uft/uft_error.h"

/* Prevent uft_error_compat.h from redefining enum values as macros. */
#define UFT_HAS_UNIFIED_ERROR_ENUM 1

/* Extension aliases: names used by some call sites but not present in
 * uft_error.h. Each #define references the canonical enum identifier
 * (not a hex value) — that way the alias always equals the enum value,
 * no matter what order headers get pulled in.
 *
 * Pre-fix ABI note: old numeric values below in comments for archaeology.
 * Any code that compared raw hex (e.g. `if (rc == 0x82)`) was already
 * broken half the time; it now fails loudly and gets caught. */
#ifndef UFT_ERR_FILE_OPEN
#define UFT_ERR_FILE_OPEN       UFT_ERR_IO            /* was 0x80 */
#endif
#ifndef UFT_ERR_FILE_READ
#define UFT_ERR_FILE_READ       UFT_ERR_IO            /* was 0x80 */
#endif
#ifndef UFT_ERR_FILE_WRITE
#define UFT_ERR_FILE_WRITE      UFT_ERR_IO            /* was 0x80 */
#endif
#ifndef UFT_ERR_IO_ERROR
#define UFT_ERR_IO_ERROR        UFT_ERR_IO            /* was 0x80 */
#endif
#ifndef UFT_ERR_INVALID_STATE
#define UFT_ERR_INVALID_STATE   UFT_ERR_INVALID_ARG   /* was 0x82 */
#endif
#ifndef UFT_ERR_INVALID_PARAM
#define UFT_ERR_INVALID_PARAM   UFT_ERR_INVALID_ARG   /* was 0x82 */
#endif
#ifndef UFT_ERR_OUT_OF_MEMORY
#define UFT_ERR_OUT_OF_MEMORY   UFT_ERR_MEMORY        /* was 0x81 */
#endif
#ifndef UFT_ERR_FORMAT_ERROR
#define UFT_ERR_FORMAT_ERROR    UFT_ERR_FORMAT        /* was 0x62 */
#endif
#ifndef UFT_ERR_CRC_ERROR
#define UFT_ERR_CRC_ERROR       UFT_ERR_CRC           /* was 0x01 */
#endif
#ifndef UFT_ERR_CORRUPT
#define UFT_ERR_CORRUPT         UFT_ERR_CORRUPTED     /* was 0x62 */
#endif
#ifndef UFT_ERR_UNSUPPORTED
#define UFT_ERR_UNSUPPORTED     UFT_ERR_NOT_SUPPORTED /* was 0x61 */
#endif
#ifndef UFT_ERR_NOT_IMPL
#define UFT_ERR_NOT_IMPL        UFT_ERR_NOT_IMPLEMENTED /* was 0x83 */
#endif
#ifndef UFT_ERR_UNKNOWN_FORMAT
#define UFT_ERR_UNKNOWN_FORMAT  UFT_ERR_FORMAT        /* was 0x60 */
#endif
#ifndef UFT_ERR_NO_DATA
#define UFT_ERR_NO_DATA         UFT_ERR_CORRUPTED     /* was 0x03 */
#endif
#ifndef UFT_ERR_ENCODING
#define UFT_ERR_ENCODING        UFT_ERR_FORMAT        /* was 0x0B */
#endif
#ifndef UFT_ERR_NOT_FOUND
#define UFT_ERR_NOT_FOUND       UFT_ERR_FILE_NOT_FOUND /* was 0x03 */
#endif

/* Read-/write-/protection-error sub-taxonomy from the old positive enum.
 * Kept so call sites that reference them still compile; each maps to the
 * closest canonical member. */
#ifndef UFT_ERR_SYNC_LOST
#define UFT_ERR_SYNC_LOST       UFT_ERR_CORRUPTED     /* was 0x02 */
#endif
#ifndef UFT_ERR_WEAK_BITS
#define UFT_ERR_WEAK_BITS       UFT_ERR_CORRUPTED     /* was 0x04 */
#endif
#ifndef UFT_ERR_TIMING
#define UFT_ERR_TIMING          UFT_ERR_CORRUPTED     /* was 0x05 */
#endif
#ifndef UFT_ERR_ID_MISMATCH
#define UFT_ERR_ID_MISMATCH     UFT_ERR_CORRUPTED     /* was 0x06 */
#endif
#ifndef UFT_ERR_DELETED_DATA
#define UFT_ERR_DELETED_DATA    UFT_ERR_CORRUPTED     /* was 0x07 */
#endif
#ifndef UFT_ERR_MISSING_SECTOR
#define UFT_ERR_MISSING_SECTOR  UFT_ERR_FILE_NOT_FOUND /* was 0x08 */
#endif
#ifndef UFT_ERR_INCOMPLETE
#define UFT_ERR_INCOMPLETE      UFT_ERR_CORRUPTED     /* was 0x09 */
#endif
#ifndef UFT_ERR_PLL_UNLOCK
#define UFT_ERR_PLL_UNLOCK      UFT_ERR_CORRUPTED     /* was 0x0A */
#endif
#ifndef UFT_ERR_WRITE_PROTECT
#define UFT_ERR_WRITE_PROTECT   UFT_ERR_NOT_PERMITTED /* was 0x20 */
#endif
#ifndef UFT_ERR_VERIFY_FAIL
#define UFT_ERR_VERIFY_FAIL     UFT_ERR_CORRUPTED     /* was 0x21 */
#endif
#ifndef UFT_ERR_WRITE_FAULT
#define UFT_ERR_WRITE_FAULT     UFT_ERR_HARDWARE      /* was 0x22 */
#endif
#ifndef UFT_ERR_TRACK_OVERFLOW
#define UFT_ERR_TRACK_OVERFLOW  UFT_ERR_BUFFER_TOO_SMALL /* was 0x23 */
#endif
#ifndef UFT_ERR_PROTECTION
#define UFT_ERR_PROTECTION      UFT_ERR_NOT_PERMITTED /* was 0x40 */
#endif
#ifndef UFT_ERR_COPY_DENIED
#define UFT_ERR_COPY_DENIED     UFT_ERR_NOT_PERMITTED /* was 0x41 */
#endif
#ifndef UFT_ERR_LONG_TRACK
#define UFT_ERR_LONG_TRACK      UFT_ERR_FORMAT        /* was 0x42 */
#endif
#ifndef UFT_ERR_NON_STANDARD
#define UFT_ERR_NON_STANDARD    UFT_ERR_FORMAT_VARIANT /* was 0x43 */
#endif
#ifndef UFT_ERR_VERSION
#define UFT_ERR_VERSION         UFT_ERR_FORMAT_VARIANT /* was 0x63 */
#endif
#ifndef UFT_ERR_CANCELLED
#define UFT_ERR_CANCELLED       UFT_ERR_NOT_PERMITTED /* was 0x85 */
#endif

/* Mixed-case UFT_ERROR_* aliases (legacy, several call sites use these). */
#ifndef UFT_ERROR_INVALID_PARAM
#define UFT_ERROR_INVALID_PARAM UFT_ERR_INVALID_ARG
#endif
#ifndef UFT_ERROR_NO_MEMORY
#define UFT_ERROR_NO_MEMORY     UFT_ERR_MEMORY
#endif
#ifndef UFT_ERROR_NOT_SUPPORTED
#define UFT_ERROR_NOT_SUPPORTED UFT_ERR_NOT_SUPPORTED
#endif
#ifndef UFT_ERROR_NOT_FOUND
#define UFT_ERROR_NOT_FOUND     UFT_ERR_FILE_NOT_FOUND
#endif
#ifndef UFT_ERROR_IO
#define UFT_ERROR_IO            UFT_ERR_IO
#endif
#ifndef UFT_ERROR_FORMAT
#define UFT_ERROR_FORMAT        UFT_ERR_FORMAT
#endif
#ifndef UFT_ERROR_CRC
#define UFT_ERROR_CRC           UFT_ERR_CRC
#endif
#ifndef UFT_ERROR_DECODE
#define UFT_ERROR_DECODE        UFT_ERR_FORMAT
#endif
#ifndef UFT_ERROR_INTERNAL
#define UFT_ERROR_INTERNAL      UFT_ERR_INTERNAL
#endif
#ifndef UFT_ERROR_INVALID_FORMAT
#define UFT_ERROR_INVALID_FORMAT UFT_ERR_FORMAT
#endif
#ifndef UFT_ERROR_NO_DATA
#define UFT_ERROR_NO_DATA       UFT_ERR_CORRUPTED
#endif

/* Note: UFT_ERC_FORMAT (typo for UFT_ERR_FORMAT) had 0 callers —
 * deleted per F12. UFT_HAS_UNIFIED_ERROR_ENUM still defined above
 * for compat with uft_error_compat.h. */

/**
 * @brief Get error description string
 */
const char* uft_error_str(uft_error_t err);

/**
 * @brief Check if error is recoverable
 */
bool uft_error_recoverable(uft_error_t err);

/* ============================================================================
 * Sector Identification (Unified - P0-001)
 * ============================================================================ */

/**
 * @brief Unified sector identification
 * 
 * This structure replaces:
 * - flux_sector_id_t (flux_decode.h)
 * - xcopy_sector_t (xcopy_protection.h)
 * - c64_sector_id_t (c64_protection.h)
 * - mfm_idam_t (mfm_decode.h)
 */
#ifndef UFT_SECTOR_ID_T_DEFINED
#define UFT_SECTOR_ID_T_DEFINED
typedef struct {
    uint16_t track;           /**< Physical track number (0-83+) */
    uint8_t  head;            /**< Head/side (0-1) */
    uint8_t  sector;          /**< Logical sector number */
    uint8_t  size_code;       /**< Size code: 0=128, 1=256, 2=512, 3=1024... */
    uint8_t  status;          /**< Status flags (UFT_SECTOR_*) */
    uint8_t  encoding;        /**< Encoding type (UFT_ENC_*) */
    uint8_t  reserved;        /**< Reserved for alignment */
} uft_sector_id_t;
#endif /* UFT_SECTOR_ID_T_DEFINED */

/* Sector status and encoding constants are intentionally NOT defined here
 * as macros. They are defined as enum members in uft_types.h and other
 * headers. Defining them as macros would clobber enum member names when
 * this header is included before those that define the enums.
 *
 * Use the uft_sector_status_t and uft_encoding_t enums from the
 * appropriate header instead. The uint8_t status/encoding fields in the
 * structs below accept the integer values from those enums. */

/* Sector ID comparison -- works with all uft_sector_id_t variants.
 * All variants have the same first 3 fields (track/cylinder, head, sector)
 * as uint8_t at the same offsets. */
#ifndef UFT_SECTOR_ID_EQUAL_DEFINED
#define UFT_SECTOR_ID_EQUAL_DEFINED
static inline bool uft_sector_id_equal(const uft_sector_id_t *a,
                                       const uft_sector_id_t *b) {
    if (!a || !b) return false;
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    return pa[0] == pb[0] && pa[1] == pb[1] && pa[2] == pb[2];
}
#endif /* UFT_SECTOR_ID_EQUAL_DEFINED */

/* Compatibility macros for legacy code */
#ifdef UFT_COMPAT_LEGACY_TYPES
#ifndef UFT_SECTOR_CYLINDER
#define UFT_SECTOR_CYLINDER(id)  (((const uint8_t*)(id))[0])
#define UFT_SECTOR_SIDE(id)      ((id)->head)
#define UFT_SECTOR_NUM(id)       ((id)->sector)
#define UFT_SECTOR_SIZE(id)      (128U << (id)->size_code)
#endif
#endif

/* uft_sector_t — canonical definition in uft/uft_types.h (included above) */

/* uft_track_t — canonical definition in uft/uft_format_plugin.h
 * (available via uft/uft_types.h forward decl + uft/uft_format_plugin.h full def) */
#include "uft/uft_format_plugin.h"

/* ============================================================================
 * Disk Image (Unified)
 * ============================================================================ */

/**
 * @brief Format identifier
 */
#ifndef UFT_FORMAT_ID_T_DEFINED
#define UFT_FORMAT_ID_T_DEFINED
typedef enum {
    UFT_FMT_UNKNOWN = 0,
    
    /* Sector-based */
    UFT_FMT_IMG,
    UFT_FMT_IMA,
    UFT_FMT_DSK,
    UFT_FMT_D64,
    UFT_FMT_D71,
    UFT_FMT_D81,
    UFT_FMT_D82,
    UFT_FMT_ADF,
    UFT_FMT_MSA,
    UFT_FMT_ST,
    UFT_FMT_ATR,
    UFT_FMT_XFD,
    
    /* Extended/Metadata */
    UFT_FMT_G64,
    UFT_FMT_NIB,
    UFT_FMT_DMK,
    UFT_FMT_TD0,
    UFT_FMT_IMD,
    UFT_FMT_EDSK,
    UFT_FMT_HFE,
    UFT_FMT_IPF,
    UFT_FMT_FDI,
    UFT_FMT_CQM,
    
    /* Flux */
    UFT_FMT_SCP,
    UFT_FMT_A2R,
    UFT_FMT_WOZ,
    UFT_FMT_UFT_KF_RAW,
    UFT_FMT_GWRAW,
    UFT_FMT_MOOF,
    
    /* Japanese */
    UFT_FMT_D88,
    UFT_FMT_NFD,
    UFT_FMT_FDD,
    UFT_FMT_HDM,
    
    UFT_FMT_MAX
} uft_format_id_t;
#endif /* UFT_FORMAT_ID_T_DEFINED */

/**
 * @brief Protection type
 */
typedef enum {
    UFT_PROT_NONE = 0,
    
    /* C64 */
    UFT_PROT_RAPIDLOK = 0x0100,
    UFT_PROT_RAPIDLOK2 = 0x0101,
    UFT_PROT_RAPIDLOK6 = 0x0102,
    UFT_PROT_VORPAL = 0x0200,
    UFT_PROT_VMAX = 0x0300,
    UFT_PROT_VMAX3 = 0x0301,
    UFT_PROT_EA = 0x0400,
    UFT_PROT_GEOS = 0x0500,
    
    /* Amiga */
    UFT_PROT_COPYLOCK = 0x1000,
    UFT_PROT_LONG_TRACK = 0x1100,
    UFT_PROT_WEAK_BITS_AMIGA = 0x1200,
    
    /* Apple */
    UFT_PROT_NIBBLE_COUNT = 0x2000,
    UFT_PROT_SPIRAL = 0x2100,
    
    /* PC */
    UFT_PROT_WEAK_BITS_PC = 0x3000,
    UFT_PROT_XDF = 0x3100,
    
} uft_protection_t;

/**
 * @brief Protection information
 */
typedef struct {
    uft_protection_t type;
    uint8_t confidence;       /**< Detection confidence 0-100 */
    uint8_t track_start;      /**< First protected track */
    uint8_t track_end;        /**< Last protected track */
    const char *name;         /**< Protection name */
    const char *description;  /**< Description */
    
    /* Preservation requirements */
    bool requires_flux;
    bool requires_timing;
    bool requires_weak_bits;
    bool requires_long_tracks;
    
} uft_protection_info_t;

/**
 * @brief Unified disk image structure
 */
typedef struct {
    /* Format info */
    uft_format_id_t format;
    char format_name[32];
    
    /* Geometry */
    uint16_t tracks;          /**< Total tracks */
    uint8_t  heads;           /**< Number of heads (1-2) */
    uint8_t  sectors_per_track; /**< 0 = variable */
    uint16_t bytes_per_sector;  /**< 0 = variable */
    
    /* Track data [track * heads + head] */
    uft_track_t **track_data;
    size_t track_count;
    
    /* Protection info */
    uft_protection_info_t protection;
    
    /* Forensic info */
    struct {
        bool has_errors;
        bool has_weak_bits;
        bool has_timing;
        uint32_t bad_sector_count;
        uint32_t recovered_count;
    } forensic;
    
    /* Multi-revision */
    uint8_t revision_count;
    
    /* File info */
    char *source_path;
    uint64_t file_size;
    uint32_t file_crc32;
    
    /* Ownership */
    bool owns_data;
    
} uft_disk_image_t;

/* ============================================================================
 * Memory Management
 * ============================================================================ */

/**
 * @brief Allocate sector structure
 */
uft_sector_t* uft_sector_alloc(size_t data_len);

/**
 * @brief Free sector structure
 */
void uft_sector_free(uft_sector_t *sector);

/**
 * @brief Allocate track structure
 */
uft_track_t* uft_track_alloc(size_t max_sectors, size_t max_raw_bits);

/**
 * @brief Free track structure
 */
void uft_track_free(uft_track_t *track);

/**
 * @brief Allocate disk image structure
 */
uft_disk_image_t* uft_disk_alloc(uint16_t tracks, uint8_t heads);

/**
 * @brief Free disk image structure
 */
void uft_disk_free(uft_disk_image_t *disk);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get format name string
 */
const char* uft_format_name(uft_format_id_t format);

/**
 * @brief Get encoding name string
 */
#ifndef UFT_ENCODING_NAME_DECLARED
#define UFT_ENCODING_NAME_DECLARED
const char* uft_encoding_name(uint8_t encoding);
#endif /* UFT_ENCODING_NAME_DECLARED */

/**
 * @brief Calculate sector size from size code
 */
static inline size_t uft_size_from_code(uint8_t code) {
    return (code < 8) ? (128U << code) : 0;
}

/**
 * @brief Calculate size code from sector size
 */
static inline uint8_t uft_code_from_size(size_t size) {
    switch (size) {
        case 128:  return 0;
        case 256:  return 1;
        case 512:  return 2;
        case 1024: return 3;
        case 2048: return 4;
        case 4096: return 5;
        case 8192: return 6;
        default:   return 2;  /* Default to 512 */
    }
}

/**
 * @brief Deep copy sector
 */
int uft_sector_copy(uft_sector_t *dest, const uft_sector_t *src);

/**
 * @brief Deep copy track
 */
int uft_track_copy(uft_track_t *dest, const uft_track_t *src);

/**
 * @brief Compare two disks
 */
typedef enum {
    UFT_CMP_IDENTICAL = 0,
    UFT_CMP_DATA_DIFFERS = 1,
    UFT_CMP_GEOMETRY_DIFFERS = 2,
    UFT_CMP_METADATA_DIFFERS = 4,
} uft_compare_result_t;

/* Legacy compare — new callers should use the Master-API uft_disk_compare
 * from include/uft/uft_disk_compare.h (operates on uft_disk_t*). */
int uft_disk_image_compare(const uft_disk_image_t *a,
                            const uft_disk_image_t *b,
                            uft_compare_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* UFT_UNIFIED_TYPES_H */
