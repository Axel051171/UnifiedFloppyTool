/**
 * @file uft_kryoflux_dtc.c
 * @brief KryoFlux DTC Command-Line Wrapper
 * 
 * Provides integration with KryoFlux hardware via the official DTC tool.
 * Since KryoFlux uses a proprietary protocol, this wrapper executes DTC
 * as an external process and reads the resulting flux files.
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#include "uft/hal/uft_kryoflux.h"
#include "uft/compat/uft_alloc.h"
#include "uft/compat/uft_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define PATH_SEP '\\'
#define mkdir(path, mode) _mkdir(path)
#define popen _popen
#define pclose _pclose
#define strcasecmp _stricmp
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <strings.h>  /* strcasecmp */
#define PATH_SEP '/'
#endif

/*============================================================================
 * CONSTANTS
 *============================================================================*/

#define KF_MAX_TRACKS       84
#define KF_MAX_SIDES        2
#define KF_SAMPLE_CLOCK     24027428.5714285  /* KryoFlux sample clock (Hz) */
#define KF_RAW_HEADER_SIZE  7
#define KF_FLUX_BLOCK       0x0D
#define KF_OOB_BLOCK        0x0D

/* DTC output formats */
#define KF_FMT_RAW          0   /* Stream files (.raw) */
#define KF_FMT_STREAM       1   /* Stream files with index */
#define KF_FMT_CT_RAW       2   /* CT Raw format */

/*============================================================================
 * CONFIGURATION STRUCTURE
 *============================================================================*/

struct uft_kf_config_s {
    /* DTC executable path */
    char dtc_path[512];
    
    /* Capture parameters */
    int start_track;
    int end_track;
    int side;               /* 0, 1, or -1 for both */
    int revolutions;
    int output_format;
    
    /* Output directory */
    char output_dir[512];
    char temp_dir[512];
    
    /* Device selection */
    int device_index;       /* -1 for auto */
    
    /* Advanced options */
    bool double_step;       /* For 40-track drives on 80-track media */
    bool index_align;       /* Align capture to index pulse */
    int retry_count;        /* Retries on read error */
    
    /* Status */
    char last_error[256];
    bool dtc_found;
};

/*============================================================================
 * INTERNAL HELPERS
 *============================================================================*/

static bool file_exists(const char *path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES);
#else
    return access(path, F_OK) == 0;
#endif
}

static bool is_executable(const char *path) {
#ifdef _WIN32
    return file_exists(path);
#else
    return access(path, X_OK) == 0;
#endif
}

static int create_directory(const char *path) {
#ifdef _WIN32
    if (CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
        return 0;
    }
    return -1;
#else
    if (mkdir(path, 0755) == 0 || errno == EEXIST) {
        return 0;
    }
    return -1;
#endif
}

/**
 * @brief Parse raw KryoFlux stream data (stub implementation)
 * 
 * This function parses raw KryoFlux stream data into flux timing values.
 * Currently not fully implemented - returns error.
 * 
 * @param raw Raw stream data
 * @param raw_size Size of raw data
 * @param flux_out Output flux array (allocated by function)
 * @param flux_count_out Number of flux values
 * @param index_out Optional index positions (can be NULL)
 * @param index_count_out Optional index count (can be NULL)
 * @return 0 on success, non-zero on error
 */
static int uft_kf_parse_raw_stream(const uint8_t *raw, size_t raw_size,
                                    uint32_t **flux_out, size_t *flux_count_out,
                                    size_t *index_out, size_t *index_count_out) {
    (void)raw;
    (void)raw_size;
    (void)index_out;
    (void)index_count_out;
    
    /* Stub: Not yet implemented */
    *flux_out = NULL;
    *flux_count_out = 0;
    return -1;  /* Return error - feature not implemented */
}

static bool find_dtc_executable(uft_kf_config_t *cfg) {
    /* Check if already set */
    if (cfg->dtc_path[0] && is_executable(cfg->dtc_path)) {
        cfg->dtc_found = true;
        return true;
    }
    
    /* Common locations to check */
    static const char *search_paths[] = {
#ifdef _WIN32
        "dtc.exe",
        "C:\\Program Files\\KryoFlux\\dtc.exe",
        "C:\\Program Files (x86)\\KryoFlux\\dtc.exe",
        "C:\\KryoFlux\\dtc.exe",
#else
        "dtc",
        "/usr/local/bin/dtc",
        "/usr/bin/dtc",
        "/opt/kryoflux/dtc",
        "~/kryoflux/dtc",
#endif
        NULL
    };
    
    for (int i = 0; search_paths[i]; i++) {
        if (is_executable(search_paths[i])) {
            strncpy(cfg->dtc_path, search_paths[i], sizeof(cfg->dtc_path) - 1);
            cfg->dtc_found = true;
            return true;
        }
    }
    
    /* Try PATH */
#ifdef _WIN32
    char path_result[512];
    if (SearchPathA(NULL, "dtc.exe", NULL, sizeof(path_result), path_result, NULL)) {
        strncpy(cfg->dtc_path, path_result, sizeof(cfg->dtc_path) - 1);
        cfg->dtc_found = true;
        return true;
    }
#else
    FILE *fp = popen("which dtc 2>/dev/null", "r");
    if (fp) {
        char result[512];
        if (fgets(result, sizeof(result), fp)) {
            /* Remove trailing newline */
            char *nl = strchr(result, '\n');
            if (nl) *nl = '\0';
            if (is_executable(result)) {
                strncpy(cfg->dtc_path, result, sizeof(cfg->dtc_path) - 1);
                cfg->dtc_found = true;
                pclose(fp);
                return true;
            }
        }
        pclose(fp);
    }
#endif
    
    snprintf(cfg->last_error, sizeof(cfg->last_error),
             "DTC not found. Install KryoFlux software or set dtc_path manually.");
    cfg->dtc_found = false;
    return false;
}

