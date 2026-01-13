/**
 * @file uft_advanced_mode.c
 * @brief UFT Advanced Mode Implementation
 * 
 * "Bei uns geht kein Bit verloren" - UFT Preservation Philosophy
 */

#include "uft/uft_advanced_mode.h"
#include "uft/uft_v3_bridge.h"
#include "uft/uft_god_mode.h"
#include "uft/uft_format_probes.h"
#include "uft/uft_protection.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * GLOBAL STATE
 * ═══════════════════════════════════════════════════════════════════════════════ */

static bool g_advanced_enabled = false;
static uft_advanced_config_t g_config = {0};

/* External v3 handlers */
extern uft_format_handler_t uft_d64_v3_handler;
extern uft_format_handler_t uft_g64_v3_handler;
extern uft_format_handler_t uft_scp_v3_handler;

/* External probe functions from format registry */
extern uft_error_t uft_d64_probe(const void* data, size_t size, int* confidence);

/* External detection functions */
extern bool uft_d64_v3_detect_protection(void* handle, char* name, size_t name_size);
extern bool uft_g64_v3_detect_protection(void* handle, char* name, size_t name_size);
extern bool uft_scp_v3_detect_protection(void* handle, char* name, size_t name_size);

/* ═══════════════════════════════════════════════════════════════════════════════
 * CONFIGURATION
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_advanced_init(void) {
    g_advanced_enabled = true;
    
    g_config.flags = UFT_ADV_USE_V3_PARSERS | 
                     UFT_ADV_AUTO_PROTECTION |
                     UFT_ADV_GOD_MODE |
                     UFT_ADV_BAYESIAN_DETECT;
    g_config.quality_threshold = 70;
    g_config.bayesian_min_confidence = 60;
    g_config.max_crc_corrections = 3;
    g_config.verbose_logging = false;
}

void uft_advanced_set_config(const uft_advanced_config_t* config) {
    if (config) g_config = *config;
}

const uft_advanced_config_t* uft_advanced_get_config(void) {
    return &g_config;
}

void uft_advanced_enable(bool enable) {
    g_advanced_enabled = enable;
    if (enable && g_config.flags == 0) {
        uft_advanced_init();
    }
}

bool uft_advanced_is_enabled(void) {
    return g_advanced_enabled;
}

void uft_advanced_enable_feature(uft_advanced_flags_t flag) {
    g_config.flags |= flag;
}

void uft_advanced_disable_feature(uft_advanced_flags_t flag) {
    g_config.flags &= ~flag;
}

bool uft_advanced_has_feature(uft_advanced_flags_t flag) {
    return (g_config.flags & flag) != 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * FORMAT DETECTION
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    FMT_UNKNOWN = 0,
    FMT_D64,
    FMT_G64,
    FMT_SCP,
    FMT_HFE,
    FMT_ADF,
    FMT_IMD,
    FMT_STX
} internal_format_t;

static internal_format_t detect_format_internal(const uint8_t* data, size_t size, 
                                                 size_t file_size, int* confidence) {
    int best_conf = 0;
    internal_format_t best_fmt = FMT_UNKNOWN;
    int conf;
    
    /* Check flux formats first (have magic bytes) */
    if (size >= 3 && data[0] == 'S' && data[1] == 'C' && data[2] == 'P') {
        *confidence = 95;
        return FMT_SCP;
    }
    
    if (size >= 8 && memcmp(data, "GCR-1541", 8) == 0) {
        *confidence = 95;
        return FMT_G64;
    }
    
    if (size >= 8 && memcmp(data, "HXCPICFE", 8) == 0) {
        *confidence = 95;
        return FMT_HFE;
    }
    
    if (size >= 4 && memcmp(data, "IMD ", 4) == 0) {
        *confidence = 95;
        return FMT_IMD;
    }
    
    /* Size-based detection for sector images */
    if (uft_d64_probe(data, size, &conf) == UFT_OK && conf > best_conf) {
        best_conf = conf;
        best_fmt = FMT_D64;
    }
    
    /* ADF by size */
    if (file_size == 901120 || file_size == 1802240) {
        if (best_conf < 85) {
            best_conf = 85;
            best_fmt = FMT_ADF;
        }
    }
    
    *confidence = best_conf;
    return best_fmt;
}

