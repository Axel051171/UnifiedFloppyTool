/**
 * @file uft_gui_params_extended.c
 * @brief UnifiedFloppyTool - GUI Parameter Implementation v3.1.4.010
 */

#include "uft/uft_gui_params_extended.h"
#include <string.h>
#include <stdio.h>

/*============================================================================
 * STRING TABLES
 *============================================================================*/

static const char* g_preset_names[] = {
    [UFT_PRESET_AUTO] = "Auto-Detect",
    [UFT_PRESET_AMIGA_DD] = "Amiga DD (880K)",
    [UFT_PRESET_AMIGA_HD] = "Amiga HD (1.76M)",
    [UFT_PRESET_PC_DD] = "IBM PC DD (720K)",
    [UFT_PRESET_PC_HD] = "IBM PC HD (1.44M)",
    [UFT_PRESET_ATARI_ST] = "Atari ST",
    [UFT_PRESET_BBC_DFS] = "BBC Micro DFS",
    [UFT_PRESET_C64_1541] = "C64 1541",
    [UFT_PRESET_APPLE_DOS33] = "Apple II DOS 3.3",
    [UFT_PRESET_DIRTY_DUMP] = "Dirty Dump (Wide Tolerance)",
    [UFT_PRESET_COPY_PROTECTION] = "Copy Protection Analysis",
    [UFT_PRESET_FORENSIC] = "Forensic Mode",
    [UFT_PRESET_CUSTOM] = "Custom"
};

static const char* g_platform_names[] = {
    [UFT_PLATFORM_AUTO] = "Auto",
    [UFT_PLATFORM_AMIGA] = "Amiga",
    [UFT_PLATFORM_AMIGA_HD] = "Amiga HD",
    [UFT_PLATFORM_AMIGA_DISKSPARE] = "Amiga DiskSpare",
    [UFT_PLATFORM_PC_DD] = "PC DD",
    [UFT_PLATFORM_PC_HD] = "PC HD",
    [UFT_PLATFORM_PC_2M] = "PC 2M",
    [UFT_PLATFORM_PC_SS] = "PC SS",
    [UFT_PLATFORM_ATARI_ST] = "Atari ST",
    [UFT_PLATFORM_BBC_DFS] = "BBC DFS",
    [UFT_PLATFORM_C64_1541] = "C64 1541",
    [UFT_PLATFORM_APPLE_DOS33] = "Apple DOS 3.3",
    [UFT_PLATFORM_APPLE_PRODOS] = "Apple ProDOS",
    [UFT_PLATFORM_MAC_GCR] = "Mac GCR"
};

static const char* g_proc_type_names[] = {
    [UFT_PROC_NORMAL] = "Normal",
    [UFT_PROC_ADAPTIVE] = "Adaptive",
    [UFT_PROC_ADAPTIVE2] = "Adaptive v2",
    [UFT_PROC_ADAPTIVE3] = "Adaptive v3",
    [UFT_PROC_ADAPTIVE_ENTROPY] = "Entropy",
    [UFT_PROC_ADAPTIVE_PREDICT] = "Predictive",
    [UFT_PROC_AUFIT] = "AUFIT",
    [UFT_PROC_WD1772_DPLL] = "WD1772 DPLL",
    [UFT_PROC_MAME_PLL] = "MAME PLL"
};

static const char* g_encoding_names[] = {
    [UFT_ENC_AUTO] = "Auto",
    [UFT_ENC_FM] = "FM",
    [UFT_ENC_MFM] = "MFM",
    [UFT_ENC_GCR] = "GCR",
    [UFT_ENC_APPLE_GCR] = "Apple GCR",
    [UFT_ENC_MAC_GCR] = "Mac GCR"
};

/*============================================================================
 * NAME LOOKUP FUNCTIONS
 *============================================================================*/

const char* uft_gui_preset_name(uft_preset_id_t preset) {
    if (preset >= UFT_PRESET_COUNT) return "Unknown";
    return g_preset_names[preset];
}

const char* uft_gui_platform_name(uft_platform_t platform) {
    if (platform >= UFT_PLATFORM_COUNT) return "Unknown";
    return g_platform_names[platform];
}

