/**
 * @file uft_hal_profiles.h
 * @brief HAL Controller Profiles - Detailed hardware capabilities
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * Detailed profiles for all supported floppy controllers:
 * - Greaseweazle (F1/F7)
 * - FluxEngine
 * - KryoFlux
 * - SuperCard Pro
 * - Applesauce
 * - XUM1541/ZoomFloppy
 * - FC5025
 * - Pauline
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_HAL_PROFILES_H
#define UFT_HAL_PROFILES_H

#include <stdint.h>
#include <stdbool.h>
#include "uft/hal/uft_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Platform Types (shared with format registry)
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_PLATFORM_DEFINED
#define UFT_PLATFORM_DEFINED
typedef enum {
    UFT_PLATFORM_GENERIC = 0,
    UFT_PLATFORM_AMIGA,
    UFT_PLATFORM_APPLE_II,
    UFT_PLATFORM_APPLE_MAC,
    UFT_PLATFORM_ATARI_8BIT,
    UFT_PLATFORM_ATARI_ST,
    UFT_PLATFORM_COMMODORE,
    UFT_PLATFORM_CPM,
    UFT_PLATFORM_IBM_PC,
    UFT_PLATFORM_MSX,
    UFT_PLATFORM_NEC_PC98,
    UFT_PLATFORM_FUJITSU_FM,
    UFT_PLATFORM_ZX_SPECTRUM,
    UFT_PLATFORM_DDR             /**< East German (DDR) computers */
} uft_platform_t;
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Extended Capability Flags
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    /* Basic capabilities */
    UFT_HAL_CAP_READ_FLUX       = (1 << 0),     /**< Raw flux reading */
    UFT_HAL_CAP_WRITE_FLUX      = (1 << 1),     /**< Raw flux writing */
    UFT_HAL_CAP_INDEX_SENSE     = (1 << 2),     /**< Index pulse detection */
    UFT_HAL_CAP_MOTOR_CTRL      = (1 << 3),     /**< Motor on/off control */
    UFT_HAL_CAP_DENSITY_SELECT  = (1 << 4),     /**< HD/DD density select */
    UFT_HAL_CAP_WRITE_PROTECT   = (1 << 5),     /**< WP sense */
    UFT_HAL_CAP_TRACK_0_SENSE   = (1 << 6),     /**< Track 0 sensor */
    
    /* Advanced capabilities */
    UFT_HAL_CAP_HALF_TRACK      = (1 << 7),     /**< Half-track stepping */
    UFT_HAL_CAP_QUARTER_TRACK   = (1 << 8),     /**< Quarter-track (Apple II) */
    UFT_HAL_CAP_HEAD_SELECT     = (1 << 9),     /**< Side/head select */
    UFT_HAL_CAP_MULTI_REV       = (1 << 10),    /**< Multi-revolution capture */
    UFT_HAL_CAP_INDEX_ALIGN     = (1 << 11),    /**< Index-aligned writes */
    UFT_HAL_CAP_PRECOMP         = (1 << 12),    /**< Write precompensation */
    UFT_HAL_CAP_ERASE           = (1 << 13),    /**< Track erase */
    
    /* Special features */
    UFT_HAL_CAP_VARIABLE_RATE   = (1 << 14),    /**< Variable bit rate */
    UFT_HAL_CAP_DISK_CHANGE     = (1 << 15),    /**< Disk change detect */
    UFT_HAL_CAP_SYNC_WORD       = (1 << 16),    /**< Sync word detection */
    UFT_HAL_CAP_GCR_DECODE      = (1 << 17),    /**< Hardware GCR decode */
    UFT_HAL_CAP_MFM_DECODE      = (1 << 18),    /**< Hardware MFM decode */
    UFT_HAL_CAP_FM_DECODE       = (1 << 19),    /**< Hardware FM decode */
    
    /* Platform-specific */
    UFT_HAL_CAP_AMIGA_HD        = (1 << 20),    /**< Amiga HD support */
    UFT_HAL_CAP_APPLE_400K      = (1 << 21),    /**< Apple GCR 400K */
    UFT_HAL_CAP_APPLE_800K      = (1 << 22),    /**< Apple GCR 800K */
    UFT_HAL_CAP_C64_GCR         = (1 << 23),    /**< C64/1541 GCR */
    UFT_HAL_CAP_PC_HD           = (1 << 24),    /**< PC 1.44MB HD */
    UFT_HAL_CAP_PC_ED           = (1 << 25),    /**< PC 2.88MB ED */
    
    /* Connection */
    UFT_HAL_CAP_USB             = (1 << 26),    /**< USB connection */
    UFT_HAL_CAP_SERIAL          = (1 << 27),    /**< Serial connection */
    UFT_HAL_CAP_PARALLEL        = (1 << 28),    /**< Parallel port */
    UFT_HAL_CAP_SHUGART         = (1 << 29),    /**< Shugart bus */
    UFT_HAL_CAP_IEC             = (1 << 30)     /**< Commodore IEC bus */
} uft_hal_cap_flags_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Controller Profile Structure
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Detailed controller profile
 */
