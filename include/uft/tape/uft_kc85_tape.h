/**
 * @file uft_kc85_tape.h
 * @brief KC85/Z1013 Tape Format Support
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * Tape format support for DDR home computers:
 * - KC85/HC900/KC87/Z9001 CAOS tape format
 * - Z1013 tape format
 * - KC Turboloader format
 *
 * Based on hctape by mrhill/Datahammer and KC85FileFormats documentation.
 *
 * Modulation: FSK (Frequency Shift Keying)
 * - Sync tone: 1200 Hz
 * - Bit 0: 2400 Hz (2 waves)
 * - Bit 1: 1200 Hz (1 wave)
 * - Stop bit: 600 Hz (1 wave)
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_KC85_TAPE_H
#define UFT_KC85_TAPE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * KC85 Tape Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief CAOS tape packet size (ID + 128 data + CRC) */
#define UFT_KC85_TAPE_PACKET_SIZE       130

/** @brief Data bytes per packet */
#define UFT_KC85_TAPE_DATA_SIZE         128

/** @brief KCC file header size */
#define UFT_KC85_KCC_HEADER_SIZE        128

/** @brief Tape file header size */
#define UFT_KC85_TAPE_HEADER_SIZE       13

/** @brief First packet ID */
#define UFT_KC85_PACKET_FIRST           0x01

/** @brief Last packet ID */
#define UFT_KC85_PACKET_LAST            0xFF

/** @brief Packet ID wrap value */
#define UFT_KC85_PACKET_WRAP            0xFE

/* Modulation frequencies (Hz) */
#define UFT_KC85_FREQ_SYNC              1200    /**< Sync tone */
#define UFT_KC85_FREQ_BIT0              2400    /**< Bit 0 (2 waves) */
#define UFT_KC85_FREQ_BIT1              1200    /**< Bit 1 (1 wave) */
#define UFT_KC85_FREQ_STOP              600     /**< Stop bit */

/* Standard baud rate */
#define UFT_KC85_BAUD_RATE              1200    /**< ~1200 baud */

/* ═══════════════════════════════════════════════════════════════════════════
 * KC85 File Types
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief KC85 file type enumeration
 */
typedef enum {
    UFT_KC85_FILE_UNKNOWN = 0,
    UFT_KC85_FILE_KCC,          /**< Machine code program (.KCC, .COM) */
    UFT_KC85_FILE_KCB,          /**< HC-BASIC program (.KCB) */
    UFT_KC85_FILE_KCM,          /**< Machine code (alternative) */
    UFT_KC85_FILE_OVR,          /**< Memory overlay/dump (.OVR) */
    UFT_KC85_FILE_SSS,          /**< BASIC tape format (.SSS) */
    UFT_KC85_FILE_TTT,          /**< Text/BASIC tape format (.TTT) */
    UFT_KC85_FILE_TXW,          /**< WordPro text file */
    UFT_KC85_FILE_TAP,          /**< Generic tape image */
    UFT_KC85_FILE_RAW           /**< Raw data */
} uft_kc85_file_type_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * KC85 Tape Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief CAOS tape packet (130 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t packet_id;              /**< Packet ID (0x01-0xFE, 0xFF=last) */
    uint8_t data[128];              /**< Data payload */
    uint8_t checksum;               /**< Sum of all 128 data bytes */
} uft_kc85_tape_packet_t;
#pragma pack(pop)

/**
 * @brief KCC file header (128 bytes) - Machine code programs
 */
#pragma pack(push, 1)
typedef struct {
    char filename[8];               /**< Filename, space-padded */
    char extension[3];              /**< Extension, space-padded */
    uint8_t protection;             /**< Copy protection (0x01 = protected if COM) */
    uint8_t num_args;               /**< Number of valid address arguments */
    uint16_t start_addr;            /**< Memory start address (little endian) */
    uint16_t end_addr;              /**< Memory end address (little endian) */
    uint16_t exec_addr;             /**< Auto-run address (little endian) */
    uint8_t reserved[109];          /**< Unused */
} uft_kc85_kcc_header_t;
#pragma pack(pop)

/**
 * @brief Tape file header (13 bytes) - SSS/TTT format
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t extension[3];           /**< Extension (ASCII + 0x80) */
    char filename[8];               /**< Filename, space-padded */
    uint16_t length;                /**< File length (little endian) */
} uft_kc85_tape_header_t;
#pragma pack(pop)

/**
 * @brief Disk BASIC header (2 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t length;                /**< File length (little endian) */
} uft_kc85_disk_basic_header_t;
#pragma pack(pop)

/**
 * @brief WordPro text file header
 */
#pragma pack(push, 1)
typedef struct {
    char filename[8];               /**< Filename, space-padded */
    char extension[3];              /**< "TXW" */
    uint8_t type_id;                /**< 0x09 */
} uft_kc85_wordpro_header_t;
#pragma pack(pop)

