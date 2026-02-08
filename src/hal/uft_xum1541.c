/**
 * @file uft_xum1541.c
 * @brief XUM1541/ZoomFloppy Implementation
 * 
 * Uses OpenCBM library (libopencbm) for communication with Commodore drives.
 * If OpenCBM is not available, provides helpful error messages.
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#include "uft/hal/uft_xum1541.h"
#include "uft/compat/uft_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

/*============================================================================
 * OPENCBM FUNCTION POINTERS (Dynamic Loading)
 *============================================================================*/

/* OpenCBM handle type */
typedef void* CBM_FILE;

/* Function pointer types */
typedef int (*cbm_driver_open_t)(CBM_FILE*, int);
typedef void (*cbm_driver_close_t)(CBM_FILE);
typedef int (*cbm_listen_t)(CBM_FILE, unsigned char, unsigned char);
typedef int (*cbm_talk_t)(CBM_FILE, unsigned char, unsigned char);
typedef int (*cbm_unlisten_t)(CBM_FILE);
typedef int (*cbm_untalk_t)(CBM_FILE);
typedef int (*cbm_raw_write_t)(CBM_FILE, const void*, size_t);
typedef int (*cbm_raw_read_t)(CBM_FILE, void*, size_t);
typedef int (*cbm_device_status_t)(CBM_FILE, unsigned char, void*, size_t);
typedef int (*cbm_identify_t)(CBM_FILE, unsigned char, void*, size_t);
typedef const char* (*cbm_get_driver_name_t)(int);

/* Global function pointers */
static struct {
    void *lib_handle;
    bool loaded;
    bool available;
    
    cbm_driver_open_t driver_open;
    cbm_driver_close_t driver_close;
    cbm_listen_t listen;
    cbm_talk_t talk;
    cbm_unlisten_t unlisten;
    cbm_untalk_t untalk;
    cbm_raw_write_t raw_write;
    cbm_raw_read_t raw_read;
    cbm_device_status_t device_status;
    cbm_identify_t identify;
    cbm_get_driver_name_t get_driver_name;
} g_opencbm = { 
    .lib_handle = NULL, 
    .loaded = false, 
    .available = false,
    .driver_open = NULL,
    .driver_close = NULL,
    .listen = NULL,
    .talk = NULL,
    .unlisten = NULL,
    .untalk = NULL,
    .raw_write = NULL,
    .raw_read = NULL,
    .device_status = NULL,
    .identify = NULL,
    .get_driver_name = NULL
};

/*============================================================================
 * CONFIGURATION
 *============================================================================*/

struct uft_xum_config_s {
    CBM_FILE cbm_fd;
    bool connected;
    int device_num;
    uft_cbm_drive_t drive_type;
    
    int start_track;
    int end_track;
    int side;
    int retries;
    
    char last_error[256];
};

/*============================================================================
 * LIBRARY LOADING
 *============================================================================*/

static bool load_opencbm(void) {
    if (g_opencbm.loaded) return g_opencbm.available;
    g_opencbm.loaded = true;
    
#ifdef _WIN32
    g_opencbm.lib_handle = LoadLibraryA("opencbm.dll");
    if (!g_opencbm.lib_handle) {
        g_opencbm.lib_handle = LoadLibraryA("C:\\Program Files\\opencbm\\opencbm.dll");
    }
    #define LOAD_FUNC(name) \
        g_opencbm.name = (cbm_##name##_t)GetProcAddress(g_opencbm.lib_handle, "cbm_" #name)
#else
    g_opencbm.lib_handle = dlopen("libopencbm.so", RTLD_NOW);
    if (!g_opencbm.lib_handle) {
        g_opencbm.lib_handle = dlopen("libopencbm.so.0", RTLD_NOW);
    }
    if (!g_opencbm.lib_handle) {
        g_opencbm.lib_handle = dlopen("/usr/local/lib/libopencbm.so", RTLD_NOW);
    }
    #define LOAD_FUNC(name) \
        g_opencbm.name = (cbm_##name##_t)dlsym(g_opencbm.lib_handle, "cbm_" #name)
#endif
    
    if (!g_opencbm.lib_handle) {
        return false;
    }
    
    LOAD_FUNC(driver_open);
    LOAD_FUNC(driver_close);
    LOAD_FUNC(listen);
    LOAD_FUNC(talk);
    LOAD_FUNC(unlisten);
    LOAD_FUNC(untalk);
    LOAD_FUNC(raw_write);
    LOAD_FUNC(raw_read);
    LOAD_FUNC(device_status);
    LOAD_FUNC(identify);
    LOAD_FUNC(get_driver_name);
    
    #undef LOAD_FUNC
    
    /* Check minimum required functions */
    g_opencbm.available = (g_opencbm.driver_open != NULL &&
                           g_opencbm.driver_close != NULL &&
                           g_opencbm.listen != NULL &&
                           g_opencbm.talk != NULL);
    
    return g_opencbm.available;
}

/*============================================================================
 * LIFECYCLE
 *============================================================================*/

uft_xum_config_t* uft_xum_config_create(void) {
    uft_xum_config_t *cfg = uft_calloc(1, sizeof(uft_xum_config_t));
    if (!cfg) return NULL;
    
    cfg->device_num = 8;  /* Default Commodore device */
    cfg->drive_type = UFT_CBM_DRIVE_AUTO;
    cfg->start_track = 1;
    cfg->end_track = 35;
    cfg->side = 0;
    cfg->retries = 3;
    
    if (!load_opencbm()) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "OpenCBM library not found. Install from: "
                 "https://github.com/OpenCBM/OpenCBM");
    }
    
    return cfg;
}

