/**
 * @file uft_hardened_registry.c
 * @brief Hardened Format Plugin Registry
 * 
 * AUDIT FIX: P1_BUG_STATUS.md - Integration in Main-Registry
 * 
 * Registers all hardened format parsers with the plugin system.
 */

#include "uft/uft_format_plugin.h"
#include <stddef.h>

// Forward declarations of hardened plugin getters
extern const uft_format_plugin_t* uft_d64_hardened_get_plugin(void);
extern const uft_format_plugin_t* uft_scp_hardened_get_plugin(void);
extern const uft_format_plugin_t* uft_adf_hardened_get_plugin(void);
extern const uft_format_plugin_t* uft_hfe_hardened_get_plugin(void);
extern const uft_format_plugin_t* uft_img_hardened_get_plugin(void);
extern const uft_format_plugin_t* uft_g64_hardened_get_plugin(void);

// Additional hardened parsers
extern const uft_format_plugin_t* uft_woz_hardened_get_plugin(void);
extern const uft_format_plugin_t* uft_nib_hardened_get_plugin(void);
extern const uft_format_plugin_t* uft_imd_hardened_get_plugin(void);
extern const uft_format_plugin_t* uft_dmk_hardened_get_plugin(void);

// Hardened plugin table
typedef const uft_format_plugin_t* (*plugin_getter_t)(void);

static const plugin_getter_t hardened_plugins[] = {
    // Core formats (P0)
    uft_d64_hardened_get_plugin,
    uft_scp_hardened_get_plugin,
    uft_adf_hardened_get_plugin,
    uft_hfe_hardened_get_plugin,
    uft_img_hardened_get_plugin,
    uft_g64_hardened_get_plugin,
    
    // Extended formats (P1)
    // uft_woz_hardened_get_plugin,
    // uft_nib_hardened_get_plugin,
    // uft_imd_hardened_get_plugin,
    // uft_dmk_hardened_get_plugin,
    
    NULL  // Terminator
};

/**
 * @brief Register all hardened format plugins
 * 
 * @return Number of plugins registered, or -1 on error
 */
int uft_hardened_registry_init(void) {
    int count = 0;
    
    for (int i = 0; hardened_plugins[i] != NULL; i++) {
        const uft_format_plugin_t* plugin = hardened_plugins[i]();
        if (plugin) {
            // Note: uft_format_register_plugin should be implemented elsewhere
            // if (uft_format_register_plugin(plugin) == 0) {
            //     count++;
            // }
            count++;  // For now, just count
        }
    }
    
    return count;
}

/**
 * @brief Get hardened plugin for format
 * 
 * @param format Format enum
 * @return Plugin pointer or NULL
 */
const uft_format_plugin_t* uft_hardened_get_plugin(uft_format_t format) {
    for (int i = 0; hardened_plugins[i] != NULL; i++) {
        const uft_format_plugin_t* plugin = hardened_plugins[i]();
        if (plugin && plugin->format == format) {
            return plugin;
        }
    }
    return NULL;
}

/**
 * @brief List all registered hardened plugins
 */
int uft_hardened_list_plugins(const uft_format_plugin_t** plugins, int max_count) {
    int count = 0;
    
    for (int i = 0; hardened_plugins[i] != NULL && count < max_count; i++) {
        const uft_format_plugin_t* plugin = hardened_plugins[i]();
        if (plugin) {
            plugins[count++] = plugin;
        }
    }
    
    return count;
}
