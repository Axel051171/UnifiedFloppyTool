/**
 * @file uft_nib_format.h
 * @brief NIB/NB2/NBZ Disk Image Format Support
 * 
 * Formats from nibtools by Pete Rittwage (c64preservation.com):
 * - NIB: Raw nibble data with header (MNIB-1541-RAW)
 * - NB2: Multi-pass NIB (16 passes per track for best read selection)
 * - NBZ: LZ77-compressed NIB format
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#ifndef UFT_NIB_FORMAT_H
#define UFT_NIB_FORMAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** NIB format magic signature */
#define NIB_SIGNATURE           "MNIB-1541-RAW"
#define NIB_SIGNATURE_LEN       13

/** NIB format version */
#define NIB_VERSION             3

/** NIB header size */
#define NIB_HEADER_SIZE         0x100

/** NIB track length (8KB per track) */
#define NIB_TRACK_LENGTH        0x2000

/** Maximum number of tracks in NIB file */
#define NIB_MAX_TRACKS          84

/** Header offset for track entries */
#define NIB_TRACK_ENTRY_OFFSET  0x10

/** NB2 passes per density */
#define NB2_PASSES_PER_DENSITY  4

/** NB2 total passes per track (4 densities × 4 passes) */
#define NB2_PASSES_PER_TRACK    16

/** NBZ LZ77 marker detection threshold */
#define NBZ_COMPRESS_THRESHOLD  4

/* ============================================================================
 * Track Density Flags
 * ============================================================================ */

/** Density mask (bits 0-1) */
#define NIB_DENSITY_MASK        0x03

/** No sync found on track */
#define NIB_FLAG_NO_SYNC        0x40

/** Killer track (all 0xFF) */
#define NIB_FLAG_KILLER         0x80

/** Track matched flag (legacy) */
#define NIB_FLAG_MATCH          0x10

/** No track cycle found */
#define NIB_FLAG_NO_CYCLE       0x20

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief NIB file header structure
 */
typedef struct {
    char        signature[13];      /**< "MNIB-1541-RAW" */
    uint8_t     version;            /**< Format version (3) */
    uint8_t     reserved1;          /**< Reserved (0x00) */
    uint8_t     halftracks;         /**< 1 if halftracks present, 0 otherwise */
    uint8_t     track_entries[240]; /**< Track/density pairs (track, density) */
} nib_header_t;

/**
 * @brief NIB image structure
 */
typedef struct {
    nib_header_t    header;                             /**< File header */
    uint8_t         *track_data[NIB_MAX_TRACKS];        /**< Track data pointers */
    size_t          track_length[NIB_MAX_TRACKS];       /**< Track lengths */
    uint8_t         track_density[NIB_MAX_TRACKS];      /**< Track densities + flags */
    int             num_tracks;                         /**< Number of tracks */
    bool            has_halftracks;                     /**< Halftracks present */
    uint8_t         disk_id[2];                         /**< Disk ID (from track 18) */
} nib_image_t;

/**
 * @brief NB2 pass information
 */
typedef struct {
    uint8_t         density;        /**< Pass density */
    size_t          length;         /**< Decoded track length */
    size_t          errors;         /**< Number of errors in pass */
    bool            selected;       /**< This pass was selected as best */
} nb2_pass_info_t;

/**
 * @brief NB2 track information
 */
typedef struct {
    nb2_pass_info_t passes[NB2_PASSES_PER_TRACK];   /**< Pass information */
    int             best_pass;                       /**< Index of best pass */
    size_t          best_errors;                     /**< Errors in best pass */
} nb2_track_info_t;

/**
 * @brief NB2 image structure (extends NIB)
 */
typedef struct {
    nib_image_t     base;                               /**< Base NIB image */
    nb2_track_info_t track_info[NIB_MAX_TRACKS];        /**< Track pass info */
    uint8_t         *all_passes[NIB_MAX_TRACKS][NB2_PASSES_PER_TRACK]; /**< All pass data */
} nb2_image_t;

/**
 * @brief Format detection result
 */
typedef enum {
    NIB_FORMAT_UNKNOWN  = 0,
    NIB_FORMAT_NIB      = 1,    /**< Standard NIB */
    NIB_FORMAT_NB2      = 2,    /**< Multi-pass NB2 */
    NIB_FORMAT_NBZ      = 3,    /**< Compressed NBZ */
    NIB_FORMAT_G64      = 4,    /**< G64 (for reference) */
} nib_format_t;

