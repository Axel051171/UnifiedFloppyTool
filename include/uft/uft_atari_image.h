/**
 * @file uft_atari_image.h
 * @brief Atari 8-bit Disk Image Format Support
 * 
 * @version 4.2.0
 * @date 2026-01-03
 * 
 * SUPPORTED FORMATS:
 * - ATR  - Standard Atari disk image (SIO2PC/APE)
 * - XFD  - Raw sector dump (no header)
 * - ATX  - VAPI extended format (copy protection)
 * - DCM  - DiskComm compressed image
 * - PRO  - APE ProSystem protected image
 * - CAS  - Cassette tape image
 * - WAV  - Audio cassette recording
 * 
 * ATX FEATURES:
 * - Sector timing data
 * - Weak/fuzzy sectors
 * - Missing sectors
 * - CRC errors
 * - Copy protection emulation
 */

#ifndef UFT_ATARI_IMAGE_H
#define UFT_ATARI_IMAGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Format Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_ATARI_FMT_UNKNOWN   = 0,
    UFT_ATARI_FMT_ATR       = 1,    /* Standard ATR */
    UFT_ATARI_FMT_XFD       = 2,    /* Raw sectors */
    UFT_ATARI_FMT_ATX       = 3,    /* VAPI extended */
    UFT_ATARI_FMT_DCM       = 4,    /* DiskComm compressed */
    UFT_ATARI_FMT_PRO       = 5,    /* APE ProSystem */
    UFT_ATARI_FMT_CAS       = 6,    /* Cassette */
    UFT_ATARI_FMT_WAV       = 7,    /* Audio tape */
    UFT_ATARI_FMT_SCP       = 8,    /* SuperCard Pro flux */
    UFT_ATARI_FMT_A2R       = 9,    /* Applesauce flux */
} uft_atari_format_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * ATR Header (16 bytes)
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_ATR_MAGIC           0x0296  /* "NICKATARI" signature */
#define UFT_ATR_HEADER_SIZE     16

#pragma pack(push, 1)
typedef struct {
    uint16_t magic;             /* 0x96 0x02 = ATR signature */
    uint16_t size_para_lo;      /* Image size in paragraphs (low) */
    uint16_t sector_size;       /* Sector size (128/256) */
    uint8_t  size_para_hi;      /* Image size paragraphs (high byte) */
    uint8_t  disk_flags;        /* Disk flags */
    uint16_t bad_sectors;       /* First bad sector (0=none) */
    uint8_t  unused[5];         /* Reserved */
    uint8_t  write_protect;     /* 0=R/W, 1=R/O */
} uft_atr_header_t;
#pragma pack(pop)

/* ATR Disk Flags */
#define UFT_ATR_FLAG_COPY_PROTECTED  0x01
#define UFT_ATR_FLAG_WRITE_PROTECT   0x02
#define UFT_ATR_FLAG_DENSITY_MASK    0xFC

/* ═══════════════════════════════════════════════════════════════════════════════
 * ATX Header - VAPI Format
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_ATX_MAGIC           0x41543858  /* "AT8X" */
#define UFT_ATX_VERSION         1

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;             /* "AT8X" (0x58385441 LE) */
    uint16_t version;           /* Format version */
    uint16_t min_version;       /* Minimum version to read */
    uint16_t creator;           /* Creator ID */
    uint16_t creator_version;   /* Creator version */
    uint32_t flags;             /* Global flags */
    uint16_t image_type;        /* Image type */
    uint8_t  density;           /* Density code */
    uint8_t  reserved;
    uint32_t image_id;          /* Unique image ID */
    uint16_t image_version;     /* Image version */
    uint16_t reserved2;
    uint32_t start_track;       /* Offset to first track record */
    uint32_t end_track;         /* Offset past last track record */
    uint8_t  padding[12];       /* Pad to 48 bytes */
} uft_atx_header_t;
#pragma pack(pop)

