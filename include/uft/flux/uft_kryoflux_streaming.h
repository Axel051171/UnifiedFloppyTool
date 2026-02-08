/**
 * @file uft_uft_kf_streaming.h
 * 
 * P1-002: Original implementation allocated 8MB per track
 *         This streaming API reduces peak memory to ~2MB
 * 
 * Key improvements:
 * - Streaming chunk-based processing
 * - Reusable buffer pool
 * - Incremental decoding
 * - Memory-mapped file support
 */

#ifndef UFT_UFT_UFT_KF_STREAMING_H
#define UFT_UFT_UFT_KF_STREAMING_H

#include "uft/core/uft_unified_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Constants */
#define UFT_KF_CHUNK_SIZE       65536   /* 64KB chunks */
#define UFT_KF_MAX_BUFFER_SIZE  (2 * 1024 * 1024)  /* 2MB max */
#define UFT_KF_POOL_BUFFERS     4       /* Number of pooled buffers */

#define UFT_KF_OOB_INVALID      0x00
#define UFT_KF_OOB_STREAM_INFO  0x01
#define UFT_KF_OOB_INDEX        0x02
#define UFT_KF_OOB_STREAM_END   0x03
#define UFT_KF_OOB_UFT_KF_INFO      0x04
#define UFT_KF_OOB_EOF          0x0D

/**
 */
typedef struct {
    uint8_t stream_position;
    uint8_t transfer_time_ms;
} uft_kf_stream_info_t;

/**
 */
typedef struct {
    uint32_t stream_position;
    uint32_t sample_counter;
    uint32_t index_counter;
} uft_kf_index_info_t;

/**
 * @brief Buffer pool entry
 */
typedef struct {
    uint8_t *data;
    size_t size;
    size_t used;
    bool in_use;
} uft_kf_buffer_t;

/**
 * @brief Streaming context
 */
typedef struct {
    /* File handling */
    int fd;                    /* File descriptor (for mmap) */
    const uint8_t *mmap_base;  /* Memory-mapped base */
    size_t mmap_size;          /* Mapped size */
    size_t file_pos;           /* Current position */
    
    /* Chunk processing */
    uint8_t chunk[UFT_KF_CHUNK_SIZE];
    size_t chunk_pos;
    size_t chunk_size;
    
    /* Buffer pool */
    uft_kf_buffer_t pool[UFT_KF_POOL_BUFFERS];
    
    /* Flux accumulator */
    uint32_t *flux_times;      /* Flux transition times */
    size_t flux_count;
    size_t flux_capacity;
    
    /* Index tracking */
    uft_kf_index_info_t indices[8];
    size_t index_count;
    
    /* Statistics */
    uint32_t sample_clock;     /* Sample clock frequency */
    uint32_t index_clock;      /* Index clock frequency */
    size_t bytes_processed;
    size_t overflow_count;
    
    /* State */
    bool initialized;
    bool eof_reached;
    uft_error_t error;
    
} uft_kf_stream_ctx_t;

/**
 * @brief Chunk callback for streaming decode
 * @param flux_data Flux transition array
 * @param flux_count Number of transitions
 * @param chunk_index Chunk number
 * @param user_data User data
 * @return 0 to continue, non-zero to stop
 */
typedef int (*uft_kf_chunk_callback_t)(const uint32_t *flux_data,
                                   size_t flux_count,
                                   int chunk_index,
                                   void *user_data);

/**
 * @brief Track callback for streaming decode
 */
typedef int (*uft_kf_track_callback_t)(const uft_track_t *track,
                                   void *user_data);

/* ============================================================================
 * Context Management
 * ============================================================================ */

/**
 * @brief Initialize streaming context
 */
uft_error_t uft_kf_stream_init(uft_kf_stream_ctx_t *ctx);

/**
 * @brief Free streaming context
 */
void uft_kf_stream_free(uft_kf_stream_ctx_t *ctx);

/**
 * @brief Reset context for new file
 */
