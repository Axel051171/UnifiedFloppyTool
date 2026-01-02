/**
 * @file uft_applesauce.h
 * @brief Applesauce Disk Image Formats (WOZ, MOOF, A2R)
 * @version 3.1.4.003
 *
 * Complete support for Applesauce preservation formats:
 *
 * **WOZ** - Apple II 5.25" disk images (bitstream)
 *   - WOZ 1.0: Fixed 6656-byte track records
 *   - WOZ 2.0: Variable-length tracks with flux support
 *   - Quarter-track resolution (160 entries in TMAP)
 *   - 4µs normalized bitcells
 *
 * **MOOF** - Macintosh 3.5" disk images (GCR/MFM)
 *   - 400K/800K GCR (Macintosh)
 *   - 1.44MB MFM (PC-compatible)
 *   - Twiggy format support
 *   - Optional flux streams (125ns resolution)
 *
 * **A2R** - Raw flux capture format
 *   - A2R 2.x/3.x versions
 *   - Picosecond-resolution timing
 *   - Multiple capture passes per track
 *   - Index hole timing
 *   - "Solved" (cleaned) track streams
 *
 * References:
 *   - https://applesaucefdc.com/woz/reference1/
 *   - https://applesaucefdc.com/woz/reference2/
 *   - https://applesaucefdc.com/moof-reference/
 *   - https://applesaucefdc.com/a2r/
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_APPLESAUCE_H
#define UFT_APPLESAUCE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Common Constants
 *============================================================================*/

/** Applesauce file signatures */
#define UFT_WOZ1_MAGIC      "WOZ1"
#define UFT_WOZ2_MAGIC      "WOZ2"
#define UFT_MOOF_MAGIC      "MOOF"
#define UFT_A2R2_MAGIC      "A2R2"
#define UFT_A2R3_MAGIC      "A2R3"

/** Header suffix bytes (all formats) */
#define UFT_APPLESAUCE_SUFFIX   { 0xFF, 0x0A, 0x0D, 0x0A }

/** CRC32 polynomial (standard) */
#define UFT_APPLESAUCE_CRC_POLY 0xEDB88320u

/** Block size for MOOF */
#define UFT_MOOF_BLOCK_SIZE     512

/** Track map size */
#define UFT_TMAP_SIZE           160

/** WOZ1 track record size */
#define UFT_WOZ1_TRK_SIZE       6656

/** WOZ1 bitstream size within track */
#define UFT_WOZ1_BITS_SIZE      6646

/*============================================================================
 * Chunk IDs (FourCC, little-endian)
 *============================================================================*/

#define UFT_CHUNK_INFO      0x4F464E49  /* "INFO" */
#define UFT_CHUNK_TMAP      0x50414D54  /* "TMAP" */
#define UFT_CHUNK_TRKS      0x534B5254  /* "TRKS" */
#define UFT_CHUNK_FLUX      0x58554C46  /* "FLUX" */
#define UFT_CHUNK_META      0x4154454D  /* "META" */
#define UFT_CHUNK_RWCP      0x50435752  /* "RWCP" - A2R raw captures */
#define UFT_CHUNK_SLVD      0x44564C53  /* "SLVD" - A2R solved tracks */

/*============================================================================
 * Disk Types
 *============================================================================*/

/** WOZ disk types */
typedef enum {
    UFT_WOZ_DISK_525    = 1,    /**< 5.25" floppy (Apple II) */
    UFT_WOZ_DISK_35     = 2,    /**< 3.5" floppy (Apple IIgs) */
} uft_woz_disk_type_t;

/** MOOF disk types */
typedef enum {
    UFT_MOOF_DISK_SSDD_GCR_400K   = 1,  /**< Single-sided DD GCR 400K */
    UFT_MOOF_DISK_DSDD_GCR_800K   = 2,  /**< Double-sided DD GCR 800K */
    UFT_MOOF_DISK_DSHD_MFM_1440K  = 3,  /**< Double-sided HD MFM 1.44M */
    UFT_MOOF_DISK_TWIGGY          = 4,  /**< Twiggy drive format */
} uft_moof_disk_type_t;

/** A2R drive types */
typedef enum {
    UFT_A2R_DRIVE_525_SS      = 1,  /**< 5.25" single-sided */
    UFT_A2R_DRIVE_35_SS       = 2,  /**< 3.5" single-sided */
    UFT_A2R_DRIVE_35_DS       = 3,  /**< 3.5" double-sided */
    UFT_A2R_DRIVE_525_DS      = 4,  /**< 5.25" double-sided */
    UFT_A2R_DRIVE_35_HD       = 5,  /**< 3.5" HD */
    UFT_A2R_DRIVE_8_SS        = 6,  /**< 8" single-sided */
    UFT_A2R_DRIVE_8_DS        = 7,  /**< 8" double-sided */
} uft_a2r_drive_type_t;

