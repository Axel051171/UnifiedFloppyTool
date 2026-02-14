/**
 * @file uft_woz.h
 * @brief WOZ Disk Image Format Support (Apple II)
 * @version 4.1.5
 * 
 * WOZ is a bit-accurate disk image format for Apple II created by
 * John K. Morris for the Applesauce project. It can capture copy-protected
 * software including Spiradisc.
 * 
 * Supports:
 * - WOZ 1.0 (2018)
 * - WOZ 2.0 (2018) 
 * - WOZ 2.1 (2021) - with flux stream support
 * 
 * Features:
 * - 5.25" and 3.5" disk support
 * - Quarter-track mapping (TMAP)
 * - Bit-level track data (TRKS)
 * - Flux timing data (FLUX) - 125ns resolution
 * - Metadata parsing (META)
 * - Write hints (WRIT)
 * - Cross-track synchronization support
 * - Fake/weak bit handling
 * 
 * References:
 * - https://applesaucefdc.com/woz/reference2/
 * - https://applesaucefdc.com/woz/reference1/
 */

#ifndef UFT_WOZ_H
#define UFT_WOZ_H

#pragma pack(push, 1)

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * WOZ Format Constants
 * ============================================================================ */

/* File signatures */
#define WOZ_SIGNATURE_V1        0x315A4F57  /* 'WOZ1' */
#define WOZ_SIGNATURE_V2        0x325A4F57  /* 'WOZ2' */
#define WOZ_HIGH_BIT_CHECK      0xFF
#define WOZ_LF_CR_LF            0x0A0D0A    /* LF CR LF */

/* Chunk IDs (little-endian) */
#define WOZ_CHUNK_INFO          0x4F464E49  /* 'INFO' */
#define WOZ_CHUNK_TMAP          0x50414D54  /* 'TMAP' */
#define WOZ_CHUNK_TRKS          0x534B5254  /* 'TRKS' */
#define WOZ_CHUNK_WRIT          0x54495257  /* 'WRIT' */
#define WOZ_CHUNK_META          0x4154454D  /* 'META' */
#define WOZ_CHUNK_FLUX          0x58554C46  /* 'FLUX' - v2.1 */

/* Disk types */
#define WOZ_DISK_525            1           /* 5.25" floppy */
#define WOZ_DISK_35             2           /* 3.5" floppy */

/* Boot sector formats (5.25" only) */
#define WOZ_BOOT_UNKNOWN        0
#define WOZ_BOOT_16_SECTOR      1           /* DOS 3.3 / ProDOS */
#define WOZ_BOOT_13_SECTOR      2           /* DOS 3.2 */
#define WOZ_BOOT_BOTH           3           /* Both formats */

/* Compatible hardware bit flags */
#define WOZ_HW_APPLE_II         0x0001
#define WOZ_HW_APPLE_II_PLUS    0x0002
#define WOZ_HW_APPLE_IIE        0x0004      /* Unenhanced */
#define WOZ_HW_APPLE_IIC        0x0008
#define WOZ_HW_APPLE_IIE_ENH    0x0010      /* Enhanced */
#define WOZ_HW_APPLE_IIGS       0x0020
#define WOZ_HW_APPLE_IIC_PLUS   0x0040
#define WOZ_HW_APPLE_III        0x0080
#define WOZ_HW_APPLE_III_PLUS   0x0100

/* Track mapping */
#define WOZ_TMAP_SIZE           160         /* 160 quarter-tracks or 80 tracks x 2 sides */
#define WOZ_TMAP_EMPTY          0xFF        /* Empty track marker */
#define WOZ_MAX_TRACKS          160

/* Timing */
#define WOZ_TIMING_525_DEFAULT  32          /* 4µs = 32 * 125ns */
#define WOZ_TIMING_35_DEFAULT   16          /* 2µs = 16 * 125ns */
#define WOZ_TICK_NS             125         /* 125 nanoseconds per tick */

/* Track data */
#define WOZ_BLOCK_SIZE          512
#define WOZ_BITS_PER_TRACK_NOM  51200       /* Nominal bits per track */
#define WOZ_BYTES_PER_TRACK_MAX 6680        /* Max for Datasoft protection */

