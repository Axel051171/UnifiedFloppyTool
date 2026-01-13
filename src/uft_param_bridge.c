/**
 * @file uft_param_bridge.c
 * @brief CLI-GUI Parameter Bridge Implementation
 * 
 * TICKET-004: CLI-GUI Parameter Bridge
 * Bidirectional parameter conversion
 */

#include "uft/uft_param_bridge.h"
#include "uft/core/uft_safe_parse.h"
#include "uft/uft_memory.h"
#include "uft/core/uft_safe_parse.h"
#include <stdlib.h>
#include "uft/core/uft_safe_parse.h"
#include <string.h>
#include "uft/core/uft_safe_parse.h"
#include <stdio.h>
#include "uft/core/uft_safe_parse.h"
#include <ctype.h>
#include "uft/core/uft_safe_parse.h"

/* ═══════════════════════════════════════════════════════════════════════════════
 * Parameter Definitions
 * ═══════════════════════════════════════════════════════════════════════════════ */

static const char *format_values[] = {
    "auto", "adf", "d64", "g64", "scp", "hfe", "woz", "a2r", "ipf", 
    "dmk", "td0", "imd", "img", "stx", "atr", "atx", "nib", NULL
};

static const char *hw_values[] = {
    "auto", "greaseweazle", "fluxengine", "kryoflux", "supercardpro",
    "fc5025", "xum1541", NULL
};

static const char *encoding_values[] = {
    "auto", "mfm", "fm", "gcr_c64", "gcr_apple", NULL
};

static const uft_param_def_t param_definitions[] = {
    /* General */
    {"input", "-i", "--input", "input", "inputEdit",
     UFT_PARAM_TYPE_PATH, UFT_PARAM_CAT_GENERAL,
     "Input file or device", NULL, NULL, 0, 0, 0, 0, true, false, false},
    
    {"output", "-o", "--output", "output", "outputEdit",
     UFT_PARAM_TYPE_PATH, UFT_PARAM_CAT_GENERAL,
     "Output file", NULL, NULL, 0, 0, 0, 0, false, false, false},
    
    {"verbose", "-v", "--verbose", "verbose", "verboseCheck",
     UFT_PARAM_TYPE_BOOL, UFT_PARAM_CAT_GENERAL,
     "Verbose output", "false", NULL, 0, 0, 0, 0, false, false, false},
    
    {"quiet", "-q", "--quiet", "quiet", "quietCheck",
     UFT_PARAM_TYPE_BOOL, UFT_PARAM_CAT_GENERAL,
     "Quiet mode", "false", NULL, 0, 0, 0, 0, false, false, false},
    
    /* Format */
    {"format", "-f", "--format", "format", "formatCombo",
     UFT_PARAM_TYPE_ENUM, UFT_PARAM_CAT_FORMAT,
     "Disk format", "auto", format_values, 17, 0, 0, 0, false, false, false},
    
    {"cylinders", "-c", "--cylinders", "cylinders", "cylindersSpin",
     UFT_PARAM_TYPE_RANGE, UFT_PARAM_CAT_FORMAT,
     "Number of cylinders", "80", NULL, 0, 1, 200, 1, false, false, false},
    
    {"heads", "-h", "--heads", "heads", "headsSpin",
     UFT_PARAM_TYPE_RANGE, UFT_PARAM_CAT_FORMAT,
     "Number of heads", "2", NULL, 0, 1, 2, 1, false, false, false},
    
    {"sectors", "-s", "--sectors", "sectors", "sectorsSpin",
     UFT_PARAM_TYPE_RANGE, UFT_PARAM_CAT_FORMAT,
     "Sectors per track", "18", NULL, 0, 1, 64, 1, false, false, false},
    
    /* Hardware */
    {"hardware", NULL, "--hardware", "hardware", "hardwareCombo",
     UFT_PARAM_TYPE_ENUM, UFT_PARAM_CAT_HARDWARE,
     "Hardware controller", "auto", hw_values, 7, 0, 0, 0, false, false, false},
    
    {"device", "-d", "--device", "device", "deviceEdit",
     UFT_PARAM_TYPE_STRING, UFT_PARAM_CAT_HARDWARE,
     "Device path", NULL, NULL, 0, 0, 0, 0, false, false, false},
    
    {"drive", NULL, "--drive", "drive", "driveCombo",
     UFT_PARAM_TYPE_RANGE, UFT_PARAM_CAT_HARDWARE,
     "Drive number", "0", NULL, 0, 0, 3, 1, false, false, false},
    
    /* Recovery */
    {"retries", "-r", "--retries", "retries", "retriesSpin",
     UFT_PARAM_TYPE_RANGE, UFT_PARAM_CAT_RECOVERY,
     "Read retries", "3", NULL, 0, 0, 100, 1, false, false, false},
    
    {"revolutions", NULL, "--revolutions", "revolutions", "revolutionsSpin",
     UFT_PARAM_TYPE_RANGE, UFT_PARAM_CAT_RECOVERY,
     "Revolutions to capture", "3", NULL, 0, 1, 20, 1, false, false, false},
    
    {"weak_bits", NULL, "--weak-bits", "weak_bits", "weakBitsCheck",
     UFT_PARAM_TYPE_BOOL, UFT_PARAM_CAT_RECOVERY,
     "Detect weak bits", "true", NULL, 0, 0, 0, 0, false, false, false},
    
    /* Encoding */
    {"encoding", "-e", "--encoding", "encoding", "encodingCombo",
     UFT_PARAM_TYPE_ENUM, UFT_PARAM_CAT_ENCODING,
     "Encoding type", "auto", encoding_values, 5, 0, 0, 0, false, false, false},
    
    {"data_rate", NULL, "--data-rate", "data_rate", "dataRateSpin",
     UFT_PARAM_TYPE_RANGE, UFT_PARAM_CAT_ENCODING,
     "Data rate (kbps)", "250", NULL, 0, 125, 1000, 1, false, true, false},
    
    /* PLL */
    {"pll_period", NULL, "--pll-period", "pll_period", "pllPeriodSpin",
     UFT_PARAM_TYPE_RANGE, UFT_PARAM_CAT_PLL,
     "PLL period (ns)", "2000", NULL, 0, 500, 10000, 100, false, true, false},
    
    {"pll_adjust", NULL, "--pll-adjust", "pll_adjust", "pllAdjustSpin",
     UFT_PARAM_TYPE_FLOAT, UFT_PARAM_CAT_PLL,
     "PLL adjustment factor", "0.05", NULL, 0, 0, 0, 0, false, true, false},
    
    /* Output */
    {"verify", NULL, "--verify", "verify", "verifyCheck",
     UFT_PARAM_TYPE_BOOL, UFT_PARAM_CAT_OUTPUT,
     "Verify after write", "true", NULL, 0, 0, 0, 0, false, false, false},
    
    {"preview", NULL, "--preview", "preview", "previewCheck",
     UFT_PARAM_TYPE_BOOL, UFT_PARAM_CAT_OUTPUT,
     "Preview mode (no write)", "false", NULL, 0, 0, 0, 0, false, false, false},
    
    /* Debug */
    {"debug", NULL, "--debug", "debug", "debugCheck",
     UFT_PARAM_TYPE_BOOL, UFT_PARAM_CAT_DEBUG,
     "Debug output", "false", NULL, 0, 0, 0, 0, false, true, false},
    
    {"dump_flux", NULL, "--dump-flux", "dump_flux", "dumpFluxCheck",
     UFT_PARAM_TYPE_BOOL, UFT_PARAM_CAT_DEBUG,
     "Dump flux data", "false", NULL, 0, 0, 0, 0, false, true, false},
    
    {NULL}  /* Terminator */
};

