/**
 * @file uft_ir_serialize.h
 * @brief UFT Intermediate Representation Serialization
 * 
 * P0-IR-004: Binary Serialization (.ufir)
 * P0-IR-005: JSON Serialization Export
 * 
 * Provides serialization for the UFT Intermediate Representation:
 * - Binary format (.ufir) for efficient storage
 * - JSON export for interchange and debugging
 * - Streaming support for large images
 * - Version compatibility
 * 
 * The binary format preserves all forensic information including:
 * - Raw bitstream data
 * - Timing information
 * - Multi-revolution data
 * - Confidence metrics
 * - Protection detection results
 */

#ifndef UFT_IR_SERIALIZE_H
#define UFT_IR_SERIALIZE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "uft/uft_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Format Constants
 *===========================================================================*/

/** UFIR file magic number */
#define UFT_IR_MAGIC            0x52494655  /**< "UFIR" in little-endian */

/** Current format version */
#define UFT_IR_VERSION_MAJOR    1
#define UFT_IR_VERSION_MINOR    0

/** Maximum supported tracks */
#define UFT_IR_MAX_TRACKS       168         /**< 84 cylinders × 2 sides */

/** Maximum supported revolutions per track */
#define UFT_IR_MAX_REVOLUTIONS  16

/** Compression types */
#define UFT_IR_COMP_NONE        0x00        /**< No compression */
#define UFT_IR_COMP_ZLIB        0x01        /**< zlib deflate */
#define UFT_IR_COMP_LZ4         0x02        /**< LZ4 (fast) */
#define UFT_IR_COMP_ZSTD        0x03        /**< Zstandard */

/** Block types */
#define UFT_IR_BLOCK_HEADER     0x01        /**< File header */
#define UFT_IR_BLOCK_METADATA   0x02        /**< Disk metadata */
#define UFT_IR_BLOCK_TRACK      0x10        /**< Track data */
#define UFT_IR_BLOCK_SECTOR     0x11        /**< Sector data */
#define UFT_IR_BLOCK_TIMING     0x12        /**< Timing data */
#define UFT_IR_BLOCK_FLUX       0x13        /**< Raw flux data */
#define UFT_IR_BLOCK_PROTECTION 0x20        /**< Protection info */
#define UFT_IR_BLOCK_CONFIDENCE 0x21        /**< Confidence data */
#define UFT_IR_BLOCK_INDEX      0xF0        /**< Track index */
#define UFT_IR_BLOCK_EOF        0xFF        /**< End of file */

/*===========================================================================
 * Binary Format Structures
 *===========================================================================*/

/**
 * @brief UFIR file header (32 bytes)
 */
UFT_PACK_BEGIN
typedef struct {
    uint32_t magic;             /**< UFT_IR_MAGIC */
    uint8_t  version_major;     /**< Major version */
    uint8_t  version_minor;     /**< Minor version */
    uint8_t  compression;       /**< Compression type */
    uint8_t  flags;             /**< Format flags */
    
    uint32_t track_count;       /**< Number of tracks */
    uint32_t total_size;        /**< Total file size (0 if streaming) */
    
    uint64_t creation_time;     /**< Unix timestamp */
    
    uint32_t checksum;          /**< Header checksum (CRC32) */
    uint32_t reserved;          /**< Reserved for future use */
} uft_ir_header_t;
UFT_PACK_END

/** Header flags */
#define UFT_IR_FLAG_HAS_TIMING      0x01    /**< Contains timing data */
#define UFT_IR_FLAG_HAS_FLUX        0x02    /**< Contains raw flux */
#define UFT_IR_FLAG_HAS_MULTIREV    0x04    /**< Multi-revolution data */
#define UFT_IR_FLAG_HAS_PROTECTION  0x08    /**< Protection analysis */
#define UFT_IR_FLAG_HAS_CONFIDENCE  0x10    /**< Confidence metrics */
#define UFT_IR_FLAG_STREAMING       0x80    /**< Streaming format */

/**
 * @brief Block header (8 bytes)
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  type;              /**< Block type */
    uint8_t  flags;             /**< Block flags */
    uint16_t track_id;          /**< Track identifier (type-specific) */
    uint32_t size;              /**< Block data size (after header) */
} uft_ir_block_header_t;
UFT_PACK_END

/**
 * @brief Track block header (16 bytes after block header)
 */
UFT_PACK_BEGIN
typedef struct {
    uint16_t track;             /**< Track number */
    uint8_t  side;              /**< Side (0 or 1) */
    uint8_t  encoding;          /**< Encoding type */
    uint8_t  sector_count;      /**< Number of sectors */
    uint8_t  revolution_count;  /**< Number of revolutions */
    uint16_t rpm;               /**< Measured RPM × 10 */
    
    uint32_t bitstream_size;    /**< Bitstream size in bits */
    uint32_t flags;             /**< Track flags */
} uft_ir_track_header_t;
UFT_PACK_END

/**
 * @brief Sector block header (12 bytes after block header)
 */