int uft_advanced_detect_format(const char* path, int* confidence) {
    FILE* f = fopen(path, "rb");
    if (!f) return FMT_UNKNOWN;
    
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    /* Read header for detection */
    uint8_t header[8192];
    size_t read_size = file_size < sizeof(header) ? file_size : sizeof(header);
    if (fread(header, 1, read_size, f) != read_size) {
        fclose(f);
        return FMT_UNKNOWN;
    }
    fclose(f);
    
    int conf = 0;
    internal_format_t fmt = detect_format_internal(header, read_size, file_size, &conf);
    
    if (confidence) *confidence = conf;
    return (int)fmt;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * ADVANCED OPEN
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_advanced_open(const char* path, uft_advanced_handle_t** out_handle) {
    if (!path || !out_handle) return UFT_ERR_INVALID_ARG;
    
    /* Allocate handle */
    uft_advanced_handle_t* h = calloc(1, sizeof(uft_advanced_handle_t));
    if (!h) return UFT_ERR_MEMORY;
    
    /* Detect format */
    int confidence = 0;
    internal_format_t fmt = (internal_format_t)uft_advanced_detect_format(path, &confidence);
    
    h->format_id = (int)fmt;
    h->detection_confidence = confidence;
    
    if (g_config.verbose_logging) {
        fprintf(stderr, "[UFT-ADV] Detected format %d with %d%% confidence\n", 
                (int)fmt, confidence);
    }
    
    /* Try v3 parser if enabled */
    uft_error_t err = UFT_ERR_FORMAT;
    
    if (g_config.flags & UFT_ADV_USE_V3_PARSERS) {
        switch (fmt) {
            case FMT_D64:
                err = uft_d64_v3_handler.open(path, &h->v3_handle);
                if (err == UFT_OK) {
                    h->using_v3 = true;
                    if (g_config.verbose_logging)
                        fprintf(stderr, "[UFT-ADV] Using D64 v3 parser\n");
                }
                break;
                
            case FMT_G64:
                err = uft_g64_v3_handler.open(path, &h->v3_handle);
                if (err == UFT_OK) {
                    h->using_v3 = true;
                    if (g_config.verbose_logging)
                        fprintf(stderr, "[UFT-ADV] Using G64 v3 parser\n");
                }
                break;
                
            case FMT_SCP:
                err = uft_scp_v3_handler.open(path, &h->v3_handle);
                if (err == UFT_OK) {
                    h->using_v3 = true;
                    if (g_config.verbose_logging)
                        fprintf(stderr, "[UFT-ADV] Using SCP v3 parser\n");
                }
                break;
                
            default:
                break;
        }
    }
    
    /* If v3 failed or not available, we still have the handle for metadata */
    if (err != UFT_OK) {
        h->using_v3 = false;
        /* Could fall back to standard parser here */
    }
    
    /* Auto-detect protection if enabled */
    if ((g_config.flags & UFT_ADV_AUTO_PROTECTION) && h->using_v3) {
        switch (fmt) {
            case FMT_D64:
                h->protection_detected = uft_d64_v3_detect_protection(
                    h->v3_handle, h->protection_name, sizeof(h->protection_name));
                break;
            case FMT_G64:
                h->protection_detected = uft_g64_v3_detect_protection(
                    h->v3_handle, h->protection_name, sizeof(h->protection_name));
                break;
            case FMT_SCP:
                h->protection_detected = uft_scp_v3_detect_protection(
                    h->v3_handle, h->protection_name, sizeof(h->protection_name));
                break;
            default:
                break;
        }
        
        if (h->protection_detected && g_config.verbose_logging) {
            fprintf(stderr, "[UFT-ADV] Protection detected: %s\n", h->protection_name);
        }
    }
    
    *out_handle = h;
    return UFT_OK;
}

void uft_advanced_close(uft_advanced_handle_t* handle) {
    if (!handle) return;
    
    if (handle->using_v3 && handle->v3_handle) {
        switch ((internal_format_t)handle->format_id) {
            case FMT_D64:
                uft_d64_v3_handler.close(handle->v3_handle);
                break;
            case FMT_G64:
                uft_g64_v3_handler.close(handle->v3_handle);
                break;
            case FMT_SCP:
                uft_scp_v3_handler.close(handle->v3_handle);
                break;
            default:
                break;
        }
    }
    
    free(handle);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * TRACK OPERATIONS WITH GOD-MODE
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_advanced_get_track_quality(uft_advanced_handle_t* handle,
                                           int cylinder, int head,
                                           uft_track_quality_t* quality) {
    if (!handle || !quality) return UFT_ERR_INVALID_ARG;
    
    memset(quality, 0, sizeof(uft_track_quality_t));
    quality->cylinder = cylinder;
    quality->head = head;
    quality->quality = 1.0;  /* Default to good */
    
    /* TODO: Actually analyze track quality from v3 parser */
    /* For now, return simulated values */
    
    return UFT_OK;
}

uft_error_t uft_advanced_read_track(uft_advanced_handle_t* handle,
                                    int cylinder, int head,
                                    uint8_t* buffer, size_t* size,
                                    uft_track_quality_t* quality) {
    if (!handle || !buffer || !size) return UFT_ERR_INVALID_ARG;
    
    /* Get track quality first */
    uft_track_quality_t q;
    uft_advanced_get_track_quality(handle, cylinder, head, &q);
    
    /* Check if God-Mode should be engaged */
    bool use_god_mode = (g_config.flags & UFT_ADV_GOD_MODE) &&
                        (q.quality * 100 < g_config.quality_threshold);
    
    if (use_god_mode) {
        handle->god_mode_active = true;
        
        if (g_config.verbose_logging) {
            fprintf(stderr, "[UFT-ADV] God-Mode engaged for track %d/%d (quality: %.1f%%)\n",
                    cylinder, head, q.quality * 100);
        }
        
        /* Apply God-Mode algorithms */
        if (g_config.flags & UFT_ADV_KALMAN_PLL) {
            /* Use Kalman PLL for timing recovery */
            uft_kalman_config_t kcfg;
            uft_kalman_state_t kstate;
            uft_kalman_config_init(&kcfg, UFT_ENCODING_GCR_C64);
            uft_kalman_init(&kstate, &kcfg);
            /* ... process flux data through Kalman ... */
        }
        
        if (g_config.flags & UFT_ADV_CRC_CORRECTION) {
            /* Attempt CRC correction on bad sectors */
            /* ... */
        }
        
        q.god_mode_used = true;
    }
    
    if (quality) *quality = q;
    
    /* TODO: Actually read track data */
    *size = 0;
    
    return UFT_OK;
}

uft_error_t uft_advanced_read_sector(uft_advanced_handle_t* handle,
                                     int cylinder, int head, int sector,
                                     uint8_t* buffer, size_t* size) {
    (void)handle; (void)cylinder; (void)head; (void)sector;
    (void)buffer; (void)size;
    /* TODO: Implement sector read with error recovery */
    return UFT_ERR_NOT_IMPLEMENTED;
}

uft_error_t uft_advanced_analyze_disk(uft_advanced_handle_t* handle,
                                      uft_track_quality_t* qualities,
                                      int* track_count) {
    if (!handle) return UFT_ERR_INVALID_ARG;
    
    /* Get geometry */
    int cyls = 35, heads = 1;
    if (handle->using_v3) {
        switch ((internal_format_t)handle->format_id) {
            case FMT_D64:
                uft_d64_v3_handler.get_geometry(handle->v3_handle, &cyls, &heads, NULL);
                break;
            case FMT_G64:
                uft_g64_v3_handler.get_geometry(handle->v3_handle, &cyls, &heads, NULL);
                break;
            case FMT_SCP:
                uft_scp_v3_handler.get_geometry(handle->v3_handle, &cyls, &heads, NULL);
                break;
            default:
                break;
        }
    }
    
    int total = cyls * heads;
    if (track_count) *track_count = total;
    
    if (qualities) {
        for (int c = 0; c < cyls; c++) {
            for (int h = 0; h < heads; h++) {
                int idx = c * heads + h;
                uft_advanced_get_track_quality(handle, c, h, &qualities[idx]);
            }
        }
    }
    
    return UFT_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * CONVENIENCE FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool uft_advanced_detect_protection(const char* path, char* name, size_t name_size) {
    uft_advanced_handle_t* h = NULL;
    
    /* Temporarily ensure protection detection is on */
    uint32_t old_flags = g_config.flags;
    g_config.flags |= UFT_ADV_AUTO_PROTECTION | UFT_ADV_USE_V3_PARSERS;
    
    if (uft_advanced_open(path, &h) != UFT_OK) {
        g_config.flags = old_flags;
        return false;
    }
    
    bool detected = h->protection_detected;
    if (detected && name && name_size > 0) {
        strncpy(name, h->protection_name, name_size - 1);
        name[name_size - 1] = '\0';
    }
    
    uft_advanced_close(h);
    g_config.flags = old_flags;
    
    return detected;
}

void uft_advanced_get_stats(uft_advanced_handle_t* handle, uft_advanced_stats_t* stats) {
    if (!handle || !stats) return;
    
    memset(stats, 0, sizeof(uft_advanced_stats_t));
    
    stats->weak_tracks = handle->weak_track_count;
    stats->recovered_sectors = handle->recovered_sector_count;
    
    /* Analyze disk for full stats */
    int track_count = 0;
    uft_advanced_analyze_disk(handle, NULL, &track_count);
    stats->total_tracks = track_count;
    
    /* TODO: Calculate actual stats from track analysis */
    stats->average_quality = 0.95;  /* Placeholder */
}

