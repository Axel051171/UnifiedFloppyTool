/**
 * @file uft_cpm_defs.c
 * @brief CP/M disk format definitions implementation
 * @version 3.9.0
 * 
 * Standard CP/M disk format definitions derived from libdsk diskdefs.
 * Reference: libdsk diskdefs file by John Elliott
 */

#include "uft/formats/uft_cpm_defs.h"
#include <string.h>
#include <stdlib.h>

/* ============================================================================
 * 8" Standard Formats
 * ============================================================================ */

/* IBM 8" SS SD - Standard CP/M 2.2 format */
const cpm_format_def_t cpm_ibm_8_sssd = {
    .name = "ibm-8-sssd",
    .description = "IBM 8\" SS SD (250K)",
    .cylinders = 77,
    .heads = 1,
    .sectors = 26,
    .sector_size = 128,
    .first_sector = 1,
    .mfm = false,
    .double_step = false,
    .dpb = {
        .spt = 26,
        .bsh = CPM_BSH_1K,
        .blm = 7,
        .exm = 0,
        .dsm = 242,
        .drm = 63,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 16,
        .off = 2,
        .psh = 0,
        .phm = 0,
    },
    .skew_type = CPM_SKEW_6_1,
    .skew_table = NULL,
    .boot_tracks = 2,
};

/* IBM 8" SS DD */
const cpm_format_def_t cpm_ibm_8_ssdd = {
    .name = "ibm-8-ssdd",
    .description = "IBM 8\" SS DD (500K)",
    .cylinders = 77,
    .heads = 1,
    .sectors = 26,
    .sector_size = 256,
    .first_sector = 1,
    .mfm = true,
    .double_step = false,
    .dpb = {
        .spt = 52,
        .bsh = CPM_BSH_2K,
        .blm = 15,
        .exm = 1,
        .dsm = 242,
        .drm = 127,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 32,
        .off = 2,
        .psh = 1,
        .phm = 1,
    },
    .skew_type = CPM_SKEW_6_1,
    .skew_table = NULL,
    .boot_tracks = 2,
};

/* IBM 8" DS DD */
const cpm_format_def_t cpm_ibm_8_dsdd = {
    .name = "ibm-8-dsdd",
    .description = "IBM 8\" DS DD (1M)",
    .cylinders = 77,
    .heads = 2,
    .sectors = 26,
    .sector_size = 256,
    .first_sector = 1,
    .mfm = true,
    .double_step = false,
    .dpb = {
        .spt = 52,
        .bsh = CPM_BSH_2K,
        .blm = 15,
        .exm = 0,
        .dsm = 493,
        .drm = 255,
        .al0 = 0xF0,
        .al1 = 0x00,
        .cks = 64,
        .off = 2,
        .psh = 1,
        .phm = 1,
    },
    .skew_type = CPM_SKEW_6_1,
    .skew_table = NULL,
    .boot_tracks = 2,
};

/* ============================================================================
 * 5.25" DD Formats
 * ============================================================================ */

/* IBM 5.25" SS DD - 160K */
const cpm_format_def_t cpm_ibm_525_ssdd = {
    .name = "ibm-525-ssdd",
    .description = "IBM 5.25\" SS DD (160K)",
    .cylinders = 40,
    .heads = 1,
    .sectors = 8,
    .sector_size = 512,
    .first_sector = 1,
    .mfm = true,
    .double_step = false,
    .dpb = {
        .spt = 32,
        .bsh = CPM_BSH_1K,
        .blm = 7,
        .exm = 0,
        .dsm = 155,
        .drm = 63,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 16,
        .off = 1,
        .psh = 2,
        .phm = 3,
    },
    .skew_type = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_tracks = 1,
};

