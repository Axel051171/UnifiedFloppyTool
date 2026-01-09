#include "uft/compat/uft_platform.h"
/**
 * @file uft_recovery_advanced.c
 * @brief Advanced Disk Recovery Implementation
 * @version 4.2.0
 * 
 * Implements safecopy and recoverdm algorithms for disk recovery.
 */

/* POSIX Feature Test Macros - MUST be before any includes */
#if !defined(_WIN32) && !defined(_WIN64)
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
    #ifndef _DEFAULT_SOURCE
        #define _DEFAULT_SOURCE 1
    #endif
#endif

#include "uft/uft_recovery_advanced.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
    #define INVALID_FD INVALID_HANDLE_VALUE
    /* Use uint64_t for timing on Windows instead of struct timespec */
    typedef uint64_t uft_time_point_t;
    #define UFT_USE_WINDOWS_TIME 1
#else
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/stat.h>
    #include <sys/time.h>
    #define INVALID_FD -1
    typedef struct timespec uft_time_point_t;
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

struct uft_recovery {
    uft_recovery_config_t config;
    
    /* Statistics */
    uft_recovery_stats_t stats;
    
    /* Bad block list */
    uft_bad_block_t *bad_blocks;
    int bad_block_count;
    int bad_block_capacity;
    
    /* State */
    bool aborted;
    uint64_t current_position;
    size_t current_block_size;
    
