/**
 * @file uft_presets.c
 * @brief Preset System - Save/Load Parameter-Konfigurationen
 * 
 * Features:
 * - Built-in Presets (read-only)
 * - User Presets (read/write)
 * - JSON-basierte Speicherung
 * - Preset-Kategorien
 */

#include "uft/uft_params.h"
#include "uft/core/uft_safe_parse.h"
#include <stdlib.h>
#include "uft/core/uft_safe_parse.h"
#include <string.h>
#include "uft/core/uft_safe_parse.h"
#include <stdio.h>
#include "uft/core/uft_safe_parse.h"
#include <sys/stat.h>
#include "uft/core/uft_safe_parse.h"
#include <dirent.h>
#include "uft/core/uft_safe_parse.h"
#include <errno.h>
#include "uft/core/uft_safe_parse.h"

// ============================================================================
// Preset Storage Paths
// ============================================================================

#define UFT_PRESET_DIR_BUILTIN  "/usr/share/uft/presets"
#define UFT_PRESET_DIR_USER     "~/.config/uft/presets"
#define UFT_PRESET_EXTENSION    ".uftpreset"
#define UFT_MAX_PRESETS         256
#define UFT_MAX_PRESET_NAME     64

// ============================================================================
// Preset Categories
// ============================================================================

typedef enum uft_preset_category {
    UFT_PRESET_CAT_GENERAL,
    UFT_PRESET_CAT_COMMODORE,
    UFT_PRESET_CAT_AMIGA,
    UFT_PRESET_CAT_APPLE,
    UFT_PRESET_CAT_IBM_PC,
    UFT_PRESET_CAT_ATARI,
    UFT_PRESET_CAT_PRESERVATION,
    UFT_PRESET_CAT_COPY_PROTECTION,
    UFT_PRESET_CAT_USER,
} uft_preset_category_t;

// ============================================================================
// Preset Structure
// ============================================================================

typedef struct uft_preset {
    char                    name[UFT_MAX_PRESET_NAME];
    char                    description[256];
    uft_preset_category_t   category;
    bool                    is_builtin;
    bool                    is_modified;
    uft_params_t            params;
} uft_preset_t;

// ============================================================================
// Built-in Presets
// ============================================================================

