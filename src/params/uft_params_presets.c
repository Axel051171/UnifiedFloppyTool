/**
 * @file uft_params_presets.c
 * @brief Extended Preset Definitions (50+ Presets)
 * 
 * PRESET CATEGORIES:
 * - PC/DOS (8 presets)
 * - Commodore (6 presets)
 * - Amiga (4 presets)
 * - Atari (6 presets)
 * - Apple (5 presets)
 * - BBC/Acorn (4 presets)
 * - TRS-80 (4 presets)
 * - MSX (2 presets)
 * - Amstrad CPC (3 presets)
 * - Spectrum (2 presets)
 * - PC-98 (4 presets)
 * - Flux/Preservation (4 presets)
 */

#include "uft/params/uft_canonical_params.h"
#include <string.h>

// ============================================================================
// PRESET DEFINITION STRUCTURE (Extended)
// ============================================================================

typedef struct {
    const char      *name;              // Unique identifier
    const char      *display_name;      // Human-readable name
    const char      *description;       // Detailed description
    const char      *category;          // For GUI grouping
    
    // Format
    uft_format_e    format;
    uft_encoding_e  encoding;
    uft_density_e   density;
    
    // Geometry
    int32_t         cylinders;
    int32_t         heads;
    int32_t         sectors_per_track;  // 0 = variable (GCR)
    int32_t         sector_size;
    int32_t         sector_base;        // First sector number
    
    // Timing
    uint32_t        datarate_bps;       // 0 = computed from encoding
    uint64_t        cell_time_ns;       // 0 = computed from datarate
    double          rpm;
    
    // Special
    uint32_t        total_size;         // Expected file size (for validation)
    uint32_t        flags;              // Special flags
} uft_preset_def_t;

// Flags
#define PRESET_FLAG_VARIABLE_SPT    (1 << 0)    // Variable sectors per track
#define PRESET_FLAG_HALF_TRACKS     (1 << 1)    // Uses half-tracks
#define PRESET_FLAG_FLUX            (1 << 2)    // Flux format
#define PRESET_FLAG_COMPRESSED      (1 << 3)    // Compressed format
#define PRESET_FLAG_ERROR_MAP       (1 << 4)    // Has error map
#define PRESET_FLAG_VARIABLE_RPM    (1 << 5)    // Variable RPM (e.g. Victor 9000)

// ============================================================================
// PRESET DATABASE (52 Presets)
// ============================================================================

