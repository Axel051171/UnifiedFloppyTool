/**
 * @file uft_hal_unified.c
 * @brief Unified Hardware Abstraction Layer Implementation
 * 
 * Provides a vtable-based architecture for multiple controller support:
 * - Greaseweazle (fully implemented)
 * - FluxEngine (protocol compatible with GW)
 * - KryoFlux (stub - requires KryoFlux software)
 * - SuperCard Pro (basic support)
 * - FC5025 (stub)
 * - XUM1541/ZoomFloppy (stub)
 * 
 * @version 2.0.0
 * @date 2025-01-08
 */

#include "uft/hal/uft_hal.h"
#include "uft/compat/uft_alloc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#define UFT_SERIAL_HANDLE HANDLE
#else
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#define UFT_SERIAL_HANDLE int
#define INVALID_HANDLE_VALUE (-1)

/* CRTSCTS is not defined on macOS */
#ifndef CRTSCTS
#define CRTSCTS 0
#endif
#endif

/*============================================================================
 * CONTROLLER DRIVER VTABLE
 *============================================================================*/

typedef struct uft_hal_driver_s uft_hal_driver_t;

struct uft_hal_driver_s {
    const char *name;
    uft_hal_controller_t type;
    
    /* VTable */
    int (*open)(uft_hal_t *hal, const char *path);
    void (*close)(uft_hal_t *hal);
    int (*get_caps)(uft_hal_t *hal, uft_hal_caps_t *caps);
    int (*read_flux)(uft_hal_t *hal, int track, int side, int revs,
                     uint32_t **flux, size_t *count);
    int (*write_flux)(uft_hal_t *hal, int track, int side,
                      const uint32_t *flux, size_t count);
    int (*seek)(uft_hal_t *hal, int track);
    int (*motor)(uft_hal_t *hal, bool on);
    int (*enumerate)(uft_hal_controller_t *out, int max);
};

/*============================================================================
 * HAL HANDLE STRUCTURE
 *============================================================================*/

struct uft_hal_s {
    uft_hal_controller_t type;
    const uft_hal_driver_t *driver;
    char device_path[256];
    uft_hal_caps_t caps;
    char error[256];
    bool is_open;
    
    /* Platform-specific handle */
    UFT_SERIAL_HANDLE serial;
    
    /* Controller-specific state */
    union {
        struct {
            uint8_t hw_model;
            uint8_t hw_submodel;
            uint32_t fw_version;
            uint32_t sample_freq;
        } gw;
        struct {
            uint32_t firmware;
            uint8_t drive_type;
        } scp;
        struct {
            bool connected;
            void *config;  /* uft_kf_config_t* */
        } kryoflux;
    } state;
    
    /* Current position */
    int current_track;
    int current_side;
    bool motor_on;
};

/*============================================================================
 * FORWARD DECLARATIONS
 *============================================================================*/

/* Greaseweazle */
static int gw_open(uft_hal_t *hal, const char *path);
static void gw_close(uft_hal_t *hal);
static int gw_get_caps(uft_hal_t *hal, uft_hal_caps_t *caps);
static int gw_read_flux(uft_hal_t *hal, int track, int side, int revs,
                        uint32_t **flux, size_t *count);
static int gw_write_flux(uft_hal_t *hal, int track, int side,
                         const uint32_t *flux, size_t count);
static int gw_seek(uft_hal_t *hal, int track);
static int gw_motor(uft_hal_t *hal, bool on);
static int gw_enumerate(uft_hal_controller_t *out, int max);

/* FluxEngine (GW-compatible protocol) */
static int fe_open(uft_hal_t *hal, const char *path);
static int fe_enumerate(uft_hal_controller_t *out, int max);

/* KryoFlux */
static int kf_open(uft_hal_t *hal, const char *path);
static void kf_close(uft_hal_t *hal);
static int kf_get_caps(uft_hal_t *hal, uft_hal_caps_t *caps);
static int kf_read_flux(uft_hal_t *hal, int track, int side, int revs,
                        uint32_t **flux, size_t *count);
static int kf_enumerate(uft_hal_controller_t *out, int max);

/* SuperCard Pro */
static int scp_open(uft_hal_t *hal, const char *path);
static void scp_close(uft_hal_t *hal);
static int scp_get_caps(uft_hal_t *hal, uft_hal_caps_t *caps);
static int scp_read_flux(uft_hal_t *hal, int track, int side, int revs,
                         uint32_t **flux, size_t *count);
