/**
 * @file uft_d88_format.h
 * @brief NEC PC-88/PC-98 D88 Disk Image Format
 */

#ifndef UFT_D88_FORMAT_H
#define UFT_D88_FORMAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UFT_D88_HEADER_SIZE         0x2B0
#define UFT_D88_SECTOR_HEADER_SIZE  16
#define UFT_D88_NAME_SIZE           17
#define UFT_D88_MAX_TRACKS          164
#define UFT_D88_TRACK_TABLE_SIZE    656

#define UFT_D88_TYPE_2D             0x00
#define UFT_D88_TYPE_2DD            0x10
#define UFT_D88_TYPE_2HD            0x20
#define UFT_D88_TYPE_1D             0x30
#define UFT_D88_TYPE_1DD            0x40

#define UFT_D88_DENSITY_MFM         0x00
#define UFT_D88_DENSITY_FM          0x40

#define UFT_D88_STATUS_NORMAL       0x00
#define UFT_D88_STATUS_DELETED      0x10
#define UFT_D88_STATUS_CRC_ERR_DAT  0xA0
#define UFT_D88_STATUS_CRC_ERR_HDR  0xB0
#define UFT_D88_STATUS_NO_DAM       0xE0
#define UFT_D88_STATUS_NO_DATA      0xF0

#define UFT_D88_PC98_2HD_TRACKS     77
#define UFT_D88_PC98_2HD_HEADS      2
#define UFT_D88_PC98_2HD_SECTORS    8
#define UFT_D88_PC98_2HD_SECSIZE    1024
#define UFT_D88_PC98_2HD_SIZE       1261568

#define UFT_D88_PC98_2DD_TRACKS     80
#define UFT_D88_PC98_2DD_HEADS      2
#define UFT_D88_PC98_2DD_SECTORS    8
#define UFT_D88_PC98_2DD_SECSIZE    512
#define UFT_D88_PC98_2DD_SIZE       655360

#define UFT_D88_PC88_2D_TRACKS      40
#define UFT_D88_PC88_2D_HEADS       2
#define UFT_D88_PC88_2D_SECTORS     16
#define UFT_D88_PC88_2D_SECSIZE     256
#define UFT_D88_PC88_2D_SIZE        327680

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
} uft_d88_header_t;
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
} uft_d88_sector_header_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

typedef struct {
    char     name[18];
    uint8_t  disk_type;
    uint8_t  density;
    uint32_t disk_size;
    uint32_t track_offsets[164];
    uint8_t  tracks;
    uint8_t  heads;
    uint8_t  sectors_per_track;
    uint16_t sector_size;
    bool     write_protect;
    bool     is_valid;
} uft_d88_info_t;

static inline const char* uft_d88_type_name(uint8_t disk_type) {
    switch (disk_type) {
        case UFT_D88_TYPE_2D:  return "2D (320KB)";
        case UFT_D88_TYPE_2DD: return "2DD (640KB)";
        case UFT_D88_TYPE_2HD: return "2HD (1.2MB)";
        case UFT_D88_TYPE_1D:  return "1D (160KB)";
        case UFT_D88_TYPE_1DD: return "1DD (320KB)";
        default:               return "Unknown";
    }
}

static inline const char* uft_d88_density_name(uint8_t density) {
    return (density == UFT_D88_DENSITY_FM) ? "FM" : "MFM";
}

static inline const char* uft_d88_status_name(uint8_t status) {
    switch (status) {
        case UFT_D88_STATUS_NORMAL:      return "Normal";
        case UFT_D88_STATUS_DELETED:     return "Deleted";
        case UFT_D88_STATUS_CRC_ERR_DAT: return "CRC Error (Data)";
        case UFT_D88_STATUS_CRC_ERR_HDR: return "CRC Error (Header)";
        case UFT_D88_STATUS_NO_DAM:      return "No Data AM";
        case UFT_D88_STATUS_NO_DATA:     return "No Data";
        default:                          return "Unknown";
    }
}

static inline uint16_t uft_d88_size_code_to_bytes(uint8_t code) {
    if (code > 6) return 0;
    return (uint16_t)(128 << code);
}

static inline uint8_t uft_d88_bytes_to_size_code(uint16_t bytes) {
    switch (bytes) {
        case 128:  return 0;
        case 256:  return 1;
        case 512:  return 2;
        case 1024: return 3;
        case 2048: return 4;
        case 4096: return 5;
        case 8192: return 6;
        default:   return 0xFF;
    }
}

