/**
 * @file uft_scp_format.h
 * @brief SuperCard Pro (SCP) Image Format Support
 * @version 3.3.0
 *
 * Source: Greaseweazle-master/src/greaseweazle/image/scp.py
 * Spec: https://www.cbmstuff.com/downloads/scp/scp_image_specs.txt
 *
 * Provides:
 * - Complete SCP file header structure
 * - Track header and data handling
 * - Flux data encoding/decoding
 * - Disk type enumeration
 * - Extension block support (WRSP)
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_SCP_FORMAT_H
#define UFT_SCP_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * SCP CONSTANTS
 *============================================================================*/

#define UFT_SCP_SIGNATURE       "SCP"
#define UFT_SCP_TRACK_SIG       "TRK"
#define UFT_SCP_EXT_SIG         "EXTS"
#define UFT_SCP_WRSP_SIG        "WRSP"

#define UFT_SCP_SAMPLE_FREQ     40000000    /* 40 MHz sample rate */
#define UFT_SCP_MAX_TRACKS      168         /* Maximum track entries in TLUT */
#define UFT_SCP_HEADER_SIZE     16          /* File header size */
#define UFT_SCP_TLUT_SIZE       (168 * 4)   /* Track Lookup Table size */
#define UFT_SCP_TLUT_OFFSET     16          /* TLUT starts at offset 16 */
#define UFT_SCP_EXT_OFFSET      0x2B0       /* Extension block offset */

/*============================================================================
 * SCP DISK TYPES
 *============================================================================*/

/**
 * @brief SCP disk type identifiers
 * 
 * Used in the header to identify the disk format
 */
typedef enum {
    /* Commodore */
    UFT_SCP_DISK_C64            = 0x00,
    
    /* Amiga */
    UFT_SCP_DISK_AMIGA          = 0x04,
    UFT_SCP_DISK_AMIGA_HD       = 0x08,
    
    /* Atari 8-bit */
    UFT_SCP_DISK_ATARI800_SD    = 0x10,
    UFT_SCP_DISK_ATARI800_DD    = 0x11,
    UFT_SCP_DISK_ATARI800_ED    = 0x12,
    
    /* Atari ST */
    UFT_SCP_DISK_ATARIST_SS     = 0x14,
    UFT_SCP_DISK_ATARIST_DS     = 0x15,
    
    /* Apple */
    UFT_SCP_DISK_APPLE_II       = 0x20,
    UFT_SCP_DISK_APPLE_II_PRO   = 0x21,
    UFT_SCP_DISK_APPLE_400K     = 0x24,
    UFT_SCP_DISK_APPLE_800K     = 0x25,
    UFT_SCP_DISK_APPLE_1M44     = 0x26,
    
    /* IBM PC */
    UFT_SCP_DISK_IBMPC_360K     = 0x30,
    UFT_SCP_DISK_IBMPC_720K     = 0x31,
    UFT_SCP_DISK_IBMPC_1M2      = 0x32,
    UFT_SCP_DISK_IBMPC_1M44     = 0x33,
    
    /* TRS-80 */
    UFT_SCP_DISK_TRS80_SSSD     = 0x40,
    UFT_SCP_DISK_TRS80_SSDD     = 0x41,
    UFT_SCP_DISK_TRS80_DSSD     = 0x42,
    UFT_SCP_DISK_TRS80_DSDD     = 0x43,
    
    /* Texas Instruments */
    UFT_SCP_DISK_TI994A         = 0x50,
    
    /* Roland */
    UFT_SCP_DISK_ROLAND_D20     = 0x60,
    
    /* Amstrad */
    UFT_SCP_DISK_AMSTRAD_CPC    = 0x70,
    
    /* Other */
    UFT_SCP_DISK_OTHER_320K     = 0x80,
    UFT_SCP_DISK_OTHER_1M2      = 0x81,
    UFT_SCP_DISK_OTHER_720K     = 0x84,
    UFT_SCP_DISK_OTHER_1M44     = 0x85,
    
    /* Tape */
    UFT_SCP_DISK_TAPE_GCR1      = 0xE0,
    UFT_SCP_DISK_TAPE_GCR2      = 0xE1,
    UFT_SCP_DISK_TAPE_MFM       = 0xE2,
    
    /* Hard Disk */
    UFT_SCP_DISK_HDD_MFM        = 0xF0,
    UFT_SCP_DISK_HDD_RLL        = 0xF1
} uft_scp_disk_type_t;

