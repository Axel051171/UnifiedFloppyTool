/**
 * @file uft_drive_provider_mock.c
 * @brief Mock Provider for IUniversalDrive (Testing)
 * 
 * TESTING PROVIDER
 * 
 * Features:
 * ✅ Deterministic synthetic flux data
 * ✅ No hardware required
 * ✅ Perfect for unit testing
 * ✅ Configurable behavior
 * 
 * @version 3.0.0
 * @date 2024-12-27
 */

#include "uft_iuniversaldrive.h"
#include "uft_error_handling.h"
#include "uft_logging.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Mock sample rate (already nanoseconds!) */
#define MOCK_SAMPLE_RATE_HZ 1000000000  /* 1 GHz = 1ns resolution */

/* Mock context */
typedef struct {
    char config[256];
    
    uint8_t current_track;
    uint8_t current_head;
    bool motor_on;
    
    /* Configuration */
    uint32_t flux_per_track;
    uint32_t cell_time_ns;
    bool add_jitter;
    
    /* Statistics */
    uint64_t flux_transitions_read;
    uint32_t read_operations;
    
} mock_context_t;

/* ========================================================================
 * SYNTHETIC FLUX GENERATION
 * ======================================================================== */

/**
 * Generate synthetic MFM flux data
 */
static void generate_synthetic_flux(
    uint32_t* flux_ns,
    uint32_t count,
    uint32_t cell_time_ns,
    bool add_jitter
) {
    /* Generate deterministic MFM pattern */
    for (uint32_t i = 0; i < count; i++) {
        /* Alternate between 2x and 3x cell times (typical MFM) */
        uint32_t cells = (i % 3 == 0) ? 3 : 2;
        uint32_t time = cells * cell_time_ns;
        
        /* Add jitter if requested */
        if (add_jitter) {
            int32_t jitter = (int32_t)(sin(i * 0.1) * 50);  /* ±50ns */
            time += jitter;
        }
        
        flux_ns[i] = time;
    }
}

/* ========================================================================
 * PROVIDER OPERATIONS
 * ======================================================================== */

/**
 * Open Mock device
 */
static uft_rc_t mock_open(const char* device_path, void** context) {
    if (!device_path || !context) {
        UFT_RETURN_ERROR(UFT_ERR_INVALID_ARG, "Invalid arguments");
    }
    
    UFT_LOG_INFO("Opening Mock device: %s", device_path);
    
    /* Allocate context */
    mock_context_t* ctx = calloc(1, sizeof(mock_context_t));
    if (!ctx) {
        UFT_RETURN_ERROR(UFT_ERR_MEMORY, "Failed to allocate Mock context");
    }
    
    strncpy(ctx->config, device_path, sizeof(ctx->config) - 1);
    
    /* Default configuration */
    ctx->flux_per_track = 100000;
    ctx->cell_time_ns = 2000;  /* MFM DD */
    ctx->add_jitter = false;
    
    *context = ctx;
    
    UFT_LOG_INFO("Mock device opened (synthetic flux generator)");
    
    return UFT_SUCCESS;
}

/**
 * Close Mock device
 */
static void mock_close(void** context) {
    if (!context || !*context) {
        return;
    }
    
    mock_context_t* ctx = (mock_context_t*)*context;
    
    UFT_LOG_INFO("Mock stats: %llu flux read, %u operations",
                 (unsigned long long)ctx->flux_transitions_read,
                 ctx->read_operations);
    
    free(ctx);
    *context = NULL;
    
    UFT_LOG_DEBUG("Mock device closed");
}

/**
 * Read flux from Mock device
 */
