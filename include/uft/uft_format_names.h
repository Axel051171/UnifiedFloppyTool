// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file uft_format_names.h
 * @brief Format Names Registry - Historische Namen und Einsatzgebiete
 * @version 1.0.0
 * @date 2025-01-02
 *
 * Diese Registry enthält alle bekannten Format-Varianten mit:
 * - Historischen/Marketing-Namen
 * - Technischen Spezifikationen
 * - Einsatzgebiete/Plattformen
 * - Zeitraum der Nutzung
 *
 * Für GUI-Auswahl und Auto-Detection.
 */

#ifndef UFT_FORMAT_NAMES_H
#define UFT_FORMAT_NAMES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * FORMAT FAMILIES
 *============================================================================*/

typedef enum {
    FORMAT_FAMILY_COMMODORE = 0,
    FORMAT_FAMILY_AMIGA,
    FORMAT_FAMILY_APPLE,
    FORMAT_FAMILY_ATARI,
    FORMAT_FAMILY_PC_IBM,
    FORMAT_FAMILY_TRS80,
    FORMAT_FAMILY_BBC_ACORN,
    FORMAT_FAMILY_JAPANESE,
    FORMAT_FAMILY_FLUX,
    FORMAT_FAMILY_OTHER
} format_family_t;

/*============================================================================
 * FORMAT ENTRY STRUCTURE
 *============================================================================*/

typedef struct {
    /* Identifiers */
    const char* id;                 /* Internal ID: "D64_STANDARD" */
    const char* extension;          /* File extension: ".d64" */
    uint32_t    magic_offset;       /* Offset for magic bytes */
    const char* magic_bytes;        /* Magic signature or NULL */
    uint32_t    magic_len;          /* Length of magic signature */
    
    /* Names */
    const char* technical_name;     /* "D64 Standard" */
    const char* marketing_name;     /* "1541 Disk Image" */
    const char* common_name;        /* "C64 Floppy" */
    
    /* Platform info */
    format_family_t family;
    const char* platforms;          /* "C64, VIC-20, C128" */
    const char* drives;             /* "1541, 1541-II, 1541C" */
    const char* era;                /* "1982-1994" */
    
    /* Technical specs */
    uint8_t  tracks;
    uint8_t  sides;
    uint8_t  encoding;              /* FM/MFM/GCR */
    uint16_t rpm;
    uint32_t total_size;            /* Typical file size */
    uint16_t sectors_per_track;     /* 0 = variable */
    uint16_t bytes_per_sector;
    
    /* Capabilities */
    bool has_error_info;
    bool supports_copy_protection;
    bool preserves_timing;
    bool preserves_weak_bits;
    bool lossless;
    
    /* GUI hints */
    const char* icon;               /* Icon name for GUI */
    const char* description;        /* Human-readable description */
    const char* wiki_url;           /* Documentation link */
    
} format_entry_t;

/*============================================================================
 * COMMODORE FORMATS
 *============================================================================*/

