/**
 * @file uft_format_parsers.h
 * @brief UnifiedFloppyTool - Flux/Bitstream Format Parsers
 * @version 3.1.4.006
 *
 * Unified format detection and parsing for modern flux/bitstream formats.
 * Each parser provides detect, read, and write capabilities where applicable.
 *
 * Sources analyzed:
 * - FluxFox: scp.rs, kryoflux.rs, td0.rs, ipf.rs, hfe.rs, imd.rs
 * - Previous UFT headers from v3.1.4.001-005
 *
 * Supported formats:
 * - SCP (SuperCardPro)
 * - Kryoflux raw stream
 * - TD0 (Teledisk)
 * - IPF (Interchangeable Preservation Format / CAPS)
 * - HFE (HxC Floppy Emulator)
 * - IMD (ImageDisk)
 * - MFI (MAME Floppy Image)
 */

#ifndef UFT_FORMAT_PARSERS_H
#define UFT_FORMAT_PARSERS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Format Detection
 *============================================================================*/

/**
 * @brief Disk image format types
 */
typedef enum {
    UFT_FORMAT_UNKNOWN     = 0,
    
    /* Raw/Sector formats */
    UFT_FORMAT_RAW         = 1,    /**< Raw sector image */
    UFT_FORMAT_DSK         = 2,    /**< Apple DSK */
    UFT_FORMAT_ADF         = 3,    /**< Amiga ADF */
    UFT_FORMAT_D64         = 4,    /**< C64 D64 */
    UFT_FORMAT_ST          = 5,    /**< Atari ST */
    
    /* Bitstream formats */
    UFT_FORMAT_IMD         = 10,   /**< ImageDisk */
    UFT_FORMAT_TD0         = 11,   /**< Teledisk */
    UFT_FORMAT_HFE         = 12,   /**< HxC Floppy Emulator */
    UFT_FORMAT_MFM         = 13,   /**< Raw MFM bitstream */
    UFT_FORMAT_86F         = 14,   /**< PCem 86F */
    
    /* Flux formats */
    UFT_FORMAT_SCP         = 20,   /**< SuperCardPro */
    UFT_FORMAT_KRYOFLUX    = 21,   /**< Kryoflux raw */
    UFT_FORMAT_IPF         = 22,   /**< SPS/CAPS IPF */
    UFT_FORMAT_A2R         = 23,   /**< Applesauce A2R */
    
    /* Container formats */
    UFT_FORMAT_WOZ         = 30,   /**< Apple WOZ */
    UFT_FORMAT_MOOF        = 31,   /**< Mac MOOF */
    UFT_FORMAT_MFI         = 32,   /**< MAME Floppy Image */
    UFT_FORMAT_PRI         = 33,   /**< PCE Raw Image */
    UFT_FORMAT_PSI         = 34,   /**< PCE Sector Image */
    UFT_FORMAT_PFI         = 35,   /**< PCE Flux Image */
    UFT_FORMAT_TC          = 36,   /**< TransCopy */
    
    /* Archive/compressed */
    UFT_FORMAT_IMZ         = 40,   /**< Compressed IMG (zip) */
    UFT_FORMAT_ADZ         = 41,   /**< Compressed ADF (gzip) */
    UFT_FORMAT_2MG         = 42    /**< Apple 2MG container */
} uft_format_type_t;

/**
 * @brief Format capabilities flags
 */
typedef enum {
    UFT_CAP_READ           = 0x0001,  /**< Can read format */
    UFT_CAP_WRITE          = 0x0002,  /**< Can write format */
    UFT_CAP_FLUX           = 0x0004,  /**< Contains flux data */
    UFT_CAP_BITSTREAM      = 0x0008,  /**< Contains bitstream data */
    UFT_CAP_SECTOR         = 0x0010,  /**< Contains sector data */
    UFT_CAP_WEAK_BITS      = 0x0020,  /**< Supports weak bits */
    UFT_CAP_METADATA       = 0x0040,  /**< Supports metadata */
    UFT_CAP_MULTI_REV      = 0x0080,  /**< Multiple revolutions */
    UFT_CAP_VARIABLE_RATE  = 0x0100,  /**< Variable data rate */
    UFT_CAP_COPY_PROTECT   = 0x0200   /**< Copy protection info */
} uft_format_caps_t;

/*============================================================================
 * SCP (SuperCardPro) Format
 *============================================================================*/

/** SCP signature "SCP" */
#define UFT_SCP_SIGNATURE           0x504353

/** SCP base capture resolution (25 ns) */
#define UFT_SCP_BASE_RESOLUTION     25

/** SCP track count */
#define UFT_SCP_TRACK_COUNT         168

