/**
 * @file uft_ipf_caps.h
 * @brief CAPS/SPS Library Definitions & Adapter
 * 
 * Official definitions from SPS (Software Preservation Society)
 * Based on ipflib 4.2 and spsdeclib 5.1 headers.
 * 
 * This header provides:
 * 1. Official CAPS type definitions
 * 2. Platform/Track/Encoder enums
 * 3. Error codes
 * 4. Dynamic library loading interface
 * 
 * @version 1.0.0
 * @date 2025-01-08
 * @see http://www.softpres.org
 */

#ifndef UFT_IPF_CAPS_H
#define UFT_IPF_CAPS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * CAPS Type Definitions (from official headers)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef uint8_t  CapsUByte;
typedef int32_t  CapsLong;
typedef uint32_t CapsULong;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define CAPS_FILEEXT     "ipf"
#define CAPS_FILEPFX     ".ipf"
#define CAPS_MAXPLATFORM 4
#define CAPS_MTRS        5      /**< Max track revolutions */

/* ═══════════════════════════════════════════════════════════════════════════════
 * Lock Flags
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define DI_LOCK_INDEX    (1UL << 0)   /**< Re-align data as index synced recording */
#define DI_LOCK_ALIGN    (1UL << 1)   /**< Decode track to word aligned size */
#define DI_LOCK_DENVAR   (1UL << 2)   /**< Generate cell density for variable density tracks */
#define DI_LOCK_DENAUTO  (1UL << 3)   /**< Generate density for automatically sized cells */
#define DI_LOCK_DENNOISE (1UL << 4)   /**< Generate density for unformatted cells */
#define DI_LOCK_NOISE    (1UL << 5)   /**< Generate unformatted data */
#define DI_LOCK_NOISEREV (1UL << 6)   /**< Generate unformatted data that changes each revolution */
#define DI_LOCK_MEMREF   (1UL << 7)   /**< Directly use source memory buffer */
#define DI_LOCK_UPDATEFD (1UL << 8)   /**< Flakey/weak data updated with each lock */
#define DI_LOCK_TYPE     (1UL << 9)   /**< Info.type holds expected structure type */
#define DI_LOCK_DENALT   (1UL << 10)  /**< Alternate density map as fractions */
#define DI_LOCK_OVLBIT   (1UL << 11)  /**< Overlap position is in bits */
#define DI_LOCK_TRKBIT   (1UL << 12)  /**< Tracklen is in bits */
#define DI_LOCK_NOUPDATE (1UL << 13)  /**< Track overlap/weak data never updated */
#define DI_LOCK_SETWSEED (1UL << 14)  /**< Set weak bit generator seed value */

#define CTIT_FLAG_FLAKEY (1UL << 31)
#define CTIT_MASK_TYPE   0xFF

/* ═══════════════════════════════════════════════════════════════════════════════
 * Platform IDs (Official SPS values)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum caps_platform {
    CAPS_PLATFORM_NA          = 0,   /**< Invalid platform */
    CAPS_PLATFORM_AMIGA       = 1,   /**< Commodore Amiga */
    CAPS_PLATFORM_ATARI_ST    = 2,   /**< Atari ST */
    CAPS_PLATFORM_PC          = 3,   /**< IBM PC compatible */
    CAPS_PLATFORM_AMSTRAD_CPC = 4,   /**< Amstrad CPC */
    CAPS_PLATFORM_SPECTRUM    = 5,   /**< ZX Spectrum */
    CAPS_PLATFORM_SAM_COUPE   = 6,   /**< Sam Coupé */
    CAPS_PLATFORM_ARCHIMEDES  = 7,   /**< Acorn Archimedes */
    CAPS_PLATFORM_C64         = 8,   /**< Commodore 64 */
    CAPS_PLATFORM_ATARI_8     = 9    /**< Atari 8-bit */
} caps_platform_t;