static char* get_temp_directory(void) {
    static char temp[512];
    
#ifdef _WIN32
    DWORD len = GetTempPathA(sizeof(temp), temp);
    if (len > 0 && len < sizeof(temp)) {
        /* Append UFT subdirectory */
        strncat(temp, "uft_kryoflux\\", sizeof(temp) - strlen(temp) - 1);
        create_directory(temp);
        return temp;
    }
    strcpy(temp, "C:\\Temp\\uft_kryoflux\\");
#else
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir) tmpdir = "/tmp";
    snprintf(temp, sizeof(temp), "%s/uft_kryoflux/", tmpdir);
#endif
    
    create_directory(temp);
    return temp;
}

/*============================================================================
 * RAW FILE PARSING
 *============================================================================*/

/**
 * @brief KryoFlux RAW stream block types
 */
typedef enum {
    KF_BLK_FLUX2    = 0x00,  /* 2-byte flux (0x00-0x07) */
    KF_BLK_NOP1     = 0x08,  /* 1-byte NOP */
    KF_BLK_NOP2     = 0x09,  /* 2-byte NOP */
    KF_BLK_NOP3     = 0x0A,  /* 3-byte NOP */
    KF_BLK_OVERFLOW = 0x0B,  /* Overflow (16-bit extension) */
    KF_BLK_FLUX3    = 0x0C,  /* 3-byte flux */
    KF_BLK_OOB      = 0x0D   /* Out-of-band data */
} kf_block_type_t;

/**
 * @brief OOB sub-types
 */
typedef enum {
    KF_OOB_INVALID  = 0x00,
    KF_OOB_INFO     = 0x01,
    KF_OOB_INDEX    = 0x02,
    KF_OOB_STREAM   = 0x03,
    KF_OOB_EOF      = 0x0D
} kf_oob_type_t;

/**
 * @brief Parse KryoFlux RAW stream file
 */
static int parse_kf_raw_file(const char *path, uint32_t **flux_out, 
                              size_t *count_out, uint32_t **index_out,
                              size_t *index_count_out) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    
    /* Get file size */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(fp);
        return -1;
    }
    
    /* Read entire file */
    uint8_t *data = malloc((size_t)file_size);
    if (!data) {
        fclose(fp);
        return -1;
    }
    
    if (fread(data, 1, (size_t)file_size, fp) != (size_t)file_size) {
        free(data);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    
    /* Allocate flux buffer (estimate: file_size / 2 transitions) */
    size_t max_flux = (size_t)file_size / 2 + 1000;
    uint32_t *flux = malloc(max_flux * sizeof(uint32_t));
    uint32_t *index = malloc(256 * sizeof(uint32_t));  /* Max 256 index pulses */
    
    if (!flux || !index) {
        free(data);
        free(flux);
        free(index);
        return -1;
    }
    
    size_t flux_count = 0;
    size_t index_count = 0;
    uint32_t overflow = 0;
    size_t pos = 0;
    uint32_t stream_pos = 0;
    
    while (pos < (size_t)file_size) {
        uint8_t byte = data[pos++];
        
        if (byte <= 0x07) {
            /* 2-byte flux value */
            if (pos >= (size_t)file_size) break;
            uint32_t val = ((uint32_t)byte << 8) | data[pos++];
            val += overflow;
            overflow = 0;
            
            if (flux_count < max_flux) {
                flux[flux_count++] = val;
            }
            stream_pos += val;
            
        } else if (byte == KF_BLK_NOP1) {
            /* 1-byte NOP - skip */
            
        } else if (byte == KF_BLK_NOP2) {
            /* 2-byte NOP */
            pos++;
            
        } else if (byte == KF_BLK_NOP3) {
            /* 3-byte NOP */
            pos += 2;
            
        } else if (byte == KF_BLK_OVERFLOW) {
            /* 16-bit overflow extension */
            overflow += 0x10000;
            
        } else if (byte == KF_BLK_FLUX3) {
            /* 3-byte flux value */
            if (pos + 1 >= (size_t)file_size) break;
            uint32_t val = ((uint32_t)data[pos] << 8) | data[pos + 1];
            pos += 2;
            val += overflow;
            overflow = 0;
            
            if (flux_count < max_flux) {
                flux[flux_count++] = val;
            }
            stream_pos += val;
            
        } else if (byte == KF_BLK_OOB) {
            /* Out-of-band block */
            if (pos + 2 >= (size_t)file_size) break;
            
            uint8_t oob_type = data[pos++];
            uint16_t oob_size = data[pos] | ((uint16_t)data[pos + 1] << 8);
            pos += 2;
            
            if (oob_type == KF_OOB_INDEX && oob_size >= 8) {
                /* Index pulse position */
                if (pos + 7 < (size_t)file_size && index_count < 256) {
                    uint32_t idx_pos = data[pos] | 
                                       ((uint32_t)data[pos + 1] << 8) |
                                       ((uint32_t)data[pos + 2] << 16) |
                                       ((uint32_t)data[pos + 3] << 24);
                    index[index_count++] = idx_pos;
                }
            } else if (oob_type == KF_OOB_EOF) {
                /* End of file */
                break;
            }
            
            pos += oob_size;
            
        } else if (byte >= 0x0E) {
            /* 1-byte flux value */
            uint32_t val = (uint32_t)byte + overflow;
            overflow = 0;
            
            if (flux_count < max_flux) {
                flux[flux_count++] = val;
            }
            stream_pos += val;
        }
    }
    
    free(data);
    
    /* Resize to actual size */
    if (flux_count > 0) {
        uint32_t *resized = realloc(flux, flux_count * sizeof(uint32_t));
        if (resized) flux = resized;
    }
    
    *flux_out = flux;
    *count_out = flux_count;
    
    if (index_out && index_count_out) {
        if (index_count > 0) {
            uint32_t *idx_resized = realloc(index, index_count * sizeof(uint32_t));
            if (idx_resized) index = idx_resized;
            *index_out = index;
            *index_count_out = index_count;
        } else {
            free(index);
            *index_out = NULL;
            *index_count_out = 0;
        }
    } else {
        free(index);
    }
    
    return 0;
}

