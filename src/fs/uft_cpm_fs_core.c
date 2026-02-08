/**
 * @file uft_cpm_fs_core.c
 * @brief CP/M Filesystem Core - DPB Database and Basic Operations
 * 
 * @author UFT Team
 * @version 1.0.0
 */

#include "uft/fs/uft_cpm_fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*===========================================================================
 * DPB Database - Known CP/M Formats
 *===========================================================================*/

/**
 * @brief Disk Parameter Block database
 * 
 * Comprehensive collection of CP/M disk formats
 */
static const uft_cpm_dpb_t g_dpb_database[] = {
    /* 8" SSSD - IBM 3740 format (original CP/M) */
    {
        .name = "8\" SSSD (IBM 3740)",
        .tracks = 77, .sides = 1, .sectors_per_track = 26, .sector_size = 128,
        .spt = 26, .bsh = 3, .blm = 7, .exm = 0,
        .dsm = 242, .drm = 63, .al0 = 0xC0, .al1 = 0x00,
        .cks = 16, .off = 2,
        .block_size = 1024, .dir_entries = 64, .dir_blocks = 2,
        .total_bytes = 256256,
        .first_sector = 1, .skew = 6, .skew_type = UFT_CPM_SKEW_PHYSICAL,
        .side_order = UFT_CPM_SIDES_ALT,
        .format = UFT_CPM_FMT_8_SSSD, .version = UFT_CPM_VERSION_22
    },
    
    /* 8" DSDD */
    {
        .name = "8\" DSDD",
        .tracks = 77, .sides = 2, .sectors_per_track = 26, .sector_size = 256,
        .spt = 52, .bsh = 4, .blm = 15, .exm = 1,
        .dsm = 493, .drm = 127, .al0 = 0xC0, .al1 = 0x00,
        .cks = 32, .off = 2,
        .block_size = 2048, .dir_entries = 128, .dir_blocks = 2,
        .total_bytes = 1013760,
        .first_sector = 1, .skew = 6, .skew_type = UFT_CPM_SKEW_PHYSICAL,
        .side_order = UFT_CPM_SIDES_ALT,
        .format = UFT_CPM_FMT_8_DSDD, .version = UFT_CPM_VERSION_22
    },
    
    /* Kaypro II - SSDD */
    {
        .name = "Kaypro II",
        .tracks = 40, .sides = 1, .sectors_per_track = 10, .sector_size = 512,
        .spt = 40, .bsh = 3, .blm = 7, .exm = 0,
        .dsm = 194, .drm = 63, .al0 = 0xC0, .al1 = 0x00,
        .cks = 16, .off = 1,
        .block_size = 1024, .dir_entries = 64, .dir_blocks = 2,
        .total_bytes = 200704,
        .first_sector = 0, .skew = 0, .skew_type = UFT_CPM_SKEW_NONE,
        .side_order = UFT_CPM_SIDES_ALT,
        .format = UFT_CPM_FMT_KAYPRO_II, .version = UFT_CPM_VERSION_22
    },
    
    /* Kaypro 4 - DSDD */
    {
        .name = "Kaypro 4",
        .tracks = 40, .sides = 2, .sectors_per_track = 10, .sector_size = 512,
        .spt = 40, .bsh = 4, .blm = 15, .exm = 1,
        .dsm = 195, .drm = 63, .al0 = 0x80, .al1 = 0x00,
        .cks = 16, .off = 1,
        .block_size = 2048, .dir_entries = 64, .dir_blocks = 1,
        .total_bytes = 401408,
        .first_sector = 0, .skew = 0, .skew_type = UFT_CPM_SKEW_NONE,
        .side_order = UFT_CPM_SIDES_SEQ,
        .format = UFT_CPM_FMT_KAYPRO_4, .version = UFT_CPM_VERSION_22
    },
    
    /* Kaypro 10 - DSQD */
    {
        .name = "Kaypro 10",
        .tracks = 80, .sides = 2, .sectors_per_track = 10, .sector_size = 512,
        .spt = 40, .bsh = 4, .blm = 15, .exm = 1,
        .dsm = 393, .drm = 127, .al0 = 0xC0, .al1 = 0x00,
        .cks = 32, .off = 2,
        .block_size = 2048, .dir_entries = 128, .dir_blocks = 2,
        .total_bytes = 806912,
        .first_sector = 0, .skew = 0, .skew_type = UFT_CPM_SKEW_NONE,
        .side_order = UFT_CPM_SIDES_SEQ,
        .format = UFT_CPM_FMT_KAYPRO_10, .version = UFT_CPM_VERSION_22
    },
    
    /* Osborne 1 - SSDD */
    {
        .name = "Osborne 1",
        .tracks = 40, .sides = 1, .sectors_per_track = 10, .sector_size = 256,
        .spt = 20, .bsh = 3, .blm = 7, .exm = 0,
        .dsm = 45, .drm = 63, .al0 = 0xC0, .al1 = 0x00,
        .cks = 16, .off = 3,
        .block_size = 1024, .dir_entries = 64, .dir_blocks = 2,
        .total_bytes = 92160,
        .first_sector = 1, .skew = 2, .skew_type = UFT_CPM_SKEW_PHYSICAL,
        .side_order = UFT_CPM_SIDES_ALT,
        .format = UFT_CPM_FMT_OSBORNE_1, .version = UFT_CPM_VERSION_22
    },
    
    /* Osborne Double Density */
    {
        .name = "Osborne DD",
        .tracks = 40, .sides = 2, .sectors_per_track = 5, .sector_size = 1024,
        .spt = 40, .bsh = 3, .blm = 7, .exm = 0,
        .dsm = 186, .drm = 63, .al0 = 0xC0, .al1 = 0x00,
        .cks = 16, .off = 3,
        .block_size = 1024, .dir_entries = 64, .dir_blocks = 2,
        .total_bytes = 389120,
        .first_sector = 1, .skew = 0, .skew_type = UFT_CPM_SKEW_NONE,
        .side_order = UFT_CPM_SIDES_ALT,
        .format = UFT_CPM_FMT_OSBORNE_DD, .version = UFT_CPM_VERSION_22
    },
    
    /* Amstrad CPC System Format */
    {
        .name = "Amstrad CPC System",
        .tracks = 40, .sides = 1, .sectors_per_track = 9, .sector_size = 512,
        .spt = 36, .bsh = 3, .blm = 7, .exm = 0,
        .dsm = 170, .drm = 63, .al0 = 0xC0, .al1 = 0x00,
        .cks = 16, .off = 2,
        .block_size = 1024, .dir_entries = 64, .dir_blocks = 2,
        .total_bytes = 178176,
        .first_sector = 0x41, .skew = 0, .skew_type = UFT_CPM_SKEW_NONE,
        .side_order = UFT_CPM_SIDES_ALT,
        .format = UFT_CPM_FMT_AMSTRAD_CPC_SYS, .version = UFT_CPM_VERSION_22
    },
    
    /* Amstrad CPC Data Format */
    {
        .name = "Amstrad CPC Data",
        .tracks = 40, .sides = 1, .sectors_per_track = 9, .sector_size = 512,
        .spt = 36, .bsh = 3, .blm = 7, .exm = 0,
        .dsm = 179, .drm = 63, .al0 = 0xC0, .al1 = 0x00,
        .cks = 16, .off = 0,
        .block_size = 1024, .dir_entries = 64, .dir_blocks = 2,
        .total_bytes = 184320,
        .first_sector = 0xC1, .skew = 0, .skew_type = UFT_CPM_SKEW_NONE,
        .side_order = UFT_CPM_SIDES_ALT,
        .format = UFT_CPM_FMT_AMSTRAD_CPC_DATA, .version = UFT_CPM_VERSION_22
    },
    
    /* Amstrad PCW */
    {
        .name = "Amstrad PCW",
        .tracks = 80, .sides = 1, .sectors_per_track = 9, .sector_size = 512,
        .spt = 36, .bsh = 4, .blm = 15, .exm = 1,
        .dsm = 174, .drm = 127, .al0 = 0xC0, .al1 = 0x00,
        .cks = 32, .off = 1,
        .block_size = 2048, .dir_entries = 128, .dir_blocks = 2,
        .total_bytes = 358400,
        .first_sector = 1, .skew = 0, .skew_type = UFT_CPM_SKEW_NONE,
        .side_order = UFT_CPM_SIDES_ALT,
        .format = UFT_CPM_FMT_AMSTRAD_PCW, .version = UFT_CPM_VERSION_30
    },
    
    /* Epson QX-10 */
    {
        .name = "Epson QX-10",
        .tracks = 40, .sides = 2, .sectors_per_track = 16, .sector_size = 256,
        .spt = 64, .bsh = 3, .blm = 7, .exm = 0,
        .dsm = 315, .drm = 127, .al0 = 0xF0, .al1 = 0x00,
        .cks = 32, .off = 2,
        .block_size = 1024, .dir_entries = 128, .dir_blocks = 4,
        .total_bytes = 327680,
        .first_sector = 1, .skew = 0, .skew_type = UFT_CPM_SKEW_NONE,
        .side_order = UFT_CPM_SIDES_SEQ,
        .format = UFT_CPM_FMT_EPSON_QX10, .version = UFT_CPM_VERSION_22
    },
    
    /* Commodore 128 CP/M */
    {
        .name = "Commodore 128 CP/M",
        .tracks = 40, .sides = 2, .sectors_per_track = 17, .sector_size = 256,
        .spt = 68, .bsh = 3, .blm = 7, .exm = 0,
        .dsm = 327, .drm = 127, .al0 = 0xF0, .al1 = 0x00,
        .cks = 32, .off = 2,
        .block_size = 1024, .dir_entries = 128, .dir_blocks = 4,
        .total_bytes = 348160,
        .first_sector = 0, .skew = 0, .skew_type = UFT_CPM_SKEW_LOGICAL,
        .side_order = UFT_CPM_SIDES_ALT,
        .format = UFT_CPM_FMT_C128, .version = UFT_CPM_VERSION_30
    },
    
    /* Apple II CP/M */
    {
        .name = "Apple II CP/M",
        .tracks = 35, .sides = 1, .sectors_per_track = 16, .sector_size = 256,
        .spt = 32, .bsh = 3, .blm = 7, .exm = 0,
        .dsm = 127, .drm = 63, .al0 = 0xC0, .al1 = 0x00,
        .cks = 16, .off = 3,
        .block_size = 1024, .dir_entries = 64, .dir_blocks = 2,
        .total_bytes = 143360,
        .first_sector = 0, .skew = 0, .skew_type = UFT_CPM_SKEW_CUSTOM,
        .skew_table = {0,6,12,3,9,15,14,5,11,2,8,14,1,7,13,4},
        .side_order = UFT_CPM_SIDES_ALT,
        .format = UFT_CPM_FMT_APPLE_CPM, .version = UFT_CPM_VERSION_22
    },
    
    /* TRS-80 Model 4 CP/M */
    {
        .name = "TRS-80 Model 4 CP/M",
        .tracks = 40, .sides = 2, .sectors_per_track = 18, .sector_size = 256,
        .spt = 72, .bsh = 4, .blm = 15, .exm = 0,
        .dsm = 177, .drm = 127, .al0 = 0xC0, .al1 = 0x00,
        .cks = 32, .off = 2,
        .block_size = 2048, .dir_entries = 128, .dir_blocks = 2,
        .total_bytes = 368640,
        .first_sector = 0, .skew = 0, .skew_type = UFT_CPM_SKEW_NONE,
        .side_order = UFT_CPM_SIDES_SEQ,
        .format = UFT_CPM_FMT_TRS80_M4, .version = UFT_CPM_VERSION_22
    },
    
    /* BBC Master 512 CP/M */
    {
        .name = "BBC Master 512 CP/M",
        .tracks = 80, .sides = 2, .sectors_per_track = 5, .sector_size = 1024,
        .spt = 40, .bsh = 4, .blm = 15, .exm = 1,
        .dsm = 197, .drm = 127, .al0 = 0xC0, .al1 = 0x00,
        .cks = 32, .off = 1,
        .block_size = 2048, .dir_entries = 128, .dir_blocks = 2,
        .total_bytes = 409600,
        .first_sector = 0, .skew = 0, .skew_type = UFT_CPM_SKEW_NONE,
        .side_order = UFT_CPM_SIDES_SEQ,
        .format = UFT_CPM_FMT_BBC_CPM, .version = UFT_CPM_VERSION_30
    },
    
    /* Morrow Micro Decision */
    {
        .name = "Morrow Micro Decision",
        .tracks = 40, .sides = 2, .sectors_per_track = 10, .sector_size = 512,
        .spt = 40, .bsh = 4, .blm = 15, .exm = 1,
        .dsm = 195, .drm = 127, .al0 = 0xC0, .al1 = 0x00,
        .cks = 32, .off = 2,
        .block_size = 2048, .dir_entries = 128, .dir_blocks = 2,
        .total_bytes = 409600,
        .first_sector = 1, .skew = 3, .skew_type = UFT_CPM_SKEW_PHYSICAL,
        .side_order = UFT_CPM_SIDES_ALT,
        .format = UFT_CPM_FMT_MORROW, .version = UFT_CPM_VERSION_22
    },
    
    /* Xerox 820 */
    {
        .name = "Xerox 820",
        .tracks = 40, .sides = 1, .sectors_per_track = 18, .sector_size = 128,
        .spt = 18, .bsh = 3, .blm = 7, .exm = 0,
        .dsm = 89, .drm = 31, .al0 = 0x80, .al1 = 0x00,
        .cks = 8, .off = 2,
        .block_size = 1024, .dir_entries = 32, .dir_blocks = 1,
        .total_bytes = 92160,
        .first_sector = 1, .skew = 5, .skew_type = UFT_CPM_SKEW_PHYSICAL,
        .side_order = UFT_CPM_SIDES_ALT,
        .format = UFT_CPM_FMT_XEROX_820, .version = UFT_CPM_VERSION_22
    },
    
    /* Zorba */
    {
        .name = "Zorba",
        .tracks = 40, .sides = 2, .sectors_per_track = 9, .sector_size = 512,
        .spt = 36, .bsh = 4, .blm = 15, .exm = 0,
        .dsm = 174, .drm = 127, .al0 = 0xC0, .al1 = 0x00,
        .cks = 32, .off = 2,
        .block_size = 2048, .dir_entries = 128, .dir_blocks = 2,
        .total_bytes = 368640,
        .first_sector = 1, .skew = 0, .skew_type = UFT_CPM_SKEW_NONE,
        .side_order = UFT_CPM_SIDES_ALT,
        .format = UFT_CPM_FMT_ZORBA, .version = UFT_CPM_VERSION_22
    },
    
    /* NEC PC-8801 */
    {
        .name = "NEC PC-8801",
        .tracks = 80, .sides = 2, .sectors_per_track = 16, .sector_size = 256,
        .spt = 64, .bsh = 4, .blm = 15, .exm = 1,
        .dsm = 315, .drm = 127, .al0 = 0xC0, .al1 = 0x00,
        .cks = 32, .off = 2,
        .block_size = 2048, .dir_entries = 128, .dir_blocks = 2,
        .total_bytes = 655360,
        .first_sector = 1, .skew = 0, .skew_type = UFT_CPM_SKEW_NONE,
        .side_order = UFT_CPM_SIDES_SEQ,
        .format = UFT_CPM_FMT_NEC_PC88, .version = UFT_CPM_VERSION_22
    },
    
    /* NEC PC-9801 */
    {
        .name = "NEC PC-9801",
        .tracks = 77, .sides = 2, .sectors_per_track = 8, .sector_size = 1024,
        .spt = 64, .bsh = 4, .blm = 15, .exm = 0,
        .dsm = 615, .drm = 127, .al0 = 0xC0, .al1 = 0x00,
        .cks = 32, .off = 1,
        .block_size = 2048, .dir_entries = 128, .dir_blocks = 2,
        .total_bytes = 1261568,
        .first_sector = 1, .skew = 0, .skew_type = UFT_CPM_SKEW_NONE,
        .side_order = UFT_CPM_SIDES_SEQ,
        .format = UFT_CPM_FMT_NEC_PC98, .version = UFT_CPM_VERSION_22
    },
    
    /* MSX-DOS */
    {
        .name = "MSX-DOS",
        .tracks = 80, .sides = 2, .sectors_per_track = 9, .sector_size = 512,
        .spt = 36, .bsh = 4, .blm = 15, .exm = 1,
        .dsm = 354, .drm = 127, .al0 = 0xC0, .al1 = 0x00,
        .cks = 32, .off = 1,
        .block_size = 2048, .dir_entries = 128, .dir_blocks = 2,
        .total_bytes = 737280,
        .first_sector = 1, .skew = 0, .skew_type = UFT_CPM_SKEW_NONE,
        .side_order = UFT_CPM_SIDES_ALT,
        .format = UFT_CPM_FMT_MSX_DOS, .version = UFT_CPM_VERSION_MSDOS
    },
    
    /* Generic 5.25" DSDD */
    {
        .name = "5.25\" DSDD Generic",
        .tracks = 40, .sides = 2, .sectors_per_track = 9, .sector_size = 512,
        .spt = 36, .bsh = 4, .blm = 15, .exm = 1,
        .dsm = 174, .drm = 63, .al0 = 0x80, .al1 = 0x00,
        .cks = 16, .off = 2,
        .block_size = 2048, .dir_entries = 64, .dir_blocks = 1,
        .total_bytes = 368640,
        .first_sector = 1, .skew = 0, .skew_type = UFT_CPM_SKEW_NONE,
        .side_order = UFT_CPM_SIDES_ALT,
        .format = UFT_CPM_FMT_525_DSDD, .version = UFT_CPM_VERSION_22
    },
    
    /* Generic 3.5" DSDD (720K) */
    {
        .name = "3.5\" DSDD Generic",
        .tracks = 80, .sides = 2, .sectors_per_track = 9, .sector_size = 512,
        .spt = 36, .bsh = 4, .blm = 15, .exm = 1,
        .dsm = 354, .drm = 127, .al0 = 0xC0, .al1 = 0x00,
        .cks = 32, .off = 2,
        .block_size = 2048, .dir_entries = 128, .dir_blocks = 2,
        .total_bytes = 737280,
        .first_sector = 1, .skew = 0, .skew_type = UFT_CPM_SKEW_NONE,
        .side_order = UFT_CPM_SIDES_ALT,
        .format = UFT_CPM_FMT_35_DSDD, .version = UFT_CPM_VERSION_22
    },
};

