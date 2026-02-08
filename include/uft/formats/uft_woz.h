/**
 * @file uft_woz.h
 * @brief WOZ 2.1 format complete support
 * 
 * P2-006: WOZ 2.0 → WOZ 2.1 upgrade
 * 
 * WOZ (Apple II disk image format):
 * - WOZ 1.0: Basic flux timing
 * - WOZ 2.0: Extended metadata, bit timing
 * - WOZ 2.1: Improved weak bit handling, FLUX chunk
 * 
 * Features:
 * - Read/Write WOZ 1.0, 2.0, 2.1
 * - Flux timing preservation
 * - Weak bit handling
 * - Full metadata support
 */

#ifndef UFT_WOZ_H
#define UFT_WOZ_H

#include "uft/core/uft_unified_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* WOZ Constants */
#define WOZ1_SIGNATURE      "WOZ1"
#define WOZ2_SIGNATURE      "WOZ2"
#define WOZ_SIGNATURE_LEN   4
#define WOZ_MAGIC           0x0A0D0AFF

/* WOZ Chunk IDs */
#define WOZ_CHUNK_INFO      0x4F464E49  /* "INFO" */
#define WOZ_CHUNK_TMAP      0x50414D54  /* "TMAP" */
#define WOZ_CHUNK_TRKS      0x534B5254  /* "TRKS" */
#define WOZ_CHUNK_WRIT      0x54495257  /* "WRIT" (WOZ 2.1) */
#define WOZ_CHUNK_FLUX      0x58554C46  /* "FLUX" (WOZ 2.1) */
#define WOZ_CHUNK_META      0x4154454D  /* "META" */

/* WOZ Disk Types */
typedef enum {
    WOZ_DISK_525        = 1,     /* 5.25" floppy */
    WOZ_DISK_35         = 2,     /* 3.5" floppy */
} woz_disk_type_t;

/* WOZ Boot Sector Format */
typedef enum {
    WOZ_BOOT_UNKNOWN    = 0,
    WOZ_BOOT_13SECTOR   = 1,     /* DOS 3.2 (13 sectors) */
    WOZ_BOOT_16SECTOR   = 2,     /* DOS 3.3 / ProDOS (16 sectors) */
    WOZ_BOOT_BOTH       = 3,     /* Both */
} woz_boot_format_t;

/* WOZ Timing Types */
typedef enum {
    WOZ_TIMING_UNKNOWN  = 0,
    WOZ_TIMING_DEFAULT  = 1,     /* Default timing */
    WOZ_TIMING_VARIABLE = 2,     /* Variable timing */
    WOZ_TIMING_FLUX     = 3,     /* Full flux timing (2.1) */
} woz_timing_t;

/**
 * @brief WOZ file header (common)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t signature[4];        /* "WOZ1" or "WOZ2" */
    uint8_t magic[4];            /* 0xFF 0x0A 0x0D 0x0A */
    uint32_t crc32;              /* CRC of remaining data */
} woz_header_t;
#pragma pack(pop)

/**
 * @brief WOZ chunk header
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t chunk_id;           /* 4-char ID as uint32 */
    uint32_t chunk_size;         /* Size of chunk data */
} woz_chunk_header_t;
#pragma pack(pop)

/**
 * @brief WOZ INFO chunk (v2)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t version;             /* WOZ version (1 or 2) */
    uint8_t disk_type;           /* woz_disk_type_t */
    uint8_t write_protected;     /* 1 = protected */
    uint8_t synchronized;        /* 1 = cross-track sync */
    uint8_t cleaned;             /* 1 = MC3470 cleaned */
    char creator[32];            /* Creator string */
    
    /* v2 extensions */
    uint8_t disk_sides;          /* Number of sides */
    uint8_t boot_sector_format;  /* woz_boot_format_t */
    uint8_t optimal_bit_timing;  /* ns/8 per bit (default: 32 = 4µs) */
    uint16_t compatible_hardware;/* Hardware bitmask */
    uint16_t required_ram;       /* KB of RAM required */
    uint16_t largest_track;      /* Blocks for largest track */
    
    /* v2.1 extensions */
    uint16_t flux_block;         /* Starting block of FLUX chunk */
    uint16_t largest_flux_track; /* Blocks for largest flux track */
    
    uint8_t reserved[10];
} woz_info_chunk_t;
#pragma pack(pop)