UFT_PACK_BEGIN
typedef struct {
    uint16_t track;             /**< Track number */
    uint8_t  side;              /**< Side */
    uint8_t  sector;            /**< Sector number */
    uint8_t  size_code;         /**< Size code */
    uint8_t  flags;             /**< Sector flags */
    uint16_t crc_stored;        /**< Stored CRC */
    uint16_t crc_calculated;    /**< Calculated CRC */
    uint16_t data_size;         /**< Data size in bytes */
} uft_ir_sector_header_t;
UFT_PACK_END

/** Sector flags */
#define UFT_IR_SECT_CRC_OK      0x01    /**< CRC valid */
#define UFT_IR_SECT_DELETED     0x02    /**< Deleted data mark */
#define UFT_IR_SECT_WEAK        0x04    /**< Contains weak bits */
#define UFT_IR_SECT_CORRECTED   0x08    /**< CRC was corrected */
#define UFT_IR_SECT_MULTIPLE    0x10    /**< Multiple interpretations */

/**
 * @brief Track index entry (8 bytes)
 */
UFT_PACK_BEGIN
typedef struct {
    uint16_t track;             /**< Track number */
    uint8_t  side;              /**< Side */
    uint8_t  sector_count;      /**< Sector count */
    uint32_t file_offset;       /**< Offset in file */
} uft_ir_index_entry_t;
UFT_PACK_END

/*===========================================================================
 * Serialization Context
 *===========================================================================*/

/**
 * @brief Serialization configuration
 */
typedef struct {
    uint8_t compression;        /**< Compression type */
    bool include_timing;        /**< Include timing data */
    bool include_flux;          /**< Include raw flux */
    bool include_multirev;      /**< Include multi-revolution */
    bool include_protection;    /**< Include protection analysis */
    bool include_confidence;    /**< Include confidence metrics */
    bool streaming;             /**< Use streaming format */
    int compression_level;      /**< Compression level (1-9) */
} uft_ir_serialize_config_t;

/**
 * @brief Serialization context
 */
typedef struct {
    FILE *file;                 /**< Output file handle */
    uft_ir_serialize_config_t config;
    
    /* Statistics */
    uint32_t tracks_written;
    uint32_t sectors_written;
    uint64_t bytes_written;
    
    /* Index building */
    uft_ir_index_entry_t *index;
    size_t index_count;
    size_t index_capacity;
    
    /* Error handling */
    int last_error;
    char error_msg[256];
} uft_ir_writer_t;

/**
 * @brief Deserialization context
 */
typedef struct {
    FILE *file;                 /**< Input file handle */
    uft_ir_header_t header;     /**< File header */
    
    /* Index */
    uft_ir_index_entry_t *index;
    size_t index_count;
    
    /* Current position */
    uint32_t current_track;
    uint64_t file_position;
    
    /* Error handling */
    int last_error;
    char error_msg[256];
} uft_ir_reader_t;

/*===========================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief Initialize serialization config with defaults
 * @param config Configuration to initialize
 */
void uft_ir_config_default(uft_ir_serialize_config_t *config);

/**
 * @brief Set config for full forensic preservation
 * @param config Configuration to initialize
 */
void uft_ir_config_forensic(uft_ir_serialize_config_t *config);

/**
 * @brief Set config for compact storage
 * @param config Configuration to initialize
 */
void uft_ir_config_compact(uft_ir_serialize_config_t *config);

/*===========================================================================
 * Binary Serialization (Writer)
 *===========================================================================*/

/**
 * @brief Create UFIR writer
 * @param path Output file path
 * @param config Serialization configuration
 * @return Writer context or NULL on error
 */
uft_ir_writer_t *uft_ir_writer_create(const char *path,
                                       const uft_ir_serialize_config_t *config);

/**
 * @brief Close writer and finalize file
 * @param writer Writer context (freed after call)
 * @return 0 on success
 */
int uft_ir_writer_close(uft_ir_writer_t *writer);

/**
 * @brief Write disk metadata block
 * @param writer Writer context
 * @param metadata Metadata key-value pairs (NULL-terminated)
 * @return 0 on success
 */
int uft_ir_write_metadata(uft_ir_writer_t *writer,
                          const char **metadata);

/**
 * @brief Write track data
 * @param writer Writer context
 * @param track Track number
 * @param side Side
 * @param encoding Encoding type
 * @param bitstream Bitstream data
 * @param bit_count Bitstream size in bits
 * @return 0 on success
 */
int uft_ir_write_track(uft_ir_writer_t *writer,
                       uint16_t track, uint8_t side, uint8_t encoding,
                       const uint8_t *bitstream, size_t bit_count);

/**
 * @brief Write sector data
 * @param writer Writer context
 * @param track Track number
 * @param side Side
 * @param sector Sector number
 * @param data Sector data
 * @param size Data size
 * @param crc_valid CRC validation result
 * @param stored_crc CRC from disk
 * @return 0 on success
 */
int uft_ir_write_sector(uft_ir_writer_t *writer,
                        uint16_t track, uint8_t side, uint8_t sector,
                        const uint8_t *data, size_t size,
                        bool crc_valid, uint16_t stored_crc);

/**
 * @brief Write timing data
 * @param writer Writer context
 * @param track Track number
 * @param side Side
 * @param timing Timing data (binary format)
 * @param size Timing data size
 * @return 0 on success
 */
