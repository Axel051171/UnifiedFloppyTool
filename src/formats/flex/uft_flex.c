/**
 * @file uft_flex.c
 * @brief FLEX/UniFLEX Disk Format Support
 * @version 4.1.2
 * 
 * FLEX - Floppy disk operating system for 6800/6809 processors
 * Used by: SWTPC, CoCo (with FLEX), Gimix, SMOKE SIGNAL, etc.
 */

#include "uft/formats/uft_flex.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Constants */
#define FLEX_SECTOR_SIZE        256
#define FLEX_SIR_TRACK          0
#define FLEX_SIR_SECTOR         3
#define FLEX_DIR_START_SECTOR   5

/* SIR offsets */
#define SIR_DISK_NAME           0x10
#define SIR_FIRST_USER_TRK      0x1D
#define SIR_FIRST_USER_SEC      0x1E
#define SIR_LAST_USER_TRK       0x1F
#define SIR_LAST_USER_SEC       0x20
#define SIR_FREE_SECTORS        0x21
#define SIR_MONTH               0x23
#define SIR_DAY                 0x24
#define SIR_YEAR                0x25
#define SIR_MAX_TRACK           0x26
#define SIR_MAX_SECTOR          0x27

#define DIR_ENTRY_SIZE          24

/* Standard geometries */
static const struct {
    int tracks, sectors, heads, sector_size;
    const char *name;
} g_flex_geom[] = {
    { 35, 10, 1, 256, "FLEX SS/SD 35T" },
    { 40, 10, 1, 256, "FLEX SS/SD 40T" },
    { 35, 18, 1, 256, "FLEX SS/DD 35T" },
    { 40, 18, 1, 256, "FLEX SS/DD 40T" },
    { 40, 18, 2, 256, "FLEX DS/DD 40T" },
    { 77, 15, 2, 256, "FLEX DS/DD 77T (8\")" },
    { 80, 18, 2, 256, "FLEX DS/DD 80T" },
    { 80, 36, 2, 256, "FLEX DS/HD 80T" },
    { 0, 0, 0, 0, NULL }
};

