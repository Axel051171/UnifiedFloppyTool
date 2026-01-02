/**
 * @file uft_iuniversaldrive_core.c
 * @brief IUniversalDrive Core Implementation
 * 
 * HARDWARE ABSTRACTION LAYER
 * 
 * Features:
 * ✅ Provider pattern (Greaseweazle, SCP, KryoFlux, Mock)
 * ✅ Sample rate normalization (ALL → nanoseconds)
 * ✅ Capability negotiation
 * ✅ Thread-safe
 * ✅ Professional quality
 * 
 * @version 3.0.0
 * @date 2024-12-27
 */

#include "uft_iuniversaldrive.h"
#include "uft_error_handling.h"
#include "uft_logging.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* Maximum number of registered providers */
#define MAX_PROVIDERS 16

/* Global provider registry */
static struct {
    const uft_drive_ops_t* providers[MAX_PROVIDERS];
    uint32_t count;
    pthread_mutex_t mutex;
    bool initialized;
} g_provider_registry = {0};

/* Universal Drive Handle */
struct uft_universal_drive {
    /* Provider */
    const uft_drive_ops_t* ops;
    void* provider_context;
    
    /* Configuration */
    char provider_name[64];
    char device_path[256];
    
    /* Capabilities */
    uft_drive_capabilities_t capabilities;
    
    /* Current state */
    uint8_t current_track;
    uint8_t current_head;
    bool motor_on;
    
    /* Thread safety */
    pthread_mutex_t mutex;
    bool mutex_initialized;
    
    /* Telemetry */
    uft_telemetry_t* telemetry;
};

/* ========================================================================
 * PROVIDER REGISTRY
 * ======================================================================== */

/**
 * Initialize provider registry
 */
static void init_provider_registry(void) {
    if (!g_provider_registry.initialized) {
        pthread_mutex_init(&g_provider_registry.mutex, NULL);
        g_provider_registry.count = 0;
        g_provider_registry.initialized = true;
        
        UFT_LOG_DEBUG("Provider registry initialized");
    }
}

/**
 * Register a provider
 */
uft_rc_t uft_drive_register_provider(const uft_drive_ops_t* ops) {
    if (!ops) {
        UFT_RETURN_ERROR(UFT_ERR_INVALID_ARG, "ops is NULL");
    }
    
    if (!ops->name || !ops->open || !ops->close) {
        UFT_RETURN_ERROR(UFT_ERR_INVALID_ARG, 
                        "Provider missing required operations");
    }
    
    init_provider_registry();
    
    pthread_mutex_lock(&g_provider_registry.mutex);
    
    /* Check if already registered */
    for (uint32_t i = 0; i < g_provider_registry.count; i++) {
        if (strcmp(g_provider_registry.providers[i]->name, ops->name) == 0) {
            pthread_mutex_unlock(&g_provider_registry.mutex);
            UFT_LOG_WARN("Provider '%s' already registered", ops->name);
            return UFT_SUCCESS;
        }
    }
    
    /* Register new provider */
    if (g_provider_registry.count >= MAX_PROVIDERS) {
        pthread_mutex_unlock(&g_provider_registry.mutex);
        UFT_RETURN_ERROR(UFT_ERR_INTERNAL, 
                        "Too many providers (%u)", MAX_PROVIDERS);
    }
    
    g_provider_registry.providers[g_provider_registry.count++] = ops;
    
    pthread_mutex_unlock(&g_provider_registry.mutex);
    
    UFT_LOG_INFO("Provider registered: %s", ops->name);
    
    return UFT_SUCCESS;
}

/**
 * Find provider by name
 */
static const uft_drive_ops_t* find_provider(const char* name) {
    init_provider_registry();
    
    pthread_mutex_lock(&g_provider_registry.mutex);
    
    for (uint32_t i = 0; i < g_provider_registry.count; i++) {
        if (strcmp(g_provider_registry.providers[i]->name, name) == 0) {
            const uft_drive_ops_t* ops = g_provider_registry.providers[i];
            pthread_mutex_unlock(&g_provider_registry.mutex);
            return ops;
        }
    }
    
    pthread_mutex_unlock(&g_provider_registry.mutex);
    
    return NULL;
}

/* ========================================================================
 * SAMPLE RATE NORMALIZATION
 * ======================================================================== */

/**
 * Normalize flux stream to nanoseconds
 * 
 * This is THE CORE of hardware abstraction:
 * - Greaseweazle: 72MHz ticks → nanoseconds
 * - SuperCard Pro: 40MHz ticks → nanoseconds
 * - KryoFlux: variable → nanoseconds
 * - Mock: already nanoseconds
 */
