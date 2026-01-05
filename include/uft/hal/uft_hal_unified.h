/**
 * @file uft_hal_unified.h
 * @brief Unified Hardware Abstraction Layer
 * @version 2.0.0
 * 
 * Hub format for raw track data, supporting multiple controllers.
 */

#ifndef UFT_HAL_UNIFIED_H
#define UFT_HAL_UNIFIED_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * HAL Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_HAL_NONE = 0,
    UFT_HAL_GREASEWEAZLE,
    UFT_HAL_FLUXENGINE,
    UFT_HAL_KRYOFLUX,
    UFT_HAL_FC5025,
    UFT_HAL_XUM1541,
    UFT_HAL_ZOOMFLOPPY,
    UFT_HAL_APPLESAUCE,
    UFT_HAL_SCP,
    UFT_HAL_PAULINE,
    UFT_HAL_COUNT
} uft_hal_type_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Capability Flags
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_HAL_CAP_READ_FLUX   (1 << 0)   /**< Can read raw flux */
#define UFT_HAL_CAP_WRITE_FLUX  (1 << 1)   /**< Can write raw flux */
#define UFT_HAL_CAP_READ_MFM    (1 << 2)   /**< Can read decoded MFM */
#define UFT_HAL_CAP_WRITE_MFM   (1 << 3)   /**< Can write encoded MFM */
#define UFT_HAL_CAP_MULTI_REV   (1 << 4)   /**< Supports multi-revolution capture */
#define UFT_HAL_CAP_HD          (1 << 5)   /**< Supports high-density */
#define UFT_HAL_CAP_INDEX       (1 << 6)   /**< Has index pulse sensing */
#define UFT_HAL_CAP_GCR_NATIVE  (1 << 7)   /**< Native GCR support (C64/1541) */
#define UFT_HAL_CAP_HALF_TRACK  (1 << 8)   /**< Supports half-track stepping */

/* ═══════════════════════════════════════════════════════════════════════════════
 * Raw Track Structures (Hub Format)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** @brief Single revolution data */
typedef struct {
    uint32_t* flux;           /**< Flux timings (in sample clock ticks) */
    size_t flux_count;        /**< Number of flux transitions */
    uint32_t index_offset;    /**< Offset to index pulse */
    uint32_t duration_ns;     /**< Revolution duration in nanoseconds */
} uft_revolution_t;

/** @brief Raw track data (hub format) */
typedef struct {
    int track;                /**< Track number */
    int side;                 /**< Side (0 or 1) */
    
    /* Single-revolution data (simple case) */
    uint32_t* flux;           /**< Flux timings */
    size_t flux_count;        /**< Number of transitions */
    
    /* Multi-revolution data */
    uft_revolution_t* revolutions;
    int revolution_count;
    
    /* Metadata */
    uint32_t sample_rate_hz;  /**< Sample clock frequency */
    uint32_t index_time_ns;   /**< Index to index time */
    uft_hal_type_t source;    /**< Source controller */
} uft_raw_track_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * HAL Info Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_hal_type_t type;      /**< Controller type */
    char name[64];            /**< Human-readable name */
    char device[256];         /**< Device path */
    char version[32];         /**< Firmware version */
    uint32_t caps;            /**< Capability flags */
} uft_hal_info_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * HAL Driver Interface
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct uft_hal_s uft_hal_t;

typedef struct {
    const char* name;
    uft_hal_type_t type;
    
    int (*open)(uft_hal_t* hal, const char* device);
    void (*close)(uft_hal_t* hal);
    int (*read_track)(uft_hal_t* hal, uft_raw_track_t* track);
    int (*write_track)(uft_hal_t* hal, const uft_raw_track_t* track);
    int (*seek)(uft_hal_t* hal, int track);
    int (*motor)(uft_hal_t* hal, bool on);
} uft_hal_driver_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** @brief Get type name */
const char* uft_hal_type_name(uft_hal_type_t type);

/** @brief Get type capabilities */
uint32_t uft_hal_type_caps(uft_hal_type_t type);

/** @brief Check if type is available on this system */
bool uft_hal_type_available(uft_hal_type_t type);

/** @brief Enumerate available hardware */
int uft_hal_enumerate(uft_hal_info_t* infos, int max_count);

/** @brief Open hardware by type */
uft_hal_t* uft_hal_open(uft_hal_type_t type, const char* device);

/** @brief Close hardware */
void uft_hal_close(uft_hal_t* hal);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Raw Track Utilities
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** @brief Initialize raw track structure */
void uft_raw_track_init(uft_raw_track_t* track);

/** @brief Free raw track data */
void uft_raw_track_free(uft_raw_track_t* track);

/** @brief Clone raw track */
uft_raw_track_t* uft_raw_track_clone(const uft_raw_track_t* track);

#ifdef __cplusplus
}
#endif

#endif /* UFT_HAL_UNIFIED_H */
