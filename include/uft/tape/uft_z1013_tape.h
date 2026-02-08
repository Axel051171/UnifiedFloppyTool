/**
 * @file uft_z1013_tape.h
 * @brief Z1013 Tape Format Support
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * Tape format support for the Z1013 hobby computer (DDR, 1985).
 * The Z1013 uses a different modulation scheme than KC85:
 * - Phase modulation instead of FSK
 * - Different timing parameters
 * - 32-byte file header
 *
 * Z1013 Tape Characteristics:
 * - CPU: U880 (Z80 clone) @ 2 MHz
 * - Standard baud rate: ~1000 baud
 * - Modulation: Phase/frequency based
 * - Block size: 32 bytes header + data
 *
 * File types:
 * - Headersave (with 32-byte header)
 * - Headersavez (compressed)
 * - Raw memory dump
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_Z1013_TAPE_H
#define UFT_Z1013_TAPE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Z1013 Tape Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Z1013 header size */
#define UFT_Z1013_HEADER_SIZE           32

/** @brief Z1013 filename length */
#define UFT_Z1013_FILENAME_LEN          16

/** @brief Z1013 block size */
#define UFT_Z1013_BLOCK_SIZE            32

/** @brief Z1013 standard baud rate */
#define UFT_Z1013_BAUD_STANDARD         1000

/** @brief Z1013 fast baud rate */
#define UFT_Z1013_BAUD_FAST             2400

/* Z1013 Modulation frequencies (Hz) - Phase modulation */
#define UFT_Z1013_FREQ_SYNC             2400    /**< Sync/leader tone */
#define UFT_Z1013_FREQ_BIT0             1200    /**< Bit 0 frequency */
#define UFT_Z1013_FREQ_BIT1             2400    /**< Bit 1 frequency */
#define UFT_Z1013_FREQ_STOP             600     /**< Stop/gap */

/** @brief Sync leader duration (ms) */
#define UFT_Z1013_SYNC_DURATION_MS      3000

/** @brief Inter-block gap (ms) */
#define UFT_Z1013_GAP_DURATION_MS       500

/* File type markers */
#define UFT_Z1013_TYPE_BASIC            0x01    /**< BASIC program */
#define UFT_Z1013_TYPE_MACHINE          0x02    /**< Machine code */
#define UFT_Z1013_TYPE_DATA             0x03    /**< Data file */
#define UFT_Z1013_TYPE_SCREEN           0x04    /**< Screen dump */
#define UFT_Z1013_TYPE_HEADERSAVE       0xD3    /**< Headersave format */
#define UFT_Z1013_TYPE_HEADERSAVEZ      0xD4    /**< Headersave compressed */

/* ═══════════════════════════════════════════════════════════════════════════
 * Z1013 File Extensions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Z1013 file type enumeration
 */
typedef enum {
    UFT_Z1013_FILE_UNKNOWN = 0,
    UFT_Z1013_FILE_Z13,             /**< Generic Z1013 file (.Z13) */
    UFT_Z1013_FILE_Z80,             /**< Z80 machine code (.Z80) */
    UFT_Z1013_FILE_BAS,             /**< BASIC program (.BAS) */
    UFT_Z1013_FILE_TXT,             /**< Text file (.TXT) */
    UFT_Z1013_FILE_BIN,             /**< Binary data (.BIN) */
    UFT_Z1013_FILE_SCR,             /**< Screen dump (.SCR) */
    UFT_Z1013_FILE_TAP,             /**< Tape image (.TAP) */
    UFT_Z1013_FILE_P,               /**< ZX81-style P file */
    UFT_Z1013_FILE_RAW              /**< Raw data */
} uft_z1013_file_type_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Z1013 Tape Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Z1013 Headersave header (32 bytes)
 *
 * Standard header format for Z1013 tape files.
 * All addresses are little-endian (Z80 format).
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t type;                   /**< File type marker (0xD3 = Headersave) */
    uint16_t start_addr;            /**< Load address (little endian) */
    uint16_t end_addr;              /**< End address (little endian) */
    uint16_t exec_addr;             /**< Execution address (little endian) */
    uint8_t reserved1;              /**< Reserved/padding */
    char filename[16];              /**< Filename (space-padded) */
    uint8_t reserved2[6];           /**< Reserved/padding */
    uint8_t flags;                  /**< Flags byte */
    uint8_t checksum;               /**< Header checksum */
} uft_z1013_header_t;
#pragma pack(pop)

/**
 * @brief Z1013 block header (for blocked transfers)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t block_num;              /**< Block number (0-255) */
    uint8_t block_type;             /**< Block type (0=data, 1=last) */
    uint16_t data_len;              /**< Data length in this block */
    uint8_t checksum;               /**< Block checksum */
} uft_z1013_block_header_t;
#pragma pack(pop)

/**
 * @brief Z1013 tape file information (parsed)
 */
