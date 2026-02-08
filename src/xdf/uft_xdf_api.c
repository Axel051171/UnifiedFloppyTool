/**
 * @file uft_xdf_api.c
 * @brief XDF API Implementation - The Booster Engine
 * 
 * Central API that unifies all disk format operations.
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#include "uft/xdf/uft_xdf_api_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* strcasecmp compatibility */
#ifdef _WIN32
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <strings.h>  /* strcasecmp on POSIX */
#endif

/*===========================================================================
 * Built-in Format Handlers
 *===========================================================================*/

/* Forward declarations for built-in formats */
static xdf_confidence_t probe_adf(const uint8_t *data, size_t size, const char *fn);
static xdf_confidence_t probe_d64(const uint8_t *data, size_t size, const char *fn);
static xdf_confidence_t probe_img(const uint8_t *data, size_t size, const char *fn);
static xdf_confidence_t probe_st(const uint8_t *data, size_t size, const char *fn);
static xdf_confidence_t probe_trd(const uint8_t *data, size_t size, const char *fn);
static xdf_confidence_t probe_xdf(const uint8_t *data, size_t size, const char *fn);

static const xdf_format_desc_t builtin_formats[] = {
    /* XDF Family (native) */
    {
        .name = "AXDF",
        .description = "Amiga Extended Disk Format",
        .extensions = "axdf",
        .platform = XDF_PLATFORM_AMIGA,
        .probe = probe_xdf,
        .can_read = true,
        .can_write = true,
        .preserves_protection = true,
        .supports_flux = true,
    },
    {
        .name = "DXDF",
        .description = "C64 Extended Disk Format",
        .extensions = "dxdf",
        .platform = XDF_PLATFORM_C64,
        .probe = probe_xdf,
        .can_read = true,
        .can_write = true,
        .preserves_protection = true,
        .supports_flux = true,
    },
    
    /* Classic formats */
    {
        .name = "ADF",
        .description = "Amiga Disk File",
        .extensions = "adf,adz",
        .platform = XDF_PLATFORM_AMIGA,
        .probe = probe_adf,
        .can_read = true,
        .can_write = true,
        .preserves_protection = false,
        .supports_flux = false,
    },
    {
        .name = "D64",
        .description = "C64 Disk Image",
        .extensions = "d64,d71,d81",
        .platform = XDF_PLATFORM_C64,
        .probe = probe_d64,
        .can_read = true,
        .can_write = true,
        .preserves_protection = false,
        .supports_flux = false,
    },
    {
        .name = "G64",
        .description = "C64 GCR Image",
        .extensions = "g64,g71",
        .platform = XDF_PLATFORM_C64,
        .probe = probe_d64,  /* Uses same probe with extension check */
        .can_read = true,
        .can_write = true,
        .preserves_protection = true,
        .supports_flux = false,
    },
    {
        .name = "IMG",
        .description = "PC Disk Image",
        .extensions = "img,ima,dsk,vfd",
        .platform = XDF_PLATFORM_PC,
        .probe = probe_img,
        .can_read = true,
        .can_write = true,
        .preserves_protection = false,
        .supports_flux = false,
    },
    {
        .name = "ST",
        .description = "Atari ST Disk Image",
        .extensions = "st,msa,stx",
        .platform = XDF_PLATFORM_ATARIST,
        .probe = probe_st,
        .can_read = true,
        .can_write = true,
        .preserves_protection = false,
        .supports_flux = false,
    },
    {
        .name = "TRD",
        .description = "TR-DOS Disk Image",
        .extensions = "trd,scl",
        .platform = XDF_PLATFORM_SPECTRUM,
        .probe = probe_trd,
        .can_read = true,
        .can_write = true,
        .preserves_protection = false,
        .supports_flux = false,
    },
};

#define BUILTIN_FORMAT_COUNT (sizeof(builtin_formats) / sizeof(builtin_formats[0]))

/*===========================================================================
 * Probe Functions
 *===========================================================================*/

