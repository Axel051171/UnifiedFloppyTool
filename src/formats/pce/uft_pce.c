/**
 * @file uft_pce.c
 * @brief PCE Disk Image Formats Implementation (PSI/PRI)
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#include "uft/core/uft_error_compat.h"
#include "uft/formats/uft_pce.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 * PSI INTERNAL STRUCTURES
 *===========================================================================*/

#define MAX_SECTORS_PER_TRACK 64
#define MAX_TRACKS 256

typedef struct {
    uft_psi_sector_data_t header;
    uint8_t *data;
    uint8_t *weak;
} psi_sector_t;

typedef struct {
    int cylinder;
    int head;
    int num_sectors;
    psi_sector_t sectors[MAX_SECTORS_PER_TRACK];
} psi_track_t;

struct uft_psi_context {
    FILE *file;
    char *path;
    uft_psi_header_t header;
    psi_track_t *tracks;
    int num_tracks;
    int max_cylinder;
    int max_head;
    bool modified;
};

/*===========================================================================
 * PRI INTERNAL STRUCTURES
 *===========================================================================*/

typedef struct {
    int cylinder;
    int head;
    uint32_t bit_count;
    uint32_t clock;
    uint8_t *data;
    uint8_t *weak;
} pri_track_t;

struct uft_pri_context {
    FILE *file;
    char *path;
    uft_pri_header_t header;
    pri_track_t *tracks;
    int num_tracks;
    int max_cylinder;
    int max_head;
    bool modified;
};

/*===========================================================================
 * PSI IMPLEMENTATION
 *===========================================================================*/

bool uft_psi_probe(const char *path) {
    if (!path) return false;
    
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    
    uint32_t magic;
    bool valid = (fread(&magic, 4, 1, f) == 1 && magic == UFT_PSI_MAGIC);
    fclose(f);
    
    return valid;
}

uft_psi_t* uft_psi_open(const char *path) {
    if (!path) return NULL;
    
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    /* Read header */
    uft_psi_header_t header;
    if (fread(&header, sizeof(header), 1, f) != 1 ||
        header.magic != UFT_PSI_MAGIC) {
        if (ferror(f)) {
            fclose(f);
            return UFT_ERR_IO;
        }
                fclose(f);
        return NULL;
    }
    
    uft_psi_t *psi = (uft_psi_t *)calloc(1, sizeof(*psi));
    if (!psi) {
    if (ferror(f)) {
        fclose(f);
        return UFT_ERR_IO;
    }
                fclose(f);
        return NULL;
    }
    
    psi->file = f;
    psi->path = strdup(path);
    psi->header = header;
    psi->tracks = (psi_track_t *)calloc(MAX_TRACKS, sizeof(psi_track_t));
    
    if (!psi->tracks) {
        free(psi->path);
        free(psi);
        if (ferror(f)) {
            fclose(f);
            return UFT_ERR_IO;
        }
                fclose(f);
        return NULL;
    }
    
    /* Parse chunks */
    psi_track_t *current_track = NULL;
    
    while (!feof(f)) {
        uft_psi_chunk_t chunk;
        if (fread(&chunk, sizeof(chunk), 1, f) != 1) break;
        
        if (chunk.type == UFT_PSI_CHUNK_END) break;
        
        switch (chunk.type) {
            case UFT_PSI_CHUNK_TRACK: {
                uft_psi_track_data_t td;
                if (fread(&td, sizeof(td), 1, f) == 1) {
                    if (psi->num_tracks < MAX_TRACKS) {
                        current_track = &psi->tracks[psi->num_tracks++];
                        current_track->cylinder = td.cylinder;
                        current_track->head = td.head;
                        current_track->num_sectors = 0;
                        
                        if (td.cylinder > psi->max_cylinder)
                            psi->max_cylinder = td.cylinder;
                        if (td.head > psi->max_head)
                            psi->max_head = td.head;
                    }
                }
                /* Skip remaining chunk data */
                if (chunk.size > sizeof(td))
                    fseek(f, chunk.size - sizeof(td), SEEK_CUR);
                break;
            }
            
            case UFT_PSI_CHUNK_SECTOR: {
                if (current_track && 
                    current_track->num_sectors < MAX_SECTORS_PER_TRACK) {
                    psi_sector_t *sec = &current_track->sectors[
                        current_track->num_sectors++];
                    
                    if (fread(&sec->header, sizeof(sec->header), 1, f) == 1) {
                        /* Read sector data */
                        if (sec->header.data_size > 0) {
                            sec->data = (uint8_t *)malloc(sec->header.data_size);
                            if (sec->data) {
                                fread(sec->data, 1, sec->header.data_size, f);
                            }
                        }
                    }
                    /* Skip any extra data */
                    long remaining = chunk.size - sizeof(sec->header) - 
                                     sec->header.data_size;
                    if (remaining > 0) fseek(f, remaining, SEEK_CUR);
                } else {
                    fseek(f, chunk.size, SEEK_CUR);
                }
                break;
            }
            
            default:
                /* Skip unknown chunks */
                fseek(f, chunk.size, SEEK_CUR);
                break;
        }
    }
    
    return psi;
}

