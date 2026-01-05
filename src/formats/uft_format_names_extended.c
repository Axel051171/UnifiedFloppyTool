// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file uft_format_names_extended.c
 * @brief Extended Format Names Registry with Historical Context
 * @version 1.0.0
 * @date 2025-01-02
 *
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║          EXTENDED FORMAT NAMES REGISTRY                                      ║
 * ║     Historische Namen, Plattformen & Einsatzgebiete                         ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * Diese Registry enthält alle bekannten Format-Varianten mit:
 * - Historischen/Marketing-Namen
 * - Technischen Spezifikationen  
 * - Einsatzgebiete/Plattformen
 * - Zeitraum der Nutzung
 * - GUI-Anzeigenamen
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * FORMAT ENTRY STRUCTURE (Extended)
 *============================================================================*/

typedef struct {
    /* Identifiers */
    const char* id;                 /* Internal ID */
    const char* extension;          /* File extension(s) */
    const char* magic;              /* Magic bytes (hex string) */
    uint32_t    magic_offset;       /* Offset for magic */
    uint32_t    typical_size;       /* Typical file size in bytes */
    
    /* Names for GUI Display */
    const char* gui_name;           /* Short name for dropdown: "D64 (C64)" */
    const char* full_name;          /* Full name: "Commodore 64 1541 Disk Image" */
    const char* marketing_name;     /* Historical marketing: "1541 Disk Image" */
    
    /* Platform & History */
    const char* platform;           /* Primary platform */
    const char* secondary_platforms;/* Additional platforms */
    const char* drives;             /* Compatible drives */
    const char* era;                /* Years of use */
    const char* created_by;         /* Creator/Company */
    
    /* Technical */
    uint8_t     tracks;
    uint8_t     sides;
    uint8_t     sectors_per_track;  /* 0 = variable */
    uint16_t    bytes_per_sector;
    uint16_t    rpm;
    const char* encoding;           /* FM/MFM/GCR/etc */
    
    /* Capabilities */
    bool        preserves_timing;
    bool        preserves_weak_bits;
    bool        has_error_info;
    bool        copy_protection;
    bool        lossless;
    bool        read_only;          /* Format is read-only in UFT */
    
    /* GUI Category */
    const char* category;           /* For grouping in GUI */
    const char* icon;               /* Icon name */
    uint8_t     priority;           /* Sort priority (lower = first) */
    
} format_entry_ext_t;

/*============================================================================
 * COMMODORE FORMATS
 *============================================================================*/