const char* uft_gui_proc_type_name(uft_processing_type_t type) {
    if (type >= UFT_PROC_COUNT) return "Unknown";
    return g_proc_type_names[type];
}

const char* uft_gui_encoding_name(uft_encoding_t encoding) {
    if (encoding > UFT_ENC_MAC_GCR && encoding != UFT_ENC_CUSTOM) return "Unknown";
    if (encoding == UFT_ENC_CUSTOM) return "Custom";
    return g_encoding_names[encoding];
}

/*============================================================================
 * DEFAULTS
 *============================================================================*/

void uft_gui_settings_init_default(uft_gui_master_settings_t* settings) {
    if (!settings) return;
    
    memset(settings, 0, sizeof(*settings));
    settings->version = 0x03010410;  /* v3.1.4.010 */
    
    /* Default Geometry (Amiga DD) */
    settings->geometry.tracks = 80;
    settings->geometry.heads = 2;
    settings->geometry.sectors_per_track = 11;
    settings->geometry.sector_size = 512;
    settings->geometry.encoding = UFT_ENC_MFM;
    settings->geometry.total_size = 80 * 2 * 11 * 512;
    settings->geometry.valid = true;
    settings->geometry.interleave = 1;
    
    /* Default Processing */
    settings->processing.proc_type = UFT_PROC_ADAPTIVE;
    settings->processing.platform = UFT_PLATFORM_AMIGA;
    
    settings->processing.timing.offset = 0;
    settings->processing.timing.min = 10;
    settings->processing.timing.four = 20;
    settings->processing.timing.six = 30;
    settings->processing.timing.max = 50;
    settings->processing.timing.thresh_4us = 2.0f;
    settings->processing.timing.thresh_6us = 3.0f;
    settings->processing.timing.thresh_8us = 4.0f;
    settings->processing.timing.hd_shift = 0;
    
    settings->processing.adaptive.rate_of_change = 4.0f;
    settings->processing.adaptive.rate_of_change2 = 100.0f;
    settings->processing.adaptive.adapt_rate_pct = 25.0f;
    settings->processing.adaptive.lowpass_radius = 100;
    settings->processing.adaptive.adapt_offset = 0;
    settings->processing.adaptive.adapt_offset2 = 0.0f;
    settings->processing.adaptive.use_entropy = false;
    settings->processing.adaptive.add_noise = false;
    
    /* Processing Options */
    settings->processing.skip_period_data = false;
    settings->processing.find_dupes = true;
    settings->processing.use_error_correction = true;
    settings->processing.only_bad_sectors = false;
    settings->processing.ignore_header_error = false;
    settings->processing.auto_refresh_sectormap = true;
    settings->processing.number_of_dups = 5;
    
    /* DPLL (WD1772 defaults) */
    settings->dpll.pll_clk = 80;
    settings->dpll.phase_correction = 90;
    settings->dpll.low_correction = 128 - 90;
    settings->dpll.high_correction = 128 + 90;
    settings->dpll.low_stop = 115;
    settings->dpll.high_stop = 141;
    settings->dpll.high_density = false;
    settings->dpll.phase_adjust_pct = 70.3f;  /* 90/128 */
    settings->dpll.period_min_pct = 89.8f;    /* 115/128 */
    settings->dpll.period_max_pct = 110.2f;   /* 141/128 */
    
    /* Flux Profile (MFM DD) */
    settings->flux_profile.profile_id = 1;
    snprintf(settings->flux_profile.name, sizeof(settings->flux_profile.name), 
             "MFM Double Density");
    settings->flux_profile.encoding = UFT_ENC_MFM;
    settings->flux_profile.tick_hz = 80000000;  /* 80 MHz / 12.5ns */
    settings->flux_profile.nominal_bitrate = 250000;
    settings->flux_profile.rotation_us = 200000;  /* 200ms = 300 RPM */
    settings->flux_profile.jitter_abs_ticks = 8;
    settings->flux_profile.jitter_rel_ppm = 1000;
    settings->flux_profile.cell_time_us = 2.0f;
    settings->flux_profile.jitter_pct = 2.5f;
    
    /* Symbol Ranges for MFM */
    settings->flux_profile.ranges[0] = (uft_gui_symbol_range_t){
        .min_ticks = 120, .max_ticks = 200, .symbol_id = 2, .name = "2T"
    };
    settings->flux_profile.ranges[1] = (uft_gui_symbol_range_t){
        .min_ticks = 200, .max_ticks = 280, .symbol_id = 3, .name = "3T"
    };
    settings->flux_profile.ranges[2] = (uft_gui_symbol_range_t){
        .min_ticks = 280, .max_ticks = 360, .symbol_id = 4, .name = "4T"
    };
    settings->flux_profile.ranges_count = 3;
    
    /* Forensic */
    settings->forensic.block_size = 512;
    settings->forensic.max_retries = 3;
    settings->forensic.retry_delay_ms = 100;
    settings->forensic.reverse_mode = false;
    settings->forensic.fill_bad_blocks = true;
    settings->forensic.fill_pattern = 0x00;
    settings->forensic.hash_md5 = true;
    settings->forensic.hash_sha256 = true;
    settings->forensic.split_output = false;
    settings->forensic.split_size = 4ULL * 1024 * 1024 * 1024;
    snprintf(settings->forensic.split_format, sizeof(settings->forensic.split_format), "000");
    settings->forensic.verify_after_write = true;
    settings->forensic.verbose_log = false;
    
    /* Metadata */
    snprintf(settings->preset_name, sizeof(settings->preset_name), "Default");
    snprintf(settings->description, sizeof(settings->description), 
             "Default settings for Amiga DD disks");
}