/* ATX Track Record Header */
#pragma pack(push, 1)
typedef struct {
    uint32_t size;              /* Total size of this record */
    uint16_t type;              /* Record type */
    uint16_t reserved;
    uint8_t  track_number;      /* Physical track number */
    uint8_t  side;              /* Side (0 or 1) */
    uint16_t sector_count;      /* Number of sectors in track */
    uint16_t rate;              /* MFM data rate */
    uint16_t reserved2;
    uint32_t flags;             /* Track flags */
    uint32_t header_size;       /* Size of this header */
    uint64_t reserved3;
} uft_atx_track_t;
#pragma pack(pop)

/* ATX Sector Header */
#pragma pack(push, 1)
typedef struct {
    uint8_t  number;            /* Sector number (1-based) */
    uint8_t  status;            /* Sector status flags */
    uint16_t position;          /* Angular position (0-26041) */
    uint32_t start_time;        /* Start timing (in bit cells) */
} uft_atx_sector_t;
#pragma pack(pop)

/* ATX Sector Status Flags */
#define UFT_ATX_SECTOR_MISSING      0x01    /* Sector not present */
#define UFT_ATX_SECTOR_WEAK         0x02    /* Weak/fuzzy data */
#define UFT_ATX_SECTOR_CRC_ERROR    0x04    /* CRC error */
#define UFT_ATX_SECTOR_DELETED      0x08    /* Deleted data mark */
#define UFT_ATX_SECTOR_FDC_ERROR    0x10    /* FDC error status */
#define UFT_ATX_SECTOR_EXTENDED     0x40    /* Extended sector data */

/* ATX Density Codes */
#define UFT_ATX_DENSITY_SD          0       /* Single density (FM) */
#define UFT_ATX_DENSITY_ED          1       /* Enhanced density (FM) */
#define UFT_ATX_DENSITY_DD          2       /* Double density (MFM) */

/* ═══════════════════════════════════════════════════════════════════════════════
 * DCM - DiskComm Compressed Format
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_DCM_MAGIC           0xFA        /* DCM signature byte */

#pragma pack(push, 1)
typedef struct {
    uint8_t  archive_type;      /* 0xFA = DCM */
    uint8_t  pass_info;         /* Pass information */
    uint8_t  density;           /* Density + last flag */
    /* Variable data follows */
} uft_dcm_header_t;
#pragma pack(pop)

/* DCM Block Types */
#define UFT_DCM_PASS_END        0x45        /* End of pass */
#define UFT_DCM_CHANGE_BEGIN    0x41        /* Change only - begin */
#define UFT_DCM_DOS_SECTOR      0x42        /* DOS sector */
#define UFT_DCM_COMPRESSED      0x43        /* Compressed sector */
#define UFT_DCM_CHANGE_END      0x44        /* Change only - end */
#define UFT_DCM_SAME            0x46        /* Same as previous */
#define UFT_DCM_UNCOMPRESSED    0x47        /* Uncompressed sector */

/* ═══════════════════════════════════════════════════════════════════════════════
 * PRO - APE ProSystem Format
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_PRO_MAGIC           "PROC"

#pragma pack(push, 1)
typedef struct {
    char     magic[4];          /* "PROC" */
    uint16_t version;           /* Format version */
    uint8_t  tracks;            /* Number of tracks */
    uint8_t  sides;             /* Number of sides */
    uint16_t sector_count;      /* Total sectors */
    uint16_t sector_size;       /* Bytes per sector */
    uint32_t phantom_flags;     /* Phantom sector bitmap */
    /* Track data follows */
} uft_pro_header_t;
#pragma pack(pop)

/* ═══════════════════════════════════════════════════════════════════════════════
 * CAS - Cassette Format
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_CAS_MAGIC           "FUJI"

#pragma pack(push, 1)
typedef struct {
    char     magic[4];          /* "FUJI" */
    /* Chunk-based format follows */
} uft_cas_header_t;
#pragma pack(pop)