typedef struct {
    uft_z1013_file_type_t type;
    char filename[17];              /**< Null-terminated filename */
    uint16_t start_addr;
    uint16_t end_addr;
    uint16_t exec_addr;
    uint32_t data_size;
    uint32_t total_size;
    bool has_header;
    bool compressed;
    uint8_t header_type;
    uint8_t block_count;
} uft_z1013_file_info_t;

/**
 * @brief Z1013 tape timing parameters
 */
typedef struct {
    uint32_t sample_rate;           /**< Audio sample rate */
    uint32_t baud_rate;             /**< Effective baud rate */
    uint32_t samples_per_bit0;      /**< Samples for bit 0 */
    uint32_t samples_per_bit1;      /**< Samples for bit 1 */
    uint32_t samples_per_sync;      /**< Samples per sync cycle */
    uint32_t sync_cycles;           /**< Number of sync cycles */
    uint32_t gap_samples;           /**< Samples for inter-block gap */
} uft_z1013_tape_timing_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_z1013_header_t) == 32, "Z1013 header must be 32 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get file type name
 */
static inline const char* uft_z1013_file_type_name(uft_z1013_file_type_t type) {
    switch (type) {
        case UFT_Z1013_FILE_Z13: return "Z13 (Z1013 Generic)";
        case UFT_Z1013_FILE_Z80: return "Z80 (Machine Code)";
        case UFT_Z1013_FILE_BAS: return "BAS (BASIC)";
        case UFT_Z1013_FILE_TXT: return "TXT (Text)";
        case UFT_Z1013_FILE_BIN: return "BIN (Binary)";
        case UFT_Z1013_FILE_SCR: return "SCR (Screen)";
        case UFT_Z1013_FILE_TAP: return "TAP (Tape Image)";
        case UFT_Z1013_FILE_P:   return "P (ZX81 Style)";
        case UFT_Z1013_FILE_RAW: return "RAW (Raw Data)";
        default: return "Unknown";
    }
}

/**
 * @brief Calculate header checksum
 */
static inline uint8_t uft_z1013_calc_checksum(const uint8_t* data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum ^= data[i];  /* XOR checksum */
    }
    return sum;
}

/**
 * @brief Verify header checksum
 */
static inline bool uft_z1013_verify_header(const uft_z1013_header_t* hdr) {
    if (!hdr) return false;
    
    /* Calculate checksum of first 31 bytes */
    uint8_t calc = uft_z1013_calc_checksum((const uint8_t*)hdr, 31);
    return calc == hdr->checksum;
}

/**
 * @brief Extract filename from header
 */
static inline void uft_z1013_get_filename(const uft_z1013_header_t* hdr,
                                           char* out, size_t max_len) {
    if (!hdr || !out || max_len < 17) return;
    
    /* Copy and trim filename */
    int i;
    for (i = 0; i < 16 && hdr->filename[i] != ' ' && hdr->filename[i] != 0; i++) {
        out[i] = hdr->filename[i];
    }
    out[i] = '\0';
}

/**
 * @brief Check if header is valid Headersave format
 */
static inline bool uft_z1013_is_headersave(const uint8_t* data, size_t size) {
    if (!data || size < 32) return false;
    
    const uft_z1013_header_t* hdr = (const uft_z1013_header_t*)data;
    
    /* Check type marker */
    if (hdr->type != UFT_Z1013_TYPE_HEADERSAVE && 
        hdr->type != UFT_Z1013_TYPE_HEADERSAVEZ) {
        return false;
    }
    
    /* Verify addresses are sensible */
    if (hdr->start_addr > hdr->end_addr) return false;
    
    return true;
}

/**
 * @brief Detect Z1013 file type from extension
 */
static inline uft_z1013_file_type_t uft_z1013_detect_type_ext(const char* ext) {
    if (!ext) return UFT_Z1013_FILE_UNKNOWN;
    
    /* Uppercase comparison */
    char uc[4] = {0};
    for (int i = 0; i < 3 && ext[i]; i++) {
        uc[i] = (ext[i] >= 'a' && ext[i] <= 'z') ? ext[i] - 32 : ext[i];
    }
    
    if (strcmp(uc, "Z13") == 0) return UFT_Z1013_FILE_Z13;
    if (strcmp(uc, "Z80") == 0) return UFT_Z1013_FILE_Z80;
    if (strcmp(uc, "BAS") == 0) return UFT_Z1013_FILE_BAS;
    if (strcmp(uc, "TXT") == 0) return UFT_Z1013_FILE_TXT;
    if (strcmp(uc, "BIN") == 0) return UFT_Z1013_FILE_BIN;
    if (strcmp(uc, "SCR") == 0) return UFT_Z1013_FILE_SCR;
    if (strcmp(uc, "TAP") == 0) return UFT_Z1013_FILE_TAP;
    if (strcmp(uc, "P") == 0)   return UFT_Z1013_FILE_P;
    
    return UFT_Z1013_FILE_UNKNOWN;
}

/**
 * @brief Parse Z1013 file header
 */
