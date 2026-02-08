/**
 * @file uft_scp_multirev.h
 * @brief SCP Multi-Revolution Reader with Confidence Fusion
 * @version 3.3.2
 * @date 2025-01-03
 *
 * PUBLIC API:
 * - scp_multirev_open()       - Open SCP file
 * - scp_multirev_close()      - Close context
 * - scp_multirev_read_track() - Read track with all revolutions
 * - scp_multirev_free_track() - Free track data
 * - scp_multirev_get_info()   - Get disk info
 * - scp_multirev_configure()  - Configure fusion parameters
 * - scp_multirev_get_stats()  - Get statistics
 */

#ifndef UFT_SCP_MULTIREV_H
#define UFT_SCP_MULTIREV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*============================================================================
 * CONSTANTS
 *============================================================================*/

#define SCP_MAX_REVOLUTIONS 5

/* Disk types */
#define SCP_DISK_C64        0x00
#define SCP_DISK_AMIGA      0x04
#define SCP_DISK_ATARI_FM   0x10
#define SCP_DISK_ATARI_MFM  0x11
#define SCP_DISK_APPLE_II   0x20
#define SCP_DISK_APPLE_GCR  0x24
#define SCP_DISK_IBM_MFM_DD 0x40
#define SCP_DISK_IBM_MFM_HD 0x44

/* Fusion methods */
#define SCP_FUSION_MEDIAN   0
#define SCP_FUSION_WEIGHTED 1
#define SCP_FUSION_BEST     2

/*============================================================================
 * TYPES
 *============================================================================*/

/* Opaque context */
typedef struct scp_multirev_ctx scp_multirev_ctx_t;

/* Single revolution data */
typedef struct {
    uint32_t  index_time_ns;     /* Index to index time in ns */
    uint32_t  flux_count;        /* Number of flux transitions */
    uint32_t* flux_data;         /* Flux timing data (ns) */
    float     rpm;               /* Calculated RPM */
    bool      valid;             /* Data valid flag */
} scp_revolution_t;

/* Multi-revolution track with confidence */
typedef struct {
    int track_number;
    int side;
    int revolution_count;
    
    /* Per-revolution data */
    scp_revolution_t revolutions[SCP_MAX_REVOLUTIONS];
    
    /* Fused data (best confidence) */
    uint32_t* fused_flux;
    uint32_t  fused_count;
    float*    confidence;        /* Per-transition confidence (0-1) */
    
    /* Weak bit detection */
    uint32_t* weak_positions;
    uint32_t  weak_count;
    
    /* Statistics */
    float avg_rpm;
    float rpm_variance;
    uint32_t avg_flux_count;
    float quality_score;         /* 0-100 overall quality */
    
    /* Forensic info */
    uint32_t checksum;
    char     timestamp[32];
} scp_multirev_track_t;

/*============================================================================
 * PUBLIC API
 *============================================================================*/

/**
 * @brief Open SCP file for multi-revolution reading
 * @param path Path to SCP file
 * @return Context pointer or NULL on error
 */
scp_multirev_ctx_t* scp_multirev_open(const char* path);

/**
 * @brief Close SCP context and free resources
 * @param ctx Pointer to context pointer (set to NULL on return)
 */
void scp_multirev_close(scp_multirev_ctx_t** ctx);

/**
 * @brief Read track with all revolutions and confidence fusion
 * @param ctx Context
 * @param track_num Track number
 * @param side Head/side (0 or 1)
 * @param track_out Output track pointer (caller must free with scp_multirev_free_track)
 * @return 0 on success, -1 on error
 */
int scp_multirev_read_track(
    scp_multirev_ctx_t* ctx,
    int track_num,
    int side,
    scp_multirev_track_t** track_out
);

/**
 * @brief Free track data
 * @param track Pointer to track pointer (set to NULL on return)
 */
void scp_multirev_free_track(scp_multirev_track_t** track);

/**
 * @brief Get disk info from SCP header
 * @param ctx Context
 * @param start_track Output: first track number (may be NULL)
 * @param end_track Output: last track number (may be NULL)
 * @param revolutions Output: revolutions per track (may be NULL)
 * @param disk_type Output: disk type code (may be NULL)
 */
void scp_multirev_get_info(
    scp_multirev_ctx_t* ctx,
    int* start_track,
    int* end_track,
    int* revolutions,
    int* disk_type
);

/**
 * @brief Configure fusion parameters
 * @param ctx Context
 * @param enable_fusion Enable multi-revolution fusion
 * @param fusion_method Fusion method (SCP_FUSION_*)
 * @param weak_threshold Variance threshold for weak bit detection (0.0-1.0)
 */
void scp_multirev_configure(
    scp_multirev_ctx_t* ctx,
    bool enable_fusion,
    int fusion_method,
    float weak_threshold
);

/**
 * @brief Get decoder statistics
 * @param ctx Context
 * @param total_flux Output: total flux transitions read (may be NULL)
 * @param tracks_decoded Output: number of tracks decoded (may be NULL)
 * @param weak_bits Output: weak bits detected (may be NULL)
 */
void scp_multirev_get_stats(
    scp_multirev_ctx_t* ctx,
    uint64_t* total_flux,
    uint32_t* tracks_decoded,
    uint32_t* weak_bits
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SCP_MULTIREV_H */
