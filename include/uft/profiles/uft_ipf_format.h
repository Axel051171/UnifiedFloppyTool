/**
 * @file uft_ipf_format.h
 * @brief IPF (Interchangeable Preservation Format) profile - SPS/CAPS preservation format
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * IPF is the preservation format developed by the Software Preservation Society
 * (SPS) for archiving copy-protected floppy disks. It captures timing-critical
 * and randomized protection schemes with high fidelity.
 *
 * Key features:
 * - Block-based structure with checksums
 * - Flux-level timing data
 * - Weak/fuzzy bit support
 * - Gap and sync pattern preservation
 *
 * Format specification: http://www.softpres.org/
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_IPF_FORMAT_H
#define UFT_IPF_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * IPF Format Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief IPF block signature "CAPS" */
#define UFT_IPF_SIGNATURE           "CAPS"

/** @brief IPF signature length */
#define UFT_IPF_SIGNATURE_LEN       4

/** @brief IPF block header size */
#define UFT_IPF_BLOCK_HEADER_SIZE   12

/** @brief IPF record types */
#define UFT_IPF_RECORD_CAPS         0x43415053  /**< "CAPS" */
#define UFT_IPF_RECORD_INFO         0x494E464F  /**< "INFO" */
#define UFT_IPF_RECORD_IMGE         0x494D4745  /**< "IMGE" */
#define UFT_IPF_RECORD_DATA         0x44415441  /**< "DATA" */

/* ═══════════════════════════════════════════════════════════════════════════
 * IPF Platform Types
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief IPF platform identifiers
 */
typedef enum {
    UFT_IPF_PLATFORM_AMIGA      = 1,    /**< Commodore Amiga */
    UFT_IPF_PLATFORM_ATARI_ST   = 2,    /**< Atari ST */
    UFT_IPF_PLATFORM_PC         = 3,    /**< IBM PC */
    UFT_IPF_PLATFORM_AMSTRAD    = 4,    /**< Amstrad CPC */
    UFT_IPF_PLATFORM_SPECTRUM   = 5,    /**< ZX Spectrum */
    UFT_IPF_PLATFORM_SAM        = 6,    /**< SAM Coupé */
    UFT_IPF_PLATFORM_ARCHIMEDES = 7,    /**< Acorn Archimedes */
    UFT_IPF_PLATFORM_C64        = 8,    /**< Commodore 64 */
    UFT_IPF_PLATFORM_ATARI_8BIT = 9     /**< Atari 8-bit */
} uft_ipf_platform_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * IPF Encoder Types
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief IPF encoder identifiers
 */
typedef enum {
    UFT_IPF_ENC_CAPS        = 1,    /**< CAPS encoder */
    UFT_IPF_ENC_SPS         = 2,    /**< SPS encoder */
    UFT_IPF_ENC_CTR         = 3     /**< Custom encoder */
} uft_ipf_encoder_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * IPF Density Types
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief IPF density identifiers
 */
typedef enum {
    UFT_IPF_DENSITY_NOISE   = 0,    /**< Unknown/noise */
    UFT_IPF_DENSITY_AUTO    = 1,    /**< Automatic */
    UFT_IPF_DENSITY_FMSD    = 2,    /**< FM single density */
    UFT_IPF_DENSITY_FMDD    = 3,    /**< FM double density */
    UFT_IPF_DENSITY_MFMSD   = 4,    /**< MFM single density */
    UFT_IPF_DENSITY_MFMDD   = 5,    /**< MFM double density */
    UFT_IPF_DENSITY_MFMHD   = 6,    /**< MFM high density */
    UFT_IPF_DENSITY_MFMED   = 7     /**< MFM extra density */
} uft_ipf_density_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * IPF Data Flags
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief IPF data element flags
 */
#define UFT_IPF_FLAG_FUZZY          0x0001  /**< Fuzzy/weak bits */
#define UFT_IPF_FLAG_NOFLUX         0x0002  /**< No flux area */
#define UFT_IPF_FLAG_SYNC           0x0004  /**< Sync pattern */
#define UFT_IPF_FLAG_GAP            0x0008  /**< Gap data */