/*============================================================================
 * PRESET LOADING
 *============================================================================*/

bool uft_gui_settings_load_preset(uft_preset_id_t preset,
                                   uft_gui_master_settings_t* settings) {
    if (!settings) return false;
    
    /* Start with defaults */
    uft_gui_settings_init_default(settings);
    
    switch (preset) {
        case UFT_PRESET_AUTO:
            snprintf(settings->preset_name, sizeof(settings->preset_name), "Auto-Detect");
            settings->processing.platform = UFT_PLATFORM_AUTO;
            break;
            
        case UFT_PRESET_AMIGA_DD:
            snprintf(settings->preset_name, sizeof(settings->preset_name), "Amiga DD");
            settings->geometry.tracks = 80;
            settings->geometry.heads = 2;
            settings->geometry.sectors_per_track = 11;
            settings->geometry.sector_size = 512;
            settings->geometry.total_size = 901120;
            settings->processing.platform = UFT_PLATFORM_AMIGA;
            break;
            
        case UFT_PRESET_AMIGA_HD:
            snprintf(settings->preset_name, sizeof(settings->preset_name), "Amiga HD");
            settings->geometry.tracks = 80;
            settings->geometry.heads = 2;
            settings->geometry.sectors_per_track = 22;
            settings->geometry.sector_size = 512;
            settings->geometry.total_size = 1802240;
            settings->processing.platform = UFT_PLATFORM_AMIGA_HD;
            settings->processing.timing.hd_shift = 1;
            settings->dpll.high_density = true;
            settings->flux_profile.nominal_bitrate = 500000;
            settings->flux_profile.cell_time_us = 1.0f;
            break;
            
        case UFT_PRESET_PC_DD:
            snprintf(settings->preset_name, sizeof(settings->preset_name), "PC DD");
            settings->geometry.tracks = 80;
            settings->geometry.heads = 2;
            settings->geometry.sectors_per_track = 9;
            settings->geometry.sector_size = 512;
            settings->geometry.total_size = 737280;
            settings->processing.platform = UFT_PLATFORM_PC_DD;
            break;
            
        case UFT_PRESET_PC_HD:
            snprintf(settings->preset_name, sizeof(settings->preset_name), "PC HD");
            settings->geometry.tracks = 80;
            settings->geometry.heads = 2;
            settings->geometry.sectors_per_track = 18;
            settings->geometry.sector_size = 512;
            settings->geometry.total_size = 1474560;
            settings->processing.platform = UFT_PLATFORM_PC_HD;
            settings->processing.timing.hd_shift = 1;
            settings->dpll.high_density = true;
            settings->flux_profile.nominal_bitrate = 500000;
            settings->flux_profile.cell_time_us = 1.0f;
            break;
            
        case UFT_PRESET_ATARI_ST:
            snprintf(settings->preset_name, sizeof(settings->preset_name), "Atari ST");
            settings->geometry.tracks = 80;
            settings->geometry.heads = 2;
            settings->geometry.sectors_per_track = 9;
            settings->geometry.sector_size = 512;
            settings->geometry.total_size = 737280;
            settings->processing.platform = UFT_PLATFORM_ATARI_ST;
            break;
            
        case UFT_PRESET_BBC_DFS:
            snprintf(settings->preset_name, sizeof(settings->preset_name), "BBC DFS");
            settings->geometry.tracks = 40;
            settings->geometry.heads = 1;
            settings->geometry.sectors_per_track = 10;
            settings->geometry.sector_size = 256;
            settings->geometry.encoding = UFT_ENC_FM;
            settings->geometry.total_size = 102400;
            settings->processing.platform = UFT_PLATFORM_BBC_DFS;
            settings->flux_profile.encoding = UFT_ENC_FM;
            settings->flux_profile.cell_time_us = 4.0f;
            break;
            
        case UFT_PRESET_C64_1541:
            snprintf(settings->preset_name, sizeof(settings->preset_name), "C64 1541");
            settings->geometry.tracks = 35;
            settings->geometry.heads = 1;
            settings->geometry.sectors_per_track = 21;  /* Variable by zone */
            settings->geometry.sector_size = 256;
            settings->geometry.encoding = UFT_ENC_GCR;
            settings->geometry.total_size = 174848;
            settings->processing.platform = UFT_PLATFORM_C64_1541;
            settings->flux_profile.encoding = UFT_ENC_GCR;
            settings->flux_profile.nominal_bitrate = 250000;
            break;
            
        case UFT_PRESET_APPLE_DOS33:
            snprintf(settings->preset_name, sizeof(settings->preset_name), "Apple DOS 3.3");
            settings->geometry.tracks = 35;
            settings->geometry.heads = 1;
            settings->geometry.sectors_per_track = 16;
            settings->geometry.sector_size = 256;
            settings->geometry.encoding = UFT_ENC_APPLE_GCR;
            settings->geometry.total_size = 143360;
            settings->processing.platform = UFT_PLATFORM_APPLE_DOS33;
            settings->flux_profile.encoding = UFT_ENC_APPLE_GCR;
            break;
            
        case UFT_PRESET_DIRTY_DUMP:
            snprintf(settings->preset_name, sizeof(settings->preset_name), "Dirty Dump");
            snprintf(settings->description, sizeof(settings->description),
                     "Wide tolerance for damaged/dirty disks");
            settings->processing.proc_type = UFT_PROC_ADAPTIVE3;
            settings->processing.adaptive.rate_of_change = 2.0f;
            settings->processing.adaptive.adapt_rate_pct = 50.0f;
            settings->processing.adaptive.lowpass_radius = 200;
            settings->processing.use_error_correction = true;
            settings->processing.ignore_header_error = true;
            settings->dpll.low_stop = 100;
            settings->dpll.high_stop = 156;
            settings->flux_profile.jitter_pct = 10.0f;
            break;
            
        case UFT_PRESET_COPY_PROTECTION:
            snprintf(settings->preset_name, sizeof(settings->preset_name), "Copy Protection");
            snprintf(settings->description, sizeof(settings->description),
                     "Analysis mode for copy-protected disks");
            settings->processing.proc_type = UFT_PROC_WD1772_DPLL;
            settings->processing.find_dupes = true;
            settings->processing.only_bad_sectors = false;
            settings->processing.ignore_header_error = true;
            break;
            
        case UFT_PRESET_FORENSIC:
            snprintf(settings->preset_name, sizeof(settings->preset_name), "Forensic");
            snprintf(settings->description, sizeof(settings->description),
                     "Full forensic imaging with hashing and verification");
            settings->forensic.hash_md5 = true;
            settings->forensic.hash_sha1 = true;
            settings->forensic.hash_sha256 = true;
            settings->forensic.hash_sha512 = true;
            settings->forensic.verify_after_write = true;
            settings->forensic.verbose_log = true;
            settings->forensic.max_retries = 5;
            break;
            
        default:
            return false;
    }
    
    return true;
}

