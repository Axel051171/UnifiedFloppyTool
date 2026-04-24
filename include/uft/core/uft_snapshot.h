/**
 * @file uft_snapshot.h
 * @brief Recovery Snapshot System
 * @version 1.0.0
 * 
 * Pre-write backup system with SHA-256 verification.
 * "Bei uns geht kein Bit verloren" - even on failed writes.
 */

#ifndef UFT_SNAPSHOT_H
#define UFT_SNAPSHOT_H

#include <stdint.h>
#include <stddef.h>
#include "uft/uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Snapshot metadata
 */
typedef struct {
    char path[512];          /**< Snapshot file path */
    uint8_t sha256[32];      /**< SHA-256 digest */
    uint64_t size_bytes;     /**< Original size */
    uint64_t timestamp_ms;   /**< Creation timestamp */
} uft_snapshot_t;

/**
 * @brief Snapshot options
 */
typedef struct {
    int compress;            /**< Compress snapshot (0=off, 1=gzip) */
    int verify_after_write;  /**< Verify hash after creating */
    int include_metadata;    /**< Include format metadata */
} uft_snapshot_opts_t;

/**
 * @brief Default snapshot options
 */
#define UFT_SNAPSHOT_OPTS_DEFAULT { \
    .compress = 0, \
    .verify_after_write = 1, \
    .include_metadata = 0 \
}

/**
 * @brief Create snapshot from in-memory bytes
 * @param snapshot_dir Directory for snapshot files
 * @param prefix Filename prefix (e.g., "backup")
 * @param buf Data to snapshot
 * @param len Data length
 * @param opts Options (NULL for defaults)
 * @param out Output snapshot info
 * @return UFT_SUCCESS or error code
 */
uft_error_t uft_snapshot_create(const char *snapshot_dir,
                                const char *prefix,
                                const uint8_t *buf,
                                size_t len,
                                const uft_snapshot_opts_t *opts,
                                uft_snapshot_t *out);

/**
 * @brief Create snapshot from file
 * @param snapshot_dir Directory for snapshot files
 * @param prefix Filename prefix
 * @param src_path Source file to backup
 * @param opts Options (NULL for defaults)
 * @param out Output snapshot info
 * @return UFT_SUCCESS or error code
 */
uft_error_t uft_snapshot_create_from_file(const char *snapshot_dir,
                                          const char *prefix,
                                          const char *src_path,
                                          const uft_snapshot_opts_t *opts,
                                          uft_snapshot_t *out);

/**
 * @brief Verify snapshot integrity
 * @param snapshot Snapshot to verify
 * @return UFT_SUCCESS if valid, UFT_ERR_CRC if corrupted
 */
uft_error_t uft_snapshot_verify(const uft_snapshot_t *snapshot);

/**
 * @brief Restore snapshot to buffer
 * @param snapshot Snapshot to restore
 * @param buf Output buffer (must be snapshot->size_bytes)
 * @param buf_size Buffer size
 * @return UFT_SUCCESS or error code
 */
uft_error_t uft_snapshot_restore(const uft_snapshot_t *snapshot,
                                 uint8_t *buf,
                                 size_t buf_size);

/**
 * @brief Restore snapshot to file
 * @param snapshot Snapshot to restore
 * @param dest_path Destination file
 * @return UFT_SUCCESS or error code
 */
uft_error_t uft_snapshot_restore_to_file(const uft_snapshot_t *snapshot,
                                         const char *dest_path);

/**
 * @brief Delete snapshot file
 * @param snapshot Snapshot to delete
 * @return UFT_SUCCESS or error code
 */
uft_error_t uft_snapshot_delete(const uft_snapshot_t *snapshot);

/**
 * @brief Get human-readable hash string
 * @param snapshot Snapshot
 * @param out Output buffer (65 bytes minimum)
 */
void uft_snapshot_get_hash_str(const uft_snapshot_t *snapshot, char out[65]);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SNAPSHOT_H */
