/**
 * @file uft_applesauce.c
 * @brief Applesauce Floppy Controller Implementation
 * 
 * Applesauce uses a proprietary serial protocol over USB.
 * This implementation provides the framework; full support requires
 * documentation from Applesauce developers.
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#include "uft/hal/uft_applesauce.h"
#include "uft/compat/uft_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
typedef HANDLE serial_t;
#define INVALID_SER INVALID_HANDLE_VALUE
#else
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
typedef int serial_t;
#define INVALID_SER (-1)
#endif

/*============================================================================
 * CONFIGURATION
 *============================================================================*/

struct uft_as_config_s {
    serial_t handle;
    bool connected;
    char port[64];
    
    uft_as_format_t format;
    uft_as_drive_t drive;
    int start_track;
    int end_track;
    int side;
    int revolutions;
    
    char firmware[32];
    char last_error[256];
};

/*============================================================================
 * FORMAT INFO
 *============================================================================*/

static const struct {
    uft_as_format_t format;
    const char *name;
    int tracks;
    int sides;
} g_formats[] = {
    { UFT_AS_FMT_DOS32,    "Apple DOS 3.2",    35, 1 },
    { UFT_AS_FMT_DOS33,    "Apple DOS 3.3",    35, 1 },
    { UFT_AS_FMT_PRODOS,   "Apple ProDOS",     35, 1 },
    { UFT_AS_FMT_PASCAL,   "Apple Pascal",     35, 1 },
    { UFT_AS_FMT_CPM,      "Apple CP/M",       35, 1 },
    { UFT_AS_FMT_LISA,     "Apple Lisa",       46, 2 },
    { UFT_AS_FMT_MAC_400K, "Macintosh 400K",   80, 1 },
    { UFT_AS_FMT_MAC_800K, "Macintosh 800K",   80, 2 },
    { UFT_AS_FMT_APPLE3,   "Apple III SOS",    35, 1 },
    { UFT_AS_FMT_RAW,      "Raw Flux",         35, 1 },
    { UFT_AS_FMT_AUTO,     "Auto-detect",       0, 0 },
};

#define FORMAT_COUNT (sizeof(g_formats) / sizeof(g_formats[0]))

/*============================================================================
 * LIFECYCLE
 *============================================================================*/

uft_as_config_t* uft_as_config_create(void) {
    uft_as_config_t *cfg = uft_calloc(1, sizeof(uft_as_config_t));
    if (!cfg) return NULL;
    
    cfg->handle = INVALID_SER;
    cfg->format = UFT_AS_FMT_AUTO;
    cfg->drive = UFT_AS_DRIVE_525;
    cfg->start_track = 0;
    cfg->end_track = 34;
    cfg->side = -1;
    cfg->revolutions = 2;
    
    return cfg;
}

void uft_as_config_destroy(uft_as_config_t *cfg) {
    if (!cfg) return;
    if (cfg->connected) {
        uft_as_close(cfg);
    }
    free(cfg);
}

int uft_as_open(uft_as_config_t *cfg, const char *port) {
    if (!cfg || !port) return -1;
    
    strncpy(cfg->port, port, sizeof(cfg->port) - 1);
    
    /*
     * Applesauce uses a proprietary protocol.
     * This would require:
     * 1. Opening serial port at correct baud rate
     * 2. Sending initialization sequence
     * 3. Reading firmware version
     */
    snprintf(cfg->last_error, sizeof(cfg->last_error),
             "Applesauce protocol not yet implemented. "
             "Device uses proprietary serial communication.");
    
    return -1;
}

void uft_as_close(uft_as_config_t *cfg) {
    if (!cfg) return;
    
#ifdef _WIN32
    if (cfg->handle != INVALID_SER) CloseHandle(cfg->handle);
#else
    if (cfg->handle != INVALID_SER) close(cfg->handle);
#endif
    
    cfg->handle = INVALID_SER;
    cfg->connected = false;
}

