/**
 * @file uft_amstrad_cpc.h
 * @brief Amstrad CPC Disk Format Support
 * 
 * Based on cpctools/amstrad_cpc source code
 * 
 * The Amstrad CPC uses a modified CP/M filesystem on 3" disks:
 *   - Data format: 40/80 tracks, 9 sectors/track, 512 bytes/sector
 *   - System format: 40 tracks, 9 sectors/track, 512 bytes/sector
 *   - Vendor format: 40 tracks, 10 sectors/track, 512 bytes/sector (PCW)
 * 
 * Sector IDs start at 0xC1 (or 0x01 on some formats)
 */

#ifndef UFT_AMSTRAD_CPC_H
#define UFT_AMSTRAD_CPC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Format Constants
 *===========================================================================*/

/** CPC Data format (standard) */
#define UFT_CPC_DATA_TRACKS         40
#define UFT_CPC_DATA_SECTORS        9
#define UFT_CPC_DATA_SECTOR_SIZE    512
#define UFT_CPC_DATA_FIRST_SECTOR   0xC1
#define UFT_CPC_DATA_CAPACITY       (40 * 9 * 512)  /* 180KB */

/** CPC System format (with boot sector) */
#define UFT_CPC_SYSTEM_TRACKS       40
#define UFT_CPC_SYSTEM_SECTORS      9
#define UFT_CPC_SYSTEM_RESERVED     2   /**< Reserved tracks for system */

/** CPC Vendor format (PCW/Spectrum +3) */
#define UFT_CPC_VENDOR_TRACKS       40
#define UFT_CPC_VENDOR_SECTORS      10  /**< Extra sector on PCW */
#define UFT_CPC_VENDOR_CAPACITY     (40 * 10 * 512)  /* 200KB */

/** Double-sided variants */
#define UFT_CPC_DS_TRACKS           80
#define UFT_CPC_DS_DATA_CAPACITY    (80 * 9 * 512)  /* 360KB */

/** EDSK extended format */
#define UFT_CPC_EDSK_MAGIC          "EXTENDED CPC DSK File\r\nDisk-Info\r\n"
#define UFT_CPC_DSK_MAGIC           "MV - CPC"
#define UFT_CPC_EDSK_TRACK_MAGIC    "Track-Info\r\n"

/*===========================================================================
 * AMSDOS Header
 *===========================================================================*/

/** AMSDOS header size */
#define UFT_AMSDOS_HEADER_SIZE      128

/** AMSDOS file types */
typedef enum {
    UFT_AMSDOS_TYPE_BASIC       = 0,    /**< BASIC program */
    UFT_AMSDOS_TYPE_PROTECTED   = 1,    /**< Protected BASIC */
    UFT_AMSDOS_TYPE_BINARY      = 2,    /**< Binary file */
    UFT_AMSDOS_TYPE_ASCII       = 0x16  /**< ASCII text file */
} uft_amsdos_type_t;

/**
 * @brief AMSDOS file header (128 bytes)
 */
typedef struct __attribute__((packed)) {
    uint8_t  user;              /**< User number (0-15) */
    char     filename[8];       /**< Filename (space-padded) */
    char     extension[3];      /**< Extension (space-padded) */
    uint8_t  extent_low;        /**< Extent number low byte */
    uint8_t  reserved1[2];      /**< Reserved */
    uint8_t  record_count;      /**< Record count in extent */
    uint8_t  reserved2[16];     /**< Reserved */
    uint8_t  file_type;         /**< AMSDOS file type */
    uint8_t  reserved3[2];      /**< Reserved */
    uint16_t load_address;      /**< Load address (little-endian) */
    uint8_t  reserved4;         /**< Reserved */
    uint16_t length;            /**< File length (little-endian) */
    uint16_t exec_address;      /**< Execution address (little-endian) */
    uint8_t  reserved5[36];     /**< Reserved */
    uint16_t file_length;       /**< Real file length (for >64K files) */
    uint8_t  reserved6[3];      /**< Reserved */
    uint16_t checksum;          /**< Header checksum */
    uint8_t  reserved7[59];     /**< Padding to 128 bytes */
} uft_amsdos_header_t;

/*===========================================================================
 * DSK/EDSK Format Structures
 *===========================================================================*/

/**
 * @brief DSK disk information block (256 bytes)
 */
typedef struct __attribute__((packed)) {
    char     magic[34];         /**< "MV - CPC..." or "EXTENDED CPC DSK..." */
    char     creator[14];       /**< Creator name */
    uint8_t  num_tracks;        /**< Number of tracks */
    uint8_t  num_sides;         /**< Number of sides */
    uint16_t track_size;        /**< Track size (DSK only, unused in EDSK) */
    uint8_t  track_sizes[204];  /**< EDSK: size of each track / 256 */
} uft_dsk_header_t;

/**
 * @brief DSK track information block (256 bytes)
 */