/** SCP flags */
#define UFT_SCP_FLAG_INDEX          0x01  /**< Index signal present */
#define UFT_SCP_FLAG_TPI            0x02  /**< 96 TPI */
#define UFT_SCP_FLAG_RPM            0x04  /**< 360 RPM */
#define UFT_SCP_FLAG_TYPE           0x08  /**< Flux type valid */
#define UFT_SCP_FLAG_READONLY       0x10  /**< Write protected */
#define UFT_SCP_FLAG_FOOTER         0x20  /**< Footer present */
#define UFT_SCP_FLAG_EXTENDED       0x40  /**< Extended mode */
#define UFT_SCP_FLAG_NON_SCP        0x80  /**< Non-SCP capture */

/**
 * @brief SCP disk manufacturer codes
 */
typedef enum {
    UFT_SCP_MFR_CBM        = 0x00,
    UFT_SCP_MFR_ATARI      = 0x10,
    UFT_SCP_MFR_APPLE      = 0x20,
    UFT_SCP_MFR_PC         = 0x30,
    UFT_SCP_MFR_TANDY      = 0x40,
    UFT_SCP_MFR_TI         = 0x50,
    UFT_SCP_MFR_ROLAND     = 0x60,
    UFT_SCP_MFR_AMSTRAD    = 0x70,
    UFT_SCP_MFR_OTHER      = 0x80,
    UFT_SCP_MFR_TAPE       = 0xE0,
    UFT_SCP_MFR_HDD        = 0xF0
} uft_scp_manufacturer_t;

/**
 * @brief SCP file header (16 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  signature[3];    /**< "SCP" */
    uint8_t  version;         /**< Version (high nibble=major, low=minor) */
    uint8_t  disk_type;       /**< Manufacturer + subtype */
    uint8_t  revolutions;     /**< Number of revolutions */
    uint8_t  start_track;     /**< Starting track */
    uint8_t  end_track;       /**< Ending track */
    uint8_t  flags;           /**< Feature flags */
    uint8_t  bit_cell_width;  /**< Bit cell width (0=variable) */
    uint8_t  heads;           /**< Number of heads (0=both, 1=0 only, 2=1 only) */
    uint8_t  resolution;      /**< Resolution multiplier */
    uint32_t checksum;        /**< CRC32 (0=skip) */
} uft_scp_header_t;

/**
 * @brief SCP track header (4 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  signature[3];    /**< "TRK" */
    uint8_t  track_number;    /**< Track number */
} uft_scp_track_header_t;

/**
 * @brief SCP revolution entry (12 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t index_time;      /**< Time from index (25ns units) */
    uint32_t flux_count;      /**< Number of flux transitions */
    uint32_t data_offset;     /**< Offset to flux data (from track header) */
} uft_scp_revolution_t;

/*============================================================================
 * Kryoflux Format
 *============================================================================*/

/** Kryoflux default clocks */
#define UFT_KFX_DEFAULT_MCK         ((18432000.0 * 73.0) / 14.0 / 2.0)
#define UFT_KFX_DEFAULT_SCK         (UFT_KFX_DEFAULT_MCK / 2.0)
#define UFT_KFX_DEFAULT_ICK         (UFT_KFX_DEFAULT_MCK / 16.0)

/** Kryoflux OOB block types */
typedef enum {
    UFT_KFX_OOB_INVALID     = 0x00,
    UFT_KFX_OOB_STREAM_INFO = 0x01,
    UFT_KFX_OOB_INDEX       = 0x02,
    UFT_KFX_OOB_STREAM_END  = 0x03,
    UFT_KFX_OOB_KF_INFO     = 0x04,
    UFT_KFX_OOB_EOF         = 0x0D
} uft_kfx_oob_type_t;

/**
 * @brief Kryoflux stream info block
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t size;            /**< Block size */
    uint32_t stream_pos;      /**< Stream position */
    uint32_t transfer_time;   /**< Transfer time (ms) */
} uft_kfx_stream_info_t;

/**
 * @brief Kryoflux index block
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t size;            /**< Block size */
    uint32_t stream_pos;      /**< Stream position */
    uint32_t sample_counter;  /**< Sample counter */
    uint32_t index_counter;   /**< Index counter */
} uft_kfx_index_block_t;

/**
 * @brief Kryoflux stream end block
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t size;            /**< Block size */
    uint32_t stream_pos;      /**< Stream position */
    uint32_t hw_status;       /**< Hardware status (0=OK, 1=buffer, 2=no index) */
} uft_kfx_stream_end_t;

/*============================================================================
 * Teledisk (TD0) Format
 *============================================================================*/

/** Teledisk signature (normal) */
#define UFT_TD0_SIGNATURE_NORMAL    0x4454  /* "TD" */

/** Teledisk signature (compressed) */
#define UFT_TD0_SIGNATURE_COMPRESSED 0x6474  /* "td" */

