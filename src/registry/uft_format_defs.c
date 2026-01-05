/**
 * @file uft_format_defs.c
 * @brief Comprehensive Floppy Disk Format Definitions Implementation
 * 
 * @copyright UFT Project 2026
 */

#include "uft/registry/uft_format_defs.h"
#include <string.h>
#include <ctype.h>

/*============================================================================
 * Commodore Formats
 *============================================================================*/

const uft_format_def_t UFT_FMT_D64 = {
    .name = "D64",
    .description = "Commodore 1541 disk image",
    .extensions = "d64",
    .form_factor = UFT_FF_525,
    .variant = UFT_VAR_SSDD,
    .encoding = UFT_ENC_GCR5,
    .heads = 1,
    .tracks = 35,
    .sectors_min = 17,
    .sectors_max = 21,
    .sector_size = 256,
    .image_size = 174848,
    .cell_size = 3250,  /* Zone 3 */
    .rpm = 300,
    .interleave = 4,
    .skew = 0,
    .gap1 = 9, .gap2 = 8, .gap3 = 7, .gap4 = 0,
    .flags = UFT_FMT_ZONED | UFT_FMT_WRITE_SUPPORT | UFT_FMT_CONVERT
};

const uft_format_def_t UFT_FMT_D64_40 = {
    .name = "D64_40",
    .description = "Commodore 1541 extended (40 tracks)",
    .extensions = "d64",
    .form_factor = UFT_FF_525,
    .variant = UFT_VAR_SSDD,
    .encoding = UFT_ENC_GCR5,
    .heads = 1,
    .tracks = 40,
    .sectors_min = 17,
    .sectors_max = 21,
    .sector_size = 256,
    .image_size = 196608,
    .cell_size = 3250,
    .rpm = 300,
    .interleave = 4,
    .skew = 0,
    .gap1 = 9, .gap2 = 8, .gap3 = 7, .gap4 = 0,
    .flags = UFT_FMT_ZONED | UFT_FMT_WRITE_SUPPORT | UFT_FMT_CONVERT
};

const uft_format_def_t UFT_FMT_D71 = {
    .name = "D71",
    .description = "Commodore 1571 disk image",
    .extensions = "d71",
    .form_factor = UFT_FF_525,
    .variant = UFT_VAR_DSDD,
    .encoding = UFT_ENC_GCR5,
    .heads = 2,
    .tracks = 35,
    .sectors_min = 17,
    .sectors_max = 21,
    .sector_size = 256,
    .image_size = 349696,
    .cell_size = 3250,
    .rpm = 300,
    .interleave = 6,
    .skew = 0,
    .gap1 = 9, .gap2 = 8, .gap3 = 7, .gap4 = 0,
    .flags = UFT_FMT_ZONED | UFT_FMT_WRITE_SUPPORT | UFT_FMT_CONVERT
};

const uft_format_def_t UFT_FMT_D81 = {
    .name = "D81",
    .description = "Commodore 1581 disk image",
    .extensions = "d81",
    .form_factor = UFT_FF_35,
    .variant = UFT_VAR_DSDD,
    .encoding = UFT_ENC_MFM,
    .heads = 2,
    .tracks = 80,
    .sectors_min = 10,
    .sectors_max = 10,
    .sector_size = 512,
    .image_size = 819200,
    .cell_size = 2000,
    .rpm = 300,
    .interleave = 1,
    .skew = 0,
    .gap1 = 50, .gap2 = 22, .gap3 = 35, .gap4 = 0,
    .flags = UFT_FMT_WRITE_SUPPORT | UFT_FMT_CONVERT
};

