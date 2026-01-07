/**
 * @file uft_decoder_adapter.h
 * @brief Decoder Adapter API - Flux to Bitstream to Sector
 * 
 * DECODER ADAPTER ARCHITECTURE:
 * ════════════════════════════════════════════════════════════════════════════
 * 
 *   ┌─────────────────────────────────────────────────────────────────────────┐
 *   │                       DECODER PIPELINE                                  │
 *   ├─────────────────────────────────────────────────────────────────────────┤
 *   │                                                                         │
 *   │   FLUX (SCP)          BITSTREAM             SECTORS                     │
 *   │   ──────────          ─────────             ───────                     │
 *   │                                                                         │
 *   │   ┌────────┐          ┌────────┐           ┌────────┐                   │
 *   │   │ Flux   │  ──────► │ Bit    │  ───────► │ Sector │                   │
 *   │   │ Timing │  PLL     │ Stream │  Sync     │ Data   │                   │
 *   │   └────────┘  Decode  └────────┘  Decode   └────────┘                   │
 *   │        │                   │                    │                       │
 *   │        ▼                   ▼                    ▼                       │
 *   │   ┌────────┐          ┌────────┐           ┌────────┐                   │
 *   │   │  SCP   │          │  G64   │           │  D64   │                   │
 *   │   │Kryoflux│          │  HFE   │           │  ADF   │                   │
 *   │   │  A2R   │          │  NIB   │           │  IMG   │                   │
 *   │   └────────┘          └────────┘           └────────┘                   │
 *   │                                                                         │
 *   └─────────────────────────────────────────────────────────────────────────┘
 * 
 * MINIMALE DECODER APIs:
 * ════════════════════════════════════════════════════════════════════════════
 * 
 *   1. PLL DECODER (Flux → Bitstream)
 *      - pll_init(params)
 *      - pll_decode_flux(samples, count) → bits
 *      - pll_get_bitcells() → timing info
 * 
 *   2. SYNC DETECTOR (Bitstream → Sectors)
 *      - sync_find(bits, pattern) → offset
 *      - header_decode(bits, offset) → header
 *      - data_decode(bits, offset, size) → sector data
 * 
 *   3. ENCODER (Sector → Bitstream)
 *      - encode_header(track, sector) → bits
 *      - encode_data(data, size) → bits
 *      - encode_gap(type, length) → bits
 * 
 * AUSTAUSCHFORMAT (Track/Bitstream):
 * ════════════════════════════════════════════════════════════════════════════
 * 
 *   Das uft_raw_track_t ist das zentrale Zwischenformat:
 *   
 *   ┌────────────────────────────────────────────────────────────────────────┐
 *   │ uft_raw_track_t                                                        │
 *   ├────────────────────────────────────────────────────────────────────────┤
 *   │ • cylinder, head                                                       │
 *   │ • encoding (MFM/FM/GCR_CBM/GCR_APPLE)                                 │
 *   │ • bit_count                                                            │
 *   │ • bits[] (packed bitstream)                                           │
 *   │ • timing[] (optional: per-bit timing in ns)                           │
 *   │ • weak_mask[] (optional: weak bit positions)                          │
 *   │ • index_positions[] (index hole positions)                            │
 *   └────────────────────────────────────────────────────────────────────────┘
 *   
 *   Dieses Format kann:
 *   - Aus SCP/Kryoflux/HFE geladen werden
 *   - Nach G64/NIB/HFE geschrieben werden
 *   - Von allen Decodern verarbeitet werden
 */

#ifndef UFT_DECODER_ADAPTER_H
#define UFT_DECODER_ADAPTER_H

#include "uft_types.h"
#include "uft_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Encoding Types
// ============================================================================

/* uft_encoding_t is defined in uft_types.h (already included) */

// ============================================================================
// Raw Track (Universal Bitstream Format)
// ============================================================================

/**
 * @brief Universal bitstream track format
 * 
 * This is the common interchange format between:
 * - flux hardware (produces raw flux)
 * - HFE files
 * - Any decoder/encoder
 */
