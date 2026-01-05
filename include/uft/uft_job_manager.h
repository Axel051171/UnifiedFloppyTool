/**
 * @file uft_job_manager.h
 * @brief Job Manager - Async Operations for GUI
 * 
 * WICHTIG FÜR GUI:
 * - Alle langläufigen Operationen sind async
 * - Progress-Callbacks für UI-Updates
 * - Cancel jederzeit möglich
 * - UI friert nie ein
 */

#ifndef UFT_JOB_MANAGER_H
#define UFT_JOB_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "uft_error.h"
#include "uft_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Opaque Types
// ============================================================================

typedef struct uft_job_manager uft_job_manager_t;
typedef struct uft_job uft_job_t;

// ============================================================================
// Job Types
// ============================================================================

typedef enum uft_job_type {
    UFT_JOB_READ_DISK,
    UFT_JOB_WRITE_DISK,
    UFT_JOB_VERIFY_DISK,
    UFT_JOB_FORMAT_DISK,
    UFT_JOB_CONVERT_IMAGE,
    UFT_JOB_ANALYZE_IMAGE,
} uft_job_type_t;

typedef enum uft_job_state {
    UFT_JOB_STATE_PENDING,
    UFT_JOB_STATE_RUNNING,
    UFT_JOB_STATE_COMPLETED,
    UFT_JOB_STATE_CANCELLED,
    UFT_JOB_STATE_FAILED,
} uft_job_state_t;

// ============================================================================
// Job Status (für Callbacks)
// ============================================================================

typedef struct uft_job_status {
    uint32_t        job_id;
    uft_job_type_t  type;
    uft_job_state_t state;
    int             progress_percent;   // 0-100
    const char*     progress_message;
    uft_error_t     result;
} uft_job_status_t;

typedef void (*uft_job_callback_t)(void* user_data, const uft_job_status_t* status);

// ============================================================================
// Job Parameters
// ============================================================================

typedef struct uft_read_job_params {
    int         device_index;
    int         start_track;
    int         end_track;
    int         retries;
    const char* output_path;
    uft_format_t output_format;
} uft_read_job_params_t;

typedef struct uft_write_job_params {
    int         device_index;
    const char* input_path;
    bool        verify_after;
    bool        format_first;
} uft_write_job_params_t;

// ============================================================================
// Lifecycle
// ============================================================================

uft_job_manager_t* uft_job_manager_create(int max_workers);
void uft_job_manager_destroy(uft_job_manager_t* mgr);

// ============================================================================
// Job Submission
// ============================================================================

uft_error_t uft_job_submit_read(uft_job_manager_t* mgr,
                                 const uft_read_job_params_t* params,
                                 uft_job_callback_t callback,
                                 void* user_data,
                                 uint32_t* job_id);

uft_error_t uft_job_submit_write(uft_job_manager_t* mgr,
                                  const uft_write_job_params_t* params,
                                  uft_job_callback_t callback,
                                  void* user_data,
                                  uint32_t* job_id);

// ============================================================================
// Job Control
// ============================================================================

uft_error_t uft_job_cancel(uft_job_manager_t* mgr, uint32_t job_id);
void uft_job_cancel_all(uft_job_manager_t* mgr);

// ============================================================================
// Job Query
// ============================================================================

uft_error_t uft_job_get_status(uft_job_manager_t* mgr,
                                uint32_t job_id,
                                uft_job_status_t* status);

bool uft_job_is_running(uft_job_manager_t* mgr, uint32_t job_id);
int uft_job_get_active_count(uft_job_manager_t* mgr);

#ifdef __cplusplus
}
#endif

#endif // UFT_JOB_MANAGER_H