const uft_format_def_t UFT_FMT_G64 = {
    .name = "G64",
    .description = "Commodore GCR raw track image",
    .extensions = "g64,g41",
    .form_factor = UFT_FF_525,
    .variant = UFT_VAR_SSDD,
    .encoding = UFT_ENC_GCR5,
    .heads = 1,
    .tracks = 42,
    .sectors_min = 17,
    .sectors_max = 21,
    .sector_size = 256,
    .image_size = 0,  /* Variable */
    .cell_size = 3250,
    .rpm = 300,
    .interleave = 0,
    .skew = 0,
    .gap1 = 0, .gap2 = 0, .gap3 = 0, .gap4 = 0,
    .flags = UFT_FMT_ZONED | UFT_FMT_RAW_TRACK | UFT_FMT_COPY_PROTECT | 
             UFT_FMT_WRITE_SUPPORT | UFT_FMT_CONVERT
};

const uft_format_def_t UFT_FMT_G71 = {
    .name = "G71",
    .description = "Commodore 1571 GCR raw track image",
    .extensions = "g71",
    .form_factor = UFT_FF_525,
    .variant = UFT_VAR_DSDD,
    .encoding = UFT_ENC_GCR5,
    .heads = 2,
    .tracks = 42,
    .sectors_min = 17,
    .sectors_max = 21,
    .sector_size = 256,
    .image_size = 0,
    .cell_size = 3250,
    .rpm = 300,
    .interleave = 0,
    .skew = 0,
    .gap1 = 0, .gap2 = 0, .gap3 = 0, .gap4 = 0,
    .flags = UFT_FMT_ZONED | UFT_FMT_RAW_TRACK | UFT_FMT_COPY_PROTECT |
             UFT_FMT_WRITE_SUPPORT
};

/*============================================================================
 * Apple II / Macintosh Formats
 *============================================================================*/

const uft_format_def_t UFT_FMT_DO = {
    .name = "DO",
    .description = "Apple II DOS 3.3 order",
    .extensions = "do,dsk",
    .form_factor = UFT_FF_525,
    .variant = UFT_VAR_SSDD,
    .encoding = UFT_ENC_GCR6,
    .heads = 1,
    .tracks = 35,
    .sectors_min = 16,
    .sectors_max = 16,
    .sector_size = 256,
    .image_size = 143360,
    .cell_size = 4000,
    .rpm = 300,
    .interleave = 0,
    .skew = 0,
    .gap1 = 0, .gap2 = 0, .gap3 = 0, .gap4 = 0,
    .flags = UFT_FMT_WRITE_SUPPORT | UFT_FMT_CONVERT
};

const uft_format_def_t UFT_FMT_PO = {
    .name = "PO",
    .description = "Apple II ProDOS order",
    .extensions = "po",
    .form_factor = UFT_FF_525,
    .variant = UFT_VAR_SSDD,
    .encoding = UFT_ENC_GCR6,
    .heads = 1,
    .tracks = 35,
    .sectors_min = 16,
    .sectors_max = 16,
    .sector_size = 256,
    .image_size = 143360,
    .cell_size = 4000,
    .rpm = 300,
    .interleave = 2,
    .skew = 0,
    .gap1 = 0, .gap2 = 0, .gap3 = 0, .gap4 = 0,
    .flags = UFT_FMT_WRITE_SUPPORT | UFT_FMT_CONVERT
};

const uft_format_def_t UFT_FMT_NIB = {
    .name = "NIB",
    .description = "Apple II nibble image",
    .extensions = "nib",
    .form_factor = UFT_FF_525,
    .variant = UFT_VAR_SSDD,
    .encoding = UFT_ENC_GCR6,
    .heads = 1,
    .tracks = 35,
    .sectors_min = 16,
    .sectors_max = 16,
    .sector_size = 256,
    .image_size = 232960,
    .cell_size = 4000,
    .rpm = 300,
    .interleave = 0,
    .skew = 0,
    .gap1 = 0, .gap2 = 0, .gap3 = 0, .gap4 = 0,
    .flags = UFT_FMT_RAW_TRACK | UFT_FMT_COPY_PROTECT | UFT_FMT_WRITE_SUPPORT
};

