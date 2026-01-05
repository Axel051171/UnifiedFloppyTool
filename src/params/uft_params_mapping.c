/**
 * @file uft_params_mapping.c
 * @brief Parameter Mapping Layer
 * 
 * Konvertiert zwischen:
 * - GUI-Feldnamen → Interne Struktur
 * - Interne Struktur → Tool CLI Argumente
 * - Interne Struktur → Tool SDK Strukturen
 */

#include "uft/uft_params.h"
#include "uft/uft_tool_adapter.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// GUI Field Aliases (für verschiedene Terminologien)
// ============================================================================

typedef struct {
    const char* gui_name;       // Name in GUI/Config
    const char* canonical;      // Kanonischer interner Name
} alias_mapping_t;

static const alias_mapping_t g_aliases[] = {
    // Track vs Cylinder
    { "track_start",        "geometry.cylinder_start" },
    { "track_end",          "geometry.cylinder_end" },
    { "tracks",             "geometry.total_cylinders" },
    { "start_track",        "geometry.cylinder_start" },
    { "end_track",          "geometry.cylinder_end" },
    
    // Side vs Head
    { "side",               "geometry.head_start" },
    { "sides",              "geometry.total_heads" },
    { "side_start",         "geometry.head_start" },
    { "side_end",           "geometry.head_end" },
    
    // Revs vs Revolutions
    { "revs",               "hardware.flux.revolutions" },
    { "num_revolutions",    "hardware.flux.revolutions" },
    
    // Retries
    { "retry",              "global.global_retries" },
    { "retries",            "global.global_retries" },
    { "max_retries",        "decoder.errors.sector_retries" },
    
    // PLL
    { "bit_rate",           "decoder.pll.initial_period_us" },
    { "cell_time",          "decoder.pll.initial_period_us" },
    { "data_rate",          "decoder.pll.initial_period_us" },
    
    // Terminator
    { NULL, NULL }
};

static const char* resolve_alias(const char* gui_name) {
    for (int i = 0; g_aliases[i].gui_name; i++) {
        if (strcasecmp(g_aliases[i].gui_name, gui_name) == 0) {
            return g_aliases[i].canonical;
        }
    }
    return gui_name;  // Return as-is if no alias
}

// ============================================================================
// GUI → Internal Conversion
// ============================================================================

uft_error_t uft_params_from_gui(uft_params_t* params,
                                 const char* field_name,
                                 const char* value) {
    if (!params || !field_name || !value) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    // Resolve alias
    const char* canonical = resolve_alias(field_name);
    
    // Find schema
    const uft_param_schema_t* schema = uft_params_get_schema_by_name(canonical);
    if (!schema) {
        return UFT_ERROR_NOT_FOUND;
    }
    
    // Get pointer to field
    void* field_ptr = (char*)params + schema->offset;
    
    // Convert based on type
    switch (schema->type) {
        case UFT_PARAM_TYPE_INT:
            *(int*)field_ptr = atoi(value);
            break;
            
        case UFT_PARAM_TYPE_DOUBLE:
            *(double*)field_ptr = atof(value);
            break;
            
        case UFT_PARAM_TYPE_BOOL:
            *(bool*)field_ptr = (strcasecmp(value, "true") == 0 ||
                                 strcasecmp(value, "yes") == 0 ||
                                 strcasecmp(value, "1") == 0);
            break;
            
        case UFT_PARAM_TYPE_STRING:
            strncpy((char*)field_ptr, value, schema->size - 1);
            break;
            
        case UFT_PARAM_TYPE_ENUM:
            *(int*)field_ptr = atoi(value);  // Simplified
            break;
            
        default:
            return UFT_ERROR_INVALID_ARG;
    }
    
    return UFT_OK;
}

// ============================================================================
// Internal → Tool CLI Arguments
// ============================================================================

// Tool-specific argument mappings
typedef struct {
    const char* internal_param;
    const char* cli_arg;
    const char* format;     // printf format or NULL for flag
} tool_arg_mapping_t;

static const tool_arg_mapping_t g_gw_mappings[] = {
    { "geometry.cylinder_start",    "--tracks",     "%d" },
    { "geometry.cylinder_end",      NULL,           ":%d" },  // Appended
    { "hardware.flux.revolutions",  "--revs",       "%d" },
    { "global.global_retries",      "--retries",    "%d" },
    { NULL, NULL, NULL }
};

