/**
 * @file uft_batch.h
 * @brief UFT Batch Processing System
 * 
 * C-004: Automated batch processing for mass preservation
 * 
 * Features:
 * - Job queue management with priorities
 * - Progress reporting with callbacks
 * - Error handling and summary
 * - Resume after interruption
 * - Parallel processing support
 * - JSON/CSV report generation
 */

#ifndef UFT_BATCH_H
#define UFT_BATCH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Maximum values */
#define UFT_BATCH_MAX_JOBS          10000   /**< Maximum jobs in queue */
#define UFT_BATCH_MAX_WORKERS       16      /**< Maximum parallel workers */
#define UFT_BATCH_MAX_PATH          1024    /**< Maximum path length */
#define UFT_BATCH_MAX_ERRORS        1000    /**< Maximum stored errors */
#define UFT_BATCH_MAX_NAME          256     /**< Maximum name length */

/** Default values */
#define UFT_BATCH_DEFAULT_WORKERS   4       /**< Default worker count */
#define UFT_BATCH_DEFAULT_RETRIES   3       /**< Default retry count */
#define UFT_BATCH_DEFAULT_TIMEOUT   300     /**< Default timeout (seconds) */

/** State file magic */
#define UFT_BATCH_STATE_MAGIC       0x55465442  /**< 'UFTB' */
#define UFT_BATCH_STATE_VERSION     1

/*===========================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Job status
 */
typedef enum {
    UFT_JOB_PENDING = 0,        /**< Waiting to be processed */
    UFT_JOB_RUNNING,            /**< Currently processing */
    UFT_JOB_COMPLETED,          /**< Successfully completed */
    UFT_JOB_FAILED,             /**< Failed (will not retry) */
    UFT_JOB_RETRY,              /**< Failed, will retry */
    UFT_JOB_SKIPPED,            /**< Skipped (e.g., already exists) */
    UFT_JOB_CANCELLED           /**< Cancelled by user */
} uft_job_status_t;

/**
 * @brief Job type
 */
typedef enum {
    UFT_JOB_READ = 0,           /**< Read/analyze disk image */
    UFT_JOB_CONVERT,            /**< Convert between formats */
    UFT_JOB_VERIFY,             /**< Verify disk image */
    UFT_JOB_REPAIR,             /**< Attempt repair */
    UFT_JOB_EXTRACT,            /**< Extract files from image */
    UFT_JOB_COMPARE,            /**< Compare two images */
    UFT_JOB_HASH,               /**< Calculate hashes */
    UFT_JOB_REPORT,             /**< Generate report */
    UFT_JOB_CUSTOM              /**< Custom operation */
} uft_job_type_t;

/**
 * @brief Job priority
 */
typedef enum {
    UFT_PRIORITY_LOW = 0,
    UFT_PRIORITY_NORMAL,
    UFT_PRIORITY_HIGH,
    UFT_PRIORITY_CRITICAL
} uft_job_priority_t;

/**
 * @brief Batch status
 */
typedef enum {
    UFT_BATCH_IDLE = 0,         /**< Not started */
    UFT_BATCH_RUNNING,          /**< Processing jobs */
    UFT_BATCH_PAUSED,           /**< Paused by user */
    UFT_BATCH_STOPPING,         /**< Stopping (finishing current) */
    UFT_BATCH_COMPLETED,        /**< All jobs done */
    UFT_BATCH_ABORTED           /**< Aborted with errors */
} uft_batch_status_t;

/**
 * @brief Error severity
 */
typedef enum {
    UFT_BATCH_ERR_INFO = 0,     /**< Informational */
    UFT_BATCH_ERR_WARNING,      /**< Warning (job continued) */
    UFT_BATCH_ERR_ERROR,        /**< Error (job failed) */
    UFT_BATCH_ERR_FATAL         /**< Fatal (batch aborted) */
} uft_batch_err_severity_t;

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief Error entry
 */