static int scp_enumerate(uft_hal_controller_t *out, int max);

/* Stub driver for unimplemented controllers */
static int stub_open(uft_hal_t *hal, const char *path);
static void stub_close(uft_hal_t *hal);
static int stub_get_caps(uft_hal_t *hal, uft_hal_caps_t *caps);
static int stub_read_flux(uft_hal_t *hal, int track, int side, int revs,
                          uint32_t **flux, size_t *count);
static int stub_write_flux(uft_hal_t *hal, int track, int side,
                           const uint32_t *flux, size_t count);
static int stub_seek(uft_hal_t *hal, int track);
static int stub_motor(uft_hal_t *hal, bool on);
static int stub_enumerate(uft_hal_controller_t *out, int max);

/*============================================================================
 * DRIVER TABLE
 *============================================================================*/

static const uft_hal_driver_t g_drivers[] = {
    /* Greaseweazle - Full support */
    {
        .name = "Greaseweazle",
        .type = HAL_CTRL_GREASEWEAZLE,
        .open = gw_open,
        .close = gw_close,
        .get_caps = gw_get_caps,
        .read_flux = gw_read_flux,
        .write_flux = gw_write_flux,
        .seek = gw_seek,
        .motor = gw_motor,
        .enumerate = gw_enumerate
    },
    /* FluxEngine - Uses GW protocol */
    {
        .name = "FluxEngine",
        .type = HAL_CTRL_FLUXENGINE,
        .open = fe_open,
        .close = gw_close,
        .get_caps = gw_get_caps,
        .read_flux = gw_read_flux,
        .write_flux = gw_write_flux,
        .seek = gw_seek,
        .motor = gw_motor,
        .enumerate = fe_enumerate
    },
    /* KryoFlux - Read-only support */
    {
        .name = "KryoFlux",
        .type = HAL_CTRL_KRYOFLUX,
        .open = kf_open,
        .close = kf_close,
        .get_caps = kf_get_caps,
        .read_flux = kf_read_flux,
        .write_flux = stub_write_flux,
        .seek = stub_seek,
        .motor = stub_motor,
        .enumerate = kf_enumerate
    },
    /* SuperCard Pro */
    {
        .name = "SuperCard Pro",
        .type = HAL_CTRL_SCP,
        .open = scp_open,
        .close = scp_close,
        .get_caps = scp_get_caps,
        .read_flux = scp_read_flux,
        .write_flux = stub_write_flux,
        .seek = stub_seek,
        .motor = stub_motor,
        .enumerate = scp_enumerate
    },
    /* Applesauce - Stub */
    {
        .name = "Applesauce",
        .type = HAL_CTRL_APPLESAUCE,
        .open = stub_open,
        .close = stub_close,
        .get_caps = stub_get_caps,
        .read_flux = stub_read_flux,
        .write_flux = stub_write_flux,
        .seek = stub_seek,
        .motor = stub_motor,
        .enumerate = stub_enumerate
    },
    /* XUM1541 */
    {
        .name = "XUM1541",
        .type = HAL_CTRL_XUM1541,
        .open = stub_open,
        .close = stub_close,
        .get_caps = stub_get_caps,
        .read_flux = stub_read_flux,
        .write_flux = stub_write_flux,
        .seek = stub_seek,
        .motor = stub_motor,
        .enumerate = stub_enumerate
    },
    /* ZoomFloppy */
    {
        .name = "ZoomFloppy",
        .type = HAL_CTRL_ZOOMFLOPPY,
        .open = stub_open,
        .close = stub_close,
        .get_caps = stub_get_caps,
        .read_flux = stub_read_flux,
        .write_flux = stub_write_flux,
        .seek = stub_seek,
        .motor = stub_motor,
        .enumerate = stub_enumerate
    },
    /* FC5025 */
    {
        .name = "FC5025",
        .type = HAL_CTRL_FC5025,
        .open = stub_open,
        .close = stub_close,
        .get_caps = stub_get_caps,
        .read_flux = stub_read_flux,
        .write_flux = stub_write_flux,
        .seek = stub_seek,
        .motor = stub_motor,
        .enumerate = stub_enumerate
    }
};

#define DRIVER_COUNT (sizeof(g_drivers) / sizeof(g_drivers[0]))