/* IBM 5.25" DS DD - 360K */
const cpm_format_def_t cpm_ibm_525_dsdd = {
    .name = "ibm-525-dsdd",
    .description = "IBM 5.25\" DS DD (360K)",
    .cylinders = 40,
    .heads = 2,
    .sectors = 9,
    .sector_size = 512,
    .first_sector = 1,
    .mfm = true,
    .double_step = false,
    .dpb = {
        .spt = 36,
        .bsh = CPM_BSH_2K,
        .blm = 15,
        .exm = 1,
        .dsm = 170,
        .drm = 63,
        .al0 = 0x80,
        .al1 = 0x00,
        .cks = 16,
        .off = 1,
        .psh = 2,
        .phm = 3,
    },
    .skew_type = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_tracks = 1,
};

/* IBM 5.25" DS QD (96tpi) - 720K */
const cpm_format_def_t cpm_ibm_525_dsqd = {
    .name = "ibm-525-dsqd",
    .description = "IBM 5.25\" DS QD 96tpi (720K)",
    .cylinders = 80,
    .heads = 2,
    .sectors = 9,
    .sector_size = 512,
    .first_sector = 1,
    .mfm = true,
    .double_step = false,
    .dpb = {
        .spt = 36,
        .bsh = CPM_BSH_2K,
        .blm = 15,
        .exm = 1,
        .dsm = 350,
        .drm = 127,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 32,
        .off = 2,
        .psh = 2,
        .phm = 3,
    },
    .skew_type = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_tracks = 2,
};

/* ============================================================================
 * 3.5" Formats
 * ============================================================================ */

/* IBM 3.5" DS DD - 720K */
const cpm_format_def_t cpm_ibm_35_dsdd = {
    .name = "ibm-35-dsdd",
    .description = "IBM 3.5\" DS DD (720K)",
    .cylinders = 80,
    .heads = 2,
    .sectors = 9,
    .sector_size = 512,
    .first_sector = 1,
    .mfm = true,
    .double_step = false,
    .dpb = {
        .spt = 36,
        .bsh = CPM_BSH_2K,
        .blm = 15,
        .exm = 1,
        .dsm = 350,
        .drm = 127,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 32,
        .off = 2,
        .psh = 2,
        .phm = 3,
    },
    .skew_type = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_tracks = 2,
};

/* IBM 3.5" DS HD - 1.44M */
const cpm_format_def_t cpm_ibm_35_dshd = {
    .name = "ibm-35-dshd",
    .description = "IBM 3.5\" DS HD (1.44M)",
    .cylinders = 80,
    .heads = 2,
    .sectors = 18,
    .sector_size = 512,
    .first_sector = 1,
    .mfm = true,
    .double_step = false,
    .dpb = {
        .spt = 72,
        .bsh = CPM_BSH_4K,
        .blm = 31,
        .exm = 1,
        .dsm = 350,
        .drm = 255,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 64,
        .off = 2,
        .psh = 2,
        .phm = 3,
    },
    .skew_type = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_tracks = 2,
};

/* ============================================================================
 * Amstrad Formats
 * ============================================================================ */

/* Amstrad PCW (CF2/CF2DD) */
const cpm_format_def_t cpm_amstrad_pcw = {
    .name = "amstrad-pcw",
    .description = "Amstrad PCW CF2 (173K)",
    .cylinders = 40,
    .heads = 1,
    .sectors = 9,
    .sector_size = 512,
    .first_sector = 1,
    .mfm = true,
    .double_step = false,
    .dpb = {
        .spt = 36,
        .bsh = CPM_BSH_1K,
        .blm = 7,
        .exm = 0,
        .dsm = 174,
        .drm = 63,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 16,
        .off = 1,
        .psh = 2,
        .phm = 3,
    },
    .skew_type = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_tracks = 1,
};