void uft_xum_config_destroy(uft_xum_config_t *cfg) {
    if (!cfg) return;
    
    if (cfg->connected) {
        uft_xum_close(cfg);
    }
    
    free(cfg);
}

int uft_xum_open(uft_xum_config_t *cfg, int device_num) {
    if (!cfg) return -1;
    
    if (!g_opencbm.available) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "OpenCBM not available. Install XUM1541/ZoomFloppy drivers.");
        return -1;
    }
    
    cfg->device_num = device_num;
    
    if (g_opencbm.driver_open(&cfg->cbm_fd, 0) != 0) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Failed to open XUM1541/ZoomFloppy device");
        return -1;
    }
    
    cfg->connected = true;
    
    /* Auto-detect drive type */
    if (cfg->drive_type == UFT_CBM_DRIVE_AUTO) {
        uft_xum_identify_drive(cfg, &cfg->drive_type);
    }
    
    return 0;
}

void uft_xum_close(uft_xum_config_t *cfg) {
    if (!cfg || !cfg->connected) return;
    
    if (g_opencbm.driver_close) {
        g_opencbm.driver_close(cfg->cbm_fd);
    }
    
    cfg->connected = false;
}

bool uft_xum_is_connected(const uft_xum_config_t *cfg) {
    return cfg && cfg->connected;
}

/*============================================================================
 * DEVICE INFO
 *============================================================================*/

int uft_xum_detect(int *device_count) {
    if (!device_count) return -1;
    *device_count = 0;
    
    if (!load_opencbm() || !g_opencbm.available) {
        return -1;
    }
    
    /* Try to open driver */
    CBM_FILE fd;
    if (g_opencbm.driver_open(&fd, 0) == 0) {
        *device_count = 1;  /* At least one adapter present */
        g_opencbm.driver_close(fd);
    }
    
    return 0;
}

int uft_xum_identify_drive(uft_xum_config_t *cfg, uft_cbm_drive_t *type) {
    if (!cfg || !cfg->connected || !type) return -1;
    
    if (!g_opencbm.identify) {
        *type = UFT_CBM_DRIVE_1541;  /* Default assumption */
        return 0;
    }
    
    char id_string[64] = {0};
    if (g_opencbm.identify(cfg->cbm_fd, (unsigned char)cfg->device_num, 
                           id_string, sizeof(id_string)) != 0) {
        *type = UFT_CBM_DRIVE_1541;
        return 0;
    }
    
    /* Parse identification string */
    if (strstr(id_string, "1581")) {
        *type = UFT_CBM_DRIVE_1581;
        cfg->end_track = 80;
    } else if (strstr(id_string, "1571")) {
        *type = UFT_CBM_DRIVE_1571;
        cfg->end_track = 35;
    } else if (strstr(id_string, "1541") || strstr(id_string, "1540")) {
        *type = UFT_CBM_DRIVE_1541;
        cfg->end_track = 35;
    } else if (strstr(id_string, "8050")) {
        *type = UFT_CBM_DRIVE_8050;
        cfg->end_track = 77;
    } else if (strstr(id_string, "8250")) {
        *type = UFT_CBM_DRIVE_8250;
        cfg->end_track = 77;
    } else if (strstr(id_string, "SFD")) {
        *type = UFT_CBM_DRIVE_SFD1001;
        cfg->end_track = 77;
    } else {
        *type = UFT_CBM_DRIVE_1541;
        cfg->end_track = 35;
    }
    
    return 0;
}

