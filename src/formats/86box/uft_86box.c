/**
 * @file uft_86box.c
 * @brief 86Box/PCem 86F floppy image format implementation
 * @version 3.9.0
 * 
 * Standalone implementation without complex dependencies.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "uft/core/uft_error_codes.h"

/* 86F Magic and Constants */
#define BOX86F_MAGIC            "86BX"
#define BOX86F_MAGIC_LEN        4
#define BOX86F_VERSION          0x0120

/* Disk variants */
#define BOX86F_DISK_360K        0x00
#define BOX86F_DISK_720K        0x01
#define BOX86F_DISK_1200K       0x02
#define BOX86F_DISK_1440K       0x03
#define BOX86F_DISK_2880K       0x04

/* Track flags */
#define BOX86F_TRACK_VALID      0x01

/* ============================================================================
 * Structures
 * ============================================================================ */

#pragma pack(push, 1)
typedef struct {
    char     magic[4];
    uint16_t version;
    uint8_t  disk_type;
    uint8_t  sides;
    uint8_t  tracks;
    uint8_t  encoding;
    uint8_t  bitcell_rate;
    uint8_t  rpm;
    uint8_t  write_protect;
    uint8_t  reserved[19];
} box86f_header_t;

typedef struct {
    uint32_t offset;
    uint32_t length;
    uint8_t  flags;
    uint8_t  sectors;
    uint16_t rpm;
} box86f_track_header_t;
#pragma pack(pop)

typedef struct {
    uint8_t  cylinder;
    uint8_t  head;
    uint8_t  flags;
    uint8_t *data;
    size_t   data_size;
} box86f_track_data_t;

typedef struct {
    box86f_header_t header;
    box86f_track_data_t *tracks;
    size_t track_count;
    uint8_t cylinders;
    uint8_t heads;
} box86f_image_t;

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static uint32_t read_le32(const uint8_t *p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void write_le32(uint8_t *p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

static void write_le16(uint8_t *p, uint16_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

/* ============================================================================
 * API Functions
 * ============================================================================ */

const char* uft_86f_disk_type_name(uint8_t type) {
    switch (type) {
        case BOX86F_DISK_360K:  return "360K DD 5.25\"";
        case BOX86F_DISK_720K:  return "720K DD 3.5\"";
        case BOX86F_DISK_1200K: return "1.2M HD 5.25\"";
        case BOX86F_DISK_1440K: return "1.44M HD 3.5\"";
        case BOX86F_DISK_2880K: return "2.88M ED 3.5\"";
        default: return "Unknown";
    }
}

void uft_86f_image_init(box86f_image_t *image) {
    if (!image) return;
    memset(image, 0, sizeof(*image));
}

void uft_86f_image_free(box86f_image_t *image) {
    if (!image) return;
    
    if (image->tracks) {
        for (size_t t = 0; t < image->track_count; t++) {
            free(image->tracks[t].data);
        }
        free(image->tracks);
    }
    memset(image, 0, sizeof(*image));
}

bool uft_86f_probe(const uint8_t *data, size_t size, int *confidence) {
    if (!data || size < sizeof(box86f_header_t)) return false;
    
    if (memcmp(data, BOX86F_MAGIC, BOX86F_MAGIC_LEN) == 0) {
        if (confidence) *confidence = 98;
        return true;
    }
    return false;
}

int uft_86f_read(const char *path, box86f_image_t *image) {
    if (!path || !image) return -1;
    
    FILE *fp = fopen(path, "rb");
    if (!fp) return -2;
    
    uft_86f_image_init(image);
    
    /* Read header */
    if (fread(&image->header, sizeof(box86f_header_t), 1, fp) != 1) {
        fclose(fp);
        return -3;
    }
    
    /* Verify magic */
    if (memcmp(image->header.magic, BOX86F_MAGIC, BOX86F_MAGIC_LEN) != 0) {
        fclose(fp);
        return -4;
    }
    
    /* Set geometry */
    image->cylinders = image->header.tracks;
    image->heads = image->header.sides;
    
    size_t num_tracks = (size_t)image->cylinders * image->heads;
    if (num_tracks == 0 || num_tracks > 400) {
        fclose(fp);
        return -5;
    }
    
    /* Allocate tracks */
    image->tracks = calloc(num_tracks, sizeof(box86f_track_data_t));
    if (!image->tracks) {
        fclose(fp);
        return -6;
    }
    image->track_count = num_tracks;
    
    /* Read track headers and data */
    for (size_t t = 0; t < num_tracks; t++) {
        box86f_track_header_t th;
        if (fread(&th, sizeof(th), 1, fp) != 1) break;
        
        box86f_track_data_t *track = &image->tracks[t];
        track->cylinder = t / image->heads;
        track->head = t % image->heads;
        track->flags = th.flags;
        
        uint32_t offset = read_le32((uint8_t*)&th.offset);
        uint32_t length = read_le32((uint8_t*)&th.length);
        
        if (length > 0 && offset > 0 && (th.flags & BOX86F_TRACK_VALID)) {
            long save_pos = ftell(fp);
            
            if (fseek(fp, offset, SEEK_SET) == 0) {
                track->data = malloc(length);
                if (track->data) {
                    if (fread(track->data, 1, length, fp) == length) {
                        track->data_size = length;
                    } else {
                        free(track->data);
                        track->data = NULL;
                    }
                }
            }
            fseek(fp, save_pos, SEEK_SET);
        }
    }
    if (ferror(fp)) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    
        
    fclose(fp);
return 0;
}

int uft_86f_write(const box86f_image_t *image, const char *path) {
    if (!image || !path) return -1;
    
    FILE *fp = fopen(path, "wb");
    if (!fp) return -2;
    
    /* Write header */
    box86f_header_t header;
    memset(&header, 0, sizeof(header));
    memcpy(header.magic, BOX86F_MAGIC, BOX86F_MAGIC_LEN);
    write_le16((uint8_t*)&header.version, BOX86F_VERSION);
    header.disk_type = image->header.disk_type;
    header.sides = image->heads;
    header.tracks = image->cylinders;
    header.encoding = image->header.encoding;
    
    fwrite(&header, sizeof(header), 1, fp);
    
    /* Calculate data offset */
    size_t track_headers_size = image->track_count * sizeof(box86f_track_header_t);
    uint32_t data_offset = sizeof(header) + track_headers_size;
    
    /* Write track headers */
    for (size_t t = 0; t < image->track_count; t++) {
        const box86f_track_data_t *track = &image->tracks[t];
        box86f_track_header_t th;
        memset(&th, 0, sizeof(th));
        
        th.flags = track->flags;
        if (track->data && track->data_size > 0) {
            th.flags |= BOX86F_TRACK_VALID;
            write_le32((uint8_t*)&th.offset, data_offset);
            write_le32((uint8_t*)&th.length, (uint32_t)track->data_size);
            data_offset += track->data_size;
        }
        
        fwrite(&th, sizeof(th), 1, fp);
    }
    
    /* Write track data */
    for (size_t t = 0; t < image->track_count; t++) {
        const box86f_track_data_t *track = &image->tracks[t];
        if (track->data && track->data_size > 0) {
            fwrite(track->data, 1, track->data_size, fp);
        }
    }
    if (ferror(fp)) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    
        
    fclose(fp);
return 0;
}
