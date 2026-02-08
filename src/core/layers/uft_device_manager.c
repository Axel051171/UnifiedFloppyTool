/**
 * @file uft_device_manager.c
 * @brief Device Manager - Abstraktion zwischen GUI und Hardware
 * 
 * LAYER SEPARATION:
 * - GUI kennt nur abstrakte Device-Info
 * - Hardware-Details sind gekapselt
 * - Observer-Pattern für Status-Updates
 */

#include "uft/uft_device_manager.h"
#include "uft/uft_hardware.h"
#include "uft/uft_safe.h"
#include <stdlib.h>
#include <string.h>
#include "uft/compat/uft_thread.h"

// ============================================================================
// Constants
// ============================================================================

#define MAX_DEVICES         16
#define MAX_OBSERVERS       8
#define SCAN_INTERVAL_MS    1000

// ============================================================================
// Internal State
// ============================================================================

typedef struct {
    uft_device_callback_t   callback;
    void*                   user_data;
    uint32_t                event_mask;
} observer_t;

struct uft_device_manager {
    // Devices
    uft_device_info_t       devices[MAX_DEVICES];
    size_t                  device_count;
    int                     selected_device;
    
    // Observers
    observer_t              observers[MAX_OBSERVERS];
    size_t                  observer_count;
    
    // State
    bool                    initialized;
    bool                    scanning;
    bool                    auto_scan;
    
    // Threading
    pthread_mutex_t         lock;
    pthread_t               scan_thread;
    bool                    scan_thread_running;
};

// ============================================================================
// Lifecycle
// ============================================================================

uft_device_manager_t* uft_device_manager_create(void) {
    uft_device_manager_t* mgr = calloc(1, sizeof(uft_device_manager_t));
    if (!mgr) return NULL;
    
    pthread_mutex_init(&mgr->lock, NULL);
    mgr->selected_device = -1;
    mgr->initialized = true;
    
    return mgr;
}

void uft_device_manager_destroy(uft_device_manager_t* mgr) {
    if (!mgr) return;
    
    // Stop scan thread
    if (mgr->scan_thread_running) {
        mgr->scan_thread_running = false;
        pthread_join(mgr->scan_thread, NULL);
    }
    
    pthread_mutex_destroy(&mgr->lock);
    free(mgr);
}

// ============================================================================
// Observer Pattern
// ============================================================================

uft_error_t uft_device_manager_add_observer(uft_device_manager_t* mgr,
                                             uft_device_callback_t callback,
                                             void* user_data,
                                             uint32_t event_mask) {
    if (!mgr || !callback) return UFT_ERROR_NULL_POINTER;
    
    pthread_mutex_lock(&mgr->lock);
    
    if (mgr->observer_count >= MAX_OBSERVERS) {
        pthread_mutex_unlock(&mgr->lock);
        return UFT_ERROR_NO_SPACE;
    }
    
    observer_t* obs = &mgr->observers[mgr->observer_count++];
    obs->callback = callback;
    obs->user_data = user_data;
    obs->event_mask = event_mask;
    
    pthread_mutex_unlock(&mgr->lock);
    return UFT_OK;
}

uft_error_t uft_device_manager_remove_observer(uft_device_manager_t* mgr,
                                                uft_device_callback_t callback) {
    if (!mgr || !callback) return UFT_ERROR_NULL_POINTER;
    
    pthread_mutex_lock(&mgr->lock);
    
    for (size_t i = 0; i < mgr->observer_count; i++) {
        if (mgr->observers[i].callback == callback) {
            // Shift remaining
            for (size_t j = i; j + 1 < mgr->observer_count; j++) {
                mgr->observers[j] = mgr->observers[j + 1];
            }
            mgr->observer_count--;
            pthread_mutex_unlock(&mgr->lock);
            return UFT_OK;
        }
    }
    
    pthread_mutex_unlock(&mgr->lock);
    return UFT_ERROR_NOT_FOUND;
}

static void notify_observers(uft_device_manager_t* mgr,
                             uft_device_event_t event,
                             const uft_device_info_t* device) {
    for (size_t i = 0; i < mgr->observer_count; i++) {
        observer_t* obs = &mgr->observers[i];
        if (obs->event_mask & (1u << event)) {
            obs->callback(obs->user_data, event, device);
        }
    }
}

// ============================================================================
// Device Scanning
// ============================================================================

static void populate_device_info(uft_device_info_t* info,
                                  const uft_hw_info_t* hw_info,
                                  int index) {
    memset(info, 0, sizeof(*info));
    
    info->index = index;
    info->connected = true;
    
    // Copy from hardware info (abstrakt - keine HW-Details exponiert)
    if (hw_info) {
        snprintf(info->name, sizeof(info->name), "%s", 
                 hw_info->device_name[0] ? hw_info->device_name : "Unknown Device");
        snprintf(info->port, sizeof(info->port), "%s",
                 hw_info->port_name[0] ? hw_info->port_name : "");
        snprintf(info->firmware, sizeof(info->firmware), "%d.%d",
                 hw_info->firmware_major, hw_info->firmware_minor);
        
        // Abstrakte Capabilities - NICHT die Hardware-Typen!
        info->capabilities = 0;
        if (hw_info->caps & UFT_HW_CAP_READ) info->capabilities |= UFT_DEVICE_CAP_READ;
        if (hw_info->caps & UFT_HW_CAP_WRITE) info->capabilities |= UFT_DEVICE_CAP_WRITE;
        if (hw_info->caps & UFT_HW_CAP_FLUX) info->capabilities |= UFT_DEVICE_CAP_FLUX;
        if (hw_info->caps & UFT_HW_CAP_VERIFY) info->capabilities |= UFT_DEVICE_CAP_VERIFY;
    }
}

