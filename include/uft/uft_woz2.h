/**
 * @file uft_woz2.h
 * @brief UnifiedFloppyTool - WOZ 2.x Format Support (Extended)
 * @version 3.1.4.008
 *
 * Complete WOZ 2.x format implementation with validation rules
 * derived from wozardry by 4am.
 *
 * WOZ2 Specification: https://applesaucefdc.com/woz/reference2/
 *
 * Sources analyzed:
 * - wozardry.py (4am, 2018-2022)
 * - test_wozardry.py (validation test cases)
 */

#ifndef UFT_WOZ2_H
#define UFT_WOZ2_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

/* Magic signatures */
#define UFT_WOZ1_MAGIC      0x315A4F57  /* "WOZ1" little-endian */
#define UFT_WOZ2_MAGIC      0x325A4F57  /* "WOZ2" little-endian */
#define UFT_MOOF_MAGIC      0x464F4F4D  /* "MOOF" little-endian */

/* Magic bytes after signature */
#define UFT_WOZ_MAGIC_FF    0xFF
#define UFT_WOZ_MAGIC_LF1   0x0A
#define UFT_WOZ_MAGIC_CR    0x0D
#define UFT_WOZ_MAGIC_LF2   0x0A

/* Chunk IDs */
#define UFT_WOZ_CHUNK_INFO  0x4F464E49  /* "INFO" */
#define UFT_WOZ_CHUNK_TMAP  0x50414D54  /* "TMAP" */
#define UFT_WOZ_CHUNK_TRKS  0x534B5254  /* "TRKS" */
#define UFT_WOZ_CHUNK_WRIT  0x54495257  /* "WRIT" (WOZ2 only) */
#define UFT_WOZ_CHUNK_FLUX  0x58554C46  /* "FLUX" (WOZ3 only) */
#define UFT_WOZ_CHUNK_META  0x4154454D  /* "META" */

/* Chunk sizes */
#define UFT_WOZ_INFO_SIZE   60
#define UFT_WOZ_TMAP_SIZE   160
#define UFT_WOZ1_TRACK_SIZE 6646

/* Track map */
#define UFT_WOZ_TMAP_EMPTY  0xFF
#define UFT_WOZ_MAX_TRACKS  160

/* Default bit timing */
#define UFT_WOZ_BIT_TIMING_525  32  /* 5.25" disk */
#define UFT_WOZ_BIT_TIMING_35   16  /* 3.5" disk */

/*============================================================================
 * Disk Types
 *============================================================================*/

typedef enum {
    UFT_WOZ_DISK_UNKNOWN = 0,
    UFT_WOZ_DISK_525     = 1,  /**< 5.25-inch (140K) */
    UFT_WOZ_DISK_35      = 2   /**< 3.5-inch (400K/800K) */
} uft_woz_disk_type_t;

typedef enum {
    UFT_MOOF_DISK_UNKNOWN = 0,
    UFT_MOOF_DISK_35_SSDD = 1,  /**< 3.5" 400K */
    UFT_MOOF_DISK_35_DSDD = 2,  /**< 3.5" 800K */
    UFT_MOOF_DISK_35_DSHD = 3   /**< 3.5" 1.44MB */
} uft_moof_disk_type_t;

typedef enum {
    UFT_WOZ_BOOT_UNKNOWN = 0,
    UFT_WOZ_BOOT_16SECTOR = 1,
    UFT_WOZ_BOOT_13SECTOR = 2,
    UFT_WOZ_BOOT_HYBRID = 3
} uft_woz_boot_sector_format_t;

/*============================================================================
 * Languages (from WOZ specification)
 *============================================================================*/

static const char* uft_woz_languages[] = {
    "English", "Spanish", "French", "German", "Chinese",
    "Japanese", "Italian", "Dutch", "Portuguese", "Danish",
    "Finnish", "Norwegian", "Swedish", "Russian", "Polish",
    "Turkish", "Arabic", "Thai", "Czech", "Hungarian",
    "Catalan", "Croatian", "Greek", "Hebrew", "Romanian",
    "Slovak", "Ukrainian", "Indonesian", "Malay", "Vietnamese",
    "Other"
};
#define UFT_WOZ_LANGUAGE_COUNT 31

/*============================================================================
 * RAM Requirements
 *============================================================================*/

static const char* uft_woz_requires_ram[] = {
    "16K", "24K", "32K", "48K", "64K", "128K",
    "256K", "512K", "768K", "1M", "1.25M", "1.5M+", "Unknown"
};
#define UFT_WOZ_RAM_COUNT 13

