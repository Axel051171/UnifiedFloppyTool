/**
 * @file uft_diskcopy.h
 * @brief Apple Disk Copy and NDIF disk image format support
 * 
 * Supports:
 * - Disk Copy 4.2 format (.img, .image) - 84-byte header, raw sectors
 * - Disk Copy 6.x NDIF format (.img, .smi) - Resource fork metadata, ADC compression
 * - Self-Mounting Image (SMI) extraction
 * - MacBinary II wrapper detection and unwrapping
 * 
 * Reference: Apple DiskImages framework, Disk Copy 4.2 format specification
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#ifndef UFT_DISKCOPY_H
#define UFT_DISKCOPY_H

#pragma pack(push, 1)

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Format Constants
 * ============================================================================ */

/** Disk Copy 4.2 header size */
#define DC42_HEADER_SIZE        84

/** MacBinary II header size */
#define MACBINARY_HEADER_SIZE   128

/** Maximum volume name length (Pascal string) */
#define DC_MAX_VOLUME_NAME      63

/** Boot signature for valid boot sector */
#define DC_BOOT_SIGNATURE       0xAA55

/* ============================================================================
 * Disk Format Identifiers
 * ============================================================================ */

/**
 * @brief Disk encoding formats
 * Used in Disk Copy 4.2 header byte 80
 */
typedef enum {
    DC_FORMAT_GCR_400K      = 0,    /**< Mac 400K GCR (single-sided) */
    DC_FORMAT_GCR_800K      = 1,    /**< Mac 800K GCR (double-sided) */
    DC_FORMAT_MFM_720K      = 2,    /**< PC/Mac 720K MFM */
    DC_FORMAT_MFM_1440K     = 3,    /**< PC/Mac 1.44MB MFM HD */
    DC_FORMAT_CUSTOM        = 0xFF, /**< Custom format */
} dc_disk_format_t;

/**
 * @brief Standard Mac floppy disk sizes
 */
typedef enum {
    DC_SIZE_400K            = 409600,   /**< 400K: 800 sectors × 512 */
    DC_SIZE_800K            = 819200,   /**< 800K: 1600 sectors × 512 */
    DC_SIZE_720K            = 737280,   /**< 720K: 1440 sectors × 512 */
    DC_SIZE_1440K           = 1474560,  /**< 1.44MB: 2880 sectors × 512 */
} dc_disk_size_t;

/**
 * @brief Image format types detected
 */
typedef enum {
    DC_TYPE_UNKNOWN         = 0,
    DC_TYPE_DC42            = 1,    /**< Disk Copy 4.2 format */
    DC_TYPE_NDIF            = 2,    /**< NDIF (Disk Copy 6.x) */
    DC_TYPE_UDIF            = 3,    /**< UDIF (.dmg) - for detection only */
    DC_TYPE_RAW             = 4,    /**< Raw sector image */
    DC_TYPE_SMI             = 5,    /**< Self-Mounting Image */
} dc_image_type_t;

/**
 * @brief MacBinary file types
 */
typedef enum {
    MB_TYPE_NONE            = 0,
    MB_TYPE_MACBINARY_I     = 1,
    MB_TYPE_MACBINARY_II    = 2,
    MB_TYPE_MACBINARY_III   = 3,
} macbinary_type_t;

/* ============================================================================
 * Disk Copy 4.2 Header Structure (84 bytes)
 * ============================================================================ */

/**
 * @brief Disk Copy 4.2 header
 * 
 * Offsets:
 *   0-63:  Volume name (Pascal string, max 63 chars + length byte)
 *   64-67: Data size (big-endian)
 *   68-71: Tag size (big-endian)
 *   72-75: Data checksum (big-endian)
 *   76-79: Tag checksum (big-endian)
 *   80:    Disk encoding (0=GCR400K, 1=GCR800K, 2=MFM720K, 3=MFM1440K)
 *   81:    Format byte (0x22 = Mac, 0x24 = ProDOS)
 *   82-83: Private/Magic (should be 0x0100)
 */
typedef struct {
    uint8_t     volume_name[64];    /**< Pascal string: length + name */
    uint32_t    data_size;          /**< Sector data size (big-endian) */
    uint32_t    tag_size;           /**< Tag data size (big-endian) */
    uint32_t    data_checksum;      /**< Data checksum (big-endian) */
    uint32_t    tag_checksum;       /**< Tag checksum (big-endian) */
    uint8_t     disk_encoding;      /**< Disk format (dc_disk_format_t) */
    uint8_t     format_byte;        /**< 0x22=Mac, 0x24=ProDOS */
    uint16_t    private_word;       /**< Should be 0x0100 (big-endian) */
} dc42_header_t;