/*============================================================================
 * DTC COMMAND EXECUTION
 *============================================================================*/

/**
 * @brief Build DTC command line
 */
static int build_dtc_command(const uft_kf_config_t *cfg, int track, int side,
                              char *cmd, size_t cmd_size) {
    int len = 0;
    
    /* Base command with output format */
    len = snprintf(cmd, cmd_size, "\"%s\" -f%d",
                   cfg->dtc_path,
                   cfg->output_format);
    
    /* Track range: -i<start> -e<end> */
    if (track >= 0) {
        len += snprintf(cmd + len, cmd_size - (size_t)len, " -i%d -e%d", track, track);
    } else {
        len += snprintf(cmd + len, cmd_size - (size_t)len, " -i%d -e%d",
                       cfg->start_track, cfg->end_track);
    }
    
    /* Side selection: -s<side> or -g0/1 for both */
    if (side >= 0) {
        len += snprintf(cmd + len, cmd_size - (size_t)len, " -s%d", side);
    } else if (cfg->side >= 0) {
        len += snprintf(cmd + len, cmd_size - (size_t)len, " -s%d", cfg->side);
    } else {
        len += snprintf(cmd + len, cmd_size - (size_t)len, " -g0");
    }
    
    /* Device index */
    if (cfg->device_index >= 0) {
        len += snprintf(cmd + len, cmd_size - (size_t)len, " -d%d", cfg->device_index);
    }
    
    /* Double step for 40-track drives */
    if (cfg->double_step) {
        len += snprintf(cmd + len, cmd_size - (size_t)len, " -k2");
    }
    
    /* Retries */
    if (cfg->retry_count > 0) {
        len += snprintf(cmd + len, cmd_size - (size_t)len, " -t%d", cfg->retry_count);
    }
    
    /* Output path */
    len += snprintf(cmd + len, cmd_size - (size_t)len, " -p\"%s\"", cfg->temp_dir);
    
    return len;
}

/**
 * @brief Execute DTC and capture output
 */
static int execute_dtc(uft_kf_config_t *cfg, const char *cmd) {
    char full_cmd[2048];
    
    /* Redirect stderr to stdout for unified capture */
#ifdef _WIN32
    snprintf(full_cmd, sizeof(full_cmd), "%s 2>&1", cmd);
#else
    snprintf(full_cmd, sizeof(full_cmd), "%s 2>&1", cmd);
#endif
    
    FILE *fp = popen(full_cmd, "r");
    if (!fp) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Failed to execute DTC: %s", cmd);
        return -1;
    }
    
    char output[4096] = {0};
    size_t total = 0;
    char line[256];
    
    while (fgets(line, sizeof(line), fp) && total < sizeof(output) - 256) {
        total += (size_t)snprintf(output + total, sizeof(output) - total, "%s", line);
    }
    
    int status = pclose(fp);
    
#ifndef _WIN32
    if (WIFEXITED(status)) {
        status = WEXITSTATUS(status);
    }
#endif
    
    if (status != 0) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "DTC failed (exit %d): %.200s", status, output);
        return -1;
    }
    
    return 0;
}

/*============================================================================
 * PUBLIC API
 *============================================================================*/

uft_kf_config_t* uft_kf_config_create(void) {
    uft_kf_config_t *cfg = uft_calloc(1, sizeof(uft_kf_config_t));
    if (!cfg) return NULL;
    
    /* Defaults */
    cfg->start_track = 0;
    cfg->end_track = 83;
    cfg->side = -1;          /* Both sides */
    cfg->revolutions = 2;
    cfg->output_format = KF_FMT_RAW;
    cfg->device_index = -1;  /* Auto */
    cfg->double_step = false;
    cfg->index_align = true;
    cfg->retry_count = 3;
    
    /* Get temp directory */
    strncpy(cfg->temp_dir, get_temp_directory(), sizeof(cfg->temp_dir) - 1);
    
    /* Try to find DTC */
    find_dtc_executable(cfg);
    
    return cfg;
}

void uft_kf_config_destroy(uft_kf_config_t *cfg) {
    if (cfg) free(cfg);
}

int uft_kf_set_dtc_path(uft_kf_config_t *cfg, const char *path) {
    if (!cfg || !path) return -1;
    
    strncpy(cfg->dtc_path, path, sizeof(cfg->dtc_path) - 1);
    
    if (!is_executable(path)) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "DTC not found or not executable: %s", path);
        cfg->dtc_found = false;
        return -1;
    }
    
    cfg->dtc_found = true;
    return 0;
}

