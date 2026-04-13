/**
 * @file uft_scp_parser.h
 * @brief SuperCard Pro (SCP) Format Parser
 * @version 1.0.0
 * @date 2026-01-06
 *
 */

#ifndef UFT_SCP_PARSER_H
#define UFT_SCP_PARSER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

#ifndef UFT_SCP_SIGNATURE
#define UFT_SCP_SIGNATURE       "SCP"
#endif
#define UFT_SCP_TRACK_SIG       "TRK"
#define UFT_SCP_FOOTER_SIG      "FPCS"
#define UFT_SCP_MAX_TRACKS      168
#define UFT_SCP_MAX_REVOLUTIONS 5
#define UFT_SCP_BASE_PERIOD_NS  25  /* 25ns base resolution */

/*============================================================================
 * Disk Types
 *============================================================================*/

/* Manufacturers (upper nibble) */
#define UFT_SCP_MAN_CBM         0x00
#define UFT_SCP_MAN_ATARI       0x10
#define UFT_SCP_MAN_APPLE       0x20
#define UFT_SCP_MAN_PC          0x30
#define UFT_SCP_MAN_TANDY       0x40
#define UFT_SCP_MAN_TI          0x50
#define UFT_SCP_MAN_ROLAND      0x60
#define UFT_SCP_MAN_OTHER       0x80

/* SCP disk type constants are defined as enums in uft_scp_format.h
 * (uft_scp_disk_type_t). Do NOT duplicate as #defines here. */

/*============================================================================
 * Flags
 *============================================================================*/

#ifndef UFT_SCP_FLAGS_DEFINED
#define UFT_SCP_FLAGS_DEFINED
#define UFT_SCP_FLAG_INDEX      0x01  /* Used index mark */
#define UFT_SCP_FLAG_96TPI      0x02  /* 96 TPI drive */
#define UFT_SCP_FLAG_360RPM     0x04  /* 360 RPM (vs 300) */
#define UFT_SCP_FLAG_NORMALIZED 0x08  /* Quality reduced */
#define UFT_SCP_FLAG_RW         0x10  /* Read/Write capable */
#define UFT_SCP_FLAG_FOOTER     0x20  /* Has extension footer */
#define UFT_SCP_FLAG_EXTENDED   0x40  /* Extended mode */
#define UFT_SCP_FLAG_CREATOR    0x80  /* Creator info */
#endif /* UFT_SCP_FLAGS_DEFINED */

/*============================================================================
 * Structures
 *============================================================================*/

#pragma pack(push, 1)

/**
 * @brief SCP file header (16 bytes)
 */
#ifndef UFT_SCP_HEADER_T_DEFINED
#define UFT_SCP_HEADER_T_DEFINED
typedef struct {
    uint8_t  signature[3];      /**< "SCP" */
    uint8_t  version;           /**< Version<<4 | Revision */
    uint8_t  disk_type;         /**< Manufacturer | Disk type */
    uint8_t  revolutions;       /**< Number of revolutions per track */
    uint8_t  start_track;       /**< Start track number */
    uint8_t  end_track;         /**< End track number */
    uint8_t  flags;             /**< Feature flags */
    uint8_t  bit_cell_width;    /**< 0=16 bits, else bits per cell */
    uint8_t  heads;             /**< 0=both, 1=side0, 2=side1 */
    uint8_t  resolution;        /**< Multiplier for 25ns base */
    uint32_t checksum;          /**< File data checksum */
} uft_scp_header_t;
#endif /* UFT_SCP_HEADER_T_DEFINED */

/**
 * @brief Track offset table entry
 */
typedef struct {
    uint32_t offset;            /**< Offset from file start, or 0 */
} uft_scp_track_offset_t;

/**
 * @brief Revolution info within track header
 */
