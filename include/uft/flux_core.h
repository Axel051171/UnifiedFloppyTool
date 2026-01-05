/**
 * @file flux_core.h
 * @brief Core Flux Data Structures
 * @version 1.0.0
 * 
 * FEATURES:
 * - Flux disk/track/bitstream representations
 * - Reference counting for memory management
 * - Compatible with uft_memory.h auto-cleanup
 */

#ifndef UFT_FLUX_CORE_H
#define UFT_FLUX_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * FORWARD DECLARATIONS
 * ============================================================================= */

typedef struct flux_disk_t flux_disk_t;
typedef struct flux_track_t flux_track_t;
typedef struct flux_bitstream_t flux_bitstream_t;

/* =============================================================================
 * FLUX TRANSITION DATA
 * ============================================================================= */

/**
 * @struct flux_sample_t
 * @brief Single flux transition sample
 */
typedef struct {
    uint64_t timestamp_ns;   /* Timestamp in nanoseconds */
    uint8_t index_pulse;     /* 1 if index pulse present, 0 otherwise */
} flux_sample_t;

/* =============================================================================
 * FLUX BITSTREAM
 * ============================================================================= */

/**
 * @struct flux_bitstream_t
 * @brief Decoded bitstream from flux data
 */
struct flux_bitstream_t {
    uint8_t *bits;           /* Bit data */
    size_t bit_count;        /* Number of bits */
    size_t byte_capacity;    /* Allocated capacity (bytes) */
    
    /* Metadata */
    enum {
        BITSTREAM_RAW,       /* Raw flux bits */
        BITSTREAM_MFM,       /* MFM-decoded */
        BITSTREAM_GCR,       /* GCR-decoded */
        BITSTREAM_FM         /* FM-decoded */
    } encoding;
    
    /* Statistics */
    size_t sync_patterns_found;
    size_t decode_errors;
};

/**
 * @brief Create empty bitstream
 */
flux_bitstream_t* flux_bitstream_create(size_t initial_capacity);

/**
 * @brief Destroy bitstream
 */
void flux_bitstream_destroy(flux_bitstream_t *bitstream);

/**
 * @brief Append bit to bitstream
 */
bool flux_bitstream_append_bit(flux_bitstream_t *bitstream, uint8_t bit);

/**
 * @brief Append byte to bitstream
 */
bool flux_bitstream_append_byte(flux_bitstream_t *bitstream, uint8_t byte);

/* =============================================================================
 * FLUX TRACK
 * ============================================================================= */

/**
 * @struct flux_track_t
 * @brief Single track's flux data
 */
struct flux_track_t {
    /* Track identification */
    int cylinder;            /* Physical track number */
    int head;                /* Head/side number (0 or 1) */
    
    /* Flux samples */
    flux_sample_t *samples;  /* Array of flux transitions */
    size_t sample_count;     /* Number of samples */
    size_t sample_capacity;  /* Allocated capacity */
    
    /* Timing info */
    uint64_t track_duration_ns;  /* Total track time */
    uint32_t index_count;        /* Number of complete revolutions */
    
    /* Quality metrics */
    float signal_quality;    /* 0.0-1.0 quality score */
    size_t weak_bits;        /* Number of weak/unstable bits */
    size_t read_retries;     /* Number of read attempts */
    
    /* Decoded data */
    flux_bitstream_t *bitstream;  /* Decoded bits (optional) */
    
    /* Reference counting */
    int refcount;
};

/**
 * @brief Create new track
 */
flux_track_t* flux_track_create(int cylinder, int head);

/**
 * @brief Destroy track (decrements refcount)
 */
void flux_track_destroy(flux_track_t **track);

/**
 * @brief Add flux sample to track
 */
bool flux_track_add_sample(flux_track_t *track, uint64_t timestamp_ns, uint8_t index);

/**
 * @brief Get average RPM from track
 */
float flux_track_get_rpm(const flux_track_t *track);

/* =============================================================================
 * FLUX DISK
 * ============================================================================= */

/**
 * @struct flux_disk_t
 * @brief Complete disk image with flux data
 */
struct flux_disk_t {
    /* Disk geometry */
    int max_cylinders;
    int max_heads;
    
    /* Tracks array (cylinder * heads + head) */
    flux_track_t **tracks;
    size_t track_count;
    
    /* Metadata */
    char *name;              /* Disk label/name */
    char *source_file;       /* Source filename */
    
    enum {
        DISK_FORMAT_UNKNOWN,
        DISK_FORMAT_IBM_MFM,
        DISK_FORMAT_AMIGA_MFM,
        DISK_FORMAT_C64_GCR,
        DISK_FORMAT_APPLE_GCR
    } format;
    
    /* Statistics */
    size_t total_size_bytes;
    float overall_quality;   /* Average quality across all tracks */
    
    /* Reference counting */
    int refcount;
};

/**
 * @brief Create new disk
 */
flux_disk_t* flux_disk_create(int cylinders, int heads);

/**
 * @brief Destroy disk (decrements refcount)
 */
void flux_disk_destroy(flux_disk_t **disk);

/**
 * @brief Get track from disk
 */
flux_track_t* flux_disk_get_track(flux_disk_t *disk, int cylinder, int head);

/**
 * @brief Set track on disk
 */
bool flux_disk_set_track(flux_disk_t *disk, int cylinder, int head, flux_track_t *track);

/**
 * @brief Calculate total disk size
 */
size_t flux_disk_calculate_size(const flux_disk_t *disk);

/* =============================================================================
 * HELPER FUNCTIONS
 * ============================================================================= */

/**
 * @brief Convert timestamp array to flux_sample array
 */
size_t flux_convert_timestamps(
    const uint64_t *timestamps,
    size_t count,
    flux_sample_t *samples
);

/**
 * @brief Detect index pulses in flux data
 */
int flux_detect_index_pulses(
    const flux_sample_t *samples,
    size_t count,
    size_t *index_positions,
    size_t max_indices
);

/**
 * @brief Calculate average bitrate from flux samples
 */
uint32_t flux_calculate_bitrate(
    const flux_sample_t *samples,
    size_t count
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLUX_CORE_H */
