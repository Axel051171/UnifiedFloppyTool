/**
 * @file uft_fc5025.c
 * @brief FC5025 USB Floppy Controller Implementation
 * 
 * The FC5025 uses libusb for communication. This implementation
 * provides the framework; actual USB communication requires the
 * fc5025 command-line tools or libusb integration.
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#include "uft/hal/uft_fc5025.h"
#include "uft/compat/uft_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/*============================================================================
 * CONFIGURATION
 *============================================================================*/

struct uft_fc_config_s {
    bool connected;
    void *usb_handle;  /* libusb_device_handle* */
    
    uft_fc_format_t format;
    uft_fc_drive_t drive;
    int start_track;
    int end_track;
    int side;
    int retries;
    bool double_step;
    
    char last_error[256];
};

/*============================================================================
 * FORMAT DEFINITIONS
 *============================================================================*/

static const struct {
    uft_fc_format_t format;
    const char *name;
    int tracks;
    int sectors;
    int sector_size;
    bool is_fm;
} g_formats[] = {
    { UFT_FC_FMT_APPLE_DOS32,  "Apple DOS 3.2",    35, 13, 256, false },
    { UFT_FC_FMT_APPLE_DOS33,  "Apple DOS 3.3",    35, 16, 256, false },
    { UFT_FC_FMT_APPLE_PRODOS, "Apple ProDOS",     35, 16, 256, false },
    { UFT_FC_FMT_IBM_FM,       "IBM FM (8\" SSSD)", 77, 26, 128, true },
    { UFT_FC_FMT_IBM_MFM,      "IBM MFM",          77, 26, 256, false },
    { UFT_FC_FMT_TRS80_SSSD,   "TRS-80 SSSD",      35, 10, 256, true },
    { UFT_FC_FMT_TRS80_SSDD,   "TRS-80 SSDD",      40, 18, 256, false },
    { UFT_FC_FMT_TRS80_DSDD,   "TRS-80 DSDD",      40, 18, 256, false },
    { UFT_FC_FMT_KAYPRO,       "Kaypro",           40, 10, 512, false },
    { UFT_FC_FMT_OSBORNE,      "Osborne",          40, 10, 256, false },
    { UFT_FC_FMT_NORTHSTAR,    "North Star",       35, 10, 256, true },
    { UFT_FC_FMT_TI994A,       "TI-99/4A",         40,  9, 256, true },
    { UFT_FC_FMT_ATARI_FM,     "Atari 810 FM",     40, 18, 128, true },
    { UFT_FC_FMT_ATARI_MFM,    "Atari 1050 MFM",   40, 18, 128, false },
    { UFT_FC_FMT_RAW_FM,       "Raw FM",           40,  0,   0, true },
    { UFT_FC_FMT_RAW_MFM,      "Raw MFM",          40,  0,   0, false },
    { UFT_FC_FMT_AUTO,         "Auto-detect",       0,  0,   0, false },
};

#define FORMAT_COUNT (sizeof(g_formats) / sizeof(g_formats[0]))

/*============================================================================
 * LIFECYCLE
 *============================================================================*/

uft_fc_config_t* uft_fc_config_create(void) {
    uft_fc_config_t *cfg = uft_calloc(1, sizeof(uft_fc_config_t));
    if (!cfg) return NULL;
    
    cfg->format = UFT_FC_FMT_AUTO;
    cfg->drive = UFT_FC_DRIVE_525_48TPI;
    cfg->start_track = 0;
    cfg->end_track = 34;
    cfg->side = -1;
    cfg->retries = 3;
    cfg->double_step = false;
    
    return cfg;
}

void uft_fc_config_destroy(uft_fc_config_t *cfg) {
    if (!cfg) return;
    
    if (cfg->connected) {
        uft_fc_close(cfg);
    }
    
    free(cfg);
}

int uft_fc_open(uft_fc_config_t *cfg) {
    if (!cfg) return -1;
    
    /* 
     * FC5025 requires libusb for direct communication.
     * Alternative: Use fc5025 command-line tools via popen().
     */
    snprintf(cfg->last_error, sizeof(cfg->last_error),
             "FC5025 requires libusb integration or fc5025 command-line tools. "
             "Install from: http://www.deviceside.com/fc5025.html");
    
    return -1;
}

void uft_fc_close(uft_fc_config_t *cfg) {
    if (!cfg) return;
    cfg->connected = false;
}