#ifndef UFT_SCP_REVOLUTION_T_DEFINED
#define UFT_SCP_REVOLUTION_T_DEFINED
typedef struct {
    uint32_t index_time;        /**< Index time in 25ns units */
    uint32_t track_length;      /**< Number of flux entries */
    uint32_t data_offset;       /**< Offset from track header start */
} uft_scp_revolution_t;
#endif /* UFT_SCP_REVOLUTION_T_DEFINED */

/**
 * @brief Track data header
 */
#ifndef UFT_SCP_TRACK_HEADER_T_DEFINED
#define UFT_SCP_TRACK_HEADER_T_DEFINED
typedef struct {
    uint8_t  signature[3];      /**< "TRK" */
    uint8_t  track_number;      /**< Track number */
    /* Followed by revolution entries and flux data */
} uft_scp_track_header_t;
#endif /* UFT_SCP_TRACK_HEADER_T_DEFINED */

/**
 * @brief On-disk extension footer (52 bytes, 13 x uint32 little-endian)
 *
 * Located at end of file (before optional 4-byte checksum).
 * Present when FLAG bit 5 (0x20) is set in the header.
 * String offsets point to null-terminated strings earlier in the file.
 */
typedef struct {
    uint32_t drive_mfg_offset;      /**< Manufacturer string offset */
    uint32_t drive_model_offset;    /**< Model string offset */
    uint32_t drive_serial_offset;   /**< Serial number string offset */
    uint32_t creator_offset;        /**< Creator string offset */
    uint32_t app_name_offset;       /**< Application name string offset */
    uint32_t app_version_offset;    /**< Application version string offset */
    uint32_t comments_offset;       /**< Comments string offset */
    uint32_t creation_timestamp;    /**< Creation time (Unix epoch) */
    uint32_t modification_timestamp;/**< Modification time (Unix epoch) */
    uint32_t app_version_bcd;       /**< Application version (BCD) */
    uint32_t hw_version_bcd;        /**< Hardware version (BCD) */
    uint32_t fw_version_bcd;        /**< Firmware version (BCD) */
    uint32_t format_revision;       /**< Image format revision */
} uft_scp_footer_t;

#pragma pack(pop)

/**
 * @brief Parsed extension footer with resolved strings
 *
 * Populated by reading the on-disk footer and then resolving each
 * string offset to a null-terminated string copied into fixed buffers.
 */
typedef struct {
    char     manufacturer[64];
    char     model[64];
    char     serial[64];
    char     creator[64];
    char     application[64];
    char     app_version[32];
    char     comments[256];
    uint32_t created_timestamp;
    uint32_t modified_timestamp;
    uint32_t app_version_bcd;
    uint32_t hw_version_bcd;
    uint32_t fw_version_bcd;
    uint32_t format_revision;
    bool     present;
} scp_extension_footer_t;

/*============================================================================
 * Parser Context
 *============================================================================*/

/**
 * @brief Parsed revolution data
 */
typedef struct {
    uint32_t index_time_ns;     /**< Index time in nanoseconds */
    uint32_t flux_count;        /**< Number of flux transitions */
    uint32_t* flux_data;        /**< Flux timing data (allocated) */
    uint32_t rpm;               /**< Calculated RPM */
} uft_scp_rev_data_t;

/**
 * @brief Parsed track data
 */
typedef struct {
    uint8_t  track_number;
    uint8_t  side;
    uint8_t  revolution_count;
    bool     valid;
    uft_scp_rev_data_t revolutions[UFT_SCP_MAX_REVOLUTIONS];
} uft_scp_track_data_t;

/**
 * @brief SCP parser context
 */
typedef struct {
    /* Header info */
    uft_scp_header_t header;
    uint8_t  version_major;
    uint8_t  version_minor;
    uint8_t  manufacturer;
    uint8_t  disk_subtype;
    
    /* Track info */
    uint32_t track_offsets[UFT_SCP_MAX_TRACKS];
    int      track_count;
    
    /* Resolution */
    uint32_t period_ns;         /**< Actual period in ns */
    
    /* Footer (optional) */
    bool     has_footer;
    uft_scp_footer_t footer;
    scp_extension_footer_t ext_footer;
    char*    creator_string;
    char*    app_name;

    /* File handle */
    FILE*    file;
    size_t   file_size;
    
    /* Status */
    int      last_error;
} uft_scp_ctx_t;

