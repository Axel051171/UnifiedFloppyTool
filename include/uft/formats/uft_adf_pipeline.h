/**
 * @file uft_adf_pipeline.h
 * @brief ADF Parser with full pipeline support
 * 
 * P1-006: ADF parser was missing Analyze/Decide/Preserve stages
 * 
 * Pipeline stages:
 * 1. READ: Load raw data
 * 2. ANALYZE: CRC, sync, timing analysis
 * 3. DECIDE: Best-of selection for multi-rev
 * 4. PRESERVE: Original bits retention
 * 5. WRITE: Output with full metadata
 */

#ifndef UFT_ADF_PIPELINE_H
#define UFT_ADF_PIPELINE_H

#include "uft/core/uft_unified_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ADF Constants */
#define ADF_TRACK_SIZE      11264   /* Raw MFM track bytes */
#define ADF_SECTOR_SIZE     512
#define ADF_SECTORS_PER_TRACK 11
#define ADF_TRACKS_DD       80
#define ADF_TRACKS_HD       160
#define ADF_HEADS           2

#define ADF_FILE_SIZE_DD    (880 * 1024)    /* 880 KB */
#define ADF_FILE_SIZE_HD    (1760 * 1024)   /* 1.76 MB */

/* Amiga MFM sync word */
#define AMIGA_MFM_SYNC      0x4489

/**
 * @brief ADF sector analysis result
 */
typedef struct {
    uint8_t sector;
    bool header_valid;
    bool data_valid;
    uint32_t header_checksum;
    uint32_t data_checksum;
    uint32_t calculated_checksum;
    
    /* Timing analysis */
    double avg_bit_time_ns;
    double timing_variance;
    
    /* Quality */
    uint8_t confidence;
    bool has_weak_bits;
    size_t bit_offset;
    
} adf_sector_analysis_t;

/**
 * @brief ADF track analysis result
 */
typedef struct {
    uint8_t track;
    uint8_t head;
    
    /* Sectors */
    adf_sector_analysis_t sectors[ADF_SECTORS_PER_TRACK];
    uint8_t sectors_found;
    uint8_t sectors_valid;
    
    /* Track-level */
    uint8_t sync_count;
    uint8_t quality;          /* 0-100 */
    bool complete;
    
    /* Format detection */
    bool is_amiga_dos;
    bool is_bootable;
    uint8_t format_type;      /* OFS=0, FFS=1, etc */
    
} adf_track_analysis_t;

/**
 * @brief ADF disk analysis result
 */
typedef struct {
    bool success;
    uft_error_t error;
    
    /* Geometry */
    uint8_t tracks;
    uint8_t heads;
    bool is_hd;
    
    /* Filesystem */
    char disk_name[32];
    uint8_t filesystem;       /* OFS, FFS, etc */
    bool is_bootable;
    uint32_t root_block;
    
    /* Quality */
    uint16_t total_sectors;
    uint16_t valid_sectors;
    uint16_t error_sectors;
    float overall_quality;
    
    /* Per-track analysis */
    adf_track_analysis_t track_analysis[ADF_TRACKS_HD];
    
} adf_disk_analysis_t;

/**
 * @brief Multi-revision input for ADF
 */
typedef struct {
    const uint8_t *data;
    size_t size;
    uint8_t quality;
    bool crc_checked;
} adf_revision_t;

/**
 * @brief ADF pipeline options
 */
typedef struct {
    /* Analyze stage */
    bool analyze_checksums;
    bool analyze_timing;
    bool detect_weak_bits;
    
    /* Decide stage */
    bool use_multi_rev;
    uint8_t min_confidence;
    
    /* Preserve stage */
    bool preserve_original;
    bool preserve_errors;
    bool preserve_timing;
    
    /* Write stage */
    bool generate_extended;
    bool include_analysis;
    
} adf_pipeline_options_t;

/**
 * @brief ADF pipeline context
 */
typedef struct {
    /* Current stage */
    enum {
        ADF_STAGE_INIT,
        ADF_STAGE_READ,
        ADF_STAGE_ANALYZE,
        ADF_STAGE_DECIDE,
        ADF_STAGE_PRESERVE,
        ADF_STAGE_WRITE,
        ADF_STAGE_DONE
    } stage;
    
    /* Data */
    uft_disk_image_t *disk;
    adf_disk_analysis_t analysis;
    
    /* Multi-revision */
    adf_revision_t revisions[16];
    size_t revision_count;
    
    /* Options */
    adf_pipeline_options_t opts;
    
    /* Callbacks */
    void (*on_progress)(int stage, int percent, void *user);
    void (*on_error)(uft_error_t err, const char *msg, void *user);
    void *user_data;
    
} adf_pipeline_ctx_t;