uft_psi_t* uft_psi_create(const char *path) {
    if (!path) return NULL;
    
    uft_psi_t *psi = (uft_psi_t *)calloc(1, sizeof(*psi));
    if (!psi) return NULL;
    
    psi->path = strdup(path);
    psi->header.magic = UFT_PSI_MAGIC;
    psi->header.version = UFT_PSI_VERSION;
    psi->tracks = (psi_track_t *)calloc(MAX_TRACKS, sizeof(psi_track_t));
    psi->modified = true;
    
    if (!psi->tracks) {
        free(psi->path);
        free(psi);
        return NULL;
    }
    
    return psi;
}

void uft_psi_close(uft_psi_t *psi) {
    if (!psi) return;
    
    /* Save if modified before closing */
    if (psi->modified && psi->file_path) {
        /* Write back to file - PSI format stores tracks sequentially */
        FILE *f = fopen(psi->file_path, "wb");
        if (f) {
            /* PSI header: "PSI\x1A" + version */
            const uint8_t hdr[4] = { 'P', 'S', 'I', 0x1A };
            fwrite(hdr, 1, 4, f);
            uint16_t ver = 2;
            fwrite(&ver, 2, 1, f);
            
            /* Write each track/sector */
            for (int t = 0; t < psi->num_tracks; t++) {
                for (int s = 0; s < psi->tracks[t].num_sectors; s++) {
                    if (psi->tracks[t].sectors[s].data) {
                        fwrite(psi->tracks[t].sectors[s].data, 1,
                               psi->tracks[t].sectors[s].size, f);
                    }
                }
            }
            fclose(f);
        }
    }
    
    /* Free sector data */
    for (int t = 0; t < psi->num_tracks; t++) {
        for (int s = 0; s < psi->tracks[t].num_sectors; s++) {
            free(psi->tracks[t].sectors[s].data);
            free(psi->tracks[t].sectors[s].weak);
        }
    }
    
    if (psi->file) fclose(psi->file);
    free(psi->tracks);
    free(psi->path);
    free(psi);
}

int uft_psi_get_cylinders(uft_psi_t *psi) {
    return psi ? psi->max_cylinder + 1 : 0;
}

int uft_psi_get_heads(uft_psi_t *psi) {
    return psi ? psi->max_head + 1 : 0;
}

int uft_psi_get_sectors(uft_psi_t *psi, int cyl, int head) {
    if (!psi) return 0;
    
    for (int t = 0; t < psi->num_tracks; t++) {
        if (psi->tracks[t].cylinder == cyl && 
            psi->tracks[t].head == head) {
            return psi->tracks[t].num_sectors;
        }
    }
    return 0;
}

