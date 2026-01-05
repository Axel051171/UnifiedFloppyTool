// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file uft_dd_v2.h
 * @brief GOD MODE DD Module API
 * @version 2.0.0-GOD
 * @date 2025-01-02
 *
 * High-performance disk copy with:
 * - SIMD-optimized operations
 * - Adaptive block sizing
 * - Bad sector mapping
 * - Resume/checkpoint support
 */

#ifndef UFT_DD_V2_H
#define UFT_DD_V2_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * TYPES
 *============================================================================*/

typedef struct dd_state_v2 dd_state_v2_t;

typedef struct {
    /* Base stats */
    uint64_t bytes_read;
    uint64_t bytes_written;
    uint64_t errors_read;
    uint64_t errors_write;
    
    /* Extended stats */
    uint64_t bad_sectors;
    uint64_t recovered_sectors;
    uint64_t skipped_sectors;
    
    /* Performance (MB/s) */
    double read_speed_mbps;
    double write_speed_mbps;
    double hash_speed_mbps;
    
    /* Adaptive info */
    size_t current_block_size;
    double current_error_rate;
    
    /* Progress */
    double percent_complete;
    double eta_seconds;
    double elapsed_seconds;
    
    /* Hashes */
    char md5[33];
    char sha256[65];
    
    /* State */
    bool is_running;
    bool is_paused;
    bool can_resume;
} dd_status_v2_t;

/*============================================================================
 * API FUNCTIONS
 *============================================================================*/

/**
 * @brief Create DD v2 instance
 */
dd_state_v2_t* dd_v2_create(void);

/**
 * @brief Destroy DD v2 instance
 */
void dd_v2_destroy(dd_state_v2_t* state);

/**
 * @brief Set source file path
 */
void dd_v2_set_source(dd_state_v2_t* state, const char* path);

/**
 * @brief Set destination file path
 */
void dd_v2_set_dest(dd_state_v2_t* state, const char* path);

/**
 * @brief Set checkpoint file for resume support
 */
void dd_v2_set_checkpoint(dd_state_v2_t* state, const char* path);

/**
 * @brief Set progress callback
 */
void dd_v2_set_progress_callback(dd_state_v2_t* state,
                                  void (*cb)(const dd_status_v2_t*, void*),
                                  void* user_data);

/**
 * @brief Start copy operation
 * @return 0 on success, 1 if cancelled, -1 on error
 */
int dd_v2_start(dd_state_v2_t* state);

/**
 * @brief Pause operation
 */
void dd_v2_pause(dd_state_v2_t* state);

/**
 * @brief Resume paused operation
 */
void dd_v2_resume(dd_state_v2_t* state);

/**
 * @brief Cancel operation
 */
void dd_v2_cancel(dd_state_v2_t* state);

/**
 * @brief Get current status
 */
void dd_v2_get_status(dd_state_v2_t* state, dd_status_v2_t* status);

/**
 * @brief Export bad sector map to file
 */
int dd_v2_export_bad_sectors(dd_state_v2_t* state, const char* path);

/*============================================================================
 * GUI PARAMETER CONSTRAINTS
 *============================================================================*/

/* Block sizes */
#define DD_V2_BLOCK_MIN         512
#define DD_V2_BLOCK_MAX         4194304     /* 4M */
#define DD_V2_BLOCK_DEFAULT     131072      /* 128K */

/* Error handling */
#define DD_V2_RETRY_MIN         0
#define DD_V2_RETRY_MAX         20
#define DD_V2_RETRY_DEFAULT     3

#ifdef __cplusplus
}
#endif

#endif /* UFT_DD_V2_H */
