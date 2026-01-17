/**
 * @file uft_ldbs.c
 * @brief LDBS (LibDsk Block Store) format implementation
 * @version 4.1.0
 * 
 * LDBS is a container format from libdsk that stores disk images
 * with metadata in a block-structured file.
 * 
 * Reference: libdsk drvldbs.c by John Elliott
 */

#include "uft/formats/uft_ldbs.h"
#include "uft/core/uft_unified_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * LDBS Format Constants
 * ============================================================================ */

#define LDBS_MAGIC      "LDB\1"    /* Magic number */
#define LDBS_MAGIC_LEN  4

/* Block types */
#define LDBS_BT_DPB     0x0001    /* Disk parameter block */
#define LDBS_BT_GEOM    0x0002    /* Disk geometry */
#define LDBS_BT_TRACK   0x0003    /* Track header */
#define LDBS_BT_SECTOR  0x0004    /* Sector data */
#define LDBS_BT_COMMENT 0x0005    /* Comment block */
#define LDBS_BT_CREATOR 0x0006    /* Creator info */
#define LDBS_BT_INFO    0x0007    /* Additional info */

/* ============================================================================
 * Structures
 * ============================================================================ */

#pragma pack(push, 1)

typedef struct {
    uint8_t  magic[4];       /* "LDB\1" */
    uint32_t version;        /* Version (1) */
    uint32_t block_count;    /* Number of blocks */
    uint32_t first_block;    /* Offset to first block */
    uint32_t flags;          /* Flags */
    uint8_t  reserved[12];   /* Reserved */
} ldbs_header_t;

typedef struct {
    uint16_t type;           /* Block type */
    uint16_t flags;          /* Block flags */
    uint32_t length;         /* Data length */
    uint32_t next;           /* Offset to next block (0 = end) */
} ldbs_block_header_t;

typedef struct {
    uint8_t  cylinders;      /* Number of cylinders */
    uint8_t  heads;          /* Number of heads */
    uint8_t  sectors;        /* Sectors per track */
    uint8_t  sector_size;    /* Sector size code (0=128, 1=256, 2=512...) */
    uint8_t  gap3;           /* GAP3 length */
    uint8_t  filler;         /* Filler byte */
    uint16_t data_rate;      /* Data rate in kbps */
    uint8_t  encoding;       /* 0=FM, 1=MFM */
    uint8_t  reserved[7];    /* Reserved */
} ldbs_geometry_t;

typedef struct {
    uint8_t  cylinder;       /* Cylinder */
    uint8_t  head;           /* Head */
    uint8_t  sector_count;   /* Number of sectors */
    uint8_t  encoding;       /* Track encoding */
    uint16_t data_rate;      /* Data rate */
    uint16_t flags;          /* Track flags */
} ldbs_track_header_t;

typedef struct {
    uint8_t  cylinder;       /* ID cylinder */
    uint8_t  head;           /* ID head */
    uint8_t  sector;         /* ID sector */
    uint8_t  size_code;      /* Size code */
    uint8_t  status;         /* Status (0=OK, 1=CRC error, etc.) */
    uint8_t  flags;          /* Sector flags */
    uint16_t data_size;      /* Actual data size */
} ldbs_sector_header_t;

