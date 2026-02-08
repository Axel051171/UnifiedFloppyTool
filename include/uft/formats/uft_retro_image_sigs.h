/**
 * @file uft_retro_image_sigs.h
 * @brief Retro Image Format Signature Database
 *
 * 400 retro image format signatures extracted from 1096 test files.
 * Covers: Atari ST/Falcon, Amiga, C64, MSX, ZX Spectrum, Apple II,
 * Atari 8-bit, CPC, GEM, PlayStation, Japanese PC, and more.
 *
 * Source: Comprehensive retro image format test collection
 * Generated from empirical analysis of real-world sample files.
 */

#ifndef UFT_RETRO_IMAGE_SIGS_H
#define UFT_RETRO_IMAGE_SIGS_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Platform identifiers */
typedef enum {
    RI_PLAT_UNKNOWN,
    RI_PLAT_ATARI_ST,
    RI_PLAT_AMIGA,
    RI_PLAT_C64,
    RI_PLAT_MSX,
    RI_PLAT_ZX_SPECTRUM,
    RI_PLAT_APPLE_II,
    RI_PLAT_ATARI_8BIT,
    RI_PLAT_CPC,
    RI_PLAT_PS1,
    RI_PLAT_GEM,
    RI_PLAT_JAPANESE_PC,
    RI_PLAT_GENERIC,
    RI_PLAT_OTHER,
    RI_PLAT_COUNT,
} ri_platform_t;

static const char *ri_platform_names[] = {
    "Unknown",
    "Atari ST/Falcon",
    "Amiga",
    "C64/VIC-20",
    "MSX",
    "ZX Spectrum",
    "Apple II",
    "Atari 8-bit",
    "Amstrad CPC",
    "PlayStation",
    "GEM/TOS",
    "Japanese PC",
    "Generic",
    "Other",
};

/* Signature entry */
typedef struct ri_sig_entry {
    const char    *ext;           /**< File extension (without dot) */
    const char    *name;          /**< Human-readable format name */
    const uint8_t *magic;         /**< Magic bytes (NULL if none) */
    uint8_t        magic_len;     /**< Length of magic bytes */
    uint32_t       min_size;      /**< Minimum observed file size */
    uint32_t       max_size;      /**< Maximum observed file size (0=unlimited) */
    uint8_t        fixed_size;    /**< 1 if all samples have identical size */
    ri_platform_t  platform;      /**< Platform/system */
    uint8_t        samples;       /**< Number of verified sample files */
} ri_sig_entry_t;

/* ============================================================================
 * Magic Byte Constants (indexed to guarantee uniqueness)
 * ============================================================================ */