/* Amstrad CPC System format */
const cpm_format_def_t cpm_amstrad_cpc = {
    .name = "amstrad-cpc-system",
    .description = "Amstrad CPC System (178K)",
    .cylinders = 40,
    .heads = 1,
    .sectors = 9,
    .sector_size = 512,
    .first_sector = 0x41,  /* Amstrad starts at 0x41 */
    .mfm = true,
    .double_step = false,
    .dpb = {
        .spt = 36,
        .bsh = CPM_BSH_1K,
        .blm = 7,
        .exm = 0,
        .dsm = 170,
        .drm = 63,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 16,
        .off = 2,
        .psh = 2,
        .phm = 3,
    },
    .skew_type = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_tracks = 2,
};

/* Amstrad CPC Data format */
const cpm_format_def_t cpm_amstrad_data = {
    .name = "amstrad-cpc-data",
    .description = "Amstrad CPC Data (178K)",
    .cylinders = 40,
    .heads = 1,
    .sectors = 9,
    .sector_size = 512,
    .first_sector = 0xC1,  /* Data format starts at 0xC1 */
    .mfm = true,
    .double_step = false,
    .dpb = {
        .spt = 36,
        .bsh = CPM_BSH_1K,
        .blm = 7,
        .exm = 0,
        .dsm = 178,
        .drm = 63,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 16,
        .off = 0,
        .psh = 2,
        .phm = 3,
    },
    .skew_type = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_tracks = 0,
};

/* ============================================================================
 * Kaypro Formats
 * ============================================================================ */

/* Kaypro II (SS DD) */
const cpm_format_def_t cpm_kaypro_ii = {
    .name = "kaypro-ii",
    .description = "Kaypro II SS DD (191K)",
    .cylinders = 40,
    .heads = 1,
    .sectors = 10,
    .sector_size = 512,
    .first_sector = 0,
    .mfm = true,
    .double_step = false,
    .dpb = {
        .spt = 40,
        .bsh = CPM_BSH_2K,
        .blm = 15,
        .exm = 1,
        .dsm = 94,
        .drm = 63,
        .al0 = 0x80,
        .al1 = 0x00,
        .cks = 16,
        .off = 1,
        .psh = 2,
        .phm = 3,
    },
    .skew_type = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_tracks = 1,
};

/* Kaypro 4 (DS DD) */
const cpm_format_def_t cpm_kaypro_4 = {
    .name = "kaypro-4",
    .description = "Kaypro 4 DS DD (390K)",
    .cylinders = 40,
    .heads = 2,
    .sectors = 10,
    .sector_size = 512,
    .first_sector = 0,
    .mfm = true,
    .double_step = false,
    .dpb = {
        .spt = 40,
        .bsh = CPM_BSH_2K,
        .blm = 15,
        .exm = 1,
        .dsm = 194,
        .drm = 63,
        .al0 = 0x80,
        .al1 = 0x00,
        .cks = 16,
        .off = 1,
        .psh = 2,
        .phm = 3,
    },
    .skew_type = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_tracks = 1,
};

/* Kaypro 10 (DS QD) */
const cpm_format_def_t cpm_kaypro_10 = {
    .name = "kaypro-10",
    .description = "Kaypro 10 DS QD (784K)",
    .cylinders = 80,
    .heads = 2,
    .sectors = 10,
    .sector_size = 512,
    .first_sector = 0,
    .mfm = true,
    .double_step = false,
    .dpb = {
        .spt = 40,
        .bsh = CPM_BSH_2K,
        .blm = 15,
        .exm = 0,
        .dsm = 394,
        .drm = 127,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 32,
        .off = 1,
        .psh = 2,
        .phm = 3,
    },
    .skew_type = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_tracks = 1,
};

/* ============================================================================
 * Osborne Formats
 * ============================================================================ */

/* Osborne 1 (SS SD) */
const cpm_format_def_t cpm_osborne_1 = {
    .name = "osborne-1",
    .description = "Osborne 1 SS SD (92K)",
    .cylinders = 40,
    .heads = 1,
    .sectors = 10,
    .sector_size = 256,
    .first_sector = 1,
    .mfm = false,  /* FM encoding */
    .double_step = false,
    .dpb = {
        .spt = 20,
        .bsh = CPM_BSH_1K,
        .blm = 7,
        .exm = 0,
        .dsm = 45,
        .drm = 63,
        .al0 = 0x80,
        .al1 = 0x00,
        .cks = 16,
        .off = 3,
        .psh = 1,
        .phm = 1,
    },
    .skew_type = CPM_SKEW_2_1,
    .skew_table = NULL,
    .boot_tracks = 3,
};