typedef struct uft_raw_track {
    // Identity
    int             cylinder;
    int             head;
    bool            is_half_track;      // For CBM/Apple
    
    // Encoding info
    uft_encoding_t  encoding;
    double          nominal_bit_rate;   // kbps
    double          nominal_rpm;
    
    // Bitstream data
    uint8_t*        bits;               // Packed bits (MSB first)
    size_t          bit_count;          // Total bits
    size_t          byte_count;         // = (bit_count + 7) / 8
    
    // Optional timing (per-bit, in nanoseconds)
    uint16_t*       timing;             // NULL if not available
    size_t          timing_count;       // Should equal bit_count
    
    // Weak bits (1 = weak/uncertain)
    uint8_t*        weak_mask;          // NULL if no weak bits
    
    // Index positions (bit offsets where index hole occurs)
    size_t*         index_positions;
    int             index_count;
    
    // Revolution data (for multi-rev captures)
    int             revolution;         // Which revolution (0-based)
    int             total_revolutions;
    
    // Quality metrics
    double          avg_bit_cell_ns;
    double          jitter_ns;
    int             decode_errors;
} uft_raw_track_t;

// ============================================================================
// PLL Decoder Interface
// ============================================================================

typedef struct uft_pll_params {
    double          nominal_bit_cell_ns;    // Expected bit cell time
    double          pll_bandwidth;          // PLL tracking (0.0-1.0)
    double          clock_tolerance;        // Allowed deviation (%)
    int             history_bits;           // Bits to average over
    bool            detect_weak_bits;
    double          weak_threshold;         // Flux amplitude threshold
} uft_pll_params_t;

typedef struct uft_pll_state uft_pll_state_t;

/**
 * @brief Initialize PLL decoder
 */
uft_pll_state_t* uft_pll_create(const uft_pll_params_t* params);

/**
 * @brief Decode flux samples to bitstream
 * @param pll PLL state
 * @param flux_samples Flux timing samples (in sample clock units)
 * @param sample_count Number of samples
 * @param sample_rate_mhz Sample clock rate
 * @param output Output raw track (allocated by caller)
 * @return UFT_OK on success
 */
uft_error_t uft_pll_decode(uft_pll_state_t* pll,
                            const uint32_t* flux_samples,
                            size_t sample_count,
                            double sample_rate_mhz,
                            uft_raw_track_t* output);

/**
 * @brief Get PLL statistics
 */
typedef struct uft_pll_stats {
    double      avg_bit_cell_ns;
    double      min_bit_cell_ns;
    double      max_bit_cell_ns;
    double      jitter_ns;
    int         pll_locks;
    int         pll_unlocks;
    int         flux_count;
    int         bit_count;
} uft_pll_stats_t;

void uft_pll_get_stats(const uft_pll_state_t* pll, uft_pll_stats_t* stats);
void uft_pll_destroy(uft_pll_state_t* pll);

// ============================================================================
// Sync/Sector Decoder Interface  
// ============================================================================

/**
 * @brief Decoded sector header
 */
typedef struct uft_sector_header {
    int         cylinder;
    int         head;
    int         sector;
    int         size_code;      // 0=128, 1=256, 2=512, 3=1024
    uint16_t    header_crc;
    bool        crc_valid;
    size_t      bit_offset;     // Where header was found
} uft_sector_header_t;

/**
 * @brief Decoded sector data
 */
typedef struct uft_sector_data {
    uint8_t*    data;
    size_t      size;
    uint16_t    data_crc;
    bool        crc_valid;
    bool        deleted_mark;   // Deleted data address mark
    size_t      bit_offset;
} uft_sector_data_t;

typedef struct uft_sync_decoder uft_sync_decoder_t;

/**
 * @brief Create sync decoder for encoding type
 */
uft_sync_decoder_t* uft_sync_create(uft_encoding_t encoding);

/**
 * @brief Find all sectors in track
 */
int uft_sync_find_sectors(uft_sync_decoder_t* dec,
                           const uft_raw_track_t* track,
                           uft_sector_header_t* headers,
                           int max_sectors);

/**
 * @brief Decode sector data after header
 */
uft_error_t uft_sync_decode_sector(uft_sync_decoder_t* dec,
                                    const uft_raw_track_t* track,
                                    const uft_sector_header_t* header,
                                    uft_sector_data_t* data);