#pragma pack(pop)

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static uint16_t read_le16(const uint8_t *p) {
    return p[0] | (p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static void write_le16(uint8_t *p, uint16_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

static void write_le32(uint8_t *p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

static uint16_t size_from_code(uint8_t code) {
    return 128 << code;
}

static uint8_t code_from_size(uint16_t size) {
    switch (size) {
        case 128:  return 0;
        case 256:  return 1;
        case 512:  return 2;
        case 1024: return 3;
        case 2048: return 4;
        default:   return 2;
    }
}

/* ============================================================================
 * Probe Function
 * ============================================================================ */

bool uft_ldbs_probe(const uint8_t *data, size_t size, int *confidence) {
    if (!data || size < sizeof(ldbs_header_t)) {
        return false;
    }
    
    if (memcmp(data, LDBS_MAGIC, LDBS_MAGIC_LEN) == 0) {
        if (confidence) *confidence = 95;
        return true;
    }
    
    return false;
}

/* ============================================================================
 * Read Functions
 * ============================================================================ */

int uft_ldbs_read(const char *path, uft_disk_image_t **out) {
    if (!path || !out) return UFT_ERR_INVALID_ARG;
    
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERR_FILE_OPEN;
    
    /* Read header */
    ldbs_header_t header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return UFT_ERR_FILE_READ;
    }
    
    /* Verify magic */
    if (memcmp(header.magic, LDBS_MAGIC, LDBS_MAGIC_LEN) != 0) {
        fclose(f);
        return UFT_ERR_FORMAT;
    }
    
    /* Read geometry block */
    ldbs_geometry_t geom = {0};
    
    uint32_t block_offset = read_le32((uint8_t*)&header.first_block);
    while (block_offset > 0) {
        fseek(f, block_offset, SEEK_SET);
        
        ldbs_block_header_t bh;
        if (fread(&bh, sizeof(bh), 1, f) != 1) break;
        
        if (read_le16((uint8_t*)&bh.type) == LDBS_BT_GEOM) {
            fread(&geom, sizeof(geom), 1, f);
            break;
        }
        
        block_offset = read_le32((uint8_t*)&bh.next);
    }
    
    if (geom.cylinders == 0 || geom.heads == 0) {
        fclose(f);
        return UFT_ERR_FORMAT;
    }
    
    /* Allocate disk image */
    uft_disk_image_t *disk = calloc(1, sizeof(uft_disk_image_t));
    if (!disk) {
        fclose(f);
        return UFT_ERR_MEMORY;
    }
    
    disk->tracks = geom.cylinders;
    disk->heads = geom.heads;
    disk->sectors_per_track = geom.sectors;
    disk->bytes_per_sector = size_from_code(geom.sector_size);
    disk->encoding = geom.encoding ? UFT_ENCODING_MFM : UFT_ENCODING_FM;
    disk->track_count = disk->tracks * disk->heads;
    
    /* Allocate track array */
    disk->track_data = calloc(disk->track_count, sizeof(void*));
    if (!disk->track_data) {
        free(disk);
        fclose(f);
        return UFT_ERR_MEMORY;
    }
    
    /* Read tracks */
    block_offset = read_le32((uint8_t*)&header.first_block);
    while (block_offset > 0) {
        fseek(f, block_offset, SEEK_SET);
        
        ldbs_block_header_t bh;
        if (fread(&bh, sizeof(bh), 1, f) != 1) break;
        
        uint16_t btype = read_le16((uint8_t*)&bh.type);
        uint32_t blen = read_le32((uint8_t*)&bh.length);
        
        if (btype == LDBS_BT_TRACK && blen >= sizeof(ldbs_track_header_t)) {
            ldbs_track_header_t th;
            fread(&th, sizeof(th), 1, f);
            
            int idx = th.cylinder * disk->heads + th.head;
            if (idx >= 0 && idx < (int)disk->track_count) {
                uft_track_t *track = uft_track_alloc(th.sector_count, 0);
                if (track) {
                    track->track_num = th.cylinder;
                    track->head = th.head;
                    track->encoding = th.encoding ? UFT_ENCODING_MFM : UFT_ENCODING_FM;
                    disk->track_data[idx] = track;
                }
            }
        }
        
        block_offset = read_le32((uint8_t*)&bh.next);
    }
    
    fclose(f);
    *out = disk;
    return UFT_OK;
}

/* ============================================================================
 * Write Functions
 * ============================================================================ */

int uft_ldbs_write(const char *path, const uft_disk_image_t *disk) {
    if (!path || !disk) return UFT_ERR_INVALID_ARG;
    
    FILE *f = fopen(path, "wb");
    if (!f) return UFT_ERR_FILE_CREATE;
    
    /* Write header */
    ldbs_header_t header = {0};
    memcpy(header.magic, LDBS_MAGIC, LDBS_MAGIC_LEN);
    write_le32((uint8_t*)&header.version, 1);
    write_le32((uint8_t*)&header.first_block, sizeof(ldbs_header_t));
    fwrite(&header, sizeof(header), 1, f);
    
    /* Write geometry block */
    ldbs_block_header_t bh = {0};
    write_le16((uint8_t*)&bh.type, LDBS_BT_GEOM);
    write_le32((uint8_t*)&bh.length, sizeof(ldbs_geometry_t));
    fwrite(&bh, sizeof(bh), 1, f);
    
    ldbs_geometry_t geom = {0};
    geom.cylinders = disk->tracks;
    geom.heads = disk->heads;
    geom.sectors = disk->sectors_per_track;
    geom.sector_size = code_from_size(disk->bytes_per_sector);
    geom.encoding = (disk->encoding == UFT_ENCODING_MFM) ? 1 : 0;
    fwrite(&geom, sizeof(geom), 1, f);
    
    fclose(f);
    return UFT_OK;
}

/* ============================================================================
 * Info Functions
 * ============================================================================ */

int uft_ldbs_get_info(const char *path, char *buf, size_t buf_size) {
    if (!path || !buf) return UFT_ERR_INVALID_ARG;
    
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERR_FILE_OPEN;
    
    ldbs_header_t header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return UFT_ERR_FILE_READ;
    }
    
    fclose(f);
    
    if (memcmp(header.magic, LDBS_MAGIC, LDBS_MAGIC_LEN) != 0) {
        return UFT_ERR_FORMAT;
    }
    
    snprintf(buf, buf_size,
        "Format: LDBS (LibDsk Block Store)\n"
        "Version: %u\n"
        "Block Count: %u\n",
        read_le32((uint8_t*)&header.version),
        read_le32((uint8_t*)&header.block_count));
    
    return UFT_OK;
}