uft_rc_t uft_drive_normalize_flux(
    const uint32_t* raw_ticks,
    uint32_t count,
    uint32_t sample_rate_hz,
    uint32_t** flux_ns,
    uint32_t* out_count
) {
    if (!raw_ticks || !flux_ns || !out_count) {
        UFT_RETURN_ERROR(UFT_ERR_INVALID_ARG, 
                        "Invalid parameters: ticks=%p, flux=%p, count=%p",
                        raw_ticks, flux_ns, out_count);
    }
    
    if (sample_rate_hz == 0) {
        UFT_RETURN_ERROR(UFT_ERR_INVALID_ARG, 
                        "Sample rate cannot be 0");
    }
    
    UFT_LOG_DEBUG("Normalizing %u transitions from %u Hz to nanoseconds",
                  count, sample_rate_hz);
    
    /* Allocate output buffer */
    uint32_t* normalized = malloc(count * sizeof(uint32_t));
    if (!normalized) {
        UFT_RETURN_ERROR(UFT_ERR_MEMORY, 
                        "Failed to allocate normalized buffer");
    }
    
    /* Convert: ticks × (1,000,000,000 / sample_rate_hz) = nanoseconds */
    for (uint32_t i = 0; i < count; i++) {
        uint64_t ns = ((uint64_t)raw_ticks[i] * 1000000000ULL) / sample_rate_hz;
        normalized[i] = (uint32_t)ns;
    }
    
    *flux_ns = normalized;
    *out_count = count;
    
    UFT_LOG_DEBUG("Normalization complete: %u → %u transitions", 
                  count, *out_count);
    
    return UFT_SUCCESS;
}

/* ========================================================================
 * DRIVE OPERATIONS
 * ======================================================================== */

/**
 * Create universal drive
 */
uft_rc_t uft_drive_create(
    const char* provider_name,
    const char* device_path,
    uft_universal_drive_t** drive
) {
    /* INPUT VALIDATION */
    if (!provider_name || !device_path || !drive) {
        UFT_RETURN_ERROR(UFT_ERR_INVALID_ARG,
                        "Invalid parameters: provider=%p, device=%p, drive=%p",
                        provider_name, device_path, drive);
    }
    
    *drive = NULL;
    
    UFT_LOG_INFO("Creating universal drive: provider='%s', device='%s'",
                 provider_name, device_path);
    UFT_TIME_START(t_create);
    
    /* Find provider */
    const uft_drive_ops_t* ops = find_provider(provider_name);
    if (!ops) {
        UFT_RETURN_ERROR(UFT_ERR_NOT_FOUND,
                        "Provider '%s' not found (not registered?)",
                        provider_name);
    }
    
    /* Allocate drive handle */
    uft_auto_cleanup(cleanup_free) uft_universal_drive_t* drv = 
        calloc(1, sizeof(uft_universal_drive_t));
    
    if (!drv) {
        UFT_RETURN_ERROR(UFT_ERR_MEMORY,
                        "Failed to allocate drive handle (%zu bytes)",
                        sizeof(uft_universal_drive_t));
    }
    
    /* Initialize mutex */
    if (pthread_mutex_init(&drv->mutex, NULL) != 0) {
        UFT_RETURN_ERROR(UFT_ERR_INTERNAL, "Failed to initialize mutex");
    }
    drv->mutex_initialized = true;
    
    /* Store configuration */
    drv->ops = ops;
    strncpy(drv->provider_name, provider_name, sizeof(drv->provider_name) - 1);
    strncpy(drv->device_path, device_path, sizeof(drv->device_path) - 1);
    
    /* Open provider */
    uft_rc_t rc = ops->open(device_path, &drv->provider_context);
    if (uft_failed(rc)) {
        UFT_CHAIN_ERROR(UFT_ERR_IO, rc,
                       "Failed to open provider '%s' on '%s'",
                       provider_name, device_path);
        return rc;
    }
    
    /* Query capabilities */
    if (ops->get_capabilities) {
        ops->get_capabilities(drv->provider_context, &drv->capabilities);
        
        UFT_LOG_INFO("Provider capabilities: flux_read=%d, flux_write=%d, index=%d",
                     drv->capabilities.can_read_flux,
                     drv->capabilities.can_write_flux,
                     drv->capabilities.has_index_pulse);
    }
    
    /* Create telemetry */
    drv->telemetry = uft_telemetry_create();
    
    /* Success! */
    *drive = drv;
    drv = NULL;  /* Prevent cleanup */
    
    UFT_TIME_LOG(t_create, "Universal drive created in %.2f ms");
    
    return UFT_SUCCESS;
}

/**
 * Destroy universal drive
 */
