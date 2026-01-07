// SPDX-License-Identifier: MIT
/*
 * hw_writer.c - Hardware Writer Implementation
 * 
 * Professional block device writer with features from GNU dd.
 * 
 * @version 2.7.0 Phase 2
 * @date 2024-12-25
 */

#define _GNU_SOURCE  // For O_DIRECT
#include "hw_writer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fd.h>  // For floppy ioctls

// For aligned allocation (dd.c style)
#ifdef HAVE_POSIX_MEMALIGN
#include <malloc.h>
#endif

/*============================================================================*
 * INTERNAL UTILITIES (From dd.c)
 *============================================================================*/

/**
 * @brief Get current time in seconds (for progress)
 */
static inline time_t get_time(void) {
    return time(NULL);
}

/**
 * @brief Calculate elapsed time
 */
static inline double elapsed_seconds(time_t start, time_t end) {
    return difftime(end, start);
}

/*============================================================================*
 * INITIALIZATION
 *============================================================================*/

int uft_hw_writer_init(void) {
    // Nothing to initialize yet
    return 0;
}

void uft_hw_writer_shutdown(void) {
    // Nothing to clean up yet
}

/*============================================================================*
 * DEFAULT OPTIONS
 *============================================================================*/

void uft_hw_writer_get_defaults(uft_hw_write_opts_t *opts) {
    if (!opts) return;
    
    memset(opts, 0, sizeof(*opts));
    
    // Default values (from dd.c DEFAULT_BLOCKSIZE)
    opts->blocksize = 512;
    opts->skip_blocks = 0;
    opts->seek_blocks = 0;
    
    // I/O flags (dd.c style)
    opts->direct_io = true;          // Use O_DIRECT for immediate writes
    opts->no_cache = true;           // Invalidate cache
    opts->sync_after_write = false;  // Don't sync every block (slow!)
    opts->sync_at_end = true;        // Sync at end
    
    // Error handling (dd.c C_NOERROR)
    opts->continue_on_error = false;
    opts->max_retries = 3;
    
    // Progress
    opts->show_progress = true;
    opts->progress_callback = NULL;
    opts->progress_user_data = NULL;
    
    // Verification
    opts->verify_after_write = false;
    
    // Alignment (dd.c alignalloc - 4K for DMA)
    opts->buffer_alignment = 4096;
}

/*============================================================================*
 * dd.c UTILITIES
 *============================================================================*/

/**
 * @brief Allocate aligned buffer (from dd.c alignalloc)
 */
void* uft_hw_alloc_aligned(size_t size, size_t alignment) {
    if (alignment == 0) {
        alignment = 4096;  // Default to 4K (page size)
    }
    
    void *buffer = NULL;
    
#ifdef HAVE_POSIX_MEMALIGN
    if (posix_memalign(&buffer, alignment, size) != 0) {
        return NULL;
    }
#else
    // Fallback: aligned_alloc or malloc
    #ifdef HAVE_ALIGNED_ALLOC
    buffer = aligned_alloc(alignment, size);
    #else
    buffer = malloc(size);  // Not aligned, but works
    #endif
#endif
    
    return buffer;
}

/**
 * @brief Free aligned buffer
 */
void uft_hw_free_aligned(void *buffer) {
    if (buffer) {
        free(buffer);
    }
}

/**
 * @brief Sync output (from dd.c synchronize_output)
 */
int uft_hw_sync_output(int fd, bool use_fdatasync, bool use_fsync) {
    if (use_fdatasync) {
        #ifdef HAVE_FDATASYNC
        if (fdatasync(fd) != 0) {
            if (errno != ENOSYS && errno != EINVAL) {
                perror("fdatasync");
                return -1;
            }
            // Fall back to fsync
            use_fsync = true;
        }
        #else
        use_fsync = true;  // No fdatasync, use fsync
        #endif
    }
    
    if (use_fsync) {
        if (fsync(fd) != 0) {
            perror("fsync");
            return -1;
        }
    }
    
    return 0;
}

/**
 * @brief Invalidate cache (from dd.c invalidate_cache)
 */
int uft_hw_invalidate_cache(int fd) {
    #ifdef HAVE_POSIX_FADVISE
    // Advise kernel to drop cache
    if (posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED) != 0) {
        // Non-fatal, just advisory
    }
    #endif
    return 0;
}

/**
 * @brief Write with retry (from dd.c error handling)
 */
ssize_t uft_hw_write_with_retry(
    int fd,
    const void *buffer,
    size_t size,
    int max_retries
) {
    ssize_t total_written = 0;
    int retries = 0;
    
    while (total_written < (ssize_t)size && retries <= max_retries) {
        ssize_t written = write(fd, 
                               (const char*)buffer + total_written,
                               size - total_written);
        
        if (written < 0) {
            if (errno == EINTR) {
                // Interrupted, retry immediately
                continue;
            }
            
            retries++;
            if (retries > max_retries) {
                return -1;
            }
            
            // Wait a bit before retry
            usleep(100000);  // 100ms
            continue;
        }
        
        if (written == 0) {
            // No progress
            retries++;
            if (retries > max_retries) {
                break;
            }
            continue;
        }
        
        total_written += written;
        retries = 0;  // Reset retry count on success
    }
    
    return total_written;
}

