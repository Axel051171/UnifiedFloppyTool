/**
 * @file ssd_loader.c
 * @brief SSD/DSD Image Loader/Writer for BBC Micro
 * 
 * SSD = Single-Sided Disk (100K or 200K)
 * DSD = Double-Sided Disk (200K or 400K)
 * Uses Acorn DFS (Disc Filing System)
 * 
 * @version 5.30.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* DFS Constants */
#define DFS_SECTOR_SIZE     256
#define DFS_SECTORS_TRACK   10
#define DFS_TRACKS_40       40
#define DFS_TRACKS_80       80

#define SSD_40_SIZE         102400   /* 40×10×256 */
#define SSD_80_SIZE         204800   /* 80×10×256 */
#define DSD_40_SIZE         204800   /* 40×2×10×256 */
#define DSD_80_SIZE         409600   /* 80×2×10×256 */

/* Catalog is in sectors 0 and 1 */
#define DFS_CAT_SECTOR0     0
#define DFS_CAT_SECTOR1     1
#define DFS_MAX_FILES       31

#pragma pack(push, 1)
typedef struct {
    char     name[7];       /* Filename (space padded) */
    char     directory;     /* Directory char (usually '$') */
    uint16_t load_addr_lo;  /* Load address low */
    uint16_t exec_addr_lo;  /* Exec address low */
    uint16_t length_lo;     /* Length low */
    uint8_t  start_sector;  /* Start sector (bits 0-7) */
    uint8_t  mixed;         /* Mixed: load_hi(2), length_hi(2), exec_hi(2), start_hi(2) */
} dfs_dirent_t;

typedef struct {
    char     title[8];      /* First 8 chars of title (in sector 0) */
    /* Sector 1 contains: */
    char     title2[4];     /* Last 4 chars of title */
    uint8_t  write_count;   /* Cycle number (BCD) */
    uint8_t  file_count;    /* Number of files × 8 */
    uint8_t  opt_sectors;   /* (opt × 16) | (sectors_hi) */
    uint8_t  sectors_lo;    /* Sectors on disk (low byte) */
} dfs_header_t;
#pragma pack(pop)

typedef enum {
    DFS_FORMAT_SSD_40,   /* 40 track single-sided */
    DFS_FORMAT_SSD_80,   /* 80 track single-sided */
    DFS_FORMAT_DSD_40,   /* 40 track double-sided */
    DFS_FORMAT_DSD_80    /* 80 track double-sided */
} dfs_format_t;

typedef struct {
    uint8_t    *data;
    size_t      size;
    dfs_format_t format;
    int         tracks;
    int         sides;
    bool        is_dsd;
} dfs_image_t;

/* ============================================================================
 * DFS Loader
 * ============================================================================ */

/**
 * @brief Detect format from file size
 */
static dfs_format_t dfs_detect_format(size_t size, const char *ext)
{
    bool is_dsd = (ext && (strcasecmp(ext, ".dsd") == 0));
    
    if (size == SSD_40_SIZE && !is_dsd) return DFS_FORMAT_SSD_40;
    if (size == SSD_80_SIZE && !is_dsd) return DFS_FORMAT_SSD_80;
    if (size == DSD_40_SIZE || (size == SSD_80_SIZE && is_dsd)) return DFS_FORMAT_DSD_40;
    if (size == DSD_80_SIZE) return DFS_FORMAT_DSD_80;
    
    /* Default based on size */
    if (size <= SSD_40_SIZE) return DFS_FORMAT_SSD_40;
    if (size <= SSD_80_SIZE) return is_dsd ? DFS_FORMAT_DSD_40 : DFS_FORMAT_SSD_80;
    return DFS_FORMAT_DSD_80;
}

/**
 * @brief Create empty DFS image
 */