static uft_rc_t mock_read_flux(void* context, uft_flux_stream_t** flux) {
    if (!context || !flux) {
        UFT_RETURN_ERROR(UFT_ERR_INVALID_ARG, "Invalid arguments");
    }
    
    mock_context_t* ctx = (mock_context_t*)context;
    
    UFT_LOG_DEBUG("Reading synthetic flux from Mock (track %d, head %d)",
                  ctx->current_track, ctx->current_head);
    
    /* Allocate flux stream */
    uft_flux_stream_t* stream = calloc(1, sizeof(uft_flux_stream_t));
    if (!stream) {
        UFT_RETURN_ERROR(UFT_ERR_MEMORY, "Failed to allocate flux stream");
    }
    
    /* Generate synthetic flux data */
    uint32_t count = ctx->flux_per_track;
    uint32_t* flux_ns = malloc(count * sizeof(uint32_t));
    
    if (!flux_ns) {
        free(stream);
        UFT_RETURN_ERROR(UFT_ERR_MEMORY, "Failed to allocate flux buffer");
    }
    
    generate_synthetic_flux(flux_ns, count, ctx->cell_time_ns, ctx->add_jitter);
    
    /* Fill stream (ALREADY in nanoseconds!) */
    stream->transitions_ns = flux_ns;
    stream->count = count;
    stream->index_offset = 0;
    stream->has_index = true;
    
    /* Update stats */
    ctx->flux_transitions_read += count;
    ctx->read_operations++;
    
    *flux = stream;
    
    UFT_LOG_DEBUG("Mock flux generated: %u transitions (synthetic MFM)",
                  count);
    
    return UFT_SUCCESS;
}

/**
 * Seek to track
 */
static uft_rc_t mock_seek(void* context, uint8_t track, uint8_t head) {
    if (!context) {
        UFT_RETURN_ERROR(UFT_ERR_INVALID_ARG, "context is NULL");
    }
    
    mock_context_t* ctx = (mock_context_t*)context;
    
    UFT_LOG_DEBUG("Mock seeking to track %d, head %d", track, head);
    
    ctx->current_track = track;
    ctx->current_head = head;
    
    return UFT_SUCCESS;
}

/**
 * Motor control
 */
static uft_rc_t mock_motor(void* context, bool on) {
    if (!context) {
        UFT_RETURN_ERROR(UFT_ERR_INVALID_ARG, "context is NULL");
    }
    
    mock_context_t* ctx = (mock_context_t*)context;
    
    UFT_LOG_DEBUG("Mock motor: %s", on ? "ON" : "OFF");
    
    ctx->motor_on = on;
    
    return UFT_SUCCESS;
}

/**
 * Get capabilities
 */
static void mock_get_capabilities(
    void* context,
    uft_drive_capabilities_t* caps
) {
    (void)context;
    
    if (!caps) return;
    
    memset(caps, 0, sizeof(*caps));
    
    /* Mock capabilities (perfect!) */
    caps->can_read_flux = true;
    caps->can_write_flux = true;
    caps->has_index_pulse = true;
    caps->can_step = true;
    caps->has_motor_control = true;
    caps->can_detect_disk = true;
    caps->can_detect_write_protect = true;
    
    caps->min_track = 0;
    caps->max_track = 83;
    caps->heads = 2;
    
    caps->sample_rate_hz = MOCK_SAMPLE_RATE_HZ;
    
    strncpy(caps->hardware_name, "Mock Device (Testing)",
            sizeof(caps->hardware_name) - 1);
    strncpy(caps->firmware_version, "SYNTHETIC",
            sizeof(caps->firmware_version) - 1);
}

/* ========================================================================
 * PROVIDER REGISTRATION
 * ======================================================================== */

static const uft_drive_ops_t mock_ops = {
    .name = "mock",
    .open = mock_open,
    .close = mock_close,
    .read_flux = mock_read_flux,
    .write_flux = NULL,
    .seek = mock_seek,
    .step = NULL,
    .motor = mock_motor,
    .erase_track = NULL,
    .get_capabilities = mock_get_capabilities,
};

/**
 * Register Mock provider
 */
uft_rc_t uft_drive_register_mock(void) {
    return uft_drive_register_provider(&mock_ops);
}
