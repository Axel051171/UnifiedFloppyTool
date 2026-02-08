/**
 * @file uft_libflux_formats.h
 * @brief HxC Format Loader API - Unified interface for 100+ disk formats
 * @version 1.0.0
 * @date 2026-01-06
 *
 * License: GPL v2
 *
 * This module provides format definitions and loader interfaces for:
 * - WOZ v1/v2/v3 (Apple II)
 * - SCP (SuperCard Pro)
 * - IPF (CAPS/SPS Preservation)
 * - D64/D81 (Commodore)
 * - DMK (TRS-80)
 * - IMD (ImageDisk)
 * - ADF (Amiga)
 * - STX (Atari ST Pasti)
 *
 */

#ifndef UFT_LIBFLUX_FORMATS_H
#define UFT_LIBFLUX_FORMATS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * WOZ Format (Apple II) - v1/v2/v3
 *============================================================================*/

/** WOZ Chunk IDs */
#define UFT_WOZ_CHUNK_INFO  0x4F464E49  /* "INFO" */
#define UFT_WOZ_CHUNK_TMAP  0x50414D54  /* "TMAP" */
#define UFT_WOZ_CHUNK_TRKS  0x534B5254  /* "TRKS" */
#define UFT_WOZ_CHUNK_META  0x4154454D  /* "META" */
#define UFT_WOZ_CHUNK_WRIT  0x54495257  /* "WRIT" (v2+) */

#pragma pack(push, 1)

/**
 * @brief WOZ file header
 */
typedef struct {
    uint8_t  header_tag[3];     /**< "WOZ" */
    uint8_t  version;           /**< '1', '2', or '3' */
    uint8_t  pad;               /**< 0xFF */
    uint8_t  lfcrlf[3];         /**< 0x0A 0x0D 0x0A */
    uint32_t crc32;             /**< CRC32 of remaining content */
} uft_woz_header_t;

/**
 * @brief WOZ chunk header
 */
typedef struct {
    uint32_t id;                /**< Chunk ID */
    uint32_t size;              /**< Chunk data size */
} uft_woz_chunk_t;

/**
 * @brief WOZ INFO chunk
 */
typedef struct {
    /* v1, v2, v3 */
    uint8_t  version;           /**< 1, 2, or 3 */
    uint8_t  disk_type;         /**< 1 = 5.25", 2 = 3.5" */
    uint8_t  write_protected;
    uint8_t  sync;
    uint8_t  cleaned;
    uint8_t  creator[32];
    
    /* v2, v3 */
    uint8_t  sides_count;
    uint8_t  boot_sector_format; /**< 1=16-sector, 2=13-sector, 3=both */
    uint8_t  bit_timing;        /**< 125ns increments (8 = 1µs) */
    uint16_t compatible_hw;     /**< Bitmask of compatible hardware */
    uint16_t required_ram;      /**< Required RAM in KB */
    uint16_t largest_track;     /**< Largest track in 512-byte blocks */
    
    /* v3 */
    uint16_t flux_block;
    uint16_t largest_flux_track;
} uft_woz_info_t;

/**
 * @brief WOZ v2+ track descriptor
 */
typedef struct {
    uint16_t starting_block;    /**< Start block (512 bytes each) */
    uint16_t block_count;       /**< Number of blocks */
    uint32_t bit_count;         /**< Number of valid bits */
} uft_woz_track_t;

/**
 * @brief WOZ v1 track (fixed size)
 */
typedef struct {
    uint8_t  bitstream[6646];
    uint16_t bytes_count;
    uint16_t bit_count;
    uint16_t bit_splice_point;
    uint8_t  splice_nibble;
    uint8_t  splice_bit_count;
    uint16_t reserved;
} uft_woz_track_v1_t;

#pragma pack(pop)