int dfs_create(dfs_image_t *img, dfs_format_t format)
{
    if (!img) return -1;
    
    memset(img, 0, sizeof(*img));
    img->format = format;
    
    switch (format) {
        case DFS_FORMAT_SSD_40:
            img->tracks = 40;
            img->sides = 1;
            img->size = SSD_40_SIZE;
            img->is_dsd = false;
            break;
        case DFS_FORMAT_SSD_80:
            img->tracks = 80;
            img->sides = 1;
            img->size = SSD_80_SIZE;
            img->is_dsd = false;
            break;
        case DFS_FORMAT_DSD_40:
            img->tracks = 40;
            img->sides = 2;
            img->size = DSD_40_SIZE;
            img->is_dsd = true;
            break;
        case DFS_FORMAT_DSD_80:
            img->tracks = 80;
            img->sides = 2;
            img->size = DSD_80_SIZE;
            img->is_dsd = true;
            break;
    }
    
    img->data = calloc(1, img->size);
    return img->data ? 0 : -1;
}

/**
 * @brief Load SSD/DSD file
 */
int dfs_load(const char *filename, dfs_image_t *img)
{
    if (!filename || !img) return -1;
    
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -1;
    
    if (fseek(fp, 0, SEEK_END) != 0) { /* seek error */ }
    size_t size = ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) { /* seek error */ }
    /* Get extension */
    const char *ext = strrchr(filename, '.');
    
    dfs_format_t format = dfs_detect_format(size, ext);
    
    if (dfs_create(img, format) != 0) {
        fclose(fp);
        return -1;
    }
    
    /* Read data */
    size_t read_size = (size < img->size) ? size : img->size;
    if (fread(img->data, 1, read_size, fp) != read_size) { /* I/O error */ }
    fclose(fp);
    
    return 0;
}

/**
 * @brief Get sector offset
 */
static size_t dfs_sector_offset(const dfs_image_t *img, int track, int side,
                                int sector)
{
    if (img->is_dsd) {
        /* DSD: interleaved tracks (side 0 track 0, side 1 track 0, ...) */
        int logical_track = track * 2 + side;
        return logical_track * DFS_SECTORS_TRACK * DFS_SECTOR_SIZE + 
               sector * DFS_SECTOR_SIZE;
    } else {
        /* SSD: sequential */
        return track * DFS_SECTORS_TRACK * DFS_SECTOR_SIZE + 
               sector * DFS_SECTOR_SIZE;
    }
}

/**
 * @brief Read sector
 */
int dfs_read_sector(const dfs_image_t *img, int track, int side, int sector,
                    uint8_t *data)
{
    if (!img || !img->data || !data) return -1;
    
    size_t offset = dfs_sector_offset(img, track, side, sector);
    if (offset + DFS_SECTOR_SIZE > img->size) return -1;
    
    memcpy(data, img->data + offset, DFS_SECTOR_SIZE);
    return 0;
}

/**
 * @brief Write sector
 */
int dfs_write_sector(dfs_image_t *img, int track, int side, int sector,
                     const uint8_t *data)
{
    if (!img || !img->data || !data) return -1;
    
    size_t offset = dfs_sector_offset(img, track, side, sector);
    if (offset + DFS_SECTOR_SIZE > img->size) return -1;
    
    memcpy(img->data + offset, data, DFS_SECTOR_SIZE);
    return 0;
}

/**
 * @brief Format disk with DFS
 */
int dfs_format(dfs_image_t *img, const char *title)
{
    if (!img || !img->data) return -1;
    
    /* Clear disk */
    memset(img->data, 0, img->size);
    
    /* Setup catalog sector 0 */
    uint8_t cat0[DFS_SECTOR_SIZE] = {0};
    memset(cat0, ' ', 8);  /* Title (first 8 chars) */
    if (title) {
        size_t len = strlen(title);
        if (len > 8) len = 8;
        memcpy(cat0, title, len);
    }
    
    /* Setup catalog sector 1 */
    uint8_t cat1[DFS_SECTOR_SIZE] = {0};
    memset(cat1, ' ', 4);  /* Title (last 4 chars) */
    if (title && strlen(title) > 8) {
        size_t len = strlen(title) - 8;
        if (len > 4) len = 4;
        memcpy(cat1, title + 8, len);
    }
    
    cat1[4] = 0x00;  /* Cycle number */
    cat1[5] = 0x00;  /* File count × 8 */
    
    int total_sectors = img->tracks * DFS_SECTORS_TRACK;
    if (img->is_dsd) total_sectors *= 2;
    
    cat1[6] = ((total_sectors >> 8) & 0x03);  /* Boot option + sectors high */
    cat1[7] = total_sectors & 0xFF;           /* Sectors low */
    
    dfs_write_sector(img, 0, 0, 0, cat0);
    dfs_write_sector(img, 0, 0, 1, cat1);
    
    /* Format side 1 if DSD */
    if (img->is_dsd) {
        memset(cat0, ' ', 8);
        memset(cat1, 0, DFS_SECTOR_SIZE);
        cat1[6] = ((total_sectors / 2 >> 8) & 0x03);
        cat1[7] = (total_sectors / 2) & 0xFF;
        
        dfs_write_sector(img, 0, 1, 0, cat0);
        dfs_write_sector(img, 0, 1, 1, cat1);
    }
    
    return 0;
}