/*============================================================================
 * A2R Capture Types
 *============================================================================*/

typedef enum {
    UFT_A2R_CAP_TIMING   = 1,    /**< Standard timing capture */
    UFT_A2R_CAP_BITS     = 2,    /**< Deprecated bits capture */
    UFT_A2R_CAP_XTIMING  = 3,    /**< Extended timing capture */
} uft_a2r_capture_type_t;

/*============================================================================
 * WOZ Structures
 *============================================================================*/

/** WOZ INFO chunk data */
typedef struct {
    uint8_t     version;        /**< INFO version (1 or 2) */
    uint8_t     disk_type;      /**< 1=5.25", 2=3.5" */
    uint8_t     write_protected;/**< 1 if write-protected */
    uint8_t     synchronized;   /**< 1 if cross-track sync used */
    uint8_t     cleaned;        /**< 1 if fake bits removed */
    char        creator[33];    /**< Creator string (trimmed) */
    
    /* WOZ 2.0 additional fields */
    uint8_t     disk_sides;     /**< 1 or 2 sides */
    uint8_t     boot_sector_format; /**< 0=unknown, 1=16-sector, 2=13-sector, 3=both */
    uint8_t     optimal_bit_timing; /**< 125ns units (default 32 = 4µs) */
    uint16_t    compatible_hardware; /**< Bit flags for Apple II models */
    uint16_t    required_ram;   /**< Required RAM in KB */
    uint16_t    largest_track;  /**< Largest track in blocks */
} uft_woz_info_t;

/** WOZ1 track record (6656 bytes) */
typedef struct {
    uint8_t     bitstream[UFT_WOZ1_BITS_SIZE]; /**< Packed bits (MSB first) */
    uint16_t    bytes_used;     /**< Actual bytes in bitstream */
    uint16_t    bit_count;      /**< Number of valid bits */
    uint16_t    splice_point;   /**< Bit index of splice */
    uint8_t     splice_nibble;  /**< Nibble value at splice */
    uint8_t     splice_bit_count; /**< Bits in splice nibble */
    uint16_t    reserved;
} uft_woz1_track_t;

/** WOZ2 track descriptor (8 bytes in TRKS array) */
typedef struct {
    uint16_t    start_block;    /**< Starting 512-byte block */
    uint16_t    block_count;    /**< Number of blocks */
    uint32_t    bit_count;      /**< Number of valid bits */
} uft_woz2_track_desc_t;

/** WOZ image handle */
typedef struct {
    int         version;        /**< 1 or 2 */
    uft_woz_info_t info;
    uint8_t     tmap[UFT_TMAP_SIZE]; /**< Quarter-track map */
    
    /* WOZ1: tracks array */
    uft_woz1_track_t *woz1_tracks;
    uint32_t    woz1_track_count;
    
    /* WOZ2: track descriptors + data blocks */
    uft_woz2_track_desc_t *woz2_descs;
    uint8_t    *woz2_data;      /**< Raw track data blocks */
    size_t      woz2_data_size;
    
    /* META chunk */
    char       *meta;           /**< Tab-delimited metadata */
    size_t      meta_len;
    
    /* CRC status */
    bool        crc_valid;
    uint32_t    crc_expected;
    uint32_t    crc_calculated;
} uft_woz_image_t;

/*============================================================================
 * MOOF Structures
 *============================================================================*/

/** MOOF INFO chunk */
typedef struct {
    uint8_t     version;
    uint8_t     disk_type;
    uint8_t     write_protected;
    uint8_t     synchronized;
    uint8_t     optimal_bit_timing_125ns; /**< Bit timing in 125ns units */
    char        creator[33];
    uint16_t    largest_track_blocks;
    uint16_t    flux_block;     /**< Block of FLUX chunk, 0 if none */
    uint16_t    largest_flux_track_blocks;
} uft_moof_info_t;

/** MOOF track descriptor */
typedef struct {
    uint16_t    start_block;    /**< Starting block (×512 for offset) */
    uint16_t    block_count;    /**< Number of blocks */
    uint32_t    bit_count;      /**< For BITS: bit count. For FLUX: byte count */
} uft_moof_track_desc_t;

/** MOOF track payload type */
typedef enum {
    UFT_MOOF_PAYLOAD_BITS = 1,
    UFT_MOOF_PAYLOAD_FLUX = 2,
} uft_moof_payload_type_t;