static const uft_preset_def_t PRESET_DATABASE[] = {
    // ========================================================================
    // PC / DOS (8 presets)
    // ========================================================================
    {
        .name = "pc_160k",
        .display_name = "PC 160K (5.25\" SS/DD)",
        .description = "IBM PC 160K single-sided double-density",
        .category = "PC/DOS",
        .format = UFT_FORMAT_IMG, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 40, .heads = 1, .sectors_per_track = 8, .sector_size = 512, .sector_base = 1,
        .datarate_bps = 250000, .rpm = 300.0, .total_size = 163840
    },
    {
        .name = "pc_180k",
        .display_name = "PC 180K (5.25\" SS/DD)",
        .description = "IBM PC 180K single-sided double-density",
        .category = "PC/DOS",
        .format = UFT_FORMAT_IMG, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 40, .heads = 1, .sectors_per_track = 9, .sector_size = 512, .sector_base = 1,
        .datarate_bps = 250000, .rpm = 300.0, .total_size = 184320
    },
    {
        .name = "pc_320k",
        .display_name = "PC 320K (5.25\" DS/DD)",
        .description = "IBM PC 320K double-sided double-density",
        .category = "PC/DOS",
        .format = UFT_FORMAT_IMG, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 40, .heads = 2, .sectors_per_track = 8, .sector_size = 512, .sector_base = 1,
        .datarate_bps = 250000, .rpm = 300.0, .total_size = 327680
    },
    {
        .name = "pc_360k",
        .display_name = "PC 360K (5.25\" DS/DD)",
        .description = "IBM PC 360K double-sided double-density",
        .category = "PC/DOS",
        .format = UFT_FORMAT_IMG, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 40, .heads = 2, .sectors_per_track = 9, .sector_size = 512, .sector_base = 1,
        .datarate_bps = 250000, .rpm = 300.0, .total_size = 368640
    },
    {
        .name = "pc_720k",
        .display_name = "PC 720K (3.5\" DS/DD)",
        .description = "IBM PC 720K double-sided double-density",
        .category = "PC/DOS",
        .format = UFT_FORMAT_IMG, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 80, .heads = 2, .sectors_per_track = 9, .sector_size = 512, .sector_base = 1,
        .datarate_bps = 250000, .rpm = 300.0, .total_size = 737280
    },
    {
        .name = "pc_1200k",
        .display_name = "PC 1.2M (5.25\" DS/HD)",
        .description = "IBM PC 1.2M double-sided high-density",
        .category = "PC/DOS",
        .format = UFT_FORMAT_IMG, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_HD,
        .cylinders = 80, .heads = 2, .sectors_per_track = 15, .sector_size = 512, .sector_base = 1,
        .datarate_bps = 500000, .rpm = 360.0, .total_size = 1228800
    },
    {
        .name = "pc_1440k",
        .display_name = "PC 1.44M (3.5\" DS/HD)",
        .description = "IBM PC 1.44M double-sided high-density",
        .category = "PC/DOS",
        .format = UFT_FORMAT_IMG, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_HD,
        .cylinders = 80, .heads = 2, .sectors_per_track = 18, .sector_size = 512, .sector_base = 1,
        .datarate_bps = 500000, .rpm = 300.0, .total_size = 1474560
    },
    {
        .name = "pc_2880k",
        .display_name = "PC 2.88M (3.5\" DS/ED)",
        .description = "IBM PC 2.88M double-sided extra-density",
        .category = "PC/DOS",
        .format = UFT_FORMAT_IMG, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_ED,
        .cylinders = 80, .heads = 2, .sectors_per_track = 36, .sector_size = 512, .sector_base = 1,
        .datarate_bps = 1000000, .rpm = 300.0, .total_size = 2949120
    },
    
    // ========================================================================
    // Commodore (6 presets)
    // ========================================================================
    {
        .name = "c64_d64_35",
        .display_name = "C64 D64 (35 Track)",
        .description = "Commodore 64 standard 35-track disk",
        .category = "Commodore",
        .format = UFT_FORMAT_D64, .encoding = UFT_ENC_GCR_CBM, .density = UFT_DENSITY_DD,
        .cylinders = 35, .heads = 1, .sectors_per_track = 0, .sector_size = 256, .sector_base = 0,
        .datarate_bps = 0, .cell_time_ns = 3200, .rpm = 300.0, .total_size = 174848,
        .flags = PRESET_FLAG_VARIABLE_SPT
    },
    {
        .name = "c64_d64_35_err",
        .display_name = "C64 D64 (35 Track + Errors)",
        .description = "Commodore 64 35-track with error map",
        .category = "Commodore",
        .format = UFT_FORMAT_D64, .encoding = UFT_ENC_GCR_CBM, .density = UFT_DENSITY_DD,
        .cylinders = 35, .heads = 1, .sectors_per_track = 0, .sector_size = 256, .sector_base = 0,
        .datarate_bps = 0, .cell_time_ns = 3200, .rpm = 300.0, .total_size = 175531,
        .flags = PRESET_FLAG_VARIABLE_SPT | PRESET_FLAG_ERROR_MAP
    },
    {
        .name = "c64_d64_40",
        .display_name = "C64 D64 (40 Track)",
        .description = "Commodore 64 extended 40-track disk",
        .category = "Commodore",
        .format = UFT_FORMAT_D64, .encoding = UFT_ENC_GCR_CBM, .density = UFT_DENSITY_DD,
        .cylinders = 40, .heads = 1, .sectors_per_track = 0, .sector_size = 256, .sector_base = 0,
        .datarate_bps = 0, .cell_time_ns = 3200, .rpm = 300.0, .total_size = 196608,
        .flags = PRESET_FLAG_VARIABLE_SPT
    },
    {
        .name = "c64_g64",
        .display_name = "C64 G64 (GCR Flux)",
        .description = "Commodore 64 GCR flux dump",
        .category = "Commodore",
        .format = UFT_FORMAT_G64, .encoding = UFT_ENC_GCR_CBM, .density = UFT_DENSITY_DD,
        .cylinders = 42, .heads = 1, .sectors_per_track = 0, .sector_size = 0, .sector_base = 0,
        .datarate_bps = 0, .cell_time_ns = 3200, .rpm = 300.0, .total_size = 0,
        .flags = PRESET_FLAG_VARIABLE_SPT | PRESET_FLAG_HALF_TRACKS | PRESET_FLAG_FLUX
    },
    {
        .name = "c128_d71",
        .display_name = "C128 D71 (Double-Sided)",
        .description = "Commodore 128 double-sided disk",
        .category = "Commodore",
        .format = UFT_FORMAT_D71, .encoding = UFT_ENC_GCR_CBM, .density = UFT_DENSITY_DD,
        .cylinders = 35, .heads = 2, .sectors_per_track = 0, .sector_size = 256, .sector_base = 0,
        .datarate_bps = 0, .cell_time_ns = 3200, .rpm = 300.0, .total_size = 349696,
        .flags = PRESET_FLAG_VARIABLE_SPT
    },
    {
        .name = "c128_d81",
        .display_name = "C128 D81 (3.5\" 800K)",
        .description = "Commodore 128/1581 3.5\" disk",
        .category = "Commodore",
        .format = UFT_FORMAT_D81, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 80, .heads = 2, .sectors_per_track = 10, .sector_size = 512, .sector_base = 1,
        .datarate_bps = 250000, .rpm = 300.0, .total_size = 819200
    },
    
    // ========================================================================
    // Amiga (4 presets)
    // ========================================================================
    {
        .name = "amiga_dd",
        .display_name = "Amiga DD (880K)",
        .description = "Amiga 880K double-density",
        .category = "Amiga",
        .format = UFT_FORMAT_ADF, .encoding = UFT_ENC_AMIGA_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 80, .heads = 2, .sectors_per_track = 11, .sector_size = 512, .sector_base = 0,
        .datarate_bps = 250000, .rpm = 300.0, .total_size = 901120
    },
    {
        .name = "amiga_hd",
        .display_name = "Amiga HD (1.76M)",
        .description = "Amiga 1.76M high-density",
        .category = "Amiga",
        .format = UFT_FORMAT_ADF, .encoding = UFT_ENC_AMIGA_MFM, .density = UFT_DENSITY_HD,
        .cylinders = 80, .heads = 2, .sectors_per_track = 22, .sector_size = 512, .sector_base = 0,
        .datarate_bps = 500000, .rpm = 300.0, .total_size = 1802240
    },
    {
        .name = "amiga_dd_pc",
        .display_name = "Amiga PC-Compatible (720K)",
        .description = "Amiga reading PC 720K disks",
        .category = "Amiga",
        .format = UFT_FORMAT_ADF, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 80, .heads = 2, .sectors_per_track = 9, .sector_size = 512, .sector_base = 1,
        .datarate_bps = 250000, .rpm = 300.0, .total_size = 737280
    },
    {
        .name = "amiga_hd_pc",
        .display_name = "Amiga PC-Compatible (1.44M)",
        .description = "Amiga reading PC 1.44M disks",
        .category = "Amiga",
        .format = UFT_FORMAT_ADF, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_HD,
        .cylinders = 80, .heads = 2, .sectors_per_track = 18, .sector_size = 512, .sector_base = 1,
        .datarate_bps = 500000, .rpm = 300.0, .total_size = 1474560
    },
    
    // ========================================================================
    // Atari (6 presets)
    // ========================================================================
    {
        .name = "atari_st_ss",
        .display_name = "Atari ST SS (360K)",
        .description = "Atari ST single-sided",
        .category = "Atari",
        .format = UFT_FORMAT_ST, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 80, .heads = 1, .sectors_per_track = 9, .sector_size = 512, .sector_base = 1,
        .datarate_bps = 250000, .rpm = 300.0, .total_size = 368640
    },
    {
        .name = "atari_st_ds",
        .display_name = "Atari ST DS (720K)",
        .description = "Atari ST double-sided",
        .category = "Atari",
        .format = UFT_FORMAT_ST, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 80, .heads = 2, .sectors_per_track = 9, .sector_size = 512, .sector_base = 1,
        .datarate_bps = 250000, .rpm = 300.0, .total_size = 737280
    },
    {
        .name = "atari_st_10sec",
        .display_name = "Atari ST 10-Sector (800K)",
        .description = "Atari ST extended 10-sector format",
        .category = "Atari",
        .format = UFT_FORMAT_ST, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 80, .heads = 2, .sectors_per_track = 10, .sector_size = 512, .sector_base = 1,
        .datarate_bps = 250000, .rpm = 300.0, .total_size = 819200
    },
    {
        .name = "atari_st_11sec",
        .display_name = "Atari ST 11-Sector (880K)",
        .description = "Atari ST extended 11-sector format",
        .category = "Atari",
        .format = UFT_FORMAT_ST, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 80, .heads = 2, .sectors_per_track = 11, .sector_size = 512, .sector_base = 1,
        .datarate_bps = 250000, .rpm = 300.0, .total_size = 901120
    },
    {
        .name = "atari_8bit_sd",
        .display_name = "Atari 8-bit SD (90K)",
        .description = "Atari 400/800/XL/XE single-density",
        .category = "Atari",
        .format = UFT_FORMAT_ATR, .encoding = UFT_ENC_FM, .density = UFT_DENSITY_SD,
        .cylinders = 40, .heads = 1, .sectors_per_track = 18, .sector_size = 128, .sector_base = 1,
        .datarate_bps = 125000, .rpm = 288.0, .total_size = 92176
    },
    {
        .name = "atari_8bit_ed",
        .display_name = "Atari 8-bit ED (130K)",
        .description = "Atari 400/800/XL/XE enhanced-density",
        .category = "Atari",
        .format = UFT_FORMAT_ATR, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 40, .heads = 1, .sectors_per_track = 26, .sector_size = 128, .sector_base = 1,
        .datarate_bps = 250000, .rpm = 288.0, .total_size = 133136
    },
    
    // ========================================================================
    // Apple (5 presets)
    // ========================================================================
    {
        .name = "apple2_dos33",
        .display_name = "Apple II DOS 3.3 (140K)",
        .description = "Apple II DOS 3.3 format",
        .category = "Apple",
        .format = UFT_FORMAT_DO, .encoding = UFT_ENC_GCR_APPLE, .density = UFT_DENSITY_DD,
        .cylinders = 35, .heads = 1, .sectors_per_track = 16, .sector_size = 256, .sector_base = 0,
        .datarate_bps = 0, .cell_time_ns = 4000, .rpm = 300.0, .total_size = 143360
    },
    {
        .name = "apple2_prodos",
        .display_name = "Apple II ProDOS (140K)",
        .description = "Apple II ProDOS format",
        .category = "Apple",
        .format = UFT_FORMAT_PO, .encoding = UFT_ENC_GCR_APPLE, .density = UFT_DENSITY_DD,
        .cylinders = 35, .heads = 1, .sectors_per_track = 16, .sector_size = 256, .sector_base = 0,
        .datarate_bps = 0, .cell_time_ns = 4000, .rpm = 300.0, .total_size = 143360
    },
    {
        .name = "apple2_nib",
        .display_name = "Apple II Nibble (232K)",
        .description = "Apple II raw nibble dump",
        .category = "Apple",
        .format = UFT_FORMAT_NIB, .encoding = UFT_ENC_GCR_APPLE, .density = UFT_DENSITY_DD,
        .cylinders = 35, .heads = 1, .sectors_per_track = 0, .sector_size = 0, .sector_base = 0,
        .datarate_bps = 0, .cell_time_ns = 4000, .rpm = 300.0, .total_size = 232960,
        .flags = PRESET_FLAG_FLUX
    },
    {
        .name = "apple2_woz",
        .display_name = "Apple II WOZ (Flux)",
        .description = "Apple II WOZ flux format",
        .category = "Apple",
        .format = UFT_FORMAT_WOZ, .encoding = UFT_ENC_GCR_APPLE, .density = UFT_DENSITY_DD,
        .cylinders = 35, .heads = 1, .sectors_per_track = 0, .sector_size = 0, .sector_base = 0,
        .datarate_bps = 0, .cell_time_ns = 4000, .rpm = 300.0, .total_size = 0,
        .flags = PRESET_FLAG_FLUX | PRESET_FLAG_HALF_TRACKS
    },
    {
        .name = "apple2_13sec",
        .display_name = "Apple II 13-Sector (113K)",
        .description = "Apple II DOS 3.2 13-sector format",
        .category = "Apple",
        .format = UFT_FORMAT_DO, .encoding = UFT_ENC_GCR_APPLE, .density = UFT_DENSITY_DD,
        .cylinders = 35, .heads = 1, .sectors_per_track = 13, .sector_size = 256, .sector_base = 0,
        .datarate_bps = 0, .cell_time_ns = 4000, .rpm = 300.0, .total_size = 116480
    },
    
    // ========================================================================
    // BBC / Acorn (4 presets)
    // ========================================================================
    {
        .name = "bbc_dfs_ss40",
        .display_name = "BBC Micro DFS SS/40 (100K)",
        .description = "BBC Micro DFS single-sided 40-track",
        .category = "BBC/Acorn",
        .format = UFT_FORMAT_SSD, .encoding = UFT_ENC_FM, .density = UFT_DENSITY_SD,
        .cylinders = 40, .heads = 1, .sectors_per_track = 10, .sector_size = 256, .sector_base = 0,
        .datarate_bps = 125000, .rpm = 300.0, .total_size = 102400
    },
    {
        .name = "bbc_dfs_ss80",
        .display_name = "BBC Micro DFS SS/80 (200K)",
        .description = "BBC Micro DFS single-sided 80-track",
        .category = "BBC/Acorn",
        .format = UFT_FORMAT_SSD, .encoding = UFT_ENC_FM, .density = UFT_DENSITY_SD,
        .cylinders = 80, .heads = 1, .sectors_per_track = 10, .sector_size = 256, .sector_base = 0,
        .datarate_bps = 125000, .rpm = 300.0, .total_size = 204800
    },
    {
        .name = "bbc_dfs_ds80",
        .display_name = "BBC Micro DFS DS/80 (400K)",
        .description = "BBC Micro DFS double-sided 80-track",
        .category = "BBC/Acorn",
        .format = UFT_FORMAT_DSD, .encoding = UFT_ENC_FM, .density = UFT_DENSITY_SD,
        .cylinders = 80, .heads = 2, .sectors_per_track = 10, .sector_size = 256, .sector_base = 0,
        .datarate_bps = 125000, .rpm = 300.0, .total_size = 409600
    },
    {
        .name = "acorn_adfs_s",
        .display_name = "Acorn ADFS S (160K)",
        .description = "Acorn ADFS small format",
        .category = "BBC/Acorn",
        .format = UFT_FORMAT_ADF_ACORN, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 40, .heads = 1, .sectors_per_track = 16, .sector_size = 256, .sector_base = 0,
        .datarate_bps = 250000, .rpm = 300.0, .total_size = 163840
    },
    
    // ========================================================================
    // TRS-80 (4 presets)
    // ========================================================================
    {
        .name = "trs80_sd",
        .display_name = "TRS-80 SD (89K)",
        .description = "TRS-80 Model I/III single-density",
        .category = "TRS-80",
        .format = UFT_FORMAT_JV1, .encoding = UFT_ENC_FM, .density = UFT_DENSITY_SD,
        .cylinders = 35, .heads = 1, .sectors_per_track = 10, .sector_size = 256, .sector_base = 0,
        .datarate_bps = 125000, .rpm = 300.0, .total_size = 89600
    },
    {
        .name = "trs80_dd",
        .display_name = "TRS-80 DD (180K)",
        .description = "TRS-80 Model III/4 double-density",
        .category = "TRS-80",
        .format = UFT_FORMAT_JV3, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 40, .heads = 1, .sectors_per_track = 18, .sector_size = 256, .sector_base = 0,
        .datarate_bps = 250000, .rpm = 300.0, .total_size = 184320
    },
    {
        .name = "trs80_dmk",
        .display_name = "TRS-80 DMK",
        .description = "TRS-80 DMK raw format",
        .category = "TRS-80",
        .format = UFT_FORMAT_DMK, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 40, .heads = 1, .sectors_per_track = 0, .sector_size = 0, .sector_base = 0,
        .datarate_bps = 250000, .rpm = 300.0, .total_size = 0,
        .flags = PRESET_FLAG_FLUX
    },
    {
        .name = "trs80_4_ds",
        .display_name = "TRS-80 Model 4 DS (360K)",
        .description = "TRS-80 Model 4 double-sided",
        .category = "TRS-80",
        .format = UFT_FORMAT_JV3, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 40, .heads = 2, .sectors_per_track = 18, .sector_size = 256, .sector_base = 0,
        .datarate_bps = 250000, .rpm = 300.0, .total_size = 368640
    },
    
    // ========================================================================
    // MSX (2 presets)
    // ========================================================================
    {
        .name = "msx_ss",
        .display_name = "MSX SS (360K)",
        .description = "MSX single-sided",
        .category = "MSX",
        .format = UFT_FORMAT_MSX_DSK, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 80, .heads = 1, .sectors_per_track = 9, .sector_size = 512, .sector_base = 1,
        .datarate_bps = 250000, .rpm = 300.0, .total_size = 368640
    },
    {
        .name = "msx_ds",
        .display_name = "MSX DS (720K)",
        .description = "MSX double-sided",
        .category = "MSX",
        .format = UFT_FORMAT_MSX_DSK, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 80, .heads = 2, .sectors_per_track = 9, .sector_size = 512, .sector_base = 1,
        .datarate_bps = 250000, .rpm = 300.0, .total_size = 737280
    },
    
    // ========================================================================
    // Amstrad CPC (3 presets)
    // ========================================================================
    {
        .name = "cpc_system",
        .display_name = "Amstrad CPC System (180K)",
        .description = "Amstrad CPC AMSDOS system format",
        .category = "Amstrad CPC",
        .format = UFT_FORMAT_DSK_CPC, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 40, .heads = 1, .sectors_per_track = 9, .sector_size = 512, .sector_base = 0xC1,
        .datarate_bps = 250000, .rpm = 300.0, .total_size = 194816
    },
    {
        .name = "cpc_data",
        .display_name = "Amstrad CPC Data (180K)",
        .description = "Amstrad CPC AMSDOS data format",
        .category = "Amstrad CPC",
        .format = UFT_FORMAT_DSK_CPC, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 40, .heads = 1, .sectors_per_track = 9, .sector_size = 512, .sector_base = 0x41,
        .datarate_bps = 250000, .rpm = 300.0, .total_size = 194816
    },
    {
        .name = "cpc_edsk",
        .display_name = "Amstrad CPC EDSK",
        .description = "Amstrad CPC Extended DSK",
        .category = "Amstrad CPC",
        .format = UFT_FORMAT_EDSK, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 42, .heads = 1, .sectors_per_track = 0, .sector_size = 0, .sector_base = 0,
        .datarate_bps = 250000, .rpm = 300.0, .total_size = 0,
        .flags = PRESET_FLAG_FLUX
    },
    
    // ========================================================================
    // Spectrum (2 presets)
    // ========================================================================
    {
        .name = "spectrum_trdos",
        .display_name = "Spectrum TR-DOS (640K)",
        .description = "ZX Spectrum TR-DOS",
        .category = "Spectrum",
        .format = UFT_FORMAT_TRD, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 80, .heads = 2, .sectors_per_track = 16, .sector_size = 256, .sector_base = 1,
        .datarate_bps = 250000, .rpm = 300.0, .total_size = 655360
    },
    {
        .name = "spectrum_scl",
        .display_name = "Spectrum SCL",
        .description = "ZX Spectrum SCL archive",
        .category = "Spectrum",
        .format = UFT_FORMAT_SCL, .encoding = UFT_ENC_AUTO, .density = UFT_DENSITY_AUTO,
        .cylinders = 0, .heads = 0, .sectors_per_track = 0, .sector_size = 0, .sector_base = 0,
        .datarate_bps = 0, .rpm = 0.0, .total_size = 0,
        .flags = PRESET_FLAG_COMPRESSED
    },
    
    // ========================================================================
    // PC-98 (4 presets)
    // ========================================================================
    {
        .name = "pc98_2hd",
        .display_name = "PC-98 2HD (1.2M)",
        .description = "NEC PC-98 high-density",
        .category = "PC-98",
        .format = UFT_FORMAT_D88, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_HD,
        .cylinders = 77, .heads = 2, .sectors_per_track = 8, .sector_size = 1024, .sector_base = 1,
        .datarate_bps = 500000, .rpm = 360.0, .total_size = 1261568
    },
    {
        .name = "pc98_2dd",
        .display_name = "PC-98 2DD (640K)",
        .description = "NEC PC-98 double-density",
        .category = "PC-98",
        .format = UFT_FORMAT_D88, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 80, .heads = 2, .sectors_per_track = 8, .sector_size = 512, .sector_base = 1,
        .datarate_bps = 250000, .rpm = 300.0, .total_size = 655360
    },
    {
        .name = "pc98_hdm",
        .display_name = "PC-98 HDM (1.2M)",
        .description = "NEC PC-98 HDM format",
        .category = "PC-98",
        .format = UFT_FORMAT_HDM, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_HD,
        .cylinders = 77, .heads = 2, .sectors_per_track = 8, .sector_size = 1024, .sector_base = 1,
        .datarate_bps = 500000, .rpm = 360.0, .total_size = 1261568
    },
    {
        .name = "pc88_2d",
        .display_name = "PC-88 2D (320K)",
        .description = "NEC PC-88 double-density",
        .category = "PC-98",
        .format = UFT_FORMAT_D88, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 40, .heads = 2, .sectors_per_track = 16, .sector_size = 256, .sector_base = 1,
        .datarate_bps = 250000, .rpm = 300.0, .total_size = 327680
    },
    
    // ========================================================================
    // Flux / Preservation (4 presets)
    // ========================================================================
    {
        .name = "flux_scp",
        .display_name = "SuperCard Pro (SCP)",
        .description = "SuperCard Pro flux dump",
        .category = "Flux",
        .format = UFT_FORMAT_SCP, .encoding = UFT_ENC_AUTO, .density = UFT_DENSITY_AUTO,
        .cylinders = 84, .heads = 2, .sectors_per_track = 0, .sector_size = 0, .sector_base = 0,
        .datarate_bps = 0, .rpm = 300.0, .total_size = 0,
        .flags = PRESET_FLAG_FLUX
    },
    {
        .name = "flux_hfe",
        .display_name = "HxC HFE",
        .description = "HxC Floppy Emulator format",
        .category = "Flux",
        .format = UFT_FORMAT_HFE, .encoding = UFT_ENC_AUTO, .density = UFT_DENSITY_AUTO,
        .cylinders = 84, .heads = 2, .sectors_per_track = 0, .sector_size = 0, .sector_base = 0,
        .datarate_bps = 0, .rpm = 300.0, .total_size = 0,
        .flags = PRESET_FLAG_FLUX
    },
    {
        .name = "flux_ipf",
        .display_name = "SPS/CAPS IPF",
        .description = "Software Preservation Society IPF",
        .category = "Flux",
        .format = UFT_FORMAT_IPF, .encoding = UFT_ENC_AUTO, .density = UFT_DENSITY_AUTO,
        .cylinders = 84, .heads = 2, .sectors_per_track = 0, .sector_size = 0, .sector_base = 0,
        .datarate_bps = 0, .rpm = 300.0, .total_size = 0,
        .flags = PRESET_FLAG_FLUX
    },
    {
        .name = "flux_kryoflux",
        .display_name = "Kryoflux Stream",
        .description = "Kryoflux raw stream files",
        .category = "Flux",
        .format = UFT_FORMAT_KF_STREAM, .encoding = UFT_ENC_AUTO, .density = UFT_DENSITY_AUTO,
        .cylinders = 84, .heads = 2, .sectors_per_track = 0, .sector_size = 0, .sector_base = 0,
        .datarate_bps = 0, .rpm = 300.0, .total_size = 0,
        .flags = PRESET_FLAG_FLUX
    },
    
    // ========================================================================
    // Macintosh (2 presets) - NEW in v3.3.1
    // ========================================================================
    {
        .name = "mac_400k",
        .display_name = "Macintosh 400K GCR",
        .description = "Macintosh 400K single-sided GCR",
        .category = "Apple",
        .format = UFT_FORMAT_DC42, .encoding = UFT_ENC_GCR_APPLE, .density = UFT_DENSITY_DD,
        .cylinders = 80, .heads = 1, .sectors_per_track = 0, .sector_size = 512, .sector_base = 0,
        .datarate_bps = 500000, .rpm = 394.0, .total_size = 409600,
        .flags = PRESET_FLAG_VARIABLE_SPT
    },
    {
        .name = "mac_800k",
        .display_name = "Macintosh 800K GCR",
        .description = "Macintosh 800K double-sided GCR",
        .category = "Apple",
        .format = UFT_FORMAT_DC42, .encoding = UFT_ENC_GCR_APPLE, .density = UFT_DENSITY_DD,
        .cylinders = 80, .heads = 2, .sectors_per_track = 0, .sector_size = 512, .sector_base = 0,
        .datarate_bps = 500000, .rpm = 394.0, .total_size = 819200,
        .flags = PRESET_FLAG_VARIABLE_SPT
    },
    
    // ========================================================================
    // DEC (2 presets) - NEW in v3.3.1
    // ========================================================================
    {
        .name = "dec_rx01",
        .display_name = "DEC RX01 (256K)",
        .description = "DEC RX01 8-inch FM",
        .category = "DEC",
        .format = UFT_FORMAT_IMD, .encoding = UFT_ENC_FM, .density = UFT_DENSITY_SD,
        .cylinders = 77, .heads = 1, .sectors_per_track = 26, .sector_size = 128, .sector_base = 1,
        .datarate_bps = 250000, .rpm = 360.0, .total_size = 256256
    },
    {
        .name = "dec_rx02",
        .display_name = "DEC RX02 (512K)",
        .description = "DEC RX02 8-inch M2FM",
        .category = "DEC",
        .format = UFT_FORMAT_IMD, .encoding = UFT_ENC_M2FM, .density = UFT_DENSITY_DD,
        .cylinders = 77, .heads = 1, .sectors_per_track = 26, .sector_size = 256, .sector_base = 1,
        .datarate_bps = 500000, .rpm = 360.0, .total_size = 512512
    },
    
    // ========================================================================
    // Victor 9000 (2 presets) - NEW in v3.3.1
    // ========================================================================
    {
        .name = "victor_ss",
        .display_name = "Victor 9000 SS (606K)",
        .description = "Victor 9000 single-sided GCR",
        .category = "Victor",
        .format = UFT_FORMAT_RAW, .encoding = UFT_ENC_GCR_VICTOR, .density = UFT_DENSITY_DD,
        .cylinders = 80, .heads = 1, .sectors_per_track = 0, .sector_size = 512, .sector_base = 0,
        .datarate_bps = 0, .rpm = 0, .total_size = 620544,
        .flags = PRESET_FLAG_VARIABLE_SPT | PRESET_FLAG_VARIABLE_RPM
    },
    {
        .name = "victor_ds",
        .display_name = "Victor 9000 DS (1.2M)",
        .description = "Victor 9000 double-sided GCR",
        .category = "Victor",
        .format = UFT_FORMAT_RAW, .encoding = UFT_ENC_GCR_VICTOR, .density = UFT_DENSITY_DD,
        .cylinders = 80, .heads = 2, .sectors_per_track = 0, .sector_size = 512, .sector_base = 0,
        .datarate_bps = 0, .rpm = 0, .total_size = 1241088,
        .flags = PRESET_FLAG_VARIABLE_SPT | PRESET_FLAG_VARIABLE_RPM
    },
    
    // ========================================================================
    // Northstar (2 presets) - NEW in v3.3.1
    // ========================================================================
    {
        .name = "northstar_sd",
        .display_name = "Northstar SD (90K)",
        .description = "Northstar single-density",
        .category = "Northstar",
        .format = UFT_FORMAT_RAW, .encoding = UFT_ENC_FM, .density = UFT_DENSITY_SD,
        .cylinders = 35, .heads = 1, .sectors_per_track = 10, .sector_size = 256, .sector_base = 0,
        .datarate_bps = 125000, .rpm = 300.0, .total_size = 89600
    },
    {
        .name = "northstar_dd",
        .display_name = "Northstar DD (180K)",
        .description = "Northstar MFM double-density",
        .category = "Northstar",
        .format = UFT_FORMAT_RAW, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 35, .heads = 1, .sectors_per_track = 10, .sector_size = 512, .sector_base = 0,
        .datarate_bps = 250000, .rpm = 300.0, .total_size = 179200
    },
    
    // ========================================================================
    // Centurion (1 preset) - NEW in v3.3.1
    // ========================================================================
    {
        .name = "centurion",
        .display_name = "Centurion MFM",
        .description = "Centurion Minicomputer MFM format",
        .category = "Minicomputer",
        .format = UFT_FORMAT_IMD, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 77, .heads = 2, .sectors_per_track = 16, .sector_size = 256, .sector_base = 0,
        .datarate_bps = 500000, .rpm = 360.0, .total_size = 630784
    },
    
    // ========================================================================
    // TI-99/4A (2 presets) - NEW in v3.3.1
    // ========================================================================
    {
        .name = "ti99_sssd",
        .display_name = "TI-99/4A SS/SD (90K)",
        .description = "TI-99/4A single-sided single-density",
        .category = "TI-99",
        .format = UFT_FORMAT_RAW, .encoding = UFT_ENC_FM, .density = UFT_DENSITY_SD,
        .cylinders = 40, .heads = 1, .sectors_per_track = 9, .sector_size = 256, .sector_base = 0,
        .datarate_bps = 125000, .rpm = 300.0, .total_size = 92160
    },
    {
        .name = "ti99_dsdd",
        .display_name = "TI-99/4A DS/DD (360K)",
        .description = "TI-99/4A double-sided double-density",
        .category = "TI-99",
        .format = UFT_FORMAT_RAW, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 40, .heads = 2, .sectors_per_track = 18, .sector_size = 256, .sector_base = 0,
        .datarate_bps = 250000, .rpm = 300.0, .total_size = 368640
    },
    
    // ========================================================================
    // Membrain (1 preset) - NEW in v3.3.1
    // ========================================================================
    {
        .name = "membrain",
        .display_name = "Membrain MFM",
        .description = "Membrain system MFM format",
        .category = "Minicomputer",
        .format = UFT_FORMAT_IMD, .encoding = UFT_ENC_MFM, .density = UFT_DENSITY_DD,
        .cylinders = 77, .heads = 2, .sectors_per_track = 26, .sector_size = 256, .sector_base = 1,
        .datarate_bps = 500000, .rpm = 360.0, .total_size = 1025024
    },
    
    // Sentinel
    {.name = NULL}
};