static const format_entry_ext_t COMMODORE_FORMATS_EXT[] = {
    /* D64 Variants */
    {
        .id = "D64_STANDARD",
        .extension = ".d64",
        .magic = NULL,
        .typical_size = 174848,
        .gui_name = "D64 (C64 Standard)",
        .full_name = "Commodore 64 1541 Disk Image",
        .marketing_name = "1541 Disk Image",
        .platform = "Commodore 64",
        .secondary_platforms = "VIC-20, C128, Plus/4",
        .drives = "1541, 1541-II, 1541C, 1541 Ultimate",
        .era = "1982-1994",
        .created_by = "Commodore",
        .tracks = 35,
        .sides = 1,
        .sectors_per_track = 0,  /* Variable: 21-17 */
        .bytes_per_sector = 256,
        .rpm = 300,
        .encoding = "GCR",
        .preserves_timing = false,
        .copy_protection = false,
        .category = "Commodore",
        .icon = "commodore",
        .priority = 1
    },
    {
        .id = "D64_ERROR",
        .extension = ".d64",
        .typical_size = 175531,  /* 174848 + 683 error bytes */
        .gui_name = "D64 (C64 + Errors)",
        .full_name = "D64 with Error Information",
        .marketing_name = "1541 Extended",
        .platform = "Commodore 64",
        .secondary_platforms = "VIC-20, C128",
        .drives = "1541",
        .era = "1982-1994",
        .tracks = 35,
        .sides = 1,
        .encoding = "GCR",
        .has_error_info = true,
        .copy_protection = true,
        .category = "Commodore",
        .priority = 2
    },
    {
        .id = "D64_40TRACK",
        .extension = ".d64",
        .typical_size = 196608,
        .gui_name = "D64 (40 Tracks)",
        .full_name = "D64 Extended 40-Track",
        .marketing_name = "1541 Extended Track",
        .platform = "Commodore 64",
        .drives = "1541 (modified)",
        .tracks = 40,
        .sides = 1,
        .encoding = "GCR",
        .category = "Commodore",
        .priority = 3
    },
    {
        .id = "D64_SPEEDDOS",
        .extension = ".d64",
        .typical_size = 197376,
        .gui_name = "D64 (SpeedDOS)",
        .full_name = "SpeedDOS Compatible Image",
        .marketing_name = "SpeedDOS",
        .platform = "Commodore 64",
        .drives = "1541 + SpeedDOS ROM",
        .tracks = 40,
        .encoding = "GCR",
        .category = "Commodore",
        .priority = 4
    },
    {
        .id = "D64_DOLPHINDOS",
        .extension = ".d64",
        .gui_name = "D64 (DolphinDOS)",
        .full_name = "DolphinDOS Compatible Image",
        .marketing_name = "DolphinDOS",
        .platform = "Commodore 64",
        .drives = "1541 + DolphinDOS",
        .tracks = 40,
        .encoding = "GCR",
        .category = "Commodore",
        .priority = 5
    },
    /* D71 */
    {
        .id = "D71_STANDARD",
        .extension = ".d71",
        .typical_size = 349696,
        .gui_name = "D71 (C128)",
        .full_name = "Commodore 128 1571 Disk Image",
        .marketing_name = "1571 Disk Image",
        .platform = "Commodore 128",
        .secondary_platforms = "C64 (single-sided)",
        .drives = "1571",
        .era = "1985-1994",
        .created_by = "Commodore",
        .tracks = 70,
        .sides = 2,
        .encoding = "GCR",
        .category = "Commodore",
        .priority = 10
    },
    /* D81 */
    {
        .id = "D81_STANDARD",
        .extension = ".d81",
        .typical_size = 819200,
        .gui_name = "D81 (3.5\")",
        .full_name = "Commodore 1581 3.5\" Disk Image",
        .marketing_name = "1581 Disk Image",
        .platform = "Commodore 64/128",
        .drives = "1581",
        .era = "1987-1994",
        .created_by = "Commodore",
        .tracks = 80,
        .sides = 2,
        .sectors_per_track = 10,
        .bytes_per_sector = 512,
        .encoding = "MFM",
        .category = "Commodore",
        .priority = 15
    },
    /* D80/D82 */
    {
        .id = "D80_STANDARD",
        .extension = ".d80",
        .typical_size = 533248,
        .gui_name = "D80 (8050)",
        .full_name = "Commodore 8050 Disk Image",
        .marketing_name = "8050 Disk Image",
        .platform = "Commodore PET/CBM",
        .drives = "8050",
        .era = "1980-1990",
        .tracks = 77,
        .sides = 1,
        .encoding = "GCR",
        .category = "Commodore",
        .priority = 20
    },
    {
        .id = "D82_STANDARD",
        .extension = ".d82",
        .typical_size = 1066496,
        .gui_name = "D82 (8250)",
        .full_name = "Commodore 8250 Disk Image",
        .marketing_name = "8250 Disk Image",
        .platform = "Commodore PET/CBM",
        .drives = "8250, SFD-1001",
        .era = "1981-1990",
        .tracks = 77,
        .sides = 2,
        .encoding = "GCR",
        .category = "Commodore",
        .priority = 21
    },
    /* G64 - GCR Raw */
    {
        .id = "G64_STANDARD",
        .extension = ".g64",
        .magic = "GCR-1541",
        .gui_name = "G64 (GCR Raw)",
        .full_name = "G64 GCR-Level Image",
        .marketing_name = "GCR Native Format",
        .platform = "Commodore 64",
        .drives = "1541",
        .era = "1985-present",
        .created_by = "VICE Team",
        .tracks = 42,
        .sides = 1,
        .encoding = "GCR",
        .preserves_timing = true,
        .copy_protection = true,
        .lossless = true,
        .category = "Commodore Preservation",
        .priority = 30
    },
    {
        .id = "G64_HALFTRACK",
        .extension = ".g64",
        .gui_name = "G64 (Half-Tracks)",
        .full_name = "G64 with Half-Track Support",
        .platform = "Commodore 64",
        .tracks = 84,  /* 42 × 2 */
        .encoding = "GCR",
        .preserves_timing = true,
        .copy_protection = true,
        .category = "Commodore Preservation",
        .priority = 31
    },
    /* G71 */
    {
        .id = "G71_STANDARD",
        .extension = ".g71",
        .gui_name = "G71 (1571 GCR)",
        .full_name = "G71 GCR-Level Image",
        .platform = "Commodore 128",
        .drives = "1571",
        .tracks = 84,
        .sides = 2,
        .encoding = "GCR",
        .preserves_timing = true,
        .category = "Commodore Preservation",
        .priority = 32
    },
    /* NIB */
    {
        .id = "NIB_C64",
        .extension = ".nib",
        .typical_size = 333744,
        .gui_name = "NIB (C64 Nibble)",
        .full_name = "Nibbler Raw Nibble Image",
        .marketing_name = "Nibbler Image",
        .platform = "Commodore 64",
        .drives = "1541",
        .era = "1990-present",
        .encoding = "GCR",
        .preserves_timing = true,
        .preserves_weak_bits = true,
        .copy_protection = true,
        .lossless = true,
        .category = "Commodore Preservation",
        .priority = 33
    },
    {.id = NULL}  /* Sentinel */
};

/*============================================================================
 * AMIGA FORMATS
 *============================================================================*/

