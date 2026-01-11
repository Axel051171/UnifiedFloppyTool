/**
 * @file uft_d77_format.h
 * @brief Fujitsu FM-7/FM-77 D77 Disk Image Format
 * 
 * D77 is a D88 variant used by Fujitsu FM-7 series computers.
 */

#ifndef UFT_D77_FORMAT_H
#define UFT_D77_FORMAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UFT_D77_HEADER_SIZE         0x2B0
#define UFT_D77_SECTOR_HEADER_SIZE  16
#define UFT_D77_NAME_SIZE           17
#define UFT_D77_MAX_TRACKS          164

#define UFT_D77_TYPE_2D             0x00
#define UFT_D77_TYPE_2DD            0x10
#define UFT_D77_TYPE_2HD            0x20

#define UFT_D77_FM7_2D_TRACKS       40
#define UFT_D77_FM7_2D_HEADS        2
#define UFT_D77_FM7_2D_SECTORS      16
#define UFT_D77_FM7_2D_SECSIZE      256
#define UFT_D77_FM7_2D_TRACK_SIZE   (16 * 256)
#define UFT_D77_FM7_2D_TOTAL_SIZE   (40 * 2 * 16 * 256)

#define UFT_D77_FM77_2DD_TRACKS     80
#define UFT_D77_FM77_2DD_HEADS      2
#define UFT_D77_FM77_2DD_SECTORS    8
#define UFT_D77_FM77_2DD_SECSIZE    512
#define UFT_D77_FM77_2DD_TRACK_SIZE (8 * 512)
#define UFT_D77_FM77_2DD_TOTAL_SIZE (80 * 2 * 8 * 512)

#define UFT_D77_FM7_1D_TRACKS       40
#define UFT_D77_FM7_1D_HEADS        1
#define UFT_D77_FM7_1D_SECTORS      16
#define UFT_D77_FM7_1D_SECSIZE      256

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
typedef struct __attribute__((packed)) {
    char     disk_name[17];
    uint8_t  reserved1[9];
    uint8_t  write_protect;
    uint8_t  disk_type;
    uint32_t disk_size;
    uint32_t track_offsets[164];
} uft_d77_header_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
typedef struct __attribute__((packed)) {
    uint8_t  cylinder;
    uint8_t  head;
    uint8_t  sector;
    uint8_t  size_code;
    uint16_t sector_count;
    uint8_t  density;
    uint8_t  deleted;
    uint8_t  status;
    uint8_t  reserved[5];
    uint16_t data_size;
} uft_d77_sector_header_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

typedef struct {
    char     name[18];
    uint8_t  disk_type;
    uint32_t disk_size;
    uint32_t track_offsets[164];
    uint8_t  tracks;
    uint8_t  heads;
    uint8_t  sectors_per_track;
    uint16_t sector_size;
    bool     write_protect;
    bool     is_fm7_format;
    bool     is_fm77_format;
    bool     is_valid;
} uft_d77_info_t;

static inline const char* uft_d77_type_name(uint8_t disk_type) {
    switch (disk_type) {
        case UFT_D77_TYPE_2D:  return "2D (320KB)";
        case UFT_D77_TYPE_2DD: return "2DD (640KB)";
        case UFT_D77_TYPE_2HD: return "2HD (1.2MB)";
        default:               return "Unknown";
    }
}

static inline const char* uft_d77_model_name(uint8_t tracks, uint8_t sectors, uint16_t sector_size) {
    if (tracks == 40 && sectors == 16 && sector_size == 256) return "FM-7/FM-77 (2D)";
    if (tracks == 80 && sectors == 8 && sector_size == 512) return "FM-77AV (2DD)";
    if (tracks == 80 && sectors == 16 && sector_size == 256) return "FM-77AV (2DD 256b)";
    return "Unknown Model";
}

static inline uint16_t uft_d77_size_code_to_bytes(uint8_t code) {
    if (code > 6) return 0;
    return (uint16_t)(128 << code);
}

static inline bool uft_d77_validate_header(const uint8_t *header, size_t size) {
    if (!header || size < UFT_D77_HEADER_SIZE) return false;
    const uft_d77_header_t *hdr = (const uft_d77_header_t *)header;
    if (hdr->disk_type != UFT_D77_TYPE_2D && hdr->disk_type != UFT_D77_TYPE_2DD &&
        hdr->disk_type != UFT_D77_TYPE_2HD) return false;
    if (hdr->disk_size < UFT_D77_HEADER_SIZE) return false;
    return true;
}