    /* Timing - platform-specific */
    uft_time_point_t start_time;
    uft_time_point_t last_progress;
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Error Strings
 * ═══════════════════════════════════════════════════════════════════════════════ */

const char *uft_recovery_error_string(uft_recovery_error_t error)
{
    switch (error) {
        case UFT_REC_ERR_NONE:    return "No error";
        case UFT_REC_ERR_READ:    return "Read error";
        case UFT_REC_ERR_TIMEOUT: return "Timeout";
        case UFT_REC_ERR_CRC:     return "CRC error";
        case UFT_REC_ERR_SEEK:    return "Seek error";
        case UFT_REC_ERR_MEDIA:   return "Media error";
        case UFT_REC_ERR_ID:      return "Sector ID not found";
        case UFT_REC_ERR_ABORT:   return "Aborted";
        case UFT_REC_ERR_MEMORY:  return "Memory error";
        case UFT_REC_ERR_IO:      return "I/O error";
        default:                  return "Unknown error";
    }
}

const char *uft_recovery_status_string(uft_sector_status_t status)
{
    switch (status) {
        case UFT_SECTOR_UNKNOWN:   return "Unknown";
        case UFT_SECTOR_GOOD:      return "Good";
        case UFT_SECTOR_RECOVERED: return "Recovered";
        case UFT_SECTOR_PARTIAL:   return "Partial";
        case UFT_SECTOR_BAD:       return "Bad";
        case UFT_SECTOR_SKIPPED:   return "Skipped";
        default:                   return "Unknown";
    }
}

const char *uft_recovery_strategy_string(uft_recovery_strategy_t strategy)
{
    switch (strategy) {
        case UFT_REC_STRATEGY_LINEAR:     return "Linear";
        case UFT_REC_STRATEGY_ADAPTIVE:   return "Adaptive";
        case UFT_REC_STRATEGY_BISECT:     return "Bisect";
        case UFT_REC_STRATEGY_AGGRESSIVE: return "Aggressive";
        case UFT_REC_STRATEGY_GENTLE:     return "Gentle";
        default:                          return "Unknown";
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Context Management
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_recovery_t *uft_recovery_create(const uft_recovery_config_t *config)
{
    uft_recovery_t *rec = (uft_recovery_t*)calloc(1, sizeof(uft_recovery_t));
    if (!rec) return NULL;
    
    if (config) {
        rec->config = *config;
    } else {
        uft_recovery_config_t def = UFT_RECOVERY_CONFIG_DEFAULT;
        rec->config = def;
    }
    
    rec->bad_block_capacity = 1024;
    rec->bad_blocks = (uft_bad_block_t*)calloc(rec->bad_block_capacity, 
                                                sizeof(uft_bad_block_t));
    if (!rec->bad_blocks) {
        free(rec);
        return NULL;
    }
    
    rec->current_block_size = rec->config.initial_block_size;
    
    return rec;
}

void uft_recovery_destroy(uft_recovery_t *rec)
{
    if (!rec) return;
    free(rec->bad_blocks);
    free(rec);
}

void uft_recovery_reset(uft_recovery_t *rec)
{
    if (!rec) return;
    
    memset(&rec->stats, 0, sizeof(rec->stats));
    rec->bad_block_count = 0;
    rec->aborted = false;
    rec->current_position = 0;
    rec->current_block_size = rec->config.initial_block_size;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Time Utilities
 * ═══════════════════════════════════════════════════════════════════════════════ */

static double get_time_sec(void)
{
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart / freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Bad Block Management
 * ═══════════════════════════════════════════════════════════════════════════════ */

static int add_bad_block(uft_recovery_t *rec, 
                         uint64_t offset, uint64_t length,
                         uft_recovery_error_t error,
                         int attempts,
                         uft_sector_status_t status)
{
    if (rec->bad_block_count >= rec->bad_block_capacity) {
        int new_cap = rec->bad_block_capacity * 2;
        uft_bad_block_t *new_blocks = (uft_bad_block_t*)realloc(
            rec->bad_blocks, new_cap * sizeof(uft_bad_block_t));
        if (!new_blocks) return -1;
        rec->bad_blocks = new_blocks;
        rec->bad_block_capacity = new_cap;
    }
    
    uft_bad_block_t *bb = &rec->bad_blocks[rec->bad_block_count++];
    bb->offset = offset;
    bb->length = length;
    bb->error = error;
    bb->attempts = attempts;
    bb->status = status;
    
    /* Update stats */
    rec->stats.bad_block_count++;
    if (length > rec->stats.largest_bad_region) {
        rec->stats.largest_bad_region = length;
    }
    
    return 0;
}

int uft_recovery_bad_block_count(const uft_recovery_t *rec)
{
    return rec ? rec->bad_block_count : 0;
}

int uft_recovery_get_bad_block(const uft_recovery_t *rec,
                               int index,
                               uft_bad_block_t *block)
{
    if (!rec || !block || index < 0 || index >= rec->bad_block_count) {
        return -1;
    }
    *block = rec->bad_blocks[index];
    return 0;
}

int uft_recovery_get_bad_blocks(const uft_recovery_t *rec,
                                uft_bad_block_t *blocks,
                                int max_blocks)
{
    if (!rec || !blocks) return 0;
    
    int count = rec->bad_block_count;
    if (count > max_blocks) count = max_blocks;
    
    memcpy(blocks, rec->bad_blocks, count * sizeof(uft_bad_block_t));
    return count;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Progress Reporting
 * ═══════════════════════════════════════════════════════════════════════════════ */

static bool report_progress(uft_recovery_t *rec, const char *status)
{
    if (!rec->config.progress_cb) return true;
    
    double elapsed = get_time_sec() - rec->stats.elapsed_seconds;
    
    uft_recovery_progress_t prog = {
        .bytes_total = rec->stats.bytes_total,
        .bytes_processed = rec->stats.bytes_read,
        .bytes_good = rec->stats.bytes_good,
        .bytes_bad = rec->stats.bytes_bad,
        .current_position = rec->current_position,
        .sectors_total = (int)rec->stats.sectors_total,
        .sectors_good = (int)rec->stats.sectors_good,
        .sectors_bad = (int)rec->stats.sectors_bad,
        .sectors_recovered = (int)rec->stats.sectors_recovered,
        .current_block_size = rec->current_block_size,
        .status_text = status
    };
    
    if (elapsed > 0) {
        prog.speed_mbps = (float)(rec->stats.bytes_read / (1024.0 * 1024.0) / elapsed);
        uint64_t remaining = rec->stats.bytes_total - rec->stats.bytes_read;
        prog.eta_seconds = (float)(remaining / (rec->stats.bytes_read / elapsed));
    }
    
    return rec->config.progress_cb(&prog, rec->config.user_data);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Low-Level I/O
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifdef _WIN32

static HANDLE open_device(const char *path, bool write)
{
    DWORD access = GENERIC_READ;
    DWORD share = FILE_SHARE_READ;
    if (write) {
        access |= GENERIC_WRITE;
        share |= FILE_SHARE_WRITE;
    }
    
    return CreateFileA(path, access, share, NULL, OPEN_EXISTING,
                       FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, NULL);
}

static void close_device(HANDLE h)
{
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
}

static int64_t device_size(HANDLE h)
{
    LARGE_INTEGER size;
    if (GetFileSizeEx(h, &size)) {
        return size.QuadPart;
    }
    return -1;
}

static int read_at(HANDLE h, uint64_t offset, void *buf, size_t size, size_t *bytes_read)
{
    LARGE_INTEGER li;
    li.QuadPart = offset;
    
    if (!SetFilePointerEx(h, li, NULL, FILE_BEGIN)) {
        return -1;
    }
    
    DWORD read = 0;
    if (!ReadFile(h, buf, (DWORD)size, &read, NULL)) {
        if (bytes_read) *bytes_read = read;
        return -1;
    }
    
    if (bytes_read) *bytes_read = read;
    return 0;
}

static int write_at(HANDLE h, uint64_t offset, const void *buf, size_t size)
{
    LARGE_INTEGER li;
    li.QuadPart = offset;
    
    if (!SetFilePointerEx(h, li, NULL, FILE_BEGIN)) {
        return -1;
    }
    
    DWORD written = 0;
    if (!WriteFile(h, buf, (DWORD)size, &written, NULL)) {
        return -1;
    }
    
    return (written == size) ? 0 : -1;
}

#else

static int open_device(const char *path, bool write)
{
    int flags = write ? O_RDWR : O_RDONLY;
#ifdef O_DIRECT
    flags |= O_DIRECT;
#endif
    return open(path, flags);
}

static void close_device(int fd)
{
    if (fd >= 0) close(fd);
}

static int64_t device_size(int fd)
{
    struct stat st;
    if (fstat(fd, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

static int read_at(int fd, uint64_t offset, void *buf, size_t size, size_t *bytes_read)
{
    if (lseek(fd, offset, SEEK_SET) < 0) {
        return -1;
    }
    
    ssize_t r = read(fd, buf, size);
    if (r < 0) {
        if (bytes_read) *bytes_read = 0;
        return -1;
    }
    
    if (bytes_read) *bytes_read = r;
    return 0;
}

static int write_at(int fd, uint64_t offset, const void *buf, size_t size)
{
    if (lseek(fd, offset, SEEK_SET) < 0) {
        return -1;
    }
    
    ssize_t w = write(fd, buf, size);
    return (w == (ssize_t)size) ? 0 : -1;
}

#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Sector Reading with Retries
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_sector_status_t uft_recovery_read_sector(
    uft_recovery_t *rec,
    int fd,
    uint64_t offset,
    size_t size,
    uint8_t *buffer,
    size_t *bytes_read)
{
    if (!rec || fd < 0 || !buffer) {
        if (bytes_read) *bytes_read = 0;
        return UFT_SECTOR_BAD;
    }
    
    size_t total_read = 0;
    int attempts = 0;
    uft_recovery_error_t last_error = UFT_REC_ERR_NONE;
    (void)last_error; /* Suppress unused warning */
    
    while (attempts < rec->config.max_retries) {
        attempts++;
        rec->stats.total_retries++;
        
        size_t got = 0;
        int result = read_at(fd, offset, buffer, size, &got);
        
        if (result == 0 && got == size) {
            /* Success */
            if (bytes_read) *bytes_read = size;
            if (attempts > 1) {
                return UFT_SECTOR_RECOVERED;
            }
            return UFT_SECTOR_GOOD;
        }
        
        if (got > total_read) {
            total_read = got;
        }
        
        last_error = UFT_REC_ERR_READ;
        rec->stats.read_errors++;
        
        /* Small delay before retry */
        #ifdef _WIN32
        Sleep(10);
        #else
        usleep(10000);
        #endif
    }
    
    /* Failed after all retries */
    if (bytes_read) *bytes_read = total_read;
    
    if (total_read > 0 && rec->config.preserve_partial) {
        return UFT_SECTOR_PARTIAL;
    }
    
    return UFT_SECTOR_BAD;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Recovery Core
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_recovery_run(uft_recovery_t *rec,
                     const char *device,
                     const char *output)
{
    if (!rec || !device || !output) return -1;
    
    uft_recovery_reset(rec);
    
    double start = get_time_sec();
    
    /* Open source */
    #ifdef _WIN32
    HANDLE src = open_device(device, false);
    if (src == INVALID_HANDLE_VALUE) return -1;
    #else
    int src = open_device(device, false);
    if (src < 0) return -1;
    #endif
    
    int64_t total_size = device_size(src);
    if (total_size <= 0) {
        close_device(src);
        return -1;
    }
    
    /* Create output */
    FILE *dst = fopen(output, "wb");
    if (!dst) {
        close_device(src);
        return -1;
    }
    
    /* Pre-allocate output file */
    if (fseek(dst, total_size - 1, SEEK_SET) != 0) { /* seek error */ }
    fputc(0, dst);
    if (fseek(dst, 0, SEEK_SET) != 0) { /* seek error */ }
    rec->stats.bytes_total = total_size;
    rec->stats.sectors_total = total_size / 512;
    
    /* Allocate buffer */
    uint8_t *buffer = (uint8_t*)malloc(rec->config.max_block_size);
    if (!buffer) {
        fclose(dst);
        close_device(src);
        return -1;
    }
    
    /* Recovery loop */
    uint64_t pos = 0;
    size_t block_size = rec->config.initial_block_size;
    int consecutive_errors = 0;
    
    while (pos < (uint64_t)total_size && !rec->aborted) {
        rec->current_position = pos;
        rec->current_block_size = block_size;
        
        /* Calculate actual read size */
        size_t read_size = block_size;
        if (pos + read_size > (uint64_t)total_size) {
            read_size = total_size - pos;
        }
        
        /* Read block */
        size_t got = 0;
        uft_sector_status_t status = uft_recovery_read_sector(
            rec, src, pos, read_size, buffer, &got);
        
        /* Write what we got */
        if (got > 0) {
            if (fseek(dst, pos, SEEK_SET) != 0) { /* seek error */ }
            if (fwrite(buffer, 1, got, dst) != got) { /* I/O error */ }
        }
        
        /* Fill remaining with pattern if needed */
        if (got < read_size && rec->config.fill_bad_sectors) {
            memset(buffer, rec->config.bad_sector_fill, read_size - got);
            if (fseek(dst, pos + got, SEEK_SET) != 0) { /* seek error */ }
            if (fwrite(buffer, 1, read_size - got, dst) != read_size - got) { /* I/O error */ }
        }
        
        /* Update statistics */
        rec->stats.bytes_read += read_size;
        
        switch (status) {
            case UFT_SECTOR_GOOD:
                rec->stats.bytes_good += read_size;
                rec->stats.sectors_good += read_size / 512;
                consecutive_errors = 0;
                
                /* Increase block size on success (adaptive) */
                if (rec->config.strategy == UFT_REC_STRATEGY_ADAPTIVE) {
                    if (block_size < rec->config.max_block_size) {
                        block_size *= 2;
                        if (block_size > rec->config.max_block_size) {
                            block_size = rec->config.max_block_size;
                        }
                    }
                }
                break;
                
            case UFT_SECTOR_RECOVERED:
                rec->stats.bytes_good += read_size;
                rec->stats.sectors_recovered += read_size / 512;
                consecutive_errors = 0;
                break;
                
            case UFT_SECTOR_PARTIAL:
                rec->stats.bytes_good += got;
                rec->stats.bytes_bad += read_size - got;
                rec->stats.sectors_bad += (read_size - got) / 512;
                add_bad_block(rec, pos + got, read_size - got, 
                             UFT_REC_ERR_READ, rec->config.max_retries,
                             UFT_SECTOR_PARTIAL);
                consecutive_errors++;
                break;
                
            case UFT_SECTOR_BAD:
                rec->stats.bytes_bad += read_size;
                rec->stats.sectors_bad += read_size / 512;
                add_bad_block(rec, pos, read_size,
                             UFT_REC_ERR_READ, rec->config.max_retries,
                             UFT_SECTOR_BAD);
                consecutive_errors++;
                
                /* Reduce block size on error (adaptive) */
                if (rec->config.strategy == UFT_REC_STRATEGY_ADAPTIVE) {
                    if (block_size > rec->config.min_block_size) {
                        block_size /= 2;
                        if (block_size < rec->config.min_block_size) {
                            block_size = rec->config.min_block_size;
                        }
                    }
                }
                break;
                
            default:
                break;
        }
        
        /* Skip ahead on too many consecutive errors */
        if (consecutive_errors >= rec->config.max_skip_retries) {
            uint64_t skip = rec->config.skip_size;
            if (skip > rec->config.max_skip_size) {
                skip = rec->config.max_skip_size;
            }
            
            rec->stats.bytes_skipped += skip;
            rec->stats.sectors_skipped += skip / 512;
            pos += skip;
            consecutive_errors = 0;
            
            add_bad_block(rec, pos, skip, UFT_REC_ERR_READ, 0, UFT_SECTOR_SKIPPED);
        } else {
            pos += read_size;
        }
        
        /* Progress callback */
        if (!report_progress(rec, "Reading...")) {
            rec->aborted = true;
        }
    }
    
    /* Cleanup */
    free(buffer);
    fclose(dst);
    close_device(src);
    
    rec->stats.elapsed_seconds = get_time_sec() - start;
    if (rec->stats.elapsed_seconds > 0) {
        rec->stats.avg_speed_mbps = (rec->stats.bytes_read / (1024.0 * 1024.0)) / 
                                     rec->stats.elapsed_seconds;
    }
    
    return rec->aborted ? -1 : 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Multi-Pass Recovery
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_recovery_multi_pass(uft_recovery_t *rec,
                            const char *device,
                            const char *output)
{
    if (!rec || !device || !output) return -1;
    
    /* First pass: fast read */
    uft_recovery_config_t saved = rec->config;
    
    rec->config.max_retries = 1;
    rec->config.initial_block_size = 1024 * 1024;  /* 1 MB */
    rec->config.strategy = UFT_REC_STRATEGY_LINEAR;
    
    int result = uft_recovery_run(rec, device, output);
    
    /* If we have bad blocks, do second pass */
    if (result == 0 && rec->bad_block_count > 0) {
        rec->config.max_retries = 3;
        rec->config.initial_block_size = 4096;
        rec->config.strategy = UFT_REC_STRATEGY_ADAPTIVE;
        
        /* Re-read bad regions */
        /* (In a full implementation, we'd re-read only bad blocks) */
    }
    
    rec->config = saved;
    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Statistics & Reporting
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_recovery_get_stats(const uft_recovery_t *rec,
                            uft_recovery_stats_t *stats)
{
    if (!rec || !stats) return;
    *stats = rec->stats;
}

void uft_recovery_print_summary(const uft_recovery_t *rec)
{
    if (!rec) return;
    
    printf("\n=== Recovery Summary ===\n");
    printf("Total:     %llu bytes\n", (unsigned long long)rec->stats.bytes_total);
    printf("Read:      %llu bytes\n", (unsigned long long)rec->stats.bytes_read);
    printf("Good:      %llu bytes (%.1f%%)\n", 
           (unsigned long long)rec->stats.bytes_good,
           100.0 * rec->stats.bytes_good / rec->stats.bytes_total);
    printf("Bad:       %llu bytes\n", (unsigned long long)rec->stats.bytes_bad);
    printf("Skipped:   %llu bytes\n", (unsigned long long)rec->stats.bytes_skipped);
    printf("Bad blocks: %d\n", rec->stats.bad_block_count);
    printf("Retries:   %d\n", rec->stats.total_retries);
    printf("Time:      %.1f seconds\n", rec->stats.elapsed_seconds);
    printf("Speed:     %.1f MB/s\n", rec->stats.avg_speed_mbps);
    printf("========================\n\n");
}

int uft_recovery_report(const uft_recovery_t *rec, const char *report_file)
{
    if (!rec || !report_file) return -1;
    
    FILE *f = fopen(report_file, "w");
    if (!f) return -1;
    
    fprintf(f, "UFT Recovery Report\n");
    fprintf(f, "==================\n\n");
    fprintf(f, "Total bytes:     %llu\n", (unsigned long long)rec->stats.bytes_total);
    fprintf(f, "Bytes read:      %llu\n", (unsigned long long)rec->stats.bytes_read);
    fprintf(f, "Bytes good:      %llu\n", (unsigned long long)rec->stats.bytes_good);
    fprintf(f, "Bytes bad:       %llu\n", (unsigned long long)rec->stats.bytes_bad);
    fprintf(f, "Recovery rate:   %.2f%%\n", 
            100.0 * rec->stats.bytes_good / rec->stats.bytes_total);
    fprintf(f, "\nBad Blocks: %d\n", rec->bad_block_count);
    fprintf(f, "-----------------\n");
    
    for (int i = 0; i < rec->bad_block_count; i++) {
        const uft_bad_block_t *bb = &rec->bad_blocks[i];
        fprintf(f, "  0x%llX - 0x%llX (%llu bytes) - %s\n",
                (unsigned long long)bb->offset,
                (unsigned long long)(bb->offset + bb->length),
                (unsigned long long)bb->length,
                uft_recovery_status_string(bb->status));
    }
    
    fclose(f);
    return 0;
}

void uft_recovery_abort(uft_recovery_t *rec)
{
    if (rec) rec->aborted = true;
}