/* ============================================================================
 * MacBinary II Header Structure (128 bytes)
 * ============================================================================ */

/**
 * @brief MacBinary II/III header
 */
typedef struct {
    uint8_t     old_version;        /**< 0x00: Old version (must be 0) */
    uint8_t     filename_len;       /**< 0x01: Filename length (1-63) */
    uint8_t     filename[63];       /**< 0x02-0x40: Filename */
    uint8_t     file_type[4];       /**< 0x41-0x44: File type (e.g., "APPL") */
    uint8_t     creator[4];         /**< 0x45-0x48: Creator (e.g., "oneb") */
    uint8_t     finder_flags_hi;    /**< 0x49: Finder flags (high byte) */
    uint8_t     zero1;              /**< 0x4A: Must be 0 */
    uint16_t    vert_pos;           /**< 0x4B-0x4C: Vertical position */
    uint16_t    horiz_pos;          /**< 0x4D-0x4E: Horizontal position */
    uint16_t    folder_id;          /**< 0x4F-0x50: Folder ID */
    uint8_t     protected_flag;     /**< 0x51: Protected flag */
    uint8_t     zero2;              /**< 0x52: Must be 0 */
    uint32_t    data_fork_len;      /**< 0x53-0x56: Data fork length (big-endian) */
    uint32_t    rsrc_fork_len;      /**< 0x57-0x5A: Resource fork length (big-endian) */
    uint32_t    creation_date;      /**< 0x5B-0x5E: Creation date */
    uint32_t    modified_date;      /**< 0x5F-0x62: Modification date */
    uint16_t    get_info_len;       /**< 0x63-0x64: Get Info comment length */
    uint8_t     finder_flags_lo;    /**< 0x65: Finder flags (low byte) */
    uint8_t     reserved1[14];      /**< 0x66-0x73: Reserved */
    uint32_t    unpacked_len;       /**< 0x74-0x77: Unpacked length */
    uint16_t    secondary_header;   /**< 0x78-0x79: Secondary header length */
    uint8_t     version;            /**< 0x7A: MacBinary version (129=II, 130=III) */
    uint8_t     min_version;        /**< 0x7B: Minimum version to read (129) */
    uint16_t    crc;                /**< 0x7C-0x7D: CRC of header bytes 0-123 */
    uint8_t     reserved2[2];       /**< 0x7E-0x7F: Reserved */
} macbinary_header_t;

/* ============================================================================
 * NDIF Resource Types
 * ============================================================================ */

/**
 * @brief NDIF block descriptor
 * Used in 'bcem' resource for block mapping
 */
typedef struct {
    uint32_t    disk_offset;        /**< Offset in logical disk */
    uint32_t    data_offset;        /**< Offset in compressed data */
    uint32_t    size;               /**< Size of block */
    uint32_t    type;               /**< Block type (raw, ADC, etc.) */
} ndif_block_t;

/**
 * @brief NDIF compression types
 */
typedef enum {
    NDIF_BLOCK_RAW          = 0,    /**< Uncompressed data */
    NDIF_BLOCK_ADC          = 1,    /**< ADC compressed */
    NDIF_BLOCK_ZERO         = 2,    /**< Zero-filled block */
    NDIF_BLOCK_COPY         = 3,    /**< Copy from another location */
} ndif_block_type_t;

/* ============================================================================
 * Analysis Results
 * ============================================================================ */

/**
 * @brief Disk Copy image analysis result
 */
