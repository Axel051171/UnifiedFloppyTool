/**
 * @file uft_gui_bridge.h
 * @brief GUI Bridge - Complete Abstraction Layer for GUI
 * 
 * DIES IST DER EINZIGE HEADER, DEN DIE GUI INKLUDIEREN SOLLTE
 * (plus uft_device_manager.h und uft_job_manager.h)
 * 
 * GUI darf NIEMALS inkludieren:
 * - uft_hardware.h
 * - uft_hw_greaseweazle.h
 * - uft_hw_kryoflux.h
 * - etc.
 */

#ifndef UFT_GUI_BRIDGE_H
#define UFT_GUI_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "uft_types.h"
#include "uft_error.h"
#include "uft_device_manager.h"
#include "uft_job_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Status Callback (für Statusbar etc.)
// ============================================================================

typedef void (*uft_gui_status_callback_t)(void* user_data, const char* message);

// ============================================================================
// Simplified Image Info (für GUI-Display)
// ============================================================================

typedef struct uft_gui_image_info {
    char            filename[256];
    char            format_name[64];
    uft_format_t    format;
    
    int             cylinders;
    int             heads;
    int             sectors_per_track;
    int             sector_size;
    uint32_t        total_sectors;
    uint32_t        bad_sectors;
    
    bool            has_flux;
    bool            modified;
} uft_gui_image_info_t;

// ============================================================================
// Format List (für "Save As" Dialog)
// ============================================================================

typedef struct uft_gui_format_entry {
    uft_format_t    format;
    const char*     name;
    const char*     extension;
    bool            recommended;
} uft_gui_format_entry_t;

typedef struct uft_gui_format_list {
    uft_gui_format_entry_t  formats[16];
    int                     count;
} uft_gui_format_list_t;

// ============================================================================
// Lifecycle
// ============================================================================

uft_error_t uft_gui_bridge_init(void);
void uft_gui_bridge_shutdown(void);

void uft_gui_bridge_set_status_callback(uft_gui_status_callback_t callback,
                                         void* user_data);

// ============================================================================
// Device Access (Simplified)
// ============================================================================

uft_device_manager_t* uft_gui_get_device_manager(void);
uft_error_t uft_gui_scan_devices(void);
size_t uft_gui_get_device_count(void);
const uft_device_info_t* uft_gui_get_device(int index);
uft_error_t uft_gui_select_device(int index);

// ============================================================================
// Image Operations (Simplified)
// ============================================================================

uft_error_t uft_gui_open_image(const char* path);
uft_error_t uft_gui_close_image(void);
uft_error_t uft_gui_save_image(const char* path, uft_format_t format);
bool uft_gui_has_image(void);
uft_error_t uft_gui_get_image_info(uft_gui_image_info_t* info);

// ============================================================================
// Format Recommendations
// ============================================================================

uft_error_t uft_gui_get_recommended_formats(uft_gui_format_list_t* list);

// ============================================================================
// Job Access (Simplified)
// ============================================================================

uft_job_manager_t* uft_gui_get_job_manager(void);
uft_error_t uft_gui_read_disk(const char* output_path,
                               uft_format_t format,
                               uint32_t* job_id);
uft_error_t uft_gui_cancel_job(uint32_t job_id);

#ifdef __cplusplus
}
#endif

#endif // UFT_GUI_BRIDGE_H