/* Osborne DD */
const cpm_format_def_t cpm_osborne_dd = {
    .name = "osborne-dd",
    .description = "Osborne DD (185K)",
    .cylinders = 40,
    .heads = 1,
    .sectors = 5,
    .sector_size = 1024,
    .first_sector = 1,
    .mfm = true,
    .double_step = false,
    .dpb = {
        .spt = 20,
        .bsh = CPM_BSH_2K,
        .blm = 15,
        .exm = 1,
        .dsm = 91,
        .drm = 63,
        .al0 = 0x80,
        .al1 = 0x00,
        .cks = 16,
        .off = 3,
        .psh = 3,
        .phm = 7,
    },
    .skew_type = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_tracks = 3,
};

/* ============================================================================
 * Epson Formats
 * ============================================================================ */

/* Epson QX-10 */
const cpm_format_def_t cpm_epson_qx10 = {
    .name = "epson-qx10",
    .description = "Epson QX-10 DD (360K)",
    .cylinders = 40,
    .heads = 2,
    .sectors = 9,
    .sector_size = 512,
    .first_sector = 1,
    .mfm = true,
    .double_step = false,
    .dpb = {
        .spt = 36,
        .bsh = CPM_BSH_2K,
        .blm = 15,
        .exm = 1,
        .dsm = 174,
        .drm = 63,
        .al0 = 0x80,
        .al1 = 0x00,
        .cks = 16,
        .off = 2,
        .psh = 2,
        .phm = 3,
    },
    .skew_type = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_tracks = 2,
};

/* Epson PX-8 */
const cpm_format_def_t cpm_epson_px8 = {
    .name = "epson-px8",
    .description = "Epson PX-8 (280K)",
    .cylinders = 40,
    .heads = 2,
    .sectors = 8,
    .sector_size = 512,
    .first_sector = 1,
    .mfm = true,
    .double_step = false,
    .dpb = {
        .spt = 64,
        .bsh = CPM_BSH_2K,
        .blm = 15,
        .exm = 1,
        .dsm = 155,
        .drm = 63,
        .al0 = 0x80,
        .al1 = 0x00,
        .cks = 16,
        .off = 1,
        .psh = 2,
        .phm = 3,
    },
    .skew_type = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_tracks = 1,
};

/* ============================================================================
 * Morrow Formats
 * ============================================================================ */

/* Morrow MD2 */
const cpm_format_def_t cpm_morrow_md2 = {
    .name = "morrow-md2",
    .description = "Morrow MD2 SS DD (184K)",
    .cylinders = 40,
    .heads = 1,
    .sectors = 10,
    .sector_size = 512,
    .first_sector = 1,
    .mfm = true,
    .double_step = false,
    .dpb = {
        .spt = 40,
        .bsh = CPM_BSH_2K,
        .blm = 15,
        .exm = 1,
        .dsm = 91,
        .drm = 63,
        .al0 = 0x80,
        .al1 = 0x00,
        .cks = 16,
        .off = 2,
        .psh = 2,
        .phm = 3,
    },
    .skew_type = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_tracks = 2,
};

/* Morrow MD3 */
const cpm_format_def_t cpm_morrow_md3 = {
    .name = "morrow-md3",
    .description = "Morrow MD3 DS DD (384K)",
    .cylinders = 40,
    .heads = 2,
    .sectors = 10,
    .sector_size = 512,
    .first_sector = 1,
    .mfm = true,
    .double_step = false,
    .dpb = {
        .spt = 40,
        .bsh = CPM_BSH_2K,
        .blm = 15,
        .exm = 1,
        .dsm = 191,
        .drm = 127,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 32,
        .off = 2,
        .psh = 2,
        .phm = 3,
    },
    .skew_type = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_tracks = 2,
};

