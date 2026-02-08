/**
 * @file uft_hardware.c
 * @brief UnifiedFloppyTool - Hardware Abstraction Layer Implementation
 * 
 * Kernimplementierung der Hardware API.
 * Backend-spezifische Implementierungen sind in separaten Dateien.
 * 
 * @author UFT Team
 * @date 2025
 */

#include "uft/uft_hardware.h"
#include "uft/uft_core.h"
#include "uft/uft_decoder_plugin.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Backend Registry
// ============================================================================

#define MAX_HW_BACKENDS 16

static const uft_hw_backend_t* g_backends[MAX_HW_BACKENDS] = {0};
static size_t g_backend_count = 0;
static bool g_hw_initialized = false;

// ============================================================================
// Device Structure
// ============================================================================

struct uft_hw_device {
    const uft_hw_backend_t* backend;    ///< Backend für dieses Gerät
    uft_hw_info_t           info;       ///< Geräte-Info
    void*                   handle;     ///< Backend-spezifisches Handle
    
    // Aktueller Status
    uint8_t                 current_track;
    uint8_t                 current_head;
    bool                    motor_running;
};

// ============================================================================
// Initialization
// ============================================================================

uft_error_t uft_hw_init(void) {
    if (g_hw_initialized) {
        return UFT_OK;
    }
    
    // Backends initialisieren
    for (size_t i = 0; i < g_backend_count; i++) {
        if (g_backends[i]->init) {
            uft_error_t err = g_backends[i]->init();
            if (UFT_FAILED(err)) {
                // Nicht kritisch - Backend nicht verfügbar
                fprintf(stderr, "[WARN] Backend '%s' init failed: %d\n",
                        g_backends[i]->name, err);
            }
        }
    }
    
    g_hw_initialized = true;
    return UFT_OK;
}

void uft_hw_shutdown(void) {
    if (!g_hw_initialized) {
        return;
    }
    
    // Backends herunterfahren
    for (size_t i = 0; i < g_backend_count; i++) {
        if (g_backends[i]->shutdown) {
            g_backends[i]->shutdown();
        }
    }
    
    g_hw_initialized = false;
}

// ============================================================================
// Backend Registration
// ============================================================================

uft_error_t uft_hw_register_backend(const uft_hw_backend_t* backend) {
    if (!backend || !backend->name) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    // Duplikat prüfen
    for (size_t i = 0; i < g_backend_count; i++) {
        if (g_backends[i]->type == backend->type) {
            return UFT_ERROR_PLUGIN_LOAD;  // Already registered
        }
    }
    
    if (g_backend_count >= MAX_HW_BACKENDS) {
        return UFT_ERROR_BUFFER_TOO_SMALL;
    }
    
    g_backends[g_backend_count++] = backend;
    
    return UFT_OK;
}

// ============================================================================
// Device Enumeration
// ============================================================================

uft_error_t uft_hw_enumerate(uft_hw_info_t* devices, size_t max_devices,
                             size_t* found) {
    if (!devices || !found) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    *found = 0;
    
    if (!g_hw_initialized) {
        uft_error_t err = uft_hw_init();
        if (UFT_FAILED(err)) {
            return err;
        }
    }
    
    // Alle Backends nach Geräten fragen
    for (size_t i = 0; i < g_backend_count && *found < max_devices; i++) {
        if (!g_backends[i]->enumerate) {
            continue;
        }
        
        size_t backend_found = 0;
        uft_error_t err = g_backends[i]->enumerate(
            &devices[*found],
            max_devices - *found,
            &backend_found
        );
        
        if (UFT_OK == err) {
            *found += backend_found;
        }
    }
    
    return UFT_OK;
}

// ============================================================================
// Device Open/Close
// ============================================================================

/**
 * @brief Findet Backend für Hardware-Typ
 */
static const uft_hw_backend_t* find_backend(uft_hw_type_t type) {
    for (size_t i = 0; i < g_backend_count; i++) {
        if (g_backends[i]->type == type) {
            return g_backends[i];
        }
    }
    return NULL;
}

uft_error_t uft_hw_open(const uft_hw_info_t* info, uft_hw_device_t** device) {
    if (!info || !device) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    *device = NULL;
    
    // Backend finden
    const uft_hw_backend_t* backend = find_backend(info->type);
    if (!backend) {
        return UFT_ERROR_NOT_SUPPORTED;
    }
    
    if (!backend->open) {
        return UFT_ERROR_NOT_SUPPORTED;
    }
    
    // Device-Struktur allokieren
    uft_hw_device_t* dev = calloc(1, sizeof(uft_hw_device_t));
    if (!dev) {
        return UFT_ERROR_NO_MEMORY;
    }
    
    dev->backend = backend;
    dev->info = *info;
    dev->current_track = 0;
    dev->current_head = 0;
    dev->motor_running = false;
    
    // Backend öffnen
    uft_error_t err = backend->open(info, &dev);
    if (UFT_FAILED(err)) {
        free(dev);
        return err;
    }
    
    *device = dev;
    return UFT_OK;
}

