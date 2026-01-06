/**
 * @file uft_format_params.h
 * @brief Unified Format Parameters for All Supported Platforms
 * 
 * Part of UFT God Mode - Complete format parameter system
 * 
 * PLATFORMS SUPPORTED:
 * - PC/DOS (IMG, IMA, DSK, VFD)
 * - Commodore (D64, G64, D71, D81, D80, D82)
 * - Amiga (ADF, ExtADF, DMS, ADZ)
 * - Atari ST (ST, MSA, STX, DIM)
 * - Atari 8-bit (ATR, ATX, XFD, DCM)
 * - Apple II (DO, PO, NIB, 2MG, WOZ)
 * - Apple Mac (DSK, DC42, DART)
 * - BBC/Acorn (SSD, DSD, ADF, ADL)
 * - TRS-80 (DMK, JV1, JV3)
 * - MSX (DSK)
 * - Amstrad CPC (DSK, EDSK)
 * - Spectrum (TRD, SCL, FDI)
 * - PC-98 (D88, FDI, HDM)
 * - Flux (SCP, KF, HFE, A2R)
 */

#ifndef UFT_FORMAT_PARAMS_H
#define UFT_FORMAT_PARAMS_H

#include <stdint.h>
#include <stdbool.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Format Categories
 * ============================================================================ */

typedef enum {
    UFT_CAT_PC_DOS,
    UFT_CAT_COMMODORE,
    UFT_CAT_AMIGA,
    UFT_CAT_ATARI_ST,
    UFT_CAT_ATARI_8BIT,
    UFT_CAT_APPLE_II,
    UFT_CAT_APPLE_MAC,
    UFT_CAT_BBC_ACORN,
    UFT_CAT_TRS80,
    UFT_CAT_MSX,
    UFT_CAT_AMSTRAD,
    UFT_CAT_SPECTRUM,
    UFT_CAT_PC98,
    UFT_CAT_FLUX,
    UFT_CAT_OTHER,
    UFT_CAT_COUNT
} uft_format_category_t;

/* ============================================================================
 * Format IDs (Extended)
 * ============================================================================ */