typedef struct {
    uint32_t    job_id;                         /**< Job that caused error */
    uft_batch_err_severity_t severity;          /**< Error severity */
    int         error_code;                     /**< Error code */
    char        message[256];                   /**< Error message */
    char        source_file[UFT_BATCH_MAX_PATH];/**< Source file */
    time_t      timestamp;                      /**< When error occurred */
} uft_batch_error_t;

/**
 * @brief Job definition
 */
typedef struct {
    uint32_t    id;                             /**< Unique job ID */
    uft_job_type_t type;                        /**< Job type */
    uft_job_status_t status;                    /**< Current status */
    uft_job_priority_t priority;                /**< Priority level */
    
    /* Input/Output */
    char        input_path[UFT_BATCH_MAX_PATH]; /**< Input file path */
    char        output_path[UFT_BATCH_MAX_PATH];/**< Output file path */
    char        format_in[32];                  /**< Input format (auto if empty) */
    char        format_out[32];                 /**< Output format */
    
    /* Options */
    uint32_t    flags;                          /**< Job-specific flags */
    char        options[256];                   /**< Additional options (JSON) */
    
    /* Execution info */
    uint8_t     retries;                        /**< Retries remaining */
    uint8_t     attempts;                       /**< Attempts made */
    time_t      created;                        /**< Creation time */
    time_t      started;                        /**< Start time */
    time_t      completed;                      /**< Completion time */
    
    /* Progress */
    float       progress;                       /**< 0.0 - 1.0 */
    char        progress_msg[128];              /**< Current operation */
    
    /* Results */
    int         result_code;                    /**< Result code */
    char        result_msg[256];                /**< Result message */
    char        hash_md5[33];                   /**< MD5 of output */
    char        hash_sha256[65];                /**< SHA256 of output */
    uint64_t    bytes_processed;                /**< Bytes processed */
} uft_batch_job_t;

/**
 * @brief Batch statistics
 */
typedef struct {
    uint32_t    total_jobs;         /**< Total jobs in queue */
    uint32_t    pending_jobs;       /**< Jobs waiting */
    uint32_t    running_jobs;       /**< Jobs currently running */
    uint32_t    completed_jobs;     /**< Successfully completed */
    uint32_t    failed_jobs;        /**< Failed jobs */
    uint32_t    skipped_jobs;       /**< Skipped jobs */
    
    uint64_t    total_bytes;        /**< Total bytes to process */
    uint64_t    processed_bytes;    /**< Bytes processed */
    
    time_t      start_time;         /**< Batch start time */
    time_t      end_time;           /**< Batch end time */
    double      elapsed_seconds;    /**< Elapsed time */
    double      estimated_remaining;/**< Estimated remaining time */
    
    double      success_rate;       /**< Success percentage */
    double      throughput_mbps;    /**< Processing speed */
} uft_batch_stats_t;

/**
 * @brief Progress callback data
 */
typedef struct {
    uint32_t    job_id;             /**< Current job ID */
    const char *job_name;           /**< Job description */
    float       job_progress;       /**< Job progress (0-1) */
    float       batch_progress;     /**< Overall progress (0-1) */
    const char *current_op;         /**< Current operation */
    uft_batch_stats_t stats;        /**< Current statistics */
} uft_batch_progress_t;

/**
 * @brief Progress callback function
 */
typedef void (*uft_batch_progress_cb)(const uft_batch_progress_t *progress,
                                      void *user_data);

/**
 * @brief Job completion callback function
 */
typedef void (*uft_batch_complete_cb)(const uft_batch_job_t *job,
                                      void *user_data);

/**
 * @brief Error callback function
 */
typedef void (*uft_batch_error_cb)(const uft_batch_error_t *error,
                                   void *user_data);

/**
 * @brief Batch configuration
 */
