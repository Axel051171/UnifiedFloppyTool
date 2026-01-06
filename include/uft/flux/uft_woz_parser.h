/**
 * @file uft_woz_parser.h
 * @brief WOZ Format Parser (Apple II Preservation)
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WOZ is the preservation format for Apple II disk images.
 * Supports v1, v2, and v3 (flux) formats.
 *
 * Source: Applesauce WOZ specification, HxCFloppyEmulator (GPL v2)
 */

#ifndef UFT_WOZ_PARSER_H
#define UFT_WOZ_PARSER_H

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

#define UFT_WOZ_SIGNATURE       "WOZ"
#define UFT_WOZ_MAX_TRACKS      160     /* TMAP size for 5.25" (40 tracks * 4 quarter tracks) */
#define UFT_WOZ_MAX_TRACKS_35   160     /* For 3.5" (80 tracks * 2 sides) */
#define UFT_WOZ_V1_TRACK_SIZE   6646    /* Fixed track size in v1 */
#define UFT_WOZ_BLOCK_SIZE      512     /* Block size for v2+ */

/*============================================================================
 * Chunk IDs
 *============================================================================*/

#define UFT_WOZ_CHUNK_INFO      0x4F464E49  /* "INFO" */
#define UFT_WOZ_CHUNK_TMAP      0x50414D54  /* "TMAP" */
#define UFT_WOZ_CHUNK_TRKS      0x534B5254  /* "TRKS" */
#define UFT_WOZ_CHUNK_META      0x4154454D  /* "META" */
#define UFT_WOZ_CHUNK_WRIT      0x54495257  /* "WRIT" (v2+) */
#define UFT_WOZ_CHUNK_FLUX      0x58554C46  /* "FLUX" (v3) */

/*============================================================================
 * Disk Types
 *============================================================================*/

#define UFT_WOZ_DISK_525        1       /* 5.25" disk */
#define UFT_WOZ_DISK_35         2       /* 3.5" disk */

/*============================================================================
 * Compatible Hardware Flags
 *============================================================================*/

#define UFT_WOZ_HW_APPLE_II         0x0001
#define UFT_WOZ_HW_APPLE_II_PLUS    0x0002
#define UFT_WOZ_HW_APPLE_IIE        0x0004
#define UFT_WOZ_HW_APPLE_IIC        0x0008
#define UFT_WOZ_HW_APPLE_IIE_ENH    0x0010
#define UFT_WOZ_HW_APPLE_IIGS       0x0020
#define UFT_WOZ_HW_APPLE_IIC_PLUS   0x0040
#define UFT_WOZ_HW_APPLE_III        0x0080
#define UFT_WOZ_HW_APPLE_III_PLUS   0x0100

/*============================================================================
 * Boot Sector Format
 *============================================================================*/

#define UFT_WOZ_BOOT_UNKNOWN    0
#define UFT_WOZ_BOOT_16_SECTOR  1
#define UFT_WOZ_BOOT_13_SECTOR  2
#define UFT_WOZ_BOOT_BOTH       3

/*============================================================================
 * Structures
 *============================================================================*/

#pragma pack(push, 1)

/**
 * @brief WOZ file header (12 bytes)
 */
typedef struct {
    uint8_t  signature[3];      /**< "WOZ" */
    uint8_t  version;           /**< '1', '2', or '3' */
    uint8_t  high_bit;          /**< 0xFF */
    uint8_t  lfcrlf[3];         /**< 0x0A 0x0D 0x0A */
    uint32_t crc32;             /**< CRC32 of remaining data */
} uft_woz_header_t;

/**
 * @brief Chunk header (8 bytes)
 */
typedef struct {
    uint32_t id;                /**< Chunk ID */
    uint32_t size;              /**< Data size (excluding header) */
} uft_woz_chunk_t;

/**
 * @brief INFO chunk (v1/v2/v3)
 */
