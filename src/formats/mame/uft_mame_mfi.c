/**
 * @file uft_mame_mfi.c
 * @brief MAME MFI (MAME Floppy Image) format implementation
 * @version 3.9.0
 * 
 * Standalone implementation without complex dependencies.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* MFI Constants */
#define MFI_MAGIC           "MAME FLOPPY IMAGE"
#define MFI_MAGIC_LEN       17

/* Track types */
#define MFI_TRACK_FM        0
#define MFI_TRACK_MFM       1
#define MFI_TRACK_GCR5      2
#define MFI_TRACK_GCR6      3
#define MFI_TRACK_RAW       4

/* ============================================================================
 * Structures
 * ============================================================================ */

#pragma pack(push, 1)
typedef struct {
    char     magic[17];
    uint8_t  version;
    uint8_t  cylinders;
    uint8_t  heads;
    uint8_t  form_factor;
    uint8_t  variant;
    uint8_t  reserved[10];
} mfi_header_t;

typedef struct {
    uint32_t offset;
    uint32_t size;
    uint32_t uncompressed;
    uint8_t  type;
    uint8_t  flags;
    uint16_t revolutions;
} mfi_track_header_t;
#pragma pack(pop)

typedef struct {
    uint8_t   cylinder;
    uint8_t   head;
    uint8_t   type;
    uint16_t  revolutions;
    uint32_t *flux_data;
    size_t    flux_count;
} mfi_track_data_t;

typedef struct {
    mfi_header_t header;
    mfi_track_data_t *tracks;
    size_t track_count;
} mfi_image_t;

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static uint32_t read_le32(const uint8_t *p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read_le16(const uint8_t *p) {
    return p[0] | (p[1] << 8);
}

static void write_le32(uint8_t *p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

/* ============================================================================
 * API Functions
 * ============================================================================ */

void uft_mfi_image_init(mfi_image_t *image) {
    if (!image) return;
    memset(image, 0, sizeof(*image));
}

void uft_mfi_image_free(mfi_image_t *image) {
    if (!image) return;
    
    if (image->tracks) {
        for (size_t t = 0; t < image->track_count; t++) {
            free(image->tracks[t].flux_data);
        }
        free(image->tracks);
    }
    memset(image, 0, sizeof(*image));
}

bool uft_mfi_probe(const uint8_t *data, size_t size, int *confidence) {
    if (!data || size < sizeof(mfi_header_t)) return false;
    
    if (memcmp(data, MFI_MAGIC, MFI_MAGIC_LEN) == 0) {
        if (confidence) *confidence = 98;
        return true;
    }
    return false;
}

int uft_mfi_read(const char *path, mfi_image_t *image) {
    if (!path || !image) return -1;
    
    FILE *fp = fopen(path, "rb");
    if (!fp) return -2;
    
    uft_mfi_image_init(image);
    
    /* Read header */
    if (fread(&image->header, sizeof(mfi_header_t), 1, fp) != 1) {
        fclose(fp);
        return -3;
    }
    
    /* Verify magic */
    if (memcmp(image->header.magic, MFI_MAGIC, MFI_MAGIC_LEN) != 0) {
        fclose(fp);
        return -4;
    }
    
    /* Calculate track count */
    size_t num_tracks = (size_t)image->header.cylinders * image->header.heads;
    if (num_tracks == 0 || num_tracks > 400) {
        fclose(fp);
        return -5;
    }
    
    /* Allocate tracks */
    image->tracks = calloc(num_tracks, sizeof(mfi_track_data_t));
    if (!image->tracks) {
        fclose(fp);
        return -6;
    }
    image->track_count = num_tracks;
    
    /* Read track headers */
    for (size_t t = 0; t < num_tracks; t++) {
        mfi_track_header_t th;
        if (fread(&th, sizeof(th), 1, fp) != 1) break;
        
        mfi_track_data_t *track = &image->tracks[t];
        track->cylinder = t / image->header.heads;
        track->head = t % image->header.heads;
        track->type = th.type;
        track->revolutions = read_le16((uint8_t*)&th.revolutions);
        
        uint32_t offset = read_le32((uint8_t*)&th.offset);
        uint32_t size = read_le32((uint8_t*)&th.size);
        
        if (size > 0 && offset > 0) {
            long save_pos = ftell(fp);
            
            if (fseek(fp, offset, SEEK_SET) == 0) {
                uint8_t *raw_data = malloc(size);
                if (raw_data) {
                    if (fread(raw_data, 1, size, fp) == size) {
                        /* Parse flux data (32-bit times) */
                        track->flux_count = size / 4;
                        track->flux_data = malloc(track->flux_count * sizeof(uint32_t));
                        if (track->flux_data) {
                            for (size_t i = 0; i < track->flux_count; i++) {
                                track->flux_data[i] = read_le32(raw_data + i * 4);
                            }
                        }
                    }
                    free(raw_data);
                }
            }
            fseek(fp, save_pos, SEEK_SET);
        }
    }
    
    fclose(fp);
    return 0;
}

int uft_mfi_write(const mfi_image_t *image, const char *path) {
    if (!image || !path) return -1;
    
    FILE *fp = fopen(path, "wb");
    if (!fp) return -2;
    
    /* Write header */
    mfi_header_t header;
    memset(&header, 0, sizeof(header));
    memcpy(header.magic, MFI_MAGIC, MFI_MAGIC_LEN);
    header.version = 1;
    header.cylinders = image->header.cylinders;
    header.heads = image->header.heads;
    header.form_factor = image->header.form_factor;
    header.variant = image->header.variant;
    
    fwrite(&header, sizeof(header), 1, fp);
    
    /* Calculate track data offset */
    size_t track_headers_size = image->track_count * sizeof(mfi_track_header_t);
    uint32_t data_offset = sizeof(header) + track_headers_size;
    
    /* Write track headers */
    for (size_t t = 0; t < image->track_count; t++) {
        const mfi_track_data_t *track = &image->tracks[t];
        mfi_track_header_t th;
        memset(&th, 0, sizeof(th));
        
        th.type = track->type;
        
        size_t track_data_size = track->flux_count * 4;
        write_le32((uint8_t*)&th.offset, data_offset);
        write_le32((uint8_t*)&th.size, (uint32_t)track_data_size);
        write_le32((uint8_t*)&th.uncompressed, (uint32_t)track_data_size);
        
        fwrite(&th, sizeof(th), 1, fp);
        data_offset += track_data_size;
    }
    
    /* Write track data */
    for (size_t t = 0; t < image->track_count; t++) {
        const mfi_track_data_t *track = &image->tracks[t];
        
        for (size_t i = 0; i < track->flux_count; i++) {
            uint8_t buf[4];
            write_le32(buf, track->flux_data[i]);
            fwrite(buf, 4, 1, fp);
        }
    }
    
    fclose(fp);
    return 0;
}

const char* uft_mfi_track_type_name(uint8_t type) {
    switch (type) {
        case MFI_TRACK_FM:   return "FM";
        case MFI_TRACK_MFM:  return "MFM";
        case MFI_TRACK_GCR5: return "GCR5";
        case MFI_TRACK_GCR6: return "GCR6";
        case MFI_TRACK_RAW:  return "Raw";
        default: return "Unknown";
    }
}
