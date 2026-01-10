/**
 * @file uft_hal_profiles.c
 * @brief Drive Profiles & Controller Capabilities
 * @version 2.0.0
 * 
 * Predefined profiles for floppy drives and controller capabilities.
 */

#include "uft/hal/uft_drive.h"
#include "uft/hal/uft_hal.h"
#include <stdio.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Drive Profiles
 * ═══════════════════════════════════════════════════════════════════════════════ */

const uft_drive_profile_t UFT_DRIVE_PROFILE_525_DD = {
    .type = UFT_DRIVE_525_DD,
    .name = "5.25\" DD (360KB)",
    .cylinders = 40,
    .heads = 2,
    .double_step = false,
    .half_tracks = false,
    .speed_zones = 0,
    .rpm = 300.0,
    .rpm_tolerance = 1.5,
    .step_time_ms = 6.0,
    .settle_time_ms = 15.0,
    .motor_spinup_ms = 500.0,
    .data_rate_dd = 250.0,
    .data_rate_hd = 0.0,
    .data_rate_ed = 0.0,
    .bit_cell_dd = 4.0,
    .bit_cell_hd = 0.0,
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
    .half_tracks = false,
    .speed_zones = 0,
    .rpm = 360.0,
    .rpm_tolerance = 1.5,
    .step_time_ms = 3.0,
    .settle_time_ms = 15.0,
    .motor_spinup_ms = 500.0,
    .data_rate_dd = 250.0,
    .data_rate_hd = 500.0,
    .data_rate_ed = 0.0,
    .bit_cell_dd = 2.0,
    .bit_cell_hd = 1.0,
    .track_length_bits = 100000,
    .write_precomp_ns = 125,
    .default_encoding = DRIVE_ENC_MFM,
};

const uft_drive_profile_t UFT_DRIVE_PROFILE_35_DD = {
    .type = UFT_DRIVE_35_DD,
    .name = "3.5\" DD (720KB)",
    .cylinders = 80,
    .heads = 2,
    .double_step = false,
    .half_tracks = false,
    .speed_zones = 0,
    .rpm = 300.0,
    .rpm_tolerance = 1.5,
    .step_time_ms = 3.0,
    .settle_time_ms = 15.0,
    .motor_spinup_ms = 500.0,
    .data_rate_dd = 250.0,
    .data_rate_hd = 0.0,
    .data_rate_ed = 0.0,
    .bit_cell_dd = 2.0,
    .bit_cell_hd = 0.0,
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
    .half_tracks = false,
    .speed_zones = 0,
    .rpm = 300.0,
    .rpm_tolerance = 1.5,
    .step_time_ms = 3.0,
    .settle_time_ms = 15.0,
    .motor_spinup_ms = 400.0,
    .data_rate_dd = 250.0,
    .data_rate_hd = 500.0,
    .data_rate_ed = 0.0,
    .bit_cell_dd = 2.0,
    .bit_cell_hd = 1.0,
    .track_length_bits = 100000,
    .write_precomp_ns = 0,
    .default_encoding = DRIVE_ENC_MFM,
};

const uft_drive_profile_t UFT_DRIVE_PROFILE_35_ED = {
    .type = UFT_DRIVE_35_ED,
    .name = "3.5\" ED (2.88MB)",
    .cylinders = 80,
    .heads = 2,
    .double_step = false,
    .half_tracks = false,
    .speed_zones = 0,
    .rpm = 300.0,
    .rpm_tolerance = 1.5,
    .step_time_ms = 3.0,
    .settle_time_ms = 15.0,
    .motor_spinup_ms = 400.0,
    .data_rate_dd = 250.0,
    .data_rate_hd = 500.0,
    .data_rate_ed = 1000.0,
    .bit_cell_dd = 2.0,
    .bit_cell_hd = 1.0,
    .track_length_bits = 200000,
    .write_precomp_ns = 0,
    .default_encoding = DRIVE_ENC_MFM,
};