static const format_entry_ext_t AMIGA_FORMATS_EXT[] = {
    {
        .id = "ADF_OFS",
        .extension = ".adf",
        .magic = "444F5300",  /* "DOS\0" */
        .typical_size = 901120,
        .gui_name = "ADF (OFS)",
        .full_name = "Amiga Original File System",
        .marketing_name = "AmigaDOS OFS",
        .platform = "Amiga 500/1000/2000",
        .secondary_platforms = "A500+, A600",
        .drives = "DF0:, DF1:",
        .era = "1985-1990",
        .created_by = "Commodore-Amiga",
        .tracks = 80,
        .sides = 2,
        .sectors_per_track = 11,
        .bytes_per_sector = 512,
        .rpm = 300,
        .encoding = "MFM (Amiga)",
        .category = "Amiga",
        .icon = "amiga",
        .priority = 1
    },
    {
        .id = "ADF_FFS",
        .extension = ".adf",
        .magic = "444F5301",  /* "DOS\x01" */
        .typical_size = 901120,
        .gui_name = "ADF (FFS)",
        .full_name = "Amiga Fast File System",
        .marketing_name = "AmigaDOS FFS",
        .platform = "Amiga 2.0+",
        .secondary_platforms = "A500+, A600, A1200, A4000",
        .era = "1990-1996",
        .tracks = 80,
        .sides = 2,
        .encoding = "MFM (Amiga)",
        .category = "Amiga",
        .priority = 2
    },
    {
        .id = "ADF_OFS_INTL",
        .extension = ".adf",
        .magic = "444F5302",  /* "DOS\x02" */
        .gui_name = "ADF (OFS-INTL)",
        .full_name = "Amiga OFS International",
        .platform = "Amiga",
        .era = "1991-1996",
        .encoding = "MFM (Amiga)",
        .category = "Amiga",
        .priority = 3
    },
    {
        .id = "ADF_FFS_INTL",
        .extension = ".adf",
        .magic = "444F5303",  /* "DOS\x03" */
        .gui_name = "ADF (FFS-INTL)",
        .full_name = "Amiga FFS International",
        .platform = "Amiga",
        .era = "1991-1996",
        .encoding = "MFM (Amiga)",
        .category = "Amiga",
        .priority = 4
    },
    {
        .id = "ADF_OFS_DC",
        .extension = ".adf",
        .magic = "444F5304",  /* "DOS\x04" */
        .gui_name = "ADF (OFS-DC)",
        .full_name = "Amiga OFS Directory Cache",
        .platform = "Amiga 3.0+",
        .secondary_platforms = "A1200, A4000",
        .era = "1992-1996",
        .encoding = "MFM (Amiga)",
        .category = "Amiga",
        .priority = 5
    },
    {
        .id = "ADF_FFS_DC",
        .extension = ".adf",
        .magic = "444F5305",  /* "DOS\x05" */
        .gui_name = "ADF (FFS-DC)",
        .full_name = "Amiga FFS Directory Cache",
        .marketing_name = "Fast File System with Dir Cache",
        .platform = "Amiga 3.0+",
        .era = "1992-1996",
        .encoding = "MFM (Amiga)",
        .category = "Amiga",
        .priority = 6
    },
    {
        .id = "ADF_HD",
        .extension = ".adf",
        .typical_size = 1802240,
        .gui_name = "ADF (HD)",
        .full_name = "Amiga High Density",
        .marketing_name = "Amiga HD Floppy",
        .platform = "Amiga 4000",
        .drives = "HD DF0:",
        .tracks = 80,
        .sides = 2,
        .sectors_per_track = 22,
        .encoding = "MFM (Amiga)",
        .category = "Amiga",
        .priority = 7
    },
    {
        .id = "ADF_KICKSTART",
        .extension = ".adf",
        .gui_name = "ADF (Kickstart)",
        .full_name = "Amiga Kickstart Disk",
        .platform = "Amiga 1000",
        .era = "1985-1987",
        .read_only = true,
        .category = "Amiga",
        .priority = 10
    },
    {.id = NULL}
};

/*============================================================================
 * APPLE FORMATS
 *============================================================================*/

static const format_entry_ext_t APPLE_FORMATS_EXT[] = {
    /* Apple II */
    {
        .id = "DO_DOS32",
        .extension = ".do,.dsk",
        .typical_size = 116480,
        .gui_name = "DO (DOS 3.2)",
        .full_name = "Apple II DOS 3.2 Order",
        .marketing_name = "DOS 3.2",
        .platform = "Apple II",
        .drives = "Disk II",
        .era = "1978-1980",
        .created_by = "Apple Computer",
        .tracks = 35,
        .sides = 1,
        .sectors_per_track = 13,
        .bytes_per_sector = 256,
        .rpm = 300,
        .encoding = "GCR (Apple)",
        .category = "Apple II",
        .icon = "apple",
        .priority = 1
    },
    {
        .id = "DO_DOS33",
        .extension = ".do,.dsk",
        .typical_size = 143360,
        .gui_name = "DO (DOS 3.3)",
        .full_name = "Apple II DOS 3.3 Order",
        .marketing_name = "DOS 3.3",
        .platform = "Apple II",
        .secondary_platforms = "Apple IIe, IIc, IIgs",
        .drives = "Disk II, DuoDisk, UniDisk",
        .era = "1980-1993",
        .tracks = 35,
        .sides = 1,
        .sectors_per_track = 16,
        .bytes_per_sector = 256,
        .encoding = "GCR (Apple)",
        .category = "Apple II",
        .priority = 2
    },
    {
        .id = "PO_PRODOS",
        .extension = ".po",
        .typical_size = 143360,
        .gui_name = "PO (ProDOS)",
        .full_name = "Apple ProDOS Order",
        .marketing_name = "ProDOS",
        .platform = "Apple IIe/IIc/IIgs",
        .era = "1983-1993",
        .tracks = 35,
        .sectors_per_track = 16,
        .encoding = "GCR (Apple)",
        .category = "Apple II",
        .priority = 3
    },
    {
        .id = "NIB_APPLE",
        .extension = ".nib",
        .typical_size = 232960,
        .gui_name = "NIB (Apple II)",
        .full_name = "Apple II Raw Nibble Image",
        .marketing_name = "Nibble Image",
        .platform = "Apple II",
        .era = "1980-present",
        .tracks = 35,
        .encoding = "GCR (Apple)",
        .preserves_timing = true,
        .copy_protection = true,
        .category = "Apple II Preservation",
        .priority = 10
    },
    {
        .id = "NIB_HALFTRACK",
        .extension = ".nib",
        .gui_name = "NIB (Half-Tracks)",
        .full_name = "Apple II Half-Track Nibble",
        .platform = "Apple II",
        .tracks = 70,  /* 35 × 2 */
        .encoding = "GCR (Apple)",
        .preserves_timing = true,
        .copy_protection = true,
        .category = "Apple II Preservation",
        .priority = 11
    },
    /* WOZ */
    {
        .id = "WOZ_V1",
        .extension = ".woz",
        .magic = "574F5A31",  /* "WOZ1" */
        .gui_name = "WOZ 1.0",
        .full_name = "WOZ Disk Image v1.0",
        .marketing_name = "Applesauce v1",
        .platform = "Apple II",
        .era = "2018-present",
        .created_by = "John K. Morris",
        .encoding = "GCR (Apple)",
        .lossless = true,
        .category = "Apple II Preservation",
        .priority = 20
    },
    {
        .id = "WOZ_V2",
        .extension = ".woz",
        .magic = "574F5A32",  /* "WOZ2" */
        .gui_name = "WOZ 2.0",
        .full_name = "WOZ Disk Image v2.0",
        .marketing_name = "Applesauce v2",
        .platform = "Apple II",
        .era = "2019-present",
        .encoding = "GCR (Apple)",
        .preserves_timing = true,
        .lossless = true,
        .category = "Apple II Preservation",
        .priority = 21
    },
    {
        .id = "WOZ_V21",
        .extension = ".woz",
        .gui_name = "WOZ 2.1 (Flux)",
        .full_name = "WOZ Disk Image v2.1 with Flux",
        .marketing_name = "Applesauce Flux",
        .platform = "Apple II",
        .era = "2020-present",
        .preserves_timing = true,
        .preserves_weak_bits = true,
        .lossless = true,
        .category = "Apple II Preservation",
        .priority = 22
    },
    /* A2R */
    {
        .id = "A2R_V2",
        .extension = ".a2r",
        .magic = "41325232",  /* "A2R2" */
        .gui_name = "A2R (Flux)",
        .full_name = "Applesauce Raw Flux Capture",
        .marketing_name = "A2R Flux",
        .platform = "Apple II",
        .era = "2019-present",
        .created_by = "John K. Morris",
        .preserves_timing = true,
        .preserves_weak_bits = true,
        .lossless = true,
        .category = "Apple II Preservation",
        .priority = 25
    },
    /* 2IMG */
    {
        .id = "2IMG_STANDARD",
        .extension = ".2mg,.2img",
        .magic = "32494D47",  /* "2IMG" */
        .gui_name = "2IMG (Universal)",
        .full_name = "Apple II Universal Disk Image",
        .marketing_name = "2IMG Universal",
        .platform = "Apple IIgs",
        .secondary_platforms = "Apple II, IIe, IIc",
        .era = "1990-present",
        .category = "Apple II",
        .priority = 30
    },
    {.id = NULL}
};