/** MOOF image handle */
typedef struct {
    uft_moof_info_t info;
    uint8_t     tmap[UFT_TMAP_SIZE];
    uint8_t     fluxmap[UFT_TMAP_SIZE];
    bool        has_fluxmap;
    
    uft_moof_track_desc_t tracks[UFT_TMAP_SIZE];
    
    /* File buffer for payload access */
    uint8_t    *file_data;
    size_t      file_size;
    
    bool        uses_flux;      /**< True if FLUX payload mode */
    
    char       *meta;
    size_t      meta_len;
    
    bool        crc_valid;
} uft_moof_image_t;

/*============================================================================
 * A2R Structures
 *============================================================================*/

/** A2R capture entry */
typedef struct {
    uint32_t    location;       /**< Track location (quarter-tracks) */
    uint8_t     capture_type;   /**< Timing, bits, or xtiming */
    uint32_t    resolution_ps;  /**< Picoseconds per tick */
    
    /* Index holes */
    uint8_t     index_count;
    uint32_t   *index_ticks;    /**< Absolute tick times of index holes */
    
    /* Packed flux data (255-run encoded) */
    uint8_t    *packed;
    uint32_t    packed_len;
    
    /* Decoded delta ticks */
    uint32_t   *deltas;
    uint32_t    deltas_count;
} uft_a2r_capture_t;

/** A2R solved track entry */
typedef struct {
    uint32_t    location;       /**< Track location */
    uint32_t    resolution_ps;
    uint8_t     mirror_out;     /**< Track to read before */
    uint8_t     mirror_in;      /**< Track to read after */
    
    uint8_t     index_count;
    uint32_t   *index_ticks;
    
    uint8_t    *packed;
    uint32_t    packed_len;
    
    uint32_t   *deltas;
    uint32_t    deltas_count;
} uft_a2r_solved_t;

/** A2R image handle */
typedef struct {
    int         version;        /**< 2 or 3 */
    
    /* INFO */
    uint8_t     info_version;
    char        creator[64];
    uint8_t     drive_type;
    uint8_t     write_protected;
    uint8_t     synchronized;
    uint8_t     hard_sector_count;
    
    /* Raw captures (RWCP) */
    uft_a2r_capture_t *captures;
    size_t      captures_count;
    
    /* Solved tracks (SLVD) */
    uft_a2r_solved_t *solved;
    size_t      solved_count;
    
    /* META */
    char       *meta;
    size_t      meta_len;
} uft_a2r_image_t;

/*============================================================================
 * CRC32 Function
 *============================================================================*/

/**
 * @brief Calculate Applesauce CRC32
 * 
 * Standard CRC32 with polynomial 0xEDB88320, initial value 0.
 * Used for file integrity in WOZ, MOOF, and A2R formats.
 *
 * @param data Data buffer
 * @param len Data length
 * @param crc_init Initial CRC value (typically 0)
 * @return CRC32 value
 */
uint32_t uft_applesauce_crc32(const uint8_t *data, size_t len, uint32_t crc_init);

/*============================================================================
 * 255-Run Encoding (A2R/MOOF Flux)
 *============================================================================*/

/**
 * @brief Decode 255-run encoded flux deltas
 *
 * Flux timing is encoded as byte values where 255 indicates continuation.
 * Example: {255, 255, 10} decodes to single delta of 520 ticks.
 *
 * @param packed Packed byte stream
 * @param packed_len Length of packed data
 * @param deltas Output: decoded delta values
 * @param deltas_count Output: number of deltas
 * @return true on success
 */
bool uft_decode_255_run(const uint8_t *packed, uint32_t packed_len,
                        uint32_t **deltas, uint32_t *deltas_count);

/**
 * @brief Encode delta values to 255-run format
 *
 * @param deltas Delta values
 * @param deltas_count Number of deltas
 * @param packed Output: packed bytes
 * @param packed_len Output: packed length
 * @return true on success
 */
bool uft_encode_255_run(const uint32_t *deltas, uint32_t deltas_count,
                        uint8_t **packed, uint32_t *packed_len);

/*============================================================================
 * WOZ API
 *============================================================================*/

/**
 * @brief Open WOZ disk image
 * @param path File path
 * @return Image handle or NULL on error
 */
uft_woz_image_t *uft_woz_open(const char *path);

/**
 * @brief Open WOZ from memory buffer
 * @param data File data
 * @param len Data length
 * @return Image handle or NULL
 */
uft_woz_image_t *uft_woz_open_mem(const uint8_t *data, size_t len);

/**
 * @brief Close WOZ image
 * @param woz Image handle
 */
void uft_woz_close(uft_woz_image_t *woz);

/**
 * @brief Get track bitstream for quarter-track
 *
 * @param woz Image handle
 * @param qtrack Quarter-track (0-159)
 * @param bits Output: bitstream data
 * @param bit_count Output: number of bits
 * @return true if track exists
 */
bool uft_woz_get_track(const uft_woz_image_t *woz, uint8_t qtrack,
                       const uint8_t **bits, uint32_t *bit_count);