/*============================================================================*
 * PROGRESS REPORTING (From dd.c)
 *============================================================================*/

/**
 * @brief Format bytes for human readability
 */
static void format_bytes(uint64_t bytes, char *buf, size_t bufsize) {
    if (bytes < 1024) {
        snprintf(buf, bufsize, "%llu B", (unsigned long long)bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buf, bufsize, "%.1f KB", bytes / 1024.0);
    } else if (bytes < 1024 * 1024 * 1024) {
        snprintf(buf, bufsize, "%.1f MB", bytes / (1024.0 * 1024.0));
    } else {
        snprintf(buf, bufsize, "%.1f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    }
}

/**
 * @brief Print progress (from dd.c print_xfer_stats)
 */
void uft_hw_print_progress(
    uint64_t current,
    uint64_t total,
    time_t start_time
) {
    time_t now = get_time();
    double elapsed = elapsed_seconds(start_time, now);
    
    char current_str[64];
    char total_str[64];
    format_bytes(current, current_str, sizeof(current_str));
    format_bytes(total, total_str, sizeof(total_str));
    
    double percent = total > 0 ? (current * 100.0) / total : 0.0;
    double speed = elapsed > 0 ? current / elapsed : 0.0;
    
    char speed_str[64];
    format_bytes((uint64_t)speed, speed_str, sizeof(speed_str));
    
    // Estimate time remaining
    double eta = (total - current) / (speed > 0 ? speed : 1);
    
    printf("\r%s / %s (%.1f%%) | %.1f s | %s/s | ETA: %.0f s    ",
           current_str, total_str, percent, elapsed, speed_str, eta);
    fflush(stdout);
}

/**
 * @brief Print final statistics (from dd.c)
 */
void uft_hw_print_stats(const uft_hw_write_stats_t *stats) {
    if (!stats) return;
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  WRITE STATISTICS\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
    
    char bytes_str[64];
    format_bytes(stats->bytes_written, bytes_str, sizeof(bytes_str));
    
    printf("Bytes written:      %s (%llu bytes)\n", 
           bytes_str, (unsigned long long)stats->bytes_written);
    printf("Full blocks:        %llu\n", (unsigned long long)stats->full_blocks_written);
    printf("Partial blocks:     %llu\n", (unsigned long long)stats->partial_blocks_written);
    printf("Errors:             %llu\n", (unsigned long long)stats->errors);
    printf("Retries:            %llu\n", (unsigned long long)stats->retries);
    
    if (stats->verify_errors > 0) {
        printf("Verify errors:      %llu ⚠️\n", (unsigned long long)stats->verify_errors);
    }
    
    printf("\n");
    printf("Duration:           %.2f seconds\n", stats->duration_seconds);
    
    char speed_str[64];
    format_bytes((uint64_t)stats->bytes_per_second, speed_str, sizeof(speed_str));
    printf("Average speed:      %s/s\n", speed_str);
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
}

/*============================================================================*
 * DEVICE UTILITIES
 *============================================================================*/

bool uft_hw_is_writable(const char *device_path) {
    if (!device_path) return false;
    
    // Try to open for writing
    int fd = open(device_path, O_WRONLY);
    if (fd < 0) {
        return false;
    }
    
    close(fd);
    return true;
}

size_t uft_hw_get_block_size(const char *device_path) {
    if (!device_path) return 0;
    
    int fd = open(device_path, O_RDONLY);
    if (fd < 0) {
        return 0;
    }
    
    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        return 0;
    }
    
    close(fd);
    
    // Return block size (default 512 for block devices)
    return st.st_blksize > 0 ? st.st_blksize : 512;
}

int uft_hw_detect_floppy_params(
    const char *device_path,
    uint8_t *cylinders_out,
    uint8_t *heads_out
) {
    if (!device_path || !cylinders_out || !heads_out) {
        return -1;
    }
    
    int fd = open(device_path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    
    struct uft_floppy_drive_params params;
    if (ioctl(fd, FDGETDRVPRM, &params) == 0) {
        *cylinders_out = 80;  // Standard
        *heads_out = 2;       // Standard
        close(fd);
        return 0;
    }
    
    close(fd);
    
    // Fallback: assume standard 3.5" floppy
    *cylinders_out = 80;
    *heads_out = 2;
    return 0;
}

/*============================================================================*
 * VERIFICATION
 *============================================================================*/

int uft_hw_verify_data(
    const char *device_path,
    const void *expected_data,
    size_t size,
    const uft_hw_write_opts_t *opts
) {
    if (!device_path || !expected_data || size == 0) {
        return -1;
    }
    
    // Allocate read buffer
    void *read_buffer = uft_hw_alloc_aligned(size, 
        opts ? opts->buffer_alignment : 4096);
    if (!read_buffer) {
        return -1;
    }
    
    // Open device for reading
    int fd = open(device_path, O_RDONLY);
    if (fd < 0) {
        uft_hw_free_aligned(read_buffer);
        return -1;
    }
    
    // Read data
    ssize_t bytes_read = read(fd, read_buffer, size);
    close(fd);
    
    if (bytes_read != (ssize_t)size) {
        uft_hw_free_aligned(read_buffer);
        return -1;
    }
    
    // Compare
    int result = memcmp(expected_data, read_buffer, size);
    
    uft_hw_free_aligned(read_buffer);
    
    return result;
}

/*============================================================================*
 * LOW-LEVEL WRITE
 *============================================================================*/

int uft_hw_write_buffer(
    const char *device_path,
    const void *buffer,
    size_t size,
    const uft_hw_write_opts_t *opts,
    uft_hw_write_stats_t *stats_out
) {
    if (!device_path || !buffer || size == 0) {
        return -1;
    }
    
    // Get options (use defaults if NULL)
    uft_hw_write_opts_t default_opts;
    if (!opts) {
        uft_hw_writer_get_defaults(&default_opts);
        opts = &default_opts;
    }
    
    // Initialize statistics
    uft_hw_write_stats_t stats = {0};
    stats.start_time = get_time();
    
    // Open device with appropriate flags (dd.c style)
    int flags = O_WRONLY;
    if (opts->direct_io) {
        #ifdef O_DIRECT
        flags |= O_DIRECT;
        #endif
    }
    
    int fd = open(device_path, flags);
    if (fd < 0) {
        fprintf(stderr, "Error: Cannot open %s: %s\n", 
                device_path, strerror(errno));
        return -1;
    }
    
    // Seek if requested (dd.c seek=)
    if (opts->seek_blocks > 0) {
        off_t offset = opts->seek_blocks * opts->blocksize;
        if (lseek(fd, offset, SEEK_SET) < 0) {
            perror("lseek");
            close(fd);
            return -1;
        }
    }
    
    // Write data with retry (dd.c style)
    ssize_t written = uft_hw_write_with_retry(fd, buffer, size, opts->max_retries);
    
    if (written < 0) {
        stats.errors++;
        if (!opts->continue_on_error) {
            close(fd);
            return -1;
        }
    } else {
        stats.bytes_written = written;
        if ((size_t)written == size) {
            stats.full_blocks_written = 1;
        } else {
            stats.partial_blocks_written = 1;
        }
    }
    
    // Sync if requested (dd.c conv=fdatasync,fsync)
    if (opts->sync_after_write || opts->sync_at_end) {
        uft_hw_sync_output(fd, opts->sync_after_write, opts->sync_at_end);
    }
    
    // Invalidate cache (dd.c oflag=nocache)
    if (opts->no_cache) {
        uft_hw_invalidate_cache(fd);
    }
    
    close(fd);
    
    // Verify if requested
    if (opts->verify_after_write && written > 0) {
        if (uft_hw_verify_data(device_path, buffer, size, opts) != 0) {
            stats.verify_errors++;
        }
    }
    
    // Finalize statistics
    stats.end_time = get_time();
    stats.duration_seconds = elapsed_seconds(stats.start_time, stats.end_time);
    stats.bytes_per_second = stats.duration_seconds > 0 ?
        stats.bytes_written / stats.duration_seconds : 0;
    
    // Copy statistics if requested
    if (stats_out) {
        *stats_out = stats;
    }
    
    return written > 0 ? 0 : -1;
}

/*============================================================================*
 * HIGH-LEVEL WRITE (UFM Integration - Placeholder)
 *============================================================================*/

int uft_hw_write_track(
    const void *track,
    const char *device_path,
    uint8_t cylinder,
    uint8_t head,
    const uft_hw_write_opts_t *opts
) {
    // TODO: Integrate with Track Encoder (v2.7.0 Phase 1)
    // 1. Get track data from UFM
    // 2. Encode to MFM bitstream
    // 3. Write with uft_hw_write_buffer()
    
    (void)track;
    (void)device_path;
    (void)cylinder;
    (void)head;
    (void)opts;
    
    fprintf(stderr, "TODO: Implement track encoder integration\n");
    return -1;
}

int uft_hw_write_ufm_disk(
    const void *ufm,
    const char *device_path,
    const uft_hw_write_opts_t *opts,
    uft_hw_write_stats_t *stats_out
) {
    // TODO: Integrate with UFM and Track Encoder
    // 1. Iterate all tracks in UFM
    // 2. Encode each track
    // 3. Write to device
    // 4. Update progress
    
    (void)ufm;
    (void)device_path;
    (void)opts;
    (void)stats_out;
    
    fprintf(stderr, "TODO: Implement UFM disk writer\n");
    return -1;
}