/**
 * @brief Get disk info
 */
int dfs_get_info(const dfs_image_t *img, char *title, int *files,
                 int *free_sectors)
{
    if (!img || !img->data) return -1;
    
    uint8_t cat0[DFS_SECTOR_SIZE], cat1[DFS_SECTOR_SIZE];
    dfs_read_sector(img, 0, 0, 0, cat0);
    dfs_read_sector(img, 0, 0, 1, cat1);
    
    if (title) {
        memcpy(title, cat0, 8);
        memcpy(title + 8, cat1, 4);
        title[12] = '\0';
        
        /* Trim trailing spaces */
        for (int i = 11; i >= 0 && title[i] == ' '; i--) {
            title[i] = '\0';
        }
    }
    
    if (files) {
        *files = cat1[5] / 8;
    }
    
    if (free_sectors) {
        int total = ((cat1[6] & 0x03) << 8) | cat1[7];
        int used = 2;  /* Catalog */
        
        /* Count used sectors from directory */
        int file_count = cat1[5] / 8;
        for (int f = 0; f < file_count; f++) {
            int entry_offset = 8 + f * 8;
            /* Length is in sector 1 */
            uint16_t len_lo = cat1[entry_offset + 4] | (cat1[entry_offset + 5] << 8);
            uint8_t mixed = cat0[entry_offset + 6];
            uint32_t length = len_lo | ((mixed & 0x30) << 12);
            used += (length + 255) / 256;
        }
        
        *free_sectors = total - used;
    }
    
    return 0;
}

/**
 * @brief Save DFS image
 */
int dfs_save(const dfs_image_t *img, const char *filename)
{
    if (!img || !img->data || !filename) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -1;
    
    size_t written = fwrite(img->data, 1, img->size, fp);
    fclose(fp);
    
    return (written == img->size) ? 0 : -1;
}

/**
 * @brief Convert SSD to DSD (duplicate to side 1)
 */
int dfs_ssd_to_dsd(const char *ssd_file, const char *dsd_file)
{
    dfs_image_t ssd;
    if (dfs_load(ssd_file, &ssd) != 0) return -1;
    
    dfs_image_t dsd;
    dfs_format_t dsd_format = (ssd.tracks == 40) ? DFS_FORMAT_DSD_40 : DFS_FORMAT_DSD_80;
    dfs_create(&dsd, dsd_format);
    
    /* Copy side 0 */
    uint8_t sector[DFS_SECTOR_SIZE];
    for (int t = 0; t < ssd.tracks; t++) {
        for (int s = 0; s < DFS_SECTORS_TRACK; s++) {
            dfs_read_sector(&ssd, t, 0, s, sector);
            dfs_write_sector(&dsd, t, 0, s, sector);
        }
    }
    
    /* Format side 1 as empty */
    dfs_format(&dsd, "SIDE1");
    
    int result = dfs_save(&dsd, dsd_file);
    
    dfs_free(&ssd);
    dfs_free(&dsd);
    
    return result;
}

/**
 * @brief Free DFS image
 */
void dfs_free(dfs_image_t *img)
{
    if (img) {
        free(img->data);
        img->data = NULL;
        img->size = 0;
    }
}