/*============================================================================
 * ATARI FORMATS
 *============================================================================*/

static const format_entry_ext_t ATARI_FORMATS_EXT[] = {
    /* Atari 8-bit */
    {
        .id = "ATR_SD",
        .extension = ".atr",
        .magic = "9602",  /* Little-endian: 0x0296 */
        .typical_size = 92176,
        .gui_name = "ATR (SD)",
        .full_name = "Atari 8-bit Single Density",
        .marketing_name = "Atari 810 Image",
        .platform = "Atari 400/800",
        .secondary_platforms = "XL, XE",
        .drives = "810",
        .era = "1979-1992",
        .created_by = "Atari",
        .tracks = 40,
        .sides = 1,
        .sectors_per_track = 18,
        .bytes_per_sector = 128,
        .rpm = 288,
        .encoding = "FM",
        .category = "Atari 8-bit",
        .icon = "atari",
        .priority = 1
    },
    {
        .id = "ATR_ED",
        .extension = ".atr",
        .typical_size = 133136,
        .gui_name = "ATR (ED)",
        .full_name = "Atari 8-bit Enhanced Density",
        .marketing_name = "Atari 1050 Image",
        .platform = "Atari XL/XE",
        .drives = "1050",
        .era = "1983-1992",
        .tracks = 40,
        .sectors_per_track = 26,
        .bytes_per_sector = 128,
        .encoding = "FM",
        .category = "Atari 8-bit",
        .priority = 2
    },
    {
        .id = "ATR_DD",
        .extension = ".atr",
        .typical_size = 183952,
        .gui_name = "ATR (DD)",
        .full_name = "Atari 8-bit Double Density",
        .marketing_name = "Atari DD Image",
        .platform = "Atari XL/XE",
        .drives = "XF551, Indus GT",
        .era = "1984-1992",
        .tracks = 40,
        .sectors_per_track = 18,
        .bytes_per_sector = 256,
        .encoding = "MFM",
        .category = "Atari 8-bit",
        .priority = 3
    },
    {
        .id = "XFD_STANDARD",
        .extension = ".xfd",
        .gui_name = "XFD (Raw)",
        .full_name = "Atari Raw Disk Image",
        .platform = "Atari 8-bit",
        .category = "Atari 8-bit",
        .priority = 5
    },
    /* Atari ST */
    {
        .id = "ST_STANDARD",
        .extension = ".st",
        .typical_size = 737280,
        .gui_name = "ST (720K)",
        .full_name = "Atari ST Standard Image",
        .marketing_name = "Atari ST Image",
        .platform = "Atari ST",
        .secondary_platforms = "STE, TT, Falcon",
        .drives = "Internal SF354/SF314",
        .era = "1985-1994",
        .created_by = "Atari",
        .tracks = 80,
        .sides = 2,
        .sectors_per_track = 9,
        .bytes_per_sector = 512,
        .rpm = 300,
        .encoding = "MFM",
        .category = "Atari ST",
        .priority = 10
    },
    {
        .id = "MSA_STANDARD",
        .extension = ".msa",
        .magic = "0E0F",
        .gui_name = "MSA (Compressed)",
        .full_name = "Magic Shadow Archiver",
        .marketing_name = "MSA Image",
        .platform = "Atari ST",
        .era = "1988-1994",
        .category = "Atari ST",
        .priority = 11
    },
    {
        .id = "STX_STANDARD",
        .extension = ".stx",
        .magic = "52535900",  /* "RSY\0" */
        .gui_name = "STX (Pasti)",
        .full_name = "Pasti Disk Image",
        .marketing_name = "Pasti STX",
        .platform = "Atari ST",
        .era = "2000-present",
        .created_by = "Jean Louis-Guérin",
        .preserves_timing = true,
        .copy_protection = true,
        .lossless = true,
        .category = "Atari ST Preservation",
        .priority = 20
    },
    {.id = NULL}
};

