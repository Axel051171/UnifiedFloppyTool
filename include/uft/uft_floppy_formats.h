/**
 * @file uft_floppy_formats.h
 * @brief UnifiedFloppyTool - Comprehensive Floppy Format Definitions v3.1.4.009
 * 
 * Complete format registry extracted from floppy-formate.zip analysis.
 * Supports 60+ disk image formats across all major platforms.
 * 
 * Platforms covered:
 * - Commodore: D64/D67/D71/D80/D81/D82/D90/D91/G64/X64/X71/X81/X128
 * - Atari ST: ST/STT/STX/STZ
 * - Amiga: ADF/ADL/ADZ
 * - Apple: DSK/NIB/NBZ/MAC_DSK
 * - PC/IBM: IMG/IMA/IMZ/DMF/XDF
 * - CPC/Amstrad: DSK/EDSK
 * - MSX: DMF_MSX
 * - X68000: DIM
 * - TI-99: V9T9/TIFILES/FIAD
 * - Tandy/TRS-80: JV3/JVC
 * - BBC: SSD/DSD
 * - Oric: ORIC_DSK
 * - SAM Coupe: MGT/SAD/SDF
 * - ZX Spectrum: TRD/SCL
 * - Generic: CQM/TD0/HFE/MFI/PFI/PRI/PSI/DFI
 * 
 * @version 3.1.4.009
 * @date 2025-12-30
 */

#ifndef UFT_FLOPPY_FORMATS_H
#define UFT_FLOPPY_FORMATS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * FORMAT IDENTIFICATION
 *===========================================================================*/

/**
 * @brief Master format enumeration
 */