const uft_format_def_t UFT_FMT_WOZ = {
    .name = "WOZ",
    .description = "Apple II flux-level image",
    .extensions = "woz",
    .form_factor = UFT_FF_525,
    .variant = UFT_VAR_SSDD,
    .encoding = UFT_ENC_GCR6,
    .heads = 1,
    .tracks = 40,
    .sectors_min = 13,
    .sectors_max = 16,
    .sector_size = 256,
    .image_size = 0,
    .cell_size = 4000,
    .rpm = 300,
    .interleave = 0,
    .skew = 0,
    .gap1 = 0, .gap2 = 0, .gap3 = 0, .gap4 = 0,
    .flags = UFT_FMT_FLUX | UFT_FMT_WEAK_BITS | UFT_FMT_COPY_PROTECT |
             UFT_FMT_TIMING
};

/*============================================================================
 * Amiga Formats
 *============================================================================*/

const uft_format_def_t UFT_FMT_ADF = {
    .name = "ADF",
    .description = "Amiga Disk File (DD)",
    .extensions = "adf",
    .form_factor = UFT_FF_35,
    .variant = UFT_VAR_DSDD,
    .encoding = UFT_ENC_MFM,
    .heads = 2,
    .tracks = 80,
    .sectors_min = 11,
    .sectors_max = 11,
    .sector_size = 512,
    .image_size = 901120,
    .cell_size = 2000,
    .rpm = 300,
    .interleave = 0,
    .skew = 0,
    .gap1 = 0, .gap2 = 0, .gap3 = 0, .gap4 = 0,
    .flags = UFT_FMT_WRITE_SUPPORT | UFT_FMT_CONVERT
};

const uft_format_def_t UFT_FMT_ADF_HD = {
    .name = "ADF_HD",
    .description = "Amiga Disk File (HD)",
    .extensions = "adf",
    .form_factor = UFT_FF_35,
    .variant = UFT_VAR_DSHD,
    .encoding = UFT_ENC_MFM,
    .heads = 2,
    .tracks = 80,
    .sectors_min = 22,
    .sectors_max = 22,
    .sector_size = 512,
    .image_size = 1802240,
    .cell_size = 1000,
    .rpm = 300,
    .interleave = 0,
    .skew = 0,
    .gap1 = 0, .gap2 = 0, .gap3 = 0, .gap4 = 0,
    .flags = UFT_FMT_WRITE_SUPPORT | UFT_FMT_CONVERT
};

/*============================================================================
 * IBM PC Formats
 *============================================================================*/

const uft_format_def_t UFT_FMT_IMG_360 = {
    .name = "IMG_360",
    .description = "IBM PC 360K (5.25\" DD)",
    .extensions = "img,ima,dsk",
    .form_factor = UFT_FF_525,
    .variant = UFT_VAR_DSDD,
    .encoding = UFT_ENC_MFM,
    .heads = 2,
    .tracks = 40,
    .sectors_min = 9,
    .sectors_max = 9,
    .sector_size = 512,
    .image_size = 368640,
    .cell_size = 2000,
    .rpm = 300,
    .interleave = 1,
    .skew = 0,
    .gap1 = 50, .gap2 = 22, .gap3 = 80, .gap4 = 0,
    .flags = UFT_FMT_WRITE_SUPPORT | UFT_FMT_CONVERT
};

const uft_format_def_t UFT_FMT_IMG_720 = {
    .name = "IMG_720",
    .description = "IBM PC 720K (3.5\" DD)",
    .extensions = "img,ima,dsk",
    .form_factor = UFT_FF_35,
    .variant = UFT_VAR_DSDD,
    .encoding = UFT_ENC_MFM,
    .heads = 2,
    .tracks = 80,
    .sectors_min = 9,
    .sectors_max = 9,
    .sector_size = 512,
    .image_size = 737280,
    .cell_size = 2000,
    .rpm = 300,
    .interleave = 1,
    .skew = 0,
    .gap1 = 50, .gap2 = 22, .gap3 = 80, .gap4 = 0,
    .flags = UFT_FMT_WRITE_SUPPORT | UFT_FMT_CONVERT
};

