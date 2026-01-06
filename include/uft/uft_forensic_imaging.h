/**
 * @file uft_forensic_imaging.h
 * @brief UnifiedFloppyTool - Forensic Imaging Module v3.1.4.009
 * 
 * Comprehensive forensic disk imaging functionality extracted from:
 * - dd_rescue 1.99.22 (Kurt Garloff) - Recovery algorithms, SIMD optimization
 * - dc3dd (DoD Cyber Crime Center) - Forensic hashing, verification
 * - dcfldd (Nicholas Harbour) - Split imaging, window hashing
 * - floppy_bios (Sergey Kiselev) - Low-level FDC register operations
 * 
 * Features:
 * - Multi-algorithm concurrent hashing (MD5/SHA1/SHA256/SHA384/SHA512)
 * - Error recovery with sector-level granularity
 * - Sparse file detection with SIMD acceleration
 * - Split output with configurable naming schemes
 * - Window-based piecewise hashing for large images
 * - Verify mode for forensic validation
 * - Progress reporting with ETA calculation
 * - Bad sector logging and mapping
 * 
 * @version 3.1.4.009
 * @date 2025-12-30
 * @license GPL-2.0-or-later
 */

#ifndef UFT_FORENSIC_IMAGING_H
#define UFT_FORENSIC_IMAGING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * CONSTANTS AND LIMITS
 *===========================================================================*/

/** Default buffer sizes (from dd_rescue) */
#define UFT_FI_SOFT_BLOCKSIZE     131072    /**< 128KB soft block */
#define UFT_FI_HARD_BLOCKSIZE     512       /**< Minimum retry unit */
#define UFT_FI_DIO_SOFTBLOCKSIZE  1048576   /**< 1MB for direct I/O */
#define UFT_FI_DIO_HARDBLOCKSIZE  512       /**< Direct I/O minimum */

/** Hash digest sizes */
#define UFT_FI_MD5_SIZE           16
#define UFT_FI_SHA1_SIZE          20
#define UFT_FI_SHA256_SIZE        32
#define UFT_FI_SHA384_SIZE        48
#define UFT_FI_SHA512_SIZE        64
#define UFT_FI_MAX_HASH_SIZE      64

/** Hash string lengths (hex + null) */
#define UFT_FI_MD5_STR_LEN        33
#define UFT_FI_SHA1_STR_LEN       41
#define UFT_FI_SHA256_STR_LEN     65
#define UFT_FI_SHA384_STR_LEN     97
#define UFT_FI_SHA512_STR_LEN     129

/** Buffer queue settings (from dc3dd) */
#define UFT_FI_NUM_BUFFERS        64
#define UFT_FI_DEFAULT_SECTOR_SZ  512

/** Split file naming */
#define UFT_FI_SPLIT_FMT_DEFAULT  "000"     /**< Numeric: .000, .001, etc. */
#define UFT_FI_SPLIT_FMT_ALPHA    "aaa"     /**< Alpha: .aaa, .aab, etc. */
#define UFT_FI_SPLIT_FMT_MAC      "MAC"     /**< .dmg, .002.dmgpart, etc. */
#define UFT_FI_SPLIT_FMT_WIN      "WIN"     /**< .001, .002, .003, etc. */

/*===========================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Hash algorithm selection flags (from dcfldd)
 */
typedef enum {
    UFT_FI_HASH_NONE   = 0,
    UFT_FI_HASH_MD5    = (1 << 0),
    UFT_FI_HASH_SHA1   = (1 << 1),
    UFT_FI_HASH_SHA256 = (1 << 2),
    UFT_FI_HASH_SHA384 = (1 << 3),
    UFT_FI_HASH_SHA512 = (1 << 4),
    UFT_FI_HASH_ALL    = 0x1F
} uft_fi_hash_flags_t;

/**
 * @brief I/O operation state (from dc3dd)
 */
typedef enum {
    UFT_FI_STATE_PENDING,
    UFT_FI_STATE_OPEN,
    UFT_FI_STATE_ACTIVE,
    UFT_FI_STATE_COMPLETE,
    UFT_FI_STATE_ERROR,
    UFT_FI_STATE_ABORTED
} uft_fi_io_state_t;

/**
 * @brief Exit/completion codes (from dc3dd)
 */