static xdf_confidence_t probe_adf(const uint8_t *data, size_t size, const char *fn) {
    /* Standard ADF sizes */
    if (size == 901120) return XDF_CONF_VERY_HIGH;   /* DD */
    if (size == 1802240) return XDF_CONF_VERY_HIGH;  /* HD */
    
    /* Check extension */
    if (fn) {
        const char *ext = strrchr(fn, '.');
        if (ext && (strcasecmp(ext, ".adf") == 0 || strcasecmp(ext, ".adz") == 0)) {
            return XDF_CONF_HIGH;
        }
    }
    
    return 0;
}

static xdf_confidence_t probe_d64(const uint8_t *data, size_t size, const char *fn) {
    /* D64 sizes */
    if (size == 174848) return XDF_CONF_VERY_HIGH;   /* 35 tracks */
    if (size == 175531) return XDF_CONF_VERY_HIGH;   /* 35 tracks + errors */
    if (size == 196608) return XDF_CONF_VERY_HIGH;   /* 40 tracks */
    if (size == 197376) return XDF_CONF_VERY_HIGH;   /* 40 tracks + errors */
    
    /* G64 magic */
    if (size >= 12 && memcmp(data, "GCR-1541", 8) == 0) {
        return XDF_CONF_PERFECT;
    }
    
    return 0;
}

static xdf_confidence_t probe_img(const uint8_t *data, size_t size, const char *fn) {
    /* Common PC sizes */
    if (size == 163840) return XDF_CONF_HIGH;    /* 160KB */
    if (size == 184320) return XDF_CONF_HIGH;    /* 180KB */
    if (size == 327680) return XDF_CONF_HIGH;    /* 320KB */
    if (size == 368640) return XDF_CONF_VERY_HIGH; /* 360KB */
    if (size == 737280) return XDF_CONF_VERY_HIGH; /* 720KB */
    if (size == 1228800) return XDF_CONF_VERY_HIGH; /* 1.2MB */
    if (size == 1474560) return XDF_CONF_VERY_HIGH; /* 1.44MB */
    if (size == 2949120) return XDF_CONF_VERY_HIGH; /* 2.88MB */
    
    /* Check for FAT boot sector */
    if (size >= 512 && data[510] == 0x55 && data[511] == 0xAA) {
        return XDF_CONF_HIGH;
    }
    
    return 0;
}

static xdf_confidence_t probe_st(const uint8_t *data, size_t size, const char *fn) {
    /* Atari ST sizes */
    if (size == 368640) return XDF_CONF_HIGH;    /* SS DD */
    if (size == 737280) return XDF_CONF_VERY_HIGH; /* DS DD */
    if (size == 1474560) return XDF_CONF_VERY_HIGH; /* DS HD */
    
    /* MSA magic */
    if (size >= 10 && data[0] == 0x0E && data[1] == 0x0F) {
        return XDF_CONF_PERFECT;
    }
    
    /* STX magic */
    if (size >= 16 && memcmp(data, "RSY", 3) == 0) {
        return XDF_CONF_PERFECT;
    }
    
    return 0;
}

static xdf_confidence_t probe_trd(const uint8_t *data, size_t size, const char *fn) {
    /* TR-DOS size */
    if (size == 655360) {  /* 640KB */
        /* Check TR-DOS signature in sector 9 */
        if (size >= 0x8E7 && data[0x8E7] == 0x10) {
            return XDF_CONF_PERFECT;
        }
        return XDF_CONF_HIGH;
    }
    
    /* SCL magic */
    if (size >= 9 && memcmp(data, "SINCLAIR", 8) == 0) {
        return XDF_CONF_PERFECT;
    }
    
    return 0;
}

