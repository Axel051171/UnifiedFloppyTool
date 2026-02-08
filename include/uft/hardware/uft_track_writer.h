/**
 * @file uft_track_writer.h
 * @brief Track Writer Module for C64 Disk Mastering
 * 
 * Complete disk mastering support for 1541/1571 drives:
 * - Track writing with alignment
 * - Write verification
 * - Motor speed calibration
 * - Killer track generation
 * - Protection-aware writing
 * 
 * Based on nibtools write.c by Pete Rittwage (c64preservation.com)
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#ifndef UFT_TRACK_WRITER_H
#define UFT_TRACK_WRITER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Track buffer size */
#define WRITER_TRACK_SIZE       0x2000

/** Maximum track length at each density */
#define WRITER_CAPACITY_D0      6250    /**< Density 0 (tracks 31-42) */
#define WRITER_CAPACITY_D1      6666    /**< Density 1 (tracks 25-30) */
#define WRITER_CAPACITY_D2      7142    /**< Density 2 (tracks 18-24) */
#define WRITER_CAPACITY_D3      7692    /**< Density 3 (tracks 1-17) */

/** Density constants for RPM calculation */
#define WRITER_DENSITY0_CONST   200000.0f
#define WRITER_DENSITY1_CONST   216666.0f
#define WRITER_DENSITY2_CONST   233333.0f
#define WRITER_DENSITY3_CONST   250000.0f

/** Default verify tolerance */
#define WRITER_VERIFY_TOLERANCE 10

/** Maximum retries on write failure */
#define WRITER_MAX_RETRIES      10

/** Density samples for calibration */
#define WRITER_DENSITY_SAMPLES  5

/** Sync flag bits in density byte */
#define WRITER_BM_NO_SYNC       0x80    /**< Track has no sync marks */
#define WRITER_BM_FF_TRACK      0x40    /**< Track is all sync (killer) */

/* ============================================================================
 * Drive Commands
 * ============================================================================ */

/** Floppy drive commands */
#define WRITER_CMD_WRITE        0x03    /**< Write track */
#define WRITER_CMD_FILLTRACK    0x04    /**< Fill track with byte */
#define WRITER_CMD_READNORMAL   0x00    /**< Read track normal */
#define WRITER_CMD_READWOSYNC   0x01    /**< Read without sync */
#define WRITER_CMD_READIHS      0x02    /**< Read with IHS */
#define WRITER_CMD_ALIGNDISK    0x08    /**< Align disk */

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Write verification result
 */
typedef enum {
    WRITER_VERIFY_OK,           /**< Write verified successfully */
    WRITER_VERIFY_WEAK_OK,      /**< Verified with weak bits */
    WRITER_VERIFY_RETRY,        /**< Needs retry */
    WRITER_VERIFY_FAILED,       /**< Verification failed */
} writer_verify_t;

/**
 * @brief Track write result
 */
typedef struct {
    bool            success;        /**< Write successful */
    int             retries;        /**< Number of retries needed */
    writer_verify_t verify_result;  /**< Verification result */
    size_t          gcr_diff;       /**< GCR difference count */
    size_t          bad_gcr;        /**< Bad GCR bytes detected */
    char            message[128];   /**< Status message */
} track_write_result_t;

/**
 * @brief Motor speed calibration result
 */
typedef struct {
    float           rpm;            /**< Average RPM */
    int             capacity[4];    /**< Capacity at each density */
    int             margin;         /**< Capacity margin */
    bool            valid;          /**< Speed within valid range */
    char            message[128];   /**< Status message */
} motor_calibration_t;

/**
 * @brief Write options
 */
typedef struct {
    bool            verify;         /**< Verify after write */
    bool            raw_mode;       /**< Raw write mode (no processing) */
    bool            backwards;      /**< Write tracks backwards */
    bool            use_ihs;        /**< Use index hole sensor */
    bool            align_disk;     /**< Enable disk alignment */
    int             presync;        /**< Pre-sync bytes to add */
    int             increase_sync;  /**< Increase sync marks */
    int             skew;           /**< Track skew value */
    int             fattrack;       /**< Fat track number (0 = none) */
    uint8_t         fillbyte;       /**< Gap fill byte (0x55 default) */
    int             extra_margin;   /**< Extra capacity margin */
    int             verify_tol;     /**< Verify tolerance */
} write_options_t;

/**
 * @brief Disk mastering session
 */