/* CAS Chunk Types */
#define UFT_CAS_CHUNK_FUJI      0x4955554A  /* "FUJI" - header */
#define UFT_CAS_CHUNK_BAUD      0x64756162  /* "baud" - baud rate */
#define UFT_CAS_CHUNK_DATA      0x61746164  /* "data" - data block */
#define UFT_CAS_CHUNK_FSK       0x206B7366  /* "fsk " - FSK block */
#define UFT_CAS_CHUNK_PWMS      0x736D7770  /* "pwms" - PWM short */
#define UFT_CAS_CHUNK_PWML      0x6C6D7770  /* "pwml" - PWM long */
#define UFT_CAS_CHUNK_PWMC      0x636D7770  /* "pwmc" - PWM custom */

/* CAS Chunk Header */
#pragma pack(push, 1)
typedef struct {
    uint32_t type;              /* Chunk type (4 chars) */
    uint16_t length;            /* Chunk data length */
    uint8_t  aux1;              /* Auxiliary byte 1 */
    uint8_t  aux2;              /* Auxiliary byte 2 */
    /* Data follows */
} uft_cas_chunk_t;
#pragma pack(pop)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Disk Geometry
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint8_t  tracks;            /* Number of tracks (40/77/80) */
    uint8_t  sides;             /* Number of sides (1/2) */
    uint8_t  sectors_per_track; /* Sectors per track (18/26) */
    uint16_t sector_size;       /* Bytes per sector (128/256/512) */
    uint32_t total_sectors;     /* Total sectors */
    uint32_t image_size;        /* Total image size in bytes */
    bool     boot_tracks_sd;    /* Boot tracks are single density */
} uft_atari_geometry_t;

/* Standard geometries */
extern const uft_atari_geometry_t UFT_ATARI_GEOM_SD;      /* 90KB */
extern const uft_atari_geometry_t UFT_ATARI_GEOM_ED;      /* 130KB */
extern const uft_atari_geometry_t UFT_ATARI_GEOM_DD;      /* 180KB */
extern const uft_atari_geometry_t UFT_ATARI_GEOM_QD;      /* 360KB */
extern const uft_atari_geometry_t UFT_ATARI_GEOM_DS_DD;   /* 360KB DS */
extern const uft_atari_geometry_t UFT_ATARI_GEOM_HD;      /* 720KB */

/* ═══════════════════════════════════════════════════════════════════════════════
 * Image Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_atari_format_t format;
    uft_atari_geometry_t geometry;
    
    /* Raw image data */
    uint8_t *data;
    size_t data_size;
    
    /* Format-specific headers */
    union {
        uft_atr_header_t atr;
        uft_atx_header_t atx;
        uft_dcm_header_t dcm;
        uft_pro_header_t pro;
        uft_cas_header_t cas;
    } header;
    
    /* ATX-specific data */
    struct {
        uft_atx_track_t *tracks;
        int track_count;
        uft_atx_sector_t **sectors;     /* Array of sector arrays per track */
        uint8_t **sector_data;          /* Actual sector data */
        uint8_t **weak_masks;           /* Weak bit masks (if any) */
    } atx;
    
    /* Metadata */
    char *filename;
    bool modified;
    bool write_protected;
} uft_atari_image_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Format Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Detect Atari image format
 * @param data Raw file data
 * @param size Data size
 * @return Format type or UFT_ATARI_FMT_UNKNOWN
 */
uft_atari_format_t uft_atari_detect_format(const uint8_t *data, size_t size);

/**
 * @brief Get format name string
 */
const char *uft_atari_format_name(uft_atari_format_t format);

/**
 * @brief Detect geometry from image data
 */
int uft_atari_detect_geometry(const uint8_t *data, size_t size,
                              uft_atari_geometry_t *geometry);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Image Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Open Atari disk image (any supported format)
 */
int uft_atari_image_open(const char *filename, uft_atari_image_t *image);

/**
 * @brief Open from memory
 */