/**
 * @brief Get WOZ metadata value
 * @param woz Image handle
 * @param key Metadata key
 * @param value Output buffer
 * @param value_len Buffer size
 * @return true if found
 */
bool uft_woz_get_meta(const uft_woz_image_t *woz, const char *key,
                      char *value, size_t value_len);

/*============================================================================
 * MOOF API
 *============================================================================*/

/**
 * @brief Open MOOF disk image
 * @param path File path
 * @return Image handle or NULL
 */
uft_moof_image_t *uft_moof_open(const char *path);

/**
 * @brief Close MOOF image
 * @param moof Image handle
 */
void uft_moof_close(uft_moof_image_t *moof);

/**
 * @brief Map physical track/side to TRKS index
 * @param moof Image handle
 * @param track Track (0-79)
 * @param side Side (0-1)
 * @param trk_index Output: TRKS index
 * @return true if track exists (not 0xFF)
 */
bool uft_moof_map_track(const uft_moof_image_t *moof,
                        uint8_t track, uint8_t side,
                        uint8_t *trk_index);

/**
 * @brief Get track payload
 * @param moof Image handle
 * @param trk_index TRKS index
 * @param data Output: data pointer
 * @param data_len Output: allocated bytes
 * @param valid_count Output: valid bits (BITS) or bytes (FLUX)
 * @param payload_type Output: BITS or FLUX
 * @return true on success
 */
bool uft_moof_get_track(const uft_moof_image_t *moof, uint8_t trk_index,
                        const uint8_t **data, size_t *data_len,
                        uint32_t *valid_count,
                        uft_moof_payload_type_t *payload_type);

/*============================================================================
 * A2R API
 *============================================================================*/

/**
 * @brief Open A2R flux capture file
 * @param path File path
 * @return Image handle or NULL
 */
uft_a2r_image_t *uft_a2r_open(const char *path);

/**
 * @brief Close A2R image
 * @param a2r Image handle
 */
void uft_a2r_close(uft_a2r_image_t *a2r);

/**
 * @brief Get raw captures for track location
 * @param a2r Image handle
 * @param location Quarter-track location
 * @param captures Output: array of captures
 * @param count Output: number of captures
 * @return true if any captures found
 */
bool uft_a2r_get_captures(const uft_a2r_image_t *a2r, uint32_t location,
                          const uft_a2r_capture_t ***captures,
                          size_t *count);

/**
 * @brief Get solved track for location
 * @param a2r Image handle
 * @param location Quarter-track location
 * @return Solved track or NULL
 */
const uft_a2r_solved_t *uft_a2r_get_solved(const uft_a2r_image_t *a2r,
                                           uint32_t location);

/**
 * @brief Convert A2R deltas to nanoseconds
 * @param deltas Delta ticks
 * @param count Number of deltas
 * @param resolution_ps Resolution in picoseconds
 * @param ns_out Output: nanosecond values
 */
void uft_a2r_deltas_to_ns(const uint32_t *deltas, uint32_t count,
                          uint32_t resolution_ps, double *ns_out);

/*============================================================================
 * Conversion Functions
 *============================================================================*/

/**
 * @brief Convert WOZ to raw sector image (DSK/DO/PO)
 * @param woz WOZ image
 * @param path Output path
 * @param format Output format (0=DSK, 1=DO, 2=PO)
 * @return 0 on success
 */
int uft_woz_to_sector(const uft_woz_image_t *woz, const char *path, int format);

/**
 * @brief Convert MOOF to raw sector image
 * @param moof MOOF image
 * @param path Output path
 * @return 0 on success
 */
int uft_moof_to_raw(const uft_moof_image_t *moof, const char *path);

/**
 * @brief Convert A2R to WOZ (bitstream synthesis)
 * @param a2r A2R image
 * @param path Output WOZ path
 * @param use_solved Use solved tracks if available
 * @return 0 on success
 */
int uft_a2r_to_woz(const uft_a2r_image_t *a2r, const char *path, bool use_solved);

/*============================================================================
 * Bit Timing Constants
 *============================================================================*/

/** WOZ default bit timing (4µs = 32 × 125ns) */
#define UFT_WOZ_DEFAULT_BIT_TIMING  32

/** MOOF GCR typical bit timing (2µs = 16 × 125ns) */
#define UFT_MOOF_GCR_BIT_TIMING     16

/** MOOF MFM typical bit timing (1µs = 8 × 125ns) */
#define UFT_MOOF_MFM_BIT_TIMING     8

/** A2R3 default resolution (125ns in picoseconds) */
#define UFT_A2R_125NS_PS            125000

#ifdef __cplusplus
}
#endif

#endif /* UFT_APPLESAUCE_H */
