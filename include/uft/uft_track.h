/**
 * @file uft_track.h
 * @brief Unified Track Data Structure (P1-4: Zentralisierung)
 * 
 * This is the SINGLE CANONICAL track definition for UFT.
 * All modules MUST use this structure.
 * 
 * Design Goals:
 * - Superset of all 7 previous track definitions
 * - Supports Flux, Bitstream, and Sector layers
 * - Preserves timing and weak-bit information
 * - Supports multi-revolution captures
 * - Supports quarter-tracks (CBM/Apple)
 * - Clear ownership rules (caller-owns-buffer)
 * 
 * @author UFT Team
 * @date 2025
 * @version 3.7.0
 */

#ifndef UFT_TRACK_H
#define UFT_TRACK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "uft/uft_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_MAX_SECTORS 64          /**< Max sectors per track (legacy compat) */
#define UFT_TRACK_MAGIC 0x54524B32  /**< "TRK2" validation magic */
#define UFT_TRACK_VERSION 2         /**< Structure version */

/* ═══════════════════════════════════════════════════════════════════════════
 * Encoding Type - use uint32_t for compatibility with uft_types.h
 * Values: 0=Unknown, 1=FM, 2=MFM, etc.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Define local constants only if uft_types.h not included */
#ifndef UFT_ENC_UNKNOWN
#define UFT_ENC_UNKNOWN   0
#define UFT_ENC_FM        1
#define UFT_ENC_MFM       2
#define UFT_ENC_GCR_CBM   3
#define UFT_ENC_GCR_APPLE 4
#define UFT_ENC_GCR_VICTOR 5
#define UFT_ENC_AMIGA     6
#define UFT_ENC_RAW       7
#define UFT_ENC_COUNT     8
#endif

/* Use uint32_t for encoding field to avoid type conflicts */
typedef uint32_t uft_track_encoding_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Data Layer Flags
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum uft_layer_flags {
    UFT_LAYER_NONE      = 0,
    UFT_LAYER_FLUX      = (1 << 0),  /**< Flux timing data available */
    UFT_LAYER_BITSTREAM = (1 << 1),  /**< Decoded bitstream available */
    UFT_LAYER_SECTORS   = (1 << 2),  /**< Decoded sectors available */
    UFT_LAYER_TIMING    = (1 << 3),  /**< Per-bit timing available */
    UFT_LAYER_WEAK      = (1 << 4),  /**< Weak-bit mask available */
    UFT_LAYER_INDEX     = (1 << 5),  /**< Index positions available */
    UFT_LAYER_MULTIREV  = (1 << 6),  /**< Multi-revolution data */
} uft_layer_flags_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Track Status Flags - use defines for compatibility
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_TRACK_OK
#define UFT_TRACK_OK            0
#define UFT_TRACK_UNFORMATTED   (1 << 0)
#define UFT_TRACK_CRC_ERRORS    (1 << 1)
#define UFT_TRACK_MISSING_DATA  (1 << 2)
#define UFT_TRACK_PROTECTED     (1 << 3)
#define UFT_TRACK_WEAK_BITS     (1 << 4)
#define UFT_TRACK_LONG          (1 << 5)
#define UFT_TRACK_SHORT         (1 << 6)
#define UFT_TRACK_HALF          (1 << 7)
#define UFT_TRACK_QUARTER       (1 << 8)
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Quality Metrics
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_TRACK_QUALITY_T_DEFINED
#define UFT_TRACK_QUALITY_T_DEFINED
typedef struct uft_track_quality {
    double  avg_bit_cell_ns;    /**< Average bit cell time (ns) */
    double  jitter_ns;          /**< Timing jitter (ns) */
    double  jitter_percent;     /**< Jitter as % of bit cell */
    int     decode_errors;      /**< PLL/decode errors */
    float   confidence;         /**< Detection confidence 0.0-1.0 */
    int     signal_strength;    /**< 0-100 */
} uft_track_quality_t;
#endif /* UFT_TRACK_QUALITY_T_DEFINED */

/* uft_sector_t — canonical definition in uft/uft_types.h (included above) */

/* ═══════════════════════════════════════════════════════════════════════════
 * Flux Data Layer
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct uft_flux_layer {
    uint32_t*   samples;            /**< Flux timing samples */
    size_t      sample_count;
    size_t      sample_capacity;
    
    double      sample_rate_mhz;    /**< Sample clock rate */
    uint32_t    tick_ns;            /**< Tick duration in ns */
    double      index_time_us;      /**< Time between index pulses */
    
    int         revolution;         /**< Current revolution (0-based) */
    int         total_revolutions;  /**< Total captured */
} uft_flux_layer_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Bitstream Data Layer
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct uft_bitstream_layer {
    uint8_t*    bits;               /**< Packed bits (MSB first) */
    size_t      bit_count;
    size_t      byte_count;
    size_t      capacity;
    
    double      bit_rate_kbps;      /**< Nominal bit rate */
    
    /* Optional per-bit timing */
    uint16_t*   timing;             /**< NULL if not available */
    size_t      timing_count;
    
    /* Optional weak-bit mask */
    uint8_t*    weak_mask;          /**< 1 = weak, NULL if none */
    
    /* Index positions (bit offsets) */
    size_t*     index_positions;
    int         index_count;
} uft_bitstream_layer_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Sector Data Layer
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct uft_sector_layer {
    uft_sector_t*   sectors;
    size_t          count;
    size_t          capacity;
    
    /* Statistics */
    int expected;
    int found;
    int good;
    int bad;
    int missing;
} uft_sector_layer_t;

