/**
 * @file uft_td0.h
 * @brief Teledisk (TD0) Format Support for UFT
 * 
 * Teledisk was a popular disk imaging program by Sydex. The format is
 * undocumented but has been reverse-engineered. This implementation is
 * based on the notes by Dave Dunfield and others.
 * 
 * The format supports optional LZSS-Huffman compression ("Advanced Compression").
 * 
 * @copyright UFT Project - Based on reverse-engineering by Dave Dunfield et al.
 */

#ifndef UFT_TD0_H
#define UFT_TD0_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Forward declaration */
struct uft_imd_image_t;

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * TD0 Format Constants
 *============================================================================*/

/** Normal TD0 signature */
#define UFT_TD0_SIG_NORMAL      0x4454  /* "TD" little-endian */

/** Advanced compression TD0 signature */
#define UFT_TD0_SIG_ADVANCED    0x6474  /* "td" little-endian */

/** End of image marker */
#define UFT_TD0_END_OF_IMAGE    0xFF

/*============================================================================
 * LZSS Compression Constants
 *============================================================================*/

/** Size of LZSS ring buffer */
#define UFT_TD0_LZSS_SBSIZE     4096

/** Size of look-ahead buffer */
#define UFT_TD0_LZSS_LASIZE     60

/** Minimum match length for compression */
#define UFT_TD0_LZSS_THRESHOLD  2

/** Number of character codes */
#define UFT_TD0_LZSS_N_CHAR     (256 - UFT_TD0_LZSS_THRESHOLD + UFT_TD0_LZSS_LASIZE)

/** Tree size */
#define UFT_TD0_LZSS_TSIZE      (UFT_TD0_LZSS_N_CHAR * 2 - 1)

/** Root position in tree */
#define UFT_TD0_LZSS_ROOT       (UFT_TD0_LZSS_TSIZE - 1)

/** Maximum frequency before tree rebuild */
#define UFT_TD0_LZSS_MAX_FREQ   0x8000

/*============================================================================
 * TD0 Header Data Rate Values
 *============================================================================*/

/**
 * @brief TD0 data rate encoding
 */
typedef enum {
    UFT_TD0_RATE_250K = 0,  /**< 250 kbps (DD) */
    UFT_TD0_RATE_300K = 1,  /**< 300 kbps (DD on HD drive) */
    UFT_TD0_RATE_500K = 2   /**< 500 kbps (HD) */
} uft_td0_rate_t;

/*============================================================================
 * TD0 Drive Type Values
 *============================================================================*/

/**
 * @brief TD0 source drive type
 */
typedef enum {
    UFT_TD0_DRIVE_525_96   = 1,  /**< 5.25" 96 TPI (1.2MB) */
    UFT_TD0_DRIVE_525_48   = 2,  /**< 5.25" 48 TPI (360K) */
    UFT_TD0_DRIVE_35_HD    = 3,  /**< 3.5" HD */
    UFT_TD0_DRIVE_35_DD    = 4,  /**< 3.5" DD */
    UFT_TD0_DRIVE_8INCH    = 5,  /**< 8" drive */
    UFT_TD0_DRIVE_35_ED    = 6   /**< 3.5" ED */
} uft_td0_drive_t;

/*============================================================================
 * TD0 Sector Flags
 *============================================================================*/

/** Sector was duplicated */
#define UFT_TD0_SEC_DUP         0x01

/** Sector has CRC error */
#define UFT_TD0_SEC_CRC         0x02

/** Sector has Deleted Address Mark */
#define UFT_TD0_SEC_DAM         0x04

/** Sector not allocated (DOS mode) */
#define UFT_TD0_SEC_DOS         0x10

/** Sector has no data field */
#define UFT_TD0_SEC_NODAT       0x20

/** Sector has no ID field */
#define UFT_TD0_SEC_NOID        0x40

/*============================================================================
 * TD0 Data Encoding Methods
 *============================================================================*/

/**
 * @brief TD0 sector data encoding methods
 */