int uft_ir_write_timing(uft_ir_writer_t *writer,
                        uint16_t track, uint8_t side,
                        const uint8_t *timing, size_t size);

/**
 * @brief Write raw flux data
 * @param writer Writer context
 * @param track Track number
 * @param side Side
 * @param flux_ns Flux intervals in nanoseconds
 * @param count Number of intervals
 * @return 0 on success
 */
int uft_ir_write_flux(uft_ir_writer_t *writer,
                      uint16_t track, uint8_t side,
                      const uint32_t *flux_ns, size_t count);

/*===========================================================================
 * Binary Deserialization (Reader)
 *===========================================================================*/

/**
 * @brief Open UFIR file for reading
 * @param path Input file path
 * @return Reader context or NULL on error
 */
uft_ir_reader_t *uft_ir_reader_open(const char *path);

/**
 * @brief Close reader
 * @param reader Reader context (freed after call)
 */
void uft_ir_reader_close(uft_ir_reader_t *reader);

/**
 * @brief Get file header
 * @param reader Reader context
 * @return Pointer to header (valid until reader closed)
 */
const uft_ir_header_t *uft_ir_get_header(const uft_ir_reader_t *reader);

/**
 * @brief Get track count
 * @param reader Reader context
 * @return Number of tracks
 */
uint32_t uft_ir_get_track_count(const uft_ir_reader_t *reader);

/**
 * @brief Check if track exists
 * @param reader Reader context
 * @param track Track number
 * @param side Side
 * @return true if track exists
 */
bool uft_ir_has_track(const uft_ir_reader_t *reader, uint16_t track, uint8_t side);

/**
 * @brief Read track bitstream
 * @param reader Reader context
 * @param track Track number
 * @param side Side
 * @param bitstream Output buffer (caller allocates)
 * @param max_bits Buffer capacity in bits
 * @param bit_count Output: actual bit count
 * @return 0 on success
 */
int uft_ir_read_track(uft_ir_reader_t *reader,
                      uint16_t track, uint8_t side,
                      uint8_t *bitstream, size_t max_bits, size_t *bit_count);

/**
 * @brief Read sector data
 * @param reader Reader context
 * @param track Track number
 * @param side Side
 * @param sector Sector number
 * @param data Output buffer (caller allocates)
 * @param max_size Buffer capacity
 * @param size Output: actual size
 * @return 0 on success
 */
int uft_ir_read_sector(uft_ir_reader_t *reader,
                       uint16_t track, uint8_t side, uint8_t sector,
                       uint8_t *data, size_t max_size, size_t *size);

/*===========================================================================
 * JSON Export
 *===========================================================================*/

/**
 * @brief JSON export configuration
 */
typedef struct {
    bool pretty_print;          /**< Format with indentation */
    bool include_bitstream;     /**< Include bitstream (base64) */
    bool include_timing;        /**< Include timing data */
    bool include_flux;          /**< Include flux data */
    bool include_hex_data;      /**< Include sector data as hex */
    int indent_spaces;          /**< Indent spaces (if pretty) */
} uft_ir_json_config_t;

/**
 * @brief Initialize JSON config with defaults
 * @param config Configuration to initialize
 */
void uft_ir_json_config_default(uft_ir_json_config_t *config);

/**
 * @brief Export UFIR to JSON file
 * @param reader Reader context
 * @param path Output JSON path
 * @param config JSON configuration
 * @return 0 on success
 */
int uft_ir_export_json(uft_ir_reader_t *reader, const char *path,
                       const uft_ir_json_config_t *config);

/**
 * @brief Export single track to JSON string
 * @param reader Reader context
 * @param track Track number
 * @param side Side
 * @param config JSON configuration
 * @param buffer Output buffer (caller allocates)
 * @param buffer_size Buffer size
 * @return Bytes written, or required size if buffer too small
 */
size_t uft_ir_track_to_json(uft_ir_reader_t *reader,
                            uint16_t track, uint8_t side,
                            const uft_ir_json_config_t *config,
                            char *buffer, size_t buffer_size);

/**
 * @brief Export metadata to JSON string
 * @param reader Reader context
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written
 */
size_t uft_ir_metadata_to_json(uft_ir_reader_t *reader,
                               char *buffer, size_t buffer_size);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Calculate CRC32 for data
 * @param data Data buffer
 * @param size Data size
 * @return CRC32 value
 */
uint32_t uft_ir_crc32(const uint8_t *data, size_t size);

/**
 * @brief Get last error message
 * @param writer Writer context (or NULL)
 * @param reader Reader context (or NULL)
 * @return Error message string
 */
const char *uft_ir_get_error(const uft_ir_writer_t *writer,
                             const uft_ir_reader_t *reader);

/**
 * @brief Verify UFIR file integrity
 * @param path File path
 * @return 0 if valid, error code otherwise
 */
int uft_ir_verify(const char *path);

/**
 * @brief Get format version string
 * @return Version string (e.g., "1.0")
 */
const char *uft_ir_version_string(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_IR_SERIALIZE_H */