const uft_drive_profile_t UFT_DRIVE_PROFILE_8_SD = {
    .type = UFT_DRIVE_8_SD,
    .name = "8\" SD (250KB)",
    .cylinders = 77,
    .heads = 1,
    .double_step = false,
    .half_tracks = false,
    .speed_zones = 0,
    .rpm = 360.0,
    .rpm_tolerance = 2.0,
    .step_time_ms = 10.0,
    .settle_time_ms = 25.0,
    .motor_spinup_ms = 1000.0,
    .data_rate_dd = 250.0,
    .data_rate_hd = 0.0,
    .data_rate_ed = 0.0,
    .bit_cell_dd = 4.0,
    .bit_cell_hd = 0.0,
    .track_length_bits = 41666,
    .write_precomp_ns = 0,
    .default_encoding = DRIVE_ENC_FM,
};

const uft_drive_profile_t UFT_DRIVE_PROFILE_8_DD = {
    .type = UFT_DRIVE_8_DD,
    .name = "8\" DD (1MB)",
    .cylinders = 77,
    .heads = 2,
    .double_step = false,
    .half_tracks = false,
    .speed_zones = 0,
    .rpm = 360.0,
    .rpm_tolerance = 2.0,
    .step_time_ms = 10.0,
    .settle_time_ms = 25.0,
    .motor_spinup_ms = 1000.0,
    .data_rate_dd = 500.0,
    .data_rate_hd = 0.0,
    .data_rate_ed = 0.0,
    .bit_cell_dd = 2.0,
    .bit_cell_hd = 0.0,
    .track_length_bits = 83333,
    .write_precomp_ns = 0,
    .default_encoding = DRIVE_ENC_MFM,
};

const uft_drive_profile_t UFT_DRIVE_PROFILE_1541 = {
    .type = UFT_DRIVE_1541,
    .name = "Commodore 1541",
    .cylinders = 35,
    .heads = 1,
    .double_step = false,
    .half_tracks = true,
    .speed_zones = 4,
    .rpm = 300.0,
    .rpm_tolerance = 2.0,
    .step_time_ms = 12.0,
    .settle_time_ms = 20.0,
    .motor_spinup_ms = 600.0,
    .data_rate_dd = 250.0,
    .data_rate_hd = 0.0,
    .data_rate_ed = 0.0,
    .bit_cell_dd = 4.0,
    .bit_cell_hd = 0.0,
    .track_length_bits = 62500,
    .write_precomp_ns = 0,
    .default_encoding = DRIVE_ENC_GCR,
};

const uft_drive_profile_t UFT_DRIVE_PROFILE_APPLE = {
    .type = UFT_DRIVE_APPLE,
    .name = "Apple II Disk II",
    .cylinders = 35,
    .heads = 1,
    .double_step = false,
    .half_tracks = true,
    .speed_zones = 0,
    .rpm = 300.0,
    .rpm_tolerance = 2.5,
    .step_time_ms = 12.0,
    .settle_time_ms = 20.0,
    .motor_spinup_ms = 1000.0,
    .data_rate_dd = 250.0,
    .data_rate_hd = 0.0,
    .data_rate_ed = 0.0,
    .bit_cell_dd = 4.0,
    .bit_cell_hd = 0.0,
    .track_length_bits = 51200,
    .write_precomp_ns = 0,
    .default_encoding = DRIVE_ENC_GCR,
};

const uft_drive_profile_t UFT_DRIVE_PROFILE_AMIGA_DD = {
    .type = UFT_DRIVE_AMIGA_DD,
    .name = "Amiga DD (880KB)",
    .cylinders = 80,
    .heads = 2,
    .double_step = false,
    .half_tracks = false,
    .speed_zones = 0,
    .rpm = 300.0,
    .rpm_tolerance = 1.5,
    .step_time_ms = 3.0,
    .settle_time_ms = 15.0,
    .motor_spinup_ms = 500.0,
    .data_rate_dd = 250.0,
    .data_rate_hd = 0.0,
    .data_rate_ed = 0.0,
    .bit_cell_dd = 2.0,
    .bit_cell_hd = 0.0,
    .track_length_bits = 105500,
    .write_precomp_ns = 0,
    .default_encoding = DRIVE_ENC_MFM,
};