int uft_xum_get_status(uft_xum_config_t *cfg, char *status, size_t max_len) {
    if (!cfg || !cfg->connected || !status) return -1;
    
    if (!g_opencbm.device_status) {
        strncpy(status, "00, OK,00,00", max_len - 1);
        return 0;
    }
    
    return g_opencbm.device_status(cfg->cbm_fd, (unsigned char)cfg->device_num,
                                    status, max_len);
}

/*============================================================================
 * CONFIGURATION
 *============================================================================*/

int uft_xum_set_device(uft_xum_config_t *cfg, int device_num) {
    if (!cfg) return -1;
    if (device_num < 4 || device_num > 30) return -1;
    
    cfg->device_num = device_num;
    return 0;
}

int uft_xum_set_track_range(uft_xum_config_t *cfg, int start, int end) {
    if (!cfg) return -1;
    if (start < 1 || start > 77) return -1;
    if (end < start || end > 77) return -1;
    
    cfg->start_track = start;
    cfg->end_track = end;
    return 0;
}

int uft_xum_set_side(uft_xum_config_t *cfg, int side) {
    if (!cfg) return -1;
    if (side < -1 || side > 1) return -1;
    
    cfg->side = side;
    return 0;
}

int uft_xum_set_retries(uft_xum_config_t *cfg, int count) {
    if (!cfg || count < 0 || count > 20) return -1;
    cfg->retries = count;
    return 0;
}

/*============================================================================
 * CAPTURE (Simplified - requires OpenCBM nibtools for full GCR)
 *============================================================================*/

int uft_xum_read_track_gcr(uft_xum_config_t *cfg, int track, int side,
                            uint8_t **gcr, size_t *size) {
    if (!cfg || !cfg->connected || !gcr || !size) return -1;
    
    (void)track;
    (void)side;
    
    snprintf(cfg->last_error, sizeof(cfg->last_error),
             "Raw GCR read requires nibtools integration. "
             "Use uft_xum_read_track() for sector-level access.");
    return -1;
}

int uft_xum_read_track(uft_xum_config_t *cfg, int track, int side,
                        uint8_t *sectors, int *sector_count,
                        uint8_t *errors) {
    if (!cfg || !cfg->connected || !sectors || !sector_count) return -1;
    
    int num_sectors = uft_xum_sectors_for_track(cfg->drive_type, track);
    if (num_sectors <= 0) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Invalid track %d for drive type", track);
        return -1;
    }
    
    (void)side;
    (void)errors;
    
    /* Would use OpenCBM parallel/serial protocol here */
    snprintf(cfg->last_error, sizeof(cfg->last_error),
             "Sector read not yet implemented - requires custom drive code upload");
    
    *sector_count = 0;
    return -1;
}

int uft_xum_read_disk(uft_xum_config_t *cfg, uft_xum_callback_t callback,
                       void *user) {
    if (!cfg || !callback) return -1;
    
    if (!cfg->connected) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Not connected to XUM1541/ZoomFloppy");
        return -1;
    }
    
    snprintf(cfg->last_error, sizeof(cfg->last_error),
             "Full disk read requires nibtools or custom drive code. "
             "Use OpenCBM's d64copy for standard reads.");
    
    (void)user;
    return -1;
}

int uft_xum_write_track(uft_xum_config_t *cfg, int track, int side,
                         const uint8_t *data, size_t size) {
    if (!cfg || !cfg->connected || !data) return -1;
    
    (void)track;
    (void)side;
    (void)size;
    
    snprintf(cfg->last_error, sizeof(cfg->last_error),
             "Track write requires nibtools integration");
    return -1;
}

/*============================================================================
 * LOW-LEVEL IEC
 *============================================================================*/

int uft_xum_iec_listen(uft_xum_config_t *cfg, int device, int secondary) {
    if (!cfg || !cfg->connected || !g_opencbm.listen) return -1;
    return g_opencbm.listen(cfg->cbm_fd, (unsigned char)device, 
                            (unsigned char)secondary);
}