/**
 * @brief WOZ TMAP chunk (track map)
 * Maps quarter tracks (0-159) to track data indices
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t track_map[160];      /* 0xFF = no track */
} woz_tmap_chunk_t;
#pragma pack(pop)

/**
 * @brief WOZ TRKS v1 track entry
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t bitstream[6646];     /* Track data */
    uint16_t bytes_used;         /* Bytes used in bitstream */
    uint16_t bit_count;          /* Number of bits */
    uint16_t splice_point;       /* Bit index of splice (0xFFFF = none) */
    uint8_t splice_nibble;       /* First nibble after splice */
    uint8_t splice_bit_count;    /* Bits in splice nibble */
    uint16_t reserved;
} woz_track_v1_t;
#pragma pack(pop)

/**
 * @brief WOZ TRKS v2 track entry header
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t starting_block;     /* First 512-byte block */
    uint16_t block_count;        /* Number of blocks */
    uint32_t bit_count;          /* Number of bits */
} woz_track_v2_t;
#pragma pack(pop)

/**
 * @brief WOZ 2.1 FLUX track entry
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t starting_block;     /* First 512-byte block */
    uint16_t block_count;        /* Number of blocks */
    uint32_t flux_count;         /* Number of flux transitions */
} woz_flux_track_t;
#pragma pack(pop)

/**
 * @brief WOZ metadata storage
 */
typedef struct {
    char *title;
    char *subtitle;
    char *publisher;
    char *developer;
    char *copyright;
    char *version;
    char *language;
    char *requires_ram;
    char *requires_machine;
    char *notes;
    char *side;
    char *side_name;
    char *contributor;
    char *image_date;
    
    /* Custom metadata */
    struct {
        char *key;
        char *value;
    } custom[32];
    int custom_count;
    
} woz_metadata_t;

/**
 * @brief WOZ read result
 */
typedef struct {
    bool success;
    uft_error_t error;
    
    uint8_t version;             /* 1 or 2 */
    woz_disk_type_t disk_type;
    woz_boot_format_t boot_format;
    
    uint8_t tracks;
    uint8_t sides;
    bool write_protected;
    bool synchronized;
    
    uint8_t bit_timing;          /* ns/8 per bit */
    
    bool has_metadata;
    woz_metadata_t metadata;
    
    bool has_flux;               /* v2.1: has FLUX chunk */
    
    uint32_t calculated_crc;
    uint32_t stored_crc;
    bool crc_valid;
    
} woz_read_result_t;

/**
 * @brief WOZ write options
 */
typedef struct {
    uint8_t version;             /* 1, 2, or 21 for 2.1 */
    woz_disk_type_t disk_type;
    woz_boot_format_t boot_format;
    
    bool write_protected;
    bool synchronized;
    
    uint8_t bit_timing;          /* 0 = default (32) */
    
    const char *creator;         /* Creator string */
    
    bool include_metadata;
    woz_metadata_t *metadata;
    
    bool include_flux;           /* v2.1: include FLUX chunk */
    
} woz_write_options_t;

/* ============================================================================
 * Metadata Functions
 * ============================================================================ */

/**
 * @brief Initialize metadata
 */
void woz_metadata_init(woz_metadata_t *meta);

/**
 * @brief Free metadata
 */
void woz_metadata_free(woz_metadata_t *meta);

/**
 * @brief Set metadata field
 */
int woz_metadata_set(woz_metadata_t *meta, const char *key, const char *value);

/**
 * @brief Get metadata field
 */
const char* woz_metadata_get(const woz_metadata_t *meta, const char *key);

/**
 * @brief Parse META chunk
 */
int woz_metadata_parse(const char *meta_str, size_t len, woz_metadata_t *out);

/**
 * @brief Serialize metadata to string
 */
char* woz_metadata_serialize(const woz_metadata_t *meta, size_t *out_len);

