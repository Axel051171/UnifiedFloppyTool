/**
 * @file uft_smart_open.c
 * @brief UFT Smart Pipeline Implementation
 * 
 * "Bei uns geht kein Bit verloren"
 */

#include "uft/uft_smart_open.h"
#include "uft/uft_god_mode.h"
#include "uft/uft_v3_bridge.h"
#include "uft/uft_error.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Registry Probe Functions (uft_error_t signature)
 * ═══════════════════════════════════════════════════════════════════════════════ */

extern uft_error_t uft_d64_probe(const void* data, size_t size, int* confidence);
extern uft_error_t uft_g64_probe(const void* data, size_t size, int* confidence);
extern uft_error_t uft_scp_probe(const void* data, size_t size, int* confidence);
extern uft_error_t uft_hfe_probe(const void* data, size_t size, int* confidence);
extern uft_error_t uft_adf_probe(const void* data, size_t size, int* confidence);
extern uft_error_t uft_imd_probe(const void* data, size_t size, int* confidence);
extern uft_error_t uft_td0_probe(const void* data, size_t size, int* confidence);
extern uft_error_t uft_img_probe(const void* data, size_t size, int* confidence);

/* Standard Probe Functions (bool signature with file_size) */
extern bool d71_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);
extern bool d80_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);
extern bool d81_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);
extern bool d82_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);
extern bool g71_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);
extern bool atr_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);
extern bool dmk_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);
extern bool trd_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Unified Probe Function Type
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef bool (*unified_probe_fn)(const uint8_t* data, size_t size, size_t file_size, int* confidence);

/* Wrappers for registry probes */
static bool wrap_d64_probe(const uint8_t* d, size_t s, size_t fs, int* c) {
    (void)fs; return uft_d64_probe(d, s, c) == UFT_OK;
}
static bool wrap_g64_probe(const uint8_t* d, size_t s, size_t fs, int* c) {
    (void)fs; return uft_g64_probe(d, s, c) == UFT_OK;
}
static bool wrap_scp_probe(const uint8_t* d, size_t s, size_t fs, int* c) {
    (void)fs; return uft_scp_probe(d, s, c) == UFT_OK;
}
static bool wrap_hfe_probe(const uint8_t* d, size_t s, size_t fs, int* c) {
    (void)fs; return uft_hfe_probe(d, s, c) == UFT_OK;
}
static bool wrap_adf_probe(const uint8_t* d, size_t s, size_t fs, int* c) {
    (void)fs; return uft_adf_probe(d, s, c) == UFT_OK;
}
static bool wrap_imd_probe(const uint8_t* d, size_t s, size_t fs, int* c) {
    (void)fs; return uft_imd_probe(d, s, c) == UFT_OK;
}
static bool wrap_td0_probe(const uint8_t* d, size_t s, size_t fs, int* c) {
    (void)fs; return uft_td0_probe(d, s, c) == UFT_OK;
}
static bool wrap_img_probe(const uint8_t* d, size_t s, size_t fs, int* c) {
    (void)fs; return uft_img_probe(d, s, c) == UFT_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal State
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint8_t* data;
    size_t size;
    char path[1024];
    void* parser_handle;
    int format_id;
    bool is_v3;
} smart_internal_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Format Detection Table
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Format IDs */
#define FMT_D64     10
#define FMT_D71     11
#define FMT_D80     12
#define FMT_D81     13
#define FMT_D82     14
#define FMT_G64     20
#define FMT_G71     21
#define FMT_SCP     30
#define FMT_HFE     31
#define FMT_ADF     40
#define FMT_ATR     50
#define FMT_IMD     60
#define FMT_TD0     61
#define FMT_IMG     70
#define FMT_DMK     80
#define FMT_TRD     90

typedef struct {
    int format_id;
    const char* name;
    const char* extension;
    unified_probe_fn probe;
    uft_format_handler_t* v3_handler;
} format_entry_t;

/* External v3 handlers */
extern uft_format_handler_t uft_d64_v3_handler;
extern uft_format_handler_t uft_g64_v3_handler;
extern uft_format_handler_t uft_scp_v3_handler;