const uft_drive_profile_t UFT_DRIVE_PROFILE_AMIGA_HD = {
    .type = UFT_DRIVE_AMIGA_HD,
    .name = "Amiga HD (1.76MB)",
    .cylinders = 80,
    .heads = 2,
    .double_step = false,
    .half_tracks = false,
    .speed_zones = 0,
    .rpm = 300.0,
    .rpm_tolerance = 1.5,
    .step_time_ms = 3.0,
    .settle_time_ms = 15.0,
    .motor_spinup_ms = 400.0,
    .data_rate_dd = 250.0,
    .data_rate_hd = 500.0,
    .data_rate_ed = 0.0,
    .bit_cell_dd = 2.0,
    .bit_cell_hd = 1.0,
    .track_length_bits = 211000,
    .write_precomp_ns = 0,
    .default_encoding = DRIVE_ENC_MFM,
};

/* Profile lookup table */
static const uft_drive_profile_t* g_drive_profiles[] = {
    NULL,  /* UFT_DRIVE_UNKNOWN */
    &UFT_DRIVE_PROFILE_525_DD,
    &UFT_DRIVE_PROFILE_525_HD,
    &UFT_DRIVE_PROFILE_35_DD,
    &UFT_DRIVE_PROFILE_35_HD,
    &UFT_DRIVE_PROFILE_35_ED,
    &UFT_DRIVE_PROFILE_8_SD,
    &UFT_DRIVE_PROFILE_8_DD,
    &UFT_DRIVE_PROFILE_1541,
    &UFT_DRIVE_PROFILE_APPLE,
    &UFT_DRIVE_PROFILE_AMIGA_DD,
    &UFT_DRIVE_PROFILE_AMIGA_HD,
};

const uft_drive_profile_t* uft_drive_get_profile(uft_drive_type_t type) {
    if (type <= UFT_DRIVE_UNKNOWN || type >= UFT_DRIVE_COUNT) {
        return NULL;
    }
    return g_drive_profiles[type];
}

int uft_drive_list_profiles(uft_drive_profile_t* profiles, int max_count) {
    if (!profiles || max_count <= 0) return 0;
    
    int count = 0;
    for (int i = 1; i < UFT_DRIVE_COUNT && count < max_count; i++) {
        if (g_drive_profiles[i]) {
            profiles[count++] = *g_drive_profiles[i];
        }
    }
    return count;
}

const char* uft_drive_type_name(uft_drive_type_t type) {
    static const char* names[] = {
        "Unknown",
        "5.25\" DD",
        "5.25\" HD",
        "3.5\" DD",
        "3.5\" HD",
        "3.5\" ED",
        "8\" SD",
        "8\" DD",
        "Commodore 1541",
        "Apple II",
        "Amiga DD",
        "Amiga HD"
    };
    if (type >= 0 && type < UFT_DRIVE_COUNT) {
        return names[type];
    }
    return "Unknown";
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Controller Capabilities
 * ═══════════════════════════════════════════════════════════════════════════════ */

const uft_controller_caps_t UFT_CAPS_GREASEWEAZLE = {
    .type = UFT_CTRL_GREASEWEAZLE,
    .name = "Greaseweazle",
    .version = "F7/F7+",
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
        "USB latency can affect long writes",
        NULL
    }
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
    }
};

const uft_controller_caps_t UFT_CAPS_KRYOFLUX = {
    .type = UFT_CTRL_KRYOFLUX,
    .name = "Kryoflux",
    .version = "DTC 3.x",
    .sample_rate_mhz = 24.027,
    .sample_resolution_ns = 41.6,
    .jitter_ns = 10.0,
    .can_read_flux = true,
    .can_read_bitstream = false,
    .can_read_sector = false,
    .can_write_flux = true,
    .can_write_bitstream = false,
    .hardware_index = true,
    .index_accuracy_ns = 42.0,
    .max_revolutions = 50,
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
    }
};