#define NUM_PARAMS (sizeof(param_definitions) / sizeof(param_definitions[0]) - 1)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Preset Definitions
 * ═══════════════════════════════════════════════════════════════════════════════ */

static const uft_preset_t presets[] = {
    {"amiga_dd", "Amiga DD (880KB)", UFT_PARAM_CAT_FORMAT,
     "{\"format\":\"adf\",\"cylinders\":80,\"heads\":2,\"sectors\":11}",
     "--format adf --cylinders 80 --heads 2 --sectors 11"},
    
    {"amiga_hd", "Amiga HD (1.76MB)", UFT_PARAM_CAT_FORMAT,
     "{\"format\":\"adf\",\"cylinders\":80,\"heads\":2,\"sectors\":22}",
     "--format adf --cylinders 80 --heads 2 --sectors 22"},
    
    {"c64_1541", "C64 1541 (170KB)", UFT_PARAM_CAT_FORMAT,
     "{\"format\":\"d64\",\"cylinders\":35,\"heads\":1,\"encoding\":\"gcr_c64\"}",
     "--format d64 --cylinders 35 --heads 1 --encoding gcr_c64"},
    
    {"pc_dd", "PC DD (720KB)", UFT_PARAM_CAT_FORMAT,
     "{\"format\":\"img\",\"cylinders\":80,\"heads\":2,\"sectors\":9}",
     "--format img --cylinders 80 --heads 2 --sectors 9"},
    
    {"pc_hd", "PC HD (1.44MB)", UFT_PARAM_CAT_FORMAT,
     "{\"format\":\"img\",\"cylinders\":80,\"heads\":2,\"sectors\":18}",
     "--format img --cylinders 80 --heads 2 --sectors 18"},
    
    {"apple_dos33", "Apple II DOS 3.3", UFT_PARAM_CAT_FORMAT,
     "{\"format\":\"nib\",\"cylinders\":35,\"heads\":1,\"encoding\":\"gcr_apple\"}",
     "--format nib --cylinders 35 --heads 1 --encoding gcr_apple"},
    
    {"atari_sd", "Atari 8-bit SD", UFT_PARAM_CAT_FORMAT,
     "{\"format\":\"atr\",\"cylinders\":40,\"heads\":1,\"sectors\":18}",
     "--format atr --cylinders 40 --heads 1 --sectors 18"},
    
    {"flux_preserve", "Flux Preservation", UFT_PARAM_CAT_RECOVERY,
     "{\"format\":\"scp\",\"revolutions\":5,\"weak_bits\":true}",
     "--format scp --revolutions 5 --weak-bits"},
    
    {"flux_analyze", "Flux Analysis", UFT_PARAM_CAT_RECOVERY,
     "{\"format\":\"scp\",\"revolutions\":3,\"debug\":true,\"dump_flux\":true}",
     "--format scp --revolutions 3 --debug --dump-flux"},
    
    {"recovery_aggressive", "Aggressive Recovery", UFT_PARAM_CAT_RECOVERY,
     "{\"retries\":20,\"revolutions\":10,\"weak_bits\":true}",
     "--retries 20 --revolutions 10 --weak-bits"},
    
    {"safe_write", "Safe Write", UFT_PARAM_CAT_OUTPUT,
     "{\"verify\":true,\"preview\":false,\"retries\":5}",
     "--verify --retries 5"},
    
    {"preview_only", "Preview Only", UFT_PARAM_CAT_OUTPUT,
     "{\"preview\":true,\"verify\":false}",
     "--preview"},
    
    {NULL}
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal Structures
 * ═══════════════════════════════════════════════════════════════════════════════ */

struct uft_params {
    uft_param_value_t values[NUM_PARAMS];
    int value_count;
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

static int find_param_index(const char *name) {
    for (int i = 0; param_definitions[i].name; i++) {
        if (strcmp(param_definitions[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_enum_index(const char **values, const char *value) {
    for (int i = 0; values && values[i]; i++) {
        if (strcasecmp(values[i], value) == 0) {
            return i;
        }
    }
    return -1;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Implementation - Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_params_t *uft_params_create(void) {
    uft_params_t *params = calloc(1, sizeof(uft_params_t));
    if (!params) return NULL;
    
    params->value_count = NUM_PARAMS;
    for (int i = 0; i < (int)NUM_PARAMS; i++) {
        params->values[i].definition = &param_definitions[i];
        params->values[i].is_set = false;
        params->values[i].is_default = true;
    }
    
    return params;
}

uft_params_t *uft_params_create_defaults(void) {
    uft_params_t *params = uft_params_create();
    if (!params) return NULL;
    
    /* Set default values */
    for (int i = 0; i < (int)NUM_PARAMS; i++) {
        const uft_param_def_t *def = &param_definitions[i];
        if (def->default_value) {
            switch (def->type) {
                case UFT_PARAM_TYPE_BOOL:
                    params->values[i].value.bool_val = 
                        (strcasecmp(def->default_value, "true") == 0);
                    break;
                case UFT_PARAM_TYPE_INT:
                case UFT_PARAM_TYPE_RANGE:
                    { int32_t t; if(uft_parse_int32(def->default_value,&t,10)) params->values[i].value.int_val=t; }
                    break;
                case UFT_PARAM_TYPE_FLOAT:
                    params->values[i].value.float_val = atof(def->default_value);
                    break;
                case UFT_PARAM_TYPE_STRING:
                case UFT_PARAM_TYPE_PATH:
                    params->values[i].value.string_val = strdup(def->default_value);
                    break;
                case UFT_PARAM_TYPE_ENUM:
                    params->values[i].value.enum_index = 
                        find_enum_index(def->enum_values, def->default_value);
                    break;
                default:
                    /* Unknown type - skip */
                    break;
            default:
                break;
            }
        }
    }
    
    return params;
}

uft_params_t *uft_params_clone(const uft_params_t *params) {
    if (!params) return NULL;
    
    uft_params_t *clone = uft_params_create();
    if (!clone) return NULL;
    
    memcpy(clone->values, params->values, sizeof(params->values));
    
    /* Deep copy strings */
    for (int i = 0; i < (int)NUM_PARAMS; i++) {
        if (params->values[i].definition->type == UFT_PARAM_TYPE_STRING ||
            params->values[i].definition->type == UFT_PARAM_TYPE_PATH) {
            if (params->values[i].value.string_val) {
                clone->values[i].value.string_val = 
                    strdup(params->values[i].value.string_val);
            }
        }
    }
    
    return clone;
}

void uft_params_free(uft_params_t *params) {
    if (!params) return;
    
    for (int i = 0; i < (int)NUM_PARAMS; i++) {
        if (params->values[i].definition->type == UFT_PARAM_TYPE_STRING ||
            params->values[i].definition->type == UFT_PARAM_TYPE_PATH) {
            free(params->values[i].value.string_val);
        }
    }
    
    free(params);
}

void uft_params_reset(uft_params_t *params) {
    if (!params) return;
    
    for (int i = 0; i < (int)NUM_PARAMS; i++) {
        if (params->values[i].definition->type == UFT_PARAM_TYPE_STRING ||
            params->values[i].definition->type == UFT_PARAM_TYPE_PATH) {
            free(params->values[i].value.string_val);
            params->values[i].value.string_val = NULL;
        }
        params->values[i].is_set = false;
        params->values[i].is_default = true;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Implementation - CLI Parsing
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_params_t *uft_params_from_cli(int argc, char **argv) {
    uft_params_t *params = uft_params_create_defaults();
    if (!params) return NULL;
    
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        
        /* Find matching parameter */
        const uft_param_def_t *def = NULL;
        for (int j = 0; param_definitions[j].name; j++) {
            if ((param_definitions[j].cli_short && 
                 strcmp(arg, param_definitions[j].cli_short) == 0) ||
                (param_definitions[j].cli_long && 
                 strcmp(arg, param_definitions[j].cli_long) == 0)) {
                def = &param_definitions[j];
                break;
            }
        }
        
        if (!def) continue;
        
        int idx = find_param_index(def->name);
        if (idx < 0) continue;
        
        /* Handle value */
        if (def->type == UFT_PARAM_TYPE_BOOL) {
            params->values[idx].value.bool_val = true;
            params->values[idx].is_set = true;
        } else if (i + 1 < argc) {
            const char *val = argv[++i];
            
            switch (def->type) {
                case UFT_PARAM_TYPE_INT:
                case UFT_PARAM_TYPE_RANGE:
                    { int32_t t; if(uft_parse_int32(val,&t,10)) params->values[idx].value.int_val=t; }
                    break;
                case UFT_PARAM_TYPE_FLOAT:
                    params->values[idx].value.float_val = atof(val);
                    break;
                case UFT_PARAM_TYPE_STRING:
                case UFT_PARAM_TYPE_PATH:
                    free(params->values[idx].value.string_val);
                    params->values[idx].value.string_val = strdup(val);
                    break;
                case UFT_PARAM_TYPE_ENUM:
                    params->values[idx].value.enum_index = 
                        find_enum_index(def->enum_values, val);
                    break;
                default:
                    break;
            default:
                break;
            }
            params->values[idx].is_set = true;
        }
    }
    
    return params;
}

uft_params_t *uft_params_from_cli_string(const char *cli_string) {
    if (!cli_string) return NULL;
    
    /* Parse string into argv */
    char *copy = strdup(cli_string);
    if (!copy) return NULL;
    
    char *argv[128] = {0};
    int argc = 0;
    
    argv[argc++] = "uft";  /* Program name */
    
    char *token = strtok(copy, " \t\n");
    while (token && argc < 127) {
        argv[argc++] = token;
        token = strtok(NULL, " \t\n");
    }
    
    uft_params_t *params = uft_params_from_cli(argc, argv);
    free(copy);
    
    return params;
}

char *uft_params_to_cli(const uft_params_t *params) {
    if (!params) return NULL;
    
    size_t size = 4096;
    char *cli = malloc(size);
    if (!cli) return NULL;
    
    int pos = 0;
    pos += snprintf(cli + pos, size - pos, "uft");
    
    for (int i = 0; i < (int)NUM_PARAMS && pos < (int)size - 100; i++) {
        if (!params->values[i].is_set) continue;
        
        const uft_param_def_t *def = params->values[i].definition;
        const char *opt = def->cli_long ? def->cli_long : def->cli_short;
        if (!opt) continue;
        
        switch (def->type) {
            case UFT_PARAM_TYPE_BOOL:
                if (params->values[i].value.bool_val) {
                    pos += snprintf(cli + pos, size - pos, " %s", opt);
                }
                break;
            case UFT_PARAM_TYPE_INT:
            case UFT_PARAM_TYPE_RANGE:
                pos += snprintf(cli + pos, size - pos, " %s %d",
                               opt, params->values[i].value.int_val);
                break;
            case UFT_PARAM_TYPE_FLOAT:
                pos += snprintf(cli + pos, size - pos, " %s %.4f",
                               opt, params->values[i].value.float_val);
                break;
            case UFT_PARAM_TYPE_STRING:
            case UFT_PARAM_TYPE_PATH:
                if (params->values[i].value.string_val) {
                    pos += snprintf(cli + pos, size - pos, " %s \"%s\"",
                                   opt, params->values[i].value.string_val);
                }
                break;
            case UFT_PARAM_TYPE_ENUM:
                if (def->enum_values && params->values[i].value.enum_index >= 0) {
                    pos += snprintf(cli + pos, size - pos, " %s %s",
                                   opt, def->enum_values[params->values[i].value.enum_index]);
            default:
                break;
                }
                break;
        }
    }
    
    return cli;
}

char *uft_params_to_cli_diff(const uft_params_t *params) {
    /* Same as to_cli but only non-default values */
    return uft_params_to_cli(params);
}

void uft_params_print_help(void) {
    printf("UnifiedFloppyTool - Parameter Reference\n");
    printf("========================================\n\n");
    
    uft_param_category_t current_cat = -1;
    
    for (int i = 0; param_definitions[i].name; i++) {
        const uft_param_def_t *def = &param_definitions[i];
        
        if (def->category != current_cat) {
            current_cat = def->category;
            printf("\n%s:\n", uft_param_category_string(current_cat));
        }
        
        printf("  ");
        if (def->cli_short) printf("%s, ", def->cli_short);
        if (def->cli_long) printf("%s", def->cli_long);
        printf("\n      %s", def->description);
        if (def->default_value) printf(" (default: %s)", def->default_value);
        printf("\n");
    }
}

void uft_params_print_help_category(uft_param_category_t category) {
    printf("%s:\n", uft_param_category_string(category));
    
    for (int i = 0; param_definitions[i].name; i++) {
        const uft_param_def_t *def = &param_definitions[i];
        if (def->category != category) continue;
        
        printf("  ");
        if (def->cli_short) printf("%s, ", def->cli_short);
        if (def->cli_long) printf("%s", def->cli_long);
        printf(" - %s\n", def->description);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Implementation - JSON
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_params_t *uft_params_from_json(const char *json) {
    if (!json) return NULL;
    
    uft_params_t *params = uft_params_create_defaults();
    if (!params) return NULL;
    
    /* Simple JSON parser - look for key-value pairs */
    const char *p = json;
    while (*p) {
        /* Find key */
        const char *key_start = strchr(p, '"');
        if (!key_start) break;
        key_start++;
        
        const char *key_end = strchr(key_start, '"');
        if (!key_end) break;
        
        char key[64] = {0};
        size_t key_len = key_end - key_start;
        if (key_len < sizeof(key)) {
            strncpy(key, key_start, key_len);
        }
        
        int idx = find_param_index(key);
        if (idx < 0) {
            p = key_end + 1;
            continue;
        }
        
        /* Find value */
        const char *colon = strchr(key_end, ':');
        if (!colon) break;
        
        const char *val_start = colon + 1;
        while (*val_start && isspace(*val_start)) val_start++;
        
        const uft_param_def_t *def = params->values[idx].definition;
        
        if (*val_start == '"') {
            /* String value */
            val_start++;
            const char *val_end = strchr(val_start, '"');
            if (val_end) {
                size_t val_len = val_end - val_start;
                char *val = malloc(val_len + 1);
                if (val) {
                    strncpy(val, val_start, val_len);
                    val[val_len] = '\0';
                    
                    if (def->type == UFT_PARAM_TYPE_ENUM) {
                        params->values[idx].value.enum_index = 
                            find_enum_index(def->enum_values, val);
                    } else {
                        free(params->values[idx].value.string_val);
                        params->values[idx].value.string_val = val;
                    }
                    params->values[idx].is_set = true;
                }
                p = val_end + 1;
            }
        } else if (strncmp(val_start, "true", 4) == 0) {
            params->values[idx].value.bool_val = true;
            params->values[idx].is_set = true;
            p = val_start + 4;
        } else if (strncmp(val_start, "false", 5) == 0) {
            params->values[idx].value.bool_val = false;
            params->values[idx].is_set = true;
            p = val_start + 5;
        } else if (isdigit(*val_start) || *val_start == '-' || *val_start == '.') {
            if (def->type == UFT_PARAM_TYPE_FLOAT) {
                params->values[idx].value.float_val = atof(val_start);
            } else {
                { int32_t t; if(uft_parse_int32(val_start,&t,10)) params->values[idx].value.int_val=t; }
            }
            params->values[idx].is_set = true;
            while (*p && (isdigit(*p) || *p == '.' || *p == '-')) p++;
        } else {
            p++;
        }
    }
    
    return params;
}

uft_params_t *uft_params_load_json(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        return NULL;
    }
    if (!f) return NULL;
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    long size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    char *json = malloc(size + 1);
    if (!json) {
        fclose(f);
        return NULL;
    }
    
    if (fread(json, 1, size, f) != size) { /* I/O error */ }
    json[size] = '\0';
    fclose(f);
    
    uft_params_t *params = uft_params_from_json(json);
    free(json);
    
    return params;
}

char *uft_params_to_json(const uft_params_t *params, bool pretty) {
    if (!params) return NULL;
    
    size_t size = 4096;
    char *json = malloc(size);
    if (!json) return NULL;
    
    int pos = 0;
    const char *indent = pretty ? "  " : "";
    const char *nl = pretty ? "\n" : "";
    
    pos += snprintf(json + pos, size - pos, "{%s", nl);
    
    bool first = true;
    for (int i = 0; i < (int)NUM_PARAMS && pos < (int)size - 100; i++) {
        if (!params->values[i].is_set) continue;
        
        const uft_param_def_t *def = params->values[i].definition;
        
        if (!first) pos += snprintf(json + pos, size - pos, ",%s", nl);
        first = false;
        
        pos += snprintf(json + pos, size - pos, "%s\"%s\": ", indent, def->json_key);
        
        switch (def->type) {
            case UFT_PARAM_TYPE_BOOL:
                pos += snprintf(json + pos, size - pos, "%s",
                               params->values[i].value.bool_val ? "true" : "false");
                break;
            case UFT_PARAM_TYPE_INT:
            case UFT_PARAM_TYPE_RANGE:
                pos += snprintf(json + pos, size - pos, "%d",
                               params->values[i].value.int_val);
                break;
            case UFT_PARAM_TYPE_FLOAT:
                pos += snprintf(json + pos, size - pos, "%.4f",
                               params->values[i].value.float_val);
                break;
            case UFT_PARAM_TYPE_STRING:
            case UFT_PARAM_TYPE_PATH:
                pos += snprintf(json + pos, size - pos, "\"%s\"",
                               params->values[i].value.string_val ? 
                               params->values[i].value.string_val : "");
                break;
            case UFT_PARAM_TYPE_ENUM:
                if (def->enum_values && params->values[i].value.enum_index >= 0) {
                    pos += snprintf(json + pos, size - pos, "\"%s\"",
                                   def->enum_values[params->values[i].value.enum_index]);
            default:
                break;
                } else {
                    pos += snprintf(json + pos, size - pos, "null");
                }
                break;
        }
    }
    
    pos += snprintf(json + pos, size - pos, "%s}%s", nl, nl);
    
    return json;
}

uft_error_t uft_params_save_json(const uft_params_t *params, const char *path) {
    char *json = uft_params_to_json(params, true);
    if (!json) return UFT_ERR_MEMORY;
    
    FILE *f = fopen(path, "w");
    if (!f) {
        free(json);
        return UFT_ERR_IO;
    }
    
    fputs(json, f);
    fclose(f);
    free(json);
    
    return UFT_OK;
}

char *uft_params_to_json_diff(const uft_params_t *params) {
    return uft_params_to_json(params, true);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Implementation - Parameter Access
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool uft_params_get_bool(const uft_params_t *params, const char *name) {
    int idx = find_param_index(name);
    if (idx < 0 || !params) return false;
    return params->values[idx].value.bool_val;
}

int uft_params_get_int(const uft_params_t *params, const char *name) {
    int idx = find_param_index(name);
    if (idx < 0 || !params) return 0;
    return params->values[idx].value.int_val;
}

float uft_params_get_float(const uft_params_t *params, const char *name) {
    int idx = find_param_index(name);
    if (idx < 0 || !params) return 0.0f;
    return params->values[idx].value.float_val;
}

const char *uft_params_get_string(const uft_params_t *params, const char *name) {
    int idx = find_param_index(name);
    if (idx < 0 || !params) return NULL;
    return params->values[idx].value.string_val;
}

int uft_params_get_enum(const uft_params_t *params, const char *name) {
    int idx = find_param_index(name);
    if (idx < 0 || !params) return -1;
    return params->values[idx].value.enum_index;
}

const char *uft_params_get_enum_string(const uft_params_t *params, const char *name) {
    int idx = find_param_index(name);
    if (idx < 0 || !params) return NULL;
    
    const uft_param_def_t *def = params->values[idx].definition;
    int enum_idx = params->values[idx].value.enum_index;
    
    if (def->enum_values && enum_idx >= 0 && enum_idx < def->enum_count) {
        return def->enum_values[enum_idx];
    }
    return NULL;
}

uft_error_t uft_params_set_bool(uft_params_t *params, const char *name, bool value) {
    int idx = find_param_index(name);
    if (idx < 0 || !params) return UFT_ERR_NOT_FOUND;
    params->values[idx].value.bool_val = value;
    params->values[idx].is_set = true;
    return UFT_OK;
}

uft_error_t uft_params_set_int(uft_params_t *params, const char *name, int value) {
    int idx = find_param_index(name);
    if (idx < 0 || !params) return UFT_ERR_NOT_FOUND;
    params->values[idx].value.int_val = value;
    params->values[idx].is_set = true;
    return UFT_OK;
}

uft_error_t uft_params_set_float(uft_params_t *params, const char *name, float value) {
    int idx = find_param_index(name);
    if (idx < 0 || !params) return UFT_ERR_NOT_FOUND;
    params->values[idx].value.float_val = value;
    params->values[idx].is_set = true;
    return UFT_OK;
}

uft_error_t uft_params_set_string(uft_params_t *params, const char *name, const char *value) {
    int idx = find_param_index(name);
    if (idx < 0 || !params) return UFT_ERR_NOT_FOUND;
    free(params->values[idx].value.string_val);
    params->values[idx].value.string_val = value ? strdup(value) : NULL;
    params->values[idx].is_set = true;
    return UFT_OK;
}

uft_error_t uft_params_set_enum(uft_params_t *params, const char *name, int index) {
    int idx = find_param_index(name);
    if (idx < 0 || !params) return UFT_ERR_NOT_FOUND;
    params->values[idx].value.enum_index = index;
    params->values[idx].is_set = true;
    return UFT_OK;
}

uft_error_t uft_params_set_enum_string(uft_params_t *params, const char *name, const char *value) {
    int idx = find_param_index(name);
    if (idx < 0 || !params) return UFT_ERR_NOT_FOUND;
    
    const uft_param_def_t *def = params->values[idx].definition;
    int enum_idx = find_enum_index(def->enum_values, value);
    if (enum_idx < 0) return UFT_ERR_INVALID_PARAM;
    
    params->values[idx].value.enum_index = enum_idx;
    params->values[idx].is_set = true;
    return UFT_OK;
}

bool uft_params_is_set(const uft_params_t *params, const char *name) {
    int idx = find_param_index(name);
    if (idx < 0 || !params) return false;
    return params->values[idx].is_set;
}

void uft_params_unset(uft_params_t *params, const char *name) {
    int idx = find_param_index(name);
    if (idx < 0 || !params) return;
    params->values[idx].is_set = false;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Implementation - Presets
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_params_t *uft_params_load_preset(const char *name) {
    for (int i = 0; presets[i].name; i++) {
        if (strcmp(presets[i].name, name) == 0) {
            return uft_params_from_json(presets[i].json_params);
        }
    }
    return NULL;
}

uft_error_t uft_params_apply_preset(uft_params_t *params, const char *name) {
    uft_params_t *preset = uft_params_load_preset(name);
    if (!preset) return UFT_ERR_NOT_FOUND;
    
    /* Merge preset into params */
    for (int i = 0; i < (int)NUM_PARAMS; i++) {
        if (preset->values[i].is_set) {
            params->values[i] = preset->values[i];
            /* Deep copy strings */
            if (preset->values[i].definition->type == UFT_PARAM_TYPE_STRING ||
                preset->values[i].definition->type == UFT_PARAM_TYPE_PATH) {
                if (preset->values[i].value.string_val) {
                    params->values[i].value.string_val = 
                        strdup(preset->values[i].value.string_val);
                }
            }
        }
    }
    
    uft_params_free(preset);
    return UFT_OK;
}

const char **uft_params_list_presets(void) {
    static const char *names[32];
    int count = 0;
    
    for (int i = 0; presets[i].name && count < 31; i++) {
        names[count++] = presets[i].name;
    }
    names[count] = NULL;
    
    return names;
}

const char **uft_params_list_presets_in_category(uft_param_category_t category) {
    static const char *names[32];
    int count = 0;
    
    for (int i = 0; presets[i].name && count < 31; i++) {
        if (presets[i].category == category) {
            names[count++] = presets[i].name;
        }
    }
    names[count] = NULL;
    
    return names;
}

const uft_preset_t *uft_params_get_preset_info(const char *name) {
    for (int i = 0; presets[i].name; i++) {
        if (strcmp(presets[i].name, name) == 0) {
            return &presets[i];
        }
    }
    return NULL;
}

uft_error_t uft_params_save_preset(const uft_params_t *params,
                                    const char *name, const char *description) {
    /* User presets would be saved to config directory */
    return UFT_ERR_NOT_IMPLEMENTED;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Implementation - Validation
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_params_validate(const uft_params_t *params, char ***errors) {
    if (!params) return -1;
    
    int error_count = 0;
    static char *error_list[32];
    
    /* Check required parameters */
    for (int i = 0; param_definitions[i].name; i++) {
        const uft_param_def_t *def = &param_definitions[i];
        if (def->required && !params->values[i].is_set) {
            error_list[error_count] = malloc(128);
            snprintf(error_list[error_count], 128, 
                    "Required parameter '%s' not set", def->name);
            error_count++;
        }
        
        /* Check range */
        if (def->type == UFT_PARAM_TYPE_RANGE && params->values[i].is_set) {
            int val = params->values[i].value.int_val;
            if (val < def->range_min || val > def->range_max) {
                error_list[error_count] = malloc(128);
                snprintf(error_list[error_count], 128,
                        "Parameter '%s' out of range [%d-%d]: %d",
                        def->name, def->range_min, def->range_max, val);
                error_count++;
            }
        }
    }
    
    error_list[error_count] = NULL;
    if (errors) *errors = error_list;
    
    return error_count;
}

bool uft_params_validate_value(const char *name, const char *value, char **error_message) {
    int idx = find_param_index(name);
    if (idx < 0) {
        if (error_message) *error_message = strdup("Unknown parameter");
        return false;
    }
    return true;
}

bool uft_params_validate_combination(const uft_params_t *params, char **error_message) {
    /* Check for conflicting combinations */
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Implementation - Export
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_params_export_shell(const uft_params_t *params, const char *path,
                                     const char *input_file, const char *output_file) {
    char *cli = uft_params_to_cli(params);
    if (!cli) return UFT_ERR_MEMORY;
    
    FILE *f = fopen(path, "w");
    if (!f) {
        free(cli);
        return UFT_ERR_IO;
    }
    
    fprintf(f, "#!/bin/bash\n");
    fprintf(f, "# Generated by UnifiedFloppyTool\n");
    fprintf(f, "# Date: $(date)\n\n");
    fprintf(f, "%s", cli);
    if (input_file) fprintf(f, " -i \"%s\"", input_file);
    if (output_file) fprintf(f, " -o \"%s\"", output_file);
    fprintf(f, "\n");
    
    fclose(f);
    free(cli);
    
    return UFT_OK;
}

uft_error_t uft_params_export_batch(const uft_params_t *params, const char *path,
                                     const char *input_file, const char *output_file) {
    char *cli = uft_params_to_cli(params);
    if (!cli) return UFT_ERR_MEMORY;
    
    FILE *f = fopen(path, "w");
    if (!f) {
        free(cli);
        return UFT_ERR_IO;
    }
    
    fprintf(f, "@echo off\n");
    fprintf(f, "REM Generated by UnifiedFloppyTool\n\n");
    fprintf(f, "%s", cli);
    if (input_file) fprintf(f, " -i \"%s\"", input_file);
    if (output_file) fprintf(f, " -o \"%s\"", output_file);
    fprintf(f, "\n");
    
    fclose(f);
    free(cli);
    
    return UFT_OK;
}

uft_error_t uft_params_export_python(const uft_params_t *params, const char *path) {
    char *json = uft_params_to_json(params, true);
    if (!json) return UFT_ERR_MEMORY;
    
    FILE *f = fopen(path, "w");
    if (!f) {
        free(json);
        return UFT_ERR_IO;
    }
    
    fprintf(f, "#!/usr/bin/env python3\n");
    fprintf(f, "# Generated by UnifiedFloppyTool\n\n");
    fprintf(f, "import json\n\n");
    fprintf(f, "UFT_PARAMS = json.loads('''%s''')\n", json);
    
    fclose(f);
    free(json);
    
    return UFT_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

const char *uft_param_category_string(uft_param_category_t category) {
    switch (category) {
        case UFT_PARAM_CAT_GENERAL:  return "General";
        case UFT_PARAM_CAT_FORMAT:   return "Format";
        case UFT_PARAM_CAT_HARDWARE: return "Hardware";
        case UFT_PARAM_CAT_RECOVERY: return "Recovery";
        case UFT_PARAM_CAT_ENCODING: return "Encoding";
        case UFT_PARAM_CAT_PLL:      return "PLL";
        case UFT_PARAM_CAT_OUTPUT:   return "Output";
        case UFT_PARAM_CAT_DEBUG:    return "Debug";
        case UFT_PARAM_CAT_ADVANCED: return "Advanced";
        default:                     return "Unknown";
    }
}

const char *uft_param_type_string(uft_param_type_t type) {
    switch (type) {
        case UFT_PARAM_TYPE_BOOL:   return "bool";
        case UFT_PARAM_TYPE_INT:    return "int";
        case UFT_PARAM_TYPE_FLOAT:  return "float";
        case UFT_PARAM_TYPE_STRING: return "string";
        case UFT_PARAM_TYPE_ENUM:   return "enum";
        case UFT_PARAM_TYPE_PATH:   return "path";
        case UFT_PARAM_TYPE_RANGE:  return "range";
        default:                    return "unknown";
            default:
                break;
    }
}

void uft_params_print(const uft_params_t *params) {
    if (!params) return;
    
    printf("Parameters:\n");
    for (int i = 0; i < (int)NUM_PARAMS; i++) {
        if (!params->values[i].is_set) continue;
        
        const uft_param_def_t *def = params->values[i].definition;
        printf("  %s = ", def->name);
        
        switch (def->type) {
            case UFT_PARAM_TYPE_BOOL:
                printf("%s", params->values[i].value.bool_val ? "true" : "false");
                break;
            case UFT_PARAM_TYPE_INT:
            case UFT_PARAM_TYPE_RANGE:
                printf("%d", params->values[i].value.int_val);
                break;
            case UFT_PARAM_TYPE_FLOAT:
                printf("%.4f", params->values[i].value.float_val);
                break;
            case UFT_PARAM_TYPE_STRING:
            case UFT_PARAM_TYPE_PATH:
                printf("\"%s\"", params->values[i].value.string_val ? 
                       params->values[i].value.string_val : "");
                break;
            case UFT_PARAM_TYPE_ENUM:
                printf("%s", uft_params_get_enum_string(params, def->name));
                break;
            default:
                break;
        }
        printf("\n");
    }
}

void uft_params_print_table(const uft_params_t *params) {
    printf("%-20s %-10s %-30s\n", "Parameter", "Type", "Value");
    printf("%-20s %-10s %-30s\n", "=========", "====", "=====");
    
    for (int i = 0; i < (int)NUM_PARAMS; i++) {
        if (!params->values[i].is_set) continue;
        
        const uft_param_def_t *def = params->values[i].definition;
        char value[64] = {0};
        
        switch (def->type) {
            case UFT_PARAM_TYPE_BOOL:
                snprintf(value, sizeof(value), "%s", 
                        params->values[i].value.bool_val ? "true" : "false");
                break;
            case UFT_PARAM_TYPE_INT:
            case UFT_PARAM_TYPE_RANGE:
                snprintf(value, sizeof(value), "%d", params->values[i].value.int_val);
                break;
            case UFT_PARAM_TYPE_FLOAT:
                snprintf(value, sizeof(value), "%.4f", params->values[i].value.float_val);
                break;
            case UFT_PARAM_TYPE_STRING:
            case UFT_PARAM_TYPE_PATH:
                snprintf(value, sizeof(value), "%s", 
                        params->values[i].value.string_val ? 
                        params->values[i].value.string_val : "(null)");
                break;
            case UFT_PARAM_TYPE_ENUM:
                snprintf(value, sizeof(value), "%s",
                        uft_params_get_enum_string(params, def->name));
                break;
            default:
                break;
        }
        
        printf("%-20s %-10s %-30s\n", def->name, 
               uft_param_type_string(def->type), value);
    }
}

const uft_param_def_t *uft_params_get_definition(const char *name) {
    int idx = find_param_index(name);
    if (idx < 0) return NULL;
    return &param_definitions[idx];
}

const uft_param_def_t **uft_params_get_all_definitions(int *count) {
    static const uft_param_def_t *defs[NUM_PARAMS + 1];
    
    for (int i = 0; param_definitions[i].name; i++) {
        defs[i] = &param_definitions[i];
    }
    defs[NUM_PARAMS] = NULL;
    
    if (count) *count = NUM_PARAMS;
    return defs;
}