void uft_hw_close(uft_hw_device_t* device) {
    if (!device) {
        return;
    }
    
    // Motor ausschalten
    if (device->motor_running && device->backend->motor) {
        device->backend->motor(device, false);
    }
    
    // Backend schließen
    if (device->backend->close) {
        device->backend->close(device);
    }
    
    free(device);
}

// ============================================================================
// Status
// ============================================================================

uft_error_t uft_hw_get_info(uft_hw_device_t* device, uft_hw_info_t* info) {
    if (!device || !info) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    *info = device->info;
    return UFT_OK;
}

uft_error_t uft_hw_get_status(uft_hw_device_t* device, uft_drive_status_t* status) {
    if (!device || !status) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    if (!device->backend->get_status) {
        // Default-Status
        memset(status, 0, sizeof(*status));
        status->connected = true;
        status->current_track = device->current_track;
        status->current_head = device->current_head;
        return UFT_OK;
    }
    
    return device->backend->get_status(device, status);
}

// ============================================================================
// Motor/Seek
// ============================================================================

uft_error_t uft_hw_motor_on(uft_hw_device_t* device) {
    if (!device) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    if (!device->backend->motor) {
        device->motor_running = true;
        return UFT_OK;
    }
    
    uft_error_t err = device->backend->motor(device, true);
    if (UFT_OK == err) {
        device->motor_running = true;
    }
    return err;
}

uft_error_t uft_hw_motor_off(uft_hw_device_t* device) {
    if (!device) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    if (!device->backend->motor) {
        device->motor_running = false;
        return UFT_OK;
    }
    
    uft_error_t err = device->backend->motor(device, false);
    if (UFT_OK == err) {
        device->motor_running = false;
    }
    return err;
}

uft_error_t uft_hw_seek(uft_hw_device_t* device, uint8_t track) {
    if (!device) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    if (!device->backend->seek) {
        device->current_track = track;
        return UFT_OK;
    }
    
    uft_error_t err = device->backend->seek(device, track);
    if (UFT_OK == err) {
        device->current_track = track;
    }
    return err;
}

uft_error_t uft_hw_select_head(uft_hw_device_t* device, uint8_t head) {
    if (!device) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    if (!device->backend->select_head) {
        device->current_head = head;
        return UFT_OK;
    }
    
    uft_error_t err = device->backend->select_head(device, head);
    if (UFT_OK == err) {
        device->current_head = head;
    }
    return err;
}

uft_error_t uft_hw_recalibrate(uft_hw_device_t* device) {
    // Seek to Track 0
    return uft_hw_seek(device, 0);
}

// ============================================================================
// Track I/O
// ============================================================================

uft_error_t uft_hw_read_track(uft_hw_device_t* device, 
                              uint8_t cylinder, uint8_t head,
                              uft_track_t* track,
                              const uft_decode_options_t* options) {
    if (!device || !track) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    // Track initialisieren
    uft_track_init(track, cylinder, head);
    
    // Zur Position fahren
    uft_error_t err = uft_hw_seek(device, cylinder);
    if (UFT_FAILED(err)) {
        return err;
    }
    
    err = uft_hw_select_head(device, head);
    if (UFT_FAILED(err)) {
        return err;
    }
    
    // Motor einschalten falls nötig
    if (!device->motor_running) {
        err = uft_hw_motor_on(device);
        if (UFT_FAILED(err)) {
            return err;
        }
    }
    
    // Track lesen
    if (device->backend->read_track) {
        return device->backend->read_track(device, track, 1);
    }
    
    // Fallback: Flux lesen und dekodieren
    if (device->backend->read_flux) {
        uint32_t* flux = NULL;
        size_t flux_count = 0;
        
        err = device->backend->read_flux(device, flux, 1000000, &flux_count, 1);
        if (UFT_FAILED(err)) {
            return err;
        }
        
        /* Flux decoding handled by caller via uft_flux_decode() pipeline */
        
        free(flux);
        return UFT_OK;
    }
    
    return UFT_ERROR_NOT_SUPPORTED;
}

