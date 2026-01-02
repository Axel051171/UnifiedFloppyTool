/**
 * @file jv3_loader.c
 * @brief JV3 Image Loader/Writer for TRS-80 Model I/III
 * 
 * JV3 is a flexible format that stores sector header info separately,
 * allowing for variable sector sizes and copy protection.
 * 
 * @version 5.30.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* JV3 Constants */
#define JV3_HEADER_SIZE     (2901 * 3)  /* 2901 sector headers × 3 bytes */
#define JV3_MAX_SECTORS     2901
#define JV3_HEADER_END      0xFF        /* End of header marker */
#define JV3_SECTOR_FREE     0xFE        /* Free sector marker */

/* Sector size codes */
#define JV3_SIZE_256        0x00
#define JV3_SIZE_128        0x01
#define JV3_SIZE_1024       0x02
#define JV3_SIZE_512        0x03

/* Flags byte */
#define JV3_FLAG_DENSITY    0x80  /* 0=single, 1=double density */
#define JV3_FLAG_DAM        0x60  /* Data address mark bits */
#define JV3_FLAG_SIDE       0x10  /* Side number */
#define JV3_FLAG_CRC_ERROR  0x08  /* CRC error in original */
#define JV3_FLAG_NONIBM     0x04  /* Non-IBM sector size */

#pragma pack(push, 1)
typedef struct {
    uint8_t track;      /* Track number */
    uint8_t sector;     /* Sector number */
    uint8_t flags;      /* Flags + size code */
} jv3_sector_header_t;
#pragma pack(pop)

typedef struct {
    uint8_t track;
    uint8_t sector;
    uint8_t side;
    int     size;
    bool    double_density;
    bool    crc_error;
    uint8_t dam;        /* Data address mark */
    uint8_t *data;
} jv3_sector_t;

typedef struct {
    jv3_sector_header_t headers[JV3_MAX_SECTORS];
    jv3_sector_t       *sectors;
    int                 sector_count;
    uint8_t            *raw_data;
    size_t              data_size;
    int                 max_track;
    int                 max_side;
    bool                write_protect;
} jv3_image_t;

/* ============================================================================
 * JV3 Helper Functions
 * ============================================================================ */

/**
 * @brief Get sector size from flags
 */
static int jv3_get_sector_size(uint8_t flags)
{
    int size_code = flags & 0x03;
    bool non_ibm = (flags & JV3_FLAG_NONIBM) != 0;
    
    if (non_ibm) {
        /* Non-standard sizes */
        switch (size_code) {
            case 0: return 256;
            case 1: return 128;
            case 2: return 1024;
            case 3: return 512;
        }
    } else {
        /* IBM standard */
        switch (size_code) {
            case 0: return 256;
            case 1: return 128;
            case 2: return 1024;
            case 3: return 512;
        }
    }
    
    return 256;
}

/**
 * @brief Make flags byte
 */
static uint8_t jv3_make_flags(int size, bool double_density, int side,
                              uint8_t dam, bool crc_error)
{
    uint8_t flags = 0;
    
    /* Size code */
    switch (size) {
        case 128:  flags |= JV3_SIZE_128; break;
        case 256:  flags |= JV3_SIZE_256; break;
        case 512:  flags |= JV3_SIZE_512; break;
        case 1024: flags |= JV3_SIZE_1024; break;
    }
    
    if (double_density) flags |= JV3_FLAG_DENSITY;
    if (side) flags |= JV3_FLAG_SIDE;
    if (crc_error) flags |= JV3_FLAG_CRC_ERROR;
    flags |= (dam << 5) & JV3_FLAG_DAM;
    
    return flags;
}

/* ============================================================================
 * JV3 Loader
 * ============================================================================ */

/**
 * @brief Create empty JV3 image
 */
int jv3_create(jv3_image_t *img)
{
    if (!img) return -1;
    
    memset(img, 0, sizeof(*img));
    
    /* Initialize headers as free */
    for (int i = 0; i < JV3_MAX_SECTORS; i++) {
        img->headers[i].track = JV3_HEADER_END;
        img->headers[i].sector = JV3_SECTOR_FREE;
        img->headers[i].flags = 0;
    }
    
    img->sectors = calloc(JV3_MAX_SECTORS, sizeof(jv3_sector_t));
    if (!img->sectors) return -1;
    
    return 0;
}

/**
 * @brief Load JV3 file
 */
int jv3_load(const char *filename, jv3_image_t *img)
{
    if (!filename || !img) return -1;
    
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -1;
    
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (size < JV3_HEADER_SIZE) {
        fclose(fp);
        return -1;
    }
    
    memset(img, 0, sizeof(*img));
    
    /* Read sector headers */
    fread(img->headers, 1, JV3_HEADER_SIZE, fp);
    
    /* Check for write protect byte */
    uint8_t wp_byte;
    fread(&wp_byte, 1, 1, fp);
    img->write_protect = (wp_byte == 0xFF);
    
    /* Calculate data size and allocate */
    img->data_size = size - JV3_HEADER_SIZE - 1;
    img->raw_data = malloc(img->data_size);
    
    if (!img->raw_data) {
        fclose(fp);
        return -1;
    }
    
    fread(img->raw_data, 1, img->data_size, fp);
    fclose(fp);
    
    /* Parse sectors */
    img->sectors = calloc(JV3_MAX_SECTORS, sizeof(jv3_sector_t));
    if (!img->sectors) {
        free(img->raw_data);
        return -1;
    }
    
    size_t data_offset = 0;
    img->sector_count = 0;
    img->max_track = 0;
    img->max_side = 0;
    
    for (int i = 0; i < JV3_MAX_SECTORS; i++) {
        if (img->headers[i].track == JV3_HEADER_END &&
            img->headers[i].sector == JV3_SECTOR_FREE) {
            continue;
        }
        
        jv3_sector_t *sect = &img->sectors[img->sector_count];
        
        sect->track = img->headers[i].track;
        sect->sector = img->headers[i].sector;
        sect->side = (img->headers[i].flags & JV3_FLAG_SIDE) ? 1 : 0;
        sect->size = jv3_get_sector_size(img->headers[i].flags);
        sect->double_density = (img->headers[i].flags & JV3_FLAG_DENSITY) != 0;
        sect->crc_error = (img->headers[i].flags & JV3_FLAG_CRC_ERROR) != 0;
        sect->dam = (img->headers[i].flags & JV3_FLAG_DAM) >> 5;
        
        if (data_offset + sect->size <= img->data_size) {
            sect->data = img->raw_data + data_offset;
            data_offset += sect->size;
        }
        
        if (sect->track > img->max_track) img->max_track = sect->track;
        if (sect->side > img->max_side) img->max_side = sect->side;
        
        img->sector_count++;
    }
    
    return 0;
}