const uft_controller_caps_t UFT_CAPS_SCP = {
    .type = UFT_CTRL_SCP,
    .name = "SuperCard Pro",
    .version = "v2.0",
    .sample_rate_mhz = 40.0,
    .sample_resolution_ns = 25.0,
    .jitter_ns = 8.0,
    .can_read_flux = true,
    .can_read_bitstream = true,
    .can_read_sector = true,
    .can_write_flux = true,
    .can_write_bitstream = true,
    .hardware_index = true,
    .index_accuracy_ns = 25.0,
    .max_revolutions = 10,
    .max_cylinders = 84,
    .max_heads = 2,
    .supports_half_tracks = true,
    .max_data_rate_kbps = 1000.0,
    .variable_data_rate = true,
    .copy_protection_support = true,
    .weak_bit_detection = true,
    .density_select = true,
    .limitations = {
        "Proprietary hardware and software",
        NULL
    }
};

const uft_controller_caps_t UFT_CAPS_FC5025 = {
    .type = UFT_CTRL_FC5025,
    .name = "FC5025",
    .version = "Device Side Data",
    .sample_rate_mhz = 12.0,
    .sample_resolution_ns = 83.3,
    .jitter_ns = 50.0,
    .can_read_flux = false,
    .can_read_bitstream = true,
    .can_read_sector = true,
    .can_write_flux = false,
    .can_write_bitstream = true,
    .hardware_index = false,
    .index_accuracy_ns = 1000.0,
    .max_revolutions = 5,
    .max_cylinders = 80,
    .max_heads = 2,
    .supports_half_tracks = false,
    .max_data_rate_kbps = 500.0,
    .variable_data_rate = false,
    .copy_protection_support = false,
    .weak_bit_detection = false,
    .density_select = true,
    .limitations = {
        "No raw flux capture - bitstream only",
        "No half-track support",
        "Limited copy protection handling",
        "Software index detection (less accurate)",
        "Higher timing jitter than other controllers",
        "5.25\" drives only",
        NULL
    }
};

const uft_controller_caps_t UFT_CAPS_XUM1541 = {
    .type = UFT_CTRL_XUM1541,
    .name = "XUM1541/ZoomFloppy",
    .version = "CBM Protocol",
    .sample_rate_mhz = 1.0,
    .sample_resolution_ns = 1000.0,
    .jitter_ns = 500.0,
    .can_read_flux = false,
    .can_read_bitstream = true,
    .can_read_sector = true,
    .can_write_flux = false,
    .can_write_bitstream = true,
    .hardware_index = false,
    .index_accuracy_ns = 10000.0,
    .max_revolutions = 1,
    .max_cylinders = 42,
    .max_heads = 2,
    .supports_half_tracks = true,
    .max_data_rate_kbps = 40.0,
    .variable_data_rate = false,
    .copy_protection_support = true,
    .weak_bit_detection = false,
    .density_select = false,
    .limitations = {
        "CBM drives only (1541, 1571, 1581)",
        "No raw flux - GCR bitstream level",
        "Slow serial bus transfer",
        "Parallel speedup requires cable mod",
        "Cannot read non-CBM disks",
        NULL
    }
};

/* Controller lookup table - matches uft_controller_type_t enum order */
static const uft_controller_caps_t* g_controller_caps[] = {
    &UFT_CAPS_GREASEWEAZLE,  /* UFT_CTRL_GREASEWEAZLE = 0 */
    &UFT_CAPS_FLUXENGINE,    /* UFT_CTRL_FLUXENGINE = 1 */
    &UFT_CAPS_KRYOFLUX,      /* UFT_CTRL_KRYOFLUX = 2 */
    &UFT_CAPS_SCP,           /* UFT_CTRL_SCP = 3 */
    &UFT_CAPS_FC5025,        /* UFT_CTRL_FC5025 = 4 */
    &UFT_CAPS_XUM1541,       /* UFT_CTRL_XUM1541 = 5 */
    NULL,                    /* UFT_CTRL_APPLESAUCE = 6 (TODO) */
};

const uft_controller_caps_t* uft_hal_get_controller_caps(uft_controller_type_t type) {
    if (type < 0 || type >= UFT_CTRL_COUNT) {
        return NULL;
    }
    return g_controller_caps[type];
}

