/**
 * @file uft_d64.c
 * @brief Commodore D64/G64 disk image support
 * 
 * Implementation of D64 (sector-based) and G64 (GCR-encoded) formats.
 */

#include "formats/uft_diskimage.h"
#include "encoding/uft_gcr.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * D64 Track Layout Table
 * ============================================================================ */

/**
 * D64 sectors per track (from disk2disk)
 * Track 1-17:  21 sectors
 * Track 18-24: 19 sectors
 * Track 25-30: 18 sectors
 * Track 31-35: 17 sectors
 */
static const uint8_t d64_sectors_per_track[] = {
    0,   /* Track 0 (unused) */
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  /* Tracks 1-10 */
    21, 21, 21, 21, 21, 21, 21,              /* Tracks 11-17 */
    19, 19, 19, 19, 19, 19, 19,              /* Tracks 18-24 */
    18, 18, 18, 18, 18, 18,                  /* Tracks 25-30 */
    17, 17, 17, 17, 17,                      /* Tracks 31-35 */
    17, 17, 17, 17, 17,                      /* Tracks 36-40 (extended) */
};

/**
 * D64 track starting offsets (cumulative sector count * 256)
 */
static const uint32_t d64_track_offset[] = {
    0,       /* Track 0 */
    0x00000, /* Track 1 */
    0x01500, /* Track 2 */
    0x02A00, /* Track 3 */
    0x03F00, /* Track 4 */
    0x05400, /* Track 5 */
    0x06900, /* Track 6 */
    0x07E00, /* Track 7 */
    0x09300, /* Track 8 */
    0x0A800, /* Track 9 */
    0x0BD00, /* Track 10 */
    0x0D200, /* Track 11 */
    0x0E700, /* Track 12 */
    0x0FC00, /* Track 13 */
    0x11100, /* Track 14 */
    0x12600, /* Track 15 */
    0x13B00, /* Track 16 */
    0x15000, /* Track 17 */
    0x16500, /* Track 18 */
    0x17800, /* Track 19 */
    0x18B00, /* Track 20 */
    0x19E00, /* Track 21 */
    0x1B100, /* Track 22 */
    0x1C400, /* Track 23 */
    0x1D700, /* Track 24 */
    0x1EA00, /* Track 25 */
    0x1FC00, /* Track 26 */
    0x20E00, /* Track 27 */
    0x22000, /* Track 28 */
    0x23200, /* Track 29 */
    0x24400, /* Track 30 */
    0x25600, /* Track 31 */
    0x26700, /* Track 32 */
    0x27800, /* Track 33 */
    0x28900, /* Track 34 */
    0x29A00, /* Track 35 */
    0x2AB00, /* Track 36 */
    0x2BC00, /* Track 37 */
    0x2CD00, /* Track 38 */
    0x2DE00, /* Track 39 */
    0x2EF00, /* Track 40 */
};

/* ============================================================================
 * D64 Functions
 * ============================================================================ */

uint8_t uft_d64_sectors_per_track(uint8_t track) {
    if (track == 0 || track > 40) return 0;
    return d64_sectors_per_track[track];
}

ssize_t uft_d64_sector_offset(uint8_t track, uint8_t sector) {
    if (track == 0 || track > 40) return -1;
    if (sector >= d64_sectors_per_track[track]) return -1;
    
    return (ssize_t)(d64_track_offset[track] + (sector * 256));
}

/**
 * @brief Validate D64 file size
 */
static bool d64_validate_size(size_t size, uint8_t *tracks, bool *has_errors) {
    *has_errors = false;
    
    switch (size) {
        case UFT_D64_SIZE_35:
            *tracks = 35;
            return true;
        case UFT_D64_SIZE_35_ERR:
            *tracks = 35;
            *has_errors = true;
            return true;
        case UFT_D64_SIZE_40:
            *tracks = 40;
            return true;
        case UFT_D64_SIZE_40_ERR:
            *tracks = 40;
            *has_errors = true;
            return true;
        default:
            return false;
    }
}

/**
 * @brief D64 image handle
 */
typedef struct {
    FILE *fp;
    bool readonly;
    uint8_t tracks;
    bool has_errors;
    uint8_t *error_table;       /* Error bytes if present */
    uint8_t *data;              /* Cached image data */
    size_t data_size;
} d64_handle_t;

/**
 * @brief Open D64 image
 */
