/**
 * @file uft_hal_profiles.c
 * @brief Drive Profiles & Controller Capabilities
 */

#include "uft/hal/uft_hal.h"
#include <string.h>

// ============================================================================
// Drive Profiles
// ============================================================================

const uft_drive_profile_t UFT_DRIVE_PROFILE_525_DD = {
    .type = UFT_DRIVE_525_DD,
    .name = "5.25\" DD (360KB)",
    .cylinders = 40,
    .heads = 2,
    .double_step = false,
    .rpm = 300.0,
    .rpm_tolerance = 1.5,
    .step_time_ms = 6.0,
    .settle_time_ms = 15.0,
    .motor_spinup_ms = 500.0,
    .data_rate_dd = 250.0,
    .bit_cell_dd = 4.0,     // FM: 4µs, MFM: 2µs
    .track_length_bits = 50000,
    .write_precomp_ns = 0,
    .default_encoding = DRIVE_ENC_MFM,
};

const uft_drive_profile_t UFT_DRIVE_PROFILE_525_HD = {
    .type = UFT_DRIVE_525_HD,
    .name = "5.25\" HD (1.2MB)",
    .cylinders = 80,
    .heads = 2,
    .double_step = false,
    .rpm = 360.0,
    .rpm_tolerance = 1.5,
    .step_time_ms = 3.0,
    .settle_time_ms = 15.0,
    .motor_spinup_ms = 500.0,
    .data_rate_dd = 250.0,
    .data_rate_hd = 500.0,
    .bit_cell_dd = 2.0,
    .bit_cell_hd = 1.0,
    .track_length_bits = 100000,
    .write_precomp_ns = 125,
    .default_encoding = DRIVE_ENC_MFM,
};

const uft_drive_profile_t UFT_DRIVE_PROFILE_35_DD = {
    .type = UFT_DRIVE_35_DD,
    .name = "3.5\" DD (720KB/880KB)",
    .cylinders = 80,
    .heads = 2,
    .double_step = false,
    .rpm = 300.0,
    .rpm_tolerance = 1.0,
    .step_time_ms = 3.0,
    .settle_time_ms = 15.0,
    .motor_spinup_ms = 400.0,
    .data_rate_dd = 250.0,
    .bit_cell_dd = 2.0,
    .track_length_bits = 100000,
    .write_precomp_ns = 0,
    .default_encoding = DRIVE_ENC_MFM,
};

const uft_drive_profile_t UFT_DRIVE_PROFILE_35_HD = {
    .type = UFT_DRIVE_35_HD,
    .name = "3.5\" HD (1.44MB)",
    .cylinders = 80,
    .heads = 2,
    .double_step = false,
    .rpm = 300.0,
    .rpm_tolerance = 1.0,
    .step_time_ms = 3.0,
    .settle_time_ms = 15.0,
    .motor_spinup_ms = 400.0,
    .data_rate_dd = 250.0,
    .data_rate_hd = 500.0,
    .bit_cell_dd = 2.0,
    .bit_cell_hd = 1.0,
    .track_length_bits = 200000,
    .write_precomp_ns = 0,
    .default_encoding = DRIVE_ENC_MFM,
};

const uft_drive_profile_t UFT_DRIVE_PROFILE_35_ED = {
    .type = UFT_DRIVE_35_ED,
    .name = "3.5\" ED (2.88MB)",
    .cylinders = 80,
    .heads = 2,
    .double_step = false,
    .rpm = 300.0,
    .rpm_tolerance = 1.0,
    .step_time_ms = 3.0,
    .settle_time_ms = 15.0,
    .motor_spinup_ms = 400.0,
    .data_rate_dd = 250.0,
    .data_rate_hd = 500.0,
    .data_rate_ed = 1000.0,
    .bit_cell_dd = 2.0,
    .bit_cell_hd = 1.0,
    .track_length_bits = 400000,
    .write_precomp_ns = 0,
    .default_encoding = DRIVE_ENC_MFM,
};