/* File offsets (for direct access) */
#define WOZ_OFFSET_HEADER       0
#define WOZ_OFFSET_INFO         20
#define WOZ_OFFSET_TMAP         88
#define WOZ_OFFSET_TRKS         256
#define WOZ_OFFSET_TRACK_DATA   1536

/* ============================================================================
 * WOZ Data Structures
 * ============================================================================ */

/**
 * @brief WOZ file header (12 bytes)
 */
typedef struct {
    uint32_t signature;         /* 'WOZ1' or 'WOZ2' */
    uint8_t  high_bit;          /* 0xFF - verify high bits work */
    uint8_t  lf_cr_lf[3];       /* 0x0A 0x0D 0x0A - detect conversion */
    uint32_t crc32;             /* CRC32 of all data after header */
} woz_header_t;

/**
 * @brief WOZ chunk header (8 bytes)
 */
typedef struct {
    uint32_t chunk_id;          /* 4-char ASCII identifier */
    uint32_t chunk_size;        /* Size of chunk data in bytes */
} woz_chunk_header_t;

/**
 * @brief WOZ INFO chunk (60 bytes)
 */
typedef struct {
    uint8_t  version;           /* INFO chunk version (1, 2, or 3) */
    uint8_t  disk_type;         /* 1 = 5.25", 2 = 3.5" */
    uint8_t  write_protected;   /* 1 = write protected */
    uint8_t  synchronized;      /* 1 = cross-track sync used */
    uint8_t  cleaned;           /* 1 = MC3470 fake bits removed */
    char     creator[32];       /* Creator software (UTF-8, space-padded) */
    
    /* Version 2+ fields */
    uint8_t  disk_sides;        /* Number of sides (1 or 2) */
    uint8_t  boot_sector_fmt;   /* Boot sector format */
    uint8_t  optimal_bit_timing;/* Bit timing in 125ns units */
    uint16_t compatible_hw;     /* Compatible hardware flags */
    uint16_t required_ram;      /* Required RAM in KB */
    uint16_t largest_track;     /* Largest track in blocks */
    
    /* Version 3+ fields (WOZ 2.1) */
    uint16_t flux_block;        /* Block where FLUX chunk starts */
    uint16_t largest_flux_track;/* Largest flux track in blocks */
    
    uint8_t  reserved[10];      /* Padding to 60 bytes */
} woz_info_t;

/**
 * @brief WOZ track entry (TRK) - 8 bytes each
 */
typedef struct {
    uint16_t starting_block;    /* First block of BITS data (relative to file) */
    uint16_t block_count;       /* Number of 512-byte blocks */
    uint32_t bit_count;         /* Number of bits (or bytes for flux) */
} woz_trk_t;

/**
 * @brief WOZ write command (WCMD) - 12 bytes
 */
typedef struct {
    uint32_t start_bit;         /* Index of starting bit */
    uint32_t bit_count;         /* Number of bits to write */
    uint8_t  leader_nibble;     /* Leader nibble value (0xFF typical) */
    uint8_t  leader_bit_count;  /* Bits per leader nibble (10 typical) */
    uint8_t  leader_count;      /* Number of leader nibbles */
    uint8_t  reserved;          /* Must be zero */
} woz_wcmd_t;

/**
 * @brief WOZ track write entry (WTRK)
 */
typedef struct {
    uint8_t  track_number;      /* Track number (quarter-tracks for 5.25") */
    uint8_t  command_count;     /* Number of write commands */
    uint8_t  write_flags;       /* Bit 0: wipe track before writing */
    uint8_t  reserved;          /* Must be zero */
    uint32_t bits_checksum;     /* CRC of BITS data */
    /* Followed by command_count WCMD structures */
} woz_wtrk_t;

/* ============================================================================
 * Metadata Keys
 * ============================================================================ */

/**
 * @brief Parsed WOZ metadata
 */
