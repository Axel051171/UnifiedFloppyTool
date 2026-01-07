/**
 * @file uft_hfe_v3.h
 * @brief HFE v3 format complete support
 * 
 * P2-004: Complete HFE v3 implementation
 * 
 * HFE (UFT HFE Format) Format:
 * - v1: Basic format with fixed-size tracks
 * - v2: Improved with variable track sizes
 * - v3: Extended with flux timing and metadata
 * 
 * Features:
 * - Read/Write all HFE versions
 * - Track timing preservation
 * - Flux data support
 * - Metadata handling
 */

#ifndef UFT_HFE_V3_H
#define UFT_HFE_V3_H

#include "uft/core/uft_unified_types.h"
#include "uft/core/uft_encoding.h"

#ifdef __cplusplus
extern "C" {
#endif

/* HFE Constants */
#define HFE_SIGNATURE       "HXCPICFE"
#define HFE_SIGNATURE_LEN   8
#define HFE_BLOCK_SIZE      512

/* HFE Versions */
#define HFE_VERSION_1       0x00
#define HFE_VERSION_2       0x01
#define HFE_VERSION_3       0x02

/* HFE Encoding Types */
typedef enum {
    HFE_ENC_UNKNOWN     = 0x00,
    HFE_ENC_AMIGA_MFM   = 0x01,
    HFE_ENC_AMIGA_FM    = 0x02,
    HFE_ENC_ATARI_MFM   = 0x03,
    HFE_ENC_ATARI_FM    = 0x04,
    HFE_ENC_PC_FM       = 0x05,
    HFE_ENC_PC_MFM      = 0x06,
    HFE_ENC_PC_MFM_HD   = 0x07,
    HFE_ENC_FM          = 0x08,
    HFE_ENC_MFM         = 0x09,
    HFE_ENC_GCR         = 0x0A,
    HFE_ENC_UNKNOWN_ENC = 0xFF,
} hfe_encoding_t;

/* HFE Floppy Interface */
typedef enum {
    HFE_IF_IBMPC_DD     = 0x00,
    HFE_IF_IBMPC_HD     = 0x01,
    HFE_IF_ATARIST_DD   = 0x02,
    HFE_IF_ATARIST_HD   = 0x03,
    HFE_IF_AMIGA_DD     = 0x04,
    HFE_IF_AMIGA_HD     = 0x05,
    HFE_IF_CPC_DD       = 0x06,
    HFE_IF_GENERIC_SDD  = 0x07,
    HFE_IF_IBMPC_ED     = 0x08,
    HFE_IF_MSX2_DD      = 0x09,
    HFE_IF_C64_DD       = 0x0A,
    HFE_IF_EMU_SHUGART  = 0x0B,
    HFE_IF_S950_DD      = 0x0C,
    HFE_IF_S950_HD      = 0x0D,
    HFE_IF_UNKNOWN      = 0xFF,
} hfe_interface_t;

/**
 * @brief HFE v1/v2 file header
 */
#pragma pack(push, 1)
typedef struct {
    char signature[8];           /* "HXCPICFE" */
    uint8_t format_revision;     /* 0=v1, 1=v2, 2=v3 */
    uint8_t tracks;              /* Number of cylinders */
    uint8_t sides;               /* Number of heads */
    uint8_t encoding;            /* hfe_encoding_t */
    uint16_t bitrate;            /* kbps (250, 300, 500) */
    uint16_t rpm;                /* Floppy RPM (300, 360) */
    uint8_t interface_mode;      /* hfe_interface_t */
    uint8_t reserved;
    uint16_t track_list_offset;  /* Offset to track LUT (blocks) */
    uint8_t write_allowed;       /* 0=protected, 0xFF=writable */
    uint8_t single_step;         /* 0=double step, 0xFF=single */
    uint8_t track0s0_altencoding;
    uint8_t track0s0_encoding;
    uint8_t track0s1_altencoding;
    uint8_t track0s1_encoding;
} hfe_header_t;
#pragma pack(pop)

/**
 * @brief HFE v3 extended header
 */
#pragma pack(push, 1)
typedef struct {
    hfe_header_t base;
    
    /* v3 extensions */
    uint32_t metadata_offset;    /* Offset to metadata block */
    uint32_t metadata_size;      /* Metadata size in bytes */
    uint8_t flags;               /* v3 flags */
    uint8_t reserved_v3[15];     /* Reserved */
} hfe_header_v3_t;
#pragma pack(pop)

/* v3 Flags */
#define HFE_V3_FLAG_FLUX        0x01    /* Contains flux timing */
#define HFE_V3_FLAG_WEAK_BITS   0x02    /* Contains weak bit info */
#define HFE_V3_FLAG_RANDOM      0x04    /* Contains random areas */

/**
 * @brief HFE track entry in LUT
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t offset;             /* Track offset (blocks) */
    uint16_t length;             /* Track length (bytes) */
} hfe_track_entry_t;
#pragma pack(pop)

/**
 * @brief HFE v3 metadata types
 */
