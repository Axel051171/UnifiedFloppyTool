/**
 * @file uft_io_abstraction.h
 * @brief Unified I/O Abstraction Layer
 * 
 * PROBLEM:
 * ════════════════════════════════════════════════════════════════════════════
 * Aber wir haben vielleicht nur SCP-Flux oder D64-Sektoren.
 * 
 * LÖSUNG:
 * ════════════════════════════════════════════════════════════════════════════
 * 
 *   ┌─────────────────────────────────────────────────────────────────────┐
 *   │                    UFT I/O ABSTRACTION LAYER                        │
 *   ├─────────────────────────────────────────────────────────────────────┤
 *   │                                                                     │
 *   │   DATA SOURCES                    UNIFIED INTERFACE                 │
 *   │   ────────────                    ─────────────────                 │
 *   │                                                                     │
 *   │   ┌─────────┐                     ┌─────────────────┐               │
 *   │   │   SCP   │───┐                 │                 │               │
 *   │   └─────────┘   │                 │  uft_track_t    │               │
 *   │   ┌─────────┐   │  ┌──────────┐   │  ─────────────  │               │
 *   │   │Kryoflux │───┼──│ DECODER  │───│  • flux_data    │               │
 *   │   └─────────┘   │  └──────────┘   │  • bitstream    │               │
 *   │   ┌─────────┐   │                 │  • sectors[]    │               │
 *   │   │   HFE   │───┘                 │  • metadata     │               │
 *   │   └─────────┘                     │                 │               │
 *   │                                   └────────┬────────┘               │
 *   │   ┌─────────┐                              │                        │
 *   │   │   G64   │──────────────────────────────┤                        │
 *   │   └─────────┘                              │                        │
 *   │   ┌─────────┐                              │                        │
 *   │   │   D64   │──────────────────────────────┘                        │
 *   │   └─────────┘                                                       │
 *   │                                                                     │
 *   │                           ▼                                         │
 *   │                                                                     │
 *   │   TOOL ADAPTERS           SYNTHESIZER                               │
 *   │   ─────────────           ───────────                               │
 *   │                                                                     │
 *   │   ┌─────────────┐         ┌─────────────┐                           │
 *   │   │  (needs G64)│         │ G64 from    │                           │
 *   │   └─────────────┘         │ flux/sector │                           │
 *   │                           └─────────────┘                           │
 *   │   ┌─────────────┐         ┌─────────────┐                           │
 *   │   │  adftools   │◄────────│ Decode      │                           │
 *   │   │  (needs ADF)│         │ ADF from    │                           │
 *   │   └─────────────┘         │ flux/HFE    │                           │
 *   │                           └─────────────┘                           │
 *   │                                                                     │
 *   └─────────────────────────────────────────────────────────────────────┘
 * 
 * DATEN-SCHICHTEN:
 * ════════════════════════════════════════════════════════════════════════════
 * 
 *   Layer 0: FLUX (Raw timing)
 *            ↓ Decode (PLL, bit detection)
 *   Layer 1: BITSTREAM (MFM/GCR encoded)
 *            ↓ Decode (sync, header, data)
 *   Layer 2: SECTOR (Raw sector data)
 *            ↓ Parse (filesystem structures)
 *   Layer 3: FILESYSTEM (Files, directories)
 * 
 * Jede Schicht kann aus der darüberliegenden abgeleitet werden.
 * Umgekehrt: Synthese mit Qualitätsverlust.
 */

#ifndef UFT_IO_ABSTRACTION_H
#define UFT_IO_ABSTRACTION_H

#include "uft_types.h"
#include "uft_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Data Layer Enumeration
// ============================================================================

typedef enum uft_data_layer {
    UFT_LAYER_FLUX       = 0,   // Raw flux timing
    UFT_LAYER_BITSTREAM  = 1,   // Encoded bitstream
    UFT_LAYER_SECTOR     = 2,   // Decoded sectors
    UFT_LAYER_FILESYSTEM = 3,   // Filesystem level
} uft_data_layer_t;

// ============================================================================
// Unified Track Structure
// ============================================================================

/**
 * @brief Sector data within a track
 * Note: May already be defined in uft_types.h or uft_sector_parser.h
 */
#ifndef UFT_IO_SECTOR_T_DEFINED
#define UFT_IO_SECTOR_T_DEFINED
typedef struct uft_io_sector {
    int             logical_sector;     // Logical sector number
    int             physical_sector;    // Physical position
    
    uint8_t*        data;               // Sector data
    size_t          data_size;          // Usually 256, 512, 1024
    
    uint8_t*        header;             // Raw header bytes
    size_t          header_size;
    
    // Status
    bool            valid;              // CRC/checksum OK
    bool            deleted;            // Deleted data mark
    bool            weak;               // Weak bits detected
    int             error_count;        // Read errors
    int             read_retries;       // Retries needed
    
    // Timing (if from flux)
    double          bit_cell_time_us;   // Average bit cell
    double          data_rate_kbps;     // Data rate
} uft_io_sector_t;
#endif /* UFT_IO_SECTOR_T_DEFINED */

/**
 * @brief Complete track with all data layers
 * Note: If uft_track.h is included first, use its definition
 */
