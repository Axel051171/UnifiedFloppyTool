/**
 * @file uft_hardware_mock.h
 * @brief Hardware Mock Framework API
 * 
 * TICKET-009: Hardware Mock Framework
 * Virtual device for testing without real hardware
 * 
 * @version 5.1.0
 * @date 2026-01-03
 */

#ifndef UFT_HARDWARE_MOCK_H
#define UFT_HARDWARE_MOCK_H

#include <stdint.h>
#include <stdbool.h>
#include "uft_types.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Mock Device Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** Mock hardware type */
typedef enum {
    UFT_MOCK_GREASEWEAZLE,      /**< Greaseweazle emulation */
    UFT_MOCK_FLUXENGINE,        /**< FluxEngine emulation */
    UFT_MOCK_KRYOFLUX,          /**< KryoFlux emulation */
    UFT_MOCK_SUPERCARDPRO,      /**< SuperCard Pro emulation */
    UFT_MOCK_GENERIC,           /**< Generic floppy controller */
} uft_mock_type_t;

/** Mock drive type */
typedef enum {
    UFT_MOCK_DRIVE_35DD,        /**< 3.5" DD (720K) */
    UFT_MOCK_DRIVE_35HD,        /**< 3.5" HD (1.44M) */
    UFT_MOCK_DRIVE_525DD,       /**< 5.25" DD (360K) */
    UFT_MOCK_DRIVE_525HD,       /**< 5.25" HD (1.2M) */
    UFT_MOCK_DRIVE_525QD,       /**< 5.25" QD (720K) */
    UFT_MOCK_DRIVE_8INCH,       /**< 8" drive */
    UFT_MOCK_DRIVE_1541,        /**< Commodore 1541 */
    UFT_MOCK_DRIVE_1571,        /**< Commodore 1571 */
} uft_mock_drive_t;

/** Error injection type */
typedef enum {
    UFT_MOCK_ERR_NONE,          /**< No error */
    UFT_MOCK_ERR_CRC,           /**< CRC error */
    UFT_MOCK_ERR_MISSING,       /**< Missing sector */
    UFT_MOCK_ERR_WEAK,          /**< Weak bits */
    UFT_MOCK_ERR_NO_INDEX,      /**< No index pulse */
    UFT_MOCK_ERR_TIMEOUT,       /**< Timeout */
    UFT_MOCK_ERR_WRITE_PROTECT, /**< Write protected */
    UFT_MOCK_ERR_NO_DISK,       /**< No disk in drive */
    UFT_MOCK_ERR_SEEK,          /**< Seek error */
    UFT_MOCK_ERR_DENSITY,       /**< Density mismatch */
} uft_mock_error_t;

/** Flux data source */
typedef enum {
    UFT_MOCK_FLUX_PERFECT,      /**< Perfect flux timing */
    UFT_MOCK_FLUX_REALISTIC,    /**< Realistic with jitter */
    UFT_MOCK_FLUX_DEGRADED,     /**< Degraded media */
    UFT_MOCK_FLUX_FROM_FILE,    /**< Load from file */
    UFT_MOCK_FLUX_GENERATED,    /**< Algorithmically generated */
} uft_mock_flux_source_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Mock Configuration
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** Track error configuration */
typedef struct {
    int             cylinder;       /**< Cylinder (-1 for all) */
    int             head;           /**< Head (-1 for all) */
    int             sector;         /**< Sector (-1 for all) */
    uft_mock_error_t error;         /**< Error to inject */
    int             probability;    /**< Probability 0-100 */
    int             retry_success;  /**< Succeed after N retries (0=never) */
} uft_mock_error_config_t;

/** Timing configuration */
typedef struct {
    uint32_t        rpm;            /**< Drive RPM (default 300) */
    float           rpm_variation;  /**< RPM variation % */
    uint32_t        bit_rate;       /**< Bit rate (bps) */
    float           jitter_ns;      /**< Timing jitter (ns) */
    uint32_t        step_time_ms;   /**< Head step time (ms) */
    uint32_t        settle_time_ms; /**< Head settle time (ms) */
    uint32_t        motor_spinup_ms;/**< Motor spinup time (ms) */
} uft_mock_timing_t;

/** Default timing */
#define UFT_MOCK_TIMING_DEFAULT { \
    .rpm = 300, \
    .rpm_variation = 0.5f, \
    .bit_rate = 250000, \
    .jitter_ns = 50.0f, \
    .step_time_ms = 3, \
    .settle_time_ms = 15, \
    .motor_spinup_ms = 500 \
}

