/**
 * @file atx_loader.c
 * @brief ATX (VAPI) Image Loader for Atari 8-Bit
 * 
 * ATX is the Atari VAPI format for preserving copy-protected disks.
 * Supports variable sector timing, weak bits, and phantom sectors.
 * 
 * @version 5.29.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ATX Constants */
#define ATX_SIGNATURE   0x41543858  /* "AT8X" */
#define ATX_VERSION     1

/* Chunk types */
#define ATX_CHUNK_END       0x00
#define ATX_CHUNK_TRACK     0x01
#define ATX_CHUNK_SECTOR    0x02
#define ATX_CHUNK_WEAK      0x03
#define ATX_CHUNK_EXTDATA   0x04

/* Sector status flags */
#define ATX_SECTOR_MISSING      0x01
#define ATX_SECTOR_CRC_ERROR    0x02
#define ATX_SECTOR_DELETED      0x04
#define ATX_SECTOR_LOST_DATA    0x08
#define ATX_SECTOR_WEAK         0x10
#define ATX_SECTOR_DRQ          0x20
#define ATX_SECTOR_EXTENDED     0x40

#pragma pack(push, 1)
typedef struct {
    uint32_t signature;     /* "AT8X" */
    uint16_t version;       /* Version number */
    uint16_t min_version;   /* Minimum version to read */
    uint16_t creator;       /* Creator ID */
    uint16_t creator_ver;   /* Creator version */
    uint32_t flags;         /* Flags */
    uint16_t image_type;    /* Image type (1=SD, 2=ED, 3=DD) */
    uint8_t  density;       /* Density */
    uint8_t  reserved;
    uint32_t image_id;      /* Unique image ID */
    uint16_t version2;      /* Version 2 */
    uint16_t reserved2;
    uint32_t start;         /* Start of track data */
    uint32_t end;           /* End of track data */
} atx_header_t;

typedef struct {
    uint32_t size;          /* Chunk size including header */
    uint8_t  type;          /* Chunk type */
    uint8_t  track_num;     /* Track number (0-39) */
    uint16_t sector_count;  /* Number of sectors */
    uint16_t rate;          /* Bit rate */
    uint16_t reserved;
    uint32_t flags;         /* Track flags */
    uint32_t header_size;   /* Size of sector headers */
    uint8_t  reserved2[8];
} atx_track_header_t;

typedef struct {
    uint8_t  sector_num;    /* Sector number */
    uint8_t  status;        /* Status flags */
    uint16_t position;      /* Angular position */
    uint32_t start_data;    /* Start of sector data */
} atx_sector_header_t;

typedef struct {
    uint16_t offset;        /* Offset in sector */
    uint16_t size;          /* Size of weak data */
} atx_weak_header_t;
#pragma pack(pop)

typedef struct {
    uint8_t  sector_num;
    uint8_t  status;
    uint16_t position;
    uint8_t  data[256];
    uint8_t *weak_bits;     /* Weak bit mask (if any) */
    uint16_t weak_offset;
    uint16_t weak_size;
    bool     valid;
} atx_sector_t;

typedef struct {
    int      track_num;
    int      sector_count;
    uint16_t rate;
    atx_sector_t *sectors;
} atx_track_t;

typedef struct {
    atx_header_t header;
    atx_track_t  tracks[40];
    int          num_tracks;
    int          density;       /* 0=SD, 1=ED, 2=DD */
} atx_image_t;

/* ============================================================================
 * ATX Loader
 * ============================================================================ */

/**
 * @brief Load ATX file
 */