typedef enum {
    UFT_TD0_ENC_RAW   = 0,  /**< Raw sector data (uncompressed) */
    UFT_TD0_ENC_REP2  = 1,  /**< 2-byte pattern repetition */
    UFT_TD0_ENC_RLE   = 2   /**< Run-length encoding */
} uft_td0_encoding_t;

/*============================================================================
 * TD0 File Structures (packed binary format)
 *============================================================================*/

/**
 * @brief TD0 main image header (12 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t signature;     /**< "TD" (0x4454) or "td" (0x6474) */
    uint8_t  sequence;      /**< Volume sequence number */
    uint8_t  check_seq;     /**< Check sequence for multi-volume */
    uint8_t  version;       /**< Teledisk version (BCD) */
    uint8_t  data_rate;     /**< Source data rate */
    uint8_t  drive_type;    /**< Source drive type */
    uint8_t  stepping;      /**< Stepping type (0=SS, 1=DS, 2=EDS) */
    uint8_t  dos_mode;      /**< DOS allocation mode */
    uint8_t  sides;         /**< Number of sides */
    uint16_t crc;           /**< Header CRC-16 */
} uft_td0_header_t;
#pragma pack(pop)

/**
 * @brief TD0 comment block header (10 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t crc;           /**< Comment CRC-16 */
    uint16_t length;        /**< Comment data length */
    uint8_t  year;          /**< Year - 1900 */
    uint8_t  month;         /**< Month (1-12) */
    uint8_t  day;           /**< Day (1-31) */
    uint8_t  hour;          /**< Hour (0-23) */
    uint8_t  minute;        /**< Minute (0-59) */
    uint8_t  second;        /**< Second (0-59) */
} uft_td0_comment_header_t;
#pragma pack(pop)

/**
 * @brief TD0 track header (4 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  nsectors;      /**< Number of sectors (0xFF = end) */
    uint8_t  cylinder;      /**< Physical cylinder */
    uint8_t  side;          /**< Physical side/head */
    uint8_t  crc;           /**< Header CRC-8 */
} uft_td0_track_header_t;
#pragma pack(pop)

/**
 * @brief TD0 sector header (6 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  cylinder;      /**< Cylinder in ID field */
    uint8_t  side;          /**< Side in ID field */
    uint8_t  sector;        /**< Sector number in ID field */
    uint8_t  size;          /**< Sector size code (128 << size) */
    uint8_t  flags;         /**< Sector flags */
    uint8_t  crc;           /**< Sector header CRC-8 */
} uft_td0_sector_header_t;
#pragma pack(pop)

/**
 * @brief TD0 data block header (3 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t offset;        /**< Offset to next data block */
    uint8_t  method;        /**< Encoding method */
} uft_td0_data_header_t;
#pragma pack(pop)

/*============================================================================
 * LZSS Decompression State
 *============================================================================*/

/**
 * @brief LZSS-Huffman decompression state
 */
typedef struct {
    /* Huffman tree */
    uint16_t parent[UFT_TD0_LZSS_TSIZE + UFT_TD0_LZSS_N_CHAR];
    uint16_t son[UFT_TD0_LZSS_TSIZE];
    uint16_t freq[UFT_TD0_LZSS_TSIZE + 1];
    
    /* Ring buffer */
    uint8_t  ring_buff[UFT_TD0_LZSS_SBSIZE + UFT_TD0_LZSS_LASIZE - 1];
    
    /* Bit buffer */
    uint16_t bitbuff;
    uint8_t  bits;
    
    /* State machine */
    uint8_t  state;
    uint16_t r;             /**< Ring buffer position */
    uint16_t i, j, k;       /**< Decoder indices */
    
    /* I/O */
    const uint8_t* input;
    size_t   input_pos;
    size_t   input_size;
    bool     eof;
} uft_td0_lzss_state_t;

/*============================================================================
 * TD0 Expanded Structures
 *============================================================================*/

/**
 * @brief TD0 sector data (expanded)
 */
typedef struct {
    uft_td0_sector_header_t header;
    uint8_t* data;          /**< Sector data (NULL if no data) */
    uint16_t data_size;     /**< Actual data size */
} uft_td0_sector_t;