#define DPB_COUNT (sizeof(g_dpb_database) / sizeof(g_dpb_database[0]))

/*===========================================================================
 * Format/Version Name Strings
 *===========================================================================*/

const char *uft_cpm_format_name(uft_cpm_format_t format) {
    for (size_t i = 0; i < DPB_COUNT; i++) {
        if (g_dpb_database[i].format == format) {
            return g_dpb_database[i].name;
        }
    }
    
    switch (format) {
        case UFT_CPM_FMT_UNKNOWN: return "Unknown";
        case UFT_CPM_FMT_GENERIC: return "Generic CP/M";
        default: return "Unknown";
    }
}

const char *uft_cpm_version_name(uft_cpm_version_t version) {
    switch (version) {
        case UFT_CPM_VERSION_UNKNOWN: return "Unknown";
        case UFT_CPM_VERSION_22:      return "CP/M 2.2";
        case UFT_CPM_VERSION_30:      return "CP/M 3.0 (Plus)";
        case UFT_CPM_VERSION_MSDOS:   return "MSX-DOS";
        case UFT_CPM_VERSION_CDOS:    return "Cromemco CDOS";
        case UFT_CPM_VERSION_ZDOS:    return "Z80DOS";
        case UFT_CPM_VERSION_ZCPR:    return "ZCPR3";
        default: return "Unknown";
    }
}

