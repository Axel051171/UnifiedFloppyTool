/**
 * @file uft_preset_flashfloppy.h
 * @brief FlashFloppy-derived format presets
 *
 * Formats extracted from FlashFloppy (Public Domain)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef UFT_PRESET_FLASHFLOPPY_H
#define UFT_PRESET_FLASHFLOPPY_H

#include "uft_preset_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief FlashFloppy-derived format presets
 */
static const uft_format_preset_t UFT_PRESETS_FLASHFLOPPY[] = {
    /* ===== TI-99/4A Formats ===== */
    {
        .name = "TI99-SSSD",
        .description = "TI-99/4A Single-Sided Single-Density",
        .category = "TI-99/4A",
        .extension = "dsk",
        .cylinders = 40,
        .heads = 1,
        .sectors_per_track = 9,
        .sector_size = 256,
        .encoding = UFT_ENCODING_FM,
        .rpm = 300,
        .data_rate = 125,
        .gap3 = 44,
        .total_size = 92160,
        .flags = UFT_PRESET_FLAG_SEQUENTIAL
    },
    {
        .name = "TI99-DSSD",
        .description = "TI-99/4A Double-Sided Single-Density",
        .category = "TI-99/4A",
        .extension = "dsk",
        .cylinders = 40,
        .heads = 2,
        .sectors_per_track = 9,
        .sector_size = 256,
        .encoding = UFT_ENCODING_FM,
        .rpm = 300,
        .data_rate = 125,
        .gap3 = 44,
        .total_size = 184320,
        .flags = UFT_PRESET_FLAG_SEQUENTIAL
    },
    {
        .name = "TI99-SSDD",
        .description = "TI-99/4A Single-Sided Double-Density",
        .category = "TI-99/4A",
        .extension = "dsk",
        .cylinders = 40,
        .heads = 1,
        .sectors_per_track = 18,
        .sector_size = 256,
        .encoding = UFT_ENCODING_MFM,
        .rpm = 300,
        .data_rate = 250,
        .gap3 = 24,
        .total_size = 184320,
        .flags = UFT_PRESET_FLAG_SEQUENTIAL
    },
    {
        .name = "TI99-DSDD",
        .description = "TI-99/4A Double-Sided Double-Density",
        .category = "TI-99/4A",
        .extension = "dsk",
        .cylinders = 40,
        .heads = 2,
        .sectors_per_track = 18,
        .sector_size = 256,
        .encoding = UFT_ENCODING_MFM,
        .rpm = 300,
        .data_rate = 250,
        .gap3 = 24,
        .total_size = 368640,
        .flags = UFT_PRESET_FLAG_SEQUENTIAL
    },
    {
        .name = "TI99-DSDD80",
        .description = "TI-99/4A DS/DD 80-track (720KB)",
        .category = "TI-99/4A",
        .extension = "dsk",
        .cylinders = 80,
        .heads = 2,
        .sectors_per_track = 18,
        .sector_size = 256,
        .encoding = UFT_ENCODING_MFM,
        .rpm = 300,
        .data_rate = 250,
        .gap3 = 24,
        .total_size = 737280,
        .flags = UFT_PRESET_FLAG_SEQUENTIAL
    },

    /* ===== PC-98 Formats ===== */
    {
        .name = "PC98-2DD",
        .description = "PC-98 2DD (640KB)",
        .category = "PC-98",
        .extension = "fdi",
        .cylinders = 80,
        .heads = 2,
        .sectors_per_track = 8,
        .sector_size = 512,
        .encoding = UFT_ENCODING_MFM,
        .rpm = 300,
        .data_rate = 250,
        .gap3 = 84,
        .total_size = 655360,
        .flags = 0
    },
    {
        .name = "PC98-2HD-1.25M",
        .description = "PC-98 2HD (1.25MB)",
        .category = "PC-98",
        .extension = "hdm",
        .cylinders = 77,
        .heads = 2,
        .sectors_per_track = 8,
        .sector_size = 1024,
        .encoding = UFT_ENCODING_MFM,
        .rpm = 360,
        .data_rate = 500,
        .gap3 = 116,
        .total_size = 1261568,
        .flags = 0
    },
    {
        .name = "PC98-2HD-1.44M",
        .description = "PC-98 2HD (1.44MB)",
        .category = "PC-98",
        .extension = "hdm",
        .cylinders = 80,
        .heads = 2,
        .sectors_per_track = 18,
        .sector_size = 512,
        .encoding = UFT_ENCODING_MFM,
        .rpm = 300,
        .data_rate = 500,
        .gap3 = 84,
        .total_size = 1474560,
        .flags = 0
    },

    /* ===== MSX Formats ===== */
    {
        .name = "MSX-360K",
        .description = "MSX 360KB (80T/1H/9S)",
        .category = "MSX",
        .extension = "dsk",
        .cylinders = 80,
        .heads = 1,
        .sectors_per_track = 9,
        .sector_size = 512,
        .encoding = UFT_ENCODING_MFM,
        .rpm = 300,
        .data_rate = 250,
        .gap3 = 84,
        .total_size = 368640,
        .flags = 0
    },
    {
        .name = "MSX-720K",
        .description = "MSX 720KB (80T/2H/9S)",
        .category = "MSX",
        .extension = "dsk",
        .cylinders = 80,
        .heads = 2,
        .sectors_per_track = 9,
        .sector_size = 512,
        .encoding = UFT_ENCODING_MFM,
        .rpm = 300,
        .data_rate = 250,
        .gap3 = 84,
        .total_size = 737280,
        .flags = 0
    },

    /* ===== MGT / SAM Coupé ===== */
    {
        .name = "MGT",
        .description = "SAM Coupé / Spectrum +D",
        .category = "SAM Coupé",
        .extension = "mgt",
        .cylinders = 80,
        .heads = 2,
        .sectors_per_track = 10,
        .sector_size = 512,
        .encoding = UFT_ENCODING_MFM,
        .rpm = 300,
        .data_rate = 250,
        .gap3 = 36,
        .total_size = 819200,
        .flags = 0
    },
    {
        .name = "SAD",
        .description = "SAM Coupé SAD format",
        .category = "SAM Coupé",
        .extension = "sad",
        .cylinders = 80,
        .heads = 2,
        .sectors_per_track = 10,
        .sector_size = 512,
        .encoding = UFT_ENCODING_MFM,
        .rpm = 300,
        .data_rate = 250,
        .gap3 = 36,
        .total_size = 819222,  /* 819200 + 22 byte header */
        .flags = UFT_PRESET_FLAG_HAS_HEADER
    },

    /* ===== UKNC (Soviet) ===== */
    {
        .name = "UKNC",
        .description = "UKNC (Soviet PDP-11 clone)",
        .category = "Soviet",
        .extension = "img",
        .cylinders = 80,
        .heads = 2,
        .sectors_per_track = 10,
        .sector_size = 512,
        .encoding = UFT_ENCODING_MFM,
        .rpm = 300,
        .data_rate = 250,
        .gap3 = 36,
        .total_size = 819200,
        .flags = UFT_PRESET_FLAG_SPECIAL_SYNC
    },

    /* ===== End Marker ===== */
    { .name = NULL }
};

#define UFT_PRESET_FLASHFLOPPY_COUNT 14

#ifdef __cplusplus
}
#endif

#endif /* UFT_PRESET_FLASHFLOPPY_H */