int uft_kf_set_output_dir(uft_kf_config_t *cfg, const char *path) {
    if (!cfg || !path) return -1;
    
    strncpy(cfg->output_dir, path, sizeof(cfg->output_dir) - 1);
    strncpy(cfg->temp_dir, path, sizeof(cfg->temp_dir) - 1);
    
    if (create_directory(path) != 0) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Cannot create output directory: %s", path);
        return -1;
    }
    
    return 0;
}

int uft_kf_set_track_range(uft_kf_config_t *cfg, int start, int end) {
    if (!cfg) return -1;
    if (start < 0 || start > KF_MAX_TRACKS) return -1;
    if (end < start || end > KF_MAX_TRACKS) return -1;
    
    cfg->start_track = start;
    cfg->end_track = end;
    return 0;
}

int uft_kf_set_side(uft_kf_config_t *cfg, int side) {
    if (!cfg) return -1;
    if (side < -1 || side > 1) return -1;
    
    cfg->side = side;
    return 0;
}

int uft_kf_set_revolutions(uft_kf_config_t *cfg, int revs) {
    if (!cfg || revs < 1 || revs > 10) return -1;
    cfg->revolutions = revs;
    return 0;
}

int uft_kf_set_device(uft_kf_config_t *cfg, int device_index) {
    if (!cfg) return -1;
    cfg->device_index = device_index;
    return 0;
}

int uft_kf_set_double_step(uft_kf_config_t *cfg, bool enabled) {
    if (!cfg) return -1;
    cfg->double_step = enabled;
    return 0;
}

int uft_kf_set_retry_count(uft_kf_config_t *cfg, int retries) {
    if (!cfg || retries < 0 || retries > 20) return -1;
    cfg->retry_count = retries;
    return 0;
}

bool uft_kf_is_available(const uft_kf_config_t *cfg) {
    if (!cfg) return false;
    return cfg->dtc_found;
}

const char* uft_kf_get_dtc_path(const uft_kf_config_t *cfg) {
    if (!cfg) return NULL;
    return cfg->dtc_found ? cfg->dtc_path : NULL;
}

const char* uft_kf_get_error(const uft_kf_config_t *cfg) {
    if (!cfg) return "NULL config";
    return cfg->last_error;
}

int uft_kf_capture_track(uft_kf_config_t *cfg, int track, int side,
                          uint32_t **flux, size_t *flux_count,
                          uint32_t **index, size_t *index_count) {
    if (!cfg || !flux || !flux_count) return -1;
    if (track < 0 || track > KF_MAX_TRACKS) return -1;
    if (side < 0 || side > 1) return -1;
    
    if (!cfg->dtc_found) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "DTC not found. Call uft_kf_set_dtc_path() first.");
        return -1;
    }
    
    /* Build and execute command */
    char cmd[2048];
    build_dtc_command(cfg, track, side, cmd, sizeof(cmd));
    
    if (execute_dtc(cfg, cmd) != 0) {
        return -1;
    }
    
    /* Build expected output filename */
    char filename[1024];
    snprintf(filename, sizeof(filename), "%s%strack%02d.%d.raw",
             cfg->temp_dir, 
             (cfg->temp_dir[strlen(cfg->temp_dir) - 1] == PATH_SEP) ? "" : "/",
             track, side);
    
    /* Parse the raw file */
    if (parse_kf_raw_file(filename, flux, flux_count, index, index_count) != 0) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Failed to parse RAW file: %s", filename);
        return -1;
    }
    
    return 0;
}

int uft_kf_capture_disk(uft_kf_config_t *cfg, uft_kf_disk_callback_t callback,
                         void *user_data) {
    if (!cfg || !callback) return -1;
    
    if (!cfg->dtc_found) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "DTC not found. Call uft_kf_set_dtc_path() first.");
        return -1;
    }
    
    int captured = 0;
    int errors = 0;
    
    /* Capture all tracks */
    for (int track = cfg->start_track; track <= cfg->end_track; track++) {
        int side_start = (cfg->side >= 0) ? cfg->side : 0;
        int side_end = (cfg->side >= 0) ? cfg->side : 1;
        
        for (int side = side_start; side <= side_end; side++) {
            uint32_t *flux = NULL;
            size_t flux_count = 0;
            uint32_t *index = NULL;
            size_t index_count = 0;
            
            int result = uft_kf_capture_track(cfg, track, side,
                                               &flux, &flux_count,
                                               &index, &index_count);
            
            /* Call user callback */
            uft_kf_track_data_t data = {
                .track = track,
                .side = side,
                .flux = flux,
                .flux_count = flux_count,
                .index = index,
                .index_count = index_count,
                .sample_clock = KF_SAMPLE_CLOCK,
                .success = (result == 0),
                .error_msg = (result != 0) ? cfg->last_error : NULL
            };
            
            int cb_result = callback(&data, user_data);
            
            /* Cleanup */
            free(flux);
            free(index);
            
            if (result == 0) {
                captured++;
            } else {
                errors++;
            }
            
            /* Check if callback wants to abort */
            if (cb_result != 0) {
                snprintf(cfg->last_error, sizeof(cfg->last_error),
                         "Capture aborted by callback at track %d side %d",
                         track, side);
                return -1;
            }
        }
    }
    
    if (errors > 0) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Capture completed with %d errors (%d tracks OK)",
                 errors, captured);
    }
    
    return captured;
}