const char *uft_cpm_strerror(uft_cpm_err_t err) {
    switch (err) {
        case UFT_CPM_OK:              return "Success";
        case UFT_CPM_ERR_NULL:        return "Null pointer";
        case UFT_CPM_ERR_MEMORY:      return "Memory allocation failed";
        case UFT_CPM_ERR_IO:          return "I/O error";
        case UFT_CPM_ERR_FORMAT:      return "Invalid format";
        case UFT_CPM_ERR_NOT_CPM:     return "Not a CP/M filesystem";
        case UFT_CPM_ERR_NOT_FOUND:   return "File not found";
        case UFT_CPM_ERR_EXISTS:      return "File already exists";
        case UFT_CPM_ERR_DIR_FULL:    return "Directory full";
        case UFT_CPM_ERR_DISK_FULL:   return "Disk full";
        case UFT_CPM_ERR_READ_ONLY:   return "File is read-only";
        case UFT_CPM_ERR_INVALID_USER: return "Invalid user number";
        case UFT_CPM_ERR_INVALID_NAME: return "Invalid filename";
        case UFT_CPM_ERR_BAD_EXTENT:  return "Corrupt extent chain";
        case UFT_CPM_ERR_VERSION:     return "Unsupported CP/M version";
        default: return "Unknown error";
    }
}