/** Mock device configuration */
typedef struct {
    uft_mock_type_t     type;           /**< Controller type */
    uft_mock_drive_t    drive;          /**< Drive type */
    
    /* Geometry */
    int                 cylinders;      /**< Number of cylinders */
    int                 heads;          /**< Number of heads */
    int                 sectors;        /**< Sectors per track */
    int                 sector_size;    /**< Bytes per sector */
    
    /* Timing */
    uft_mock_timing_t   timing;         /**< Timing configuration */
    
    /* Flux */
    uft_mock_flux_source_t flux_source; /**< Flux data source */
    const char          *flux_file;     /**< Path to flux file (if FROM_FILE) */
    
    /* Error injection */
    uft_mock_error_config_t *errors;    /**< Error configurations */
    int                 error_count;    /**< Number of error configs */
    
    /* Behavior */
    bool                write_protect;  /**< Write protected */
    bool                disk_present;   /**< Disk in drive */
    bool                simulate_timing;/**< Simulate real timing delays */
    bool                log_operations; /**< Log all operations */
    
    /* Callbacks */
    void                (*on_read)(int cyl, int head, void *user);
    void                (*on_write)(int cyl, int head, void *user);
    void                (*on_seek)(int cyl, void *user);
    void                *callback_user;
} uft_mock_config_t;

/** Default configuration */
#define UFT_MOCK_CONFIG_DEFAULT { \
    .type = UFT_MOCK_GREASEWEAZLE, \
    .drive = UFT_MOCK_DRIVE_35DD, \
    .cylinders = 80, \
    .heads = 2, \
    .sectors = 9, \
    .sector_size = 512, \
    .timing = UFT_MOCK_TIMING_DEFAULT, \
    .flux_source = UFT_MOCK_FLUX_REALISTIC, \
    .flux_file = NULL, \
    .errors = NULL, \
    .error_count = 0, \
    .write_protect = false, \
    .disk_present = true, \
    .simulate_timing = false, \
    .log_operations = true, \
    .on_read = NULL, \
    .on_write = NULL, \
    .on_seek = NULL, \
    .callback_user = NULL \
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Mock Device Handle
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** Opaque mock device handle */
typedef struct uft_mock_device uft_mock_device_t;

/** Device statistics */
typedef struct {
    uint64_t        reads;          /**< Total reads */
    uint64_t        writes;         /**< Total writes */
    uint64_t        seeks;          /**< Total seeks */
    uint64_t        errors_injected;/**< Errors injected */
    uint64_t        retries;        /**< Retry attempts */
    uint64_t        bytes_read;     /**< Bytes read */
    uint64_t        bytes_written;  /**< Bytes written */
    int             current_cylinder; /**< Current head position */
    int             current_head;   /**< Current head */
    uint64_t        time_reading_ms;/**< Time spent reading */
    uint64_t        time_writing_ms;/**< Time spent writing */
    uint64_t        time_seeking_ms;/**< Time spent seeking */
} uft_mock_stats_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Device Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create mock device
 * @param config Device configuration
 * @return New mock device handle
 */
uft_mock_device_t *uft_mock_create(const uft_mock_config_t *config);

/**
 * @brief Create mock device with preset
 * @param type Controller type
 * @param drive Drive type
 * @return New mock device handle
 */
uft_mock_device_t *uft_mock_create_preset(uft_mock_type_t type, uft_mock_drive_t drive);

/**
 * @brief Destroy mock device
 * @param dev Device to destroy
 */
void uft_mock_destroy(uft_mock_device_t *dev);

/**
 * @brief Reset device state
 * @param dev Device handle
 */
void uft_mock_reset(uft_mock_device_t *dev);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Data Loading
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Load disk image into mock device
 * @param dev Device handle
 * @param path Path to disk image
 * @return UFT_OK on success
 */
uft_error_t uft_mock_load_image(uft_mock_device_t *dev, const char *path);

/**
 * @brief Load flux data from file
 * @param dev Device handle
 * @param path Path to flux file (SCP, HFE, etc.)
 * @return UFT_OK on success
 */
uft_error_t uft_mock_load_flux(uft_mock_device_t *dev, const char *path);

/**
 * @brief Generate test pattern
 * @param dev Device handle
 * @param pattern Pattern type (0=zeros, 1=ones, 2=random, 3=sequential)
 * @return UFT_OK on success
 */
uft_error_t uft_mock_generate_pattern(uft_mock_device_t *dev, int pattern);

