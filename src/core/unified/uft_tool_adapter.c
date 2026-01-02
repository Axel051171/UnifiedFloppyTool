/**
 * @file uft_tool_adapter.c
 * @brief Tool Adapter Registry Implementation
 * 
 * Verwaltet externe Tools wie Greaseweazle CLI, nibtools, etc.
 */

#include "uft/uft_tool_adapter.h"
#include "uft/uft_safe.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Registry Storage
// ============================================================================

#define MAX_TOOLS 16

static struct {
    const uft_tool_adapter_t* tools[MAX_TOOLS];
    size_t count;
    const char* preferred_tool;
} g_tool_registry = {0};

// ============================================================================
// Registration
// ============================================================================

uft_error_t uft_tool_register(const uft_tool_adapter_t* adapter) {
    if (!adapter || !adapter->name) {
        return UFT_ERROR_INVALID_ARG;
    }
    
    if (g_tool_registry.count >= MAX_TOOLS) {
        return UFT_ERROR_NO_SPACE;
    }
    
    // Check for duplicate
    for (size_t i = 0; i < g_tool_registry.count; i++) {
        if (strcmp(g_tool_registry.tools[i]->name, adapter->name) == 0) {
            return UFT_ERROR_ALREADY_EXISTS;
        }
    }
    
    g_tool_registry.tools[g_tool_registry.count++] = adapter;
    return UFT_OK;
}

uft_error_t uft_tool_unregister(const char* name) {
    if (!name) return UFT_ERROR_INVALID_ARG;
    
    for (size_t i = 0; i < g_tool_registry.count; i++) {
        if (strcmp(g_tool_registry.tools[i]->name, name) == 0) {
            for (size_t j = i; j < g_tool_registry.count - 1; j++) {
                g_tool_registry.tools[j] = g_tool_registry.tools[j + 1];
            }
            g_tool_registry.count--;
            return UFT_OK;
        }
    }
    
    return UFT_ERROR_NOT_FOUND;
}

// ============================================================================
// Lookup
// ============================================================================

const uft_tool_adapter_t* uft_tool_find(const char* name) {
    if (!name) return NULL;
    
    for (size_t i = 0; i < g_tool_registry.count; i++) {
        if (strcmp(g_tool_registry.tools[i]->name, name) == 0) {
            return g_tool_registry.tools[i];
        }
    }
    
    return NULL;
}

const uft_tool_adapter_t* uft_tool_find_for_format(uft_format_t format) {
    // First try preferred tool
    if (g_tool_registry.preferred_tool) {
        const uft_tool_adapter_t* pref = uft_tool_find(g_tool_registry.preferred_tool);
        if (pref && (pref->supported_formats & (1u << format))) {
            if (pref->is_available && pref->is_available()) {
                return pref;
            }
        }
    }
    
    // Find first available tool supporting this format
    for (size_t i = 0; i < g_tool_registry.count; i++) {
        const uft_tool_adapter_t* tool = g_tool_registry.tools[i];
        if (tool->supported_formats & (1u << format)) {
            if (!tool->is_available || tool->is_available()) {
                return tool;
            }
        }
    }
    
    return NULL;
}

const uft_tool_adapter_t* uft_tool_find_for_operation(uft_tool_cap_t required_caps) {
    // First try preferred tool
    if (g_tool_registry.preferred_tool) {
        const uft_tool_adapter_t* pref = uft_tool_find(g_tool_registry.preferred_tool);
        if (pref && (pref->capabilities & required_caps) == required_caps) {
            if (pref->is_available && pref->is_available()) {
                return pref;
            }
        }
    }
    
    // Find first available tool with required capabilities
    for (size_t i = 0; i < g_tool_registry.count; i++) {
        const uft_tool_adapter_t* tool = g_tool_registry.tools[i];
        if ((tool->capabilities & required_caps) == required_caps) {
            if (!tool->is_available || tool->is_available()) {
                return tool;
            }
        }
    }
    
    return NULL;
}

size_t uft_tool_list(const uft_tool_adapter_t** tools, size_t max_count) {
    if (!tools || max_count == 0) {
        return g_tool_registry.count;
    }
    
    size_t count = (g_tool_registry.count < max_count) ? 
                    g_tool_registry.count : max_count;
    
    for (size_t i = 0; i < count; i++) {
        tools[i] = g_tool_registry.tools[i];
    }
    
    return count;
}

// ============================================================================
// Preference
// ============================================================================

uft_error_t uft_tool_set_preferred(const char* tool_name) {
    if (tool_name) {
        // Verify tool exists
        if (!uft_tool_find(tool_name)) {
            return UFT_ERROR_NOT_FOUND;
        }
    }
    
    g_tool_registry.preferred_tool = tool_name;
    return UFT_OK;
}

const char* uft_tool_get_preferred(void) {
    return g_tool_registry.preferred_tool;
}

// ============================================================================
// Built-in Tool Adapters (Stubs)
// ============================================================================

// --- Greaseweazle CLI Adapter ---