typedef enum {
    UFT_FI_EXIT_SUCCESS = 0,
    UFT_FI_EXIT_COMPLETED,
    UFT_FI_EXIT_PARTIAL,      /**< Completed with recoverable errors */
    UFT_FI_EXIT_ABORTED,
    UFT_FI_EXIT_FAILED,
    UFT_FI_EXIT_VERIFY_FAIL
} uft_fi_exit_code_t;

/**
 * @brief Verification mode (from dc3dd)
 */
typedef enum {
    UFT_FI_VERIFY_NONE,
    UFT_FI_VERIFY_STANDARD,      /**< Hash comparison */
    UFT_FI_VERIFY_DEVICE_PARTIAL,/**< Re-read partial */
    UFT_FI_VERIFY_DEVICE_FULL    /**< Re-read entire device */
} uft_fi_verify_mode_t;

/**
 * @brief Error codes (FDC compatible + forensic extensions)
 */
typedef enum {
    UFT_FI_ERR_SUCCESS       = 0x00,
    UFT_FI_ERR_INVALID       = 0x01, /**< Invalid function/parameter */
    UFT_FI_ERR_ADDR_MARK     = 0x02, /**< Address mark not found */
    UFT_FI_ERR_WRITE_PROT    = 0x03, /**< Write protected */
    UFT_FI_ERR_SECTOR_NF     = 0x04, /**< Sector not found */
    UFT_FI_ERR_DISK_CHANGED  = 0x06, /**< Media changed */
    UFT_FI_ERR_DMA_OVERRUN   = 0x08, /**< DMA overrun */
    UFT_FI_ERR_DMA_BOUNDARY  = 0x09, /**< DMA 64K boundary */
    UFT_FI_ERR_BAD_FORMAT    = 0x0C, /**< Unknown format */
    UFT_FI_ERR_CRC           = 0x10, /**< CRC error */
    UFT_FI_ERR_CTRL_FAIL     = 0x20, /**< Controller failure */
    UFT_FI_ERR_SEEK          = 0x40, /**< Seek failed */
    UFT_FI_ERR_TIMEOUT       = 0x80, /**< Timeout / not ready */
    /* Extended forensic errors */
    UFT_FI_ERR_IO            = 0x81, /**< Generic I/O error */
    UFT_FI_ERR_HASH_MISMATCH = 0x82, /**< Hash verification failed */
    UFT_FI_ERR_SIZE_MISMATCH = 0x83, /**< Size mismatch */
    UFT_FI_ERR_ALLOCATION    = 0x84, /**< Memory allocation failed */
    UFT_FI_ERR_CANCELLED     = 0x85  /**< User cancelled */
} uft_fi_error_t;

/**
 * @brief Log level (from dd_rescue)
 */
typedef enum {
    UFT_FI_LOG_NOHDR = 0,
    UFT_FI_LOG_DEBUG,
    UFT_FI_LOG_INFO,
    UFT_FI_LOG_WARN,
    UFT_FI_LOG_GOOD,
    UFT_FI_LOG_FATAL,
    UFT_FI_LOG_INPUT
} uft_fi_log_level_t;

/*===========================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Hash context wrapper (generic for all algorithms)
 */
typedef struct {
    void *context;                      /**< Algorithm-specific context */
    uint8_t sum[UFT_FI_MAX_HASH_SIZE];  /**< Binary hash result */
    char result[UFT_FI_SHA512_STR_LEN]; /**< Hex string result */
    uint64_t bytes_hashed;              /**< Bytes processed */
} uft_fi_hash_ctx_t;

/**
 * @brief Hash algorithm descriptor (from dcfldd pattern)
 */
typedef struct {
    const char *name;                   /**< Algorithm name */
    uft_fi_hash_flags_t flag;           /**< Selection flag */
    size_t context_size;                /**< Size of context struct */
    size_t sum_size;                    /**< Size of hash digest */
    void (*init)(void *ctx);            /**< Initialize context */
    void (*update)(void *ctx, const void *data, size_t len);
    void (*finish)(void *ctx, void *out);
} uft_fi_hash_algo_t;

/**
 * @brief Hash output structure for concurrent multi-hash
 */
typedef struct uft_fi_hash_output {
    const uft_fi_hash_algo_t *algorithm;
    uft_fi_hash_ctx_t *total_hash;      /**< Full image hash */
    uft_fi_hash_ctx_t *window_hash;     /**< Current window hash */
    uft_fi_hash_ctx_t *piecewise_list;  /**< List of window hashes */
    uint64_t piecewise_count;
    uint64_t window_size;               /**< Bytes per hash window */
    struct uft_fi_hash_output *next;    /**< Linked list for multi-hash */
} uft_fi_hash_output_t;