// ========================================================================
// NEW PRESETS (v3.3.1 God Mode Integration)
// ========================================================================

// Note: Additional presets defined in extended_presets section above

#define PRESET_COUNT (sizeof(PRESET_DATABASE) / sizeof(PRESET_DATABASE[0]) - 1)

// ============================================================================
// PRESET API IMPLEMENTATION
// ============================================================================

int uft_preset_count(void) {
    return PRESET_COUNT;
}

int uft_preset_list(const char **names, int max_count) {
    int count = 0;
    for (size_t i = 0; PRESET_DATABASE[i].name != NULL && count < max_count; i++) {
        names[count++] = PRESET_DATABASE[i].name;
    }
    return count;
}

int uft_preset_list_by_category(const char *category, const char **names, int max_count) {
    int count = 0;
    for (size_t i = 0; PRESET_DATABASE[i].name != NULL && count < max_count; i++) {
        if (strcmp(PRESET_DATABASE[i].category, category) == 0) {
            names[count++] = PRESET_DATABASE[i].name;
        }
    }
    return count;
}

int uft_preset_get_categories(const char **categories, int max_count) {
    static const char* CATEGORIES[] = {
        "PC/DOS", "Commodore", "Amiga", "Atari", "Apple",
        "BBC/Acorn", "TRS-80", "MSX", "Amstrad CPC", 
        "Spectrum", "PC-98", "Flux", "DEC", "Victor",
        "Northstar", "Minicomputer", "TI-99", NULL
    };
    
    int count = 0;
    for (int i = 0; CATEGORIES[i] != NULL && count < max_count; i++) {
        categories[count++] = CATEGORIES[i];
    }
    return count;
}