/*============================================================================
 * GEOMETRY FROM SIZE
 *============================================================================*/

typedef struct {
    uint64_t size;
    int32_t tracks;
    int32_t heads;
    int32_t spt;
    int32_t ss;
    uft_encoding_t enc;
} geom_candidate_t;

static const geom_candidate_t g_geom_candidates[] = {
    /* Amiga */
    { 901120,   80, 2, 11, 512, UFT_ENC_MFM },   /* Amiga DD */
    { 1802240,  80, 2, 22, 512, UFT_ENC_MFM },   /* Amiga HD */
    { 983040,   82, 2, 12, 512, UFT_ENC_MFM },   /* DiskSpare 984K */
    { 960000,   80, 2, 12, 512, UFT_ENC_MFM },   /* DiskSpare */
    
    /* PC */
    { 1474560,  80, 2, 18, 512, UFT_ENC_MFM },   /* PC HD 1.44M */
    { 737280,   80, 2,  9, 512, UFT_ENC_MFM },   /* PC DD 720K */
    { 368640,   80, 1,  9, 512, UFT_ENC_MFM },   /* PC SS/DD */
    { 163840,   40, 1,  8, 512, UFT_ENC_MFM },   /* PC 160K */
    { 327680,   40, 2,  8, 512, UFT_ENC_MFM },   /* PC 320K */
    { 184320,   40, 1,  9, 512, UFT_ENC_MFM },   /* PC 180K */
    { 368640,   40, 2,  9, 512, UFT_ENC_MFM },   /* PC 360K */
    { 1228800,  80, 2, 15, 512, UFT_ENC_MFM },   /* PC 1.2M */
    { 1966080,  80, 2, 12, 1024, UFT_ENC_MFM },  /* PC 2M */
    
    /* Atari ST */
    { 819200,   82, 2, 10, 512, UFT_ENC_MFM },   /* ST Extended */
    
    /* C64 */
    { 174848,   35, 1, 21, 256, UFT_ENC_GCR },   /* D64 35 tracks */
    { 196608,   40, 1, 21, 256, UFT_ENC_GCR },   /* D64 40 tracks */
    { 175531,   35, 1, 21, 256, UFT_ENC_GCR },   /* D64 + errors */
    { 349696,   35, 2, 21, 256, UFT_ENC_GCR },   /* D71 */
    { 822400,   80, 2, 10, 512, UFT_ENC_MFM },   /* D81 */
    
    /* Apple */
    { 143360,   35, 1, 16, 256, UFT_ENC_APPLE_GCR }, /* DOS 3.3 */
    { 140800,   35, 1, 16, 256, UFT_ENC_APPLE_GCR }, /* ProDOS */
    { 409600,   80, 1, 10, 512, UFT_ENC_MAC_GCR },   /* Mac 400K */
    { 819200,   80, 2, 10, 512, UFT_ENC_MAC_GCR },   /* Mac 800K */
    
    /* BBC */
    { 102400,   40, 1, 10, 256, UFT_ENC_FM },    /* BBC SS */
    { 204800,   80, 1, 10, 256, UFT_ENC_FM },    /* BBC DS */
    
    { 0, 0, 0, 0, 0, UFT_ENC_AUTO }  /* Terminator */
};

