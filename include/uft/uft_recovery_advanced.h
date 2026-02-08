#ifndef UFT_RECOVERY_ADVANCED_H
#define UFT_RECOVERY_ADVANCED_H

/**
 * @file uft_recovery.h
 * @brief Advanced Disk Recovery Algorithms
 * 
 * @version 4.2.0
 * @date 2026-01-03
 * 
 * ALGORITHMS:
 * - safecopy-style low-level recovery
 * - recoverdm bad sector handling
 * - Multi-pass adaptive reading
 * - Error mapping and interpolation
 * 
 * FEATURES:
 * - Configurable retry strategies
 * - Sector-level error tracking
 * - Bad block mapping
 * - Progress reporting
 * - Partial read support
 * 
 * USAGE:
 * ```c
 * uft_recovery_config_t config = UFT_RECOVERY_CONFIG_DEFAULT;
 * config.max_retries = 5;
 * config.progress_cb = my_progress;
 * 
 * uft_recovery_t *rec = uft_recovery_create(&config);
 * uft_recovery_run(rec, device, output_file);
 * uft_recovery_report(rec, "recovery.log");
 * uft_recovery_destroy(rec);
 * ```
 */

#ifndef UFT_RECOVERY_H
#define UFT_RECOVERY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_RECOVERY_MAX_RETRIES    10
#define UFT_RECOVERY_DEFAULT_BLOCK  512
#define UFT_RECOVERY_MAX_SKIP       (1024 * 1024)  /* 1 MB max skip */

/* ═══════════════════════════════════════════════════════════════════════════════
 * Error Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_REC_ERR_NONE        = 0,
    UFT_REC_ERR_READ        = 1,    /* Read error */
    UFT_REC_ERR_TIMEOUT     = 2,    /* Read timeout */
    UFT_REC_ERR_CRC         = 3,    /* CRC/ECC error */
    UFT_REC_ERR_SEEK        = 4,    /* Seek error */
    UFT_REC_ERR_MEDIA       = 5,    /* Media error */
    UFT_REC_ERR_ID          = 6,    /* Sector ID not found */
    UFT_REC_ERR_ABORT       = 7,    /* Aborted by user */
    UFT_REC_ERR_MEMORY      = 8,    /* Memory allocation */
    UFT_REC_ERR_IO          = 9     /* I/O error */
} uft_recovery_error_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Recovery Strategy
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_REC_STRATEGY_LINEAR,        /* Read sequentially, skip on error */
    UFT_REC_STRATEGY_ADAPTIVE,      /* Adjust block size on errors */
    UFT_REC_STRATEGY_BISECT,        /* Binary search for good sectors */
    UFT_REC_STRATEGY_AGGRESSIVE,    /* Many retries, small blocks */
    UFT_REC_STRATEGY_GENTLE         /* Few retries, skip quickly */
} uft_recovery_strategy_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Sector Status
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_SECTOR_UNKNOWN      = 0,
    UFT_SECTOR_GOOD         = 1,
    UFT_SECTOR_RECOVERED    = 2,    /* Read after retries */
    UFT_SECTOR_PARTIAL      = 3,    /* Partially recovered */
    UFT_SECTOR_BAD          = 4,    /* Unrecoverable */
    UFT_SECTOR_SKIPPED      = 5     /* Skipped */
} uft_sector_status_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Progress Callback
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint64_t bytes_total;           /* Total bytes to process */
    uint64_t bytes_processed;       /* Bytes processed so far */
    uint64_t bytes_good;            /* Good bytes read */
    uint64_t bytes_bad;             /* Bad bytes */
    uint64_t current_position;      /* Current position */
    
    int      sectors_total;
    int      sectors_good;
    int      sectors_bad;
    int      sectors_recovered;
    
    int      current_retry;
    size_t   current_block_size;
    float    speed_mbps;            /* Current speed in MB/s */
    float    eta_seconds;           /* Estimated time remaining */
    
    const char *status_text;        /* Human-readable status */
} uft_recovery_progress_t;

/**
 * @brief Progress callback function
 * @return false to abort recovery
 */