/**
 * @brief Add sector to JV3 image
 */
int jv3_add_sector(jv3_image_t *img, int track, int side, int sector,
                   int size, bool double_density, const uint8_t *data)
{
    if (!img || !img->sectors || !data) return -1;
    if (img->sector_count >= JV3_MAX_SECTORS) return -1;
    
    int idx = img->sector_count;
    
    img->headers[idx].track = track;
    img->headers[idx].sector = sector;
    img->headers[idx].flags = jv3_make_flags(size, double_density, side, 0xFB, false);
    
    img->sectors[idx].track = track;
    img->sectors[idx].sector = sector;
    img->sectors[idx].side = side;
    img->sectors[idx].size = size;
    img->sectors[idx].double_density = double_density;
    img->sectors[idx].data = malloc(size);
    
    if (!img->sectors[idx].data) return -1;
    
    memcpy(img->sectors[idx].data, data, size);
    
    if (track > img->max_track) img->max_track = track;
    if (side > img->max_side) img->max_side = side;
    
    img->sector_count++;
    
    return 0;
}

/**
 * @brief Read sector from JV3
 */
int jv3_read_sector(const jv3_image_t *img, int track, int side, int sector,
                    uint8_t *data, int *size)
{
    if (!img || !img->sectors || !data) return -1;
    
    for (int i = 0; i < img->sector_count; i++) {
        if (img->sectors[i].track == track &&
            img->sectors[i].side == side &&
            img->sectors[i].sector == sector) {
            
            if (size) *size = img->sectors[i].size;
            memcpy(data, img->sectors[i].data, img->sectors[i].size);
            return img->sectors[i].crc_error ? 1 : 0;
        }
    }
    
    return -1;  /* Sector not found */
}

/**
 * @brief Save JV3 image
 */
int jv3_save(const jv3_image_t *img, const char *filename)
{
    if (!img || !filename) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -1;
    
    /* Write sector headers */
    fwrite(img->headers, 1, JV3_HEADER_SIZE, fp);
    
    /* Write protect byte */
    uint8_t wp = img->write_protect ? 0xFF : 0x00;
    fwrite(&wp, 1, 1, fp);
    
    /* Write sector data */
    for (int i = 0; i < img->sector_count; i++) {
        if (img->sectors[i].data) {
            fwrite(img->sectors[i].data, 1, img->sectors[i].size, fp);
        }
    }
    
    fclose(fp);
    return 0;
}

/**
 * @brief Convert DMK to JV3
 */
int jv3_from_dmk(const char *dmk_file, const char *jv3_file)
{
    /* Simplified DMK→JV3 conversion */
    FILE *fp = fopen(dmk_file, "rb");
    if (!fp) return -1;
    
    /* Read DMK header */
    uint8_t header[16];
    fread(header, 1, 16, fp);
    
    int tracks = header[1];
    int track_len = header[2] | (header[3] << 8);
    bool double_sided = !(header[4] & 0x10);
    int sides = double_sided ? 2 : 1;
    
    jv3_image_t jv3;
    jv3_create(&jv3);
    
    uint8_t *track_buf = malloc(track_len);
    
    for (int t = 0; t < tracks; t++) {
        for (int s = 0; s < sides; s++) {
            fread(track_buf, 1, track_len, fp);
            
            /* Parse IDAM table */
            uint16_t *idam = (uint16_t*)track_buf;
            
            for (int i = 0; i < 64 && idam[i] != 0; i++) {
                uint16_t offset = idam[i] & 0x3FFF;
                bool dd = (idam[i] & 0x8000) != 0;
                
                if (offset < track_len - 7) {
                    uint8_t *id = track_buf + offset;
                    
                    if (id[0] == 0xFE) {  /* ID address mark */
                        int sect_track = id[1];
                        int sect_side = id[2];
                        int sect_num = id[3];
                        int sect_size = 128 << id[4];
                        
                        /* Find data */
                        /* Simplified: assume data follows ID */
                        uint8_t sector_data[1024] = {0};
                        
                        jv3_add_sector(&jv3, sect_track, sect_side, sect_num,
                                       sect_size, dd, sector_data);
                    }
                }
            }
        }
    }
    
    free(track_buf);
    fclose(fp);
    
    int result = jv3_save(&jv3, jv3_file);
    jv3_free(&jv3);
    
    return result;
}

/**
 * @brief Free JV3 image
 */
void jv3_free(jv3_image_t *img)
{
    if (img) {
        if (img->sectors && !img->raw_data) {
            /* If we allocated sector data individually */
            for (int i = 0; i < img->sector_count; i++) {
                free(img->sectors[i].data);
            }
        }
        free(img->sectors);
        free(img->raw_data);
        memset(img, 0, sizeof(*img));
    }
}
