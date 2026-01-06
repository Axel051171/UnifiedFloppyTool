/**
 * @file uft_imd.h
 * @brief ImageDisk (IMD) Format Support for UFT
 * 
 * ImageDisk is a disk image format created by Dave Dunfield for archiving
 * floppy disks. It supports multiple data rates, sector sizes, and densities.
 * 
 * Format specification based on IMD documentation and source code analysis.
 * 
 * @copyright UFT Project - Based on public domain IMD format specification
 */

#ifndef UFT_IMD_H
#define UFT_IMD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * IMD Format Constants
 *============================================================================*/

/** IMD file signature */
#define UFT_IMD_SIGNATURE       "IMD "

/** IMD comment terminator (ASCII EOF) */
#define UFT_IMD_COMMENT_END     0x1A

/** Maximum sectors per track */
#define UFT_IMD_MAX_SECTORS     256

/** Maximum track size in bytes */
#define UFT_IMD_MAX_TRACK_SIZE  32768

/*============================================================================
 * IMD Mode Values (Data Rate / Density)
 *============================================================================*/

/**
 * @brief IMD track mode values
 * 
 * Mode encodes both data transfer rate and recording density:
 * - Modes 0-2: FM (Single Density)
 * - Modes 3-5: MFM (Double Density)
 */
typedef enum {
    UFT_IMD_MODE_500K_FM  = 0,  /**< 500 kbps FM (250 kbps effective) */
    UFT_IMD_MODE_300K_FM  = 1,  /**< 300 kbps FM (150 kbps effective) */
    UFT_IMD_MODE_250K_FM  = 2,  /**< 250 kbps FM (125 kbps effective) */
    UFT_IMD_MODE_500K_MFM = 3,  /**< 500 kbps MFM */
    UFT_IMD_MODE_300K_MFM = 4,  /**< 300 kbps MFM */
    UFT_IMD_MODE_250K_MFM = 5,  /**< 250 kbps MFM */
    UFT_IMD_MODE_MAX      = 5
} uft_imd_mode_t;

/**
 * @brief Get data rate in kbps from mode
 */
static inline uint16_t uft_imd_mode_to_rate(uft_imd_mode_t mode) {
    static const uint16_t rates[] = { 500, 300, 250, 500, 300, 250 };
    return (mode <= UFT_IMD_MODE_MAX) ? rates[mode] : 0;
}

/**
 * @brief Check if mode is MFM (double density)
 */
static inline bool uft_imd_mode_is_mfm(uft_imd_mode_t mode) {
    return mode >= UFT_IMD_MODE_500K_MFM;
}

/**
 * @brief Get mode name string
 */
static inline const char* uft_imd_mode_name(uft_imd_mode_t mode) {
    static const char* names[] = {
        "500K FM", "300K FM", "250K FM",
        "500K MFM", "300K MFM", "250K MFM"
    };
    return (mode <= UFT_IMD_MODE_MAX) ? names[mode] : "Unknown";
}

/*============================================================================
 * IMD Sector Size Encoding
 *============================================================================*/

/**
 * @brief IMD sector size codes
 * Actual size = 128 << code
 */
typedef enum {
    UFT_IMD_SSIZE_128  = 0,  /**< 128 bytes/sector */
    UFT_IMD_SSIZE_256  = 1,  /**< 256 bytes/sector */
    UFT_IMD_SSIZE_512  = 2,  /**< 512 bytes/sector */
    UFT_IMD_SSIZE_1024 = 3,  /**< 1024 bytes/sector */
    UFT_IMD_SSIZE_2048 = 4,  /**< 2048 bytes/sector */
    UFT_IMD_SSIZE_4096 = 5,  /**< 4096 bytes/sector */
    UFT_IMD_SSIZE_8192 = 6,  /**< 8192 bytes/sector */
    UFT_IMD_SSIZE_VAR  = 0xFF /**< Variable size (extension) */
} uft_imd_ssize_t;

/**
 * @brief Convert sector size code to actual bytes
 */
static inline uint16_t uft_imd_ssize_to_bytes(uint8_t code) {
    if (code == UFT_IMD_SSIZE_VAR) return 0;
    if (code > 6) return 0;
    return 128U << code;
}

/**
 * @brief Convert sector size in bytes to code
 */
static inline uint8_t uft_imd_bytes_to_ssize(uint16_t bytes) {
    switch (bytes) {
        case 128:  return 0;
        case 256:  return 1;
        case 512:  return 2;
        case 1024: return 3;
        case 2048: return 4;
        case 4096: return 5;
        case 8192: return 6;
        default:   return 0xFF;
    }
}

/*============================================================================
 * IMD Head Flags
 *============================================================================*/

/** Head value mask (actual head number) */
#define UFT_IMD_HEAD_MASK       0x01

/** Sector Cylinder Map present flag */
#define UFT_IMD_HEAD_CYLMAP     0x80

