// SPDX-License-Identifier: MIT
/*
 * disk_io.h - Disk I/O Helpers Header
 * 
 * @version 2.8.1
 * @date 2024-12-26
 */

#ifndef DISK_IO_H
#define DISK_IO_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * BLOCK I/O
 *============================================================================*/

/**
 * @brief Read blocks with retry
 * 
 * @param fd File descriptor
 * @param buf Buffer
 * @param block_count Number of blocks
 * @param block_size Block size (0 = default 512)
 * @return Bytes read or -1 on error
 */
ssize_t disk_read_blocks(int fd, void *buf, size_t block_count, size_t block_size);

/**
 * @brief Write blocks with retry
 * 
 * @param fd File descriptor
 * @param buf Buffer
 * @param block_count Number of blocks
 * @param block_size Block size (0 = default 512)
 * @return Bytes written or -1 on error
 */
ssize_t disk_write_blocks(int fd, const void *buf, size_t block_count, size_t block_size);

/*============================================================================*
 * DEVICE OPERATIONS
 *============================================================================*/

/**
 * @brief Open device for reading
 * 
 * @param path Device path
 * @param flags Additional flags
 * @return File descriptor or -1
 */
int disk_open_read(const char *path, int flags);

/**
 * @brief Open device for writing
 * 
 * @param path Device path
 * @param flags Additional flags
 * @return File descriptor or -1
 */
int disk_open_write(const char *path, int flags);

/**
 * @brief Get device size
 * 
 * @param fd File descriptor
 * @return Size in bytes or -1
 */
off_t disk_get_size(int fd);

/**
 * @brief Seek to block
 * 
 * @param fd File descriptor
 * @param block_num Block number
 * @param block_size Block size (0 = default 512)
 * @return 0 on success
 */
int disk_seek_block(int fd, off_t block_num, size_t block_size);

/*============================================================================*
 * BUFFER OPERATIONS
 *============================================================================*/

/**
 * @brief Allocate aligned buffer
 * 
 * @param size Buffer size
 * @return Buffer pointer or NULL
 */
void* disk_alloc_buffer(size_t size);

/**
 * @brief Free aligned buffer
 * 
 * @param buf Buffer to free
 */
void disk_free_buffer(void *buf);

#ifdef __cplusplus
}
#endif

#endif /* DISK_IO_H */
