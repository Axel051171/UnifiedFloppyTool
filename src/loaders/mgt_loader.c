/**
 * @file mgt_loader.c
 * @brief MGT Image Loader/Writer for SAM Coupé
 * 
 * MGT is the native disk format for SAM Coupé (Miles Gordon Technology).
 * 80 tracks × 2 sides × 10 sectors × 512 bytes = 800K
 * 
 * @version 5.30.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* MGT Constants */
#define MGT_SECTOR_SIZE     512
#define MGT_SECTORS_TRACK   10
#define MGT_TRACKS          80
#define MGT_SIDES           2

#define MGT_DISK_SIZE       819200  /* 80×2×10×512 */

/* Directory constants */
#define MGT_DIR_ENTRIES     80      /* 80 directory entries */
#define MGT_DIR_SECTORS     4       /* Directory uses 4 sectors (track 0) */

/* File types */
#define MGT_TYPE_FREE       0x00
#define MGT_TYPE_ZXBASIC    0x01    /* ZX BASIC */
#define MGT_TYPE_ZXNUMARRAY 0x02
#define MGT_TYPE_ZXSTRARRAY 0x03
#define MGT_TYPE_CODE       0x04
#define MGT_TYPE_ZXSNAP48   0x05
#define MGT_TYPE_MDRFILE    0x06
#define MGT_TYPE_SCREEN     0x07
#define MGT_TYPE_SPECIAL    0x08
#define MGT_TYPE_ZXSNAP128  0x09
#define MGT_TYPE_OPENTYPE   0x0A
#define MGT_TYPE_EXECUTE    0x0B
#define MGT_TYPE_UNIDOS_DIR 0x0C
#define MGT_TYPE_UNIDOS_CRE 0x0D
#define MGT_TYPE_BASIC      0x10    /* SAM BASIC */
#define MGT_TYPE_NUMARRAY   0x11
#define MGT_TYPE_STRARRAY   0x12
#define MGT_TYPE_SAMCODE    0x13
#define MGT_TYPE_SAMSCREEN  0x14
#define MGT_TYPE_ERASED     0xFF

#pragma pack(push, 1)
typedef struct {
    uint8_t  status;        /* File type / status */
    char     name[10];      /* Filename (space padded) */
    uint8_t  sectors_hi;    /* Sector count high nibble + flags */
    uint8_t  sectors_lo;    /* Sector count low byte */
    uint8_t  start_track;   /* Starting track */
    uint8_t  start_sector;  /* Starting sector */
    uint8_t  sector_map[195]; /* Sector allocation map */
    /* Total: 210 bytes per entry */
} mgt_dirent_t;

typedef struct {
    uint8_t  type;          /* File type */
    uint16_t length;        /* File length */
    uint16_t start;         /* Start address */
    uint16_t exec;          /* Execute address */
    uint8_t  pages;         /* Number of pages */
} mgt_fileheader_t;
#pragma pack(pop)

typedef struct {
    uint8_t *data;
    size_t   size;
    int      tracks;
    int      sides;
} mgt_image_t;

/* ============================================================================
 * MGT Loader
 * ============================================================================ */

/**
 * @brief Create empty MGT image
 */
int mgt_create(mgt_image_t *img)
{
    if (!img) return -1;
    
    memset(img, 0, sizeof(*img));
    
    img->tracks = MGT_TRACKS;
    img->sides = MGT_SIDES;
    img->size = MGT_DISK_SIZE;
    
    img->data = calloc(1, img->size);
    return img->data ? 0 : -1;
}

/**
 * @brief Load MGT file
 */
int mgt_load(const char *filename, mgt_image_t *img)
{
    if (!filename || !img) return -1;
    
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -1;
    
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (mgt_create(img) != 0) {
        fclose(fp);
        return -1;
    }
    
    size_t read_size = (size < img->size) ? size : img->size;
    fread(img->data, 1, read_size, fp);
    fclose(fp);
    
    return 0;
}

/**
 * @brief Get sector offset
 * MGT uses interleaved sides: Track0/Side0, Track0/Side1, Track1/Side0, ...
 */
static size_t mgt_sector_offset(int track, int side, int sector)
{
    /* Sector numbers are 1-based on real disk, 0-based here */
    int logical_track = track * MGT_SIDES + side;
    return (logical_track * MGT_SECTORS_TRACK + sector) * MGT_SECTOR_SIZE;
}

/**
 * @brief Read sector
 */
int mgt_read_sector(const mgt_image_t *img, int track, int side, int sector,
                    uint8_t *data)
{
    if (!img || !img->data || !data) return -1;
    if (track < 0 || track >= img->tracks) return -1;
    if (side < 0 || side >= img->sides) return -1;
    if (sector < 0 || sector >= MGT_SECTORS_TRACK) return -1;
    
    size_t offset = mgt_sector_offset(track, side, sector);
    memcpy(data, img->data + offset, MGT_SECTOR_SIZE);
    
    return 0;
}

/**
 * @brief Write sector
 */
int mgt_write_sector(mgt_image_t *img, int track, int side, int sector,
                     const uint8_t *data)
{
    if (!img || !img->data || !data) return -1;
    if (track < 0 || track >= img->tracks) return -1;
    if (side < 0 || side >= img->sides) return -1;
    if (sector < 0 || sector >= MGT_SECTORS_TRACK) return -1;
    
    size_t offset = mgt_sector_offset(track, side, sector);
    memcpy(img->data + offset, data, MGT_SECTOR_SIZE);
    
    return 0;
}