void uft_drive_destroy(uft_universal_drive_t** drive) {
    if (!drive || !*drive) {
        return;
    }
    
    uft_universal_drive_t* drv = *drive;
    
    UFT_LOG_DEBUG("Destroying universal drive: %s", drv->provider_name);
    
    /* Log telemetry */
    if (drv->telemetry) {
        uft_telemetry_log(drv->telemetry);
        uft_telemetry_destroy(&drv->telemetry);
    }
    
    /* Close provider */
    if (drv->ops && drv->ops->close) {
        drv->ops->close(&drv->provider_context);
    }
    
    /* Destroy mutex */
    if (drv->mutex_initialized) {
        pthread_mutex_destroy(&drv->mutex);
    }
    
    /* Free handle */
    free(drv);
    *drive = NULL;
    
    UFT_LOG_DEBUG("Universal drive destroyed");
}

/**
 * Read flux from drive
 */
uft_rc_t uft_drive_read_flux(
    uft_universal_drive_t* drive,
    uft_flux_stream_t** flux
) {
    if (!drive || !flux) {
        UFT_RETURN_ERROR(UFT_ERR_INVALID_ARG,
                        "Invalid parameters: drive=%p, flux=%p",
                        drive, flux);
    }
    
    if (!drive->capabilities.can_read_flux) {
        UFT_RETURN_ERROR(UFT_ERR_NOT_SUPPORTED,
                        "Provider '%s' does not support flux reading",
                        drive->provider_name);
    }
    
    pthread_mutex_lock(&drive->mutex);
    
    UFT_LOG_DEBUG("Reading flux: track=%d, head=%d",
                  drive->current_track, drive->current_head);
    UFT_TIME_START(t_read);
    
    /* Call provider */
    uft_rc_t rc = drive->ops->read_flux(drive->provider_context, flux);
    
    if (uft_success(rc) && drive->telemetry) {
        uft_telemetry_update(drive->telemetry, "flux_transitions", 
                            (*flux)->count);
    }
    
    pthread_mutex_unlock(&drive->mutex);
    
    if (uft_success(rc)) {
        UFT_TIME_LOG(t_read, "Flux read in %.2f ms (%u transitions)",
                     (*flux)->count);
    }
    
    return rc;
}

/**
 * Seek to track
 */
uft_rc_t uft_drive_seek(
    uft_universal_drive_t* drive,
    uint8_t track,
    uint8_t head
) {
    if (!drive) {
        UFT_RETURN_ERROR(UFT_ERR_INVALID_ARG, "drive is NULL");
    }
    
    if (!drive->ops->seek) {
        UFT_RETURN_ERROR(UFT_ERR_NOT_SUPPORTED,
                        "Provider '%s' does not support seeking",
                        drive->provider_name);
    }
    
    pthread_mutex_lock(&drive->mutex);
    
    UFT_LOG_INFO("Seeking to track %d, head %d", track, head);
    
    uft_rc_t rc = drive->ops->seek(drive->provider_context, track, head);
    
    if (uft_success(rc)) {
        drive->current_track = track;
        drive->current_head = head;
    }
    
    pthread_mutex_unlock(&drive->mutex);
    
    return rc;
}

/**
 * Motor control
 */
uft_rc_t uft_drive_motor(uft_universal_drive_t* drive, bool on) {
    if (!drive) {
        UFT_RETURN_ERROR(UFT_ERR_INVALID_ARG, "drive is NULL");
    }
    
    if (!drive->ops->motor) {
        /* Not all providers support motor control - not an error */
        UFT_LOG_DEBUG("Provider '%s' does not support motor control",
                     drive->provider_name);
        return UFT_SUCCESS;
    }
    
    pthread_mutex_lock(&drive->mutex);
    
    UFT_LOG_DEBUG("Motor: %s", on ? "ON" : "OFF");
    
    uft_rc_t rc = drive->ops->motor(drive->provider_context, on);
    
    if (uft_success(rc)) {
        drive->motor_on = on;
    }
    
    pthread_mutex_unlock(&drive->mutex);
    
    return rc;
}

/**
 * Get capabilities
 */
bool uft_drive_has_capability(
    const uft_universal_drive_t* drive,
    uft_drive_capability_flag_t cap
) {
    if (!drive) {
        return false;
    }
    
    return (drive->capabilities.flags & cap) != 0;
}

/**
 * Get info
 */
uft_rc_t uft_drive_get_info(
    const uft_universal_drive_t* drive,
    uft_drive_info_t* info
) {
    if (!drive || !info) {
        UFT_RETURN_ERROR(UFT_ERR_INVALID_ARG, "Invalid arguments");
    }
    
    memset(info, 0, sizeof(*info));
    
    strncpy(info->provider_name, drive->provider_name, 
            sizeof(info->provider_name) - 1);
    strncpy(info->device_path, drive->device_path,
            sizeof(info->device_path) - 1);
    
    info->current_track = drive->current_track;
    info->current_head = drive->current_head;
    info->motor_on = drive->motor_on;
    
    memcpy(&info->capabilities, &drive->capabilities,
           sizeof(uft_drive_capabilities_t));
    
    return UFT_SUCCESS;
}
