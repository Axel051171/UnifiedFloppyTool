/**
 * @file fdi_writer.c
 * @brief FDI (Formatted Disk Image) Writer
 * 
 * FDI is a universal disk image format that stores both data and format info.
 * Used by various emulators (Spectrum, CPC, MSX, etc.)
 * 
 * @version 5.30.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* FDI Constants */
#define FDI_SIGNATURE       "FDI"
#define FDI_VERSION         0x0200  /* Version 2.0 */

/* Data rates */
#define FDI_RATE_500        0x00    /* 500 kbps (HD) */
#define FDI_RATE_300        0x01    /* 300 kbps */
#define FDI_RATE_250        0x02    /* 250 kbps (DD) */
#define FDI_RATE_1000       0x03    /* 1000 kbps (ED) */

/* Recording modes */
#define FDI_MODE_FM         0x00    /* FM (SD) */
#define FDI_MODE_MFM        0x01    /* MFM */

#pragma pack(push, 1)
typedef struct {
    char     signature[3];      /* "FDI" */
    uint8_t  write_protect;     /* 0=RW, 1=RO */
    uint16_t cylinders;         /* Number of cylinders */
    uint16_t heads;             /* Number of heads */
    uint16_t desc_offset;       /* Offset to description */
    uint16_t data_offset;       /* Offset to track data */
    uint16_t extra_offset;      /* Offset to extra data */
    uint8_t  reserved[12];
} fdi_header_t;

typedef struct {
    uint32_t offset;            /* Offset to track data */
    uint16_t reserved;
    uint8_t  sectors;           /* Number of sectors */
} fdi_track_entry_t;

typedef struct {
    uint8_t  cylinder;
    uint8_t  head;
    uint8_t  sector;
    uint8_t  size;              /* Size code (0=128, 1=256, 2=512, 3=1024) */
    uint8_t  flags;             /* Flags (CRC error, etc.) */
    uint16_t offset;            /* Offset within track */
} fdi_sector_entry_t;
#pragma pack(pop)

typedef struct {
    uint8_t  cylinder;
    uint8_t  head;
    uint8_t  sector;
    int      size;
    uint8_t  flags;
    uint8_t *data;
} fdi_sector_t;

typedef struct {
    int          sector_count;
    fdi_sector_t *sectors;
} fdi_track_t;

typedef struct {
    fdi_header_t header;
    char        *description;
    fdi_track_t *tracks;
    int          track_count;
    int          cylinders;
    int          heads;
} fdi_image_t;

/* ============================================================================
 * FDI Writer
 * ============================================================================ */

/**
 * @brief Create empty FDI image
 */
int fdi_create(fdi_image_t *img, int cylinders, int heads)
{
    if (!img) return -1;
    
    memset(img, 0, sizeof(*img));
    
    memcpy(img->header.signature, FDI_SIGNATURE, 3);
    img->header.cylinders = cylinders;
    img->header.heads = heads;
    
    img->cylinders = cylinders;
    img->heads = heads;
    img->track_count = cylinders * heads;
    
    img->tracks = calloc(img->track_count, sizeof(fdi_track_t));
    return img->tracks ? 0 : -1;
}

/**
 * @brief Set description
 */
int fdi_set_description(fdi_image_t *img, const char *desc)
{
    if (!img) return -1;
    
    free(img->description);
    img->description = desc ? strdup(desc) : NULL;
    
    return 0;
}

/**
 * @brief Add sector to track
 */
int fdi_add_sector(fdi_image_t *img, int cylinder, int head, int sector,
                   int size, uint8_t flags, const uint8_t *data)
{
    if (!img || !img->tracks || !data) return -1;
    
    int track_idx = cylinder * img->heads + head;
    if (track_idx >= img->track_count) return -1;
    
    fdi_track_t *track = &img->tracks[track_idx];
    
    int s = track->sector_count;
    track->sectors = realloc(track->sectors, (s + 1) * sizeof(fdi_sector_t));
    
    if (!track->sectors) return -1;
    
    track->sectors[s].cylinder = cylinder;
    track->sectors[s].head = head;
    track->sectors[s].sector = sector;
    track->sectors[s].size = size;
    track->sectors[s].flags = flags;
    track->sectors[s].data = malloc(size);
    
    if (!track->sectors[s].data) return -1;
    
    memcpy(track->sectors[s].data, data, size);
    track->sector_count++;
    
    return 0;
}

/**
 * @brief Get size code from sector size
 */
static uint8_t fdi_size_code(int size)
{
    switch (size) {
        case 128:  return 0;
        case 256:  return 1;
        case 512:  return 2;
        case 1024: return 3;
        case 2048: return 4;
        case 4096: return 5;
        default:   return 2;
    }
}