/**
 * @brief Format MGT disk
 */
int mgt_format(mgt_image_t *img, const char *label)
{
    if (!img || !img->data) return -1;
    
    /* Clear disk */
    memset(img->data, 0, img->size);
    
    /* Initialize directory sectors on track 0, both sides */
    /* Each side has 2 directory sectors (sectors 1-2) */
    /* Each 512-byte sector holds 2 directory entries (256 bytes each) */
    
    /* Mark all directory entries as free */
    uint8_t empty_dir[MGT_SECTOR_SIZE];
    memset(empty_dir, 0, MGT_SECTOR_SIZE);
    
    /* Side 0, sectors 1-2 */
    mgt_write_sector(img, 0, 0, 0, empty_dir);
    mgt_write_sector(img, 0, 0, 1, empty_dir);
    
    /* Side 1, sectors 1-2 */
    mgt_write_sector(img, 0, 1, 0, empty_dir);
    mgt_write_sector(img, 0, 1, 1, empty_dir);
    
    /* Set disk label in first directory entry if provided */
    if (label && strlen(label) > 0) {
        uint8_t dir_sector[MGT_SECTOR_SIZE];
        mgt_read_sector(img, 0, 0, 0, dir_sector);
        
        /* First entry as label (type 0x10 = BASIC with special marker) */
        dir_sector[0] = MGT_TYPE_BASIC;
        memset(dir_sector + 1, ' ', 10);
        size_t len = strlen(label);
        if (len > 10) len = 10;
        memcpy(dir_sector + 1, label, len);
        
        mgt_write_sector(img, 0, 0, 0, dir_sector);
    }
    
    return 0;
}

/**
 * @brief List files on MGT disk
 */
int mgt_list_files(const mgt_image_t *img, char names[][11], uint8_t *types,
                   int max_files)
{
    if (!img || !img->data) return -1;
    
    int count = 0;
    uint8_t sector[MGT_SECTOR_SIZE];
    
    /* Read all 4 directory sectors (2 per side) */
    int dir_sectors[4][2] = {{0,0}, {0,1}, {1,0}, {1,1}};
    
    for (int d = 0; d < 4 && count < max_files; d++) {
        int side = dir_sectors[d][0];
        int sect = dir_sectors[d][1];
        
        mgt_read_sector(img, 0, side, sect, sector);
        
        /* 2 entries per sector (256 bytes each, but we use simplified 210) */
        for (int e = 0; e < 2 && count < max_files; e++) {
            uint8_t *entry = sector + e * 256;
            uint8_t type = entry[0];
            
            if (type == MGT_TYPE_FREE || type == MGT_TYPE_ERASED) continue;
            
            if (names) {
                memcpy(names[count], entry + 1, 10);
                names[count][10] = '\0';
                
                /* Trim trailing spaces */
                for (int i = 9; i >= 0 && names[count][i] == ' '; i--) {
                    names[count][i] = '\0';
                }
            }
            
            if (types) {
                types[count] = type;
            }
            
            count++;
        }
    }
    
    return count;
}

/**
 * @brief Get file type name
 */
const char* mgt_type_name(uint8_t type)
{
    switch (type) {
        case MGT_TYPE_FREE:       return "Free";
        case MGT_TYPE_ZXBASIC:    return "ZX BASIC";
        case MGT_TYPE_ZXNUMARRAY: return "ZX NumArray";
        case MGT_TYPE_ZXSTRARRAY: return "ZX StrArray";
        case MGT_TYPE_CODE:       return "Code";
        case MGT_TYPE_ZXSNAP48:   return "ZX Snap 48K";
        case MGT_TYPE_MDRFILE:    return "Microdrive";
        case MGT_TYPE_SCREEN:     return "Screen$";
        case MGT_TYPE_SPECIAL:    return "Special";
        case MGT_TYPE_ZXSNAP128:  return "ZX Snap 128K";
        case MGT_TYPE_BASIC:      return "SAM BASIC";
        case MGT_TYPE_NUMARRAY:   return "NumArray";
        case MGT_TYPE_STRARRAY:   return "StrArray";
        case MGT_TYPE_SAMCODE:    return "SAM Code";
        case MGT_TYPE_SAMSCREEN:  return "SAM Screen";
        case MGT_TYPE_ERASED:     return "Erased";
        default:                  return "Unknown";
    }
}

/**
 * @brief Save MGT image
 */
int mgt_save(const mgt_image_t *img, const char *filename)
{
    if (!img || !img->data || !filename) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -1;
    
    size_t written = fwrite(img->data, 1, img->size, fp);
    fclose(fp);
    
    return (written == img->size) ? 0 : -1;
}

/**
 * @brief Convert raw 800K image to MGT
 */
int mgt_from_raw(const char *raw_file, const char *mgt_file)
{
    FILE *fp = fopen(raw_file, "rb");
    if (!fp) return -1;
    
    mgt_image_t img;
    mgt_create(&img);
    
    fread(img.data, 1, img.size, fp);
    fclose(fp);
    
    int result = mgt_save(&img, mgt_file);
    mgt_free(&img);
    
    return result;
}

/**
 * @brief Free MGT image
 */
void mgt_free(mgt_image_t *img)
{
    if (img) {
        free(img->data);
        img->data = NULL;
        img->size = 0;
    }
}
