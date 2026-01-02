/**
 * @file uft_job_manager.c
 * @brief Job Manager - Asynchrone Operations mit Progress/Cancel
 * 
 * LAYER SEPARATION:
 * - GUI friert nie ein (Worker-Threads)
 * - Progress-Updates via Callbacks
 * - Cancel jederzeit möglich
 */

#include "uft/uft_job_manager.h"
#include "uft/uft_safe.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

// ============================================================================
// Constants
// ============================================================================

#define MAX_JOBS            16
#define JOB_QUEUE_SIZE      32

// ============================================================================
// Job Structure
// ============================================================================

typedef enum {
    JOB_STATE_PENDING,
    JOB_STATE_RUNNING,
    JOB_STATE_COMPLETED,
    JOB_STATE_CANCELLED,
    JOB_STATE_FAILED
} job_state_t;

struct uft_job {
    uint32_t            id;
    uft_job_type_t      type;
    job_state_t         state;
    
    // Progress
    int                 progress_percent;
    char                progress_message[256];
    
    // Result
    uft_error_t         result;
    void*               result_data;
    
    // Cancel
    volatile bool       cancel_requested;
    
    // Callback
    uft_job_callback_t  callback;
    void*               callback_user;
    
    // Parameters
    void*               params;
    size_t              params_size;
    
    // Threading
    pthread_t           thread;
    bool                thread_started;
};

// ============================================================================
// Job Manager Structure
// ============================================================================

struct uft_job_manager {
    uft_job_t*          jobs[MAX_JOBS];
    size_t              job_count;
    uint32_t            next_job_id;
    
    pthread_mutex_t     lock;
    bool                initialized;
    
    // Worker pool
    int                 max_workers;
    int                 active_workers;
};

// ============================================================================
// Lifecycle
// ============================================================================

uft_job_manager_t* uft_job_manager_create(int max_workers) {
    uft_job_manager_t* mgr = calloc(1, sizeof(uft_job_manager_t));
    if (!mgr) return NULL;
    
    pthread_mutex_init(&mgr->lock, NULL);
    mgr->max_workers = max_workers > 0 ? max_workers : 4;
    mgr->next_job_id = 1;
    mgr->initialized = true;
    
    return mgr;
}

void uft_job_manager_destroy(uft_job_manager_t* mgr) {
    if (!mgr) return;
    
    // Cancel all pending jobs
    pthread_mutex_lock(&mgr->lock);
    for (size_t i = 0; i < mgr->job_count; i++) {
        if (mgr->jobs[i]) {
            mgr->jobs[i]->cancel_requested = true;
            if (mgr->jobs[i]->thread_started) {
                pthread_join(mgr->jobs[i]->thread, NULL);
            }
            free(mgr->jobs[i]->params);
            free(mgr->jobs[i]);
        }
    }
    pthread_mutex_unlock(&mgr->lock);
    
    pthread_mutex_destroy(&mgr->lock);
    free(mgr);
}

// ============================================================================
// Job Creation
// ============================================================================

static uft_job_t* create_job(uft_job_manager_t* mgr, uft_job_type_t type) {
    uft_job_t* job = calloc(1, sizeof(uft_job_t));
    if (!job) return NULL;
    
    job->id = mgr->next_job_id++;
    job->type = type;
    job->state = JOB_STATE_PENDING;
    job->result = UFT_OK;
    
    return job;
}

// ============================================================================
// Worker Thread Functions
// ============================================================================

typedef struct {
    uft_job_manager_t*  mgr;
    uft_job_t*          job;
} worker_context_t;

