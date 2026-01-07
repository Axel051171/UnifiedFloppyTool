/**
 * @file uft_hardware_mock.c
 * @brief Hardware Mock Framework Implementation
 * 
 * TICKET-009: Hardware Mock Framework
 */

#include "uft/uft_hardware_mock.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/time.h>
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define MAX_CYLINDERS   256
#define MAX_HEADS       2
#define MAX_TRACK_SIZE  32768
#define MAX_FLUX_SIZE   500000
#define MAX_ERRORS      64
#define LOG_SIZE        65536

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal Structures
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint8_t         *data;
    size_t          size;
    uint32_t        *flux;
    size_t          flux_count;
    bool            has_weak_bits;
    size_t          weak_bit_offset;
    size_t          weak_bit_count;
} mock_track_t;

struct uft_mock_device {
    uft_mock_config_t   config;
    uft_mock_stats_t    stats;
    
    /* Track data */
    mock_track_t        tracks[MAX_CYLINDERS][MAX_HEADS];
    
    /* Error injection */
    uft_mock_error_config_t errors[MAX_ERRORS];
    int                 error_count;
    float               global_error_rate;
    
    /* State */
    int                 current_cylinder;
    int                 current_head;
    bool                motor_on;
    
    /* Logging */
    char                *log;
    size_t              log_pos;
    size_t              log_size;
    
    /* Random state */
    unsigned int        rand_seed;
};