/*===========================================================================
 * DPB Lookup
 *===========================================================================*/

bool uft_cpm_get_dpb(uft_cpm_format_t format, uft_cpm_dpb_t *dpb) {
    if (!dpb) return false;
    
    for (size_t i = 0; i < DPB_COUNT; i++) {
        if (g_dpb_database[i].format == format) {
            *dpb = g_dpb_database[i];
            return true;
        }
    }
    return false;
}

/**
 * @brief Image size to format mapping table
 */
static const struct {
    size_t size;
    uft_cpm_format_t format;
} g_size_to_format[] = {
    { 256256,  UFT_CPM_FMT_8_SSSD },      /* 77×26×128 */
    { 505856,  UFT_CPM_FMT_8_SSDD },      /* 77×26×256 */
    { 512512,  UFT_CPM_FMT_8_DSSD },      /* 77×26×128×2 */
    { 1013760, UFT_CPM_FMT_8_DSDD },      /* 77×26×256×2 */
    { 200704,  UFT_CPM_FMT_KAYPRO_II },   /* 40×10×512 - 1 track */
    { 204800,  UFT_CPM_FMT_KAYPRO_II },   /* 40×10×512 */
    { 400384,  UFT_CPM_FMT_KAYPRO_4 },    /* 40×10×512×2 - 1 track */
    { 409600,  UFT_CPM_FMT_KAYPRO_4 },    /* 40×10×512×2 */
    { 819200,  UFT_CPM_FMT_KAYPRO_10 },   /* 80×10×512×2 */
    { 102400,  UFT_CPM_FMT_OSBORNE_1 },   /* 40×10×256 */
    { 100352,  UFT_CPM_FMT_OSBORNE_1 },   /* 40×10×256 - 3 tracks */
    { 409600,  UFT_CPM_FMT_OSBORNE_DD },  /* 40×5×1024×2 */
    { 184320,  UFT_CPM_FMT_AMSTRAD_CPC_DATA }, /* 40×9×512 */
    { 194560,  UFT_CPM_FMT_AMSTRAD_CPC_SYS },  /* Including reserved */
    { 368640,  UFT_CPM_FMT_525_DSDD },    /* 40×9×512×2 */
    { 737280,  UFT_CPM_FMT_35_DSDD },     /* 80×9×512×2 */
    { 143360,  UFT_CPM_FMT_APPLE_CPM },   /* 35×16×256 */
    { 327680,  UFT_CPM_FMT_EPSON_QX10 },  /* 40×16×256×2 */
    { 348160,  UFT_CPM_FMT_C128 },        /* 40×17×256×2 */
    { 1261568, UFT_CPM_FMT_NEC_PC98 },    /* 77×8×1024×2 */
};