static uint16_t read_be16(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static void write_be16(uint8_t *p, uint16_t val) {
    p[0] = (uint8_t)((val >> 8) & 0xFF);
    p[1] = (uint8_t)(val & 0xFF);
}

static int calc_lsn(int track, int sector, int spt) {
    return track * spt + (sector - 1);
}

int uft_flex_probe(const uint8_t *data, size_t size, int *out_tracks, 
                   int *out_sectors, int *out_heads, const char **out_name) {
    if (!data || size < FLEX_SECTOR_SIZE * 4) return 0;
    
    for (int i = 0; g_flex_geom[i].name; i++) {
        size_t expected = (size_t)g_flex_geom[i].tracks * g_flex_geom[i].sectors * 
                          g_flex_geom[i].heads * g_flex_geom[i].sector_size;
        if (size != expected) continue;
        
        size_t sir_off = (size_t)calc_lsn(FLEX_SIR_TRACK, FLEX_SIR_SECTOR,
                                           g_flex_geom[i].sectors) * FLEX_SECTOR_SIZE;
        if (sir_off + FLEX_SECTOR_SIZE > size) continue;
        
        const uint8_t *sir = data + sir_off;
        uint8_t max_trk = sir[SIR_MAX_TRACK];
        uint8_t max_sec = sir[SIR_MAX_SECTOR];
        uint8_t first_trk = sir[SIR_FIRST_USER_TRK];
        uint8_t first_sec = sir[SIR_FIRST_USER_SEC];
        
        if (max_trk > 0 && max_trk <= 80 && max_sec > 0 && max_sec <= 40 &&
            first_trk < max_trk && first_sec > 0 && first_sec <= max_sec) {
            
            int valid = 1;
            for (int j = 0; j < 8; j++) {
                uint8_t c = sir[SIR_DISK_NAME + j];
                if (c != 0 && (c < 0x20 || c > 0x7E)) { valid = 0; break; }
            }
            
            if (valid) {
                if (out_tracks) *out_tracks = g_flex_geom[i].tracks;
                if (out_sectors) *out_sectors = g_flex_geom[i].sectors;
                if (out_heads) *out_heads = g_flex_geom[i].heads;
                if (out_name) *out_name = g_flex_geom[i].name;
                return 85;
            }
        }
    }
    return 0;
}

int uft_flex_read(const char *path, uft_flex_image_t **image) {
    if (!path || !image) return UFT_ERR_INVALID_PARAM;
    
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERR_IO;
    
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz <= 0) { fclose(f); return UFT_ERR_IO; }
    size_t size = (size_t)sz;
    if (fseek(f, 0, SEEK_SET) != 0) return UFT_ERR_IO;
    
    uint8_t *data = malloc(size);
    if (!data) { fclose(f); return UFT_ERR_MEMORY; }
    
    if (fread(data, 1, size, f) != size) {
        free(data); fclose(f); return UFT_ERR_IO;
    }
    fclose(f);
    
    int tracks, sectors, heads;
    const char *name;
    if (uft_flex_probe(data, size, &tracks, &sectors, &heads, &name) < 50) {
        free(data); return UFT_ERR_UNKNOWN_FORMAT;
    }
    
    uft_flex_image_t *img = calloc(1, sizeof(uft_flex_image_t));
    if (!img) { free(data); return UFT_ERR_MEMORY; }
    
    img->data = data;
    img->size = size;
    img->tracks = tracks;
    img->sectors = sectors;
    img->heads = heads;
    img->sector_size = FLEX_SECTOR_SIZE;
    
    size_t sir_off = (size_t)calc_lsn(FLEX_SIR_TRACK, FLEX_SIR_SECTOR,
                                       img->sectors) * FLEX_SECTOR_SIZE;
    const uint8_t *sir = data + sir_off;
    
    memcpy(img->volume_name, sir + SIR_DISK_NAME, 8);
    img->volume_name[8] = '\0';
    for (int i = 7; i >= 0 && img->volume_name[i] == ' '; i--)
        img->volume_name[i] = '\0';
    
    img->free_sectors = read_be16(sir + SIR_FREE_SECTORS);
    img->max_track = sir[SIR_MAX_TRACK];
    img->max_sector = sir[SIR_MAX_SECTOR];
    
    *image = img;
    return UFT_OK;
}

void uft_flex_free(uft_flex_image_t *image) {
    if (image) { free(image->data); free(image); }
}

int uft_flex_read_directory(uft_flex_image_t *image, uft_flex_dir_entry_t *entries,
                            int max_entries, int *count) {
    if (!image || !entries || !count) return UFT_ERR_INVALID_PARAM;
    *count = 0;
    
    int dir_track = 0, dir_sector = FLEX_DIR_START_SECTOR;
    
    while (dir_track != 0 || dir_sector != 0) {
        size_t offset = (size_t)calc_lsn(dir_track, dir_sector, image->sectors) *
                        (size_t)image->sector_size;
        if (offset + FLEX_SECTOR_SIZE > image->size) break;
        
        const uint8_t *sec = image->data + offset;
        int next_track = sec[0], next_sector = sec[1];
        
        for (int i = 0; i < 10 && *count < max_entries; i++) {
            const uint8_t *e = sec + 16 + (i * DIR_ENTRY_SIZE);
            if (e[0] == 0x00 || e[0] == 0xFF || e[0] == 0xE5) continue;
            
            uft_flex_dir_entry_t *de = &entries[*count];
            memcpy(de->filename, e, 8); de->filename[8] = '\0';
            memcpy(de->extension, e + 8, 3); de->extension[3] = '\0';
            de->attributes = e[11];
            de->start_track = e[13]; de->start_sector = e[14];
            de->end_track = e[15]; de->end_sector = e[16];
            de->sector_count = read_be16(e + 17);
            de->month = e[21]; de->day = e[22]; de->year = e[23];
            (*count)++;
        }
        dir_track = next_track; dir_sector = next_sector;
        if (dir_sector == 0 && dir_track == 0) break;
    }
    return UFT_OK;
}

