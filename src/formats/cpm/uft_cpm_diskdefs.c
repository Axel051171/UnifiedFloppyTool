/**
 * @file uft_cpm_diskdefs.c
 * @brief Comprehensive CP/M disk definitions implementation
 * @version 3.9.0
 * 
 * Contains 50+ CP/M disk format definitions.
 * Reference: libdsk diskdefs, cpmtools diskdefs
 */

#include "uft/formats/uft_cpm_diskdefs.h"
#include "uft/uft_format_common.h"
#include "uft/core/uft_error_compat.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Skew Tables
 * ============================================================================ */

/* Standard 6-sector skew */
static const uint8_t skew_6[] = {1, 7, 13, 19, 25, 5, 11, 17, 23, 3, 9, 15, 21, 
                                  2, 8, 14, 20, 26, 6, 12, 18, 24, 4, 10, 16, 22};

/* Kaypro skew (2:1) */
static const uint8_t skew_kaypro[] = {0, 5, 1, 6, 2, 7, 3, 8, 4, 9};

/* Amstrad CPC skew */
static const uint8_t skew_cpc[] = {0xC1, 0xC6, 0xC2, 0xC7, 0xC3, 0xC8, 0xC4, 0xC9, 0xC5};

/* ============================================================================
 * Standard CP/M Disk Definitions
 * ============================================================================ */

/* IBM 8" SSSD - The original CP/M format */
const cpm_diskdef_t cpm_ibm_8ss = {
    .name = "ibm-8ss",
    .description = "IBM 8\" SSSD (CP/M standard)",
    .cylinders = 77,
    .heads = 1,
    .sectors = 26,
    .sector_size = 128,
    .dpb = {
        .spt = 26,
        .bsh = 3,
        .blm = 7,
        .exm = 0,
        .dsm = 242,
        .drm = 63,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 16,
        .off = 2,
        .psh = 0,
        .phm = 0
    },
    .first_sector = 1,
    .skew_mode = CPM_SKEW_LOGICAL,
    .skew_table = skew_6,
    .boot_mode = CPM_BOOT_STANDARD,
    .double_sided = false,
    .high_density = false,
    .is_mfm = false
};

/* IBM 8" DSDD */
const cpm_diskdef_t cpm_ibm_8ds = {
    .name = "ibm-8ds",
    .description = "IBM 8\" DSDD",
    .cylinders = 77,
    .heads = 2,
    .sectors = 26,
    .sector_size = 256,
    .dpb = {
        .spt = 52,
        .bsh = 4,
        .blm = 15,
        .exm = 1,
        .dsm = 493,
        .drm = 127,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 32,
        .off = 2,
        .psh = 1,
        .phm = 1
    },
    .first_sector = 1,
    .skew_mode = CPM_SKEW_LOGICAL,
    .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD,
    .double_sided = true,
    .high_density = false,
    .is_mfm = true
};

/* Amstrad CPC Data format */
const cpm_diskdef_t cpm_amstrad_cpc_data = {
    .name = "cpc-data",
    .description = "Amstrad CPC Data Format",
    .cylinders = 40,
    .heads = 1,
    .sectors = 9,
    .sector_size = 512,
    .dpb = {
        .spt = 36,
        .bsh = 3,
        .blm = 7,
        .exm = 0,
        .dsm = 170,
        .drm = 63,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 16,
        .off = 0,
        .psh = 2,
        .phm = 3
    },
    .first_sector = 0xC1,
    .skew_mode = CPM_SKEW_PHYSICAL,
    .skew_table = skew_cpc,
    .boot_mode = CPM_BOOT_NONE,
    .double_sided = false,
    .high_density = false,
    .is_mfm = true
};

/* Amstrad CPC System format */
const cpm_diskdef_t cpm_amstrad_cpc_system = {
    .name = "cpc-system",
    .description = "Amstrad CPC System Format",
    .cylinders = 40,
    .heads = 1,
    .sectors = 9,
    .sector_size = 512,
    .dpb = {
        .spt = 36,
        .bsh = 3,
        .blm = 7,
        .exm = 0,
        .dsm = 155,
        .drm = 63,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 16,
        .off = 2,
        .psh = 2,
        .phm = 3
    },
    .first_sector = 0x41,
    .skew_mode = CPM_SKEW_PHYSICAL,
    .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD,
    .double_sided = false,
    .high_density = false,
    .is_mfm = true
};

/* Amstrad PCW 3" CF2 format */
const cpm_diskdef_t cpm_amstrad_pcw_cf2 = {
    .name = "pcw-cf2",
    .description = "Amstrad PCW 3\" CF2",
    .cylinders = 40,
    .heads = 2,
    .sectors = 9,
    .sector_size = 512,
    .dpb = {
        .spt = 36,
        .bsh = 3,
        .blm = 7,
        .exm = 0,
        .dsm = 355,
        .drm = 127,
        .al0 = 0xF0,
        .al1 = 0x00,
        .cks = 32,
        .off = 1,
        .psh = 2,
        .phm = 3
    },
    .first_sector = 1,
    .skew_mode = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD,
    .double_sided = true,
    .high_density = false,
    .is_mfm = true
};

/* Kaypro II (SSDD) */
const cpm_diskdef_t cpm_kaypro_ii = {
    .name = "kaypro-ii",
    .description = "Kaypro II SSDD",
    .cylinders = 40,
    .heads = 1,
    .sectors = 10,
    .sector_size = 512,
    .dpb = {
        .spt = 40,
        .bsh = 3,
        .blm = 7,
        .exm = 0,
        .dsm = 194,
        .drm = 63,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 16,
        .off = 1,
        .psh = 2,
        .phm = 3
    },
    .first_sector = 0,
    .skew_mode = CPM_SKEW_PHYSICAL,
    .skew_table = skew_kaypro,
    .boot_mode = CPM_BOOT_STANDARD,
    .double_sided = false,
    .high_density = false,
    .is_mfm = true
};