typedef enum {
    UFT_FMT_UNKNOWN = 0,
    
    /* Raw sector images */
    UFT_FMT_RAW_IMG,            /**< Generic raw sector image */
    UFT_FMT_RAW_IMA,            /**< IBM PC floppy image */
    UFT_FMT_RAW_DSK,            /**< Generic DSK (platform-dependent) */
    
    /* Commodore formats */
    UFT_FMT_D64,                /**< C64 1541 single-sided */
    UFT_FMT_D67,                /**< C64 2040/3040 */
    UFT_FMT_D71,                /**< C64/C128 1571 double-sided */
    UFT_FMT_D80,                /**< CBM 8050 */
    UFT_FMT_D81,                /**< C64/C128 1581 3.5" */
    UFT_FMT_D82,                /**< CBM 8250/SFD-1001 */
    UFT_FMT_D90,                /**< CMD FD-2000/4000 HD */
    UFT_FMT_D91,                /**< CMD FD-2000/4000 ED */
    UFT_FMT_G64,                /**< GCR raw track image */
    UFT_FMT_G71,                /**< GCR raw track (double-sided) */
    UFT_FMT_X64,                /**< X64 with header */
    UFT_FMT_X71,                /**< X71 with header */
    UFT_FMT_X81,                /**< X81 with header */
    UFT_FMT_X128,               /**< X128 with header */
    UFT_FMT_P64,                /**< Preservation Project format */
    UFT_FMT_NIB,                /**< NIBTOOLS raw nibble */
    UFT_FMT_NBZ,                /**< Compressed NIB */
    UFT_FMT_D1M,                /**< CMD FD-2000 DD */
    UFT_FMT_D2M,                /**< CMD FD-2000 HD */
    UFT_FMT_D4M,                /**< CMD FD-4000 ED */
    UFT_FMT_DNP,                /**< CMD native partition */
    UFT_FMT_DNP2,               /**< CMD native partition (alt) */
    UFT_FMT_DHD,                /**< CMD HD native */
    UFT_FMT_LNX,                /**< Lynx archive */
    UFT_FMT_T64,                /**< Tape64 container */
    UFT_FMT_TAP,                /**< C64 tape image */
    UFT_FMT_P00,                /**< PC64 PRG container */
    UFT_FMT_PRG,                /**< Raw PRG file */
    UFT_FMT_CRT,                /**< Cartridge image */
    
    /* Amiga formats */
    UFT_FMT_ADF,                /**< Amiga Disk File (OFS/FFS) */
    UFT_FMT_ADL,                /**< Amiga Disk File Long (HD) */
    UFT_FMT_ADZ,                /**< gzip compressed ADF */
    UFT_FMT_DMS,                /**< DiskMasher System */
    
    /* Atari ST formats */
    UFT_FMT_ST,                 /**< Atari ST raw sector */
    UFT_FMT_STT,                /**< Atari ST with track info */
    UFT_FMT_STX,                /**< Pasti format (protection) */
    UFT_FMT_STZ,                /**< Compressed ST */
    UFT_FMT_MSA,                /**< Magic Shadow Archiver */
    
    /* Apple formats */
    UFT_FMT_DSK_APPLE,          /**< Apple II sector image */
    UFT_FMT_DO,                 /**< DOS 3.3 order */
    UFT_FMT_PO,                 /**< ProDOS order */
    UFT_FMT_NIB_APPLE,          /**< Apple II nibble image */
    UFT_FMT_2IMG,               /**< 2IMG universal Apple II */
    UFT_FMT_MAC_DSK,            /**< Macintosh 400K/800K */
    UFT_FMT_DC42,               /**< DiskCopy 4.2 */
    UFT_FMT_DART,               /**< Apple DART archive */
    
    /* Amstrad/CPC formats */
    UFT_FMT_DSK_CPC,            /**< Amstrad CPC DSK */
    UFT_FMT_EDSK,               /**< Extended DSK format */
    
    /* PC formats */
    UFT_FMT_IMG_PC,             /**< PC raw sector image */
    UFT_FMT_IMA_PC,             /**< PC floppy image */
    UFT_FMT_IMZ,                /**< gzip compressed IMG */
    UFT_FMT_DMF,                /**< Distribution Media Format */
    UFT_FMT_XDF,                /**< eXtended Density Format */
    UFT_FMT_DCP,                /**< DiskCopy Pro */
    UFT_FMT_DCU,                /**< DiskCopy Utility */
    
    /* MSX formats */
    UFT_FMT_DMF_MSX,            /**< MSX disk image */
    
    /* X68000 formats */
    UFT_FMT_DIM,                /**< X68000 disk image */
    UFT_FMT_XDF_X68K,           /**< X68000 extended */
    
    /* TI-99 formats */
    UFT_FMT_V9T9,               /**< V9T9 sector image */
    UFT_FMT_PC99,               /**< PC99 format */
    UFT_FMT_TIFILES,            /**< TIFILES container */
    UFT_FMT_FIAD,               /**< File In A Directory */
    
    /* TRS-80 formats */
    UFT_FMT_JV3,                /**< Jeff Vavasour v3 */
    UFT_FMT_JVC,                /**< JV3 compatible */
    UFT_FMT_DMK,                /**< David Keil format */
    
    /* BBC Micro formats */
    UFT_FMT_SSD,                /**< BBC single-sided */
    UFT_FMT_DSD,                /**< BBC double-sided */
    UFT_FMT_ADF_BBC,            /**< BBC ADFS */
    UFT_FMT_ADL_BBC,            /**< BBC ADFS large */
    
    /* Oric formats */
    UFT_FMT_ORIC_DSK,           /**< Oric MFM disk */
    
    /* SAM Coupe formats */
    UFT_FMT_MGT,                /**< Miles Gordon Technology */
    UFT_FMT_SAD,                /**< SAM Disk */
    UFT_FMT_SDF,                /**< SAM Disk Format */
    
    /* ZX Spectrum formats */
    UFT_FMT_TRD,                /**< TR-DOS */
    UFT_FMT_SCL,                /**< Sinclair */
    UFT_FMT_FDI,                /**< FDI (UKV Spectrum) */
    
    /* NEC PC-98 formats */
    UFT_FMT_NFD,                /**< NEC PC-98 */
    UFT_FMT_FDD_NEC,            /**< NEC FDD */
    
    /* Sharp formats */
    UFT_FMT_SF7,                /**< Sharp SF-7000 */
    
    /* Generic/Archive formats */
    UFT_FMT_CQM,                /**< CopyQM compressed */
    UFT_FMT_TD0,                /**< Teledisk */
    UFT_FMT_IMD,                /**< ImageDisk */
    
    /* Flux/Track formats */
    UFT_FMT_HFE,                /**< HxC Floppy Emulator */
    UFT_FMT_HFE_V3,             /**< HFE version 3 */
    UFT_FMT_MFI,                /**< MAME Floppy Image */
    UFT_FMT_SCP,                /**< SuperCard Pro */
    UFT_FMT_KF,                 /**< Kryoflux stream */
    UFT_FMT_KFRAW,              /**< Kryoflux raw */
    UFT_FMT_GWRAW,              /**< Greaseweazle raw */
    UFT_FMT_A2R,                /**< Applesauce */
    UFT_FMT_WOZ,                /**< Applesauce WOZ */
    
    /* PCE formats (Hampa Hug) */
    UFT_FMT_PFI,                /**< PCE Flux Image */
    UFT_FMT_PRI,                /**< PCE Raw Image */
    UFT_FMT_PSI,                /**< PCE Sector Image */
    UFT_FMT_DFI,                /**< DiscFerret Image */
    
    /* Nintendo formats */
    UFT_FMT_FDS,                /**< Famicom Disk System */
    UFT_FMT_QD,                 /**< Quick Disk */
    
    /* NES/Famicom */
    UFT_FMT_EDD,                /**< Enigma Disk Dumper */
    
    UFT_FMT_COUNT               /**< Total format count */
} uft_format_id_t;