/* uft_track_t — canonical definition in uft/uft_format_plugin.h */
#include "uft/uft_format_plugin.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * Lifecycle Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Allocate a new track with specified layers
 * @param layers Bitmask of uft_layer_flags_t
 * @param bit_count Initial bit capacity
 * @return New track or NULL
 */
uft_track_t* uft_track_alloc(uint32_t layers, size_t bit_count);

/**
 * @brief Free track and all internal data
 */
void uft_track_free(uft_track_t *track);

/**
 * @brief Initialize existing track (for stack allocation)
 */
/* uft_track_init: canonical declaration in uft_format_plugin.h
 * (has additional cylinder, head params) */
#ifndef UFT_TRACK_INIT_DECLARED
/* Canonical signature: uft_track_init(track, cyl, head) in uft_format_plugin.h
 * This legacy no-arg version exists for backward compat */
void uft_track_init(uft_track_t *track, int cylinder, int head);
#define UFT_TRACK_INIT_DECLARED
#endif

/**
 * @brief Clone a track (deep copy)
 */
uft_track_t* uft_track_clone(const uft_track_t *src);

/**
 * @brief Clear internal data without freeing track
 */
void uft_track_clear(uft_track_t *track);

/* ═══════════════════════════════════════════════════════════════════════════
 * Layer Management
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_track_add_layer(uft_track_t *track, uft_layer_flags_t layer, size_t capacity);
void uft_track_remove_layer(uft_track_t *track, uft_layer_flags_t layer);

static inline bool uft_track_has_layer(const uft_track_t *track, uft_layer_flags_t layer) {
    return track && (track->available_layers & layer);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Bitstream Operations
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_track_set_bits(uft_track_t *track, const uint8_t *bits, size_t bit_count);
int uft_track_get_bits(const uft_track_t *track, uint8_t *bits, size_t *bit_count);
int uft_track_set_timing(uft_track_t *track, const uint16_t *timing, size_t count);
int uft_track_set_weak_mask(uft_track_t *track, const uint8_t *mask, size_t byte_count);

/* ═══════════════════════════════════════════════════════════════════════════
 * Sector Operations
 * ═══════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_track_add_sector(uft_track_t *track, const uft_sector_t *sector);
const uft_sector_t* uft_track_get_sector(const uft_track_t *track, int record);
const uft_sector_t* uft_track_get_sectors(const uft_track_t *track, size_t *count);

/* ═══════════════════════════════════════════════════════════════════════════
 * Flux Operations
 * ═══════════════════════════════════════════════════════════════════════════ */

/* uft_track_set_flux: canonical in uft_format_plugin.h (returns uft_error_t) */
#ifndef UFT_TRACK_SET_FLUX_DECLARED
int uft_track_set_flux(uft_track_t *track, const uint32_t *samples,
                       size_t count, double sample_rate_mhz);
#endif
int uft_track_add_revolution(uft_track_t *track, const uint32_t *samples, size_t count);

/* ═══════════════════════════════════════════════════════════════════════════
 * Validation
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_track_compare(const uft_track_t *a, const uft_track_t *b);
int uft_track_validate(const uft_track_t *track);
const char* uft_track_status_str(const uft_track_t *track, char *buf, size_t buf_size);

/* ═══════════════════════════════════════════════════════════════════════════
 * Convenience Macros
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_TRACK_VALID(t) ((t) && (t)->_magic == UFT_TRACK_MAGIC)

#define UFT_TRACK_BIT_COUNT(t) \
    (((t) && (t)->bitstream) ? (t)->bitstream->bit_count : \
     ((t) && (t)->raw_data) ? (t)->raw_len * 8 : 0)

#define UFT_TRACK_SECTOR_COUNT(t) \
    (((t) && (t)->sector_layer) ? (t)->sector_layer->count : \
     (t) ? (t)->sector_count : 0)

#define UFT_TRACK_POS_FMT "Cyl %02d Head %d"
#define UFT_TRACK_POS_ARGS(t) (t)->cylinder, (t)->head

#ifdef __cplusplus
}
#endif

#endif /* UFT_TRACK_H */