bool uft_gui_geometry_from_size(uint64_t file_size,
                                 uft_gui_geometry_t* geometry) {
    if (!geometry) return false;
    
    memset(geometry, 0, sizeof(*geometry));
    
    for (const geom_candidate_t* c = g_geom_candidates; c->size > 0; c++) {
        if (c->size == file_size) {
            geometry->tracks = c->tracks;
            geometry->heads = c->heads;
            geometry->sectors_per_track = c->spt;
            geometry->sector_size = c->ss;
            geometry->encoding = c->enc;
            geometry->total_size = file_size;
            geometry->valid = true;
            geometry->interleave = 1;
            return true;
        }
    }
    
    /* Try to infer from common divisors */
    if (file_size % 512 == 0) {
        uint64_t sectors = file_size / 512;
        
        /* Try common geometries */
        int32_t test_spt[] = { 18, 15, 11, 10, 9, 8 };
        int32_t test_heads[] = { 2, 1 };
        int32_t test_tracks[] = { 80, 82, 84, 40, 35 };
        
        for (int ti = 0; test_tracks[ti]; ti++) {
            for (int hi = 0; hi < 2; hi++) {
                for (int si = 0; test_spt[si]; si++) {
                    uint64_t calc = test_tracks[ti] * test_heads[hi] * test_spt[si];
                    if (calc == sectors) {
                        geometry->tracks = test_tracks[ti];
                        geometry->heads = test_heads[hi];
                        geometry->sectors_per_track = test_spt[si];
                        geometry->sector_size = 512;
                        geometry->encoding = UFT_ENC_MFM;
                        geometry->total_size = file_size;
                        geometry->valid = true;
                        geometry->interleave = 1;
                        return true;
                    }
                }
            }
        }
    }
    
    geometry->valid = false;
    return false;
}