const uft_format_def_t UFT_FMT_IMG_1200 = {
    .name = "IMG_1200",
    .description = "IBM PC 1.2M (5.25\" HD)",
    .extensions = "img,ima,dsk",
    .form_factor = UFT_FF_525,
    .variant = UFT_VAR_DSHD,
    .encoding = UFT_ENC_MFM,
    .heads = 2,
    .tracks = 80,
    .sectors_min = 15,
    .sectors_max = 15,
    .sector_size = 512,
    .image_size = 1228800,
    .cell_size = 1200,
    .rpm = 360,
    .interleave = 1,
    .skew = 0,
    .gap1 = 50, .gap2 = 22, .gap3 = 54, .gap4 = 0,
    .flags = UFT_FMT_WRITE_SUPPORT | UFT_FMT_CONVERT
};

const uft_format_def_t UFT_FMT_IMG_1440 = {
    .name = "IMG_1440",
    .description = "IBM PC 1.44M (3.5\" HD)",
    .extensions = "img,ima,dsk",
    .form_factor = UFT_FF_35,
    .variant = UFT_VAR_DSHD,
    .encoding = UFT_ENC_MFM,
    .heads = 2,
    .tracks = 80,
    .sectors_min = 18,
    .sectors_max = 18,
    .sector_size = 512,
    .image_size = 1474560,
    .cell_size = 1000,
    .rpm = 300,
    .interleave = 1,
    .skew = 0,
    .gap1 = 80, .gap2 = 22, .gap3 = 108, .gap4 = 0,
    .flags = UFT_FMT_WRITE_SUPPORT | UFT_FMT_CONVERT
};

const uft_format_def_t UFT_FMT_IMG_2880 = {
    .name = "IMG_2880",
    .description = "IBM PC 2.88M (3.5\" ED)",
    .extensions = "img,ima,dsk",
    .form_factor = UFT_FF_35,
    .variant = UFT_VAR_DSED,
    .encoding = UFT_ENC_MFM,
    .heads = 2,
    .tracks = 80,
    .sectors_min = 36,
    .sectors_max = 36,
    .sector_size = 512,
    .image_size = 2949120,
    .cell_size = 500,
    .rpm = 300,
    .interleave = 1,
    .skew = 0,
    .gap1 = 80, .gap2 = 22, .gap3 = 41, .gap4 = 0,
    .flags = UFT_FMT_WRITE_SUPPORT | UFT_FMT_CONVERT
};

/*============================================================================
 * Atari Formats
 *============================================================================*/

const uft_format_def_t UFT_FMT_ATR = {
    .name = "ATR",
    .description = "Atari 8-bit disk image",
    .extensions = "atr",
    .form_factor = UFT_FF_525,
    .variant = UFT_VAR_SSDD,
    .encoding = UFT_ENC_FM,
    .heads = 1,
    .tracks = 40,
    .sectors_min = 18,
    .sectors_max = 18,
    .sector_size = 128,
    .image_size = 92176,
    .cell_size = 4000,
    .rpm = 288,
    .interleave = 1,
    .skew = 0,
    .gap1 = 0, .gap2 = 0, .gap3 = 0, .gap4 = 0,
    .flags = UFT_FMT_WRITE_SUPPORT | UFT_FMT_CONVERT
};

const uft_format_def_t UFT_FMT_XFD = {
    .name = "XFD",
    .description = "Atari XFormer disk image",
    .extensions = "xfd",
    .form_factor = UFT_FF_525,
    .variant = UFT_VAR_SSDD,
    .encoding = UFT_ENC_FM,
    .heads = 1,
    .tracks = 40,
    .sectors_min = 18,
    .sectors_max = 18,
    .sector_size = 128,
    .image_size = 92160,
    .cell_size = 4000,
    .rpm = 288,
    .interleave = 1,
    .skew = 0,
    .gap1 = 0, .gap2 = 0, .gap3 = 0, .gap4 = 0,
    .flags = UFT_FMT_WRITE_SUPPORT | UFT_FMT_CONVERT
};