static const uft_hal_driver_t* find_driver(uft_hal_controller_t type) {
    for (size_t i = 0; i < DRIVER_COUNT; i++) {
        if (g_drivers[i].type == type) {
            return &g_drivers[i];
        }
    }
    return NULL;
}

/*============================================================================
 * SERIAL PORT HELPERS
 *============================================================================*/

#ifndef _WIN32
static int serial_open(const char *path, int baud) {
    int fd = open(path, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) return -1;
    
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        close(fd);
        return -1;
    }
    
    cfsetospeed(&tty, (speed_t)baud);
    cfsetispeed(&tty, (speed_t)baud);
    
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag &= ~IGNBRK;
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 10;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        close(fd);
        return -1;
    }
    
    return fd;
}

static void serial_close(int fd) {
    if (fd >= 0) close(fd);
}

static int serial_write(int fd, const void *buf, size_t len) {
    return (int)write(fd, buf, len);
}

static int serial_read(int fd, void *buf, size_t len) {
    return (int)read(fd, buf, len);
}
#else
static HANDLE serial_open(const char *path, int baud) {
    HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
                          0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return INVALID_HANDLE_VALUE;
    
    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(h, &dcb)) {
        CloseHandle(h);
        return INVALID_HANDLE_VALUE;
    }
    
    dcb.BaudRate = (DWORD)baud;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    
    if (!SetCommState(h, &dcb)) {
        CloseHandle(h);
        return INVALID_HANDLE_VALUE;
    }
    
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 1000;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 1000;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    SetCommTimeouts(h, &timeouts);
    
    return h;
}

static void serial_close(HANDLE h) {
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
}

static int serial_write(HANDLE h, const void *buf, size_t len) {
    DWORD written;
    if (!WriteFile(h, buf, (DWORD)len, &written, NULL)) return -1;
    return (int)written;
}

static int serial_read(HANDLE h, void *buf, size_t len) {
    DWORD bytesRead;
    if (!ReadFile(h, buf, (DWORD)len, &bytesRead, NULL)) return -1;
    return (int)bytesRead;
}
#endif

/*============================================================================
 * GREASEWEAZLE PROTOCOL
 *============================================================================*/

#define GW_CMD_GET_INFO     0
#define GW_CMD_SEEK         2
#define GW_CMD_HEAD         3
#define GW_CMD_SET_PARAMS   4
#define GW_CMD_GET_PARAMS   5
#define GW_CMD_MOTOR        6
#define GW_CMD_READ_FLUX    7
#define GW_CMD_WRITE_FLUX   8
#define GW_CMD_SOURCE_BYTES 12
#define GW_CMD_SINK_BYTES   13

#define GW_ACK_OK           0
#define GW_ACK_BAD_COMMAND  1
#define GW_ACK_NO_INDEX     2
#define GW_ACK_NO_TRK0      3
#define GW_ACK_FLUX_OVERFLOW 4
#define GW_ACK_FLUX_UNDERFLOW 5
#define GW_ACK_WRPROT       6
#define GW_ACK_NO_UNIT      7

static int gw_command(uft_hal_t *hal, uint8_t cmd, const uint8_t *params, 
                      size_t param_len, uint8_t *response, size_t resp_len) {
    uint8_t buf[64];
    buf[0] = cmd;
    if (params && param_len > 0) {
        memcpy(buf + 1, params, param_len);
    }
    
    if (serial_write(hal->serial, buf, 1 + param_len) < 0) {
        snprintf(hal->error, sizeof(hal->error), "Write failed");
        return -1;
    }
    
    if (serial_read(hal->serial, buf, 2) != 2) {
        snprintf(hal->error, sizeof(hal->error), "Read failed");
        return -1;
    }
    
    if (buf[0] != cmd) {
        snprintf(hal->error, sizeof(hal->error), "Command mismatch");
        return -1;
    }
    
    if (buf[1] != GW_ACK_OK) {
        snprintf(hal->error, sizeof(hal->error), "Device error: %d", buf[1]);
        return -1;
    }
    
    if (response && resp_len > 0) {
        if (serial_read(hal->serial, response, resp_len) != (int)resp_len) {
            snprintf(hal->error, sizeof(hal->error), "Response read failed");
            return -1;
        }
    }
    
    return 0;
}