/** Get platform name string */
const char *caps_platform_name(caps_platform_t platform);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Image Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum caps_image_type {
    CAPS_IMAGE_NA  = 0,   /**< Invalid image type */
    CAPS_IMAGE_FDD = 1    /**< Floppy disk */
} caps_image_type_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Track Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum caps_track_type {
    CAPS_TRACK_NA    = 0,   /**< Invalid type */
    CAPS_TRACK_NOISE = 1,   /**< Cells are unformatted (random size) */
    CAPS_TRACK_AUTO  = 2,   /**< Automatic cell size */
    CAPS_TRACK_VAR   = 3    /**< Variable density */
} caps_track_type_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Cell Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum caps_cell_type {
    CAPS_CELL_NA  = 0,   /**< Invalid cell type */
    CAPS_CELL_2US = 1    /**< 2µs cells (standard MFM) */
} caps_cell_type_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Encoder Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum caps_encoder_type {
    CAPS_ENC_NA  = 0,   /**< Undefined encoder */
    CAPS_ENC_MFM = 1,   /**< MFM encoding */
    CAPS_ENC_RAW = 2    /**< Raw (no encoding, test data only) */
} caps_encoder_type_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Gap Size Modes
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum caps_gap_mode {
    CAPS_GAP_FIXED  = 0,   /**< Fixed size, can't be changed */
    CAPS_GAP_AUTO   = 1,   /**< Size can be changed, resize info calculated automatically */
    CAPS_GAP_RESIZE = 2    /**< Size can be changed, resize info is scripted */
} caps_gap_mode_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Data Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum caps_data_type {
    CAPS_DATA_NA   = 0,   /**< Undefined */
    CAPS_DATA_WEAK = 1    /**< Weak bits */
} caps_data_type_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Error Codes (Official SPS values)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum caps_error {
    CAPS_OK              = 0,
    CAPS_UNSUPPORTED     = 1,
    CAPS_GENERIC         = 2,
    CAPS_OUT_OF_RANGE    = 3,
    CAPS_READ_ONLY       = 4,
    CAPS_OPEN            = 5,
    CAPS_TYPE            = 6,
    CAPS_SHORT           = 7,
    CAPS_TRACK_HEADER    = 8,
    CAPS_TRACK_STREAM    = 9,
    CAPS_TRACK_DATA      = 10,
    CAPS_DENSITY_HEADER  = 11,
    CAPS_DENSITY_STREAM  = 12,
    CAPS_DENSITY_DATA    = 13,
    CAPS_INCOMPATIBLE    = 14,
    CAPS_UNSUPPORTED_TYPE = 15,
    CAPS_BAD_BLOCK_TYPE  = 16,
    CAPS_BAD_BLOCK_SIZE  = 17,
    CAPS_BAD_DATA_START  = 18,
    CAPS_BUFFER_SHORT    = 19
} caps_error_t;