/**
 * @brief Format capability flags
 */
typedef enum {
    UFT_FMT_CAP_READ        = (1 << 0),   /**< Can read sectors */
    UFT_FMT_CAP_WRITE       = (1 << 1),   /**< Can write sectors */
    UFT_FMT_CAP_CREATE      = (1 << 2),   /**< Can create new images */
    UFT_FMT_CAP_RESIZE      = (1 << 3),   /**< Can resize image */
    UFT_FMT_CAP_FLUX        = (1 << 4),   /**< Contains flux data */
    UFT_FMT_CAP_TIMING      = (1 << 5),   /**< Contains timing data */
    UFT_FMT_CAP_WEAK_BITS   = (1 << 6),   /**< Supports weak bits */
    UFT_FMT_CAP_MULTI_REV   = (1 << 7),   /**< Multiple revolutions */
    UFT_FMT_CAP_PROTECTION  = (1 << 8),   /**< Copy protection support */
    UFT_FMT_CAP_COMPRESS    = (1 << 9),   /**< Compressed format */
    UFT_FMT_CAP_DIRECTORY   = (1 << 10),  /**< Has directory structure */
    UFT_FMT_CAP_ARCHIVE     = (1 << 11),  /**< Multi-file archive */
} uft_format_caps_t;

/**
 * @brief Platform/system identification
 */
typedef enum {
    UFT_PLATFORM_GENERIC = 0,
    UFT_PLATFORM_IBM_PC,
    UFT_PLATFORM_COMMODORE,
    UFT_PLATFORM_AMIGA,
    UFT_PLATFORM_ATARI_ST,
    UFT_PLATFORM_APPLE_II,
    UFT_PLATFORM_MACINTOSH,
    UFT_PLATFORM_AMSTRAD_CPC,
    UFT_PLATFORM_MSX,
    UFT_PLATFORM_X68000,
    UFT_PLATFORM_TI99,
    UFT_PLATFORM_TRS80,
    UFT_PLATFORM_BBC,
    UFT_PLATFORM_ORIC,
    UFT_PLATFORM_SAM_COUPE,
    UFT_PLATFORM_ZX_SPECTRUM,
    UFT_PLATFORM_NEC_PC98,
    UFT_PLATFORM_SHARP,
    UFT_PLATFORM_NINTENDO,
    UFT_PLATFORM_FLUX_GENERIC
} uft_platform_t;

/*===========================================================================
 * FORMAT GEOMETRY
 *===========================================================================*/

/**
 * @brief Standard geometry definitions
 */
typedef struct {
    uint32_t tracks;        /**< Cylinders/tracks */
    uint32_t heads;         /**< Sides */
    uint32_t sectors;       /**< Sectors per track */
    uint32_t sector_size;   /**< Bytes per sector */
    uint32_t rpm;           /**< Rotation speed */
    uint32_t encoding;      /**< MFM/FM/GCR/etc */
    uint32_t data_rate;     /**< kbps */
} uft_geometry_t;

/**
 * @brief Encoding types
 */