typedef struct {
    uint8_t     num_workers;        /**< Number of parallel workers */
    uint8_t     max_retries;        /**< Maximum retries per job */
    uint16_t    timeout_seconds;    /**< Job timeout */
    
    bool        skip_existing;      /**< Skip if output exists */
    bool        verify_output;      /**< Verify output after write */
    bool        calculate_hashes;   /**< Calculate MD5/SHA256 */
    bool        stop_on_fatal;      /**< Stop batch on fatal error */
    bool        generate_report;    /**< Generate final report */
    bool        save_state;         /**< Save state for resume */
    
    char        output_dir[UFT_BATCH_MAX_PATH]; /**< Output directory */
    char        state_file[UFT_BATCH_MAX_PATH]; /**< State file for resume */
    char        report_file[UFT_BATCH_MAX_PATH];/**< Report output file */
    
    /* Callbacks */
    uft_batch_progress_cb progress_cb;  /**< Progress callback */
    uft_batch_complete_cb complete_cb;  /**< Job completion callback */
    uft_batch_error_cb error_cb;        /**< Error callback */
    void       *user_data;              /**< User data for callbacks */
} uft_batch_config_t;

/**
 * @brief Batch context (opaque)
 */
typedef struct uft_batch_ctx uft_batch_ctx_t;

/*===========================================================================
 * Function Prototypes - Lifecycle
 *===========================================================================*/

/**
 * @brief Initialize batch configuration with defaults
 */
void uft_batch_config_init(uft_batch_config_t *config);

/**
 * @brief Create batch context
 * 
 * @param config Configuration
 * @return Batch context or NULL on error
 */
uft_batch_ctx_t *uft_batch_create(const uft_batch_config_t *config);

/**
 * @brief Destroy batch context
 */
void uft_batch_destroy(uft_batch_ctx_t *ctx);

/*===========================================================================
 * Function Prototypes - Job Management
 *===========================================================================*/

/**
 * @brief Add job to queue
 * 
 * @param ctx Batch context
 * @param type Job type
 * @param input_path Input file path
 * @param output_path Output file path (NULL for in-place/analysis)
 * @param priority Job priority
 * @return Job ID or 0 on error
 */
uint32_t uft_batch_add_job(uft_batch_ctx_t *ctx, uft_job_type_t type,
                           const char *input_path, const char *output_path,
                           uft_job_priority_t priority);

/**
 * @brief Add job with extended options
 */
uint32_t uft_batch_add_job_ex(uft_batch_ctx_t *ctx, const uft_batch_job_t *job);

/**
 * @brief Add multiple jobs from directory
 * 
 * @param ctx Batch context
 * @param input_dir Input directory
 * @param pattern File pattern (e.g., "*.adf")
 * @param recursive Search recursively
 * @param type Job type
 * @return Number of jobs added
 */
uint32_t uft_batch_add_directory(uft_batch_ctx_t *ctx, const char *input_dir,
                                  const char *pattern, bool recursive,
                                  uft_job_type_t type);

/**
 * @brief Add jobs from file list
 * 
 * @param ctx Batch context
 * @param list_file Path to file containing paths (one per line)
 * @param type Job type
 * @return Number of jobs added
 */
uint32_t uft_batch_add_from_list(uft_batch_ctx_t *ctx, const char *list_file,
                                  uft_job_type_t type);

/**
 * @brief Remove job from queue
 */
int uft_batch_remove_job(uft_batch_ctx_t *ctx, uint32_t job_id);

/**
 * @brief Clear all pending jobs
 */
void uft_batch_clear_pending(uft_batch_ctx_t *ctx);

/**
 * @brief Get job by ID
 */
const uft_batch_job_t *uft_batch_get_job(uft_batch_ctx_t *ctx, uint32_t job_id);

/**
 * @brief Get all jobs matching status
 */
size_t uft_batch_get_jobs_by_status(uft_batch_ctx_t *ctx, uft_job_status_t status,
                                    uft_batch_job_t *jobs, size_t max_jobs);

/*===========================================================================
 * Function Prototypes - Execution
 *===========================================================================*/

/**
 * @brief Start batch processing
 * 
 * @param ctx Batch context
 * @return 0 on success
 */
int uft_batch_start(uft_batch_ctx_t *ctx);

/**
 * @brief Pause batch processing
 */