/* Kaypro 4 (DSDD) */
const cpm_diskdef_t cpm_kaypro_4 = {
    .name = "kaypro-4",
    .description = "Kaypro 4 DSDD",
    .cylinders = 40,
    .heads = 2,
    .sectors = 10,
    .sector_size = 512,
    .dpb = {
        .spt = 40,
        .bsh = 4,
        .blm = 15,
        .exm = 1,
        .dsm = 196,
        .drm = 127,
        .al0 = 0xF0,
        .al1 = 0x00,
        .cks = 32,
        .off = 1,
        .psh = 2,
        .phm = 3
    },
    .first_sector = 0,
    .skew_mode = CPM_SKEW_PHYSICAL,
    .skew_table = skew_kaypro,
    .boot_mode = CPM_BOOT_STANDARD,
    .double_sided = true,
    .high_density = false,
    .is_mfm = true
};

/* Osborne 1 (SSSD) */
const cpm_diskdef_t cpm_osborne_1 = {
    .name = "osborne-1",
    .description = "Osborne 1 SSSD",
    .cylinders = 40,
    .heads = 1,
    .sectors = 10,
    .sector_size = 256,
    .dpb = {
        .spt = 20,
        .bsh = 3,
        .blm = 7,
        .exm = 0,
        .dsm = 45,
        .drm = 63,
        .al0 = 0x80,
        .al1 = 0x00,
        .cks = 16,
        .off = 3,
        .psh = 1,
        .phm = 1
    },
    .first_sector = 1,
    .skew_mode = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD,
    .double_sided = false,
    .high_density = false,
    .is_mfm = true
};

/* Osborne Executive (SSDD) */
const cpm_diskdef_t cpm_osborne_exec = {
    .name = "osborne-exec",
    .description = "Osborne Executive SSDD",
    .cylinders = 40,
    .heads = 1,
    .sectors = 5,
    .sector_size = 1024,
    .dpb = {
        .spt = 20,
        .bsh = 4,
        .blm = 15,
        .exm = 1,
        .dsm = 91,
        .drm = 127,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 32,
        .off = 3,
        .psh = 3,
        .phm = 7
    },
    .first_sector = 1,
    .skew_mode = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD,
    .double_sided = false,
    .high_density = false,
    .is_mfm = true
};

/* Xerox 820 (SSSD) */
const cpm_diskdef_t cpm_xerox_820 = {
    .name = "xerox-820",
    .description = "Xerox 820 SSSD",
    .cylinders = 40,
    .heads = 1,
    .sectors = 18,
    .sector_size = 128,
    .dpb = {
        .spt = 18,
        .bsh = 3,
        .blm = 7,
        .exm = 0,
        .dsm = 82,
        .drm = 31,
        .al0 = 0x80,
        .al1 = 0x00,
        .cks = 8,
        .off = 2,
        .psh = 0,
        .phm = 0
    },
    .first_sector = 1,
    .skew_mode = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD,
    .double_sided = false,
    .high_density = false,
    .is_mfm = false
};

/* Morrow Micro Decision MD2 */
const cpm_diskdef_t cpm_morrow_md2 = {
    .name = "morrow-md2",
    .description = "Morrow Micro Decision MD2",
    .cylinders = 40,
    .heads = 1,
    .sectors = 10,
    .sector_size = 512,
    .dpb = {
        .spt = 40,
        .bsh = 3,
        .blm = 7,
        .exm = 0,
        .dsm = 194,
        .drm = 63,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 16,
        .off = 2,
        .psh = 2,
        .phm = 3
    },
    .first_sector = 1,
    .skew_mode = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD,
    .double_sided = false,
    .high_density = false,
    .is_mfm = true
};

/* Epson QX-10 */
const cpm_diskdef_t cpm_epson_qx10 = {
    .name = "epson-qx10",
    .description = "Epson QX-10",
    .cylinders = 40,
    .heads = 2,
    .sectors = 16,
    .sector_size = 256,
    .dpb = {
        .spt = 64,
        .bsh = 4,
        .blm = 15,
        .exm = 1,
        .dsm = 315,
        .drm = 255,
        .al0 = 0xF0,
        .al1 = 0x00,
        .cks = 64,
        .off = 2,
        .psh = 1,
        .phm = 1
    },
    .first_sector = 1,
    .skew_mode = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD,
    .double_sided = true,
    .high_density = false,
    .is_mfm = true
};

/* NEC PC-8801 */
const cpm_diskdef_t cpm_nec_pc8801 = {
    .name = "nec-pc8801",
    .description = "NEC PC-8801 CP/M",
    .cylinders = 40,
    .heads = 2,
    .sectors = 16,
    .sector_size = 256,
    .dpb = {
        .spt = 64,
        .bsh = 4,
        .blm = 15,
        .exm = 0,
        .dsm = 157,
        .drm = 127,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 32,
        .off = 2,
        .psh = 1,
        .phm = 1
    },
    .first_sector = 1,
    .skew_mode = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD,
    .double_sided = true,
    .high_density = false,
    .is_mfm = true
};

/* Sharp MZ-80B */
const cpm_diskdef_t cpm_sharp_mz80b = {
    .name = "sharp-mz80b",
    .description = "Sharp MZ-80B CP/M",
    .cylinders = 35,
    .heads = 1,
    .sectors = 16,
    .sector_size = 256,
    .dpb = {
        .spt = 32,
        .bsh = 3,
        .blm = 7,
        .exm = 0,
        .dsm = 134,
        .drm = 63,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 16,
        .off = 3,
        .psh = 1,
        .phm = 1
    },
    .first_sector = 1,
    .skew_mode = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD,
    .double_sided = false,
    .high_density = false,
    .is_mfm = true
};