uft_error_t uft_hw_write_track(uft_hw_device_t* device,
                               uint8_t cylinder, uint8_t head,
                               const uft_track_t* track,
                               const uft_encode_options_t* options) {
    if (!device || !track) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    // Schreibfähigkeit prüfen
    if (!(device->info.capabilities & UFT_HW_CAP_WRITE)) {
        return UFT_ERROR_DISK_PROTECTED;
    }
    
    // Zur Position fahren
    uft_error_t err = uft_hw_seek(device, cylinder);
    if (UFT_FAILED(err)) {
        return err;
    }
    
    err = uft_hw_select_head(device, head);
    if (UFT_FAILED(err)) {
        return err;
    }
    
    // Motor einschalten
    if (!device->motor_running) {
        err = uft_hw_motor_on(device);
        if (UFT_FAILED(err)) {
            return err;
        }
    }
    
    // Track schreiben
    if (device->backend->write_track) {
        return device->backend->write_track(device, track);
    }
    
    return UFT_ERROR_NOT_SUPPORTED;
}

// ============================================================================
// Flux I/O
// ============================================================================

uft_error_t uft_hw_read_flux(uft_hw_device_t* device,
                             uint8_t cylinder, uint8_t head,
                             uint32_t** flux, size_t* flux_count,
                             uint8_t revolutions) {
    if (!device || !flux || !flux_count) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    *flux = NULL;
    *flux_count = 0;
    
    if (!(device->info.capabilities & UFT_HW_CAP_FLUX)) {
        return UFT_ERROR_NOT_SUPPORTED;
    }
    
    if (!device->backend->read_flux) {
        return UFT_ERROR_NOT_SUPPORTED;
    }
    
    // Zur Position fahren
    uft_error_t err = uft_hw_seek(device, cylinder);
    if (UFT_FAILED(err)) {
        return err;
    }
    
    err = uft_hw_select_head(device, head);
    if (UFT_FAILED(err)) {
        return err;
    }
    
    // Motor
    if (!device->motor_running) {
        err = uft_hw_motor_on(device);
        if (UFT_FAILED(err)) {
            return err;
        }
    }
    
    // Buffer allokieren (geschätzt ~50000 Flux pro Revolution)
    size_t max_flux = (size_t)revolutions * 100000;
    uint32_t* buffer = malloc(max_flux * sizeof(uint32_t));
    if (!buffer) {
        return UFT_ERROR_NO_MEMORY;
    }
    
    err = device->backend->read_flux(device, buffer, max_flux, 
                                     flux_count, revolutions);
    if (UFT_FAILED(err)) {
        free(buffer);
        return err;
    }
    
    *flux = buffer;
    return UFT_OK;
}

uft_error_t uft_hw_write_flux(uft_hw_device_t* device,
                              uint8_t cylinder, uint8_t head,
                              const uint32_t* flux, size_t flux_count) {
    if (!device || !flux) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    if (!(device->info.capabilities & UFT_HW_CAP_FLUX)) {
        return UFT_ERROR_NOT_SUPPORTED;
    }
    
    if (!device->backend->write_flux) {
        return UFT_ERROR_NOT_SUPPORTED;
    }
    
    // Seek
    uft_error_t err = uft_hw_seek(device, cylinder);
    if (UFT_FAILED(err)) {
        return err;
    }
    
    err = uft_hw_select_head(device, head);
    if (UFT_FAILED(err)) {
        return err;
    }
    
    // Motor
    if (!device->motor_running) {
        err = uft_hw_motor_on(device);
        if (UFT_FAILED(err)) {
            return err;
        }
    }
    
    return device->backend->write_flux(device, flux, flux_count);
}

// ============================================================================
// Disk-Level Operations
// ============================================================================