/*============================================================================
 * VALIDATION
 *============================================================================*/

bool uft_gui_proc_settings_validate(const uft_gui_proc_settings_t* settings,
                                     char* error_msg, size_t error_len) {
    if (!settings) {
        if (error_msg && error_len > 0) {
            snprintf(error_msg, error_len, "Settings pointer is NULL");
        }
        return false;
    }
    
    /* Check range bounds */
    if (settings->start > settings->end) {
        if (error_msg && error_len > 0) {
            snprintf(error_msg, error_len, "Start cannot be greater than end");
        }
        return false;
    }
    
    /* Check timing thresholds order */
    if (settings->timing.four >= settings->timing.six ||
        settings->timing.six >= settings->timing.max) {
        if (error_msg && error_len > 0) {
            snprintf(error_msg, error_len, 
                     "Timing thresholds must be: four < six < max");
        }
        return false;
    }
    
    /* Check adaptive rate */
    if (settings->adaptive.rate_of_change <= 0) {
        if (error_msg && error_len > 0) {
            snprintf(error_msg, error_len, "Rate of change must be positive");
        }
        return false;
    }
    
    /* Check lowpass radius */
    if (settings->adaptive.lowpass_radius < 0 || 
        settings->adaptive.lowpass_radius > 10000) {
        if (error_msg && error_len > 0) {
            snprintf(error_msg, error_len, "Lowpass radius out of range (0-10000)");
        }
        return false;
    }
    
    return true;
}

/*============================================================================
 * FLUX PROFILE GENERATION
 *============================================================================*/