typedef bool (*uft_recovery_progress_fn)(
    const uft_recovery_progress_t *progress,
    void *user_data
);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Configuration
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Strategy */
    uft_recovery_strategy_t strategy;
    
    /* Block sizes */
    size_t initial_block_size;      /* Starting block size */
    size_t min_block_size;          /* Minimum block size */
    size_t max_block_size;          /* Maximum block size */
    
    /* Retries */
    int    max_retries;             /* Retries per sector */
    int    max_skip_retries;        /* Retries before skipping */
    
    /* Timeouts (milliseconds) */
    int    read_timeout;            /* Single read timeout */
    int    sector_timeout;          /* Total time per sector */
    
    /* Behavior */
    bool   fill_bad_sectors;        /* Fill bad sectors with pattern */
    uint8_t bad_sector_fill;        /* Fill byte (default 0) */
    bool   preserve_partial;        /* Keep partial reads */
    bool   reverse_direction;       /* Read backwards */
    bool   verify_writes;           /* Verify written data */
    
    /* Skipping */
    size_t skip_size;               /* Bytes to skip on persistent error */
    size_t max_skip_size;           /* Maximum skip size */
    
    /* Callbacks */
    uft_recovery_progress_fn progress_cb;
    void  *user_data;
    int    progress_interval_ms;    /* Progress callback interval */
    
} uft_recovery_config_t;

#define UFT_RECOVERY_CONFIG_DEFAULT { \
    .strategy = UFT_REC_STRATEGY_ADAPTIVE, \
    .initial_block_size = 65536, \
    .min_block_size = 512, \
    .max_block_size = 1048576, \
    .max_retries = 3, \
    .max_skip_retries = 1, \
    .read_timeout = 3000, \
    .sector_timeout = 10000, \
    .fill_bad_sectors = true, \
    .bad_sector_fill = 0, \
    .preserve_partial = true, \
    .reverse_direction = false, \
    .verify_writes = false, \
    .skip_size = 4096, \
    .max_skip_size = 1048576, \
    .progress_cb = NULL, \
    .user_data = NULL, \
    .progress_interval_ms = 500 \
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Bad Block Entry
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint64_t offset;                /* Byte offset */
    uint64_t length;                /* Length in bytes */
    uft_recovery_error_t error;     /* Error type */
    int      attempts;              /* Read attempts made */
    uft_sector_status_t status;     /* Final status */
} uft_bad_block_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Recovery Context (opaque)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct uft_recovery uft_recovery_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Recovery Statistics
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Byte counts */
    uint64_t bytes_total;
    uint64_t bytes_read;
    uint64_t bytes_good;
    uint64_t bytes_bad;
    uint64_t bytes_skipped;
    
    /* Sector counts */
    uint64_t sectors_total;
    uint64_t sectors_good;
    uint64_t sectors_recovered;
    uint64_t sectors_bad;
    uint64_t sectors_skipped;
    
    /* Performance */
    double   elapsed_seconds;
    double   avg_speed_mbps;
    
    /* Error details */
    int      read_errors;
    int      crc_errors;
    int      timeout_errors;
    int      seek_errors;
    int      total_retries;
    
    /* Bad block info */
    int      bad_block_count;
    uint64_t largest_bad_region;
    
} uft_recovery_stats_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Recovery Context
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create recovery context
 * @param config Configuration (NULL for defaults)
 * @return Recovery context or NULL on error
 */
uft_recovery_t *uft_recovery_create(const uft_recovery_config_t *config);

/**
 * @brief Destroy recovery context
 */
void uft_recovery_destroy(uft_recovery_t *rec);

/**
 * @brief Reset recovery context for new operation
 */
void uft_recovery_reset(uft_recovery_t *rec);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Recovery Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Recover from device to file
 * @param rec Recovery context
 * @param device Source device/file path
 * @param output Output file path
 * @return 0 on success (may have bad sectors)
 */
int uft_recovery_run(uft_recovery_t *rec,
                     const char *device,
                     const char *output);

/**
 * @brief Recover from device to memory
 * @param rec Recovery context
 * @param device Source device/file
 * @param data Output buffer (caller allocates)
 * @param size Buffer size
 * @return Bytes successfully read
 */