const uft_format_def_t UFT_FMT_ST = {
    .name = "ST",
    .description = "Atari ST raw sector image",
    .extensions = "st",
    .form_factor = UFT_FF_35,
    .variant = UFT_VAR_DSDD,
    .encoding = UFT_ENC_MFM,
    .heads = 2,
    .tracks = 80,
    .sectors_min = 9,
    .sectors_max = 10,
    .sector_size = 512,
    .image_size = 737280,
    .cell_size = 2000,
    .rpm = 300,
    .interleave = 0,
    .skew = 0,
    .gap1 = 60, .gap2 = 22, .gap3 = 40, .gap4 = 0,
    .flags = UFT_FMT_WRITE_SUPPORT | UFT_FMT_CONVERT
};

const uft_format_def_t UFT_FMT_MSA = {
    .name = "MSA",
    .description = "Atari ST compressed image",
    .extensions = "msa",
    .form_factor = UFT_FF_35,
    .variant = UFT_VAR_DSDD,
    .encoding = UFT_ENC_MFM,
    .heads = 2,
    .tracks = 80,
    .sectors_min = 9,
    .sectors_max = 10,
    .sector_size = 512,
    .image_size = 0,
    .cell_size = 2000,
    .rpm = 300,
    .interleave = 0,
    .skew = 0,
    .gap1 = 0, .gap2 = 0, .gap3 = 0, .gap4 = 0,
    .flags = UFT_FMT_COMPRESSED | UFT_FMT_CONVERT
};

const uft_format_def_t UFT_FMT_STX = {
    .name = "STX",
    .description = "Atari ST extended (Pasti)",
    .extensions = "stx",
    .form_factor = UFT_FF_35,
    .variant = UFT_VAR_DSDD,
    .encoding = UFT_ENC_MFM,
    .heads = 2,
    .tracks = 80,
    .sectors_min = 9,
    .sectors_max = 11,
    .sector_size = 512,
    .image_size = 0,
    .cell_size = 2000,
    .rpm = 300,
    .interleave = 0,
    .skew = 0,
    .gap1 = 0, .gap2 = 0, .gap3 = 0, .gap4 = 0,
    .flags = UFT_FMT_TIMING | UFT_FMT_COPY_PROTECT | UFT_FMT_WEAK_BITS
};

/*============================================================================
 * TRS-80 Formats
 *============================================================================*/

const uft_format_def_t UFT_FMT_DMK = {
    .name = "DMK",
    .description = "TRS-80 raw track image",
    .extensions = "dmk",
    .form_factor = UFT_FF_525,
    .variant = UFT_VAR_DSDD,
    .encoding = UFT_ENC_MFM,
    .heads = 2,
    .tracks = 40,
    .sectors_min = 10,
    .sectors_max = 18,
    .sector_size = 256,
    .image_size = 0,
    .cell_size = 2000,
    .rpm = 300,
    .interleave = 0,
    .skew = 0,
    .gap1 = 0, .gap2 = 0, .gap3 = 0, .gap4 = 0,
    .flags = UFT_FMT_RAW_TRACK | UFT_FMT_COPY_PROTECT | UFT_FMT_WRITE_SUPPORT
};

const uft_format_def_t UFT_FMT_JV1 = {
    .name = "JV1",
    .description = "TRS-80 JV1 sector image",
    .extensions = "dsk",
    .form_factor = UFT_FF_525,
    .variant = UFT_VAR_SSSD,
    .encoding = UFT_ENC_FM,
    .heads = 1,
    .tracks = 35,
    .sectors_min = 10,
    .sectors_max = 10,
    .sector_size = 256,
    .image_size = 89600,
    .cell_size = 4000,
    .rpm = 300,
    .interleave = 0,
    .skew = 0,
    .gap1 = 0, .gap2 = 0, .gap3 = 0, .gap4 = 0,
    .flags = UFT_FMT_WRITE_SUPPORT | UFT_FMT_CONVERT
};