static const uint8_t ri_mag_000[] = { 0x02, 0x52, 0x13, 0xAA, 0x4C, 0xAA, 0xF4, 0x51, 0x49, 0x51, 0x05, 0xD5 }; /* .3 ".R..L..QIQ.." */
static const uint8_t ri_mag_001[] = { 0xC1, 0xD0, 0xD0, 0x00, 0x00, 0x00, 0xED, 0x0F, 0xCB, 0x0F, 0x34, 0x05 }; /* .3201 "..........4." */
static const uint8_t ri_mag_002[] = { 0x47, 0x4F, 0x44, 0x30, 0xAD, 0x04, 0x88, 0x87, 0x87, 0x78, 0x77, 0x88 }; /* .4bt "GOD0.....xw." */
static const uint8_t ri_mag_003[] = { 0x3F, 0x3C, 0x38, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x42, 0x42 }; /* .4mi "?<84......BB" */
static const uint8_t ri_mag_004[] = { 0x75, 0x0F, 0x27, 0x3A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .4pl "u.':........" */
static const uint8_t ri_mag_005[] = { 0x93, 0xA5, 0xC6, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .4pm "............" */
static const uint8_t ri_mag_006[] = { 0x00, 0x38 }; /* .64c ".8" */
static const uint8_t ri_mag_007[] = { 0x42, 0x00, 0x00, 0x44, 0x00, 0x00, 0x54, 0x00, 0x05, 0x55, 0x40, 0x05 }; /* .a "B..D..T..U@." */
static const uint8_t ri_mag_008[] = { 0xFF, 0x98, 0x00, 0x90, 0x4F, 0xFF, 0xFF, 0xEF, 0xFA, 0x01, 0x03, 0xFE }; /* .a4r "....O......." */
static const uint8_t ri_mag_009[] = { 0x00, 0x40, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xAA, 0xAA }; /* .a64 ".@.........." */
static const uint8_t ri_mag_010[] = { 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .aas ". .........." */
static const uint8_t ri_mag_011[] = { 0x41, 0x6D }; /* .abk "Am" */
static const uint8_t ri_mag_012[] = { 0x46, 0x4F, 0x52, 0x4D, 0x00, 0x00 }; /* .acbm "FORM.." */
static const uint8_t ri_mag_013[] = { 0x00, 0x06, 0x0B, 0x0F, 0xEF, 0xFB, 0xFA, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF }; /* .acs "............" */
static const uint8_t ri_mag_014[] = { 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .afl ".@.........." */
static const uint8_t ri_mag_016[] = { 0x41, 0x47, 0x53 }; /* .ags "AGS" */
static const uint8_t ri_mag_017[] = { 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x03 }; /* .all "............" */
static const uint8_t ri_mag_018[] = { 0x00, 0x40, 0xFF, 0xC2, 0x07, 0xD5, 0xFF, 0xC2, 0x07, 0x55, 0xFF, 0xC2 }; /* .ami ".@.......U.." */
static const uint8_t ri_mag_019[] = { 0x12, 0x08, 0xD4, 0xD4, 0xD4, 0xD4, 0xC6, 0xC6, 0xC6, 0xD4, 0xC6, 0xC6 }; /* .an2 "............" */
static const uint8_t ri_mag_020[] = { 0x07, 0x0A, 0x00, 0x28, 0xCA, 0x94, 0x46, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .an4 "...(..F....." */
static const uint8_t ri_mag_021[] = { 0x27, 0x0B, 0x92, 0x14, 0xB8, 0xC4, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .an5 "'.....4....." */
static const uint8_t ri_mag_023[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .ap3 "............" */
static const uint8_t ri_mag_024[] = { 0x22, 0x21, 0x11, 0x22, 0x12, 0x10, 0x24, 0x06, 0x00, 0x21, 0x22, 0x12 }; /* .apa ""!."..$..!"." */
static const uint8_t ri_mag_026[] = { 0x9A, 0xF8, 0x39, 0x21, 0x04, 0x1D, 0x00, 0x14, 0x14, 0x14, 0x14, 0x14 }; /* .apl "..9!........" */
static const uint8_t ri_mag_027[] = { 0x53, 0x31, 0x30, 0x31, 0x00, 0x3E, 0x00 }; /* .app "S101.>." */
static const uint8_t ri_mag_028[] = { 0x53, 0x31, 0x30, 0x31, 0x28, 0x1E, 0x00, 0x01, 0x0F, 0x02, 0x0E, 0x0C }; /* .aps "S101(......." */
static const uint8_t ri_mag_029[] = { 0x11, 0x11, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x32, 0x10, 0x12 }; /* .apv ".."""""""2.." */
static const uint8_t ri_mag_030[] = { 0x53, 0x53, 0x5F, 0x53, 0x49, 0x46, 0x20, 0x20, 0x20, 0x20, 0x30, 0x2E }; /* .arv "SS_SIF    0." */
static const uint8_t ri_mag_031[] = { 0x53, 0x6F, 0x53, 0x6F, 0x53, 0x53, 0x6F, 0x53, 0x6F, 0x6F, 0x53, 0x53 }; /* .atr "SoSoSSoSooSS" */
static const uint8_t ri_mag_032[] = { 0x42, 0x26, 0x57, 0x32, 0x35, 0x36, 0x01, 0x00, 0x01, 0x40, 0xA1, 0xA7 }; /* .b&w "B&W256...@.." */
static const uint8_t ri_mag_033[] = { 0x42, 0x26, 0x57, 0x32, 0x35, 0x36, 0x00, 0xCC, 0x01, 0x27, 0xFF, 0xFF }; /* .b_w "B&W256...'.." */
static const uint8_t ri_mag_034[] = { 0x2A, 0x80, 0x00, 0x00, 0x02, 0x54, 0x4A, 0xA9, 0xAA, 0x00, 0x00, 0x00 }; /* .bb0 "*....TJ....." */
static const uint8_t ri_mag_035[] = { 0xA5, 0x5E, 0xA7, 0x69, 0xA7, 0x5A, 0xA7, 0x5B, 0xA5, 0xAF, 0xB4, 0xA7 }; /* .bb1 ".^.i.Z.[...." */
static const uint8_t ri_mag_036[] = { 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F }; /* .bb2 "????????????" */
static const uint8_t ri_mag_037[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .bb4 "............" */
static const uint8_t ri_mag_038[] = { 0xFF, 0x88, 0x8B, 0x9A, 0x9A, 0x9A, 0x9A, 0x9A, 0xFF, 0x00, 0x0F, 0xF0 }; /* .bb5 "............" */
static const uint8_t ri_mag_039[] = { 0x10, 0x00, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0x40, 0xC0 }; /* .bbg "..........@." */
static const uint8_t ri_mag_040[] = { 0x46, 0x4F, 0x52, 0x4D, 0x00, 0x00, 0xB1, 0x5A, 0x49, 0x4C, 0x42, 0x4D }; /* .beam "FORM...ZILBM" */
static const uint8_t ri_mag_041[] = { 0xFF, 0x3B, 0x62, 0x0A, 0x09, 0x00, 0x00, 0x09, 0x09, 0x09, 0x00, 0x00 }; /* .bfli ".;b........." */
static const uint8_t ri_mag_042[] = { 0x23, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x22, 0x22, 0x00, 0x00, 0x10 }; /* .bg9 "#......""..." */
static const uint8_t ri_mag_043[] = { 0x42, 0x55, 0x47, 0x42, 0x49, 0x54, 0x45, 0x52, 0x5F, 0x41, 0x50, 0x41 }; /* .bgp "BUGBITER_APA" */
static const uint8_t ri_mag_044[] = { 0x00, 0x00 }; /* .bil ".." */
static const uint8_t ri_mag_045[] = { 0x15, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x4A, 0x8A, 0xEC, 0xA2 }; /* .bkg ".UUUUUUUJ..." */
static const uint8_t ri_mag_046[] = { 0x46, 0x4F, 0x52, 0x4D, 0x00, 0x00, 0x1C, 0xD0, 0x49, 0x4C, 0x42, 0x4D }; /* .bl1 "FORM....ILBM" */
static const uint8_t ri_mag_047[] = { 0x46, 0x4F, 0x52, 0x4D, 0x00, 0x00, 0x00, 0xFC, 0x49, 0x4C, 0x42, 0x4D }; /* .bl2 "FORM....ILBM" */
static const uint8_t ri_mag_048[] = { 0x46, 0x4F, 0x52, 0x4D, 0x00, 0x00, 0x00, 0x7E, 0x49, 0x4C, 0x42, 0x4D }; /* .bl3 "FORM...~ILBM" */
static const uint8_t ri_mag_049[] = { 0x42, 0x4D, 0xCB, 0x02, 0x02, 0x80, 0x01, 0xE0, 0xAA, 0x80, 0x00, 0x00 }; /* .bm "BM.........." */
static const uint8_t ri_mag_050[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .bmc4 "............" */
static const uint8_t ri_mag_051[] = { 0x00, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .bml ".;.........." */
static const uint8_t ri_mag_052[] = { 0x00, 0x11, 0x00, 0x00 }; /* .bp "...." */
static const uint8_t ri_mag_053[] = { 0x55, 0x49, 0x4D, 0x47, 0x01, 0x00, 0x00 }; /* .bp1 "UIMG..." */
static const uint8_t ri_mag_054[] = { 0x55, 0x49, 0x4D, 0x47, 0x01, 0x00, 0x00 }; /* .bp2 "UIMG..." */
static const uint8_t ri_mag_055[] = { 0x55, 0x49, 0x4D, 0x47, 0x01, 0x00, 0x00 }; /* .bp4 "UIMG..." */
static const uint8_t ri_mag_056[] = { 0x55, 0x49, 0x4D, 0x47, 0x01, 0x00, 0x00 }; /* .bp6 "UIMG..." */
static const uint8_t ri_mag_057[] = { 0x55, 0x49, 0x4D, 0x47, 0x01, 0x00, 0x00 }; /* .bp8 "UIMG..." */
static const uint8_t ri_mag_058[] = { 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }; /* .bru "............" */
static const uint8_t ri_mag_059[] = { 0x42, 0x30, 0x9B, 0x27, 0x00, 0x00, 0x9B, 0x05, 0x00, 0x18, 0x10, 0x10 }; /* .bs "B0.'........" */
static const uint8_t ri_mag_060[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .bsc "............" */
static const uint8_t ri_mag_061[] = { 0x62, 0x73, 0x70, 0xC0, 0x00, 0x00, 0x64, 0x6F, 0x20, 0x73, 0x63, 0x65 }; /* .bsp "bsp...do sce" */
static const uint8_t ri_mag_062[] = { 0x55, 0x49, 0x4D, 0x47, 0x01, 0x00, 0x00 }; /* .c01 "UIMG..." */
static const uint8_t ri_mag_063[] = { 0x55, 0x49, 0x4D, 0x47, 0x01, 0x00, 0x00 }; /* .c02 "UIMG..." */
static const uint8_t ri_mag_064[] = { 0x55, 0x49, 0x4D, 0x47, 0x01, 0x00, 0x00 }; /* .c04 "UIMG..." */
static const uint8_t ri_mag_065[] = { 0x55, 0x49, 0x4D, 0x47, 0x01, 0x00, 0x00 }; /* .c06 "UIMG..." */
static const uint8_t ri_mag_066[] = { 0x55, 0x49, 0x4D, 0x47, 0x01, 0x00, 0x00 }; /* .c08 "UIMG..." */
static const uint8_t ri_mag_067[] = { 0x55, 0x49, 0x4D, 0x47, 0x01, 0x00, 0x00, 0x00, 0x10, 0x02, 0x00, 0xF0 }; /* .c16 "UIMG........" */
static const uint8_t ri_mag_068[] = { 0x55, 0x49, 0x4D, 0x47, 0x01, 0x00, 0x00, 0x00, 0x18, 0x03, 0x00, 0xF0 }; /* .c24 "UIMG........" */
static const uint8_t ri_mag_069[] = { 0x55, 0x49, 0x4D, 0x47, 0x01, 0x00, 0x00, 0x00, 0x20, 0x04, 0x00, 0xF0 }; /* .c32 "UIMG.... ..." */
static const uint8_t ri_mag_070[] = { 0x43, 0x41, 0x01, 0x00 }; /* .ca1 "CA.." */
static const uint8_t ri_mag_071[] = { 0x43, 0x41, 0x01, 0x01, 0x00, 0x00, 0x03, 0x33, 0x05, 0x55, 0x07, 0x77 }; /* .ca2 "CA.....3.U.w" */
static const uint8_t ri_mag_072[] = { 0x43, 0x41, 0x01, 0x02, 0x78, 0x00, 0x00, 0x02, 0x78, 0x27, 0xAA, 0x6D }; /* .ca3 "CA..x...x'.m" */
static const uint8_t ri_mag_073[] = { 0x43, 0x49, 0x4E, 0x20, 0x31, 0x2E, 0x32, 0x20 }; /* .cci "CIN 1.2 " */
static const uint8_t ri_mag_074[] = { 0xEF, 0x7E, 0x19, 0x08, 0x01, 0x00, 0x9E, 0x32, 0x30, 0x38, 0x30, 0x20 }; /* .cdu ".~.....2080 " */
static const uint8_t ri_mag_075[] = { 0x45, 0x59, 0x45, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .ce1 "EYES........" */
static const uint8_t ri_mag_076[] = { 0x45, 0x59, 0x45, 0x53, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .ce2 "EYES........" */
static const uint8_t ri_mag_077[] = { 0x45, 0x59, 0x45, 0x53, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .ce3 "EYES........" */
static const uint8_t ri_mag_078[] = { 0xFF, 0xFF, 0x00, 0x00, 0x02, 0x11, 0x01, 0x00, 0x06, 0x33, 0x07, 0x44 }; /* .cel ".........3.D" */
static const uint8_t ri_mag_079[] = { 0x00, 0x40 }; /* .cfli ".@" */
static const uint8_t ri_mag_080[] = { 0x52, 0x49, 0x46, 0x46 }; /* .cgx "RIFF" */
static const uint8_t ri_mag_081[] = { 0x63, 0x68, 0x72, 0x24, 0x20, 0x30 }; /* .ch$ "chr$ 0" */
static const uint8_t ri_mag_082[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x40, 0xF0 }; /* .ch4 "......... @." */
static const uint8_t ri_mag_083[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x40, 0xF8 }; /* .ch6 "......... @." */
static const uint8_t ri_mag_084[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x20, 0x7E }; /* .ch8 ".......... ~" */
static const uint8_t ri_mag_085[] = { 0x00, 0x80, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 }; /* .che "..UUUUUUUUUU" */
static const uint8_t ri_mag_086[] = { 0xC6, 0x70, 0xC8, 0x70, 0xDD, 0x70, 0xF8, 0x70, 0x1F, 0x71, 0x35, 0x71 }; /* .chr ".p.p.p.p.q5q" */
static const uint8_t ri_mag_087[] = { 0x16, 0x16, 0x16, 0x24, 0x00, 0x00, 0x80, 0x00, 0xB8, 0x00, 0xB5, 0x00 }; /* .chs "...$........" */
static const uint8_t ri_mag_088[] = { 0x43, 0x48, 0x58, 0x00, 0x00 }; /* .chx "CHX.." */
static const uint8_t ri_mag_089[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x22, 0x04, 0x44, 0x07, 0x07 }; /* .cl0 ".......".D.." */
static const uint8_t ri_mag_090[] = { 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x07, 0x06, 0x40, 0x07, 0x77 }; /* .cl1 ".........@.w" */
static const uint8_t ri_mag_091[] = { 0x00, 0x00, 0x00, 0x02, 0x05, 0x55, 0x00, 0x00, 0x03, 0x33, 0x04, 0x20 }; /* .cl2 ".....U...3. " */
static const uint8_t ri_mag_092[] = { 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .cle ".`.........." */
static const uint8_t ri_mag_093[] = { 0x54, 0x5F, 0x56, 0x44, 0x44, 0x55, 0x55, 0x44, 0x54, 0x55, 0x56, 0x44 }; /* .cm5 "T_VDDUUDTUVD" */
static const uint8_t ri_mag_094[] = { 0x9A, 0xAA, 0xAA, 0x01, 0x9A, 0xAA, 0xAA, 0x01, 0xAC, 0xAA, 0xAA, 0x01 }; /* .cpi "............" */
static const uint8_t ri_mag_095[] = { 0x43, 0x41, 0x4C, 0x41, 0x4D, 0x55, 0x53, 0x43, 0x52, 0x47, 0x03, 0xE8 }; /* .crg "CALAMUSCRG.." */
static const uint8_t ri_mag_096[] = { 0x20, 0x15, 0x00, 0x00, 0x00, 0x00, 0x01, 0x07, 0x0C, 0x08, 0x00, 0x00 }; /* .cs " ..........." */
static const uint8_t ri_mag_097[] = { 0x43, 0x54, 0x4D, 0x05 }; /* .ctm "CTM." */
static const uint8_t ri_mag_098[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .cut "............" */
static const uint8_t ri_mag_099[] = { 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .cwg "............" */
static const uint8_t ri_mag_100[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .da4 "............" */
static const uint8_t ri_mag_101[] = { 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD }; /* .dap "............" */
static const uint8_t ri_mag_102[] = { 0x44, 0x47, 0x43 }; /* .dc1 "DGC" */
static const uint8_t ri_mag_103[] = { 0x46, 0x4F, 0x52, 0x4D, 0x00 }; /* .dctv "FORM." */
static const uint8_t ri_mag_104[] = { 0x00, 0x1C, 0x0E, 0x0E, 0x0E, 0x05, 0x0D, 0x05, 0x05, 0x0D, 0x05, 0x05 }; /* .dd "............" */
static const uint8_t ri_mag_105[] = { 0x46, 0x4F, 0x52, 0x4D, 0x00 }; /* .deep "FORM." */
static const uint8_t ri_mag_106[] = { 0x00, 0x00, 0x19, 0x68, 0x00, 0x00, 0x14, 0x84, 0xE6, 0x00, 0x01, 0x40 }; /* .del "...h.......@" */
static const uint8_t ri_mag_107[] = { 0x44, 0x47, 0x55, 0x01, 0x01, 0x40, 0x00, 0xC8, 0x24, 0x1A, 0x00, 0x0C }; /* .dg1 "DGU..@..$..." */
static const uint8_t ri_mag_108[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .dgi "............" */
static const uint8_t ri_mag_109[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .dgp "............" */
static const uint8_t ri_mag_110[] = { 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .dhgr "............" */
static const uint8_t ri_mag_111[] = { 0x03, 0x0A, 0x00, 0x00, 0x36, 0x2C, 0x52, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .din "....6,R....." */
static const uint8_t ri_mag_112[] = { 0x51, 0x11, 0x6A, 0xEE, 0xA5, 0x04, 0x54, 0x55, 0x55, 0x55, 0x56, 0x55 }; /* .dit "Q.j...TUUUVU" */
static const uint8_t ri_mag_113[] = { 0x42, 0x00, 0x00, 0x00, 0x00 }; /* .dlm "B...." */
static const uint8_t ri_mag_114[] = { 0x00, 0x58, 0x44, 0x52, 0x41, 0x5A, 0x4C, 0x41, 0x43, 0x45, 0x21, 0x20 }; /* .dlp ".XDRAZLACE! " */
static const uint8_t ri_mag_115[] = { 0x00, 0x58, 0x0F, 0x0B, 0x05, 0x0F, 0x0B, 0x0B, 0x0B, 0x05, 0x03, 0x03 }; /* .dol ".X.........." */
static const uint8_t ri_mag_116[] = { 0x89, 0x41, 0x05, 0x0A, 0x91, 0x44, 0x48, 0x28, 0xA9, 0x14, 0x11, 0x14 }; /* .doo ".A...DH(...." */
static const uint8_t ri_mag_117[] = { 0x00, 0x00, 0x19, 0x68, 0x00, 0x00, 0x14, 0x84, 0x00, 0x00, 0x03, 0x2C }; /* .dph "...h.......," */
static const uint8_t ri_mag_118[] = { 0x46, 0x4F, 0x52, 0x4D, 0x00, 0x02, 0x92, 0xE8, 0x49, 0x4C, 0x42, 0x4D }; /* .dr "FORM....ILBM" */
static const uint8_t ri_mag_119[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; /* .drg "............" */
static const uint8_t ri_mag_120[] = { 0x00, 0x58, 0x44, 0x52, 0x41, 0x5A, 0x4C, 0x41, 0x43, 0x45, 0x21, 0x20 }; /* .drl ".XDRAZLACE! " */
static const uint8_t ri_mag_121[] = { 0x00, 0x58, 0x44, 0x52, 0x41, 0x5A, 0x50, 0x41, 0x49, 0x4E, 0x54, 0x20 }; /* .drp ".XDRAZPAINT " */
static const uint8_t ri_mag_122[] = { 0x00, 0x58, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .drz ".X.........." */
static const uint8_t ri_mag_123[] = { 0x01, 0x92, 0x02, 0x34, 0x0D, 0x4B, 0x07, 0xED, 0x55, 0x55, 0x00, 0x00 }; /* .du2 "...4.K..UU.." */
static const uint8_t ri_mag_124[] = { 0x01, 0x11, 0x01, 0x11, 0x02, 0x11, 0x02, 0x21, 0x02, 0x22, 0x03, 0x22 }; /* .duo ".......!."."" */
static const uint8_t ri_mag_125[] = { 0x00, 0x05, 0x05, 0x06, 0x06, 0x07, 0x0F, 0x07, 0x0B, 0x00, 0x05, 0x0F }; /* .ebd "............" */
static const uint8_t ri_mag_126[] = { 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .eci ".@.........." */
static const uint8_t ri_mag_127[] = { 0x00, 0x40, 0xF3, 0xF3, 0x19, 0x00, 0x0A, 0x02, 0x2A, 0x0A, 0xAA, 0x2A }; /* .ecp ".@......*..*" */
static const uint8_t ri_mag_128[] = { 0x33, 0x43, 0x42, 0x44, 0x45, 0x69, 0xAC, 0xCC, 0xCD, 0xFF, 0xFF, 0xFF }; /* .esc "3CBDEi......" */
static const uint8_t ri_mag_129[] = { 0x54, 0x4D, 0x53, 0x00, 0x03, 0x2C }; /* .esm "TMS..," */
static const uint8_t ri_mag_130[] = { 0x45, 0x5A, 0x00, 0xC8, 0x00, 0x00, 0x01, 0x12, 0x01, 0x01, 0x02, 0x24 }; /* .eza "EZ.........$" */
static const uint8_t ri_mag_131[] = { 0x00, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0xA4, 0xAE, 0x04 }; /* .f80 "............" */
static const uint8_t ri_mag_132[] = { 0xF0, 0x38 }; /* .fbi ".8" */
static const uint8_t ri_mag_133[] = { 0xFF, 0x3A, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .ffli ".:f........." */
static const uint8_t ri_mag_134[] = { 0xFF, 0xFF, 0x00, 0xB6, 0xFF, 0xBA, 0x65, 0x57, 0x9A, 0xA7, 0x55, 0x66 }; /* .fge "......eW..Uf" */
static const uint8_t ri_mag_135[] = { 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x7F }; /* .fgs ". ........@." */
static const uint8_t ri_mag_136[] = { 0x46, 0x4C, 0x55, 0x46, 0x46, 0x36, 0x34 }; /* .flf "FLUFF64" */
static const uint8_t ri_mag_137[] = { 0x00, 0x3C, 0x00, 0x04, 0x04, 0x06, 0x00, 0x04, 0x04, 0x04, 0x00, 0x0A }; /* .fli ".<.........." */
static const uint8_t ri_mag_138[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x30, 0x10 }; /* .fn2 "........000." */
static const uint8_t ri_mag_139[] = { 0xF0, 0x3F, 0x46, 0x55, 0x4E, 0x50, 0x41, 0x49, 0x4E, 0x54, 0x20, 0x28 }; /* .fp2 ".?FUNPAINT (" */
static const uint8_t ri_mag_140[] = { 0x80, 0x37, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .fpr ".7.........." */
static const uint8_t ri_mag_141[] = { 0x00, 0x40, 0x5B, 0x40, 0x80, 0x40, 0x80, 0x80, 0xC0, 0x80, 0x60, 0x00 }; /* .fpt ".@[@.@....`." */
static const uint8_t ri_mag_142[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .ftc "............" */
static const uint8_t ri_mag_143[] = { 0xFE, 0xFE }; /* .fwa ".." */
static const uint8_t ri_mag_144[] = { 0x42, 0x00, 0x3C, 0x42, 0x99, 0xA1, 0xA1, 0x99, 0x42, 0x3C, 0x0F, 0x0F }; /* .g "B.<B....B<.." */
static const uint8_t ri_mag_145[] = { 0x43, 0x44, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33 }; /* .g09 "CD3333333333" */
static const uint8_t ri_mag_146[] = { 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 }; /* .g10 "UUUUUUUUUUUU" */
static const uint8_t ri_mag_147[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .g11 "............" */
static const uint8_t ri_mag_148[] = { 0x47, 0x32, 0x46, 0x5A, 0x4C, 0x49, 0x42, 0x78 }; /* .g2f "G2FZLIBx" */
static const uint8_t ri_mag_149[] = { 0x47, 0x39, 0x42, 0x0B, 0x00 }; /* .g9b "G9B.." */
static const uint8_t ri_mag_150[] = { 0x53, 0x31, 0x30, 0x31, 0x00, 0x1E, 0x00, 0x01, 0x0F, 0x02, 0x09, 0x0E }; /* .g9s "S101........" */
static const uint8_t ri_mag_151[] = { 0x47, 0x9B }; /* .gb "G." */
static const uint8_t ri_mag_152[] = { 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .gcd ". .........." */
static const uint8_t ri_mag_153[] = { 0xFE, 0x00, 0x00, 0x9F, 0x76, 0x00, 0x00, 0x77, 0x77, 0x77, 0x77, 0x77 }; /* .ge5 "....v..wwwww" */
static const uint8_t ri_mag_154[] = { 0xFE, 0x00, 0x00, 0xFF, 0xFA, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x11 }; /* .ge7 "............" */
static const uint8_t ri_mag_155[] = { 0xFE, 0x00, 0x00, 0xFF, 0xD3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .ge8 "............" */
static const uint8_t ri_mag_156[] = { 0xFF, 0xFF, 0x30, 0x53, 0x4F, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .ged "..0SO......." */
static const uint8_t ri_mag_157[] = { 0x47, 0x46, 0x32, 0x35, 0x00, 0x00 }; /* .gfb "GF25.." */
static const uint8_t ri_mag_158[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xCC, 0x20, 0xF0, 0xD4, 0xE0, 0xEE, 0xF0, 0xD1 }; /* .gfx "..... ......" */
static const uint8_t ri_mag_159[] = { 0x00, 0x60 }; /* .gg ".`" */
static const uint8_t ri_mag_160[] = { 0x9E, 0x00, 0x92, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .ghg "............" */
static const uint8_t ri_mag_161[] = { 0x00, 0x60, 0xFF, 0xFB, 0xFB, 0xEE, 0xEB, 0xBA, 0xEA, 0xEE, 0xAA, 0xAA }; /* .gig ".`.........." */
static const uint8_t ri_mag_162[] = { 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .gih ".`.........." */
static const uint8_t ri_mag_163[] = { 0x80, 0x00, 0x6A, 0x00, 0xFA, 0xFA, 0xFA, 0xFA, 0xFA, 0xFA, 0xFA, 0xFA }; /* .gl8 "..j........." */
static const uint8_t ri_mag_164[] = { 0xBC, 0x00, 0xC9, 0x00, 0x45, 0x3F, 0x3B, 0x3F, 0x3D, 0x3F, 0x3B, 0x3F }; /* .glc "....E?;?=?;?" */
static const uint8_t ri_mag_165[] = { 0x8C, 0x00, 0x8E, 0x00, 0xCA, 0xC8, 0xCA, 0xC8, 0xCA, 0xD0, 0xCB, 0xC8 }; /* .gls "............" */
static const uint8_t ri_mag_166[] = { 0x04, 0x00, 0x01, 0x40, 0x00, 0xF0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; /* .god "...@........" */
static const uint8_t ri_mag_167[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .gr1 "............" */
static const uint8_t ri_mag_168[] = { 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A }; /* .gr2 "............" */
static const uint8_t ri_mag_169[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x03, 0x00, 0x00, 0x00 }; /* .gr3 ".......0...." */
static const uint8_t ri_mag_170[] = { 0xFC, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x30, 0xF3, 0xC0, 0x00, 0x00 }; /* .gr7 ".......0...." */
static const uint8_t ri_mag_171[] = { 0x11, 0x21, 0x22, 0x11, 0x10, 0x10, 0x00, 0x11, 0x00, 0x11, 0x22, 0x34 }; /* .gr9 ".!"......."4" */
static const uint8_t ri_mag_172[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .gr9p "............" */
static const uint8_t ri_mag_173[] = { 0x48, 0x50, 0x48, 0x50, 0x34, 0x38, 0x2D, 0x41, 0x1E, 0x2B, 0xB0, 0xFB }; /* .gro "HPHP48-A.+.." */
static const uint8_t ri_mag_174[] = { 0xFE, 0x00, 0x00, 0xFF, 0x37, 0x00, 0x00, 0xF7, 0x0E, 0x01, 0x02, 0xC0 }; /* .grp "....7......." */
static const uint8_t ri_mag_175[] = { 0xC7, 0x5C, 0x00, 0x00, 0x04, 0x4D, 0x41, 0x49, 0x4E, 0x00, 0x00, 0x40 }; /* .gs ".\...MAIN..@" */
static const uint8_t ri_mag_176[] = { 0x00, 0x40 }; /* .gun ".@" */
static const uint8_t ri_mag_177[] = { 0x46, 0x4F, 0x52, 0x4D, 0x00, 0x02, 0x01, 0x28, 0x49, 0x4C, 0x42, 0x4D }; /* .ham6 "FORM...(ILBM" */
static const uint8_t ri_mag_178[] = { 0x46, 0x4F, 0x52, 0x4D, 0x00, 0x04, 0x02, 0x22, 0x49, 0x4C, 0x42, 0x4D }; /* .ham8 "FORM..."ILBM" */
static const uint8_t ri_mag_179[] = { 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .hbm ". .........." */
static const uint8_t ri_mag_180[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; /* .hci "............" */
static const uint8_t ri_mag_181[] = { 0x48, 0x43, 0x4D, 0x41, 0x38, 0x01 }; /* .hcm "HCMA8." */
static const uint8_t ri_mag_182[] = { 0x00, 0x20, 0xFF, 0xFF, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xFF, 0xFF }; /* .hed ". .........." */
static const uint8_t ri_mag_183[] = { 0x00, 0x5C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .het ".\.........." */
static const uint8_t ri_mag_184[] = { 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .hfc ".@.........." */
static const uint8_t ri_mag_185[] = { 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .hfd ".@.........." */
static const uint8_t ri_mag_186[] = { 0x07, 0x7E, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F }; /* .hgr ".~.........." */
static const uint8_t ri_mag_187[] = { 0x00, 0x40 }; /* .him ".@" */
static const uint8_t ri_mag_188[] = { 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .hlf ". .........." */
static const uint8_t ri_mag_189[] = { 0x76, 0xAF, 0xD3, 0xFE, 0x21, 0x00, 0x58, 0x11, 0x01, 0x58, 0x01, 0xFF }; /* .hlr "v...!.X..X.." */
static const uint8_t ri_mag_190[] = { 0x00, 0x60, 0xFF, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xFF, 0x00 }; /* .hpc ".`.........." */
static const uint8_t ri_mag_191[] = { 0x00, 0x20, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; /* .hpi ". .........." */
static const uint8_t ri_mag_192[] = { 0x7F, 0xFF, 0xC8, 0x80, 0x48, 0x80, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00 }; /* .hpk "....H......." */
static const uint8_t ri_mag_193[] = { 0x00, 0x00, 0x06, 0x03, 0x00, 0x56, 0xAF, 0x00, 0xFF, 0x15, 0x02, 0xFF }; /* .hpm ".....V......" */
static const uint8_t ri_mag_194[] = { 0x53, 0x31, 0x30, 0x31, 0x89, 0x3E, 0x00, 0x0F, 0x01, 0x0E, 0x02, 0x0D }; /* .hps "S101.>......" */
static const uint8_t ri_mag_195[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; /* .hrg "............" */
static const uint8_t ri_mag_196[] = { 0xAE, 0x28, 0xAA, 0xAA, 0x2A, 0xEA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA }; /* .hrm ".(..*......." */
static const uint8_t ri_mag_197[] = { 0x16, 0x16, 0x16, 0x24, 0x00, 0x00, 0x80, 0x00, 0xBF, 0x3F, 0xA0, 0x00 }; /* .hrs "...$.....?.." */
static const uint8_t ri_mag_198[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .hs2 "............" */
static const uint8_t ri_mag_199[] = { 0x49, 0x43, 0x42, 0x33, 0x03, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x20 }; /* .ib3 "ICB3..... . " */
static const uint8_t ri_mag_200[] = { 0x49, 0x43, 0x42, 0x49, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x20 }; /* .ibi "ICBI..... . " */
static const uint8_t ri_mag_201[] = { 0x49, 0x4D, 0x44, 0x43, 0x00, 0x00, 0x00, 0x00, 0x03, 0x33, 0x02, 0x22 }; /* .ic1 "IMDC.....3."" */
static const uint8_t ri_mag_202[] = { 0x49, 0x4D, 0x44, 0x43, 0x00, 0x02, 0x07, 0x77, 0x00, 0x00, 0x07, 0x77 }; /* .ic3 "IMDC...w...w" */
static const uint8_t ri_mag_203[] = { 0x46, 0x4F, 0x52, 0x4D, 0x00 }; /* .iff "FORM." */
static const uint8_t ri_mag_204[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .ifl "............" */
static const uint8_t ri_mag_205[] = { 0xFF, 0xFF, 0xF6, 0xA3, 0xFF, 0xBB, 0xFF, 0x5F, 0x00, 0xFA, 0xC8, 0x72 }; /* .ige "......._...r" */
static const uint8_t ri_mag_206[] = { 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .ihe ". .........." */
static const uint8_t ri_mag_207[] = { 0x49, 0x53, 0x5F, 0x49, 0x4D, 0x41, 0x47, 0x45, 0x00 }; /* .iim "IS_IMAGE." */
static const uint8_t ri_mag_209[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .ild "............" */
static const uint8_t ri_mag_210[] = { 0x53, 0x31, 0x30, 0x31, 0x00, 0x3C, 0x00, 0x0F, 0x01, 0x02, 0x0D, 0x0E }; /* .ils "S101.<......" */
static const uint8_t ri_mag_211[] = { 0x01, 0x00, 0x74, 0x30, 0x66, 0x18, 0xD5, 0x55, 0x55, 0x55, 0x55, 0x55 }; /* .imn "..t0f..UUUUU" */
static const uint8_t ri_mag_212[] = { 0xE3, 0x10, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0xEB, 0x00, 0x30 }; /* .info "...........0" */
static const uint8_t ri_mag_213[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .ing "............" */
static const uint8_t ri_mag_214[] = { 0x53, 0x31, 0x30, 0x31, 0x84, 0x3E, 0x00, 0x01, 0x04, 0x0C, 0x0F, 0x05 }; /* .ins "S101.>......" */
static const uint8_t ri_mag_215[] = { 0x00, 0x00, 0x42, 0x52, 0x55, 0x53, 0x04 }; /* .ip "..BRUS." */
static const uint8_t ri_mag_216[] = { 0x01, 0x00, 0x30, 0xC2, 0x90, 0x0A, 0x96, 0x02, 0xA4, 0x04, 0x22, 0x08 }; /* .ip2 "..0......."." */
static const uint8_t ri_mag_217[] = { 0x01, 0x00, 0x32, 0x9C, 0xD6, 0x28, 0xB2, 0x1A, 0x90, 0x92, 0xBF, 0xFF }; /* .ipc "..2..(......" */
static const uint8_t ri_mag_218[] = { 0x00, 0x40 }; /* .iph ".@" */
static const uint8_t ri_mag_219[] = { 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .ipt ".@.........." */
static const uint8_t ri_mag_220[] = { 0x01, 0x0E, 0x58, 0x00, 0x98, 0x1C, 0x56, 0x8C, 0x40, 0x1A, 0xC0, 0x00 }; /* .ir2 "..X...V.@..." */
static const uint8_t ri_mag_221[] = { 0x01, 0x00, 0x18, 0x32, 0x26, 0x20, 0x2E, 0x9B, 0xEA, 0x7B, 0xEE, 0xA8 }; /* .irg "...2& ...{.." */
static const uint8_t ri_mag_222[] = { 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x1F, 0x1F, 0x00, 0x00 }; /* .ish ".@.........." */
static const uint8_t ri_mag_223[] = { 0x00, 0x3C }; /* .ism ".<" */
static const uint8_t ri_mag_224[] = { 0x11, 0x35, 0xF7, 0x0B, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; /* .ist ".5.........." */
static const uint8_t ri_mag_225[] = { 0xFF, 0xFF, 0x00, 0xA0, 0xFF, 0xA7 }; /* .jgp "......" */
static const uint8_t ri_mag_226[] = { 0x00, 0x5C, 0xFE }; /* .jj ".\." */
static const uint8_t ri_mag_227[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF }; /* .kfx "............" */
static const uint8_t ri_mag_228[] = { 0x4B, 0x44, 0x00, 0x00, 0x01, 0x11, 0x02, 0x22, 0x03, 0x33, 0x04, 0x44 }; /* .kid "KD.....".3.D" */
static const uint8_t ri_mag_229[] = { 0x00, 0x60, 0x03, 0xFB, 0xEC, 0x0B, 0xBF, 0xFC, 0x0E, 0xFF, 0xBC, 0x2F }; /* .koa ".`........./" */
static const uint8_t ri_mag_230[] = { 0xFF, 0xFF, 0x00, 0x00, 0x9C, 0x0B, 0x4B, 0x42, 0x1B, 0x05, 0x08, 0x00 }; /* .kpr "......KB...." */
static const uint8_t ri_mag_231[] = { 0x51, 0x55, 0x6A, 0xBB, 0xA9, 0x41, 0x45, 0x45, 0x55, 0x2F, 0xAA, 0x95 }; /* .kss "QUj..AEEU/.." */
static const uint8_t ri_mag_232[] = { 0x46, 0x4F, 0x52, 0x4D }; /* .lbm "FORM" */
static const uint8_t ri_mag_233[] = { 0x10, 0x12, 0x33, 0x30, 0x02, 0x10, 0x22, 0x22, 0x22, 0x22, 0x24, 0x22 }; /* .lce "..30..""""$"" */
static const uint8_t ri_mag_234[] = { 0xCC, 0xF5, 0xE4, 0xE5, 0xEB, 0xA0, 0xCD, 0xE1, 0xEB, 0xE5, 0xF2, 0xA0 }; /* .ldm "............" */
static const uint8_t ri_mag_235[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0x55, 0x55, 0x55 }; /* .leo "........UUUU" */
static const uint8_t ri_mag_236[] = { 0x00, 0x18 }; /* .lp3 ".." */
static const uint8_t ri_mag_237[] = { 0x47, 0x77, 0xF7, 0x00, 0xF0, 0x70, 0xF7, 0x70, 0xF0, 0x07, 0xF7, 0x07 }; /* .lpk "Gw...p.p...." */
static const uint8_t ri_mag_239[] = { 0x4D, 0x41, 0x4B, 0x49, 0x30, 0x32, 0x20, 0x20 }; /* .mag "MAKI02  " */
static const uint8_t ri_mag_240[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .mbg "............" */
static const uint8_t ri_mag_241[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x8F, 0xF9 }; /* .mc "............" */
static const uint8_t ri_mag_242[] = { 0x00, 0x9C, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11 }; /* .mci "............" */
static const uint8_t ri_mag_243[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x40, 0x01 }; /* .mcp "..........@." */
static const uint8_t ri_mag_244[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFA, 0xAA, 0x95, 0x55 }; /* .mcpp "...........U" */
static const uint8_t ri_mag_246[] = { 0xF1, 0x10, 0x0C, 0x12, 0xD8, 0x07, 0x9E, 0x20, 0x38, 0x35, 0x38, 0x34 }; /* .mg "....... 8584" */
static const uint8_t ri_mag_247[] = { 0x4D, 0x47, 0x48, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .mg1 "MGH........." */
static const uint8_t ri_mag_248[] = { 0x4D, 0x47, 0x48, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .mg2 "MGH........." */
static const uint8_t ri_mag_249[] = { 0x4D, 0x47, 0x48, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .mg4 "MGH........." */
static const uint8_t ri_mag_250[] = { 0x4D, 0x47, 0x48, 0x01, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .mg8 "MGH........." */
static const uint8_t ri_mag_251[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .mga "............" */
static const uint8_t ri_mag_252[] = { 0xF4, 0x0E, 0x36, 0x00, 0x00 }; /* .mgp "..6.." */
static const uint8_t ri_mag_253[] = { 0x4D, 0x53, 0x58, 0x4D, 0x49, 0x47 }; /* .mig "MSXMIG" */
static const uint8_t ri_mag_254[] = { 0xDC, 0x18, 0xFF, 0x80, 0x69, 0x67, 0x14, 0x00, 0x01, 0xE8, 0x03, 0xE8 }; /* .mil "....ig......" */
static const uint8_t ri_mag_255[] = { 0x93, 0x00, 0x00, 0x00, 0x00, 0x0F, 0xEA, 0xFE, 0xAF, 0xC0, 0x00, 0x00 }; /* .mis "............" */
static const uint8_t ri_mag_256[] = { 0x4D, 0x41, 0x4B, 0x49, 0x30, 0x31 }; /* .mki "MAKI01" */
static const uint8_t ri_mag_257[] = { 0x31, 0x30, 0x30, 0x1A }; /* .ml1 "100." */
static const uint8_t ri_mag_258[] = { 0x00, 0x20, 0x09, 0x16, 0x55, 0x55, 0x15, 0x55, 0x15, 0x48, 0x00, 0x51 }; /* .mle ". ..UU.U.H.Q" */
static const uint8_t ri_mag_259[] = { 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .mlt "............" */
static const uint8_t ri_mag_260[] = { 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .mon ". .........." */
static const uint8_t ri_mag_261[] = { 0x46, 0x4F, 0x52, 0x4D, 0x00 }; /* .mp "FORM." */
static const uint8_t ri_mag_262[] = { 0x07, 0x77, 0x07, 0x00, 0x00, 0x70, 0xF0, 0x00, 0xFB, 0xBB, 0x3D, 0x9D }; /* .mpk ".w...p....=." */
static const uint8_t ri_mag_263[] = { 0x1E, 0x0D, 0x07, 0x10, 0x0E, 0x34, 0x88, 0x88, 0xFA, 0x70, 0xA8, 0xF8 }; /* .mpl ".....4...p.." */
static const uint8_t ri_mag_264[] = { 0x4D, 0x50, 0x50 }; /* .mpp "MPP" */
static const uint8_t ri_mag_265[] = { 0x22, 0xC8, 0x02, 0x02, 0x02, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x01 }; /* .msl ""..........." */
static const uint8_t ri_mag_266[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .mur "............" */
static const uint8_t ri_mag_267[] = { 0x40, 0x40, 0x40, 0x20 }; /* .mx1 "@@@ " */
static const uint8_t ri_mag_268[] = { 0x20, 0x20, 0x78, 0x25 }; /* .nl3 "  x%" */
static const uint8_t ri_mag_269[] = { 0x44, 0x41, 0x49, 0x53, 0x59, 0x2D, 0x44, 0x4F, 0x54, 0x20, 0x4E, 0x4C }; /* .nlq "DAISY-DOT NL" */
static const uint8_t ri_mag_270[] = { 0x00, 0x00, 0x01, 0x00, 0x02, 0x01, 0x03, 0x01, 0x10, 0x00, 0x11, 0x00 }; /* .nxi "............" */
static const uint8_t ri_mag_271[] = { 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .ocp ". .........." */
static const uint8_t ri_mag_272[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18 }; /* .odf "............" */
static const uint8_t ri_mag_273[] = { 0x00, 0x0C }; /* .p11 ".." */
static const uint8_t ri_mag_274[] = { 0x32, 0x34, 0x33, 0x34, 0x31, 0x0D, 0x0A, 0x00, 0x00, 0x06, 0x52, 0x06 }; /* .p3c "24341.....R." */
static const uint8_t ri_mag_275[] = { 0x00, 0x18, 0x01, 0x0E, 0x00, 0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .p41 "............" */
static const uint8_t ri_mag_276[] = { 0x00, 0x18, 0x60, 0xD0, 0xD0, 0x60, 0x00, 0x20, 0xC0, 0x70, 0xE0, 0xA0 }; /* .p64 "..`..`. .p.." */
static const uint8_t ri_mag_277[] = { 0x50, 0x41, 0x42, 0x4C, 0x4F, 0x20, 0x50, 0x41, 0x43, 0x4B, 0x45, 0x44 }; /* .pa3 "PABLO PACKED" */
static const uint8_t ri_mag_278[] = { 0x70, 0x4D, 0x38, 0x36, 0x0A, 0xFF, 0x0C, 0x0A, 0xFF, 0x0A, 0xFF, 0x0A }; /* .pac "pM86........" */
static const uint8_t ri_mag_279[] = { 0x00, 0x00, 0x00 }; /* .pbx "..." */
static const uint8_t ri_mag_280[] = { 0x80, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x20, 0x01, 0x21, 0x01, 0x31 }; /* .pc1 "....... .!.1" */
static const uint8_t ri_mag_281[] = { 0x80, 0x01, 0x00, 0x00, 0x02, 0x22, 0x04, 0x44, 0x07, 0x77, 0x00, 0x00 }; /* .pc2 ".....".D.w.." */
static const uint8_t ri_mag_282[] = { 0x80, 0x02, 0x00, 0x00, 0x07, 0x77, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .pc3 ".....w......" */
static const uint8_t ri_mag_284[] = { 0x01, 0x40, 0x00, 0xC8 }; /* .pcs ".@.." */
static const uint8_t ri_mag_285[] = { 0x44, 0x59, 0x4E, 0x41, 0x4D, 0x49, 0x43, 0x20, 0x50, 0x55, 0x42, 0x4C }; /* .pct "DYNAMIC PUBL" */
static const uint8_t ri_mag_286[] = { 0x50, 0x4E, 0x49, 0x4B, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01 }; /* .pg "PNIK........" */
static const uint8_t ri_mag_287[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x22, 0x04, 0x44, 0x07, 0x07 }; /* .pg0 ".......".D.." */
static const uint8_t ri_mag_288[] = { 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x07, 0x06, 0x40, 0x07, 0x77 }; /* .pg1 ".........@.w" */
static const uint8_t ri_mag_289[] = { 0x00, 0x00, 0x00, 0x02, 0x05, 0x55, 0x00, 0x00, 0x03, 0x33, 0x04, 0x20 }; /* .pg2 ".....U...3. " */
static const uint8_t ri_mag_290[] = { 0x50, 0x47, 0x01, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x99, 0x00, 0x02 }; /* .pgc "PG.........." */
static const uint8_t ri_mag_291[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xFF, 0xF0, 0x00, 0x0F, 0xFC }; /* .pgf "............" */
static const uint8_t ri_mag_292[] = { 0xFF, 0xFF, 0x06, 0x82 }; /* .pgr "...." */
static const uint8_t ri_mag_293[] = { 0x00, 0x00 }; /* .pi1 ".." */
static const uint8_t ri_mag_294[] = { 0x00, 0x01, 0x07, 0x53, 0x06, 0x32, 0x07, 0x42, 0x05, 0x21, 0x07, 0x77 }; /* .pi2 "...S.2.B.!.w" */
static const uint8_t ri_mag_295[] = { 0x00, 0x02, 0x0F, 0xCF, 0x0F, 0x00, 0x00, 0xF8, 0x00, 0x00, 0x0F, 0xFF }; /* .pi3 "............" */
static const uint8_t ri_mag_296[] = { 0x00, 0x04, 0x00, 0x00, 0x0F, 0xF0, 0x0F, 0x00, 0x00, 0x5F, 0x00, 0xBF }; /* .pi5 "........._.." */
static const uint8_t ri_mag_297[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x1C }; /* .pi7 "............" */
static const uint8_t ri_mag_298[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; /* .pi8 "............" */
static const uint8_t ri_mag_299[] = { 0x00, 0x0D, 0x55, 0x55, 0x56, 0x56, 0x5A, 0x58, 0x6A, 0x6A, 0x62, 0x48 }; /* .pic0 "..UUVVZXjjbH" */
static const uint8_t ri_mag_300[] = { 0x00, 0x94, 0x0A, 0x0A, 0x0A, 0x2A, 0x0A, 0x2A, 0x0A, 0x09, 0x09, 0x09 }; /* .pic1 ".....*.*...." */
static const uint8_t ri_mag_301[] = { 0x04, 0x22, 0x4D, 0x18, 0x64, 0x70, 0xB9, 0x98, 0xC6, 0x00, 0x00, 0xF0 }; /* .pl4 "."M.dp......" */
static const uint8_t ri_mag_302[] = { 0x10, 0x00, 0x33, 0x02, 0x33, 0x04, 0x64, 0x06, 0x00, 0x00, 0x00, 0x00 }; /* .pl6 "..3.3.d....." */
static const uint8_t ri_mag_303[] = { 0x00, 0x00 }; /* .pl7 ".." */
static const uint8_t ri_mag_304[] = { 0x49, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .pla "I..........." */
static const uint8_t ri_mag_305[] = { 0x53, 0x31, 0x30, 0x31, 0x00, 0x1E, 0x00, 0x0F, 0x01, 0x02, 0x0E, 0x03 }; /* .pls "S101........" */
static const uint8_t ri_mag_306[] = { 0xF0, 0xED, 0xE4, 0x34, 0xA8, 0x54, 0x88, 0x04, 0x08, 0x04, 0x20, 0x00 }; /* .pmd "...4.T.... ." */
static const uint8_t ri_mag_307[] = { 0x8E, 0x3F, 0x14, 0x08, 0xD1, 0x07, 0x9E, 0x32, 0x30, 0x37, 0x30, 0x14 }; /* .pmg ".?.....2070." */
static const uint8_t ri_mag_308[] = { 0x00, 0x3C, 0x00, 0x00, 0x00, 0x0B, 0x0B, 0x0B, 0x0D, 0x0D, 0x03, 0x0D }; /* .pp ".<.........." */
static const uint8_t ri_mag_309[] = { 0x74, 0x6D, 0x38, 0x39, 0x50, 0x53, 0x00 }; /* .psc "tm89PS." */
static const uint8_t ri_mag_310[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .psf "............" */
static const uint8_t ri_mag_311[] = { 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44 }; /* .pzm "DDDDDDDDDDDD" */
static const uint8_t ri_mag_312[] = { 0x1A, 0x00, 0x11, 0x01, 0x01, 0xCF, 0xAD, 0xCB, 0xF0, 0xE3, 0x00, 0x4D }; /* .q4 "...........M" */
static const uint8_t ri_mag_313[] = { 0x52, 0x41, 0x47, 0x2D, 0x44, 0x21, 0x00, 0x00, 0x00 }; /* .rag "RAG-D!..." */
static const uint8_t ri_mag_314[] = { 0x52, 0x41, 0x47, 0x2D, 0x44, 0x21, 0x00, 0x00, 0x00, 0x03, 0xE5, 0x80 }; /* .ragc "RAG-D!......" */
static const uint8_t ri_mag_315[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; /* .rap "............" */
static const uint8_t ri_mag_316[] = { 0x28, 0x63, 0x29, 0x46, 0x2E, 0x4D, 0x41, 0x52, 0x43, 0x48, 0x41, 0x4C }; /* .rgh "(c)F.MARCHAL" */
static const uint8_t ri_mag_317[] = { 0x52, 0x49, 0x50 }; /* .rip "RIP" */
static const uint8_t ri_mag_318[] = { 0x1B, 0x47, 0x48, 0x20 }; /* .rle ".GH " */
static const uint8_t ri_mag_319[] = { 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 }; /* .rm2 "UUUUUUUUUUUU" */
static const uint8_t ri_mag_320[] = { 0xFF, 0x80, 0xC9, 0xC7, 0x1A, 0x00, 0x01, 0x01, 0x0E, 0x00, 0x28, 0x00 }; /* .rm4 "..........(." */
static const uint8_t ri_mag_321[] = { 0x00, 0x5C, 0x19, 0x19, 0x00, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5 }; /* .rp ".\.........." */
static const uint8_t ri_mag_322[] = { 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .rpm ".`.........." */
static const uint8_t ri_mag_323[] = { 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x03, 0x00, 0xFF, 0xFF, 0xFF, 0xFF }; /* .rst "............" */
static const uint8_t ri_mag_324[] = { 0x69, 0x69, 0x6A, 0x6C, 0x6F, 0x71, 0x73, 0x75, 0x71, 0x70, 0x6D, 0x6A }; /* .rwh "iijloqsuqpmj" */
static const uint8_t ri_mag_325[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xE9, 0xF1, 0xF3, 0xF3, 0xEB, 0xEE, 0xEA, 0xEA }; /* .rwl "............" */
static const uint8_t ri_mag_326[] = { 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00 }; /* .rys "............" */
static const uint8_t ri_mag_327[] = { 0xFE, 0x00, 0x00, 0x9F, 0x76, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66 }; /* .s15 "....v..fffff" */
static const uint8_t ri_mag_328[] = { 0xFE, 0x00, 0x00, 0x9F, 0x76, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .s16 "....v......." */
static const uint8_t ri_mag_329[] = { 0xFE, 0x00, 0x00, 0xA0, 0xFA, 0x00, 0x00, 0xB9, 0xEE, 0xEE, 0xEE, 0xEE }; /* .s17 "............" */
static const uint8_t ri_mag_330[] = { 0xFE, 0x00, 0x00, 0x9F, 0xFA, 0x00, 0x00, 0x92, 0x92, 0xB2, 0x92, 0xB2 }; /* .s18 "............" */
static const uint8_t ri_mag_331[] = { 0xFE, 0x00, 0x00, 0x9F, 0xFA, 0x00, 0x00, 0xE3, 0xE0, 0xE3, 0xE0, 0xE3 }; /* .s1a "............" */
static const uint8_t ri_mag_332[] = { 0xFE, 0x00, 0x00 }; /* .s1c "..." */
static const uint8_t ri_mag_333[] = { 0x00, 0x78, 0x95, 0x59, 0x58, 0x59, 0x95, 0x85, 0x89, 0x89, 0x98, 0x89 }; /* .sar ".x.YXY......" */
static const uint8_t ri_mag_334[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x22, 0x04, 0x55, 0x07, 0x77 }; /* .sc0 ".......".U.w" */
static const uint8_t ri_mag_335[] = { 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x07, 0x06, 0x40, 0x07, 0x77 }; /* .sc1 ".........@.w" */
static const uint8_t ri_mag_336[] = { 0xFE, 0x00, 0x00, 0xFF }; /* .sc3 "...." */
static const uint8_t ri_mag_337[] = { 0xFE, 0x00, 0x00, 0xFF }; /* .sc4 "...." */
static const uint8_t ri_mag_338[] = { 0xFE, 0x00, 0x00 }; /* .sc5 "..." */
static const uint8_t ri_mag_339[] = { 0xFE, 0x00, 0x00 }; /* .sc6 "..." */
static const uint8_t ri_mag_340[] = { 0xFE, 0x00, 0x00 }; /* .sc7 "..." */
static const uint8_t ri_mag_341[] = { 0xFE, 0x00, 0x00 }; /* .sc8 "..." */
static const uint8_t ri_mag_342[] = { 0xFE, 0x00, 0x00 }; /* .sca "..." */
static const uint8_t ri_mag_343[] = { 0xFE, 0x00, 0x00 }; /* .scc "..." */
static const uint8_t ri_mag_344[] = { 0xDD, 0xDD, 0x0D, 0x00, 0xDD, 0xDE, 0xED, 0xDE, 0xDD, 0x0E, 0xEE, 0xE0 }; /* .scs4 "............" */
static const uint8_t ri_mag_345[] = { 0x00, 0x00, 0x00, 0x00, 0x0D, 0xDD, 0x0F, 0xD2, 0x0F, 0xE2, 0x04, 0x44 }; /* .sd0 "...........D" */
static const uint8_t ri_mag_346[] = { 0x00, 0x00, 0x00, 0x00, 0x07, 0x77, 0x07, 0x04, 0x00, 0x07, 0x00, 0x00 }; /* .sd1 ".....w......" */
static const uint8_t ri_mag_347[] = { 0x00, 0x00, 0x00, 0x00, 0x07, 0x77, 0x05, 0x00, 0x07, 0x00, 0x00, 0x00 }; /* .sd2 ".....w......" */
static const uint8_t ri_mag_348[] = { 0x53, 0x31, 0x30, 0x31, 0x00, 0x1E, 0x00, 0x01, 0x0F, 0x02, 0x09, 0x0E }; /* .sfd "S101........" */
static const uint8_t ri_mag_349[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC0, 0x00 }; /* .sg3 "............" */
static const uint8_t ri_mag_350[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .sge "............" */
static const uint8_t ri_mag_351[] = { 0x99, 0xFA, 0xAC, 0xFA, 0xAA, 0xAA, 0xAA, 0x9A, 0xFA, 0xC9, 0x66, 0x96 }; /* .sh3 "..........f." */
static const uint8_t ri_mag_352[] = { 0x46, 0x4F, 0x52, 0x4D, 0x00, 0x00, 0xB4, 0x1A, 0x49, 0x4C, 0x42, 0x4D }; /* .sham "FORM....ILBM" */
static const uint8_t ri_mag_354[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x30 }; /* .sif ".........000" */
static const uint8_t ri_mag_355[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; /* .skp "............" */
static const uint8_t ri_mag_356[] = { 0x53, 0x50, 0x00, 0x00, 0x00, 0x00 }; /* .sps "SP...." */
static const uint8_t ri_mag_357[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .spu "............" */
static const uint8_t ri_mag_358[] = { 0x53, 0x50, 0x58 }; /* .spx "SPX" */
static const uint8_t ri_mag_359[] = { 0xFE, 0x00, 0x00 }; /* .sr5 "..." */
static const uint8_t ri_mag_360[] = { 0xFE, 0x00, 0x00, 0x00, 0x6A, 0x00, 0x00, 0x55, 0x55, 0x55, 0x55, 0x6A }; /* .sr6 "....j..UUUUj" */
static const uint8_t ri_mag_361[] = { 0xFE, 0x00, 0x00, 0x00, 0xD4, 0x00, 0x00 }; /* .sr8 "......." */
static const uint8_t ri_mag_362[] = { 0xFE, 0x00, 0x00, 0xFF, 0xD3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .srs "............" */
static const uint8_t ri_mag_363[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; /* .srt "............" */
static const uint8_t ri_mag_364[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .ss1 "............" */
static const uint8_t ri_mag_365[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .ss2 "............" */
static const uint8_t ri_mag_366[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; /* .ss3 "............" */
static const uint8_t ri_mag_367[] = { 0x28, 0x08, 0xFE, 0xA8, 0xA9, 0x57, 0x57, 0xFF, 0x90, 0x00, 0x00, 0x20 }; /* .ssb "(....WW.... " */
static const uint8_t ri_mag_368[] = { 0x2E, 0x20, 0x6E, 0x64, 0x00, 0x04, 0x64, 0x60, 0x00, 0x04, 0x64, 0x60 }; /* .stl ". nd..d`..d`" */
static const uint8_t ri_mag_369[] = { 0xAD, 0x00, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .stp "..y........." */
static const uint8_t ri_mag_370[] = { 0x00, 0x02, 0x07, 0x77, 0x00, 0x06, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .suh "...w........" */
static const uint8_t ri_mag_371[] = { 0x7F, 0x53, 0x58, 0x47, 0x03, 0x00, 0x00 }; /* .sxg ".SXG..." */
static const uint8_t ri_mag_372[] = { 0xFF, 0xFF, 0x00, 0x98, 0xFF, 0x9B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .sxs "............" */
static const uint8_t ri_mag_373[] = { 0x54, 0x52, 0x55, 0x45, 0x43, 0x4F, 0x4C, 0x52, 0x00, 0x01, 0xF4, 0xD8 }; /* .tcp "TRUECOLR...." */
static const uint8_t ri_mag_374[] = { 0x43, 0x4F, 0x4B, 0x45, 0x20, 0x66, 0x6F, 0x72, 0x6D, 0x61, 0x74, 0x2E }; /* .tg1 "COKE format." */
static const uint8_t ri_mag_375[] = { 0x10, 0x00, 0x00, 0x00 }; /* .tim "...." */
static const uint8_t ri_mag_376[] = { 0x00, 0x03, 0x00, 0x0E, 0x00, 0x0F, 0x00, 0x01, 0x01, 0x16, 0x01, 0x16 }; /* .timg "............" */
static const uint8_t ri_mag_377[] = { 0x54, 0x49, 0x50, 0x01, 0x00, 0xA0 }; /* .tip "TIP..." */
static const uint8_t ri_mag_378[] = { 0x00, 0x07, 0x77, 0x00, 0x00, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00, 0x07 }; /* .tn1 "..w........." */
static const uint8_t ri_mag_379[] = { 0x01, 0x07, 0x77, 0x07, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x07, 0x07 }; /* .tn2 "..w...p....." */
static const uint8_t ri_mag_380[] = { 0x02, 0x07, 0x77, 0x07, 0x00, 0x23, 0x40, 0x00, 0x00, 0x00, 0x01, 0x00 }; /* .tn3 "..w..#@....." */
static const uint8_t ri_mag_381[] = { 0x03, 0x8F, 0x03, 0x00, 0xF4, 0x00, 0x00, 0x01, 0x60, 0x01, 0x50, 0x01 }; /* .tn4 "........`.P." */
static const uint8_t ri_mag_382[] = { 0x50, 0x4E, 0x54, 0x00, 0x01, 0x00, 0x00 }; /* .tpi "PNT...." */
static const uint8_t ri_mag_383[] = { 0x54, 0x52, 0x55, 0x50, 0x01, 0x80, 0x00, 0xF0, 0x00, 0x00, 0x00, 0xE0 }; /* .trp "TRUP........" */
static const uint8_t ri_mag_384[] = { 0x49, 0x6E, 0x64, 0x79, 0x01, 0x80, 0x00, 0xF0, 0x00, 0x00, 0x00, 0x00 }; /* .tru "Indy........" */
static const uint8_t ri_mag_385[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00 }; /* .tx0 "............" */
static const uint8_t ri_mag_386[] = { 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x23, 0x33 }; /* .txe """""""""""#3" */
static const uint8_t ri_mag_387[] = { 0xFF, 0xFF, 0x00, 0x06, 0xFF, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .txs "............" */
static const uint8_t ri_mag_388[] = { 0x42, 0x4D, 0xCB, 0x02 }; /* .vbm "BM.." */
static const uint8_t ri_mag_389[] = { 0x00, 0x58, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F }; /* .vid ".X.........." */
static const uint8_t ri_mag_390[] = { 0x6B, 0x61, 0x74, 0x6F, 0x6E, 0x5F, 0x30, 0x2E, 0x67, 0x32, 0x66, 0x0D }; /* .vsc "katon_0.g2f." */
static const uint8_t ri_mag_391[] = { 0x96, 0x8A, 0xDC, 0x7B, 0x77, 0x8B, 0xDC, 0xDE, 0xCD, 0xDD, 0xDE, 0xEC }; /* .vzi "...{w......." */
static const uint8_t ri_mag_392[] = { 0x59, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* .wnd "Y6.........." */
static const uint8_t ri_mag_393[] = { 0xBD, 0xDF, 0xB5, 0x9F, 0xB5, 0x9F, 0xAD, 0x5F, 0xAD, 0x5F, 0xA5, 0x1F }; /* .xga "......._._.." */
static const uint8_t ri_mag_394[] = { 0xFE, 0x00, 0x00, 0xFF, 0xD3, 0x00, 0x00, 0x07, 0x07, 0x00, 0x00, 0x07 }; /* .yjk "............" */
static const uint8_t ri_mag_395[] = { 0x46, 0x4F, 0x52, 0x4D, 0x41, 0x54, 0x2D, 0x41, 0x00, 0x00, 0x00, 0x00 }; /* .zim "FORMAT-A...." */
static const uint8_t ri_mag_396[] = { 0x22, 0x22, 0x22, 0x21, 0x11, 0x22, 0x22, 0x33, 0x34, 0x33, 0x22, 0x22 }; /* .zm4 """"!.""343""" */
static const uint8_t ri_mag_397[] = { 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30 }; /* .zp1 "000000000000" */
static const uint8_t ri_mag_398[] = { 0xB0, 0xF0, 0x70, 0x00, 0x00, 0x00, 0x30, 0x30, 0xFC, 0x30, 0x30, 0x60 }; /* .zs "..p...00.00`" */
static const uint8_t ri_mag_399[] = { 0x5A, 0x58, 0x2D, 0x50, 0x61, 0x69, 0x6E, 0x74, 0x62, 0x72, 0x75, 0x73 }; /* .zxp "ZX-Paintbrus" */

#define RI_SIG_COUNT 400

static const ri_sig_entry_t ri_signatures[RI_SIG_COUNT] = {
    {"3", "3", ri_mag_000, 12, 18432, 18432, 1, RI_PLAT_OTHER, 1},
    {"3201", "Apple 3201 Color", ri_mag_001, 12, 30673, 30673, 1, RI_PLAT_APPLE_II, 1},
    {"4bt", "4BT", ri_mag_002, 12, 22580, 22580, 1, RI_PLAT_OTHER, 1},
    {"4mi", "4MI", ri_mag_003, 12, 244, 244, 1, RI_PLAT_OTHER, 1},
    {"4pl", "4PL", ri_mag_004, 12, 964, 964, 1, RI_PLAT_OTHER, 1},
    {"4pm", "4PM", ri_mag_005, 12, 1204, 1204, 1, RI_PLAT_OTHER, 1},
    {"64c", "C64 Image", ri_mag_006, 2, 505, 2050, 0, RI_PLAT_C64, 4},
    {"a", "A", ri_mag_007, 12, 8130, 8130, 1, RI_PLAT_OTHER, 1},
    {"a4r", "A4R", ri_mag_008, 12, 6550, 6550, 1, RI_PLAT_OTHER, 1},
    {"a64", "A64", ri_mag_009, 12, 10242, 10242, 1, RI_PLAT_OTHER, 1},
    {"aas", "AAS", ri_mag_010, 12, 9009, 9009, 1, RI_PLAT_OTHER, 1},
    {"abk", "AMOS Bank", ri_mag_011, 2, 952, 19340, 0, RI_PLAT_AMIGA, 4},
    {"acbm", "IFF ACBM", ri_mag_012, 6, 30834, 51444, 0, RI_PLAT_AMIGA, 3},
    {"acs", "ACS", ri_mag_013, 12, 1028, 1028, 1, RI_PLAT_OTHER, 1},
    {"afl", "AFL", ri_mag_014, 12, 16385, 16385, 1, RI_PLAT_OTHER, 1},
    {"agp", "AGP Image", NULL, 0, 7690, 7690, 1, RI_PLAT_AMIGA, 4},
    {"ags", "AGS Image", ri_mag_016, 3, 7696, 65552, 0, RI_PLAT_ATARI_ST, 3},
    {"all", "ALL", ri_mag_017, 12, 8157, 8157, 1, RI_PLAT_OTHER, 1},
    {"ami", "AMI", ri_mag_018, 12, 5668, 5668, 1, RI_PLAT_OTHER, 1},
    {"an2", "AN2", ri_mag_019, 12, 173, 173, 1, RI_PLAT_OTHER, 1},
    {"an4", "AN4", ri_mag_020, 12, 95, 95, 1, RI_PLAT_OTHER, 1},
    {"an5", "AN5", ri_mag_021, 12, 487, 487, 1, RI_PLAT_OTHER, 1},
    {"ap2", "Apple II Image", NULL, 0, 7680, 7680, 1, RI_PLAT_APPLE_II, 2},
    {"ap3", "Apple III Image", ri_mag_023, 12, 15872, 15872, 1, RI_PLAT_APPLE_II, 1},
    {"apa", "APA Image", ri_mag_024, 12, 7720, 7720, 1, RI_PLAT_ATARI_8BIT, 1},
    {"apc", "APC Image", NULL, 0, 7720, 7720, 1, RI_PLAT_ATARI_8BIT, 3},
    {"apl", "APL", ri_mag_026, 12, 1677, 1677, 1, RI_PLAT_OTHER, 1},
    {"app", "S101 Image", ri_mag_027, 7, 9271, 10144, 0, RI_PLAT_OTHER, 3},
    {"aps", "APS", ri_mag_028, 12, 4821, 4821, 1, RI_PLAT_OTHER, 1},
    {"apv", "APV", ri_mag_029, 12, 15360, 15360, 1, RI_PLAT_OTHER, 1},
    {"arv", "ARV", ri_mag_030, 12, 66426, 66426, 1, RI_PLAT_OTHER, 1},
    {"atr", "ATR", ri_mag_031, 12, 768, 768, 1, RI_PLAT_OTHER, 1},
    {"b&w", "B&W", ri_mag_032, 12, 81930, 81930, 1, RI_PLAT_OTHER, 1},
    {"b_w", "B_W", ri_mag_033, 12, 60190, 60190, 1, RI_PLAT_OTHER, 1},
    {"bb0", "BB0", ri_mag_034, 12, 20480, 20480, 1, RI_PLAT_OTHER, 1},
    {"bb1", "BB1", ri_mag_035, 12, 20480, 20480, 1, RI_PLAT_OTHER, 1},
    {"bb2", "BB2", ri_mag_036, 12, 20480, 20480, 1, RI_PLAT_OTHER, 1},
    {"bb4", "BB4", ri_mag_037, 12, 10240, 10240, 1, RI_PLAT_OTHER, 1},
    {"bb5", "BB5", ri_mag_038, 12, 10240, 10240, 1, RI_PLAT_OTHER, 1},
    {"bbg", "BBG", ri_mag_039, 12, 20064, 20064, 1, RI_PLAT_OTHER, 1},
    {"beam", "BEAM", ri_mag_040, 12, 45410, 45410, 1, RI_PLAT_OTHER, 1},
    {"bfli", "BFLI Image", ri_mag_041, 12, 33795, 33795, 1, RI_PLAT_C64, 1},
    {"bg9", "BG9", ri_mag_042, 12, 15360, 15360, 1, RI_PLAT_OTHER, 1},
    {"bgp", "BGP", ri_mag_043, 12, 19209, 19209, 1, RI_PLAT_OTHER, 1},
    {"bil", "Biolab Image", ri_mag_044, 2, 32032, 32034, 0, RI_PLAT_ATARI_ST, 2},
    {"bkg", "BKG", ri_mag_045, 12, 3856, 3856, 1, RI_PLAT_OTHER, 1},
    {"bl1", "BL1", ri_mag_046, 12, 7384, 7384, 1, RI_PLAT_OTHER, 1},
    {"bl2", "BL2", ri_mag_047, 12, 260, 260, 1, RI_PLAT_OTHER, 1},
    {"bl3", "BL3", ri_mag_048, 12, 134, 134, 1, RI_PLAT_OTHER, 1},
    {"bm", "BM", ri_mag_049, 12, 38408, 38408, 1, RI_PLAT_OTHER, 1},
    {"bmc4", "BMC4", ri_mag_050, 12, 11904, 11904, 1, RI_PLAT_OTHER, 1},
    {"bml", "Bitmap Loader", ri_mag_051, 12, 17474, 17474, 1, RI_PLAT_C64, 1},
    {"bp", "BP Image", ri_mag_052, 4, 4083, 4083, 1, RI_PLAT_OTHER, 2},
    {"bp1", "UIMG (1-bit)", ri_mag_053, 7, 16218, 16222, 0, RI_PLAT_OTHER, 4},
    {"bp2", "UIMG (2-bit)", ri_mag_054, 7, 32422, 32422, 1, RI_PLAT_OTHER, 3},
    {"bp4", "UIMG (4-bit)", ri_mag_055, 7, 64846, 64878, 0, RI_PLAT_OTHER, 4},
    {"bp6", "UIMG (6-bit)", ri_mag_056, 7, 97342, 97470, 0, RI_PLAT_OTHER, 2},
    {"bp8", "UIMG (8-bit)", ri_mag_057, 7, 130126, 130638, 0, RI_PLAT_OTHER, 2},
    {"bru", "BRU", ri_mag_058, 12, 64, 64, 1, RI_PLAT_OTHER, 1},
    {"bs", "BS", ri_mag_059, 12, 4643, 4643, 1, RI_PLAT_OTHER, 1},
    {"bsc", "BSC", ri_mag_060, 12, 11136, 11136, 1, RI_PLAT_OTHER, 1},
    {"bsp", "BSP", ri_mag_061, 12, 14900, 14900, 1, RI_PLAT_OTHER, 1},
    {"c01", "Canvas ST (1-bit)", ri_mag_062, 7, 16218, 129622, 0, RI_PLAT_ATARI_ST, 7},
    {"c02", "Canvas ST (2-bit)", ri_mag_063, 7, 32422, 129622, 0, RI_PLAT_ATARI_ST, 6},
    {"c04", "Canvas ST (4-bit)", ri_mag_064, 7, 64846, 129678, 0, RI_PLAT_ATARI_ST, 7},
    {"c06", "Canvas ST (6-bit)", ri_mag_065, 7, 129742, 129870, 0, RI_PLAT_ATARI_ST, 2},
    {"c08", "Canvas ST (8-bit)", ri_mag_066, 7, 130126, 130638, 0, RI_PLAT_ATARI_ST, 2},
    {"c16", "Canvas ST (16-bit)", ri_mag_067, 12, 64814, 64814, 1, RI_PLAT_ATARI_ST, 1},
    {"c24", "Canvas ST (24-bit)", ri_mag_068, 12, 97214, 97214, 1, RI_PLAT_ATARI_ST, 1},
    {"c32", "Canvas ST (32-bit)", ri_mag_069, 12, 129614, 129614, 1, RI_PLAT_ATARI_ST, 1},
    {"ca1", "CRACK Art (Low)", ri_mag_070, 4, 8584, 18598, 0, RI_PLAT_ATARI_ST, 2},
    {"ca2", "CRACK Art (Med)", ri_mag_071, 12, 9971, 9971, 1, RI_PLAT_ATARI_ST, 1},
    {"ca3", "CRACK Art (High)", ri_mag_072, 12, 28452, 28452, 1, RI_PLAT_ATARI_ST, 1},
    {"cci", "CIN v1.2", ri_mag_073, 8, 5913, 7042, 0, RI_PLAT_OTHER, 2},
    {"cdu", "CDU", ri_mag_074, 12, 10277, 10277, 1, RI_PLAT_OTHER, 1},
    {"ce1", "Canvas Extra (Low)", ri_mag_075, 12, 192022, 192022, 1, RI_PLAT_ATARI_ST, 1},
    {"ce2", "Canvas Extra (Med)", ri_mag_076, 12, 256022, 256022, 1, RI_PLAT_ATARI_ST, 1},
    {"ce3", "Canvas Extra (High)", ri_mag_077, 12, 256022, 256022, 1, RI_PLAT_ATARI_ST, 1},
    {"cel", "CEL", ri_mag_078, 12, 21520, 21520, 1, RI_PLAT_OTHER, 1},
    {"cfli", "CFLI Image", ri_mag_079, 2, 8170, 8170, 1, RI_PLAT_C64, 2},
    {"cgx", "CGX (RIFF-based)", ri_mag_080, 4, 30182, 240224, 0, RI_PLAT_AMIGA, 2},
    {"ch$", "CHR$ (ZX)", ri_mag_081, 6, 13831, 27655, 0, RI_PLAT_ZX_SPECTRUM, 2},
    {"ch4", "CHR 4-color", ri_mag_082, 12, 2048, 2048, 1, RI_PLAT_ZX_SPECTRUM, 1},
    {"ch6", "CHR 6-color", ri_mag_083, 12, 2048, 2048, 1, RI_PLAT_ZX_SPECTRUM, 1},
    {"ch8", "CHR 8-color", ri_mag_084, 12, 2048, 2048, 1, RI_PLAT_ZX_SPECTRUM, 1},
    {"che", "CHE", ri_mag_085, 12, 20482, 20482, 1, RI_PLAT_OTHER, 1},
    {"chr", "Character Set", ri_mag_086, 12, 3072, 3072, 1, RI_PLAT_GENERIC, 1},
    {"chs", "CHS", ri_mag_087, 12, 794, 794, 1, RI_PLAT_OTHER, 1},
    {"chx", "CHX Image", ri_mag_088, 5, 4222, 16165, 0, RI_PLAT_ZX_SPECTRUM, 3},
    {"cl0", "CL0", ri_mag_089, 12, 21758, 21758, 1, RI_PLAT_OTHER, 1},
    {"cl1", "CL1", ri_mag_090, 12, 9658, 9658, 1, RI_PLAT_OTHER, 1},
    {"cl2", "CL2", ri_mag_091, 12, 3914, 3914, 1, RI_PLAT_OTHER, 1},
    {"cle", "CLE", ri_mag_092, 12, 8194, 8194, 1, RI_PLAT_OTHER, 1},
    {"cm5", "CM5", ri_mag_093, 12, 2049, 2049, 1, RI_PLAT_OTHER, 1},
    {"cpi", "CPI", ri_mag_094, 12, 6947, 6947, 1, RI_PLAT_OTHER, 1},
    {"crg", "Calamus Raster", ri_mag_095, 12, 4381, 23784, 0, RI_PLAT_OTHER, 2},
    {"cs", "CS", ri_mag_096, 12, 5378, 5378, 1, RI_PLAT_OTHER, 1},
    {"ctm", "CTM Image", ri_mag_097, 4, 3300, 4875, 0, RI_PLAT_OTHER, 3},
    {"cut", "CUT", ri_mag_098, 12, 1188, 1188, 1, RI_PLAT_OTHER, 1},
    {"cwg", "CWG", ri_mag_099, 12, 10007, 10007, 1, RI_PLAT_OTHER, 1},
    {"da4", "DA4", ri_mag_100, 12, 64000, 64000, 1, RI_PLAT_OTHER, 1},
    {"dap", "DAPaint", ri_mag_101, 12, 77568, 77568, 1, RI_PLAT_ATARI_ST, 1},
    {"dc1", "DGC Image", ri_mag_102, 3, 20086, 65034, 0, RI_PLAT_OTHER, 3},
    {"dctv", "DCTV Image", ri_mag_103, 5, 56710, 231480, 0, RI_PLAT_AMIGA, 4},
    {"dd", "DD", ri_mag_104, 12, 9218, 9218, 1, RI_PLAT_OTHER, 1},
    {"deep", "IFF DEEP", ri_mag_105, 5, 80496, 545692, 0, RI_PLAT_AMIGA, 3},
    {"del", "DEL", ri_mag_106, 12, 12276, 12276, 1, RI_PLAT_OTHER, 1},
    {"dg1", "DG1", ri_mag_107, 12, 65032, 65032, 1, RI_PLAT_OTHER, 1},
    {"dgi", "DGI", ri_mag_108, 12, 15362, 15362, 1, RI_PLAT_OTHER, 1},
    {"dgp", "DGP", ri_mag_109, 12, 15362, 15362, 1, RI_PLAT_OTHER, 1},
    {"dhgr", "Double Hi-Res", ri_mag_110, 12, 16384, 16384, 1, RI_PLAT_APPLE_II, 1},
    {"din", "DIN", ri_mag_111, 12, 17351, 17351, 1, RI_PLAT_OTHER, 1},
    {"dit", "DIT", ri_mag_112, 12, 3845, 3845, 1, RI_PLAT_OTHER, 1},
    {"dlm", "DLM Image", ri_mag_113, 5, 256, 256, 1, RI_PLAT_C64, 2},
    {"dlp", "DLP", ri_mag_114, 12, 8931, 8931, 1, RI_PLAT_OTHER, 1},
    {"dol", "DOL", ri_mag_115, 12, 10242, 10242, 1, RI_PLAT_OTHER, 1},
    {"doo", "Doodle (Atari ST)", ri_mag_116, 12, 32000, 32000, 1, RI_PLAT_ATARI_ST, 1},
    {"dph", "DPH", ri_mag_117, 12, 34999, 34999, 1, RI_PLAT_OTHER, 1},
    {"dr", "DR", ri_mag_118, 12, 168688, 168688, 1, RI_PLAT_OTHER, 1},
    {"drg", "DRG", ri_mag_119, 12, 6400, 6400, 1, RI_PLAT_OTHER, 1},
    {"drl", "DRL", ri_mag_120, 12, 8931, 8931, 1, RI_PLAT_OTHER, 1},
    {"drp", "DRP", ri_mag_121, 12, 1246, 1246, 1, RI_PLAT_OTHER, 1},
    {"drz", "DRZ", ri_mag_122, 12, 10051, 10051, 1, RI_PLAT_OTHER, 1},
    {"du2", "DU2", ri_mag_123, 12, 113600, 113600, 1, RI_PLAT_OTHER, 1},
    {"duo", "DUO", ri_mag_124, 12, 113600, 113600, 1, RI_PLAT_OTHER, 1},
    {"ebd", "EBD", ri_mag_125, 12, 41008, 41008, 1, RI_PLAT_OTHER, 1},
    {"eci", "ECI", ri_mag_126, 12, 32770, 32770, 1, RI_PLAT_OTHER, 1},
    {"ecp", "ECP", ri_mag_127, 12, 12568, 12568, 1, RI_PLAT_OTHER, 1},
    {"esc", "ESC", ri_mag_128, 12, 15362, 15362, 1, RI_PLAT_OTHER, 1},
    {"esm", "TMS Image", ri_mag_129, 6, 32812, 452588, 0, RI_PLAT_OTHER, 3},
    {"eza", "EZA", ri_mag_130, 12, 25582, 25582, 1, RI_PLAT_OTHER, 1},
    {"f80", "F80", ri_mag_131, 12, 512, 512, 1, RI_PLAT_OTHER, 1},
    {"fbi", "FBI", ri_mag_132, 2, 5226, 7077, 0, RI_PLAT_OTHER, 2},
    {"ffli", "FFLI Image", ri_mag_133, 12, 26115, 26115, 1, RI_PLAT_C64, 1},
    {"fge", "FGE", ri_mag_134, 12, 1286, 1286, 1, RI_PLAT_OTHER, 1},
    {"fgs", "FGS", ri_mag_135, 12, 8002, 8002, 1, RI_PLAT_OTHER, 1},
    {"flf", "FIGlet Font", ri_mag_136, 7, 1057, 82238, 0, RI_PLAT_ATARI_ST, 16},
    {"fli", "FLI Image", ri_mag_137, 12, 17409, 17409, 1, RI_PLAT_C64, 1},
    {"fn2", "Font v2", ri_mag_138, 12, 2048, 2048, 1, RI_PLAT_GENERIC, 1},
    {"fp2", "FP2", ri_mag_139, 12, 17082, 17082, 1, RI_PLAT_OTHER, 1},
    {"fpr", "FPR", ri_mag_140, 12, 18370, 18370, 1, RI_PLAT_OTHER, 1},
    {"fpt", "FPT", ri_mag_141, 12, 10004, 10004, 1, RI_PLAT_OTHER, 1},
    {"ftc", "FTC Image", ri_mag_142, 12, 184320, 184320, 1, RI_PLAT_OTHER, 2},
    {"fwa", "FWA Image", ri_mag_143, 2, 8250, 8625, 0, RI_PLAT_OTHER, 2},
    {"g", "G", ri_mag_144, 12, 514, 514, 1, RI_PLAT_OTHER, 1},
    {"g09", "G09", ri_mag_145, 12, 15360, 15360, 1, RI_PLAT_OTHER, 1},
    {"g10", "G10", ri_mag_146, 12, 7689, 7689, 1, RI_PLAT_OTHER, 1},
    {"g11", "G11", ri_mag_147, 12, 7680, 7680, 1, RI_PLAT_OTHER, 1},
    {"g2f", "G2F (MSX)", ri_mag_148, 8, 1081, 10512, 0, RI_PLAT_MSX, 10},
    {"g9b", "G9B (MSX)", ri_mag_149, 5, 8896, 524304, 0, RI_PLAT_MSX, 8},
    {"g9s", "G9S", ri_mag_150, 12, 4774, 4774, 1, RI_PLAT_OTHER, 1},
    {"gb", "GB Image", ri_mag_151, 2, 5447, 13522, 0, RI_PLAT_OTHER, 2},
    {"gcd", "GCD", ri_mag_152, 12, 8194, 8194, 1, RI_PLAT_OTHER, 1},
    {"ge5", "GE5", ri_mag_153, 12, 30375, 30375, 1, RI_PLAT_OTHER, 1},
    {"ge7", "GE7", ri_mag_154, 12, 64384, 64384, 1, RI_PLAT_OTHER, 1},
    {"ge8", "GE8", ri_mag_155, 12, 54279, 54279, 1, RI_PLAT_OTHER, 1},
    {"ged", "GED", ri_mag_156, 12, 11302, 11302, 1, RI_PLAT_OTHER, 1},
    {"gfb", "GF2.5 Image", ri_mag_157, 6, 33556, 65556, 0, RI_PLAT_OTHER, 2},
    {"gfx", "GFX", ri_mag_158, 12, 18432, 18432, 1, RI_PLAT_OTHER, 1},
    {"gg", "GG Image", ri_mag_159, 2, 5978, 6656, 0, RI_PLAT_OTHER, 2},
    {"ghg", "GHG", ri_mag_160, 12, 2923, 2923, 1, RI_PLAT_OTHER, 1},
    {"gig", "GIG", ri_mag_161, 12, 10003, 10003, 1, RI_PLAT_OTHER, 1},
    {"gih", "GIH", ri_mag_162, 12, 8002, 8002, 1, RI_PLAT_OTHER, 1},
    {"gl8", "GL8", ri_mag_163, 12, 13572, 13572, 1, RI_PLAT_OTHER, 1},
    {"glc", "GLC", ri_mag_164, 12, 37792, 37792, 1, RI_PLAT_OTHER, 1},
    {"gls", "GLS", ri_mag_165, 12, 19884, 19884, 1, RI_PLAT_OTHER, 1},
    {"god", "GodPaint", ri_mag_166, 12, 153606, 153606, 1, RI_PLAT_ATARI_ST, 1},
    {"gr1", "GR1", ri_mag_167, 12, 480, 480, 1, RI_PLAT_OTHER, 1},
    {"gr2", "GR2", ri_mag_168, 12, 240, 240, 1, RI_PLAT_OTHER, 1},
    {"gr3", "GR3", ri_mag_169, 12, 244, 244, 1, RI_PLAT_OTHER, 1},
    {"gr7", "GR7", ri_mag_170, 12, 3844, 3844, 1, RI_PLAT_OTHER, 1},
    {"gr9", "Atari Graphics 9", ri_mag_171, 12, 7680, 7680, 1, RI_PLAT_ATARI_8BIT, 1},
    {"gr9p", "GR9P", ri_mag_172, 12, 2400, 2400, 1, RI_PLAT_OTHER, 1},
    {"gro", "GRO", ri_mag_173, 12, 6120, 6120, 1, RI_PLAT_OTHER, 1},
    {"grp", "GRP", ri_mag_174, 12, 14343, 14343, 1, RI_PLAT_OTHER, 1},
    {"gs", "GS", ri_mag_175, 12, 23776, 23776, 1, RI_PLAT_OTHER, 1},
    {"gun", "GunPaint", ri_mag_176, 2, 33603, 33603, 1, RI_PLAT_C64, 2},
    {"ham6", "Amiga HAM6", ri_mag_177, 12, 131376, 131376, 1, RI_PLAT_AMIGA, 1},
    {"ham8", "Amiga HAM8", ri_mag_178, 12, 262698, 262698, 1, RI_PLAT_AMIGA, 1},
    {"hbm", "HBM", ri_mag_179, 12, 8002, 8002, 1, RI_PLAT_OTHER, 1},
    {"hci", "HCI", ri_mag_180, 12, 16006, 16006, 1, RI_PLAT_OTHER, 1},
    {"hcm", "HCM Image", ri_mag_181, 6, 8208, 8208, 1, RI_PLAT_OTHER, 2},
    {"hed", "HED", ri_mag_182, 12, 9218, 9218, 1, RI_PLAT_OTHER, 1},
    {"het", "HET", ri_mag_183, 12, 9217, 9217, 1, RI_PLAT_OTHER, 1},
    {"hfc", "HFC", ri_mag_184, 12, 16386, 16386, 1, RI_PLAT_OTHER, 1},
    {"hfd", "HFD", ri_mag_185, 12, 16386, 16386, 1, RI_PLAT_OTHER, 1},
    {"hgr", "Hi-Res Graphics", ri_mag_186, 12, 8192, 8192, 1, RI_PLAT_APPLE_II, 1},
    {"him", "HIM", ri_mag_187, 2, 5523, 16385, 0, RI_PLAT_OTHER, 2},
    {"hlf", "HLF", ri_mag_188, 12, 24578, 24578, 1, RI_PLAT_OTHER, 1},
    {"hlr", "HLR Image", ri_mag_189, 12, 1628, 1628, 1, RI_PLAT_ZX_SPECTRUM, 3},
    {"hpc", "HPC", ri_mag_190, 12, 9003, 9003, 1, RI_PLAT_OTHER, 1},
    {"hpi", "HPI Image", ri_mag_191, 12, 8002, 8002, 1, RI_PLAT_C64, 1},
    {"hpk", "HPK", ri_mag_192, 12, 12260, 12260, 1, RI_PLAT_OTHER, 1},
    {"hpm", "HPM Image", ri_mag_193, 12, 3494, 3494, 1, RI_PLAT_C64, 1},
    {"hps", "HPS (Hard Interlace+Spectrum)", ri_mag_194, 12, 10865, 10865, 1, RI_PLAT_ATARI_ST, 1},
    {"hrg", "HRG", ri_mag_195, 12, 24578, 24578, 1, RI_PLAT_OTHER, 1},
    {"hrm", "HRM", ri_mag_196, 12, 92000, 92000, 1, RI_PLAT_OTHER, 1},
    {"hrs", "HRS", ri_mag_197, 12, 8021, 8021, 1, RI_PLAT_OTHER, 1},
    {"hs2", "HS2", ri_mag_198, 12, 94815, 94815, 1, RI_PLAT_OTHER, 1},
    {"ib3", "ICB3 Image", ri_mag_199, 12, 1600, 1600, 1, RI_PLAT_OTHER, 2},
    {"ibi", "IBI", ri_mag_200, 12, 704, 704, 1, RI_PLAT_OTHER, 1},
    {"ic1", "IC1", ri_mag_201, 12, 1524, 1524, 1, RI_PLAT_OTHER, 1},
    {"ic3", "IC3", ri_mag_202, 12, 4378, 4378, 1, RI_PLAT_OTHER, 1},
    {"iff", "IFF/ILBM", ri_mag_203, 5, 4052, 326334, 0, RI_PLAT_AMIGA, 18},
    {"ifl", "IFL", ri_mag_204, 12, 9216, 9216, 1, RI_PLAT_OTHER, 1},
    {"ige", "IGE", ri_mag_205, 12, 6160, 6160, 1, RI_PLAT_OTHER, 1},
    {"ihe", "IHE", ri_mag_206, 12, 16194, 16194, 1, RI_PLAT_OTHER, 1},
    {"iim", "Imagic Film/GEM Image", ri_mag_207, 9, 25616, 307216, 0, RI_PLAT_ATARI_ST, 6},
    {"ilc", "ILC", NULL, 0, 15360, 15360, 1, RI_PLAT_AMIGA, 2},
    {"ild", "ILD", ri_mag_209, 12, 8195, 8195, 1, RI_PLAT_OTHER, 1},
    {"ils", "ILS", ri_mag_210, 12, 9238, 9238, 1, RI_PLAT_OTHER, 1},
    {"imn", "IMN", ri_mag_211, 12, 17350, 17350, 1, RI_PLAT_OTHER, 1},
    {"info", "INFO", ri_mag_212, 12, 2958, 2958, 1, RI_PLAT_OTHER, 1},
    {"ing", "ING", ri_mag_213, 12, 16052, 16052, 1, RI_PLAT_OTHER, 1},
    {"ins", "INS", ri_mag_214, 12, 12928, 12928, 1, RI_PLAT_OTHER, 1},
    {"ip", "BRUS Image", ri_mag_215, 7, 19307, 26701, 0, RI_PLAT_OTHER, 3},
    {"ip2", "IP2", ri_mag_216, 12, 17358, 17358, 1, RI_PLAT_OTHER, 1},
    {"ipc", "IPC", ri_mag_217, 12, 17354, 17354, 1, RI_PLAT_OTHER, 1},
    {"iph", "IPH", ri_mag_218, 2, 9002, 9002, 1, RI_PLAT_OTHER, 2},
    {"ipt", "IPT", ri_mag_219, 12, 10003, 10003, 1, RI_PLAT_OTHER, 1},
    {"ir2", "IR2", ri_mag_220, 12, 18314, 18314, 1, RI_PLAT_OTHER, 1},
    {"irg", "IRG", ri_mag_221, 12, 18310, 18310, 1, RI_PLAT_OTHER, 1},
    {"ish", "ISH", ri_mag_222, 12, 9194, 9194, 1, RI_PLAT_OTHER, 1},
    {"ism", "ISM Image", ri_mag_223, 2, 10218, 10218, 1, RI_PLAT_OTHER, 2},
    {"ist", "IST", ri_mag_224, 12, 17184, 17184, 1, RI_PLAT_OTHER, 1},
    {"jgp", "JGP Image", ri_mag_225, 6, 2054, 2054, 1, RI_PLAT_OTHER, 2},
    {"jj", "JJ Image", ri_mag_226, 3, 1659, 6608, 0, RI_PLAT_OTHER, 2},
    {"kfx", "KFX", ri_mag_227, 12, 420, 420, 1, RI_PLAT_OTHER, 1},
    {"kid", "KID", ri_mag_228, 12, 63054, 63054, 1, RI_PLAT_OTHER, 1},
    {"koa", "Koala Painter", ri_mag_229, 12, 10003, 10003, 1, RI_PLAT_C64, 1},
    {"kpr", "KPR", ri_mag_230, 12, 2979, 2979, 1, RI_PLAT_OTHER, 1},
    {"kss", "KSS", ri_mag_231, 12, 6404, 6404, 1, RI_PLAT_OTHER, 1},
    {"lbm", "IFF/ILBM (LBM)", ri_mag_232, 4, 6474, 292950, 0, RI_PLAT_AMIGA, 8},
    {"lce", "LCE", ri_mag_233, 12, 49234, 49234, 1, RI_PLAT_OTHER, 1},
    {"ldm", "LDM Image", ri_mag_234, 12, 1241, 1601, 0, RI_PLAT_ZX_SPECTRUM, 2},
    {"leo", "LEO", ri_mag_235, 12, 2580, 2580, 1, RI_PLAT_OTHER, 1},
    {"lp3", "LP3 Image", ri_mag_236, 2, 4098, 4174, 0, RI_PLAT_OTHER, 2},
    {"lpk", "LPK", ri_mag_237, 12, 16921, 16921, 1, RI_PLAT_OTHER, 1},
    {"lum", "LUM", NULL, 0, 4766, 4766, 1, RI_PLAT_OTHER, 2},
    {"mag", "MAKI (Japanese)", ri_mag_239, 8, 1792, 115411, 0, RI_PLAT_JAPANESE_PC, 24},
    {"mbg", "MBG", ri_mag_240, 12, 16384, 16384, 1, RI_PLAT_OTHER, 1},
    {"mc", "MC", ri_mag_241, 12, 12288, 12288, 1, RI_PLAT_OTHER, 1},
    {"mci", "MCI Image", ri_mag_242, 12, 19434, 19434, 1, RI_PLAT_C64, 1},
    {"mcp", "MCP", ri_mag_243, 12, 16008, 16008, 1, RI_PLAT_OTHER, 1},
    {"mcpp", "MCPP", ri_mag_244, 12, 8008, 8008, 1, RI_PLAT_OTHER, 1},
    {"mcs", "MCS", NULL, 0, 10185, 10185, 1, RI_PLAT_OTHER, 2},
    {"mg", "MG Image", ri_mag_246, 12, 4097, 4097, 1, RI_PLAT_ZX_SPECTRUM, 2},
    {"mg1", "MG 1-color", ri_mag_247, 12, 19456, 19456, 1, RI_PLAT_ZX_SPECTRUM, 1},
    {"mg2", "MG 2-color", ri_mag_248, 12, 18688, 18688, 1, RI_PLAT_ZX_SPECTRUM, 1},
    {"mg4", "MG 4-color", ri_mag_249, 12, 15616, 15616, 1, RI_PLAT_ZX_SPECTRUM, 1},
    {"mg8", "MG 8-color", ri_mag_250, 12, 14080, 14080, 1, RI_PLAT_ZX_SPECTRUM, 1},
    {"mga", "MGA", ri_mag_251, 12, 7856, 7856, 1, RI_PLAT_OTHER, 1},
    {"mgp", "MGP Image", ri_mag_252, 5, 3845, 3845, 1, RI_PLAT_ZX_SPECTRUM, 2},
    {"mig", "MSX Image", ri_mag_253, 6, 1365, 80495, 0, RI_PLAT_MSX, 14},
    {"mil", "MIL", ri_mag_254, 12, 10022, 10022, 1, RI_PLAT_OTHER, 1},
    {"mis", "MIS", ri_mag_255, 12, 61, 61, 1, RI_PLAT_OTHER, 1},
    {"mki", "MAKI v01", ri_mag_256, 6, 16071, 57326, 0, RI_PLAT_JAPANESE_PC, 3},
    {"ml1", "ML1 Image", ri_mag_257, 4, 1800, 7991, 0, RI_PLAT_OTHER, 2},
    {"mle", "MLE", ri_mag_258, 12, 4098, 4098, 1, RI_PLAT_OTHER, 1},
    {"mlt", "MLT", ri_mag_259, 12, 12288, 12288, 1, RI_PLAT_OTHER, 1},
    {"mon", "MON", ri_mag_260, 12, 8194, 8194, 1, RI_PLAT_OTHER, 1},
    {"mp", "Amiga Multi-Palette", ri_mag_261, 5, 77000, 135454, 0, RI_PLAT_AMIGA, 2},
    {"mpk", "MPK", ri_mag_262, 12, 14445, 14445, 1, RI_PLAT_OTHER, 1},
    {"mpl", "MPL", ri_mag_263, 12, 129, 129, 1, RI_PLAT_OTHER, 1},
    {"mpp", "MPP Image", ri_mag_264, 3, 45648, 81434, 0, RI_PLAT_OTHER, 3},
    {"msl", "MSL", ri_mag_265, 12, 36, 36, 1, RI_PLAT_OTHER, 1},
    {"mur", "MUR", ri_mag_266, 12, 32000, 32000, 1, RI_PLAT_OTHER, 1},
    {"mx1", "MSX1 Screen", ri_mag_267, 4, 5289, 20623, 0, RI_PLAT_MSX, 5},
    {"nl3", "NL3 Image", ri_mag_268, 4, 3271, 3298, 0, RI_PLAT_OTHER, 2},
    {"nlq", "NLQ", ri_mag_269, 12, 1745, 1745, 1, RI_PLAT_OTHER, 1},
    {"nxi", "NXI", ri_mag_270, 12, 49664, 49664, 1, RI_PLAT_OTHER, 1},
    {"ocp", "OCP Art Studio", ri_mag_271, 12, 10018, 10018, 1, RI_PLAT_CPC, 1},
    {"odf", "ODF", ri_mag_272, 12, 1280, 1280, 1, RI_PLAT_OTHER, 1},
    {"p11", "P11 Image", ri_mag_273, 2, 3083, 3243, 0, RI_PLAT_ATARI_8BIT, 2},
    {"p3c", "P3C", ri_mag_274, 12, 48874, 48874, 1, RI_PLAT_OTHER, 1},
    {"p41", "P41", ri_mag_275, 12, 6155, 6155, 1, RI_PLAT_OTHER, 1},
    {"p64", "P64", ri_mag_276, 12, 10050, 10050, 1, RI_PLAT_OTHER, 1},
    {"pa3", "PA3", ri_mag_277, 12, 32079, 32079, 1, RI_PLAT_OTHER, 1},
    {"pac", "PAC", ri_mag_278, 12, 7285, 7285, 1, RI_PLAT_OTHER, 1},
    {"pbx", "PixelBox", ri_mag_279, 3, 32512, 46077, 0, RI_PLAT_ATARI_ST, 4},
    {"pc1", "Degas Elite Compressed (Low)", ri_mag_280, 12, 28222, 28222, 1, RI_PLAT_ATARI_ST, 1},
    {"pc2", "Degas Elite Compressed (Med)", ri_mag_281, 12, 7597, 7597, 1, RI_PLAT_ATARI_ST, 1},
    {"pc3", "Degas Elite Compressed (High)", ri_mag_282, 12, 13304, 13304, 1, RI_PLAT_ATARI_ST, 1},
    {"pci", "PCI", NULL, 0, 115648, 115648, 1, RI_PLAT_OTHER, 2},
    {"pcs", "PCS Image", ri_mag_284, 4, 99506, 101798, 0, RI_PLAT_OTHER, 3},
    {"pct", "PCT", ri_mag_285, 12, 27520, 27520, 1, RI_PLAT_OTHER, 1},
    {"pg", "PG", ri_mag_286, 12, 19351, 19351, 1, RI_PLAT_OTHER, 1},
    {"pg0", "PG0", ri_mag_287, 12, 39018, 39018, 1, RI_PLAT_OTHER, 1},
    {"pg1", "PG1", ri_mag_288, 12, 19062, 19062, 1, RI_PLAT_OTHER, 1},
    {"pg2", "PG2", ri_mag_289, 12, 4279, 4279, 1, RI_PLAT_OTHER, 1},
    {"pgc", "PGC", ri_mag_290, 12, 480, 480, 1, RI_PLAT_OTHER, 1},
    {"pgf", "PGF", ri_mag_291, 12, 1920, 1920, 1, RI_PLAT_OTHER, 1},
    {"pgr", "PGR Image", ri_mag_292, 4, 6191, 11350, 0, RI_PLAT_OTHER, 3},
    {"pi1", "Degas Elite (Low)", ri_mag_293, 2, 32066, 44834, 0, RI_PLAT_ATARI_ST, 4},
    {"pi2", "Degas Elite (Med)", ri_mag_294, 12, 32034, 32034, 1, RI_PLAT_ATARI_ST, 1},
    {"pi3", "Degas Elite (High)", ri_mag_295, 12, 32034, 32034, 1, RI_PLAT_ATARI_ST, 1},
    {"pi5", "Degas Elite (5-plane)", ri_mag_296, 12, 153634, 153634, 1, RI_PLAT_ATARI_ST, 1},
    {"pi7", "Degas Elite (7-plane)", ri_mag_297, 12, 308224, 308224, 1, RI_PLAT_ATARI_ST, 1},
    {"pi8", "Degas Elite (8-plane)", ri_mag_298, 12, 7680, 7685, 0, RI_PLAT_ATARI_ST, 2},
    {"pic0", "PIC0", ri_mag_299, 12, 3890, 3890, 1, RI_PLAT_OTHER, 1},
    {"pic1", "PIC1", ri_mag_300, 12, 244, 244, 1, RI_PLAT_OTHER, 1},
    {"pl4", "PL4", ri_mag_301, 12, 50859, 50859, 1, RI_PLAT_OTHER, 1},
    {"pl6", "PL6", ri_mag_302, 12, 256, 256, 1, RI_PLAT_OTHER, 1},
    {"pl7", "PL7", ri_mag_303, 2, 256, 256, 1, RI_PLAT_OTHER, 3},
    {"pla", "PLA", ri_mag_304, 12, 241, 241, 1, RI_PLAT_OTHER, 1},
    {"pls", "PLS", ri_mag_305, 12, 4271, 4271, 1, RI_PLAT_OTHER, 1},
    {"pmd", "PMD", ri_mag_306, 12, 4107, 4107, 1, RI_PLAT_OTHER, 1},
    {"pmg", "PMG", ri_mag_307, 12, 9332, 9332, 1, RI_PLAT_OTHER, 1},
    {"pp", "PP", ri_mag_308, 12, 33602, 33602, 1, RI_PLAT_OTHER, 1},
    {"psc", "tm89PS (MSX Screen)", ri_mag_309, 7, 18, 32016, 0, RI_PLAT_ATARI_ST, 5},
    {"psf", "PSF", ri_mag_310, 12, 573, 573, 1, RI_PLAT_OTHER, 1},
    {"pzm", "PZM", ri_mag_311, 12, 15362, 15362, 1, RI_PLAT_OTHER, 1},
    {"q4", "Q4", ri_mag_312, 12, 58352, 58352, 1, RI_PLAT_OTHER, 1},
    {"rag", "RAG Image (Canvas)", ri_mag_313, 9, 17432, 129432, 0, RI_PLAT_ATARI_ST, 5},
    {"ragc", "RAG Compressed", ri_mag_314, 12, 257054, 257054, 1, RI_PLAT_ATARI_ST, 1},
    {"rap", "RAP", ri_mag_315, 12, 7681, 7681, 1, RI_PLAT_OTHER, 1},
    {"rgh", "RGH", ri_mag_316, 12, 14410, 14410, 1, RI_PLAT_OTHER, 1},
    {"rip", "RIP Graphics", ri_mag_317, 3, 1495, 16044, 0, RI_PLAT_ATARI_8BIT, 8},
    {"rle", "RLE Graphics", ri_mag_318, 4, 3117, 18964, 0, RI_PLAT_OTHER, 2},
    {"rm2", "RM2", ri_mag_319, 12, 8192, 8192, 1, RI_PLAT_OTHER, 1},
    {"rm4", "RM4", ri_mag_320, 12, 5200, 5200, 1, RI_PLAT_OTHER, 1},
    {"rp", "RP", ri_mag_321, 12, 10242, 10242, 1, RI_PLAT_OTHER, 1},
    {"rpm", "RPM", ri_mag_322, 12, 10006, 10006, 1, RI_PLAT_OTHER, 1},
    {"rst", "RST", ri_mag_323, 12, 6800, 6800, 1, RI_PLAT_OTHER, 1},
    {"rwh", "RWH", ri_mag_324, 12, 256000, 256000, 1, RI_PLAT_OTHER, 1},
    {"rwl", "RWL", ri_mag_325, 12, 64000, 64000, 1, RI_PLAT_OTHER, 1},
    {"rys", "RYS", ri_mag_326, 12, 3840, 3840, 1, RI_PLAT_OTHER, 1},
    {"s15", "S15", ri_mag_327, 12, 30375, 30375, 1, RI_PLAT_OTHER, 1},
    {"s16", "S16", ri_mag_328, 12, 30351, 30351, 1, RI_PLAT_OTHER, 1},
    {"s17", "S17", ri_mag_329, 12, 64167, 64167, 1, RI_PLAT_OTHER, 1},
    {"s18", "S18", ri_mag_330, 12, 54279, 54279, 1, RI_PLAT_OTHER, 1},
    {"s1a", "S1A", ri_mag_331, 12, 64167, 64167, 1, RI_PLAT_OTHER, 1},
    {"s1c", "S1C Image", ri_mag_332, 3, 49159, 54280, 0, RI_PLAT_OTHER, 3},
    {"sar", "SAR", ri_mag_333, 12, 10219, 10219, 1, RI_PLAT_OTHER, 1},
    {"sc0", "SC0", ri_mag_334, 12, 15937, 15937, 1, RI_PLAT_OTHER, 1},
    {"sc1", "SC1", ri_mag_335, 12, 9658, 9658, 1, RI_PLAT_OTHER, 1},
    {"sc3", "MSX Screen 3", ri_mag_336, 4, 1543, 16391, 0, RI_PLAT_MSX, 2},
    {"sc4", "MSX Screen 4", ri_mag_337, 4, 14343, 32775, 0, RI_PLAT_MSX, 3},
    {"sc5", "MSX Screen 5", ri_mag_338, 3, 27143, 32775, 0, RI_PLAT_MSX, 5},
    {"sc6", "MSX Screen 6", ri_mag_339, 3, 22280, 32775, 0, RI_PLAT_MSX, 5},
    {"sc7", "MSX Screen 7", ri_mag_340, 3, 54279, 64264, 0, RI_PLAT_MSX, 5},
    {"sc8", "MSX Screen 8", ri_mag_341, 3, 54279, 64167, 0, RI_PLAT_MSX, 6},
    {"sca", "MSX Screen A", ri_mag_342, 3, 64167, 64384, 0, RI_PLAT_CPC, 6},
    {"scc", "MSX Screen C", ri_mag_343, 3, 49159, 64167, 0, RI_PLAT_MSX, 5},
    {"scs4", "SCS4", ri_mag_344, 12, 24617, 24617, 1, RI_PLAT_OTHER, 1},
    {"sd0", "SD0", ri_mag_345, 12, 32128, 32128, 1, RI_PLAT_OTHER, 1},
    {"sd1", "SD1", ri_mag_346, 12, 32128, 32128, 1, RI_PLAT_OTHER, 1},
    {"sd2", "SD2", ri_mag_347, 12, 32128, 32128, 1, RI_PLAT_OTHER, 1},
    {"sfd", "SFD", ri_mag_348, 12, 4774, 4774, 1, RI_PLAT_OTHER, 1},
    {"sg3", "SG3", ri_mag_349, 12, 240, 240, 1, RI_PLAT_OTHER, 1},
    {"sge", "SGE", ri_mag_350, 12, 960, 960, 1, RI_PLAT_OTHER, 1},
    {"sh3", "SH3", ri_mag_351, 12, 38400, 38400, 1, RI_PLAT_OTHER, 1},
    {"sham", "SHAM", ri_mag_352, 12, 46114, 46114, 1, RI_PLAT_OTHER, 1},
    {"shc", "SHC", NULL, 0, 17920, 17920, 1, RI_PLAT_OTHER, 2},
    {"sif", "SIF", ri_mag_354, 12, 2048, 2048, 1, RI_PLAT_OTHER, 1},
    {"skp", "SKP", ri_mag_355, 12, 7680, 7680, 1, RI_PLAT_OTHER, 1},
    {"sps", "Spectrum 512 Smooshed", ri_mag_356, 6, 20298, 39538, 0, RI_PLAT_ATARI_ST, 2},
    {"spu", "SPU", ri_mag_357, 12, 51104, 51104, 1, RI_PLAT_OTHER, 1},
    {"spx", "Spectrum 512 Extended", ri_mag_358, 3, 54178, 509669, 0, RI_PLAT_ATARI_ST, 3},
    {"sr5", "MSX Screen 5 (raw)", ri_mag_359, 3, 27136, 30471, 0, RI_PLAT_MSX, 4},
    {"sr6", "MSX Screen 6 (raw)", ri_mag_360, 12, 27144, 27144, 1, RI_PLAT_MSX, 1},
    {"sr8", "MSX Screen 8 (raw)", ri_mag_361, 7, 54279, 54400, 0, RI_PLAT_MSX, 2},
    {"srs", "SRS", ri_mag_362, 12, 54280, 54280, 1, RI_PLAT_OTHER, 1},
    {"srt", "SRT", ri_mag_363, 12, 32038, 32038, 1, RI_PLAT_OTHER, 1},
    {"ss1", "ScreenShot 1", ri_mag_364, 12, 7461, 7461, 1, RI_PLAT_CPC, 1},
    {"ss2", "ScreenShot 2", ri_mag_365, 12, 14885, 14885, 1, RI_PLAT_CPC, 1},
    {"ss3", "ScreenShot 3", ri_mag_366, 12, 24633, 24633, 1, RI_PLAT_CPC, 1},
    {"ssb", "SSB", ri_mag_367, 12, 32768, 32768, 1, RI_PLAT_OTHER, 1},
    {"stl", "STL", ri_mag_368, 12, 3072, 3072, 1, RI_PLAT_OTHER, 1},
    {"stp", "STP", ri_mag_369, 12, 5238, 5238, 1, RI_PLAT_OTHER, 1},
    {"suh", "SUH", ri_mag_370, 12, 32034, 32034, 1, RI_PLAT_OTHER, 1},
    {"sxg", "SXG Image", ri_mag_371, 7, 38926, 76944, 0, RI_PLAT_ZX_SPECTRUM, 2},
    {"sxs", "SXS Image", ri_mag_372, 12, 1030, 1030, 1, RI_PLAT_ZX_SPECTRUM, 3},
    {"tcp", "TCP", ri_mag_373, 12, 128216, 128216, 1, RI_PLAT_OTHER, 1},
    {"tg1", "TG1", ri_mag_374, 12, 128018, 128018, 1, RI_PLAT_OTHER, 1},
    {"tim", "TIM (PlayStation)", ri_mag_375, 4, 16928, 230420, 0, RI_PLAT_PS1, 4},
    {"timg", "TIMG", ri_mag_376, 12, 225697, 225697, 1, RI_PLAT_OTHER, 1},
    {"tip", "TIP Image", ri_mag_377, 6, 12009, 14289, 0, RI_PLAT_ATARI_8BIT, 4},
    {"tn1", "Tiny Low", ri_mag_378, 12, 11383, 11383, 1, RI_PLAT_ATARI_ST, 1},
    {"tn2", "Tiny Med", ri_mag_379, 12, 26274, 26274, 1, RI_PLAT_ATARI_ST, 1},
    {"tn3", "Tiny High", ri_mag_380, 12, 30978, 30978, 1, RI_PLAT_ATARI_ST, 1},
    {"tn4", "Tiny 4-plane", ri_mag_381, 12, 23801, 23801, 1, RI_PLAT_ATARI_ST, 1},
    {"tpi", "PNT Image", ri_mag_382, 7, 25824, 32152, 0, RI_PLAT_OTHER, 2},
    {"trp", "TRP", ri_mag_383, 12, 184328, 184328, 1, RI_PLAT_OTHER, 1},
    {"tru", "TRU", ri_mag_384, 12, 184576, 184576, 1, RI_PLAT_OTHER, 1},
    {"tx0", "TX0", ri_mag_385, 12, 257, 257, 1, RI_PLAT_OTHER, 1},
    {"txe", "TXE", ri_mag_386, 12, 3840, 3840, 1, RI_PLAT_OTHER, 1},
    {"txs", "TXS", ri_mag_387, 12, 262, 262, 1, RI_PLAT_OTHER, 1},
    {"vbm", "VBM Bitmap", ri_mag_388, 4, 8008, 98312, 0, RI_PLAT_OTHER, 3},
    {"vid", "VID", ri_mag_389, 12, 10050, 10050, 1, RI_PLAT_OTHER, 1},
    {"vsc", "VSC", ri_mag_390, 12, 26, 26, 1, RI_PLAT_OTHER, 1},
    {"vzi", "VZI", ri_mag_391, 12, 16000, 16000, 1, RI_PLAT_OTHER, 1},
    {"wnd", "WND", ri_mag_392, 12, 3072, 3072, 1, RI_PLAT_OTHER, 1},
    {"xga", "XGA", ri_mag_393, 12, 368640, 368640, 1, RI_PLAT_OTHER, 1},
    {"yjk", "YJK", ri_mag_394, 12, 54400, 54400, 1, RI_PLAT_OTHER, 1},
    {"zim", "ZIM", ri_mag_395, 12, 130060, 130060, 1, RI_PLAT_OTHER, 1},
    {"zm4", "ZM4", ri_mag_396, 12, 2048, 2048, 1, RI_PLAT_OTHER, 1},
    {"zp1", "ZP1", ri_mag_397, 12, 1536, 1536, 1, RI_PLAT_OTHER, 1},
    {"zs", "ZS", ri_mag_398, 12, 1026, 1026, 1, RI_PLAT_OTHER, 1},
    {"zxp", "ZX Paintbrush", ri_mag_399, 12, 51889, 68194, 0, RI_PLAT_ZX_SPECTRUM, 2},
};

/* ============================================================================
 * Lookup Functions
 * ============================================================================ */

/** Find all signatures matching given header bytes */
static inline uint16_t ri_find_by_magic(
    const uint8_t *data, size_t data_len,
    const ri_sig_entry_t **matches, uint16_t max_matches)
{
    uint16_t count = 0;
    for (uint16_t i = 0; i < RI_SIG_COUNT && count < max_matches; i++) {
        const ri_sig_entry_t *s = &ri_signatures[i];
        if (s->magic && s->magic_len > 0 && s->magic_len <= data_len) {
            if (memcmp(data, s->magic, s->magic_len) == 0)
                matches[count++] = s;
        }
    }
    return count;
}

/** Find signature by file extension */
static inline const ri_sig_entry_t *ri_find_by_ext(const char *ext)
{
    for (uint16_t i = 0; i < RI_SIG_COUNT; i++) {
        if (strcmp(ri_signatures[i].ext, ext) == 0)
            return &ri_signatures[i];
    }
    return NULL;
}

/** Find all signatures for a specific platform */
static inline uint16_t ri_find_by_platform(
    ri_platform_t platform,
    const ri_sig_entry_t **matches, uint16_t max_matches)
{
    uint16_t count = 0;
    for (uint16_t i = 0; i < RI_SIG_COUNT && count < max_matches; i++) {
        if (ri_signatures[i].platform == platform)
            matches[count++] = &ri_signatures[i];
    }
    return count;
}

/**
 * Multi-factor format detection.
 * Combines magic bytes, file extension, and file size for best match.
 * Returns NULL if no reasonable match found.
 */
static inline const ri_sig_entry_t *ri_detect(
    const uint8_t *data, size_t data_len,
    uint32_t file_size, const char *ext)
{
    const ri_sig_entry_t *best = NULL;
    int best_score = 0;

    for (uint16_t i = 0; i < RI_SIG_COUNT; i++) {
        const ri_sig_entry_t *s = &ri_signatures[i];
        int score = 0;

        /* Magic bytes match: strongest signal */
        if (s->magic && s->magic_len > 0 && s->magic_len <= data_len) {
            if (memcmp(data, s->magic, s->magic_len) == 0)
                score += s->magic_len * 10;  /* longer magic = higher confidence */
        }

        /* Extension match */
        if (ext && strcmp(s->ext, ext) == 0)
            score += 20;

        /* Exact size match on fixed-size format: very strong */
        if (file_size > 0) {
            if (s->fixed_size && file_size == s->min_size)
                score += 30;
            else if (!s->fixed_size && file_size >= s->min_size && file_size <= s->max_size)
                score += 10;
        }

        if (score > best_score) {
            best_score = score;
            best = s;
        }
    }

    /* Require minimum confidence */
    return (best_score >= 20) ? best : NULL;
}

/** Get detection confidence as percentage (0-100) */
static inline int ri_detect_confidence(
    const ri_sig_entry_t *sig,
    const uint8_t *data, size_t data_len,
    uint32_t file_size, const char *ext)
{
    if (!sig) return 0;
    int score = 0;
    if (sig->magic && sig->magic_len > 0 && sig->magic_len <= data_len)
        if (memcmp(data, sig->magic, sig->magic_len) == 0)
            score += 15 + sig->magic_len * 8;
    if (ext && strcmp(sig->ext, ext) == 0)
        score += 25;
    if (file_size > 0 && sig->fixed_size && file_size == sig->min_size)
        score += 20;
    else if (file_size > 0 && file_size >= sig->min_size && file_size <= sig->max_size)
        score += 10;
    return (score > 100) ? 100 : score;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_RETRO_IMAGE_SIGS_H */