/**
 * @brief NIB analysis result
 */
typedef struct {
    nib_format_t    format;                 /**< Detected format */
    int             version;                /**< Format version */
    int             num_tracks;             /**< Number of tracks */
    bool            has_halftracks;         /**< Halftracks present */
    size_t          file_size;              /**< File size */
    size_t          uncompressed_size;      /**< Uncompressed size (NBZ) */
    float           compression_ratio;      /**< Compression ratio (NBZ) */
    uint8_t         disk_id[2];             /**< Disk ID */
    char            format_name[32];        /**< Format name string */
    char            description[256];       /**< Detailed description */
} nib_analysis_t;

/* ============================================================================
 * API Functions - NIB Format
 * ============================================================================ */

/**
 * @brief Detect NIB/NB2/NBZ format from file
 * @param filename Path to file
 * @return Detected format type
 */
nib_format_t nib_detect_format(const char *filename);

/**
 * @brief Detect NIB/NB2/NBZ format from buffer
 * @param data File data
 * @param size Data size
 * @return Detected format type
 */
nib_format_t nib_detect_format_buffer(const uint8_t *data, size_t size);

/**
 * @brief Analyze NIB/NB2/NBZ file
 * @param filename Path to file
 * @param result Output analysis result
 * @return 0 on success, error code otherwise
 */
int nib_analyze(const char *filename, nib_analysis_t *result);

/**
 * @brief Load NIB file
 * @param filename Path to NIB file
 * @param image Output image structure (must be freed with nib_free)
 * @return 0 on success, error code otherwise
 */
int nib_load(const char *filename, nib_image_t **image);

/**
 * @brief Load NIB from buffer
 * @param data NIB file data
 * @param size Data size
 * @param image Output image structure
 * @return 0 on success, error code otherwise
 */
int nib_load_buffer(const uint8_t *data, size_t size, nib_image_t **image);

/**
 * @brief Save NIB file
 * @param filename Output path
 * @param image Image to save
 * @return 0 on success, error code otherwise
 */
int nib_save(const char *filename, const nib_image_t *image);

/**
 * @brief Save NIB to buffer
 * @param image Image to save
 * @param data Output buffer (must be freed)
 * @param size Output size
 * @return 0 on success, error code otherwise
 */
int nib_save_buffer(const nib_image_t *image, uint8_t **data, size_t *size);

/**
 * @brief Free NIB image
 * @param image Image to free
 */
void nib_free(nib_image_t *image);

/**
 * @brief Create new NIB image
 * @param has_halftracks Include halftracks
 * @return New image, or NULL on error
 */
nib_image_t *nib_create(bool has_halftracks);

/**
 * @brief Get track data from NIB image
 * @param image NIB image
 * @param halftrack Halftrack number (2-84)
 * @param data Output data pointer
 * @param length Output data length
 * @param density Output density + flags
 * @return 0 on success, error code otherwise
 */
int nib_get_track(const nib_image_t *image, int halftrack,
                  const uint8_t **data, size_t *length, uint8_t *density);

/**
 * @brief Set track data in NIB image
 * @param image NIB image
 * @param halftrack Halftrack number (2-84)
 * @param data Track data
 * @param length Track length
 * @param density Density + flags
 * @return 0 on success, error code otherwise
 */
int nib_set_track(nib_image_t *image, int halftrack,
                  const uint8_t *data, size_t length, uint8_t density);

/* ============================================================================
 * API Functions - NB2 Format
 * ============================================================================ */

/**
 * @brief Load NB2 file
 * @param filename Path to NB2 file
 * @param image Output image structure
 * @return 0 on success, error code otherwise
 */
int nb2_load(const char *filename, nb2_image_t **image);

/**
 * @brief Load NB2 from buffer
 * @param data NB2 file data
 * @param size Data size
 * @param image Output image structure
 * @return 0 on success, error code otherwise
 */
int nb2_load_buffer(const uint8_t *data, size_t size, nb2_image_t **image);

/**
 * @brief Get best pass for track
 * @param image NB2 image
 * @param halftrack Halftrack number
 * @param info Output pass info (can be NULL)
 * @return Best pass index, or -1 on error
 */
int nb2_get_best_pass(const nb2_image_t *image, int halftrack, nb2_pass_info_t *info);

/**
 * @brief Get specific pass data
 * @param image NB2 image
 * @param halftrack Halftrack number
 * @param pass Pass index (0-15)
 * @param data Output data pointer
 * @return 0 on success, error code otherwise
 */