static inline bool uft_d77_parse(const uint8_t *data, size_t size, uft_d77_info_t *info) {
    if (!data || !info || size < UFT_D77_HEADER_SIZE) return false;
    memset(info, 0, sizeof(uft_d77_info_t));
    const uft_d77_header_t *hdr = (const uft_d77_header_t *)data;
    memcpy(info->name, hdr->disk_name, 17);
    info->name[17] = '\0';
    info->disk_type = hdr->disk_type;
    info->disk_size = hdr->disk_size;
    info->write_protect = (hdr->write_protect != 0);
    memcpy(info->track_offsets, hdr->track_offsets, sizeof(info->track_offsets));
    switch (hdr->disk_type) {
        case UFT_D77_TYPE_2D:
            info->tracks = 40; info->heads = 2; info->sectors_per_track = 16; info->sector_size = 256;
            info->is_fm7_format = true; break;
        case UFT_D77_TYPE_2DD:
            info->tracks = 80; info->heads = 2; info->sectors_per_track = 8; info->sector_size = 512;
            info->is_fm77_format = true; break;
        case UFT_D77_TYPE_2HD:
            info->tracks = 77; info->heads = 2; info->sectors_per_track = 8; info->sector_size = 1024; break;
        default:
            info->tracks = 40; info->heads = 2; info->sectors_per_track = 16; info->sector_size = 256; break;
    }
    info->is_valid = uft_d77_validate_header(data, size);
    return info->is_valid;
}

static inline uint32_t uft_d77_track_offset(const uft_d77_info_t *info, uint8_t track, uint8_t head) {
    if (!info) return 0;
    uint32_t idx = track * 2 + head;
    if (idx >= UFT_D77_MAX_TRACKS) return 0;
    return info->track_offsets[idx];
}

static inline bool uft_d77_is_fm7_compatible(const uft_d77_info_t *info) {
    if (!info || !info->is_valid) return false;
    return (info->tracks == 40 && info->heads == 2 && info->sectors_per_track == 16 && info->sector_size == 256);
}

static inline bool uft_d77_is_fm77_compatible(const uft_d77_info_t *info) {
    if (!info || !info->is_valid) return false;
    return (info->tracks == 80 && info->heads == 2 && info->sectors_per_track == 8 && info->sector_size == 512);
}

static inline double uft_d77_probe(const uint8_t *data, size_t size) {
    if (!data || size < UFT_D77_HEADER_SIZE) return 0.0;
    const uft_d77_header_t *hdr = (const uft_d77_header_t *)data;
    if (hdr->disk_type != UFT_D77_TYPE_2D && hdr->disk_type != UFT_D77_TYPE_2DD &&
        hdr->disk_type != UFT_D77_TYPE_2HD) return 0.0;
    double score = 0.4;
    if (hdr->disk_size >= UFT_D77_HEADER_SIZE && hdr->disk_size <= size) score += 0.2;
    if (hdr->track_offsets[0] == 0 || hdr->track_offsets[0] == UFT_D77_HEADER_SIZE) score += 0.2;
    uint32_t data_size = hdr->disk_size - UFT_D77_HEADER_SIZE;
    if (data_size == UFT_D77_FM7_2D_TOTAL_SIZE || data_size == UFT_D77_FM77_2DD_TOTAL_SIZE) score += 0.15;
    return score > 1.0 ? 1.0 : score;
}

static inline bool uft_d77_create_fm7_2d(uint8_t *header, const char *name) {
    if (!header) return false;
    memset(header, 0, UFT_D77_HEADER_SIZE);
    if (name) {
        size_t len = strlen(name);
        if (len > 16) len = 16;
        memcpy(header, name, len);
    }
    header[0x1B] = UFT_D77_TYPE_2D;
    uint32_t size = UFT_D77_HEADER_SIZE;
    header[0x1C] = (uint8_t)(size & 0xFF);
    header[0x1D] = (uint8_t)((size >> 8) & 0xFF);
    header[0x1E] = (uint8_t)((size >> 16) & 0xFF);
    header[0x1F] = (uint8_t)((size >> 24) & 0xFF);
    return true;
}

static inline bool uft_d77_create_fm77_2dd(uint8_t *header, const char *name) {
    if (!header) return false;
    memset(header, 0, UFT_D77_HEADER_SIZE);
    if (name) {
        size_t len = strlen(name);
        if (len > 16) len = 16;
        memcpy(header, name, len);
    }
    header[0x1B] = UFT_D77_TYPE_2DD;
    uint32_t size = UFT_D77_HEADER_SIZE;
    header[0x1C] = (uint8_t)(size & 0xFF);
    header[0x1D] = (uint8_t)((size >> 8) & 0xFF);
    header[0x1E] = (uint8_t)((size >> 16) & 0xFF);
    header[0x1F] = (uint8_t)((size >> 24) & 0xFF);
    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_D77_FORMAT_H */