static const format_entry_t COMMODORE_FORMATS[] = {
    {
        .id = "D64_STANDARD",
        .extension = ".d64",
        .technical_name = "D64 Standard",
        .marketing_name = "1541 Disk Image",
        .common_name = "C64 Floppy",
        .family = FORMAT_FAMILY_COMMODORE,
        .platforms = "C64, VIC-20, C128",
        .drives = "1541, 1541-II",
        .era = "1982-1994",
        .tracks = 35,
        .sides = 1,
        .encoding = 2,  /* GCR */
        .rpm = 300,
        .total_size = 174848,
        .sectors_per_track = 0,  /* Variable: 21-17 */
        .bytes_per_sector = 256,
        .has_error_info = false,
        .supports_copy_protection = false,
        .preserves_timing = false,
        .preserves_weak_bits = false,
        .lossless = false,
        .icon = "commodore",
        .description = "Standard C64/1541 disk image, 35 tracks, 683 blocks",
        .wiki_url = "https://vice-emu.sourceforge.io/vice_17.html"
    },
    {
        .id = "D64_ERROR",
        .extension = ".d64",
        .technical_name = "D64 with Error Info",
        .marketing_name = "1541 Extended",
        .common_name = "D64 + Errors",
        .family = FORMAT_FAMILY_COMMODORE,
        .platforms = "C64, VIC-20, C128",
        .drives = "1541, 1541-II",
        .era = "1982-1994",
        .tracks = 35,
        .sides = 1,
        .encoding = 2,
        .rpm = 300,
        .total_size = 175531,  /* 174848 + 683 */
        .has_error_info = true,
        .supports_copy_protection = true,
        .description = "D64 with per-sector error information bytes"
    },
    {
        .id = "D64_40TRACK",
        .extension = ".d64",
        .technical_name = "D64 40-Track",
        .marketing_name = "1541 Extended Track",
        .common_name = "D64 40 Tracks",
        .family = FORMAT_FAMILY_COMMODORE,
        .tracks = 40,
        .sides = 1,
        .total_size = 196608,
        .description = "Extended D64 using tracks 36-40 for extra storage"
    },
    {
        .id = "D64_SPEEDDOS",
        .extension = ".d64",
        .technical_name = "SpeedDOS D64",
        .marketing_name = "SpeedDOS",
        .common_name = "SpeedDOS Image",
        .family = FORMAT_FAMILY_COMMODORE,
        .tracks = 40,
        .total_size = 197376,  /* With SpeedDOS BAM */
        .description = "SpeedDOS compatible image with parallel BAM"
    },
    {
        .id = "D71_STANDARD",
        .extension = ".d71",
        .technical_name = "D71 Standard",
        .marketing_name = "1571 Disk Image",
        .common_name = "C128 Floppy",
        .family = FORMAT_FAMILY_COMMODORE,
        .platforms = "C128",
        .drives = "1571",
        .era = "1985-1994",
        .tracks = 70,
        .sides = 2,
        .total_size = 349696,
        .description = "Double-sided C128/1571 disk image"
    },
    {
        .id = "D81_STANDARD",
        .extension = ".d81",
        .technical_name = "D81 Standard",
        .marketing_name = "1581 Disk Image",
        .common_name = "3.5\" C64/C128",
        .family = FORMAT_FAMILY_COMMODORE,
        .platforms = "C64, C128",
        .drives = "1581",
        .era = "1987-1994",
        .tracks = 80,
        .sides = 2,
        .encoding = 1,  /* MFM */
        .total_size = 819200,
        .sectors_per_track = 10,
        .bytes_per_sector = 512,
        .description = "3.5\" DD MFM format, 80 tracks, 3160 blocks"
    },
    {
        .id = "G64_STANDARD",
        .extension = ".g64",
        .technical_name = "G64 GCR Image",
        .marketing_name = "GCR Native",
        .common_name = "Preservation Format",
        .family = FORMAT_FAMILY_COMMODORE,
        .tracks = 42,
        .sides = 1,
        .encoding = 2,
        .preserves_timing = true,
        .supports_copy_protection = true,
        .lossless = true,
        .description = "Bit-level GCR preservation format"
    },
    {
        .id = "NIB_C64",
        .extension = ".nib",
        .technical_name = "NIB Raw Nibble",
        .marketing_name = "Nibbler Image",
        .common_name = "C64 Nibble",
        .family = FORMAT_FAMILY_COMMODORE,
        .preserves_timing = true,
        .preserves_weak_bits = true,
        .lossless = true,
        .description = "Raw nibble data for copy protection preservation"
    },
    { .id = NULL }  /* Sentinel */
};

/*============================================================================
 * AMIGA FORMATS
 *============================================================================*/