/* ═══════════════════════════════════════════════════════════════════════════
 * IPF Block Types
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief IPF block element types
 */
typedef enum {
    UFT_IPF_ELEM_GAP        = 1,    /**< Gap element */
    UFT_IPF_ELEM_SYNC       = 2,    /**< Sync element */
    UFT_IPF_ELEM_DATA       = 3,    /**< Data element */
    UFT_IPF_ELEM_MARK       = 4,    /**< Mark element */
    UFT_IPF_ELEM_FORWARD    = 5,    /**< Forward gap */
    UFT_IPF_ELEM_BACKWARD   = 6     /**< Backward gap */
} uft_ipf_element_type_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * IPF Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief IPF block header (12 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t type;                  /**< Block type (big-endian) */
    uint32_t length;                /**< Block length (big-endian) */
    uint32_t crc;                   /**< Block CRC (big-endian) */
} uft_ipf_block_header_t;
#pragma pack(pop)

/**
 * @brief IPF CAPS record (12 bytes after header)
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t type;                  /**< Encoder type */
    uint32_t reserved[2];           /**< Reserved */
} uft_ipf_caps_record_t;
#pragma pack(pop)

/**
 * @brief IPF INFO record (96 bytes after header)
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t type;                  /**< Media type */
    uint32_t encoder_type;          /**< Encoder type */
    uint32_t encoder_rev;           /**< Encoder revision */
    uint32_t release;               /**< SPS release number */
    uint32_t revision;              /**< SPS revision number */
    uint32_t origin;                /**< Disk origin */
    uint32_t min_cylinder;          /**< Minimum cylinder */
    uint32_t max_cylinder;          /**< Maximum cylinder */
    uint32_t min_head;              /**< Minimum head */
    uint32_t max_head;              /**< Maximum head */
    uint32_t creation_date;         /**< Creation date (YYYYMMDD) */
    uint32_t creation_time;         /**< Creation time (HHMMSSTTT) */
    uint32_t platforms[4];          /**< Supported platforms */
    uint32_t disk_num;              /**< Disk number */
    uint32_t creator_id;            /**< Creator ID */
    uint32_t reserved[3];           /**< Reserved */
} uft_ipf_info_record_t;
#pragma pack(pop)

/**
 * @brief IPF IMGE record (80 bytes after header)
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t cylinder;              /**< Cylinder number */
    uint32_t head;                  /**< Head number */
    uint32_t density;               /**< Density type */
    uint32_t signal_type;           /**< Signal type */
    uint32_t track_bytes;           /**< Track size in bytes */
    uint32_t start_byte;            /**< Starting byte offset */
    uint32_t start_bit;             /**< Starting bit offset */
    uint32_t data_bits;             /**< Data bit count */
    uint32_t gap_bits;              /**< Gap bit count */
    uint32_t track_bits;            /**< Total track bits */
    uint32_t block_count;           /**< Number of data blocks */
    uint32_t encoder_process;       /**< Encoder process */
    uint32_t track_flags;           /**< Track flags */
    uint32_t data_key;              /**< Data record key */
    uint32_t reserved[6];           /**< Reserved */
} uft_ipf_imge_record_t;
#pragma pack(pop)

/**
 * @brief IPF DATA record header (28 bytes after header)
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t size;                  /**< Data size */
    uint32_t crc;                   /**< Data CRC */
    uint32_t key;                   /**< Data key (matches IMGE.data_key) */
    uint32_t reserved[4];           /**< Reserved */
} uft_ipf_data_header_t;
#pragma pack(pop)

/**
 * @brief Parsed IPF information
 */
