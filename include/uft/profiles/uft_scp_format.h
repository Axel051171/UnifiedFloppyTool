/**
 * @file uft_scp_format.h
 * @brief SCP (SuperCard Pro) format profile - Modern flux-level preservation format
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * SuperCard Pro (SCP) is a flux-level disk imaging format created by Jim Drew
 * for the SuperCard Pro hardware. It captures raw magnetic flux transitions
 * at high resolution, making it ideal for preserving copy-protected disks.
 *
 * Key features:
 * - Flux transition timing at 25ns resolution (40 MHz)
 * - Multiple revolution support for weak bit analysis
 * - Supports all disk types (5.25", 3.5", 8")
 * - Index-to-index track capture
 *
 * Format specification: https://www.cbmstuff.com/downloads/scp/scp_image_specs.txt
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_SCP_FORMAT_H
#define UFT_SCP_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * SCP Format Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief SCP signature "SCP" */
#define UFT_SCP_SIGNATURE           "SCP"

/** @brief SCP signature length */
#define UFT_SCP_SIGNATURE_LEN       3

/** @brief SCP header size */
#define UFT_SCP_HEADER_SIZE         16

/** @brief SCP track header size */
#define UFT_SCP_TRACK_HEADER_SIZE   4

/** @brief SCP revolution header size */
#define UFT_SCP_REV_HEADER_SIZE     12

/** @brief Maximum tracks in SCP file */
#define UFT_SCP_MAX_TRACKS          168

/** @brief SCP timestamp extension signature */
#define UFT_SCP_EXT_SIGNATURE       "EXTS"

/** @brief SCP footer signature */
#define UFT_SCP_FOOTER_SIGNATURE    "FPCS"

/** @brief SCP base clock frequency (40 MHz = 25ns resolution) */
#define UFT_SCP_BASE_CLOCK_HZ       40000000

/** @brief SCP time resolution in nanoseconds */
#define UFT_SCP_TIME_RESOLUTION_NS  25

/* ═══════════════════════════════════════════════════════════════════════════
 * SCP Version Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief SCP version 1.0 */
#define UFT_SCP_VERSION_1_0         0x10

/** @brief SCP version 1.1 */
#define UFT_SCP_VERSION_1_1         0x11

/** @brief SCP version 1.2 */
#define UFT_SCP_VERSION_1_2         0x12

/** @brief SCP version 1.3 */
#define UFT_SCP_VERSION_1_3         0x13

/** @brief SCP version 1.4 */
#define UFT_SCP_VERSION_1_4         0x14

/** @brief SCP version 2.0 */
#define UFT_SCP_VERSION_2_0         0x20

/** @brief SCP version 2.4 (current) */
#define UFT_SCP_VERSION_2_4         0x24

/* ═══════════════════════════════════════════════════════════════════════════
 * SCP Disk Types
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief SCP disk type codes
 */