/* ============================================================================
 * Other Formats
 * ============================================================================ */

/* Bondwell */
const cpm_format_def_t cpm_bondwell = {
    .name = "bondwell",
    .description = "Bondwell (180K)",
    .cylinders = 40,
    .heads = 1,
    .sectors = 9,
    .sector_size = 512,
    .first_sector = 1,
    .mfm = true,
    .double_step = false,
    .dpb = {
        .spt = 36,
        .bsh = CPM_BSH_2K,
        .blm = 15,
        .exm = 1,
        .dsm = 89,
        .drm = 63,
        .al0 = 0x80,
        .al1 = 0x00,
        .cks = 16,
        .off = 1,
        .psh = 2,
        .phm = 3,
    },
    .skew_type = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_tracks = 1,
};

/* Sanyo MBC-55x */
const cpm_format_def_t cpm_sanyo_mbc55x = {
    .name = "sanyo-mbc55x",
    .description = "Sanyo MBC-55x (256K)",
    .cylinders = 70,
    .heads = 1,
    .sectors = 16,
    .sector_size = 256,
    .first_sector = 1,
    .mfm = true,
    .double_step = false,
    .dpb = {
        .spt = 64,
        .bsh = CPM_BSH_2K,
        .blm = 15,
        .exm = 0,
        .dsm = 137,
        .drm = 63,
        .al0 = 0x80,
        .al1 = 0x00,
        .cks = 16,
        .off = 2,
        .psh = 1,
        .phm = 1,
    },
    .skew_type = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_tracks = 2,
};

/* NEC PC-8801 */
const cpm_format_def_t cpm_nec_pc8801 = {
    .name = "nec-pc8801",
    .description = "NEC PC-8801 (320K)",
    .cylinders = 80,
    .heads = 1,
    .sectors = 16,
    .sector_size = 256,
    .first_sector = 1,
    .mfm = true,
    .double_step = false,
    .dpb = {
        .spt = 64,
        .bsh = CPM_BSH_2K,
        .blm = 15,
        .exm = 0,
        .dsm = 159,
        .drm = 127,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 32,
        .off = 2,
        .psh = 1,
        .phm = 1,
    },
    .skew_type = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_tracks = 2,
};

/* Intertec Superbrain */
const cpm_format_def_t cpm_superbrain = {
    .name = "superbrain",
    .description = "Superbrain SS DD (170K)",
    .cylinders = 35,
    .heads = 1,
    .sectors = 10,
    .sector_size = 512,
    .first_sector = 1,
    .mfm = true,
    .double_step = false,
    .dpb = {
        .spt = 40,
        .bsh = CPM_BSH_2K,
        .blm = 15,
        .exm = 1,
        .dsm = 84,
        .drm = 63,
        .al0 = 0x80,
        .al1 = 0x00,
        .cks = 16,
        .off = 1,
        .psh = 2,
        .phm = 3,
    },
    .skew_type = CPM_SKEW_2_1,
    .skew_table = NULL,
    .boot_tracks = 1,
};

/* Superbrain DD */
const cpm_format_def_t cpm_superbrain_dd = {
    .name = "superbrain-dd",
    .description = "Superbrain DS DD (350K)",
    .cylinders = 35,
    .heads = 2,
    .sectors = 10,
    .sector_size = 512,
    .first_sector = 1,
    .mfm = true,
    .double_step = false,
    .dpb = {
        .spt = 40,
        .bsh = CPM_BSH_2K,
        .blm = 15,
        .exm = 0,
        .dsm = 174,
        .drm = 127,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 32,
        .off = 1,
        .psh = 2,
        .phm = 3,
    },
    .skew_type = CPM_SKEW_2_1,
    .skew_table = NULL,
    .boot_tracks = 1,
};