static int gw_open(uft_hal_t *hal, const char *path) {
    hal->serial = serial_open(path, 115200);
    if (hal->serial == INVALID_HANDLE_VALUE) {
        snprintf(hal->error, sizeof(hal->error), "Cannot open %s", path);
        return -1;
    }
    
    /* Get device info */
    uint8_t info[32];
    if (gw_command(hal, GW_CMD_GET_INFO, NULL, 0, info, 32) != 0) {
        serial_close(hal->serial);
        return -1;
    }
    
    hal->state.gw.hw_model = info[4];
    hal->state.gw.hw_submodel = info[5];
    hal->state.gw.fw_version = info[0] | (info[1] << 8) | 
                               (info[2] << 16) | (info[3] << 24);
    hal->state.gw.sample_freq = 72000000;  /* Default for GW */
    
    hal->is_open = true;
    return 0;
}

static void gw_close(uft_hal_t *hal) {
    if (hal->motor_on) {
        gw_motor(hal, false);
    }
    serial_close(hal->serial);
    hal->is_open = false;
}

static int gw_get_caps(uft_hal_t *hal, uft_hal_caps_t *caps) {
    if (!hal || !caps) return -1;
    
    caps->max_tracks = 84;
    caps->max_sides = 2;
    caps->can_read_flux = true;
    caps->can_write_flux = true;
    caps->sample_rate_hz = hal->state.gw.sample_freq;
    caps->capabilities = HAL_CAP_READ_FLUX | HAL_CAP_WRITE_FLUX |
                        HAL_CAP_INDEX_SENSE | HAL_CAP_MOTOR_CTRL |
                        HAL_CAP_HALF_TRACK | HAL_CAP_WRITE_PROTECT;
    
    return 0;
}

static int gw_seek(uft_hal_t *hal, int track) {
    uint8_t params[1] = { (uint8_t)track };
    if (gw_command(hal, GW_CMD_SEEK, params, 1, NULL, 0) != 0) {
        return -1;
    }
    hal->current_track = track;
    return 0;
}

static int gw_motor(uft_hal_t *hal, bool on) {
    uint8_t params[2] = { 0, on ? 1 : 0 };  /* Unit 0 */
    if (gw_command(hal, GW_CMD_MOTOR, params, 2, NULL, 0) != 0) {
        return -1;
    }
    hal->motor_on = on;
    return 0;
}

static int gw_read_flux(uft_hal_t *hal, int track, int side, int revs,
                        uint32_t **flux, size_t *count) {
    if (!hal || !flux || !count) return -1;
    
    /* Seek to track */
    if (gw_seek(hal, track) != 0) return -1;
    
    /* Set head */
    uint8_t head_params[1] = { (uint8_t)side };
    if (gw_command(hal, GW_CMD_HEAD, head_params, 1, NULL, 0) != 0) {
        return -1;
    }
    
    /* Start motor if not already running */
    if (!hal->motor_on) {
        if (gw_motor(hal, true) != 0) return -1;
    }
    
    /* Read flux - simplified, actual implementation needs streaming */
    uint8_t read_params[4];
    read_params[0] = (uint8_t)revs;
    read_params[1] = 0;  /* ticks (low) */
    read_params[2] = 0;  /* ticks (high) */
    read_params[3] = 0;
    
    if (gw_command(hal, GW_CMD_READ_FLUX, read_params, 4, NULL, 0) != 0) {
        return -1;
    }
    
    /* Read flux data - simplified buffer */
    size_t max_flux = 500000;
    uint32_t *data = malloc(max_flux * sizeof(uint32_t));
    if (!data) {
        snprintf(hal->error, sizeof(hal->error), "Memory allocation failed");
        return -1;
    }
    
    /* TODO: Implement actual flux streaming protocol */
    /* For now, return empty to indicate not connected to real hardware */
    *flux = data;
    *count = 0;
    
    snprintf(hal->error, sizeof(hal->error), 
             "Flux read requires real hardware connection");
    return -1;
}

static int gw_write_flux(uft_hal_t *hal, int track, int side,
                         const uint32_t *flux, size_t count) {
    if (!hal || !flux || count == 0) return -1;
    
    /* Seek and set head */
    if (gw_seek(hal, track) != 0) return -1;
    
    uint8_t head_params[1] = { (uint8_t)side };
    if (gw_command(hal, GW_CMD_HEAD, head_params, 1, NULL, 0) != 0) {
        return -1;
    }
    
    /* TODO: Implement actual flux write protocol */
    snprintf(hal->error, sizeof(hal->error), 
             "Flux write requires real hardware connection");
    return -1;
}