typedef struct {
    char title[128];            /* Product title */
    char subtitle[128];         /* Product subtitle */
    char publisher[128];        /* Publisher name */
    char developer[256];        /* Developer(s), pipe-separated */
    char copyright[64];         /* Copyright info */
    char version[32];           /* Software version */
    char language[32];          /* Language */
    char requires_ram[16];      /* RAM requirement */
    char requires_machine[64];  /* Compatible machines, pipe-separated */
    char notes[512];            /* Additional notes */
    char side[32];              /* "Disk #, Side [A|B]" */
    char side_name[32];         /* Side name (Front, Player, etc.) */
    char contributor[64];       /* Person who imaged disk */
    char image_date[32];        /* RFC3339 date */
} woz_metadata_t;

/* ============================================================================
 * WOZ Image Structure
 * ============================================================================ */

/**
 * @brief Complete WOZ image in memory
 */
typedef struct {
    /* Header info */
    uint32_t version;           /* 1 or 2 */
    uint32_t file_crc;          /* File CRC32 */
    bool     crc_valid;         /* CRC validation result */
    
    /* INFO chunk */
    woz_info_t info;
    
    /* TMAP chunk - track mapping */
    uint8_t tmap[WOZ_TMAP_SIZE];
    
    /* FLUX chunk - flux track mapping (v2.1) */
    uint8_t flux_map[WOZ_TMAP_SIZE];
    bool    has_flux;
    
    /* TRKS chunk - track data */
    woz_trk_t trks[WOZ_MAX_TRACKS];
    uint8_t  *track_data;       /* All track data */
    size_t   track_data_size;   /* Total track data size */
    
    /* META chunk */
    woz_metadata_t metadata;
    bool     has_metadata;
    
    /* WRIT chunk */
    woz_wtrk_t *write_hints;    /* Array of write hints */
    int      write_hint_count;
    bool     has_write_hints;
    
    /* Calculated values */
    int      total_tracks;      /* Number of unique tracks */
    int      quarter_tracks;    /* Number of mapped quarter-tracks */
    bool     is_525;            /* true = 5.25", false = 3.5" */
    
} woz_image_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Load a WOZ file into memory
 * @param filename Path to WOZ file
 * @param image Output image structure (must be freed with woz_free)
 * @return 0 on success, error code on failure
 */
int woz_load(const char *filename, woz_image_t **image);

/**
 * @brief Load WOZ from memory buffer
 * @param data Buffer containing WOZ data
 * @param size Size of buffer
 * @param image Output image structure
 * @return 0 on success, error code on failure
 */
int woz_load_from_memory(const uint8_t *data, size_t size, woz_image_t **image);

/**
 * @brief Free WOZ image resources
 * @param image Image to free
 */
void woz_free(woz_image_t *image);

/**
 * @brief Save WOZ image to file
 * @param image Image to save
 * @param filename Output filename
 * @return 0 on success, error code on failure
 */
int woz_save(const woz_image_t *image, const char *filename);

/**
 * @brief Get track data for a specific quarter-track (5.25")
 * @param image WOZ image
 * @param quarter_track Quarter-track number (0-159)
 * @param data Output pointer to track data
 * @param bit_count Output bit count
 * @return 0 on success, -1 if track is empty
 */
int woz_get_track_525(const woz_image_t *image, int quarter_track,
                       const uint8_t **data, uint32_t *bit_count);

/**
 * @brief Get track data for a specific track/side (3.5")
 * @param image WOZ image
 * @param track Track number (0-79)
 * @param side Side (0 or 1)
 * @param data Output pointer to track data
 * @param bit_count Output bit count
 * @return 0 on success, -1 if track is empty
 */
int woz_get_track_35(const woz_image_t *image, int track, int side,
                      const uint8_t **data, uint32_t *bit_count);

/**
 * @brief Get flux data for a track (WOZ 2.1)
 * @param image WOZ image
 * @param quarter_track Quarter-track number
 * @param data Output pointer to flux data
 * @param byte_count Output byte count
 * @return 0 on success, -1 if no flux data
 */
int woz_get_flux(const woz_image_t *image, int quarter_track,
                  const uint8_t **data, uint32_t *byte_count);

