/**
 * @file uft_dc42_format.h
 * @brief Apple DiskCopy 4.2 Format
 */

#ifndef UFT_DC42_FORMAT_H
#define UFT_DC42_FORMAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UFT_DC42_MAGIC              0x0100
#define UFT_DC42_HEADER_SIZE        84
#define UFT_DC42_MAX_NAME_LEN       63
#define UFT_DC42_TAG_SIZE           12
#define UFT_DC42_SECTOR_SIZE        512

#define UFT_DC42_FORMAT_400K_SS     0x00
#define UFT_DC42_FORMAT_800K_DS     0x01
#define UFT_DC42_FORMAT_1440K_HD    0x02

#define UFT_DC42_ENCODING_GCR       0x12
#define UFT_DC42_ENCODING_MFM       0x22
#define UFT_DC42_ENCODING_RAW       0x00

#define UFT_DC42_400K_SIZE          409600
#define UFT_DC42_800K_SIZE          819200
#define UFT_DC42_1440K_SIZE         1474560
#define UFT_DC42_400K_TRACKS        80
#define UFT_DC42_400K_HEADS         1
#define UFT_DC42_400K_SECTORS       800
#define UFT_DC42_800K_TRACKS        80
#define UFT_DC42_800K_HEADS         2
#define UFT_DC42_800K_SECTORS       1600
#define UFT_DC42_1440K_TRACKS       80
#define UFT_DC42_1440K_HEADS        2
#define UFT_DC42_1440K_SECTORS      2880

static const uint8_t uft_dc42_zone_tracks[5] = {16, 32, 48, 64, 80};
static const uint8_t uft_dc42_zone_spt[5] = {12, 11, 10, 9, 8};

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
typedef struct __attribute__((packed)) {
    uint8_t  name_len;
    char     disk_name[63];
    uint32_t data_size;
    uint32_t tag_size;
    uint32_t data_checksum;
    uint32_t tag_checksum;
    uint8_t  disk_format;
    uint8_t  format_byte;
    uint16_t magic;
} uft_dc42_header_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

typedef struct {
    char     disk_name[64];
    uint32_t data_size;
    uint32_t tag_size;
    uint32_t data_checksum;
    uint32_t tag_checksum;
    uint8_t  disk_format;
    uint8_t  format_byte;
    size_t   total_sectors;
    size_t   file_size;
    bool     has_tags;
    bool     is_gcr;
    bool     is_valid;
    uint8_t  tracks;
    uint8_t  heads;
    uint16_t sector_size;
} uft_dc42_info_t;