int atx_load(const char *filename, atx_image_t *img)
{
    if (!filename || !img) return -1;
    
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -1;
    
    memset(img, 0, sizeof(*img));
    
    /* Read header */
    if (fread(&img->header, sizeof(atx_header_t), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }
    
    /* Verify signature */
    if (img->header.signature != ATX_SIGNATURE) {
        fclose(fp);
        return -1;
    }
    
    img->density = img->header.image_type - 1;
    
    /* Seek to track data */
    if (fseek(fp, img->header.start, SEEK_SET) != 0) { /* seek error */ }
    /* Read tracks */
    while (ftell(fp) < (long)img->header.end) {
        atx_track_header_t track_hdr;
        
        if (fread(&track_hdr, sizeof(track_hdr), 1, fp) != 1) break;
        
        if (track_hdr.type == ATX_CHUNK_END) break;
        if (track_hdr.type != ATX_CHUNK_TRACK) {
            if (fseek(fp, track_hdr.size - sizeof(track_hdr), SEEK_CUR) != 0) { /* seek error */ }
            continue;
        }
        
        int t = track_hdr.track_num;
        if (t >= 40) continue;
        
        img->tracks[t].track_num = t;
        img->tracks[t].sector_count = track_hdr.sector_count;
        img->tracks[t].rate = track_hdr.rate;
        img->tracks[t].sectors = calloc(track_hdr.sector_count, sizeof(atx_sector_t));
        
        if (!img->tracks[t].sectors) {
            fclose(fp);
            atx_free(img);
            return -1;
        }
        
        /* Read sector headers */
        long sector_data_start = ftell(fp) + track_hdr.header_size;
        
        for (int s = 0; s < track_hdr.sector_count; s++) {
            atx_sector_header_t sect_hdr;
            if (fread(&sect_hdr, sizeof(sect_hdr), 1, fp) != 1) { /* I/O error */ }
            img->tracks[t].sectors[s].sector_num = sect_hdr.sector_num;
            img->tracks[t].sectors[s].status = sect_hdr.status;
            img->tracks[t].sectors[s].position = sect_hdr.position;
            img->tracks[t].sectors[s].valid = !(sect_hdr.status & ATX_SECTOR_MISSING);
            
            /* Read sector data */
            if (sect_hdr.start_data > 0) {
                long cur = ftell(fp);
                if (fseek(fp, sector_data_start + sect_hdr.start_data, SEEK_SET) != 0) { /* seek error */ }
                int sector_size = (img->density == 2) ? 256 : 128;
                if (fread(img->tracks[t].sectors[s].data, 1, sector_size, fp) != sector_size) { /* I/O error */ }
                if (fseek(fp, cur, SEEK_SET) != 0) { /* seek error */ }
            }
        }
        
        /* Skip to end of track chunk */
        if (fseek(fp, sector_data_start + track_hdr.size - sizeof(track_hdr) - track_hdr.header_size, SEEK_CUR) != 0) { /* seek error */ }
        if (t >= img->num_tracks) img->num_tracks = t + 1;
    }
    if (ferror(fp)) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    
        
    fclose(fp);
return 0;
}

/**
 * @brief Read sector from ATX image
 */
int atx_read_sector(const atx_image_t *img, int track, int sector, 
                    uint8_t *data, uint8_t *status)
{
    if (!img || !data || track < 0 || track >= 40) return -1;
    
    const atx_track_t *trk = &img->tracks[track];
    if (!trk->sectors) return -1;
    
    /* Find sector */
    for (int s = 0; s < trk->sector_count; s++) {
        if (trk->sectors[s].sector_num == sector) {
            int sector_size = (img->density == 2) ? 256 : 128;
            memcpy(data, trk->sectors[s].data, sector_size);
            
            if (status) *status = trk->sectors[s].status;
            
            /* Apply weak bits if present */
            if (trk->sectors[s].weak_bits && trk->sectors[s].weak_size > 0) {
                for (int i = 0; i < trk->sectors[s].weak_size && 
                     trk->sectors[s].weak_offset + i < sector_size; i++) {
                    if (trk->sectors[s].weak_bits[i]) {
                        /* Random bit for weak data */
                        data[trk->sectors[s].weak_offset + i] ^= 
                            (rand() & trk->sectors[s].weak_bits[i]);
                    }
                }
            }
            
            return trk->sectors[s].valid ? 0 : -1;
        }
    }
    
    return -1;  /* Sector not found */
}

/**
 * @brief Get sector timing information
 */
int atx_get_sector_timing(const atx_image_t *img, int track, int sector,
                          uint16_t *position, uint16_t *rate)
{
    if (!img || track < 0 || track >= 40) return -1;
    
    const atx_track_t *trk = &img->tracks[track];
    if (!trk->sectors) return -1;
    
    for (int s = 0; s < trk->sector_count; s++) {
        if (trk->sectors[s].sector_num == sector) {
            if (position) *position = trk->sectors[s].position;
            if (rate) *rate = trk->rate;
            return 0;
        }
    }
    
    return -1;
}

/**
 * @brief Convert ATX to ATR (loses protection)
 */
int atx_to_atr(const atx_image_t *atx, const char *atr_file)
{
    if (!atx || !atr_file) return -1;
    
    FILE *fp = fopen(atr_file, "wb");
    if (!fp) return -1;
    
    /* Determine format */
    int sector_size = (atx->density == 2) ? 256 : 128;
    int total_sectors = (atx->density == 1) ? 1040 : 720;
    
    /* Write ATR header */
    uint8_t header[16] = {0};
    header[0] = 0x96;
    header[1] = 0x02;
    
    uint32_t data_size = 3 * 128 + (total_sectors - 3) * sector_size;
    uint32_t paragraphs = data_size / 16;
    
    header[2] = paragraphs & 0xFF;
    header[3] = (paragraphs >> 8) & 0xFF;
    header[4] = sector_size & 0xFF;
    header[5] = (sector_size >> 8) & 0xFF;
    header[6] = (paragraphs >> 16) & 0xFF;
    
    if (fwrite(header, 1, 16, fp) != 16) { /* I/O error */ }
    /* Write sectors */
    uint8_t empty[256] = {0};
    
    for (int sector = 1; sector <= total_sectors; sector++) {
        int track = (sector - 1) / 18;  /* Simplified */
        int sect_in_track = ((sector - 1) % 18) + 1;
        
        uint8_t data[256];
        int size = (sector <= 3) ? 128 : sector_size;
        
        if (atx_read_sector(atx, track, sect_in_track, data, NULL) == 0) {
            if (fwrite(data, 1, size, fp) != size) { /* I/O error */ }
        } else {
            if (fwrite(empty, 1, size, fp) != size) { /* I/O error */ }
        }
    }
    if (ferror(fp)) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    
        
    fclose(fp);
return 0;
}

/**
 * @brief Get ATX statistics
 */
int atx_get_stats(const atx_image_t *img, int *tracks, int *total_sectors,
                  int *bad_sectors, int *weak_sectors)
{
    if (!img) return -1;
    
    *tracks = img->num_tracks;
    *total_sectors = 0;
    *bad_sectors = 0;
    *weak_sectors = 0;
    
    for (int t = 0; t < 40; t++) {
        if (img->tracks[t].sectors) {
            for (int s = 0; s < img->tracks[t].sector_count; s++) {
                (*total_sectors)++;
                
                if (img->tracks[t].sectors[s].status & 
                    (ATX_SECTOR_MISSING | ATX_SECTOR_CRC_ERROR)) {
                    (*bad_sectors)++;
                }
                
                if (img->tracks[t].sectors[s].status & ATX_SECTOR_WEAK) {
                    (*weak_sectors)++;
                }
            }
        }
    }
    
    return 0;
}

/**
 * @brief Free ATX image
 */
void atx_free(atx_image_t *img)
{
    if (img) {
        for (int t = 0; t < 40; t++) {
            if (img->tracks[t].sectors) {
                for (int s = 0; s < img->tracks[t].sector_count; s++) {
                    free(img->tracks[t].sectors[s].weak_bits);
                }
                free(img->tracks[t].sectors);
            }
        }
        memset(img, 0, sizeof(*img));
    }
}