/* ============================================================================
 * WOZ I/O
 * ============================================================================ */

/**
 * @brief Read WOZ file
 */
uft_error_t uft_woz_read(const char *path,
                         uft_disk_image_t **out_disk,
                         woz_read_result_t *result);

/**
 * @brief Read WOZ from memory
 */
uft_error_t uft_woz_read_mem(const uint8_t *data, size_t size,
                             uft_disk_image_t **out_disk,
                             woz_read_result_t *result);

/**
 * @brief Write WOZ file
 */
uft_error_t uft_woz_write(const uft_disk_image_t *disk,
                          const char *path,
                          const woz_write_options_t *opts);

/**
 * @brief Write WOZ to memory
 */
uft_error_t uft_woz_write_mem(const uft_disk_image_t *disk,
                              uint8_t *buffer, size_t buffer_size,
                              size_t *out_size,
                              const woz_write_options_t *opts);

/**
 * @brief Initialize write options
 */
void uft_woz_write_options_init(woz_write_options_t *opts);

/**
 * @brief Detect WOZ version
 */
int uft_woz_detect_version(const uint8_t *data, size_t size);

/**
 * @brief Validate WOZ header
 */
bool uft_woz_validate_header(const woz_header_t *header);

/**
 * @brief Calculate WOZ CRC
 */
uint32_t uft_woz_calculate_crc(const uint8_t *data, size_t size);

/* ============================================================================
 * Track Operations
 * ============================================================================ */

/**
 * @brief Read track from WOZ
 */
uft_error_t woz_read_track(const uint8_t *woz_data, size_t woz_size,
                           int track_idx, uft_track_t *out_track);

/**
 * @brief Write track to WOZ format
 */
uft_error_t woz_write_track(const uft_track_t *track,
                            uint8_t *out_data, size_t *out_size,
                            int version);

/**
 * @brief Convert quarter-track to track index
 */
int woz_quarter_track_to_index(int quarter_track);

/**
 * @brief Get track for quarter-track position
 */
int woz_get_track(const woz_tmap_chunk_t *tmap, int quarter_track);

/* ============================================================================
 * FLUX Operations (v2.1)
 * ============================================================================ */

/**
 * @brief Read FLUX chunk
 */
uft_error_t woz_read_flux(const uint8_t *woz_data, size_t woz_size,
                          int track_idx,
                          uint32_t **out_flux, size_t *out_count);

/**
 * @brief Write FLUX chunk
 */
uft_error_t woz_write_flux(const uint32_t *flux, size_t count,
                           uint8_t *out_data, size_t *out_size);

/**
 * @brief Convert flux to bits
 */
uft_error_t woz_flux_to_bits(const uint32_t *flux, size_t flux_count,
                             uint8_t *out_bits, size_t *out_bit_count,
                             uint32_t sample_rate);

/**
 * @brief Convert bits to flux
 */
uft_error_t woz_bits_to_flux(const uint8_t *bits, size_t bit_count,
                             uint32_t **out_flux, size_t *out_count,
                             uint32_t sample_rate);

/* ============================================================================
 * Conversion
 * ============================================================================ */

/**
 * @brief Convert DSK to WOZ
 */
uft_error_t uft_dsk_to_woz(const uint8_t *dsk_data, size_t dsk_size,
                           uint8_t *woz_data, size_t *woz_size,
                           const woz_write_options_t *opts);

/**
 * @brief Convert WOZ to DSK
 */
uft_error_t uft_woz_to_dsk(const uint8_t *woz_data, size_t woz_size,
                           uint8_t *dsk_data, size_t *dsk_size);

/**
 * @brief Convert NIB to WOZ
 */
uft_error_t uft_nib_to_woz(const uint8_t *nib_data, size_t nib_size,
                           uint8_t *woz_data, size_t *woz_size,
                           const woz_write_options_t *opts);

/**
 * @brief Convert WOZ to NIB
 */
uft_error_t uft_woz_to_nib(const uint8_t *woz_data, size_t woz_size,
                           uint8_t *nib_data, size_t *nib_size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_WOZ_H */