typedef enum {
    HFE_META_NONE       = 0x00,
    HFE_META_TITLE      = 0x01,
    HFE_META_AUTHOR     = 0x02,
    HFE_META_DATE       = 0x03,
    HFE_META_TOOL       = 0x04,
    HFE_META_COMMENT    = 0x05,
    HFE_META_PLATFORM   = 0x06,
    HFE_META_CUSTOM     = 0xFF,
} hfe_meta_type_t;

/**
 * @brief HFE metadata entry
 */
typedef struct {
    hfe_meta_type_t type;
    char *value;
    size_t value_len;
} hfe_metadata_entry_t;

/**
 * @brief HFE metadata collection
 */
typedef struct {
    hfe_metadata_entry_t *entries;
    size_t count;
    size_t capacity;
} hfe_metadata_t;

/**
 * @brief HFE track data (v3)
 */
typedef struct {
    uint8_t *data;               /* Raw MFM/FM data (interleaved) */
    size_t data_len;
    
    /* v3 extensions */
    uint32_t *flux_timing;       /* Flux timing (optional) */
    size_t flux_count;
    
    uint8_t *weak_mask;          /* Weak bit mask (optional) */
    size_t weak_len;
    
} hfe_track_data_t;

/**
 * @brief HFE read result
 */
typedef struct {
    bool success;
    uft_error_t error;
    
    uint8_t version;
    uint8_t tracks;
    uint8_t sides;
    hfe_encoding_t encoding;
    hfe_interface_t interface_mode;
    
    uint16_t bitrate;
    uint16_t rpm;
    
    bool has_metadata;
    hfe_metadata_t metadata;
    
} hfe_read_result_t;

/**
 * @brief HFE write options
 */
typedef struct {
    uint8_t version;             /* HFE version to write */
    hfe_encoding_t encoding;
    hfe_interface_t interface_mode;
    
    uint16_t bitrate;            /* 0 = auto */
    uint16_t rpm;                /* 0 = auto */
    
    bool include_metadata;
    hfe_metadata_t *metadata;
    
    bool include_flux;           /* v3: include flux timing */
    bool include_weak_bits;      /* v3: include weak bit mask */
    
} hfe_write_options_t;

/* ============================================================================
 * Metadata Functions
 * ============================================================================ */

/**
 * @brief Initialize metadata
 */
void hfe_metadata_init(hfe_metadata_t *meta);

/**
 * @brief Free metadata
 */
void hfe_metadata_free(hfe_metadata_t *meta);

/**
 * @brief Add metadata entry
 */
int hfe_metadata_add(hfe_metadata_t *meta, hfe_meta_type_t type,
                     const char *value);

/**
 * @brief Get metadata entry
 */
const char* hfe_metadata_get(const hfe_metadata_t *meta, hfe_meta_type_t type);

/* ============================================================================
 * HFE I/O
 * ============================================================================ */

/**
 * @brief Read HFE file
 */
uft_error_t uft_hfe_read(const char *path,
                         uft_disk_image_t **out_disk,
                         hfe_read_result_t *result);

/**
 * @brief Read HFE from memory
 */
uft_error_t uft_hfe_read_mem(const uint8_t *data, size_t size,
                             uft_disk_image_t **out_disk,
                             hfe_read_result_t *result);

/**
 * @brief Write HFE file
 */
uft_error_t uft_hfe_write(const uft_disk_image_t *disk,
                          const char *path,
                          const hfe_write_options_t *opts);

/**
 * @brief Write HFE to memory
 */
uft_error_t uft_hfe_write_mem(const uft_disk_image_t *disk,
                              uint8_t *buffer, size_t buffer_size,
                              size_t *out_size,
                              const hfe_write_options_t *opts);

/**
 * @brief Initialize write options
 */
void uft_hfe_write_options_init(hfe_write_options_t *opts);

/**
 * @brief Detect HFE version from file
 */
int uft_hfe_detect_version(const uint8_t *data, size_t size);

/**
 * @brief Validate HFE header
 */
bool uft_hfe_validate_header(const hfe_header_t *header);

/* ============================================================================
 * Track Conversion
 * ============================================================================ */

/**
 * @brief Convert track to HFE format (interleaved)
 */
uft_error_t hfe_convert_track_to_hfe(const uft_track_t *track,
                                     hfe_track_data_t *out_hfe);

/**
 * @brief Convert HFE track to UFT format
 */
uft_error_t hfe_convert_track_from_hfe(const hfe_track_data_t *hfe,
                                       uft_track_t *out_track);

/**
 * @brief Free track data
 */
void hfe_track_data_free(hfe_track_data_t *track);

/* ============================================================================
 * Encoding Helpers
 * ============================================================================ */

/**
 * @brief Get HFE encoding from UFT encoding
 */
hfe_encoding_t hfe_encoding_from_uft(uft_disk_encoding_t enc);

/**
 * @brief Get UFT encoding from HFE encoding
 */
uft_disk_encoding_t hfe_encoding_to_uft(hfe_encoding_t enc);

/**
 * @brief Get encoding name
 */
const char* hfe_encoding_name(hfe_encoding_t enc);

/**
 * @brief Get interface name
 */
const char* hfe_interface_name(hfe_interface_t iface);

#ifdef __cplusplus
}
#endif

#endif /* UFT_HFE_V3_H */