typedef enum {
    UFT_ENC_FM,             /**< Frequency Modulation (SD) */
    UFT_ENC_MFM,            /**< Modified FM (DD/HD) */
    UFT_ENC_GCR,            /**< Group Coded Recording (CBM/Apple) */
    UFT_ENC_M2FM,           /**< Modified MFM */
    UFT_ENC_RAW             /**< Raw flux / unknown */
} uft_encoding_t;

/* Standard geometries */
#define UFT_GEOM_360K   (uft_geometry_t){40, 2, 9, 512, 300, UFT_ENC_MFM, 250}
#define UFT_GEOM_720K   (uft_geometry_t){80, 2, 9, 512, 300, UFT_ENC_MFM, 250}
#define UFT_GEOM_1200K  (uft_geometry_t){80, 2, 15, 512, 360, UFT_ENC_MFM, 500}
#define UFT_GEOM_1440K  (uft_geometry_t){80, 2, 18, 512, 300, UFT_ENC_MFM, 500}
#define UFT_GEOM_2880K  (uft_geometry_t){80, 2, 36, 512, 300, UFT_ENC_MFM, 1000}

#define UFT_GEOM_D64    (uft_geometry_t){35, 1, 21, 256, 300, UFT_ENC_GCR, 250}
#define UFT_GEOM_D71    (uft_geometry_t){35, 2, 21, 256, 300, UFT_ENC_GCR, 250}
#define UFT_GEOM_D81    (uft_geometry_t){80, 2, 10, 512, 300, UFT_ENC_MFM, 250}

#define UFT_GEOM_ADF_DD (uft_geometry_t){80, 2, 11, 512, 300, UFT_ENC_MFM, 250}
#define UFT_GEOM_ADF_HD (uft_geometry_t){80, 2, 22, 512, 300, UFT_ENC_MFM, 500}

#define UFT_GEOM_ST_SS  (uft_geometry_t){80, 1, 9, 512, 300, UFT_ENC_MFM, 250}
#define UFT_GEOM_ST_DS  (uft_geometry_t){80, 2, 9, 512, 300, UFT_ENC_MFM, 250}
#define UFT_GEOM_ST_HD  (uft_geometry_t){80, 2, 18, 512, 300, UFT_ENC_MFM, 500}

/*===========================================================================
 * FORMAT DESCRIPTOR
 *===========================================================================*/

/**
 * @brief Complete format descriptor
 */
typedef struct {
    uft_format_id_t id;
    const char *name;           /**< Short name */
    const char *description;    /**< Human-readable description */
    const char *extension;      /**< Primary file extension */
    const char *extensions_alt; /**< Alternative extensions (comma-sep) */
    uft_platform_t platform;
    uint32_t capabilities;      /**< uft_format_caps_t flags */
    uft_geometry_t default_geometry;
    uint32_t header_size;       /**< Size of format header (0=none) */
    uint32_t magic_offset;      /**< Offset of magic bytes */
    uint32_t magic_size;        /**< Size of magic signature */
    const uint8_t *magic_bytes; /**< Magic signature bytes */
    uint32_t min_size;          /**< Minimum valid file size */
    uint32_t max_size;          /**< Maximum valid file size (0=unlimited) */
} uft_format_desc_t;

/*===========================================================================
 * FORMAT DETECTION
 *===========================================================================*/

/**
 * @brief Detection result with confidence
 */
typedef struct {
    uft_format_id_t format;
    int confidence;             /**< 0-100 */
    const char *reason;
    uft_geometry_t detected_geometry;
    bool has_errors;
} uft_detect_result_t;

/**
 * @brief Detect format from file header
 * @param data File header data
 * @param size Size of header data
 * @param file_size Total file size (for size-based detection)
 * @param extension File extension (optional, for hints)
 * @param results Array to fill with results
 * @param max_results Maximum results to return
 * @return Number of candidate formats found
 */
int uft_detect_format(const uint8_t *data, size_t size, uint64_t file_size,
                      const char *extension, uft_detect_result_t *results,
                      int max_results);

/**
 * @brief Get format descriptor by ID
 */
const uft_format_desc_t* uft_get_format_desc(uft_format_id_t id);

/**
 * @brief Get format descriptor by extension
 */