/** WOZ compatible hardware flags */
#define UFT_WOZ_HW_APPLE_II          0x0001
#define UFT_WOZ_HW_APPLE_II_PLUS     0x0002
#define UFT_WOZ_HW_APPLE_IIE         0x0004
#define UFT_WOZ_HW_APPLE_IIC         0x0008
#define UFT_WOZ_HW_APPLE_IIE_ENH     0x0010
#define UFT_WOZ_HW_APPLE_IIGS        0x0020
#define UFT_WOZ_HW_APPLE_IIC_PLUS    0x0040
#define UFT_WOZ_HW_APPLE_III         0x0080
#define UFT_WOZ_HW_APPLE_III_PLUS    0x0100

/*============================================================================
 * SCP Format (SuperCard Pro)
 *============================================================================*/

#pragma pack(push, 1)

/**
 * @brief SCP file header
 */
typedef struct {
    uint8_t  signature[3];      /**< "SCP" */
    uint8_t  version;           /**< Version (high nibble = major) */
    uint8_t  disk_type;
    uint8_t  nr_revolutions;
    uint8_t  start_track;
    uint8_t  end_track;
    uint8_t  flags;
    uint8_t  bitcell_width;     /**< 0 = 16-bit, other = bits */
    uint16_t heads;             /**< 0 = both, 1 = side 0, 2 = side 1 */
    uint8_t  resolution;        /**< 0 = 25ns */
    uint32_t checksum;
} uft_scp_header_t;

/**
 * @brief SCP track header
 */
typedef struct {
    uint8_t  signature[3];      /**< "TRK" */
    uint8_t  track_num;
} uft_scp_track_header_t;

/**
 * @brief SCP revolution header
 */
typedef struct {
    uint32_t duration;          /**< Index time in ticks */
    uint32_t length;            /**< Flux data length */
    uint32_t offset;            /**< Offset from track start */
} uft_scp_revolution_t;

#pragma pack(pop)

/** SCP disk types */
#define UFT_SCP_DISK_C64            0x00
#define UFT_SCP_DISK_AMIGA          0x04
#define UFT_SCP_DISK_ATARI_FM       0x10
#define UFT_SCP_DISK_ATARI_MFM      0x14
#define UFT_SCP_DISK_APPLE_II       0x20
#define UFT_SCP_DISK_APPLE_PRO      0x24
#define UFT_SCP_DISK_APPLE_MAC      0x30
#define UFT_SCP_DISK_IBM_360        0x40
#define UFT_SCP_DISK_IBM_720        0x44
#define UFT_SCP_DISK_IBM_1200       0x48
#define UFT_SCP_DISK_IBM_1440       0x4C

/** SCP flags */
#define UFT_SCP_FLAG_INDEX          0x01
#define UFT_SCP_FLAG_96TPI          0x02
#define UFT_SCP_FLAG_360RPM         0x04
#define UFT_SCP_FLAG_NORMALIZED     0x08
#define UFT_SCP_FLAG_RW             0x10
#define UFT_SCP_FLAG_FOOTER         0x20
#define UFT_SCP_FLAG_EXTENDED       0x40
#define UFT_SCP_FLAG_CREATOR        0x80

/*============================================================================
 * IPF Format (CAPS/SPS Preservation)
 *============================================================================*/

#pragma pack(push, 1)

/**
 * @brief IPF record header
 */
typedef struct {
    uint8_t  type[4];           /**< Record type */
    uint32_t length;            /**< Record length */
    uint32_t crc;               /**< CRC of record data */
} uft_ipf_record_t;

/**
 * @brief IPF INFO record
 */
typedef struct {
    uint32_t media_type;        /**< 1 = floppy */
    uint32_t encoder_type;      /**< 1 = CAPS */
    uint32_t encoder_rev;       /**< Encoder revision */
    uint32_t file_key;          /**< File key */
    uint32_t file_rev;          /**< File revision */
    uint32_t origin;            /**< Origin CRC */
    uint32_t min_track;
    uint32_t max_track;
    uint32_t min_side;
    uint32_t max_side;
    uint32_t creation_date;
    uint32_t creation_time;
    uint32_t platform[4];       /**< Platform flags */
    uint32_t disk_num;
    uint32_t creator_id;
    uint32_t reserved[3];
} uft_ipf_info_t;