bool uft_as_is_connected(const uft_as_config_t *cfg) {
    return cfg && cfg->connected;
}

/*============================================================================
 * CONFIGURATION
 *============================================================================*/

int uft_as_set_format(uft_as_config_t *cfg, uft_as_format_t format) {
    if (!cfg) return -1;
    cfg->format = format;
    
    for (size_t i = 0; i < FORMAT_COUNT; i++) {
        if (g_formats[i].format == format && g_formats[i].tracks > 0) {
            cfg->end_track = g_formats[i].tracks - 1;
            break;
        }
    }
    return 0;
}

int uft_as_set_drive(uft_as_config_t *cfg, uft_as_drive_t drive) {
    if (!cfg) return -1;
    cfg->drive = drive;
    return 0;
}

int uft_as_set_track_range(uft_as_config_t *cfg, int start, int end) {
    if (!cfg) return -1;
    if (start < 0 || end < start) return -1;
    cfg->start_track = start;
    cfg->end_track = end;
    return 0;
}

int uft_as_set_side(uft_as_config_t *cfg, int side) {
    if (!cfg) return -1;
    if (side < -1 || side > 1) return -1;
    cfg->side = side;
    return 0;
}

int uft_as_set_revolutions(uft_as_config_t *cfg, int revs) {
    if (!cfg || revs < 1 || revs > 10) return -1;
    cfg->revolutions = revs;
    return 0;
}

/*============================================================================
 * DEVICE INFO
 *============================================================================*/

int uft_as_detect(char ports[][64], int max_ports) {
    (void)ports;
    (void)max_ports;
    /* Would scan serial ports for Applesauce signature */
    return 0;
}

int uft_as_get_firmware_version(uft_as_config_t *cfg, char *version, size_t max_len) {
    if (!cfg || !version) return -1;
    
    if (!cfg->connected) {
        snprintf(cfg->last_error, sizeof(cfg->last_error), "Not connected");
        return -1;
    }
    
    strncpy(version, cfg->firmware, max_len - 1);
    return 0;
}

/*============================================================================
 * CAPTURE
 *============================================================================*/

int uft_as_read_track(uft_as_config_t *cfg, int track, int side,
                       uint32_t **flux, size_t *count) {
    if (!cfg || !flux || !count) return -1;
    
    (void)track;
    (void)side;
    
    snprintf(cfg->last_error, sizeof(cfg->last_error),
             "Applesauce track read not implemented");
    return -1;
}

int uft_as_read_disk(uft_as_config_t *cfg, uft_as_callback_t callback, void *user) {
    if (!cfg || !callback) return -1;
    
    (void)user;
    
    snprintf(cfg->last_error, sizeof(cfg->last_error),
             "Applesauce disk read not implemented");
    return -1;
}

int uft_as_write_track(uft_as_config_t *cfg, int track, int side,
                        const uint32_t *flux, size_t count) {
    if (!cfg || !flux) return -1;
    
    (void)track;
    (void)side;
    (void)count;
    
    snprintf(cfg->last_error, sizeof(cfg->last_error),
             "Applesauce track write not implemented");
    return -1;
}

/*============================================================================
 * UTILITIES
 *============================================================================*/

double uft_as_ticks_to_ns(uint32_t ticks) {
    return (double)ticks * 125.0;  /* 8MHz = 125ns per tick */
}

uint32_t uft_as_ns_to_ticks(double ns) {
    return (uint32_t)(ns / 125.0 + 0.5);
}

double uft_as_get_sample_clock(void) {
    return (double)UFT_AS_SAMPLE_CLOCK;
}

const char* uft_as_get_error(const uft_as_config_t *cfg) {
    return cfg ? cfg->last_error : "NULL config";
}

const char* uft_as_format_name(uft_as_format_t format) {
    for (size_t i = 0; i < FORMAT_COUNT; i++) {
        if (g_formats[i].format == format) {
            return g_formats[i].name;
        }
    }
    return "Unknown";
}