static const format_entry_t AMIGA_FORMATS[] = {
    {
        .id = "ADF_OFS",
        .extension = ".adf",
        .magic_bytes = "DOS\x00",
        .magic_len = 4,
        .technical_name = "ADF OFS",
        .marketing_name = "Original File System",
        .common_name = "Amiga Kickstart 1.x",
        .family = FORMAT_FAMILY_AMIGA,
        .platforms = "Amiga 500, 1000, 2000",
        .drives = "DF0:, DF1:",
        .era = "1985-1990",
        .tracks = 80,
        .sides = 2,
        .encoding = 1,  /* MFM */
        .rpm = 300,
        .total_size = 901120,
        .sectors_per_track = 11,
        .bytes_per_sector = 512,
        .description = "Original Amiga filesystem, 512 bytes/block"
    },
    {
        .id = "ADF_FFS",
        .extension = ".adf",
        .magic_bytes = "DOS\x01",
        .magic_len = 4,
        .technical_name = "ADF FFS",
        .marketing_name = "Fast File System",
        .common_name = "Amiga 2.0+",
        .family = FORMAT_FAMILY_AMIGA,
        .platforms = "Amiga 500+, 600, 1200, 2000, 3000, 4000",
        .era = "1990-1996",
        .tracks = 80,
        .sides = 2,
        .total_size = 901120,
        .description = "Fast File System with improved caching"
    },
    {
        .id = "ADF_FFS_INTL",
        .extension = ".adf",
        .magic_bytes = "DOS\x03",
        .magic_len = 4,
        .technical_name = "ADF FFS International",
        .marketing_name = "FFS International Mode",
        .common_name = "Amiga International",
        .family = FORMAT_FAMILY_AMIGA,
        .era = "1991-1996",
        .description = "FFS with international character support"
    },
    {
        .id = "ADF_DCFS",
        .extension = ".adf",
        .magic_bytes = "DOS\x05",
        .magic_len = 4,
        .technical_name = "ADF Dir Cache FFS",
        .marketing_name = "FFS Directory Cache",
        .common_name = "Amiga 3.0+",
        .family = FORMAT_FAMILY_AMIGA,
        .platforms = "Amiga 1200, 4000",
        .era = "1992-1996",
        .description = "FFS with directory caching for faster access"
    },
    {
        .id = "ADF_HD",
        .extension = ".adf",
        .technical_name = "ADF High Density",
        .marketing_name = "HD Floppy",
        .common_name = "Amiga HD",
        .family = FORMAT_FAMILY_AMIGA,
        .platforms = "Amiga 4000",
        .drives = "HD DF0:",
        .tracks = 80,
        .sides = 2,
        .total_size = 1802240,
        .sectors_per_track = 22,
        .description = "1.76MB High Density Amiga format"
    },
    { .id = NULL }
};

/*============================================================================
 * APPLE FORMATS
 *============================================================================*/