bool uft_fc_is_connected(const uft_fc_config_t *cfg) {
    return cfg && cfg->connected;
}

/*============================================================================
 * CONFIGURATION
 *============================================================================*/

int uft_fc_set_format(uft_fc_config_t *cfg, uft_fc_format_t format) {
    if (!cfg) return -1;
    
    cfg->format = format;
    
    /* Auto-set track range based on format */
    for (size_t i = 0; i < FORMAT_COUNT; i++) {
        if (g_formats[i].format == format && g_formats[i].tracks > 0) {
            cfg->end_track = g_formats[i].tracks - 1;
            break;
        }
    }
    
    return 0;
}

int uft_fc_set_drive(uft_fc_config_t *cfg, uft_fc_drive_t drive) {
    if (!cfg) return -1;
    cfg->drive = drive;
    return 0;
}

int uft_fc_set_track_range(uft_fc_config_t *cfg, int start, int end) {
    if (!cfg) return -1;
    if (start < 0 || end < start) return -1;
    
    cfg->start_track = start;
    cfg->end_track = end;
    return 0;
}

int uft_fc_set_side(uft_fc_config_t *cfg, int side) {
    if (!cfg) return -1;
    if (side < -1 || side > 1) return -1;
    
    cfg->side = side;
    return 0;
}

int uft_fc_set_retries(uft_fc_config_t *cfg, int count) {
    if (!cfg || count < 0 || count > 20) return -1;
    cfg->retries = count;
    return 0;
}

int uft_fc_set_double_step(uft_fc_config_t *cfg, bool enable) {
    if (!cfg) return -1;
    cfg->double_step = enable;
    return 0;
}

/*============================================================================
 * DEVICE INFO
 *============================================================================*/

int uft_fc_detect(int *device_count) {
    if (!device_count) return -1;
    
    /* Would use libusb_get_device_list() and filter by VID/PID */
    *device_count = 0;
    return 0;
}

int uft_fc_get_firmware_version(uft_fc_config_t *cfg, int *version) {
    if (!cfg || !version) return -1;
    
    if (!cfg->connected) {
        snprintf(cfg->last_error, sizeof(cfg->last_error), "Not connected");
        return -1;
    }
    
    *version = 0;
    return 0;
}

/*============================================================================
 * CAPTURE
 *============================================================================*/

int uft_fc_read_track(uft_fc_config_t *cfg, int track, int side,
                       uint8_t **data, size_t *size) {
    if (!cfg || !data || !size) return -1;
    
    (void)track;
    (void)side;
    
    snprintf(cfg->last_error, sizeof(cfg->last_error),
             "FC5025 track read not implemented - requires libusb");
    return -1;
}

int uft_fc_read_disk(uft_fc_config_t *cfg, uft_fc_callback_t callback, void *user) {
    if (!cfg || !callback) return -1;
    
    (void)user;
    
    snprintf(cfg->last_error, sizeof(cfg->last_error),
             "FC5025 disk read not implemented - requires libusb");
    return -1;
}

/*============================================================================
 * UTILITIES
 *============================================================================*/

const char* uft_fc_get_error(const uft_fc_config_t *cfg) {
    return cfg ? cfg->last_error : "NULL config";
}

const char* uft_fc_format_name(uft_fc_format_t format) {
    for (size_t i = 0; i < FORMAT_COUNT; i++) {
        if (g_formats[i].format == format) {
            return g_formats[i].name;
        }
    }
    return "Unknown";
}

const char* uft_fc_drive_name(uft_fc_drive_t drive) {
    switch (drive) {
        case UFT_FC_DRIVE_525_48TPI: return "5.25\" 48 TPI (40 track)";
        case UFT_FC_DRIVE_525_96TPI: return "5.25\" 96 TPI (80 track)";
        case UFT_FC_DRIVE_8_SSSD:    return "8\" SS/SD";
        case UFT_FC_DRIVE_8_DSDD:    return "8\" DS/DD";
        default:                     return "Unknown";
    }
}

int uft_fc_tracks_for_format(uft_fc_format_t format) {
    for (size_t i = 0; i < FORMAT_COUNT; i++) {
        if (g_formats[i].format == format) {
            return g_formats[i].tracks;
        }
    }
    return 0;
}

int uft_fc_sectors_for_format(uft_fc_format_t format) {
    for (size_t i = 0; i < FORMAT_COUNT; i++) {
        if (g_formats[i].format == format) {
            return g_formats[i].sectors;
        }
    }
    return 0;
}