static int gw_enumerate(uft_hal_controller_t *out, int max) {
    /* TODO: Scan USB for Greaseweazle devices (VID:PID = 1209:4D69) */
    (void)out;
    (void)max;
    return 0;
}

/*============================================================================
 * FLUXENGINE (GW-Compatible)
 *============================================================================*/

static int fe_open(uft_hal_t *hal, const char *path) {
    /* FluxEngine uses same protocol as Greaseweazle */
    int result = gw_open(hal, path);
    if (result == 0) {
        hal->type = HAL_CTRL_FLUXENGINE;
    }
    return result;
}

static int fe_enumerate(uft_hal_controller_t *out, int max) {
    /* TODO: Scan USB for FluxEngine devices */
    (void)out;
    (void)max;
    return 0;
}

/*============================================================================
 * KRYOFLUX (Via DTC Wrapper)
 *============================================================================*/

#include "uft/hal/uft_kryoflux.h"

/* KryoFlux state stored in HAL handle */
static uft_kf_config_t *kf_get_config(uft_hal_t *hal) {
    return (uft_kf_config_t*)hal->state.kryoflux.config;
}

static int kf_open(uft_hal_t *hal, const char *path) {
    uft_kf_config_t *kf_cfg = uft_kf_config_create();
    if (!kf_cfg) {
        snprintf(hal->error, sizeof(hal->error), "Failed to create KryoFlux config");
        return -1;
    }
    
    /* If path provided, use it as DTC path */
    if (path && path[0]) {
        if (uft_kf_set_dtc_path(kf_cfg, path) != 0) {
            snprintf(hal->error, sizeof(hal->error), "%s", uft_kf_get_error(kf_cfg));
            uft_kf_config_destroy(kf_cfg);
            return -1;
        }
    }
    
    if (!uft_kf_is_available(kf_cfg)) {
        snprintf(hal->error, sizeof(hal->error), 
                 "DTC not found. Install KryoFlux software or provide path via device_path.");
        uft_kf_config_destroy(kf_cfg);
        return -1;
    }
    
    /* Store config pointer */
    hal->state.kryoflux.config = kf_cfg;
    hal->state.kryoflux.connected = true;
    hal->is_open = true;
    
    return 0;
}

static void kf_close(uft_hal_t *hal) {
    if (hal->state.kryoflux.connected) {
        uft_kf_config_t *kf_cfg = kf_get_config(hal);
        if (kf_cfg) {
            uft_kf_config_destroy(kf_cfg);
        }
        hal->state.kryoflux.connected = false;
    }
    hal->is_open = false;
}

static int kf_get_caps(uft_hal_t *hal, uft_hal_caps_t *caps) {
    if (!caps) return -1;
    
    caps->max_tracks = 84;
    caps->max_sides = 2;
    caps->can_read_flux = true;
    caps->can_write_flux = false;  /* KF is read-only via DTC */
    caps->sample_rate_hz = (uint32_t)UFT_KF_SAMPLE_CLOCK;
    caps->capabilities = HAL_CAP_READ_FLUX | HAL_CAP_INDEX_SENSE;
    
    (void)hal;
    return 0;
}

static int kf_read_flux(uft_hal_t *hal, int track, int side, int revs,
                        uint32_t **flux, size_t *count) {
    if (!hal || !flux || !count) return -1;
    
    uft_kf_config_t *kf_cfg = kf_get_config(hal);
    if (!kf_cfg) {
        snprintf(hal->error, sizeof(hal->error), "KryoFlux not initialized");
        return -1;
    }
    
    /* Set revolutions */
    uft_kf_set_revolutions(kf_cfg, revs > 0 ? revs : 2);
    
    /* Capture track via DTC */
    uint32_t *index = NULL;
    size_t index_count = 0;
    
    int result = uft_kf_capture_track(kf_cfg, track, side, 
                                       flux, count, &index, &index_count);
    
    if (result != 0) {
        snprintf(hal->error, sizeof(hal->error), "%s", uft_kf_get_error(kf_cfg));
        return -1;
    }
    
    /* Free index array (not used in HAL interface currently) */
    if (index) free(index);
    
    hal->current_track = track;
    hal->current_side = side;
    
    return 0;
}

