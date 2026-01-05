/**
 * @file trd_loader.c
 * @brief TRD Image Loader/Writer for ZX Spectrum TR-DOS
 * 
 * TRD is the standard disk format for Beta Disk Interface on ZX Spectrum.
 * 640K format: 80 tracks × 2 sides × 16 sectors × 256 bytes
 * 
 * @version 5.30.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* TRD Constants */
#define TRD_SECTOR_SIZE     256
#define TRD_SECTORS_TRACK   16
#define TRD_TRACKS          80
#define TRD_SIDES           2

#define TRD_640K_SIZE       655360  /* 80×2×16×256 */
#define TRD_720K_SIZE       737280  /* 80×2×18×256 (extended) */

/* Disk info sector (Track 0, Sector 9) */
#define TRD_INFO_TRACK      0
#define TRD_INFO_SECTOR     8   /* 0-based */

/* File types */
#define TRD_TYPE_BASIC      'B'
#define TRD_TYPE_NUMARRAY   'D'
#define TRD_TYPE_CHARARRAY  'C'
#define TRD_TYPE_CODE       'C'
#define TRD_TYPE_PRINT      '#'

#pragma pack(push, 1)
typedef struct {
    char     name[8];       /* Filename (space padded) */
    char     type;          /* File type */
    uint16_t start;         /* Start address or BASIC line */
    uint16_t length;        /* File length */
    uint8_t  sectors;       /* Length in sectors */
    uint8_t  first_sector;  /* First sector */
    uint8_t  first_track;   /* First track */
} trd_dirent_t;

typedef struct {
    uint8_t  end_track;     /* First free track */
    uint8_t  end_sector;    /* First free sector */
    uint8_t  disk_type;     /* Disk type */
    uint8_t  file_count;    /* Number of files */
    uint16_t free_sectors;  /* Free sector count */
    uint8_t  trdos_id;      /* TR-DOS ID (0x10) */
    uint8_t  reserved[2];
    uint8_t  password[9];   /* Disk password (space padded) */
    uint8_t  reserved2;
    uint8_t  deleted_files; /* Number of deleted files */
    char     label[8];      /* Disk label */
    uint8_t  reserved3[3];
} trd_diskinfo_t;
#pragma pack(pop)

typedef struct {
    uint8_t *data;
    size_t   size;
    int      tracks;
    int      sides;
    int      sectors_per_track;
} trd_image_t;

/* ============================================================================
 * TRD Loader
 * ============================================================================ */

/**
 * @brief Create empty TRD image
 */
int trd_create(trd_image_t *img, bool double_sided)
{
    if (!img) return -1;
    
    memset(img, 0, sizeof(*img));
    
    img->tracks = TRD_TRACKS;
    img->sides = double_sided ? 2 : 1;
    img->sectors_per_track = TRD_SECTORS_TRACK;
    img->size = img->tracks * img->sides * img->sectors_per_track * TRD_SECTOR_SIZE;
    
    img->data = calloc(1, img->size);
    if (!img->data) return -1;
    
    return 0;
}

/**
 * @brief Load TRD file
 */
int trd_load(const char *filename, trd_image_t *img)
{
    if (!filename || !img) return -1;
    
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -1;
    
    if (fseek(fp, 0, SEEK_END) != 0) { /* seek error */ }
    size_t size = ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) { /* seek error */ }
    memset(img, 0, sizeof(*img));
    
    /* Determine geometry from size */
    if (size == TRD_640K_SIZE) {
        img->tracks = 80;
        img->sides = 2;
        img->sectors_per_track = 16;
    } else if (size == TRD_640K_SIZE / 2) {
        img->tracks = 80;
        img->sides = 1;
        img->sectors_per_track = 16;
    } else if (size == TRD_720K_SIZE) {
        img->tracks = 80;
        img->sides = 2;
        img->sectors_per_track = 18;
    } else {
        /* Try to guess */
        img->tracks = 80;
        img->sides = 2;
        img->sectors_per_track = size / (80 * 2 * 256);
    }
    
    img->size = size;
    img->data = malloc(size);
    
    if (!img->data) {
        fclose(fp);
        return -1;
    }
    
    if (fread(img->data, 1, size, fp) != size) { /* I/O error */ }
    fclose(fp);
    
    return 0;
}

/**
 * @brief Get sector offset
 */
static size_t trd_sector_offset(const trd_image_t *img, int track, int side, 
                                int sector)
{
    /* Interleaved sides: T0S0, T0S1, T1S0, T1S1, ... */
    int logical_track = track * img->sides + side;
    return (logical_track * img->sectors_per_track + sector) * TRD_SECTOR_SIZE;
}