/*============================================================================
 * FLUX CONVERSION UTILITIES
 *============================================================================*/

double uft_kf_ticks_to_ns(uint32_t ticks) {
    return (double)ticks * (1e9 / KF_SAMPLE_CLOCK);
}

double uft_kf_ticks_to_us(uint32_t ticks) {
    return (double)ticks * (1e6 / KF_SAMPLE_CLOCK);
}

uint32_t uft_kf_ns_to_ticks(double ns) {
    return (uint32_t)(ns * KF_SAMPLE_CLOCK / 1e9 + 0.5);
}

uint32_t uft_kf_us_to_ticks(double us) {
    return (uint32_t)(us * KF_SAMPLE_CLOCK / 1e6 + 0.5);
}

double uft_kf_get_sample_clock(void) {
    return KF_SAMPLE_CLOCK;
}

/*============================================================================
 * DEVICE DETECTION
 *============================================================================*/

int uft_kf_detect_devices(uft_kf_config_t *cfg, int *device_count) {
    if (!cfg || !device_count) return -1;
    
    if (!cfg->dtc_found) {
        *device_count = 0;
        return 0;
    }
    
    /* Run DTC with info query */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "\"%s\" -i 2>&1", cfg->dtc_path);
    
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        *device_count = 0;
        return -1;
    }
    
    char output[4096] = {0};
    size_t total = 0;
    char line[256];
    
    while (fgets(line, sizeof(line), fp) && total < sizeof(output) - 256) {
        total += (size_t)snprintf(output + total, sizeof(output) - total, "%s", line);
    }
    pclose(fp);
    
    /* Count "KryoFlux" occurrences */
    int count = 0;
    const char *ptr = output;
    while ((ptr = strstr(ptr, "KryoFlux")) != NULL) {
        count++;
        ptr++;
    }
    
    *device_count = count;
    return 0;
}

/*============================================================================
 * PRESETS
 *============================================================================*/

static const struct {
    uft_kf_drive_type_t type;
    const char *name;
    int tracks;
    bool double_step;
    int sides;
} g_drive_presets[] = {
    { UFT_KF_DRIVE_AUTO,    "Auto-detect",   83, false, 2 },
    { UFT_KF_DRIVE_35_DD,   "3.5\" DD",      79, false, 2 },
    { UFT_KF_DRIVE_35_HD,   "3.5\" HD",      79, false, 2 },
    { UFT_KF_DRIVE_525_DD,  "5.25\" DD",     39, false, 2 },
    { UFT_KF_DRIVE_525_HD,  "5.25\" HD",     79, false, 2 },
    { UFT_KF_DRIVE_525_40,  "5.25\" 40-trk", 39, true,  1 },
    { UFT_KF_DRIVE_8_SSSD,  "8\" SS/SD",     76, false, 1 },
    { UFT_KF_DRIVE_8_DSDD,  "8\" DS/DD",     76, false, 2 },
};

static const struct {
    uft_kf_platform_t platform;
    const char *name;
    int start_track;
    int end_track;
    int side;           /* -1 = both */
    bool double_step;
    int revolutions;
} g_platform_presets[] = {
    { UFT_KF_PLATFORM_GENERIC,    "Generic",        0, 83, -1, false, 2 },
    { UFT_KF_PLATFORM_AMIGA,      "Amiga",          0, 79, -1, false, 2 },
    { UFT_KF_PLATFORM_ATARI_ST,   "Atari ST",       0, 79, -1, false, 2 },
    { UFT_KF_PLATFORM_C64,        "Commodore 64",   0, 39, -1, true,  3 },
    { UFT_KF_PLATFORM_C1541,      "C1541",          0, 39,  0, true,  3 },
    { UFT_KF_PLATFORM_APPLE_II,   "Apple II",       0, 34, -1, true,  3 },
    { UFT_KF_PLATFORM_IBM_PC,     "IBM PC",         0, 79, -1, false, 2 },
    { UFT_KF_PLATFORM_BBC_MICRO,  "BBC Micro",      0, 79, -1, false, 2 },
    { UFT_KF_PLATFORM_TRS80,      "TRS-80",         0, 39, -1, true,  2 },
    { UFT_KF_PLATFORM_AMSTRAD_CPC,"Amstrad CPC",    0, 39, -1, false, 2 },
    { UFT_KF_PLATFORM_MSX,        "MSX",            0, 79, -1, false, 2 },
    { UFT_KF_PLATFORM_PC98,       "NEC PC-98",      0, 76, -1, false, 2 },
    { UFT_KF_PLATFORM_X68000,     "Sharp X68000",   0, 79, -1, false, 2 },
    { UFT_KF_PLATFORM_FM_TOWNS,   "FM Towns",       0, 79, -1, false, 2 },
};

#define DRIVE_PRESET_COUNT (sizeof(g_drive_presets) / sizeof(g_drive_presets[0]))
#define PLATFORM_PRESET_COUNT (sizeof(g_platform_presets) / sizeof(g_platform_presets[0]))

int uft_kf_apply_drive_preset(uft_kf_config_t *cfg, uft_kf_drive_type_t drive_type) {
    if (!cfg) return -1;
    
    for (size_t i = 0; i < DRIVE_PRESET_COUNT; i++) {
        if (g_drive_presets[i].type == drive_type) {
            cfg->end_track = g_drive_presets[i].tracks;
            cfg->double_step = g_drive_presets[i].double_step;
            if (g_drive_presets[i].sides == 1) {
                cfg->side = 0;
            }
            return 0;
        }
    }
    return -1;
}