void uft_kf_stream_reset(uft_kf_stream_ctx_t *ctx);

/* ============================================================================
 * Buffer Pool
 * ============================================================================ */

/**
 * @brief Acquire buffer from pool
 */
uft_kf_buffer_t* uft_kf_pool_acquire(uft_kf_stream_ctx_t *ctx, size_t min_size);

/**
 * @brief Release buffer back to pool
 */
void uft_kf_pool_release(uft_kf_stream_ctx_t *ctx, uft_kf_buffer_t *buf);

/**
 * @brief Get pool memory usage
 */
size_t uft_kf_pool_memory_usage(const uft_kf_stream_ctx_t *ctx);

/* ============================================================================
 * File I/O
 * ============================================================================ */

/**
 */
uft_error_t uft_kf_stream_open(uft_kf_stream_ctx_t *ctx, const char *path);

/**
 * @brief Open from file descriptor
 */
uft_error_t uft_kf_stream_open_fd(uft_kf_stream_ctx_t *ctx, int fd);

/**
 * @brief Open from memory buffer
 */
uft_error_t uft_kf_stream_open_mem(uft_kf_stream_ctx_t *ctx,
                               const uint8_t *data, size_t size);

/**
 * @brief Close stream
 */
void uft_kf_stream_close(uft_kf_stream_ctx_t *ctx);

/* ============================================================================
 * Streaming Decode
 * ============================================================================ */

/**
 * @brief Read next chunk
 * @return Number of bytes read, 0 on EOF, -1 on error
 */
int uft_kf_stream_read_chunk(uft_kf_stream_ctx_t *ctx);

/**
 * @brief Process chunk and extract flux data
 */
uft_error_t uft_kf_stream_process_chunk(uft_kf_stream_ctx_t *ctx);

/**
 * @brief Decode complete track with streaming
 * @param ctx Streaming context
 * @param callback Called for each chunk
 * @param user_data User data for callback
 * @return UFT_OK on success
 */
uft_error_t uft_kf_stream_decode_track(uft_kf_stream_ctx_t *ctx,
                                   uft_kf_chunk_callback_t callback,
                                   void *user_data);

/**
 * @brief Decode track to uft_track_t
 */
uft_error_t uft_kf_stream_to_track(uft_kf_stream_ctx_t *ctx,
                               uft_track_t *out_track);

/* ============================================================================
 * Multi-Track Streaming
 * ============================================================================ */

/**
 * @brief Decode entire disk with streaming
 * @param callback Called for each track
 * @param user_data User data
 * @return UFT_OK on success
 */
uft_error_t uft_kf_stream_decode_disk(const char *directory,
                                  uft_kf_track_callback_t callback,
                                  void *user_data);

/**
 * @brief Stream disk to uft_disk_image_t
 */
uft_error_t uft_kf_stream_to_disk(const char *directory,
                              uft_disk_image_t **out_disk);

/* ============================================================================
 * Utilities
 * ============================================================================ */

/**
 * @brief Get stream info
 */
uft_error_t uft_kf_stream_get_info(const uft_kf_stream_ctx_t *ctx,
                               uft_kf_stream_info_t *out_info);

/**
 * @brief Get index positions
 */
uft_error_t uft_kf_stream_get_indices(const uft_kf_stream_ctx_t *ctx,
                                  uft_kf_index_info_t *indices,
                                  size_t *count);

/**
 * @brief Convert sample counter to nanoseconds
 */
double uft_kf_samples_to_ns(const uft_kf_stream_ctx_t *ctx, uint32_t samples);

/**
 * @brief Get memory statistics
 */
typedef struct {
    size_t peak_memory;
    size_t current_memory;
    size_t pool_memory;
    size_t flux_memory;
    size_t chunk_count;
} uft_kf_memory_stats_t;

void uft_kf_stream_get_memory_stats(const uft_kf_stream_ctx_t *ctx,
                                uft_kf_memory_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* UFT_UFT_UFT_KF_STREAMING_H */