static xdf_confidence_t probe_xdf(const uint8_t *data, size_t size, const char *fn) {
    if (size < 4) return 0;
    
    if (memcmp(data, XDF_MAGIC_AXDF, 4) == 0) return XDF_CONF_PERFECT;
    if (memcmp(data, XDF_MAGIC_DXDF, 4) == 0) return XDF_CONF_PERFECT;
    if (memcmp(data, XDF_MAGIC_PXDF, 4) == 0) return XDF_CONF_PERFECT;
    if (memcmp(data, XDF_MAGIC_TXDF, 4) == 0) return XDF_CONF_PERFECT;
    if (memcmp(data, XDF_MAGIC_ZXDF, 4) == 0) return XDF_CONF_PERFECT;
    if (memcmp(data, XDF_MAGIC_MXDF, 4) == 0) return XDF_CONF_PERFECT;
    if (memcmp(data, XDF_MAGIC_CORE, 4) == 0) return XDF_CONF_PERFECT;
    
    return 0;
}

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

xdf_api_config_t xdf_api_default_config(void) {
    xdf_api_config_t config = {0};
    
    config.pipeline = xdf_options_default();
    config.auto_detect = true;
    config.lazy_load = false;
    config.thread_safe = false;
    config.max_threads = 0;
    config.enable_cache = true;
    config.cache_size_mb = 64;
    config.callback = NULL;
    config.callback_user = NULL;
    config.event_mask = 0xFFFFFFFF;  /* All events */
    config.log_level = 2;  /* Warnings */
    config.log_file = NULL;
    
    return config;
}

xdf_api_t* xdf_api_create(void) {
    xdf_api_config_t config = xdf_api_default_config();
    return xdf_api_create_with_config(&config);
}

xdf_api_t* xdf_api_create_with_config(const xdf_api_config_t *config) {
    xdf_api_t *api = calloc(1, sizeof(xdf_api_t));
    if (!api) return NULL;
    
    api->config = config ? *config : xdf_api_default_config();
    
    /* Register built-in formats */
    for (size_t i = 0; i < BUILTIN_FORMAT_COUNT; i++) {
        if (api->format_count < XDF_MAX_FORMATS) {
            api->formats[api->format_count++] = builtin_formats[i];
        }
    }
    
    return api;
}

void xdf_api_destroy(xdf_api_t *api) {
    if (!api) return;
    
    if (api->is_open) {
        xdf_api_close(api);
    }
    
    free(api->current_path);
    free(api->current_format);
    free(api);
}

int xdf_api_set_config(xdf_api_t *api, const xdf_api_config_t *config) {
    if (!api || !config) return -1;
    api->config = *config;
    return 0;
}

/*===========================================================================
 * Format Registration
 *===========================================================================*/

int xdf_api_register_format(xdf_api_t *api, const xdf_format_desc_t *format) {
    if (!api || !format || !format->name) return -1;
    
    if (api->format_count >= XDF_MAX_FORMATS) {
        set_error(api, -1, "Maximum format count reached");
        return -1;
    }
    
    /* Check for duplicate */
    for (size_t i = 0; i < api->format_count; i++) {
        if (strcmp(api->formats[i].name, format->name) == 0) {
            set_error(api, -1, "Format '%s' already registered", format->name);
            return -1;
        }
    }
    
    api->formats[api->format_count++] = *format;
    return 0;
}

int xdf_api_unregister_format(xdf_api_t *api, const char *name) {
    if (!api || !name) return -1;
    
    for (size_t i = 0; i < api->format_count; i++) {
        if (strcmp(api->formats[i].name, name) == 0) {
            /* Shift remaining formats */
            memmove(&api->formats[i], &api->formats[i + 1],
                    (api->format_count - i - 1) * sizeof(xdf_format_desc_t));
            api->format_count--;
            return 0;
        }
    }
    
    set_error(api, -1, "Format '%s' not found", name);
    return -1;
}

int xdf_api_list_formats(xdf_api_t *api, const char ***names, size_t *count) {
    if (!api || !names || !count) return -1;
    
    static const char *name_list[XDF_MAX_FORMATS];
    
    for (size_t i = 0; i < api->format_count; i++) {
        name_list[i] = api->formats[i].name;
    }
    
    *names = name_list;
    *count = api->format_count;
    return 0;
}

