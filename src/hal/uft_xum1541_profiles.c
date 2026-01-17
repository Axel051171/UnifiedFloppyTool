/**
 * @file uft_xum1541_profiles.c
 * @brief XUM1541 Hardware Variant Profiles
 * @version 4.1.1
 * 
 * Supports multiple XUM1541 hardware variants:
 * - ZoomFloppy (commercial, by RETRO Innovations)
 * - XUM1541-II (DIY, by tebl, with 7406 inverter)
 * - XUM1541 Original (DIY, Arduino Pro Micro)
 * - AT90USBKEY (development board)
 * 
 * Each variant has different USB VID/PID and capabilities.
 */

#include "uft/hal/uft_xum1541.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Hardware Variant Definitions
 * ============================================================================ */

typedef enum {
    XUM1541_VARIANT_UNKNOWN = 0,
    XUM1541_VARIANT_ZOOMFLOPPY,      /* Commercial ZoomFloppy */
    XUM1541_VARIANT_ZOOMFLOPPY_IEEE, /* ZoomFloppy with IEEE-488 */
    XUM1541_VARIANT_XUM1541_II,      /* tebl XUM1541-II with 7406 */
    XUM1541_VARIANT_XUM1541_ORIG,    /* Original XUM1541 (Pro Micro) */
    XUM1541_VARIANT_AT90USBKEY,      /* AT90USBKEY dev board */
    XUM1541_VARIANT_BUMBLEB,         /* Bumble-B board */
} xum1541_variant_t;

typedef struct {
    xum1541_variant_t variant;
    const char *name;
    const char *description;
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t revision_min;      /* Minimum firmware revision */
    uint16_t revision_max;      /* Maximum firmware revision (0 = any) */
    
    /* Capabilities */
    bool has_parallel;          /* Parallel port for 2x speed */
    bool has_ieee488;           /* IEEE-488 for vintage drives */
    bool has_7406_inverter;     /* 7406 hex inverter for signal quality */
    bool has_srq_nibbling;      /* SRQ nibbling support */
    bool has_activity_led;      /* Activity LED */
    
    /* Timing parameters */
    int iec_delay_us;           /* IEC bus delay (microseconds) */
    int parallel_delay_us;      /* Parallel transfer delay */
    
    /* Firmware info */
    const char *firmware_file;  /* Recommended firmware file */
    const char *firmware_url;   /* Download URL */
} xum1541_profile_t;

/* ============================================================================
 * Profile Database
 * ============================================================================ */

