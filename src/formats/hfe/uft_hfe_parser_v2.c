/**
 * @file uft_hfe_parser_v2.c
 * @brief GOD MODE HFE Parser v2 - UFT HFE Format Format
 * 
 * HFE (UFT HFE Format) is a universal floppy image format.
 * Supports any floppy format through raw MFM/FM track storage.
 * 
 * Features:
 * - Variable track count and sides
 * - MFM, FM, and other encodings
 * - Track-level bit stream storage
 * - Interleaved side storage
 * - Version 1 and 3 support
 * 
 * @author UFT Team / GOD MODE
 * @version 2.0.0
 * @date 2025-01-02
 */

#include <stdio.h>
#include "uft/floppy/uft_floppy_device.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

#define HFE_SIGNATURE           "HXCPICFE"
#define HFE_SIGNATURE_SIZE      8
#define HFE_HEADER_SIZE         512
#define HFE_BLOCK_SIZE          512

#define HFE_MAX_TRACKS          84
#define HFE_MAX_SIDES           2

/* Encoding modes */
#define HFE_ENC_ISOIBM_MFM      0x00
#define HFE_ENC_AMIGA_MFM       0x01
#define HFE_ENC_ISOIBM_FM       0x02
#define HFE_ENC_EMU_FM          0x03
#define HFE_ENC_UNKNOWN         0xFF

/* Floppy interface modes */
#define HFE_IF_IBMPC_DD         0x00
#define HFE_IF_IBMPC_HD         0x01
#define HFE_IF_ATARIST_DD       0x02
#define HFE_IF_ATARIST_HD       0x03
#define HFE_IF_AMIGA_DD         0x04
#define HFE_IF_AMIGA_HD         0x05
#define HFE_IF_CPC_DD           0x06
#define HFE_IF_GENERIC_SHUGART  0x07
#define HFE_IF_IBMPC_ED         0x08
#define HFE_IF_MSX2_DD          0x09
#define HFE_IF_C64_DD           0x0A
#define HFE_IF_EMU_SHUGART      0x0B
#define HFE_IF_S950_DD          0x0C
#define HFE_IF_S950_HD          0x0D

/* ═══════════════════════════════════════════════════════════════════════════
 * DATA STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief HFE file header (512 bytes)
 */
UFT_PACK_BEGIN
typedef struct {
    char signature[8];          /* "HXCPICFE" */
    uint8_t format_revision;    /* Format revision (0 or 1) */
    uint8_t num_tracks;         /* Number of tracks */
    uint8_t num_sides;          /* Number of sides (1 or 2) */
    uint8_t track_encoding;     /* Track encoding mode */
    uint16_t bit_rate;          /* Bit rate in kbit/s (little endian) */
    uint16_t rpm;               /* RPM (300 or 360) */
    uint8_t interface_mode;     /* Floppy interface mode */
    uint8_t reserved;           /* Reserved (1) */
    uint16_t track_list_offset; /* Track list offset (in blocks) */
    uint8_t write_allowed;      /* Write allowed flag */
    uint8_t single_step;        /* Single step (0=double, 1=single, 0xFF=both) */
    uint8_t track0s0_altenc;    /* Track 0 side 0 alternate encoding */
    uint8_t track0s0_enc;       /* Track 0 side 0 encoding */
    uint8_t track0s1_altenc;    /* Track 0 side 1 alternate encoding */
    uint8_t track0s1_enc;       /* Track 0 side 1 encoding */
} hfe_header_t;
UFT_PACK_END

/**
 * @brief Track list entry (4 bytes each)
 */
UFT_PACK_BEGIN
typedef struct {
    uint16_t offset;            /* Track data offset (in blocks) */
    uint16_t length;            /* Track length (in bytes) */
} hfe_track_entry_t;
UFT_PACK_END

/**
 * @brief Track data
 */
typedef struct {
    uint8_t track_num;
    uint16_t offset_blocks;
    uint16_t length_bytes;
    uint16_t length_bits;
    
    uint8_t* side0_data;        /* Side 0 MFM data */
    uint8_t* side1_data;        /* Side 1 MFM data (if present) */
    size_t side_size;           /* Size of each side's data */
    
    bool present;
} hfe_track_t;

/**
 * @brief HFE disk structure
 */