/*============================================================================
 * Compatible Hardware (Apple II models)
 *============================================================================*/

static const char* uft_woz_requires_machine[] = {
    "2", "2+", "2e", "2c", "2e+", "2gs", "2c+", "3", "3+"
};
#define UFT_WOZ_MACHINE_COUNT 9

/*============================================================================
 * WOZ File Header
 *============================================================================*/

typedef struct {
    uint32_t signature;     /**< WOZ1/WOZ2/MOOF */
    uint8_t  ff_marker;     /**< Must be 0xFF */
    uint8_t  lf_cr_lf[3];   /**< Must be 0x0A 0x0D 0x0A */
    uint32_t crc32;         /**< CRC of all data after header */
} uft_woz_header_t;

/*============================================================================
 * INFO Chunk (60 bytes)
 *============================================================================*/

typedef struct {
    uint8_t  version;           /**< INFO version (1 for WOZ1, 2+ for WOZ2) */
    uint8_t  disk_type;         /**< 1=5.25", 2=3.5" */
    uint8_t  write_protected;   /**< 0=no, 1=yes */
    uint8_t  synchronized;      /**< 0=no, 1=yes */
    uint8_t  cleaned;           /**< 0=no, 1=yes (or optimal_bit_timing for MOOF) */
    char     creator[32];       /**< UTF-8 creator string */
    
    /* WOZ2 only (v2+) */
    uint8_t  disk_sides;        /**< 1 or 2 */
    uint8_t  boot_sector_format;/**< 0-3 */
    uint8_t  optimal_bit_timing;/**< 8-40 depending on disk type */
    uint16_t compatible_hardware; /**< 9-bit mask */
    uint16_t required_ram;      /**< RAM in KB */
    uint16_t largest_track;     /**< Largest track blocks */
    
    uint8_t  reserved[22];
} uft_woz_info_t;

/*============================================================================
 * TRKS Chunk Entry (WOZ2)
 *============================================================================*/

typedef struct {
    uint16_t starting_block;    /**< 0=unused, 3+=valid */
    uint16_t block_count;       /**< Number of 512-byte blocks */
    uint32_t bit_count;         /**< Number of bits in track */
} uft_woz2_trk_entry_t;

/*============================================================================
 * Track Data
 *============================================================================*/

typedef struct {
    uint8_t *raw_bytes;         /**< Bitstream data */
    uint32_t bit_count;         /**< Number of valid bits */
    size_t   byte_count;        /**< Allocated byte count */
    
    /* WOZ1 splice info */
    uint16_t splice_point;      /**< Bit position of splice (0xFFFF=none) */
    uint8_t  splice_nibble;     /**< Nibble at splice */
    uint8_t  splice_bit_count;  /**< 8, 9, or 10 */
} uft_woz_track_t;

/*============================================================================
 * WOZ Disk Image
 *============================================================================*/

typedef struct {
    uint32_t        image_type;     /**< WOZ1/WOZ2/MOOF magic */
    uft_woz_info_t  info;
    
    uint8_t         tmap[160];      /**< Track map */
    uft_woz_track_t tracks[160];    /**< Track data */
    size_t          track_count;    /**< Number of valid tracks */
    
    uint8_t         flux[160];      /**< FLUX chunk (WOZ3) */
    bool            has_flux;
    
    uint8_t        *writ_data;      /**< WRIT chunk raw data */
    size_t          writ_len;
    
    /* Metadata (simplified key-value store) */
    char           *meta_raw;       /**< Raw metadata string */
    size_t          meta_len;
} uft_woz_image_t;

/*============================================================================
 * Validation Error Codes
 *============================================================================*/