int uft_batch_pause(uft_batch_ctx_t *ctx);

/**
 * @brief Resume batch processing
 */
int uft_batch_resume(uft_batch_ctx_t *ctx);

/**
 * @brief Stop batch processing (wait for current jobs)
 */
int uft_batch_stop(uft_batch_ctx_t *ctx);

/**
 * @brief Abort batch processing (cancel current jobs)
 */
int uft_batch_abort(uft_batch_ctx_t *ctx);

/**
 * @brief Wait for batch completion
 * 
 * @param ctx Batch context
 * @param timeout_ms Timeout in milliseconds (0 = infinite)
 * @return 0 on completion, -1 on timeout
 */
int uft_batch_wait(uft_batch_ctx_t *ctx, uint32_t timeout_ms);

/**
 * @brief Process single job (blocking)
 */
int uft_batch_process_one(uft_batch_ctx_t *ctx, uint32_t job_id);

/*===========================================================================
 * Function Prototypes - Status & Progress
 *===========================================================================*/

/**
 * @brief Get batch status
 */
uft_batch_status_t uft_batch_get_status(const uft_batch_ctx_t *ctx);

/**
 * @brief Get batch statistics
 */
void uft_batch_get_stats(const uft_batch_ctx_t *ctx, uft_batch_stats_t *stats);

/**
 * @brief Get overall progress (0.0 - 1.0)
 */
float uft_batch_get_progress(const uft_batch_ctx_t *ctx);

/**
 * @brief Get error count
 */
uint32_t uft_batch_get_error_count(const uft_batch_ctx_t *ctx);

/**
 * @brief Get errors
 */
size_t uft_batch_get_errors(const uft_batch_ctx_t *ctx, uft_batch_error_t *errors,
                            size_t max_errors);

/*===========================================================================
 * Function Prototypes - State Persistence
 *===========================================================================*/

/**
 * @brief Save batch state for resume
 * 
 * @param ctx Batch context
 * @param path State file path
 * @return 0 on success
 */
int uft_batch_save_state(const uft_batch_ctx_t *ctx, const char *path);

/**
 * @brief Load batch state for resume
 * 
 * @param config Configuration for new context
 * @param path State file path
 * @return Batch context or NULL on error
 */
uft_batch_ctx_t *uft_batch_load_state(const uft_batch_config_t *config,
                                      const char *path);

/**
 * @brief Check if state file exists and is valid
 */
bool uft_batch_state_exists(const char *path);

/*===========================================================================
 * Function Prototypes - Reporting
 *===========================================================================*/

/**
 * @brief Generate JSON report
 * 
 * @param ctx Batch context
 * @param path Output file path
 * @return 0 on success
 */
int uft_batch_report_json(const uft_batch_ctx_t *ctx, const char *path);

/**
 * @brief Generate CSV report
 */
int uft_batch_report_csv(const uft_batch_ctx_t *ctx, const char *path);

/**
 * @brief Generate Markdown report
 */
int uft_batch_report_markdown(const uft_batch_ctx_t *ctx, const char *path);

/**
 * @brief Generate HTML report
 */
int uft_batch_report_html(const uft_batch_ctx_t *ctx, const char *path);

/**
 * @brief Print summary to stdout
 */
void uft_batch_print_summary(const uft_batch_ctx_t *ctx);

/*===========================================================================
 * Function Prototypes - Utilities
 *===========================================================================*/

/**
 * @brief Get job type name
 */
const char *uft_job_type_name(uft_job_type_t type);

/**
 * @brief Get job status name
 */
const char *uft_job_status_name(uft_job_status_t status);

/**
 * @brief Get batch status name
 */
const char *uft_batch_status_name(uft_batch_status_t status);

/**
 * @brief Format duration as string (e.g., "1h 23m 45s")
 */
void uft_batch_format_duration(double seconds, char *buffer, size_t size);

/**
 * @brief Format bytes as string (e.g., "1.23 GB")
 */
void uft_batch_format_bytes(uint64_t bytes, char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_BATCH_H */
