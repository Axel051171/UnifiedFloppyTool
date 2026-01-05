/**
 * @file uft_gui_bridge.c
 * @brief GUI Bridge - Saubere Schnittstelle für GUI-Layer
 * 
 * LAYER SEPARATION COMPLETE:
 * - GUI inkludiert nur diesen Header + Device Manager + Job Manager
 * - Keine Hardware-Details, keine Format-Details
 * - Alles über abstrakte Interfaces
 */

#include "uft/uft_gui_bridge.h"
#include "uft/uft_device_manager.h"
#include "uft/uft_job_manager.h"
#include "uft/uft_format_advisor.h"
#include "uft/uft_unified_image.h"
#include "uft/uft_safe.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Global Bridge State
// ============================================================================

static struct {
    uft_device_manager_t*   device_mgr;
    uft_job_manager_t*      job_mgr;
    uft_unified_image_t*    current_image;
    
    uft_gui_status_callback_t status_callback;
    void*                   status_user;
    
    bool                    initialized;
} g_bridge = {0};

// ============================================================================
// Initialization
// ============================================================================

uft_error_t uft_gui_bridge_init(void) {
    if (g_bridge.initialized) return UFT_OK;
    
    g_bridge.device_mgr = uft_device_manager_create();
    if (!g_bridge.device_mgr) return UFT_ERROR_NO_MEMORY;
    
    g_bridge.job_mgr = uft_job_manager_create(4);
    if (!g_bridge.job_mgr) {
        uft_device_manager_destroy(g_bridge.device_mgr);
        return UFT_ERROR_NO_MEMORY;
    }
    
    g_bridge.initialized = true;
    return UFT_OK;
}

void uft_gui_bridge_shutdown(void) {
    if (!g_bridge.initialized) return;
    
    if (g_bridge.current_image) {
        uft_image_destroy(g_bridge.current_image);
    }
    
    uft_job_manager_destroy(g_bridge.job_mgr);
    uft_device_manager_destroy(g_bridge.device_mgr);
    
    memset(&g_bridge, 0, sizeof(g_bridge));
}

// ============================================================================
// Status Callback
// ============================================================================

void uft_gui_bridge_set_status_callback(uft_gui_status_callback_t callback,
                                         void* user_data) {
    g_bridge.status_callback = callback;
    g_bridge.status_user = user_data;
}

static void notify_status(const char* message) {
    if (g_bridge.status_callback) {
        g_bridge.status_callback(g_bridge.status_user, message);
    }
}

// ============================================================================
// Device Access (Delegiert an Device Manager)
// ============================================================================

uft_device_manager_t* uft_gui_get_device_manager(void) {
    return g_bridge.device_mgr;
}

uft_error_t uft_gui_scan_devices(void) {
    notify_status("Scanning for devices...");
    uft_error_t err = uft_device_manager_scan(g_bridge.device_mgr);
    
    size_t count = uft_device_manager_get_count(g_bridge.device_mgr);
    if (count > 0) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Found %zu device(s)", count);
        notify_status(msg);
    } else {
        notify_status("No devices found");
    }
    
    return err;
}

size_t uft_gui_get_device_count(void) {
    return uft_device_manager_get_count(g_bridge.device_mgr);
}

const uft_device_info_t* uft_gui_get_device(int index) {
    return uft_device_manager_get_device(g_bridge.device_mgr, index);
}

uft_error_t uft_gui_select_device(int index) {
    return uft_device_manager_select(g_bridge.device_mgr, index);
}

// ============================================================================
// Image Operations (Vereinfacht für GUI)
// ============================================================================

uft_error_t uft_gui_open_image(const char* path) {
    if (!path) return UFT_ERROR_NULL_POINTER;
    
    notify_status("Opening image...");
    
    // Close existing
    if (g_bridge.current_image) {
        uft_image_destroy(g_bridge.current_image);
    }
    
    g_bridge.current_image = uft_image_create();
    if (!g_bridge.current_image) {
        notify_status("Error: Out of memory");
        return UFT_ERROR_NO_MEMORY;
    }
    
    uft_error_t err = uft_image_open(g_bridge.current_image, path);
    if (err != UFT_OK) {
        uft_image_destroy(g_bridge.current_image);
        g_bridge.current_image = NULL;
        notify_status("Error: Failed to open image");
        return err;
    }
    
    notify_status("Image loaded successfully");
    return UFT_OK;
}

uft_error_t uft_gui_close_image(void) {
    if (g_bridge.current_image) {
        uft_image_destroy(g_bridge.current_image);
        g_bridge.current_image = NULL;
        notify_status("Image closed");
    }
    return UFT_OK;
}

uft_error_t uft_gui_save_image(const char* path, uft_format_t format) {
    if (!g_bridge.current_image) return UFT_ERROR_NO_DATA;
    if (!path) return UFT_ERROR_NULL_POINTER;
    
    notify_status("Saving image...");
    uft_error_t err = uft_image_save(g_bridge.current_image, path, format);
    
    if (err == UFT_OK) {
        notify_status("Image saved successfully");
    } else {
        notify_status("Error: Failed to save image");
    }
    
    return err;
}

