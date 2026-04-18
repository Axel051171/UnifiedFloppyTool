/**
 * @file uft_format_detect_complete.h
 * @brief Complete Format Detection Header
 * @version 5.31.0
 */

#ifndef UFT_FORMAT_DETECT_COMPLETE_H
#define UFT_FORMAT_DETECT_COMPLETE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Format enumeration - 70+ formats supported */
typedef enum {
    UFT_FMT_UNKNOWN = 0,
    
    /* Amiga (5) */
    UFT_FMT_ADF, UFT_FMT_ADZ, UFT_FMT_DMS, UFT_FMT_IPF, UFT_FMT_ADF_EXT,
    
    /* Commodore (14) */
    UFT_FMT_D64, UFT_FMT_D71, UFT_FMT_D81, UFT_FMT_D80, UFT_FMT_D82,
    UFT_FMT_G64, UFT_FMT_G71, UFT_FMT_NIB, UFT_FMT_NBZ, UFT_FMT_P64,
    UFT_FMT_X64, UFT_FMT_T64, UFT_FMT_TAP_C64,
    
    /* Apple II (8) */
    UFT_FMT_DO, UFT_FMT_PO, UFT_FMT_NIB_APPLE, UFT_FMT_2MG, 
    UFT_FMT_WOZ, UFT_FMT_A2R, UFT_FMT_DSK_APPLE, UFT_FMT_DC42,
    
    /* Atari (9) */
    UFT_FMT_ATR, UFT_FMT_ATX, UFT_FMT_XFD, UFT_FMT_DCM, UFT_FMT_PRO,
    UFT_FMT_ST, UFT_FMT_MSA, UFT_FMT_STX, UFT_FMT_DIM,
    
    /* PC (10) */
    UFT_FMT_IMG, UFT_FMT_IMA, UFT_FMT_IMD, UFT_FMT_TD0, UFT_FMT_FDI,
    UFT_FMT_DSK_CPC, UFT_FMT_EDSK, UFT_FMT_MFM, UFT_FMT_ISO, UFT_FMT_VFD,
    
    /* Flux (8) */
    UFT_FMT_SCP, UFT_FMT_HFE, UFT_FMT_HFE_V3, UFT_FMT_KRYOFLUX,
    UFT_FMT_DMK, UFT_FMT_FLX, UFT_FMT_CT, UFT_FMT_RAW_FLUX,
    
    /* Spectrum (7) */
    UFT_FMT_TRD, UFT_FMT_SCL, UFT_FMT_FDD, UFT_FMT_OPD, UFT_FMT_MGT,
    UFT_FMT_TAP, UFT_FMT_TZX,
    
    /* BBC/Acorn (5) */
    UFT_FMT_SSD, UFT_FMT_DSD, UFT_FMT_ADF_ACORN, UFT_FMT_ADL, UFT_FMT_UEF,
    
    /* TRS-80 (3) */
    UFT_FMT_JV1, UFT_FMT_JV3, UFT_FMT_DMK_TRS,
    
    /* Japanese (6) */
    UFT_FMT_D88, UFT_FMT_NFD, UFT_FMT_FDI_PC98, UFT_FMT_HDM,
    UFT_FMT_XDF, UFT_FMT_DIM_X68,
    
    /* SAM Coupé (3) */
    UFT_FMT_SAD, UFT_FMT_SDF, UFT_FMT_MGT_SAM,
    
    /* Other (7) */
    UFT_FMT_DSK_MSX, UFT_FMT_CAS, UFT_FMT_DSK_CPC464, UFT_FMT_TI99,
    UFT_FMT_DSK_ORIC, UFT_FMT_DSK_DRAGON, UFT_FMT_VDK, UFT_FMT_OS9,
    
    UFT_FMT_COUNT
} uft_format_t;

/* Detection result structure */
typedef struct {
    uft_format_t format;        /* Detected format enum */
    char         name[32];      /* Format name (e.g., "D64") */
    char         version[32];   /* Version/variant (e.g., "35 Track") */
    char         system[32];    /* System name (e.g., "Commodore 64") */
    char         description[128]; /* Full description */
    int          confidence;    /* 0-100 confidence score */
    
    /* Geometry */
    int          tracks;
    int          sides;
    int          sectors;
    int          sector_size;
    uint64_t     file_size;
    
    /* Flags */
    bool         has_errors;
    bool         write_protected;
    bool         compressed;
    bool         copy_protected;
} uft_format_info_t;

/**
 * @brief Detect disk image format
 * 
 * @param path      Path to disk image file
 * @param info      Output: detected format information
 * @return          0 on success, -1 on error
 */
int uft_detect_format(const char *path, uft_format_info_t *info);

/**
 * @brief Detect and print format information
 */
void uft_detect_format_print(const char *path);

/**
 * @brief Get format name string
 */
const char* uft_format_name(uft_format_t format);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMAT_DETECT_COMPLETE_H */