size_t uft_recovery_to_mem(uft_recovery_t *rec,
                           const char *device,
                           uint8_t *data,
                           size_t size);

/**
 * @brief Recover specific range
 */
int uft_recovery_range(uft_recovery_t *rec,
                       const char *device,
                       const char *output,
                       uint64_t start,
                       uint64_t length);

/**
 * @brief Continue previous recovery (from log)
 */
int uft_recovery_resume(uft_recovery_t *rec,
                        const char *device,
                        const char *output,
                        const char *log_file);

/**
 * @brief Abort current recovery
 */
void uft_recovery_abort(uft_recovery_t *rec);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Multi-Pass Recovery
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief First pass: fast read of good sectors
 */
int uft_recovery_pass_fast(uft_recovery_t *rec,
                           const char *device,
                           const char *output);

/**
 * @brief Second pass: retry bad sectors with smaller blocks
 */
int uft_recovery_pass_retry(uft_recovery_t *rec,
                            const char *device,
                            const char *output);

/**
 * @brief Final pass: aggressive recovery of remaining bad sectors
 */
int uft_recovery_pass_scrape(uft_recovery_t *rec,
                             const char *device,
                             const char *output);

/**
 * @brief Run all passes automatically
 */
int uft_recovery_multi_pass(uft_recovery_t *rec,
                            const char *device,
                            const char *output);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Bad Block Management
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get bad block count
 */
int uft_recovery_bad_block_count(const uft_recovery_t *rec);

/**
 * @brief Get bad block by index
 */
int uft_recovery_get_bad_block(const uft_recovery_t *rec,
                               int index,
                               uft_bad_block_t *block);

/**
 * @brief Get all bad blocks
 */
int uft_recovery_get_bad_blocks(const uft_recovery_t *rec,
                                uft_bad_block_t *blocks,
                                int max_blocks);

/**
 * @brief Load bad block map from file
 */
int uft_recovery_load_map(uft_recovery_t *rec, const char *map_file);

/**
 * @brief Save bad block map to file
 */
int uft_recovery_save_map(const uft_recovery_t *rec, const char *map_file);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Statistics and Reporting
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get recovery statistics
 */
void uft_recovery_get_stats(const uft_recovery_t *rec,
                            uft_recovery_stats_t *stats);

/**
 * @brief Generate recovery report
 */
int uft_recovery_report(const uft_recovery_t *rec,
                        const char *report_file);

/**
 * @brief Generate recovery log (for resume)
 */
int uft_recovery_write_log(const uft_recovery_t *rec,
                           const char *log_file);

/**
 * @brief Print summary to stdout
 */
void uft_recovery_print_summary(const uft_recovery_t *rec);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Sector-Level Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read single sector with retries
 * @param rec Recovery context
 * @param fd File descriptor
 * @param offset Byte offset
 * @param size Bytes to read
 * @param buffer Output buffer
 * @param bytes_read Actual bytes read
 * @return Sector status
 */
uft_sector_status_t uft_recovery_read_sector(
    uft_recovery_t *rec,
    int fd,
    uint64_t offset,
    size_t size,
    uint8_t *buffer,
    size_t *bytes_read
);

/**
 * @brief Get status of specific sector
 */
uft_sector_status_t uft_recovery_sector_status(
    const uft_recovery_t *rec,
    uint64_t offset
);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get error string
 */
const char *uft_recovery_error_string(uft_recovery_error_t error);

/**
 * @brief Get status string
 */
const char *uft_recovery_status_string(uft_sector_status_t status);

/**
 * @brief Get strategy string
 */
const char *uft_recovery_strategy_string(uft_recovery_strategy_t strategy);

/**
 * @brief Verify recovered data against original (if available)
 */
int uft_recovery_verify(const char *original,
                        const char *recovered,
                        uft_bad_block_t *differences,
                        int max_differences);

#ifdef __cplusplus
}
#endif

#endif /* UFT_RECOVERY_H */

#endif /* UFT_RECOVERY_ADVANCED_H */