/**
 * @brief Get disk type name
 */
static inline const char* uft_scp_disk_type_name(uft_scp_disk_type_t type) {
    switch (type) {
        case UFT_SCP_DISK_C64:          return "Commodore 64";
        case UFT_SCP_DISK_AMIGA:        return "Amiga DD";
        case UFT_SCP_DISK_AMIGA_HD:     return "Amiga HD";
        case UFT_SCP_DISK_ATARI800_SD:  return "Atari 800 SD";
        case UFT_SCP_DISK_ATARI800_DD:  return "Atari 800 DD";
        case UFT_SCP_DISK_ATARI800_ED:  return "Atari 800 ED";
        case UFT_SCP_DISK_ATARIST_SS:   return "Atari ST SS";
        case UFT_SCP_DISK_ATARIST_DS:   return "Atari ST DS";
        case UFT_SCP_DISK_APPLE_II:     return "Apple II";
        case UFT_SCP_DISK_APPLE_II_PRO: return "Apple II Pro";
        case UFT_SCP_DISK_APPLE_400K:   return "Apple 400K";
        case UFT_SCP_DISK_APPLE_800K:   return "Apple 800K";
        case UFT_SCP_DISK_APPLE_1M44:   return "Apple 1.44M";
        case UFT_SCP_DISK_IBMPC_360K:   return "IBM PC 360K";
        case UFT_SCP_DISK_IBMPC_720K:   return "IBM PC 720K";
        case UFT_SCP_DISK_IBMPC_1M2:    return "IBM PC 1.2M";
        case UFT_SCP_DISK_IBMPC_1M44:   return "IBM PC 1.44M";
        case UFT_SCP_DISK_TRS80_SSSD:   return "TRS-80 SSSD";
        case UFT_SCP_DISK_TRS80_SSDD:   return "TRS-80 SSDD";
        case UFT_SCP_DISK_TRS80_DSSD:   return "TRS-80 DSSD";
        case UFT_SCP_DISK_TRS80_DSDD:   return "TRS-80 DSDD";
        case UFT_SCP_DISK_TI994A:       return "TI-99/4A";
        case UFT_SCP_DISK_ROLAND_D20:   return "Roland D-20";
        case UFT_SCP_DISK_AMSTRAD_CPC:  return "Amstrad CPC";
        case UFT_SCP_DISK_OTHER_320K:   return "Other 320K";
        case UFT_SCP_DISK_OTHER_1M2:    return "Other 1.2M";
        case UFT_SCP_DISK_OTHER_720K:   return "Other 720K";
        case UFT_SCP_DISK_OTHER_1M44:   return "Other 1.44M";
        case UFT_SCP_DISK_TAPE_GCR1:    return "Tape GCR1";
        case UFT_SCP_DISK_TAPE_GCR2:    return "Tape GCR2";
        case UFT_SCP_DISK_TAPE_MFM:     return "Tape MFM";
        case UFT_SCP_DISK_HDD_MFM:      return "HDD MFM";
        case UFT_SCP_DISK_HDD_RLL:      return "HDD RLL";
        default:                        return "Unknown";
    }
}

/*============================================================================
 * SCP HEADER FLAGS
 *============================================================================*/

typedef enum {
    UFT_SCP_FLAG_INDEXED        = (1 << 0),  /* Index-cued tracks */
    UFT_SCP_FLAG_TPI_96         = (1 << 1),  /* 96 TPI drive (else 48 TPI) */
    UFT_SCP_FLAG_RPM_360        = (1 << 2),  /* 360 RPM drive (else 300 RPM) */
    UFT_SCP_FLAG_NORMALISED     = (1 << 3),  /* Flux is normalized */
    UFT_SCP_FLAG_READWRITE      = (1 << 4),  /* Image is R/W capable */
    UFT_SCP_FLAG_FOOTER         = (1 << 5),  /* Contains extension footer */
    UFT_SCP_FLAG_EXTENDED_MODE  = (1 << 6),  /* Extended type for other media */
    UFT_SCP_FLAG_FLUX_CREATOR   = (1 << 7)   /* Not created by SuperCard Pro */
} uft_scp_flags_t;

/*============================================================================
 * SCP FILE HEADER (16 bytes)
 *============================================================================*/

#if defined(_MSC_VER)
    #pragma pack(push, 1)
#endif

typedef struct
#if defined(__GNUC__) || defined(__clang__)
    