static bool gw_is_available(void) {
    // Check if 'gw' command exists
    FILE* p = popen("which gw 2>/dev/null", "r");
    if (!p) return false;
    
    char buf[256];
    bool found = (fgets(buf, sizeof(buf), p) != NULL);
    pclose(p);
    
    return found;
}

static bool gw_detect_hardware(char* info, size_t size) {
    FILE* p = popen("gw info 2>/dev/null", "r");
    if (!p) return false;
    
    char buf[1024] = {0};
    size_t total = 0;
    while (fgets(buf + total, sizeof(buf) - total, p) && total < sizeof(buf) - 1) {
        total = strlen(buf);
    }
    pclose(p);
    
    if (total > 0 && info && size > 0) {
        snprintf(info, size, "%s", buf);
        return true;
    }
    
    return false;
}

static uft_error_t gw_read_disk(void* context,
                                 const uft_tool_read_params_t* params,
                                 uft_unified_image_t* output) {
    (void)context;
    (void)params;
    (void)output;
    
    // TODO: Implement actual gw read
    // gw read --format=scp output.scp
    
    return UFT_ERROR_NOT_IMPLEMENTED;
}

static const uft_tool_adapter_t g_tool_gw = {
    .name = "gw",
    .version = "1.0",
    .description = "Greaseweazle Command Line Tool",
    .capabilities = UFT_TOOL_CAP_READ | UFT_TOOL_CAP_WRITE | UFT_TOOL_CAP_FLUX | UFT_TOOL_CAP_HARDWARE,
    .supported_formats = (1 << UFT_FORMAT_SCP) | (1 << UFT_FORMAT_HFE),
    .init = NULL,
    .cleanup = NULL,
    .is_available = gw_is_available,
    .detect_hardware = gw_detect_hardware,
    .read_disk = gw_read_disk,
    .write_disk = NULL,
    .convert = NULL,
};

// --- nibtools Adapter ---

static bool nibtools_is_available(void) {
    FILE* p = popen("which nibread 2>/dev/null", "r");
    if (!p) return false;
    
    char buf[256];
    bool found = (fgets(buf, sizeof(buf), p) != NULL);
    pclose(p);
    
    return found;
}

static const uft_tool_adapter_t g_tool_nibtools = {
    .name = "nibtools",
    .version = "1.0",
    .description = "Commodore Disk Tools",
    .capabilities = UFT_TOOL_CAP_READ | UFT_TOOL_CAP_WRITE | UFT_TOOL_CAP_SECTOR | UFT_TOOL_CAP_HARDWARE,
    .supported_formats = (1 << UFT_FORMAT_D64) | (1 << UFT_FORMAT_G64) | (1 << UFT_FORMAT_NBZ),
    .init = NULL,
    .cleanup = NULL,
    .is_available = nibtools_is_available,
    .detect_hardware = NULL,
    .read_disk = NULL,
    .write_disk = NULL,
    .convert = NULL,
};

// --- hxcfe Adapter ---

static bool hxcfe_is_available(void) {
    FILE* p = popen("which hxcfe 2>/dev/null", "r");
    if (!p) return false;
    
    char buf[256];
    bool found = (fgets(buf, sizeof(buf), p) != NULL);
    pclose(p);
    
    return found;
}

static uft_error_t hxcfe_convert(void* context,
                                  const char* input,
                                  const char* output,
                                  uft_format_t format) {
    (void)context;
    
    if (!input || !output) return UFT_ERROR_INVALID_ARG;
    
    // Build command
    char cmd[1024];
    const char* fmt_str = "raw";
    
    switch (format) {
        case UFT_FORMAT_HFE: fmt_str = "hfe"; break;
        case UFT_FORMAT_IMG: fmt_str = "raw"; break;
        case UFT_FORMAT_SCP: fmt_str = "scp"; break;
        default: break;
    }
    
    snprintf(cmd, sizeof(cmd), "hxcfe -finput:\"%s\" -foutput:\"%s\" -conv:%s 2>/dev/null",
             input, output, fmt_str);
    
    int ret = system(cmd);
    return (ret == 0) ? UFT_OK : UFT_ERROR_TOOL_FAILED;
}

static const uft_tool_adapter_t g_tool_hxcfe = {
    .name = "hxcfe",
    .version = "1.0",
    .description = "HxC Floppy Emulator Tool",
    .capabilities = UFT_TOOL_CAP_CONVERT | UFT_TOOL_CAP_FLUX | UFT_TOOL_CAP_SECTOR,
    .supported_formats = 0xFFFFFFFF,  // Supports many formats
    .init = NULL,
    .cleanup = NULL,
    .is_available = hxcfe_is_available,
    .detect_hardware = NULL,
    .read_disk = NULL,
    .write_disk = NULL,
    .convert = hxcfe_convert,
};

// ============================================================================
// Initialization
// ============================================================================

void uft_register_builtin_tools(void) {
    uft_tool_register(&g_tool_gw);
    uft_tool_register(&g_tool_nibtools);
    uft_tool_register(&g_tool_hxcfe);
}