#define SIZE_MAP_COUNT (sizeof(g_size_to_format) / sizeof(g_size_to_format[0]))

uft_cpm_format_t uft_cpm_detect_format_by_size(size_t size) {
    for (size_t i = 0; i < SIZE_MAP_COUNT; i++) {
        if (g_size_to_format[i].size == size) {
            return g_size_to_format[i].format;
        }
    }
    return UFT_CPM_FMT_UNKNOWN;
}

/*===========================================================================
 * Lifecycle Functions
 *===========================================================================*/

uft_cpm_ctx_t *uft_cpm_create(void) {
    uft_cpm_ctx_t *ctx = calloc(1, sizeof(uft_cpm_ctx_t));
    return ctx;
}

void uft_cpm_destroy(uft_cpm_ctx_t *ctx) {
    if (!ctx) return;
    
    if (ctx->owns_data && ctx->data) {
        free(ctx->data);
    }
    if (ctx->block_map) {
        free(ctx->block_map);
    }
    if (ctx->dir_cache) {
        free(ctx->dir_cache);
    }
    free(ctx);
}

/*===========================================================================
 * Sector/Block Address Calculation
 *===========================================================================*/

/**
 * @brief Apply sector skew
 */
static uint8_t apply_skew(const uft_cpm_dpb_t *dpb, uint8_t logical_sector) {
    if (!dpb) return logical_sector;
    
    switch (dpb->skew_type) {
        case UFT_CPM_SKEW_NONE:
            return logical_sector;
            
        case UFT_CPM_SKEW_PHYSICAL:
            if (dpb->skew > 0 && dpb->sectors_per_track > 0) {
                return (logical_sector * dpb->skew) % dpb->sectors_per_track;
            }
            return logical_sector;
            
        case UFT_CPM_SKEW_CUSTOM:
            if (logical_sector < sizeof(dpb->skew_table)) {
                return dpb->skew_table[logical_sector];
            }
            return logical_sector;
            
        case UFT_CPM_SKEW_LOGICAL:
        default:
            return logical_sector;
    }
}

