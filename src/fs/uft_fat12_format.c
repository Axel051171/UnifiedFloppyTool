/**
 * @file uft_fat12_format.c
 * @brief FAT12/FAT16 Disk Formatting
 * @version 3.6.0
 * 
 * Format new FAT images, create standard floppy images
 */

#include "uft/fs/uft_fat12.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*===========================================================================
 * Internal Helpers
 *===========================================================================*/

/** Write little-endian 16-bit */
static inline void write_le16(uint8_t *p, uint16_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

/** Write little-endian 32-bit */
static inline void write_le32(uint8_t *p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

/*===========================================================================
 * Standard Floppy Geometries
 *===========================================================================*/

/** Standard floppy geometries */
static const uft_fat_geometry_t fat_geometries[] = {
    /* 5.25" formats */
    { "160KB 5.25\" SS/DD",  320, 8, 1, 40, 1, 64,  1, 0xFE, UFT_FAT_PLATFORM_PC },
    { "180KB 5.25\" SS/DD",  360, 9, 1, 40, 1, 64,  2, 0xFC, UFT_FAT_PLATFORM_PC },
    { "320KB 5.25\" DS/DD",  640, 8, 2, 40, 2, 112, 1, 0xFF, UFT_FAT_PLATFORM_PC },
    { "360KB 5.25\" DS/DD",  720, 9, 2, 40, 2, 112, 2, 0xFD, UFT_FAT_PLATFORM_PC },
    { "1.2MB 5.25\" DS/HD", 2400, 15, 2, 80, 1, 224, 7, 0xF9, UFT_FAT_PLATFORM_PC },
    
    /* 3.5" formats */
    { "720KB 3.5\" DS/DD",  1440, 9, 2, 80, 2, 112, 3, 0xF9, UFT_FAT_PLATFORM_PC },
    { "1.44MB 3.5\" DS/HD", 2880, 18, 2, 80, 1, 224, 9, 0xF0, UFT_FAT_PLATFORM_PC },
    { "2.88MB 3.5\" DS/ED", 5760, 36, 2, 80, 2, 240, 9, 0xF0, UFT_FAT_PLATFORM_PC },
    
    /* Atari ST */
    { "360KB Atari SS/DD",  720, 9, 1, 80, 2, 112, 2, 0xF9, UFT_FAT_PLATFORM_ATARI },
    { "720KB Atari DS/DD", 1440, 9, 2, 80, 2, 112, 3, 0xF9, UFT_FAT_PLATFORM_ATARI },
    
    /* MSX */
    { "360KB MSX DS/DD",   720, 9, 2, 40, 2, 112, 2, 0xF9, UFT_FAT_PLATFORM_MSX },
    { "720KB MSX DS/DD",  1440, 9, 2, 80, 2, 112, 3, 0xF9, UFT_FAT_PLATFORM_MSX },
    
    /* End marker */
    { NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

const uft_fat_geometry_t *uft_fat_get_geometry(size_t index) {
    if (fat_geometries[index].name == NULL) return NULL;
    return &fat_geometries[index];
}

const uft_fat_geometry_t *uft_fat_find_geometry(uint32_t total_sectors) {
    for (size_t i = 0; fat_geometries[i].name; i++) {
        if (fat_geometries[i].total_sectors == total_sectors) {
            return &fat_geometries[i];
        }
    }
    return NULL;
}

/*===========================================================================
 * Boot Sector Creation
 *===========================================================================*/

/**
 * @brief Create boot sector
 */
static void create_boot_sector(uint8_t *sector, const uft_fat_geometry_t *geom,
                               const char *label, uint32_t serial, const char *oem) {
    memset(sector, 0, UFT_FAT_SECTOR_SIZE);
    
    /* Jump instruction (JMP short + NOP) */
    sector[0] = 0xEB;
    sector[1] = 0x3C;
    sector[2] = 0x90;
    
    /* OEM name */
    const char *oem_name = oem ? oem : "UFT 3.6 ";
    memset(sector + 3, ' ', 8);
    strncpy((char *)sector + 3, oem_name, 8);
    
    /* BIOS Parameter Block (BPB) */
    write_le16(sector + 0x0B, UFT_FAT_SECTOR_SIZE);       /* Bytes per sector */
    sector[0x0D] = geom->sectors_per_cluster;             /* Sectors per cluster */
    write_le16(sector + 0x0E, 1);                         /* Reserved sectors */
    sector[0x10] = 2;                                     /* Number of FATs */
    write_le16(sector + 0x11, geom->root_entries);        /* Root directory entries */
    
    if (geom->total_sectors < 0x10000) {
        write_le16(sector + 0x13, geom->total_sectors);   /* Total sectors (16-bit) */
    } else {
        write_le16(sector + 0x13, 0);
        write_le32(sector + 0x20, geom->total_sectors);   /* Total sectors (32-bit) */
    }
    
    sector[0x15] = geom->media_type;                      /* Media descriptor */
    write_le16(sector + 0x16, geom->fat_sectors);         /* Sectors per FAT */
    write_le16(sector + 0x18, geom->sectors_per_track);   /* Sectors per track */
    write_le16(sector + 0x1A, geom->heads);               /* Number of heads */
    write_le32(sector + 0x1C, 0);                         /* Hidden sectors */
    
    /* Extended BPB (DOS 3.4+) */
    sector[0x24] = 0x00;                                  /* Physical drive number */
    sector[0x25] = 0x00;                                  /* Reserved */
    sector[0x26] = 0x29;                                  /* Extended boot signature */
    
    /* Serial number */
    if (serial == 0) {
        /* Generate from current time */
        time_t now = time(NULL);
        serial = (uint32_t)now ^ ((uint32_t)now << 16);
    }
    write_le32(sector + 0x27, serial);
    
    /* Volume label */
    memset(sector + 0x2B, ' ', 11);
    if (label && label[0]) {
        size_t len = strlen(label);
        if (len > 11) len = 11;
        memcpy(sector + 0x2B, label, len);
    } else {
        memcpy(sector + 0x2B, "NO NAME    ", 11);
    }
    
    /* File system type */
    uint32_t data_clusters = geom->total_sectors - 1 - (2 * geom->fat_sectors) -
                            ((geom->root_entries * 32 + 511) / 512);
    data_clusters /= geom->sectors_per_cluster;
    
    if (data_clusters < 4085) {
        memcpy(sector + 0x36, "FAT12   ", 8);
    } else {
        memcpy(sector + 0x36, "FAT16   ", 8);
    }
    
    /* Boot signature */
    sector[0x1FE] = 0x55;
    sector[0x1FF] = 0xAA;
}

/*===========================================================================
 * FAT Table Initialization
 *===========================================================================*/

/**
 * @brief Initialize FAT table
 */
static void init_fat(uint8_t *fat, size_t fat_bytes, uint8_t media_type,
                     bool is_fat12) {
    memset(fat, 0, fat_bytes);
    
    if (is_fat12) {
        /* FAT12: First two entries are reserved */
        /* Entry 0: Media descriptor + 0xFF */
        fat[0] = media_type;
        fat[1] = 0xFF;
        fat[2] = 0xFF;
    } else {
        /* FAT16: First two entries are reserved */
        fat[0] = media_type;
        fat[1] = 0xFF;
        fat[2] = 0xFF;
        fat[3] = 0xFF;
    }
}

/*===========================================================================
 * Format Operations
 *===========================================================================*/

int uft_fat_format(uft_fat_ctx_t *ctx, const uft_fat_format_opts_t *opts) {
    if (!ctx || !ctx->data || !opts || !opts->geometry) {
        return UFT_FAT_ERR_INVALID;
    }
    if (ctx->read_only) return UFT_FAT_ERR_READONLY;
    
    const uft_fat_geometry_t *geom = opts->geometry;
    
    /* Verify image size */
    size_t required_size = (size_t)geom->total_sectors * UFT_FAT_SECTOR_SIZE;
    if (ctx->data_size < required_size) {
        return UFT_FAT_ERR_INVALID;
    }
    
    /* Zero entire image if not quick format */
    if (!opts->quick_format) {
        memset(ctx->data, 0, ctx->data_size);
    }
    
    /* Create boot sector */
    create_boot_sector(ctx->data, geom, opts->label, opts->serial, opts->oem_name);
    
    /* Determine FAT type */
    uint32_t root_sectors = (geom->root_entries * 32 + UFT_FAT_SECTOR_SIZE - 1) / UFT_FAT_SECTOR_SIZE;
    uint32_t data_sectors = geom->total_sectors - 1 - (2 * geom->fat_sectors) - root_sectors;
    uint32_t data_clusters = data_sectors / geom->sectors_per_cluster;
    bool is_fat12 = (data_clusters < 4085);
    
    /* Initialize FAT tables */
    size_t fat_bytes = geom->fat_sectors * UFT_FAT_SECTOR_SIZE;
    uint8_t *fat1 = ctx->data + UFT_FAT_SECTOR_SIZE;  /* After boot sector */
    uint8_t *fat2 = fat1 + fat_bytes;
    
    init_fat(fat1, fat_bytes, geom->media_type, is_fat12);
    memcpy(fat2, fat1, fat_bytes);  /* Copy to second FAT */
    
    /* Initialize root directory (zeros) */
    uint32_t root_start = 1 + (2 * geom->fat_sectors);
    uint8_t *root = ctx->data + root_start * UFT_FAT_SECTOR_SIZE;
    memset(root, 0, root_sectors * UFT_FAT_SECTOR_SIZE);
    
    /* Create volume label entry if specified */
    if (opts->label && opts->label[0]) {
        uint8_t *vol_entry = root;
        memset(vol_entry, ' ', 11);
        size_t len = strlen(opts->label);
        if (len > 11) len = 11;
        for (size_t i = 0; i < len; i++) {
            vol_entry[i] = toupper((unsigned char)opts->label[i]);
        }
        vol_entry[11] = UFT_FAT_ATTR_VOLUME_ID;
        
        /* Timestamp */
        uint16_t fat_time, fat_date;
        uft_fat_from_unix_time(time(NULL), &fat_time, &fat_date);
        write_le16(vol_entry + 14, fat_time);
        write_le16(vol_entry + 16, fat_date);
        write_le16(vol_entry + 22, fat_time);
        write_le16(vol_entry + 24, fat_date);
    }
    
    ctx->modified = true;
    
    /* Re-open to parse the formatted structure */
    return uft_fat_open(ctx, ctx->data, ctx->data_size, false);
}

int uft_fat_create_image(const char *filename, const uft_fat_format_opts_t *opts) {
    if (!filename || !opts || !opts->geometry) {
        return UFT_FAT_ERR_INVALID;
    }
    
    const uft_fat_geometry_t *geom = opts->geometry;
    size_t image_size = (size_t)geom->total_sectors * UFT_FAT_SECTOR_SIZE;
    
    /* Allocate memory */
    uint8_t *data = calloc(1, image_size);
    if (!data) return UFT_FAT_ERR_NOMEM;
    
    /* Create context */
    uft_fat_ctx_t *ctx = uft_fat_create();
    if (!ctx) {
        free(data);
        return UFT_FAT_ERR_NOMEM;
    }
    
    ctx->data = data;
    ctx->data_size = image_size;
    ctx->owns_data = true;
    
    /* Format */
    int rc = uft_fat_format(ctx, opts);
    if (rc != 0) {
        uft_fat_destroy(ctx);
        return rc;
    }
    
    /* Write to file */
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        uft_fat_destroy(ctx);
        return UFT_FAT_ERR_IO;
    }
    
    if (fwrite(data, 1, image_size, fp) != image_size) {
        fclose(fp);
        uft_fat_destroy(ctx);
        return UFT_FAT_ERR_IO;
    }
    
    fclose(fp);
    uft_fat_destroy(ctx);
    
    return 0;
}

/*===========================================================================
 * Quick Format Helpers
 *===========================================================================*/

/**
 * @brief Create a new 1.44MB floppy image
 */
int uft_fat_create_1440k(const char *filename, const char *label) {
    const uft_fat_geometry_t *geom = uft_fat_find_geometry(2880);
    if (!geom) return UFT_FAT_ERR_INVALID;
    
    uft_fat_format_opts_t opts = {
        .geometry = geom,
        .label = label,
        .serial = 0,
        .oem_name = NULL,
        .quick_format = false,
        .bootable = false
    };
    
    return uft_fat_create_image(filename, &opts);
}

/**
 * @brief Create a new 720KB floppy image
 */
int uft_fat_create_720k(const char *filename, const char *label) {
    const uft_fat_geometry_t *geom = uft_fat_find_geometry(1440);
    if (!geom) return UFT_FAT_ERR_INVALID;
    
    uft_fat_format_opts_t opts = {
        .geometry = geom,
        .label = label,
        .serial = 0,
        .oem_name = NULL,
        .quick_format = false,
        .bootable = false
    };
    
    return uft_fat_create_image(filename, &opts);
}

/**
 * @brief Create a new 360KB floppy image
 */
int uft_fat_create_360k(const char *filename, const char *label) {
    const uft_fat_geometry_t *geom = uft_fat_find_geometry(720);
    if (!geom) return UFT_FAT_ERR_INVALID;
    
    uft_fat_format_opts_t opts = {
        .geometry = geom,
        .label = label,
        .serial = 0,
        .oem_name = NULL,
        .quick_format = false,
        .bootable = false
    };
    
    return uft_fat_create_image(filename, &opts);
}