typedef struct {
    /* v1, v2, v3 common */
    uint8_t  version;           /**< INFO version (1, 2, or 3) */
    uint8_t  disk_type;         /**< 1=5.25", 2=3.5" */
    uint8_t  write_protected;   /**< 1=protected */
    uint8_t  synchronized;      /**< 1=cross-track sync */
    uint8_t  cleaned;           /**< 1=MC3470 fake bits removed */
    uint8_t  creator[32];       /**< Creator application */
    
    /* v2+ only */
    uint8_t  sides;             /**< Number of sides */
    uint8_t  boot_sector_fmt;   /**< Boot sector format */
    uint8_t  optimal_bit_timing;/**< 125ns increments */
    uint16_t compatible_hw;     /**< Compatible hardware bitmask */
    uint16_t required_ram;      /**< Required RAM in KB */
    uint16_t largest_track;     /**< Largest track in blocks */
    
    /* v3 only */
    uint16_t flux_block;        /**< Starting block for flux data */
    uint16_t largest_flux_track;/**< Largest flux track in blocks */
} uft_woz_info_t;

/**
 * @brief Track entry for v2+ (8 bytes per track)
 */
typedef struct {
    uint16_t starting_block;    /**< Starting 512-byte block */
    uint16_t block_count;       /**< Number of blocks */
    uint32_t bit_count;         /**< Number of valid bits */
} uft_woz_trk_v2_t;

/**
 * @brief Track entry for v1 (6656 bytes per track)
 */
typedef struct {
    uint8_t  bitstream[6646];   /**< Track data */
    uint16_t bytes_used;        /**< Bytes used in bitstream */
    uint16_t bit_count;         /**< Number of valid bits */
    uint16_t splice_point;      /**< Splice position (0xFFFF if none) */
    uint8_t  splice_nibble;     /**< Splice nibble value */
    uint8_t  splice_bit_count;  /**< Splice bit count */
    uint16_t reserved;
} uft_woz_trk_v1_t;

#pragma pack(pop)

/*============================================================================
 * Parsed Data Structures
 *============================================================================*/

/**
 * @brief Parsed track data
 */
typedef struct {
    uint8_t  track_index;       /**< Track index in TMAP (0-159) */
    uint8_t  quarter_track;     /**< Quarter track number (5.25") */
    uint8_t  physical_track;    /**< Physical track number */
    uint8_t  side;              /**< Side (0 or 1 for 3.5") */
    
    uint32_t bit_count;         /**< Number of valid bits */
    uint32_t byte_count;        /**< Number of bytes */
    uint8_t* bitstream;         /**< Bitstream data (allocated) */
    
    /* v1 specific */
    uint16_t splice_point;      /**< Splice position */
    uint8_t  splice_nibble;     /**< Splice nibble value */
    
    /* v3 flux data */
    bool     has_flux;          /**< Has flux data (v3) */
    uint32_t flux_count;        /**< Number of flux entries */
    uint32_t* flux_data;        /**< Flux timing data (allocated) */
    
    bool     valid;
} uft_woz_track_t;

/**
 * @brief Metadata key-value pair
 */
typedef struct {
    char key[64];
    char value[256];
} uft_woz_meta_t;

/**
 * @brief WOZ parser context
 */
typedef struct {
    /* Header */
    uft_woz_header_t header;
    uint8_t  woz_version;       /**< 1, 2, or 3 */
    
    /* INFO chunk */
    uft_woz_info_t info;
    bool     has_info;
    
    /* TMAP (track map) */
    uint8_t  tmap[UFT_WOZ_MAX_TRACKS];
    bool     has_tmap;
    int      tmap_size;
    
    /* Track data */
    int      track_count;       /**< Number of unique tracks */
    int      max_track;         /**< Highest track number */
    
    /* Metadata */
    uft_woz_meta_t* metadata;
    int      meta_count;
    
    /* File data */
    uint8_t* file_data;         /**< Complete file in memory */
    size_t   file_size;
    FILE*    file;
    
    /* Chunk offsets (for direct access) */
    int      info_offset;
    int      tmap_offset;
    int      trks_offset;
    int      meta_offset;
    int      flux_offset;
    
    /* Status */
    int      last_error;
    bool     crc_valid;
} uft_woz_ctx_t;

/*============================================================================
 * Error Codes
 *============================================================================*/

