// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * uft_dd.h - Unified Floppy Tool DD Module
 * 
 * Combines best features from:
 *   - dd_rescue: Recovery algorithms, reverse reading, adaptive block sizes
 *   - DC3DD: Forensic hashing, wipe patterns, verification
 *   - dcfldd: Multiple outputs, hash-on-copy, splitting
 * 
 * Special Features for UFT:
 *   - Direct floppy output (raw sector writes)
 *   - Flux-to-image and image-to-floppy
 *   - Recovery-aware copying with bad sector handling
 *   - GUI-controllable parameters
 * 
 * @version 1.0.0
 * @date 2025-01-01
 */

#ifndef UFT_DD_H
#define UFT_DD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * CONSTANTS
 *============================================================================*/

/* Default block sizes (from dd_rescue) */
#define DD_SOFT_BLOCKSIZE       131072      /* 128K for normal I/O */
#define DD_HARD_BLOCKSIZE       512         /* Minimum sector size */
#define DD_DIO_BLOCKSIZE        1048576     /* 1M for direct I/O */

/* Floppy-specific sizes */
#define FLOPPY_SECTOR_SIZE      512
#define FLOPPY_DD_SECTORS       1440        /* 720K */
#define FLOPPY_HD_SECTORS       2880        /* 1.44M */

/* Hash types */
#define HASH_NONE               0
#define HASH_MD5                1
#define HASH_SHA1               2
#define HASH_SHA256             4
#define HASH_SHA512             8
#define HASH_ALL                (HASH_MD5 | HASH_SHA1 | HASH_SHA256 | HASH_SHA512)

/* Wipe patterns (from DC3DD) */
#define WIPE_ZERO               0x00
#define WIPE_ONE                0xFF
#define WIPE_RANDOM             0x100       /* Special: use random data */
#define WIPE_DOD_3PASS          0x101       /* DoD 5220.22-M 3-pass */
#define WIPE_DOD_7PASS          0x102       /* DoD 5220.22-M 7-pass */
#define WIPE_GUTMANN            0x103       /* Gutmann 35-pass */

/*============================================================================*
 * DATA STRUCTURES
 *============================================================================*/

/*----------------------------------------------------------------------------*
 * Block Size Parameters (from dd_rescue)
 *----------------------------------------------------------------------------*/
typedef struct {
    size_t soft_blocksize;      /* Normal read/write size (128K default) */
    size_t hard_blocksize;      /* Minimum size on error (512 default) */
    size_t dio_blocksize;       /* Direct I/O size (1M default) */
    bool auto_adjust;           /* Auto-adjust on errors */
} dd_blocksize_t;

/* Constraints for GUI */
#define DD_SOFT_BS_MIN          512
#define DD_SOFT_BS_MAX          16777216    /* 16M */
#define DD_SOFT_BS_DEFAULT      131072

#define DD_HARD_BS_MIN          512
#define DD_HARD_BS_MAX          65536
#define DD_HARD_BS_DEFAULT      512

/*----------------------------------------------------------------------------*
 * Recovery Parameters (from dd_rescue)
 *----------------------------------------------------------------------------*/
typedef struct {
    bool enabled;               /* Enable recovery mode */
    bool reverse;               /* Read backwards (for head crashes) */
    bool sparse;                /* Create sparse output file */
    bool nosparse;              /* Never create sparse files */
    int max_errors;             /* Max errors before abort (0=infinite) */
    int retry_count;            /* Retries per bad sector */
    int retry_delay_ms;         /* Delay between retries */
    bool sync_on_error;         /* Sync after each error */
    bool continue_on_error;     /* Continue after error (noerror) */
    bool fill_on_error;         /* Fill unreadable with pattern */
    uint8_t fill_pattern;       /* Pattern for unreadable sectors */
} dd_recovery_t;

/* Constraints for GUI */
#define DD_MAX_ERRORS_MIN       0
#define DD_MAX_ERRORS_MAX       100000
#define DD_MAX_ERRORS_DEFAULT   0           /* 0 = infinite */

#define DD_RETRY_COUNT_MIN      0
#define DD_RETRY_COUNT_MAX      100
#define DD_RETRY_COUNT_DEFAULT  3

#define DD_RETRY_DELAY_MIN      0
#define DD_RETRY_DELAY_MAX      10000
#define DD_RETRY_DELAY_DEFAULT  100

/*----------------------------------------------------------------------------*
 * Hashing Parameters (from DC3DD/dcfldd)
 *----------------------------------------------------------------------------*/
typedef struct {
    int algorithms;             /* Bitmask: HASH_MD5 | HASH_SHA256 etc */
    bool hash_input;            /* Hash input data */
    bool hash_output;           /* Hash output data */
    bool hash_window;           /* Hash per-window (for split) */
    size_t window_size;         /* Window size for piecewise hash */
    bool verify_after;          /* Verify by re-reading after write */
} dd_hash_t;