static inline bool uft_d88_validate_header(const uint8_t *header, size_t size) {
    if (!header || size < UFT_D88_HEADER_SIZE) return false;
    const uft_d88_header_t *hdr = (const uft_d88_header_t *)header;
    if (hdr->disk_type != UFT_D88_TYPE_2D && hdr->disk_type != UFT_D88_TYPE_2DD &&
        hdr->disk_type != UFT_D88_TYPE_2HD && hdr->disk_type != UFT_D88_TYPE_1D &&
        hdr->disk_type != UFT_D88_TYPE_1DD) return false;
    if (hdr->disk_size < UFT_D88_HEADER_SIZE) return false;
    if (hdr->track_offsets[0] > 0 && hdr->track_offsets[0] < UFT_D88_HEADER_SIZE) return false;
    return true;
}

static inline bool uft_d88_parse(const uint8_t *data, size_t size, uft_d88_info_t *info) {
    if (!data || !info || size < UFT_D88_HEADER_SIZE) return false;
    memset(info, 0, sizeof(uft_d88_info_t));
    const uft_d88_header_t *hdr = (const uft_d88_header_t *)data;
    memcpy(info->name, hdr->disk_name, 17);
    info->name[17] = '\0';
    info->disk_type = hdr->disk_type;
    info->disk_size = hdr->disk_size;
    info->write_protect = (hdr->write_protect != 0);
    memcpy(info->track_offsets, hdr->track_offsets, sizeof(info->track_offsets));
    switch (hdr->disk_type) {
        case UFT_D88_TYPE_2HD:
            info->tracks = 77; info->heads = 2; info->sectors_per_track = 8; info->sector_size = 1024; break;
        case UFT_D88_TYPE_2DD:
            info->tracks = 80; info->heads = 2; info->sectors_per_track = 8; info->sector_size = 512; break;
        case UFT_D88_TYPE_2D:
            info->tracks = 40; info->heads = 2; info->sectors_per_track = 16; info->sector_size = 256; break;
        case UFT_D88_TYPE_1D:
            info->tracks = 40; info->heads = 1; info->sectors_per_track = 16; info->sector_size = 256; break;
        case UFT_D88_TYPE_1DD:
            info->tracks = 80; info->heads = 1; info->sectors_per_track = 8; info->sector_size = 512; break;
        default:
            info->tracks = 80; info->heads = 2; info->sectors_per_track = 8; info->sector_size = 512; break;
    }
    info->is_valid = uft_d88_validate_header(data, size);
    return info->is_valid;
}

static inline uint32_t uft_d88_track_offset(const uft_d88_info_t *info, uint8_t track, uint8_t head) {
    if (!info) return 0;
    uint32_t idx = track * 2 + head;
    if (idx >= UFT_D88_MAX_TRACKS) return 0;
    return info->track_offsets[idx];
}

static inline double uft_d88_probe(const uint8_t *data, size_t size) {
    if (!data || size < UFT_D88_HEADER_SIZE) return 0.0;
    const uft_d88_header_t *hdr = (const uft_d88_header_t *)data;
    if (hdr->disk_type != UFT_D88_TYPE_2D && hdr->disk_type != UFT_D88_TYPE_2DD &&
        hdr->disk_type != UFT_D88_TYPE_2HD && hdr->disk_type != UFT_D88_TYPE_1D &&
        hdr->disk_type != UFT_D88_TYPE_1DD) return 0.0;
    double score = 0.4;
    if (hdr->disk_size >= UFT_D88_HEADER_SIZE && hdr->disk_size <= size) score += 0.2;
    if (hdr->track_offsets[0] == 0 || hdr->track_offsets[0] == UFT_D88_HEADER_SIZE) score += 0.2;
    if (hdr->write_protect <= 1) score += 0.1;
    if (size > UFT_D88_HEADER_SIZE) {
        const uft_d88_sector_header_t *sec = (const uft_d88_sector_header_t *)(data + UFT_D88_HEADER_SIZE);
        if (sec->size_code <= 6 && sec->sector_count <= 26) score += 0.1;
    }
    return score > 1.0 ? 1.0 : score;
}

static inline bool uft_d88_create_header(uint8_t *header, const char *name, uint8_t disk_type) {
    if (!header) return false;
    memset(header, 0, UFT_D88_HEADER_SIZE);
    if (name) {
        size_t len = strlen(name);
        if (len > 16) len = 16;
        memcpy(header, name, len);
    }
    header[0x1B] = disk_type;
    uint32_t size = UFT_D88_HEADER_SIZE;
    header[0x1C] = (uint8_t)(size & 0xFF);
    header[0x1D] = (uint8_t)((size >> 8) & 0xFF);
    header[0x1E] = (uint8_t)((size >> 16) & 0xFF);
    header[0x1F] = (uint8_t)((size >> 24) & 0xFF);
    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_D88_FORMAT_H */