/*============================================================================
 * Error Codes
 *============================================================================*/

#define UFT_SCP_OK              0
#define UFT_SCP_ERR_NULLPTR     -1
#define UFT_SCP_ERR_OPEN        -2
#define UFT_SCP_ERR_READ        -3
#define UFT_SCP_ERR_SIGNATURE   -4
#define UFT_SCP_ERR_VERSION     -5
#define UFT_SCP_ERR_CHECKSUM    -6
#define UFT_SCP_ERR_MEMORY      -7
#define UFT_SCP_ERR_TRACK       -8
#define UFT_SCP_ERR_OVERFLOW    -9

/*============================================================================
 * API Functions
 *============================================================================*/

/**
 * @brief Create SCP parser context
 * @return Context or NULL on error
 */
uft_scp_ctx_t* uft_scp_create(void);

/**
 * @brief Destroy SCP parser context
 * @param ctx Context to destroy
 */
void uft_scp_destroy(uft_scp_ctx_t* ctx);

/**
 * @brief Open and parse SCP file
 * @param ctx Parser context
 * @param filename Path to SCP file
 * @return UFT_SCP_OK on success
 */
int uft_scp_open(uft_scp_ctx_t* ctx, const char* filename);

/**
 * @brief Open from memory buffer
 * @param ctx Parser context
 * @param data Buffer containing SCP data
 * @param size Size of buffer
 * @return UFT_SCP_OK on success
 */
int uft_scp_open_memory(uft_scp_ctx_t* ctx, const uint8_t* data, size_t size);

/**
 * @brief Close SCP file
 * @param ctx Parser context
 */
void uft_scp_close(uft_scp_ctx_t* ctx);

/**
 * @brief Get track count
 * @param ctx Parser context
 * @return Number of tracks
 */
int uft_scp_get_track_count(uft_scp_ctx_t* ctx);

/**
 * @brief Check if track exists
 * @param ctx Parser context
 * @param track Track number
 * @return true if track exists
 */
bool uft_scp_has_track(uft_scp_ctx_t* ctx, int track);

/**
 * @brief Read track data
 * @param ctx Parser context
 * @param track Track number
 * @param data Output track data (caller must free flux_data)
 * @return UFT_SCP_OK on success
 */
int uft_scp_read_track(uft_scp_ctx_t* ctx, int track, uft_scp_track_data_t* data);

/**
 * @brief Free track data
 * @param data Track data to free
 */
void uft_scp_free_track(uft_scp_track_data_t* data);

/**
 * @brief Get disk type name
 * @param disk_type Disk type byte
 * @return Name string
 */
#ifndef UFT_SCP_DISK_TYPE_NAME_DECLARED
#define UFT_SCP_DISK_TYPE_NAME_DECLARED
const char* uft_scp_disk_type_name(uint8_t disk_type);
#endif /* UFT_SCP_DISK_TYPE_NAME_DECLARED */

/**
 * @brief Get manufacturer name
 * @param disk_type Disk type byte
 * @return Manufacturer name
 */
const char* uft_scp_manufacturer_name(uint8_t disk_type);

/**
 * @brief Calculate RPM from index time
 * @param index_time_ns Index time in nanoseconds
 * @return RPM value
 */
uint32_t uft_scp_calculate_rpm(uint32_t index_time_ns);

/**
 * @brief Convert flux data to nanoseconds
 * @param ctx Parser context
 * @param flux_value Raw flux value (big endian 16-bit)
 * @return Time in nanoseconds
 */
uint32_t uft_scp_flux_to_ns(uft_scp_ctx_t* ctx, uint16_t flux_value);

/**
 * @brief Verify file checksum
 * @param ctx Parser context
 * @return true if checksum valid
 */
bool uft_scp_verify_checksum(uft_scp_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SCP_PARSER_H */