typedef struct {
    /* Image identification */
    dc_image_type_t     image_type;         /**< Detected image type */
    macbinary_type_t    macbinary_type;     /**< MacBinary wrapper type */
    bool                is_valid;           /**< Image is valid */
    
    /* Volume information */
    char                volume_name[DC_MAX_VOLUME_NAME + 1];
    dc_disk_format_t    disk_format;        /**< Disk encoding format */
    uint32_t            sector_count;       /**< Number of sectors */
    uint32_t            sector_size;        /**< Bytes per sector (usually 512) */
    
    /* Size information */
    uint32_t            data_size;          /**< Sector data size */
    uint32_t            tag_size;           /**< Tag data size (12 bytes/sector for GCR) */
    uint32_t            total_size;         /**< Total disk size */
    
    /* Checksums */
    uint32_t            data_checksum;      /**< Stored data checksum */
    uint32_t            tag_checksum;       /**< Stored tag checksum */
    uint32_t            calculated_checksum;/**< Calculated checksum */
    bool                checksum_valid;     /**< Checksums match */
    
    /* MacBinary info (if wrapped) */
    char                mb_filename[64];    /**< MacBinary filename */
    char                mb_type[5];         /**< File type code */
    char                mb_creator[5];      /**< Creator code */
    uint32_t            mb_data_fork_len;   /**< Data fork length */
    uint32_t            mb_rsrc_fork_len;   /**< Resource fork length */
    
    /* Offsets for extraction */
    uint32_t            header_offset;      /**< Offset to DC/NDIF header */
    uint32_t            data_offset;        /**< Offset to sector data */
    uint32_t            tag_offset;         /**< Offset to tag data */
    uint32_t            rsrc_offset;        /**< Offset to resource fork */
    
    /* NDIF specific */
    bool                is_compressed;      /**< Data is compressed */
    uint32_t            block_count;        /**< Number of NDIF blocks */
    
    /* SMI specific */
    bool                has_stub;           /**< Has executable stub */
    uint32_t            stub_size;          /**< Size of 68K stub */
    
    /* Format details */
    uint8_t             format_byte;        /**< Original format byte */
    char                format_description[64];
} dc_analysis_result_t;

/* ============================================================================
 * API Functions - Detection and Analysis
 * ============================================================================ */

/**
 * @brief Detect image format from file data
 * @param data File data
 * @param size Size of data
 * @return Detected image type
 */
dc_image_type_t dc_detect_format(const uint8_t *data, size_t size);

/**
 * @brief Detect MacBinary wrapper
 * @param data File data
 * @param size Size of data
 * @return MacBinary type or MB_TYPE_NONE
 */
macbinary_type_t dc_detect_macbinary(const uint8_t *data, size_t size);

/**
 * @brief Analyze Disk Copy image
 * @param data File data
 * @param size Size of data
 * @param result Output analysis result
 * @return 0 on success, negative on error
 */
int dc_analyze(const uint8_t *data, size_t size, dc_analysis_result_t *result);

/**
 * @brief Validate Disk Copy 4.2 header
 * @param header Pointer to header data
 * @return true if valid DC42 header
 */
bool dc42_validate_header(const dc42_header_t *header);

/**
 * @brief Parse Disk Copy 4.2 header
 * @param data File data (at header offset)
 * @param size Size of data
 * @param result Output analysis result
 * @return 0 on success, negative on error
 */
int dc42_parse_header(const uint8_t *data, size_t size, dc_analysis_result_t *result);

/**
 * @brief Parse MacBinary header
 * @param data File data
 * @param size Size of data
 * @param result Output analysis result
 * @return 0 on success, negative on error
 */
int macbinary_parse_header(const uint8_t *data, size_t size, dc_analysis_result_t *result);

/* ============================================================================
 * API Functions - Data Extraction
 * ============================================================================ */

/**
 * @brief Extract raw disk data from Disk Copy image
 * @param data Source file data
 * @param size Size of source data
 * @param result Previous analysis result
 * @param output Output buffer for raw disk data
 * @param output_size Size of output buffer
 * @return Number of bytes written, or negative on error
 */
int dc_extract_disk_data(const uint8_t *data, size_t size,
                          const dc_analysis_result_t *result,
                          uint8_t *output, size_t output_size);

/**
 * @brief Extract tag data from Disk Copy image (if present)
 * @param data Source file data
 * @param size Size of source data
 * @param result Previous analysis result
 * @param output Output buffer for tag data
 * @param output_size Size of output buffer
 * @return Number of bytes written, or negative on error
 */
int dc_extract_tag_data(const uint8_t *data, size_t size,
                         const dc_analysis_result_t *result,
                         uint8_t *output, size_t output_size);

/**
 * @brief Unwrap MacBinary to get raw data/resource forks
 * @param data MacBinary file data
 * @param size Size of data
 * @param data_fork Output for data fork (can be NULL)
 * @param data_fork_size Size of data fork buffer
 * @param rsrc_fork Output for resource fork (can be NULL)
 * @param rsrc_fork_size Size of resource fork buffer
 * @return 0 on success, negative on error
 */