/*============================================================================
 * PC/IBM FORMATS
 *============================================================================*/

static const format_entry_ext_t PC_FORMATS_EXT[] = {
    {
        .id = "IMG_160K",
        .extension = ".img",
        .typical_size = 163840,
        .gui_name = "IMG (160K)",
        .full_name = "IBM PC 160K 5.25\" SSDD",
        .marketing_name = "PC-DOS 1.0",
        .platform = "IBM PC",
        .drives = "5.25\" SSDD",
        .era = "1981-1983",
        .created_by = "IBM",
        .tracks = 40,
        .sides = 1,
        .sectors_per_track = 8,
        .bytes_per_sector = 512,
        .rpm = 300,
        .encoding = "MFM",
        .category = "PC/DOS",
        .icon = "pc",
        .priority = 1
    },
    {
        .id = "IMG_180K",
        .extension = ".img",
        .typical_size = 184320,
        .gui_name = "IMG (180K)",
        .full_name = "IBM PC 180K 5.25\" SSDD",
        .marketing_name = "PC-DOS 2.0",
        .platform = "IBM PC",
        .era = "1983-1985",
        .tracks = 40,
        .sides = 1,
        .sectors_per_track = 9,
        .category = "PC/DOS",
        .priority = 2
    },
    {
        .id = "IMG_320K",
        .extension = ".img",
        .typical_size = 327680,
        .gui_name = "IMG (320K)",
        .full_name = "IBM PC 320K 5.25\" DSDD",
        .platform = "IBM PC/XT",
        .era = "1983-1986",
        .tracks = 40,
        .sides = 2,
        .sectors_per_track = 8,
        .category = "PC/DOS",
        .priority = 3
    },
    {
        .id = "IMG_360K",
        .extension = ".img",
        .typical_size = 368640,
        .gui_name = "IMG (360K)",
        .full_name = "IBM PC 360K 5.25\" DSDD",
        .marketing_name = "PC/XT Standard",
        .platform = "IBM PC/XT",
        .era = "1983-1990",
        .tracks = 40,
        .sides = 2,
        .sectors_per_track = 9,
        .category = "PC/DOS",
        .priority = 4
    },
    {
        .id = "IMG_720K",
        .extension = ".img",
        .typical_size = 737280,
        .gui_name = "IMG (720K)",
        .full_name = "IBM PC 720K 3.5\" DSDD",
        .marketing_name = "PC/AT 3.5\"",
        .platform = "IBM PC/AT",
        .secondary_platforms = "PS/2",
        .era = "1986-1995",
        .tracks = 80,
        .sides = 2,
        .sectors_per_track = 9,
        .category = "PC/DOS",
        .priority = 5
    },
    {
        .id = "IMG_1200K",
        .extension = ".img",
        .typical_size = 1228800,
        .gui_name = "IMG (1.2M)",
        .full_name = "IBM PC 1.2M 5.25\" DSHD",
        .marketing_name = "PC/AT 5.25\" HD",
        .platform = "IBM PC/AT",
        .era = "1984-1995",
        .tracks = 80,
        .sides = 2,
        .sectors_per_track = 15,
        .rpm = 360,
        .category = "PC/DOS",
        .priority = 6
    },
    {
        .id = "IMG_1440K",
        .extension = ".img",
        .typical_size = 1474560,
        .gui_name = "IMG (1.44M)",
        .full_name = "IBM PC 1.44M 3.5\" DSHD",
        .marketing_name = "PC Standard 3.5\"",
        .platform = "IBM PC",
        .era = "1987-2010",
        .tracks = 80,
        .sides = 2,
        .sectors_per_track = 18,
        .category = "PC/DOS",
        .priority = 7
    },
    {
        .id = "IMG_2880K",
        .extension = ".img",
        .typical_size = 2949120,
        .gui_name = "IMG (2.88M)",
        .full_name = "IBM PC 2.88M 3.5\" ED",
        .marketing_name = "PS/2 ED",
        .platform = "IBM PS/2",
        .era = "1991-1995",
        .tracks = 80,
        .sides = 2,
        .sectors_per_track = 36,
        .category = "PC/DOS",
        .priority = 8
    },
    {
        .id = "IMG_DMF",
        .extension = ".img",
        .typical_size = 1720320,
        .gui_name = "IMG (DMF)",
        .full_name = "Microsoft Distribution Media Format",
        .marketing_name = "DMF 1.68M",
        .platform = "IBM PC",
        .era = "1993-2000",
        .tracks = 80,
        .sides = 2,
        .sectors_per_track = 21,
        .category = "PC/DOS",
        .priority = 9
    },
    {
        .id = "IMG_XDF",
        .extension = ".xdf",
        .typical_size = 1884160,
        .gui_name = "XDF (Extended)",
        .full_name = "IBM XDF Extended Format",
        .marketing_name = "OS/2 XDF",
        .platform = "IBM PS/2",
        .era = "1992-1996",
        .category = "PC/DOS",
        .priority = 10
    },
    {.id = NULL}
};