const uft_format_def_t UFT_FMT_JV3 = {
    .name = "JV3",
    .description = "TRS-80 JV3 sector image",
    .extensions = "dsk,jv3",
    .form_factor = UFT_FF_525,
    .variant = UFT_VAR_DSDD,
    .encoding = UFT_ENC_MFM,
    .heads = 2,
    .tracks = 40,
    .sectors_min = 10,
    .sectors_max = 18,
    .sector_size = 256,
    .image_size = 0,
    .cell_size = 2000,
    .rpm = 300,
    .interleave = 0,
    .skew = 0,
    .gap1 = 0, .gap2 = 0, .gap3 = 0, .gap4 = 0,
    .flags = UFT_FMT_WRITE_SUPPORT | UFT_FMT_CONVERT
};

/*============================================================================
 * Flux/Preservation Formats
 *============================================================================*/

const uft_format_def_t UFT_FMT_SCP = {
    .name = "SCP",
    .description = "SuperCard Pro flux image",
    .extensions = "scp",
    .form_factor = UFT_FF_UNKNOWN,
    .variant = UFT_VAR_UNKNOWN,
    .encoding = UFT_ENC_RAW,
    .heads = 2,
    .tracks = 84,
    .sectors_min = 0,
    .sectors_max = 0,
    .sector_size = 0,
    .image_size = 0,
    .cell_size = 0,
    .rpm = 0,
    .interleave = 0,
    .skew = 0,
    .gap1 = 0, .gap2 = 0, .gap3 = 0, .gap4 = 0,
    .flags = UFT_FMT_FLUX | UFT_FMT_WEAK_BITS | UFT_FMT_COPY_PROTECT |
             UFT_FMT_TIMING | UFT_FMT_WRITE_SUPPORT
};

const uft_format_def_t UFT_FMT_KF = {
    .name = "KF",
    .description = "KryoFlux stream files",
    .extensions = "raw",
    .form_factor = UFT_FF_UNKNOWN,
    .variant = UFT_VAR_UNKNOWN,
    .encoding = UFT_ENC_RAW,
    .heads = 2,
    .tracks = 84,
    .sectors_min = 0,
    .sectors_max = 0,
    .sector_size = 0,
    .image_size = 0,
    .cell_size = 0,
    .rpm = 0,
    .interleave = 0,
    .skew = 0,
    .gap1 = 0, .gap2 = 0, .gap3 = 0, .gap4 = 0,
    .flags = UFT_FMT_FLUX | UFT_FMT_WEAK_BITS | UFT_FMT_COPY_PROTECT |
             UFT_FMT_TIMING
};

const uft_format_def_t UFT_FMT_HFE = {
    .name = "HFE",
    .description = "HxC Floppy Emulator image",
    .extensions = "hfe",
    .form_factor = UFT_FF_UNKNOWN,
    .variant = UFT_VAR_UNKNOWN,
    .encoding = UFT_ENC_RAW,
    .heads = 2,
    .tracks = 84,
    .sectors_min = 0,
    .sectors_max = 0,
    .sector_size = 0,
    .image_size = 0,
    .cell_size = 0,
    .rpm = 0,
    .interleave = 0,
    .skew = 0,
    .gap1 = 0, .gap2 = 0, .gap3 = 0, .gap4 = 0,
    .flags = UFT_FMT_RAW_TRACK | UFT_FMT_WRITE_SUPPORT | UFT_FMT_CONVERT
};

