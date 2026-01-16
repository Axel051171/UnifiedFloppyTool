/**
 * @file uft_dfi.h
 * @brief DFI (DiscFerret Image) format support
 * @version 3.9.0
 * 
 * DFI is the native image format for the DiscFerret flux-level
 * disk capture hardware. It stores raw flux transitions captured
 * directly from the drive head.
 * 
 * Features:
 * - Raw flux transition timing data
 * - Multiple revolutions per track
 * - Index pulse timing
 * - High precision timing (up to 100 MHz sample rate)
 * - Track-by-track storage
 * 
 * Reference: DiscFerret documentation, libdsk
 */

#ifndef UFT_DFI_H
#define UFT_DFI_H

#include "uft/core/uft_unified_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* DFI Magic Numbers */
#define DFI_MAGIC               "DFE2"
#define DFI_MAGIC_LEN           4
#define DFI_TRACK_MAGIC         "TRK0"
#define DFI_TRACK_MAGIC_LEN     4
#define DFI_STREAM_MAGIC        "STRM"

/* DFI File Header Size */
#define DFI_HEADER_SIZE         8

/* DFI Timing Constants */
#define DFI_DEFAULT_SAMPLE_RATE 100000000   /* 100 MHz */
#define DFI_MFM_CLOCK_NS        1000        /* 1 µs per MFM clock */

/* DFI Data Encoding */
#define DFI_DATA_DELTA          0x00        /* Delta-encoded timing */
#define DFI_DATA_INDEX          0x80        /* Index pulse marker */
#define DFI_DATA_EXTENDED       0xFF        /* Extended timing value */

/* Maximum values */
#define DFI_MAX_TRACKS          168         /* 84 cylinders * 2 sides */
#define DFI_MAX_REVOLUTIONS     10

/**
 * @brief DFI file header (8 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    char     magic[4];          /* "DFE2" */
    uint16_t version;           /* Format version */
    uint16_t flags;             /* File flags */
} dfi_file_header_t;

/**
 * @brief DFI track header (8+ bytes)
 */
typedef struct {
    char     magic[4];          /* "TRK0" */
    uint32_t data_length;       /* Length of track data */
    /* Track data follows */
} dfi_track_header_t;
#pragma pack(pop)

/**
 * @brief DFI flux transition data for one track
 */
typedef struct {
    uint8_t  cylinder;
    uint8_t  head;
    
    uint32_t *flux_times;       /* Flux transition times (in sample clocks) */
    size_t   flux_count;        /* Number of transitions */
    
    uint32_t *index_times;      /* Index pulse times */
    size_t   index_count;       /* Number of revolutions */
    
    uint32_t sample_rate;       /* Sample rate (Hz) */
    uint32_t total_time;        /* Total track time in samples */
    
} dfi_track_data_t;

/**
 * @brief DFI image structure
 */
typedef struct {
    dfi_file_header_t header;
    
    uint8_t  cylinders;
    uint8_t  heads;
    uint32_t sample_rate;
    
    dfi_track_data_t *tracks;
    size_t   track_count;
    
} dfi_image_t;

/**
 * @brief DFI read options
 */
typedef struct {
    uint32_t sample_rate;       /* Override sample rate (0=auto) */
    bool     decode_flux;       /* Decode flux to sectors */
    uint8_t  revolution;        /* Which revolution to decode (0=best) */
} dfi_read_options_t;

/**
 * @brief DFI write options
 */
typedef struct {
    uint32_t sample_rate;       /* Sample rate (0=default 100MHz) */
    bool     include_index;     /* Include index pulse data */
} dfi_write_options_t;

/**
 * @brief DFI read result
 */
typedef struct {
    bool success;
    uft_error_t error;
    const char *error_detail;
    
    uint8_t  cylinders;
    uint8_t  heads;
    uint32_t sample_rate;
    size_t   track_count;
    
    size_t   total_flux_count;
    size_t   total_index_count;
    size_t   image_size;
    
    /* Decoded geometry (if flux decoded) */
    uint8_t  detected_sectors;
    uint16_t detected_sector_size;
    uft_encoding_t detected_encoding;
    
} dfi_read_result_t;

/* ============================================================================
 * DFI Functions
 * ============================================================================ */

/**
 * @brief Initialize DFI image structure
 */
void uft_dfi_image_init(dfi_image_t *image);

/**
 * @brief Free DFI image resources
 */
void uft_dfi_image_free(dfi_image_t *image);

/**
 * @brief Initialize read options
 */
void uft_dfi_read_options_init(dfi_read_options_t *opts);

/**
 * @brief Initialize write options
 */
void uft_dfi_write_options_init(dfi_write_options_t *opts);

/**
 * @brief Read DFI file
 */
uft_error_t uft_dfi_read(const char *path,
                         dfi_image_t *image,
                         const dfi_read_options_t *opts,
                         dfi_read_result_t *result);

/**
 * @brief Read DFI from memory
 */
uft_error_t uft_dfi_read_mem(const uint8_t *data, size_t size,
                             dfi_image_t *image,
                             const dfi_read_options_t *opts,
                             dfi_read_result_t *result);

/**
 * @brief Write DFI file
 */
uft_error_t uft_dfi_write(const dfi_image_t *image,
                          const char *path,
                          const dfi_write_options_t *opts);

/**
 * @brief Convert DFI to sector-based disk image
 */
uft_error_t uft_dfi_to_disk(const dfi_image_t *dfi,
                            uft_disk_image_t **out_disk,
                            const dfi_read_options_t *opts);

/**
 * @brief Probe if data is DFI format
 */
bool uft_dfi_probe(const uint8_t *data, size_t size, int *confidence);

/**
 * @brief Validate DFI file header
 */
bool uft_dfi_validate_header(const dfi_file_header_t *header);

/**
 * @brief Get track from DFI image
 */
dfi_track_data_t* uft_dfi_get_track(dfi_image_t *image, uint8_t cyl, uint8_t head);

/**
 * @brief Calculate bit rate from flux timing
 */
uint32_t uft_dfi_calc_bitrate(const dfi_track_data_t *track);

/**
 * @brief Detect encoding type from flux data
 */
uft_encoding_t uft_dfi_detect_encoding(const dfi_track_data_t *track);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DFI_H */
