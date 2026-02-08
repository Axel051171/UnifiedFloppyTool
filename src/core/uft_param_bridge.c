/**
 * @file uft_param_bridge.c
 * @brief CLI-GUI Parameter Bridge Implementation (W-P1-002)
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#include "uft/uft_param_bridge.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*===========================================================================
 * PARAMETER DEFINITIONS
 *===========================================================================*/

static const char *FORMAT_ENUM_VALUES[] = {
    "auto", "adf", "d64", "g64", "scp", "hfe", "img", "td0", "imd", "woz", NULL
};

static const char *ENCODING_ENUM_VALUES[] = {
    "auto", "mfm", "fm", "gcr", NULL
};

static const char *HARDWARE_ENUM_VALUES[] = {
    "auto", "kryoflux", "greaseweazle", "supercard", "fc5025", NULL
};

/* Core parameter definitions */
static const uft_param_def_t PARAM_DEFINITIONS[] = {
    /* General */
    {
        .name = "input", .cli_short = "-i", .cli_long = "--input",
        .json_key = "input", .gui_widget = "inputFileEdit",
        .type = UFT_PARAM_TYPE_PATH, .category = UFT_PARAM_CAT_GENERAL,
        .description = "Input file path", .required = true
    },
    {
        .name = "output", .cli_short = "-o", .cli_long = "--output",
        .json_key = "output", .gui_widget = "outputFileEdit",
        .type = UFT_PARAM_TYPE_PATH, .category = UFT_PARAM_CAT_GENERAL,
        .description = "Output file path"
    },
    {
        .name = "verbose", .cli_short = "-v", .cli_long = "--verbose",
        .json_key = "verbose", .gui_widget = "verboseCheckBox",
        .type = UFT_PARAM_TYPE_BOOL, .category = UFT_PARAM_CAT_GENERAL,
        .description = "Enable verbose output", .default_value = "false"
    },
    {
        .name = "quiet", .cli_short = "-q", .cli_long = "--quiet",
        .json_key = "quiet", .gui_widget = "quietCheckBox",
        .type = UFT_PARAM_TYPE_BOOL, .category = UFT_PARAM_CAT_GENERAL,
        .description = "Suppress non-error output", .default_value = "false"
    },
    
    /* Format */
    {
        .name = "format", .cli_short = "-f", .cli_long = "--format",
        .json_key = "format", .gui_widget = "formatComboBox",
        .type = UFT_PARAM_TYPE_ENUM, .category = UFT_PARAM_CAT_FORMAT,
        .description = "Disk image format", .default_value = "auto",
        .enum_values = FORMAT_ENUM_VALUES, .enum_count = 10
    },
    {
        .name = "sides", .cli_short = "-s", .cli_long = "--sides",
        .json_key = "sides", .gui_widget = "sidesSpinBox",
        .type = UFT_PARAM_TYPE_RANGE, .category = UFT_PARAM_CAT_FORMAT,
        .description = "Number of disk sides", .default_value = "2",
        .range_min = 1, .range_max = 2, .range_step = 1
    },
    {
        .name = "tracks", .cli_short = "-t", .cli_long = "--tracks",
        .json_key = "tracks", .gui_widget = "tracksSpinBox",
        .type = UFT_PARAM_TYPE_RANGE, .category = UFT_PARAM_CAT_FORMAT,
        .description = "Number of tracks", .default_value = "80",
        .range_min = 35, .range_max = 84, .range_step = 1
    },
    
    /* Encoding */
    {
        .name = "encoding", .cli_short = "-e", .cli_long = "--encoding",
        .json_key = "encoding", .gui_widget = "encodingComboBox",
        .type = UFT_PARAM_TYPE_ENUM, .category = UFT_PARAM_CAT_ENCODING,
        .description = "Data encoding", .default_value = "auto",
        .enum_values = ENCODING_ENUM_VALUES, .enum_count = 4
    },
    
    /* Hardware */
    {
        .name = "hardware", .cli_short = "-H", .cli_long = "--hardware",
        .json_key = "hardware", .gui_widget = "hardwareComboBox",
        .type = UFT_PARAM_TYPE_ENUM, .category = UFT_PARAM_CAT_HARDWARE,
        .description = "Hardware interface", .default_value = "auto",
        .enum_values = HARDWARE_ENUM_VALUES, .enum_count = 5
    },
    {
        .name = "device", .cli_short = "-d", .cli_long = "--device",
        .json_key = "device", .gui_widget = "deviceEdit",
        .type = UFT_PARAM_TYPE_STRING, .category = UFT_PARAM_CAT_HARDWARE,
        .description = "Device path or serial port"
    },
    
    /* PLL */
    {
        .name = "pll_adjust", .cli_short = NULL, .cli_long = "--pll-adjust",
        .json_key = "pll_adjust", .gui_widget = "pllAdjustSpinBox",
        .type = UFT_PARAM_TYPE_RANGE, .category = UFT_PARAM_CAT_PLL,
        .description = "PLL adjustment percentage", .default_value = "15",
        .range_min = 5, .range_max = 30, .range_step = 1, .expert = true
    },
    {
        .name = "pll_phase", .cli_short = NULL, .cli_long = "--pll-phase",
        .json_key = "pll_phase", .gui_widget = "pllPhaseSpinBox",
        .type = UFT_PARAM_TYPE_RANGE, .category = UFT_PARAM_CAT_PLL,
        .description = "PLL phase percentage", .default_value = "60",
        .range_min = 30, .range_max = 90, .range_step = 5, .expert = true
    },
    
    /* Recovery */
    {
        .name = "retries", .cli_short = "-r", .cli_long = "--retries",
        .json_key = "retries", .gui_widget = "retriesSpinBox",
        .type = UFT_PARAM_TYPE_RANGE, .category = UFT_PARAM_CAT_RECOVERY,
        .description = "Read retry count", .default_value = "5",
        .range_min = 0, .range_max = 50, .range_step = 1
    },
    {
        .name = "revolutions", .cli_short = NULL, .cli_long = "--revolutions",
        .json_key = "revolutions", .gui_widget = "revolutionsSpinBox",
        .type = UFT_PARAM_TYPE_RANGE, .category = UFT_PARAM_CAT_RECOVERY,
        .description = "Revolutions to capture", .default_value = "3",
        .range_min = 1, .range_max = 10, .range_step = 1
    },
    {
        .name = "merge_revs", .cli_short = NULL, .cli_long = "--merge-revolutions",
        .json_key = "merge_revolutions", .gui_widget = "mergeRevsCheckBox",
        .type = UFT_PARAM_TYPE_BOOL, .category = UFT_PARAM_CAT_RECOVERY,
        .description = "Merge multiple revolutions", .default_value = "true"
    },
    
    /* Output */
    {
        .name = "verify", .cli_short = NULL, .cli_long = "--verify",
        .json_key = "verify", .gui_widget = "verifyCheckBox",
        .type = UFT_PARAM_TYPE_BOOL, .category = UFT_PARAM_CAT_OUTPUT,
        .description = "Verify after write", .default_value = "true"
    },
    {
        .name = "report", .cli_short = NULL, .cli_long = "--report",
        .json_key = "report", .gui_widget = "reportEdit",
        .type = UFT_PARAM_TYPE_PATH, .category = UFT_PARAM_CAT_OUTPUT,
        .description = "Generate report file"
    },
    
    /* Debug */
    {
        .name = "debug", .cli_short = NULL, .cli_long = "--debug",
        .json_key = "debug", .gui_widget = "debugCheckBox",
        .type = UFT_PARAM_TYPE_BOOL, .category = UFT_PARAM_CAT_DEBUG,
        .description = "Enable debug mode", .default_value = "false", .expert = true
    },
    {
        .name = "log_file", .cli_short = NULL, .cli_long = "--log",
        .json_key = "log_file", .gui_widget = "logFileEdit",
        .type = UFT_PARAM_TYPE_PATH, .category = UFT_PARAM_CAT_DEBUG,
        .description = "Log file path", .expert = true
    },
    
    /* Sentinel */
    { .name = NULL }
};