#define DD_HASH_WINDOW_MIN      1048576     /* 1M */
#define DD_HASH_WINDOW_MAX      1073741824  /* 1G */
#define DD_HASH_WINDOW_DEFAULT  10485760    /* 10M */

/*----------------------------------------------------------------------------*
 * Wipe Parameters (from DC3DD)
 *----------------------------------------------------------------------------*/
typedef struct {
    bool enabled;               /* Enable wipe mode */
    int pattern;                /* WIPE_ZERO, WIPE_RANDOM, etc */
    uint8_t custom_byte;        /* Custom byte pattern */
    const char *custom_text;    /* Custom text pattern */
    int passes;                 /* Number of passes */
    bool verify_wipe;           /* Verify after wipe */
} dd_wipe_t;

#define DD_WIPE_PASSES_MIN      1
#define DD_WIPE_PASSES_MAX      35
#define DD_WIPE_PASSES_DEFAULT  1

/*----------------------------------------------------------------------------*
 * Output Parameters
 *----------------------------------------------------------------------------*/
typedef struct {
    bool split_output;          /* Split into multiple files */
    size_t split_size;          /* Size per split file */
    const char *split_format;   /* Filename format (printf-style) */
    bool append;                /* Append to output */
    bool truncate;              /* Truncate output */
    bool direct_io;             /* Use O_DIRECT */
    bool sync_writes;           /* Sync after each write */
    int sync_frequency;         /* Sync every N blocks */
} dd_output_t;

#define DD_SPLIT_SIZE_MIN       1048576     /* 1M */
#define DD_SPLIT_SIZE_MAX       4294967296ULL /* 4G */
#define DD_SPLIT_SIZE_DEFAULT   0           /* 0 = no split */

#define DD_SYNC_FREQ_MIN        0
#define DD_SYNC_FREQ_MAX        10000
#define DD_SYNC_FREQ_DEFAULT    0           /* 0 = no periodic sync */

/*----------------------------------------------------------------------------*
 * Floppy-Specific Parameters (NEW for UFT)
 *----------------------------------------------------------------------------*/
typedef enum {
    FLOPPY_NONE = 0,
    FLOPPY_RAW_DEVICE,          /* /dev/fd0 or \\.\A: */
    FLOPPY_USB_DEVICE,          /* USB floppy */
    FLOPPY_GREASEWEAZLE,        /* Via Greaseweazle */
    FLOPPY_FLUXENGINE,          /* Via FluxEngine */
    FLOPPY_KRYOFLUX             /* Via KryoFlux */
} floppy_type_t;

typedef struct {
    bool enabled;               /* Enable floppy output */
    floppy_type_t type;         /* Floppy device type */
    const char *device;         /* Device path */
    int drive_number;           /* Drive unit (0-3) */
    
    /* Geometry */
    int tracks;                 /* 40 or 80 */
    int heads;                  /* 1 or 2 */
    int sectors_per_track;      /* 9, 11, 18, etc */
    int sector_size;            /* 512 typically */
    
    /* Write options */
    bool format_before;         /* Format disk before writing */
    bool verify_sectors;        /* Verify each sector after write */
    int write_retries;          /* Retries for write errors */
    bool skip_bad_sectors;      /* Skip instead of abort */
    
    /* Timing (for hardware controllers) */
    int step_delay_ms;          /* Head step delay */
    int settle_delay_ms;        /* Head settle delay */
    int motor_delay_ms;         /* Motor spin-up delay */
} dd_floppy_t;

/* Floppy constraints for GUI */
#define DD_FLOPPY_TRACKS_MIN    40
#define DD_FLOPPY_TRACKS_MAX    85
#define DD_FLOPPY_TRACKS_DEFAULT 80

#define DD_FLOPPY_HEADS_MIN     1
#define DD_FLOPPY_HEADS_MAX     2
#define DD_FLOPPY_HEADS_DEFAULT 2

#define DD_FLOPPY_SPT_MIN       1
#define DD_FLOPPY_SPT_MAX       21
#define DD_FLOPPY_SPT_DEFAULT   18

#define DD_FLOPPY_RETRIES_MIN   0
#define DD_FLOPPY_RETRIES_MAX   20
#define DD_FLOPPY_RETRIES_DEFAULT 3

#define DD_FLOPPY_STEP_DELAY_MIN    1
#define DD_FLOPPY_STEP_DELAY_MAX    50
#define DD_FLOPPY_STEP_DELAY_DEFAULT 3

#define DD_FLOPPY_SETTLE_DELAY_MIN  5
#define DD_FLOPPY_SETTLE_DELAY_MAX  100
#define DD_FLOPPY_SETTLE_DELAY_DEFAULT 15

#define DD_FLOPPY_MOTOR_DELAY_MIN   100
#define DD_FLOPPY_MOTOR_DELAY_MAX   2000
#define DD_FLOPPY_MOTOR_DELAY_DEFAULT 500

/*----------------------------------------------------------------------------*
 * Progress/Status Reporting
 *----------------------------------------------------------------------------*/