#pragma pack(pop)

/** IPF record types */
#define UFT_IPF_RECORD_CAPS     "CAPS"
#define UFT_IPF_RECORD_INFO     "INFO"
#define UFT_IPF_RECORD_IMGE     "IMGE"
#define UFT_IPF_RECORD_DATA     "DATA"
#define UFT_IPF_RECORD_CTEX     "CTEX"  /* Context */
#define UFT_IPF_RECORD_CTEI     "CTEI"  /* Context Instance */

/*============================================================================
 * DMK Format (TRS-80)
 *============================================================================*/

#pragma pack(push, 1)

/**
 * @brief DMK file header
 */
typedef struct {
    uint8_t  write_protected;   /**< 0xFF = protected */
    uint8_t  tracks;            /**< Number of tracks */
    uint16_t track_length;      /**< Track length in bytes */
    uint8_t  flags;             /**< Options */
    uint8_t  reserved[7];
    uint32_t native_density;    /**< 0 = default */
} uft_dmk_header_t;

#pragma pack(pop)

/** DMK flags */
#define UFT_DMK_FLAG_SINGLE_SIDED   0x10
#define UFT_DMK_FLAG_SINGLE_DENSITY 0x40
#define UFT_DMK_FLAG_IGNORE_DENSITY 0x80

/*============================================================================
 * IMD Format (ImageDisk)
 *============================================================================*/

#pragma pack(push, 1)

/**
 * @brief IMD sector header
 */
typedef struct {
    uint8_t  mode;              /**< Recording mode */
    uint8_t  cylinder;          /**< Cylinder number */
    uint8_t  head;              /**< Head (bit 6 = cylinder map, bit 7 = head map) */
    uint8_t  sectors;           /**< Number of sectors */
    uint8_t  sector_size;       /**< Sector size code (0=128, 1=256, etc.) */
} uft_imd_track_header_t;

#pragma pack(pop)

/** IMD modes */
#define UFT_IMD_MODE_500K_FM    0
#define UFT_IMD_MODE_300K_FM    1
#define UFT_IMD_MODE_250K_FM    2
#define UFT_IMD_MODE_500K_MFM   3
#define UFT_IMD_MODE_300K_MFM   4
#define UFT_IMD_MODE_250K_MFM   5

/** IMD sector data types */
#define UFT_IMD_DATA_UNAVAILABLE    0
#define UFT_IMD_DATA_NORMAL         1
#define UFT_IMD_DATA_COMPRESSED     2
#define UFT_IMD_DATA_DELETED        3
#define UFT_IMD_DATA_DELETED_COMP   4
#define UFT_IMD_DATA_ERROR          5
#define UFT_IMD_DATA_ERROR_COMP     6
#define UFT_IMD_DATA_DEL_ERROR      7
#define UFT_IMD_DATA_DEL_ERROR_COMP 8

/*============================================================================
 * STX Format (Atari ST Pasti)
 *============================================================================*/

#pragma pack(push, 1)

/**
 * @brief STX file header
 */
typedef struct {
    uint8_t  signature[4];      /**< "RSY\0" */
    uint16_t version;           /**< Version (3 = Pasti) */
    uint16_t tool;              /**< Tool used */
    uint16_t reserved1;
    uint8_t  tracks;
    uint8_t  revision;
    uint32_t reserved2;
} uft_stx_header_t;

/**
 * @brief STX track header
 */
typedef struct {
    uint32_t record_size;
    uint32_t fuzzy_count;
    uint16_t sector_count;
    uint16_t flags;
    uint16_t track_length;
    uint8_t  track_num;
    uint8_t  track_type;
} uft_stx_track_t;

