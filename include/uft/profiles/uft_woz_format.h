/**
 * @file uft_woz_format.h
 * @brief Apple II WOZ Disk Image Format
 */

#ifndef UFT_WOZ_FORMAT_H
#define UFT_WOZ_FORMAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UFT_WOZ_SIGNATURE_WOZ1      0x315A4F57
#define UFT_WOZ_SIGNATURE_WOZ2      0x325A4F57
#define UFT_WOZ_MAGIC               0x0A0D0AFF
#define UFT_WOZ_HEADER_SIZE         12

#define UFT_WOZ_CHUNK_INFO          0x4F464E49
#define UFT_WOZ_CHUNK_TMAP          0x50414D54
#define UFT_WOZ_CHUNK_TRKS          0x534B5254
#define UFT_WOZ_CHUNK_WRIT          0x54495257
#define UFT_WOZ_CHUNK_META          0x4154454D
#define UFT_WOZ_CHUNK_FLUX          0x58554C46

#define UFT_WOZ_CHUNK_HEADER_SIZE   8
#define UFT_WOZ_INFO_SIZE           60
#define UFT_WOZ_TMAP_SIZE           160
#define UFT_WOZ_V1_TRACK_SIZE       6656
#define UFT_WOZ_EMPTY_TRACK         0xFF
#define UFT_WOZ_MAX_QUARTER_TRACKS  160

#define UFT_WOZ_DISK_525            1
#define UFT_WOZ_DISK_35             2

#define UFT_WOZ_BOOT_UNKNOWN        0
#define UFT_WOZ_BOOT_DOS32          1
#define UFT_WOZ_BOOT_DOS33          2
#define UFT_WOZ_BOOT_PRODOS         3
#define UFT_WOZ_BOOT_PASCAL         4

#define UFT_WOZ_HW_APPLE2           0x0001
#define UFT_WOZ_HW_APPLE2_PLUS      0x0002
#define UFT_WOZ_HW_APPLE2E          0x0004
#define UFT_WOZ_HW_APPLE2C          0x0008
#define UFT_WOZ_HW_APPLE2E_ENH      0x0010
#define UFT_WOZ_HW_APPLE2GS         0x0020
#define UFT_WOZ_HW_APPLE2C_PLUS     0x0040
#define UFT_WOZ_HW_APPLE3           0x0080
#define UFT_WOZ_HW_APPLE3_PLUS      0x0100

#define UFT_WOZ_GCR_ADDR_PROLOGUE_1 0xD5
#define UFT_WOZ_GCR_ADDR_PROLOGUE_2 0xAA
#define UFT_WOZ_GCR_ADDR_PROLOGUE_3 0x96
#define UFT_WOZ_GCR_DATA_PROLOGUE_3 0xAD
#define UFT_WOZ_GCR_EPILOGUE_1      0xDE
#define UFT_WOZ_GCR_EPILOGUE_2      0xAA
#define UFT_WOZ_GCR_EPILOGUE_3      0xEB
#define UFT_WOZ_TIMING_525          32
#define UFT_WOZ_TIMING_35           16

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
typedef struct __attribute__((packed)) {
    uint32_t signature;
    uint32_t magic;
    uint32_t crc32;
} uft_woz_header_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
typedef struct __attribute__((packed)) {
    uint32_t chunk_id;
    uint32_t chunk_size;
} uft_woz_chunk_header_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

/* WOZ INFO chunk - 60 bytes for WOZ2 format */
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
typedef struct __attribute__((packed)) {
    uint8_t  info_version;          /* 1 byte */
    uint8_t  disk_type;             /* 1 byte */
    uint8_t  write_protected;       /* 1 byte */
    uint8_t  synchronized;          /* 1 byte */
    uint8_t  cleaned;               /* 1 byte */
    char     creator[32];           /* 32 bytes */
    uint8_t  disk_sides;            /* 1 byte - WOZ2+ */
    uint8_t  boot_sector_format;    /* 1 byte - WOZ2+ */
    uint8_t  optimal_bit_timing;    /* 1 byte - WOZ2+ */
    uint16_t compatible_hardware;   /* 2 bytes - WOZ2+ */
    uint16_t required_ram;          /* 2 bytes - WOZ2+ */
    uint16_t largest_track;         /* 2 bytes - WOZ2+ */
    uint16_t flux_block;            /* 2 bytes - WOZ2.1+ */
    uint16_t largest_flux_track;    /* 2 bytes - WOZ2.1+ */
    uint8_t  reserved[10];          /* 10 bytes padding to 60 */
} uft_woz_info_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
typedef struct __attribute__((packed)) {
    uint8_t  bitstream[6646];
    uint16_t bytes_used;
    uint16_t bit_count;
    uint16_t splice_point;
    uint8_t  splice_nibble;
    uint8_t  splice_bit_count;
    uint16_t reserved;
} uft_woz_v1_track_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
typedef struct __attribute__((packed)) {
    uint16_t starting_block;
    uint16_t block_count;
    uint32_t bit_count;
} uft_woz_v2_trk_entry_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