void uft_gui_flux_profile_for_platform(uft_platform_t platform,
                                        uft_gui_flux_profile_t* profile) {
    if (!profile) return;
    
    memset(profile, 0, sizeof(*profile));
    profile->tick_hz = 80000000;  /* 80 MHz default */
    
    switch (platform) {
        case UFT_PLATFORM_AMIGA:
        case UFT_PLATFORM_PC_DD:
        case UFT_PLATFORM_ATARI_ST:
            profile->encoding = UFT_ENC_MFM;
            profile->nominal_bitrate = 250000;
            profile->cell_time_us = 2.0f;
            profile->jitter_pct = 2.5f;
            snprintf(profile->name, sizeof(profile->name), "MFM DD 250kbps");
            /* 2T/3T/4T ranges für 2µs cells @ 80MHz */
            profile->ranges[0] = (uft_gui_symbol_range_t){ 120, 200, 2, "2T" };
            profile->ranges[1] = (uft_gui_symbol_range_t){ 200, 280, 3, "3T" };
            profile->ranges[2] = (uft_gui_symbol_range_t){ 280, 360, 4, "4T" };
            profile->ranges_count = 3;
            break;
            
        case UFT_PLATFORM_AMIGA_HD:
        case UFT_PLATFORM_PC_HD:
            profile->encoding = UFT_ENC_MFM;
            profile->nominal_bitrate = 500000;
            profile->cell_time_us = 1.0f;
            profile->jitter_pct = 2.5f;
            snprintf(profile->name, sizeof(profile->name), "MFM HD 500kbps");
            profile->ranges[0] = (uft_gui_symbol_range_t){ 60, 100, 2, "2T" };
            profile->ranges[1] = (uft_gui_symbol_range_t){ 100, 140, 3, "3T" };
            profile->ranges[2] = (uft_gui_symbol_range_t){ 140, 180, 4, "4T" };
            profile->ranges_count = 3;
            break;
            
        case UFT_PLATFORM_BBC_DFS:
            profile->encoding = UFT_ENC_FM;
            profile->nominal_bitrate = 125000;
            profile->cell_time_us = 4.0f;
            profile->jitter_pct = 3.0f;
            snprintf(profile->name, sizeof(profile->name), "FM 125kbps");
            profile->ranges[0] = (uft_gui_symbol_range_t){ 240, 400, 1, "1T" };
            profile->ranges[1] = (uft_gui_symbol_range_t){ 400, 560, 2, "2T" };
            profile->ranges_count = 2;
            break;
            
        case UFT_PLATFORM_C64_1541:
            profile->encoding = UFT_ENC_GCR;
            profile->nominal_bitrate = 250000;
            profile->cell_time_us = 3.25f;  /* Zone average */
            profile->jitter_pct = 3.0f;
            snprintf(profile->name, sizeof(profile->name), "C64 GCR");
            /* GCR hat andere Bucket-Struktur */
            profile->ranges[0] = (uft_gui_symbol_range_t){ 200, 280, 1, "1" };
            profile->ranges[1] = (uft_gui_symbol_range_t){ 280, 360, 2, "01" };
            profile->ranges[2] = (uft_gui_symbol_range_t){ 360, 440, 3, "001" };
            profile->ranges[3] = (uft_gui_symbol_range_t){ 440, 520, 4, "0001" };
            profile->ranges_count = 4;
            break;
            
        case UFT_PLATFORM_APPLE_DOS33:
        case UFT_PLATFORM_APPLE_PRODOS:
            profile->encoding = UFT_ENC_APPLE_GCR;
            profile->nominal_bitrate = 250000;
            profile->cell_time_us = 4.0f;
            profile->jitter_pct = 3.0f;
            snprintf(profile->name, sizeof(profile->name), "Apple II GCR");
            break;
            
        case UFT_PLATFORM_MAC_GCR:
            profile->encoding = UFT_ENC_MAC_GCR;
            profile->nominal_bitrate = 400000;
            profile->cell_time_us = 2.5f;
            profile->jitter_pct = 2.5f;
            snprintf(profile->name, sizeof(profile->name), "Mac GCR");
            break;
            
        default:
            profile->encoding = UFT_ENC_AUTO;
            snprintf(profile->name, sizeof(profile->name), "Auto");
            break;
    }
}

/*============================================================================
 * DPLL HELPERS
 *============================================================================*/

void uft_gui_dpll_from_percent(uft_percent_t phase_pct,
                                uft_percent_t freq_pct,
                                uft_gui_dpll_settings_t* dpll) {
    if (!dpll) return;
    
    /* Phase correction: phase_pct% of 128 */
    dpll->phase_correction = (int32_t)(phase_pct * 128.0f / 100.0f);
    if (dpll->phase_correction < 10) dpll->phase_correction = 10;
    if (dpll->phase_correction > 120) dpll->phase_correction = 120;
    
    dpll->low_correction = 128 - dpll->phase_correction;
    dpll->high_correction = 128 + dpll->phase_correction;
    
    /* Period bounds: 128 +/- freq_pct% */
    int32_t delta = (int32_t)(freq_pct * 128.0f / 100.0f);
    dpll->low_stop = 128 - delta;
    dpll->high_stop = 128 + delta;
    
    if (dpll->low_stop < 64) dpll->low_stop = 64;
    if (dpll->high_stop > 192) dpll->high_stop = 192;
    
    dpll->phase_adjust_pct = phase_pct;
    dpll->period_min_pct = (float)dpll->low_stop / 128.0f * 100.0f;
    dpll->period_max_pct = (float)dpll->high_stop / 128.0f * 100.0f;
}

void uft_gui_adaptive_from_roc(float rate_of_change,
                                int32_t lowpass_radius,
                                uft_gui_adaptive_processing_t* adaptive) {
    if (!adaptive) return;
    
    adaptive->rate_of_change = rate_of_change;
    adaptive->rate_of_change2 = (float)lowpass_radius;
    adaptive->lowpass_radius = lowpass_radius;
    
    /* Convert to percentage (inverse relationship) */
    if (rate_of_change > 0) {
        adaptive->adapt_rate_pct = 100.0f / rate_of_change;
    } else {
        adaptive->adapt_rate_pct = 100.0f;
    }
}