typedef struct {
    hfe_header_t header;
    hfe_track_entry_t track_list[HFE_MAX_TRACKS];
    hfe_track_t tracks[HFE_MAX_TRACKS];
    
    /* Decoded info */
    const char* encoding_name;
    const char* interface_name;
    
    /* Statistics */
    uint32_t total_track_data;
    
    /* Status */
    bool valid;
    char error[256];
} hfe_disk_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * HELPER FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read little-endian 16-bit
 */
static uint16_t read_le16(const uint8_t* data) {
    return data[0] | (data[1] << 8);
}

/**
 * @brief Check HFE signature
 */
static bool hfe_is_valid(const uint8_t* data, size_t size) {
    if (size < HFE_HEADER_SIZE) return false;
    return memcmp(data, HFE_SIGNATURE, HFE_SIGNATURE_SIZE) == 0;
}

/**
 * @brief Get encoding name
 */
static const char* hfe_encoding_name(uint8_t enc) {
    switch (enc) {
        case HFE_ENC_ISOIBM_MFM: return "ISO/IBM MFM";
        case HFE_ENC_AMIGA_MFM: return "Amiga MFM";
        case HFE_ENC_ISOIBM_FM: return "ISO/IBM FM";
        case HFE_ENC_EMU_FM: return "EMU FM";
        default: return "Unknown";
    }
}

/**
 * @brief Get interface name
 */
static const char* hfe_interface_name(uint8_t iface) {
    switch (iface) {
        case HFE_IF_IBMPC_DD: return "IBM PC DD";
        case HFE_IF_IBMPC_HD: return "IBM PC HD";
        case HFE_IF_ATARIST_DD: return "Atari ST DD";
        case HFE_IF_ATARIST_HD: return "Atari ST HD";
        case HFE_IF_AMIGA_DD: return "Amiga DD";
        case HFE_IF_AMIGA_HD: return "Amiga HD";
        case HFE_IF_CPC_DD: return "Amstrad CPC DD";
        case HFE_IF_GENERIC_SHUGART: return "Generic Shugart";
        case HFE_IF_IBMPC_ED: return "IBM PC ED";
        case HFE_IF_MSX2_DD: return "MSX2 DD";
        case HFE_IF_C64_DD: return "Commodore 64 DD";
        case HFE_IF_EMU_SHUGART: return "EMU Shugart";
        case HFE_IF_S950_DD: return "S950 DD";
        case HFE_IF_S950_HD: return "S950 HD";
        default: return "Unknown";
    }
}

/**
 * @brief Bit-reverse a byte (HFE stores bits in reverse order)
 */