#define PARAM_COUNT 19

/*===========================================================================
 * PRESET DEFINITIONS
 *===========================================================================*/

static const uft_preset_t PRESETS[] = {
    {
        .name = "amiga_dd",
        .description = "Amiga DD (880K)",
        .category = UFT_PARAM_CAT_FORMAT,
        .json_params = "{\"format\":\"adf\",\"encoding\":\"mfm\",\"sides\":2,\"tracks\":80}",
        .cli_args = "-f adf -e mfm -s 2 -t 80"
    },
    {
        .name = "c64_1541",
        .description = "Commodore 64 (1541)",
        .category = UFT_PARAM_CAT_FORMAT,
        .json_params = "{\"format\":\"d64\",\"encoding\":\"gcr\",\"sides\":1,\"tracks\":35}",
        .cli_args = "-f d64 -e gcr -s 1 -t 35"
    },
    {
        .name = "ibm_pc_hd",
        .description = "IBM PC HD (1.44MB)",
        .category = UFT_PARAM_CAT_FORMAT,
        .json_params = "{\"format\":\"img\",\"encoding\":\"mfm\",\"sides\":2,\"tracks\":80}",
        .cli_args = "-f img -e mfm -s 2 -t 80"
    },
    {
        .name = "recovery_aggressive",
        .description = "Aggressive Recovery",
        .category = UFT_PARAM_CAT_RECOVERY,
        .json_params = "{\"retries\":20,\"revolutions\":5,\"merge_revolutions\":true}",
        .cli_args = "-r 20 --revolutions 5 --merge-revolutions"
    },
    {
        .name = "recovery_fast",
        .description = "Fast Recovery",
        .category = UFT_PARAM_CAT_RECOVERY,
        .json_params = "{\"retries\":2,\"revolutions\":1,\"merge_revolutions\":false}",
        .cli_args = "-r 2 --revolutions 1"
    },
    /* Sentinel */
    { .name = NULL }
};