const uft_drive_profile_t UFT_DRIVE_PROFILE_CBM_1541 = {
    .type = UFT_DRIVE_CBM_1541,
    .name = "Commodore 1541",
    .cylinders = 42,        // Standard 35, extended to 42
    .heads = 1,
    .double_step = false,
    .rpm = 300.0,           // Actually varies by zone
    .rpm_tolerance = 2.0,
    .step_time_ms = 12.0,   // Slower than PC drives
    .settle_time_ms = 20.0,
    .motor_spinup_ms = 1000.0,
    .data_rate_dd = 0,      // Variable by zone
    .bit_cell_dd = 0,       // GCR timing varies
    .track_length_bits = 0, // Zone-dependent
    .write_precomp_ns = 0,
    .default_encoding = DRIVE_ENC_GCR_CBM,
    .half_tracks = true,
    .speed_zones = 4,
    // Zone 1 (tracks 1-17):  21 sectors, 3.25µs bit cell
    // Zone 2 (tracks 18-24): 19 sectors, 3.50µs bit cell
    // Zone 3 (tracks 25-30): 18 sectors, 3.75µs bit cell
    // Zone 4 (tracks 31-35): 17 sectors, 4.00µs bit cell
};

const uft_drive_profile_t UFT_DRIVE_PROFILE_CBM_1571 = {
    .type = UFT_DRIVE_CBM_1571,
    .name = "Commodore 1571",
    .cylinders = 42,
    .heads = 2,             // Double-sided
    .double_step = false,
    .rpm = 300.0,
    .rpm_tolerance = 2.0,
    .step_time_ms = 12.0,
    .settle_time_ms = 20.0,
    .motor_spinup_ms = 1000.0,
    .default_encoding = DRIVE_ENC_GCR_CBM,
    .half_tracks = true,
    .speed_zones = 4,
};

const uft_drive_profile_t UFT_DRIVE_PROFILE_APPLE_525 = {
    .type = UFT_DRIVE_APPLE_525,
    .name = "Apple II 5.25\"",
    .cylinders = 40,        // 35 standard, some have 40
    .heads = 1,
    .double_step = false,
    .rpm = 300.0,
    .rpm_tolerance = 2.0,
    .step_time_ms = 20.0,   // Slow stepper
    .settle_time_ms = 30.0,
    .motor_spinup_ms = 1000.0,
    .data_rate_dd = 250.0,
    .bit_cell_dd = 4.0,     // GCR: ~4µs
    .track_length_bits = 51200, // Approx
    .write_precomp_ns = 0,
    .default_encoding = DRIVE_ENC_GCR_APPLE,
    .half_tracks = true,    // Common for copy protection
};

// ============================================================================
// Controller Capabilities
// ============================================================================

const uft_controller_caps_t UFT_CAPS_GREASEWEAZLE = {
    .type = UFT_CTRL_GREASEWEAZLE,
    .name = "Greaseweazle",
    .version = "F7/F7+",
    
    .sample_rate_mhz = 72.0,        // F7: 72MHz, F7+: up to 84MHz
    .sample_resolution_ns = 13.9,   // 1/72MHz
    .jitter_ns = 5.0,
    
    .can_read_flux = true,
    .can_read_bitstream = true,
    .can_read_sector = true,
    .can_write_flux = true,
    .can_write_bitstream = true,
    
    .hardware_index = true,
    .index_accuracy_ns = 10.0,
    .max_revolutions = 20,
    
    .max_cylinders = 84,
    .max_heads = 2,
    .supports_half_tracks = true,
    
    .max_data_rate_kbps = 1000.0,
    .variable_data_rate = true,
    
    .copy_protection_support = true,
    .weak_bit_detection = true,
    .density_select = true,
    
    .limitations = {
        "USB latency can affect long writes",
        NULL
    },
};