static inline bool uft_z1013_parse_header(const uint8_t* data, size_t size,
                                           uft_z1013_file_info_t* info) {
    if (!data || size < 32 || !info) return false;
    
    memset(info, 0, sizeof(*info));
    
    const uft_z1013_header_t* hdr = (const uft_z1013_header_t*)data;
    
    uft_z1013_get_filename(hdr, info->filename, sizeof(info->filename));
    
    info->header_type = hdr->type;
    info->start_addr = hdr->start_addr;
    info->end_addr = hdr->end_addr;
    info->exec_addr = hdr->exec_addr;
    info->has_header = true;
    info->compressed = (hdr->type == UFT_Z1013_TYPE_HEADERSAVEZ);
    
    if (hdr->end_addr > hdr->start_addr) {
        info->data_size = hdr->end_addr - hdr->start_addr + 1;
    }
    info->total_size = size;
    
    /* Detect file type from header type */
    switch (hdr->type) {
        case UFT_Z1013_TYPE_BASIC:
            info->type = UFT_Z1013_FILE_BAS;
            break;
        case UFT_Z1013_TYPE_MACHINE:
        case UFT_Z1013_TYPE_HEADERSAVE:
        case UFT_Z1013_TYPE_HEADERSAVEZ:
            info->type = UFT_Z1013_FILE_Z80;
            break;
        case UFT_Z1013_TYPE_SCREEN:
            info->type = UFT_Z1013_FILE_SCR;
            break;
        default:
            info->type = UFT_Z1013_FILE_BIN;
            break;
    }
    
    return true;
}

/**
 * @brief Probe for Z1013 tape format
 * @return Confidence score (0-100)
 */
static inline int uft_z1013_tape_probe(const uint8_t* data, size_t size) {
    if (!data || size < 32) return 0;
    
    int score = 0;
    
    const uft_z1013_header_t* hdr = (const uft_z1013_header_t*)data;
    
    /* Check for Headersave type marker */
    if (hdr->type == UFT_Z1013_TYPE_HEADERSAVE ||
        hdr->type == UFT_Z1013_TYPE_HEADERSAVEZ) {
        score += 40;
    } else if (hdr->type >= 0x01 && hdr->type <= 0x04) {
        score += 20;
    }
    
    /* Check address validity */
    if (hdr->start_addr < hdr->end_addr && hdr->end_addr < 0x10000) {
        score += 20;
        
        /* Common Z1013 address ranges */
        if (hdr->start_addr >= 0x0100 && hdr->start_addr < 0xEC00) {
            score += 10;
        }
    }
    
    /* Check filename (printable ASCII) */
    bool valid_name = true;
    for (int i = 0; i < 16; i++) {
        char c = hdr->filename[i];
        if (c != 0 && c != ' ' && (c < 0x20 || c > 0x7E)) {
            valid_name = false;
            break;
        }
    }
    if (valid_name) score += 15;
    
    /* Verify checksum */
    if (uft_z1013_verify_header(hdr)) {
        score += 15;
    }
    
    return (score > 100) ? 100 : score;
}

/**
 * @brief Initialize Z1013 tape timing
 */
static inline void uft_z1013_init_timing(uft_z1013_tape_timing_t* timing,
                                          uint32_t sample_rate,
                                          uint32_t baud_rate) {
    if (!timing) return;
    
    timing->sample_rate = sample_rate;
    timing->baud_rate = baud_rate;
    
    /* Calculate samples per bit based on baud rate */
    timing->samples_per_bit0 = sample_rate / UFT_Z1013_FREQ_BIT0;
    timing->samples_per_bit1 = sample_rate / UFT_Z1013_FREQ_BIT1;
    timing->samples_per_sync = sample_rate / UFT_Z1013_FREQ_SYNC;
    
    /* Sync leader: ~3 seconds of 2400 Hz */
    timing->sync_cycles = (UFT_Z1013_SYNC_DURATION_MS * UFT_Z1013_FREQ_SYNC) / 1000;
    
    /* Inter-block gap */
    timing->gap_samples = (UFT_Z1013_GAP_DURATION_MS * sample_rate) / 1000;
}

/**
 * @brief Print file info
 */
static inline void uft_z1013_print_file_info(const uft_z1013_file_info_t* info) {
    if (!info) return;
    
    printf("Z1013 File Information:\n");
    printf("  Filename:   %s\n", info->filename);
    printf("  Type:       %s\n", uft_z1013_file_type_name(info->type));
    printf("  Header:     0x%02X\n", info->header_type);
    printf("  Start Addr: 0x%04X\n", info->start_addr);
    printf("  End Addr:   0x%04X\n", info->end_addr);
    printf("  Exec Addr:  0x%04X\n", info->exec_addr);
    printf("  Data Size:  %u bytes\n", info->data_size);
    printf("  Compressed: %s\n", info->compressed ? "Yes" : "No");
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_Z1013_TAPE_H */