/** Get error string */
const char *caps_error_string(caps_error_t err);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Date/Time Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct caps_datetime {
    uint32_t year;
    uint32_t month;
    uint32_t day;
    uint32_t hour;
    uint32_t min;
    uint32_t sec;
    uint32_t tick;
} caps_datetime_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Image Info Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct caps_image_info {
    uint32_t        type;        /**< Image type */
    uint32_t        release;     /**< Release ID */
    uint32_t        revision;    /**< Revision ID */
    uint32_t        min_cylinder;/**< Lowest cylinder */
    uint32_t        max_cylinder;/**< Highest cylinder */
    uint32_t        min_head;    /**< Lowest head */
    uint32_t        max_head;    /**< Highest head */
    caps_datetime_t crdt;        /**< Creation date/time */
    uint32_t        platform[CAPS_MAXPLATFORM]; /**< Platform IDs */
} caps_image_info_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Track Info Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct caps_track_info {
    uint32_t type;           /**< Track type */
    uint32_t cylinder;       /**< Cylinder number */
    uint32_t head;           /**< Head number */
    uint32_t sector_count;   /**< Available sectors */
    uint32_t sector_size;    /**< Sector size */
    uint32_t track_count;    /**< Track variant count */
    uint8_t *track_buf;      /**< Track buffer */
    uint32_t track_len;      /**< Track buffer length */
    uint8_t *track_data[CAPS_MTRS]; /**< Track data per revolution */
    uint32_t track_size[CAPS_MTRS]; /**< Track data sizes */
    uint32_t time_len;       /**< Timing buffer length */
    uint32_t *time_buf;      /**< Timing buffer */
    int32_t  overlap;        /**< Overlap position */
    uint32_t start_bit;      /**< Start position in bits */
    uint32_t wseed;          /**< Weak bit generator seed */
    uint32_t weak_count;     /**< Number of weak data areas */
} caps_track_info_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Sector Info Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct caps_sector_info {
    uint32_t desc_data_size; /**< Data size in bits from IPF descriptor */
    uint32_t desc_gap_size;  /**< Gap size in bits from IPF descriptor */
    uint32_t data_size;      /**< Data size in bits from decoder */
    uint32_t gap_size;       /**< Gap size in bits from decoder */
    uint32_t data_start;     /**< Data start position in bits */
    uint32_t gap_start;      /**< Gap start position in bits */
    uint32_t gap_size_ws0;   /**< Gap size before write splice */
    uint32_t gap_size_ws1;   /**< Gap size after write splice */
    uint32_t gap_ws0_mode;   /**< Gap mode before write splice */
    uint32_t gap_ws1_mode;   /**< Gap mode after write splice */
    uint32_t cell_type;      /**< Bitcell type */
    uint32_t enc_type;       /**< Encoder type */
} caps_sector_info_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Version Info Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct caps_version_info {
    uint32_t type;      /**< Library type */
    uint32_t release;   /**< Release ID */
    uint32_t revision;  /**< Revision ID */
    uint32_t flag;      /**< Supported flags */
} caps_version_info_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Revolution Info Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct caps_revolution_info {
    int32_t next;    /**< Revolution number for next lock call */
    int32_t last;    /**< Revolution number from last lock call */
    int32_t real;    /**< Real revolution number */
    int32_t max;     /**< Max revolution available (<0 = unlimited, 0 = empty) */
} caps_revolution_info_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * CAPS Library Handle (Dynamic Loading)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Windows <windows.h> defines LoadImage as LoadImageW when UNICODE is set.
 * We must undefine it to use LoadImage as a struct member name. */
#ifdef LoadImage
#undef LoadImage
#endif

typedef struct caps_lib {
    void *handle;     /**< Dynamic library handle */
    bool  loaded;     /**< True if library is loaded */
    
    /* Function pointers */
    int32_t (*Init)(void);
    int32_t (*Exit)(void);
    int32_t (*AddImage)(void);
    int32_t (*RemImage)(int32_t id);
    int32_t (*LockImage)(int32_t id, const char *name);
    int32_t (*LockImageMemory)(int32_t id, uint8_t *buffer, uint32_t length, uint32_t flag);
    int32_t (*UnlockImage)(int32_t id);
    int32_t (*LoadImage)(int32_t id, uint32_t flag);
    int32_t (*GetImageInfo)(caps_image_info_t *pi, int32_t id);
    int32_t (*LockTrack)(void *trackinfo, int32_t id, uint32_t cyl, uint32_t head, uint32_t flag);
    int32_t (*UnlockTrack)(int32_t id, uint32_t cyl, uint32_t head);
    int32_t (*UnlockAllTracks)(int32_t id);
    const char *(*GetPlatformName)(uint32_t pid);
    int32_t (*GetVersionInfo)(caps_version_info_t *vi, uint32_t flag);
} caps_lib_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * CAPS Library Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Load CAPS library dynamically
 * @param lib Library handle to initialize
 * @param path Optional path to library (NULL = system default)
 * @return true on success
 */
bool caps_lib_load(caps_lib_t *lib, const char *path);

/**
 * @brief Unload CAPS library
 */
void caps_lib_unload(caps_lib_t *lib);

/**
 * @brief Check if CAPS library is available
 */
bool caps_lib_available(void);

/**
 * @brief Get CAPS library version
 */
bool caps_lib_get_version(caps_lib_t *lib, caps_version_info_t *vi);

#ifdef __cplusplus
}
#endif

#endif /* UFT_IPF_CAPS_H */