static const uft_preset_t g_builtin_presets[] = {
    // === COMMODORE ===
    {
        .name = "C64 1541 Standard",
        .description = "Standard C64 1541 disk read (35 tracks, GCR)",
        .category = UFT_PRESET_CAT_COMMODORE,
        .is_builtin = true,
        .params = {
            .struct_size = sizeof(uft_params_t),
            .version = 1,
            .global = { .global_retries = 3, .verify_after_write = true },
            .geometry = {
                .cylinder_start = 0, .cylinder_end = 34,
                .head_start = 0, .head_end = 0,
                .sector_size = 256, .total_cylinders = 35, .total_heads = 1,
            },
            .format = { .output_format = UFT_FORMAT_D64 },
            .decoder = {
                .encoding = UFT_ENCODING_GCR_CBM,
                .pll = { .initial_period_us = 3.5, .tolerance = 0.25 },
            },
        },
    },
    {
        .name = "C64 1541 Preservation",
        .description = "Full preservation with error info (G64 format)",
        .category = UFT_PRESET_CAT_PRESERVATION,
        .is_builtin = true,
        .params = {
            .struct_size = sizeof(uft_params_t),
            .version = 1,
            .global = { .global_retries = 5 },
            .geometry = {
                .cylinder_start = 0, .cylinder_end = 41,
                .head_start = 0, .head_end = 0,
                .sector_size = 256, .total_cylinders = 42, .total_heads = 1,
            },
            .format = {
                .output_format = UFT_FORMAT_G64,
                .cbm = { .use_half_tracks = true, .preserve_errors = true },
                .protection = { .preserve_weak_bits = true, .preserve_timing = true },
            },
            .hardware = { .flux = { .revolutions = 5, .index_aligned = true } },
            .decoder = { .encoding = UFT_ENCODING_GCR_CBM },
        },
    },
    {
        .name = "C64 1571 Double-Sided",
        .description = "C128/1571 double-sided disk",
        .category = UFT_PRESET_CAT_COMMODORE,
        .is_builtin = true,
        .params = {
            .struct_size = sizeof(uft_params_t),
            .version = 1,
            .geometry = {
                .cylinder_start = 0, .cylinder_end = 34,
                .head_start = 0, .head_end = 1,
                .sector_size = 256, .total_cylinders = 35, .total_heads = 2,
            },
            .format = { .output_format = UFT_FORMAT_D64 },
            .decoder = { .encoding = UFT_ENCODING_GCR_CBM },
        },
    },
    
    // === AMIGA ===
    {
        .name = "Amiga DD Standard",
        .description = "Amiga 880KB double-density disk",
        .category = UFT_PRESET_CAT_AMIGA,
        .is_builtin = true,
        .params = {
            .struct_size = sizeof(uft_params_t),
            .version = 1,
            .global = { .rpm = 300 },
            .geometry = {
                .cylinder_start = 0, .cylinder_end = 79,
                .head_start = 0, .head_end = 1,
                .sectors_per_track = 11, .sector_size = 512,
                .total_cylinders = 80, .total_heads = 2,
            },
            .format = {
                .output_format = UFT_FORMAT_ADF,
                .amiga = { .filesystem = 1 },  // OFS
            },
            .decoder = {
                .encoding = UFT_ENCODING_MFM,
                .pll = { .initial_period_us = 2.0 },
            },
        },
    },
    {
        .name = "Amiga HD",
        .description = "Amiga 1.76MB high-density disk",
        .category = UFT_PRESET_CAT_AMIGA,
        .is_builtin = true,
        .params = {
            .struct_size = sizeof(uft_params_t),
            .version = 1,
            .geometry = {
                .cylinder_start = 0, .cylinder_end = 79,
                .head_start = 0, .head_end = 1,
                .sectors_per_track = 22, .sector_size = 512,
                .total_cylinders = 80, .total_heads = 2,
            },
            .format = {
                .output_format = UFT_FORMAT_ADF,
                .amiga = { .allow_hd = true },
            },
            .decoder = { .pll = { .initial_period_us = 1.0 } },
        },
    },
    {
        .name = "Amiga Flux Preservation",
        .description = "Full flux capture for copy protection",
        .category = UFT_PRESET_CAT_PRESERVATION,
        .is_builtin = true,
        .params = {
            .struct_size = sizeof(uft_params_t),
            .version = 1,
            .geometry = {
                .cylinder_start = 0, .cylinder_end = 84,
                .head_start = 0, .head_end = 1,
            },
            .format = {
                .output_format = UFT_FORMAT_SCP,
                .protection = {
                    .preserve_weak_bits = true,
                    .preserve_long_tracks = true,
                    .preserve_timing = true,
                },
            },
            .hardware = { .flux = { .revolutions = 5, .index_aligned = true } },
        },
    },
    
    // === APPLE ===
    {
        .name = "Apple II DOS 3.3",
        .description = "Apple II 16-sector DOS 3.3 disk",
        .category = UFT_PRESET_CAT_APPLE,
        .is_builtin = true,
        .params = {
            .struct_size = sizeof(uft_params_t),
            .version = 1,
            .geometry = {
                .cylinder_start = 0, .cylinder_end = 34,
                .head_start = 0, .head_end = 0,
                .sectors_per_track = 16, .sector_size = 256,
                .total_cylinders = 35, .total_heads = 1,
            },
            .format = {
                .output_format = UFT_FORMAT_DSK,
                .apple = { .dos_version = 33 },
            },
            .decoder = {
                .encoding = UFT_ENCODING_GCR_APPLE,
                .pll = { .initial_period_us = 4.0 },
            },
        },
    },
    {
        .name = "Apple II ProDOS",
        .description = "Apple II ProDOS disk",
        .category = UFT_PRESET_CAT_APPLE,
        .is_builtin = true,
        .params = {
            .struct_size = sizeof(uft_params_t),
            .version = 1,
            .geometry = {
                .cylinder_start = 0, .cylinder_end = 34,
                .head_start = 0, .head_end = 0,
                .sectors_per_track = 16, .sector_size = 256,
            },
            .format = { .apple = { .dos_version = 0 } },  // ProDOS
            .decoder = { .encoding = UFT_ENCODING_GCR_APPLE },
        },
    },
    
    // === IBM PC ===
    {
        .name = "PC 360KB 5.25\"",
        .description = "IBM PC 360KB double-density 5.25\"",
        .category = UFT_PRESET_CAT_IBM_PC,
        .is_builtin = true,
        .params = {
            .struct_size = sizeof(uft_params_t),
            .version = 1,
            .global = { .rpm = 300 },
            .geometry = {
                .cylinder_start = 0, .cylinder_end = 39,
                .head_start = 0, .head_end = 1,
                .sectors_per_track = 9, .sector_size = 512,
                .total_cylinders = 40, .total_heads = 2,
            },
            .format = { .output_format = UFT_FORMAT_IMG },
            .decoder = {
                .encoding = UFT_ENCODING_MFM,
                .pll = { .initial_period_us = 4.0 },
            },
        },
    },
    {
        .name = "PC 720KB 3.5\"",
        .description = "IBM PC 720KB double-density 3.5\"",
        .category = UFT_PRESET_CAT_IBM_PC,
        .is_builtin = true,
        .params = {
            .struct_size = sizeof(uft_params_t),
            .version = 1,
            .global = { .rpm = 300 },
            .geometry = {
                .cylinder_start = 0, .cylinder_end = 79,
                .head_start = 0, .head_end = 1,
                .sectors_per_track = 9, .sector_size = 512,
                .total_cylinders = 80, .total_heads = 2,
            },
            .format = { .output_format = UFT_FORMAT_IMG },
            .decoder = { .pll = { .initial_period_us = 4.0 } },
        },
    },
    {
        .name = "PC 1.2MB 5.25\" HD",
        .description = "IBM PC 1.2MB high-density 5.25\"",
        .category = UFT_PRESET_CAT_IBM_PC,
        .is_builtin = true,
        .params = {
            .struct_size = sizeof(uft_params_t),
            .version = 1,
            .global = { .rpm = 360 },
            .geometry = {
                .cylinder_start = 0, .cylinder_end = 79,
                .head_start = 0, .head_end = 1,
                .sectors_per_track = 15, .sector_size = 512,
                .total_cylinders = 80, .total_heads = 2,
            },
            .format = { .output_format = UFT_FORMAT_IMG },
            .decoder = { .pll = { .initial_period_us = 2.0 } },
        },
    },
    {
        .name = "PC 1.44MB 3.5\" HD",
        .description = "IBM PC 1.44MB high-density 3.5\"",
        .category = UFT_PRESET_CAT_IBM_PC,
        .is_builtin = true,
        .params = {
            .struct_size = sizeof(uft_params_t),
            .version = 1,
            .global = { .rpm = 300 },
            .geometry = {
                .cylinder_start = 0, .cylinder_end = 79,
                .head_start = 0, .head_end = 1,
                .sectors_per_track = 18, .sector_size = 512,
                .total_cylinders = 80, .total_heads = 2,
            },
            .format = { .output_format = UFT_FORMAT_IMG },
            .decoder = { .pll = { .initial_period_us = 2.0 } },
        },
    },
    {
        .name = "PC 2.88MB 3.5\" ED",
        .description = "IBM PC 2.88MB extra-density 3.5\"",
        .category = UFT_PRESET_CAT_IBM_PC,
        .is_builtin = true,
        .params = {
            .struct_size = sizeof(uft_params_t),
            .version = 1,
            .geometry = {
                .cylinder_start = 0, .cylinder_end = 79,
                .head_start = 0, .head_end = 1,
                .sectors_per_track = 36, .sector_size = 512,
            },
            .decoder = { .pll = { .initial_period_us = 1.0 } },
        },
    },
    
    // === ATARI ===
    {
        .name = "Atari ST DD",
        .description = "Atari ST 720KB double-density",
        .category = UFT_PRESET_CAT_ATARI,
        .is_builtin = true,
        .params = {
            .struct_size = sizeof(uft_params_t),
            .version = 1,
            .geometry = {
                .cylinder_start = 0, .cylinder_end = 79,
                .head_start = 0, .head_end = 1,
                .sectors_per_track = 9, .sector_size = 512,
                .total_cylinders = 80, .total_heads = 2,
            },
            .format = { .output_format = UFT_FORMAT_DSK },
            .decoder = { .pll = { .initial_period_us = 4.0 } },
        },
    },
    {
        .name = "Atari ST HD",
        .description = "Atari ST 1.44MB high-density",
        .category = UFT_PRESET_CAT_ATARI,
        .is_builtin = true,
        .params = {
            .struct_size = sizeof(uft_params_t),
            .version = 1,
            .geometry = {
                .cylinder_start = 0, .cylinder_end = 79,
                .head_start = 0, .head_end = 1,
                .sectors_per_track = 18, .sector_size = 512,
            },
            .format = { .output_format = UFT_FORMAT_DSK },
            .decoder = { .pll = { .initial_period_us = 2.0 } },
        },
    },
    
    // === PRESERVATION ===
    {
        .name = "SCP Full Preservation",
        .description = "Maximum quality flux capture (5 revolutions)",
        .category = UFT_PRESET_CAT_PRESERVATION,
        .is_builtin = true,
        .params = {
            .struct_size = sizeof(uft_params_t),
            .version = 1,
            .geometry = {
                .cylinder_start = 0, .cylinder_end = 83,
                .head_start = 0, .head_end = 1,
            },
            .format = {
                .output_format = UFT_FORMAT_SCP,
                .protection = {
                    .preserve_weak_bits = true,
                    .preserve_long_tracks = true,
                    .preserve_timing = true,
                },
            },
            .hardware = {
                .flux = { .revolutions = 5, .index_aligned = true },
            },
            .output = { .generate_report = true, .generate_hash = true },
        },
    },
    {
        .name = "Kryoflux Stream",
        .description = "Kryoflux raw stream capture",
        .category = UFT_PRESET_CAT_PRESERVATION,
        .is_builtin = true,
        .params = {
            .struct_size = sizeof(uft_params_t),
            .version = 1,
            .geometry = {
                .cylinder_start = 0, .cylinder_end = 83,
                .head_start = 0, .head_end = 1,
            },
            .format = { .output_format = UFT_FORMAT_KRYOFLUX },
            .hardware = { .flux = { .revolutions = 3 } },
        },
    },
    
    // === COPY PROTECTION ===
    {
        .name = "Copy Protection Analysis",
        .description = "Settings for analyzing protected disks",
        .category = UFT_PRESET_CAT_COPY_PROTECTION,
        .is_builtin = true,
        .params = {
            .struct_size = sizeof(uft_params_t),
            .version = 1,
            .global = { .global_retries = 10 },
            .geometry = {
                .cylinder_start = 0, .cylinder_end = 85,
                .head_start = 0, .head_end = 1,
            },
            .format = {
                .output_format = UFT_FORMAT_SCP,
                .protection = {
                    .preserve_weak_bits = true,
                    .preserve_long_tracks = true,
                    .preserve_timing = true,
                },
            },
            .hardware = { .flux = { .revolutions = 10, .index_aligned = true } },
            .decoder = {
                .errors = { .sector_retries = 10, .use_multiple_revs = true },
            },
            .output = { .generate_report = true },
        },
    },
};