const uft_format_def_t UFT_FMT_IPF = {
    .name = "IPF",
    .description = "Interchangeable Preservation Format",
    .extensions = "ipf",
    .form_factor = UFT_FF_UNKNOWN,
    .variant = UFT_VAR_UNKNOWN,
    .encoding = UFT_ENC_RAW,
    .heads = 2,
    .tracks = 84,
    .sectors_min = 0,
    .sectors_max = 0,
    .sector_size = 0,
    .image_size = 0,
    .cell_size = 0,
    .rpm = 0,
    .interleave = 0,
    .skew = 0,
    .gap1 = 0, .gap2 = 0, .gap3 = 0, .gap4 = 0,
    .flags = UFT_FMT_FLUX | UFT_FMT_WEAK_BITS | UFT_FMT_COPY_PROTECT |
             UFT_FMT_TIMING
};

const uft_format_def_t UFT_FMT_IMD = {
    .name = "IMD",
    .description = "ImageDisk sector image",
    .extensions = "imd",
    .form_factor = UFT_FF_UNKNOWN,
    .variant = UFT_VAR_UNKNOWN,
    .encoding = UFT_ENC_RAW,
    .heads = 2,
    .tracks = 80,
    .sectors_min = 8,
    .sectors_max = 26,
    .sector_size = 0,
    .image_size = 0,
    .cell_size = 0,
    .rpm = 0,
    .interleave = 0,
    .skew = 0,
    .gap1 = 0, .gap2 = 0, .gap3 = 0, .gap4 = 0,
    .flags = UFT_FMT_WRITE_SUPPORT | UFT_FMT_CONVERT
};

const uft_format_def_t UFT_FMT_TD0 = {
    .name = "TD0",
    .description = "TeleDisk image",
    .extensions = "td0",
    .form_factor = UFT_FF_UNKNOWN,
    .variant = UFT_VAR_UNKNOWN,
    .encoding = UFT_ENC_RAW,
    .heads = 2,
    .tracks = 80,
    .sectors_min = 8,
    .sectors_max = 26,
    .sector_size = 0,
    .image_size = 0,
    .cell_size = 0,
    .rpm = 0,
    .interleave = 0,
    .skew = 0,
    .gap1 = 0, .gap2 = 0, .gap3 = 0, .gap4 = 0,
    .flags = UFT_FMT_COMPRESSED | UFT_FMT_CONVERT
};

const uft_format_def_t UFT_FMT_CPC_DSK = {
    .name = "CPC_DSK",
    .description = "Amstrad CPC disk image",
    .extensions = "dsk",
    .form_factor = UFT_FF_3,
    .variant = UFT_VAR_SSDD,
    .encoding = UFT_ENC_MFM,
    .heads = 1,
    .tracks = 40,
    .sectors_min = 9,
    .sectors_max = 9,
    .sector_size = 512,
    .image_size = 194816,
    .cell_size = 2000,
    .rpm = 300,
    .interleave = 0,
    .skew = 0,
    .gap1 = 0, .gap2 = 0, .gap3 = 0, .gap4 = 0,
    .flags = UFT_FMT_WRITE_SUPPORT | UFT_FMT_CONVERT
};

const uft_format_def_t UFT_FMT_BBC_SSD = {
    .name = "BBC_SSD",
    .description = "BBC Micro single-sided",
    .extensions = "ssd",
    .form_factor = UFT_FF_525,
    .variant = UFT_VAR_SSDD,
    .encoding = UFT_ENC_FM,
    .heads = 1,
    .tracks = 40,
    .sectors_min = 10,
    .sectors_max = 10,
    .sector_size = 256,
    .image_size = 102400,
    .cell_size = 4000,
    .rpm = 300,
    .interleave = 0,
    .skew = 0,
    .gap1 = 0, .gap2 = 0, .gap3 = 0, .gap4 = 0,
    .flags = UFT_FMT_WRITE_SUPPORT | UFT_FMT_CONVERT
};