/** Sector Head Map present flag */
#define UFT_IMD_HEAD_HEADMAP    0x40

/*============================================================================
 * IMD Sector Data Record Types
 *============================================================================*/

/**
 * @brief IMD sector data record types
 */
typedef enum {
    UFT_IMD_SEC_UNAVAIL       = 0x00, /**< Sector data unavailable */
    UFT_IMD_SEC_NORMAL        = 0x01, /**< Normal data follows */
    UFT_IMD_SEC_COMPRESSED    = 0x02, /**< Compressed (all same value) */
    UFT_IMD_SEC_DELETED       = 0x03, /**< Deleted data mark */
    UFT_IMD_SEC_DEL_COMP      = 0x04, /**< Deleted + compressed */
    UFT_IMD_SEC_ERROR         = 0x05, /**< Normal with read error */
    UFT_IMD_SEC_ERR_COMP      = 0x06, /**< Error + compressed */
    UFT_IMD_SEC_DEL_ERROR     = 0x07, /**< Deleted with error */
    UFT_IMD_SEC_DEL_ERR_COMP  = 0x08  /**< Deleted + error + compressed */
} uft_imd_sectype_t;

/**
 * @brief Check if sector type indicates data is present
 */
static inline bool uft_imd_sec_has_data(uint8_t type) {
    return type != UFT_IMD_SEC_UNAVAIL;
}

/**
 * @brief Check if sector type indicates compressed data
 */
static inline bool uft_imd_sec_is_compressed(uint8_t type) {
    return (type == UFT_IMD_SEC_COMPRESSED) ||
           (type == UFT_IMD_SEC_DEL_COMP) ||
           (type == UFT_IMD_SEC_ERR_COMP) ||
           (type == UFT_IMD_SEC_DEL_ERR_COMP);
}

/**
 * @brief Check if sector has deleted address mark
 */
static inline bool uft_imd_sec_is_deleted(uint8_t type) {
    return (type >= UFT_IMD_SEC_DELETED && type <= UFT_IMD_SEC_DEL_COMP) ||
           (type >= UFT_IMD_SEC_DEL_ERROR);
}

/**
 * @brief Check if sector had read error
 */
static inline bool uft_imd_sec_has_error(uint8_t type) {
    return type >= UFT_IMD_SEC_ERROR;
}

/*============================================================================
 * IMD File Structures
 *============================================================================*/

/**
 * @brief IMD file header (parsed from ASCII header line)
 */
typedef struct {
    uint8_t  version_major;     /**< IMD version major */
    uint8_t  version_minor;     /**< IMD version minor */
    uint8_t  day;               /**< Creation day */
    uint8_t  month;             /**< Creation month */
    uint16_t year;              /**< Creation year */
    uint8_t  hour;              /**< Creation hour */
    uint8_t  minute;            /**< Creation minute */
    uint8_t  second;            /**< Creation second */
} uft_imd_header_t;

/**
 * @brief IMD track header (binary format in file)
 */
typedef struct __attribute__((packed)) {
    uint8_t mode;               /**< Mode (data rate/density) */
    uint8_t cylinder;           /**< Cylinder number (0-255) */
    uint8_t head;               /**< Head (0-1) + optional map flags */
    uint8_t nsectors;           /**< Number of sectors */
    uint8_t sector_size;        /**< Sector size code */
} uft_imd_track_header_t;

/**
 * @brief IMD track data (expanded for processing)
 */
typedef struct {
    uft_imd_track_header_t header;
    
    uint8_t  smap[UFT_IMD_MAX_SECTORS];  /**< Sector numbering map */
    uint8_t  cmap[UFT_IMD_MAX_SECTORS];  /**< Cylinder map (optional) */
    uint8_t  hmap[UFT_IMD_MAX_SECTORS];  /**< Head map (optional) */
    uint8_t  stype[UFT_IMD_MAX_SECTORS]; /**< Sector types */
    uint16_t ssize[UFT_IMD_MAX_SECTORS]; /**< Sector sizes (if variable) */
    
    bool     has_cylmap;        /**< Cylinder map present */
    bool     has_headmap;       /**< Head map present */
    bool     has_varsizes;      /**< Variable sector sizes */
    
    uint8_t* data;              /**< Sector data buffer */
    size_t   data_size;         /**< Total data size */
    size_t   sector_offsets[UFT_IMD_MAX_SECTORS]; /**< Offset to each sector */
} uft_imd_track_t;

/**
 * @brief IMD image structure
 */
typedef struct {
    uft_imd_header_t header;    /**< Parsed header */
    char*    comment;           /**< Comment text (null-terminated) */
    size_t   comment_len;       /**< Comment length */
    
    uint16_t num_tracks;        /**< Number of tracks */
    uint16_t num_cylinders;     /**< Number of cylinders */
    uint8_t  num_heads;         /**< Number of heads (1 or 2) */
    
    uft_imd_track_t* tracks;    /**< Track array */
    
    /* Statistics */
    uint32_t total_sectors;     /**< Total sector count */
    uint32_t compressed_sectors;/**< Compressed sector count */
    uint32_t deleted_sectors;   /**< Deleted sector count */
    uint32_t bad_sectors;       /**< Bad sector count */
    uint32_t unavail_sectors;   /**< Unavailable sector count */
} uft_imd_image_t;