int32_t uft_cpm_sector_offset(const uft_cpm_ctx_t *ctx,
                               uint8_t track, uint8_t sector, uint8_t side) {
    if (!ctx) return -1;
    
    const uft_cpm_dpb_t *dpb = &ctx->dpb;
    
    /* Validate parameters */
    if (track >= dpb->tracks) return -1;
    if (side >= dpb->sides) return -1;
    if (sector >= dpb->sectors_per_track) return -1;
    
    /* Apply skew */
    uint8_t phys_sector = apply_skew(dpb, sector);
    
    /* Calculate offset based on side ordering */
    uint32_t offset = 0;
    uint32_t track_size = (uint32_t)dpb->sectors_per_track * dpb->sector_size;
    
    switch (dpb->side_order) {
        case UFT_CPM_SIDES_SEQ:
            /* All tracks on side 0, then all on side 1 */
            offset = ((uint32_t)side * dpb->tracks + track) * track_size;
            break;
            
        case UFT_CPM_SIDES_ALT:
        default:
            /* Alternating: track 0 side 0, track 0 side 1, track 1 side 0, ... */
            offset = ((uint32_t)track * dpb->sides + side) * track_size;
            break;
            
        case UFT_CPM_SIDES_OUTOUT:
        case UFT_CPM_SIDES_OUTIN:
            /* Special head ordering - treat as alternating for now */
            offset = ((uint32_t)track * dpb->sides + side) * track_size;
            break;
    }
    
    /* Add sector offset */
    offset += phys_sector * dpb->sector_size;
    
    /* Validate offset */
    if (offset + dpb->sector_size > ctx->size) return -1;
    
    return (int32_t)offset;
}

uft_cpm_err_t uft_cpm_read_sector(const uft_cpm_ctx_t *ctx,
                                   uint8_t track, uint8_t sector, uint8_t side,
                                   uint8_t *buffer) {
    if (!ctx || !buffer) return UFT_CPM_ERR_NULL;
    if (!ctx->data) return UFT_CPM_ERR_FORMAT;
    
    int32_t offset = uft_cpm_sector_offset(ctx, track, sector, side);
    if (offset < 0) return UFT_CPM_ERR_FORMAT;
    
    memcpy(buffer, ctx->data + offset, ctx->dpb.sector_size);
    return UFT_CPM_OK;
}

uft_cpm_err_t uft_cpm_write_sector(uft_cpm_ctx_t *ctx,
                                    uint8_t track, uint8_t sector, uint8_t side,
                                    const uint8_t *buffer) {
    if (!ctx || !buffer) return UFT_CPM_ERR_NULL;
    if (!ctx->data) return UFT_CPM_ERR_FORMAT;
    
    int32_t offset = uft_cpm_sector_offset(ctx, track, sector, side);
    if (offset < 0) return UFT_CPM_ERR_FORMAT;
    
    memcpy(ctx->data + offset, buffer, ctx->dpb.sector_size);
    ctx->modified = true;
    return UFT_CPM_OK;
}

/**
 * @brief Convert block number to linear sector number
 */
static uint32_t block_to_linear_sector(const uft_cpm_dpb_t *dpb, uint16_t block) {
    /* Calculate sectors per block (using 128-byte logical sectors) */
    uint32_t sectors_per_block = dpb->block_size / 128;
    
    /* Reserved sectors at start */
    uint32_t reserved_sectors = (uint32_t)dpb->off * dpb->spt;
    
    /* Linear sector number */
    return reserved_sectors + (uint32_t)block * sectors_per_block;
}