typedef struct {
    uint32_t encoder_type;          /**< Encoder type */
    uint32_t release;               /**< SPS release */
    uint32_t revision;              /**< SPS revision */
    uint32_t min_cylinder;          /**< Min cylinder */
    uint32_t max_cylinder;          /**< Max cylinder */
    uint32_t min_head;              /**< Min head */
    uint32_t max_head;              /**< Max head */
    uint32_t platforms;             /**< Primary platform */
    uint32_t disk_number;           /**< Disk number */
    uint32_t creation_date;         /**< Creation date */
    uint32_t track_count;           /**< Number of tracks */
    bool has_fuzzy;                 /**< Contains fuzzy data */
    uint8_t sides;                  /**< Detected sides */
    uint8_t cylinders;              /**< Detected cylinders */
} uft_ipf_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Size Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_ipf_block_header_t) == 12, "IPF block header must be 12 bytes");
_Static_assert(sizeof(uft_ipf_info_record_t) == 84, "IPF info record must be 84 bytes");
_Static_assert(sizeof(uft_ipf_imge_record_t) == 80, "IPF imge record must be 80 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Inline Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read big-endian 32-bit value
 * @param data Pointer to data
 * @return Native byte order value
 */
static inline uint32_t uft_ipf_read_be32(const uint8_t* data) {
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) | data[3];
}

/**
 * @brief Write big-endian 32-bit value
 * @param data Output pointer
 * @param value Value to write
 */
static inline void uft_ipf_write_be32(uint8_t* data, uint32_t value) {
    data[0] = (value >> 24) & 0xFF;
    data[1] = (value >> 16) & 0xFF;
    data[2] = (value >> 8) & 0xFF;
    data[3] = value & 0xFF;
}

/**
 * @brief Get platform name
 * @param platform Platform ID
 * @return Platform description
 */
static inline const char* uft_ipf_platform_name(uint32_t platform) {
    switch (platform) {
        case UFT_IPF_PLATFORM_AMIGA:      return "Amiga";
        case UFT_IPF_PLATFORM_ATARI_ST:   return "Atari ST";
        case UFT_IPF_PLATFORM_PC:         return "IBM PC";
        case UFT_IPF_PLATFORM_AMSTRAD:    return "Amstrad CPC";
        case UFT_IPF_PLATFORM_SPECTRUM:   return "ZX Spectrum";
        case UFT_IPF_PLATFORM_SAM:        return "SAM Coupé";
        case UFT_IPF_PLATFORM_ARCHIMEDES: return "Archimedes";
        case UFT_IPF_PLATFORM_C64:        return "Commodore 64";
        case UFT_IPF_PLATFORM_ATARI_8BIT: return "Atari 8-bit";
        default: return "Unknown";
    }
}

/**
 * @brief Get density name
 * @param density Density ID
 * @return Density description
 */
static inline const char* uft_ipf_density_name(uint32_t density) {
    switch (density) {
        case UFT_IPF_DENSITY_NOISE:  return "Unknown/Noise";
        case UFT_IPF_DENSITY_AUTO:   return "Automatic";
        case UFT_IPF_DENSITY_FMSD:   return "FM SD";
        case UFT_IPF_DENSITY_FMDD:   return "FM DD";
        case UFT_IPF_DENSITY_MFMSD:  return "MFM SD";
        case UFT_IPF_DENSITY_MFMDD:  return "MFM DD";
        case UFT_IPF_DENSITY_MFMHD:  return "MFM HD";
        case UFT_IPF_DENSITY_MFMED:  return "MFM ED";
        default: return "Unknown";
    }
}

/**
 * @brief Get encoder name
 * @param encoder Encoder type
 * @return Encoder description
 */
static inline const char* uft_ipf_encoder_name(uint32_t encoder) {
    switch (encoder) {
        case UFT_IPF_ENC_CAPS: return "CAPS";
        case UFT_IPF_ENC_SPS:  return "SPS";
        case UFT_IPF_ENC_CTR:  return "Custom";
        default: return "Unknown";
    }
}

/**
 * @brief Get block type name
 * @param type Block type
 * @return Block type description
 */