static const xum1541_profile_t g_xum1541_profiles[] = {
    /* ZoomFloppy - Commercial Version */
    {
        .variant = XUM1541_VARIANT_ZOOMFLOPPY,
        .name = "ZoomFloppy",
        .description = "Commercial XUM1541 by RETRO Innovations",
        .vendor_id = 0x16D0,
        .product_id = 0x0504,
        .revision_min = 0x0007,
        .revision_max = 0,
        .has_parallel = true,
        .has_ieee488 = false,
        .has_7406_inverter = true,
        .has_srq_nibbling = true,
        .has_activity_led = true,
        .iec_delay_us = 10,
        .parallel_delay_us = 5,
        .firmware_file = "xum1541-ZOOMFLOPPY-v08.hex",
        .firmware_url = "https://github.com/OpenCBM/OpenCBM/tree/master/xum1541"
    },
    
    /* ZoomFloppy with IEEE-488 */
    {
        .variant = XUM1541_VARIANT_ZOOMFLOPPY_IEEE,
        .name = "ZoomFloppy+IEEE",
        .description = "ZoomFloppy with IEEE-488 extension",
        .vendor_id = 0x16D0,
        .product_id = 0x0504,
        .revision_min = 0x0007,
        .revision_max = 0,
        .has_parallel = true,
        .has_ieee488 = true,
        .has_7406_inverter = true,
        .has_srq_nibbling = true,
        .has_activity_led = true,
        .iec_delay_us = 10,
        .parallel_delay_us = 5,
        .firmware_file = "xum1541-ZOOMFLOPPY-v08.hex",
        .firmware_url = "https://github.com/OpenCBM/OpenCBM/tree/master/xum1541"
    },
    
    /* XUM1541-II by tebl (with 7406 inverter) */
    {
        .variant = XUM1541_VARIANT_XUM1541_II,
        .name = "XUM1541-II",
        .description = "DIY XUM1541-II by tebl (Pro Micro + 7406)",
        .vendor_id = 0x16D0,
        .product_id = 0x0504,
        .revision_min = 0x0008,
        .revision_max = 0,
        .has_parallel = true,       /* With parallel adapter module */
        .has_ieee488 = false,
        .has_7406_inverter = true,  /* Key feature: 7406 hex inverter */
        .has_srq_nibbling = true,
        .has_activity_led = true,
        .iec_delay_us = 10,
        .parallel_delay_us = 5,
        .firmware_file = "xum1541-PROMICRO_7406-v08.hex",
        .firmware_url = "https://github.com/tebl/C64-XUM1541-II"
    },
    
    /* Original XUM1541 (without 7406) */
    {
        .variant = XUM1541_VARIANT_XUM1541_ORIG,
        .name = "XUM1541",
        .description = "Original DIY XUM1541 (Pro Micro, no inverter)",
        .vendor_id = 0x16D0,
        .product_id = 0x0504,
        .revision_min = 0x0007,
        .revision_max = 0x0007,     /* v07 recommended for non-7406 */
        .has_parallel = false,
        .has_ieee488 = false,
        .has_7406_inverter = false,
        .has_srq_nibbling = false,
        .has_activity_led = false,
        .iec_delay_us = 15,         /* Longer delay without 7406 */
        .parallel_delay_us = 10,
        .firmware_file = "xum1541-PROMICRO-v07.hex",
        .firmware_url = "https://github.com/tebl/C64-XUM1541"
    },
    
    /* AT90USBKEY Development Board */
    {
        .variant = XUM1541_VARIANT_AT90USBKEY,
        .name = "AT90USBKEY",
        .description = "Atmel AT90USBKEY development board",
        .vendor_id = 0x16D0,
        .product_id = 0x0504,
        .revision_min = 0x0001,
        .revision_max = 0,
        .has_parallel = true,
        .has_ieee488 = false,
        .has_7406_inverter = false,
        .has_srq_nibbling = true,
        .has_activity_led = true,
        .iec_delay_us = 12,
        .parallel_delay_us = 8,
        .firmware_file = "xum1541-AT90USBKEY-v08.hex",
        .firmware_url = "http://www.root.org/~nate/c64/xum1541/"
    },
    
    /* Bumble-B Board */
    {
        .variant = XUM1541_VARIANT_BUMBLEB,
        .name = "Bumble-B",
        .description = "Bumble-B daughterboard with ATmega32U2",
        .vendor_id = 0x16D0,
        .product_id = 0x0504,
        .revision_min = 0x0006,
        .revision_max = 0,
        .has_parallel = true,
        .has_ieee488 = false,
        .has_7406_inverter = true,
        .has_srq_nibbling = true,
        .has_activity_led = true,
        .iec_delay_us = 10,
        .parallel_delay_us = 5,
        .firmware_file = "xum1541-BUMBLEB-v08.hex",
        .firmware_url = "https://github.com/OpenCBM/OpenCBM/tree/master/xum1541"
    },
    
    /* End marker */
    { .variant = XUM1541_VARIANT_UNKNOWN, .name = NULL }
};

/* ============================================================================
 * Profile API
 * ============================================================================ */