/** TD0 sector flags */
#define UFT_TD0_SECTOR_CRC_ERROR    0x02
#define UFT_TD0_SECTOR_DELETED      0x04
#define UFT_TD0_SECTOR_SKIPPED      0x10
#define UFT_TD0_SECTOR_NO_DAM       0x20

/**
 * @brief Teledisk file header (12 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t signature;       /**< "TD" or "td" */
    uint8_t  sequence;        /**< Sequence number */
    uint8_t  check_seq;       /**< Check sequence */
    uint8_t  version;         /**< Version number */
    uint8_t  data_rate;       /**< Data rate (0=250K, 1=300K, 2=500K) */
    uint8_t  drive_type;      /**< Drive type */
    uint8_t  stepping;        /**< Stepping mode */
    uint8_t  dos_alloc;       /**< DOS allocation flag */
    uint8_t  heads;           /**< Number of heads */
    uint16_t crc;             /**< CRC16 */
} uft_td0_header_t;

/**
 * @brief Teledisk comment header (10 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t crc;             /**< CRC16 */
    uint16_t length;          /**< Comment length */
    uint8_t  year;            /**< Year (since 1900) */
    uint8_t  month;           /**< Month (1-12) */
    uint8_t  day;             /**< Day (1-31) */
    uint8_t  hour;            /**< Hour (0-23) */
    uint8_t  minute;          /**< Minute (0-59) */
    uint8_t  second;          /**< Second (0-59) */
} uft_td0_comment_t;

/**
 * @brief Teledisk track header (4 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  sectors;         /**< Sector count (0xFF = end) */
    uint8_t  cylinder;        /**< Cylinder number */
    uint8_t  head;            /**< Head number */
    uint8_t  crc;             /**< CRC8 */
} uft_td0_track_t;

/**
 * @brief Teledisk sector header (6 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  cylinder;        /**< Cylinder in ID */
    uint8_t  head;            /**< Head in ID */
    uint8_t  sector;          /**< Sector number */
    uint8_t  size_code;       /**< Size code (N) */
    uint8_t  flags;           /**< Sector flags */
    uint8_t  crc;             /**< Data CRC8 */
} uft_td0_sector_t;

/**
 * @brief TD0 CRC16 calculation (polynomial 0xA097)
 */
static inline uint16_t uft_td0_crc16(const uint8_t *data, size_t length, uint16_t init_crc)
{
    uint16_t crc = init_crc;
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            crc = (crc << 1) ^ ((crc & 0x8000) ? 0xA097 : 0);
        }
    }
    return crc;
}

/*============================================================================
 * IPF (SPS/CAPS) Format
 *============================================================================*/

/** IPF signature "CAPS" */
#define UFT_IPF_SIGNATURE           0x53504143

/** IPF chunk types */
typedef enum {
    UFT_IPF_CHUNK_CAPS     = 0x53504143,  /* 'CAPS' */
    UFT_IPF_CHUNK_INFO     = 0x4F464E49,  /* 'INFO' */
    UFT_IPF_CHUNK_IMAGE    = 0x47414D49,  /* 'IMAG' */
    UFT_IPF_CHUNK_DATA     = 0x41544144,  /* 'DATA' */
    UFT_IPF_CHUNK_COMMENT  = 0x544D4D43   /* 'CMMT' */
} uft_ipf_chunk_type_t;

/** IPF encoder types */
typedef enum {
    UFT_IPF_ENCODER_UNKNOWN = 0,
    UFT_IPF_ENCODER_V1      = 1,
    UFT_IPF_ENCODER_V2      = 2
} uft_ipf_encoder_t;

/**
 * @brief IPF chunk header (8 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t id;              /**< Chunk ID (4 chars, big-endian) */
    uint32_t size;            /**< Chunk size */
} uft_ipf_chunk_t;

/**
 * @brief IPF INFO record
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t media_type;
    uint32_t encoder_type;
    uint32_t encoder_rev;
    uint32_t file_key;
    uint32_t file_rev;
    uint32_t origin;
    uint32_t min_track;
    uint32_t max_track;
    uint32_t min_side;
    uint32_t max_side;
    uint32_t creation_date;
    uint32_t creation_time;
    uint32_t platform[4];
    uint32_t disk_number;
    uint32_t creator_id;
    uint32_t reserved[3];
} uft_ipf_info_t;

/*============================================================================
 * HFE (HxC Floppy Emulator) Format
 *============================================================================*/

/** HFE signature "HXCPICFE" */
#define UFT_HFE_SIGNATURE           "HXCPICFE"

/** HFE encoding modes */
typedef enum {
    UFT_HFE_ENCODING_ISOIBM_MFM    = 0x00,
    UFT_HFE_ENCODING_AMIGA_MFM     = 0x01,
    UFT_HFE_ENCODING_ISOIBM_FM     = 0x02,
    UFT_HFE_ENCODING_EMU_FM        = 0x03,
    UFT_HFE_ENCODING_UNKNOWN       = 0xFF
} uft_hfe_encoding_t;