static inline const char* uft_ipf_block_type_name(uint32_t type) {
    switch (type) {
        case UFT_IPF_RECORD_CAPS: return "CAPS";
        case UFT_IPF_RECORD_INFO: return "INFO";
        case UFT_IPF_RECORD_IMGE: return "IMGE";
        case UFT_IPF_RECORD_DATA: return "DATA";
        default: return "Unknown";
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CRC Calculation
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Calculate IPF CRC32
 * @param data Data to checksum
 * @param length Data length
 * @return CRC32 value
 */
static inline uint32_t uft_ipf_crc32(const uint8_t* data, size_t length) {
    static const uint32_t table[256] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
        0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
        0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
        0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
        0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
        0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
        0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
        0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
        0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
        0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
        0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
        0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
        0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
        0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
        0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
        0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
        0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
        0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
        0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
        0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
        0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
        0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
        0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
        0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
        0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
        0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
        0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
        0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
        0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
        0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
        0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd706b3,
        0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
    };
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Header Validation and Parsing
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Validate IPF signature
 * @param data File data
 * @param size File size
 * @return true if valid signature
 */
static inline bool uft_ipf_validate_signature(const uint8_t* data, size_t size) {
    if (!data || size < UFT_IPF_BLOCK_HEADER_SIZE) return false;
    
    uint32_t type = uft_ipf_read_be32(data);
    return (type == UFT_IPF_RECORD_CAPS);
}

/**
 * @brief Parse IPF file into info structure
 * @param data File data
 * @param size File size
 * @param info Output info structure
 * @return true if successfully parsed
 */
static inline bool uft_ipf_parse(const uint8_t* data, size_t size,
                                 uft_ipf_info_t* info) {
    if (!data || !info || size < UFT_IPF_BLOCK_HEADER_SIZE) return false;
    if (!uft_ipf_validate_signature(data, size)) return false;
    
    memset(info, 0, sizeof(*info));
    
    size_t offset = 0;
    
    while (offset + UFT_IPF_BLOCK_HEADER_SIZE <= size) {
        uint32_t type = uft_ipf_read_be32(data + offset);
        uint32_t length = uft_ipf_read_be32(data + offset + 4);
        
        if (length < UFT_IPF_BLOCK_HEADER_SIZE) break;
        if (offset + length > size) break;
        
        switch (type) {
            case UFT_IPF_RECORD_INFO: {
                if (length >= UFT_IPF_BLOCK_HEADER_SIZE + 96) {
                    const uint8_t* rec = data + offset + UFT_IPF_BLOCK_HEADER_SIZE;
                    
                    info->encoder_type = uft_ipf_read_be32(rec + 4);
                    info->release = uft_ipf_read_be32(rec + 12);
                    info->revision = uft_ipf_read_be32(rec + 16);
                    info->min_cylinder = uft_ipf_read_be32(rec + 24);
                    info->max_cylinder = uft_ipf_read_be32(rec + 28);
                    info->min_head = uft_ipf_read_be32(rec + 32);
                    info->max_head = uft_ipf_read_be32(rec + 36);
                    info->creation_date = uft_ipf_read_be32(rec + 40);
                    info->platforms = uft_ipf_read_be32(rec + 48);
                    info->disk_number = uft_ipf_read_be32(rec + 64);
                    
                    info->cylinders = info->max_cylinder - info->min_cylinder + 1;
                    info->sides = info->max_head - info->min_head + 1;
                }
                break;
            }
            
            case UFT_IPF_RECORD_IMGE: {
                info->track_count++;
                break;
            }
            
            default:
                break;
        }
        
        offset += length;
    }
    
    return (info->track_count > 0);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Probe and Detection
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Probe data to determine if it's an IPF file
 * @param data File data
 * @param size File size
 * @return Confidence score 0-100
 */
static inline int uft_ipf_probe(const uint8_t* data, size_t size) {
    if (!data || size < UFT_IPF_BLOCK_HEADER_SIZE) return 0;
    
    /* Check CAPS signature */
    if (!uft_ipf_validate_signature(data, size)) return 0;
    
    int score = 60;  /* Base score for CAPS signature */
    
    /* Check block length is reasonable */
    uint32_t length = uft_ipf_read_be32(data + 4);
    if (length >= UFT_IPF_BLOCK_HEADER_SIZE && length < size) {
        score += 15;
    }
    
    /* Look for INFO block */
    if (length < size) {
        size_t next = length;
        if (next + UFT_IPF_BLOCK_HEADER_SIZE <= size) {
            uint32_t next_type = uft_ipf_read_be32(data + next);
            if (next_type == UFT_IPF_RECORD_INFO) {
                score += 25;
            }
        }
    }
    
    return (score > 100) ? 100 : score;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_IPF_FORMAT_H */