/*============================================================================
 * SLIDER CONFIGS
 *============================================================================*/

uft_gui_slider_config_t uft_gui_get_slider_roc(void) {
    return (uft_gui_slider_config_t){
        .min_value = 1.0f,
        .max_value = 16.0f,
        .default_value = 4.0f,
        .step = 0.5f,
        .label = "Rate of Change",
        .unit = "",
        .tooltip = "Adaptation speed (lower = faster)"
    };
}

uft_gui_slider_config_t uft_gui_get_slider_lowpass(void) {
    return (uft_gui_slider_config_t){
        .min_value = 0.0f,
        .max_value = 1024.0f,
        .default_value = 100.0f,
        .step = 10.0f,
        .label = "Lowpass Radius",
        .unit = "samples",
        .tooltip = "Moving average window size"
    };
}

uft_gui_slider_config_t uft_gui_get_slider_phase(void) {
    return (uft_gui_slider_config_t){
        .min_value = 10.0f,
        .max_value = 95.0f,
        .default_value = 70.0f,
        .step = 1.0f,
        .label = "Phase Correction",
        .unit = "%",
        .tooltip = "PLL phase tracking strength"
    };
}

uft_gui_slider_config_t uft_gui_get_slider_freq(void) {
    return (uft_gui_slider_config_t){
        .min_value = 1.0f,
        .max_value = 20.0f,
        .default_value = 10.0f,
        .step = 0.5f,
        .label = "Frequency Tolerance",
        .unit = "%",
        .tooltip = "PLL frequency adjustment range"
    };
}

uft_gui_slider_config_t uft_gui_get_slider_retries(void) {
    return (uft_gui_slider_config_t){
        .min_value = 0.0f,
        .max_value = 10.0f,
        .default_value = 3.0f,
        .step = 1.0f,
        .label = "Max Retries",
        .unit = "",
        .tooltip = "Maximum read retries for bad sectors"
    };
}

/*============================================================================
 * COMBO ITEMS
 *============================================================================*/

static const uft_gui_combo_item_t g_platform_items[] = {
    { UFT_PLATFORM_AUTO, "Auto", "Automatic detection" },
    { UFT_PLATFORM_AMIGA, "Amiga DD", "Amiga 880K Double Density" },
    { UFT_PLATFORM_AMIGA_HD, "Amiga HD", "Amiga 1.76M High Density" },
    { UFT_PLATFORM_PC_DD, "PC DD", "IBM PC 720K" },
    { UFT_PLATFORM_PC_HD, "PC HD", "IBM PC 1.44M" },
    { UFT_PLATFORM_ATARI_ST, "Atari ST", "Atari ST 720K" },
    { UFT_PLATFORM_BBC_DFS, "BBC DFS", "BBC Micro DFS (FM)" },
    { UFT_PLATFORM_C64_1541, "C64 1541", "Commodore 64 GCR" },
    { UFT_PLATFORM_APPLE_DOS33, "Apple DOS", "Apple II DOS 3.3" },
    { -1, NULL, NULL }
};

static const uft_gui_combo_item_t g_proc_type_items[] = {
    { UFT_PROC_NORMAL, "Normal", "Standard processing" },
    { UFT_PROC_ADAPTIVE, "Adaptive", "Adaptive thresholds" },
    { UFT_PROC_ADAPTIVE2, "Adaptive v2", "With lowpass filter" },
    { UFT_PROC_ADAPTIVE3, "Adaptive v3", "Enhanced algorithm" },
    { UFT_PROC_WD1772_DPLL, "WD1772 DPLL", "FDC emulation" },
    { UFT_PROC_MAME_PLL, "MAME PLL", "MAME-style PLL" },
    { -1, NULL, NULL }
};

const uft_gui_combo_item_t* uft_gui_get_platforms(int* count) {
    if (count) *count = UFT_PLATFORM_COUNT;
    return g_platform_items;
}

const uft_gui_combo_item_t* uft_gui_get_proc_types(int* count) {
    if (count) *count = UFT_PROC_COUNT;
    return g_proc_type_items;
}