/**
 * @brief HFE file header (512 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    char     signature[8];    /**< "HXCPICFE" */
    uint8_t  format_revision; /**< Format revision (0) */
    uint8_t  track_count;     /**< Number of tracks */
    uint8_t  side_count;      /**< Number of sides (1 or 2) */
    uint8_t  track_encoding;  /**< Track encoding mode */
    uint16_t bit_rate;        /**< Bit rate in Kbps */
    uint16_t rpm;             /**< RPM (0=default) */
    uint8_t  interface_mode;  /**< Interface mode */
    uint8_t  reserved;        /**< Reserved (1) */
    uint16_t track_list_offset; /**< Track list offset (in blocks) */
    uint8_t  write_allowed;   /**< Write allowed flag */
    uint8_t  single_step;     /**< Single step (0xFF=auto) */
    uint8_t  track0s0_altenc;
    uint8_t  track0s0_enc;
    uint8_t  track0s1_altenc;
    uint8_t  track0s1_enc;
} uft_hfe_header_t;

/**
 * @brief HFE track entry (4 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t offset;          /**< Track data offset (in blocks of 512) */
    uint16_t length;          /**< Track data length (bytes) */
} uft_hfe_track_entry_t;

/*============================================================================
 * IMD (ImageDisk) Format
 *============================================================================*/

/** IMD signature "IMD " */
#define UFT_IMD_SIGNATURE           "IMD "

/** IMD mode values */
typedef enum {
    UFT_IMD_MODE_500K_FM    = 0,
    UFT_IMD_MODE_300K_FM    = 1,
    UFT_IMD_MODE_250K_FM    = 2,
    UFT_IMD_MODE_500K_MFM   = 3,
    UFT_IMD_MODE_300K_MFM   = 4,
    UFT_IMD_MODE_250K_MFM   = 5
} uft_imd_mode_t;

/** IMD sector flags */
#define UFT_IMD_SECTOR_NORMAL       0x00
#define UFT_IMD_SECTOR_COMPRESSED   0x01
#define UFT_IMD_SECTOR_DELETED      0x02
#define UFT_IMD_SECTOR_ERROR        0x04

/**
 * @brief IMD track header
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  mode;
    uint8_t  cylinder;
    uint8_t  head;
    uint8_t  sectors;
    uint8_t  size_code;
} uft_imd_track_t;

/*============================================================================
 * Format Detection Functions
 *============================================================================*/

/**
 * @brief Detect format from file data
 */
uft_format_type_t uft_format_detect(const uint8_t *data, size_t size);

/**
 * @brief Detect format from file extension
 */
uft_format_type_t uft_format_detect_extension(const char *filename);

/**
 * @brief Get format capabilities
 */
uint32_t uft_format_get_caps(uft_format_type_t format);

/**
 * @brief Get format name string
 */
const char *uft_format_get_name(uft_format_type_t format);

/*============================================================================
 * SCP Parser Functions
 *============================================================================*/

typedef struct {
    uft_scp_header_t header;
    uint32_t *track_offsets;
    size_t track_count;
    uint8_t *data;
    size_t data_size;
} uft_scp_file_t;

int uft_scp_read(const uint8_t *data, size_t size, uft_scp_file_t *scp);
int uft_scp_get_track_flux(const uft_scp_file_t *scp, int track, int revolution,
                           double *out_deltas, size_t max_deltas);
void uft_scp_free(uft_scp_file_t *scp);

/*============================================================================
 * Kryoflux Parser Functions
 *============================================================================*/

typedef struct {
    double sck;
    double ick;
    double *flux_deltas;
    size_t flux_count;
    double *index_times;
    size_t index_count;
    uint32_t hw_status;
} uft_kfx_stream_t;

int uft_kfx_read_stream(const uint8_t *data, size_t size, uft_kfx_stream_t *stream);
void uft_kfx_free(uft_kfx_stream_t *stream);

/*============================================================================
 * LZHUF Compression (for TD0)
 *============================================================================*/

typedef struct {
    size_t window_size;
    size_t lookahead;
    size_t threshold;
    uint8_t precursor;
    bool has_header;
    size_t in_offset;
    size_t out_offset;
} uft_lzhuf_options_t;

static const uft_lzhuf_options_t UFT_LZHUF_TD0_OPTIONS = {
    .window_size = 4096,
    .lookahead = 60,
    .threshold = 2,
    .precursor = 0x20,
    .has_header = true,
    .in_offset = 0,
    .out_offset = 0
};

int uft_lzhuf_decompress(const uint8_t *src, size_t src_size,
                         uint8_t *dst, size_t dst_size,
                         const uft_lzhuf_options_t *opts);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMAT_PARSERS_H */