typedef struct {
    /* Hardware interface */
    void            *hw_context;    /**< Hardware-specific context */
    
    /* Callbacks */
    int (*send_cmd)(void *ctx, uint8_t cmd, const uint8_t *data, size_t len);
    int (*burst_read)(void *ctx);
    int (*burst_write)(void *ctx, uint8_t byte);
    int (*burst_write_track)(void *ctx, const uint8_t *data, size_t len);
    int (*burst_read_track)(void *ctx, uint8_t *data, size_t max_len);
    int (*step_to)(void *ctx, int halftrack);
    int (*set_density)(void *ctx, uint8_t density);
    int (*motor_on)(void *ctx);
    int (*motor_off)(void *ctx);
    int (*track_capacity)(void *ctx);
    
    /* Current state */
    int             current_track;  /**< Current head position */
    uint8_t         current_density;/**< Current density setting */
    
    /* Calibration */
    motor_calibration_t calibration;/**< Motor calibration data */
    bool            calibrated;     /**< Motor calibrated */
    
    /* Options */
    write_options_t options;        /**< Write options */
    
    /* Statistics */
    int             tracks_written; /**< Total tracks written */
    int             errors;         /**< Total errors */
    int             retries;        /**< Total retries */
} writer_session_t;

/**
 * @brief Disk image for mastering
 */
typedef struct {
    uint8_t         *track_data;    /**< Track data buffer */
    uint8_t         *track_density; /**< Density per track */
    size_t          *track_length;  /**< Length per track */
    int             num_tracks;     /**< Number of tracks */
    int             start_track;    /**< First track (halftrack) */
    int             end_track;      /**< Last track (halftrack) */
    bool            has_halftracks; /**< Halftracks present */
} master_image_t;

/* ============================================================================
 * API Functions - Session Management
 * ============================================================================ */

/**
 * @brief Create writer session
 * @return New session, or NULL on error
 */
writer_session_t *writer_create_session(void);

/**
 * @brief Close writer session
 * @param session Session to close
 */
void writer_close_session(writer_session_t *session);

/**
 * @brief Get default write options
 * @param options Output options
 */
void writer_get_defaults(write_options_t *options);

/* ============================================================================
 * API Functions - Calibration
 * ============================================================================ */

/**
 * @brief Calibrate motor speed
 * @param session Writer session
 * @param result Output calibration result
 * @return 0 on success
 */
int writer_calibrate(writer_session_t *session, motor_calibration_t *result);

/**
 * @brief Get track capacity at density
 * @param session Writer session
 * @param density Density (0-3)
 * @return Track capacity, or -1 on error
 */
int writer_get_capacity(const writer_session_t *session, int density);

/**
 * @brief Check if motor speed is valid
 * @param rpm RPM value
 * @return true if valid (280-320 RPM)
 */
bool writer_speed_valid(float rpm);

/* ============================================================================
 * API Functions - Track Writing
 * ============================================================================ */

/**
 * @brief Write single track
 * @param session Writer session
 * @param halftrack Halftrack number (2-84)
 * @param data Track data
 * @param length Track length
 * @param density Track density (0-3) with optional flags
 * @param result Output result (can be NULL)
 * @return 0 on success
 */
int writer_write_track(writer_session_t *session, int halftrack,
                       const uint8_t *data, size_t length,
                       uint8_t density, track_write_result_t *result);

/**
 * @brief Fill track with byte pattern
 * @param session Writer session
 * @param halftrack Halftrack number
 * @param fill_byte Fill byte (0xFF for killer, 0x00 for erase)
 * @return 0 on success
 */
int writer_fill_track(writer_session_t *session, int halftrack, uint8_t fill_byte);

/**
 * @brief Write killer track (all sync)
 * @param session Writer session
 * @param halftrack Halftrack number
 * @return 0 on success
 */
int writer_kill_track(writer_session_t *session, int halftrack);

/**
 * @brief Erase track
 * @param session Writer session
 * @param halftrack Halftrack number
 * @return 0 on success
 */
int writer_erase_track(writer_session_t *session, int halftrack);

/* ============================================================================
 * API Functions - Disk Mastering
 * ============================================================================ */

/**
 * @brief Master entire disk from image
 * @param session Writer session
 * @param image Disk image to write
 * @param progress_cb Progress callback (can be NULL)
 * @param user_data Callback user data
 * @return 0 on success
 */
int writer_master_disk(writer_session_t *session, const master_image_t *image,
                       void (*progress_cb)(int track, int total, const char *msg, void *ud),
                       void *user_data);

/**
 * @brief Unformat/wipe entire disk
 * @param session Writer session
 * @param start_track Start halftrack
 * @param end_track End halftrack
 * @param passes Number of wipe passes
 * @return 0 on success
 */