#endif
{
    uint8_t  signature[3];      /* 0x00: "SCP" */
    uint8_t  version;           /* 0x03: Version (0x18 = v1.8) */
    uint8_t  disk_type;         /* 0x04: Disk type (see enum) */
    uint8_t  nr_revs;           /* 0x05: Number of revolutions */
    uint8_t  start_track;       /* 0x06: Start track */
    uint8_t  end_track;         /* 0x07: End track */
    uint8_t  flags;             /* 0x08: Flags (see enum) */
    uint8_t  cell_width;        /* 0x09: Bit cell width (0 = default) */
    uint8_t  heads;             /* 0x0A: Number of heads (0=both, 1=head0, 2=head1) */
    uint8_t  resolution;        /* 0x0B: Capture resolution (0 = 25ns) */
    uint32_t checksum;          /* 0x0C: Checksum of data after header */
} uft_scp_header_t;

#if defined(_MSC_VER)
    #pragma pack(pop)
#endif

/*============================================================================
 * SCP TRACK DATA HEADER (TDH) - 12 bytes per revolution
 *============================================================================*/

#if defined(_MSC_VER)
    #pragma pack(push, 1)
#endif

typedef struct
#if defined(__GNUC__) || defined(__clang__)
    
#endif
{
    uint8_t  signature[3];      /* "TRK" */
    uint8_t  track_nr;          /* Track number */
} uft_scp_track_header_t;

typedef struct
#if defined(__GNUC__) || defined(__clang__)
    
#endif
{
    uint32_t index_time;        /* Time for this revolution in SCP ticks */
    uint32_t flux_count;        /* Number of flux entries */
    uint32_t data_offset;       /* Offset to flux data (from track header) */
} uft_scp_revolution_t;

#if defined(_MSC_VER)
    #pragma pack(pop)
#endif

/*============================================================================
 * SCP EXTENSION BLOCKS
 *============================================================================*/

#if defined(_MSC_VER)
    #pragma pack(push, 1)
#endif

typedef struct
#if defined(__GNUC__) || defined(__clang__)
    
#endif
{
    uint8_t  signature[4];      /* "EXTS" */
    uint32_t length;            /* Length of extension area */
} uft_scp_ext_header_t;

typedef struct
#if defined(__GNUC__) || defined(__clang__)
    
#endif
{
    uint8_t  signature[4];      /* Chunk signature (e.g., "WRSP") */
    uint32_t length;            /* Length of chunk data */
} uft_scp_ext_chunk_t;

/* Write Splice extension: contains splice positions for each track */
typedef struct
#if defined(__GNUC__) || defined(__clang__)
    
#endif
{
    uint32_t flags;             /* Reserved (must be 0) */
    uint32_t splice[168];       /* Splice position per track in SCP ticks */
} uft_scp_wrsp_data_t;

#if defined(_MSC_VER)
    #pragma pack(pop)
#endif

/*============================================================================
 * SCP TIMING CONVERSION
 *============================================================================*/

/**
 * @brief Convert SCP ticks to nanoseconds
 * 
 * SCP uses 40MHz clock = 25ns per tick
 */
static inline double uft_scp_ticks_to_ns(uint32_t ticks) {
    return (double)ticks * 1e9 / UFT_SCP_SAMPLE_FREQ;
}

static inline double uft_scp_ticks_to_us(uint32_t ticks) {
    return (double)ticks * 1e6 / UFT_SCP_SAMPLE_FREQ;
}

static inline double uft_scp_ticks_to_ms(uint32_t ticks) {
    return (double)ticks * 1e3 / UFT_SCP_SAMPLE_FREQ;
}

static inline uint32_t uft_scp_ns_to_ticks(double ns) {
    return (uint32_t)(ns * UFT_SCP_SAMPLE_FREQ / 1e9);
}

static inline uint32_t uft_scp_us_to_ticks(double us) {
    return (uint32_t)(us * UFT_SCP_SAMPLE_FREQ / 1e6);
}

/*============================================================================
 * SCP FLUX DATA ENCODING
 *============================================================================*/

/**
 * @brief Decode SCP flux data (16-bit big-endian values)
 * 
 * SCP stores flux as 16-bit big-endian values.
 * Value of 0x0000 means "add 65536 and read next value"
 * This allows encoding arbitrarily long flux times.
 * 
 * @param data Raw flux data
 * @param len Length of data in bytes
 * @param flux_out Output flux array
 * @param max_flux Maximum flux entries
 * @return Number of flux values decoded
 */
