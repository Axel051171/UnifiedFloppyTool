/**
 * @file uft_device_manager.h
 * @brief Device Manager - Abstract Device Layer for GUI
 * 
 * LAYER SEPARATION RULES:
 * - GUI inkludiert NUR diesen Header
 * - Keine Hardware-Details (GW, KF, etc.) exposed
 * - Observer-Pattern für async Updates
 */

#ifndef UFT_DEVICE_MANAGER_H
#define UFT_DEVICE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Opaque Type (versteckt Hardware-Details)
// ============================================================================

typedef struct uft_device_manager uft_device_manager_t;

// ============================================================================
// Abstract Device Capabilities (NICHT Hardware-spezifisch!)
// ============================================================================

typedef enum uft_device_cap {
    UFT_DEVICE_CAP_READ     = (1 << 0),
    UFT_DEVICE_CAP_WRITE    = (1 << 1),
    UFT_DEVICE_CAP_FLUX     = (1 << 2),
    UFT_DEVICE_CAP_VERIFY   = (1 << 3),
    UFT_DEVICE_CAP_FORMAT   = (1 << 4),
    UFT_DEVICE_CAP_ERASE    = (1 << 5),
} uft_device_cap_t;

// ============================================================================
// Abstract Device Info (GUI-safe - keine HW-Referenzen)
// ============================================================================

typedef struct uft_device_info {
    int         index;
    char        port[32];           // "/dev/ttyACM0" oder "COM3"
    char        firmware[16];       // "1.2"
    uint32_t    capabilities;       // Bitmask von uft_device_cap_t
    bool        connected;
    
    // KEINE Hardware-spezifischen Felder wie:
    // - uft_hw_type_t
    // - void* handle
    // - Backend-Pointer
} uft_device_info_t;

// ============================================================================
// Events (für Observer-Pattern)
// ============================================================================

typedef enum uft_device_event {
    UFT_DEVICE_EVENT_SCAN_START,
    UFT_DEVICE_EVENT_SCAN_COMPLETE,
    UFT_DEVICE_EVENT_CONNECTED,
    UFT_DEVICE_EVENT_DISCONNECTED,
    UFT_DEVICE_EVENT_SELECTED,
    UFT_DEVICE_EVENT_ERROR,
    UFT_DEVICE_EVENT_PROGRESS,
} uft_device_event_t;

typedef void (*uft_device_callback_t)(void* user_data,
                                       uft_device_event_t event,
                                       const uft_device_info_t* device);

// ============================================================================
// Lifecycle
// ============================================================================

uft_device_manager_t* uft_device_manager_create(void);
void uft_device_manager_destroy(uft_device_manager_t* mgr);

// ============================================================================
// Observer Pattern (GUI registriert sich hier)
// ============================================================================

uft_error_t uft_device_manager_add_observer(uft_device_manager_t* mgr,
                                             uft_device_callback_t callback,
                                             void* user_data,
                                             uint32_t event_mask);

uft_error_t uft_device_manager_remove_observer(uft_device_manager_t* mgr,
                                                uft_device_callback_t callback);

// ============================================================================
// Scanning
// ============================================================================

uft_error_t uft_device_manager_scan(uft_device_manager_t* mgr);
uft_error_t uft_device_manager_start_auto_scan(uft_device_manager_t* mgr);
void uft_device_manager_stop_auto_scan(uft_device_manager_t* mgr);

// ============================================================================
// Selection
// ============================================================================

uft_error_t uft_device_manager_select(uft_device_manager_t* mgr, int index);
int uft_device_manager_get_selected(const uft_device_manager_t* mgr);

// ============================================================================
// Query (alle Queries liefern abstrakte Info)
// ============================================================================

size_t uft_device_manager_get_count(const uft_device_manager_t* mgr);
const uft_device_info_t* uft_device_manager_get_device(const uft_device_manager_t* mgr, int index);
uft_error_t uft_device_manager_get_all(const uft_device_manager_t* mgr,
                                        uft_device_info_t* devices,
                                        size_t max_devices,
                                        size_t* actual_count);

// ============================================================================
// Status
// ============================================================================

bool uft_device_manager_is_busy(const uft_device_manager_t* mgr);
const char* uft_device_manager_get_status_string(const uft_device_manager_t* mgr);

#ifdef __cplusplus
}
#endif

#endif // UFT_DEVICE_MANAGER_H