#define NUM_BUILTIN_PRESETS (sizeof(g_builtin_presets) / sizeof(g_builtin_presets[0]))

// ============================================================================
// Preset Registry
// ============================================================================

static struct {
    uft_preset_t    presets[UFT_MAX_PRESETS];
    size_t          count;
    bool            initialized;
    char            user_dir[512];
} g_preset_registry = {0};

// ============================================================================
// Path Helpers
// ============================================================================

static void expand_path(const char* path, char* expanded, size_t size) {
    if (path[0] == '~') {
        const char* home = getenv("HOME");
        if (home) {
            snprintf(expanded, size, "%s%s", home, path + 1);
        } else {
            strncpy(expanded, path, size - 1);
        }
    } else {
        strncpy(expanded, path, size - 1);
    }
    expanded[size - 1] = '\0';
}

static void ensure_dir_exists(const char* path) {
    char tmp[512];
    strncpy(tmp, path, sizeof(tmp) - 1);
    
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

// ============================================================================
// JSON Serialization (Simple Implementation)
// ============================================================================

static void write_json_string(FILE* f, const char* key, const char* value) {
    fprintf(f, "  \"%s\": \"%s\",\n", key, value ? value : "");
}

static void write_json_int(FILE* f, const char* key, int value) {
    fprintf(f, "  \"%s\": %d,\n", key, value);
}

static void write_json_double(FILE* f, const char* key, double value) {
    fprintf(f, "  \"%s\": %.6f,\n", key, value);
}

static void write_json_bool(FILE* f, const char* key, bool value) {
    fprintf(f, "  \"%s\": %s,\n", key, value ? "true" : "false");
}

static uft_error_t preset_to_json(const uft_preset_t* preset, const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) return UFT_ERROR_IO;
    
    fprintf(f, "{\n");
    
    // Metadata
    write_json_string(f, "name", preset->name);
    write_json_string(f, "description", preset->description);
    write_json_int(f, "category", preset->category);
    write_json_int(f, "version", preset->params.version);
    
    // Global
    fprintf(f, "  \"global\": {\n");
    write_json_int(f, "    device_index", preset->params.global.device_index);
    write_json_double(f, "    rpm", preset->params.global.rpm);
    write_json_int(f, "    global_retries", preset->params.global.global_retries);
    write_json_bool(f, "    verify_after_write", preset->params.global.verify_after_write);
    fprintf(f, "  },\n");
    
    // Geometry
    fprintf(f, "  \"geometry\": {\n");
    write_json_int(f, "    cylinder_start", preset->params.geometry.cylinder_start);
    write_json_int(f, "    cylinder_end", preset->params.geometry.cylinder_end);
    write_json_int(f, "    head_start", preset->params.geometry.head_start);
    write_json_int(f, "    head_end", preset->params.geometry.head_end);
    write_json_int(f, "    sectors_per_track", preset->params.geometry.sectors_per_track);
    write_json_int(f, "    sector_size", preset->params.geometry.sector_size);
    fprintf(f, "  },\n");
    
    // Format
    fprintf(f, "  \"format\": {\n");
    write_json_int(f, "    input_format", preset->params.format.input_format);
    write_json_int(f, "    output_format", preset->params.format.output_format);
    fprintf(f, "  },\n");
    
    // Hardware
    fprintf(f, "  \"hardware\": {\n");
    write_json_int(f, "    revolutions", preset->params.hardware.flux.revolutions);
    write_json_bool(f, "    index_aligned", preset->params.hardware.flux.index_aligned);
    fprintf(f, "  },\n");
    
    // Decoder
    fprintf(f, "  \"decoder\": {\n");
    write_json_int(f, "    encoding", preset->params.decoder.encoding);
    write_json_double(f, "    pll_period_us", preset->params.decoder.pll.initial_period_us);
    write_json_double(f, "    pll_tolerance", preset->params.decoder.pll.tolerance);
    fprintf(f, "  }\n");
    
    fprintf(f, "}\n");
    fclose(f);
    
    return UFT_OK;
}