const uft_format_desc_t* uft_get_format_by_ext(const char *ext);

/**
 * @brief Get all formats for a platform
 */
int uft_get_platform_formats(uft_platform_t platform,
                              uft_format_id_t *formats, int max_formats);

/*===========================================================================
 * COMMON FORMAT HEADERS
 *===========================================================================*/

/**
 * @brief DIM header (X68000)
 */
typedef struct __attribute__((packed)) {
    uint8_t media_type;         /**< 0x00-0x03 = format type */
    uint8_t track_present[160]; /**< Track presence bitmap */
    uint8_t reserved[0x5A];
    char marker[13];            /**< "DIFC HEADER  " at 0xAB */
} uft_dim_header_t;

/**
 * @brief CQM header (CopyQM)
 */
typedef struct __attribute__((packed)) {
    uint8_t reserved[0x18];
    uint16_t bytes_per_sector;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint8_t padding[0x6E];
    uint8_t checksum;           /**< Sum of header bytes = 0 mod 256 */
} uft_cqm_header_t;

/**
 * @brief TD0 header (Teledisk)
 */
typedef struct __attribute__((packed)) {
    char signature[2];          /**< "TD" or "td" */
    uint8_t sequence;           /**< Volume sequence */
    uint8_t check_signature;    /**< Check byte */
    uint8_t version;            /**< Version number */
    uint8_t data_rate;          /**< 0=250K, 1=300K, 2=500K */
    uint8_t drive_type;         /**< Drive type */
    uint8_t stepping;           /**< Stepping flag */
    uint8_t dos_allocation;     /**< DOS allocation flag */
    uint8_t sides;              /**< Number of sides */
    uint16_t crc;               /**< Header CRC */
} uft_td0_header_t;

/**
 * @brief HFE header
 */
typedef struct __attribute__((packed)) {
    char signature[8];          /**< "HXCPICFE" */
    uint8_t revision;
    uint8_t number_of_tracks;
    uint8_t number_of_sides;
    uint8_t track_encoding;     /**< 0=ISO MFM, 1=Amiga MFM, etc */
    uint16_t bit_rate;          /**< kbps / 2 */
    uint16_t floppy_rpm;
    uint8_t floppy_interface;   /**< 0=IBM PC, 1=Amiga, etc */
    uint8_t reserved;
    uint16_t track_list_offset; /**< Offset to track table / 512 */
    uint8_t write_allowed;
    uint8_t single_step;
    uint8_t track0s0_altenc;
    uint8_t track0s0_encoding;
    uint8_t track0s1_altenc;
    uint8_t track0s1_encoding;
} uft_hfe_header_t;

/**
 * @brief EDSK header (Extended DSK)
 */
typedef struct __attribute__((packed)) {
    char signature[34];         /**< "EXTENDED CPC DSK File\r\nDisk-Info\r\n" */
    char creator[14];
    uint8_t tracks;
    uint8_t sides;
    uint16_t unused;
    uint8_t track_sizes[204];   /**< Size/256 for each track */
} uft_edsk_header_t;

/**
 * @brief SCP header (SuperCard Pro)
 */
typedef struct __attribute__((packed)) {
    char signature[3];          /**< "SCP" */
    uint8_t version;
    uint8_t disk_type;
    uint8_t revolutions;
    uint8_t start_track;
    uint8_t end_track;
    uint8_t flags;
    uint8_t bit_cell_encoding;
    uint8_t heads;
    uint8_t resolution;
    uint32_t checksum;
} uft_scp_header_t;

/*===========================================================================
 * D64/G64 SPECIFICS (Commodore)
 *===========================================================================*/

/**
 * @brief D64 zone table: sectors per track by zone
 */
static const uint8_t UFT_D64_SECTORS_PER_TRACK[] = {
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  /* 1-17: 21 sectors */
    19, 19, 19, 19, 19, 19, 19,                                          /* 18-24: 19 sectors */
    18, 18, 18, 18, 18, 18,                                              /* 25-30: 18 sectors */
    17, 17, 17, 17, 17,                                                  /* 31-35: 17 sectors */
    17, 17, 17, 17, 17                                                   /* 36-40: 17 sectors (extended) */
};

/**
 * @brief D64 standard sizes
 */