/**
 * @brief Bad sector record for forensic logging
 */
typedef struct uft_fi_bad_sector {
    uint64_t sector_number;
    uint64_t lba_offset;                /**< Byte offset in image */
    int error_code;
    int retry_count;
    time_t timestamp;
    struct uft_fi_bad_sector *next;
} uft_fi_bad_sector_t;

/**
 * @brief Split file output context (from dcfldd)
 */
typedef struct {
    char *base_name;                    /**< Base filename */
    char *format;                       /**< Extension format (000/aaa/MAC/WIN) */
    int current_fd;                     /**< Current file descriptor */
    uint64_t max_bytes;                 /**< Max bytes per split */
    uint64_t current_bytes;             /**< Bytes in current split */
    uint64_t total_bytes;               /**< Total bytes written */
    uint32_t split_count;               /**< Number of splits created */
} uft_fi_split_ctx_t;

/**
 * @brief Input source configuration
 */
typedef struct {
    char *path;                         /**< Source path or device */
    int fd;                             /**< File descriptor */
    uint64_t size;                      /**< Total size in bytes */
    uint64_t sector_size;               /**< Logical sector size */
    bool is_device;                     /**< True if block device */
    bool is_floppy;                     /**< True if floppy device */
    uint64_t skip_sectors;              /**< Sectors to skip at start */
    uint64_t max_sectors;               /**< Max sectors to read (0=all) */
} uft_fi_input_t;

/**
 * @brief Output destination configuration
 */
typedef struct {
    char *path;                         /**< Destination path */
    int fd;                             /**< File descriptor */
    bool append;                        /**< Append mode */
    uint64_t skip_sectors;              /**< Sectors to skip at start */
    uft_fi_split_ctx_t *split;          /**< Split context if splitting */
    uft_fi_verify_mode_t verify_mode;
} uft_fi_output_t;

/**
 * @brief Progress/statistics tracking
 */
typedef struct {
    uint64_t bytes_read;
    uint64_t bytes_written;
    uint64_t sectors_processed;
    uint64_t sectors_total;
    uint64_t bad_sectors;
    uint64_t recovered_sectors;
    uint64_t errors_total;
    time_t start_time;
    time_t last_update;
    double transfer_rate;               /**< Bytes per second */
    int eta_seconds;                    /**< Estimated time remaining */
    bool interrupted;
} uft_fi_progress_t;

/**
 * @brief Recovery options (from dd_rescue)
 */
typedef struct {
    bool enable_recovery;               /**< Try to recover bad sectors */
    bool fill_pattern;                  /**< Fill bad sectors with pattern */
    uint8_t fill_byte;                  /**< Fill byte (default 0x00) */
    int max_retries;                    /**< Max retries per sector */
    int retry_delay_ms;                 /**< Delay between retries */
    bool reverse_on_error;              /**< Try reverse read on error */
    bool reduce_blocksize;              /**< Reduce to hard_bs on error */
    uint64_t soft_blocksize;            /**< Normal block size */
    uint64_t hard_blocksize;            /**< Minimum retry block size */
} uft_fi_recovery_opts_t;

/**
 * @brief Main imaging job configuration
 */
typedef struct {
    uft_fi_input_t input;
    uft_fi_output_t output;
    uft_fi_hash_flags_t hash_flags;
    uft_fi_hash_output_t *hash_outputs;
    uint64_t hash_window_size;          /**< Bytes per hash window (0=full) */
    uft_fi_recovery_opts_t recovery;
    uft_fi_progress_t progress;
    uft_fi_bad_sector_t *bad_sector_list;
    char *log_path;                     /**< Path to write log file */
    int log_fd;
    uft_fi_log_level_t log_level;
    uft_fi_io_state_t state;
    uft_fi_exit_code_t exit_code;
    void *user_data;                    /**< User callback data */
    void (*progress_callback)(const uft_fi_progress_t *prog, void *user);
    void (*log_callback)(uft_fi_log_level_t level, const char *msg, void *user);
} uft_fi_job_t;

/*===========================================================================
 * SIMD SPARSE DETECTION (from dd_rescue)
 *===========================================================================*/