const char* uft_hal_controller_type_name(uft_controller_type_t type) {
    static const char* names[] = {
        "Greaseweazle",         /* UFT_CTRL_GREASEWEAZLE = 0 */
        "FluxEngine",           /* UFT_CTRL_FLUXENGINE = 1 */
        "Kryoflux",             /* UFT_CTRL_KRYOFLUX = 2 */
        "SuperCard Pro",        /* UFT_CTRL_SCP = 3 */
        "FC5025",               /* UFT_CTRL_FC5025 = 4 */
        "XUM1541/ZoomFloppy",   /* UFT_CTRL_XUM1541 = 5 */
        "Applesauce",           /* UFT_CTRL_APPLESAUCE = 6 */
    };
    if (type >= 0 && type < UFT_CTRL_COUNT) {
        return names[type];
    }
    return "Unknown";
}

void uft_hal_print_controller_caps(const uft_controller_caps_t* caps) {
    if (!caps) return;
    
    printf("Controller: %s %s\n", caps->name, caps->version);
    printf("  Sampling: %.1f MHz (%.1f ns resolution)\n",
           caps->sample_rate_mhz, caps->sample_resolution_ns);
    printf("  Jitter: %.1f ns\n", caps->jitter_ns);
    printf("  Capabilities:\n");
    printf("    Read Flux:      %s\n", caps->can_read_flux ? "Yes" : "NO");
    printf("    Read Bitstream: %s\n", caps->can_read_bitstream ? "Yes" : "NO");
    printf("    Write Flux:     %s\n", caps->can_write_flux ? "Yes" : "NO");
    printf("    Hardware Index: %s\n", caps->hardware_index ? "Yes" : "NO");
    printf("    Half-tracks:    %s\n", caps->supports_half_tracks ? "Yes" : "NO");
    printf("    Copy Protect:   %s\n", caps->copy_protection_support ? "Yes" : "LIMITED");
    printf("  Limits: %d cyl, %d heads, %d revs\n",
           caps->max_cylinders, caps->max_heads, caps->max_revolutions);
    
    if (caps->limitations[0]) {
        printf("  Known Limitations:\n");
        for (int i = 0; caps->limitations[i]; i++) {
            printf("    - %s\n", caps->limitations[i]);
        }
    }
}

bool uft_hal_controller_has_feature(const uft_controller_caps_t* caps, const char* feature) {
    if (!caps || !feature) return false;
    
    if (strcmp(feature, "flux") == 0) return caps->can_read_flux;
    if (strcmp(feature, "write") == 0) return caps->can_write_flux;
    if (strcmp(feature, "halftrack") == 0) return caps->supports_half_tracks;
    if (strcmp(feature, "weakbit") == 0) return caps->weak_bit_detection;
    if (strcmp(feature, "protection") == 0) return caps->copy_protection_support;
    if (strcmp(feature, "bitstream") == 0) return caps->can_read_bitstream;
    if (strcmp(feature, "sector") == 0) return caps->can_read_sector;
    if (strcmp(feature, "index") == 0) return caps->hardware_index;
    
    return false;
}

uft_controller_type_t uft_hal_recommend_controller(bool need_flux, bool need_write, bool need_halftrack) {
    /* Priority order: Greaseweazle > FluxEngine > Kryoflux > SCP > FC5025 > XUM1541 */
    
    if (need_flux) {
        if (need_halftrack) {
            return UFT_CTRL_GREASEWEAZLE;  /* Best for flux + halftrack */
        }
        return UFT_CTRL_GREASEWEAZLE;  /* Best overall flux */
    }
    
    if (need_write && need_halftrack) {
        return UFT_CTRL_GREASEWEAZLE;
    }
    
    if (need_write) {
        return UFT_CTRL_GREASEWEAZLE;
    }
    
    if (need_halftrack) {
        return UFT_CTRL_XUM1541;  /* Good for CBM with half-tracks */
    }
    
    /* General purpose - Greaseweazle is the most versatile */
    return UFT_CTRL_GREASEWEAZLE;
}