/**
 * @brief Read sector
 */
int trd_read_sector(const trd_image_t *img, int track, int side, int sector,
                    uint8_t *data)
{
    if (!img || !img->data || !data) return -1;
    if (track < 0 || track >= img->tracks) return -1;
    if (side < 0 || side >= img->sides) return -1;
    if (sector < 0 || sector >= img->sectors_per_track) return -1;
    
    size_t offset = trd_sector_offset(img, track, side, sector);
    memcpy(data, img->data + offset, TRD_SECTOR_SIZE);
    
    return 0;
}

/**
 * @brief Write sector
 */
int trd_write_sector(trd_image_t *img, int track, int side, int sector,
                     const uint8_t *data)
{
    if (!img || !img->data || !data) return -1;
    if (track < 0 || track >= img->tracks) return -1;
    if (side < 0 || side >= img->sides) return -1;
    if (sector < 0 || sector >= img->sectors_per_track) return -1;
    
    size_t offset = trd_sector_offset(img, track, side, sector);
    memcpy(img->data + offset, data, TRD_SECTOR_SIZE);
    
    return 0;
}

/**
 * @brief Format TRD disk
 */
int trd_format(trd_image_t *img, const char *label)
{
    if (!img || !img->data) return -1;
    
    /* Clear disk */
    memset(img->data, 0, img->size);
    
    /* Setup disk info sector (Track 0, Sector 8) */
    uint8_t info[TRD_SECTOR_SIZE] = {0};
    trd_diskinfo_t *diskinfo = (trd_diskinfo_t*)(info + 0xE1);
    
    diskinfo->end_track = 1;
    diskinfo->end_sector = 0;
    diskinfo->disk_type = (img->sides == 2) ? 0x16 : 0x17;
    diskinfo->file_count = 0;
    diskinfo->free_sectors = (img->tracks * img->sides - 1) * img->sectors_per_track;
    diskinfo->trdos_id = 0x10;
    
    memset(diskinfo->label, ' ', 8);
    if (label) {
        size_t len = strlen(label);
        if (len > 8) len = 8;
        memcpy(diskinfo->label, label, len);
    }
    
    trd_write_sector(img, 0, 0, 8, info);
    
    return 0;
}

/**
 * @brief Get disk info
 */
int trd_get_info(const trd_image_t *img, int *files, int *free_sectors,
                 char *label)
{
    if (!img || !img->data) return -1;
    
    uint8_t info[TRD_SECTOR_SIZE];
    trd_read_sector(img, 0, 0, 8, info);
    
    trd_diskinfo_t *diskinfo = (trd_diskinfo_t*)(info + 0xE1);
    
    if (files) *files = diskinfo->file_count;
    if (free_sectors) *free_sectors = diskinfo->free_sectors;
    if (label) {
        memcpy(label, diskinfo->label, 8);
        label[8] = '\0';
        /* Trim trailing spaces */
        for (int i = 7; i >= 0 && label[i] == ' '; i--) {
            label[i] = '\0';
        }
    }
    
    return 0;
}

/**
 * @brief List files on disk
 */
int trd_list_files(const trd_image_t *img, trd_dirent_t *entries, 
                   int max_entries)
{
    if (!img || !img->data || !entries) return -1;
    
    int count = 0;
    
    /* Directory is in sectors 0-7 of track 0 */
    for (int sect = 0; sect < 8 && count < max_entries; sect++) {
        uint8_t sector_data[TRD_SECTOR_SIZE];
        trd_read_sector(img, 0, 0, sect, sector_data);
        
        /* Each sector has 16 directory entries (16 bytes each) */
        for (int e = 0; e < 16 && count < max_entries; e++) {
            trd_dirent_t *entry = (trd_dirent_t*)(sector_data + e * 16);
            
            if (entry->name[0] == 0x00) continue;  /* Empty */
            if (entry->name[0] == 0x01) continue;  /* Deleted */
            
            memcpy(&entries[count], entry, sizeof(trd_dirent_t));
            count++;
        }
    }
    
    return count;
}

/**
 * @brief Save TRD image
 */
int trd_save(const trd_image_t *img, const char *filename)
{
    if (!img || !img->data || !filename) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -1;
    
    size_t written = fwrite(img->data, 1, img->size, fp);
    fclose(fp);
    
    return (written == img->size) ? 0 : -1;
}

/**
 * @brief Free TRD image
 */
void trd_free(trd_image_t *img)
{
    if (img) {
        free(img->data);
        img->data = NULL;
        img->size = 0;
    }
}