const uft_format_def_t UFT_FMT_BBC_DSD = {
    .name = "BBC_DSD",
    .description = "BBC Micro double-sided",
    .extensions = "dsd",
    .form_factor = UFT_FF_525,
    .variant = UFT_VAR_DSDD,
    .encoding = UFT_ENC_FM,
    .heads = 2,
    .tracks = 40,
    .sectors_min = 10,
    .sectors_max = 10,
    .sector_size = 256,
    .image_size = 204800,
    .cell_size = 4000,
    .rpm = 300,
    .interleave = 0,
    .skew = 0,
    .gap1 = 0, .gap2 = 0, .gap3 = 0, .gap4 = 0,
    .flags = UFT_FMT_WRITE_SUPPORT | UFT_FMT_CONVERT
};

/*============================================================================
 * Format Registry
 *============================================================================*/

static const uft_format_def_t* const format_registry[] = {
    /* Commodore */
    &UFT_FMT_D64, &UFT_FMT_D64_40, &UFT_FMT_D71, &UFT_FMT_D81,
    &UFT_FMT_G64, &UFT_FMT_G71,
    /* Apple */
    &UFT_FMT_DO, &UFT_FMT_PO, &UFT_FMT_NIB, &UFT_FMT_WOZ,
    /* Amiga */
    &UFT_FMT_ADF, &UFT_FMT_ADF_HD,
    /* IBM PC */
    &UFT_FMT_IMG_360, &UFT_FMT_IMG_720, &UFT_FMT_IMG_1200,
    &UFT_FMT_IMG_1440, &UFT_FMT_IMG_2880,
    /* Atari */
    &UFT_FMT_ATR, &UFT_FMT_XFD, &UFT_FMT_ST, &UFT_FMT_MSA, &UFT_FMT_STX,
    /* TRS-80 */
    &UFT_FMT_DMK, &UFT_FMT_JV1, &UFT_FMT_JV3,
    /* Flux/Preservation */
    &UFT_FMT_SCP, &UFT_FMT_KF, &UFT_FMT_HFE, &UFT_FMT_IPF,
    /* Other */
    &UFT_FMT_IMD, &UFT_FMT_TD0, &UFT_FMT_CPC_DSK,
    &UFT_FMT_BBC_SSD, &UFT_FMT_BBC_DSD,
    NULL
};

size_t uft_format_count(void)
{
    size_t count = 0;
    while (format_registry[count]) count++;
    return count;
}

const uft_format_def_t* uft_format_by_index(size_t index)
{
    size_t count = uft_format_count();
    if (index >= count) return NULL;
    return format_registry[index];
}

static int strcasecmp_local(const char *a, const char *b)
{
    while (*a && *b) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

const uft_format_def_t* uft_format_by_name(const char *name)
{
    if (!name) return NULL;
    
    for (size_t i = 0; format_registry[i]; i++) {
        if (strcasecmp_local(format_registry[i]->name, name) == 0) {
            return format_registry[i];
        }
    }
    return NULL;
}

const uft_format_def_t* uft_format_by_extension(const char *ext)
{
    if (!ext) return NULL;
    
    /* Skip leading dot */
    if (*ext == '.') ext++;
    
    for (size_t i = 0; format_registry[i]; i++) {
        const char *exts = format_registry[i]->extensions;
        if (!exts) continue;
        
        /* Search comma-separated list */
        const char *p = exts;
        while (*p) {
            const char *start = p;
            while (*p && *p != ',') p++;
            
            size_t len = p - start;
            if (len > 0 && strlen(ext) == len &&
                strncasecmp(start, ext, len) == 0) {
                return format_registry[i];
            }
            
            if (*p == ',') p++;
        }
    }
    return NULL;
}

const uft_format_def_t* uft_format_by_size(uint32_t size)
{
    const uft_format_def_t* match = NULL;
    
    for (size_t i = 0; format_registry[i]; i++) {
        if (format_registry[i]->image_size == size) {
            if (match) {
                /* Ambiguous - multiple formats with same size */
                return NULL;
            }
            match = format_registry[i];
        }
    }
    return match;
}