typedef enum {
    /* PC/DOS */
    UFT_FMT_IMG = 0,        /**< Raw sector image */
    UFT_FMT_IMA,            /**< ImageDisk format */
    UFT_FMT_VFD,            /**< Virtual Floppy Disk */
    UFT_FMT_IMD,            /**< ImageDisk with metadata */
    UFT_FMT_TD0,            /**< TeleDisk */
    UFT_FMT_FDI_PC,         /**< FDI (PC variant) */
    
    /* Commodore */
    UFT_FMT_D64,            /**< C64 1541 image */
    UFT_FMT_G64,            /**< C64 GCR flux-level */
    UFT_FMT_D71,            /**< C128 1571 image */
    UFT_FMT_D81,            /**< C128/C65 1581 image */
    UFT_FMT_D80,            /**< CBM 8050 image */
    UFT_FMT_D82,            /**< CBM 8250 image */
    UFT_FMT_P64,            /**< P64 flux format */
    UFT_FMT_NIB,            /**< NIB raw GCR */
    
    /* Amiga */
    UFT_FMT_ADF,            /**< Amiga Disk File */
    UFT_FMT_ADF_EXT,        /**< Extended ADF */
    UFT_FMT_DMS,            /**< Disk Masher System */
    UFT_FMT_ADZ,            /**< Gzipped ADF */
    UFT_FMT_FDI_AMIGA,      /**< FDI (Amiga variant) */
    
    /* Atari ST */
    UFT_FMT_ST,             /**< Raw ST image */
    UFT_FMT_MSA,            /**< Magic Shadow Archiver */
    UFT_FMT_STX,            /**< Pasti format */
    UFT_FMT_DIM,            /**< FastCopy DIM */
    
    /* Atari 8-bit */
    UFT_FMT_ATR,            /**< Atari 8-bit image */
    UFT_FMT_ATX,            /**< Atari 8-bit extended */
    UFT_FMT_XFD,            /**< Xformer image */
    UFT_FMT_DCM,            /**< DiskComm compressed */
    
    /* Apple II */
    UFT_FMT_DO,             /**< DOS 3.3 order */
    UFT_FMT_PO,             /**< ProDOS order */
    UFT_FMT_NIB_APPLE,      /**< Apple NIB */
    UFT_FMT_2MG,            /**< 2IMG universal */
    UFT_FMT_WOZ,            /**< WOZ flux format */
    UFT_FMT_A2R,            /**< Applesauce A2R */
    
    /* Apple Mac */
    UFT_FMT_DSK_MAC,        /**< Mac 400K/800K */
    UFT_FMT_DC42,           /**< Disk Copy 4.2 */
    UFT_FMT_DART,           /**< DART compressed */
    
    /* BBC/Acorn */
    UFT_FMT_SSD,            /**< Single-sided DFS */
    UFT_FMT_DSD,            /**< Double-sided DFS */
    UFT_FMT_ADF_BBC,        /**< ADFS image */
    UFT_FMT_ADL,            /**< ADFS large */
    
    /* TRS-80 */
    UFT_FMT_DMK,            /**< DMK format */
    UFT_FMT_JV1,            /**< JV1 format */
    UFT_FMT_JV3,            /**< JV3 format */
    
    /* MSX */
    UFT_FMT_DSK_MSX,        /**< MSX disk */
    
    /* Amstrad CPC */
    UFT_FMT_DSK_CPC,        /**< Standard CPC */
    UFT_FMT_EDSK,           /**< Extended DSK */
    
    /* Spectrum */
    UFT_FMT_TRD,            /**< TR-DOS */
    UFT_FMT_SCL,            /**< SCL archive */
    UFT_FMT_FDI_SPEC,       /**< FDI Spectrum */
    
    /* PC-98 */
    UFT_FMT_D88,            /**< D88 format */
    UFT_FMT_FDI_98,         /**< FDI PC-98 */
    UFT_FMT_HDM,            /**< HDM format */
    UFT_FMT_NFD,            /**< NFD format */
    
    /* Flux */
    UFT_FMT_SCP,            /**< SuperCard Pro */
    UFT_FMT_KF,             /**< KryoFlux stream */
    UFT_FMT_HFE,            /**< HxC Floppy Emulator */
    UFT_FMT_HFE_V3,         /**< HFE version 3 */
    UFT_FMT_MFM,            /**< Raw MFM stream */
    UFT_FMT_IPF,            /**< Interchangeable */
    
    UFT_FMT_COUNT,
    UFT_FMT_UNKNOWN = -1
} uft_format_id_t;

/* ============================================================================
 * Encoding Types
 * ============================================================================ */

typedef enum {
    UFT_ENC_MFM,            /**< Modified Frequency Modulation */
    UFT_ENC_FM,             /**< Frequency Modulation */
    UFT_ENC_GCR_CBM,        /**< Commodore GCR */
    UFT_ENC_GCR_APPLE,      /**< Apple GCR */
    UFT_ENC_GCR_MAC,        /**< Macintosh GCR */
    UFT_ENC_GCR_VICTOR,     /**< Victor 9000 GCR */
    UFT_ENC_M2FM,           /**< Modified MFM (DEC) */
    UFT_ENC_RAW,            /**< Raw flux data */
} uft_encoding_type_t;

/* ============================================================================
 * Format Definition Structure
 * ============================================================================ */

typedef struct {
    /* Identification */
    uft_format_id_t id;
    const char* name;           /**< Short name (e.g., "d64") */
    const char* display_name;   /**< Display name */
    const char* description;
    const char* extension;      /**< Primary file extension */
    const char* extensions_alt; /**< Alternative extensions */
    uft_format_category_t category;
    
    /* Magic bytes for detection */
    const uint8_t* magic;
    size_t magic_len;
    size_t magic_offset;
    
    /* Geometry */
    struct {
        int32_t cylinders;
        int32_t heads;
        int32_t sectors_per_track;  /**< 0 = variable */
        int32_t sector_size;
        int32_t sector_base;        /**< First sector number */
        uint32_t total_size;        /**< Expected size (0 = variable) */
    } geometry;
    
    /* Encoding */
    uft_encoding_type_t encoding;
    uint32_t data_rate;             /**< Bits per second */
    double rpm;                     /**< Rotations per minute */
    
    /* Capabilities */
    uint32_t caps;
    
    /* Platform-specific data */
    void* platform_data;
    
} uft_format_def_t;