void uft_sync_destroy(uft_sync_decoder_t* dec);

// ============================================================================
// Encoder Interface (Sector → Bitstream)
// ============================================================================

typedef struct uft_encoder uft_encoder_t;

/**
 * @brief Create encoder for encoding type
 */
uft_encoder_t* uft_encoder_create(uft_encoding_t encoding);

/**
 * @brief Set track parameters
 */
typedef struct uft_track_format {
    int         sectors_per_track;
    int         sector_size;
    int         interleave;
    int         gap1_bytes;     // Post-index gap
    int         gap2_bytes;     // Post-ID gap
    int         gap3_bytes;     // Post-data gap
    int         gap4_bytes;     // Pre-index gap
    uint8_t     fill_byte;
} uft_track_format_t;

uft_error_t uft_encoder_set_format(uft_encoder_t* enc,
                                    const uft_track_format_t* fmt);

/**
 * @brief Encode complete track from sectors
 */
uft_error_t uft_encoder_encode_track(uft_encoder_t* enc,
                                      int cylinder, int head,
                                      const uint8_t* sector_data[],
                                      int sector_count,
                                      uft_raw_track_t* output);

void uft_encoder_destroy(uft_encoder_t* enc);

// ============================================================================
// Decoder Adapter (High-Level Interface)
// ============================================================================

/**
 * @brief Decoder adapter combines PLL + Sync decoder
 */
typedef struct uft_decoder_adapter uft_decoder_adapter_t;

/**
 * @brief Create decoder adapter for format
 */
uft_decoder_adapter_t* uft_decoder_adapter_create(uft_encoding_t encoding);

/**
 * @brief Decode flux to sectors (full pipeline)
 * 
 * Flux → [PLL] → Bitstream → [Sync] → Sectors
 */
uft_error_t uft_decoder_adapter_flux_to_sectors(
    uft_decoder_adapter_t* dec,
    const uint32_t* flux_samples,
    size_t sample_count,
    double sample_rate_mhz,
    uft_sector_data_t* sectors,
    int* sector_count,
    int max_sectors);

/**
 * @brief Decode flux to bitstream only
 */
uft_error_t uft_decoder_adapter_flux_to_bitstream(
    uft_decoder_adapter_t* dec,
    const uint32_t* flux_samples,
    size_t sample_count,
    double sample_rate_mhz,
    uft_raw_track_t* output);

/**
 * @brief Decode bitstream to sectors
 */
uft_error_t uft_decoder_adapter_bitstream_to_sectors(
    uft_decoder_adapter_t* dec,
    const uft_raw_track_t* track,
    uft_sector_data_t* sectors,
    int* sector_count,
    int max_sectors);

/**
 * @brief Encode sectors to bitstream
 */
uft_error_t uft_decoder_adapter_sectors_to_bitstream(
    uft_decoder_adapter_t* dec,
    int cylinder, int head,
    const uft_sector_data_t* sectors,
    int sector_count,
    uft_raw_track_t* output);

void uft_decoder_adapter_destroy(uft_decoder_adapter_t* dec);

// ============================================================================
// Memory Management
// ============================================================================

void uft_raw_track_init(uft_raw_track_t* track);
void uft_raw_track_free(uft_raw_track_t* track);
uft_error_t uft_raw_track_alloc_bits(uft_raw_track_t* track, size_t bit_count);
uft_error_t uft_raw_track_alloc_timing(uft_raw_track_t* track);
uft_error_t uft_raw_track_clone(const uft_raw_track_t* src, uft_raw_track_t* dst);

void uft_sector_data_free(uft_sector_data_t* sector);

// ============================================================================
// Format-Specific Defaults
// ============================================================================

/**
 * @brief Get default PLL params for encoding
 */
const uft_pll_params_t* uft_pll_defaults(uft_encoding_t encoding);

/**
 * @brief Get default track format
 */
const uft_track_format_t* uft_track_format_defaults(uft_encoding_t encoding,
                                                     bool high_density);

#ifdef __cplusplus
}
#endif

#endif // UFT_DECODER_ADAPTER_H