uft_cpm_err_t uft_cpm_block_to_sectors(const uft_cpm_ctx_t *ctx,
                                        uint16_t block,
                                        uint8_t *track_out, uint8_t *sector_out,
                                        uint8_t *side_out, uint8_t *count_out) {
    if (!ctx) return UFT_CPM_ERR_NULL;
    
    const uft_cpm_dpb_t *dpb = &ctx->dpb;
    
    /* Validate block number */
    if (block > dpb->dsm) return UFT_CPM_ERR_FORMAT;
    
    /* Get linear sector number */
    uint32_t linear = block_to_linear_sector(dpb, block);
    
    /* Convert to physical coordinates */
    /* Sectors per track in 128-byte units */
    uint32_t spt_128 = dpb->spt;
    
    /* Physical sector size multiplier */
    uint32_t phys_per_128 = dpb->sector_size / 128;
    
    /* How many physical sectors in a block */
    uint32_t phys_per_block = dpb->block_size / dpb->sector_size;
    
    if (count_out) *count_out = phys_per_block;
    
    /* Calculate track and sector */
    uint32_t track_linear = linear / spt_128;
    uint32_t sector_in_track = (linear % spt_128) / phys_per_128;
    
    /* Handle double-sided */
    if (dpb->sides == 2) {
        if (dpb->side_order == UFT_CPM_SIDES_SEQ) {
            if (track_linear >= dpb->tracks) {
                if (side_out) *side_out = 1;
                if (track_out) *track_out = track_linear - dpb->tracks;
            } else {
                if (side_out) *side_out = 0;
                if (track_out) *track_out = track_linear;
            }
        } else {
            /* Alternating */
            if (side_out) *side_out = track_linear % 2;
            if (track_out) *track_out = track_linear / 2;
        }
    } else {
        if (side_out) *side_out = 0;
        if (track_out) *track_out = track_linear;
    }
    
    if (sector_out) *sector_out = sector_in_track + dpb->first_sector;
    
    return UFT_CPM_OK;
}

uft_cpm_err_t uft_cpm_read_block(const uft_cpm_ctx_t *ctx,
                                  uint16_t block, uint8_t *buffer) {
    if (!ctx || !buffer) return UFT_CPM_ERR_NULL;
    if (!ctx->data) return UFT_CPM_ERR_FORMAT;
    
    const uft_cpm_dpb_t *dpb = &ctx->dpb;
    
    /* Validate block */
    if (block > dpb->dsm) return UFT_CPM_ERR_FORMAT;
    
    /* Get physical coordinates */
    uint8_t track, sector, side, count;
    uft_cpm_err_t err = uft_cpm_block_to_sectors(ctx, block, 
                                                  &track, &sector, &side, &count);
    if (err != UFT_CPM_OK) return err;
    
    /* Read all sectors in the block */
    uint8_t *ptr = buffer;
    for (uint8_t i = 0; i < count; i++) {
        err = uft_cpm_read_sector(ctx, track, sector + i, side, ptr);
        if (err != UFT_CPM_OK) return err;
        ptr += dpb->sector_size;
        
        /* Handle sector wrap */
        if (sector + i + 1 >= dpb->sectors_per_track) {
            sector = dpb->first_sector;
            if (dpb->sides == 2 && dpb->side_order == UFT_CPM_SIDES_ALT) {
                if (side == 0) {
                    side = 1;
                } else {
                    side = 0;
                    track++;
                }
            } else {
                track++;
            }
        }
    }
    
    return UFT_CPM_OK;
}

uft_cpm_err_t uft_cpm_write_block(uft_cpm_ctx_t *ctx,
                                   uint16_t block, const uint8_t *buffer) {
    if (!ctx || !buffer) return UFT_CPM_ERR_NULL;
    if (!ctx->data) return UFT_CPM_ERR_FORMAT;
    
    const uft_cpm_dpb_t *dpb = &ctx->dpb;
    
    /* Validate block */
    if (block > dpb->dsm) return UFT_CPM_ERR_FORMAT;
    
    /* Get physical coordinates */
    uint8_t track, sector, side, count;
    uft_cpm_err_t err = uft_cpm_block_to_sectors(ctx, block,
                                                  &track, &sector, &side, &count);
    if (err != UFT_CPM_OK) return err;
    
    /* Write all sectors in the block */
    const uint8_t *ptr = buffer;
    for (uint8_t i = 0; i < count; i++) {
        err = uft_cpm_write_sector(ctx, track, sector + i, side, ptr);
        if (err != UFT_CPM_OK) return err;
        ptr += dpb->sector_size;
        
        /* Handle sector wrap */
        if (sector + i + 1 >= dpb->sectors_per_track) {
            sector = dpb->first_sector;
            if (dpb->sides == 2 && dpb->side_order == UFT_CPM_SIDES_ALT) {
                if (side == 0) {
                    side = 1;
                } else {
                    side = 0;
                    track++;
                }
            } else {
                track++;
            }
        }
    }
    
    return UFT_CPM_OK;
}

/*===========================================================================
 * Filename Handling
 *===========================================================================*/