static int read_json_int(const char* json, const char* key) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char* pos = strstr(json, search);
    if (!pos) return 0;
    pos += strlen(search);
    while (*pos == ' ' || *pos == '\t') pos++;
    { int32_t t=0; uft_parse_int32(pos,&t,10); return t; }
}

static double read_json_double(const char* json, const char* key) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char* pos = strstr(json, search);
    if (!pos) return 0;
    pos += strlen(search);
    while (*pos == ' ' || *pos == '\t') pos++;
    return atof(pos);
}

static bool read_json_bool(const char* json, const char* key) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char* pos = strstr(json, search);
    if (!pos) return false;
    pos += strlen(search);
    while (*pos == ' ' || *pos == '\t') pos++;
    return (strncmp(pos, "true", 4) == 0);
}

static void read_json_string(const char* json, const char* key, 
                              char* value, size_t size) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char* pos = strstr(json, search);
    if (!pos) { value[0] = '\0'; return; }
    pos += strlen(search);
    while (*pos == ' ' || *pos == '\t' || *pos == '"') pos++;
    
    size_t i = 0;
    while (*pos && *pos != '"' && i < size - 1) {
        value[i++] = *pos++;
    }
    value[i] = '\0';
}

static uft_error_t json_to_preset(const char* path, uft_preset_t* preset) {
    FILE* f = fopen(path, "r");
    if (!f) return UFT_ERROR_NOT_FOUND;
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    long size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    char* json = malloc(size + 1);
    if (!json) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    
    if (fread(json, 1, size, f) != size) { /* I/O error */ }
    json[size] = '\0';
    fclose(f);
    
    // Parse
    memset(preset, 0, sizeof(*preset));
    read_json_string(json, "name", preset->name, sizeof(preset->name));
    read_json_string(json, "description", preset->description, sizeof(preset->description));
    preset->category = read_json_int(json, "category");
    preset->params.version = read_json_int(json, "version");
    preset->params.struct_size = sizeof(uft_params_t);
    
    // Global
    preset->params.global.device_index = read_json_int(json, "device_index");
    preset->params.global.rpm = read_json_double(json, "rpm");
    preset->params.global.global_retries = read_json_int(json, "global_retries");
    preset->params.global.verify_after_write = read_json_bool(json, "verify_after_write");
    
    // Geometry
    preset->params.geometry.cylinder_start = read_json_int(json, "cylinder_start");
    preset->params.geometry.cylinder_end = read_json_int(json, "cylinder_end");
    preset->params.geometry.head_start = read_json_int(json, "head_start");
    preset->params.geometry.head_end = read_json_int(json, "head_end");
    preset->params.geometry.sectors_per_track = read_json_int(json, "sectors_per_track");
    preset->params.geometry.sector_size = read_json_int(json, "sector_size");
    
    // Format
    preset->params.format.input_format = read_json_int(json, "input_format");
    preset->params.format.output_format = read_json_int(json, "output_format");
    
    // Hardware
    preset->params.hardware.flux.revolutions = read_json_int(json, "revolutions");
    preset->params.hardware.flux.index_aligned = read_json_bool(json, "index_aligned");
    
    // Decoder
    preset->params.decoder.encoding = read_json_int(json, "encoding");
    preset->params.decoder.pll.initial_period_us = read_json_double(json, "pll_period_us");
    preset->params.decoder.pll.tolerance = read_json_double(json, "pll_tolerance");
    
    free(json);
    return UFT_OK;
}