typedef enum {
    /* Commodore */
    UFT_SCP_DISK_C64            = 0x00,     /**< Commodore 64 */
    UFT_SCP_DISK_C1541          = UFT_SCP_DISK_C64,
    UFT_SCP_DISK_AMIGA          = 0x04,     /**< Amiga */
    
    /* Atari */
    UFT_SCP_DISK_ATARI_FM       = 0x10,     /**< Atari 400/800 FM */
    UFT_SCP_DISK_ATARI_MFM      = 0x11,     /**< Atari 400/800 MFM */
    UFT_SCP_DISK_ATARI_ST       = 0x14,     /**< Atari ST */
    UFT_SCP_DISK_ATARI_TT       = 0x15,     /**< Atari TT */
    
    /* Apple */
    UFT_SCP_DISK_APPLE_II       = 0x20,     /**< Apple II */
    UFT_SCP_DISK_APPLE_IIP      = 0x21,     /**< Apple II+ */
    UFT_SCP_DISK_APPLE_IIE      = 0x22,     /**< Apple IIe */
    UFT_SCP_DISK_APPLE_IIGS     = 0x24,     /**< Apple IIgs */
    UFT_SCP_DISK_MAC_400K       = 0x28,     /**< Mac 400K */
    UFT_SCP_DISK_MAC_800K       = 0x29,     /**< Mac 800K */
    UFT_SCP_DISK_MAC_HD         = 0x2A,     /**< Mac HD */
    
    /* PC */
    UFT_SCP_DISK_PC_360K        = 0x30,     /**< PC 360KB */
    UFT_SCP_DISK_PC_720K        = 0x31,     /**< PC 720KB */
    UFT_SCP_DISK_PC_1200K       = 0x32,     /**< PC 1.2MB */
    UFT_SCP_DISK_PC_1440K       = 0x33,     /**< PC 1.44MB */
    
    /* Tandy */
    UFT_SCP_DISK_TRS80          = 0x40,     /**< TRS-80 */
    UFT_SCP_DISK_TRS80_II       = 0x41,     /**< TRS-80 II */
    
    /* TI */
    UFT_SCP_DISK_TI994A         = 0x50,     /**< TI-99/4A */
    
    /* Roland */
    UFT_SCP_DISK_ROLAND_D50     = 0x60,     /**< Roland D50 */
    
    /* Amstrad */
    UFT_SCP_DISK_AMSTRAD_CPC    = 0x70,     /**< Amstrad CPC */
    
    /* Other */
    UFT_SCP_DISK_OTHER          = 0x80,     /**< Other/Unknown */
    UFT_SCP_DISK_TAPE_GCR       = 0xE0,     /**< Tape GCR stream */
    UFT_SCP_DISK_TAPE_MFM       = 0xE1,     /**< Tape MFM stream */
    UFT_SCP_DISK_HDD_MFM        = 0xF0,     /**< Hard Drive MFM */
    UFT_SCP_DISK_360RPM         = 0xFE,     /**< Flux image - 360 RPM */
    UFT_SCP_DISK_300RPM         = 0xFF      /**< Flux image - 300 RPM */
} uft_scp_disk_type_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * SCP Header Flags
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief SCP header flag bits
 */
#define UFT_SCP_FLAG_INDEX          0x01    /**< Flux data starts at index */
#define UFT_SCP_FLAG_TPI_96         0x02    /**< 96 TPI (vs 48 TPI) */
#define UFT_SCP_FLAG_RPM_360        0x04    /**< 360 RPM (vs 300 RPM) */
#define UFT_SCP_FLAG_NORMALIZED     0x08    /**< Flux data is normalized */
#define UFT_SCP_FLAG_READ_WRITE     0x10    /**< Read/write image (vs read-only) */
#define UFT_SCP_FLAG_FOOTER         0x20    /**< Footer present */
#define UFT_SCP_FLAG_EXTENDED       0x40    /**< Extended mode (non-floppy) */
#define UFT_SCP_FLAG_CREATOR        0x80    /**< Creator info present */

/* ═══════════════════════════════════════════════════════════════════════════
 * SCP Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief SCP file header (16 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t signature[3];       /**< "SCP" signature */
    uint8_t version;            /**< Format version */
    uint8_t disk_type;          /**< Disk type code */
    uint8_t revolutions;        /**< Number of revolutions per track */
    uint8_t start_track;        /**< Starting track number */
    uint8_t end_track;          /**< Ending track number */
    uint8_t flags;              /**< Header flags */
    uint8_t bit_cell_width;     /**< Bit cell width (0=16-bit, non-0=custom) */
    uint8_t heads;              /**< Number of heads (0=both) */
    uint8_t resolution;         /**< Capture resolution (0=25ns) */
    uint32_t checksum;          /**< Data checksum */
} uft_scp_header_t;
#pragma pack(pop)

/**
 * @brief SCP track data header (TDH) entry (4 bytes)
 * 
 * Stored as array after main header, one per track
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t offset;            /**< Offset to track data (0 = no track) */
} uft_scp_track_entry_t;
#pragma pack(pop)

/**
 * @brief SCP track data header
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t signature[3];       /**< "TRK" signature */
    uint8_t track_number;       /**< Track number */
} uft_scp_track_header_t;
#pragma pack(pop)