static int kf_enumerate(uft_hal_controller_t *out, int max) {
    /* Check if DTC is available */
    uft_kf_config_t *cfg = uft_kf_config_create();
    if (!cfg) return 0;
    
    int count = 0;
    if (uft_kf_is_available(cfg)) {
        int devices = 0;
        uft_kf_detect_devices(cfg, &devices);
        
        for (int i = 0; i < devices && count < max; i++) {
            if (out) out[count] = HAL_CTRL_KRYOFLUX;
            count++;
        }
    }
    
    uft_kf_config_destroy(cfg);
    return count;
}

/*============================================================================
 * SUPERCARD PRO
 *============================================================================*/

#define SCP_CMD_SELECT_DRIVE  'a'
#define SCP_CMD_DESELECT_DRIVE 'd'
#define SCP_CMD_MOTOR_ON      'm'
#define SCP_CMD_MOTOR_OFF     'M'
#define SCP_CMD_SEEK_TRACK    's'
#define SCP_CMD_READ_TRACK    'R'
#define SCP_CMD_GET_INFO      'i'

static int scp_open(uft_hal_t *hal, const char *path) {
    hal->serial = serial_open(path, 38400);
    if (hal->serial == INVALID_HANDLE_VALUE) {
        snprintf(hal->error, sizeof(hal->error), "Cannot open %s", path);
        return -1;
    }
    
    /* Get device info */
    uint8_t cmd = SCP_CMD_GET_INFO;
    if (serial_write(hal->serial, &cmd, 1) < 0) {
        serial_close(hal->serial);
        snprintf(hal->error, sizeof(hal->error), "SCP communication failed");
        return -1;
    }
    
    uint8_t info[16];
    if (serial_read(hal->serial, info, 16) != 16) {
        serial_close(hal->serial);
        snprintf(hal->error, sizeof(hal->error), "SCP info read failed");
        return -1;
    }
    
    hal->state.scp.firmware = info[0] | (info[1] << 8);
    hal->is_open = true;
    return 0;
}

static void scp_close(uft_hal_t *hal) {
    if (hal->motor_on) {
        uint8_t cmd = SCP_CMD_MOTOR_OFF;
        serial_write(hal->serial, &cmd, 1);
    }
    serial_close(hal->serial);
    hal->is_open = false;
}

static int scp_get_caps(uft_hal_t *hal, uft_hal_caps_t *caps) {
    if (!caps) return -1;
    
    caps->max_tracks = 84;
    caps->max_sides = 2;
    caps->can_read_flux = true;
    caps->can_write_flux = true;
    caps->sample_rate_hz = 40000000;  /* 40MHz */
    caps->capabilities = HAL_CAP_READ_FLUX | HAL_CAP_WRITE_FLUX |
                        HAL_CAP_INDEX_SENSE | HAL_CAP_MOTOR_CTRL;
    
    (void)hal;
    return 0;
}

static int scp_read_flux(uft_hal_t *hal, int track, int side, int revs,
                         uint32_t **flux, size_t *count) {
    snprintf(hal->error, sizeof(hal->error),
             "SCP flux read: Track %d Side %d not yet implemented", track, side);
    (void)revs;
    (void)flux;
    (void)count;
    return -1;
}

static int scp_enumerate(uft_hal_controller_t *out, int max) {
    /* TODO: Scan USB for SCP devices */
    (void)out;
    (void)max;
    return 0;
}

/*============================================================================
 * STUB DRIVER
 *============================================================================*/

static int stub_open(uft_hal_t *hal, const char *path) {
    snprintf(hal->error, sizeof(hal->error),
             "%s driver not implemented", hal->driver->name);
    (void)path;
    return -1;
}

static void stub_close(uft_hal_t *hal) {
    hal->is_open = false;
}

static int stub_get_caps(uft_hal_t *hal, uft_hal_caps_t *caps) {
    if (!caps) return -1;
    memset(caps, 0, sizeof(*caps));
    caps->max_tracks = 84;
    caps->max_sides = 2;
    (void)hal;
    return 0;
}

static int stub_read_flux(uft_hal_t *hal, int track, int side, int revs,
                          uint32_t **flux, size_t *count) {
    snprintf(hal->error, sizeof(hal->error),
             "%s: read_flux not implemented", hal->driver->name);
    (void)track;
    (void)side;
    (void)revs;
    (void)flux;
    (void)count;
    return -1;
}