static const format_entry_t g_formats[] = {
    /* Commodore */
    { FMT_D64, "D64", "d64", wrap_d64_probe, &uft_d64_v3_handler },
    { FMT_D71, "D71", "d71", d71_probe, NULL },
    { FMT_D80, "D80", "d80", d80_probe, NULL },
    { FMT_D81, "D81", "d81", d81_probe, NULL },
    { FMT_D82, "D82", "d82", d82_probe, NULL },
    { FMT_G64, "G64", "g64", wrap_g64_probe, &uft_g64_v3_handler },
    { FMT_G71, "G71", "g71", g71_probe, NULL },
    
    /* Flux */
    { FMT_SCP, "SCP", "scp", wrap_scp_probe, &uft_scp_v3_handler },
    { FMT_HFE, "HFE", "hfe", wrap_hfe_probe, NULL },
    
    /* Amiga */
    { FMT_ADF, "ADF", "adf", wrap_adf_probe, NULL },
    
    /* Atari */
    { FMT_ATR, "ATR", "atr", atr_probe, NULL },
    
    /* PC */
    { FMT_IMD, "IMD", "imd", wrap_imd_probe, NULL },
    { FMT_TD0, "TD0", "td0", wrap_td0_probe, NULL },
    { FMT_IMG, "IMG", "img", wrap_img_probe, NULL },
    
    /* TRS-80 */
    { FMT_DMK, "DMK", "dmk", dmk_probe, NULL },
    
    /* ZX Spectrum */
    { FMT_TRD, "TRD", "trd", trd_probe, NULL },
    
    { 0, NULL, NULL, NULL, NULL }
};