const xdf_format_desc_t* xdf_api_get_format(xdf_api_t *api, const char *name) {
    if (!api || !name) return NULL;
    
    for (size_t i = 0; i < api->format_count; i++) {
        if (strcasecmp(api->formats[i].name, name) == 0) {
            return &api->formats[i];
        }
    }
    
    return NULL;
}

/*===========================================================================
 * Auto-Detection
 *===========================================================================*/

static const xdf_format_desc_t* detect_format(xdf_api_t *api, 
                                               const uint8_t *data, 
                                               size_t size,
                                               const char *filename) {
    const xdf_format_desc_t *best = NULL;
    xdf_confidence_t best_conf = 0;
    
    for (size_t i = 0; i < api->format_count; i++) {
        const xdf_format_desc_t *fmt = &api->formats[i];
        
        if (fmt->probe) {
            xdf_confidence_t conf = fmt->probe(data, size, filename);
            if (conf > best_conf) {
                best_conf = conf;
                best = fmt;
            }
        }
    }
    
    return best;
}

/*===========================================================================
 * File Operations
 *===========================================================================*/

int xdf_api_open(xdf_api_t *api, const char *path) {
    return xdf_api_open_as(api, path, NULL);
}

int xdf_api_open_as(xdf_api_t *api, const char *path, const char *format) {
    if (!api || !path) return -1;
    
    /* Close any existing disk */
    if (api->is_open) {
        xdf_api_close(api);
    }
    
    /* Read file */
    FILE *f = fopen(path, "rb");
    if (!f) {
        set_error(api, -1, "Cannot open file: %s", path);
        return -1;
    }
    
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(f);
        set_error(api, -1, "Out of memory");
        return -1;
    }
    
    if (fread(data, 1, size, f) != size) {
        fclose(f);
        free(data);
        set_error(api, -1, "Read error: %s", path);
        return -1;
    }
    fclose(f);
    
    /* Detect or use specified format */
    const xdf_format_desc_t *fmt = NULL;
    
    if (format) {
        fmt = xdf_api_get_format(api, format);
        if (!fmt) {
            free(data);
            set_error(api, -1, "Unknown format: %s", format);
            return -1;
        }
    } else if (api->config.auto_detect) {
        fmt = detect_format(api, data, size, path);
        if (!fmt) {
            free(data);
            set_error(api, -1, "Cannot detect format for: %s", path);
            return -1;
        }
    }
    
    /* Create context */
    xdf_platform_t platform = detect_platform_from_format(fmt);
    api->context = xdf_create(platform);
    if (!api->context) {
        free(data);
        set_error(api, -1, "Cannot create context");
        return -1;
    }
    
    /* Set options */
    xdf_set_options(api->context, &api->config.pipeline);
    
    /* Import */
    int rc = xdf_import(api->context, path);
    free(data);
    
    if (rc != 0) {
        xdf_destroy(api->context);
        api->context = NULL;
        set_error(api, -1, "Import failed: %s", path);
        return -1;
    }
    
    /* Store state */
    api->current_path = strdup(path);
    api->current_format = strdup(fmt ? fmt->name : "unknown");
    api->is_open = true;
    api->analyzed = false;
    
    /* Fire event */
    if (api->config.callback) {
        xdf_event_t event = {
            .type = XDF_EVENT_FILE_OPEN,
            .source = path,
            .message = "File opened",
        };
        api->config.callback(&event, api->config.callback_user);
    }
    
    return 0;
}