static const tool_arg_mapping_t g_fluxengine_mappings[] = {
    { "geometry.cylinder_start",    "--cylinders",  "%d" },
    { "geometry.cylinder_end",      NULL,           "-%d" },
    { "geometry.head_start",        "--heads",      "%d" },
    { NULL, NULL, NULL }
};

static const tool_arg_mapping_t g_nibtools_mappings[] = {
    { "geometry.cylinder_start",    "--start-track", "%d" },
    { "geometry.cylinder_end",      "--end-track",   "%d" },
    { "global.global_retries",      "--retries",     "%d" },
    { NULL, NULL, NULL }
};

static const tool_arg_mapping_t* get_tool_mappings(const char* tool_name) {
    if (strcmp(tool_name, "gw") == 0) return g_gw_mappings;
    if (strcmp(tool_name, "fluxengine") == 0) return g_fluxengine_mappings;
    if (strcmp(tool_name, "nibtools") == 0) return g_nibtools_mappings;
    return NULL;
}

uft_error_t uft_params_to_tool_args(const uft_params_t* params,
                                     const char* tool_name,
                                     char* args, size_t args_size) {
    if (!params || !tool_name || !args) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    const tool_arg_mapping_t* mappings = get_tool_mappings(tool_name);
    if (!mappings) {
        return UFT_ERROR_NOT_FOUND;
    }
    
    args[0] = '\0';
    size_t pos = 0;
    
    for (int i = 0; mappings[i].internal_param; i++) {
        const uft_param_schema_t* schema = 
            uft_params_get_schema_by_name(mappings[i].internal_param);
        if (!schema) continue;
        
        const void* field_ptr = (const char*)params + schema->offset;
        
        // Skip if using default
        bool is_default = false;
        switch (schema->type) {
            case UFT_PARAM_TYPE_INT:
                is_default = (*(const int*)field_ptr == schema->int_range.def);
                break;
            case UFT_PARAM_TYPE_DOUBLE:
                is_default = (*(const double*)field_ptr == schema->double_range.def);
                break;
            case UFT_PARAM_TYPE_BOOL:
                is_default = (*(const bool*)field_ptr == schema->bool_val.def);
                break;
            default:
                break;
        }
        
        if (is_default && mappings[i].cli_arg) continue;  // Skip defaults
        
        // Format argument
        char arg_buf[128];
        if (mappings[i].cli_arg) {
            switch (schema->type) {
                case UFT_PARAM_TYPE_INT:
                    snprintf(arg_buf, sizeof(arg_buf), "%s=%d",
                             mappings[i].cli_arg, *(const int*)field_ptr);
                    break;
                case UFT_PARAM_TYPE_DOUBLE:
                    snprintf(arg_buf, sizeof(arg_buf), "%s=%.2f",
                             mappings[i].cli_arg, *(const double*)field_ptr);
                    break;
                case UFT_PARAM_TYPE_BOOL:
                    if (*(const bool*)field_ptr) {
                        snprintf(arg_buf, sizeof(arg_buf), "%s", mappings[i].cli_arg);
                    } else {
                        continue;
                    }
                    break;
                default:
                    continue;
            }
            
            if (pos + strlen(arg_buf) + 2 < args_size) {
                if (pos > 0) {
                    args[pos++] = ' ';
                }
                strcpy(args + pos, arg_buf);
                pos += strlen(arg_buf);
            }
        }
    }
    
    // Special handling for track range (gw style: --tracks=0:79)
    if (strcmp(tool_name, "gw") == 0) {
        if (params->geometry.cylinder_start >= 0 || 
            params->geometry.cylinder_end >= 0) {
            char track_arg[64];
            int start = params->geometry.cylinder_start >= 0 ? 
                        params->geometry.cylinder_start : 0;
            int end = params->geometry.cylinder_end;
            
            if (end >= 0) {
                snprintf(track_arg, sizeof(track_arg), "--tracks=%d:%d", start, end);
            } else {
                snprintf(track_arg, sizeof(track_arg), "--tracks=%d", start);
            }
            
            // Find and replace existing --tracks or append
            char* existing = strstr(args, "--tracks");
            if (existing) {
                // Already handled above
            } else if (pos + strlen(track_arg) + 2 < args_size) {
                if (pos > 0) args[pos++] = ' ';
                strcpy(args + pos, track_arg);
            }
        }
    }
    
    return UFT_OK;
}

