/**
 * @file uft_tool_registry.c
 * @brief Tool Registry - Zentrale Verwaltung aller Tool Adapter
 * 
 * Features:
 * - Automatische Tool-Erkennung
 * - Priorisierung nach Capabilities
 * - Fallback-Ketten
 */

#include "uft/uft_tool_adapter.h"
#include "uft/uft_safe.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// External Tool Adapters
// ============================================================================

extern const uft_tool_adapter_t uft_tool_greaseweazle;
extern const uft_tool_adapter_t uft_tool_fluxengine;
extern const uft_tool_adapter_t uft_tool_GCR tools;
extern const uft_tool_adapter_t uft_tool_libflux;
extern const uft_tool_adapter_t uft_tool_adftools;
extern const uft_tool_adapter_t uft_tool_disk_analyse;

// ============================================================================
// Registry Storage
// ============================================================================

#define MAX_TOOLS 32

static struct {
    const uft_tool_adapter_t*   tools[MAX_TOOLS];
    void*                       contexts[MAX_TOOLS];
    size_t                      count;
    bool                        initialized;
    const char*                 preferred_tool;
} g_tool_registry = {0};

// ============================================================================
// Initialization
// ============================================================================

uft_error_t uft_tool_registry_init(void) {
    if (g_tool_registry.initialized) return UFT_OK;
    
    memset(&g_tool_registry, 0, sizeof(g_tool_registry));
    
    // Register all built-in tools
    const uft_tool_adapter_t* builtin_tools[] = {
        &uft_tool_greaseweazle,
        &uft_tool_fluxengine,
        &uft_tool_GCR tools,
        &uft_tool_libflux,
        &uft_tool_adftools,
        &uft_tool_disk_analyse,
    };
    
    size_t builtin_count = sizeof(builtin_tools) / sizeof(builtin_tools[0]);
    
    for (size_t i = 0; i < builtin_count; i++) {
        uft_tool_register(builtin_tools[i]);
    }
    
    g_tool_registry.initialized = true;
    return UFT_OK;
}

void uft_tool_registry_shutdown(void) {
    // Cleanup all tool contexts
    for (size_t i = 0; i < g_tool_registry.count; i++) {
        if (g_tool_registry.contexts[i] && g_tool_registry.tools[i]->cleanup) {
            g_tool_registry.tools[i]->cleanup(g_tool_registry.contexts[i]);
        }
    }
    
    memset(&g_tool_registry, 0, sizeof(g_tool_registry));
}

// ============================================================================
// Registration
// ============================================================================