const uft_controller_caps_t UFT_CAPS_FLUXENGINE = {
    .type = UFT_CTRL_FLUXENGINE,
    .name = "FluxEngine",
    .version = "Cypress FX2",
    
    .sample_rate_mhz = 72.0,
    .sample_resolution_ns = 13.9,
    .jitter_ns = 5.0,
    
    .can_read_flux = true,
    .can_read_bitstream = true,
    .can_read_sector = true,
    .can_write_flux = true,
    .can_write_bitstream = true,
    
    .hardware_index = true,
    .index_accuracy_ns = 10.0,
    .max_revolutions = 20,
    
    .max_cylinders = 84,
    .max_heads = 2,
    .supports_half_tracks = true,
    
    .max_data_rate_kbps = 1000.0,
    .variable_data_rate = true,
    
    .copy_protection_support = true,
    .weak_bit_detection = true,
    .density_select = true,
    
    .limitations = {
        "Profile-based, some formats need specific profiles",
        NULL
    },
};

const uft_controller_caps_t UFT_CAPS_KRYOFLUX = {
    .type = UFT_CTRL_KRYOFLUX,
    .name = "Kryoflux",
    .version = "DTC 3.x",
    
    .sample_rate_mhz = 24.027,      // SRM clock
    .sample_resolution_ns = 41.6,   // ~42ns
    .jitter_ns = 10.0,
    
    .can_read_flux = true,
    .can_read_bitstream = false,    // Flux only, decode in software
    .can_read_sector = false,
    .can_write_flux = true,
    .can_write_bitstream = false,
    
    .hardware_index = true,
    .index_accuracy_ns = 42.0,
    .max_revolutions = 50,          // Stream mode
    
    .max_cylinders = 86,
    .max_heads = 2,
    .supports_half_tracks = true,
    
    .max_data_rate_kbps = 500.0,
    .variable_data_rate = false,
    
    .copy_protection_support = true,
    .weak_bit_detection = true,
    .density_select = true,
    
    .limitations = {
        "Proprietary hardware",
        "Stream format is multi-file",
        "Lower sample rate than Greaseweazle",
        NULL
    },
};

const uft_controller_caps_t UFT_CAPS_FC5025 = {
    .type = UFT_CTRL_FC5025,
    .name = "FC5025",
    .version = "Device Side Data",
    
    .sample_rate_mhz = 12.0,        // 12MHz
    .sample_resolution_ns = 83.3,   // ~83ns
    .jitter_ns = 50.0,              // Higher jitter
    
    .can_read_flux = false,         // ⚠️ NO RAW FLUX!
    .can_read_bitstream = true,     // Bitstream only
    .can_read_sector = true,
    .can_write_flux = false,
    .can_write_bitstream = true,
    
    .hardware_index = false,        // Software index detection
    .index_accuracy_ns = 1000.0,    // Poor accuracy
    .max_revolutions = 5,
    
    .max_cylinders = 80,
    .max_heads = 2,
    .supports_half_tracks = false,  // ⚠️ NO HALF-TRACKS
    
    .max_data_rate_kbps = 500.0,
    .variable_data_rate = false,
    
    .copy_protection_support = false, // ⚠️ LIMITED
    .weak_bit_detection = false,
    .density_select = true,
    
    .limitations = {
        "No raw flux capture - bitstream only",
        "No half-track support",
        "Limited copy protection handling",
        "Software index detection (less accurate)",
        "Higher timing jitter than other controllers",
        "5.25\" drives only (no Apple 3.5\")",
        NULL
    },
};