#define PRESET_COUNT 5

/*===========================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

struct uft_params {
    uft_param_value_t values[PARAM_COUNT];
    int count;
};

/*===========================================================================
 * HELPER FUNCTIONS
 *===========================================================================*/

static int find_param_index(const char *name) {
    if (!name) return -1;
    for (int i = 0; i < PARAM_COUNT; i++) {
        if (PARAM_DEFINITIONS[i].name && strcmp(PARAM_DEFINITIONS[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_enum_index(const char **values, const char *value) {
    if (!values || !value) return -1;
    for (int i = 0; values[i]; i++) {
        if (strcmp(values[i], value) == 0) return i;
    }
    return -1;
}

static void parse_default_value(uft_param_value_t *val, const uft_param_def_t *def) {
    if (!def->default_value) return;
    
    switch (def->type) {
        case UFT_PARAM_TYPE_BOOL:
            val->value.bool_val = (strcmp(def->default_value, "true") == 0);
            break;
        case UFT_PARAM_TYPE_INT:
        case UFT_PARAM_TYPE_RANGE:
            val->value.int_val = atoi(def->default_value);
            break;
        case UFT_PARAM_TYPE_FLOAT:
            val->value.float_val = (float)atof(def->default_value);
            break;
        case UFT_PARAM_TYPE_ENUM:
            val->value.enum_index = find_enum_index(def->enum_values, def->default_value);
            break;
        case UFT_PARAM_TYPE_STRING:
        case UFT_PARAM_TYPE_PATH:
            val->value.string_val = strdup(def->default_value);
            break;
    }
    val->is_default = true;
}

/*===========================================================================
 * LIFECYCLE
 *===========================================================================*/

uft_params_t *uft_params_create(void) {
    uft_params_t *params = (uft_params_t *)calloc(1, sizeof(uft_params_t));
    if (!params) return NULL;
    
    params->count = PARAM_COUNT;
    for (int i = 0; i < PARAM_COUNT; i++) {
        params->values[i].definition = &PARAM_DEFINITIONS[i];
    }
    
    return params;
}

uft_params_t *uft_params_create_defaults(void) {
    uft_params_t *params = uft_params_create();
    if (!params) return NULL;
    
    for (int i = 0; i < PARAM_COUNT; i++) {
        parse_default_value(&params->values[i], &PARAM_DEFINITIONS[i]);
    }
    
    return params;
}

uft_params_t *uft_params_clone(const uft_params_t *params) {
    if (!params) return NULL;
    
    uft_params_t *clone = uft_params_create();
    if (!clone) return NULL;
    
    for (int i = 0; i < PARAM_COUNT; i++) {
        clone->values[i] = params->values[i];
        /* Deep copy strings */
        if ((PARAM_DEFINITIONS[i].type == UFT_PARAM_TYPE_STRING ||
             PARAM_DEFINITIONS[i].type == UFT_PARAM_TYPE_PATH) &&
            params->values[i].value.string_val) {
            clone->values[i].value.string_val = strdup(params->values[i].value.string_val);
        }
    }
    
    return clone;
}

void uft_params_free(uft_params_t *params) {
    if (!params) return;
    
    for (int i = 0; i < PARAM_COUNT; i++) {
        if ((PARAM_DEFINITIONS[i].type == UFT_PARAM_TYPE_STRING ||
             PARAM_DEFINITIONS[i].type == UFT_PARAM_TYPE_PATH) &&
            params->values[i].value.string_val) {
            free(params->values[i].value.string_val);
        }
    }
    
    free(params);
}

void uft_params_reset(uft_params_t *params) {
    if (!params) return;
    
    for (int i = 0; i < PARAM_COUNT; i++) {
        if ((PARAM_DEFINITIONS[i].type == UFT_PARAM_TYPE_STRING ||
             PARAM_DEFINITIONS[i].type == UFT_PARAM_TYPE_PATH) &&
            params->values[i].value.string_val) {
            free(params->values[i].value.string_val);
        }
        memset(&params->values[i].value, 0, sizeof(params->values[i].value));
        params->values[i].is_set = false;
        params->values[i].is_default = false;
        parse_default_value(&params->values[i], &PARAM_DEFINITIONS[i]);
    }
}

/*===========================================================================
 * PARAMETER ACCESS
 *===========================================================================*/

bool uft_params_get_bool(const uft_params_t *params, const char *name) {
    if (!params) return false;
    int idx = find_param_index(name);
    if (idx < 0) return false;
    return params->values[idx].value.bool_val;
}

int uft_params_get_int(const uft_params_t *params, const char *name) {
    if (!params) return 0;
    int idx = find_param_index(name);
    if (idx < 0) return 0;
    return params->values[idx].value.int_val;
}

float uft_params_get_float(const uft_params_t *params, const char *name) {
    if (!params) return 0.0f;
    int idx = find_param_index(name);
    if (idx < 0) return 0.0f;
    return params->values[idx].value.float_val;
}

const char *uft_params_get_string(const uft_params_t *params, const char *name) {
    if (!params) return NULL;
    int idx = find_param_index(name);
    if (idx < 0) return NULL;
    return params->values[idx].value.string_val;
}

int uft_params_get_enum(const uft_params_t *params, const char *name) {
    if (!params) return 0;
    int idx = find_param_index(name);
    if (idx < 0) return 0;
    return params->values[idx].value.enum_index;
}

const char *uft_params_get_enum_string(const uft_params_t *params, const char *name) {
    if (!params) return NULL;
    int idx = find_param_index(name);
    if (idx < 0) return NULL;
    
    const uft_param_def_t *def = &PARAM_DEFINITIONS[idx];
    int enum_idx = params->values[idx].value.enum_index;
    
    if (def->enum_values && enum_idx >= 0 && enum_idx < def->enum_count) {
        return def->enum_values[enum_idx];
    }
    return NULL;
}

uft_error_t uft_params_set_bool(uft_params_t *params, const char *name, bool value) {
    if (!params) return UFT_ERR_INVALID_ARG;
    int idx = find_param_index(name);
    if (idx < 0) return UFT_ERR_FILE_NOT_FOUND;
    
    params->values[idx].value.bool_val = value;
    params->values[idx].is_set = true;
    params->values[idx].is_default = false;
    return UFT_OK;
}

uft_error_t uft_params_set_int(uft_params_t *params, const char *name, int value) {
    if (!params) return UFT_ERR_INVALID_ARG;
    int idx = find_param_index(name);
    if (idx < 0) return UFT_ERR_FILE_NOT_FOUND;
    
    const uft_param_def_t *def = &PARAM_DEFINITIONS[idx];
    if (def->type == UFT_PARAM_TYPE_RANGE) {
        if (value < def->range_min) value = def->range_min;
        if (value > def->range_max) value = def->range_max;
    }
    
    params->values[idx].value.int_val = value;
    params->values[idx].is_set = true;
    params->values[idx].is_default = false;
    return UFT_OK;
}

uft_error_t uft_params_set_float(uft_params_t *params, const char *name, float value) {
    if (!params) return UFT_ERR_INVALID_ARG;
    int idx = find_param_index(name);
    if (idx < 0) return UFT_ERR_FILE_NOT_FOUND;
    
    params->values[idx].value.float_val = value;
    params->values[idx].is_set = true;
    params->values[idx].is_default = false;
    return UFT_OK;
}

uft_error_t uft_params_set_string(uft_params_t *params, const char *name, const char *value) {
    if (!params) return UFT_ERR_INVALID_ARG;
    int idx = find_param_index(name);
    if (idx < 0) return UFT_ERR_FILE_NOT_FOUND;
    
    free(params->values[idx].value.string_val);
    params->values[idx].value.string_val = value ? strdup(value) : NULL;
    params->values[idx].is_set = true;
    params->values[idx].is_default = false;
    return UFT_OK;
}

uft_error_t uft_params_set_enum(uft_params_t *params, const char *name, int index) {
    if (!params) return UFT_ERR_INVALID_ARG;
    int idx = find_param_index(name);
    if (idx < 0) return UFT_ERR_FILE_NOT_FOUND;
    
    const uft_param_def_t *def = &PARAM_DEFINITIONS[idx];
    if (index < 0 || index >= def->enum_count) return UFT_ERR_INVALID_ARG;
    
    params->values[idx].value.enum_index = index;
    params->values[idx].is_set = true;
    params->values[idx].is_default = false;
    return UFT_OK;
}

uft_error_t uft_params_set_enum_string(uft_params_t *params, const char *name, const char *value) {
    if (!params || !value) return UFT_ERR_INVALID_ARG;
    int idx = find_param_index(name);
    if (idx < 0) return UFT_ERR_FILE_NOT_FOUND;
    
    const uft_param_def_t *def = &PARAM_DEFINITIONS[idx];
    int enum_idx = find_enum_index(def->enum_values, value);
    if (enum_idx < 0) return UFT_ERR_FILE_NOT_FOUND;
    
    return uft_params_set_enum(params, name, enum_idx);
}

bool uft_params_is_set(const uft_params_t *params, const char *name) {
    if (!params) return false;
    int idx = find_param_index(name);
    if (idx < 0) return false;
    return params->values[idx].is_set;
}

void uft_params_unset(uft_params_t *params, const char *name) {
    if (!params) return;
    int idx = find_param_index(name);
    if (idx < 0) return;
    
    if ((PARAM_DEFINITIONS[idx].type == UFT_PARAM_TYPE_STRING ||
         PARAM_DEFINITIONS[idx].type == UFT_PARAM_TYPE_PATH) &&
        params->values[idx].value.string_val) {
        free(params->values[idx].value.string_val);
    }
    
    memset(&params->values[idx].value, 0, sizeof(params->values[idx].value));
    params->values[idx].is_set = false;
    parse_default_value(&params->values[idx], &PARAM_DEFINITIONS[idx]);
}

/*===========================================================================
 * JSON SERIALIZATION
 *===========================================================================*/

char *uft_params_to_json(const uft_params_t *params, bool pretty) {
    if (!params) return NULL;
    
    const size_t json_size = 8192;
    char *json = (char *)malloc(json_size);
    if (!json) return NULL;
    
    size_t offset = 0;
    size_t remaining = json_size;
    const char *nl = pretty ? "\n" : "";
    const char *indent = pretty ? "  " : "";
    
#define JSON_APPEND(...) do { \
    int n = snprintf(json + offset, remaining, __VA_ARGS__); \
    if (n > 0 && (size_t)n < remaining) { offset += n; remaining -= n; } \
} while(0)
    
    JSON_APPEND("{%s", nl);
    
    bool first = true;
    for (int i = 0; i < PARAM_COUNT; i++) {
        if (!params->values[i].is_set && params->values[i].is_default) continue;
        
        const uft_param_def_t *def = &PARAM_DEFINITIONS[i];
        if (!def->json_key) continue;
        
        if (!first) JSON_APPEND(",%s", nl);
        first = false;
        
        JSON_APPEND("%s\"%s\": ", indent, def->json_key);
        
        switch (def->type) {
            case UFT_PARAM_TYPE_BOOL:
                JSON_APPEND("%s", params->values[i].value.bool_val ? "true" : "false");
                break;
            case UFT_PARAM_TYPE_INT:
            case UFT_PARAM_TYPE_RANGE:
                JSON_APPEND("%d", params->values[i].value.int_val);
                break;
            case UFT_PARAM_TYPE_FLOAT:
                JSON_APPEND("%.2f", params->values[i].value.float_val);
                break;
            case UFT_PARAM_TYPE_ENUM:
                if (def->enum_values && params->values[i].value.enum_index < def->enum_count) {
                    JSON_APPEND("\"%s\"", def->enum_values[params->values[i].value.enum_index]);
                }
                break;
            case UFT_PARAM_TYPE_STRING:
            case UFT_PARAM_TYPE_PATH:
                if (params->values[i].value.string_val) {
                    JSON_APPEND("\"%s\"", params->values[i].value.string_val);
                } else {
                    JSON_APPEND("null");
                }
                break;
        }
    }
    
    JSON_APPEND("%s}", nl);
    
#undef JSON_APPEND
    
    return json;
}

uft_params_t *uft_params_from_json(const char *json) {
    if (!json) return NULL;
    
    uft_params_t *params = uft_params_create_defaults();
    if (!params) return NULL;
    
    /* Simple JSON parser - finds "key": value pairs */
    const char *p = json;
    while (*p) {
        /* Find key */
        const char *key_start = strchr(p, '"');
        if (!key_start) break;
        key_start++;
        
        const char *key_end = strchr(key_start, '"');
        if (!key_end) break;
        
        char key[64];
        size_t key_len = key_end - key_start;
        if (key_len >= sizeof(key)) key_len = sizeof(key) - 1;
        strncpy(key, key_start, key_len);
        key[key_len] = '\0';
        
        /* Find colon and value */
        const char *colon = strchr(key_end, ':');
        if (!colon) break;
        
        const char *val_start = colon + 1;
        while (*val_start && isspace(*val_start)) val_start++;
        
        /* Find param by json_key */
        int idx = -1;
        for (int i = 0; i < PARAM_COUNT; i++) {
            if (PARAM_DEFINITIONS[i].json_key && 
                strcmp(PARAM_DEFINITIONS[i].json_key, key) == 0) {
                idx = i;
                break;
            }
        }
        
        if (idx >= 0) {
            const uft_param_def_t *def = &PARAM_DEFINITIONS[idx];
            
            if (*val_start == '"') {
                /* String value */
                val_start++;
                const char *val_end = strchr(val_start, '"');
                if (val_end) {
                    char value[512];
                    size_t val_len = val_end - val_start;
                    if (val_len >= sizeof(value)) val_len = sizeof(value) - 1;
                    strncpy(value, val_start, val_len);
                    value[val_len] = '\0';
                    
                    if (def->type == UFT_PARAM_TYPE_ENUM) {
                        uft_params_set_enum_string(params, def->name, value);
                    } else {
                        uft_params_set_string(params, def->name, value);
                    }
                    p = val_end + 1;
                    continue;
                }
            } else if (strncmp(val_start, "true", 4) == 0) {
                uft_params_set_bool(params, def->name, true);
                p = val_start + 4;
                continue;
            } else if (strncmp(val_start, "false", 5) == 0) {
                uft_params_set_bool(params, def->name, false);
                p = val_start + 5;
                continue;
            } else if (*val_start == '-' || isdigit(*val_start)) {
                /* Number */
                char *end;
                if (def->type == UFT_PARAM_TYPE_FLOAT) {
                    float f = strtof(val_start, &end);
                    uft_params_set_float(params, def->name, f);
                } else {
                    int n = (int)strtol(val_start, &end, 10);
                    uft_params_set_int(params, def->name, n);
                }
                p = end;
                continue;
            }
        }
        
        p = key_end + 1;
    }
    
    return params;
}

/*===========================================================================
 * CLI CONVERSION
 *===========================================================================*/

char *uft_params_to_cli(const uft_params_t *params) {
    if (!params) return NULL;
    
    const size_t cli_size = 4096;
    char *cli = (char *)malloc(cli_size);
    if (!cli) return NULL;
    
    char *p = cli;
    char *end = cli + cli_size;
    *p = '\0';
    
    for (int i = 0; i < PARAM_COUNT; i++) {
        if (!params->values[i].is_set) continue;
        
        const uft_param_def_t *def = &PARAM_DEFINITIONS[i];
        const char *opt = def->cli_long ? def->cli_long : def->cli_short;
        if (!opt) continue;
        
        size_t remaining = (size_t)(end - p);
        if (remaining < 2) break;  /* No space left */
        
        int n = 0;
        switch (def->type) {
            case UFT_PARAM_TYPE_BOOL:
                if (params->values[i].value.bool_val) {
                    n = snprintf(p, remaining, "%s ", opt);
                }
                break;
            case UFT_PARAM_TYPE_INT:
            case UFT_PARAM_TYPE_RANGE:
                n = snprintf(p, remaining, "%s %d ", opt, params->values[i].value.int_val);
                break;
            case UFT_PARAM_TYPE_FLOAT:
                n = snprintf(p, remaining, "%s %.2f ", opt, params->values[i].value.float_val);
                break;
            case UFT_PARAM_TYPE_ENUM:
                if (def->enum_values && params->values[i].value.enum_index < def->enum_count) {
                    n = snprintf(p, remaining, "%s %s ", opt, 
                                 def->enum_values[params->values[i].value.enum_index]);
                }
                break;
            case UFT_PARAM_TYPE_STRING:
            case UFT_PARAM_TYPE_PATH:
                if (params->values[i].value.string_val) {
                    n = snprintf(p, remaining, "%s \"%s\" ", opt, params->values[i].value.string_val);
                }
                break;
        }
        if (n > 0 && (size_t)n < remaining) p += n;
    }
    
    /* Remove trailing space */
    if (p > cli && *(p-1) == ' ') *(p-1) = '\0';
    
    return cli;
}

/*===========================================================================
 * PRESETS
 *===========================================================================*/

uft_params_t *uft_params_load_preset(const char *name) {
    if (!name) return NULL;
    
    for (int i = 0; i < PRESET_COUNT; i++) {
        if (PRESETS[i].name && strcmp(PRESETS[i].name, name) == 0) {
            return uft_params_from_json(PRESETS[i].json_params);
        }
    }
    
    return NULL;
}

uft_error_t uft_params_apply_preset(uft_params_t *params, const char *name) {
    if (!params || !name) return UFT_ERR_INVALID_ARG;
    
    uft_params_t *preset = uft_params_load_preset(name);
    if (!preset) return UFT_ERR_FILE_NOT_FOUND;
    
    /* Merge preset into params */
    for (int i = 0; i < PARAM_COUNT; i++) {
        if (preset->values[i].is_set) {
            params->values[i] = preset->values[i];
            /* Deep copy strings */
            if ((PARAM_DEFINITIONS[i].type == UFT_PARAM_TYPE_STRING ||
                 PARAM_DEFINITIONS[i].type == UFT_PARAM_TYPE_PATH) &&
                preset->values[i].value.string_val) {
                params->values[i].value.string_val = strdup(preset->values[i].value.string_val);
            }
        }
    }
    
    uft_params_free(preset);
    return UFT_OK;
}

const uft_preset_t *uft_params_get_preset_info(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < PRESET_COUNT; i++) {
        if (PRESETS[i].name && strcmp(PRESETS[i].name, name) == 0) {
            return &PRESETS[i];
        }
    }
    return NULL;
}

/*===========================================================================
 * DEFINITIONS ACCESS
 *===========================================================================*/

const uft_param_def_t *uft_params_get_definition(const char *name) {
    int idx = find_param_index(name);
    if (idx < 0) return NULL;
    return &PARAM_DEFINITIONS[idx];
}

const char *uft_params_widget_to_param(const char *widget_name) {
    if (!widget_name) return NULL;
    for (int i = 0; i < PARAM_COUNT; i++) {
        if (PARAM_DEFINITIONS[i].gui_widget &&
            strcmp(PARAM_DEFINITIONS[i].gui_widget, widget_name) == 0) {
            return PARAM_DEFINITIONS[i].name;
        }
    }
    return NULL;
}

const char *uft_params_param_to_widget(const char *param_name) {
    const uft_param_def_t *def = uft_params_get_definition(param_name);
    return def ? def->gui_widget : NULL;
}

/*===========================================================================
 * UTILITY
 *===========================================================================*/

const char *uft_param_category_string(uft_param_category_t category) {
    static const char *names[] = {
        "General", "Format", "Hardware", "Recovery",
        "Encoding", "PLL", "Output", "Debug", "Advanced"
    };
    if (category > UFT_PARAM_CAT_ADVANCED) return "Unknown";
    return names[category];
}

const char *uft_param_type_string(uft_param_type_t type) {
    static const char *names[] = {
        "Bool", "Int", "Float", "String", "Enum", "Path", "Range"
    };
    if (type > UFT_PARAM_TYPE_RANGE) return "Unknown";
    return names[type];
}

void uft_params_print(const uft_params_t *params) {
    if (!params) return;
    
    printf("Parameters:\n");
    for (int i = 0; i < PARAM_COUNT; i++) {
        if (!params->values[i].is_set && !params->values[i].is_default) continue;
        
        const uft_param_def_t *def = &PARAM_DEFINITIONS[i];
        printf("  %s: ", def->name);
        
        switch (def->type) {
            case UFT_PARAM_TYPE_BOOL:
                printf("%s", params->values[i].value.bool_val ? "true" : "false");
                break;
            case UFT_PARAM_TYPE_INT:
            case UFT_PARAM_TYPE_RANGE:
                printf("%d", params->values[i].value.int_val);
                break;
            case UFT_PARAM_TYPE_FLOAT:
                printf("%.2f", params->values[i].value.float_val);
                break;
            case UFT_PARAM_TYPE_ENUM:
                printf("%s", uft_params_get_enum_string(params, def->name));
                break;
            case UFT_PARAM_TYPE_STRING:
            case UFT_PARAM_TYPE_PATH:
                printf("%s", params->values[i].value.string_val ? 
                       params->values[i].value.string_val : "(null)");
                break;
        }
        
        if (params->values[i].is_default) printf(" (default)");
        printf("\n");
    }
}
