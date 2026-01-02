/**
 * @file uft_pipeline.c
 * @brief Pipeline Implementation
 * 
 * GOD MODE: Stub implementation for API testing
 */

#include "uft/pipeline/uft_pipeline.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// INTERNAL STRUCTURE
// ============================================================================

struct uft_pipeline_s {
    uft_pipeline_config_t config;
    uft_pipeline_stage_t stage;
    bool cancelled;
    int error_code;
    char error_msg[256];
};

// ============================================================================
// STAGE NAMES
// ============================================================================

static const char* stage_names[] = {
    "IDLE",
    "ACQUIRE",
    "DECODE",
    "NORMALIZE",
    "EXPORT",
    "VERIFY",
    "COMPLETE",
    "ERROR"
};

const char* uft_pipeline_stage_name(uft_pipeline_stage_t stage) {
    if (stage < 0 || stage > UFT_STAGE_ERROR) return "UNKNOWN";
    return stage_names[stage];
}

// ============================================================================
// LIFECYCLE
// ============================================================================

uft_pipeline_t* uft_pipeline_create(void) {
    uft_pipeline_t* pipe = calloc(1, sizeof(uft_pipeline_t));
    if (!pipe) return NULL;
    
    pipe->stage = UFT_STAGE_IDLE;
    uft_pipeline_config_default(&pipe->config);
    
    return pipe;
}

void uft_pipeline_destroy(uft_pipeline_t* pipe) {
    if (pipe) {
        free(pipe);
    }
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void uft_pipeline_config_default(uft_pipeline_config_t* config) {
    if (!config) return;
    
    memset(config, 0, sizeof(*config));
    
    // Acquisition
    config->revolutions = 3;
    config->start_cylinder = 0;
    config->end_cylinder = 79;
    config->heads = 2;
    
    // Decode
    config->encoding = 0;  // Auto
    config->pll_bandwidth = 0.05;
    config->max_retries = 3;
    
    // Normalize
    config->fill_missing = false;
    config->fill_byte = 0x00;
    config->attempt_recovery = true;
    
    // Export
    config->target_format = 0;
    config->output_path = NULL;
    config->compress = false;
    
    // Verify
    config->verify_export = true;
    config->hash_sectors = true;
    
    // Progress
    config->progress_cb = NULL;
    config->progress_user = NULL;
}

int uft_pipeline_configure(uft_pipeline_t* pipe, const uft_pipeline_config_t* config) {
    if (!pipe || !config) return -1;
    memcpy(&pipe->config, config, sizeof(uft_pipeline_config_t));
    return 0;
}

uft_pipeline_stage_t uft_pipeline_get_stage(const uft_pipeline_t* pipe) {
    if (!pipe) return UFT_STAGE_ERROR;
    return pipe->stage;
}

void uft_pipeline_cancel(uft_pipeline_t* pipe) {
    if (pipe) pipe->cancelled = true;
}

// ============================================================================
// CLEANUP FUNCTIONS
// ============================================================================

void uft_flux_free(uft_flux_t* flux) {
    if (!flux) return;
    free(flux->flux_data);
    flux->flux_data = NULL;
    free(flux->index_times);
    flux->index_times = NULL;
    flux->flux_count = 0;
    flux->index_count = 0;
}

void uft_bitstream_free(uft_bitstream_t* bits) {
    if (!bits) return;
    free(bits->bits);
    bits->bits = NULL;
    free(bits->weak_mask);
    bits->weak_mask = NULL;
    free(bits->timing);
    bits->timing = NULL;
    bits->bit_count = 0;
}

void uft_track_free(uft_track_t* track) {
    if (!track) return;
    if (track->sectors) {
        for (int i = 0; i < track->sector_count; i++) {
            free(track->sectors[i].data);
        }
        free(track->sectors);
    }
    free(track->raw_data);
    memset(track, 0, sizeof(*track));
}

void uft_image_free(uft_image_t* image) {
    if (!image) return;
    if (image->tracks) {
        for (int i = 0; i < image->track_count; i++) {
            uft_track_free(&image->tracks[i]);
        }
        free(image->tracks);
    }
    memset(image, 0, sizeof(*image));
}