uft_error_t uft_tool_register(const uft_tool_adapter_t* tool) {
    if (!tool || !tool->name) return UFT_ERROR_NULL_POINTER;
    
    if (g_tool_registry.count >= MAX_TOOLS) {
        return UFT_ERROR_NO_SPACE;
    }
    
    // Check duplicate
    for (size_t i = 0; i < g_tool_registry.count; i++) {
        if (strcmp(g_tool_registry.tools[i]->name, tool->name) == 0) {
            return UFT_ERROR_ALREADY_EXISTS;
        }
    }
    
    g_tool_registry.tools[g_tool_registry.count++] = tool;
    return UFT_OK;
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

// ============================================================================
// Capability-based Selection
// ============================================================================

const uft_tool_adapter_t* uft_tool_find_for_operation(uft_tool_cap_t caps) {
    const uft_tool_adapter_t* best = NULL;
    int best_score = 0;
    
    // First check preferred tool
    if (g_tool_registry.preferred_tool) {
        const uft_tool_adapter_t* pref = uft_tool_find(g_tool_registry.preferred_tool);
        if (pref && (pref->capabilities & caps) == caps) {
            if (!pref->is_available || pref->is_available()) {
                return pref;
            }
        }
    }
    
    // Find best available tool
    for (size_t i = 0; i < g_tool_registry.count; i++) {
        const uft_tool_adapter_t* tool = g_tool_registry.tools[i];
        
        // Check capabilities
        if ((tool->capabilities & caps) != caps) continue;
        
        // Check availability
        if (tool->is_available && !tool->is_available()) continue;
        
        // Score based on capability match
        int score = 0;
        uint32_t c = tool->capabilities;
        while (c) { score += c & 1; c >>= 1; }
        
        // Bonus for hardware support
        if (tool->capabilities & UFT_TOOL_CAP_HARDWARE) score += 10;
        
        if (score > best_score) {
            best_score = score;
            best = tool;
        }
    }
    
    return best;
}

const uft_tool_adapter_t* uft_tool_find_for_format(uft_format_t format) {
    const uft_tool_adapter_t* best = NULL;
    int best_score = 0;
    
    uint32_t format_bit = (1u << format);
    
    for (size_t i = 0; i < g_tool_registry.count; i++) {
        const uft_tool_adapter_t* tool = g_tool_registry.tools[i];
        
        // Check format support
        if (!(tool->supported_formats & format_bit)) continue;
        
        // Check availability
        if (tool->is_available && !tool->is_available()) continue;
        
        // Score
        int score = 1;
        if (tool->capabilities & UFT_TOOL_CAP_HARDWARE) score += 10;
        if (tool->capabilities & UFT_TOOL_CAP_FLUX) score += 5;
        
        if (score > best_score) {
            best_score = score;
            best = tool;
        }
    }
    
    return best;
}

// ============================================================================
// List Tools
// ============================================================================

size_t uft_tool_list(const uft_tool_adapter_t** tools, size_t max_count) {
    if (!tools || max_count == 0) {
        return g_tool_registry.count;
    }
    
    size_t count = g_tool_registry.count < max_count ? 
                   g_tool_registry.count : max_count;
    
    for (size_t i = 0; i < count; i++) {
        tools[i] = g_tool_registry.tools[i];
    }
    
    return count;
}

// ============================================================================
// Availability Check
// ============================================================================

size_t uft_tool_list_available(const uft_tool_adapter_t** tools, size_t max_count) {
    size_t count = 0;
    
    for (size_t i = 0; i < g_tool_registry.count && count < max_count; i++) {
        const uft_tool_adapter_t* tool = g_tool_registry.tools[i];
        
        if (!tool->is_available || tool->is_available()) {
            if (tools) tools[count] = tool;
            count++;
        }
    }
    
    return count;
}

// ============================================================================
// Preference
// ============================================================================

uft_error_t uft_tool_set_preferred(const char* name) {
    if (name && !uft_tool_find(name)) {
        return UFT_ERROR_NOT_FOUND;
    }
    
    g_tool_registry.preferred_tool = name;
    return UFT_OK;
}

// ============================================================================
// Context Management
// ============================================================================

uft_error_t uft_tool_get_context(const uft_tool_adapter_t* tool, void** context) {
    if (!tool || !context) return UFT_ERROR_NULL_POINTER;
    
    // Find index
    for (size_t i = 0; i < g_tool_registry.count; i++) {
        if (g_tool_registry.tools[i] == tool) {
            // Initialize if needed
            if (!g_tool_registry.contexts[i] && tool->init) {
                uft_error_t err = tool->init(&g_tool_registry.contexts[i]);
                if (err != UFT_OK) return err;
            }
            
            *context = g_tool_registry.contexts[i];
            return UFT_OK;
        }
    }
    
    return UFT_ERROR_NOT_FOUND;
}

// ============================================================================
// Diagnostics
// ============================================================================

void uft_tool_print_status(void) {
    printf("=== Tool Registry Status ===\n");
    printf("Registered: %zu tools\n\n", g_tool_registry.count);
    
    for (size_t i = 0; i < g_tool_registry.count; i++) {
        const uft_tool_adapter_t* tool = g_tool_registry.tools[i];
        bool avail = !tool->is_available || tool->is_available();
        
        printf("  [%c] %-15s - %s\n",
               avail ? '+' : '-',
               tool->name,
               tool->description);
        
        printf("      Caps:");
        if (tool->capabilities & UFT_TOOL_CAP_READ) printf(" READ");
        if (tool->capabilities & UFT_TOOL_CAP_WRITE) printf(" WRITE");
        if (tool->capabilities & UFT_TOOL_CAP_FLUX) printf(" FLUX");
        if (tool->capabilities & UFT_TOOL_CAP_HARDWARE) printf(" HW");
        if (tool->capabilities & UFT_TOOL_CAP_CONVERT) printf(" CONV");
        printf("\n");
    }
    
    printf("\nPreferred: %s\n", 
           g_tool_registry.preferred_tool ? g_tool_registry.preferred_tool : "(none)");
}