int uft_kf_apply_platform_preset(uft_kf_config_t *cfg, uft_kf_platform_t platform) {
    if (!cfg) return -1;
    
    for (size_t i = 0; i < PLATFORM_PRESET_COUNT; i++) {
        if (g_platform_presets[i].platform == platform) {
            cfg->start_track = g_platform_presets[i].start_track;
            cfg->end_track = g_platform_presets[i].end_track;
            cfg->side = g_platform_presets[i].side;
            cfg->double_step = g_platform_presets[i].double_step;
            cfg->revolutions = g_platform_presets[i].revolutions;
            return 0;
        }
    }
    return -1;
}

const char* uft_kf_platform_name(uft_kf_platform_t platform) {
    for (size_t i = 0; i < PLATFORM_PRESET_COUNT; i++) {
        if (g_platform_presets[i].platform == platform) {
            return g_platform_presets[i].name;
        }
    }
    return "Unknown";
}

const char* uft_kf_drive_name(uft_kf_drive_type_t drive_type) {
    for (size_t i = 0; i < DRIVE_PRESET_COUNT; i++) {
        if (g_drive_presets[i].type == drive_type) {
            return g_drive_presets[i].name;
        }
    }
    return "Unknown";
}

/*============================================================================
 * UFT PARAMETER INTEGRATION
 *============================================================================*/

/* Forward declaration for UFT params (avoid circular include) */
struct uft_params_s;

uft_kf_config_t* uft_kf_config_from_params(const void *params) {
    uft_kf_config_t *cfg = uft_kf_config_create();
    if (!cfg) return NULL;
    
    /* If no params provided, return defaults */
    if (!params) return cfg;
    
    /* 
     * TODO: Read from UFT parameter system once integrated
     * 
     * Example keys:
     * - "kryoflux.dtc_path"    -> string
     * - "kryoflux.device"      -> int (-1 = auto)
     * - "kryoflux.start_track" -> int
     * - "kryoflux.end_track"   -> int
     * - "kryoflux.side"        -> int (-1 = both)
     * - "kryoflux.revolutions" -> int
     * - "kryoflux.double_step" -> bool
     * - "kryoflux.retry_count" -> int
     * - "kryoflux.platform"    -> string (e.g., "amiga", "c64")
     */
    
    return cfg;
}

int uft_kf_config_to_params(const uft_kf_config_t *cfg, void *params) {
    if (!cfg || !params) return -1;
    
    /*
     * TODO: Write to UFT parameter system once integrated
     */
    
    return 0;
}

/*============================================================================
 * CLI ARGUMENT PARSING
 *============================================================================*/

/**
 * @brief Parse command-line arguments into config
 * 
 * Supports:
 *   --dtc-path <path>     Path to DTC executable
 *   --device <n>          Device index (0-based)
 *   --tracks <s>-<e>      Track range (e.g., "0-79")
 *   --side <n>            Side (0, 1, or "both")
 *   --revs <n>            Revolutions to capture
 *   --double-step         Enable double stepping
 *   --retries <n>         Retry count
 *   --platform <name>     Apply platform preset
 *   --output <dir>        Output directory
 */
int uft_kf_config_parse_args(uft_kf_config_t *cfg, int argc, char **argv) {
    if (!cfg || !argv) return -1;
    
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--dtc-path") == 0 && i + 1 < argc) {
            uft_kf_set_dtc_path(cfg, argv[++i]);
        }
        else if (strcmp(argv[i], "--device") == 0 && i + 1 < argc) {
            cfg->device_index = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--tracks") == 0 && i + 1 < argc) {
            int s, e;
            if (sscanf(argv[++i], "%d-%d", &s, &e) == 2) {
                uft_kf_set_track_range(cfg, s, e);
            }
        }
        else if (strcmp(argv[i], "--side") == 0 && i + 1 < argc) {
            const char *arg = argv[++i];
            if (strcmp(arg, "both") == 0) {
                cfg->side = -1;
            } else {
                cfg->side = atoi(arg);
            }
        }
        else if (strcmp(argv[i], "--revs") == 0 && i + 1 < argc) {
            uft_kf_set_revolutions(cfg, atoi(argv[++i]));
        }
        else if (strcmp(argv[i], "--double-step") == 0) {
            cfg->double_step = true;
        }
        else if (strcmp(argv[i], "--retries") == 0 && i + 1 < argc) {
            uft_kf_set_retry_count(cfg, atoi(argv[++i]));
        }
        else if (strcmp(argv[i], "--platform") == 0 && i + 1 < argc) {
            const char *name = argv[++i];
            /* Match platform by name */
            for (size_t p = 0; p < PLATFORM_PRESET_COUNT; p++) {
                if (strcasecmp(name, g_platform_presets[p].name) == 0 ||
                    (strstr(g_platform_presets[p].name, name) != NULL)) {
                    uft_kf_apply_platform_preset(cfg, g_platform_presets[p].platform);
                    break;
                }
            }
        }
        else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            uft_kf_set_output_dir(cfg, argv[++i]);
        }
    }
    
    return 0;
}

/**
 * @brief Print usage help
 */