/**
 * @brief CPU capability flags for SIMD selection
 */
typedef struct {
    bool has_sse2;
    bool has_avx2;
    bool has_neon;
    bool has_sve;
} uft_fi_cpu_caps_t;

/**
 * @brief Detect CPU SIMD capabilities
 */
void uft_fi_detect_cpu_caps(uft_fi_cpu_caps_t *caps);

/**
 * @brief Find first non-zero byte in buffer (C reference)
 * @param blk Buffer to scan
 * @param len Length in bytes
 * @return Offset of first non-zero byte, or len if all zeros
 */
size_t uft_fi_find_nonzero_c(const uint8_t *blk, size_t len);

/**
 * @brief Find first non-zero byte (auto-dispatch to best SIMD)
 */
size_t uft_fi_find_nonzero(const uint8_t *blk, size_t len);

/**
 * @brief Find first non-zero byte from end (backward scan)
 */
size_t uft_fi_find_nonzero_bkw(const uint8_t *blk_end, size_t len);

#ifdef __SSE2__
/**
 * @brief SSE2-optimized zero detection
 */
size_t uft_fi_find_nonzero_sse2(const uint8_t *blk, size_t len);
#endif

#ifdef __AVX2__
/**
 * @brief AVX2-optimized zero detection
 */
size_t uft_fi_find_nonzero_avx2(const uint8_t *blk, size_t len);
#endif

/*===========================================================================
 * INLINE HELPER FUNCTIONS
 *===========================================================================*/

/**
 * @brief Check if buffer is all zeros (sparse detection)
 */
static inline bool uft_fi_is_sparse_block(const uint8_t *buf, size_t len) {
    return uft_fi_find_nonzero(buf, len) == len;
}

/**
 * @brief Convert hash bytes to hex string
 */
static inline void uft_fi_hash_to_hex(const uint8_t *hash, size_t hash_len,
                                       char *out) {
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < hash_len; i++) {
        out[i * 2]     = hex[(hash[i] >> 4) & 0x0F];
        out[i * 2 + 1] = hex[hash[i] & 0x0F];
    }
    out[hash_len * 2] = '\0';
}

/**
 * @brief Calculate midpoint for binary search (block-aligned)
 * Used for device size probing (from dcfldd)
 */
static inline uint64_t uft_fi_midpoint(uint64_t a, uint64_t b, uint64_t blksz) {
    uint64_t a_blocks = a / blksz;
    uint64_t b_blocks = b / blksz;
    uint64_t mid_blocks = (b_blocks - a_blocks) / 2 + a_blocks;
    return mid_blocks * blksz;
}

/**
 * @brief Calculate progress percentage
 */
static inline int uft_fi_progress_percent(const uft_fi_progress_t *p) {
    if (p->sectors_total == 0) return 0;
    return (int)((p->sectors_processed * 100) / p->sectors_total);
}

/**
 * @brief Calculate ETA in seconds
 */
static inline int uft_fi_calc_eta(const uft_fi_progress_t *p) {
    if (p->sectors_processed == 0 || p->transfer_rate <= 0) return -1;
    uint64_t remaining = p->sectors_total - p->sectors_processed;
    double sectors_per_sec = p->transfer_rate / UFT_FI_DEFAULT_SECTOR_SZ;
    if (sectors_per_sec <= 0) return -1;
    return (int)(remaining / sectors_per_sec);
}

/**
 * @brief Format ETA as human-readable string
 */
static inline void uft_fi_format_eta(int seconds, char *buf, size_t buflen) {
    if (seconds < 0) {
        snprintf(buf, buflen, "calculating...");
    } else if (seconds < 60) {
        snprintf(buf, buflen, "%ds", seconds);
    } else if (seconds < 3600) {
        snprintf(buf, buflen, "%dm %02ds", seconds / 60, seconds % 60);
    } else {
        snprintf(buf, buflen, "%dh %02dm", seconds / 3600, (seconds % 3600) / 60);
    }
}

/*===========================================================================
 * SPLIT FILE NAMING (from dcfldd)
 *===========================================================================*/

/**
 * @brief Generate split file extension
 * @param format Format string ("000", "aaa", "MAC", "WIN")
 * @param num Split number (0-based)
 * @param out Output buffer
 * @param outlen Output buffer length
 * @return 0 on success, -1 on error
 */