/* Televideo 803 */
const cpm_format_def_t cpm_televideo_803 = {
    .name = "televideo-803",
    .description = "Televideo 803 (340K)",
    .cylinders = 40,
    .heads = 2,
    .sectors = 18,
    .sector_size = 256,
    .first_sector = 1,
    .mfm = true,
    .double_step = false,
    .dpb = {
        .spt = 36,
        .bsh = CPM_BSH_2K,
        .blm = 15,
        .exm = 0,
        .dsm = 169,
        .drm = 127,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 32,
        .off = 2,
        .psh = 1,
        .phm = 1,
    },
    .skew_type = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_tracks = 2,
};

/* Telcon Zorba */
const cpm_format_def_t cpm_zorba = {
    .name = "zorba",
    .description = "Telcon Zorba (392K)",
    .cylinders = 40,
    .heads = 2,
    .sectors = 10,
    .sector_size = 512,
    .first_sector = 1,
    .mfm = true,
    .double_step = false,
    .dpb = {
        .spt = 40,
        .bsh = CPM_BSH_2K,
        .blm = 15,
        .exm = 1,
        .dsm = 195,
        .drm = 127,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 32,
        .off = 1,
        .psh = 2,
        .phm = 3,
    },
    .skew_type = CPM_SKEW_NONE,
    .skew_table = NULL,
    .boot_tracks = 1,
};

/* ============================================================================
 * Format Registry
 * ============================================================================ */

static const cpm_format_def_t *cpm_all_formats[] = {
    /* 8" formats */
    &cpm_ibm_8_sssd,
    &cpm_ibm_8_ssdd,
    &cpm_ibm_8_dsdd,
    
    /* 5.25" formats */
    &cpm_ibm_525_ssdd,
    &cpm_ibm_525_dsdd,
    &cpm_ibm_525_dsqd,
    
    /* 3.5" formats */
    &cpm_ibm_35_dsdd,
    &cpm_ibm_35_dshd,
    
    /* Amstrad */
    &cpm_amstrad_pcw,
    &cpm_amstrad_cpc,
    &cpm_amstrad_data,
    
    /* Kaypro */
    &cpm_kaypro_ii,
    &cpm_kaypro_4,
    &cpm_kaypro_10,
    
    /* Osborne */
    &cpm_osborne_1,
    &cpm_osborne_dd,
    
    /* Epson */
    &cpm_epson_qx10,
    &cpm_epson_px8,
    
    /* Morrow */
    &cpm_morrow_md2,
    &cpm_morrow_md3,
    
    /* Others */
    &cpm_bondwell,
    &cpm_sanyo_mbc55x,
    &cpm_nec_pc8801,
    &cpm_superbrain,
    &cpm_superbrain_dd,
    &cpm_televideo_803,
    &cpm_zorba,
    
    NULL  /* Terminator */
};

const cpm_format_def_t** uft_cpm_get_all_formats(size_t *count) {
    if (count) {
        size_t n = 0;
        while (cpm_all_formats[n]) n++;
        *count = n;
    }
    return cpm_all_formats;
}

const cpm_format_def_t* uft_cpm_find_format(const char *name) {
    if (!name) return NULL;
    
    for (size_t i = 0; cpm_all_formats[i]; i++) {
        if (strcmp(cpm_all_formats[i]->name, name) == 0) {
            return cpm_all_formats[i];
        }
    }
    
    return NULL;
}

const cpm_format_def_t* uft_cpm_find_by_geometry(
    uint8_t cyls, uint8_t heads, uint8_t spt, uint16_t secsize) {
    
    for (size_t i = 0; cpm_all_formats[i]; i++) {
        const cpm_format_def_t *fmt = cpm_all_formats[i];
        if (fmt->cylinders == cyls &&
            fmt->heads == heads &&
            fmt->sectors == spt &&
            fmt->sector_size == secsize) {
            return fmt;
        }
    }
    
    return NULL;
}