/**
 * @brief Convert WOZ to DSK/DO format (sector-based)
 * @param image WOZ image
 * @param output Output buffer (must be 143360 bytes for 35 tracks)
 * @param output_size Size of output buffer
 * @param dos_order true for .DO format, false for .PO format
 * @return 0 on success, error code on failure
 */
int woz_to_dsk(const woz_image_t *image, uint8_t *output, size_t output_size,
                bool dos_order);

/**
 * @brief Convert WOZ to NIB format (nibble-based)
 * @param image WOZ image
 * @param output Output buffer (must be 232960 bytes)
 * @param output_size Size of output buffer
 * @return 0 on success, error code on failure
 */
int woz_to_nib(const woz_image_t *image, uint8_t *output, size_t output_size);

/**
 * @brief Create WOZ from DSK/DO format
 * @param dsk_data Input DSK data (143360 bytes)
 * @param dsk_size Size of input
 * @param dos_order true if .DO format, false if .PO format
 * @param image Output WOZ image
 * @return 0 on success, error code on failure
 */
int woz_from_dsk(const uint8_t *dsk_data, size_t dsk_size, bool dos_order,
                  woz_image_t **image);

/**
 * @brief Verify WOZ file integrity (CRC check)
 * @param image WOZ image
 * @return true if CRC is valid or not present
 */
bool woz_verify_crc(const woz_image_t *image);

/**
 * @brief Calculate CRC32 of data
 * @param crc Initial CRC value (use 0)
 * @param data Data to checksum
 * @param size Size of data
 * @return CRC32 value
 */
uint32_t woz_crc32(uint32_t crc, const uint8_t *data, size_t size);

/**
 * @brief Get version string for WOZ format
 * @param version Version number (1 or 2)
 * @return Version string
 */
const char *woz_version_string(uint32_t version);

/**
 * @brief Get disk type string
 * @param disk_type Disk type value
 * @return "5.25\"" or "3.5\""
 */
const char *woz_disk_type_string(uint8_t disk_type);

/**
 * @brief Get hardware compatibility string
 * @param hw_flags Hardware compatibility flags
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Number of characters written
 */
int woz_hardware_string(uint16_t hw_flags, char *buffer, size_t size);

/**
 * @brief Generate human-readable info about WOZ image
 * @param image WOZ image
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Number of characters written
 */
int woz_info_string(const woz_image_t *image, char *buffer, size_t size);

/**
 * @brief Check if track uses Spiradisc-style protection
 * Looks for quarter-track stepping patterns
 * @param image WOZ image
 * @return true if Spiradisc-like protection detected
 */
bool woz_detect_spiradisc(const woz_image_t *image);

/**
 * @brief Check if disk has cross-track synchronization
 * @param image WOZ image
 * @return true if synchronized tracks detected
 */
bool woz_has_sync_tracks(const woz_image_t *image);

/**
 * @brief Simulate MC3470 fake bit behavior
 * When reading more than 2 consecutive zero bits, returns random bits
 * @param bit_stream Input bit stream
 * @param bit_count Number of bits
 * @param position Current position (updated)
 * @return Next nibble value
 */
uint8_t woz_read_nibble_mc3470(const uint8_t *bit_stream, uint32_t bit_count,
                                 uint32_t *position);

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define WOZ_OK                  0
#define WOZ_ERR_FILE_NOT_FOUND  -1
#define WOZ_ERR_INVALID_HEADER  -2
#define WOZ_ERR_INVALID_CRC     -3
#define WOZ_ERR_UNSUPPORTED_VER -4
#define WOZ_ERR_MISSING_INFO    -5
#define WOZ_ERR_MISSING_TMAP    -6
#define WOZ_ERR_MISSING_TRKS    -7
#define WOZ_ERR_OUT_OF_MEMORY   -8
#define WOZ_ERR_CORRUPT_DATA    -9
#define WOZ_ERR_WRITE_FAILED    -10

#ifdef __cplusplus
}
#endif

#pragma pack(pop)

#endif /* UFT_WOZ_H */