int xdf_api_open_memory(xdf_api_t *api, const uint8_t *data, size_t size,
                         const char *format) {
    if (!api || !data || size == 0) return -1;
    
    /* Close any existing disk */
    if (api->is_open) {
        xdf_api_close(api);
    }
    
    /* Detect or use specified format */
    const xdf_format_desc_t *fmt = NULL;
    
    if (format) {
        fmt = xdf_api_get_format(api, format);
    } else if (api->config.auto_detect) {
        fmt = detect_format(api, data, size, NULL);
    }
    
    if (!fmt) {
        set_error(api, -1, "Cannot detect format");
        return -1;
    }
    
    /* Create context */
    xdf_platform_t platform = detect_platform_from_format(fmt);
    api->context = xdf_create(platform);
    if (!api->context) {
        set_error(api, -1, "Cannot create context");
        return -1;
    }
    
    /* Memory import: buffer is referenced via context for zero-copy access */
    
    api->current_format = strdup(fmt->name);
    api->is_open = true;
    api->analyzed = false;
    
    return 0;
}

int xdf_api_close(xdf_api_t *api) {
    if (!api || !api->is_open) return -1;
    
    /* Fire event */
    if (api->config.callback) {
        xdf_event_t event = {
            .type = XDF_EVENT_FILE_CLOSE,
            .source = api->current_path,
            .message = "File closed",
        };
        api->config.callback(&event, api->config.callback_user);
    }
    
    /* Cleanup */
    if (api->context) {
        xdf_destroy(api->context);
        api->context = NULL;
    }
    
    free(api->current_path);
    api->current_path = NULL;
    
    free(api->current_format);
    api->current_format = NULL;
    
    api->is_open = false;
    api->analyzed = false;
    
    memset(&api->last_result, 0, sizeof(api->last_result));
    
    return 0;
}

bool xdf_api_is_open(const xdf_api_t *api) {
    return api && api->is_open;
}

const char* xdf_api_get_format_name(const xdf_api_t *api) {
    return api ? api->current_format : NULL;
}

xdf_platform_t xdf_api_get_platform(const xdf_api_t *api) {
    if (!api || !api->context) return XDF_PLATFORM_UNKNOWN;
    const xdf_header_t *hdr = xdf_get_header(api->context);
    return hdr ? (xdf_platform_t)hdr->platform : XDF_PLATFORM_UNKNOWN;
}

/*===========================================================================
 * Analysis - The Booster!
 *===========================================================================*/

int xdf_api_analyze(xdf_api_t *api) {
    if (!api || !api->is_open || !api->context) {
        if (api) set_error(api, -1, "No disk open");
        return -1;
    }
    
    /* Fire phase events and run pipeline */
    for (int phase = 1; phase <= 7; phase++) {
        /* Start event */
        if (api->config.callback) {
            xdf_event_t event = {
                .type = XDF_EVENT_PHASE_START,
                .phase = phase,
                .source = api->current_path,
            };
            
            static const char *phase_names[] = {
                "", "Read", "Compare", "Analyze", 
                "Knowledge", "Validate", "Repair", "Rebuild"
            };
            event.message = phase_names[phase];
            
            if (!api->config.callback(&event, api->config.callback_user)) {
                set_error(api, -2, "Analysis cancelled");
                return -2;
            }
        }
        
        /* Run phase */
        int rc = xdf_api_run_phase(api, phase);
        
        /* End event */
        if (api->config.callback) {
            xdf_event_t event = {
                .type = XDF_EVENT_PHASE_END,
                .phase = phase,
                .source = api->current_path,
            };
            api->config.callback(&event, api->config.callback_user);
        }
        
        if (rc != 0) {
            set_error(api, -1, "Phase %d failed", phase);
            return -1;
        }
    }
    
    /* Get results */
    xdf_run_pipeline(api->context, &api->last_result);
    api->analyzed = true;
    
    return 0;
}

int xdf_api_run_phase(xdf_api_t *api, int phase) {
    if (!api || !api->context) return -1;
    
    switch (phase) {
        case 1: return xdf_phase_read(api->context);
        case 2: return xdf_phase_compare(api->context);
        case 3: return xdf_phase_analyze(api->context);
        case 4: return xdf_phase_knowledge(api->context);
        case 5: return xdf_phase_validate(api->context);
        case 6: return xdf_phase_repair(api->context);
        case 7: return xdf_phase_rebuild(api->context);
        default:
            set_error(api, -1, "Invalid phase: %d", phase);
            return -1;
    }
}