/**
 * @brief SCP revolution header (12 bytes per revolution)
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t index_time;        /**< Index time (in SCP ticks) */
    uint32_t track_length;      /**< Track length in bits */
    uint32_t data_offset;       /**< Offset to flux data (from TDH) */
} uft_scp_rev_header_t;
#pragma pack(pop)

/**
 * @brief SCP extension header
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t signature[4];       /**< "EXTS" signature */
    uint32_t timestamp_offset;  /**< Offset to timestamp data */
    /* Additional extension data follows */
} uft_scp_extension_t;
#pragma pack(pop)

/**
 * @brief SCP footer
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t manufacturer_offset;  /**< Offset to manufacturer string */
    uint32_t model_offset;         /**< Offset to model string */
    uint32_t serial_offset;        /**< Offset to serial string */
    uint32_t creator_offset;       /**< Offset to creator string */
    uint32_t application_offset;   /**< Offset to application string */
    uint32_t comments_offset;      /**< Offset to comments string */
    uint64_t creation_time;        /**< Creation timestamp */
    uint64_t modification_time;    /**< Modification timestamp */
    uint8_t application_version;   /**< Application version */
    uint8_t scp_version;           /**< SCP hardware version */
    uint8_t scp_revision;          /**< SCP hardware revision */
    uint8_t reserved[5];           /**< Reserved */
    uint8_t signature[4];          /**< "FPCS" signature */
} uft_scp_footer_t;
#pragma pack(pop)

/**
 * @brief Parsed SCP file information
 */
