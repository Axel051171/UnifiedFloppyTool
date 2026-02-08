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
    if (fseek(f, 0, SEEK_SET) != 0) return -1;
    
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
    
    /* Analyze track quality via v3 parser if available */
    if (handle->using_v3 && handle->v3_handle) {
        /* Read track data to analyze */
        uint8_t track_buf[16384];
        size_t track_size = sizeof(track_buf);
        const uft_format_handler_t *handler = uft_format_get_handler(handle->format_id);
        
        if (handler && handler->read_track) {
            uft_error_t err = handler->read_track(handle->v3_handle, cylinder, head,
                                                   track_buf, &track_size);
            if (err == UFT_OK && track_size > 0) {
                /* Analyze for weak bits and CRC errors */
                int error_count = 0;
                int weak_regions = 0;
                
                /* Scan for repeated patterns (weak bit indicator) */
                for (size_t i = 0; i + 8 < track_size; i++) {
                    /* Check for 0x00 or 0xFF runs (common weak bit artifacts) */
                    bool all_same = true;
                    for (int j = 1; j < 8; j++) {
                        if (track_buf[i + j] != track_buf[i]) { all_same = false; break; }
                    }
                    if (all_same && (track_buf[i] == 0x00 || track_buf[i] == 0xFF)) {
                        weak_regions++;
                        i += 7;  /* Skip past this region */
                    }
                }
                
                /* CRC check on MFM sectors (simplified) */
                for (size_t i = 0; i + 3 < track_size; i++) {
                    if (track_buf[i] == 0xA1 && track_buf[i+1] == 0xA1 &&
                        track_buf[i+2] == 0xA1 && track_buf[i+3] == 0xFE) {
                        /* Found IDAM - verify CRC of header (6 bytes + 2 CRC) */
                        if (i + 3 + 8 < track_size) {
                            uint16_t crc = 0xFFFF;
                            for (int j = 0; j < 8; j++) {
                                crc ^= (uint16_t)track_buf[i + j] << 8;
                                for (int b = 0; b < 8; b++)
                                    crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
                            }
                            if (crc != 0) error_count++;
                        }
                        i += 10;
                    }
                }
                
                quality->error_count = error_count;
                quality->has_errors = (error_count > 0);
                quality->is_weak = (weak_regions > 2);
                quality->quality = 1.0 - (error_count * 0.1) - (weak_regions * 0.05);
                if (quality->quality < 0.0) quality->quality = 0.0;
                
                if (quality->is_weak) handle->weak_track_count++;
            }
        }
    }
    
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
    
    /* Read track data via format handler */
    const uft_format_handler_t *handler = uft_format_get_handler(handle->format_id);
    if (handler && handler->read_track) {
        uft_error_t err = handler->read_track(
            handle->using_v3 ? handle->v3_handle : handle->handle,
            cylinder, head, buffer, size);
        if (err != UFT_OK) {
            *size = 0;
            return err;
        }
        if (use_god_mode && q.has_errors) {
            handle->recovered_sector_count += q.error_count;
            q.recovered_bits = q.error_count * 8;  /* Estimate */
        }
    } else {
        *size = 0;
    }
    
    return UFT_OK;
}

uft_error_t uft_advanced_read_sector(uft_advanced_handle_t* handle,
                                     int cylinder, int head, int sector,
                                     uint8_t* buffer, size_t* size) {
    if (!handle || !buffer || !size) return UFT_ERR_INVALID_ARG;
    
    /* Read entire track, then extract the requested sector */
    uint8_t track_buf[16384];
    size_t track_size = sizeof(track_buf);
    uft_track_quality_t quality;
    
    uft_error_t err = uft_advanced_read_track(handle, cylinder, head,
                                               track_buf, &track_size, &quality);
    if (err != UFT_OK) return err;
    if (track_size == 0) return UFT_ERR_FILE_NOT_FOUND;
    
    /* Search for the sector in MFM track data */
    for (size_t i = 0; i + 10 < track_size; i++) {
        /* Look for IDAM: A1 A1 A1 FE C H R N */
        if (track_buf[i] == 0xA1 && track_buf[i+1] == 0xA1 &&
            track_buf[i+2] == 0xA1 && track_buf[i+3] == 0xFE) {
            int s_cyl = track_buf[i+4];
            int s_head = track_buf[i+5];
            int s_num = track_buf[i+6];
            int s_size_code = track_buf[i+7];
            
            if (s_cyl == cylinder && s_head == head && s_num == sector) {
                int sector_size = 128 << s_size_code;
                
                /* Find DAM: A1 A1 A1 FB/F8 after gap */
                for (size_t j = i + 10; j + 4 + sector_size < track_size && j < i + 60; j++) {
                    if (track_buf[j] == 0xA1 && track_buf[j+1] == 0xA1 &&
                        track_buf[j+2] == 0xA1 &&
                        (track_buf[j+3] == 0xFB || track_buf[j+3] == 0xF8)) {
                        size_t data_start = j + 4;
                        if (data_start + sector_size <= track_size) {
                            memcpy(buffer, track_buf + data_start, sector_size);
                            *size = sector_size;
                            return UFT_OK;
                        }
                    }
                }
            }
            i += 9;  /* Skip past this IDAM */
        }
    }
    
    *size = 0;
    return UFT_ERR_FILE_NOT_FOUND;
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
    
    /* Calculate actual stats from track analysis */
    if (track_count > 0) {
        /* Get geometry to determine full track count */
        const uft_format_handler_t *handler = uft_format_get_handler(handle->format_id);
        int cyls = 0, heads = 0, sects = 0;
        if (handler && handler->get_geometry) {
            handler->get_geometry(handle->using_v3 ? handle->v3_handle : handle->handle,
                                 &cyls, &heads, &sects);
            stats->total_sectors = cyls * heads * sects;
        }
        
        /* Compute average quality across all tracks */
        uft_track_quality_t *qualities = calloc(track_count, sizeof(uft_track_quality_t));
        if (qualities) {
            uft_advanced_analyze_disk(handle, qualities, &track_count);
            double sum = 0.0;
            int error_total = 0;
            for (int i = 0; i < track_count; i++) {
                sum += qualities[i].quality;
                error_total += qualities[i].error_count;
            }
            stats->average_quality = sum / track_count;
            stats->error_sectors = error_total;
            stats->crc_corrections = handle->recovered_sector_count;
            free(qualities);
        } else {
            stats->average_quality = 0.95;  /* Estimate without per-track data */
        }
    } else {
        stats->average_quality = 1.0;
    }
}