const char *uft_preset_get_description(const char *name) {
    for (size_t i = 0; PRESET_DATABASE[i].name != NULL; i++) {
        if (strcmp(PRESET_DATABASE[i].name, name) == 0) {
            return PRESET_DATABASE[i].description;
        }
    }
    return NULL;
}

int uft_preset_apply(const char *name, uft_canonical_params_t *params) {
    if (!name || !params) return -1;
    
    for (size_t i = 0; PRESET_DATABASE[i].name != NULL; i++) {
        const uft_preset_def_t *p = &PRESET_DATABASE[i];
        if (strcmp(p->name, name) == 0) {
            // Apply preset
            *params = uft_params_init();
            
            // Format
            params->format.input_format = p->format;
            params->format.output_format = p->format;
            params->format.encoding = p->encoding;
            params->format.density = p->density;
            
            // Geometry
            params->geometry.cylinders = p->cylinders;
            params->geometry.heads = p->heads;
            params->geometry.sectors_per_track = p->sectors_per_track;
            params->geometry.sector_size = p->sector_size;
            params->geometry.sector_base = p->sector_base;
            params->geometry.head_mask = (p->heads == 2) ? 0x03 : 0x01;
            
            // Timing
            if (p->datarate_bps > 0) {
                params->timing.datarate_bps = p->datarate_bps;
            }
            if (p->cell_time_ns > 0) {
                params->timing.cell_time_ns = p->cell_time_ns;
            }
            params->timing.rpm = p->rpm;
            
            // Source
            snprintf(params->source, sizeof(params->source), "preset:%s", name);
            
            // Recompute derived values
            uft_params_recompute(params);
            
            return 0;
        }
    }
    
    return -1;  // Not found
}

uint32_t uft_preset_get_expected_size(const char *name) {
    for (size_t i = 0; PRESET_DATABASE[i].name != NULL; i++) {
        if (strcmp(PRESET_DATABASE[i].name, name) == 0) {
            return PRESET_DATABASE[i].total_size;
        }
    }
    return 0;
}