int uft_fi_split_extension(const char *format, int num, char *out, size_t outlen);

/**
 * @brief Calculate maximum splits for format
 */
int uft_fi_split_max_count(const char *format);

/*===========================================================================
 * HASH OPERATIONS
 *===========================================================================*/

/**
 * @brief Get hash algorithm descriptor by flag
 */
const uft_fi_hash_algo_t* uft_fi_get_hash_algo(uft_fi_hash_flags_t flag);

/**
 * @brief Initialize hash outputs for selected algorithms
 */
int uft_fi_hash_init(uft_fi_job_t *job);

/**
 * @brief Update all active hashes with data
 */
void uft_fi_hash_update(uft_fi_job_t *job, const void *data, size_t len);

/**
 * @brief Finalize hashes and generate results
 */
void uft_fi_hash_finalize(uft_fi_job_t *job);

/**
 * @brief Free hash resources
 */
void uft_fi_hash_cleanup(uft_fi_job_t *job);

/*===========================================================================
 * SIZE PROBING (from dcfldd)
 *===========================================================================*/

/**
 * @brief Probe device/file size
 * Uses BLKGETSIZE64 on Linux, binary search fallback elsewhere
 * @param fd File descriptor
 * @param is_device True if fd is a block device
 * @return Size in bytes, or 0 on error
 */
uint64_t uft_fi_probe_size(int fd, bool is_device);

/*===========================================================================
 * MAIN IMAGING OPERATIONS
 *===========================================================================*/

/**
 * @brief Create new imaging job with defaults
 */
uft_fi_job_t* uft_fi_job_new(void);

/**
 * @brief Free imaging job and all resources
 */
void uft_fi_job_free(uft_fi_job_t *job);

/**
 * @brief Set input source
 */
int uft_fi_set_input(uft_fi_job_t *job, const char *path);

/**
 * @brief Set output destination
 */
int uft_fi_set_output(uft_fi_job_t *job, const char *path);

/**
 * @brief Configure split output
 */
int uft_fi_set_split(uft_fi_job_t *job, uint64_t max_bytes, const char *format);

/**
 * @brief Execute imaging job
 * @return Exit code
 */
uft_fi_exit_code_t uft_fi_execute(uft_fi_job_t *job);

/**
 * @brief Request job cancellation
 */
void uft_fi_cancel(uft_fi_job_t *job);

/**
 * @brief Execute verification pass
 */
uft_fi_exit_code_t uft_fi_verify(uft_fi_job_t *job);

/*===========================================================================
 * ERROR RECOVERY (from dd_rescue)
 *===========================================================================*/

/**
 * @brief Read with automatic retry and recovery
 * @param job Imaging job context
 * @param buf Output buffer
 * @param offset Byte offset to read from
 * @param len Bytes to read
 * @param actual Actual bytes read (output)
 * @return Error code
 */
uft_fi_error_t uft_fi_read_recover(uft_fi_job_t *job, uint8_t *buf,
                                    uint64_t offset, size_t len, size_t *actual);

/**
 * @brief Record bad sector in log
 */
void uft_fi_log_bad_sector(uft_fi_job_t *job, uint64_t sector, int error);

/**
 * @brief Get bad sector list
 */
const uft_fi_bad_sector_t* uft_fi_get_bad_sectors(const uft_fi_job_t *job);

/**
 * @brief Export bad sector map to file
 */
int uft_fi_export_bad_map(const uft_fi_job_t *job, const char *path);

/*===========================================================================
 * LOGGING
 *===========================================================================*/

/**
 * @brief Write message to job log
 */
void uft_fi_log(uft_fi_job_t *job, uft_fi_log_level_t level,
                const char *fmt, ...);

/**
 * @brief Write forensic audit header to log
 */
void uft_fi_log_header(uft_fi_job_t *job);

/**
 * @brief Write forensic audit footer with hashes
 */
void uft_fi_log_footer(uft_fi_job_t *job);

/*===========================================================================
 * FDC LOW-LEVEL OPERATIONS (from floppy_bios)
 *===========================================================================*/

/**
 * @brief FDC register offsets
 */
#define UFT_FDC_REG_DOR     2  /**< Digital Output Register */
#define UFT_FDC_REG_STATUS  4  /**< Main Status Register */
#define UFT_FDC_REG_DATA    5  /**< Data Register */
#define UFT_FDC_REG_DIR     7  /**< Digital Input Register */
#define UFT_FDC_REG_CCR     7  /**< Configuration Control Register */