static inline uint16_t uft_dc42_read_be16(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static inline uint32_t uft_dc42_read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static inline void uft_dc42_write_be16(uint8_t *p, uint16_t val) {
    p[0] = (uint8_t)(val >> 8); p[1] = (uint8_t)(val & 0xFF);
}

static inline void uft_dc42_write_be32(uint8_t *p, uint32_t val) {
    p[0] = (uint8_t)(val >> 24); p[1] = (uint8_t)(val >> 16);
    p[2] = (uint8_t)(val >> 8); p[3] = (uint8_t)(val & 0xFF);
}

static inline bool uft_dc42_validate_header(const uint8_t *header) {
    if (!header) return false;
    uint16_t magic = uft_dc42_read_be16(header + 82);
    if (magic != UFT_DC42_MAGIC) return false;
    if (header[0] > UFT_DC42_MAX_NAME_LEN) return false;
    return true;
}

static inline bool uft_dc42_parse(const uint8_t *data, size_t size, uft_dc42_info_t *info) {
    if (!data || !info || size < UFT_DC42_HEADER_SIZE) return false;
    memset(info, 0, sizeof(uft_dc42_info_t));
    uint8_t name_len = data[0];
    if (name_len > UFT_DC42_MAX_NAME_LEN) name_len = UFT_DC42_MAX_NAME_LEN;
    memcpy(info->disk_name, data + 1, name_len);
    info->disk_name[name_len] = '\0';
    info->data_size = uft_dc42_read_be32(data + 64);
    info->tag_size = uft_dc42_read_be32(data + 68);
    info->data_checksum = uft_dc42_read_be32(data + 72);
    info->tag_checksum = uft_dc42_read_be32(data + 76);
    info->disk_format = data[80];
    info->format_byte = data[81];
    uint16_t magic = uft_dc42_read_be16(data + 82);
    info->is_valid = (magic == UFT_DC42_MAGIC);
    if (!info->is_valid) return false;
    info->has_tags = (info->tag_size > 0);
    info->is_gcr = (info->format_byte == UFT_DC42_ENCODING_GCR);
    info->file_size = UFT_DC42_HEADER_SIZE + info->data_size + info->tag_size;
    switch (info->data_size) {
        case UFT_DC42_400K_SIZE: info->tracks = 80; info->heads = 1; info->total_sectors = 800; break;
        case UFT_DC42_800K_SIZE: info->tracks = 80; info->heads = 2; info->total_sectors = 1600; break;
        case UFT_DC42_1440K_SIZE: info->tracks = 80; info->heads = 2; info->total_sectors = 2880; break;
        default: info->tracks = 80; info->heads = 2; info->total_sectors = info->data_size / 512; break;
    }
    info->sector_size = 512;
    return true;
}

static inline const char* uft_dc42_format_name(uint8_t disk_format) {
    switch (disk_format) {
        case UFT_DC42_FORMAT_400K_SS: return "Mac 400K (SS)";
        case UFT_DC42_FORMAT_800K_DS: return "Mac 800K (DS)";
        case UFT_DC42_FORMAT_1440K_HD: return "Mac 1.44MB (HD)";
        default: return "Unknown";
    }
}

static inline const char* uft_dc42_encoding_name(uint8_t format_byte) {
    switch (format_byte) {
        case UFT_DC42_ENCODING_GCR: return "GCR (Sony)";
        case UFT_DC42_ENCODING_MFM: return "MFM";
        case UFT_DC42_ENCODING_RAW: return "Raw/ProDOS";
        default: return "Unknown";
    }
}

static inline uint8_t uft_dc42_gcr_sectors_per_track(uint8_t track) {
    for (int zone = 0; zone < 5; zone++) {
        if (track < uft_dc42_zone_tracks[zone]) return uft_dc42_zone_spt[zone];
    }
    return 8;
}

static inline size_t uft_dc42_gcr_sector_offset(uint8_t track, uint8_t head, uint8_t sector) {
    size_t offset = 0;
    uint8_t total_tracks = track + (head * 80);
    for (uint8_t t = 0; t < total_tracks && t < 160; t++) {
        offset += uft_dc42_gcr_sectors_per_track(t % 80) * UFT_DC42_SECTOR_SIZE;
    }
    return offset + sector * UFT_DC42_SECTOR_SIZE;
}

static inline uint32_t uft_dc42_crc32(const uint8_t *data, size_t size) {
    uint32_t crc = 0;
    if (!data) return 0;
    for (size_t i = 0; i < size; i++) {
        crc = (crc >> 1) | ((crc & 1) ? 0x80000000 : 0);
        crc += data[i];
    }
    return crc;
}

static inline double uft_dc42_probe(const uint8_t *data, size_t size) {
    if (!data || size < UFT_DC42_HEADER_SIZE) return 0.0;
    uint16_t magic = uft_dc42_read_be16(data + 82);
    if (magic != UFT_DC42_MAGIC) return 0.0;
    double score = 0.5;
    if (data[0] <= UFT_DC42_MAX_NAME_LEN) score += 0.1;
    uint32_t data_size = uft_dc42_read_be32(data + 64);
    if (data_size == UFT_DC42_400K_SIZE || data_size == UFT_DC42_800K_SIZE || data_size == UFT_DC42_1440K_SIZE) score += 0.2;
    uint32_t tag_size = uft_dc42_read_be32(data + 68);
    size_t expected = UFT_DC42_HEADER_SIZE + data_size + tag_size;
    if (size >= expected && size <= expected + 16) score += 0.15;
    if (data[80] <= 0x02) score += 0.05;
    return score > 1.0 ? 1.0 : score;
}

static inline bool uft_dc42_create_header(uint8_t *header, const char *name, uint32_t data_size, uint32_t tag_size, uint8_t disk_format) {
    if (!header || !name) return false;
    memset(header, 0, UFT_DC42_HEADER_SIZE);
    size_t name_len = strlen(name);
    if (name_len > UFT_DC42_MAX_NAME_LEN) name_len = UFT_DC42_MAX_NAME_LEN;
    header[0] = (uint8_t)name_len;
    memcpy(header + 1, name, name_len);
    uft_dc42_write_be32(header + 64, data_size);
    uft_dc42_write_be32(header + 68, tag_size);
    header[80] = disk_format;
    header[81] = (disk_format == UFT_DC42_FORMAT_1440K_HD) ? UFT_DC42_ENCODING_MFM : UFT_DC42_ENCODING_GCR;
    uft_dc42_write_be16(header + 82, UFT_DC42_MAGIC);
    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_DC42_FORMAT_H */
