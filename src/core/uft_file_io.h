#ifndef UFT_FILE_IO_H
#define UFT_FILE_IO_H
/**
 * @file uft_file_io.h
 * @brief Safe File I/O Functions for UFT
 * 
 * Wrapper functions with proper error handling and return value checking.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include "uft_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * File Open/Close
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Enable/disable path security checks
 * @param enabled 1 to enable (default), 0 to disable
 * 
 * When enabled, uft_file_open() will reject paths containing ".."
 * Disable only for trusted paths where traversal is intentional.
 */
void uft_file_set_security(int enabled);

/**
 * @brief Get current security check status
 * @return 1 if enabled, 0 if disabled
 */
int uft_file_get_security(void);

/**
 * @brief Safe file open with error handling
 * @param path File path
 * @param mode Open mode ("rb", "wb", "r+b", etc.)
 * @param out_file Output file handle
 * @return UFT_OK on success, error code on failure
 * 
 * @note If security is enabled, paths with ".." will be rejected
 */
uft_error_t uft_file_open(const char *path, const char *mode, FILE **out_file);

/**
 * @brief Safe file open with base directory constraint
 * @param path File path
 * @param mode Open mode
 * @param base_dir Base directory (path must resolve within this)
 * @param out_file Output file handle
 * @return UFT_OK, UFT_ERROR_SECURITY, or other error
 * 
 * Use this function when opening user-provided paths to ensure
 * they cannot escape a designated directory.
 */
uft_error_t uft_file_open_safe(const char *path, const char *mode, 
                               const char *base_dir, FILE **out_file);

/**
 * @brief Safe file close
 * @param f File handle (may be NULL)
 */
void uft_file_close(FILE *f);

/* ═══════════════════════════════════════════════════════════════════════════
 * Read Operations
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read exact number of bytes
 * @param f File handle
 * @param buf Destination buffer
 * @param size Number of bytes to read
 * @return UFT_OK, UFT_ERROR_FILE_TRUNCATED, or UFT_ERROR_FILE_READ
 */
uft_error_t uft_file_read(FILE *f, void *buf, size_t size);

/**
 * @brief Read up to size bytes (partial read OK)
 * @param f File handle
 * @param buf Destination buffer
 * @param size Maximum bytes to read
 * @param out_read Actual bytes read
 * @return UFT_OK or UFT_ERROR_FILE_READ
 */
uft_error_t uft_file_read_partial(FILE *f, void *buf, size_t size, size_t *out_read);

/**
 * @brief Read entire file into newly allocated buffer
 * @param path File path
 * @param out_data Output data (must be freed)
 * @param out_size Output size
 * @return UFT_OK or error code
 */
uft_error_t uft_file_read_all(const char *path, uint8_t **out_data, size_t *out_size);

/* ═══════════════════════════════════════════════════════════════════════════
 * Write Operations
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Write exact number of bytes
 * @param f File handle
 * @param buf Source buffer
 * @param size Number of bytes to write
 * @return UFT_OK or UFT_ERROR_FILE_WRITE
 */
uft_error_t uft_file_write(FILE *f, const void *buf, size_t size);

/**
 * @brief Write buffer to file (creates/overwrites)
 * @param path File path
 * @param data Data buffer
 * @param size Data size
 * @return UFT_OK or error code
 */
uft_error_t uft_file_write_all(const char *path, const void *data, size_t size);

/* ═══════════════════════════════════════════════════════════════════════════
 * Seek/Position
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Seek to position
 * @param f File handle
 * @param offset Offset
 * @param whence SEEK_SET, SEEK_CUR, or SEEK_END
 * @return UFT_OK or UFT_ERROR_FILE_SEEK
 */
uft_error_t uft_file_seek(FILE *f, long offset, int whence);

/**
 * @brief Get current position
 * @param f File handle
 * @param out_pos Output position
 * @return UFT_OK or UFT_ERROR_FILE_SEEK
 */
uft_error_t uft_file_tell(FILE *f, long *out_pos);

/**
 * @brief Get file size
 * @param f File handle
 * @param out_size Output size
 * @return UFT_OK or error code
 */
uft_error_t uft_file_size(FILE *f, size_t *out_size);

/* ═══════════════════════════════════════════════════════════════════════════
 * Utility
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Check if file exists
 * @param path File path
 * @return 1 if exists, 0 if not
 */
int uft_file_exists(const char *path);

/**
 * @brief Get file size by path
 * @param path File path
 * @param out_size Output size
 * @return UFT_OK or error code
 */
uft_error_t uft_file_size_path(const char *path, size_t *out_size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FILE_IO_H */