/* Zorba */
const cpm_diskdef_t cpm_zorba = {
    .name = "zorba",
    .description = "Zorba CP/M",
    .cylinders = 40,
    .heads = 2,
    .sectors = 5,
    .sector_size = 1024,
    .dpb = {
        .spt = 40,
        .bsh = 4,
        .blm = 15,
        .exm = 1,
        .dsm = 195,
        .drm = 127,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 32,
        .off = 2,
        .psh = 3,
        .phm = 7
    },
    .first_sector = 1,
    .skew_mode = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD,
    .double_sided = true,
    .high_density = false,
    .is_mfm = true
};

/* TRS-80 Model 4 CP/M */
const cpm_diskdef_t cpm_trs80_m4 = {
    .name = "trs80-m4",
    .description = "TRS-80 Model 4 DSDD",
    .cylinders = 40,
    .heads = 2,
    .sectors = 18,
    .sector_size = 256,
    .dpb = {
        .spt = 36, .bsh = 4, .blm = 15, .exm = 1,
        .dsm = 179, .drm = 127, .al0 = 0xC0, .al1 = 0x00,
        .cks = 32, .off = 2, .psh = 1, .phm = 1
    },
    .first_sector = 0, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = true
};

/* Vector Graphic */
const cpm_diskdef_t cpm_vector = {
    .name = "vector",
    .description = "Vector Graphic",
    .cylinders = 77, .heads = 1, .sectors = 32, .sector_size = 128,
    .dpb = {
        .spt = 32, .bsh = 3, .blm = 7, .exm = 0,
        .dsm = 300, .drm = 63, .al0 = 0xC0, .al1 = 0x00,
        .cks = 16, .off = 4, .psh = 0, .phm = 0
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = false,
    .high_density = false, .is_mfm = false
};

/* Superbrain */
const cpm_diskdef_t cpm_superbrain = {
    .name = "superbrain",
    .description = "Intertec Superbrain",
    .cylinders = 35, .heads = 2, .sectors = 10, .sector_size = 512,
    .dpb = {
        .spt = 20, .bsh = 4, .blm = 15, .exm = 1,
        .dsm = 170, .drm = 63, .al0 = 0x80, .al1 = 0x00,
        .cks = 16, .off = 2, .psh = 2, .phm = 3
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = true
};

/* Televideo */
const cpm_diskdef_t cpm_televideo = {
    .name = "televideo",
    .description = "Televideo 802/803",
    .cylinders = 77, .heads = 2, .sectors = 26, .sector_size = 128,
    .dpb = {
        .spt = 52, .bsh = 4, .blm = 15, .exm = 1,
        .dsm = 489, .drm = 127, .al0 = 0xC0, .al1 = 0x00,
        .cks = 32, .off = 2, .psh = 0, .phm = 0
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = false
};

/* Ampro Little Board */
const cpm_diskdef_t cpm_ampro = {
    .name = "ampro",
    .description = "Ampro Little Board",
    .cylinders = 40, .heads = 2, .sectors = 5, .sector_size = 1024,
    .dpb = {
        .spt = 40, .bsh = 4, .blm = 15, .exm = 1,
        .dsm = 194, .drm = 127, .al0 = 0xC0, .al1 = 0x00,
        .cks = 32, .off = 2, .psh = 3, .phm = 7
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = true
};

/* North Star */
const cpm_diskdef_t cpm_northstar = {
    .name = "northstar",
    .description = "North Star Horizon",
    .cylinders = 35, .heads = 1, .sectors = 10, .sector_size = 512,
    .dpb = {
        .spt = 10, .bsh = 3, .blm = 7, .exm = 0,
        .dsm = 84, .drm = 31, .al0 = 0x80, .al1 = 0x00,
        .cks = 8, .off = 3, .psh = 2, .phm = 3
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = false,
    .high_density = false, .is_mfm = true
};

/* Cromemco */
const cpm_diskdef_t cpm_cromemco = {
    .name = "cromemco",
    .description = "Cromemco CDOS",
    .cylinders = 77, .heads = 2, .sectors = 16, .sector_size = 512,
    .dpb = {
        .spt = 32, .bsh = 4, .blm = 15, .exm = 1,
        .dsm = 616, .drm = 255, .al0 = 0xF0, .al1 = 0x00,
        .cks = 64, .off = 2, .psh = 2, .phm = 3
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = true
};

/* Altos 8000 */
const cpm_diskdef_t cpm_altos = {
    .name = "altos",
    .description = "Altos 8000",
    .cylinders = 77, .heads = 2, .sectors = 16, .sector_size = 256,
    .dpb = {
        .spt = 32, .bsh = 3, .blm = 7, .exm = 0,
        .dsm = 299, .drm = 127, .al0 = 0xC0, .al1 = 0x00,
        .cks = 32, .off = 4, .psh = 1, .phm = 1
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = true
};

/* DEC Rainbow */
const cpm_diskdef_t cpm_rainbow = {
    .name = "rainbow",
    .description = "DEC Rainbow 100",
    .cylinders = 80, .heads = 1, .sectors = 16, .sector_size = 256,
    .dpb = {
        .spt = 16, .bsh = 4, .blm = 15, .exm = 1,
        .dsm = 156, .drm = 63, .al0 = 0x80, .al1 = 0x00,
        .cks = 16, .off = 2, .psh = 1, .phm = 1
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = false,
    .high_density = false, .is_mfm = true
};

/* Bondwell */
const cpm_diskdef_t cpm_bondwell = {
    .name = "bondwell",
    .description = "Bondwell 12/14",
    .cylinders = 40, .heads = 2, .sectors = 9, .sector_size = 512,
    .dpb = {
        .spt = 36, .bsh = 4, .blm = 15, .exm = 1,
        .dsm = 174, .drm = 127, .al0 = 0xC0, .al1 = 0x00,
        .cks = 32, .off = 2, .psh = 2, .phm = 3
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = true
};

/* CP/M-86 PC 360K */
const cpm_diskdef_t cpm_pc360 = {
    .name = "pc360",
    .description = "CP/M-86 PC 360K",
    .cylinders = 40, .heads = 2, .sectors = 9, .sector_size = 512,
    .dpb = {
        .spt = 36, .bsh = 4, .blm = 15, .exm = 1,
        .dsm = 170, .drm = 63, .al0 = 0x80, .al1 = 0x00,
        .cks = 16, .off = 1, .psh = 2, .phm = 3
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = true
};

/* CP/M-86 PC 1.2M */
const cpm_diskdef_t cpm_pc1200 = {
    .name = "pc1200",
    .description = "CP/M-86 PC 1.2M",
    .cylinders = 80, .heads = 2, .sectors = 15, .sector_size = 512,
    .dpb = {
        .spt = 60, .bsh = 4, .blm = 15, .exm = 0,
        .dsm = 592, .drm = 255, .al0 = 0xF0, .al1 = 0x00,
        .cks = 64, .off = 1, .psh = 2, .phm = 3
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = true, .is_mfm = true
};

/* C128 CP/M */
const cpm_diskdef_t cpm_c128 = {
    .name = "c128",
    .description = "Commodore 128 CP/M",
    .cylinders = 80, .heads = 2, .sectors = 9, .sector_size = 512,
    .dpb = {
        .spt = 36, .bsh = 4, .blm = 15, .exm = 0,
        .dsm = 354, .drm = 127, .al0 = 0xC0, .al1 = 0x00,
        .cks = 32, .off = 2, .psh = 2, .phm = 3
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = true
};

/* MSX CP/M */
const cpm_diskdef_t cpm_msx = {
    .name = "msx",
    .description = "MSX CP/M",
    .cylinders = 80, .heads = 2, .sectors = 9, .sector_size = 512,
    .dpb = {
        .spt = 36, .bsh = 4, .blm = 15, .exm = 0,
        .dsm = 350, .drm = 127, .al0 = 0xC0, .al1 = 0x00,
        .cks = 32, .off = 3, .psh = 2, .phm = 3
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = true
};

/* Sanyo MBC */
const cpm_diskdef_t cpm_sanyo = {
    .name = "sanyo",
    .description = "Sanyo MBC-550/555",
    .cylinders = 40, .heads = 2, .sectors = 8, .sector_size = 512,
    .dpb = {
        .spt = 32, .bsh = 3, .blm = 7, .exm = 0,
        .dsm = 155, .drm = 63, .al0 = 0xC0, .al1 = 0x00,
        .cks = 16, .off = 1, .psh = 2, .phm = 3
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = true
};

/* Eagle II */
const cpm_diskdef_t cpm_eagle = {
    .name = "eagle",
    .description = "Eagle II",
    .cylinders = 35, .heads = 2, .sectors = 17, .sector_size = 256,
    .dpb = {
        .spt = 34, .bsh = 3, .blm = 7, .exm = 0,
        .dsm = 143, .drm = 63, .al0 = 0xC0, .al1 = 0x00,
        .cks = 16, .off = 4, .psh = 1, .phm = 1
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = true
};

/* Lobo Max-80 */
const cpm_diskdef_t cpm_lobo = {
    .name = "lobo",
    .description = "Lobo Max-80",
    .cylinders = 40, .heads = 2, .sectors = 10, .sector_size = 512,
    .dpb = {
        .spt = 40, .bsh = 4, .blm = 15, .exm = 1,
        .dsm = 194, .drm = 127, .al0 = 0xC0, .al1 = 0x00,
        .cks = 32, .off = 2, .psh = 2, .phm = 3
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = true
};

/* SOL-20 */
const cpm_diskdef_t cpm_sol20 = {
    .name = "sol20",
    .description = "SOL-20/Processor Technology",
    .cylinders = 35, .heads = 1, .sectors = 10, .sector_size = 256,
    .dpb = {
        .spt = 10, .bsh = 3, .blm = 7, .exm = 0,
        .dsm = 83, .drm = 31, .al0 = 0x80, .al1 = 0x00,
        .cks = 8, .off = 3, .psh = 1, .phm = 1
    },
    .first_sector = 0, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = false,
    .high_density = false, .is_mfm = true
};

/* Actrix Access */
const cpm_diskdef_t cpm_actrix = {
    .name = "actrix",
    .description = "Actrix Access Computer",
    .cylinders = 40, .heads = 1, .sectors = 10, .sector_size = 512,
    .dpb = {
        .spt = 20, .bsh = 4, .blm = 15, .exm = 1,
        .dsm = 98, .drm = 63, .al0 = 0x80, .al1 = 0x00,
        .cks = 16, .off = 1, .psh = 2, .phm = 3
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = false,
    .high_density = false, .is_mfm = true
};

/* Advance 86 */
const cpm_diskdef_t cpm_advance86 = {
    .name = "advance86",
    .description = "Advance 86",
    .cylinders = 40, .heads = 2, .sectors = 8, .sector_size = 512,
    .dpb = {
        .spt = 32, .bsh = 3, .blm = 7, .exm = 0,
        .dsm = 157, .drm = 63, .al0 = 0xC0, .al1 = 0x00,
        .cks = 16, .off = 1, .psh = 2, .phm = 3
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = true
};

/* BigBoard I */
const cpm_diskdef_t cpm_bigboard = {
    .name = "bigboard",
    .description = "BigBoard I",
    .cylinders = 77, .heads = 1, .sectors = 26, .sector_size = 128,
    .dpb = {
        .spt = 26, .bsh = 3, .blm = 7, .exm = 0,
        .dsm = 242, .drm = 63, .al0 = 0xC0, .al1 = 0x00,
        .cks = 16, .off = 2, .psh = 0, .phm = 0
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = false,
    .high_density = false, .is_mfm = false
};

/* BigBoard II */
const cpm_diskdef_t cpm_bigboard2 = {
    .name = "bigboard2",
    .description = "BigBoard II",
    .cylinders = 80, .heads = 2, .sectors = 5, .sector_size = 1024,
    .dpb = {
        .spt = 40, .bsh = 4, .blm = 15, .exm = 0,
        .dsm = 394, .drm = 255, .al0 = 0xF0, .al1 = 0x00,
        .cks = 64, .off = 2, .psh = 3, .phm = 7
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = true
};

/* Ithaca DPS-1 */
const cpm_diskdef_t cpm_dps1 = {
    .name = "dps1",
    .description = "Ithaca DPS-1",
    .cylinders = 40, .heads = 2, .sectors = 8, .sector_size = 512,
    .dpb = {
        .spt = 32, .bsh = 3, .blm = 7, .exm = 0,
        .dsm = 157, .drm = 63, .al0 = 0xC0, .al1 = 0x00,
        .cks = 16, .off = 2, .psh = 2, .phm = 3
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = true
};

/* Zenith Z-100 */
const cpm_diskdef_t cpm_z100 = {
    .name = "z100",
    .description = "Zenith Z-100",
    .cylinders = 40, .heads = 2, .sectors = 8, .sector_size = 512,
    .dpb = {
        .spt = 32, .bsh = 4, .blm = 15, .exm = 1,
        .dsm = 155, .drm = 63, .al0 = 0x80, .al1 = 0x00,
        .cks = 16, .off = 2, .psh = 2, .phm = 3
    },
    .first_sector = 0, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = true
};

/* PMC Micromate */
const cpm_diskdef_t cpm_micromate = {
    .name = "micromate",
    .description = "PMC Micromate",
    .cylinders = 40, .heads = 2, .sectors = 9, .sector_size = 512,
    .dpb = {
        .spt = 36, .bsh = 4, .blm = 15, .exm = 1,
        .dsm = 174, .drm = 63, .al0 = 0x80, .al1 = 0x00,
        .cks = 16, .off = 2, .psh = 2, .phm = 3
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = true
};

/* ============================================================================
 * Additional libdsk-derived definitions
 * ============================================================================ */

/* IMSAI VIO */
const cpm_diskdef_t cpm_imsai = {
    .name = "imsai",
    .description = "IMSAI VIO",
    .cylinders = 77, .heads = 1, .sectors = 32, .sector_size = 128,
    .dpb = {
        .spt = 32, .bsh = 3, .blm = 7, .exm = 0,
        .dsm = 299, .drm = 63, .al0 = 0xC0, .al1 = 0x00,
        .cks = 16, .off = 2, .psh = 0, .phm = 0
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = false,
    .high_density = false, .is_mfm = false
};

/* Morrow Micro Decision MD3 */
const cpm_diskdef_t cpm_morrow_md3 = {
    .name = "morrow-md3",
    .description = "Morrow Micro Decision MD3",
    .cylinders = 80, .heads = 2, .sectors = 5, .sector_size = 1024,
    .dpb = {
        .spt = 40, .bsh = 4, .blm = 15, .exm = 1,
        .dsm = 394, .drm = 127, .al0 = 0xC0, .al1 = 0x00,
        .cks = 32, .off = 2, .psh = 3, .phm = 7
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = true
};

/* Microbee */
const cpm_diskdef_t cpm_microbee = {
    .name = "microbee",
    .description = "Microbee CP/M",
    .cylinders = 40, .heads = 2, .sectors = 10, .sector_size = 512,
    .dpb = {
        .spt = 40, .bsh = 4, .blm = 15, .exm = 0,
        .dsm = 194, .drm = 63, .al0 = 0xC0, .al1 = 0x00,
        .cks = 16, .off = 2, .psh = 2, .phm = 3
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = true
};

/* Commodore 128 CP/M */
const cpm_diskdef_t cpm_c128_dsdd = {
    .name = "c128-dd",
    .description = "Commodore 128 CP/M DSDD",
    .cylinders = 80, .heads = 2, .sectors = 9, .sector_size = 512,
    .dpb = {
        .spt = 36, .bsh = 4, .blm = 15, .exm = 0,
        .dsm = 354, .drm = 127, .al0 = 0xC0, .al1 = 0x00,
        .cks = 32, .off = 2, .psh = 2, .phm = 3
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = true
};

/* Amstrad PCW 720K */
const cpm_diskdef_t cpm_pcw720 = {
    .name = "pcw720",
    .description = "Amstrad PCW 720K",
    .cylinders = 80, .heads = 2, .sectors = 9, .sector_size = 512,
    .dpb = {
        .spt = 36, .bsh = 4, .blm = 15, .exm = 0,
        .dsm = 354, .drm = 255, .al0 = 0xF0, .al1 = 0x00,
        .cks = 64, .off = 1, .psh = 2, .phm = 3
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = true
};

/* Tatung Einstein */
const cpm_diskdef_t cpm_einstein = {
    .name = "einstein",
    .description = "Tatung Einstein",
    .cylinders = 40, .heads = 1, .sectors = 10, .sector_size = 512,
    .dpb = {
        .spt = 20, .bsh = 3, .blm = 7, .exm = 0,
        .dsm = 92, .drm = 63, .al0 = 0xC0, .al1 = 0x00,
        .cks = 16, .off = 4, .psh = 2, .phm = 3
    },
    .first_sector = 0, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = false,
    .high_density = false, .is_mfm = true
};

/* Gemini */
const cpm_diskdef_t cpm_gemini = {
    .name = "gemini",
    .description = "Gemini Galaxy",
    .cylinders = 40, .heads = 2, .sectors = 9, .sector_size = 512,
    .dpb = {
        .spt = 36, .bsh = 4, .blm = 15, .exm = 1,
        .dsm = 174, .drm = 63, .al0 = 0x80, .al1 = 0x00,
        .cks = 16, .off = 2, .psh = 2, .phm = 3
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = true
};

/* RML 380Z */
const cpm_diskdef_t cpm_rml380z = {
    .name = "rml380z",
    .description = "Research Machines 380Z",
    .cylinders = 40, .heads = 1, .sectors = 10, .sector_size = 512,
    .dpb = {
        .spt = 20, .bsh = 4, .blm = 15, .exm = 1,
        .dsm = 98, .drm = 63, .al0 = 0x80, .al1 = 0x00,
        .cks = 16, .off = 3, .psh = 2, .phm = 3
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = false,
    .high_density = false, .is_mfm = true
};

/* Superbrain QD */
const cpm_diskdef_t cpm_superbrain_qd = {
    .name = "superbrain-qd",
    .description = "Superbrain QD (Quad Density)",
    .cylinders = 80, .heads = 2, .sectors = 10, .sector_size = 512,
    .dpb = {
        .spt = 40, .bsh = 4, .blm = 15, .exm = 0,
        .dsm = 394, .drm = 127, .al0 = 0xC0, .al1 = 0x00,
        .cks = 32, .off = 2, .psh = 2, .phm = 3
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = true
};

/* Wang Professional */
const cpm_diskdef_t cpm_wang = {
    .name = "wang",
    .description = "Wang Professional Computer",
    .cylinders = 80, .heads = 2, .sectors = 8, .sector_size = 512,
    .dpb = {
        .spt = 32, .bsh = 4, .blm = 15, .exm = 0,
        .dsm = 314, .drm = 127, .al0 = 0xC0, .al1 = 0x00,
        .cks = 32, .off = 2, .psh = 2, .phm = 3
    },
    .first_sector = 0, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = true
};

/* Torch CPN */
const cpm_diskdef_t cpm_torch = {
    .name = "torch",
    .description = "Torch CPN",
    .cylinders = 80, .heads = 1, .sectors = 10, .sector_size = 512,
    .dpb = {
        .spt = 20, .bsh = 4, .blm = 15, .exm = 0,
        .dsm = 194, .drm = 63, .al0 = 0xC0, .al1 = 0x00,
        .cks = 16, .off = 1, .psh = 2, .phm = 3
    },
    .first_sector = 0, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = false,
    .high_density = false, .is_mfm = true
};

/* Jonos Escort */
const cpm_diskdef_t cpm_jonos = {
    .name = "jonos",
    .description = "Jonos Escort",
    .cylinders = 40, .heads = 2, .sectors = 10, .sector_size = 512,
    .dpb = {
        .spt = 40, .bsh = 4, .blm = 15, .exm = 0,
        .dsm = 195, .drm = 127, .al0 = 0xC0, .al1 = 0x00,
        .cks = 32, .off = 1, .psh = 2, .phm = 3
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = true
};

/* Otrona Attache */
const cpm_diskdef_t cpm_otrona = {
    .name = "otrona",
    .description = "Otrona Attache",
    .cylinders = 40, .heads = 2, .sectors = 10, .sector_size = 512,
    .dpb = {
        .spt = 40, .bsh = 4, .blm = 15, .exm = 0,
        .dsm = 194, .drm = 63, .al0 = 0x80, .al1 = 0x00,
        .cks = 16, .off = 2, .psh = 2, .phm = 3
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = true
};

/* NEC APC */
const cpm_diskdef_t cpm_nec_apc = {
    .name = "nec-apc",
    .description = "NEC Advanced Personal Computer",
    .cylinders = 77, .heads = 2, .sectors = 8, .sector_size = 1024,
    .dpb = {
        .spt = 64, .bsh = 5, .blm = 31, .exm = 3,
        .dsm = 299, .drm = 255, .al0 = 0xC0, .al1 = 0x00,
        .cks = 64, .off = 2, .psh = 4, .phm = 15
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = true, .is_mfm = true
};

/* Sord M23 */
const cpm_diskdef_t cpm_sord = {
    .name = "sord",
    .description = "Sord M23",
    .cylinders = 40, .heads = 2, .sectors = 16, .sector_size = 256,
    .dpb = {
        .spt = 64, .bsh = 4, .blm = 15, .exm = 0,
        .dsm = 155, .drm = 63, .al0 = 0xC0, .al1 = 0x00,
        .cks = 16, .off = 2, .psh = 1, .phm = 1
    },
    .first_sector = 1, .skew_mode = CPM_SKEW_NONE, .skew_table = NULL,
    .boot_mode = CPM_BOOT_STANDARD, .double_sided = true,
    .high_density = false, .is_mfm = true
};

/* ============================================================================
 * Format Table
 * ============================================================================ */

static const cpm_diskdef_t *all_diskdefs[] = {
    &cpm_ibm_8ss,
    &cpm_ibm_8ds,
    &cpm_amstrad_cpc_data,
    &cpm_amstrad_cpc_system,
    &cpm_amstrad_pcw_cf2,
    &cpm_kaypro_ii,
    &cpm_kaypro_4,
    &cpm_osborne_1,
    &cpm_osborne_exec,
    &cpm_xerox_820,
    &cpm_morrow_md2,
    &cpm_epson_qx10,
    &cpm_nec_pc8801,
    &cpm_sharp_mz80b,
    &cpm_zorba,
    /* Extended definitions */
    &cpm_trs80_m4,
    &cpm_vector,
    &cpm_superbrain,
    &cpm_televideo,
    &cpm_ampro,
    &cpm_northstar,
    &cpm_cromemco,
    &cpm_altos,
    &cpm_rainbow,
    &cpm_bondwell,
    &cpm_pc360,
    &cpm_pc1200,
    &cpm_c128,
    &cpm_msx,
    &cpm_sanyo,
    &cpm_eagle,
    &cpm_lobo,
    &cpm_sol20,
    /* Additional definitions */
    &cpm_actrix,
    &cpm_advance86,
    &cpm_bigboard,
    &cpm_bigboard2,
    &cpm_dps1,
    &cpm_z100,
    &cpm_micromate,
    /* libdsk-derived definitions */
    &cpm_imsai,
    &cpm_morrow_md3,
    &cpm_microbee,
    &cpm_c128_dsdd,
    &cpm_pcw720,
    &cpm_einstein,
    &cpm_gemini,
    &cpm_rml380z,
    &cpm_superbrain_qd,
    &cpm_wang,
    &cpm_torch,
    &cpm_jonos,
    &cpm_otrona,
    &cpm_nec_apc,
    &cpm_sord,
    NULL
};

#define NUM_DISKDEFS (sizeof(all_diskdefs) / sizeof(all_diskdefs[0]) - 1)

/* ============================================================================
 * CP/M Disk Definition Functions
 * ============================================================================ */

const cpm_diskdef_t* cpm_get_diskdef(const char *name) {
    if (!name) return NULL;
    
    for (size_t i = 0; i < NUM_DISKDEFS; i++) {
        if (strcmp(all_diskdefs[i]->name, name) == 0) {
            return all_diskdefs[i];
        }
    }
    return NULL;
}

const cpm_diskdef_t* cpm_get_diskdef_by_index(size_t index) {
    if (index >= NUM_DISKDEFS) return NULL;
    return all_diskdefs[index];
}

size_t cpm_get_diskdef_count(void) {
    return NUM_DISKDEFS;
}

size_t cpm_list_diskdefs(const char **names, size_t max_count) {
    size_t count = 0;
    for (size_t i = 0; i < NUM_DISKDEFS && count < max_count; i++) {
        names[count++] = all_diskdefs[i]->name;
    }
    return count;
}

const cpm_diskdef_t* cpm_detect_format_by_disk(const uft_disk_image_t *disk) {
    if (!disk) return NULL;
    
    /* Try to match geometry */
    for (size_t i = 0; i < NUM_DISKDEFS; i++) {
        const cpm_diskdef_t *def = all_diskdefs[i];
        
        if (disk->tracks == def->cylinders &&
            disk->heads == def->heads &&
            disk->sectors_per_track == def->sectors &&
            disk->bytes_per_sector == def->sector_size) {
            return def;
        }
    }
    
    return NULL;
}

uint8_t cpm_physical_to_logical(const cpm_diskdef_t *def, uint8_t physical) {
    if (!def || !def->skew_table) return physical;
    
    for (int i = 0; i < def->sectors; i++) {
        if (def->skew_table[i] == physical) {
            return i;
        }
    }
    return physical;
}

uint8_t cpm_logical_to_physical(const cpm_diskdef_t *def, uint8_t logical) {
    if (!def || !def->skew_table || logical >= def->sectors) {
        return logical;
    }
    return def->skew_table[logical];
}

size_t cpm_block_size(const cpm_dpb_t *dpb) {
    if (!dpb) return 0;
    return 128 << dpb->bsh;
}

size_t cpm_disk_capacity(const cpm_diskdef_t *def) {
    if (!def) return 0;
    return (size_t)def->cylinders * def->heads * def->sectors * def->sector_size;
}

size_t cpm_directory_size(const cpm_diskdef_t *def) {
    if (!def) return 0;
    return ((size_t)def->dpb.drm + 1) * 32;
}

/* ============================================================================
 * CP/M File I/O with definitions
 * ============================================================================ */

static uint8_t code_from_size(uint16_t size) {
    switch (size) {
        case 128:  return 0;
        case 256:  return 1;
        case 512:  return 2;
        case 1024: return 3;
        default:   return 2;
    }
}

uft_error_t uft_cpm_read_with_def(const char *path,
                                  const cpm_diskdef_t *def,
                                  uft_disk_image_t **out_disk) {
    if (!path || !def || !out_disk) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return UFT_ERR_IO;
    }
    
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(fp);
        return UFT_ERR_MEMORY;
    }
    
    if (fread(data, 1, size, fp) != size) {
        free(data);
        fclose(fp);
        return UFT_ERR_IO;
    }
    
    fclose(fp);
    
    /* Allocate disk image */
    uft_disk_image_t *disk = uft_disk_alloc(def->cylinders, def->heads);
    if (!disk) {
        free(data);
        return UFT_ERR_MEMORY;
    }
    
    disk->format = UFT_FMT_RAW;
    snprintf(disk->format_name, sizeof(disk->format_name), "CP/M-%s", def->name);
    disk->sectors_per_track = def->sectors;
    disk->bytes_per_sector = def->sector_size;
    
    /* Read track data */
    size_t data_pos = 0;
    uint8_t size_code = code_from_size(def->sector_size);
    uft_encoding_t encoding = def->is_mfm ? UFT_ENC_MFM : UFT_ENC_FM;
    
    for (uint8_t c = 0; c < def->cylinders; c++) {
        for (uint8_t h = 0; h < def->heads; h++) {
            size_t idx = c * def->heads + h;
            
            uft_track_t *track = uft_track_alloc(def->sectors, 0);
            if (!track) {
                uft_disk_free(disk);
                free(data);
                return UFT_ERR_MEMORY;
            }
            
            track->cylinder = c;
            track->head = h;
            track->encoding = encoding;
            
            for (uint8_t s = 0; s < def->sectors; s++) {
                /* Apply sector skew if needed */
                uint8_t phys_sector = s;
                if (def->skew_table) {
                    phys_sector = cpm_logical_to_physical(def, s);
                }
                
                uft_sector_t *sect = &track->sectors[s];
                sect->id.cylinder = c;
                sect->id.head = h;
                sect->id.sector = def->first_sector + phys_sector;
                sect->id.size_code = size_code;
                sect->status = UFT_SECTOR_OK;
                
                sect->data = malloc(def->sector_size);
                sect->data_size = def->sector_size;
                
                if (sect->data) {
                    if (data_pos + def->sector_size <= size) {
                        memcpy(sect->data, data + data_pos, def->sector_size);
                    } else {
                        memset(sect->data, 0xE5, def->sector_size);
                    }
                }
                data_pos += def->sector_size;
                
                track->sector_count++;
            }
            
            disk->track_data[idx] = track;
        }
    }
    
    free(data);
    *out_disk = disk;
    return UFT_OK;
}

uft_error_t uft_cpm_write_with_def(const uft_disk_image_t *disk,
                                   const char *path,
                                   const cpm_diskdef_t *def) {
    if (!disk || !path || !def) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    size_t total_size = cpm_disk_capacity(def);
    uint8_t *output = malloc(total_size);
    if (!output) {
        return UFT_ERR_MEMORY;
    }
    
    memset(output, 0xE5, total_size);
    
    /* Write track data */
    size_t data_pos = 0;
    
    for (uint16_t c = 0; c < def->cylinders; c++) {
        for (uint8_t h = 0; h < def->heads; h++) {
            size_t idx = c * def->heads + h;
            uft_track_t *track = (idx < (size_t)(disk->tracks * disk->heads)) ?
                                  disk->track_data[idx] : NULL;
            
            for (uint8_t s = 0; s < def->sectors; s++) {
                if (track && s < track->sector_count && track->sectors[s].data) {
                    memcpy(output + data_pos, track->sectors[s].data,
                           def->sector_size);
                }
                data_pos += def->sector_size;
            }
        }
    }
    
    /* Write file */
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        free(output);
        return UFT_ERR_IO;
    }
    
    size_t written = fwrite(output, 1, total_size, fp);
    fclose(fp);
    free(output);
    
    return (written == total_size) ? UFT_OK : UFT_ERR_IO;
}

uft_error_t uft_cpm_format(uft_disk_image_t **out_disk,
                           const cpm_diskdef_t *def,
                           uint8_t directory_fill) {
    if (!out_disk || !def) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    uft_disk_image_t *disk = uft_disk_alloc(def->cylinders, def->heads);
    if (!disk) {
        return UFT_ERR_MEMORY;
    }
    
    disk->format = UFT_FMT_RAW;
    snprintf(disk->format_name, sizeof(disk->format_name), "CP/M-%s", def->name);
    disk->sectors_per_track = def->sectors;
    disk->bytes_per_sector = def->sector_size;
    
    uint8_t size_code = code_from_size(def->sector_size);
    uft_encoding_t encoding = def->is_mfm ? UFT_ENC_MFM : UFT_ENC_FM;
    
    /* Calculate directory location */
    size_t dir_sectors = cpm_directory_size(def) / def->sector_size;
    size_t boot_sectors = (size_t)def->dpb.off * def->sectors;
    
    for (uint8_t c = 0; c < def->cylinders; c++) {
        for (uint8_t h = 0; h < def->heads; h++) {
            size_t idx = c * def->heads + h;
            size_t track_start_sector = idx * def->sectors;
            
            uft_track_t *track = uft_track_alloc(def->sectors, 0);
            if (!track) {
                uft_disk_free(disk);
                return UFT_ERR_MEMORY;
            }
            
            track->cylinder = c;
            track->head = h;
            track->encoding = encoding;
            
            for (uint8_t s = 0; s < def->sectors; s++) {
                uft_sector_t *sect = &track->sectors[s];
                sect->id.cylinder = c;
                sect->id.head = h;
                sect->id.sector = def->first_sector + s;
                sect->id.size_code = size_code;
                sect->status = UFT_SECTOR_OK;
                
                sect->data = malloc(def->sector_size);
                sect->data_size = def->sector_size;
                
                if (sect->data) {
                    size_t abs_sector = track_start_sector + s;
                    
                    if (abs_sector >= boot_sectors && 
                        abs_sector < boot_sectors + dir_sectors) {
                        /* Directory sector */
                        memset(sect->data, directory_fill, def->sector_size);
                    } else {
                        /* Data sector */
                        memset(sect->data, 0xE5, def->sector_size);
                    }
                }
                
                track->sector_count++;
            }
            
            disk->track_data[idx] = track;
        }
    }
    
    *out_disk = disk;
    return UFT_OK;
}