static const uint8_t uft_woz_gcr_valid_nibbles[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

static inline int uft_woz_detect_version(const uint8_t *data, size_t size) {
    if (!data || size < UFT_WOZ_HEADER_SIZE) return 0;
    const uft_woz_header_t *hdr = (const uft_woz_header_t *)data;
    if (hdr->magic != UFT_WOZ_MAGIC) return 0;
    if (hdr->signature == UFT_WOZ_SIGNATURE_WOZ1) return 1;
    if (hdr->signature == UFT_WOZ_SIGNATURE_WOZ2) return 2;
    return 0;
}

static inline const char* uft_woz_disk_type_name(uint8_t disk_type) {
    switch (disk_type) {
        case UFT_WOZ_DISK_525: return "5.25\"";
        case UFT_WOZ_DISK_35:  return "3.5\"";
        default:               return "Unknown";
    }
}

static inline const char* uft_woz_boot_format_name(uint8_t format) {
    switch (format) {
        case UFT_WOZ_BOOT_UNKNOWN: return "Unknown";
        case UFT_WOZ_BOOT_DOS32:   return "DOS 3.2 (13-sector)";
        case UFT_WOZ_BOOT_DOS33:   return "DOS 3.3 (16-sector)";
        case UFT_WOZ_BOOT_PRODOS:  return "ProDOS";
        case UFT_WOZ_BOOT_PASCAL:  return "Pascal";
        default:                   return "Unknown";
    }
}

static inline float uft_woz_quarter_track_to_track(uint8_t qtrack) {
    return (float)qtrack / 4.0f;
}

static inline bool uft_woz_is_valid_nibble(uint8_t byte) {
    if (byte == 0xAA || byte == 0xD5) return false;
    for (int i = 0; i < 64; i++) {
        if (uft_woz_gcr_valid_nibbles[i] == byte) return true;
    }
    return false;
}

static inline const uft_woz_chunk_header_t* uft_woz_find_chunk(
    const uint8_t *data, size_t size, uint32_t chunk_id) {
    if (!data || size < UFT_WOZ_HEADER_SIZE + UFT_WOZ_CHUNK_HEADER_SIZE) return NULL;
    size_t offset = UFT_WOZ_HEADER_SIZE;
    while (offset + UFT_WOZ_CHUNK_HEADER_SIZE <= size) {
        const uft_woz_chunk_header_t *chunk = (const uft_woz_chunk_header_t *)(data + offset);
        if (chunk->chunk_id == chunk_id) return chunk;
        offset += UFT_WOZ_CHUNK_HEADER_SIZE + chunk->chunk_size;
        if (chunk->chunk_size == 0 || offset > size) break;
    }
    return NULL;
}

static inline double uft_woz_probe(const uint8_t *data, size_t size) {
    if (!data || size < UFT_WOZ_HEADER_SIZE) return 0.0;
    int version = uft_woz_detect_version(data, size);
    if (version == 0) return 0.0;
    double score = 0.5;
    if (uft_woz_find_chunk(data, size, UFT_WOZ_CHUNK_INFO) != NULL) score += 0.2;
    if (uft_woz_find_chunk(data, size, UFT_WOZ_CHUNK_TMAP) != NULL) score += 0.15;
    if (uft_woz_find_chunk(data, size, UFT_WOZ_CHUNK_TRKS) != NULL) score += 0.15;
    return score > 1.0 ? 1.0 : score;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_WOZ_FORMAT_H */