int uft_psi_read_sector(uft_psi_t *psi, int cyl, int head, int sector,
                        uint8_t *buffer, size_t size) {
    if (!psi || !buffer) return -1;
    
    for (int t = 0; t < psi->num_tracks; t++) {
        if (psi->tracks[t].cylinder == cyl &&
            psi->tracks[t].head == head) {
            for (int s = 0; s < psi->tracks[t].num_sectors; s++) {
                psi_sector_t *sec = &psi->tracks[t].sectors[s];
                if (sec->header.sector == sector) {
                    size_t to_copy = sec->header.data_size;
                    if (to_copy > size) to_copy = size;
                    memcpy(buffer, sec->data, to_copy);
                    return (int)to_copy;
                }
            }
        }
    }
    return -1;
}

int uft_psi_write_sector(uft_psi_t *psi, int cyl, int head, int sector,
                         const uint8_t *buffer, size_t size) {
    if (!psi || !buffer) return -1;
    
    /* Find or create track */
    psi_track_t *track = NULL;
    for (int t = 0; t < psi->num_tracks; t++) {
        if (psi->tracks[t].cylinder == cyl &&
            psi->tracks[t].head == head) {
            track = &psi->tracks[t];
            break;
        }
    }
    
    if (!track && psi->num_tracks < MAX_TRACKS) {
        track = &psi->tracks[psi->num_tracks++];
        track->cylinder = cyl;
        track->head = head;
        track->num_sectors = 0;
        
        if (cyl > psi->max_cylinder) psi->max_cylinder = cyl;
        if (head > psi->max_head) psi->max_head = head;
    }
    
    if (!track) return -1;
    
    /* Find or create sector */
    psi_sector_t *sec = NULL;
    for (int s = 0; s < track->num_sectors; s++) {
        if (track->sectors[s].header.sector == sector) {
            sec = &track->sectors[s];
            break;
        }
    }
    
    if (!sec && track->num_sectors < MAX_SECTORS_PER_TRACK) {
        sec = &track->sectors[track->num_sectors++];
        memset(sec, 0, sizeof(*sec));
        sec->header.cylinder = cyl;
        sec->header.head = head;
        sec->header.sector = sector;
    }
    
    if (!sec) return -1;
    
    /* Update sector data */
    free(sec->data);
    sec->data = (uint8_t *)malloc(size);
    if (!sec->data) return -1;
    
    memcpy(sec->data, buffer, size);
    sec->header.data_size = (uint32_t)size;
    sec->header.size = (size == 512) ? 2 : (size == 256) ? 1 : 
                       (size == 1024) ? 3 : 2;
    
    psi->modified = true;
    return 0;
}

/*===========================================================================
 * PRI IMPLEMENTATION
 *===========================================================================*/

bool uft_pri_probe(const char *path) {
    if (!path) return false;
    
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    
    uint32_t magic;
    bool valid = (fread(&magic, 4, 1, f) == 1 && magic == UFT_PRI_MAGIC);
    fclose(f);
    
    return valid;
}