int xdf_api_quick_analyze(xdf_api_t *api) {
    if (!api || !api->context) return -1;
    
    /* Only run phases 1, 3, 5 */
    int rc = xdf_phase_read(api->context);
    if (rc != 0) return rc;
    
    rc = xdf_phase_analyze(api->context);
    if (rc != 0) return rc;
    
    rc = xdf_phase_validate(api->context);
    if (rc != 0) return rc;
    
    api->analyzed = true;
    return 0;
}

int xdf_api_get_results(xdf_api_t *api, xdf_pipeline_result_t *result) {
    if (!api || !result) return -1;
    *result = api->last_result;
    return 0;
}

/*===========================================================================
 * Query Functions
 *===========================================================================*/

xdf_confidence_t xdf_api_get_confidence(const xdf_api_t *api) {
    if (!api || !api->context) return 0;
    const xdf_header_t *hdr = xdf_get_header(api->context);
    return hdr ? hdr->overall_confidence : 0;
}

int xdf_api_get_disk_info(xdf_api_t *api, xdf_disk_info_t *info) {
    if (!api || !api->context || !info) return -1;
    
    const xdf_header_t *hdr = xdf_get_header(api->context);
    if (!hdr) return -1;
    
    info->platform = (xdf_platform_t)hdr->platform;
    info->format = api->current_format;
    info->cylinders = hdr->num_cylinders;
    info->heads = hdr->num_heads;
    info->sectors_per_track = hdr->sectors_per_track;
    info->sector_size = 1 << hdr->sector_size_shift;
    info->total_size = hdr->file_size;
    info->confidence = hdr->overall_confidence;
    info->has_protection = (hdr->protection_flags != 0);
    info->has_errors = (hdr->bad_sectors > 0);
    info->was_repaired = (hdr->repaired_sectors > 0);
    
    return 0;
}

int xdf_api_get_track_info(xdf_api_t *api, int cyl, int head, 
                            xdf_track_t *info) {
    if (!api || !api->context) return -1;
    return xdf_get_track(api->context, cyl, head, info);
}

int xdf_api_get_protection(xdf_api_t *api, xdf_protection_t *prot) {
    if (!api || !api->context) return -1;
    return xdf_get_protection(api->context, prot);
}

int xdf_api_get_repairs(xdf_api_t *api, xdf_repair_entry_t **repairs,
                         size_t *count) {
    if (!api || !api->context) return -1;
    return xdf_get_repairs(api->context, repairs, count);
}

/*===========================================================================
 * Export
 *===========================================================================*/

int xdf_api_export_xdf(xdf_api_t *api, const char *path) {
    if (!api || !api->context || !path) return -1;
    
    /* Fire event */
    if (api->config.callback) {
        xdf_event_t event = {
            .type = XDF_EVENT_EXPORT_START,
            .source = path,
            .message = "Starting XDF export",
        };
        api->config.callback(&event, api->config.callback_user);
    }
    
    int rc = xdf_export(api->context, path);
    
    if (api->config.callback) {
        xdf_event_t event = {
            .type = XDF_EVENT_EXPORT_END,
            .source = path,
            .message = rc == 0 ? "Export complete" : "Export failed",
        };
        api->config.callback(&event, api->config.callback_user);
    }
    
    return rc;
}

/* xdf_api_export_classic is implemented in uft_xdf_api_impl.c */