int uft_flex_extract_file(uft_flex_image_t *image, const uft_flex_dir_entry_t *entry,
                          const char *output_path) {
    if (!image || !entry || !output_path) return UFT_ERR_INVALID_PARAM;
    
    FILE *out = fopen(output_path, "wb");
    if (!out) return UFT_ERR_IO;
    
    int track = entry->start_track, sector = entry->start_sector;
    int written = 0;
    
    while (track != 0 || sector != 0) {
        size_t offset = (size_t)calc_lsn(track, sector, image->sectors) *
                        (size_t)image->sector_size;
        if (offset + FLEX_SECTOR_SIZE > image->size) break;
        
        const uint8_t *sec = image->data + offset;
        int next_track = sec[0], next_sector = sec[1];
        
        fwrite(sec + 4, 1, FLEX_SECTOR_SIZE - 4, out);
        written++;
        
        track = next_track; sector = next_sector;
        if (written > entry->sector_count + 10) break;
    }
    if (ferror(out)) {
        fclose(out);
        return UFT_ERR_IO;
    }
    
        
    fclose(out);
return UFT_OK;
}

int uft_flex_get_info(uft_flex_image_t *image, char *buf, size_t bufsize) {
    if (!image || !buf) return UFT_ERR_INVALID_PARAM;
    
    snprintf(buf, bufsize,
             "FLEX Disk Image\nVolume: %s\nGeometry: %dx%dx%d\n"
             "Free: %d sectors\n",
             image->volume_name, image->tracks, image->sectors, image->heads,
             image->free_sectors);
    return UFT_OK;
}

int uft_flex_create(const char *path, int tracks, int sectors, int heads,
                    const char *volume_name) {
    if (!path) return UFT_ERR_INVALID_PARAM;
    if (tracks <= 0) tracks = 40;
    if (sectors <= 0) sectors = 18;
    if (heads <= 0) heads = 1;
    
    size_t size = (size_t)tracks * (size_t)sectors * (size_t)heads * FLEX_SECTOR_SIZE;
    uint8_t *data = calloc(1, size);
    if (!data) return UFT_ERR_MEMORY;
    
    size_t sir_off = (size_t)calc_lsn(FLEX_SIR_TRACK, FLEX_SIR_SECTOR, sectors) *
                     FLEX_SECTOR_SIZE;
    uint8_t *sir = data + sir_off;
    
    if (volume_name && volume_name[0])
        strncpy((char*)(sir + SIR_DISK_NAME), volume_name, 8);
    else
        memcpy(sir + SIR_DISK_NAME, "FLEXDISK", 8);
    
    sir[SIR_FIRST_USER_TRK] = 1; sir[SIR_FIRST_USER_SEC] = 1;
    sir[SIR_LAST_USER_TRK] = (uint8_t)(tracks - 1); 
    sir[SIR_LAST_USER_SEC] = (uint8_t)sectors;
    sir[SIR_MAX_TRACK] = (uint8_t)(tracks - 1); 
    sir[SIR_MAX_SECTOR] = (uint8_t)sectors;
    write_be16(sir + SIR_FREE_SECTORS, (uint16_t)((tracks - 1) * sectors * heads));
    sir[SIR_MONTH] = 1; sir[SIR_DAY] = 16; sir[SIR_YEAR] = 26;
    
    FILE *f = fopen(path, "wb");
    if (!f) { free(data); return UFT_ERR_IO; }
    fwrite(data, 1, size, f);
    fclose(f); free(data);
    return UFT_OK;
}