/* ============================================================================
 * Pipeline API
 * ============================================================================ */

/**
 * @brief Initialize pipeline options
 */
void adf_pipeline_options_init(adf_pipeline_options_t *opts);

/**
 * @brief Initialize pipeline context
 */
void adf_pipeline_init(adf_pipeline_ctx_t *ctx);

/**
 * @brief Free pipeline context
 */
void adf_pipeline_free(adf_pipeline_ctx_t *ctx);

/**
 * @brief Add revision for multi-rev processing
 */
int adf_pipeline_add_revision(adf_pipeline_ctx_t *ctx,
                              const uint8_t *data, size_t size,
                              uint8_t quality);

/**
 * @brief Run full pipeline
 */
uft_error_t adf_pipeline_run(adf_pipeline_ctx_t *ctx,
                             const char *input_path,
                             const char *output_path);

/**
 * @brief Run single pipeline stage
 */
uft_error_t adf_pipeline_run_stage(adf_pipeline_ctx_t *ctx, int stage);

/* ============================================================================
 * Stage 1: READ
 * ============================================================================ */

/**
 * @brief Read ADF file
 */
uft_error_t adf_read(const char *path,
                     uft_disk_image_t **out_disk);

/**
 * @brief Read ADF from memory
 */
uft_error_t adf_read_mem(const uint8_t *data, size_t size,
                         uft_disk_image_t **out_disk);

/* ============================================================================
 * Stage 2: ANALYZE
 * ============================================================================ */

/**
 * @brief Analyze complete disk
 */
uft_error_t adf_analyze(const uft_disk_image_t *disk,
                        adf_disk_analysis_t *out_analysis);

/**
 * @brief Analyze single track
 */
uft_error_t adf_analyze_track(const uft_track_t *track,
                              adf_track_analysis_t *out_analysis);

/**
 * @brief Analyze single sector
 */
uft_error_t adf_analyze_sector(const uint8_t *sector_data,
                               size_t size,
                               adf_sector_analysis_t *out_analysis);

/**
 * @brief Verify Amiga checksum
 */
uint32_t adf_checksum(const uint8_t *data, size_t size);

/**
 * @brief Verify sector header checksum
 */
bool adf_verify_header(const uint8_t *header);

/**
 * @brief Verify sector data checksum
 */
bool adf_verify_data(const uint8_t *data, size_t size,
                     uint32_t expected_checksum);

/* ============================================================================
 * Stage 3: DECIDE
 * ============================================================================ */

/**
 * @brief Select best revision for each sector
 */
uft_error_t adf_decide(adf_pipeline_ctx_t *ctx);

/**
 * @brief Select best sector from multiple reads
 */
int adf_decide_sector(const adf_sector_analysis_t *analyses,
                      size_t count);

/**
 * @brief Merge multi-revision data
 */
uft_error_t adf_merge_revisions(const adf_revision_t *revisions,
                                size_t count,
                                uint8_t *out_data,
                                size_t *out_size);

/* ============================================================================
 * Stage 4: PRESERVE
 * ============================================================================ */

/**
 * @brief Preserve original data with metadata
 */
uft_error_t adf_preserve(const uft_disk_image_t *disk,
                         const adf_disk_analysis_t *analysis,
                         uft_disk_image_t *out_preserved);

/**
 * @brief Store original bits alongside decoded
 */
uft_error_t adf_preserve_track(const uft_track_t *original,
                               const adf_track_analysis_t *analysis,
                               uft_track_t *preserved);

/* ============================================================================
 * Stage 5: WRITE
 * ============================================================================ */

/**
 * @brief Write ADF file
 */
uft_error_t adf_write(const uft_disk_image_t *disk,
                      const char *path);

/**
 * @brief Write ADF to memory
 */
uft_error_t adf_write_mem(const uft_disk_image_t *disk,
                          uint8_t *buffer, size_t buffer_size,
                          size_t *out_size);

/**
 * @brief Write extended ADF with analysis
 */
uft_error_t adf_write_extended(const uft_disk_image_t *disk,
                               const adf_disk_analysis_t *analysis,
                               const char *path);

/* ============================================================================
 * Utilities
 * ============================================================================ */

/**
 * @brief Detect ADF filesystem type
 */
uint8_t adf_detect_filesystem(const uint8_t *boot_block);

/**
 * @brief Get filesystem name
 */
const char* adf_filesystem_name(uint8_t type);

/**
 * @brief Validate ADF file
 */
bool adf_validate(const uint8_t *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ADF_PIPELINE_H */