/*============================================================================
 * Gap Length Table (from IMD source)
 *============================================================================*/

/**
 * @brief Gap length table entry
 * Used to determine appropriate gap lengths for formatting
 */
typedef struct {
    uint8_t  sector_size;       /**< Sector size code */
    uint8_t  max_sectors;       /**< Maximum sectors for this config */
    uint8_t  gap_write;         /**< Gap3 for write operations */
    uint8_t  gap_format;        /**< Gap3 for format operations */
} uft_imd_gap_entry_t;

/**
 * @brief Gap length tables by media type
 */
extern const uft_imd_gap_entry_t uft_imd_gap_8inch_fm[];
extern const uft_imd_gap_entry_t uft_imd_gap_8inch_mfm[];
extern const uft_imd_gap_entry_t uft_imd_gap_5inch_fm[];
extern const uft_imd_gap_entry_t uft_imd_gap_5inch_mfm[];

/*============================================================================
 * IMD API Functions
 *============================================================================*/

/**
 * @brief Initialize IMD image structure
 * @param img Image structure to initialize
 * @return UFT_ERR_OK on success
 */
int uft_imd_init(uft_imd_image_t* img);

/**
 * @brief Free IMD image resources
 * @param img Image to free
 */
void uft_imd_free(uft_imd_image_t* img);

/**
 * @brief Parse IMD header line
 * @param line Header line (e.g., "IMD 1.18: 01/01/2024 12:00:00")
 * @param header Output header structure
 * @return UFT_ERR_OK on success
 */
int uft_imd_parse_header(const char* line, uft_imd_header_t* header);

/**
 * @brief Read IMD image from file
 * @param filename Path to IMD file
 * @param img Output image structure
 * @return UFT_ERR_OK on success
 */
int uft_imd_read(const char* filename, uft_imd_image_t* img);

/**
 * @brief Read IMD image from memory buffer
 * @param data Buffer containing IMD data
 * @param size Size of buffer
 * @param img Output image structure
 * @return UFT_ERR_OK on success
 */
int uft_imd_read_mem(const uint8_t* data, size_t size, uft_imd_image_t* img);

/**
 * @brief Write IMD image to file
 * @param filename Output filename
 * @param img Image to write
 * @return UFT_ERR_OK on success
 */
int uft_imd_write(const char* filename, const uft_imd_image_t* img);

/**
 * @brief Convert IMD image to raw binary
 * @param img Source IMD image
 * @param data Output buffer (caller must free)
 * @param size Output size
 * @param fill Fill byte for missing sectors
 * @return UFT_ERR_OK on success
 */
int uft_imd_to_raw(const uft_imd_image_t* img, uint8_t** data, 
                   size_t* size, uint8_t fill);

/**
 * @brief Create IMD image from raw binary
 * @param data Raw binary data
 * @param size Data size
 * @param params Format parameters
 * @param img Output IMD image
 * @return UFT_ERR_OK on success
 */
int uft_imd_from_raw(const uint8_t* data, size_t size,
                     const uft_imd_track_header_t* params,
                     uft_imd_image_t* img);

/**
 * @brief Get track by cylinder and head
 * @param img IMD image
 * @param cylinder Cylinder number
 * @param head Head number
 * @return Track pointer or NULL if not found
 */
uft_imd_track_t* uft_imd_get_track(uft_imd_image_t* img, 
                                    uint8_t cylinder, uint8_t head);

/**
 * @brief Read sector data
 * @param track Track containing sector
 * @param sector_num Sector number (from smap)
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Bytes read or negative error
 */
int uft_imd_read_sector(const uft_imd_track_t* track, uint8_t sector_num,
                        uint8_t* buffer, size_t size);

/**
 * @brief Get recommended gap lengths
 * @param mode Track mode
 * @param sector_size Sector size code
 * @param nsectors Number of sectors
 * @param gap_write Output gap for write
 * @param gap_format Output gap for format
 * @return UFT_ERR_OK on success
 */
int uft_imd_get_gap_lengths(uft_imd_mode_t mode, uint8_t sector_size,
                            uint8_t nsectors, uint8_t* gap_write,
                            uint8_t* gap_format);

/**
 * @brief Validate IMD image
 * @param img Image to validate
 * @return UFT_ERR_OK if valid
 */
int uft_imd_validate(const uft_imd_image_t* img);

/**
 * @brief Print IMD image information
 * @param img Image to describe
 * @param verbose Verbose output flag
 */
void uft_imd_print_info(const uft_imd_image_t* img, bool verbose);

#ifdef __cplusplus
}
#endif

#endif /* UFT_IMD_H */