/* Capability flags */
#define UFT_CAP_READ            0x0001
#define UFT_CAP_WRITE           0x0002
#define UFT_CAP_CREATE          0x0004
#define UFT_CAP_VARIABLE_SPT    0x0008
#define UFT_CAP_ERROR_MAP       0x0010
#define UFT_CAP_FLUX_LEVEL      0x0020
#define UFT_CAP_COMPRESSED      0x0040
#define UFT_CAP_MULTI_REV       0x0080
#define UFT_CAP_WEAK_BITS       0x0100
#define UFT_CAP_HALF_TRACKS     0x0200
#define UFT_CAP_PROTECTION      0x0400

/* ============================================================================
 * Format Parameters (Runtime)
 * ============================================================================ */

typedef struct {
    uft_format_id_t format;
    
    /* Geometry override */
    int32_t cylinders;
    int32_t heads;
    int32_t sectors_per_track;
    int32_t sector_size;
    int32_t sector_base;
    
    /* Timing override */
    uint32_t data_rate;
    double rpm;
    double cell_time_ns;
    
    /* Options */
    bool auto_detect;
    bool verify_after_write;
    bool preserve_errors;
    bool include_flux;
    
    /* Validation */
    bool validated;
    char error_msg[256];
    
} uft_format_params_t;

/* ============================================================================
 * Function Prototypes
 * ============================================================================ */

/**
 * @brief Get format definition by ID
 */
const uft_format_def_t* uft_format_get_def(uft_format_id_t id);

/**
 * @brief Get format definition by name
 */
const uft_format_def_t* uft_format_get_by_name(const char* name);

/**
 * @brief Get format definition by extension
 */
const uft_format_def_t* uft_format_get_by_extension(const char* ext);

/**
 * @brief Detect format from file content
 */
uft_format_id_t uft_format_detect(const uint8_t* data, size_t size, 
                                   const char* filename);

/**
 * @brief Get all formats in a category
 */
int uft_format_get_by_category(uft_format_category_t cat,
                                uft_format_id_t* out, int max_count);

/**
 * @brief Get category name
 */
const char* uft_format_category_name(uft_format_category_t cat);

/**
 * @brief Get default parameters for format
 */
uft_format_params_t uft_format_params_default(uft_format_id_t format);

/**
 * @brief Validate format parameters
 */
bool uft_format_params_validate(uft_format_params_t* params);

/**
 * @brief Convert format params to JSON
 */
char* uft_format_params_to_json(const uft_format_params_t* params);

/* ============================================================================
 * Quick Access Macros
 * ============================================================================ */

/* Commodore presets */
#define UFT_PRESET_D64_35()  uft_format_params_default(UFT_FMT_D64)
#define UFT_PRESET_D64_40()  ({ uft_format_params_t p = uft_format_params_default(UFT_FMT_D64); p.cylinders = 40; p; })
#define UFT_PRESET_G64()     uft_format_params_default(UFT_FMT_G64)
#define UFT_PRESET_D81()     uft_format_params_default(UFT_FMT_D81)

/* Amiga presets */
#define UFT_PRESET_ADF_DD()  uft_format_params_default(UFT_FMT_ADF)
#define UFT_PRESET_ADF_HD()  ({ uft_format_params_t p = uft_format_params_default(UFT_FMT_ADF); p.sectors_per_track = 22; p; })

/* Apple presets */
#define UFT_PRESET_DO()      uft_format_params_default(UFT_FMT_DO)
#define UFT_PRESET_PO()      uft_format_params_default(UFT_FMT_PO)
#define UFT_PRESET_WOZ()     uft_format_params_default(UFT_FMT_WOZ)

/* Atari presets */
#define UFT_PRESET_ST_SS()   uft_format_params_default(UFT_FMT_ST)
#define UFT_PRESET_ST_DS()   ({ uft_format_params_t p = uft_format_params_default(UFT_FMT_ST); p.heads = 2; p; })
#define UFT_PRESET_ATR()     uft_format_params_default(UFT_FMT_ATR)

/* PC presets */
#define UFT_PRESET_PC_720K() ({ uft_format_params_t p = uft_format_params_default(UFT_FMT_IMG); \
    p.cylinders = 80; p.heads = 2; p.sectors_per_track = 9; p; })
#define UFT_PRESET_PC_1440K() ({ uft_format_params_t p = uft_format_params_default(UFT_FMT_IMG); \
    p.cylinders = 80; p.heads = 2; p.sectors_per_track = 18; p.data_rate = 500000; p; })

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMAT_PARAMS_H */