/**
 * @brief Tape file information (parsed)
 */
typedef struct {
    uft_kc85_file_type_t type;
    char filename[12];              /**< Full filename with extension */
    uint16_t start_addr;
    uint16_t end_addr;
    uint16_t exec_addr;
    uint32_t data_size;
    uint32_t total_size;            /**< Including header */
    bool protected;
    bool has_autorun;
    uint8_t num_packets;
} uft_kc85_file_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Z1013 Tape Format
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Z1013 tape header (32 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t type;                   /**< File type */
    uint16_t start_addr;            /**< Start address */
    uint16_t end_addr;              /**< End address */
    uint16_t exec_addr;             /**< Execution address */
    char filename[16];              /**< Filename */
    uint8_t reserved[7];            /**< Reserved */
} uft_z1013_tape_header_t;
#pragma pack(pop)

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_kc85_tape_packet_t) == 130, "Tape packet must be 130 bytes");
_Static_assert(sizeof(uft_kc85_kcc_header_t) == 128, "KCC header must be 128 bytes");
_Static_assert(sizeof(uft_kc85_tape_header_t) == 13, "Tape header must be 13 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get file type name
 */
static inline const char* uft_kc85_file_type_name(uft_kc85_file_type_t type) {
    switch (type) {
        case UFT_KC85_FILE_KCC: return "KCC (Machine Code)";
        case UFT_KC85_FILE_KCB: return "KCB (HC-BASIC)";
        case UFT_KC85_FILE_KCM: return "KCM (Machine Code)";
        case UFT_KC85_FILE_OVR: return "OVR (Memory Dump)";
        case UFT_KC85_FILE_SSS: return "SSS (BASIC Tape)";
        case UFT_KC85_FILE_TTT: return "TTT (Text/BASIC)";
        case UFT_KC85_FILE_TXW: return "TXW (WordPro)";
        case UFT_KC85_FILE_TAP: return "TAP (Tape Image)";
        case UFT_KC85_FILE_RAW: return "RAW (Raw Data)";
        default: return "Unknown";
    }
}

/**
 * @brief Get file extension string
 */
static inline const char* uft_kc85_file_type_ext(uft_kc85_file_type_t type) {
    switch (type) {
        case UFT_KC85_FILE_KCC: return "KCC";
        case UFT_KC85_FILE_KCB: return "KCB";
        case UFT_KC85_FILE_KCM: return "KCM";
        case UFT_KC85_FILE_OVR: return "OVR";
        case UFT_KC85_FILE_SSS: return "SSS";
        case UFT_KC85_FILE_TTT: return "TTT";
        case UFT_KC85_FILE_TXW: return "TXW";
        case UFT_KC85_FILE_TAP: return "TAP";
        default: return "???";
    }
}

/**
 * @brief Calculate packet checksum
 */
static inline uint8_t uft_kc85_calc_checksum(const uint8_t* data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

/**
 * @brief Verify packet checksum
 */
static inline bool uft_kc85_verify_packet(const uft_kc85_tape_packet_t* pkt) {
    if (!pkt) return false;
    return uft_kc85_calc_checksum(pkt->data, 128) == pkt->checksum;
}

/**
 * @brief Extract filename from KCC header
 */
static inline void uft_kc85_get_kcc_filename(const uft_kc85_kcc_header_t* hdr,
                                              char* out, size_t max_len) {
    if (!hdr || !out || max_len < 12) return;
    
    /* Copy and trim filename */
    int i;
    for (i = 0; i < 8 && hdr->filename[i] != ' '; i++) {
        out[i] = hdr->filename[i];
    }
    out[i] = '.';
    i++;
    
    /* Copy extension */
    for (int j = 0; j < 3 && hdr->extension[j] != ' '; j++, i++) {
        out[i] = hdr->extension[j];
    }
    out[i] = '\0';
}

/**
 * @brief Extract filename from tape header
 */
static inline void uft_kc85_get_tape_filename(const uft_kc85_tape_header_t* hdr,
                                               char* out, size_t max_len) {
    if (!hdr || !out || max_len < 12) return;
    
    /* Copy filename */
    int i;
    for (i = 0; i < 8 && hdr->filename[i] != ' '; i++) {
        out[i] = hdr->filename[i];
    }
    out[i] = '.';
    i++;
    
    /* Copy extension (strip high bit) */
    for (int j = 0; j < 3; j++, i++) {
        out[i] = hdr->extension[j] & 0x7F;
    }
    out[i] = '\0';
}

/**
 * @brief Detect file type from extension
 */
static inline uft_kc85_file_type_t uft_kc85_detect_type_ext(const char* ext) {
    if (!ext) return UFT_KC85_FILE_UNKNOWN;
    
    /* Uppercase comparison */
    char uc[4] = {0};
    for (int i = 0; i < 3 && ext[i]; i++) {
        uc[i] = (ext[i] >= 'a' && ext[i] <= 'z') ? ext[i] - 32 : ext[i];
    }
    
    if (strcmp(uc, "KCC") == 0) return UFT_KC85_FILE_KCC;
    if (strcmp(uc, "COM") == 0) return UFT_KC85_FILE_KCC;
    if (strcmp(uc, "KCB") == 0) return UFT_KC85_FILE_KCB;
    if (strcmp(uc, "KCM") == 0) return UFT_KC85_FILE_KCM;
    if (strcmp(uc, "OVR") == 0) return UFT_KC85_FILE_OVR;
    if (strcmp(uc, "SSS") == 0) return UFT_KC85_FILE_SSS;
    if (strcmp(uc, "TTT") == 0) return UFT_KC85_FILE_TTT;
    if (strcmp(uc, "TXW") == 0) return UFT_KC85_FILE_TXW;
    if (strcmp(uc, "TAP") == 0) return UFT_KC85_FILE_TAP;
    
    return UFT_KC85_FILE_UNKNOWN;
}

/**
 * @brief Detect file type from header
 */
static inline uft_kc85_file_type_t uft_kc85_detect_type(const uint8_t* data, size_t size) {
    if (!data || size < 13) return UFT_KC85_FILE_UNKNOWN;
    
    /* Check for KCC header (128 bytes, has valid addresses) */
    if (size >= 128) {
        const uft_kc85_kcc_header_t* kcc = (const uft_kc85_kcc_header_t*)data;
        
        /* Check for valid address arguments */
        if (kcc->num_args >= 2 && kcc->num_args <= 3) {
            uint16_t start = kcc->start_addr;
            uint16_t end = kcc->end_addr;
            
            /* Sanity check addresses */
            if (start < end && end < 0x10000) {
                /* Check extension */
                if (memcmp(kcc->extension, "KCC", 3) == 0 ||
                    memcmp(kcc->extension, "COM", 3) == 0) {
                    return UFT_KC85_FILE_KCC;
                }
                if (memcmp(kcc->extension, "KCB", 3) == 0) {
                    return UFT_KC85_FILE_KCB;
                }
            }
        }
    }
    
    /* Check for tape header (13 bytes with high-bit extension) */
    const uft_kc85_tape_header_t* tape = (const uft_kc85_tape_header_t*)data;
    if ((tape->extension[0] & 0x80) && 
        (tape->extension[1] & 0x80) && 
        (tape->extension[2] & 0x80)) {
        char ext[4] = {
            tape->extension[0] & 0x7F,
            tape->extension[1] & 0x7F,
            tape->extension[2] & 0x7F,
            0
        };
        if (strcmp(ext, "SSS") == 0) return UFT_KC85_FILE_SSS;
        if (strcmp(ext, "TTT") == 0) return UFT_KC85_FILE_TTT;
    }
    
    return UFT_KC85_FILE_UNKNOWN;
}

/**
 * @brief Parse KCC file info
 */
static inline bool uft_kc85_parse_kcc(const uint8_t* data, size_t size,
                                       uft_kc85_file_info_t* info) {
    if (!data || size < 128 || !info) return false;
    
    memset(info, 0, sizeof(*info));
    
    const uft_kc85_kcc_header_t* hdr = (const uft_kc85_kcc_header_t*)data;
    
    uft_kc85_get_kcc_filename(hdr, info->filename, sizeof(info->filename));
    
    info->type = UFT_KC85_FILE_KCC;
    info->start_addr = hdr->start_addr;
    info->end_addr = hdr->end_addr;
    info->exec_addr = hdr->exec_addr;
    info->data_size = (hdr->end_addr > hdr->start_addr) ? 
                      (hdr->end_addr - hdr->start_addr + 1) : 0;
    info->total_size = size;
    info->protected = (hdr->protection == 0x01);
    info->has_autorun = (hdr->num_args >= 3 && hdr->exec_addr != 0);
    info->num_packets = (size + 127) / 128;
    
    return true;
}

/**
 * @brief Parse tape file info
 */
static inline bool uft_kc85_parse_tape(const uint8_t* data, size_t size,
                                        uft_kc85_file_info_t* info) {
    if (!data || size < 13 || !info) return false;
    
    memset(info, 0, sizeof(*info));
    
    const uft_kc85_tape_header_t* hdr = (const uft_kc85_tape_header_t*)data;
    
    uft_kc85_get_tape_filename(hdr, info->filename, sizeof(info->filename));
    
    /* Detect type from extension */
    char ext[4] = {
        hdr->extension[0] & 0x7F,
        hdr->extension[1] & 0x7F,
        hdr->extension[2] & 0x7F,
        0
    };
    info->type = uft_kc85_detect_type_ext(ext);
    
    info->data_size = hdr->length;
    info->total_size = hdr->length + 13;  /* Header + data */
    info->num_packets = (info->total_size + 127) / 128;
    
    return true;
}

/**
 * @brief Probe for KC85 tape format
 * @return Confidence score (0-100)
 */
static inline int uft_kc85_tape_probe(const uint8_t* data, size_t size) {
    if (!data || size < 13) return 0;
    
    int score = 0;
    
    /* Check for KCC format */
    if (size >= 128) {
        const uft_kc85_kcc_header_t* kcc = (const uft_kc85_kcc_header_t*)data;
        
        if (kcc->num_args >= 2 && kcc->num_args <= 3) {
            score += 20;
            
            if (kcc->start_addr < kcc->end_addr) {
                score += 20;
            }
            
            /* Check for valid extension */
            bool valid_ext = true;
            for (int i = 0; i < 3; i++) {
                char c = kcc->extension[i];
                if (c < 0x20 || c > 0x7E) {
                    valid_ext = false;
                    break;
                }
            }
            if (valid_ext) score += 15;
        }
    }
    
    /* Check for tape header with high-bit extension */
    const uft_kc85_tape_header_t* tape = (const uft_kc85_tape_header_t*)data;
    if ((tape->extension[0] & 0x80) && 
        (tape->extension[1] & 0x80) && 
        (tape->extension[2] & 0x80)) {
        score += 30;
        
        /* Check for valid filename characters */
        bool valid_name = true;
        for (int i = 0; i < 8; i++) {
            char c = tape->filename[i];
            if (c < 0x20 || c > 0x7E) {
                valid_name = false;
                break;
            }
        }
        if (valid_name) score += 15;
    }
    
    /* Size check (multiples of 128 are common) */
    if (size % 128 == 0) {
        score += 5;
    }
    
    return (score > 100) ? 100 : score;
}

/**
 * @brief Calculate number of packets needed for data
 */
static inline int uft_kc85_calc_packets(size_t data_size) {
    return (data_size + 127) / 128;
}

/**
 * @brief Get next packet ID
 */
static inline uint8_t uft_kc85_next_packet_id(uint8_t current) {
    if (current == 0) return UFT_KC85_PACKET_FIRST;
    if (current >= UFT_KC85_PACKET_WRAP) return UFT_KC85_PACKET_FIRST;
    return current + 1;
}

/**
 * @brief Print file info
 */
static inline void uft_kc85_print_file_info(const uft_kc85_file_info_t* info) {
    if (!info) return;
    
    printf("KC85 File Information:\n");
    printf("  Filename:   %s\n", info->filename);
    printf("  Type:       %s\n", uft_kc85_file_type_name(info->type));
    printf("  Start Addr: 0x%04X\n", info->start_addr);
    printf("  End Addr:   0x%04X\n", info->end_addr);
    printf("  Exec Addr:  0x%04X\n", info->exec_addr);
    printf("  Data Size:  %u bytes\n", info->data_size);
    printf("  Total Size: %u bytes\n", info->total_size);
    printf("  Protected:  %s\n", info->protected ? "Yes" : "No");
    printf("  Auto-Run:   %s\n", info->has_autorun ? "Yes" : "No");
    printf("  Packets:    %d\n", info->num_packets);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Wave/Audio Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief KC85 tape timing parameters
 */
typedef struct {
    uint32_t sample_rate;           /**< Audio sample rate (e.g., 44100) */
    uint32_t samples_per_bit0;      /**< Samples for bit 0 (2400 Hz) */
    uint32_t samples_per_bit1;      /**< Samples for bit 1 (1200 Hz) */
    uint32_t samples_per_stop;      /**< Samples for stop bit (600 Hz) */
    uint32_t samples_per_sync;      /**< Samples per sync wave (1200 Hz) */
    uint32_t sync_waves;            /**< Number of sync waves before data */
} uft_kc85_tape_timing_t;

/**
 * @brief Initialize tape timing for sample rate
 */
static inline void uft_kc85_init_timing(uft_kc85_tape_timing_t* timing, 
                                         uint32_t sample_rate) {
    if (!timing) return;
    
    timing->sample_rate = sample_rate;
    timing->samples_per_bit0 = sample_rate / UFT_KC85_FREQ_BIT0;  /* 2400 Hz */
    timing->samples_per_bit1 = sample_rate / UFT_KC85_FREQ_BIT1;  /* 1200 Hz */
    timing->samples_per_stop = sample_rate / UFT_KC85_FREQ_STOP;  /* 600 Hz */
    timing->samples_per_sync = sample_rate / UFT_KC85_FREQ_SYNC;  /* 1200 Hz */
    timing->sync_waves = 8000;  /* Long sync before first packet */
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_KC85_TAPE_H */
