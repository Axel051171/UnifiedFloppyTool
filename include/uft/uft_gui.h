/**
 * @file uft_gui.h
 * @brief UnifiedFloppyTool - GUI Integration Header
 * 
 * DIESER HEADER IST FÜR GUI-ENTWICKLER BESTIMMT!
 * 
 * Enthält nur die abstrakten Interfaces:
 * - Keine Hardware-Details
 * - Keine Format-Interna
 * - Keine Low-Level Decoder
 */

#ifndef UFT_GUI_H
#define UFT_GUI_H

// Essential types only
#include "uft_types.h"
#include "uft_error.h"

// GUI-safe abstractions
#include "uft_gui_bridge.h"
#include "uft_device_manager.h"
#include "uft_job_manager.h"
#include "uft_format_advisor.h"

/*
 * USAGE EXAMPLE (Qt):
 * 
 * // In MainWindow constructor:
 * uft_gui_bridge_init();
 * uft_gui_bridge_set_status_callback(statusCallback, this);
 * 
 * // Connect device manager to UI:
 * uft_device_manager_add_observer(
 *     uft_gui_get_device_manager(),
 *     deviceCallback,
 *     this,
 *     0xFFFF  // All events
 * );
 * 
 * // Start auto-scan:
 * uft_device_manager_start_auto_scan(uft_gui_get_device_manager());
 * 
 * // Read disk (async):
 * uint32_t job_id;
 * uft_gui_read_disk("/output/disk.scp", UFT_FORMAT_SCP, &job_id);
 * 
 * // In destructor:
 * uft_gui_bridge_shutdown();
 */

#ifdef __cplusplus
extern "C" {
#endif

// Convenience: All-in-one init for GUI apps
static inline uft_error_t uft_gui_init_all(void) {
    return uft_gui_bridge_init();
}

static inline void uft_gui_shutdown_all(void) {
    uft_gui_bridge_shutdown();
}

#ifdef __cplusplus
}
#endif

// Qt-specific helpers (if Qt is available)
#ifdef QT_CORE_LIB

#include <QObject>
#include <QString>

// Helper to convert UFT strings to QString
inline QString uft_to_qstring(const char* s) {
    return s ? QString::fromUtf8(s) : QString();
}

// Helper to convert UFT error to QString
inline QString uft_error_qstring(uft_error_t err) {
    return uft_to_qstring(uft_error_string(err));
}

#endif // QT_CORE_LIB

#endif // UFT_GUI_H