int uft_atari_image_open_mem(const uint8_t *data, size_t size,
                             uft_atari_image_t *image);

/**
 * @brief Create new image with specified geometry
 */
int uft_atari_image_create(uft_atari_image_t *image,
                           const uft_atari_geometry_t *geometry,
                           uft_atari_format_t format);

/**
 * @brief Save image to file
 */
int uft_atari_image_save(const uft_atari_image_t *image, const char *filename);

/**
 * @brief Save in different format
 */
int uft_atari_image_save_as(const uft_atari_image_t *image, 
                            const char *filename,
                            uft_atari_format_t format);

/**
 * @brief Close and free image
 */
void uft_atari_image_close(uft_atari_image_t *image);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Sector Access
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read sector by number (1-based)
 */
int uft_atari_image_read_sector(const uft_atari_image_t *image,
                                uint16_t sector_num,
                                uint8_t *buffer,
                                size_t buffer_size);

/**
 * @brief Write sector by number
 */
int uft_atari_image_write_sector(uft_atari_image_t *image,
                                 uint16_t sector_num,
                                 const uint8_t *data,
                                 size_t data_size);

/**
 * @brief Read sector by track/side/sector
 */
int uft_atari_image_read_tss(const uft_atari_image_t *image,
                             uint8_t track, uint8_t side, uint8_t sector,
                             uint8_t *buffer, size_t buffer_size);

/**
 * @brief Get sector status (for ATX format)
 */
uint8_t uft_atari_image_sector_status(const uft_atari_image_t *image,
                                      uint16_t sector_num);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - ATX Specific
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get ATX sector timing
 */
int uft_atx_get_timing(const uft_atari_image_t *image,
                       uint16_t sector_num,
                       uint32_t *start_time,
                       uint16_t *position);

/**
 * @brief Set ATX sector as weak/fuzzy
 */
int uft_atx_set_weak(uft_atari_image_t *image,
                     uint16_t sector_num,
                     const uint8_t *weak_mask,
                     size_t mask_size);

/**
 * @brief Get weak bit mask for sector
 */
int uft_atx_get_weak(const uft_atari_image_t *image,
                     uint16_t sector_num,
                     uint8_t *weak_mask,
                     size_t *mask_size);

/**
 * @brief Mark sector as missing
 */
int uft_atx_set_missing(uft_atari_image_t *image, uint16_t sector_num);

/**
 * @brief Mark sector with CRC error
 */
int uft_atx_set_crc_error(uft_atari_image_t *image, uint16_t sector_num);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Format Conversion
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Convert between formats
 */
int uft_atari_convert(const uft_atari_image_t *src,
                      uft_atari_image_t *dst,
                      uft_atari_format_t dst_format);

/**
 * @brief Decompress DCM to ATR
 */
int uft_dcm_decompress(const uint8_t *dcm_data, size_t dcm_size,
                       uint8_t **atr_data, size_t *atr_size);

/**
 * @brief Compress ATR to DCM
 */
int uft_dcm_compress(const uint8_t *atr_data, size_t atr_size,
                     uint8_t **dcm_data, size_t *dcm_size);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Utilities
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Print image information
 */
void uft_atari_image_info(const uft_atari_image_t *image);

/**
 * @brief Validate image structure
 */
int uft_atari_image_validate(const uft_atari_image_t *image);

/**
 * @brief Calculate checksum
 */
uint32_t uft_atari_image_checksum(const uft_atari_image_t *image);

/**
 * @brief Convert sector number to track/side/sector
 */
void uft_atari_sector_to_tss(uint16_t sector_num,
                             const uft_atari_geometry_t *geometry,
                             uint8_t *track, uint8_t *side, uint8_t *sector);

/**
 * @brief Convert track/side/sector to sector number
 */
uint16_t uft_atari_tss_to_sector(uint8_t track, uint8_t side, uint8_t sector,
                                 const uft_atari_geometry_t *geometry);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ATARI_IMAGE_H */
