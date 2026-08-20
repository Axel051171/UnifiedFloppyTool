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

/* The encoding constants come from uft_types.h, included above — there is no
 * fallback here on purpose.
 *
 * There used to be one, guarded by `#ifndef UFT_ENC_UNKNOWN`. That guard could
 * never fire: in uft_types.h UFT_ENC_UNKNOWN is an enum constant, not a macro,
 * so the preprocessor never sees it and the fallback was always defined. Its
 * numbers disagreed with the enum — UFT_ENC_GCR_CBM was 3 here and 9 there —
 * so the same name meant two different things depending on which headers a
 * translation unit pulled in. Plugins (uft_types.h only) wrote 9; anything that
 * also included this header compared against 3 and never matched.
 *
 * Found by test_corpus_cbm_vice (MF-427) when a G71 track read back with an
 * encoding that could not be named. Do not reintroduce a local copy: if a
 * constant is missing, add it to uft_types.h. */

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
 * Track Status Flags
 *
 * Canonical: enum uft_track_status in uft/uft_types.h (included above).
 * A former compat #define block here carried DIFFERENT values (UNFORMATTED
 * was 1<<0 vs the canonical 1<<2) and — because #ifndef cannot see enum
 * constants — silently shadowed the enum in every TU that included this
 * header, so setters and readers could disagree on what a status value
 * means. Removed in MF-371 (AUD-3); all names now resolve to the enum.
 * Do NOT reintroduce value-carrying macros for these names.
 * ═══════════════════════════════════════════════════════════════════════════ */

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
    
    /* Optional weak-bit mask.
     *
     * Granularity (C) of four — see KNOWN_ISSUES PROT-12. This layer is
     * currently UNPOPULATED: no code in the tree fills a
     * uft_bitstream_layer_t. When it is implemented, the mask indexes `bits`
     * and must therefore follow the PER-BIT convention of
     * struct uft_track.weak_mask (one entry per bit, not bit-packed), NOT the
     * per-byte convention of struct uft_sector.weak_mask. Written down now
     * because an unstated granularity on an empty field is what a future
     * implementer would have had to guess. */
    uint8_t*    weak_mask;          /**< 1 = weak (per BIT), NULL if none */
    
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
 * @brief Allocate a heap track with room for sectors and raw bits.
 *
 * @param max_sectors  Sector capacity to preallocate
 * @param max_raw_bits Raw-data capacity in bits, 0 for none
 * @return New track (owns_data = true) or NULL. Release with uft_track_free().
 *
 * MF-433: this used to be declared here as
 * `uft_track_alloc(uint32_t layers, size_t bit_count)` — a signature that
 * never existed. The one definition, uft_unified_types.c:224, takes a sector
 * count, and all fourteen callers pass one. The wrong declaration survived
 * because no translation unit both included this header and called the
 * function; the first one to do so failed to compile, which is how it was
 * found. Same shape as the enum-vs-macro class (ARCH-5), one level up: one
 * fact, three headers, one of them wrong.
 */
uft_track_t* uft_track_alloc(size_t max_sectors, size_t max_raw_bits);

/**
 * @brief Free track and all internal data
 */
/**
 * @brief Release everything a track OWNS, leaving the struct itself alone.
 *
 * Use this for tracks that live on the stack or inside another object — which
 * is nearly all of them, because plugin->read_track() fills a caller-provided
 * uft_track_t. After the call the struct is zeroed where it mattered and can
 * be released again or refilled.
 *
 * MF-433: uft_track_free() ends in free(track). Twenty-two of its twenty-nine
 * call sites passed a stack address, eighteen of them in the generic
 * verify_track helper that plugins wire up — free() on a stack pointer, the
 * heap corruption only hidden because owns_data was false and the interesting
 * part of the function was skipped. Two roundtrip tests carry comments
 * explaining the trap instead of a fix. This is the fix.
 */
void uft_track_release(uft_track_t *track);

/**
 * @brief Release the track AND free the struct.
 *
 * Only for tracks obtained from uft_track_alloc() or otherwise heap-allocated.
 * For a stack track use uft_track_release().
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



/* ═══════════════════════════════════════════════════════════════════════════
 * Layer Management
 * ═══════════════════════════════════════════════════════════════════════════ */


static inline bool uft_track_has_layer(const uft_track_t *track, uft_layer_flags_t layer) {
    return track && (track->available_layers & layer);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Bitstream Operations
 * ═══════════════════════════════════════════════════════════════════════════ */


/* ═══════════════════════════════════════════════════════════════════════════
 * Sector Operations
 * ═══════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_track_add_sector(uft_track_t *track, const uft_sector_t *sector);
const uft_sector_t* uft_track_get_sector(const uft_track_t *track, int record);

/* ═══════════════════════════════════════════════════════════════════════════
 * Flux Operations
 * ═══════════════════════════════════════════════════════════════════════════ */

/* uft_track_set_flux: canonical in uft_format_plugin.h (returns uft_error_t) */
#ifndef UFT_TRACK_SET_FLUX_DECLARED
int uft_track_set_flux(uft_track_t *track, const uint32_t *samples,
                       size_t count, double sample_rate_mhz);
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Validation
 * ═══════════════════════════════════════════════════════════════════════════ */


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
