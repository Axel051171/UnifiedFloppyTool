// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file uft_dd_v3.h
 * @brief GOD MODE ULTRA DD Module API
 * @version 3.0.0-GOD-ULTRA
 * @date 2025-01-02
 *
 * Maximum performance disk copy with:
 * - Parallel I/O thread pool
 * - Memory-mapped large files
 * - Sparse file detection/creation
 * - Forensic audit trail
 * - Multiple hash algorithms
 * - Copy protection analysis
 */

#ifndef UFT_DD_V3_H
#define UFT_DD_V3_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * CONSTANTS
 *============================================================================*/

#define DD_V3_HASH_NONE         0x00
#define DD_V3_HASH_MD5          0x01
#define DD_V3_HASH_SHA256       0x02
#define DD_V3_HASH_SHA512       0x04
#define DD_V3_HASH_BLAKE3       0x08
#define DD_V3_HASH_XXH3         0x10
#define DD_V3_HASH_ALL          0x1F

#define DD_V3_COMPRESS_NONE     0
#define DD_V3_COMPRESS_LZ4      1
#define DD_V3_COMPRESS_ZSTD     2
#define DD_V3_COMPRESS_AUTO     3

/*============================================================================
 * TYPES
 *============================================================================*/

typedef struct dd_state_v3 dd_state_v3_t;

/* Configuration */
typedef struct {
    /* Files */
    const char* source_path;
    const char* dest_path;
    const char* checkpoint_path;
    const char* audit_log_path;
    const char* bad_sector_map_path;
    
    /* Offsets */
    uint64_t skip_bytes;
    uint64_t seek_bytes;
    uint64_t max_bytes;
    
    /* Block sizing */
    size_t block_size;
    size_t min_block_size;
    bool auto_block_size;
    
    /* Threading */
    int worker_threads;             /* 1-16, default 4 */
    int io_queue_depth;             /* 1-64, default 16 */
    
    /* Memory mapping */
    bool enable_mmap;               /* default true */
    size_t mmap_threshold;          /* default 1GB */
    
    /* Sparse files */
    bool detect_sparse;             /* default true */
    bool create_sparse;             /* default true */
    size_t sparse_threshold;        /* default 4096 */
    
    /* Hashing */
    uint32_t hash_algorithms;       /* DD_V3_HASH_* flags */
    bool hash_in_parallel;
    
    /* Compression */
    int compression_type;
    int compression_level;          /* 1-22 for ZSTD */
    
    /* Recovery */
    int max_retries;
    int retry_delay_ms;
    bool fill_on_error;
    uint8_t fill_pattern;
    
    /* Forensic */
    bool forensic_mode;
    bool preserve_timestamps;
    bool generate_report;
    
    /* Analysis */
    bool analyze_patterns;
    bool detect_protection;
    
    /* Bandwidth */
    uint64_t bandwidth_limit_bps;   /* 0 = unlimited */
    
    /* Verification */
    bool verify_after_write;
    bool verify_sector_by_sector;
    
} dd_config_v3_t;

/* Extended status */
typedef struct {
    /* Bytes */
    uint64_t bytes_read;
    uint64_t bytes_written;
    uint64_t bytes_verified;
    
    /* Errors */
    uint64_t errors_read;
    uint64_t errors_write;
    uint64_t errors_recovered;
    
    /* Sparse */
    uint64_t sparse_bytes_skipped;
    uint64_t sparse_regions;
    
    /* Performance (MB/s) */
    double read_speed_mbps;
    double write_speed_mbps;
    double verify_speed_mbps;
    double effective_speed_mbps;
    
    /* I/O stats */
    uint64_t io_ops_total;
    uint64_t io_ops_parallel;
    double avg_latency_us;
    double max_latency_us;
    
    /* Hash */
    double hash_speed_mbps;
    uint32_t hash_algorithms_active;
    
    /* Progress */
    double percent_complete;
    double eta_seconds;
    double elapsed_seconds;
    
    /* Compression */
    uint64_t bytes_before_compress;
    uint64_t bytes_after_compress;
    double compression_ratio;
    
    /* Forensic */
    uint64_t audit_entries;
    bool forensic_mode;
    
    /* Copy protection */
    bool copy_protection_detected;
    const char* protection_type;
    
    /* State */
    bool is_running;
    bool is_paused;
    bool is_mmap_mode;
    int worker_threads;
    
} dd_status_v3_t;

/*============================================================================
 * API FUNCTIONS
 *============================================================================*/

void dd_v3_config_init(dd_config_v3_t* config);
dd_state_v3_t* dd_v3_create(const dd_config_v3_t* config);
void dd_v3_destroy(dd_state_v3_t* state);

int dd_v3_run(dd_state_v3_t* state);
void dd_v3_pause(dd_state_v3_t* state);
void dd_v3_resume(dd_state_v3_t* state);
void dd_v3_cancel(dd_state_v3_t* state);

void dd_v3_get_status(dd_state_v3_t* state, dd_status_v3_t* status);

/*============================================================================
 * GUI PARAMETER CONSTRAINTS
 *============================================================================*/

#define DD_V3_WORKERS_MIN       1
#define DD_V3_WORKERS_MAX       16
#define DD_V3_WORKERS_DEFAULT   4

#define DD_V3_QUEUE_MIN         1
#define DD_V3_QUEUE_MAX         64
#define DD_V3_QUEUE_DEFAULT     16

#define DD_V3_BLOCK_MIN         512
#define DD_V3_BLOCK_MAX         16777216
#define DD_V3_BLOCK_DEFAULT     1048576

#ifdef __cplusplus
}
#endif

#endif /* UFT_DD_V3_H */