const uft_controller_caps_t UFT_CAPS_XUM1541 = {
    .type = UFT_CTRL_XUM1541,
    .name = "XUM1541/ZoomFloppy",
    .version = "CBM Protocol",
    
    .sample_rate_mhz = 1.0,         // CBM serial bus speed
    .sample_resolution_ns = 1000.0, // ~1µs
    .jitter_ns = 500.0,
    
    .can_read_flux = false,         // ⚠️ NO FLUX - GCR BITSTREAM ONLY
    .can_read_bitstream = true,     // G64/NIB
    .can_read_sector = true,        // D64
    .can_write_flux = false,
    .can_write_bitstream = true,
    
    .hardware_index = false,
    .index_accuracy_ns = 10000.0,   // Very low accuracy
    .max_revolutions = 1,
    
    .max_cylinders = 42,            // CBM limit
    .max_heads = 2,                 // 1571 only
    .supports_half_tracks = true,
    
    .max_data_rate_kbps = 40.0,     // CBM serial is slow
    .variable_data_rate = false,
    
    .copy_protection_support = true, // GCR-level
    .weak_bit_detection = false,
    .density_select = false,        // GCR only
    
    .limitations = {
        "CBM drives only (1541, 1571, 1581)",
        "No raw flux - GCR bitstream level",
        "Slow serial bus transfer",
        "Parallel speedup requires cable mod",
        "Cannot read non-CBM disks",
        NULL
    },
};

// ============================================================================
// Profile Lookup
// ============================================================================

static const uft_drive_profile_t* g_drive_profiles[] = {
    &UFT_DRIVE_PROFILE_525_DD,
    &UFT_DRIVE_PROFILE_525_HD,
    &UFT_DRIVE_PROFILE_35_DD,
    &UFT_DRIVE_PROFILE_35_HD,
    &UFT_DRIVE_PROFILE_35_ED,
    &UFT_DRIVE_PROFILE_CBM_1541,
    &UFT_DRIVE_PROFILE_CBM_1571,
    &UFT_DRIVE_PROFILE_APPLE_525,
    NULL
};

static const uft_controller_caps_t* g_controller_caps[] = {
    &UFT_CAPS_GREASEWEAZLE,
    &UFT_CAPS_FLUXENGINE,
    &UFT_CAPS_KRYOFLUX,
    &UFT_CAPS_FC5025,
    &UFT_CAPS_XUM1541,
    NULL
};

const uft_drive_profile_t* uft_hal_get_drive_profile(uft_drive_type_t type) {
    for (int i = 0; g_drive_profiles[i]; i++) {
        if (g_drive_profiles[i]->type == type) {
            return g_drive_profiles[i];
        }
    }
    return NULL;
}

const uft_controller_caps_t* uft_hal_get_controller_caps(uft_controller_type_t type) {
    for (int i = 0; g_controller_caps[i]; i++) {
        if (g_controller_caps[i]->type == type) {
            return g_controller_caps[i];
        }
    }
    return NULL;
}

// ============================================================================
// Print Capabilities
// ============================================================================

void uft_hal_print_controller_caps(const uft_controller_caps_t* caps) {
    if (!caps) return;
    
    printf("Controller: %s %s\n", caps->name, caps->version);
    printf("  Sample Rate: %.1f MHz (%.1f ns resolution)\n",
           caps->sample_rate_mhz, caps->sample_resolution_ns);
    printf("  Jitter: %.1f ns\n", caps->jitter_ns);
    printf("  Capabilities:\n");
    printf("    Read Flux:      %s\n", caps->can_read_flux ? "Yes" : "NO");
    printf("    Read Bitstream: %s\n", caps->can_read_bitstream ? "Yes" : "NO");
    printf("    Write Flux:     %s\n", caps->can_write_flux ? "Yes" : "NO");
    printf("    Hardware Index: %s\n", caps->hardware_index ? "Yes" : "NO");
    printf("    Half-tracks:    %s\n", caps->supports_half_tracks ? "Yes" : "NO");
    printf("    Copy Protect:   %s\n", caps->copy_protection_support ? "Yes" : "LIMITED");
    printf("  Limits: %d cylinders, %d heads, %d revolutions\n",
           caps->max_cylinders, caps->max_heads, caps->max_revolutions);
    
    if (caps->limitations[0]) {
        printf("  Limitations:\n");
        for (int i = 0; caps->limitations[i]; i++) {
            printf("    - %s\n", caps->limitations[i]);
        }
    }
}