#ifndef UFT_TRACK_T_DEFINED
#define UFT_TRACK_T_DEFINED
typedef struct uft_track {
    // Identity
    int             cylinder;
    int             head;
    
    // Available layers (bitmask)
    uint32_t        available_layers;
    
    // Layer 0: Flux data
    struct {
        uint32_t*   samples;            // Flux timing samples
        size_t      sample_count;
        int         revolution_count;
        double      sample_rate_mhz;
        double      index_time_us;      // Time between index pulses
    } flux;
    
    // Layer 1: Bitstream
    struct {
        uint8_t*    bits;               // Packed bitstream
        size_t      bit_count;
        double      bit_rate_kbps;
        
        // Encoding info
        enum {
            ENC_MFM,
            ENC_FM,
            ENC_GCR_CBM,
            ENC_GCR_APPLE,
            ENC_AMIGA_MFM,
        } encoding;
    } bitstream;
    
    // Layer 2: Sectors
    struct {
        uft_io_sector_t* sectors;
        int           sector_count;
        int           sector_size;      // Common size
        int           interleave;       // Sector interleave
    } sectors;
    
    // Metadata
    struct {
        bool        copy_protected;
        bool        non_standard;
        char        notes[256];
    } meta;
} uft_track_t;
#endif /* UFT_TRACK_T_DEFINED */

// ============================================================================
// I/O Source Interface
// ============================================================================

typedef struct uft_io_source uft_io_source_t;

/**
 * @brief I/O source operations
 */
typedef struct uft_io_source_ops {
    // Get source info
    const char* (*get_name)(uft_io_source_t* src);
    uft_data_layer_t (*get_native_layer)(uft_io_source_t* src);
    uft_format_t (*get_format)(uft_io_source_t* src);
    
    // Geometry
    int (*get_cylinders)(uft_io_source_t* src);
    int (*get_heads)(uft_io_source_t* src);
    int (*get_sectors)(uft_io_source_t* src, int cyl, int head);
    
    // Read track at specified layer
    uft_error_t (*read_track)(uft_io_source_t* src,
                               int cylinder, int head,
                               uft_data_layer_t layer,
                               uft_track_t* track);
    
    // Check if layer is available
    bool (*has_layer)(uft_io_source_t* src, uft_data_layer_t layer);
    
    // Close/cleanup
    void (*close)(uft_io_source_t* src);
} uft_io_source_ops_t;

struct uft_io_source {
    const uft_io_source_ops_t* ops;
    void* private_data;
    
    // Cached info
    char            path[512];
    uft_format_t    format;
    uft_data_layer_t native_layer;
    int             cylinders;
    int             heads;
};

// ============================================================================
// I/O Sink Interface
// ============================================================================

typedef struct uft_io_sink uft_io_sink_t;

typedef struct uft_io_sink_ops {
    const char* (*get_name)(uft_io_sink_t* sink);
    uft_format_t (*get_format)(uft_io_sink_t* sink);
    uft_data_layer_t (*get_required_layer)(uft_io_sink_t* sink);
    
    // Write track
    uft_error_t (*write_track)(uft_io_sink_t* sink,
                                int cylinder, int head,
                                const uft_track_t* track);
    
    // Finalize (write headers, etc.)
    uft_error_t (*finalize)(uft_io_sink_t* sink);
    
    void (*close)(uft_io_sink_t* sink);
} uft_io_sink_ops_t;

struct uft_io_sink {
    const uft_io_sink_ops_t* ops;
    void* private_data;
    
    char            path[512];
    uft_format_t    format;
    uft_data_layer_t required_layer;
};

// ============================================================================
// Layer Conversion
// ============================================================================

/**
 * @brief Convert track to different layer
 * 
 * Downward (decode): FLUX → BITSTREAM → SECTOR
 * Upward (synthesize): SECTOR → BITSTREAM → FLUX (lossy!)
 */
uft_error_t uft_track_convert_layer(uft_track_t* track,
                                     uft_data_layer_t target_layer,
                                     const void* options);

/**
 * @brief Check if conversion is possible
 */
bool uft_track_can_convert(const uft_track_t* track,
                            uft_data_layer_t target_layer,
                            const char** warning);

// ============================================================================
// Source/Sink Factories
// ============================================================================

/**
 * @brief Open file as I/O source (auto-detect format)
 */
uft_error_t uft_io_open_source(const char* path,
                                uft_io_source_t** source);

/**
 * @brief Open specific format as source
 */
uft_error_t uft_io_open_source_format(const char* path,
                                       uft_format_t format,
                                       uft_io_source_t** source);

/**
 * @brief Create output sink
 */
uft_error_t uft_io_create_sink(const char* path,
                                uft_format_t format,
                                int cylinders, int heads,
                                uft_io_sink_t** sink);

/**
 * @brief Copy with automatic layer conversion
 */
uft_error_t uft_io_copy(uft_io_source_t* source,
                         uft_io_sink_t* sink,
                         void (*progress)(int percent, void* user),
                         void* user);

// ============================================================================
// Memory Management
// ============================================================================

void uft_track_init(uft_track_t* track);
void uft_track_free(uft_track_t* track);
uft_error_t uft_track_clone(const uft_track_t* src, uft_track_t* dst);

void uft_io_source_close(uft_io_source_t* source);
void uft_io_sink_close(uft_io_sink_t* sink);

#ifdef __cplusplus
}
#endif

#endif // UFT_IO_ABSTRACTION_H