static inline size_t uft_scp_decode_flux(const uint8_t *data, size_t len,
                                          uint32_t *flux_out, size_t max_flux) {
    size_t flux_count = 0;
    uint32_t accumulator = 0;
    
    for (size_t i = 0; i + 1 < len && flux_count < max_flux; i += 2) {
        uint16_t value = ((uint16_t)data[i] << 8) | data[i + 1];
        
        if (value == 0) {
            /* Overflow: add 65536 and continue */
            accumulator += 65536;
        } else {
            flux_out[flux_count++] = accumulator + value;
            accumulator = 0;
        }
    }
    
    return flux_count;
}

/**
 * @brief Encode flux data to SCP format
 * 
 * @param flux Input flux array
 * @param flux_count Number of flux values
 * @param data_out Output buffer
 * @param max_len Maximum output length
 * @return Number of bytes written
 */
static inline size_t uft_scp_encode_flux(const uint32_t *flux, size_t flux_count,
                                          uint8_t *data_out, size_t max_len) {
    size_t pos = 0;
    
    for (size_t i = 0; i < flux_count && pos + 2 <= max_len; i++) {
        uint32_t value = flux[i];
        
        /* Handle overflow values */
        while (value > 65535) {
            data_out[pos++] = 0x00;
            data_out[pos++] = 0x00;
            value -= 65536;
            if (pos + 2 > max_len) break;
        }
        
        if (pos + 2 <= max_len) {
            data_out[pos++] = (value >> 8) & 0xFF;
            data_out[pos++] = value & 0xFF;
        }
    }
    
    return pos;
}

/*============================================================================
 * SCP HEADER VALIDATION
 *============================================================================*/

static inline bool uft_scp_validate_header(const uft_scp_header_t *hdr) {
    if (memcmp(hdr->signature, UFT_SCP_SIGNATURE, 3) != 0)
        return false;
    
    if (hdr->nr_revs == 0)
        return false;
    
    if (hdr->end_track < hdr->start_track)
        return false;
    
    return true;
}

/*============================================================================
 * SCP CHECKSUM
 *============================================================================*/

/**
 * @brief Calculate SCP checksum
 * 
 * Simple 32-bit sum of all bytes after the header
 */
static inline uint32_t uft_scp_checksum(const uint8_t *data, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

/*============================================================================
 * SCP GUI PARAMETERS
 *============================================================================*/

typedef struct {
    uft_scp_disk_type_t disk_type;
    uint8_t  revs;              /**< Revolutions to write */
    bool     legacy_ss;         /**< Generate legacy single-sided image */
    bool     index_cued;        /**< Write index-cued data */
    bool     include_wrsp;      /**< Include write splice extension */
} uft_scp_write_params_t;

static inline void uft_scp_write_params_init(uft_scp_write_params_t *p) {
    p->disk_type = UFT_SCP_DISK_OTHER_1M44;
    p->revs = 2;
    p->legacy_ss = false;
    p->index_cued = true;
    p->include_wrsp = true;
}

/*============================================================================
 * SCP TRACK LOOKUP TABLE (TLUT)
 *============================================================================*/

typedef struct {
    uint32_t offsets[UFT_SCP_MAX_TRACKS];   /* Track offsets */
    bool     present[UFT_SCP_MAX_TRACKS];   /* Track presence flags */
    uint8_t  track_count;                   /* Number of tracks present */
} uft_scp_tlut_t;

static inline void uft_scp_tlut_init(uft_scp_tlut_t *tlut) {
    memset(tlut, 0, sizeof(*tlut));
}

static inline void uft_scp_tlut_parse(uft_scp_tlut_t *tlut, 
                                       const uint32_t *raw_offsets) {
    uft_scp_tlut_init(tlut);
    
    for (int i = 0; i < UFT_SCP_MAX_TRACKS; i++) {
        tlut->offsets[i] = raw_offsets[i];
        if (raw_offsets[i] != 0) {
            tlut->present[i] = true;
            tlut->track_count++;
        }
    }
}

/**
 * @brief Get cylinder and side from track number
 */
static inline void uft_scp_track_to_cyl_side(uint8_t track_nr, 
                                              uint8_t *cyl, uint8_t *side) {
    *cyl = track_nr / 2;
    *side = track_nr & 1;
}

static inline uint8_t uft_scp_cyl_side_to_track(uint8_t cyl, uint8_t side) {
    return cyl * 2 + side;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_SCP_FORMAT_H */