// Read disk worker
static void* read_disk_worker(void* arg) {
    worker_context_t* ctx = (worker_context_t*)arg;
    uft_job_t* job = ctx->job;
    uft_job_manager_t* mgr = ctx->mgr;
    
    pthread_mutex_lock(&mgr->lock);
    job->state = JOB_STATE_RUNNING;
    mgr->active_workers++;
    pthread_mutex_unlock(&mgr->lock);
    
    // Simulate work with progress
    for (int i = 0; i <= 100 && !job->cancel_requested; i += 5) {
        job->progress_percent = i;
        snprintf(job->progress_message, sizeof(job->progress_message),
                 "Reading track %d...", i / 5);
        
        if (job->callback) {
            uft_job_status_t status = {
                .job_id = job->id,
                .type = job->type,
                .state = UFT_JOB_STATE_RUNNING,
                .progress_percent = i,
                .progress_message = job->progress_message
            };
            job->callback(job->callback_user, &status);
        }
        
        usleep(50000);  // 50ms
    }
    
    pthread_mutex_lock(&mgr->lock);
    
    if (job->cancel_requested) {
        job->state = JOB_STATE_CANCELLED;
        job->result = UFT_ERROR_CANCELLED;
    } else {
        job->state = JOB_STATE_COMPLETED;
        job->result = UFT_OK;
    }
    
    mgr->active_workers--;
    pthread_mutex_unlock(&mgr->lock);
    
    // Final callback
    if (job->callback) {
        uft_job_status_t status = {
            .job_id = job->id,
            .type = job->type,
            .state = job->state == JOB_STATE_COMPLETED ? 
                     UFT_JOB_STATE_COMPLETED : UFT_JOB_STATE_CANCELLED,
            .progress_percent = 100,
            .result = job->result
        };
        job->callback(job->callback_user, &status);
    }
    
    free(ctx);
    return NULL;
}

// Write disk worker
static void* write_disk_worker(void* arg) {
    worker_context_t* ctx = (worker_context_t*)arg;
    uft_job_t* job = ctx->job;
    uft_job_manager_t* mgr = ctx->mgr;
    
    pthread_mutex_lock(&mgr->lock);
    job->state = JOB_STATE_RUNNING;
    mgr->active_workers++;
    pthread_mutex_unlock(&mgr->lock);
    
    // Similar to read...
    for (int i = 0; i <= 100 && !job->cancel_requested; i += 5) {
        job->progress_percent = i;
        snprintf(job->progress_message, sizeof(job->progress_message),
                 "Writing track %d...", i / 5);
        
        if (job->callback) {
            uft_job_status_t status = {
                .job_id = job->id,
                .type = job->type,
                .state = UFT_JOB_STATE_RUNNING,
                .progress_percent = i,
                .progress_message = job->progress_message
            };
            job->callback(job->callback_user, &status);
        }
        
        usleep(50000);
    }
    
    pthread_mutex_lock(&mgr->lock);
    job->state = job->cancel_requested ? JOB_STATE_CANCELLED : JOB_STATE_COMPLETED;
    job->result = job->cancel_requested ? UFT_ERROR_CANCELLED : UFT_OK;
    mgr->active_workers--;
    pthread_mutex_unlock(&mgr->lock);
    
    // Final callback
    if (job->callback) {
        uft_job_status_t status = {
            .job_id = job->id,
            .type = job->type,
            .state = job->state == JOB_STATE_COMPLETED ? 
                     UFT_JOB_STATE_COMPLETED : UFT_JOB_STATE_CANCELLED,
            .progress_percent = 100,
            .result = job->result
        };
        job->callback(job->callback_user, &status);
    }
    
    free(ctx);
    return NULL;
}

// ============================================================================
// Job Submission
// ============================================================================

uft_error_t uft_job_submit_read(uft_job_manager_t* mgr,
                                 const uft_read_job_params_t* params,
                                 uft_job_callback_t callback,
                                 void* user_data,
                                 uint32_t* job_id) {
    if (!mgr || !params || !job_id) return UFT_ERROR_NULL_POINTER;
    
    pthread_mutex_lock(&mgr->lock);
    
    if (mgr->job_count >= MAX_JOBS) {
        pthread_mutex_unlock(&mgr->lock);
        return UFT_ERROR_NO_SPACE;
    }
    
    uft_job_t* job = create_job(mgr, UFT_JOB_READ_DISK);
    if (!job) {
        pthread_mutex_unlock(&mgr->lock);
        return UFT_ERROR_NO_MEMORY;
    }
    
    job->callback = callback;
    job->callback_user = user_data;
    
    // Copy params
    job->params = malloc(sizeof(*params));
    if (job->params) {
        memcpy(job->params, params, sizeof(*params));
        job->params_size = sizeof(*params);
    }
    
    mgr->jobs[mgr->job_count++] = job;
    *job_id = job->id;
    
    pthread_mutex_unlock(&mgr->lock);
    
    // Start worker
    worker_context_t* ctx = malloc(sizeof(*ctx));
    if (ctx) {
        ctx->mgr = mgr;
        ctx->job = job;
        job->thread_started = true;
        pthread_create(&job->thread, NULL, read_disk_worker, ctx);
    }
    
    return UFT_OK;
}