#pragma pack(pop)

/** STX track types */
#define UFT_STX_TYPE_STANDARD   0x00
#define UFT_STX_TYPE_SYNC       0x80

/*============================================================================
 * Format Detection
 *============================================================================*/

/**
 * @brief Detected format type
 */
typedef enum {
    UFT_FORMAT_UNKNOWN      = 0,
    
    /* Apple */
    UFT_FORMAT_WOZ          = 10,
    UFT_FORMAT_WOZ_V1       = 11,
    UFT_FORMAT_WOZ_V2       = 12,
    UFT_FORMAT_WOZ_V3       = 13,
    UFT_FORMAT_NIB          = 14,
    UFT_FORMAT_DO           = 15,
    UFT_FORMAT_PO           = 16,
    UFT_FORMAT_2MG          = 17,
    
    /* Preservation */
    UFT_FORMAT_SCP          = 20,
    UFT_FORMAT_IPF          = 21,
    UFT_FORMAT_KRYOFLUX     = 22,
    UFT_FORMAT_A2R          = 23,
    
    /* Commodore */
    UFT_FORMAT_D64          = 30,
    UFT_FORMAT_G64          = 31,
    UFT_FORMAT_D81          = 32,
    UFT_FORMAT_D71          = 33,
    UFT_FORMAT_D80          = 34,
    UFT_FORMAT_D82          = 35,
    
    /* Amiga */
    UFT_FORMAT_ADF          = 40,
    UFT_FORMAT_ADZ          = 41,
    UFT_FORMAT_DMS          = 42,
    UFT_FORMAT_FDI          = 43,
    
    /* Atari */
    UFT_FORMAT_STX          = 50,
    UFT_FORMAT_ST           = 51,
    UFT_FORMAT_MSA          = 52,
    
    /* TRS-80 */
    UFT_FORMAT_DMK          = 60,
    UFT_FORMAT_JV1          = 61,
    UFT_FORMAT_JV3          = 62,
    
    /* PC/IBM */
    UFT_FORMAT_IMD          = 70,
    UFT_FORMAT_IMG          = 71,
    UFT_FORMAT_TD0          = 72,
    UFT_FORMAT_DSK          = 73,
    
    /* HxC */
    UFT_FORMAT_HFE          = 80,
    UFT_FORMAT_HFE_V3       = 81,
    UFT_FORMAT_MFM          = 82,
    UFT_FORMAT_AFI          = 83,
    
    /* Other */
    UFT_FORMAT_RAW          = 90,
    UFT_FORMAT_FLUX         = 91,
} uft_format_type_t;

/**
 * @brief Format detection result
 */
typedef struct {
    uft_format_type_t type;     /**< Detected format */
    uint8_t  confidence;        /**< Confidence 0-100 */
    uint8_t  version;           /**< Format version */
    uint32_t magic;             /**< Magic bytes found */
    char     name[32];          /**< Format name */
} uft_format_detect_t;

/*============================================================================
 * API Functions
 *============================================================================*/

/**
 * @brief Detect format from file header
 * @param data File header data (minimum 16 bytes)
 * @param size Size of data
 * @param result Detection result
 * @return true if format detected
 */
bool uft_libflux_detect_format(const uint8_t* data, size_t size,
                           uft_format_detect_t* result);

/**
 * @brief Get format name
 * @param type Format type
 * @return Format name string
 */
const char* uft_libflux_format_name(uft_format_type_t type);

/**
 * @brief Check if format supports flux data
 * @param type Format type
 * @return true if flux format
 */
bool uft_libflux_is_flux_format(uft_format_type_t type);

/**
 * @brief Check if format is preservation quality
 * @param type Format type
 * @return true if preservation format
 */
bool uft_libflux_is_preservation_format(uft_format_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_LIBFLUX_FORMATS_H */