// ============================================================================
// Public API
// ============================================================================

uft_error_t uft_preset_init(void) {
    if (g_preset_registry.initialized) return UFT_OK;
    
    memset(&g_preset_registry, 0, sizeof(g_preset_registry));
    
    // Set up user directory
    expand_path(UFT_PRESET_DIR_USER, g_preset_registry.user_dir, 
                sizeof(g_preset_registry.user_dir));
    ensure_dir_exists(g_preset_registry.user_dir);
    
    // Load built-in presets
    for (size_t i = 0; i < NUM_BUILTIN_PRESETS; i++) {
        g_preset_registry.presets[g_preset_registry.count++] = g_builtin_presets[i];
    }
    
    // Load user presets
    DIR* dir = opendir(g_preset_registry.user_dir);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) && g_preset_registry.count < UFT_MAX_PRESETS) {
            if (strstr(entry->d_name, UFT_PRESET_EXTENSION)) {
                char path[768];
                snprintf(path, sizeof(path), "%s/%s", 
                         g_preset_registry.user_dir, entry->d_name);
                
                uft_preset_t preset;
                if (json_to_preset(path, &preset) == UFT_OK) {
                    preset.is_builtin = false;
                    preset.category = UFT_PRESET_CAT_USER;
                    g_preset_registry.presets[g_preset_registry.count++] = preset;
                }
            }
        }
        closedir(dir);
    }
    
    g_preset_registry.initialized = true;
    return UFT_OK;
}