typedef enum {
    UFT_WOZ_OK = 0,
    UFT_WOZ_ERR_EOF,
    UFT_WOZ_ERR_NO_WOZ_MARKER,
    UFT_WOZ_ERR_NO_FF,
    UFT_WOZ_ERR_NO_LF,
    UFT_WOZ_ERR_BAD_CRC,
    UFT_WOZ_ERR_MISSING_INFO,
    UFT_WOZ_ERR_BAD_INFO_SIZE,
    UFT_WOZ_ERR_BAD_VERSION,
    UFT_WOZ_ERR_BAD_DISK_TYPE,
    UFT_WOZ_ERR_BAD_WRITE_PROTECTED,
    UFT_WOZ_ERR_BAD_SYNCHRONIZED,
    UFT_WOZ_ERR_BAD_CLEANED,
    UFT_WOZ_ERR_BAD_CREATOR,
    UFT_WOZ_ERR_BAD_DISK_SIDES,
    UFT_WOZ_ERR_BAD_BOOT_SECTOR_FORMAT,
    UFT_WOZ_ERR_BAD_OPTIMAL_BIT_TIMING,
    UFT_WOZ_ERR_BAD_COMPATIBLE_HARDWARE,
    UFT_WOZ_ERR_BAD_RAM,
    UFT_WOZ_ERR_MISSING_TMAP,
    UFT_WOZ_ERR_BAD_TMAP_SIZE,
    UFT_WOZ_ERR_BAD_TMAP_ENTRY,
    UFT_WOZ_ERR_BAD_TRKS_STARTING_BLOCK,
    UFT_WOZ_ERR_BAD_TRKS_BLOCK_COUNT,
    UFT_WOZ_ERR_BAD_TRKS_BIT_COUNT,
    UFT_WOZ_ERR_BAD_META_ENCODING,
    UFT_WOZ_ERR_BAD_META_FORMAT,
    UFT_WOZ_ERR_DUPLICATE_META_KEY,
    UFT_WOZ_ERR_OOM
} uft_woz_error_t;

/*============================================================================
 * Inline Validation Functions
 *============================================================================*/

/**
 * @brief Validate version field
 */
static inline uft_woz_error_t uft_woz_validate_version(uint32_t image_type, uint8_t version) {
    if (image_type == UFT_WOZ1_MAGIC && version != 1)
        return UFT_WOZ_ERR_BAD_VERSION;
    if (image_type == UFT_WOZ2_MAGIC && version < 2)
        return UFT_WOZ_ERR_BAD_VERSION;
    if (image_type == UFT_MOOF_MAGIC && version != 1)
        return UFT_WOZ_ERR_BAD_VERSION;
    return UFT_WOZ_OK;
}

/**
 * @brief Validate disk type field
 */
static inline uft_woz_error_t uft_woz_validate_disk_type(uint32_t image_type, uint8_t disk_type) {
    if (image_type == UFT_MOOF_MAGIC) {
        if (disk_type > 3) return UFT_WOZ_ERR_BAD_DISK_TYPE;
    } else {
        if (disk_type != 1 && disk_type != 2) return UFT_WOZ_ERR_BAD_DISK_TYPE;
    }
    return UFT_WOZ_OK;
}

/**
 * @brief Validate boolean field (0 or 1)
 */
static inline bool uft_woz_is_valid_bool(uint8_t v) {
    return v == 0 || v == 1;
}

/**
 * @brief Validate disk sides (WOZ2)
 */
static inline uft_woz_error_t uft_woz_validate_disk_sides(uint8_t disk_type, uint8_t disk_sides) {
    if (disk_type == 1) { /* 5.25" */
        if (disk_sides != 1) return UFT_WOZ_ERR_BAD_DISK_SIDES;
    } else { /* 3.5" */
        if (disk_sides != 1 && disk_sides != 2) return UFT_WOZ_ERR_BAD_DISK_SIDES;
    }
    return UFT_WOZ_OK;
}

/**
 * @brief Validate boot sector format (WOZ2)
 */
static inline uft_woz_error_t uft_woz_validate_boot_sector_format(uint8_t disk_type, uint8_t fmt) {
    if (disk_type == 1) { /* 5.25" */
        if (fmt > 3) return UFT_WOZ_ERR_BAD_BOOT_SECTOR_FORMAT;
    } else { /* 3.5" */
        if (fmt != 0) return UFT_WOZ_ERR_BAD_BOOT_SECTOR_FORMAT;
    }
    return UFT_WOZ_OK;
}

/**
 * @brief Validate optimal bit timing (WOZ2)
 */
static inline uft_woz_error_t uft_woz_validate_optimal_bit_timing(uint32_t image_type,
                                                                   uint8_t disk_type,
                                                                   uint8_t timing) {
    if (image_type == UFT_MOOF_MAGIC) {
        if (timing != 8 && timing != 16) return UFT_WOZ_ERR_BAD_OPTIMAL_BIT_TIMING;
    } else if (disk_type == 1) { /* 5.25" */
        if (timing < 24 || timing > 40) return UFT_WOZ_ERR_BAD_OPTIMAL_BIT_TIMING;
    } else { /* 3.5" */
        if (timing < 8 || timing > 24) return UFT_WOZ_ERR_BAD_OPTIMAL_BIT_TIMING;
    }
    return UFT_WOZ_OK;
}