/**
 * @brief Set track data directly
 * @param dev Device handle
 * @param cylinder Cylinder number
 * @param head Head number
 * @param data Track data
 * @param size Data size
 * @return UFT_OK on success
 */
uft_error_t uft_mock_set_track(uft_mock_device_t *dev, int cylinder, int head,
                                const uint8_t *data, size_t size);

/**
 * @brief Set flux data directly
 * @param dev Device handle
 * @param cylinder Cylinder number
 * @param head Head number
 * @param flux Flux timing data (ns between transitions)
 * @param count Number of flux transitions
 * @return UFT_OK on success
 */
uft_error_t uft_mock_set_flux(uft_mock_device_t *dev, int cylinder, int head,
                               const uint32_t *flux, size_t count);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Read/Write Operations (HAL-compatible)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read track data
 * @param dev Device handle
 * @param cylinder Cylinder number
 * @param head Head number
 * @param buffer Output buffer
 * @param size Buffer size
 * @param bytes_read Output: bytes actually read
 * @return UFT_OK on success
 */
uft_error_t uft_mock_read_track(uft_mock_device_t *dev, int cylinder, int head,
                                 uint8_t *buffer, size_t size, size_t *bytes_read);

/**
 * @brief Write track data
 * @param dev Device handle
 * @param cylinder Cylinder number
 * @param head Head number
 * @param data Data to write
 * @param size Data size
 * @return UFT_OK on success
 */
uft_error_t uft_mock_write_track(uft_mock_device_t *dev, int cylinder, int head,
                                  const uint8_t *data, size_t size);

/**
 * @brief Read sector
 * @param dev Device handle
 * @param cylinder Cylinder number
 * @param head Head number
 * @param sector Sector number
 * @param buffer Output buffer
 * @param size Buffer size
 * @return UFT_OK on success
 */
uft_error_t uft_mock_read_sector(uft_mock_device_t *dev, int cylinder, int head,
                                  int sector, uint8_t *buffer, size_t size);

/**
 * @brief Write sector
 * @param dev Device handle
 * @param cylinder Cylinder number
 * @param head Head number
 * @param sector Sector number
 * @param data Data to write
 * @param size Data size
 * @return UFT_OK on success
 */
uft_error_t uft_mock_write_sector(uft_mock_device_t *dev, int cylinder, int head,
                                   int sector, const uint8_t *data, size_t size);

/**
 * @brief Read raw flux
 * @param dev Device handle
 * @param cylinder Cylinder number
 * @param head Head number
 * @param flux Output flux data
 * @param max_transitions Maximum transitions
 * @param count Output: actual transitions
 * @return UFT_OK on success
 */
uft_error_t uft_mock_read_flux(uft_mock_device_t *dev, int cylinder, int head,
                                uint32_t *flux, size_t max_transitions, size_t *count);

/**
 * @brief Write raw flux
 * @param dev Device handle
 * @param cylinder Cylinder number
 * @param head Head number
 * @param flux Flux timing data
 * @param count Number of transitions
 * @return UFT_OK on success
 */
uft_error_t uft_mock_write_flux(uft_mock_device_t *dev, int cylinder, int head,
                                 const uint32_t *flux, size_t count);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Control Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Seek to cylinder
 * @param dev Device handle
 * @param cylinder Target cylinder
 * @return UFT_OK on success
 */
uft_error_t uft_mock_seek(uft_mock_device_t *dev, int cylinder);

/**
 * @brief Select head
 * @param dev Device handle
 * @param head Head number
 * @return UFT_OK on success
 */
uft_error_t uft_mock_select_head(uft_mock_device_t *dev, int head);

/**
 * @brief Motor control
 * @param dev Device handle
 * @param on true to start motor
 * @return UFT_OK on success
 */
uft_error_t uft_mock_motor(uft_mock_device_t *dev, bool on);

/**
 * @brief Get index pulse position
 * @param dev Device handle
 * @return Index position in track (0-based sample number)
 */
int uft_mock_get_index(uft_mock_device_t *dev);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Error Injection
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Add error injection rule
 * @param dev Device handle
 * @param config Error configuration
 * @return UFT_OK on success
 */
uft_error_t uft_mock_add_error(uft_mock_device_t *dev, const uft_mock_error_config_t *config);

/**
 * @brief Clear all error injection rules
 * @param dev Device handle
 */
void uft_mock_clear_errors(uft_mock_device_t *dev);

/**
 * @brief Set global error rate
 * @param dev Device handle
 * @param rate Error rate 0.0-1.0
 */
void uft_mock_set_error_rate(uft_mock_device_t *dev, float rate);