const xum1541_profile_t* xum1541_get_profile_by_revision(uint16_t revision) {
    /* Check for specific revisions that indicate hardware variants */
    for (int i = 0; g_xum1541_profiles[i].name != NULL; i++) {
        const xum1541_profile_t *p = &g_xum1541_profiles[i];
        
        if (p->revision_min > 0 && revision >= p->revision_min) {
            if (p->revision_max == 0 || revision <= p->revision_max) {
                return p;
            }
        }
    }
    
    /* Default to ZoomFloppy for unknown revisions */
    return &g_xum1541_profiles[0];
}

const xum1541_profile_t* xum1541_get_profile_by_name(const char *name) {
    if (!name) return NULL;
    
    for (int i = 0; g_xum1541_profiles[i].name != NULL; i++) {
        if (strcasecmp(g_xum1541_profiles[i].name, name) == 0) {
            return &g_xum1541_profiles[i];
        }
    }
    
    return NULL;
}

int xum1541_list_profiles(char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) return -1;
    
    size_t offset = 0;
    offset += snprintf(buffer + offset, buffer_size - offset,
        "═══════════════════════════════════════════════════════════════════\n"
        "                    XUM1541 Hardware Profiles\n"
        "═══════════════════════════════════════════════════════════════════\n\n");
    
    for (int i = 0; g_xum1541_profiles[i].name != NULL && offset < buffer_size; i++) {
        const xum1541_profile_t *p = &g_xum1541_profiles[i];
        
        offset += snprintf(buffer + offset, buffer_size - offset,
            "┌─────────────────────────────────────────────────────────────────┐\n"
            "│ %-15s │ %-45s │\n"
            "├─────────────────────────────────────────────────────────────────┤\n"
            "│ VID:PID: %04X:%04X  │  Firmware: v%02X - v%02X                    │\n"
            "│ Parallel: %-3s  IEEE-488: %-3s  7406: %-3s  SRQ: %-3s            │\n"
            "│ Firmware: %-52s │\n"
            "└─────────────────────────────────────────────────────────────────┘\n\n",
            p->name, p->description,
            p->vendor_id, p->product_id,
            p->revision_min, p->revision_max ? p->revision_max : 0xFF,
            p->has_parallel ? "Yes" : "No",
            p->has_ieee488 ? "Yes" : "No",
            p->has_7406_inverter ? "Yes" : "No",
            p->has_srq_nibbling ? "Yes" : "No",
            p->firmware_file);
    }
    
    return (int)offset;
}

void xum1541_print_profile(const xum1541_profile_t *profile) {
    if (!profile) return;
    
    printf("\n");
    printf("XUM1541 Hardware Profile: %s\n", profile->name);
    printf("═════════════════════════════════════════════════════════════════\n");
    printf("Description:    %s\n", profile->description);
    printf("USB VID:PID:    %04X:%04X\n", profile->vendor_id, profile->product_id);
    printf("Firmware Range: v%02X - v%02X\n", 
           profile->revision_min, 
           profile->revision_max ? profile->revision_max : 0xFF);
    printf("\n");
    printf("Capabilities:\n");
    printf("  • Parallel Port:    %s\n", profile->has_parallel ? "Yes (2x speed)" : "No");
    printf("  • IEEE-488:         %s\n", profile->has_ieee488 ? "Yes (8050/8250)" : "No");
    printf("  • 7406 Inverter:    %s\n", profile->has_7406_inverter ? "Yes (better signals)" : "No");
    printf("  • SRQ Nibbling:     %s\n", profile->has_srq_nibbling ? "Yes" : "No");
    printf("  • Activity LED:     %s\n", profile->has_activity_led ? "Yes" : "No");
    printf("\n");
    printf("Timing:\n");
    printf("  • IEC Delay:        %d µs\n", profile->iec_delay_us);
    printf("  • Parallel Delay:   %d µs\n", profile->parallel_delay_us);
    printf("\n");
    printf("Firmware:\n");
    printf("  • File: %s\n", profile->firmware_file);
    printf("  • URL:  %s\n", profile->firmware_url);
    printf("\n");
}