bool uft_gui_has_image(void) {
    return g_bridge.current_image != NULL;
}

// ============================================================================
// Image Info (für GUI-Display)
// ============================================================================

uft_error_t uft_gui_get_image_info(uft_gui_image_info_t* info) {
    if (!info) return UFT_ERROR_NULL_POINTER;
    
    memset(info, 0, sizeof(*info));
    
    if (!g_bridge.current_image) {
        return UFT_ERROR_NO_DATA;
    }
    
    const uft_unified_image_t* img = g_bridge.current_image;
    
    info->cylinders = img->geometry.cylinders;
    info->heads = img->geometry.heads;
    info->sectors_per_track = img->geometry.sectors_per_track;
    info->sector_size = img->geometry.sector_size;
    info->total_sectors = img->sector_meta.total_sectors;
    info->bad_sectors = img->sector_meta.bad_sectors;
    
    info->format = img->detected_format;
    info->has_flux = uft_image_has_layer(img, UFT_LAYER_FLUX);
    info->modified = img->modified;
    
    snprintf(info->format_name, sizeof(info->format_name), "%s",
             uft_format_get_name(img->detected_format));
    
    if (img->path) {
        const char* filename = strrchr(img->path, '/');
        if (!filename) filename = strrchr(img->path, '\\');
        if (filename) filename++;
        else filename = img->path;
        
        snprintf(info->filename, sizeof(info->filename), "%s", filename);
    }
    
    return UFT_OK;
}

// ============================================================================
// Format Recommendation (für "Save As" Dialog)
// ============================================================================

uft_error_t uft_gui_get_recommended_formats(uft_gui_format_list_t* list) {
    if (!list) return UFT_ERROR_NULL_POINTER;
    
    memset(list, 0, sizeof(*list));
    
    if (!g_bridge.current_image) {
        // Defaults wenn kein Image
        list->formats[0] = (uft_gui_format_entry_t){
            .format = UFT_FORMAT_IMG,
            .name = "Raw Image",
            .extension = ".img"
        };
        list->formats[1] = (uft_gui_format_entry_t){
            .format = UFT_FORMAT_SCP,
            .name = "SuperCard Pro",
            .extension = ".scp"
        };
        list->count = 2;
        return UFT_OK;
    }
    
    uft_format_advice_t advice;
    uft_get_format_advice(g_bridge.current_image, &advice);
    
    // Recommended first
    list->formats[0] = (uft_gui_format_entry_t){
        .format = advice.recommended_format,
        .name = uft_format_get_name(advice.recommended_format),
        .extension = uft_format_get_extension(advice.recommended_format),
        .recommended = true
    };
    list->count = 1;
    
    // Alternatives
    for (int i = 0; i < advice.alternative_count && list->count < 16; i++) {
        list->formats[list->count++] = (uft_gui_format_entry_t){
            .format = advice.alternatives[i],
            .name = uft_format_get_name(advice.alternatives[i]),
            .extension = uft_format_get_extension(advice.alternatives[i]),
            .recommended = false
        };
    }
    
    return UFT_OK;
}

// ============================================================================
// Job Access (Delegiert an Job Manager)
// ============================================================================

uft_job_manager_t* uft_gui_get_job_manager(void) {
    return g_bridge.job_mgr;
}

// ============================================================================
// Convenience: Read Disk
// ============================================================================

static void read_job_callback(void* user, const uft_job_status_t* status) {
    char msg[128];
    
    switch (status->state) {
        case UFT_JOB_STATE_RUNNING:
            snprintf(msg, sizeof(msg), "Reading: %d%% - %s",
                     status->progress_percent,
                     status->progress_message ? status->progress_message : "");
            break;
        case UFT_JOB_STATE_COMPLETED:
            snprintf(msg, sizeof(msg), "Read completed successfully");
            break;
        case UFT_JOB_STATE_CANCELLED:
            snprintf(msg, sizeof(msg), "Read cancelled");
            break;
        case UFT_JOB_STATE_FAILED:
            snprintf(msg, sizeof(msg), "Read failed: %s", uft_error_string(status->result));
            break;
        default:
            return;
    }
    
    if (g_bridge.status_callback) {
        g_bridge.status_callback(g_bridge.status_user, msg);
    }
}

uft_error_t uft_gui_read_disk(const char* output_path,
                               uft_format_t format,
                               uint32_t* job_id) {
    int device = uft_device_manager_get_selected(g_bridge.device_mgr);
    if (device < 0) {
        notify_status("Error: No device selected");
        return UFT_ERROR_NO_DEVICE;
    }
    
    uft_read_job_params_t params = {
        .device_index = device,
        .start_track = 0,
        .end_track = -1,  // All
        .retries = 3,
        .output_path = output_path,
        .output_format = format
    };
    
    return uft_job_submit_read(g_bridge.job_mgr, &params,
                                read_job_callback, NULL, job_id);
}

uft_error_t uft_gui_cancel_job(uint32_t job_id) {
    return uft_job_cancel(g_bridge.job_mgr, job_id);
}