/**
 * @brief TD0 track data (expanded)
 */
typedef struct {
    uft_td0_track_header_t header;
    uint8_t  nsectors;      /**< Number of sectors */
    uft_td0_sector_t* sectors;
} uft_td0_track_t;

/**
 * @brief TD0 image (expanded)
 */
typedef struct {
    uft_td0_header_t header;
    
    /* Comment */
    uft_td0_comment_header_t comment_header;
    char*    comment;
    bool     has_comment;
    
    /* Tracks */
    uint16_t num_tracks;
    uft_td0_track_t* tracks;
    
    /* Metadata */
    uint16_t cylinders;
    uint8_t  heads;
    bool     advanced_compression;
} uft_td0_image_t;

/*============================================================================
 * Huffman Decode Tables (from Teledisk reverse-engineering)
 *============================================================================*/

/**
 * @brief Huffman d_code table for position decoding
 */
extern const uint8_t uft_td0_d_code[256];

/**
 * @brief Huffman d_len table for bit lengths
 */
extern const uint8_t uft_td0_d_len[16];

/*============================================================================
 * TD0 API Functions
 *============================================================================*/

/**
 * @brief Initialize TD0 image structure
 */
int uft_td0_init(uft_td0_image_t* img);

/**
 * @brief Free TD0 image resources
 */
void uft_td0_free(uft_td0_image_t* img);

/**
 * @brief Check if data appears to be TD0 format
 * @param data Data buffer
 * @param size Data size (need at least 2 bytes)
 * @return true if TD0 signature found
 */
bool uft_td0_detect(const uint8_t* data, size_t size);

/**
 * @brief Check if TD0 uses advanced compression
 */
bool uft_td0_is_compressed(const uft_td0_header_t* header);

/**
 * @brief Read TD0 image from file
 */
int uft_td0_read(const char* filename, uft_td0_image_t* img);

/**
 * @brief Read TD0 image from memory
 */
int uft_td0_read_mem(const uint8_t* data, size_t size, uft_td0_image_t* img);

/**
 * @brief Convert TD0 to IMD format
 */
int uft_td0_to_imd(const uft_td0_image_t* td0, struct uft_imd_image_t* imd);

/**
 * @brief Convert TD0 to raw binary
 */
int uft_td0_to_raw(const uft_td0_image_t* img, uint8_t** data,
                   size_t* size, uint8_t fill);

/*============================================================================
 * LZSS Decompression Functions
 *============================================================================*/

/**
 * @brief Initialize LZSS decompression state
 * @param state Decompression state to initialize
 * @param data Compressed data
 * @param size Data size
 */
void uft_td0_lzss_init(uft_td0_lzss_state_t* state,
                       const uint8_t* data, size_t size);

/**
 * @brief Get next decompressed byte
 * @param state Decompression state
 * @return Decompressed byte or -1 on EOF
 */
int uft_td0_lzss_getbyte(uft_td0_lzss_state_t* state);

/**
 * @brief Read block of decompressed data
 * @param state Decompression state
 * @param buffer Output buffer
 * @param size Bytes to read
 * @return Bytes actually read
 */
size_t uft_td0_lzss_read(uft_td0_lzss_state_t* state,
                         uint8_t* buffer, size_t size);

/*============================================================================
 * Sector Data Decoding
 *============================================================================*/

/**
 * @brief Decode RLE-compressed sector data
 * @param src Source data (after data header)
 * @param src_size Source data size
 * @param dst Destination buffer
 * @param dst_size Expected output size
 * @param method Encoding method
 * @return Bytes decoded or negative error
 */
int uft_td0_decode_sector(const uint8_t* src, size_t src_size,
                          uint8_t* dst, size_t dst_size,
                          uint8_t method);

/**
 * @brief Get drive type name
 */
const char* uft_td0_drive_name(uft_td0_drive_t type);

/**
 * @brief Print TD0 image information
 */
void uft_td0_print_info(const uft_td0_image_t* img, bool verbose);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TD0_H */