// ============================================================================
// Internal → Tool SDK Struct
// ============================================================================

uft_error_t uft_params_to_tool_sdk(const uft_params_t* params,
                                    const char* tool_name,
                                    void* sdk_struct, size_t sdk_size) {
    if (!params || !tool_name || !sdk_struct) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    // Convert to uft_tool_read_params_t
    if (sdk_size >= sizeof(uft_tool_read_params_t)) {
        uft_tool_read_params_t* rp = (uft_tool_read_params_t*)sdk_struct;
        
        rp->struct_size = sizeof(*rp);
        rp->device_index = params->global.device_index;
        rp->start_track = params->geometry.cylinder_start;
        rp->end_track = params->geometry.cylinder_end;
        rp->start_head = params->geometry.head_start;
        rp->end_head = params->geometry.head_end;
        rp->retries = params->global.global_retries;
        rp->revolutions = params->hardware.flux.revolutions;
        rp->format = params->format.output_format;
        
        return UFT_OK;
    }
    
    return UFT_ERROR_INVALID_ARG;
}

// ============================================================================
// Semantic Conflict Detection
// ============================================================================

typedef struct {
    const char* param_a;
    const char* param_b;
    const char* description;
    bool (*check)(const uft_params_t*);
} conflict_rule_t;

static bool check_track_vs_cylinder(const uft_params_t* p) {
    // If both track notation and cylinder notation are set differently
    // This would be caught if we had separate track fields
    return false;  // No conflict in canonical representation
}

static bool check_sector_vs_format(const uft_params_t* p) {
    // D64 requires 256-byte sectors
    if (p->format.output_format == UFT_FORMAT_D64 && 
        p->geometry.sector_size != 256 &&
        p->geometry.sector_size != 0) {
        return true;  // Conflict!
    }
    return false;
}

static bool check_flux_vs_sector_output(const uft_params_t* p) {
    // Can't output flux format from sector-only source
    bool flux_output = (p->format.output_format == UFT_FORMAT_SCP ||
                        p->format.output_format == UFT_FORMAT_KRYOFLUX);
    bool sector_input = (p->format.input_format == UFT_FORMAT_D64 ||
                         p->format.input_format == UFT_FORMAT_ADF ||
                         p->format.input_format == UFT_FORMAT_IMG);
    
    return flux_output && sector_input;
}

static const conflict_rule_t g_conflicts[] = {
    {
        .param_a = "geometry.sector_size",
        .param_b = "format.output_format",
        .description = "Sector size incompatible with output format",
        .check = check_sector_vs_format,
    },
    {
        .param_a = "format.output_format",
        .param_b = "format.input_format",
        .description = "Cannot create flux output from sector-only input",
        .check = check_flux_vs_sector_output,
    },
    { NULL, NULL, NULL, NULL }
};

int uft_params_check_conflicts(const uft_params_t* params,
                                char** conflicts, int max_conflicts) {
    if (!params) return 0;
    
    int count = 0;
    for (int i = 0; g_conflicts[i].param_a && count < max_conflicts; i++) {
        if (g_conflicts[i].check(params)) {
            if (conflicts) {
                conflicts[count] = (char*)g_conflicts[i].description;
            }
            count++;
        }
    }
    
    return count;
}

// ============================================================================
// Dependency Checking
// ============================================================================

bool uft_params_check_dependency(const uft_params_t* params,
                                  const char* param_name) {
    if (!params || !param_name) return true;  // No dependency
    
    const uft_param_schema_t* schema = uft_params_get_schema_by_name(param_name);
    if (!schema || !schema->depends_on) return true;
    
    // Simple dependency parsing: "param == value"
    // Full implementation would use expression parser
    
    // For now, just check if the dependency field is set
    const char* dep = schema->depends_on;
    
    // Example: "format.input_format == UFT_FORMAT_D64"
    // We'd parse this and check
    
    return true;  // Simplified
}