uft_error_t uft_job_submit_write(uft_job_manager_t* mgr,
                                  const uft_write_job_params_t* params,
                                  uft_job_callback_t callback,
                                  void* user_data,
                                  uint32_t* job_id) {
    if (!mgr || !params || !job_id) return UFT_ERROR_NULL_POINTER;
    
    pthread_mutex_lock(&mgr->lock);
    
    if (mgr->job_count >= MAX_JOBS) {
        pthread_mutex_unlock(&mgr->lock);
        return UFT_ERROR_NO_SPACE;
    }
    
    uft_job_t* job = create_job(mgr, UFT_JOB_WRITE_DISK);
    if (!job) {
        pthread_mutex_unlock(&mgr->lock);
        return UFT_ERROR_NO_MEMORY;
    }
    
    job->callback = callback;
    job->callback_user = user_data;
    
    mgr->jobs[mgr->job_count++] = job;
    *job_id = job->id;
    
    pthread_mutex_unlock(&mgr->lock);
    
    // Start worker
    worker_context_t* ctx = malloc(sizeof(*ctx));
    if (ctx) {
        ctx->mgr = mgr;
        ctx->job = job;
        job->thread_started = true;
        pthread_create(&job->thread, NULL, write_disk_worker, ctx);
    }
    
    return UFT_OK;
}

// ============================================================================
// Job Control
// ============================================================================

uft_error_t uft_job_cancel(uft_job_manager_t* mgr, uint32_t job_id) {
    if (!mgr) return UFT_ERROR_NULL_POINTER;
    
    pthread_mutex_lock(&mgr->lock);
    
    for (size_t i = 0; i < mgr->job_count; i++) {
        if (mgr->jobs[i] && mgr->jobs[i]->id == job_id) {
            mgr->jobs[i]->cancel_requested = true;
            pthread_mutex_unlock(&mgr->lock);
            return UFT_OK;
        }
    }
    
    pthread_mutex_unlock(&mgr->lock);
    return UFT_ERROR_NOT_FOUND;
}

void uft_job_cancel_all(uft_job_manager_t* mgr) {
    if (!mgr) return;
    
    pthread_mutex_lock(&mgr->lock);
    for (size_t i = 0; i < mgr->job_count; i++) {
        if (mgr->jobs[i]) {
            mgr->jobs[i]->cancel_requested = true;
        }
    }
    pthread_mutex_unlock(&mgr->lock);
}

// ============================================================================
// Job Query
// ============================================================================

uft_error_t uft_job_get_status(uft_job_manager_t* mgr,
                                uint32_t job_id,
                                uft_job_status_t* status) {
    if (!mgr || !status) return UFT_ERROR_NULL_POINTER;
    
    pthread_mutex_lock(&mgr->lock);
    
    for (size_t i = 0; i < mgr->job_count; i++) {
        if (mgr->jobs[i] && mgr->jobs[i]->id == job_id) {
            uft_job_t* job = mgr->jobs[i];
            
            status->job_id = job->id;
            status->type = job->type;
            status->progress_percent = job->progress_percent;
            status->progress_message = job->progress_message;
            status->result = job->result;
            
            switch (job->state) {
                case JOB_STATE_PENDING: status->state = UFT_JOB_STATE_PENDING; break;
                case JOB_STATE_RUNNING: status->state = UFT_JOB_STATE_RUNNING; break;
                case JOB_STATE_COMPLETED: status->state = UFT_JOB_STATE_COMPLETED; break;
                case JOB_STATE_CANCELLED: status->state = UFT_JOB_STATE_CANCELLED; break;
                case JOB_STATE_FAILED: status->state = UFT_JOB_STATE_FAILED; break;
            }
            
            pthread_mutex_unlock(&mgr->lock);
            return UFT_OK;
        }
    }
    
    pthread_mutex_unlock(&mgr->lock);
    return UFT_ERROR_NOT_FOUND;
}

bool uft_job_is_running(uft_job_manager_t* mgr, uint32_t job_id) {
    if (!mgr) return false;
    
    pthread_mutex_lock(&mgr->lock);
    
    for (size_t i = 0; i < mgr->job_count; i++) {
        if (mgr->jobs[i] && mgr->jobs[i]->id == job_id) {
            bool running = (mgr->jobs[i]->state == JOB_STATE_RUNNING);
            pthread_mutex_unlock(&mgr->lock);
            return running;
        }
    }
    
    pthread_mutex_unlock(&mgr->lock);
    return false;
}

int uft_job_get_active_count(uft_job_manager_t* mgr) {
    if (!mgr) return 0;
    return mgr->active_workers;
}
