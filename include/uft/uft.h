/**
 * @file uft.h
 * @brief UnifiedFloppyTool - Master Include Header
 * 
 * Inkludiert alle öffentlichen APIs.
 * Für GUI-Entwickler: Nur uft_gui.h verwenden!
 */

#ifndef UFT_H
#define UFT_H

// Version
#define UFT_VERSION_MAJOR   2
#define UFT_VERSION_MINOR   0
#define UFT_VERSION_PATCH   0
#define UFT_VERSION_STRING  "2.0.0"

// Core Types & Errors
#include "uft_types.h"
#include "uft_error.h"
#include "uft_safe.h"

// Unified Image Model (Phase 1)
#include "uft_unified_image.h"
#include "uft_decoder_registry.h"
#include "uft_operations.h"

// Layer Separation (Phase 3)
#include "uft_device_manager.h"
#include "uft_format_advisor.h"
#include "uft_job_manager.h"

// Tool Adapters (Phase 4)
#include "uft_tool_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the entire UFT library
 */
static inline uft_error_t uft_library_init(void) {
    uft_error_t err;
    
    // Initialize tool registry
    err = uft_tool_registry_init();
    if (err != UFT_OK) return err;
    
    // Register built-in decoders
    extern void uft_register_builtin_decoders(void);
    uft_register_builtin_decoders();
    
    return UFT_OK;
}

/**
 * @brief Shutdown the UFT library
 */
static inline void uft_library_shutdown(void) {
    uft_tool_registry_shutdown();
}

#ifdef __cplusplus
}
#endif

#endif // UFT_H