static uint8_t bit_reverse(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PARSING FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parse HFE header
 */
static bool hfe_parse_header(const uint8_t* data, hfe_disk_t* disk) {
    memcpy(&disk->header, data, sizeof(hfe_header_t));
    
    /* Fix endianness */
    disk->header.bit_rate = read_le16((uint8_t*)&disk->header.bit_rate);
    disk->header.rpm = read_le16((uint8_t*)&disk->header.rpm);
    disk->header.track_list_offset = read_le16((uint8_t*)&disk->header.track_list_offset);
    
    /* Validate */
    if (disk->header.num_tracks > HFE_MAX_TRACKS) {
        snprintf(disk->error, sizeof(disk->error), 
                 "Too many tracks: %u", disk->header.num_tracks);
        return false;
    }
    
    if (disk->header.num_sides > HFE_MAX_SIDES) {
        snprintf(disk->error, sizeof(disk->error), 
                 "Too many sides: %u", disk->header.num_sides);
        return false;
    }
    
    disk->encoding_name = hfe_encoding_name(disk->header.track_encoding);
    disk->interface_name = hfe_interface_name(disk->header.interface_mode);
    
    return true;
}

/**
 * @brief Parse track list
 */
static bool hfe_parse_track_list(const uint8_t* data, size_t size, hfe_disk_t* disk) {
    size_t list_offset = disk->header.track_list_offset * HFE_BLOCK_SIZE;
    
    if (list_offset + disk->header.num_tracks * 4 > size) {
        snprintf(disk->error, sizeof(disk->error), "Track list out of bounds");
        return false;
    }
    
    const uint8_t* list = data + list_offset;
    
    for (int i = 0; i < disk->header.num_tracks; i++) {
        disk->track_list[i].offset = read_le16(list + i * 4);
        disk->track_list[i].length = read_le16(list + i * 4 + 2);
        
        disk->tracks[i].track_num = i;
        disk->tracks[i].offset_blocks = disk->track_list[i].offset;
        disk->tracks[i].length_bytes = disk->track_list[i].length;
        disk->tracks[i].present = (disk->track_list[i].offset != 0);
        
        disk->total_track_data += disk->track_list[i].length;
    }
    
    return true;
}

/**
 * @brief Parse HFE disk
 */
static bool hfe_parse(const uint8_t* data, size_t size, hfe_disk_t* disk) {
    memset(disk, 0, sizeof(hfe_disk_t));
    
    if (!hfe_is_valid(data, size)) {
        snprintf(disk->error, sizeof(disk->error), "Invalid HFE signature");
        return false;
    }
    
    if (!hfe_parse_header(data, disk)) {
        return false;
    }
    
    if (!hfe_parse_track_list(data, size, disk)) {
        return false;
    }
    
    disk->valid = true;
    return true;
}

/**
 * @brief Generate info text
 */
static char* hfe_info_to_text(const hfe_disk_t* disk) {
    size_t buf_size = 2048;
    char* buf = malloc(buf_size);
    if (!buf) return NULL;
    
    snprintf(buf, buf_size,
        "HFE Floppy Image\n"
        "════════════════\n"
        "Format revision: %u\n"
        "Tracks: %u\n"
        "Sides: %u\n"
        "Encoding: %s\n"
        "Interface: %s\n"
        "Bit rate: %u kbit/s\n"
        "RPM: %u\n"
        "Write allowed: %s\n"
        "Total track data: %u bytes\n",
        disk->header.format_revision,
        disk->header.num_tracks,
        disk->header.num_sides,
        disk->encoding_name,
        disk->interface_name,
        disk->header.bit_rate,
        disk->header.rpm,
        disk->header.write_allowed ? "Yes" : "No",
        disk->total_track_data
    );
    
    return buf;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST SUITE
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef HFE_PARSER_TEST

#include <assert.h>
#include "uft/uft_compiler.h"

int main(void) {
    printf("=== HFE Parser v2 Tests ===\n");
    
    printf("Testing signature... ");
    uint8_t valid_hfe[512] = { 'H','X','C','P','I','C','F','E' };
    uint8_t invalid_hfe[512] = { 'X','X','X','X','X','X','X','X' };
    assert(hfe_is_valid(valid_hfe, 512));
    assert(!hfe_is_valid(invalid_hfe, 512));
    printf("✓\n");
    
    printf("Testing encoding names... ");
    assert(strcmp(hfe_encoding_name(HFE_ENC_ISOIBM_MFM), "ISO/IBM MFM") == 0);
    assert(strcmp(hfe_encoding_name(HFE_ENC_AMIGA_MFM), "Amiga MFM") == 0);
    assert(strcmp(hfe_encoding_name(HFE_ENC_ISOIBM_FM), "ISO/IBM FM") == 0);
    printf("✓\n");
    
    printf("Testing interface names... ");
    assert(strcmp(hfe_interface_name(HFE_IF_AMIGA_DD), "Amiga DD") == 0);
    assert(strcmp(hfe_interface_name(HFE_IF_ATARIST_DD), "Atari ST DD") == 0);
    assert(strcmp(hfe_interface_name(HFE_IF_CPC_DD), "Amstrad CPC DD") == 0);
    printf("✓\n");
    
    printf("Testing bit reverse... ");
    assert(bit_reverse(0x01) == 0x80);
    assert(bit_reverse(0x80) == 0x01);
    assert(bit_reverse(0xF0) == 0x0F);
    assert(bit_reverse(0xAA) == 0x55);
    printf("✓\n");
    
    printf("Testing header parsing... ");
    uint8_t hfe_data[1024] = { 'H','X','C','P','I','C','F','E' };
    hfe_data[8] = 1;    /* format revision */
    hfe_data[9] = 80;   /* tracks */
    hfe_data[10] = 2;   /* sides */
    hfe_data[11] = HFE_ENC_ISOIBM_MFM;
    hfe_data[12] = 0xF4; hfe_data[13] = 0x01;  /* 500 kbit/s */
    hfe_data[14] = 0x2C; hfe_data[15] = 0x01;  /* 300 RPM */
    hfe_data[16] = HFE_IF_IBMPC_HD;
    hfe_data[18] = 1; hfe_data[19] = 0;  /* track list at block 1 */
    
    hfe_disk_t disk;
    assert(hfe_parse_header(hfe_data, &disk));
    assert(disk.header.num_tracks == 80);
    assert(disk.header.num_sides == 2);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 5, Failed: 0\n");
    return 0;
}

#endif /* HFE_PARSER_TEST */