size_t uft_preset_count(void) {
    return g_preset_registry.count;
}

const uft_preset_t* uft_preset_get(size_t index) {
    if (index >= g_preset_registry.count) return NULL;
    return &g_preset_registry.presets[index];
}

const uft_preset_t* uft_preset_find(const char* name) {
    if (!name) return NULL;
    
    for (size_t i = 0; i < g_preset_registry.count; i++) {
        if (strcmp(g_preset_registry.presets[i].name, name) == 0) {
            return &g_preset_registry.presets[i];
        }
    }
    return NULL;
}

uft_error_t uft_preset_save(const char* name, const uft_params_t* params) {
    if (!name || !params) return UFT_ERROR_NULL_POINTER;
    
    // Create preset
    uft_preset_t preset = {0};
    strncpy(preset.name, name, sizeof(preset.name) - 1);
    preset.category = UFT_PRESET_CAT_USER;
    preset.is_builtin = false;
    preset.params = *params;
    
    // Save to file
    char path[768];
    snprintf(path, sizeof(path), "%s/%s%s",
             g_preset_registry.user_dir, name, UFT_PRESET_EXTENSION);
    
    uft_error_t err = preset_to_json(&preset, path);
    if (err != UFT_OK) return err;
    
    // Add to registry (or update existing)
    for (size_t i = 0; i < g_preset_registry.count; i++) {
        if (strcmp(g_preset_registry.presets[i].name, name) == 0) {
            g_preset_registry.presets[i] = preset;
            return UFT_OK;
        }
    }
    
    if (g_preset_registry.count < UFT_MAX_PRESETS) {
        g_preset_registry.presets[g_preset_registry.count++] = preset;
    }
    
    return UFT_OK;
}

