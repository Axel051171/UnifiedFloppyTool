// SPDX-License-Identifier: MIT
/*
 * disk_io.c - Disk I/O Helpers (inspired by GNU dd)
 * 
 * Robust block-based I/O for raw disk access.
 * Extracted best practices from GNU coreutils dd.
 * 
 * @version 2.8.1
 * @date 2024-12-26
 */

#include "uft/uft_error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "disk_io.h"

/*============================================================================*
 * CONSTANTS
 *============================================================================*/

#define DEFAULT_BLOCK_SIZE  512
#define MAX_RETRIES         3
#define RETRY_DELAY_MS      100

/*============================================================================*
 * ERROR HANDLING
 *============================================================================*/

/**
 * @brief Print error with retry info
 */
static void print_retry_error(const char *op, int attempt, int max_attempts)
{
    fprintf(stderr, "disk_io: %s failed (attempt %d/%d): %s\n",
            op, attempt, max_attempts, strerror(errno));
}

/*============================================================================*
 * BLOCK I/O OPERATIONS
 *============================================================================*/

/**
 * @brief Read blocks from device with retry
 * 
 * Implements robust read with automatic retry on failure.
 * Similar to dd conv=noerror with retry logic.
 */
ssize_t disk_read_blocks(int fd, void *buf, size_t block_count, size_t block_size)
{
    if (fd < 0 || !buf || block_count == 0) {
        errno = EINVAL;
        return -1;
    }
    
    if (block_size == 0) {
        block_size = DEFAULT_BLOCK_SIZE;
    }
    
    size_t total_size = block_count * block_size;
    size_t bytes_read = 0;
    
    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        ssize_t n = read(fd, (char*)buf + bytes_read, total_size - bytes_read);
        
        if (n > 0) {
            bytes_read += n;
            if (bytes_read >= total_size) {
                return bytes_read;  /* Success */
            }
            /* Partial read - continue */
            continue;
        }
        
        if (n == 0) {
            /* EOF */
            return bytes_read;
        }
        
        /* Error */
        if (errno == EINTR) {
            /* Interrupted - retry immediately */
            continue;
        }
        
        if (attempt < MAX_RETRIES) {
            print_retry_error("read", attempt, MAX_RETRIES);
            usleep(RETRY_DELAY_MS * 1000);
        } else {
            /* Final attempt failed */
            return -1;
        }
    }
    
    return bytes_read;
}

/**
 * @brief Write blocks to device with retry
 */
ssize_t disk_write_blocks(int fd, const void *buf, size_t block_count, size_t block_size)
{
    if (fd < 0 || !buf || block_count == 0) {
        errno = EINVAL;
        return -1;
    }
    
    if (block_size == 0) {
        block_size = DEFAULT_BLOCK_SIZE;
    }
    
    size_t total_size = block_count * block_size;
    size_t bytes_written = 0;
    
    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        ssize_t n = write(fd, (const char*)buf + bytes_written, 
                         total_size - bytes_written);
        
        if (n > 0) {
            bytes_written += n;
            if (bytes_written >= total_size) {
                return bytes_written;  /* Success */
            }
            /* Partial write - continue */
            continue;
        }
        
        /* Error */
        if (errno == EINTR) {
            continue;
        }
        
        if (attempt < MAX_RETRIES) {
            print_retry_error("write", attempt, MAX_RETRIES);
            usleep(RETRY_DELAY_MS * 1000);
        } else {
            return -1;
        }
    }
    
    return bytes_written;
}

/*============================================================================*
 * DEVICE OPERATIONS
 *============================================================================*/

/**
 * @brief Open device for reading
 */
int disk_open_read(const char *path, int flags)
{
    if (!path) {
        errno = EINVAL;
        return -1;
    }
    
    int fd = open(path, O_RDONLY | flags);
    if (fd < 0) {
        return -1;
    }
    
    return fd;
}

/**
 * @brief Open device for writing
 */
int disk_open_write(const char *path, int flags)
{
    if (!path) {
        errno = EINVAL;
        return -1;
    }
    
    int fd = open(path, O_WRONLY | O_CREAT | flags, 0644);
    if (fd < 0) {
        return -1;
    }
    
    return fd;
}

/**
 * @brief Get device size
 */
off_t disk_get_size(int fd)
{
    if (fd < 0) {
        errno = EINVAL;
        return -1;
    }
    
    struct stat st;
    if (fstat(fd, &st) < 0) {
        return -1;
    }
    
    return st.st_size;
}

/**
 * @brief Seek to block
 */
int disk_seek_block(int fd, off_t block_num, size_t block_size)
{
    if (fd < 0) {
        errno = EINVAL;
        return -1;
    }
    
    if (block_size == 0) {
        block_size = DEFAULT_BLOCK_SIZE;
    }
    
    off_t offset = block_num * block_size;
    if (lseek(fd, offset, SEEK_SET) < 0) {
        return -1;
    }
    
    return 0;
}

/*============================================================================*
 * BUFFER OPERATIONS
 *============================================================================*/

/**
 * @brief Allocate aligned buffer
 * 
 * Allocates buffer aligned for optimal I/O performance.
 */
void* disk_alloc_buffer(size_t size)
{
    if (size == 0) {
        errno = EINVAL;
        return NULL;
    }
    
    /* Align to 4096 for optimal I/O */
    void *buf = NULL;
    
#ifdef _POSIX_C_SOURCE
    if (posix_memalign(&buf, 4096, size) != 0) {
        return NULL;
    }
#else
    buf = malloc(size);
#endif
    
    return buf;
}

/**
 * @brief Free aligned buffer
 */
void disk_free_buffer(void *buf)
{
    free(buf);
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
