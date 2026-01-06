/**
 * @file uft_stream_copy.h
 * @brief Streaming Disk Copy Pipeline
 * 
 * Inspired by BLITZ Atari ST copy system which performed
 * simultaneous read/write operations for maximum speed.
 * 
 * BLITZ Innovation:
 * - Read from Drive A and Write to Drive B simultaneously
 * - Uses printer port as data bridge
 * - 20 seconds for single-sided, 40 seconds for double-sided
 * 
 * UFT Implementation:
 * - Buffered streaming pipeline
 * - Works with any hardware that supports concurrent R/W
 * - Fallback to sequential mode if streaming not possible
 * 
 * @version 1.0.0
 */

#ifndef UFT_STREAM_COPY_H
#define UFT_STREAM_COPY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Forward Declarations
 *============================================================================*/

/* Hardware abstraction (to be implemented per platform) */
typedef struct uft_flux_reader uft_flux_reader_t;
typedef struct uft_flux_writer uft_flux_writer_t;

/*============================================================================
 * Constants
 *============================================================================*/

/** Default buffer sizes */
#define UFT_STREAM_BUFFER_DEFAULT   (256 * 1024)    /**< 256 KB */
#define UFT_STREAM_BUFFER_MIN       (64 * 1024)     /**< 64 KB minimum */
#define UFT_STREAM_BUFFER_MAX       (4 * 1024 * 1024) /**< 4 MB maximum */

/** Streaming thresholds */
#define UFT_STREAM_WATERMARK_HIGH   75  /**< Pause read at 75% full */
#define UFT_STREAM_WATERMARK_LOW    25  /**< Resume read at 25% full */

/*============================================================================
 * Stream Copy Mode
 *============================================================================*/

/**
 * @brief Streaming copy mode
 */
typedef enum {
    UFT_STREAM_AUTO = 0,        /**< Auto-select based on hardware */
    UFT_STREAM_SIMULTANEOUS,    /**< Simultaneous R/W (BLITZ style) */
    UFT_STREAM_SEQUENTIAL,      /**< Sequential read-then-write */
    UFT_STREAM_PIPELINE         /**< Pipelined (read ahead) */
} uft_stream_mode_t;

/**
 * @brief Stream copy status
 */
typedef enum {
    UFT_STREAM_OK = 0,
    UFT_STREAM_ERROR_INIT,
    UFT_STREAM_ERROR_READ,
    UFT_STREAM_ERROR_WRITE,
    UFT_STREAM_ERROR_SYNC_LOST,
    UFT_STREAM_ERROR_BUFFER_OVERFLOW,
    UFT_STREAM_ERROR_BUFFER_UNDERFLOW,
    UFT_STREAM_ERROR_TIMEOUT,
    UFT_STREAM_ERROR_CANCELLED
} uft_stream_status_t;

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * @brief Stream statistics
 */
typedef struct {
    uint64_t bytes_read;
    uint64_t bytes_written;
    uint32_t tracks_completed;
    uint32_t tracks_total;
    uint32_t read_errors;
    uint32_t write_errors;
    uint32_t retries;
    uint32_t sync_losses;
    double elapsed_ms;
    double throughput_kbs;      /**< KB/s */
} uft_stream_stats_t;

/**
 * @brief Stream buffer
 */
typedef struct {
    uint8_t *data;
    size_t capacity;
    size_t read_pos;            /**< Consumer position */
    size_t write_pos;           /**< Producer position */
    size_t used;                /**< Bytes currently in buffer */
    bool overflow;
    bool underflow;
} uft_stream_buffer_t;

/**
 * @brief Stream copy configuration
 */
typedef struct {
    uft_stream_mode_t mode;
    size_t buffer_size;
    uint8_t watermark_high;     /**< Pause read percentage */
    uint8_t watermark_low;      /**< Resume read percentage */
    uint32_t timeout_ms;        /**< Operation timeout */
    bool verify_after_write;
    bool retry_on_error;
    uint8_t max_retries;
    bool preserve_timing;       /**< Preserve flux timing */
} uft_stream_config_t;

/**
 * @brief Stream copy context
 */