int macbinary_unwrap(const uint8_t *data, size_t size,
                      uint8_t *data_fork, size_t data_fork_size,
                      uint8_t *rsrc_fork, size_t rsrc_fork_size);

/* ============================================================================
 * API Functions - Checksum
 * ============================================================================ */

/**
 * @brief Calculate Disk Copy checksum
 * Algorithm: Running sum with rotate-right-1
 * @param data Data to checksum
 * @param size Size of data
 * @return Calculated checksum
 */
uint32_t dc_calculate_checksum(const uint8_t *data, size_t size);

/**
 * @brief Verify data checksum
 * @param data Source file data
 * @param size Size of data
 * @param result Analysis result with stored checksum
 * @return true if checksum matches
 */
bool dc_verify_checksum(const uint8_t *data, size_t size,
                         const dc_analysis_result_t *result);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Get disk format description string
 * @param format Disk format
 * @return Human-readable description
 */
const char *dc_format_description(dc_disk_format_t format);

/**
 * @brief Get image type description string
 * @param type Image type
 * @return Human-readable description
 */
const char *dc_type_description(dc_image_type_t type);

/**
 * @brief Generate analysis report
 * @param result Analysis result
 * @param buffer Output buffer
 * @param buffer_size Size of buffer
 * @return Number of characters written
 */
int dc_generate_report(const dc_analysis_result_t *result,
                        char *buffer, size_t buffer_size);

/**
 * @brief Get expected disk size for format
 * @param format Disk format
 * @return Expected size in bytes, or 0 if unknown
 */
uint32_t dc_expected_size(dc_disk_format_t format);

/**
 * @brief Determine disk format from size
 * @param data_size Size of sector data
 * @return Best matching format
 */
dc_disk_format_t dc_format_from_size(uint32_t data_size);

/* ============================================================================
 * API Functions - Creation
 * ============================================================================ */

/**
 * @brief Create Disk Copy 4.2 header
 * @param volume_name Volume name
 * @param format Disk format
 * @param data Sector data for checksum
 * @param data_size Size of sector data
 * @param tag_data Tag data for checksum (can be NULL)
 * @param tag_size Size of tag data
 * @param header Output header buffer (must be 84 bytes)
 * @return 0 on success, negative on error
 */
int dc42_create_header(const char *volume_name,
                        dc_disk_format_t format,
                        const uint8_t *data, size_t data_size,
                        const uint8_t *tag_data, size_t tag_size,
                        dc42_header_t *header);

/**
 * @brief Create complete Disk Copy 4.2 image
 * @param volume_name Volume name
 * @param format Disk format
 * @param data Sector data
 * @param data_size Size of sector data
 * @param output Output buffer
 * @param output_size Size of output buffer
 * @return Number of bytes written, or negative on error
 */
int dc42_create_image(const char *volume_name,
                       dc_disk_format_t format,
                       const uint8_t *data, size_t data_size,
                       uint8_t *output, size_t output_size);

/* ============================================================================
 * NDIF Specific Functions
 * ============================================================================ */

/**
 * @brief Check if NDIF image is compressed
 * @param data File data
 * @param size Size of data
 * @param result Analysis result
 * @return true if compressed
 */
bool ndif_is_compressed(const uint8_t *data, size_t size,
                         const dc_analysis_result_t *result);

/**
 * @brief Decompress ADC block
 * ADC (Apple Data Compression) is a simple LZ-style compression
 * @param input Compressed data
 * @param input_size Size of compressed data
 * @param output Output buffer
 * @param output_size Size of output buffer
 * @return Decompressed size, or negative on error
 */
int adc_decompress(const uint8_t *input, size_t input_size,
                    uint8_t *output, size_t output_size);

/* ============================================================================
 * SMI (Self-Mounting Image) Functions
 * ============================================================================ */

/**
 * @brief Detect SMI executable stub
 * @param data File data
 * @param size Size of data
 * @return Size of stub, or 0 if not SMI
 */
uint32_t smi_detect_stub(const uint8_t *data, size_t size);

/**
 * @brief Extract disk image from SMI
 * @param data SMI file data
 * @param size Size of data
 * @param output Output buffer for disk image
 * @param output_size Size of output buffer
 * @return Size of extracted image, or negative on error
 */
int smi_extract_image(const uint8_t *data, size_t size,
                       uint8_t *output, size_t output_size);

#ifdef __cplusplus
}
#endif

#pragma pack(pop)

#endif /* UFT_DISKCOPY_H */