#define NUM_FORMATS (sizeof(g_formats) / sizeof(g_formats[0]) - 1)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Initialization
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_smart_options_init(uft_smart_options_t* opts) {
    if (!opts) return;
    
    memset(opts, 0, sizeof(uft_smart_options_t));
    opts->use_bayesian_detect = true;
    opts->prefer_v3_parsers = true;
    opts->auto_detect_protection = true;
    opts->enable_god_mode = false;
    opts->enable_multi_rev_fusion = true;
    opts->enable_crc_correction = true;
    opts->strict_mode = false;
    opts->min_confidence = 70;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Format Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

static const format_entry_t* detect_format(const uint8_t* data, size_t size, 
                                           size_t file_size, int* confidence) {
    const format_entry_t* best = NULL;
    int best_conf = 0;
    
    for (size_t i = 0; i < NUM_FORMATS; i++) {
        if (!g_formats[i].probe) continue;
        
        int conf = 0;
        if (g_formats[i].probe(data, size, file_size, &conf)) {
            if (conf > best_conf) {
                best_conf = conf;
                best = &g_formats[i];
            }
        }
    }
    
    if (confidence) *confidence = best_conf;
    return best;
}

static const format_entry_t* detect_by_extension(const char* path) {
    const char* dot = strrchr(path, '.');
    if (!dot) return NULL;
    
    for (size_t i = 0; i < NUM_FORMATS; i++) {
        if (g_formats[i].extension && 
            strcasecmp(dot + 1, g_formats[i].extension) == 0) {
            return &g_formats[i];
        }
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Protection Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void detect_protection(smart_internal_t* internal, 
                              uft_protection_result_t* prot,
                              const format_entry_t* fmt) {
    memset(prot, 0, sizeof(uft_protection_result_t));
    
    if (!internal->is_v3 || !internal->parser_handle) return;
    
    char name[64] = {0};
    bool detected = false;
    
    if (fmt->format_id == FMT_D64) {
        detected = uft_d64_v3_detect_protection(internal->parser_handle, name, sizeof(name));
        if (detected) strncpy(prot->platform, "Commodore 64", sizeof(prot->platform) - 1);
    } else if (fmt->format_id == FMT_G64) {
        detected = uft_g64_v3_detect_protection(internal->parser_handle, name, sizeof(name));
        if (detected) strncpy(prot->platform, "Commodore 64", sizeof(prot->platform) - 1);
    } else if (fmt->format_id == FMT_SCP) {
        detected = uft_scp_v3_detect_protection(internal->parser_handle, name, sizeof(name));
        if (detected) strncpy(prot->platform, "Multi-Platform", sizeof(prot->platform) - 1);
    }
    
    if (detected) {
        prot->detected = true;
        strncpy(prot->scheme_name, name, sizeof(prot->scheme_name) - 1);
        prot->confidence = 80;
        prot->indicator_count = 1;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Quality Analysis
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void analyze_quality(smart_internal_t* internal,
                           uft_quality_result_t* quality,
                           const uft_smart_options_t* opts) {
    memset(quality, 0, sizeof(uft_quality_result_t));
    
    quality->level = UFT_QUALITY_GOOD;
    quality->readable_sectors = 100;
    quality->total_sectors = 100;
    
    if (opts->enable_god_mode && internal->data) {
        uft_decoder_metrics_t metrics;
        uft_calculate_metrics(internal->data, internal->size, 0, &metrics);
        
        quality->crc_errors = metrics.bad_checksums;
        quality->bit_error_rate = metrics.bit_error_rate;
        
        if (metrics.signal_quality >= 0.95) {
            quality->level = UFT_QUALITY_PERFECT;
        } else if (metrics.signal_quality >= 0.85) {
            quality->level = UFT_QUALITY_EXCELLENT;
        } else if (metrics.signal_quality >= 0.70) {
            quality->level = UFT_QUALITY_GOOD;
        } else if (metrics.signal_quality >= 0.50) {
            quality->level = UFT_QUALITY_FAIR;
        } else if (metrics.signal_quality >= 0.25) {
            quality->level = UFT_QUALITY_POOR;
        } else {
            quality->level = UFT_QUALITY_UNREADABLE;
        }
        
        quality->god_mode_used = true;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Main API
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_smart_open(const char* path, const uft_smart_options_t* opts,
                   uft_smart_result_t* result) {
    if (!path || !result) return -1;
    
    memset(result, 0, sizeof(uft_smart_result_t));
    
    uft_smart_options_t default_opts;
    if (!opts) {
        uft_smart_options_init(&default_opts);
        opts = &default_opts;
    }
    
    if (opts->progress_cb) {
        opts->progress_cb(0, "Opening file...", opts->user_data);
    }
    
    /* Open and read file */
    FILE* f = fopen(path, "rb");
    if (!f) {
        snprintf(result->error, sizeof(result->error), "Cannot open file: %s", path);
        return -1;
    }
    
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    size_t header_size = (file_size > 65536) ? 65536 : file_size;
    uint8_t* header = malloc(header_size);
    if (!header) {
        fclose(f);
        snprintf(result->error, sizeof(result->error), "Out of memory");
        return -1;
    }
    
    if (fread(header, 1, header_size, f) != header_size) {
        free(header);
        fclose(f);
        snprintf(result->error, sizeof(result->error), "Read error");
        return -1;
    }
    fclose(f);
    
    if (opts->progress_cb) {
        opts->progress_cb(20, "Detecting format...", opts->user_data);
    }
    
    /* Detect format */
    int confidence = 0;
    const format_entry_t* fmt = detect_format(header, header_size, file_size, &confidence);
    
    /* Fallback to extension */
    if (!fmt || confidence < opts->min_confidence) {
        const format_entry_t* ext_fmt = detect_by_extension(path);
        if (ext_fmt && (!fmt || confidence < 50)) {
            fmt = ext_fmt;
            confidence = 50;
        }
    }
    
    if (!fmt) {
        free(header);
        snprintf(result->error, sizeof(result->error), "Unknown format");
        return -1;
    }
    
    result->detection.format_id = fmt->format_id;
    result->detection.format_name = fmt->name;
    result->detection.confidence = confidence;
    
    if (opts->progress_cb) {
        opts->progress_cb(40, "Parsing disk image...", opts->user_data);
    }
    
    /* Create internal state */
    smart_internal_t* internal = calloc(1, sizeof(smart_internal_t));
    if (!internal) {
        free(header);
        snprintf(result->error, sizeof(result->error), "Out of memory");
        return -1;
    }
    
    internal->data = header;
    internal->size = header_size;
    strncpy(internal->path, path, sizeof(internal->path) - 1);
    internal->format_id = fmt->format_id;
    
    /* Open with v3 parser if available */
    if (opts->prefer_v3_parsers && fmt->v3_handler && fmt->v3_handler->open) {
        if (fmt->v3_handler->open(path, &internal->parser_handle) == UFT_OK) {
            internal->is_v3 = true;
            result->detection.using_v3_parser = true;
        }
    }
    
    if (opts->progress_cb) {
        opts->progress_cb(60, "Analyzing protection...", opts->user_data);
    }
    
    /* Protection detection */
    if (opts->auto_detect_protection) {
        detect_protection(internal, &result->protection, fmt);
        
        if (result->protection.detected) {
            snprintf(result->warnings + strlen(result->warnings),
                    sizeof(result->warnings) - strlen(result->warnings),
                    "Protection detected: %s (%s)\n",
                    result->protection.scheme_name,
                    result->protection.platform);
        }
    }
    
    if (opts->progress_cb) {
        opts->progress_cb(80, "Analyzing quality...", opts->user_data);
    }
    
    /* Quality analysis */
    analyze_quality(internal, &result->quality, opts);
    
    if (result->quality.level < UFT_QUALITY_GOOD) {
        snprintf(result->warnings + strlen(result->warnings),
                sizeof(result->warnings) - strlen(result->warnings),
                "Quality: %s\n", uft_quality_level_name(result->quality.level));
    }
    
    if (opts->progress_cb) {
        opts->progress_cb(100, "Done", opts->user_data);
    }
    
    result->handle = internal;
    return 0;
}

void uft_smart_close(uft_smart_result_t* result) {
    if (!result || !result->handle) return;
    
    smart_internal_t* internal = (smart_internal_t*)result->handle;
    
    if (internal->is_v3 && internal->parser_handle) {
        for (size_t i = 0; i < NUM_FORMATS; i++) {
            if (g_formats[i].format_id == internal->format_id && 
                g_formats[i].v3_handler && g_formats[i].v3_handler->close) {
                g_formats[i].v3_handler->close(internal->parser_handle);
                break;
            }
        }
    }
    
    free(internal->data);
    free(internal);
    result->handle = NULL;
}

int uft_smart_reanalyze(uft_smart_result_t* result, const uft_smart_options_t* opts) {
    if (!result || !result->handle) return -1;
    
    smart_internal_t* internal = (smart_internal_t*)result->handle;
    
    const format_entry_t* fmt = NULL;
    for (size_t i = 0; i < NUM_FORMATS; i++) {
        if (g_formats[i].format_id == internal->format_id) {
            fmt = &g_formats[i];
            break;
        }
    }
    
    if (!fmt) return -1;
    
    if (opts->auto_detect_protection) {
        detect_protection(internal, &result->protection, fmt);
    }
    
    analyze_quality(internal, &result->quality, opts);
    
    return 0;
}

const char* uft_quality_level_name(uft_quality_level_t level) {
    switch (level) {
        case UFT_QUALITY_PERFECT:    return "Perfect";
        case UFT_QUALITY_EXCELLENT:  return "Excellent";
        case UFT_QUALITY_GOOD:       return "Good";
        case UFT_QUALITY_FAIR:       return "Fair";
        case UFT_QUALITY_POOR:       return "Poor";
        case UFT_QUALITY_UNREADABLE: return "Unreadable";
        default:                     return "Unknown";
    }
}

char* uft_smart_report(const uft_smart_result_t* result) {
    if (!result) return NULL;
    
    char* report = malloc(4096);
    if (!report) return NULL;
    
    snprintf(report, 4096,
        "═══════════════════════════════════════════════════════════════\n"
        "                    UFT Smart Open Report\n"
        "═══════════════════════════════════════════════════════════════\n\n"
        "FORMAT DETECTION\n"
        "  Format:      %s\n"
        "  Confidence:  %d%%\n"
        "  v3 Parser:   %s\n\n"
        "PROTECTION ANALYSIS\n"
        "  Detected:    %s\n"
        "%s%s%s"
        "\nQUALITY ASSESSMENT\n"
        "  Level:       %s\n"
        "  Sectors:     %d / %d readable\n"
        "  CRC Errors:  %d (corrected: %d)\n"
        "  Weak Bits:   %d (resolved: %d)\n"
        "  God-Mode:    %s\n"
        "%s%s"
        "═══════════════════════════════════════════════════════════════\n",
        result->detection.format_name ? result->detection.format_name : "Unknown",
        result->detection.confidence,
        result->detection.using_v3_parser ? "Yes" : "No",
        result->protection.detected ? "Yes" : "No",
        result->protection.detected ? "  Scheme:      " : "",
        result->protection.detected ? result->protection.scheme_name : "",
        result->protection.detected ? "\n" : "",
        uft_quality_level_name(result->quality.level),
        result->quality.readable_sectors,
        result->quality.total_sectors,
        result->quality.crc_errors,
        result->quality.crc_corrected,
        result->quality.weak_bits_found,
        result->quality.weak_bits_resolved,
        result->quality.god_mode_used ? "Used" : "Not needed",
        result->warnings[0] ? "\nWARNINGS\n" : "",
        result->warnings[0] ? result->warnings : "");
    
    return report;
}

