/**
 * @file uft_tool_adapter.h
 * @brief Tool Adapter Interface - Vollständige Definition
 * 
 * Abstraktion für externe Tools:
 * - adftools, disk-analyse
 */

#ifndef UFT_TOOL_ADAPTER_H
#define UFT_TOOL_ADAPTER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "uft_error.h"
#include "uft_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct uft_unified_image uft_unified_image_t;

// ============================================================================
// Tool Capabilities
// ============================================================================

typedef enum uft_tool_cap {
    UFT_TOOL_CAP_READ       = (1 << 0),     // Kann von Disk lesen
    UFT_TOOL_CAP_WRITE      = (1 << 1),     // Kann auf Disk schreiben
    UFT_TOOL_CAP_FLUX       = (1 << 2),     // Unterstützt Flux-Daten
    UFT_TOOL_CAP_SECTOR     = (1 << 3),     // Unterstützt Sektor-Daten
    UFT_TOOL_CAP_HARDWARE   = (1 << 4),     // Hat Hardware-Zugriff
    UFT_TOOL_CAP_CONVERT    = (1 << 5),     // Kann konvertieren
    UFT_TOOL_CAP_FORMAT     = (1 << 6),     // Kann formatieren
    UFT_TOOL_CAP_VERIFY     = (1 << 7),     // Kann verifizieren
    UFT_TOOL_CAP_INFO       = (1 << 8),     // Kann Disk-Info abfragen
} uft_tool_cap_t;

// ============================================================================
// Progress Callback
// ============================================================================

typedef void (*uft_progress_callback_t)(void* user_data,
                                         int percent,
                                         const char* message);

// ============================================================================
// Read Parameters
// ============================================================================

typedef struct uft_tool_read_params {
    size_t          struct_size;        // Für ABI-Stabilität
    int             device_index;
    int             start_track;
    int             end_track;          // -1 = alle
    int             start_head;
    int             end_head;
    int             retries;
    int             revolutions;        // Für Flux-Capture
    uft_format_t    format;             // Zielformat
    uft_geometry_preset_t geometry;     // Geometrie-Preset
    
    // Progress
    uft_progress_callback_t progress_cb;
    void*           progress_user;
    volatile bool*  cancel_flag;
} uft_tool_read_params_t;

// ============================================================================
// Write Parameters
// ============================================================================

typedef struct uft_tool_write_params {
    size_t          struct_size;
    int             device_index;
    int             start_track;
    int             end_track;
    int             start_head;
    int             end_head;
    bool            verify;
    bool            erase_empty;
    bool            precomp;            // Write precompensation
    
    uft_progress_callback_t progress_cb;
    void*           progress_user;
    volatile bool*  cancel_flag;
} uft_tool_write_params_t;

// ============================================================================
// Disk Info
// ============================================================================

typedef struct uft_tool_disk_info {
    bool            disk_present;
    bool            write_protected;
    double          rpm;
    int             detected_tracks;
    int             detected_heads;
    char            label[64];
} uft_tool_disk_info_t;

// ============================================================================
// Tool Adapter Interface
// ============================================================================

typedef struct uft_tool_adapter {
    // Metadata
    const char*     name;
    const char*     version;
    const char*     description;
    uint32_t        capabilities;       // Bitmask von uft_tool_cap_t
    uint32_t        supported_formats;  // Bitmask von (1 << uft_format_t)
    
    // Lifecycle
    uft_error_t     (*init)(void** context);
    void            (*cleanup)(void* context);
    
    // Availability
    bool            (*is_available)(void);
    bool            (*detect_hardware)(char* info, size_t size);
    
    // Core Operations
    uft_error_t     (*read_disk)(void* context,
                                  const uft_tool_read_params_t* params,
                                  uft_unified_image_t* output);
    
    uft_error_t     (*write_disk)(void* context,
                                   const uft_tool_write_params_t* params,
                                   const uft_unified_image_t* input);
    
    uft_error_t     (*convert)(void* context,
                                const char* input_path,
                                const char* output_path,
                                uft_format_t output_format);
    
    // Optional Operations
    uft_error_t     (*get_disk_info)(void* context,
                                      uft_tool_disk_info_t* info);
    
    uft_error_t     (*seek)(void* context, int track, int head);
    uft_error_t     (*reset)(void* context);
    
} uft_tool_adapter_t;

// ============================================================================
// Registry API
// ============================================================================

uft_error_t uft_tool_registry_init(void);
void uft_tool_registry_shutdown(void);

uft_error_t uft_tool_register(const uft_tool_adapter_t* tool);

const uft_tool_adapter_t* uft_tool_find(const char* name);
const uft_tool_adapter_t* uft_tool_find_for_operation(uft_tool_cap_t caps);
const uft_tool_adapter_t* uft_tool_find_for_format(uft_format_t format);

size_t uft_tool_list(const uft_tool_adapter_t** tools, size_t max_count);
size_t uft_tool_list_available(const uft_tool_adapter_t** tools, size_t max_count);

uft_error_t uft_tool_set_preferred(const char* name);
uft_error_t uft_tool_get_context(const uft_tool_adapter_t* tool, void** context);

void uft_tool_print_status(void);

// ============================================================================
// Convenience Functions
// ============================================================================

static inline uft_tool_read_params_t uft_tool_read_params_default(void) {
    uft_tool_read_params_t p = {0};
    p.struct_size = sizeof(p);
    p.start_track = 0;
    p.end_track = -1;
    p.start_head = 0;
    p.end_head = 1;
    p.retries = 3;
    p.revolutions = 3;
    return p;
}

static inline uft_tool_write_params_t uft_tool_write_params_default(void) {
    uft_tool_write_params_t p = {0};
    p.struct_size = sizeof(p);
    p.start_track = 0;
    p.end_track = -1;
    p.start_head = 0;
    p.end_head = 1;
    p.verify = true;
    return p;
}

#ifdef __cplusplus
}
#endif

#endif // UFT_TOOL_ADAPTER_H