#define UFT_WOZ_OK              0
#define UFT_WOZ_ERR_NULLPTR     -1
#define UFT_WOZ_ERR_OPEN        -2
#define UFT_WOZ_ERR_READ        -3
#define UFT_WOZ_ERR_SIGNATURE   -4
#define UFT_WOZ_ERR_VERSION     -5
#define UFT_WOZ_ERR_CRC         -6
#define UFT_WOZ_ERR_MEMORY      -7
#define UFT_WOZ_ERR_CHUNK       -8
#define UFT_WOZ_ERR_TRACK       -9
#define UFT_WOZ_ERR_FORMAT      -10

/*============================================================================
 * API Functions
 *============================================================================*/

/**
 * @brief Create WOZ parser context
 * @return Context or NULL
 */
uft_woz_ctx_t* uft_woz_create(void);

/**
 * @brief Destroy parser context
 * @param ctx Context
 */
void uft_woz_destroy(uft_woz_ctx_t* ctx);

/**
 * @brief Open WOZ file
 * @param ctx Context
 * @param filename Path to WOZ file
 * @return UFT_WOZ_OK on success
 */
int uft_woz_open(uft_woz_ctx_t* ctx, const char* filename);

/**
 * @brief Open from memory
 * @param ctx Context
 * @param data WOZ file data
 * @param size Data size
 * @return UFT_WOZ_OK on success
 */
int uft_woz_open_memory(uft_woz_ctx_t* ctx, const uint8_t* data, size_t size);

/**
 * @brief Close WOZ file
 * @param ctx Context
 */
void uft_woz_close(uft_woz_ctx_t* ctx);

/**
 * @brief Get track count
 * @param ctx Context
 * @return Number of tracks
 */
int uft_woz_get_track_count(uft_woz_ctx_t* ctx);

/**
 * @brief Check if track exists
 * @param ctx Context
 * @param quarter_track Quarter track number (0-159)
 * @return true if track exists
 */
bool uft_woz_has_track(uft_woz_ctx_t* ctx, int quarter_track);

/**
 * @brief Read track data
 * @param ctx Context
 * @param quarter_track Quarter track number (0-159)
 * @param track Output track data
 * @return UFT_WOZ_OK on success
 */
int uft_woz_read_track(uft_woz_ctx_t* ctx, int quarter_track, uft_woz_track_t* track);

/**
 * @brief Read flux data (v3 only)
 * @param ctx Context
 * @param quarter_track Quarter track number
 * @param track Output track data with flux
 * @return UFT_WOZ_OK on success
 */
int uft_woz_read_flux(uft_woz_ctx_t* ctx, int quarter_track, uft_woz_track_t* track);

/**
 * @brief Free track data
 * @param track Track to free
 */
void uft_woz_free_track(uft_woz_track_t* track);

/**
 * @brief Get metadata value
 * @param ctx Context
 * @param key Metadata key
 * @return Value or NULL
 */
const char* uft_woz_get_metadata(uft_woz_ctx_t* ctx, const char* key);

/**
 * @brief Get disk type name
 * @param disk_type Disk type code
 * @return Name string
 */
const char* uft_woz_disk_type_name(uint8_t disk_type);

/**
 * @brief Get compatible hardware names
 * @param flags Hardware flags
 * @param buffer Output buffer
 * @param size Buffer size
 */
void uft_woz_hw_names(uint16_t flags, char* buffer, size_t size);

/**
 * @brief Calculate bit timing in nanoseconds
 * @param bit_timing Optimal bit timing value
 * @return Timing in nanoseconds
 */
uint32_t uft_woz_bit_timing_ns(uint8_t bit_timing);

/**
 * @brief Verify CRC32
 * @param ctx Context
 * @return true if CRC valid
 */
bool uft_woz_verify_crc(uft_woz_ctx_t* ctx);

/**
 * @brief Decode nibbles from bitstream
 * @param bitstream Bitstream data
 * @param bit_count Number of bits
 * @param nibbles Output nibble buffer
 * @param max_nibbles Maximum nibbles
 * @return Number of nibbles decoded
 */
int uft_woz_decode_nibbles(const uint8_t* bitstream, uint32_t bit_count,
                           uint8_t* nibbles, int max_nibbles);

#ifdef __cplusplus
}
#endif

#endif /* UFT_WOZ_PARSER_H */