/*============================================================================
 * FLUX FORMATS
 *============================================================================*/

static const format_entry_ext_t FLUX_FORMATS_EXT[] = {
    {
        .id = "SCP_V1",
        .extension = ".scp",
        .magic = "534350",  /* "SCP" */
        .gui_name = "SCP (SuperCard Pro)",
        .full_name = "SuperCard Pro Flux Image",
        .marketing_name = "SCP Image",
        .platform = "Universal",
        .era = "2013-present",
        .created_by = "Jim Drew",
        .preserves_timing = true,
        .preserves_weak_bits = true,
        .lossless = true,
        .category = "Flux Formats",
        .icon = "flux",
        .priority = 1
    },
    {
        .id = "SCP_V25",
        .extension = ".scp",
        .gui_name = "SCP v2.5",
        .full_name = "SuperCard Pro v2.5",
        .platform = "Universal",
        .era = "2015-present",
        .preserves_timing = true,
        .preserves_weak_bits = true,
        .lossless = true,
        .category = "Flux Formats",
        .priority = 2
    },
    {
        .id = "HFE_V1",
        .extension = ".hfe",
        .magic = "48584350494346",  /* "HXCPICF" */
        .gui_name = "HFE (v1)",
        .full_name = "HxC Floppy Emulator v1",
        .marketing_name = "HFE Standard",
        .platform = "Universal",
        .era = "2006-present",
        .created_by = "Jean-François Del Nero",
        .category = "Flux Formats",
        .priority = 10
    },
    {
        .id = "HFE_V2",
        .extension = ".hfe",
        .gui_name = "HFE (v2 Variable)",
        .full_name = "HxC v2 Variable Bitrate",
        .platform = "Universal",
        .era = "2010-present",
        .category = "Flux Formats",
        .priority = 11
    },
    {
        .id = "HFE_V3",
        .extension = ".hfe",
        .magic = "48584348464556",  /* "HXCHFEV" */
        .gui_name = "HFE (v3 Opcodes)",
        .full_name = "HxC v3 with Opcodes",
        .platform = "Universal",
        .era = "2015-present",
        .preserves_timing = true,
        .preserves_weak_bits = true,
        .category = "Flux Formats",
        .priority = 12
    },
    {
        .id = "IPF_STANDARD",
        .extension = ".ipf",
        .magic = "43415053",  /* "CAPS" */
        .gui_name = "IPF (SPS)",
        .full_name = "Interchangeable Preservation Format",
        .marketing_name = "SPS IPF",
        .platform = "Amiga, Atari ST",
        .era = "2001-present",
        .created_by = "Software Preservation Society",
        .preserves_timing = true,
        .preserves_weak_bits = true,
        .copy_protection = true,
        .lossless = true,
        .category = "Flux Formats",
        .priority = 20
    },
    {
        .id = "KF_RAW",
        .extension = ".raw",
        .gui_name = "KryoFlux (Raw)",
        .full_name = "KryoFlux Stream Files",
        .marketing_name = "KF Stream",
        .platform = "Universal",
        .era = "2009-present",
        .created_by = "Software Preservation Society",
        .preserves_timing = true,
        .preserves_weak_bits = true,
        .lossless = true,
        .category = "Flux Formats",
        .priority = 30
    },
    {
        .id = "GW_RAW",
        .extension = ".raw,.gw",
        .gui_name = "Greaseweazle",
        .full_name = "Greaseweazle Raw Flux",
        .marketing_name = "GW Raw",
        .platform = "Universal",
        .era = "2019-present",
        .created_by = "Keir Fraser",
        .preserves_timing = true,
        .preserves_weak_bits = true,
        .lossless = true,
        .category = "Flux Formats",
        .priority = 31
    },
    /* UFF - Our format */
    {
        .id = "UFF_V1",
        .extension = ".uff",
        .magic = "55464600",  /* "UFF\0" */
        .gui_name = "UFF (UFT Universal)",
        .full_name = "UFT Universal Flux Format",
        .marketing_name = "Kein Bit geht verloren",
        .platform = "Universal",
        .era = "2025-present",
        .created_by = "UFT Project",
        .preserves_timing = true,
        .preserves_weak_bits = true,
        .copy_protection = true,
        .lossless = true,
        .has_error_info = true,
        .category = "Flux Formats",
        .priority = 0  /* Highest priority */
    },
    {.id = NULL}
};

/*============================================================================
 * PC-98 / JAPANESE FORMATS
 *============================================================================*/