typedef struct {
    /* Hardware */
    uft_flux_reader_t *reader;
    uft_flux_writer_t *writer;
    
    /* Configuration */
    uft_stream_config_t config;
    
    /* State */
    uft_stream_mode_t active_mode;
    uft_stream_status_t status;
    bool running;
    bool cancelled;
    
    /* Current position */
    uint8_t current_track;
    uint8_t current_side;
    uint8_t total_tracks;
    uint8_t total_sides;
    
    /* Buffer */
    uft_stream_buffer_t buffer;
    
    /* Statistics */
    uft_stream_stats_t stats;
    
    /* Timestamps */
    uint64_t start_time_ns;
    uint64_t last_activity_ns;
    
    /* Callbacks */
    void (*progress_cb)(void *ctx, uint8_t track, uint8_t side, int percent);
    void (*error_cb)(void *ctx, uft_stream_status_t status, const char *msg);
    void *callback_ctx;
} uft_stream_copy_t;

/*============================================================================
 * Configuration
 *============================================================================*/

/**
 * @brief Get default stream configuration
 */
static inline uft_stream_config_t uft_stream_config_default(void)
{
    uft_stream_config_t cfg = {
        .mode = UFT_STREAM_AUTO,
        .buffer_size = UFT_STREAM_BUFFER_DEFAULT,
        .watermark_high = UFT_STREAM_WATERMARK_HIGH,
        .watermark_low = UFT_STREAM_WATERMARK_LOW,
        .timeout_ms = 5000,
        .verify_after_write = true,
        .retry_on_error = true,
        .max_retries = 3,
        .preserve_timing = true
    };
    return cfg;
}

/**
 * @brief Get BLITZ-compatible configuration
 * 
 * Optimized for simultaneous read/write like original BLITZ.
 */
static inline uft_stream_config_t uft_stream_config_blitz(void)
{
    uft_stream_config_t cfg = {
        .mode = UFT_STREAM_SIMULTANEOUS,
        .buffer_size = 64 * 1024,  /* Smaller buffer for low latency */
        .watermark_high = 80,
        .watermark_low = 20,
        .timeout_ms = 1000,        /* 1 second sync timeout */
        .verify_after_write = false, /* Speed over safety */
        .retry_on_error = false,
        .max_retries = 0,
        .preserve_timing = true
    };
    return cfg;
}

/*============================================================================
 * Buffer Operations
 *============================================================================*/

/**
 * @brief Initialize stream buffer
 */
static inline int uft_stream_buffer_init(uft_stream_buffer_t *buf, 
                                          size_t capacity)
{
    buf->data = (uint8_t *)malloc(capacity);
    if (!buf->data) return -1;
    
    buf->capacity = capacity;
    buf->read_pos = 0;
    buf->write_pos = 0;
    buf->used = 0;
    buf->overflow = false;
    buf->underflow = false;
    return 0;
}

/**
 * @brief Free stream buffer
 */
static inline void uft_stream_buffer_free(uft_stream_buffer_t *buf)
{
    if (buf->data) {
        free(buf->data);
        buf->data = NULL;
    }
    buf->capacity = 0;
    buf->used = 0;
}

/**
 * @brief Get buffer fill percentage
 */
static inline int uft_stream_buffer_fill_percent(const uft_stream_buffer_t *buf)
{
    if (buf->capacity == 0) return 0;
    return (int)((buf->used * 100) / buf->capacity);
}

/**
 * @brief Check if buffer should pause producer
 */
static inline bool uft_stream_buffer_should_pause(const uft_stream_buffer_t *buf,
                                                   uint8_t watermark_high)
{
    return uft_stream_buffer_fill_percent(buf) >= watermark_high;
}

/**
 * @brief Check if buffer should resume producer
 */
static inline bool uft_stream_buffer_should_resume(const uft_stream_buffer_t *buf,
                                                    uint8_t watermark_low)
{
    return uft_stream_buffer_fill_percent(buf) <= watermark_low;
}

/**
 * @brief Write to buffer (producer side)
 * 
 * @return Number of bytes written
 */
static inline size_t uft_stream_buffer_write(uft_stream_buffer_t *buf,
                                              const uint8_t *data,
                                              size_t size)
{
    size_t available = buf->capacity - buf->used;
    size_t to_write = (size < available) ? size : available;
    
    if (to_write == 0) {
        buf->overflow = true;
        return 0;
    }
    
    /* Copy data (handle wrap-around) */
    size_t first_chunk = buf->capacity - buf->write_pos;
    if (first_chunk >= to_write) {
        memcpy(buf->data + buf->write_pos, data, to_write);
    } else {
        memcpy(buf->data + buf->write_pos, data, first_chunk);
        memcpy(buf->data, data + first_chunk, to_write - first_chunk);
    }
    
    buf->write_pos = (buf->write_pos + to_write) % buf->capacity;
    buf->used += to_write;
    
    return to_write;
}

/**
 * @brief Read from buffer (consumer side)
 * 
 * @return Number of bytes read
 */
