/**
 * @file uft_hfe_format.h
 * @brief HxC Floppy Emulator HFE Format - Complete Implementation
 */

#ifndef UFT_HFE_FORMAT_H
#define UFT_HFE_FORMAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UFT_HFE_SIGNATURE_V1        "HXCPICFE"
#define UFT_HFE_SIGNATURE_V3        "HXCHFEV3"
#define UFT_HFE_SIGNATURE_SIZE      8
#define UFT_HFE_HEADER_SIZE         512
#define UFT_HFE_TRACK_TABLE_OFFSET  512
#define UFT_HFE_BLOCK_SIZE          512
#define UFT_HFE_TRACK_ENTRY_SIZE    4

#define UFT_HFE_ENCODING_ISO_MFM        0x00
#define UFT_HFE_ENCODING_AMIGA_MFM      0x01
#define UFT_HFE_ENCODING_ISO_FM         0x02
#define UFT_HFE_ENCODING_EMU_FM         0x03
#define UFT_HFE_ENCODING_UNKNOWN        0xFF

#define UFT_HFE_IF_IBM_PC_DD            0x00
#define UFT_HFE_IF_IBM_PC_HD            0x01
#define UFT_HFE_IF_ATARI_ST_DD          0x02
#define UFT_HFE_IF_ATARI_ST_HD          0x03
#define UFT_HFE_IF_AMIGA_DD             0x04
#define UFT_HFE_IF_AMIGA_HD             0x05
#define UFT_HFE_IF_CPC_DD               0x06
#define UFT_HFE_IF_GENERIC_SHUGART_DD   0x07
#define UFT_HFE_IF_IBM_PC_ED            0x08
#define UFT_HFE_IF_MSX2_DD              0x09
#define UFT_HFE_IF_C64_DD               0x0A
#define UFT_HFE_IF_EMU_SHUGART          0x0B
#define UFT_HFE_IF_S950_DD              0x0C
#define UFT_HFE_IF_S950_HD              0x0D
#define UFT_HFE_IF_DISABLE              0xFE

#define UFT_HFE_V3_OP_NOP           0xF0
#define UFT_HFE_V3_OP_SETINDEX      0xF1
#define UFT_HFE_V3_OP_SETBITRATE    0xF2
#define UFT_HFE_V3_OP_SKIPBITS      0xF3
#define UFT_HFE_V3_OP_RAND          0xF4

#pragma pack(push, 1)
typedef struct {
    char     signature[8];
    uint8_t  format_revision;
    uint8_t  track_count;
    uint8_t  side_count;
    uint8_t  track_encoding;
    uint16_t bitrate;
    uint16_t rpm;
    uint8_t  interface_mode;
    uint8_t  reserved1;
    uint16_t track_list_offset;
    uint8_t  write_allowed;
    uint8_t  single_step;
    uint8_t  track0s0_altencoding;
    uint8_t  track0s0_encoding;
    uint8_t  track0s1_altencoding;
    uint8_t  track0s1_encoding;
    uint8_t  reserved2[486];
} uft_hfe_header_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint16_t offset;
    uint16_t length;
} uft_hfe_track_entry_t;
#pragma pack(pop)

typedef struct {
    uint8_t     mode;
    const char* name;
    uint16_t    bitrate;
    uint16_t    rpm;
    uint8_t     tracks;
    uint8_t     heads;
    uint32_t    capacity;
} uft_hfe_interface_info_t;

typedef struct {
    uint8_t  version;
    uint8_t  track_count;
    uint8_t  side_count;
    uint8_t  track_encoding;
    uint16_t bitrate;
    uint16_t rpm;
    uint8_t  interface_mode;
    uint16_t track_list_offset;
    bool     write_allowed;
    bool     is_valid;
} uft_hfe_info_t;

static const uft_hfe_interface_info_t uft_hfe_interface_table[] = {
    { UFT_HFE_IF_IBM_PC_DD,         "IBM PC DD",     250, 300, 80, 2, 720*1024 },
    { UFT_HFE_IF_IBM_PC_HD,         "IBM PC HD",     500, 300, 80, 2, 1440*1024 },
    { UFT_HFE_IF_ATARI_ST_DD,       "Atari ST DD",   250, 300, 80, 2, 720*1024 },
    { UFT_HFE_IF_ATARI_ST_HD,       "Atari ST HD",   500, 300, 80, 2, 1440*1024 },
    { UFT_HFE_IF_AMIGA_DD,          "Amiga DD",      250, 300, 80, 2, 880*1024 },
    { UFT_HFE_IF_AMIGA_HD,          "Amiga HD",      500, 300, 80, 2, 1760*1024 },
    { UFT_HFE_IF_CPC_DD,            "CPC DD",        250, 300, 40, 1, 180*1024 },
    { UFT_HFE_IF_GENERIC_SHUGART_DD,"Shugart DD",    250, 300, 80, 2, 720*1024 },
    { UFT_HFE_IF_IBM_PC_ED,         "IBM PC ED",     1000,300, 80, 2, 2880*1024 },
    { UFT_HFE_IF_MSX2_DD,           "MSX2 DD",       250, 300, 80, 2, 720*1024 },
    { UFT_HFE_IF_C64_DD,            "C64 DD",        250, 300, 35, 1, 170*1024 },
    { UFT_HFE_IF_EMU_SHUGART,       "EMU Shugart",   250, 300, 80, 2, 720*1024 },
    { UFT_HFE_IF_S950_DD,           "S950 DD",       250, 300, 80, 2, 800*1024 },
    { UFT_HFE_IF_S950_HD,           "S950 HD",       500, 300, 80, 2, 1600*1024 },
};