/**
 * @brief Validate compatible hardware bitfield (WOZ2)
 */
static inline uft_woz_error_t uft_woz_validate_compatible_hardware(uint16_t hw) {
    /* Only lower 9 bits valid */
    if (hw >= 0x0200) return UFT_WOZ_ERR_BAD_COMPATIBLE_HARDWARE;
    return UFT_WOZ_OK;
}

/**
 * @brief Get track quarter string
 */
static inline const char* uft_woz_track_quarter(int tmap_index) {
    static const char* quarters[] = {".00", ".25", ".50", ".75"};
    return quarters[tmap_index % 4];
}

/**
 * @brief Convert TMAP index to track number string
 */
static inline void uft_woz_track_str(int tmap_index, char *buf, size_t cap) {
    if (!buf || cap < 8) return;
    snprintf(buf, cap, "%d%s", tmap_index / 4, uft_woz_track_quarter(tmap_index));
}

/*============================================================================
 * CRC-32 Calculation
 *============================================================================*/

/**
 * @brief Calculate CRC-32 (same as zlib crc32)
 */
static inline uint32_t uft_woz_crc32(const uint8_t *data, size_t len) {
    static const uint32_t table[256] = {
        0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
        0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
        0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
        0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
        0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
        0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
        0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
        0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
        0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
        0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
        0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
        0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
        0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
        0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
        0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
        0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
        0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
        0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
        0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
        0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
        0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
        0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
        0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
        0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
        0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
        0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
        0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
        0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
        0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
        0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
        0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
        0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
        0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
        0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
        0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
        0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
        0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
        0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
        0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
        0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
        0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD706B3,
        0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
        0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
    };
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

/*============================================================================
 * Function Declarations
 *============================================================================*/

/**
 * @brief Initialize WOZ image structure
 */
void uft_woz_image_init(uft_woz_image_t *img);

/**
 * @brief Free WOZ image resources
 */
void uft_woz_image_destroy(uft_woz_image_t *img);

/**
 * @brief Parse WOZ from buffer
 */
uft_woz_error_t uft_woz_parse(const uint8_t *data, size_t len, uft_woz_image_t *out);

/**
 * @brief Serialize WOZ to buffer
 * @return Bytes written, or 0 on error
 */
size_t uft_woz_serialize(const uft_woz_image_t *img, uint8_t *buf, size_t cap);

/**
 * @brief Get track bitstream by TMAP index
 */
const uft_woz_track_t* uft_woz_get_track(const uft_woz_image_t *img, int tmap_index);

/**
 * @brief Set track bitstream
 */
bool uft_woz_set_track(uft_woz_image_t *img, int tmap_index,
                       const uint8_t *bits, uint32_t bit_count);

/**
 * @brief Remove track
 */
bool uft_woz_remove_track(uft_woz_image_t *img, int tmap_index);

/**
 * @brief Get metadata value by key
 */
const char* uft_woz_get_meta(const uft_woz_image_t *img, const char *key);

/**
 * @brief Set metadata value
 */
bool uft_woz_set_meta(uft_woz_image_t *img, const char *key, const char *value);

/**
 * @brief Probe if buffer is WOZ format
 */
static inline bool uft_woz_probe(const uint8_t *buf, size_t len) {
    if (len < 8) return false;
    uint32_t magic = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
    return magic == UFT_WOZ1_MAGIC || magic == UFT_WOZ2_MAGIC || magic == UFT_MOOF_MAGIC;
}

/**
 * @brief Get error string
 */
static inline const char* uft_woz_error_str(uft_woz_error_t err) {
    static const char* msgs[] = {
        "OK", "Unexpected EOF", "No WOZ marker", "Missing FF byte",
        "Missing LF bytes", "Bad CRC", "Missing INFO chunk", "Bad INFO size",
        "Bad version", "Bad disk type", "Bad write_protected", "Bad synchronized",
        "Bad cleaned", "Bad creator", "Bad disk_sides", "Bad boot_sector_format",
        "Bad optimal_bit_timing", "Bad compatible_hardware", "Bad required_ram",
        "Missing TMAP chunk", "Bad TMAP size", "Bad TMAP entry",
        "Bad TRKS starting_block", "Bad TRKS block_count", "Bad TRKS bit_count",
        "Bad META encoding", "Bad META format", "Duplicate META key", "Out of memory"
    };
    if (err < 0 || err >= sizeof(msgs)/sizeof(msgs[0])) return "Unknown error";
    return msgs[err];
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_WOZ2_H */