static const format_entry_t APPLE_FORMATS[] = {
    {
        .id = "NIB_APPLE",
        .extension = ".nib",
        .technical_name = "Apple II NIB",
        .marketing_name = "Nibble Image",
        .common_name = "Apple II Raw",
        .family = FORMAT_FAMILY_APPLE,
        .platforms = "Apple II, IIe, IIc, IIgs",
        .drives = "Disk II",
        .era = "1977-1993",
        .tracks = 35,
        .sides = 1,
        .encoding = 2,  /* GCR */
        .rpm = 300,
        .total_size = 232960,
        .preserves_timing = true,
        .description = "Raw nibble data, 6656 bytes per track"
    },
    {
        .id = "DO_DOS33",
        .extension = ".do",
        .technical_name = "DOS 3.3 Order",
        .marketing_name = "Apple DOS 3.3",
        .common_name = "DOS Order",
        .family = FORMAT_FAMILY_APPLE,
        .era = "1980-1993",
        .tracks = 35,
        .sides = 1,
        .total_size = 143360,
        .sectors_per_track = 16,
        .bytes_per_sector = 256,
        .description = "Standard DOS 3.3 sector order"
    },
    {
        .id = "PO_PRODOS",
        .extension = ".po",
        .technical_name = "ProDOS Order",
        .marketing_name = "Apple ProDOS",
        .common_name = "ProDOS Order",
        .family = FORMAT_FAMILY_APPLE,
        .era = "1983-1993",
        .total_size = 143360,
        .description = "ProDOS sector interleave order"
    },
    {
        .id = "WOZ_V1",
        .extension = ".woz",
        .magic_bytes = "WOZ1",
        .magic_len = 4,
        .technical_name = "WOZ 1.0",
        .marketing_name = "Applesauce v1",
        .common_name = "WOZ Image",
        .family = FORMAT_FAMILY_APPLE,
        .era = "2018-",
        .preserves_timing = false,
        .preserves_weak_bits = false,
        .lossless = true,
        .description = "Bit-level preservation, no timing info"
    },
    {
        .id = "WOZ_V2",
        .extension = ".woz",
        .magic_bytes = "WOZ2",
        .magic_len = 4,
        .technical_name = "WOZ 2.0",
        .marketing_name = "Applesauce v2",
        .common_name = "WOZ 2 Image",
        .family = FORMAT_FAMILY_APPLE,
        .era = "2019-",
        .preserves_timing = true,
        .lossless = true,
        .description = "Bit-level with optimal bit timing"
    },
    {
        .id = "WOZ_V21",
        .extension = ".woz",
        .magic_bytes = "WOZ2",
        .magic_len = 4,
        .technical_name = "WOZ 2.1",
        .marketing_name = "Applesauce v2.1",
        .common_name = "WOZ 2.1 Flux",
        .family = FORMAT_FAMILY_APPLE,
        .era = "2020-",
        .preserves_timing = true,
        .preserves_weak_bits = true,
        .lossless = true,
        .description = "WOZ 2 with flux-level support"
    },
    {
        .id = "2MG_STANDARD",
        .extension = ".2mg",
        .magic_bytes = "2IMG",
        .magic_len = 4,
        .technical_name = "2IMG Universal",
        .marketing_name = "Universal Disk Image",
        .common_name = "2MG Image",
        .family = FORMAT_FAMILY_APPLE,
        .description = "Headered container for DO/PO images"
    },
    {
        .id = "A2R_FLUX",
        .extension = ".a2r",
        .magic_bytes = "A2R2",
        .magic_len = 4,
        .technical_name = "Applesauce Raw Flux",
        .marketing_name = "A2R Flux Capture",
        .common_name = "Apple Flux",
        .family = FORMAT_FAMILY_APPLE,
        .preserves_timing = true,
        .preserves_weak_bits = true,
        .lossless = true,
        .description = "Flux-level capture from Applesauce"
    },
    { .id = NULL }
};

/*============================================================================
 * FLUX FORMATS
 *============================================================================*/