#define UFT_HFE_INTERFACE_COUNT (sizeof(uft_hfe_interface_table) / sizeof(uft_hfe_interface_table[0]))

static inline const char* uft_hfe_encoding_name(uint8_t encoding) {
    switch (encoding) {
        case UFT_HFE_ENCODING_ISO_MFM:   return "ISO/IBM MFM";
        case UFT_HFE_ENCODING_AMIGA_MFM: return "Amiga MFM";
        case UFT_HFE_ENCODING_ISO_FM:    return "ISO/IBM FM";
        case UFT_HFE_ENCODING_EMU_FM:    return "EMU FM";
        default:                          return "Unknown";
    }
}

static inline const char* uft_hfe_interface_name(uint8_t mode) {
    for (size_t i = 0; i < UFT_HFE_INTERFACE_COUNT; i++) {
        if (uft_hfe_interface_table[i].mode == mode) return uft_hfe_interface_table[i].name;
    }
    return "Unknown";
}

static inline const uft_hfe_interface_info_t* uft_hfe_interface_info(uint8_t mode) {
    for (size_t i = 0; i < UFT_HFE_INTERFACE_COUNT; i++) {
        if (uft_hfe_interface_table[i].mode == mode) return &uft_hfe_interface_table[i];
    }
    return NULL;
}

static inline int uft_hfe_detect_version(const uint8_t *data, size_t size) {
    if (!data || size < UFT_HFE_SIGNATURE_SIZE) return 0;
    if (memcmp(data, UFT_HFE_SIGNATURE_V1, UFT_HFE_SIGNATURE_SIZE) == 0) return 1;
    if (memcmp(data, UFT_HFE_SIGNATURE_V3, UFT_HFE_SIGNATURE_SIZE) == 0) return 3;
    return 0;
}

static inline bool uft_hfe_validate_header(const uint8_t *header) {
    if (!header) return false;
    int version = uft_hfe_detect_version(header, UFT_HFE_HEADER_SIZE);
    if (version == 0) return false;
    const uft_hfe_header_t *hdr = (const uft_hfe_header_t *)header;
    if (hdr->track_count == 0 || hdr->track_count > 100) return false;
    if (hdr->side_count == 0 || hdr->side_count > 2) return false;
    if (hdr->bitrate < 100 || hdr->bitrate > 1500) return false;
    return true;
}

static inline size_t uft_hfe_track_offset(const uft_hfe_track_entry_t *entry) {
    if (!entry) return 0;
    return (size_t)entry->offset * UFT_HFE_BLOCK_SIZE;
}

static inline uint8_t uft_hfe_reverse_bits(uint8_t byte) {
    byte = ((byte & 0xF0) >> 4) | ((byte & 0x0F) << 4);
    byte = ((byte & 0xCC) >> 2) | ((byte & 0x33) << 2);
    byte = ((byte & 0xAA) >> 1) | ((byte & 0x55) << 1);
    return byte;
}

static inline double uft_hfe_probe(const uint8_t *data, size_t size) {
    if (!data || size < UFT_HFE_HEADER_SIZE) return 0.0;
    int version = uft_hfe_detect_version(data, size);
    if (version == 0) return 0.0;
    double score = 0.5;
    const uft_hfe_header_t *hdr = (const uft_hfe_header_t *)data;
    if (hdr->track_count > 0 && hdr->track_count <= 100) score += 0.15;
    if (hdr->side_count == 1 || hdr->side_count == 2) score += 0.1;
    if (hdr->bitrate >= 100 && hdr->bitrate <= 1500) score += 0.1;
    if (hdr->track_encoding <= UFT_HFE_ENCODING_EMU_FM) score += 0.1;
    if (uft_hfe_interface_info(hdr->interface_mode) != NULL) score += 0.1;
    return score > 1.0 ? 1.0 : score;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_HFE_FORMAT_H */