typedef struct {
    /* Identification */
    uft_hal_controller_t type;
    const char* name;
    const char* manufacturer;
    const char* description;
    const char* website;
    
    /* Capabilities */
    uint32_t capabilities;          /**< Bitmask of uft_hal_cap_flags_t */
    
    /* Timing specifications */
    uint32_t sample_clock_hz;       /**< Sample clock frequency */
    uint32_t min_flux_ns;           /**< Minimum flux period (ns) */
    uint32_t max_flux_ns;           /**< Maximum flux period (ns) */
    uint32_t timing_resolution_ns;  /**< Timing resolution (ns) */
    
    /* Track limits */
    uint8_t max_tracks;             /**< Maximum track number */
    uint8_t max_sides;              /**< Maximum sides (1 or 2) */
    bool supports_80_track;         /**< 80-track drives */
    bool supports_40_track;         /**< 40-track drives */
    
    /* Buffer/memory */
    uint32_t flux_buffer_size;      /**< Hardware flux buffer (bytes) */
    uint32_t max_revolutions;       /**< Max revolutions per capture */
    
    /* Interface */
    uint32_t usb_vid;               /**< USB Vendor ID */
    uint32_t usb_pid;               /**< USB Product ID */
    uint32_t baud_rate;             /**< Serial baud rate (if applicable) */
    
    /* Firmware */
    const char* min_firmware;       /**< Minimum firmware version */
    bool firmware_upgradeable;      /**< Can upgrade firmware */
    
    /* Supported platforms */
    uint32_t platforms;             /**< Bitmask of supported platforms */
    
    /* Pricing/availability */
    bool open_source;               /**< Open-source design */
    bool currently_available;       /**< Still in production */
} uft_controller_profile_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Controller Profiles Database
 * ═══════════════════════════════════════════════════════════════════════════ */