/**
 * @brief FDC status bits
 */
#define UFT_FDC_STAT_READY  0x80  /**< FDC ready */
#define UFT_FDC_STAT_DIR    0x40  /**< Direction: 0=CPU->FDC, 1=FDC->CPU */
#define UFT_FDC_STAT_DMA    0x20  /**< DMA enabled */
#define UFT_FDC_STAT_BUSY   0x10  /**< FDC busy */

/**
 * @brief Media state codes (from floppy_bios)
 */
typedef enum {
    UFT_FDC_MEDIA_360_IN_360   = 0x93,  /**< 250Kbps, 360K in 360K */
    UFT_FDC_MEDIA_720          = 0x97,  /**< 250Kbps, 720K */
    UFT_FDC_MEDIA_360_IN_1200  = 0x74,  /**< 300Kbps, 360K in 1.2M */
    UFT_FDC_MEDIA_1200_IN_1200 = 0x15,  /**< 500Kbps, 1.2M in 1.2M */
    UFT_FDC_MEDIA_1440         = 0x17,  /**< 500Kbps, 1.44M */
    UFT_FDC_MEDIA_2880         = 0xD7   /**< 1Mbps, 2.88M */
} uft_fdc_media_state_t;

/**
 * @brief Data transfer rate values
 */
typedef enum {
    UFT_FDC_RATE_500KBPS = 0x00,  /**< 500 Kbit/sec (HD) */
    UFT_FDC_RATE_300KBPS = 0x01,  /**< 300 Kbit/sec (360K in 1.2M) */
    UFT_FDC_RATE_250KBPS = 0x02,  /**< 250 Kbit/sec (DD) */
    UFT_FDC_RATE_1MBPS   = 0x03   /**< 1 Mbit/sec (ED) */
} uft_fdc_rate_t;

/**
 * @brief Determine data rate from media state
 */
static inline uft_fdc_rate_t uft_fdc_state_to_rate(uft_fdc_media_state_t state) {
    return (uft_fdc_rate_t)((state >> 6) & 0x03);
}

/**
 * @brief Check if media state is established
 */
static inline bool uft_fdc_is_established(uft_fdc_media_state_t state) {
    return (state & 0x10) != 0;
}

/*===========================================================================
 * GUI INTEGRATION STRUCTURES
 *===========================================================================*/

/**
 * @brief Imaging parameters for GUI binding
 */
typedef struct {
    /* Source selection */
    char source_path[1024];
    bool source_is_device;
    uint64_t source_size;
    uint32_t source_sector_size;
    
    /* Destination */
    char dest_path[1024];
    bool dest_split;
    uint64_t dest_split_size;
    char dest_split_format[8];
    
    /* Hashing */
    bool hash_md5;
    bool hash_sha1;
    bool hash_sha256;
    bool hash_sha384;
    bool hash_sha512;
    uint64_t hash_window_size;          /**< 0 = full image only */
    
    /* Recovery */
    bool recovery_enabled;
    int recovery_retries;
    bool recovery_fill_zeros;
    
    /* Verification */
    uft_fi_verify_mode_t verify_mode;
    
    /* Logging */
    char log_path[1024];
    bool log_verbose;
    
    /* Operation */
    uint64_t skip_input_sectors;
    uint64_t skip_output_sectors;
    uint64_t max_sectors;
} uft_fi_gui_params_t;

/**
 * @brief GUI status update structure
 */
typedef struct {
    uft_fi_io_state_t state;
    int percent_complete;
    uint64_t bytes_processed;
    uint64_t bytes_total;
    uint64_t bad_sectors;
    double transfer_rate_mbps;
    char eta_string[32];
    char current_hash_md5[UFT_FI_MD5_STR_LEN];
    char current_hash_sha1[UFT_FI_SHA1_STR_LEN];
    char current_hash_sha256[UFT_FI_SHA256_STR_LEN];
    char status_message[256];
} uft_fi_gui_status_t;

/**
 * @brief Create job from GUI parameters
 */
uft_fi_job_t* uft_fi_job_from_gui(const uft_fi_gui_params_t *params);

/**
 * @brief Get current status for GUI update
 */
void uft_fi_get_gui_status(const uft_fi_job_t *job, uft_fi_gui_status_t *status);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORENSIC_IMAGING_H */