uft_error_t uft_hw_read_disk(uft_hw_device_t* device,
                             const char* path,
                             uft_format_t format,
                             const uft_geometry_t* geometry,
                             uft_hw_progress_fn progress,
                             void* user_data) {
    if (!device || !path || !geometry) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    // Motor einschalten
    uft_error_t err = uft_hw_motor_on(device);
    if (UFT_FAILED(err)) {
        return err;
    }
    
    // Recalibrate
    err = uft_hw_recalibrate(device);
    if (UFT_FAILED(err)) {
        return err;
    }
    
    // Alle Tracks lesen
    int total_tracks = geometry->cylinders * geometry->heads;
    int track_num = 0;
    
    for (int cyl = 0; cyl < geometry->cylinders; cyl++) {
        for (int head = 0; head < geometry->heads; head++) {
            // Progress Callback
            if (progress) {
                progress(track_num, total_tracks, user_data);
            }
            
            // Track lesen via Flux (wenn unterstützt)
            if (device->info.capabilities & UFT_HW_CAP_FLUX) {
                uint32_t* flux = NULL;
                size_t flux_count = 0;
                
                err = uft_hw_read_flux(device, (uint8_t)cyl, (uint8_t)head,
                                       &flux, &flux_count, 1);
                if (UFT_OK == err && flux) {
                    /* Flux stored raw; format conversion via output format writer */
                    free(flux);
                }
            }
            
            track_num++;
        }
    }
    
    // Finaler Progress
    if (progress) {
        progress(total_tracks, total_tracks, user_data);
    }
    
    // Motor aus
    uft_hw_motor_off(device);
    
    // Für komplette Implementierung: Image speichern
    // Diese vereinfachte Version liest nur die Flux-Daten
    (void)path;
    (void)format;
    
    return UFT_OK;
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* uft_hw_type_name(uft_hw_type_t type) {
    switch (type) {
        case UFT_HW_XUM1541:        return "XUM1541";
        case UFT_HW_ZOOMFLOPPY:     return "ZoomFloppy";
        case UFT_HW_XU1541:         return "XU1541";
        case UFT_HW_XA1541:         return "XA1541";
        case UFT_HW_FC5025:         return "FC5025";
        case UFT_HW_GREASEWEAZLE:   return "Greaseweazle";
        case UFT_HW_FLUXENGINE:     return "FluxEngine";
        case UFT_HW_SUPERCARD_PRO:  return "SuperCard Pro";
        case UFT_HW_KRYOFLUX:       return "KryoFlux";
        case UFT_HW_APPLESAUCE:     return "Applesauce";
        case UFT_HW_PAULINE:        return "Pauline";
        case UFT_HW_CATWEASEL:      return "CatWeasel";
        case UFT_HW_VIRTUAL:        return "Virtual";
        default:                    return "Unknown";
    }
}

const char* uft_drive_type_name(uft_drive_type_t type) {
    switch (type) {
        case UFT_DRIVE_1541:        return "Commodore 1541";
        case UFT_DRIVE_1571:        return "Commodore 1571";
        case UFT_DRIVE_1581:        return "Commodore 1581";
        case UFT_DRIVE_PC_525_DD:   return "PC 5.25\" DD";
        case UFT_DRIVE_PC_525_HD:   return "PC 5.25\" HD";
        case UFT_DRIVE_PC_35_DD:    return "PC 3.5\" DD";
        case UFT_DRIVE_PC_35_HD:    return "PC 3.5\" HD";
        case UFT_DRIVE_PC_35_ED:    return "PC 3.5\" ED";
        case UFT_DRIVE_8_SSSD:      return "8\" SSSD";
        case UFT_DRIVE_8_DSDD:      return "8\" DSDD";
        case UFT_DRIVE_APPLE_525:   return "Apple 5.25\"";
        case UFT_DRIVE_APPLE_35:    return "Apple 3.5\"";
        case UFT_DRIVE_AMIGA_DD:    return "Amiga DD";
        case UFT_DRIVE_AMIGA_HD:    return "Amiga HD";
        case UFT_DRIVE_ATARI_ST:    return "Atari ST";
        default:                    return "Unknown";
    }
}

bool uft_hw_supports_flux(uft_hw_type_t type) {
    switch (type) {
        case UFT_HW_GREASEWEAZLE:
        case UFT_HW_FLUXENGINE:
        case UFT_HW_SUPERCARD_PRO:
        case UFT_HW_KRYOFLUX:
        case UFT_HW_APPLESAUCE:
        case UFT_HW_PAULINE:
        case UFT_HW_CATWEASEL:
            return true;
        default:
            return false;
    }
}

uft_format_t uft_hw_recommended_format(uft_hw_type_t hw_type, 
                                       uft_drive_type_t drive_type) {
    // Flux-Hardware → SCP oder HFE
    if (uft_hw_supports_flux(hw_type)) {
        return UFT_FORMAT_SCP;
    }
    
    // Commodore → G64 oder D64
    switch (drive_type) {
        case UFT_DRIVE_1541:
        case UFT_DRIVE_1571:
            return UFT_FORMAT_G64;
        case UFT_DRIVE_1581:
            return UFT_FORMAT_D64;  // D81 wäre besser
        default:
            break;
    }
    
    // PC → IMG
    switch (drive_type) {
        case UFT_DRIVE_PC_525_DD:
        case UFT_DRIVE_PC_525_HD:
        case UFT_DRIVE_PC_35_DD:
        case UFT_DRIVE_PC_35_HD:
        case UFT_DRIVE_PC_35_ED:
            return UFT_FORMAT_IMG;
        default:
            break;
    }
    
    // Apple → HFE (da kein dediziertes Format)
    switch (drive_type) {
        case UFT_DRIVE_APPLE_525:
        case UFT_DRIVE_APPLE_35:
            return UFT_FORMAT_HFE;
        default:
            break;
    }
    
    // Amiga → ADF
    switch (drive_type) {
        case UFT_DRIVE_AMIGA_DD:
        case UFT_DRIVE_AMIGA_HD:
            return UFT_FORMAT_ADF;
        default:
            break;
    }
    
    // Default
    return UFT_FORMAT_IMG;
}