static inline size_t uft_stream_buffer_read(uft_stream_buffer_t *buf,
                                             uint8_t *data,
                                             size_t size)
{
    size_t to_read = (size < buf->used) ? size : buf->used;
    
    if (to_read == 0) {
        buf->underflow = true;
        return 0;
    }
    
    /* Copy data (handle wrap-around) */
    size_t first_chunk = buf->capacity - buf->read_pos;
    if (first_chunk >= to_read) {
        memcpy(data, buf->data + buf->read_pos, to_read);
    } else {
        memcpy(data, buf->data + buf->read_pos, first_chunk);
        memcpy(data + first_chunk, buf->data, to_read - first_chunk);
    }
    
    buf->read_pos = (buf->read_pos + to_read) % buf->capacity;
    buf->used -= to_read;
    
    return to_read;
}

/*============================================================================
 * Stream Copy Operations
 *============================================================================*/

/**
 * @brief Initialize stream copy context
 * 
 * @param ctx Context to initialize
 * @param reader Flux reader (source)
 * @param writer Flux writer (destination)
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int uft_stream_copy_init(uft_stream_copy_t *ctx,
                         uft_flux_reader_t *reader,
                         uft_flux_writer_t *writer,
                         const uft_stream_config_t *config);

/**
 * @brief Free stream copy resources
 */
void uft_stream_copy_free(uft_stream_copy_t *ctx);

/**
 * @brief Set progress callback
 */
void uft_stream_copy_set_progress_cb(uft_stream_copy_t *ctx,
                                     void (*cb)(void *ctx, uint8_t track, 
                                               uint8_t side, int percent),
                                     void *user_ctx);

/**
 * @brief Set error callback
 */
void uft_stream_copy_set_error_cb(uft_stream_copy_t *ctx,
                                  void (*cb)(void *ctx, 
                                            uft_stream_status_t status,
                                            const char *msg),
                                  void *user_ctx);

/**
 * @brief Copy entire disk
 * 
 * @param ctx Stream context
 * @param tracks Number of tracks
 * @param sides Number of sides (1 or 2)
 * @return UFT_STREAM_OK on success
 */
uft_stream_status_t uft_stream_copy_disk(uft_stream_copy_t *ctx,
                                         uint8_t tracks,
                                         uint8_t sides);

/**
 * @brief Copy single track
 * 
 * @param ctx Stream context
 * @param track Track number
 * @param side Side number (0 or 1)
 * @return UFT_STREAM_OK on success
 */
uft_stream_status_t uft_stream_copy_track(uft_stream_copy_t *ctx,
                                          uint8_t track,
                                          uint8_t side);

/**
 * @brief Cancel running operation
 */
void uft_stream_copy_cancel(uft_stream_copy_t *ctx);

/**
 * @brief Get current statistics
 */
const uft_stream_stats_t *uft_stream_copy_stats(const uft_stream_copy_t *ctx);

/**
 * @brief Get status string
 */
const char *uft_stream_status_string(uft_stream_status_t status);

/*============================================================================
 * Timing Estimation
 *============================================================================*/

/**
 * @brief Estimate copy time
 * 
 * @param tracks Number of tracks
 * @param sides Number of sides
 * @param mode Copy mode
 * @param rpm Drive RPM
 * @return Estimated time in seconds
 */
static inline double uft_stream_estimate_time(uint8_t tracks, 
                                               uint8_t sides,
                                               uft_stream_mode_t mode,
                                               double rpm)
{
    /* Time per revolution */
    double rev_time = 60.0 / rpm;
    
    /* Revolutions needed per track */
    double revs_per_track = (mode == UFT_STREAM_SIMULTANEOUS) ? 1.0 : 2.0;
    
    /* Head movement time (estimate 5ms per track) */
    double seek_time = 0.005;
    
    /* Total time */
    double total_time = (double)(tracks * sides) * 
                        (rev_time * revs_per_track + seek_time);
    
    /* Add overhead for side changes */
    if (sides > 1) {
        total_time += tracks * 0.01;  /* 10ms per side change */
    }
    
    return total_time;
}

/**
 * @brief Format time estimate as string
 */
static inline void uft_stream_format_time(double seconds, 
                                           char *buffer, 
                                           size_t size)
{
    if (seconds < 60) {
        snprintf(buffer, size, "%.1f seconds", seconds);
    } else {
        int mins = (int)(seconds / 60);
        int secs = (int)(seconds) % 60;
        snprintf(buffer, size, "%d:%02d", mins, secs);
    }
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_STREAM_COPY_H */