uft_error_t uft_device_manager_scan(uft_device_manager_t* mgr) {
    if (!mgr) return UFT_ERROR_NULL_POINTER;
    
    pthread_mutex_lock(&mgr->lock);
    
    if (mgr->scanning) {
        pthread_mutex_unlock(&mgr->lock);
        return UFT_ERROR_BUSY;
    }
    
    mgr->scanning = true;
    notify_observers(mgr, UFT_DEVICE_EVENT_SCAN_START, NULL);
    
    pthread_mutex_unlock(&mgr->lock);
    
    // Hardware scannen
    uft_hw_info_t hw_devices[MAX_DEVICES];
    size_t found = 0;
    
    uft_error_t err = uft_hw_enumerate(hw_devices, MAX_DEVICES, &found);
    
    pthread_mutex_lock(&mgr->lock);
    
    // Alte Liste mit neuer vergleichen
    size_t old_count = mgr->device_count;
    
    // Neue Devices aktualisieren
    mgr->device_count = 0;
    for (size_t i = 0; i < found; i++) {
        populate_device_info(&mgr->devices[i], &hw_devices[i], (int)i);
        mgr->device_count++;
        
        // Check if new device
        bool is_new = true;
        for (size_t j = 0; j < old_count; j++) {
            // Simplified comparison
            is_new = false;
            break;
        }
        
        if (is_new) {
            notify_observers(mgr, UFT_DEVICE_EVENT_CONNECTED, &mgr->devices[i]);
        }
    }
    
    mgr->scanning = false;
    notify_observers(mgr, UFT_DEVICE_EVENT_SCAN_COMPLETE, NULL);
    
    pthread_mutex_unlock(&mgr->lock);
    
    return err;
}

// Background scan thread
static void* scan_thread_func(void* arg) {
    uft_device_manager_t* mgr = (uft_device_manager_t*)arg;
    
    while (mgr->scan_thread_running) {
        uft_device_manager_scan(mgr);
        
        // Sleep (simplified)
        struct timespec ts = { .tv_sec = 0, .tv_nsec = SCAN_INTERVAL_MS * 1000000 };
        nanosleep(&ts, NULL);
    }
    
    return NULL;
}

uft_error_t uft_device_manager_start_auto_scan(uft_device_manager_t* mgr) {
    if (!mgr) return UFT_ERROR_NULL_POINTER;
    
    if (mgr->scan_thread_running) return UFT_OK;
    
    mgr->scan_thread_running = true;
    mgr->auto_scan = true;
    
    if (pthread_create(&mgr->scan_thread, NULL, scan_thread_func, mgr) != 0) {
        mgr->scan_thread_running = false;
        return UFT_ERROR_THREAD;
    }
    
    return UFT_OK;
}

void uft_device_manager_stop_auto_scan(uft_device_manager_t* mgr) {
    if (!mgr || !mgr->scan_thread_running) return;
    
    mgr->scan_thread_running = false;
    mgr->auto_scan = false;
    pthread_join(mgr->scan_thread, NULL);
}

// ============================================================================
// Device Selection
// ============================================================================

uft_error_t uft_device_manager_select(uft_device_manager_t* mgr, int index) {
    if (!mgr) return UFT_ERROR_NULL_POINTER;
    
    pthread_mutex_lock(&mgr->lock);
    
    if (index < 0 || (size_t)index >= mgr->device_count) {
        pthread_mutex_unlock(&mgr->lock);
        return UFT_ERROR_INVALID_ARG;
    }
    
    int old_selection = mgr->selected_device;
    mgr->selected_device = index;
    
    if (old_selection != index) {
        notify_observers(mgr, UFT_DEVICE_EVENT_SELECTED, &mgr->devices[index]);
    }
    
    pthread_mutex_unlock(&mgr->lock);
    return UFT_OK;
}

int uft_device_manager_get_selected(const uft_device_manager_t* mgr) {
    if (!mgr) return -1;
    return mgr->selected_device;
}

// ============================================================================
// Device Query
// ============================================================================

size_t uft_device_manager_get_count(const uft_device_manager_t* mgr) {
    if (!mgr) return 0;
    return mgr->device_count;
}

const uft_device_info_t* uft_device_manager_get_device(const uft_device_manager_t* mgr,
                                                        int index) {
    if (!mgr) return NULL;
    if (index < 0 || (size_t)index >= mgr->device_count) return NULL;
    return &mgr->devices[index];
}

uft_error_t uft_device_manager_get_all(const uft_device_manager_t* mgr,
                                        uft_device_info_t* devices,
                                        size_t max_devices,
                                        size_t* actual_count) {
    if (!mgr || !devices || !actual_count) return UFT_ERROR_NULL_POINTER;
    
    size_t count = mgr->device_count < max_devices ? mgr->device_count : max_devices;
    memcpy(devices, mgr->devices, count * sizeof(uft_device_info_t));
    *actual_count = count;
    
    return UFT_OK;
}

// ============================================================================
// Status Query (Abstract - no hardware details exposed)
// ============================================================================

bool uft_device_manager_is_busy(const uft_device_manager_t* mgr) {
    if (!mgr) return false;
    return mgr->scanning;
}

const char* uft_device_manager_get_status_string(const uft_device_manager_t* mgr) {
    if (!mgr) return "Invalid";
    
    if (mgr->scanning) return "Scanning...";
    if (mgr->device_count == 0) return "No devices found";
    if (mgr->selected_device < 0) return "No device selected";
    
    return "Ready";
}