static const format_entry_ext_t JAPANESE_FORMATS_EXT[] = {
    {
        .id = "D88_STANDARD",
        .extension = ".d88,.d77,.d68",
        .gui_name = "D88 (PC-88/98)",
        .full_name = "NEC PC-88/PC-98 Disk Image",
        .marketing_name = "D88 Image",
        .platform = "NEC PC-88",
        .secondary_platforms = "PC-98, FM-7",
        .era = "1981-1998",
        .created_by = "NEC",
        .tracks = 80,
        .sides = 2,
        .encoding = "MFM",
        .category = "Japanese",
        .icon = "pc98",
        .priority = 1
    },
    {
        .id = "FDI_PC98",
        .extension = ".fdi",
        .gui_name = "FDI (PC-98)",
        .full_name = "PC-98 FDI Image",
        .platform = "NEC PC-98",
        .era = "1990-2000",
        .category = "Japanese",
        .priority = 2
    },
    {
        .id = "NFD_STANDARD",
        .extension = ".nfd",
        .gui_name = "NFD (T98-Next)",
        .full_name = "T98-Next NFD Image",
        .platform = "NEC PC-98",
        .category = "Japanese",
        .priority = 3
    },
    {
        .id = "HDM_STANDARD",
        .extension = ".hdm",
        .typical_size = 1261568,
        .gui_name = "HDM (PC-98 HD)",
        .full_name = "PC-98 High Density Image",
        .platform = "NEC PC-98",
        .tracks = 77,
        .sides = 2,
        .sectors_per_track = 8,
        .bytes_per_sector = 1024,
        .category = "Japanese",
        .priority = 4
    },
    {
        .id = "DIM_STANDARD",
        .extension = ".dim",
        .gui_name = "DIM (PC-98)",
        .full_name = "DIM Disk Image",
        .platform = "NEC PC-98",
        .category = "Japanese",
        .priority = 5
    },
    {.id = NULL}
};

/*============================================================================
 * OTHER FORMATS
 *============================================================================*/

static const format_entry_ext_t OTHER_FORMATS_EXT[] = {
    /* TRS-80 */
    {
        .id = "DMK_STANDARD",
        .extension = ".dmk",
        .gui_name = "DMK (TRS-80)",
        .full_name = "David Keil's Disk Image",
        .marketing_name = "TRS-80 DMK",
        .platform = "TRS-80",
        .secondary_platforms = "CoCo",
        .era = "1977-1990",
        .tracks = 40,
        .encoding = "FM/MFM",
        .preserves_timing = true,
        .copy_protection = true,
        .category = "TRS-80",
        .priority = 1
    },
    /* CPC */
    {
        .id = "DSK_CPC",
        .extension = ".dsk",
        .magic = "4D562D43504331",  /* "MV-CPC1" / "EXTENDED" */
        .gui_name = "DSK (CPC/Spectrum)",
        .full_name = "Amstrad CPC/Spectrum DSK",
        .platform = "Amstrad CPC",
        .secondary_platforms = "ZX Spectrum +3",
        .era = "1984-1992",
        .tracks = 40,
        .sides = 1,
        .encoding = "MFM",
        .category = "Amstrad",
        .priority = 10
    },
    {
        .id = "DSK_CPC_EXTENDED",
        .extension = ".dsk",
        .gui_name = "DSK (Extended)",
        .full_name = "Extended CPC Disk Image",
        .platform = "Amstrad CPC",
        .preserves_timing = true,
        .copy_protection = true,
        .category = "Amstrad",
        .priority = 11
    },
    /* BBC */
    {
        .id = "SSD_BBC",
        .extension = ".ssd",
        .typical_size = 102400,
        .gui_name = "SSD (BBC Micro)",
        .full_name = "BBC Micro Single-Sided",
        .platform = "BBC Micro",
        .drives = "8271",
        .era = "1981-1990",
        .tracks = 40,
        .sides = 1,
        .sectors_per_track = 10,
        .bytes_per_sector = 256,
        .encoding = "FM",
        .category = "BBC",
        .priority = 20
    },
    {
        .id = "DSD_BBC",
        .extension = ".dsd",
        .typical_size = 204800,
        .gui_name = "DSD (BBC Micro)",
        .full_name = "BBC Micro Double-Sided",
        .platform = "BBC Micro",
        .tracks = 40,
        .sides = 2,
        .category = "BBC",
        .priority = 21
    },
    /* TI-99 */
    {
        .id = "DSK_TI99",
        .extension = ".dsk",
        .gui_name = "DSK (TI-99/4A)",
        .full_name = "Texas Instruments TI-99/4A",
        .platform = "TI-99/4A",
        .era = "1979-1984",
        .tracks = 40,
        .sides = 1,
        .encoding = "FM",
        .category = "Texas Instruments",
        .priority = 30
    },
    /* IMD */
    {
        .id = "IMD_STANDARD",
        .extension = ".imd",
        .magic = "494D44",  /* "IMD" */
        .gui_name = "IMD (ImageDisk)",
        .full_name = "ImageDisk Format",
        .marketing_name = "Dave Dunfield IMD",
        .platform = "Universal",
        .era = "2005-present",
        .created_by = "Dave Dunfield",
        .preserves_timing = true,
        .has_error_info = true,
        .category = "Archive Formats",
        .priority = 40
    },
    /* TD0 */
    {
        .id = "TD0_STANDARD",
        .extension = ".td0",
        .magic = "5444",  /* "TD" or "td" */
        .gui_name = "TD0 (TeleDisk)",
        .full_name = "Sydex TeleDisk Image",
        .marketing_name = "TeleDisk",
        .platform = "Universal",
        .era = "1985-2000",
        .created_by = "Sydex",
        .has_error_info = true,
        .category = "Archive Formats",
        .priority = 41
    },
    /* CQM */
    {
        .id = "CQM_STANDARD",
        .extension = ".cqm",
        .gui_name = "CQM (CopyQM)",
        .full_name = "CopyQM Compressed Image",
        .platform = "Universal",
        .era = "1990-2000",
        .category = "Archive Formats",
        .priority = 42
    },
    {.id = NULL}
};

/*============================================================================
 * API FUNCTIONS
 *============================================================================*/

/**
 * @brief Get all formats for GUI dropdown
 */