typedef struct __attribute__((packed)) {
    char     magic[12];         /**< "Track-Info\r\n" */
    uint8_t  padding[4];        /**< Unused */
    uint8_t  track;             /**< Track number */
    uint8_t  side;              /**< Side number */
    uint8_t  unused[2];         /**< Unused */
    uint8_t  sector_size;       /**< Sector size code (2=512) */
    uint8_t  num_sectors;       /**< Number of sectors */
    uint8_t  gap3_length;       /**< GAP3 length */
    uint8_t  filler_byte;       /**< Filler byte */
    /* Followed by sector info blocks */
} uft_dsk_track_header_t;

/**
 * @brief DSK sector information (8 bytes per sector)
 */
typedef struct __attribute__((packed)) {
    uint8_t  track;             /**< Track (C) */
    uint8_t  side;              /**< Side (H) */
    uint8_t  sector_id;         /**< Sector ID (R) */
    uint8_t  size;              /**< Size code (N) */
    uint8_t  fdcstat1;          /**< FDC status 1 */
    uint8_t  fdcstat2;          /**< FDC status 2 */
    uint16_t data_length;       /**< EDSK: actual data length */
} uft_dsk_sector_info_t;

/*===========================================================================
 * CP/M on CPC
 *===========================================================================*/

/** CP/M directory entry size */
#define UFT_CPC_CPM_DIRENTRY_SIZE   32

/** CP/M extent mask for CPC */
#define UFT_CPC_CPM_EXTENT_MASK     0x1F

/** Block size (allocation unit) */
#define UFT_CPC_BLOCK_SIZE          1024

/** Directory entries (CPC Data format) */
#define UFT_CPC_DIR_ENTRIES         64

/**
 * @brief CPC CP/M directory entry
 */
typedef struct __attribute__((packed)) {
    uint8_t  user;              /**< User number (0-15, 0xE5=deleted) */
    char     filename[8];       /**< Filename */
    char     extension[3];      /**< Extension (with flags in high bits) */
    uint8_t  extent_low;        /**< Extent number low */
    uint8_t  reserved1;         /**< Reserved */
    uint8_t  extent_high;       /**< Extent number high */
    uint8_t  record_count;      /**< Records in this extent (0-128) */
    uint8_t  allocation[16];    /**< Block allocation map */
} uft_cpc_cpm_dirent_t;

/*===========================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Detect CPC disk format
 * @param data Disk image data
 * @param size Image size
 * @return 'D'=Data, 'S'=System, 'V'=Vendor, 'E'=EDSK, 0=unknown
 */
char uft_cpc_detect_format(const uint8_t *data, size_t size);

/**
 * @brief Check if DSK/EDSK file
 * @param data File data
 * @param size File size
 * @return true if valid DSK/EDSK
 */
bool uft_cpc_is_dsk(const uint8_t *data, size_t size);

/**
 * @brief Check if extended DSK (EDSK)
 * @param data DSK file data
 * @return true if EDSK format
 */
static inline bool uft_cpc_is_edsk(const uint8_t *data)
{
    return (data[0] == 'E' && data[1] == 'X' && data[2] == 'T');
}

/**
 * @brief Calculate AMSDOS header checksum
 * @param header 128-byte header
 * @return 16-bit checksum
 */
uint16_t uft_amsdos_checksum(const uint8_t header[128]);

/**
 * @brief Validate AMSDOS header
 * @param header Header to validate
 * @return true if header is valid
 */
bool uft_amsdos_validate(const uft_amsdos_header_t *header);

/**
 * @brief Get sector ID for track/sector
 * @param format 'D'=Data, 'S'=System, 'V'=Vendor
 * @param sector Logical sector (0-based)
 * @return Sector ID (typically 0xC1+sector)
 */
static inline uint8_t uft_cpc_sector_id(char format, uint8_t sector)
{
    (void)format; /* Same for all standard formats */
    return UFT_CPC_DATA_FIRST_SECTOR + sector;
}

/**
 * @brief Calculate byte offset in image
 * @param track Track number
 * @param side Side number
 * @param sector Sector number (0-based)
 * @param num_sides Total sides (1 or 2)
 * @param sectors_per_track Sectors per track
 * @return Byte offset
 */
static inline uint32_t uft_cpc_offset(uint8_t track, uint8_t side,
                                       uint8_t sector, uint8_t num_sides,
                                       uint8_t sectors_per_track)
{
    uint32_t linear_track = track * num_sides + side;
    uint32_t linear_sector = linear_track * sectors_per_track + sector;
    return linear_sector * UFT_CPC_DATA_SECTOR_SIZE;
}

/**
 * @brief Read sector from DSK image
 * @param dsk DSK file data
 * @param dsk_size DSK file size
 * @param track Track number
 * @param side Side number
 * @param sector_id Sector ID
 * @param buffer Output buffer (512 bytes)
 * @return true on success
 */
bool uft_cpc_read_sector(const uint8_t *dsk, size_t dsk_size,
                         uint8_t track, uint8_t side, uint8_t sector_id,
                         uint8_t *buffer);

/**
 * @brief List files in CP/M directory
 * @param dsk DSK file data
 * @param dsk_size DSK file size
 * @param callback Function called for each file
 * @param user_data User data passed to callback
 * @return Number of files found
 */
int uft_cpc_list_files(const uint8_t *dsk, size_t dsk_size,
                       void (*callback)(const char *name, uint32_t size, void *ud),
                       void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* UFT_AMSTRAD_CPC_H */