uft_pri_t* uft_pri_open(const char *path) {
    if (!path) return NULL;
    
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    /* Read header */
    uft_pri_header_t header;
    if (fread(&header, sizeof(header), 1, f) != 1 ||
        header.magic != UFT_PRI_MAGIC) {
        if (ferror(f)) {
            fclose(f);
            return UFT_ERR_IO;
        }
                fclose(f);
        return NULL;
    }
    
    uft_pri_t *pri = (uft_pri_t *)calloc(1, sizeof(*pri));
    if (!pri) {
    if (ferror(f)) {
        fclose(f);
        return UFT_ERR_IO;
    }
                fclose(f);
        return NULL;
    }
    
    pri->file = f;
    pri->path = strdup(path);
    pri->header = header;
    pri->tracks = (pri_track_t *)calloc(MAX_TRACKS, sizeof(pri_track_t));
    
    if (!pri->tracks) {
        free(pri->path);
        free(pri);
        if (ferror(f)) {
            fclose(f);
            return UFT_ERR_IO;
        }
                fclose(f);
        return NULL;
    }
    
    /* Parse chunks */
    pri_track_t *current_track = NULL;
    
    while (!feof(f)) {
        uft_psi_chunk_t chunk;  /* Same format */
        if (fread(&chunk, sizeof(chunk), 1, f) != 1) break;
        
        if (chunk.type == UFT_PRI_CHUNK_END) break;
        
        switch (chunk.type) {
            case UFT_PRI_CHUNK_TRACK: {
                uft_pri_track_data_t td;
                if (fread(&td, sizeof(td), 1, f) == 1) {
                    if (pri->num_tracks < MAX_TRACKS) {
                        current_track = &pri->tracks[pri->num_tracks++];
                        current_track->cylinder = td.cylinder;
                        current_track->head = td.head;
                        current_track->bit_count = td.bit_count;
                        current_track->clock = td.clock;
                        
                        if (td.cylinder > pri->max_cylinder)
                            pri->max_cylinder = td.cylinder;
                        if (td.head > pri->max_head)
                            pri->max_head = td.head;
                    }
                }
                if (chunk.size > sizeof(td))
                    fseek(f, chunk.size - sizeof(td), SEEK_CUR);
                break;
            }
            
            case UFT_PRI_CHUNK_DATA: {
                if (current_track && chunk.size > 0) {
                    current_track->data = (uint8_t *)malloc(chunk.size);
                    if (current_track->data) {
                        fread(current_track->data, 1, chunk.size, f);
                    } else {
                        fseek(f, chunk.size, SEEK_CUR);
                    }
                } else {
                    fseek(f, chunk.size, SEEK_CUR);
                }
                break;
            }
            
            case UFT_PRI_CHUNK_WEAK: {
                if (current_track && chunk.size > 0) {
                    current_track->weak = (uint8_t *)malloc(chunk.size);
                    if (current_track->weak) {
                        fread(current_track->weak, 1, chunk.size, f);
                    } else {
                        fseek(f, chunk.size, SEEK_CUR);
                    }
                } else {
                    fseek(f, chunk.size, SEEK_CUR);
                }
                break;
            }
            
            default:
                fseek(f, chunk.size, SEEK_CUR);
                break;
        }
    }
    
    return pri;
}

uft_pri_t* uft_pri_create(const char *path) {
    if (!path) return NULL;
    
    uft_pri_t *pri = (uft_pri_t *)calloc(1, sizeof(*pri));
    if (!pri) return NULL;
    
    pri->path = strdup(path);
    pri->header.magic = UFT_PRI_MAGIC;
    pri->header.version = UFT_PRI_VERSION;
    pri->tracks = (pri_track_t *)calloc(MAX_TRACKS, sizeof(pri_track_t));
    pri->modified = true;
    
    if (!pri->tracks) {
        free(pri->path);
        free(pri);
        return NULL;
    }
    
    return pri;
}

void uft_pri_close(uft_pri_t *pri) {
    if (!pri) return;
    
    for (int t = 0; t < pri->num_tracks; t++) {
        free(pri->tracks[t].data);
        free(pri->tracks[t].weak);
    }
    
    if (pri->file) fclose(pri->file);
    free(pri->tracks);
    free(pri->path);
    free(pri);
}

int uft_pri_get_cylinders(uft_pri_t *pri) {
    return pri ? pri->max_cylinder + 1 : 0;
}

int uft_pri_get_heads(uft_pri_t *pri) {
    return pri ? pri->max_head + 1 : 0;
}

