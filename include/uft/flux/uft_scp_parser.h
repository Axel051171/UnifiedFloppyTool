/**
 * @file uft_scp_parser.h
 * @brief SuperCard Pro (SCP) Format Parser
 * @version 1.0.0
 * @date 2026-01-06
 *
 * Source: SCP format specification v2.0 by Jim Drew
 * HxCFloppyEmulator (Jean-François DEL NERO) - GPL v2
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

#define UFT_SCP_SIGNATURE       "SCP"
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

/* CBM disk types */
#define UFT_SCP_DISK_C64        0x00
#define UFT_SCP_DISK_AMIGA      0x04

/* Atari disk types */
#define UFT_SCP_DISK_ATARI_FM_SS    0x00
#define UFT_SCP_DISK_ATARI_FM_DS    0x01
#define UFT_SCP_DISK_ATARI_ST_SS    0x04
#define UFT_SCP_DISK_ATARI_ST_DS    0x05

/* Apple disk types */
#define UFT_SCP_DISK_APPLE_II       0x00
#define UFT_SCP_DISK_APPLE_II_PRO   0x01
#define UFT_SCP_DISK_APPLE_400K     0x04
#define UFT_SCP_DISK_APPLE_800K     0x05
#define UFT_SCP_DISK_APPLE_1440K    0x06

/* PC disk types */
#define UFT_SCP_DISK_PC_360K    0x00
#define UFT_SCP_DISK_PC_720K    0x01
#define UFT_SCP_DISK_PC_1200K   0x02
#define UFT_SCP_DISK_PC_1440K   0x03

/*============================================================================
 * Flags
 *============================================================================*/

#define UFT_SCP_FLAG_INDEX      0x01  /* Used index mark */
#define UFT_SCP_FLAG_96TPI      0x02  /* 96 TPI drive */
#define UFT_SCP_FLAG_360RPM     0x04  /* 360 RPM (vs 300) */
#define UFT_SCP_FLAG_NORMALIZED 0x08  /* Quality reduced */
#define UFT_SCP_FLAG_RW         0x10  /* Read/Write capable */
#define UFT_SCP_FLAG_FOOTER     0x20  /* Has extension footer */
#define UFT_SCP_FLAG_EXTENDED   0x40  /* Extended mode */
#define UFT_SCP_FLAG_CREATOR    0x80  /* Creator info */

/*============================================================================
 * Structures
 *============================================================================*/

#pragma pack(push, 1)

/**
 * @brief SCP file header (16 bytes)
 */
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

/**
 * @brief Track offset table entry
 */
typedef struct {
    uint32_t offset;            /**< Offset from file start, or 0 */
} uft_scp_track_offset_t;

/**
 * @brief Revolution info within track header
 */
typedef struct {
    uint32_t index_time;        /**< Index time in 25ns units */
    uint32_t track_length;      /**< Number of flux entries */
    uint32_t data_offset;       /**< Offset from track header start */
} uft_scp_revolution_t;

/**
 * @brief Track data header
 */
typedef struct {
    uint8_t  signature[3];      /**< "TRK" */
    uint8_t  track_number;      /**< Track number */
    /* Followed by revolution entries and flux data */
} uft_scp_track_header_t;

/**
 * @brief Extension footer
 */
typedef struct {
    uint32_t drive_mfg_offset;
    uint32_t drive_model_offset;
    uint32_t drive_serial_offset;
    uint32_t creator_offset;
    uint32_t app_name_offset;
    uint32_t comments_offset;
    uint64_t creation_timestamp;
    uint64_t modification_timestamp;
    uint8_t  app_version;
    uint8_t  scp_hw_version;
    uint8_t  scp_fw_version;
    uint8_t  format_revision;
    uint8_t  footer_sig[4];     /**< "FPCS" */
} uft_scp_footer_t;

#pragma pack(pop)

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
const char* uft_scp_disk_type_name(uint8_t disk_type);

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