/**
 * @brief Save FDI image
 */
int fdi_save(const fdi_image_t *img, const char *filename)
{
    if (!img || !filename) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -1;
    
    /* Calculate offsets */
    size_t header_size = sizeof(fdi_header_t);
    size_t track_table_size = img->track_count * sizeof(fdi_track_entry_t);
    
    size_t desc_offset = header_size + track_table_size;
    size_t desc_len = img->description ? strlen(img->description) + 1 : 1;
    
    /* Calculate sector header sizes */
    size_t sector_headers_size = 0;
    for (int t = 0; t < img->track_count; t++) {
        sector_headers_size += img->tracks[t].sector_count * sizeof(fdi_sector_entry_t);
    }
    
    size_t data_offset = desc_offset + desc_len + sector_headers_size;
    
    /* Update header */
    fdi_header_t header = img->header;
    header.desc_offset = desc_offset;
    header.data_offset = data_offset;
    header.extra_offset = 0;
    
    fwrite(&header, sizeof(header), 1, fp);
    
    /* Write track table */
    uint32_t current_data_offset = 0;
    size_t current_sector_header_offset = desc_offset + desc_len;
    
    for (int t = 0; t < img->track_count; t++) {
        fdi_track_entry_t entry = {0};
        
        if (img->tracks[t].sector_count > 0) {
            entry.offset = current_sector_header_offset - header_size;
            entry.sectors = img->tracks[t].sector_count;
            
            current_sector_header_offset += 
                img->tracks[t].sector_count * sizeof(fdi_sector_entry_t);
        }
        
        fwrite(&entry, sizeof(entry), 1, fp);
    }
    
    /* Write description */
    if (img->description) {
        fwrite(img->description, 1, strlen(img->description) + 1, fp);
    } else {
        uint8_t zero = 0;
        fwrite(&zero, 1, 1, fp);
    }
    
    /* Write sector headers and track data */
    current_data_offset = 0;
    
    for (int t = 0; t < img->track_count; t++) {
        /* Sector headers for this track */
        uint16_t track_offset = 0;
        
        for (int s = 0; s < img->tracks[t].sector_count; s++) {
            fdi_sector_entry_t sect = {0};
            sect.cylinder = img->tracks[t].sectors[s].cylinder;
            sect.head = img->tracks[t].sectors[s].head;
            sect.sector = img->tracks[t].sectors[s].sector;
            sect.size = fdi_size_code(img->tracks[t].sectors[s].size);
            sect.flags = img->tracks[t].sectors[s].flags;
            sect.offset = track_offset;
            
            fwrite(&sect, sizeof(sect), 1, fp);
            
            track_offset += img->tracks[t].sectors[s].size;
        }
    }
    
    /* Write sector data */
    for (int t = 0; t < img->track_count; t++) {
        for (int s = 0; s < img->tracks[t].sector_count; s++) {
            fwrite(img->tracks[t].sectors[s].data, 1,
                   img->tracks[t].sectors[s].size, fp);
        }
    }
    
    fclose(fp);
    return 0;
}

/**
 * @brief Convert raw IMG to FDI
 */
int fdi_from_img(const char *img_file, const char *fdi_file,
                 int cylinders, int heads, int sectors, int sector_size)
{
    FILE *fp = fopen(img_file, "rb");
    if (!fp) return -1;
    
    fdi_image_t fdi;
    fdi_create(&fdi, cylinders, heads);
    fdi_set_description(&fdi, "Created by UnifiedFloppyTool");
    
    uint8_t *sector_data = malloc(sector_size);
    
    for (int c = 0; c < cylinders; c++) {
        for (int h = 0; h < heads; h++) {
            for (int s = 1; s <= sectors; s++) {
                if (fread(sector_data, 1, sector_size, fp) != (size_t)sector_size) {
                    break;
                }
                fdi_add_sector(&fdi, c, h, s, sector_size, 0, sector_data);
            }
        }
    }
    
    free(sector_data);
    fclose(fp);
    
    int result = fdi_save(&fdi, fdi_file);
    fdi_free(&fdi);
    
    return result;
}

/**
 * @brief Free FDI image
 */
void fdi_free(fdi_image_t *img)
{
    if (img) {
        free(img->description);
        
        if (img->tracks) {
            for (int t = 0; t < img->track_count; t++) {
                for (int s = 0; s < img->tracks[t].sector_count; s++) {
                    free(img->tracks[t].sectors[s].data);
                }
                free(img->tracks[t].sectors);
            }
            free(img->tracks);
        }
        
        memset(img, 0, sizeof(*img));
    }
}