void uft_kf_print_help(void) {
    printf("KryoFlux DTC Wrapper Options:\n");
    printf("  --dtc-path <path>     Path to DTC executable\n");
    printf("  --device <n>          Device index (0-based, -1=auto)\n");
    printf("  --tracks <s>-<e>      Track range (e.g., '0-79')\n");
    printf("  --side <n|both>       Side (0, 1, or 'both')\n");
    printf("  --revs <n>            Revolutions to capture (1-10)\n");
    printf("  --double-step         Enable double stepping (40-track disks)\n");
    printf("  --retries <n>         Retry count on errors (0-20)\n");
    printf("  --platform <name>     Apply platform preset\n");
    printf("  --output <dir>        Output directory\n");
    printf("\n");
    printf("Platform Presets:\n");
    for (size_t i = 0; i < PLATFORM_PRESET_COUNT; i++) {
        printf("  %-16s  Tracks %d-%d, %s, %d rev%s%s\n",
               g_platform_presets[i].name,
               g_platform_presets[i].start_track,
               g_platform_presets[i].end_track,
               g_platform_presets[i].side < 0 ? "both sides" :
                   (g_platform_presets[i].side == 0 ? "side 0" : "side 1"),
               g_platform_presets[i].revolutions,
               g_platform_presets[i].revolutions > 1 ? "s" : "",
               g_platform_presets[i].double_step ? ", double-step" : "");
    }
}

/**
 * @brief Print current configuration
 */
void uft_kf_print_config(const uft_kf_config_t *cfg) {
    if (!cfg) return;
    
    printf("KryoFlux Configuration:\n");
    printf("  DTC Path:      %s\n", cfg->dtc_found ? cfg->dtc_path : "(not found)");
    printf("  Device:        %s\n", cfg->device_index < 0 ? "auto" : "");
    if (cfg->device_index >= 0) printf("%d", cfg->device_index);
    printf("  Track Range:   %d-%d\n", cfg->start_track, cfg->end_track);
    printf("  Side:          %s\n", cfg->side < 0 ? "both" : 
                                    (cfg->side == 0 ? "0" : "1"));
    printf("  Revolutions:   %d\n", cfg->revolutions);
    printf("  Double Step:   %s\n", cfg->double_step ? "yes" : "no");
    printf("  Retries:       %d\n", cfg->retry_count);
    printf("  Output Dir:    %s\n", cfg->temp_dir);
}

/*============================================================================
 * WRITE OPERATIONS
 *============================================================================*/

int uft_kf_flux_to_raw(const uint32_t* flux, size_t count,
                            const uint32_t* index, size_t index_count,
                            uint8_t* output, size_t max_size) {
    if (!flux || count == 0 || !output || max_size < 100) return -1;
    
    size_t pos = 0;
    
    /* KryoFlux RAW format:
     * - 1-byte flux: 0x01-0x07 (value)
     * - 2-byte flux: 0x08, low, high
     * - 3-byte flux: 0x0C, low, mid, high  (OVL16)
     * - NOP: 0x08 (no flux, timing filler)
     * - Overflow: 0x0B (add 0x10000 to next value)
     * - OOB blocks: 0x0D, type, len_low, len_high, data...
     */
    
    /* Write OOB header block (type 1 = stream info) */
    if (pos + 8 < max_size) {
        output[pos++] = 0x0D;  /* OOB marker */
        output[pos++] = 0x01;  /* Type: stream info */
        output[pos++] = 0x04;  /* Length low */
        output[pos++] = 0x00;  /* Length high */
        /* Stream info: offset (4 bytes) */
        output[pos++] = 0x00;
        output[pos++] = 0x00;
        output[pos++] = 0x00;
        output[pos++] = 0x00;
    }
    
    /* Convert flux values */
    for (size_t i = 0; i < count && pos + 4 < max_size; i++) {
        uint32_t val = flux[i];
        
        /* Check for index pulse */
        if (index && index_count > 0) {
            for (size_t idx = 0; idx < index_count; idx++) {
                if (index[idx] == i) {
                    /* Write index OOB block */
                    if (pos + 12 < max_size) {
                        output[pos++] = 0x0D;  /* OOB */
                        output[pos++] = 0x02;  /* Type: index */
                        output[pos++] = 0x08;  /* Length: 8 */
                        output[pos++] = 0x00;
                        /* Stream position (4 bytes) + timer (4 bytes) */
                        {
                            uint32_t stream_pos = (uint32_t)pos;
                            output[pos++] = stream_pos & 0xFF;
                            output[pos++] = (stream_pos >> 8) & 0xFF;
                            output[pos++] = (stream_pos >> 16) & 0xFF;
                            output[pos++] = (stream_pos >> 24) & 0xFF;
                        }
                        output[pos++] = 0x00;
                        output[pos++] = 0x00;
                        output[pos++] = 0x00;
                        output[pos++] = 0x00;
                    }
                    break;
                }
            }
        }
        
        if (val <= 0x07) {
            /* 1-byte encoding: 0x01-0x07 */
            output[pos++] = (uint8_t)val;
        } else if (val <= 0x7FF) {
            /* 2-byte encoding: 0x08 + low + high */
            output[pos++] = 0x08;
            output[pos++] = val & 0xFF;
            output[pos++] = (val >> 8) & 0xFF;
        } else if (val <= 0xFFFF) {
            /* Still 2-byte but with overflow */
            output[pos++] = 0x00;  /* Flux1 with value 0 = NOP */
            output[pos++] = 0x08;
            output[pos++] = val & 0xFF;
            output[pos++] = (val >> 8) & 0xFF;
        } else {
            /* 3-byte encoding with OVL16 */
            uint32_t overflow_count = val >> 16;
            uint16_t remainder = val & 0xFFFF;
            
            /* Write overflow markers */
            while (overflow_count-- > 0 && pos < max_size) {
                output[pos++] = 0x0B;  /* Overflow */
            }
            
            /* Write remainder */
            if (pos + 3 < max_size) {
                output[pos++] = 0x0C;  /* OVL16 */
                output[pos++] = remainder & 0xFF;
                output[pos++] = (remainder >> 8) & 0xFF;
            }
        }
    }
    
    /* Write end-of-stream OOB block */
    if (pos + 8 < max_size) {
        uint32_t stream_pos = (uint32_t)pos;
        output[pos++] = 0x0D;
        output[pos++] = 0x0D;  /* Type: EOF */
        output[pos++] = 0x04;
        output[pos++] = 0x00;
        output[pos++] = stream_pos & 0xFF;
        output[pos++] = (stream_pos >> 8) & 0xFF;
        output[pos++] = (stream_pos >> 16) & 0xFF;
        output[pos++] = (stream_pos >> 24) & 0xFF;
    }
    
    return (int)pos;
}