int writer_unformat_disk(writer_session_t *session, int start_track, 
                         int end_track, int passes);

/**
 * @brief Initialize disk with alignment sweep
 * @param session Writer session
 * @param start_track Start halftrack
 * @param end_track End halftrack
 * @return 0 on success
 */
int writer_init_aligned(writer_session_t *session, int start_track, int end_track);

/* ============================================================================
 * API Functions - Track Processing
 * ============================================================================ */

/**
 * @brief Check and set sync flags
 * @param track_data Track data
 * @param density Current density
 * @param length Track length
 * @return Updated density with flags
 */
uint8_t writer_check_sync_flags(const uint8_t *track_data, uint8_t density, size_t length);

/**
 * @brief Check if track is formatted
 * @param track_data Track data
 * @param length Track length
 * @return true if formatted
 */
bool writer_check_formatted(const uint8_t *track_data, size_t length);

/**
 * @brief Compress track for writing
 * @param halftrack Halftrack number
 * @param track_data Track data (modified in place)
 * @param density Track density
 * @param length Track length
 * @return Compressed length
 */
size_t writer_compress_track(int halftrack, uint8_t *track_data, 
                              uint8_t density, size_t length);

/**
 * @brief Lengthen sync marks
 * @param track_data Track data
 * @param length Track length
 * @param capacity Track capacity
 * @return Number of bytes added
 */
int writer_lengthen_sync(uint8_t *track_data, size_t length, size_t capacity);

/**
 * @brief Replace bytes in buffer
 * @param data Buffer
 * @param length Buffer length
 * @param find Byte to find
 * @param replace Replacement byte
 * @return Number of replacements
 */
int writer_replace_bytes(uint8_t *data, size_t length, uint8_t find, uint8_t replace);

/* ============================================================================
 * API Functions - Verification
 * ============================================================================ */

/**
 * @brief Verify track against original
 * @param session Writer session
 * @param halftrack Halftrack number
 * @param original Original track data
 * @param original_len Original length
 * @param density Track density
 * @param result Output result
 * @return 0 on success
 */
int writer_verify_track(writer_session_t *session, int halftrack,
                        const uint8_t *original, size_t original_len,
                        uint8_t density, track_write_result_t *result);

/* ============================================================================
 * API Functions - Image Management
 * ============================================================================ */

/**
 * @brief Create master image from track buffers
 * @param track_data Track data array (WRITER_TRACK_SIZE per track)
 * @param track_density Density array
 * @param track_length Length array
 * @param start_track Start halftrack
 * @param end_track End halftrack
 * @return New image, or NULL on error
 */
master_image_t *writer_create_image(uint8_t *track_data, uint8_t *track_density,
                                    size_t *track_length, int start_track, int end_track);

/**
 * @brief Free master image
 * @param image Image to free (does not free track buffers)
 */
void writer_free_image(master_image_t *image);

/**
 * @brief Load master image from NIB file
 * @param filename NIB file path
 * @param image Output image
 * @return 0 on success
 */
int writer_load_nib(const char *filename, master_image_t **image);

/**
 * @brief Load master image from G64 file
 * @param filename G64 file path
 * @param image Output image
 * @return 0 on success
 */
int writer_load_g64(const char *filename, master_image_t **image);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Get default density for track
 * @param track Track number (1-42)
 * @return Density (0-3)
 */
uint8_t writer_default_density(int track);

/**
 * @brief Get default capacity for density
 * @param density Density (0-3)
 * @return Capacity in bytes
 */
size_t writer_default_capacity(int density);

/**
 * @brief Calculate RPM from capacity
 * @param capacity Measured capacity
 * @param density Density (0-3)
 * @return Calculated RPM
 */
float writer_calc_rpm(int capacity, int density);

/**
 * @brief Get sectors per track
 * @param track Track number (1-42)
 * @return Number of sectors
 */
int writer_sectors_per_track(int track);

/* ============================================================================
 * Software-only Functions (no hardware)
 * ============================================================================ */

/**
 * @brief Prepare track buffer for writing
 * Processes track data for optimal mastering
 * @param track_data Input/output track data
 * @param length Track length
 * @param density Track density
 * @param options Write options
 * @param output_len Output: prepared length
 * @return 0 on success
 */
int writer_prepare_track(uint8_t *track_data, size_t length,
                         uint8_t density, const write_options_t *options,
                         size_t *output_len);

/**
 * @brief Create null writer session (for testing)
 * @return Null session
 */
writer_session_t *writer_create_null_session(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TRACK_WRITER_H */
