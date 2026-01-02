// SPDX-License-Identifier: MIT
/*
 * hw_writer.h - Hardware Writer API
 * 
 * Professional block device writer for writing disk images to hardware.
 * Inspired by GNU dd (40+ years of production testing!).
 * 
 * Features extracted from dd.c:
 *   - Direct I/O (O_DIRECT)
 *   - Progress reporting
 *   - Error recovery (continue on error)
 *   - Sync mechanisms (fdatasync/fsync)
 *   - Cache invalidation
 *   - Aligned buffers (DMA compatible)
 *   - Statistics tracking
 * 
 * @version 2.7.0 Phase 2
 * @date 2024-12-25
 */

#ifndef UFT_HW_WRITER_H
#define UFT_HW_WRITER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * WRITE OPTIONS (Inspired by dd.c)
 *============================================================================*/

/**
 * @brief Hardware writer options
 */
typedef struct {
    // Block handling (from dd.c)
    size_t blocksize;           /**< Write block size (default: 512) */
    size_t skip_blocks;         /**< Skip input blocks */
    size_t seek_blocks;         /**< Seek output blocks */
    
    // I/O flags (from dd.c O_DIRECT, etc.)
    bool direct_io;             /**< Use O_DIRECT (bypass cache) */
    bool no_cache;              /**< Invalidate cache after write */
    bool sync_after_write;      /**< fdatasync after each block */
    bool sync_at_end;           /**< fsync at end */
    
    // Error handling (from dd.c C_NOERROR)
    bool continue_on_error;     /**< Continue on write errors */
    int max_retries;            /**< Max retries per block (default: 3) */
    
    // Progress (from dd.c STATUS_PROGRESS)
    bool show_progress;         /**< Show real-time progress */
    void (*progress_callback)(uint64_t current, uint64_t total, void *user);
    void *progress_user_data;
    
    // Verification
    bool verify_after_write;    /**< Read back and verify */
    
    // Alignment (from dd.c alignalloc)
    size_t buffer_alignment;    /**< Buffer alignment (4096 for DMA) */
    
} uft_hw_write_opts_t;

/*============================================================================*
 * STATISTICS (Inspired by dd.c)
 *============================================================================*/

/**
 * @brief Write statistics
 */
typedef struct {
    // From dd.c
    uint64_t full_blocks_written;    /**< Full blocks written */
    uint64_t partial_blocks_written; /**< Partial blocks written */
    uint64_t bytes_written;          /**< Total bytes written */
    uint64_t errors;                 /**< Write errors */
    uint64_t retries;                /**< Total retries */
    
    // Timing
    time_t start_time;          /**< Write start time */
    time_t end_time;            /**< Write end time */
    double duration_seconds;    /**< Total duration */
    double bytes_per_second;    /**< Write speed */
    
    // Verification
    uint64_t verify_errors;     /**< Verification errors */
    
} uft_hw_write_stats_t;

/*============================================================================*
 * CORE API
 *============================================================================*/

/**
 * @brief Initialize hardware writer subsystem
 */
int uft_hw_writer_init(void);

/**
 * @brief Shutdown hardware writer subsystem
 */
void uft_hw_writer_shutdown(void);

/**
 * @brief Get default write options
 * 
 * @param opts Options structure to fill
 */
void uft_hw_writer_get_defaults(uft_hw_write_opts_t *opts);

/**
 * @brief Write UFM disk to hardware device
 * 
 * Writes a complete UFM disk image to a hardware device (e.g., /dev/fd0).
 * Uses track encoder (v2.7.0) to convert logical sectors to MFM bitstream.
 * 
 * SYNERGY WITH TRACK ENCODER:
 *   - Encodes each track to MFM
 *   - Handles long tracks (copy protection!)
 *   - Handles weak bits (v2.7.1!)
 * 
 * dd.c FEATURES USED:
 *   - Direct I/O for immediate writes
 *   - Progress reporting
 *   - Error recovery
 *   - Sync for data integrity
 * 
 * @param ufm UFM disk image
 * @param device_path Device path (e.g., "/dev/fd0")
 * @param opts Write options (NULL for defaults)
 * @param stats_out Statistics (optional, can be NULL)
 * @return 0 on success, negative on error
 */
int uft_hw_write_ufm_disk(
    const void *ufm,  // ufm_disk_t pointer
    const char *device_path,
    const uft_hw_write_opts_t *opts,
    uft_hw_write_stats_t *stats_out
);

/**
 * @brief Write single track to hardware device
 * 
 * @param track UFM track
 * @param device_path Device path
 * @param cylinder Cylinder number
 * @param head Head number
 * @param opts Write options
 * @return 0 on success, negative on error
 */