/**
 * @brief Inject weak bits at position
 * @param dev Device handle
 * @param cylinder Cylinder
 * @param head Head
 * @param bit_offset Bit offset in track
 * @param count Number of weak bits
 */
void uft_mock_inject_weak_bits(uft_mock_device_t *dev, int cylinder, int head,
                                size_t bit_offset, size_t count);

/* ═══════════════════════════════════════════════════════════════════════════════
 * State Control
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Set write protect state
 * @param dev Device handle
 * @param protect true for write protected
 */
void uft_mock_set_write_protect(uft_mock_device_t *dev, bool protect);

/**
 * @brief Set disk present state
 * @param dev Device handle
 * @param present true if disk is present
 */
void uft_mock_set_disk_present(uft_mock_device_t *dev, bool present);

/**
 * @brief Get device configuration
 * @param dev Device handle
 * @return Configuration (do not free)
 */
const uft_mock_config_t *uft_mock_get_config(const uft_mock_device_t *dev);

/**
 * @brief Get device statistics
 * @param dev Device handle
 * @return Statistics (do not free)
 */
const uft_mock_stats_t *uft_mock_get_stats(const uft_mock_device_t *dev);

/**
 * @brief Reset statistics
 * @param dev Device handle
 */
void uft_mock_reset_stats(uft_mock_device_t *dev);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Logging and Debugging
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Enable/disable operation logging
 * @param dev Device handle
 * @param enable true to enable
 */
void uft_mock_set_logging(uft_mock_device_t *dev, bool enable);

/**
 * @brief Get operation log
 * @param dev Device handle
 * @return Log string (caller must free)
 */
char *uft_mock_get_log(const uft_mock_device_t *dev);

/**
 * @brief Clear operation log
 * @param dev Device handle
 */
void uft_mock_clear_log(uft_mock_device_t *dev);

/**
 * @brief Export device state as JSON
 * @param dev Device handle
 * @return JSON string (caller must free)
 */
char *uft_mock_export_state(const uft_mock_device_t *dev);

/**
 * @brief Save disk contents to image file
 * @param dev Device handle
 * @param path Output path
 * @param format Output format
 * @return UFT_OK on success
 */
uft_error_t uft_mock_save_image(const uft_mock_device_t *dev, const char *path,
                                 uft_format_t format);

/* ═══════════════════════════════════════════════════════════════════════════════
 * HAL Integration
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Register mock device with HAL
 * @param dev Mock device
 * @return UFT_OK on success
 */
uft_error_t uft_mock_register_hal(uft_mock_device_t *dev);

/**
 * @brief Unregister mock device from HAL
 * @param dev Mock device
 */
void uft_mock_unregister_hal(uft_mock_device_t *dev);

/**
 * @brief Check if mock mode is active
 * @return true if using mock device
 */
bool uft_mock_is_active(void);

/**
 * @brief Get current mock device (if registered)
 * @return Mock device or NULL
 */
uft_mock_device_t *uft_mock_get_active(void);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Test Data Generation
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generate Amiga DD disk
 * @param dev Device handle
 * @return UFT_OK on success
 */
uft_error_t uft_mock_gen_amiga_dd(uft_mock_device_t *dev);

/**
 * @brief Generate C64 disk (35 tracks)
 * @param dev Device handle
 * @return UFT_OK on success
 */
uft_error_t uft_mock_gen_c64(uft_mock_device_t *dev);

/**
 * @brief Generate PC 720K disk
 * @param dev Device handle
 * @return UFT_OK on success
 */
uft_error_t uft_mock_gen_pc_720k(uft_mock_device_t *dev);

/**
 * @brief Generate PC 1.44M disk
 * @param dev Device handle
 * @return UFT_OK on success
 */
uft_error_t uft_mock_gen_pc_1440k(uft_mock_device_t *dev);

/**
 * @brief Generate Apple II disk
 * @param dev Device handle
 * @return UFT_OK on success
 */
uft_error_t uft_mock_gen_apple2(uft_mock_device_t *dev);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** Get mock type name */
const char *uft_mock_type_name(uft_mock_type_t type);

/** Get drive type name */
const char *uft_mock_drive_name(uft_mock_drive_t drive);

/** Get error type name */
const char *uft_mock_error_name(uft_mock_error_t error);

/** Print device info */
void uft_mock_print_info(const uft_mock_device_t *dev);

/** Print statistics */
void uft_mock_print_stats(const uft_mock_device_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* UFT_HARDWARE_MOCK_H */