int nb2_get_pass(const nb2_image_t *image, int halftrack, int pass, const uint8_t **data);

/**
 * @brief Free NB2 image
 * @param image Image to free
 */
void nb2_free(nb2_image_t *image);

/* ============================================================================
 * API Functions - NBZ Format (LZ77 Compression)
 * ============================================================================ */

/**
 * @brief Load NBZ file (compressed NIB)
 * @param filename Path to NBZ file
 * @param image Output NIB image
 * @return 0 on success, error code otherwise
 */
int nbz_load(const char *filename, nib_image_t **image);

/**
 * @brief Load NBZ from buffer
 * @param data Compressed data
 * @param size Compressed size
 * @param image Output NIB image
 * @return 0 on success, error code otherwise
 */
int nbz_load_buffer(const uint8_t *data, size_t size, nib_image_t **image);

/**
 * @brief Save NBZ file
 * @param filename Output path
 * @param image Image to compress and save
 * @return 0 on success, error code otherwise
 */
int nbz_save(const char *filename, const nib_image_t *image);

/**
 * @brief Save NBZ to buffer
 * @param image Image to compress
 * @param data Output compressed buffer (must be freed)
 * @param size Output compressed size
 * @return 0 on success, error code otherwise
 */
int nbz_save_buffer(const nib_image_t *image, uint8_t **data, size_t *size);

/* ============================================================================
 * API Functions - LZ77 Compression
 * ============================================================================ */

/**
 * @brief Compress data using LZ77
 * @param in Input data
 * @param out Output buffer (should be at least insize + insize/256 + 1)
 * @param insize Input size
 * @return Compressed size, or 0 on error
 */
size_t lz77_compress(const uint8_t *in, uint8_t *out, size_t insize);

/**
 * @brief Fast LZ77 compression (uses more memory)
 * @param in Input data
 * @param out Output buffer
 * @param insize Input size
 * @return Compressed size, or 0 on error
 */
size_t lz77_compress_fast(const uint8_t *in, uint8_t *out, size_t insize);

/**
 * @brief Decompress LZ77 data
 * @param in Compressed data
 * @param out Output buffer
 * @param insize Compressed size
 * @return Decompressed size, or 0 on error
 */
size_t lz77_decompress(const uint8_t *in, uint8_t *out, size_t insize);

/* ============================================================================
 * API Functions - Conversion
 * ============================================================================ */

/**
 * @brief Convert NIB to G64 format
 * @param nib_image Source NIB image
 * @param g64_data Output G64 data (must be freed)
 * @param g64_size Output G64 size
 * @param align_tracks Align tracks before conversion
 * @return 0 on success, error code otherwise
 */
int nib_to_g64(const nib_image_t *nib_image, uint8_t **g64_data, size_t *g64_size, bool align_tracks);

/**
 * @brief Convert G64 to NIB format
 * @param g64_data G64 data
 * @param g64_size G64 size
 * @param nib_image Output NIB image
 * @return 0 on success, error code otherwise
 */
int g64_to_nib(const uint8_t *g64_data, size_t g64_size, nib_image_t **nib_image);

/**
 * @brief Convert NIB to D64 format
 * @param nib_image Source NIB image
 * @param d64_data Output D64 data (must be freed)
 * @param d64_size Output D64 size
 * @param error_info Output error info (683 bytes, can be NULL)
 * @return 0 on success, error code otherwise
 */
int nib_to_d64(const nib_image_t *nib_image, uint8_t **d64_data, size_t *d64_size, uint8_t *error_info);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Get format name string
 * @param format Format type
 * @return Format name
 */
const char *nib_format_name(nib_format_t format);

/**
 * @brief Extract disk ID from track 18
 * @param track_data Track 18 data
 * @param track_length Track length
 * @param disk_id Output disk ID (2 bytes)
 * @return true if ID found
 */
bool nib_extract_disk_id(const uint8_t *track_data, size_t track_length, uint8_t *disk_id);

/**
 * @brief Check track for errors
 * @param track_data Track data
 * @param track_length Track length
 * @param track Track number
 * @param disk_id Disk ID
 * @return Number of errors found
 */
size_t nib_check_track_errors(const uint8_t *track_data, size_t track_length, int track, const uint8_t *disk_id);

/**
 * @brief Generate analysis report
 * @param analysis Analysis result
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Length of report string
 */
size_t nib_generate_report(const nib_analysis_t *analysis, char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_NIB_FORMAT_H */