#define UFT_D64_SIZE_35_NO_ERRORS   174848   /**< 35 tracks, no error info */
#define UFT_D64_SIZE_35_ERRORS      175531   /**< 35 tracks + error info */
#define UFT_D64_SIZE_40_NO_ERRORS   196608   /**< 40 tracks, no error info */
#define UFT_D64_SIZE_40_ERRORS      197376   /**< 40 tracks + error info */

/**
 * @brief G64 header
 */
typedef struct __attribute__((packed)) {
    char signature[8];          /**< "GCR-1541" */
    uint8_t version;
    uint8_t tracks;
    uint16_t track_size;        /**< Maximum track size */
} uft_g64_header_t;

/**
 * @brief Calculate D64 track offset
 */
static inline uint32_t uft_d64_track_offset(int track) {
    uint32_t offset = 0;
    for (int t = 1; t < track && t <= 40; t++) {
        offset += UFT_D64_SECTORS_PER_TRACK[t - 1] * 256;
    }
    return offset;
}

/*===========================================================================
 * AMIGA ADF SPECIFICS
 *===========================================================================*/

/**
 * @brief ADF bootblock magic
 */
#define UFT_ADF_DOS_MAGIC       0x444F5300  /**< "DOS\0" */
#define UFT_ADF_KICKSTART_MAGIC 0x4B49434B  /**< "KICK" */

/**
 * @brief ADF sizes
 */
#define UFT_ADF_SIZE_DD         901120      /**< DD: 80*2*11*512 */
#define UFT_ADF_SIZE_HD         1802240     /**< HD: 80*2*22*512 */

/**
 * @brief Detect ADF filesystem type
 */
typedef enum {
    UFT_ADF_FS_UNKNOWN,
    UFT_ADF_FS_OFS,             /**< Original File System */
    UFT_ADF_FS_FFS,             /**< Fast File System */
    UFT_ADF_FS_OFS_INTL,        /**< OFS International */
    UFT_ADF_FS_FFS_INTL,        /**< FFS International */
    UFT_ADF_FS_OFS_DC,          /**< OFS Directory Cache */
    UFT_ADF_FS_FFS_DC           /**< FFS Directory Cache */
} uft_adf_fs_type_t;

/*===========================================================================
 * ATARI ST SPECIFICS
 *===========================================================================*/

/**
 * @brief STX header
 */
typedef struct __attribute__((packed)) {
    char signature[4];          /**< "RSY\0" */
    uint16_t version;
    uint16_t tool_used;
    uint16_t reserved1;
    uint8_t tracks;
    uint8_t revision;
    uint32_t reserved2;
} uft_stx_header_t;

/**
 * @brief MSA header
 */
typedef struct __attribute__((packed)) {
    uint16_t signature;         /**< 0x0E0F */
    uint16_t sectors_per_track;
    uint16_t sides;             /**< 0 or 1 */
    uint16_t starting_track;
    uint16_t ending_track;
} uft_msa_header_t;

/*===========================================================================
 * GUI INTEGRATION - FORMAT SELECTION
 *===========================================================================*/

/**
 * @brief Format info for GUI display
 */
typedef struct {
    uft_format_id_t id;
    const char *name;
    const char *platform_name;
    const char *description;
    uint32_t typical_size;
    bool can_read;
    bool can_write;
    bool supports_protection;
} uft_format_gui_info_t;

/**
 * @brief Get all formats for GUI list
 */
int uft_get_all_formats_gui(uft_format_gui_info_t *info, int max_count);

/**
 * @brief Get formats filtered by capability
 */
int uft_get_formats_by_caps(uint32_t required_caps, uint32_t excluded_caps,
                            uft_format_gui_info_t *info, int max_count);

/**
 * @brief Format conversion compatibility check
 */
typedef struct {
    uft_format_id_t source;
    uft_format_id_t target;
    bool direct_convert;        /**< Lossless 1:1 conversion possible */
    bool lossy_convert;         /**< Conversion with data loss */
    const char *warning;        /**< Warning message if lossy */
} uft_convert_compat_t;

/**
 * @brief Check conversion compatibility
 */
int uft_check_convert_compat(uft_format_id_t source, uft_format_id_t target,
                              uft_convert_compat_t *result);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLOPPY_FORMATS_H */