uft_error_t uft_preset_load(const char* name, uft_params_t* params) {
    if (!name || !params) return UFT_ERROR_NULL_POINTER;
    
    const uft_preset_t* preset = uft_preset_find(name);
    if (!preset) return UFT_ERROR_NOT_FOUND;
    
    *params = preset->params;
    return UFT_OK;
}

uft_error_t uft_preset_delete(const char* name) {
    if (!name) return UFT_ERROR_NULL_POINTER;
    
    // Find preset
    for (size_t i = 0; i < g_preset_registry.count; i++) {
        if (strcmp(g_preset_registry.presets[i].name, name) == 0) {
            if (g_preset_registry.presets[i].is_builtin) {
                return UFT_ERROR_NOT_ALLOWED;  // Can't delete built-in
            }
            
            // Delete file
            char path[768];
            snprintf(path, sizeof(path), "%s/%s%s",
                     g_preset_registry.user_dir, name, UFT_PRESET_EXTENSION);
            unlink(path);
            
            // Remove from registry
            memmove(&g_preset_registry.presets[i],
                    &g_preset_registry.presets[i + 1],
                    (g_preset_registry.count - i - 1) * sizeof(uft_preset_t));
            g_preset_registry.count--;
            
            return UFT_OK;
        }
    }
    
    return UFT_ERROR_NOT_FOUND;
}

int uft_preset_list(const char** names, int max_count) {
    int count = 0;
    for (size_t i = 0; i < g_preset_registry.count && count < max_count; i++) {
        if (names) {
            names[count] = g_preset_registry.presets[i].name;
        }
        count++;
    }
    return count;
}

int uft_preset_list_by_category(uft_preset_category_t cat,
                                 const uft_preset_t** presets,
                                 int max_count) {
    int count = 0;
    for (size_t i = 0; i < g_preset_registry.count && count < max_count; i++) {
        if (g_preset_registry.presets[i].category == cat) {
            if (presets) {
                presets[count] = &g_preset_registry.presets[i];
            }
            count++;
        }
    }
    return count;
}

const char* uft_preset_category_name(uft_preset_category_t cat) {
    switch (cat) {
        case UFT_PRESET_CAT_GENERAL:         return "General";
        case UFT_PRESET_CAT_COMMODORE:       return "Commodore";
        case UFT_PRESET_CAT_AMIGA:           return "Amiga";
        case UFT_PRESET_CAT_APPLE:           return "Apple";
        case UFT_PRESET_CAT_IBM_PC:          return "IBM PC";
        case UFT_PRESET_CAT_ATARI:           return "Atari";
        case UFT_PRESET_CAT_PRESERVATION:    return "Preservation";
        case UFT_PRESET_CAT_COPY_PROTECTION: return "Copy Protection";
        case UFT_PRESET_CAT_USER:            return "User Presets";
        default:                             return "Unknown";
    }
}