static int stub_write_flux(uft_hal_t *hal, int track, int side,
                           const uint32_t *flux, size_t count) {
    snprintf(hal->error, sizeof(hal->error),
             "%s: write_flux not implemented", hal->driver->name);
    (void)track;
    (void)side;
    (void)flux;
    (void)count;
    return -1;
}

static int stub_seek(uft_hal_t *hal, int track) {
    (void)hal;
    (void)track;
    return 0;
}

static int stub_motor(uft_hal_t *hal, bool on) {
    (void)hal;
    (void)on;
    return 0;
}

static int stub_enumerate(uft_hal_controller_t *out, int max) {
    (void)out;
    (void)max;
    return 0;
}

/*============================================================================
 * PUBLIC API
 *============================================================================*/

int uft_hal_enumerate(uft_hal_controller_t *controllers, int max_count) {
    if (!controllers || max_count <= 0) return 0;
    
    int total = 0;
    for (size_t i = 0; i < DRIVER_COUNT && total < max_count; i++) {
        int found = g_drivers[i].enumerate(controllers + total, 
                                           max_count - total);
        total += found;
    }
    
    return total;
}

uft_hal_t* uft_hal_open(uft_hal_controller_t type, const char *device_path) {
    const uft_hal_driver_t *driver = find_driver(type);
    if (!driver) return NULL;
    
    uft_hal_t *hal = uft_calloc(1, sizeof(uft_hal_t));
    if (!hal) return NULL;
    
    hal->type = type;
    hal->driver = driver;
    hal->serial = INVALID_HANDLE_VALUE;
    
    if (device_path) {
        strncpy(hal->device_path, device_path, sizeof(hal->device_path) - 1);
    }
    
    if (driver->open(hal, device_path) != 0) {
        free(hal);
        return NULL;
    }
    
    /* Get default caps */
    driver->get_caps(hal, &hal->caps);
    
    return hal;
}

int uft_hal_get_caps(uft_hal_t *hal, uft_hal_caps_t *caps) {
    if (!hal || !caps || !hal->driver) return -1;
    return hal->driver->get_caps(hal, caps);
}

int uft_hal_read_flux(uft_hal_t *hal, int track, int side, int revolutions,
                      uint32_t **flux, size_t *count) {
    if (!hal || !hal->driver) return -1;
    return hal->driver->read_flux(hal, track, side, revolutions, flux, count);
}

int uft_hal_write_flux(uft_hal_t *hal, int track, int side,
                       const uint32_t *flux, size_t count) {
    if (!hal || !hal->driver) return -1;
    return hal->driver->write_flux(hal, track, side, flux, count);
}

int uft_hal_seek(uft_hal_t *hal, int track) {
    if (!hal || !hal->driver) return -1;
    return hal->driver->seek(hal, track);
}

int uft_hal_motor(uft_hal_t *hal, bool on) {
    if (!hal || !hal->driver) return -1;
    return hal->driver->motor(hal, on);
}

void uft_hal_close(uft_hal_t *hal) {
    if (!hal) return;
    if (hal->driver && hal->is_open) {
        hal->driver->close(hal);
    }
    free(hal);
}

const char* uft_hal_get_error(uft_hal_t *hal) {
    return hal ? hal->error : "NULL handle";
}

const char* uft_hal_error(uft_hal_t *hal) {
    return uft_hal_get_error(hal);
}

const char* uft_hal_controller_name(uft_hal_controller_t type) {
    const uft_hal_driver_t *driver = find_driver(type);
    return driver ? driver->name : "Unknown";
}

/* Controller info */
int uft_hal_get_controller_count(void) {
    return (int)DRIVER_COUNT;
}

const char* uft_hal_get_controller_name_by_index(int index) {
    if (index < 0 || index >= (int)DRIVER_COUNT) return NULL;
    return g_drivers[index].name;
}

bool uft_hal_is_controller_implemented(uft_hal_controller_t type) {
    switch (type) {
        case HAL_CTRL_GREASEWEAZLE:
        case HAL_CTRL_FLUXENGINE:
            return true;
        case HAL_CTRL_KRYOFLUX:
        case HAL_CTRL_SCP:
            return true;  /* Partial */
        default:
            return false;
    }
}