int uft_xum_iec_talk(uft_xum_config_t *cfg, int device, int secondary) {
    if (!cfg || !cfg->connected || !g_opencbm.talk) return -1;
    return g_opencbm.talk(cfg->cbm_fd, (unsigned char)device,
                          (unsigned char)secondary);
}

int uft_xum_iec_unlisten(uft_xum_config_t *cfg) {
    if (!cfg || !cfg->connected || !g_opencbm.unlisten) return -1;
    return g_opencbm.unlisten(cfg->cbm_fd);
}

int uft_xum_iec_untalk(uft_xum_config_t *cfg) {
    if (!cfg || !cfg->connected || !g_opencbm.untalk) return -1;
    return g_opencbm.untalk(cfg->cbm_fd);
}

int uft_xum_iec_write(uft_xum_config_t *cfg, const uint8_t *data, size_t len) {
    if (!cfg || !cfg->connected || !data || !g_opencbm.raw_write) return -1;
    return g_opencbm.raw_write(cfg->cbm_fd, data, len);
}

int uft_xum_iec_read(uft_xum_config_t *cfg, uint8_t *data, size_t max_len) {
    if (!cfg || !cfg->connected || !data || !g_opencbm.raw_read) return -1;
    return g_opencbm.raw_read(cfg->cbm_fd, data, max_len);
}

/*============================================================================
 * UTILITIES
 *============================================================================*/

const char* uft_xum_get_error(const uft_xum_config_t *cfg) {
    return cfg ? cfg->last_error : "NULL config";
}

const char* uft_xum_drive_name(uft_cbm_drive_t type) {
    switch (type) {
        case UFT_CBM_DRIVE_AUTO:    return "Auto-detect";
        case UFT_CBM_DRIVE_1541:    return "Commodore 1541";
        case UFT_CBM_DRIVE_1541_II: return "Commodore 1541-II";
        case UFT_CBM_DRIVE_1570:    return "Commodore 1570";
        case UFT_CBM_DRIVE_1571:    return "Commodore 1571";
        case UFT_CBM_DRIVE_1581:    return "Commodore 1581";
        case UFT_CBM_DRIVE_SFD1001: return "Commodore SFD-1001";
        case UFT_CBM_DRIVE_8050:    return "Commodore 8050";
        case UFT_CBM_DRIVE_8250:    return "Commodore 8250";
        default:                    return "Unknown";
    }
}

int uft_xum_tracks_for_drive(uft_cbm_drive_t type) {
    switch (type) {
        case UFT_CBM_DRIVE_1541:
        case UFT_CBM_DRIVE_1541_II:
        case UFT_CBM_DRIVE_1570:
        case UFT_CBM_DRIVE_1571:
            return 35;  /* Standard, can extend to 40 */
        case UFT_CBM_DRIVE_1581:
            return 80;
        case UFT_CBM_DRIVE_SFD1001:
        case UFT_CBM_DRIVE_8050:
        case UFT_CBM_DRIVE_8250:
            return 77;
        default:
            return 35;
    }
}

int uft_xum_sectors_for_track(uft_cbm_drive_t type, int track) {
    /* 1541/1571 sector layout */
    if (type == UFT_CBM_DRIVE_1541 || type == UFT_CBM_DRIVE_1541_II ||
        type == UFT_CBM_DRIVE_1570 || type == UFT_CBM_DRIVE_1571 ||
        type == UFT_CBM_DRIVE_AUTO) {
        if (track >= 1 && track <= 17) return 21;
        if (track >= 18 && track <= 24) return 19;
        if (track >= 25 && track <= 30) return 18;
        if (track >= 31 && track <= 40) return 17;
        return 0;
    }
    
    /* 1581 - 10 sectors per track */
    if (type == UFT_CBM_DRIVE_1581) {
        return (track >= 1 && track <= 80) ? 40 : 0;
    }
    
    /* 8050/8250/SFD-1001 */
    if (type == UFT_CBM_DRIVE_8050 || type == UFT_CBM_DRIVE_8250 ||
        type == UFT_CBM_DRIVE_SFD1001) {
        if (track >= 1 && track <= 39) return 29;
        if (track >= 40 && track <= 53) return 27;
        if (track >= 54 && track <= 64) return 25;
        if (track >= 65 && track <= 77) return 23;
        return 0;
    }
    
    return 0;
}