typedef struct {
    const format_entry_ext_t* formats;
    size_t count;
    const char* category;
} format_category_t;

static format_category_t get_all_categories(void) {
    static const format_category_t cats[] = {
        {COMMODORE_FORMATS_EXT, 0, "Commodore"},
        {AMIGA_FORMATS_EXT, 0, "Amiga"},
        {APPLE_FORMATS_EXT, 0, "Apple"},
        {ATARI_FORMATS_EXT, 0, "Atari"},
        {PC_FORMATS_EXT, 0, "PC/DOS"},
        {JAPANESE_FORMATS_EXT, 0, "Japanese"},
        {FLUX_FORMATS_EXT, 0, "Flux Formats"},
        {OTHER_FORMATS_EXT, 0, "Other"},
    };
    (void)cats;  /* Suppress unused warning */
    return cats[0];
}

/**
 * @brief Find format by ID
 */
const format_entry_ext_t* format_find_by_id(const char* id) {
    const format_entry_ext_t* lists[] = {
        COMMODORE_FORMATS_EXT, AMIGA_FORMATS_EXT, APPLE_FORMATS_EXT,
        ATARI_FORMATS_EXT, PC_FORMATS_EXT, FLUX_FORMATS_EXT,
        JAPANESE_FORMATS_EXT, OTHER_FORMATS_EXT
    };
    
    for (size_t i = 0; i < sizeof(lists)/sizeof(lists[0]); i++) {
        for (const format_entry_ext_t* f = lists[i]; f->id != NULL; f++) {
            if (strcmp(f->id, id) == 0) {
                return f;
            }
        }
    }
    return NULL;
}

/**
 * @brief Find format by extension and size
 */
const format_entry_ext_t* format_detect_by_size(const char* ext, uint32_t size) {
    const format_entry_ext_t* lists[] = {
        COMMODORE_FORMATS_EXT, AMIGA_FORMATS_EXT, APPLE_FORMATS_EXT,
        ATARI_FORMATS_EXT, PC_FORMATS_EXT, FLUX_FORMATS_EXT,
        JAPANESE_FORMATS_EXT, OTHER_FORMATS_EXT
    };
    
    for (size_t i = 0; i < sizeof(lists)/sizeof(lists[0]); i++) {
        for (const format_entry_ext_t* f = lists[i]; f->id != NULL; f++) {
            if (f->extension && strstr(f->extension, ext)) {
                if (f->typical_size == 0 || f->typical_size == size ||
                    (size > f->typical_size - 1024 && size < f->typical_size + 1024)) {
                    return f;
                }
            }
        }
    }
    return NULL;
}

/**
 * @brief Get GUI display name
 */
const char* format_get_gui_name(const format_entry_ext_t* f) {
    return f ? f->gui_name : "Unknown";
}

/**
 * @brief Print format info
 */
void format_print_info(const format_entry_ext_t* f) {
    if (!f) return;
    
    printf("╔══════════════════════════════════════════════════════════════════════╗\n");
    printf("║ %s\n", f->full_name);
    printf("╠══════════════════════════════════════════════════════════════════════╣\n");
    printf("║ ID:          %s\n", f->id);
    printf("║ Extension:   %s\n", f->extension);
    printf("║ Platform:    %s\n", f->platform);
    if (f->secondary_platforms)
        printf("║ Also:        %s\n", f->secondary_platforms);
    if (f->drives)
        printf("║ Drives:      %s\n", f->drives);
    if (f->era)
        printf("║ Era:         %s\n", f->era);
    if (f->encoding)
        printf("║ Encoding:    %s\n", f->encoding);
    if (f->typical_size > 0)
        printf("║ Typical:     %u bytes\n", f->typical_size);
    printf("║ Preservation: %s%s%s%s\n",
           f->preserves_timing ? "Timing " : "",
           f->preserves_weak_bits ? "WeakBits " : "",
           f->lossless ? "Lossless " : "",
           f->copy_protection ? "CopyProt" : "");
    printf("╚══════════════════════════════════════════════════════════════════════╝\n");
}

/*============================================================================
 * TEST
 *============================================================================*/

#ifdef UFT_TEST

int main(void) {
    printf("\n=== Format Names Extended Test ===\n\n");
    
    /* Test find by ID */
    const format_entry_ext_t* f = format_find_by_id("D64_STANDARD");
    if (f) {
        printf("Found D64_STANDARD:\n");
        format_print_info(f);
    }
    
    /* Test UFF */
    f = format_find_by_id("UFF_V1");
    if (f) {
        printf("\nFound UFF_V1:\n");
        format_print_info(f);
    }
    
    /* Test detect by size */
    f = format_detect_by_size(".adf", 901120);
    if (f) {
        printf("\nDetected by size (.adf, 901120):\n");
        printf("  → %s\n", f->gui_name);
    }
    
    /* List all Commodore formats */
    printf("\nAll Commodore Formats:\n");
    for (const format_entry_ext_t* fmt = COMMODORE_FORMATS_EXT; fmt->id != NULL; fmt++) {
        printf("  %s - %s\n", fmt->gui_name, fmt->full_name);
    }
    
    /* List all Flux formats */
    printf("\nAll Flux Formats:\n");
    for (const format_entry_ext_t* fmt = FLUX_FORMATS_EXT; fmt->id != NULL; fmt++) {
        printf("  %s - %s%s\n", fmt->gui_name, fmt->full_name,
               fmt->lossless ? " [lossless]" : "");
    }
    
    printf("\n=== Test Complete ===\n");
    return 0;
}

#endif /* UFT_TEST */