int uft_hw_write_track(
    const void *track,  // ufm_track_t pointer
    const char *device_path,
    uint8_t cylinder,
    uint8_t head,
    const uft_hw_write_opts_t *opts
);

/**
 * @brief Write raw buffer to device
 * 
 * Low-level write with dd.c features (sync, retry, progress).
 * 
 * @param device_path Device path
 * @param buffer Data to write
 * @param size Buffer size
 * @param opts Write options
 * @param stats_out Statistics (optional)
 * @return 0 on success, negative on error
 */
int uft_hw_write_buffer(
    const char *device_path,
    const void *buffer,
    size_t size,
    const uft_hw_write_opts_t *opts,
    uft_hw_write_stats_t *stats_out
);

/*============================================================================*
 * dd.c INSPIRED UTILITIES
 *============================================================================*/

/**
 * @brief Allocate aligned buffer for DMA
 * 
 * From dd.c alignalloc: Allocates memory aligned to page boundaries
 * for Direct I/O and DMA operations.
 * 
 * @param size Buffer size
 * @param alignment Alignment (default: 4096)
 * @return Aligned buffer or NULL
 */
void* uft_hw_alloc_aligned(size_t size, size_t alignment);

/**
 * @brief Free aligned buffer
 * 
 * @param buffer Buffer to free
 */
void uft_hw_free_aligned(void *buffer);

/**
 * @brief Sync output to device
 * 
 * From dd.c synchronize_output: Ensures data is written to hardware.
 * 
 * @param fd File descriptor
 * @param use_fdatasync Use fdatasync (data only)
 * @param use_fsync Use fsync (data + metadata)
 * @return 0 on success, negative on error
 */
int uft_hw_sync_output(int fd, bool use_fdatasync, bool use_fsync);

/**
 * @brief Invalidate cache for device
 * 
 * From dd.c invalidate_cache: Forces immediate write to hardware.
 * 
 * @param fd File descriptor
 * @return 0 on success, negative on error
 */
int uft_hw_invalidate_cache(int fd);

/**
 * @brief Write with retry and error recovery
 * 
 * From dd.c error handling: Retries write on failure.
 * 
 * @param fd File descriptor
 * @param buffer Data to write
 * @param size Buffer size
 * @param max_retries Maximum retries
 * @return Bytes written, or negative on error
 */
ssize_t uft_hw_write_with_retry(
    int fd,
    const void *buffer,
    size_t size,
    int max_retries
);

/*============================================================================*
 * PROGRESS REPORTING (From dd.c)
 *============================================================================*/

/**
 * @brief Print write statistics
 * 
 * From dd.c print_xfer_stats: Shows transfer statistics.
 * 
 * Example output:
 *   "65536 bytes (66 kB) written in 1.5 s (44 kB/s)"
 * 
 * @param stats Statistics to print
 */
void uft_hw_print_stats(const uft_hw_write_stats_t *stats);

/**
 * @brief Update progress
 * 
 * From dd.c progress reporting: Real-time progress updates.
 * 
 * @param current Current bytes written
 * @param total Total bytes to write
 * @param start_time Start time
 */
void uft_hw_print_progress(
    uint64_t current,
    uint64_t total,
    time_t start_time
);

/*============================================================================*
 * VERIFICATION
 *============================================================================*/

/**
 * @brief Verify written data
 * 
 * Reads back written data and compares with original.
 * 
 * @param device_path Device path
 * @param expected_data Expected data
 * @param size Data size
 * @param opts Options (for blocksize, etc.)
 * @return 0 if verified, negative on mismatch
 */
int uft_hw_verify_data(
    const char *device_path,
    const void *expected_data,
    size_t size,
    const uft_hw_write_opts_t *opts
);

/*============================================================================*
 * DEVICE UTILITIES
 *============================================================================*/

/**
 * @brief Check if device is writable
 * 
 * @param device_path Device path
 * @return True if writable
 */
bool uft_hw_is_writable(const char *device_path);

/**
 * @brief Get device block size
 * 
 * @param device_path Device path
 * @return Block size or 0 on error
 */
size_t uft_hw_get_block_size(const char *device_path);

/**
 * @brief Detect floppy drive parameters
 * 
 * @param device_path Device path (e.g., "/dev/fd0")
 * @param cylinders_out Number of cylinders (output)
 * @param heads_out Number of heads (output)
 * @return 0 on success, negative on error
 */
int uft_hw_detect_floppy_params(
    const char *device_path,
    uint8_t *cylinders_out,
    uint8_t *heads_out
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_HW_WRITER_H */