/* Active mock device for HAL integration */
static uft_mock_device_t *g_active_mock = NULL;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uint64_t get_timestamp_ms(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

static void delay_ms(uint32_t ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

static int mock_random(uft_mock_device_t *dev) {
    dev->rand_seed = dev->rand_seed * 1103515245 + 12345;
    return (dev->rand_seed >> 16) & 0x7FFF;
}

static void log_operation(uft_mock_device_t *dev, const char *fmt, ...) {
    if (!dev->config.log_operations || !dev->log) return;
    
    va_list args;
    va_start(args, fmt);
    
    int remaining = dev->log_size - dev->log_pos - 1;
    if (remaining > 0) {
        int written = vsnprintf(dev->log + dev->log_pos, remaining, fmt, args);
        if (written > 0) {
            dev->log_pos += written;
            if (dev->log_pos < dev->log_size - 1) {
                dev->log[dev->log_pos++] = '\n';
            }
        }
    }
    
    va_end(args);
}

static uft_mock_error_t check_errors(uft_mock_device_t *dev, int cyl, int head, int sector) {
    /* Global error rate */
    if (dev->global_error_rate > 0.0f) {
        float r = (float)(mock_random(dev) % 10000) / 10000.0f;
        if (r < dev->global_error_rate) {
            dev->stats.errors_injected++;
            return UFT_MOCK_ERR_CRC;
        }
    }
    
    /* Specific error rules */
    for (int i = 0; i < dev->error_count; i++) {
        uft_mock_error_config_t *e = &dev->errors[i];
        
        if ((e->cylinder == -1 || e->cylinder == cyl) &&
            (e->head == -1 || e->head == head) &&
            (e->sector == -1 || e->sector == sector)) {
            
            if (e->probability >= 100 || 
                (mock_random(dev) % 100) < e->probability) {
                dev->stats.errors_injected++;
                return e->error;
            }
        }
    }
    
    return UFT_MOCK_ERR_NONE;
}

static size_t get_track_size(const uft_mock_config_t *cfg) {
    return cfg->sectors * cfg->sector_size;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Device Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_mock_device_t *uft_mock_create(const uft_mock_config_t *config) {
    uft_mock_device_t *dev = calloc(1, sizeof(uft_mock_device_t));
    if (!dev) return NULL;
    
    if (config) {
        dev->config = *config;
    } else {
        uft_mock_config_t defaults = UFT_MOCK_CONFIG_DEFAULT;
        dev->config = defaults;
    }
    
    /* Initialize logging */
    dev->log_size = LOG_SIZE;
    dev->log = malloc(dev->log_size);
    if (dev->log) {
        dev->log[0] = '\0';
    }
    
    /* Initialize random */
    dev->rand_seed = (unsigned int)time(NULL);
    
    /* Allocate track storage */
    size_t track_size = get_track_size(&dev->config);
    for (int c = 0; c < dev->config.cylinders && c < MAX_CYLINDERS; c++) {
        for (int h = 0; h < dev->config.heads && h < MAX_HEADS; h++) {
            dev->tracks[c][h].data = calloc(1, track_size);
            dev->tracks[c][h].size = track_size;
        }
    }
    
    log_operation(dev, "Mock device created: %s drive, %dx%d geometry",
                  uft_mock_drive_name(dev->config.drive),
                  dev->config.cylinders, dev->config.heads);
    
    return dev;
}

uft_mock_device_t *uft_mock_create_preset(uft_mock_type_t type, uft_mock_drive_t drive) {
    uft_mock_config_t config = UFT_MOCK_CONFIG_DEFAULT;
    config.type = type;
    config.drive = drive;
    
    /* Set geometry based on drive type */
    switch (drive) {
        case UFT_MOCK_DRIVE_35DD:
            config.cylinders = 80;
            config.heads = 2;
            config.sectors = 9;
            config.sector_size = 512;
            break;
        case UFT_MOCK_DRIVE_35HD:
            config.cylinders = 80;
            config.heads = 2;
            config.sectors = 18;
            config.sector_size = 512;
            break;
        case UFT_MOCK_DRIVE_525DD:
            config.cylinders = 40;
            config.heads = 2;
            config.sectors = 9;
            config.sector_size = 512;
            break;
        case UFT_MOCK_DRIVE_525HD:
            config.cylinders = 80;
            config.heads = 2;
            config.sectors = 15;
            config.sector_size = 512;
            break;
        case UFT_MOCK_DRIVE_1541:
            config.cylinders = 35;
            config.heads = 1;
            config.sectors = 21;  /* Variable, max */
            config.sector_size = 256;
            break;
        case UFT_MOCK_DRIVE_1571:
            config.cylinders = 35;
            config.heads = 2;
            config.sectors = 21;
            config.sector_size = 256;
            break;
        default:
            break;
    }
    
    return uft_mock_create(&config);
}

void uft_mock_destroy(uft_mock_device_t *dev) {
    if (!dev) return;
    
    /* Unregister from HAL */
    if (g_active_mock == dev) {
        g_active_mock = NULL;
    }
    
    /* Free track data */
    for (int c = 0; c < MAX_CYLINDERS; c++) {
        for (int h = 0; h < MAX_HEADS; h++) {
            free(dev->tracks[c][h].data);
            free(dev->tracks[c][h].flux);
        }
    }
    
    free(dev->log);
    free(dev);
}

void uft_mock_reset(uft_mock_device_t *dev) {
    if (!dev) return;
    
    dev->current_cylinder = 0;
    dev->current_head = 0;
    dev->motor_on = false;
    
    memset(&dev->stats, 0, sizeof(dev->stats));
    
    if (dev->log) {
        dev->log[0] = '\0';
        dev->log_pos = 0;
    }
    
    log_operation(dev, "Device reset");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Data Loading
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_mock_load_image(uft_mock_device_t *dev, const char *path) {
    if (!dev || !path) return UFT_ERR_INVALID_PARAM;
    
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERR_IO;
    
    size_t track_size = get_track_size(&dev->config);
    
    for (int c = 0; c < dev->config.cylinders; c++) {
        for (int h = 0; h < dev->config.heads; h++) {
            if (dev->tracks[c][h].data) {
                size_t read = fread(dev->tracks[c][h].data, 1, track_size, f);
                dev->tracks[c][h].size = read;
            }
        }
    }
    
    fclose(f);
    
    log_operation(dev, "Loaded image: %s", path);
    return UFT_OK;
}

uft_error_t uft_mock_load_flux(uft_mock_device_t *dev, const char *path) {
    if (!dev || !path) return UFT_ERR_INVALID_PARAM;
    
    /* Simplified - just set flux source */
    dev->config.flux_source = UFT_MOCK_FLUX_FROM_FILE;
    dev->config.flux_file = path;
    
    log_operation(dev, "Set flux source: %s", path);
    return UFT_OK;
}

uft_error_t uft_mock_generate_pattern(uft_mock_device_t *dev, int pattern) {
    if (!dev) return UFT_ERR_INVALID_PARAM;
    
    size_t track_size = get_track_size(&dev->config);
    
    for (int c = 0; c < dev->config.cylinders; c++) {
        for (int h = 0; h < dev->config.heads; h++) {
            if (!dev->tracks[c][h].data) continue;
            
            uint8_t *data = dev->tracks[c][h].data;
            
            switch (pattern) {
                case 0:  /* Zeros */
                    memset(data, 0x00, track_size);
                    break;
                case 1:  /* Ones */
                    memset(data, 0xFF, track_size);
                    break;
                case 2:  /* Random */
                    for (size_t i = 0; i < track_size; i++) {
                        data[i] = mock_random(dev) & 0xFF;
                    }
                    break;
                case 3:  /* Sequential */
                    for (size_t i = 0; i < track_size; i++) {
                        data[i] = (i + c + h) & 0xFF;
                    }
                    break;
                default:  /* Unknown pattern - use zeros */
                    memset(data, 0x00, track_size);
                    break;
            }
            
            dev->tracks[c][h].size = track_size;
        }
    }
    
    log_operation(dev, "Generated pattern %d", pattern);
    return UFT_OK;
}

uft_error_t uft_mock_set_track(uft_mock_device_t *dev, int cylinder, int head,
                                const uint8_t *data, size_t size) {
    if (!dev || !data) return UFT_ERR_INVALID_PARAM;
    if (cylinder >= MAX_CYLINDERS || head >= MAX_HEADS) return UFT_ERR_INVALID_PARAM;
    
    mock_track_t *track = &dev->tracks[cylinder][head];
    
    if (!track->data) {
        track->data = malloc(MAX_TRACK_SIZE);
    }
    
    size_t copy_size = (size < MAX_TRACK_SIZE) ? size : MAX_TRACK_SIZE;
    memcpy(track->data, data, copy_size);
    track->size = copy_size;
    
    log_operation(dev, "Set track C%d/H%d (%zu bytes)", cylinder, head, copy_size);
    return UFT_OK;
}

uft_error_t uft_mock_set_flux(uft_mock_device_t *dev, int cylinder, int head,
                               const uint32_t *flux, size_t count) {
    if (!dev || !flux) return UFT_ERR_INVALID_PARAM;
    if (cylinder >= MAX_CYLINDERS || head >= MAX_HEADS) return UFT_ERR_INVALID_PARAM;
    
    mock_track_t *track = &dev->tracks[cylinder][head];
    
    free(track->flux);
    track->flux = malloc(count * sizeof(uint32_t));
    if (!track->flux) return UFT_ERR_MEMORY;
    
    memcpy(track->flux, flux, count * sizeof(uint32_t));
    track->flux_count = count;
    
    log_operation(dev, "Set flux C%d/H%d (%zu transitions)", cylinder, head, count);
    return UFT_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Read/Write Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_mock_read_track(uft_mock_device_t *dev, int cylinder, int head,
                                 uint8_t *buffer, size_t size, size_t *bytes_read) {
    if (!dev || !buffer) return UFT_ERR_INVALID_PARAM;
    if (cylinder >= MAX_CYLINDERS || head >= MAX_HEADS) return UFT_ERR_INVALID_PARAM;
    
    uint64_t start = get_timestamp_ms();
    
    /* Check disk presence */
    if (!dev->config.disk_present) {
        log_operation(dev, "READ C%d/H%d - NO DISK", cylinder, head);
        return UFT_ERR_HARDWARE;
    }
    
    /* Simulate seek if needed */
    if (dev->current_cylinder != cylinder) {
        int steps = abs(cylinder - dev->current_cylinder);
        if (dev->config.simulate_timing) {
            delay_ms(steps * dev->config.timing.step_time_ms);
            delay_ms(dev->config.timing.settle_time_ms);
        }
        dev->current_cylinder = cylinder;
        dev->stats.seeks++;
        dev->stats.time_seeking_ms += steps * dev->config.timing.step_time_ms;
    }
    
    /* Check for errors */
    uft_mock_error_t err = check_errors(dev, cylinder, head, -1);
    if (err != UFT_MOCK_ERR_NONE) {
        log_operation(dev, "READ C%d/H%d - ERROR %s", cylinder, head, uft_mock_error_name(err));
        switch (err) {
            case UFT_MOCK_ERR_CRC:     return UFT_ERR_CRC;
            case UFT_MOCK_ERR_TIMEOUT: return UFT_ERR_TIMEOUT;
            case UFT_MOCK_ERR_NO_DISK: return UFT_ERR_HARDWARE;
            default:                   return UFT_ERR_IO;
        }
    }
    
    /* Read track data */
    mock_track_t *track = &dev->tracks[cylinder][head];
    size_t copy_size = (track->size < size) ? track->size : size;
    
    if (track->data && copy_size > 0) {
        memcpy(buffer, track->data, copy_size);
    }
    
    if (bytes_read) *bytes_read = copy_size;
    
    dev->stats.reads++;
    dev->stats.bytes_read += copy_size;
    dev->stats.time_reading_ms += get_timestamp_ms() - start;
    
    /* Callback */
    if (dev->config.on_read) {
        dev->config.on_read(cylinder, head, dev->config.callback_user);
    }
    
    log_operation(dev, "READ C%d/H%d - %zu bytes", cylinder, head, copy_size);
    return UFT_OK;
}

uft_error_t uft_mock_write_track(uft_mock_device_t *dev, int cylinder, int head,
                                  const uint8_t *data, size_t size) {
    if (!dev || !data) return UFT_ERR_INVALID_PARAM;
    if (cylinder >= MAX_CYLINDERS || head >= MAX_HEADS) return UFT_ERR_INVALID_PARAM;
    
    uint64_t start = get_timestamp_ms();
    
    /* Check write protect */
    if (dev->config.write_protect) {
        log_operation(dev, "WRITE C%d/H%d - WRITE PROTECTED", cylinder, head);
        return UFT_ERR_STATE;
    }
    
    /* Check disk presence */
    if (!dev->config.disk_present) {
        log_operation(dev, "WRITE C%d/H%d - NO DISK", cylinder, head);
        return UFT_ERR_HARDWARE;
    }
    
    /* Simulate seek if needed */
    if (dev->current_cylinder != cylinder) {
        int steps = abs(cylinder - dev->current_cylinder);
        if (dev->config.simulate_timing) {
            delay_ms(steps * dev->config.timing.step_time_ms);
            delay_ms(dev->config.timing.settle_time_ms);
        }
        dev->current_cylinder = cylinder;
        dev->stats.seeks++;
    }
    
    /* Check for errors */
    uft_mock_error_t err = check_errors(dev, cylinder, head, -1);
    if (err != UFT_MOCK_ERR_NONE) {
        log_operation(dev, "WRITE C%d/H%d - ERROR %s", cylinder, head, uft_mock_error_name(err));
        return UFT_ERR_IO;
    }
    
    /* Write track data */
    mock_track_t *track = &dev->tracks[cylinder][head];
    
    if (!track->data) {
        track->data = malloc(MAX_TRACK_SIZE);
    }
    
    size_t copy_size = (size < MAX_TRACK_SIZE) ? size : MAX_TRACK_SIZE;
    memcpy(track->data, data, copy_size);
    track->size = copy_size;
    
    dev->stats.writes++;
    dev->stats.bytes_written += copy_size;
    dev->stats.time_writing_ms += get_timestamp_ms() - start;
    
    /* Callback */
    if (dev->config.on_write) {
        dev->config.on_write(cylinder, head, dev->config.callback_user);
    }
    
    log_operation(dev, "WRITE C%d/H%d - %zu bytes", cylinder, head, copy_size);
    return UFT_OK;
}

uft_error_t uft_mock_read_sector(uft_mock_device_t *dev, int cylinder, int head,
                                  int sector, uint8_t *buffer, size_t size) {
    if (!dev || !buffer) return UFT_ERR_INVALID_PARAM;
    if (sector < 0 || sector >= dev->config.sectors) return UFT_ERR_INVALID_PARAM;
    
    /* Check for sector-specific errors */
    uft_mock_error_t err = check_errors(dev, cylinder, head, sector);
    if (err != UFT_MOCK_ERR_NONE) {
        log_operation(dev, "READ C%d/H%d/S%d - ERROR %s", 
                      cylinder, head, sector, uft_mock_error_name(err));
        return UFT_ERR_CRC;
    }
    
    mock_track_t *track = &dev->tracks[cylinder][head];
    if (!track->data) return UFT_ERR_NO_DATA;
    
    size_t offset = sector * dev->config.sector_size;
    size_t copy_size = dev->config.sector_size;
    if (copy_size > size) copy_size = size;
    
    if (offset + copy_size <= track->size) {
        memcpy(buffer, track->data + offset, copy_size);
    }
    
    log_operation(dev, "READ C%d/H%d/S%d - %zu bytes", cylinder, head, sector, copy_size);
    return UFT_OK;
}

uft_error_t uft_mock_write_sector(uft_mock_device_t *dev, int cylinder, int head,
                                   int sector, const uint8_t *data, size_t size) {
    if (!dev || !data) return UFT_ERR_INVALID_PARAM;
    if (sector < 0 || sector >= dev->config.sectors) return UFT_ERR_INVALID_PARAM;
    
    if (dev->config.write_protect) {
        return UFT_ERR_STATE;
    }
    
    mock_track_t *track = &dev->tracks[cylinder][head];
    if (!track->data) {
        track->data = calloc(1, get_track_size(&dev->config));
        track->size = get_track_size(&dev->config);
    }
    
    size_t offset = sector * dev->config.sector_size;
    size_t copy_size = (size < (size_t)dev->config.sector_size) ? size : dev->config.sector_size;
    
    memcpy(track->data + offset, data, copy_size);
    
    log_operation(dev, "WRITE C%d/H%d/S%d - %zu bytes", cylinder, head, sector, copy_size);
    return UFT_OK;
}

uft_error_t uft_mock_read_flux(uft_mock_device_t *dev, int cylinder, int head,
                                uint32_t *flux, size_t max_transitions, size_t *count) {
    if (!dev || !flux || !count) return UFT_ERR_INVALID_PARAM;
    
    mock_track_t *track = &dev->tracks[cylinder][head];
    
    if (track->flux && track->flux_count > 0) {
        /* Return stored flux */
        *count = (track->flux_count < max_transitions) ? track->flux_count : max_transitions;
        memcpy(flux, track->flux, *count * sizeof(uint32_t));
    } else {
        /* Generate flux from data */
        /* Simplified: 4us bit cells for MFM */
        *count = 0;
        uint32_t time = 0;
        uint32_t bit_cell = 4000;  /* 4us in ns */
        
        for (size_t i = 0; i < track->size && *count < max_transitions - 1; i++) {
            for (int b = 7; b >= 0 && *count < max_transitions - 1; b--) {
                time += bit_cell;
                if ((track->data[i] >> b) & 1) {
                    flux[(*count)++] = time;
                    time = 0;
                }
            }
        }
    }
    
    log_operation(dev, "READ FLUX C%d/H%d - %zu transitions", cylinder, head, *count);
    return UFT_OK;
}

uft_error_t uft_mock_write_flux(uft_mock_device_t *dev, int cylinder, int head,
                                 const uint32_t *flux, size_t count) {
    return uft_mock_set_flux(dev, cylinder, head, flux, count);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Control Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_mock_seek(uft_mock_device_t *dev, int cylinder) {
    if (!dev) return UFT_ERR_INVALID_PARAM;
    if (cylinder < 0 || cylinder >= dev->config.cylinders) return UFT_ERR_INVALID_PARAM;
    
    int steps = abs(cylinder - dev->current_cylinder);
    
    if (dev->config.simulate_timing && steps > 0) {
        delay_ms(steps * dev->config.timing.step_time_ms);
        delay_ms(dev->config.timing.settle_time_ms);
    }
    
    dev->current_cylinder = cylinder;
    dev->stats.current_cylinder = cylinder;
    dev->stats.seeks++;
    
    if (dev->config.on_seek) {
        dev->config.on_seek(cylinder, dev->config.callback_user);
    }
    
    log_operation(dev, "SEEK to C%d (%d steps)", cylinder, steps);
    return UFT_OK;
}

uft_error_t uft_mock_select_head(uft_mock_device_t *dev, int head) {
    if (!dev) return UFT_ERR_INVALID_PARAM;
    if (head < 0 || head >= dev->config.heads) return UFT_ERR_INVALID_PARAM;
    
    dev->current_head = head;
    dev->stats.current_head = head;
    
    log_operation(dev, "SELECT HEAD %d", head);
    return UFT_OK;
}

uft_error_t uft_mock_motor(uft_mock_device_t *dev, bool on) {
    if (!dev) return UFT_ERR_INVALID_PARAM;
    
    if (on && !dev->motor_on && dev->config.simulate_timing) {
        delay_ms(dev->config.timing.motor_spinup_ms);
    }
    
    dev->motor_on = on;
    
    log_operation(dev, "MOTOR %s", on ? "ON" : "OFF");
    return UFT_OK;
}

int uft_mock_get_index(uft_mock_device_t *dev) {
    if (!dev) return 0;
    return 0;  /* Index at start of track */
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Error Injection
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_mock_add_error(uft_mock_device_t *dev, const uft_mock_error_config_t *config) {
    if (!dev || !config) return UFT_ERR_INVALID_PARAM;
    if (dev->error_count >= MAX_ERRORS) return UFT_ERR_LIMIT;
    
    dev->errors[dev->error_count++] = *config;
    
    log_operation(dev, "Added error rule: C%d/H%d/S%d -> %s (%d%%)",
                  config->cylinder, config->head, config->sector,
                  uft_mock_error_name(config->error), config->probability);
    return UFT_OK;
}

void uft_mock_clear_errors(uft_mock_device_t *dev) {
    if (!dev) return;
    dev->error_count = 0;
    dev->global_error_rate = 0.0f;
    log_operation(dev, "Cleared all error rules");
}

void uft_mock_set_error_rate(uft_mock_device_t *dev, float rate) {
    if (!dev) return;
    dev->global_error_rate = (rate < 0.0f) ? 0.0f : (rate > 1.0f) ? 1.0f : rate;
    log_operation(dev, "Set global error rate: %.2f%%", rate * 100);
}

void uft_mock_inject_weak_bits(uft_mock_device_t *dev, int cylinder, int head,
                                size_t bit_offset, size_t count) {
    if (!dev || cylinder >= MAX_CYLINDERS || head >= MAX_HEADS) return;
    
    mock_track_t *track = &dev->tracks[cylinder][head];
    track->has_weak_bits = true;
    track->weak_bit_offset = bit_offset;
    track->weak_bit_count = count;
    
    log_operation(dev, "Injected %zu weak bits at C%d/H%d offset %zu",
                  count, cylinder, head, bit_offset);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * State Control
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_mock_set_write_protect(uft_mock_device_t *dev, bool protect) {
    if (!dev) return;
    dev->config.write_protect = protect;
    log_operation(dev, "Write protect: %s", protect ? "ON" : "OFF");
}

void uft_mock_set_disk_present(uft_mock_device_t *dev, bool present) {
    if (!dev) return;
    dev->config.disk_present = present;
    log_operation(dev, "Disk present: %s", present ? "YES" : "NO");
}

const uft_mock_config_t *uft_mock_get_config(const uft_mock_device_t *dev) {
    return dev ? &dev->config : NULL;
}

const uft_mock_stats_t *uft_mock_get_stats(const uft_mock_device_t *dev) {
    return dev ? &dev->stats : NULL;
}

void uft_mock_reset_stats(uft_mock_device_t *dev) {
    if (!dev) return;
    memset(&dev->stats, 0, sizeof(dev->stats));
    dev->stats.current_cylinder = dev->current_cylinder;
    dev->stats.current_head = dev->current_head;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Logging
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_mock_set_logging(uft_mock_device_t *dev, bool enable) {
    if (!dev) return;
    dev->config.log_operations = enable;
}

char *uft_mock_get_log(const uft_mock_device_t *dev) {
    if (!dev || !dev->log) return strdup("");
    return strdup(dev->log);
}

void uft_mock_clear_log(uft_mock_device_t *dev) {
    if (!dev || !dev->log) return;
    dev->log[0] = '\0';
    dev->log_pos = 0;
}

char *uft_mock_export_state(const uft_mock_device_t *dev) {
    if (!dev) return strdup("{}");
    
    size_t size = 4096;
    char *json = malloc(size);
    if (!json) return NULL;
    
    snprintf(json, size,
        "{\n"
        "  \"type\": \"%s\",\n"
        "  \"drive\": \"%s\",\n"
        "  \"cylinders\": %d,\n"
        "  \"heads\": %d,\n"
        "  \"sectors\": %d,\n"
        "  \"current_cylinder\": %d,\n"
        "  \"current_head\": %d,\n"
        "  \"motor_on\": %s,\n"
        "  \"write_protect\": %s,\n"
        "  \"disk_present\": %s,\n"
        "  \"stats\": {\n"
        "    \"reads\": %lu,\n"
        "    \"writes\": %lu,\n"
        "    \"seeks\": %lu,\n"
        "    \"errors\": %lu,\n"
        "    \"bytes_read\": %lu,\n"
        "    \"bytes_written\": %lu\n"
        "  }\n"
        "}\n",
        uft_mock_type_name(dev->config.type),
        uft_mock_drive_name(dev->config.drive),
        dev->config.cylinders,
        dev->config.heads,
        dev->config.sectors,
        dev->current_cylinder,
        dev->current_head,
        dev->motor_on ? "true" : "false",
        dev->config.write_protect ? "true" : "false",
        dev->config.disk_present ? "true" : "false",
        (unsigned long)dev->stats.reads,
        (unsigned long)dev->stats.writes,
        (unsigned long)dev->stats.seeks,
        (unsigned long)dev->stats.errors_injected,
        (unsigned long)dev->stats.bytes_read,
        (unsigned long)dev->stats.bytes_written);
    
    return json;
}

uft_error_t uft_mock_save_image(const uft_mock_device_t *dev, const char *path,
                                 uft_format_t format) {
    if (!dev || !path) return UFT_ERR_INVALID_PARAM;
    (void)format;  /* TODO: format-specific output */
    
    FILE *f = fopen(path, "wb");
    if (!f) return UFT_ERR_IO;
    
    for (int c = 0; c < dev->config.cylinders; c++) {
        for (int h = 0; h < dev->config.heads; h++) {
            const mock_track_t *track = &dev->tracks[c][h];
            if (track->data && track->size > 0) {
                if (fwrite(track->data, 1, track->size, f) != track->size) { /* I/O error */ }
            }
        }
    }
    
    fclose(f);
    return UFT_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * HAL Integration
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_mock_register_hal(uft_mock_device_t *dev) {
    if (!dev) return UFT_ERR_INVALID_PARAM;
    g_active_mock = dev;
    log_operation(dev, "Registered with HAL");
    return UFT_OK;
}

void uft_mock_unregister_hal(uft_mock_device_t *dev) {
    if (g_active_mock == dev) {
        g_active_mock = NULL;
        log_operation(dev, "Unregistered from HAL");
    }
}

bool uft_mock_is_active(void) {
    return g_active_mock != NULL;
}

uft_mock_device_t *uft_mock_get_active(void) {
    return g_active_mock;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Test Data Generation
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_mock_gen_amiga_dd(uft_mock_device_t *dev) {
    if (!dev) return UFT_ERR_INVALID_PARAM;
    
    dev->config.cylinders = 80;
    dev->config.heads = 2;
    dev->config.sectors = 11;
    dev->config.sector_size = 512;
    
    /* Generate Amiga boot block pattern */
    uint8_t boot[512] = {'D', 'O', 'S', 0};  /* Amiga DOS signature */
    
    for (int c = 0; c < 80; c++) {
        for (int h = 0; h < 2; h++) {
            if (!dev->tracks[c][h].data) {
                dev->tracks[c][h].data = calloc(1, 11 * 512);
            }
            dev->tracks[c][h].size = 11 * 512;
            
            if (c == 0 && h == 0) {
                memcpy(dev->tracks[c][h].data, boot, 512);
            }
        }
    }
    
    log_operation(dev, "Generated Amiga DD disk");
    return UFT_OK;
}

uft_error_t uft_mock_gen_c64(uft_mock_device_t *dev) {
    if (!dev) return UFT_ERR_INVALID_PARAM;
    
    dev->config.cylinders = 35;
    dev->config.heads = 1;
    dev->config.sectors = 21;
    dev->config.sector_size = 256;
    
    /* D64 format: variable sectors per track */
    int sectors_per_track[] = {
        21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  /* 1-17 */
        19, 19, 19, 19, 19, 19, 19,  /* 18-24 */
        18, 18, 18, 18, 18, 18,       /* 25-30 */
        17, 17, 17, 17, 17            /* 31-35 */
    };
    
    for (int c = 0; c < 35; c++) {
        int spt = sectors_per_track[c];
        size_t track_size = spt * 256;
        
        if (!dev->tracks[c][0].data) {
            dev->tracks[c][0].data = calloc(1, track_size);
        }
        dev->tracks[c][0].size = track_size;
    }
    
    log_operation(dev, "Generated C64 disk (35 tracks)");
    return UFT_OK;
}

uft_error_t uft_mock_gen_pc_720k(uft_mock_device_t *dev) {
    if (!dev) return UFT_ERR_INVALID_PARAM;
    
    dev->config.cylinders = 80;
    dev->config.heads = 2;
    dev->config.sectors = 9;
    dev->config.sector_size = 512;
    
    /* FAT12 boot sector */
    uint8_t boot[512] = {0xEB, 0x3C, 0x90};  /* Jump + NOP */
    memcpy(boot + 3, "MSDOS5.0", 8);         /* OEM */
    boot[11] = 0x00; boot[12] = 0x02;        /* Bytes per sector (512) */
    boot[13] = 0x02;                          /* Sectors per cluster */
    boot[14] = 0x01; boot[15] = 0x00;        /* Reserved sectors */
    boot[16] = 0x02;                          /* Number of FATs */
    boot[510] = 0x55; boot[511] = 0xAA;       /* Boot signature */
    
    for (int c = 0; c < 80; c++) {
        for (int h = 0; h < 2; h++) {
            if (!dev->tracks[c][h].data) {
                dev->tracks[c][h].data = calloc(1, 9 * 512);
            }
            dev->tracks[c][h].size = 9 * 512;
            
            if (c == 0 && h == 0) {
                memcpy(dev->tracks[c][h].data, boot, 512);
            }
        }
    }
    
    log_operation(dev, "Generated PC 720K disk");
    return UFT_OK;
}

uft_error_t uft_mock_gen_pc_1440k(uft_mock_device_t *dev) {
    if (!dev) return UFT_ERR_INVALID_PARAM;
    
    dev->config.cylinders = 80;
    dev->config.heads = 2;
    dev->config.sectors = 18;
    dev->config.sector_size = 512;
    
    uft_mock_gen_pc_720k(dev);  /* Reuse with different geometry */
    
    /* Reallocate for 18 sectors */
    for (int c = 0; c < 80; c++) {
        for (int h = 0; h < 2; h++) {
            free(dev->tracks[c][h].data);
            dev->tracks[c][h].data = calloc(1, 18 * 512);
            dev->tracks[c][h].size = 18 * 512;
        }
    }
    
    log_operation(dev, "Generated PC 1.44M disk");
    return UFT_OK;
}

uft_error_t uft_mock_gen_apple2(uft_mock_device_t *dev) {
    if (!dev) return UFT_ERR_INVALID_PARAM;
    
    dev->config.cylinders = 35;
    dev->config.heads = 1;
    dev->config.sectors = 16;
    dev->config.sector_size = 256;
    
    for (int c = 0; c < 35; c++) {
        if (!dev->tracks[c][0].data) {
            dev->tracks[c][0].data = calloc(1, 16 * 256);
        }
        dev->tracks[c][0].size = 16 * 256;
    }
    
    log_operation(dev, "Generated Apple II disk");
    return UFT_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

const char *uft_mock_type_name(uft_mock_type_t type) {
    switch (type) {
        case UFT_MOCK_GREASEWEAZLE: return "Greaseweazle";
        case UFT_MOCK_FLUXENGINE:   return "FluxEngine";
        case UFT_MOCK_KRYOFLUX:     return "KryoFlux";
        case UFT_MOCK_SUPERCARDPRO: return "SuperCard Pro";
        case UFT_MOCK_GENERIC:      return "Generic";
        default:                    return "Unknown";
    }
}

const char *uft_mock_drive_name(uft_mock_drive_t drive) {
    switch (drive) {
        case UFT_MOCK_DRIVE_35DD:   return "3.5\" DD";
        case UFT_MOCK_DRIVE_35HD:   return "3.5\" HD";
        case UFT_MOCK_DRIVE_525DD:  return "5.25\" DD";
        case UFT_MOCK_DRIVE_525HD:  return "5.25\" HD";
        case UFT_MOCK_DRIVE_525QD:  return "5.25\" QD";
        case UFT_MOCK_DRIVE_8INCH:  return "8\"";
        case UFT_MOCK_DRIVE_1541:   return "1541";
        case UFT_MOCK_DRIVE_1571:   return "1571";
        default:                    return "Unknown";
    }
}

const char *uft_mock_error_name(uft_mock_error_t error) {
    switch (error) {
        case UFT_MOCK_ERR_NONE:          return "None";
        case UFT_MOCK_ERR_CRC:           return "CRC";
        case UFT_MOCK_ERR_MISSING:       return "Missing Sector";
        case UFT_MOCK_ERR_WEAK:          return "Weak Bits";
        case UFT_MOCK_ERR_NO_INDEX:      return "No Index";
        case UFT_MOCK_ERR_TIMEOUT:       return "Timeout";
        case UFT_MOCK_ERR_WRITE_PROTECT: return "Write Protected";
        case UFT_MOCK_ERR_NO_DISK:       return "No Disk";
        case UFT_MOCK_ERR_SEEK:          return "Seek Error";
        case UFT_MOCK_ERR_DENSITY:       return "Density Mismatch";
        default:                         return "Unknown";
    }
}

void uft_mock_print_info(const uft_mock_device_t *dev) {
    if (!dev) return;
    
    printf("Mock Device Info:\n");
    printf("  Type:       %s\n", uft_mock_type_name(dev->config.type));
    printf("  Drive:      %s\n", uft_mock_drive_name(dev->config.drive));
    printf("  Geometry:   %d cyl x %d heads x %d sectors x %d bytes\n",
           dev->config.cylinders, dev->config.heads,
           dev->config.sectors, dev->config.sector_size);
    printf("  Position:   C%d/H%d\n", dev->current_cylinder, dev->current_head);
    printf("  Motor:      %s\n", dev->motor_on ? "ON" : "OFF");
    printf("  Write Prot: %s\n", dev->config.write_protect ? "YES" : "NO");
    printf("  Disk:       %s\n", dev->config.disk_present ? "Present" : "Empty");
}

void uft_mock_print_stats(const uft_mock_device_t *dev) {
    if (!dev) return;
    
    printf("Mock Device Statistics:\n");
    printf("  Reads:      %lu\n", (unsigned long)dev->stats.reads);
    printf("  Writes:     %lu\n", (unsigned long)dev->stats.writes);
    printf("  Seeks:      %lu\n", (unsigned long)dev->stats.seeks);
    printf("  Errors:     %lu\n", (unsigned long)dev->stats.errors_injected);
    printf("  Bytes Read: %lu\n", (unsigned long)dev->stats.bytes_read);
    printf("  Bytes Writ: %lu\n", (unsigned long)dev->stats.bytes_written);
}