typedef struct {
    uint8_t version;            /**< Format version */
    uint8_t disk_type;          /**< Disk type code */
    uint8_t revolutions;        /**< Revolutions per track */
    uint8_t start_track;        /**< Starting track number */
    uint8_t end_track;          /**< Ending track number */
    uint8_t flags;              /**< Header flags */
    uint8_t heads;              /**< Number of heads */
    uint8_t resolution;         /**< Capture resolution */
    uint32_t checksum;          /**< Data checksum */
    uint32_t track_count;       /**< Number of tracks */
    bool has_footer;            /**< Footer present */
    bool index_aligned;         /**< Flux starts at index */
    bool is_96tpi;              /**< 96 TPI density */
    bool is_360rpm;             /**< 360 RPM rotation */
    bool is_normalized;         /**< Flux is normalized */
    bool is_read_write;         /**< Image is read/write */
    uint32_t capture_time_ns;   /**< Capture resolution in ns */
} uft_scp_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Size Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_scp_header_t) == 16, "SCP header must be 16 bytes");
_Static_assert(sizeof(uft_scp_track_entry_t) == 4, "SCP track entry must be 4 bytes");
_Static_assert(sizeof(uft_scp_track_header_t) == 4, "SCP track header must be 4 bytes");
_Static_assert(sizeof(uft_scp_rev_header_t) == 12, "SCP revolution header must be 12 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Inline Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get capture resolution in nanoseconds
 * @param resolution Resolution byte from header (0 = 25ns)
 * @return Resolution in nanoseconds
 */
static inline uint32_t uft_scp_resolution_ns(uint8_t resolution) {
    return (resolution == 0) ? 25 : resolution * 25;
}

/**
 * @brief Convert SCP ticks to nanoseconds
 * @param ticks SCP tick count
 * @param resolution Resolution byte (0 = 25ns)
 * @return Time in nanoseconds
 */
static inline uint64_t uft_scp_ticks_to_ns(uint32_t ticks, uint8_t resolution) {
    return (uint64_t)ticks * uft_scp_resolution_ns(resolution);
}

/**
 * @brief Convert nanoseconds to SCP ticks
 * @param ns Time in nanoseconds
 * @param resolution Resolution byte (0 = 25ns)
 * @return SCP tick count
 */
static inline uint32_t uft_scp_ns_to_ticks(uint64_t ns, uint8_t resolution) {
    uint32_t res_ns = uft_scp_resolution_ns(resolution);
    return (uint32_t)((ns + res_ns / 2) / res_ns);
}

/**
 * @brief Get version string
 * @param version Version byte
 * @return Version string
 */
static inline const char* uft_scp_version_name(uint8_t version) {
    switch (version) {
        case UFT_SCP_VERSION_1_0: return "1.0";
        case UFT_SCP_VERSION_1_1: return "1.1";
        case UFT_SCP_VERSION_1_2: return "1.2";
        case UFT_SCP_VERSION_1_3: return "1.3";
        case UFT_SCP_VERSION_1_4: return "1.4";
        case UFT_SCP_VERSION_2_0: return "2.0";
        case UFT_SCP_VERSION_2_4: return "2.4";
        default: return "Unknown";
    }
}

/**
 * @brief Get disk type name
 * @param disk_type Disk type code
 * @return Disk type description
 */
static inline const char* uft_scp_disk_type_name(uint8_t disk_type) {
    switch (disk_type) {
        case UFT_SCP_DISK_C64:       return "Commodore 64/1541";
        case UFT_SCP_DISK_AMIGA:     return "Amiga";
        case UFT_SCP_DISK_ATARI_FM:  return "Atari 400/800 FM";
        case UFT_SCP_DISK_ATARI_MFM: return "Atari 400/800 MFM";
        case UFT_SCP_DISK_ATARI_ST:  return "Atari ST";
        case UFT_SCP_DISK_ATARI_TT:  return "Atari TT";
        case UFT_SCP_DISK_APPLE_II:  return "Apple II";
        case UFT_SCP_DISK_APPLE_IIP: return "Apple II+";
        case UFT_SCP_DISK_APPLE_IIE: return "Apple IIe";
        case UFT_SCP_DISK_APPLE_IIGS: return "Apple IIgs";
        case UFT_SCP_DISK_MAC_400K:  return "Macintosh 400K";
        case UFT_SCP_DISK_MAC_800K:  return "Macintosh 800K";
        case UFT_SCP_DISK_MAC_HD:    return "Macintosh HD";
        case UFT_SCP_DISK_PC_360K:   return "PC 360KB";
        case UFT_SCP_DISK_PC_720K:   return "PC 720KB";
        case UFT_SCP_DISK_PC_1200K:  return "PC 1.2MB";
        case UFT_SCP_DISK_PC_1440K:  return "PC 1.44MB";
        case UFT_SCP_DISK_TRS80:     return "TRS-80";
        case UFT_SCP_DISK_TRS80_II:  return "TRS-80 Model II";
        case UFT_SCP_DISK_TI994A:    return "TI-99/4A";
        case UFT_SCP_DISK_ROLAND_D50: return "Roland D50";
        case UFT_SCP_DISK_AMSTRAD_CPC: return "Amstrad CPC";
        case UFT_SCP_DISK_OTHER:     return "Other";
        case UFT_SCP_DISK_360RPM:    return "Flux image (360 RPM)";
        case UFT_SCP_DISK_300RPM:    return "Flux image (300 RPM)";
        default: return "Unknown";
    }
}

/**
 * @brief Describe header flags
 * @param flags Header flags byte
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Pointer to buffer
 */
static inline char* uft_scp_describe_flags(uint8_t flags, char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) return NULL;
    buffer[0] = '\0';
    
    size_t pos = 0;
    if (flags & UFT_SCP_FLAG_INDEX) {
        pos += snprintf(buffer + pos, buffer_size - pos, "Index ");
    }
    if (flags & UFT_SCP_FLAG_TPI_96) {
        pos += snprintf(buffer + pos, buffer_size - pos, "96TPI ");
    }
    if (flags & UFT_SCP_FLAG_RPM_360) {
        pos += snprintf(buffer + pos, buffer_size - pos, "360RPM ");
    }
    if (flags & UFT_SCP_FLAG_NORMALIZED) {
        pos += snprintf(buffer + pos, buffer_size - pos, "Normalized ");
    }
    if (flags & UFT_SCP_FLAG_READ_WRITE) {
        pos += snprintf(buffer + pos, buffer_size - pos, "R/W ");
    }
    if (flags & UFT_SCP_FLAG_FOOTER) {
        pos += snprintf(buffer + pos, buffer_size - pos, "Footer ");
    }
    
    if (pos > 0 && buffer[pos - 1] == ' ') {
        buffer[pos - 1] = '\0';
    } else if (pos == 0) {
        snprintf(buffer, buffer_size, "None");
    }
    
    return buffer;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Header Validation and Parsing
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Validate SCP file signature
 * @param data Pointer to file data
 * @param size Size of data buffer
 * @return true if valid SCP signature
 */
static inline bool uft_scp_validate_signature(const uint8_t* data, size_t size) {
    if (!data || size < UFT_SCP_SIGNATURE_LEN) return false;
    return (memcmp(data, UFT_SCP_SIGNATURE, UFT_SCP_SIGNATURE_LEN) == 0);
}

/**
 * @brief Validate SCP header
 * @param header Pointer to SCP header
 * @return true if header is valid
 */
static inline bool uft_scp_validate_header(const uft_scp_header_t* header) {
    if (!header) return false;
    
    /* Check signature */
    if (memcmp(header->signature, UFT_SCP_SIGNATURE, UFT_SCP_SIGNATURE_LEN) != 0) {
        return false;
    }
    
    /* Check track range */
    if (header->end_track < header->start_track) {
        return false;
    }
    
    /* Check track count is reasonable */
    if ((header->end_track - header->start_track + 1) > UFT_SCP_MAX_TRACKS) {
        return false;
    }
    
    /* Check revolutions */
    if (header->revolutions == 0) {
        return false;
    }
    
    return true;
}

/**
 * @brief Parse SCP header into info structure
 * @param data Pointer to file data
 * @param size Size of data buffer
 * @param info Output info structure
 * @return true if successfully parsed
 */
static inline bool uft_scp_parse_header(const uint8_t* data, size_t size,
                                        uft_scp_info_t* info) {
    if (!data || !info || size < UFT_SCP_HEADER_SIZE) return false;
    
    const uft_scp_header_t* header = (const uft_scp_header_t*)data;
    if (!uft_scp_validate_header(header)) return false;
    
    memset(info, 0, sizeof(*info));
    
    info->version = header->version;
    info->disk_type = header->disk_type;
    info->revolutions = header->revolutions;
    info->start_track = header->start_track;
    info->end_track = header->end_track;
    info->flags = header->flags;
    info->heads = header->heads;
    info->resolution = header->resolution;
    info->checksum = header->checksum;
    info->track_count = header->end_track - header->start_track + 1;
    
    /* Parse flags */
    info->has_footer = (header->flags & UFT_SCP_FLAG_FOOTER) != 0;
    info->index_aligned = (header->flags & UFT_SCP_FLAG_INDEX) != 0;
    info->is_96tpi = (header->flags & UFT_SCP_FLAG_TPI_96) != 0;
    info->is_360rpm = (header->flags & UFT_SCP_FLAG_RPM_360) != 0;
    info->is_normalized = (header->flags & UFT_SCP_FLAG_NORMALIZED) != 0;
    info->is_read_write = (header->flags & UFT_SCP_FLAG_READ_WRITE) != 0;
    
    /* Calculate capture time resolution */
    info->capture_time_ns = uft_scp_resolution_ns(header->resolution);
    
    return true;
}

/**
 * @brief Get track offset from track table
 * @param data File data
 * @param size File size
 * @param track_number Track number (absolute, not relative to start_track)
 * @return Offset to track data, or 0 if not present
 */
static inline uint32_t uft_scp_get_track_offset(const uint8_t* data, size_t size,
                                                uint8_t track_number) {
    if (!data || size < UFT_SCP_HEADER_SIZE) return 0;
    
    const uft_scp_header_t* header = (const uft_scp_header_t*)data;
    
    /* Check if track is in range */
    if (track_number < header->start_track || track_number > header->end_track) {
        return 0;
    }
    
    /* Calculate offset to track entry */
    size_t entry_offset = UFT_SCP_HEADER_SIZE + (track_number * sizeof(uint32_t));
    if (entry_offset + sizeof(uint32_t) > size) {
        return 0;
    }
    
    /* Read track offset (little-endian) */
    uint32_t offset;
    memcpy(&offset, data + entry_offset, sizeof(offset));
    
    return offset;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Probe and Detection
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Probe data to determine if it's an SCP file
 * @param data Pointer to file data
 * @param size Size of data buffer
 * @return Confidence score 0-100 (0 = not SCP, 100 = definitely SCP)
 */
static inline int uft_scp_probe(const uint8_t* data, size_t size) {
    if (!data || size < UFT_SCP_HEADER_SIZE) return 0;
    
    /* Check signature */
    if (!uft_scp_validate_signature(data, size)) return 0;
    
    int score = 50;  /* Base score for signature match */
    
    const uft_scp_header_t* header = (const uft_scp_header_t*)data;
    
    /* Check version */
    if (header->version >= UFT_SCP_VERSION_1_0 && header->version <= UFT_SCP_VERSION_2_4) {
        score += 15;
    }
    
    /* Check track range validity */
    if (header->end_track >= header->start_track &&
        (header->end_track - header->start_track + 1) <= UFT_SCP_MAX_TRACKS) {
        score += 15;
    }
    
    /* Check revolutions */
    if (header->revolutions >= 1 && header->revolutions <= 10) {
        score += 10;
    }
    
    /* Check for valid track offsets */
    size_t expected_min_size = UFT_SCP_HEADER_SIZE + 
                               ((header->end_track + 1) * sizeof(uint32_t));
    if (size >= expected_min_size) {
        score += 10;
    }
    
    return (score > 100) ? 100 : score;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Creation Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize an SCP header
 * @param header Output header structure
 * @param disk_type Disk type code
 * @param start_track Starting track number
 * @param end_track Ending track number
 * @param revolutions Number of revolutions per track
 * @param flags Header flags
 */
static inline void uft_scp_create_header(uft_scp_header_t* header,
                                         uint8_t disk_type,
                                         uint8_t start_track,
                                         uint8_t end_track,
                                         uint8_t revolutions,
                                         uint8_t flags) {
    if (!header) return;
    
    memset(header, 0, sizeof(*header));
    
    memcpy(header->signature, UFT_SCP_SIGNATURE, UFT_SCP_SIGNATURE_LEN);
    header->version = UFT_SCP_VERSION_2_4;
    header->disk_type = disk_type;
    header->revolutions = revolutions;
    header->start_track = start_track;
    header->end_track = end_track;
    header->flags = flags;
    header->bit_cell_width = 0;  /* 16-bit flux values */
    header->heads = 0;           /* Both heads */
    header->resolution = 0;      /* 25ns resolution */
    header->checksum = 0;        /* Calculate after data is added */
}

/**
 * @brief Initialize a revolution header
 * @param rev Output revolution header
 * @param index_time Index pulse timing (SCP ticks)
 * @param track_length Track length in bits
 * @param data_offset Offset to flux data
 */
static inline void uft_scp_create_rev_header(uft_scp_rev_header_t* rev,
                                             uint32_t index_time,
                                             uint32_t track_length,
                                             uint32_t data_offset) {
    if (!rev) return;
    
    rev->index_time = index_time;
    rev->track_length = track_length;
    rev->data_offset = data_offset;
}

/**
 * @brief Calculate expected rotation time in SCP ticks for given RPM
 * @param rpm Rotation speed (typically 300 or 360)
 * @param resolution Resolution byte (0 = 25ns)
 * @return Expected ticks per rotation
 */
static inline uint32_t uft_scp_rotation_ticks(uint32_t rpm, uint8_t resolution) {
    /* Rotation period in nanoseconds */
    uint64_t period_ns = (60ULL * 1000000000ULL) / rpm;
    return uft_scp_ns_to_ticks(period_ns, resolution);
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_SCP_FORMAT_H */