int uft_kf_write_track(uft_kf_config_t* config, int track, int side,
                        const uint32_t* flux, size_t count) {
    if (!config || !flux || count == 0) return -1;
    
    /* Check if DTC supports writing */
    if (!uft_kf_write_supported(config)) {
        snprintf(config->last_error, sizeof(config->last_error),
                 "DTC write not supported or not available");
        return -1;
    }
    
    /* Convert flux to RAW format */
    size_t raw_size = count * 4 + 100;  /* Estimate */
    uint8_t* raw_data = malloc(raw_size);
    if (!raw_data) return -1;
    
    ssize_t raw_len = uft_kf_flux_to_raw(flux, count, NULL, 0, raw_data, raw_size);
    if (raw_len < 0) {
        free(raw_data);
        return -1;
    }
    
    /* Write to temp file */
    char temp_path[1024];
    snprintf(temp_path, sizeof(temp_path), "/tmp/uft_kf_write_%d_%d.raw", 
             track, side);
    
    FILE* f = fopen(temp_path, "wb");
    if (!f) {
        free(raw_data);
        snprintf(config->last_error, sizeof(config->last_error),
                 "Cannot create temp file: %s", temp_path);
        return -1;
    }
    
    fwrite(raw_data, 1, raw_len, f);
    fclose(f);
    free(raw_data);
    
    /* Build DTC command for write */
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "%s -w -p -i0 -e%d -s%d -g%d -t%d \"%s\"",
             config->dtc_path,
             track,       /* End track */
             side,        /* Side */
             config->double_step ? 2 : 1,  /* Step */
             track,       /* Track */
             temp_path);
    
    /* Execute */
    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        snprintf(config->last_error, sizeof(config->last_error),
                 "Cannot execute DTC write command");
        return -1;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), pipe)) {
        /* Check for errors */
        if (strstr(line, "error") || strstr(line, "Error")) {
            snprintf(config->last_error, sizeof(config->last_error),
                     "DTC write error: %s", line);
            pclose(pipe);
            return -1;
        }
    }
    
    int status = pclose(pipe);
    
    /* Cleanup temp file */
    remove(temp_path);
    
    return (status == 0) ? 0 : -1;
}

int uft_kf_write_disk(uft_kf_config_t* config, const char* input_dir,
                       uft_kf_disk_callback_t callback, void* user) {
    if (!config || !input_dir) return -1;
    
    int tracks_written = 0;
    
    for (int track = config->start_track; track <= config->end_track; track++) {
        int side_start = (config->side >= 0) ? config->side : 0;
        int side_end = (config->side >= 0) ? config->side : 1;
        
        for (int side = side_start; side <= side_end; side++) {
            /* Look for raw file */
            char raw_path[1024];
            snprintf(raw_path, sizeof(raw_path), "%s/track%02d.%d.raw",
                     input_dir, track, side);
            
            FILE* f = fopen(raw_path, "rb");
            if (!f) {
                /* Try alternate naming */
                snprintf(raw_path, sizeof(raw_path), "%s/track%02d_%d.raw",
                         input_dir, track, side);
                f = fopen(raw_path, "rb");
            }
            
            if (!f) continue;  /* Skip missing tracks */
            
            /* Read raw file */
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fseek(f, 0, SEEK_SET);
            
            uint8_t* raw = malloc(size);
            if (!raw) {
                fclose(f);
                continue;
            }
            
            fread(raw, 1, size, f);
            fclose(f);
            
            /* Parse raw to get flux data */
            uint32_t* flux = NULL;
            size_t flux_count = 0;
            
            if (uft_kf_parse_raw_stream(raw, size, &flux, &flux_count, 
                                         NULL, NULL) == 0) {
                /* Write track */
                if (uft_kf_write_track(config, track, side, flux, flux_count) == 0) {
                    tracks_written++;
                }
                free(flux);
            }
            
            free(raw);
            
            /* Progress callback */
            if (callback) {
                uft_kf_track_data_t progress = {
                    .track = track,
                    .side = side,
                    .success = true,
                    .flux_count = flux_count
                };
                if (callback(&progress, user) != 0) {
                    return tracks_written;  /* Abort */
                }
            }
        }
    }
    
    return tracks_written;
}

bool uft_kf_write_supported(const uft_kf_config_t* config) {
    if (!config) return false;
    
    /* Check DTC version - write support was added in firmware 3.0+ */
    /* For now, assume write is supported if DTC is available */
    return uft_kf_is_available(config);
}