static const uft_controller_profile_t UFT_CONTROLLER_PROFILES[] = {
    /* ───────────────────────────────────────────────────────────────────────
     * Greaseweazle
     * ─────────────────────────────────────────────────────────────────────── */
    {
        .type = HAL_CTRL_GREASEWEAZLE,
        .name = "Greaseweazle",
        .manufacturer = "Keir Fraser",
        .description = "Open-source USB floppy controller",
        .website = "https://github.com/keirf/greaseweazle",
        
        .capabilities = UFT_HAL_CAP_READ_FLUX | UFT_HAL_CAP_WRITE_FLUX |
                        UFT_HAL_CAP_INDEX_SENSE | UFT_HAL_CAP_MOTOR_CTRL |
                        UFT_HAL_CAP_DENSITY_SELECT | UFT_HAL_CAP_WRITE_PROTECT |
                        UFT_HAL_CAP_TRACK_0_SENSE | UFT_HAL_CAP_HALF_TRACK |
                        UFT_HAL_CAP_MULTI_REV | UFT_HAL_CAP_INDEX_ALIGN |
                        UFT_HAL_CAP_ERASE | UFT_HAL_CAP_USB | UFT_HAL_CAP_SHUGART |
                        UFT_HAL_CAP_AMIGA_HD | UFT_HAL_CAP_PC_HD | UFT_HAL_CAP_C64_GCR,
        
        .sample_clock_hz = 72000000,    /* 72 MHz (F7) */
        .min_flux_ns = 1000,
        .max_flux_ns = 500000,
        .timing_resolution_ns = 14,     /* ~14ns @ 72MHz */
        
        .max_tracks = 83,
        .max_sides = 2,
        .supports_80_track = true,
        .supports_40_track = true,
        
        .flux_buffer_size = 524288,     /* 512KB */
        .max_revolutions = 32,
        
        .usb_vid = 0x1209,
        .usb_pid = 0x4D69,
        .baud_rate = 0,
        
        .min_firmware = "1.0",
        .firmware_upgradeable = true,
        
        .platforms = (1 << UFT_PLATFORM_IBM_PC) | (1 << UFT_PLATFORM_AMIGA) |
                     (1 << UFT_PLATFORM_ATARI_ST) | (1 << UFT_PLATFORM_COMMODORE),
        
        .open_source = true,
        .currently_available = true
    },
    
    /* ───────────────────────────────────────────────────────────────────────
     * FluxEngine
     * ─────────────────────────────────────────────────────────────────────── */
    {
        .type = HAL_CTRL_FLUXENGINE,
        .name = "FluxEngine",
        .manufacturer = "David Given",
        .description = "Cypress PSoC5-based flux controller",
        .website = "http://cowlark.com/fluxengine/",
        
        .capabilities = UFT_HAL_CAP_READ_FLUX | UFT_HAL_CAP_WRITE_FLUX |
                        UFT_HAL_CAP_INDEX_SENSE | UFT_HAL_CAP_MOTOR_CTRL |
                        UFT_HAL_CAP_DENSITY_SELECT | UFT_HAL_CAP_TRACK_0_SENSE |
                        UFT_HAL_CAP_MULTI_REV | UFT_HAL_CAP_USB | UFT_HAL_CAP_SHUGART,
        
        .sample_clock_hz = 12000000,    /* 12 MHz */
        .min_flux_ns = 2000,
        .max_flux_ns = 500000,
        .timing_resolution_ns = 83,     /* ~83ns @ 12MHz */
        
        .max_tracks = 83,
        .max_sides = 2,
        .supports_80_track = true,
        .supports_40_track = true,
        
        .flux_buffer_size = 65536,
        .max_revolutions = 8,
        
        .usb_vid = 0x04B4,
        .usb_pid = 0x1465,
        .baud_rate = 0,
        
        .min_firmware = "0.1",
        .firmware_upgradeable = true,
        
        .platforms = (1 << UFT_PLATFORM_IBM_PC) | (1 << UFT_PLATFORM_AMIGA) |
                     (1 << UFT_PLATFORM_APPLE_II) | (1 << UFT_PLATFORM_CPM),
        
        .open_source = true,
        .currently_available = true
    },
    
    /* ───────────────────────────────────────────────────────────────────────
     * KryoFlux
     * ─────────────────────────────────────────────────────────────────────── */
    {
        .type = HAL_CTRL_KRYOFLUX,
        .name = "KryoFlux",
        .manufacturer = "Software Preservation Society",
        .description = "Professional flux preservation device",
        .website = "https://kryoflux.com/",
        
        .capabilities = UFT_HAL_CAP_READ_FLUX | UFT_HAL_CAP_WRITE_FLUX |
                        UFT_HAL_CAP_INDEX_SENSE | UFT_HAL_CAP_MOTOR_CTRL |
                        UFT_HAL_CAP_DENSITY_SELECT | UFT_HAL_CAP_WRITE_PROTECT |
                        UFT_HAL_CAP_TRACK_0_SENSE | UFT_HAL_CAP_HALF_TRACK |
                        UFT_HAL_CAP_MULTI_REV | UFT_HAL_CAP_INDEX_ALIGN |
                        UFT_HAL_CAP_PRECOMP | UFT_HAL_CAP_USB | UFT_HAL_CAP_SHUGART |
                        UFT_HAL_CAP_VARIABLE_RATE,
        
        .sample_clock_hz = 24027428,    /* 24.027428 MHz */
        .min_flux_ns = 500,
        .max_flux_ns = 1000000,
        .timing_resolution_ns = 42,     /* ~42ns */
        
        .max_tracks = 86,
        .max_sides = 2,
        .supports_80_track = true,
        .supports_40_track = true,
        
        .flux_buffer_size = 1048576,    /* 1MB */
        .max_revolutions = 64,
        
        .usb_vid = 0x03EB,
        .usb_pid = 0x6124,
        .baud_rate = 0,
        
        .min_firmware = "3.0",
        .firmware_upgradeable = true,
        
        .platforms = (1 << UFT_PLATFORM_IBM_PC) | (1 << UFT_PLATFORM_AMIGA) |
                     (1 << UFT_PLATFORM_ATARI_ST) | (1 << UFT_PLATFORM_APPLE_II) |
                     (1 << UFT_PLATFORM_APPLE_MAC) | (1 << UFT_PLATFORM_COMMODORE),
        
        .open_source = false,
        .currently_available = true
    },
    
    /* ───────────────────────────────────────────────────────────────────────
     * SuperCard Pro
     * ─────────────────────────────────────────────────────────────────────── */
    {
        .type = HAL_CTRL_SCP,
        .name = "SuperCard Pro",
        .manufacturer = "Jim Drew",
        .description = "High-precision flux capture device",
        .website = "https://www.cbmstuff.com/",
        
        .capabilities = UFT_HAL_CAP_READ_FLUX | UFT_HAL_CAP_WRITE_FLUX |
                        UFT_HAL_CAP_INDEX_SENSE | UFT_HAL_CAP_MOTOR_CTRL |
                        UFT_HAL_CAP_DENSITY_SELECT | UFT_HAL_CAP_WRITE_PROTECT |
                        UFT_HAL_CAP_TRACK_0_SENSE | UFT_HAL_CAP_HALF_TRACK |
                        UFT_HAL_CAP_MULTI_REV | UFT_HAL_CAP_INDEX_ALIGN |
                        UFT_HAL_CAP_ERASE | UFT_HAL_CAP_USB | UFT_HAL_CAP_SHUGART |
                        UFT_HAL_CAP_AMIGA_HD | UFT_HAL_CAP_PC_HD,
        
        .sample_clock_hz = 40000000,    /* 40 MHz */
        .min_flux_ns = 500,
        .max_flux_ns = 800000,
        .timing_resolution_ns = 25,     /* 25ns @ 40MHz */
        
        .max_tracks = 84,
        .max_sides = 2,
        .supports_80_track = true,
        .supports_40_track = true,
        
        .flux_buffer_size = 2097152,    /* 2MB */
        .max_revolutions = 5,
        
        .usb_vid = 0x0483,
        .usb_pid = 0x5740,
        .baud_rate = 0,
        
        .min_firmware = "2.0",
        .firmware_upgradeable = true,
        
        .platforms = (1 << UFT_PLATFORM_IBM_PC) | (1 << UFT_PLATFORM_AMIGA) |
                     (1 << UFT_PLATFORM_ATARI_ST) | (1 << UFT_PLATFORM_COMMODORE),
        
        .open_source = false,
        .currently_available = true
    },
    
    /* ───────────────────────────────────────────────────────────────────────
     * Applesauce
     * ─────────────────────────────────────────────────────────────────────── */
    {
        .type = HAL_CTRL_APPLESAUCE,
        .name = "Applesauce",
        .manufacturer = "John Googin",
        .description = "Apple II flux preservation device",
        .website = "https://applesaucefdc.com/",
        
        .capabilities = UFT_HAL_CAP_READ_FLUX | UFT_HAL_CAP_WRITE_FLUX |
                        UFT_HAL_CAP_INDEX_SENSE | UFT_HAL_CAP_MOTOR_CTRL |
                        UFT_HAL_CAP_QUARTER_TRACK | UFT_HAL_CAP_MULTI_REV |
                        UFT_HAL_CAP_USB | UFT_HAL_CAP_APPLE_400K | UFT_HAL_CAP_APPLE_800K,
        
        .sample_clock_hz = 8000000,     /* 8 MHz */
        .min_flux_ns = 2000,
        .max_flux_ns = 500000,
        .timing_resolution_ns = 125,    /* 125ns @ 8MHz */
        
        .max_tracks = 40,
        .max_sides = 2,
        .supports_80_track = false,
        .supports_40_track = true,
        
        .flux_buffer_size = 262144,     /* 256KB */
        .max_revolutions = 16,
        
        .usb_vid = 0x0000,              /* TBD */
        .usb_pid = 0x0000,
        .baud_rate = 0,
        
        .min_firmware = "1.0",
        .firmware_upgradeable = true,
        
        .platforms = (1 << UFT_PLATFORM_APPLE_II) | (1 << UFT_PLATFORM_APPLE_MAC),
        
        .open_source = false,
        .currently_available = true
    },
    
    /* ───────────────────────────────────────────────────────────────────────
     * XUM1541 / ZoomFloppy
     * ─────────────────────────────────────────────────────────────────────── */
    {
        .type = HAL_CTRL_XUM1541,
        .name = "XUM1541/ZoomFloppy",
        .manufacturer = "RETRO Innovations",
        .description = "Commodore IEC/IEEE bus adapter",
        .website = "http://store.go4retro.com/",
        
        .capabilities = UFT_HAL_CAP_USB | UFT_HAL_CAP_IEC |
                        UFT_HAL_CAP_C64_GCR | UFT_HAL_CAP_READ_FLUX,
        
        .sample_clock_hz = 0,           /* Drive-dependent */
        .min_flux_ns = 0,
        .max_flux_ns = 0,
        .timing_resolution_ns = 0,
        
        .max_tracks = 42,               /* C64: 35-42 tracks */
        .max_sides = 1,
        .supports_80_track = false,
        .supports_40_track = true,
        
        .flux_buffer_size = 0,
        .max_revolutions = 1,
        
        .usb_vid = 0x16D0,
        .usb_pid = 0x0504,
        .baud_rate = 0,
        
        .min_firmware = "1.0",
        .firmware_upgradeable = true,
        
        .platforms = (1 << UFT_PLATFORM_COMMODORE),
        
        .open_source = true,
        .currently_available = true
    },
    
    /* ───────────────────────────────────────────────────────────────────────
     * ZoomFloppy (alias)
     * ─────────────────────────────────────────────────────────────────────── */
    {
        .type = HAL_CTRL_ZOOMFLOPPY,
        .name = "ZoomFloppy",
        .manufacturer = "RETRO Innovations",
        .description = "USB Commodore IEC adapter",
        .website = "http://store.go4retro.com/",
        
        .capabilities = UFT_HAL_CAP_USB | UFT_HAL_CAP_IEC |
                        UFT_HAL_CAP_C64_GCR | UFT_HAL_CAP_READ_FLUX,
        
        .sample_clock_hz = 0,
        .min_flux_ns = 0,
        .max_flux_ns = 0,
        .timing_resolution_ns = 0,
        
        .max_tracks = 42,
        .max_sides = 1,
        .supports_80_track = false,
        .supports_40_track = true,
        
        .flux_buffer_size = 0,
        .max_revolutions = 1,
        
        .usb_vid = 0x16D0,
        .usb_pid = 0x0504,
        .baud_rate = 0,
        
        .min_firmware = "1.0",
        .firmware_upgradeable = true,
        
        .platforms = (1 << UFT_PLATFORM_COMMODORE),
        
        .open_source = true,
        .currently_available = true
    },
    
    /* ───────────────────────────────────────────────────────────────────────
     * FC5025
     * ─────────────────────────────────────────────────────────────────────── */
    {
        .type = HAL_CTRL_FC5025,
        .name = "FC5025",
        .manufacturer = "Device Side Data",
        .description = "5.25\" floppy controller",
        .website = "http://www.deviceside.com/",
        
        .capabilities = UFT_HAL_CAP_READ_FLUX | UFT_HAL_CAP_INDEX_SENSE |
                        UFT_HAL_CAP_MOTOR_CTRL | UFT_HAL_CAP_DENSITY_SELECT |
                        UFT_HAL_CAP_USB | UFT_HAL_CAP_SHUGART |
                        UFT_HAL_CAP_MFM_DECODE | UFT_HAL_CAP_FM_DECODE,
        
        .sample_clock_hz = 0,           /* Hardware decode */
        .min_flux_ns = 0,
        .max_flux_ns = 0,
        .timing_resolution_ns = 0,
        
        .max_tracks = 83,
        .max_sides = 2,
        .supports_80_track = true,
        .supports_40_track = true,
        
        .flux_buffer_size = 32768,
        .max_revolutions = 2,
        
        .usb_vid = 0x16C0,
        .usb_pid = 0x06D6,
        .baud_rate = 0,
        
        .min_firmware = "1.0",
        .firmware_upgradeable = false,
        
        .platforms = (1 << UFT_PLATFORM_IBM_PC) | (1 << UFT_PLATFORM_CPM),
        
        .open_source = false,
        .currently_available = false    /* Discontinued */
    },
    
    /* Sentinel */
    { .type = HAL_CTRL_COUNT, .name = NULL }
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Profile Access Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get controller profile
 */
static inline const uft_controller_profile_t* uft_hal_get_profile(uft_hal_controller_t type) {
    for (int i = 0; UFT_CONTROLLER_PROFILES[i].name != NULL; i++) {
        if (UFT_CONTROLLER_PROFILES[i].type == type) {
            return &UFT_CONTROLLER_PROFILES[i];
        }
    }
    return NULL;
}

/**
 * @brief Check if controller has capability
 */
static inline bool uft_hal_has_cap(uft_hal_controller_t type, uft_hal_cap_flags_t cap) {
    const uft_controller_profile_t* p = uft_hal_get_profile(type);
    return p ? (p->capabilities & cap) != 0 : false;
}

/**
 * @brief Check if controller supports platform
 */
static inline bool uft_hal_supports_platform(uft_hal_controller_t type, uft_platform_t platform) {
    const uft_controller_profile_t* p = uft_hal_get_profile(type);
    return p ? (p->platforms & (1 << platform)) != 0 : false;
}

/**
 * @brief Get sample clock in Hz
 */
static inline uint32_t uft_hal_get_sample_clock(uft_hal_controller_t type) {
    const uft_controller_profile_t* p = uft_hal_get_profile(type);
    return p ? p->sample_clock_hz : 0;
}

/**
 * @brief Get timing resolution in nanoseconds
 */
static inline uint32_t uft_hal_get_timing_resolution(uft_hal_controller_t type) {
    const uft_controller_profile_t* p = uft_hal_get_profile(type);
    return p ? p->timing_resolution_ns : 0;
}

/**
 * @brief Check if controller is open-source
 */
static inline bool uft_hal_is_open_source(uft_hal_controller_t type) {
    const uft_controller_profile_t* p = uft_hal_get_profile(type);
    return p ? p->open_source : false;
}

/**
 * @brief Check if controller is currently available
 */
static inline bool uft_hal_is_available(uft_hal_controller_t type) {
    const uft_controller_profile_t* p = uft_hal_get_profile(type);
    return p ? p->currently_available : false;
}

/**
 * @brief Find controllers supporting a platform
 */
static inline int uft_hal_find_by_platform(uft_platform_t platform, 
                                            uft_hal_controller_t* types, int max) {
    int count = 0;
    for (int i = 0; UFT_CONTROLLER_PROFILES[i].name != NULL && count < max; i++) {
        if (UFT_CONTROLLER_PROFILES[i].platforms & (1 << platform)) {
            types[count++] = UFT_CONTROLLER_PROFILES[i].type;
        }
    }
    return count;
}

/**
 * @brief Find controllers with capability
 */
static inline int uft_hal_find_by_cap(uft_hal_cap_flags_t cap,
                                       uft_hal_controller_t* types, int max) {
    int count = 0;
    for (int i = 0; UFT_CONTROLLER_PROFILES[i].name != NULL && count < max; i++) {
        if (UFT_CONTROLLER_PROFILES[i].capabilities & cap) {
            types[count++] = UFT_CONTROLLER_PROFILES[i].type;
        }
    }
    return count;
}

/**
 * @brief Print controller profile (debug)
 */
static inline void uft_hal_print_profile(uft_hal_controller_t type) {
    const uft_controller_profile_t* p = uft_hal_get_profile(type);
    if (!p) {
        printf("Unknown controller\n");
        return;
    }
    
    printf("Controller: %s\n", p->name);
    printf("  Manufacturer: %s\n", p->manufacturer);
    printf("  Description: %s\n", p->description);
    printf("  Website: %s\n", p->website);
    printf("  Sample Clock: %u Hz\n", p->sample_clock_hz);
    printf("  Resolution: %u ns\n", p->timing_resolution_ns);
    printf("  Max Tracks: %d\n", p->max_tracks);
    printf("  Open Source: %s\n", p->open_source ? "Yes" : "No");
    printf("  Available: %s\n", p->currently_available ? "Yes" : "No");
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_HAL_PROFILES_H */