typedef struct {
    /* Counts */
    uint64_t bytes_read;
    uint64_t bytes_written;
    uint64_t blocks_full;
    uint64_t blocks_partial;
    uint64_t errors_read;
    uint64_t errors_write;
    uint64_t sectors_skipped;
    
    /* Timing */
    time_t start_time;
    time_t current_time;
    double elapsed_seconds;
    double bytes_per_second;
    double eta_seconds;
    
    /* Progress */
    double percent_complete;
    uint64_t total_size;
    
    /* Current position */
    uint64_t current_offset;
    int current_track;
    int current_head;
    int current_sector;
    
    /* Hashes (if enabled) */
    char md5_input[33];
    char md5_output[33];
    char sha1_input[41];
    char sha1_output[41];
    char sha256_input[65];
    char sha256_output[65];
    
    /* Status message */
    char status_message[256];
    bool is_running;
    bool is_paused;
    bool has_error;
} dd_status_t;

/*----------------------------------------------------------------------------*
 * Master Configuration
 *----------------------------------------------------------------------------*/
typedef struct {
    /* Input */
    const char *input_file;     /* Input file/device */
    uint64_t skip_bytes;        /* Bytes to skip at input start */
    uint64_t max_bytes;         /* Maximum bytes to copy (0=all) */
    
    /* Output */
    const char *output_file;    /* Output file/device (can be NULL if floppy) */
    uint64_t seek_bytes;        /* Bytes to seek at output start */
    
    /* Component configurations */
    dd_blocksize_t blocksize;
    dd_recovery_t recovery;
    dd_hash_t hash;
    dd_wipe_t wipe;
    dd_output_t output;
    dd_floppy_t floppy;
    
    /* Logging */
    const char *log_file;       /* Log file path */
    int log_level;              /* 0=none, 1=errors, 2=info, 3=debug */
    bool log_timestamps;        /* Include timestamps */
    
    /* Callbacks */
    void (*progress_callback)(const dd_status_t *status, void *user_data);
    void (*error_callback)(const char *message, int error_code, void *user_data);
    void *user_data;
    
} dd_config_t;

/*============================================================================*
 * FUNCTION DECLARATIONS
 *============================================================================*/

/**
 * @brief Initialize configuration with defaults
 */
void dd_config_init(dd_config_t *config);

/**
 * @brief Validate configuration
 * @return 0 if valid, error code otherwise
 */
int dd_config_validate(const dd_config_t *config);

/**
 * @brief Start DD operation
 * @return 0 on success
 */
int dd_start(const dd_config_t *config);

/**
 * @brief Pause running operation
 */
void dd_pause(void);

/**
 * @brief Resume paused operation
 */
void dd_resume(void);

/**
 * @brief Cancel running operation
 */
void dd_cancel(void);

/**
 * @brief Get current status
 */
void dd_get_status(dd_status_t *status);

/**
 * @brief Check if operation is running
 */
bool dd_is_running(void);

/*============================================================================*
 * FLOPPY-SPECIFIC FUNCTIONS
 *============================================================================*/

/**
 * @brief Detect available floppy devices
 * @return Number of devices found
 */
int dd_floppy_detect(char **devices, int max_devices);

/**
 * @brief Get floppy geometry
 */
int dd_floppy_get_geometry(const char *device, dd_floppy_t *floppy);

/**
 * @brief Format floppy disk
 */
int dd_floppy_format(const dd_floppy_t *floppy);

/**
 * @brief Write sector directly
 */
int dd_floppy_write_sector(const dd_floppy_t *floppy, 
                           int track, int head, int sector,
                           const uint8_t *data, size_t len);

/**
 * @brief Read sector directly
 */
int dd_floppy_read_sector(const dd_floppy_t *floppy,
                          int track, int head, int sector,
                          uint8_t *data, size_t len);

/**
 * @brief Write image to floppy
 */
int dd_floppy_write_image(const dd_floppy_t *floppy,
                          const uint8_t *image, size_t image_size,
                          void (*progress)(int track, int head, void *user),
                          void *user_data);

/**
 * @brief Read floppy to image
 */
int dd_floppy_read_image(const dd_floppy_t *floppy,
                         uint8_t *image, size_t max_size,
                         void (*progress)(int track, int head, void *user),
                         void *user_data);

/*============================================================================*
 * HASH FUNCTIONS
 *============================================================================*/

/**
 * @brief Get hash result as hex string
 */
const char *dd_hash_get_md5(void);
const char *dd_hash_get_sha1(void);
const char *dd_hash_get_sha256(void);
const char *dd_hash_get_sha512(void);

/**
 * @brief Verify hash matches expected
 */
bool dd_hash_verify(int algorithm, const char *expected_hex);

/*============================================================================*
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Parse size string (e.g., "1M", "512K", "1G")
 */
uint64_t dd_parse_size(const char *str);

/**
 * @brief Format size for display
 */
const char *dd_format_size(uint64_t bytes);

/**
 * @brief Format time for display
 */
const char *dd_format_time(double seconds);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DD_H */