int uft_d64_open(const char *path, bool readonly, d64_handle_t **handle) {
    if (!path || !handle) return -1;
    
    FILE *fp = fopen(path, readonly ? "rb" : "r+b");
    if (!fp) return -1;
    
    /* Get file size */
    fseek(fp, 0, SEEK_END);
    size_t size = (size_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    /* Validate size */
    uint8_t tracks;
    bool has_errors;
    if (!d64_validate_size(size, &tracks, &has_errors)) {
        fclose(fp);
        return -2;
    }
    
    /* Allocate handle */
    d64_handle_t *h = calloc(1, sizeof(d64_handle_t));
    if (!h) {
        fclose(fp);
        return -3;
    }
    
    h->fp = fp;
    h->readonly = readonly;
    h->tracks = tracks;
    h->has_errors = has_errors;
    
    /* Calculate data size (without error table) */
    h->data_size = (tracks == 35) ? UFT_D64_SIZE_35 : UFT_D64_SIZE_40;
    
    /* Load image into memory */
    h->data = malloc(h->data_size);
    if (!h->data) {
        free(h);
        fclose(fp);
        return -3;
    }
    
    if (fread(h->data, 1, h->data_size, fp) != h->data_size) {
        free(h->data);
        free(h);
        fclose(fp);
        return -4;
    }
    
    /* Load error table if present */
    if (has_errors) {
        size_t error_size = size - h->data_size;
        h->error_table = malloc(error_size);
        if (h->error_table) {
            if (fread(h->error_table, 1, error_size, fp) != error_size) {
                /* Non-fatal: just don't have error info */
                free(h->error_table);
                h->error_table = NULL;
            }
        }
    }
    
    *handle = h;
    return 0;
}

/**
 * @brief Close D64 image
 */
void uft_d64_close(d64_handle_t *h) {
    if (!h) return;
    
    /* Write back changes if not readonly */
    if (!h->readonly && h->fp && h->data) {
        fseek(h->fp, 0, SEEK_SET);
        fwrite(h->data, 1, h->data_size, h->fp);
        if (h->error_table) {
            size_t error_size = h->has_errors ? 
                ((h->tracks == 35) ? 683 : 768) : 0;
            if (error_size > 0) {
                fwrite(h->error_table, 1, error_size, h->fp);
            }
        }
    }
    
    if (h->fp) fclose(h->fp);
    if (h->data) free(h->data);
    if (h->error_table) free(h->error_table);
    free(h);
}

/**
 * @brief Read D64 sector
 */
ssize_t uft_d64_read_sector(d64_handle_t *h, uint8_t track, uint8_t sector,
                             uint8_t *buffer) {
    if (!h || !buffer) return -1;
    
    ssize_t offset = uft_d64_sector_offset(track, sector);
    if (offset < 0 || (size_t)offset + 256 > h->data_size) return -1;
    
    memcpy(buffer, h->data + offset, 256);
    return 256;
}

/**
 * @brief Write D64 sector
 */
ssize_t uft_d64_write_sector(d64_handle_t *h, uint8_t track, uint8_t sector,
                              const uint8_t *data) {
    if (!h || !data || h->readonly) return -1;
    
    ssize_t offset = uft_d64_sector_offset(track, sector);
    if (offset < 0 || (size_t)offset + 256 > h->data_size) return -1;
    
    memcpy(h->data + offset, data, 256);
    return 256;
}

/**
 * @brief Get D64 sector error
 */
uint8_t uft_d64_get_error(d64_handle_t *h, uint8_t track, uint8_t sector) {
    if (!h || !h->error_table) return 0;
    
    /* Calculate error table index */
    int index = 0;
    for (int t = 1; t < track; t++) {
        index += d64_sectors_per_track[t];
    }
    index += sector;
    
    return h->error_table[index];
}

/* ============================================================================
 * G64 Functions
 * ============================================================================ */

/** G64 header structure */
typedef struct {
    char signature[8];          /* "GCR-1541" */
    uint8_t version;            /* 0 */
    uint8_t track_count;        /* Number of tracks */
    uint16_t max_track_size;    /* Maximum track size */
} __attribute__((packed)) g64_header_t;

/**
 * @brief G64 image handle
 */
typedef struct {
    FILE *fp;
    bool readonly;
    g64_header_t header;
    uint32_t *track_offsets;    /* Offset to each track */
    uint32_t *speed_offsets;    /* Offset to speed zones */
    uint16_t *track_sizes;      /* Size of each track */
} g64_handle_t;

/**
 * @brief Open G64 image
 */
int uft_g64_open(const char *path, bool readonly, g64_handle_t **handle) {
    if (!path || !handle) return -1;
    
    FILE *fp = fopen(path, readonly ? "rb" : "r+b");
    if (!fp) return -1;
    
    g64_handle_t *h = calloc(1, sizeof(g64_handle_t));
    if (!h) {
        fclose(fp);
        return -3;
    }
    
    /* Read header */
    if (fread(&h->header, sizeof(g64_header_t), 1, fp) != 1) {
        free(h);
        fclose(fp);
        return -4;
    }
    
    /* Verify signature */
    if (memcmp(h->header.signature, "GCR-1541", 8) != 0) {
        free(h);
        fclose(fp);
        return -5;
    }
    
    h->fp = fp;
    h->readonly = readonly;
    
    /* Allocate track arrays */
    size_t num_tracks = h->header.track_count;
    h->track_offsets = calloc(num_tracks, sizeof(uint32_t));
    h->speed_offsets = calloc(num_tracks, sizeof(uint32_t));
    h->track_sizes = calloc(num_tracks, sizeof(uint16_t));
    
    if (!h->track_offsets || !h->speed_offsets || !h->track_sizes) {
        free(h->track_offsets);
        free(h->speed_offsets);
        free(h->track_sizes);
        free(h);
        fclose(fp);
        return -3;
    }
    
    /* Read track offset table */
    for (size_t i = 0; i < num_tracks; i++) {
        uint32_t offset;
        if (fread(&offset, sizeof(uint32_t), 1, fp) != 1) break;
        h->track_offsets[i] = offset;  /* Little-endian */
    }
    
    /* Read speed zone offset table */
    for (size_t i = 0; i < num_tracks; i++) {
        uint32_t offset;
        if (fread(&offset, sizeof(uint32_t), 1, fp) != 1) break;
        h->speed_offsets[i] = offset;
    }
    
    /* Read track sizes from track data headers */
    for (size_t i = 0; i < num_tracks; i++) {
        if (h->track_offsets[i] == 0) continue;
        
        fseek(fp, (long)h->track_offsets[i], SEEK_SET);
        uint16_t size;
        if (fread(&size, sizeof(uint16_t), 1, fp) == 1) {
            h->track_sizes[i] = size;
        }
    }
    
    *handle = h;
    return 0;
}

/**
 * @brief Close G64 image
 */
void uft_g64_close(g64_handle_t *h) {
    if (!h) return;
    
    if (h->fp) fclose(h->fp);
    free(h->track_offsets);
    free(h->speed_offsets);
    free(h->track_sizes);
    free(h);
}

/**
 * @brief Read G64 raw track (GCR encoded)
 */
ssize_t uft_g64_read_track(g64_handle_t *h, uint8_t track,
                           uint8_t *buffer, size_t buf_size) {
    if (!h || !buffer || track >= h->header.track_count) return -1;
    
    uint32_t offset = h->track_offsets[track];
    if (offset == 0) return 0;  /* Empty track */
    
    uint16_t size = h->track_sizes[track];
    if (size == 0 || size > buf_size) return -1;
    
    fseek(h->fp, (long)(offset + 2), SEEK_SET);  /* Skip size word */
    if (fread(buffer, 1, size, h->fp) != size) return -1;
    
    return (ssize_t)size;
}

/**
 * @brief Convert D64 to G64
 * 
 * Encodes D64 sector data to GCR format for G64.
 */
int uft_d64_to_g64(d64_handle_t *d64, const char *g64_path) {
    if (!d64 || !g64_path) return -1;
    
    FILE *fp = fopen(g64_path, "wb");
    if (!fp) return -1;
    
    /* Write G64 header */
    g64_header_t header;
    memcpy(header.signature, "GCR-1541", 8);
    header.version = 0;
    header.track_count = d64->tracks * 2;  /* Half-tracks */
    header.max_track_size = 7928;
    fwrite(&header, sizeof(header), 1, fp);
    
    /* Calculate offsets */
    uint32_t data_offset = (uint32_t)(sizeof(header) + 
                           header.track_count * 4 * 2);  /* Offset + speed tables */
    
    /* Write track offset table (placeholder) */
    uint32_t *offsets = calloc(header.track_count, sizeof(uint32_t));
    fwrite(offsets, sizeof(uint32_t), header.track_count, fp);
    
    /* Write speed zone table (placeholder) */
    uint32_t *speeds = calloc(header.track_count, sizeof(uint32_t));
    fwrite(speeds, sizeof(uint32_t), header.track_count, fp);
    
    /* Encode and write tracks */
    uint8_t sector_buf[256];
    uint8_t gcr_track[8192];
    
    for (uint8_t track = 1; track <= d64->tracks; track++) {
        uint8_t half_track = (track - 1) * 2;
        size_t gcr_offset = 0;
        
        /* Get sectors for this track */
        uint8_t sectors = d64_sectors_per_track[track];
        
        /* Determine speed zone */
        uint8_t speed_zone;
        if (track <= 17) speed_zone = 3;
        else if (track <= 24) speed_zone = 2;
        else if (track <= 30) speed_zone = 1;
        else speed_zone = 0;
        
        /* Build GCR track */
        for (uint8_t sector = 0; sector < sectors; sector++) {
            /* Read sector data */
            ssize_t read = uft_d64_read_sector(d64, track, sector, sector_buf);
            if (read != 256) continue;
            
            /* Add gap */
            for (int i = 0; i < 8; i++) {
                gcr_track[gcr_offset++] = 0x55;
            }
            
            /* Encode header */
            uint8_t disk_id[2] = {0x41, 0x41};  /* Default ID */
            uft_c64_gcr_encode_header(track, sector, disk_id, 
                                       gcr_track + gcr_offset);
            gcr_offset += 10 + 5;  /* Header + sync */
            
            /* Add header gap */
            for (int i = 0; i < 9; i++) {
                gcr_track[gcr_offset++] = 0x55;
            }
            
            /* Encode data sector */
            uft_c64_gcr_encode_sector(sector_buf, gcr_track + gcr_offset,
                                       UFT_C64_BLOCK_DATA);
            gcr_offset += 325 + 5;  /* Data + sync */
        }
        
        /* Write track */
        offsets[half_track] = data_offset;
        speeds[half_track] = speed_zone;
        
        uint16_t track_size = (uint16_t)gcr_offset;
        fseek(fp, (long)data_offset, SEEK_SET);
        fwrite(&track_size, sizeof(uint16_t), 1, fp);
        fwrite(gcr_track, 1, gcr_offset, fp);
        
        data_offset += 2 + (uint32_t)gcr_offset;
    }
    
    /* Write updated offset tables */
    fseek(fp, sizeof(header), SEEK_SET);
    fwrite(offsets, sizeof(uint32_t), header.track_count, fp);
    fwrite(speeds, sizeof(uint32_t), header.track_count, fp);
    
    free(offsets);
    free(speeds);
    fclose(fp);
    
    return 0;
}

/**
 * @brief Convert G64 to D64
 * 
 * Decodes GCR data to sector format.
 */
int uft_g64_to_d64(g64_handle_t *g64, const char *d64_path) {
    if (!g64 || !d64_path) return -1;
    
    FILE *fp = fopen(d64_path, "wb");
    if (!fp) return -1;
    
    uint8_t gcr_track[8192];
    uint8_t sector_buf[256];
    
    /* Allocate D64 data */
    uint8_t tracks = (g64->header.track_count + 1) / 2;
    if (tracks > 40) tracks = 40;
    
    size_t d64_size = (tracks == 35) ? UFT_D64_SIZE_35 : UFT_D64_SIZE_40;
    uint8_t *d64_data = calloc(1, d64_size);
    if (!d64_data) {
        fclose(fp);
        return -3;
    }
    
    /* Decode tracks */
    for (uint8_t track = 1; track <= tracks; track++) {
        uint8_t half_track = (track - 1) * 2;
        
        ssize_t gcr_len = uft_g64_read_track(g64, half_track, gcr_track, sizeof(gcr_track));
        if (gcr_len <= 0) continue;
        
        uint8_t sectors = d64_sectors_per_track[track];
        
        /* Find and decode each sector */
        for (uint8_t sector = 0; sector < sectors; sector++) {
            /* Search for sector header in GCR data */
            /* This is simplified - real implementation needs proper GCR parsing */
            
            uint8_t block_id;
            if (uft_c64_gcr_decode_sector(gcr_track, sector_buf, &block_id) == UFT_GCR_OK) {
                ssize_t offset = uft_d64_sector_offset(track, sector);
                if (offset >= 0) {
                    memcpy(d64_data + offset, sector_buf, 256);
                }
            }
        }
    }
    
    fwrite(d64_data, 1, d64_size, fp);
    free(d64_data);
    fclose(fp);
    
    return 0;
}