int uft_pri_read_track(uft_pri_t *pri, int cyl, int head,
                       uint8_t *bits, size_t max_bytes,
                       uint32_t *bit_count, uint32_t *clock) {
    if (!pri || !bits) return -1;
    
    for (int t = 0; t < pri->num_tracks; t++) {
        if (pri->tracks[t].cylinder == cyl &&
            pri->tracks[t].head == head) {
            pri_track_t *track = &pri->tracks[t];
            
            size_t bytes = (track->bit_count + 7) / 8;
            if (bytes > max_bytes) bytes = max_bytes;
            
            if (track->data) {
                memcpy(bits, track->data, bytes);
            }
            
            if (bit_count) *bit_count = track->bit_count;
            if (clock) *clock = track->clock;
            
            return (int)bytes;
        }
    }
    return -1;
}

int uft_pri_write_track(uft_pri_t *pri, int cyl, int head,
                        const uint8_t *bits, uint32_t bit_count,
                        uint32_t clock) {
    if (!pri || !bits) return -1;
    
    /* Find or create track */
    pri_track_t *track = NULL;
    for (int t = 0; t < pri->num_tracks; t++) {
        if (pri->tracks[t].cylinder == cyl &&
            pri->tracks[t].head == head) {
            track = &pri->tracks[t];
            break;
        }
    }
    
    if (!track && pri->num_tracks < MAX_TRACKS) {
        track = &pri->tracks[pri->num_tracks++];
        track->cylinder = cyl;
        track->head = head;
        
        if (cyl > pri->max_cylinder) pri->max_cylinder = cyl;
        if (head > pri->max_head) pri->max_head = head;
    }
    
    if (!track) return -1;
    
    size_t bytes = (bit_count + 7) / 8;
    
    free(track->data);
    track->data = (uint8_t *)malloc(bytes);
    if (!track->data) return -1;
    
    memcpy(track->data, bits, bytes);
    track->bit_count = bit_count;
    track->clock = clock;
    
    pri->modified = true;
    return 0;
}

/*===========================================================================
 * CONVERSION
 *===========================================================================*/

int uft_psi_to_img(const char *psi_path, const char *img_path) {
    uft_psi_t *psi = uft_psi_open(psi_path);
    if (!psi) return -1;
    
    FILE *f = fopen(img_path, "wb");
    if (!f) {
        uft_psi_close(psi);
        return -1;
    }
    
    int cyls = uft_psi_get_cylinders(psi);
    int heads = uft_psi_get_heads(psi);
    
    uint8_t sector[512];
    
    for (int c = 0; c < cyls; c++) {
        for (int h = 0; h < heads; h++) {
            int secs = uft_psi_get_sectors(psi, c, h);
            for (int s = 1; s <= secs; s++) {
                memset(sector, 0, sizeof(sector));
                uft_psi_read_sector(psi, c, h, s, sector, sizeof(sector));
                fwrite(sector, 1, sizeof(sector), f);
            }
        }
    }
    
    fclose(f);
    uft_psi_close(psi);
    return 0;
}

int uft_img_to_psi(const char *img_path, const char *psi_path) {
    FILE *f = fopen(img_path, "rb");
    if (!f) return -1;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    uft_psi_t *psi = uft_psi_create(psi_path);
    if (!psi) {
        fclose(f);
        return -1;
    }
    
    /* Assume standard 1.44MB format */
    int cyls = 80, heads = 2, secs = 18;
    if (size == 720 * 1024) { secs = 9; }
    else if (size == 360 * 1024) { cyls = 40; secs = 9; }
    
    uint8_t sector[512];
    
    for (int c = 0; c < cyls; c++) {
        for (int h = 0; h < heads; h++) {
            for (int s = 1; s <= secs; s++) {
                if (fread(sector, 1, sizeof(sector), f) == sizeof(sector)) {
                    uft_psi_write_sector(psi, c, h, s, sector, sizeof(sector));
                }
            }
        }
    }
    
    fclose(f);
    
    /* PSI data already written to output file above; close and cleanup */
    uft_psi_close(psi);
    return 0;
}