bool uft_cpm_parse_filename(const char *input, char *name, char *ext) {
    if (!input || !name || !ext) return false;
    
    /* Initialize with spaces */
    memset(name, ' ', UFT_CPM_MAX_NAME);
    name[UFT_CPM_MAX_NAME] = '\0';
    memset(ext, ' ', UFT_CPM_MAX_EXT);
    ext[UFT_CPM_MAX_EXT] = '\0';
    
    /* Find dot separator */
    const char *dot = strchr(input, '.');
    
    /* Copy name part */
    size_t name_len = dot ? (size_t)(dot - input) : strlen(input);
    if (name_len > UFT_CPM_MAX_NAME) name_len = UFT_CPM_MAX_NAME;
    
    for (size_t i = 0; i < name_len; i++) {
        char c = toupper((unsigned char)input[i]);
        /* Validate character */
        if (c >= 'A' && c <= 'Z') {
            name[i] = c;
        } else if (c >= '0' && c <= '9') {
            name[i] = c;
        } else if (c == '_' || c == '-' || c == '$' || c == '#') {
            name[i] = c;
        } else if (c == ' ') {
            /* Stop at space */
            break;
        } else {
            return false; /* Invalid character */
        }
    }
    
    /* Copy extension if present */
    if (dot && dot[1]) {
        const char *ext_src = dot + 1;
        size_t ext_len = strlen(ext_src);
        if (ext_len > UFT_CPM_MAX_EXT) ext_len = UFT_CPM_MAX_EXT;
        
        for (size_t i = 0; i < ext_len; i++) {
            char c = toupper((unsigned char)ext_src[i]);
            if (c >= 'A' && c <= 'Z') {
                ext[i] = c;
            } else if (c >= '0' && c <= '9') {
                ext[i] = c;
            } else if (c == '_' || c == '-' || c == '$' || c == '#') {
                ext[i] = c;
            } else if (c == ' ') {
                break;
            } else {
                return false;
            }
        }
    }
    
    return true;
}

void uft_cpm_format_filename(const uft_cpm_dir_entry_t *entry, char *buffer) {
    if (!entry || !buffer) return;
    
    /* Copy name (strip high bits for attributes) */
    int name_len = 0;
    for (int i = 0; i < 8; i++) {
        char c = entry->name[i] & 0x7F;
        if (c != ' ') {
            buffer[name_len++] = c;
        }
    }
    
    /* Add dot if extension present */
    bool has_ext = false;
    for (int i = 0; i < 3; i++) {
        if ((entry->ext[i] & 0x7F) != ' ') {
            has_ext = true;
            break;
        }
    }
    
    if (has_ext) {
        buffer[name_len++] = '.';
        for (int i = 0; i < 3; i++) {
            char c = entry->ext[i] & 0x7F;
            if (c != ' ') {
                buffer[name_len++] = c;
            }
        }
    }
    
    buffer[name_len] = '\0';
}

bool uft_cpm_valid_filename(const char *name) {
    if (!name || !*name) return false;
    
    char parsed_name[UFT_CPM_MAX_NAME + 1];
    char parsed_ext[UFT_CPM_MAX_EXT + 1];
    
    return uft_cpm_parse_filename(name, parsed_name, parsed_ext);
}

/*===========================================================================
 * Time Conversion
 *===========================================================================*/

/* CP/M epoch: January 1, 1978 */
#define CPM_EPOCH_UNIX 252460800L

time_t uft_cpm_to_unix_time(uint16_t cpm_date, uint8_t hour, uint8_t minute) {
    if (cpm_date == 0) return 0;
    
    /* Convert BCD to binary */
    int h = ((hour >> 4) & 0x0F) * 10 + (hour & 0x0F);
    int m = ((minute >> 4) & 0x0F) * 10 + (minute & 0x0F);
    
    /* Clamp values */
    if (h > 23) h = 0;
    if (m > 59) m = 0;
    
    /* Calculate Unix time */
    time_t result = CPM_EPOCH_UNIX;
    result += (time_t)cpm_date * 86400;  /* Days to seconds */
    result += h * 3600;
    result += m * 60;
    
    return result;
}

void uft_cpm_from_unix_time(time_t unix_time, uint16_t *cpm_date,
                             uint8_t *hour, uint8_t *minute) {
    if (!cpm_date || !hour || !minute) return;
    
    if (unix_time < CPM_EPOCH_UNIX) {
        *cpm_date = 0;
        *hour = 0;
        *minute = 0;
        return;
    }
    
    time_t diff = unix_time - CPM_EPOCH_UNIX;
    
    /* Calculate days */
    uint32_t days = diff / 86400;
    if (days > 65535) days = 65535;
    *cpm_date = (uint16_t)days;
    
    /* Calculate time */
    uint32_t remaining = diff % 86400;
    int h = remaining / 3600;
    int m = (remaining % 3600) / 60;
    
    /* Convert to BCD */
    *hour = (uint8_t)(((h / 10) << 4) | (h % 10));
    *minute = (uint8_t)(((m / 10) << 4) | (m % 10));
}