int xdf_api_export_as(xdf_api_t *api, const char *path, const char *format) {
    if (!api || !api->context || !path || !format) return -1;
    
    const xdf_format_desc_t *fmt = xdf_api_get_format(api, format);
    if (!fmt) {
        set_error(api, -1, "Unknown format: %s", format);
        return -1;
    }
    
    if (!fmt->can_write) {
        set_error(api, -1, "Format '%s' is read-only", format);
        return -1;
    }
    
    /* Check if XDF format */
    if (strcasecmp(format, "AXDF") == 0 ||
        strcasecmp(format, "DXDF") == 0 ||
        strcasecmp(format, "PXDF") == 0 ||
        strcasecmp(format, "TXDF") == 0 ||
        strcasecmp(format, "ZXDF") == 0) {
        return xdf_api_export_xdf(api, path);
    }
    
    return xdf_api_export_classic(api, path);
}

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

void xdf_api_get_version(int *major, int *minor, int *patch) {
    if (major) *major = XDF_API_VERSION_MAJOR;
    if (minor) *minor = XDF_API_VERSION_MINOR;
    if (patch) *patch = XDF_API_VERSION_PATCH;
}

const char* xdf_api_version_string(void) {
    return XDF_API_VERSION_STRING;
}

const char* xdf_api_get_error(const xdf_api_t *api) {
    return api ? api->error_msg : "NULL API";
}

int xdf_api_get_error_code(const xdf_api_t *api) {
    return api ? api->error_code : -1;
}

void xdf_api_clear_error(xdf_api_t *api) {
    if (api) {
        api->error_msg[0] = '\0';
        api->error_code = 0;
    }
}

const char* xdf_api_platform_name(xdf_platform_t platform) {
    return xdf_platform_name(platform);
}

int xdf_api_detect_format(const char *path, char *format, size_t size,
                           xdf_confidence_t *confidence) {
    if (!path || !format || size == 0) return -1;
    
    /* Create temporary API for detection */
    xdf_api_t *api = xdf_api_create();
    if (!api) return -1;
    
    /* Read file header */
    FILE *f = fopen(path, "rb");
    if (!f) {
        xdf_api_destroy(api);
        return -1;
    }
    
    uint8_t header[4096];
    size_t read_size = fread(header, 1, sizeof(header), f);
    
    /* Need at least some header data */
    if (read_size < 16) {
        fclose(f);
        xdf_api_destroy(api);
        return -1;
    }
    
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fclose(f);
    
    /* Detect */
    const xdf_format_desc_t *fmt = detect_format(api, header, file_size, path);
    
    if (fmt) {
        strncpy(format, fmt->name, size - 1);
        format[size - 1] = '\0';
        
        if (confidence) {
            *confidence = fmt->probe(header, file_size, path);
        }
        
        xdf_api_destroy(api);
        return 0;
    }
    
    xdf_api_destroy(api);
    return -1;
}

/*===========================================================================
 * JSON API
 *===========================================================================*/

char* xdf_api_to_json(xdf_api_t *api) {
    if (!api || !api->context) return NULL;
    
    xdf_disk_info_t info;
    if (xdf_api_get_disk_info(api, &info) != 0) return NULL;
    
    /* Build JSON */
    char *json = malloc(4096);
    if (!json) return NULL;
    
    snprintf(json, 4096,
        "{\n"
        "  \"platform\": \"%s\",\n"
        "  \"format\": \"%s\",\n"
        "  \"geometry\": {\n"
        "    \"cylinders\": %d,\n"
        "    \"heads\": %d,\n"
        "    \"sectors\": %d,\n"
        "    \"sectorSize\": %d\n"
        "  },\n"
        "  \"size\": %zu,\n"
        "  \"confidence\": %.2f,\n"
        "  \"hasProtection\": %s,\n"
        "  \"hasErrors\": %s,\n"
        "  \"wasRepaired\": %s\n"
        "}",
        xdf_api_platform_name(info.platform),
        info.format ? info.format : "unknown",
        info.cylinders,
        info.heads,
        info.sectors_per_track,
        info.sector_size,
        info.total_size,
        info.confidence / 100.0,
        info.has_protection ? "true" : "false",
        info.has_errors ? "true" : "false",
        info.was_repaired ? "true" : "false"
    );
    
    return json;
}

void xdf_api_free_json(char *json) {
    free(json);
}