static const format_entry_t FLUX_FORMATS[] = {
    {
        .id = "SCP_V1",
        .extension = ".scp",
        .magic_bytes = "SCP",
        .magic_len = 3,
        .technical_name = "SuperCard Pro v1",
        .marketing_name = "SCP Image",
        .common_name = "SCP Flux",
        .family = FORMAT_FAMILY_FLUX,
        .era = "2013-",
        .preserves_timing = true,
        .preserves_weak_bits = true,
        .lossless = true,
        .description = "Jim Drew's flux format, multi-revolution"
    },
    {
        .id = "SCP_V2",
        .extension = ".scp",
        .technical_name = "SuperCard Pro v2",
        .marketing_name = "SCP v2 Extended",
        .common_name = "SCP v2",
        .family = FORMAT_FAMILY_FLUX,
        .era = "2015-",
        .preserves_timing = true,
        .preserves_weak_bits = true,
        .lossless = true,
        .description = "Extended SCP with additional metadata"
    },
    {
        .id = "HFE_V1",
        .extension = ".hfe",
        .magic_bytes = "HXCPICFE",
        .magic_len = 8,
        .technical_name = "UFT HFE Format v1",
        .marketing_name = "HFE Standard",
        .common_name = "HFE Image",
        .family = FORMAT_FAMILY_FLUX,
        .era = "2006-",
        .description = "UFT Project's emulator format"
    },
    {
        .id = "HFE_V2",
        .extension = ".hfe",
        .technical_name = "HxC v2",
        .marketing_name = "HFE Variable Bitrate",
        .common_name = "HFE v2",
        .family = FORMAT_FAMILY_FLUX,
        .era = "2010-",
        .description = "HFE with variable bitrate support"
    },
    {
        .id = "HFE_V3",
        .extension = ".hfe",
        .magic_bytes = "HXCHFEV3",
        .magic_len = 8,
        .technical_name = "HxC v3",
        .marketing_name = "HFE v3 Opcodes",
        .common_name = "HFE v3",
        .family = FORMAT_FAMILY_FLUX,
        .era = "2015-",
        .preserves_timing = true,
        .preserves_weak_bits = true,
        .description = "HFE v3 with opcodes, weak bits, splice markers"
    },
    {
        .id = "IPF_STANDARD",
        .extension = ".ipf",
        .magic_bytes = "CAPS",
        .magic_len = 4,
        .technical_name = "Interchangeable Preservation Format",
        .marketing_name = "SPS IPF",
        .common_name = "IPF Master",
        .family = FORMAT_FAMILY_FLUX,
        .era = "2001-",
        .preserves_timing = true,
        .preserves_weak_bits = true,
        .supports_copy_protection = true,
        .lossless = true,
        .description = "Software Preservation Society master format"
    },
    {
        .id = "UFT_KF_RAW",
        .extension = ".raw",
        .technical_name = "KryoFlux Raw",
        .marketing_name = "KryoFlux Stream",
        .common_name = "KF Raw",
        .family = FORMAT_FAMILY_FLUX,
        .era = "2009-",
        .preserves_timing = true,
        .preserves_weak_bits = true,
        .lossless = true,
        .description = "Software Preservation Society stream format"
    },
    {
        .id = "MFM_GW",
        .extension = ".raw",
        .technical_name = "GreaseWeazle Raw",
        .marketing_name = "GW Flux",
        .common_name = "GW Raw",
        .family = FORMAT_FAMILY_FLUX,
        .era = "2019-",
        .preserves_timing = true,
        .preserves_weak_bits = true,
        .lossless = true,
    },
    {
        .id = "UFF_V1",
        .extension = ".uff",
        .magic_bytes = "UFF\x00",
        .magic_len = 4,
        .technical_name = "UFT Flux Format",
        .marketing_name = "UFT Universal Flux",
        .common_name = "UFF Master",
        .family = FORMAT_FAMILY_FLUX,
        .era = "2025-",
        .preserves_timing = true,
        .preserves_weak_bits = true,
        .supports_copy_protection = true,
        .lossless = true,
        .description = "Kein Bit geht verloren - UFT master format"
    },
    { .id = NULL }
};

/*============================================================================
 * API FUNCTIONS
 *============================================================================*/

/**
 * @brief Get format entry by ID
 */
const format_entry_t* format_get_by_id(const char* id);

/**
 * @brief Get format entry by extension
 */
const format_entry_t* format_get_by_extension(const char* ext);

/**
 * @brief Get all formats for a family
 */
const format_entry_t* format_get_family(format_family_t family);

/**
 * @brief Get format display name for GUI
 */
const char* format_get_display_name(const format_entry_t* entry);

/**
 * @brief Auto-detect format from file
 */
const format_entry_t* format_detect_from_file(const char* path);

/**
 * @brief Get format capabilities string
 */
const char* format_get_capabilities(const format_entry_t* entry);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMAT_NAMES_H */
