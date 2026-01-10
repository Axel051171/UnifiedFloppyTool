/**
 * @file uft_apple_format.c
 * @brief Apple II Disk Image Creation and Formatting
 * @version 3.7.0
 * 
 * Create new DOS 3.3 and ProDOS disk images.
 */

#include "uft/fs/uft_apple_dos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include "uft/uft_compiler.h"

/*===========================================================================
 * DOS 3.3 Image Creation
 *===========================================================================*/

/**
 * @brief Initialize DOS 3.3 VTOC
 */
static void init_dos33_vtoc(uft_dos33_vtoc_t *vtoc, uint8_t volume) {
    memset(vtoc, 0, sizeof(*vtoc));
    
    vtoc->catalog_track = UFT_DOS33_CATALOG_TRACK;
    vtoc->catalog_sector = UFT_DOS33_CATALOG_SECTOR;
    vtoc->dos_version = 3;
    vtoc->volume_number = volume;
    vtoc->max_ts_pairs = 122;
    vtoc->last_track_alloc = 17;
    vtoc->alloc_direction = 1;
    vtoc->tracks_per_disk = 35;
    vtoc->sectors_per_track = 16;
    vtoc->bytes_per_sector = 256;
    
    /* Initialize bitmap: all sectors free except track 0 (boot) and 17 (catalog) */
    for (int t = 0; t < 35; t++) {
        int byte_offset = t * 4;
        
        if (t == 0) {
            /* Track 0: sectors 0-2 used for boot, rest free */
            vtoc->bitmap[byte_offset + 0] = 0x1F;  /* sectors 0-2 used */
            vtoc->bitmap[byte_offset + 1] = 0xFF;  /* sectors 8-15 free */
        } else if (t == 17) {
            /* Track 17: all sectors used for VTOC and catalog */
            vtoc->bitmap[byte_offset + 0] = 0x00;
            vtoc->bitmap[byte_offset + 1] = 0x00;
        } else {
            /* All sectors free */
            vtoc->bitmap[byte_offset + 0] = 0xFF;
            vtoc->bitmap[byte_offset + 1] = 0xFF;
        }
    }
}

/**
 * @brief Initialize DOS 3.3 catalog chain
 */
static void init_dos33_catalog(uint8_t *image) {
    /* Catalog sectors: Track 17, Sectors 15-1 (linked list) */
    for (int s = 15; s >= 1; s--) {
        size_t offset = (17 * 16 + s) * UFT_APPLE_SECTOR_SIZE;
        
        /* Clear sector */
        memset(&image[offset], 0, UFT_APPLE_SECTOR_SIZE);
        
        /* Link to next catalog sector */
        if (s > 1) {
            image[offset + 1] = 17;      /* Next track */
            image[offset + 2] = s - 1;   /* Next sector */
        }
    }
}

int uft_apple_create_dos33(const char *filename, uint8_t volume) {
    if (!filename) return UFT_APPLE_ERR_INVALID;
    
    /* Validate volume number */
    if (volume == 0 || volume == 255) volume = 254;
    
    /* Allocate image buffer */
    size_t image_size = UFT_APPLE_TRACKS * UFT_APPLE_SECTORS_PER_TRACK * 
                        UFT_APPLE_SECTOR_SIZE;
    uint8_t *image = calloc(1, image_size);
    if (!image) return UFT_APPLE_ERR_NOMEM;
    
    /* Initialize VTOC */
    uft_dos33_vtoc_t vtoc;
    init_dos33_vtoc(&vtoc, volume);
    
    size_t vtoc_offset = (UFT_DOS33_VTOC_TRACK * UFT_APPLE_SECTORS_PER_TRACK + 
                          UFT_DOS33_VTOC_SECTOR) * UFT_APPLE_SECTOR_SIZE;
    memcpy(&image[vtoc_offset], &vtoc, sizeof(vtoc));
    
    /* Initialize catalog chain */
    init_dos33_catalog(image);
    
    /* Write boot sector signature (minimal) */
    image[0] = 0x01;  /* Jump to boot code */
    
    /* Write file */
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        free(image);
        return UFT_APPLE_ERR_IO;
    }
    
    size_t written = fwrite(image, 1, image_size, fp);
    fclose(fp);
    free(image);
    
    if (written != image_size) return UFT_APPLE_ERR_IO;
    
    return 0;
}

/*===========================================================================
 * ProDOS Image Creation
 *===========================================================================*/

/**
 * @brief Initialize ProDOS volume directory header
 */
static void init_prodos_volume_header(uint8_t *block, const char *name, 
                                      uint16_t total_blocks) {
    memset(block, 0, 512);
    
    /* Previous block = 0 */
    block[0] = 0;
    block[1] = 0;
    
    /* Next block = 3 (if directory spans multiple blocks) */
    block[2] = 3;
    block[3] = 0;
    
    /* Volume directory header (entry 0) */
    int name_len = strlen(name);
    if (name_len > 15) name_len = 15;
    
    /* Storage type 0xF + name length */
    block[4] = (0x0F << 4) | name_len;
    
    /* Volume name (uppercase) */
    for (int i = 0; i < name_len; i++) {
        block[5 + i] = toupper(name[i]);
    }
    
    /* Reserved (0x14-0x1B) = 0 */
    
    /* Creation date/time */
    uft_prodos_datetime_t now = uft_prodos_from_unix_time(time(NULL));
    block[0x1C] = now.date & 0xFF;
    block[0x1D] = (now.date >> 8) & 0xFF;
    block[0x1E] = now.time & 0xFF;
    block[0x1F] = (now.time >> 8) & 0xFF;
    
    /* Version = 0 */
    block[0x20] = 0;
    
    /* Min version = 0 */
    block[0x21] = 0;
    
    /* Access = 0xC3 (destroy, rename, write, read enabled) */
    block[0x22] = 0xC3;
    
    /* Entry length = 0x27 (39 bytes) */
    block[0x23] = 0x27;
    
    /* Entries per block = 0x0D (13) */
    block[0x24] = 0x0D;
    
    /* File count = 0 */
    block[0x25] = 0;
    block[0x26] = 0;
    
    /* Bitmap pointer (block 6 typically) */
    block[0x27] = 6;
    block[0x28] = 0;
    
    /* Total blocks */
    block[0x29] = total_blocks & 0xFF;
    block[0x2A] = (total_blocks >> 8) & 0xFF;
}

/**
 * @brief Initialize ProDOS volume bitmap
 */
static void init_prodos_bitmap(uint8_t *image, uint16_t bitmap_block, 
                               uint16_t total_blocks) {
    /* Calculate bitmap size */
    uint16_t bitmap_bytes = (total_blocks + 7) / 8;
    uint16_t bitmap_blocks = (bitmap_bytes + 511) / 512;
    
    /* Initialize all blocks as free (bit = 1) */
    for (uint16_t b = 0; b < bitmap_blocks; b++) {
        size_t offset = (bitmap_block + b) * 512;
        memset(&image[offset], 0xFF, 512);
    }
    
    /* Mark boot blocks as used (blocks 0-1) */
    size_t bitmap_offset = bitmap_block * 512;
    image[bitmap_offset] &= 0x3F;  /* Blocks 0,1 used */
    
    /* Mark volume directory blocks as used (blocks 2-5) */
    image[bitmap_offset] &= 0x03;  /* Blocks 2-7 used (we use 2-5) */
    
    /* Mark bitmap blocks as used */
    for (uint16_t b = 0; b < bitmap_blocks; b++) {
        uint16_t block = bitmap_block + b;
        uint16_t byte_idx = block / 8;
        uint8_t bit = 7 - (block & 7);
        image[bitmap_offset + byte_idx] &= ~(1 << bit);
    }
    
    /* Mark blocks beyond total as used */
    for (uint16_t b = total_blocks; b < (bitmap_bytes * 8); b++) {
        uint16_t byte_idx = b / 8;
        uint8_t bit = 7 - (b & 7);
        if (byte_idx < bitmap_bytes) {
            image[bitmap_offset + byte_idx] &= ~(1 << bit);
        }
    }
}

int uft_apple_create_prodos(const char *filename, const char *volume_name,
                            uint16_t blocks) {
    if (!filename || !volume_name) return UFT_APPLE_ERR_INVALID;
    
    /* Validate block count */
    if (blocks == 0) blocks = 280;  /* Default: 140K floppy */
    if (blocks < 16) blocks = 16;
    if (blocks > 65535) blocks = 65535;
    
    /* Allocate image buffer */
    size_t image_size = blocks * 512;
    uint8_t *image = calloc(1, image_size);
    if (!image) return UFT_APPLE_ERR_NOMEM;
    
    /* Block 0-1: Boot blocks (leave zeroed for now) */
    
    /* Block 2: Volume directory key block */
    init_prodos_volume_header(&image[2 * 512], volume_name, blocks);
    
    /* Blocks 3-5: Additional directory blocks */
    for (int b = 3; b <= 5; b++) {
        size_t offset = b * 512;
        
        /* Previous block */
        image[offset + 0] = (b - 1) & 0xFF;
        image[offset + 1] = ((b - 1) >> 8) & 0xFF;
        
        /* Next block */
        if (b < 5) {
            image[offset + 2] = (b + 1) & 0xFF;
            image[offset + 3] = ((b + 1) >> 8) & 0xFF;
        }
    }
    
    /* Block 6+: Volume bitmap */
    init_prodos_bitmap(image, 6, blocks);
    
    /* Write file */
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        free(image);
        return UFT_APPLE_ERR_IO;
    }
    
    size_t written = fwrite(image, 1, image_size, fp);
    fclose(fp);
    free(image);
    
    if (written != image_size) return UFT_APPLE_ERR_IO;
    
    return 0;
}

/*===========================================================================
 * High-Level Format Functions
 *===========================================================================*/

/**
 * @brief Create empty disk image of specified type
 */
int uft_apple_create_image(const char *filename, uft_apple_fs_t fs_type,
                           const char *volume_name, uint8_t volume_number,
                           uint16_t size_blocks) {
    switch (fs_type) {
    case UFT_APPLE_FS_DOS33:
    case UFT_APPLE_FS_DOS32:
        return uft_apple_create_dos33(filename, volume_number);
        
    case UFT_APPLE_FS_PRODOS:
        return uft_apple_create_prodos(filename, 
                                       volume_name ? volume_name : "BLANK",
                                       size_blocks);
        
    default:
        return UFT_APPLE_ERR_BADTYPE;
    }
}

/*===========================================================================
 * DSK/DO/PO File Handling
 *===========================================================================*/

/**
 * @brief Detect image format by extension
 */
uft_apple_order_t uft_apple_detect_order_by_extension(const char *filename) {
    if (!filename) return UFT_APPLE_ORDER_DOS;
    
    const char *ext = strrchr(filename, '.');
    if (!ext) return UFT_APPLE_ORDER_DOS;
    
    ext++;  /* Skip dot */
    
    /* Case-insensitive comparison */
    char lower[8];
    int i;
    for (i = 0; i < 7 && ext[i]; i++) {
        lower[i] = tolower(ext[i]);
    }
    lower[i] = '\0';
    
    if (strcmp(lower, "po") == 0) return UFT_APPLE_ORDER_PRODOS;
    if (strcmp(lower, "dsk") == 0) return UFT_APPLE_ORDER_DOS;
    if (strcmp(lower, "do") == 0) return UFT_APPLE_ORDER_DOS;
    
    return UFT_APPLE_ORDER_DOS;  /* Default */
}

/**
 * @brief Convert between sector orderings
 */
int uft_apple_convert_order(const uint8_t *src, uint8_t *dst, size_t size,
                            uft_apple_order_t from, uft_apple_order_t to) {
    if (!src || !dst || size == 0) return UFT_APPLE_ERR_INVALID;
    
    if (from == to) {
        if (src != dst) memcpy(dst, src, size);
        return 0;
    }
    
    /* DOS <-> ProDOS sector interleave conversion */
    static const uint8_t dos_to_prodos[16] = {
        0x0, 0xE, 0xD, 0xC, 0xB, 0xA, 0x9, 0x8,
        0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1, 0xF
    };
    
    static const uint8_t prodos_to_dos[16] = {
        0x0, 0xE, 0xD, 0xC, 0xB, 0xA, 0x9, 0x8,
        0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1, 0xF
    };
    
    const uint8_t *map;
    if (from == UFT_APPLE_ORDER_DOS && to == UFT_APPLE_ORDER_PRODOS) {
        map = dos_to_prodos;
    } else if (from == UFT_APPLE_ORDER_PRODOS && to == UFT_APPLE_ORDER_DOS) {
        map = prodos_to_dos;
    } else {
        return UFT_APPLE_ERR_INVALID;
    }
    
    /* Temporary buffer if src == dst */
    uint8_t *temp = NULL;
    if (src == dst) {
        temp = malloc(16 * UFT_APPLE_SECTOR_SIZE);
        if (!temp) return UFT_APPLE_ERR_NOMEM;
    }
    
    size_t tracks = size / (16 * UFT_APPLE_SECTOR_SIZE);
    
    for (size_t t = 0; t < tracks; t++) {
        const uint8_t *src_track = src + t * 16 * UFT_APPLE_SECTOR_SIZE;
        uint8_t *dst_track = dst + t * 16 * UFT_APPLE_SECTOR_SIZE;
        
        if (temp) {
            /* Copy track to temp, then rearrange */
            memcpy(temp, src_track, 16 * UFT_APPLE_SECTOR_SIZE);
            src_track = temp;
        }
        
        for (int s = 0; s < 16; s++) {
            memcpy(dst_track + map[s] * UFT_APPLE_SECTOR_SIZE,
                   src_track + s * UFT_APPLE_SECTOR_SIZE,
                   UFT_APPLE_SECTOR_SIZE);
        }
    }
    
    free(temp);
    return 0;
}

/*===========================================================================
 * 2IMG Header Support
 *===========================================================================*/

/** 2IMG header structure */
UFT_PACK_BEGIN
typedef struct {
    char magic[4];          /* "2IMG" */
    char creator[4];        /* Creator ID */
    uint16_t header_size;   /* Header size (64) */
    uint16_t version;       /* Version (1) */
    uint32_t format;        /* 0=DOS, 1=ProDOS, 2=NIB */
    uint32_t flags;         /* Flags */
    uint32_t blocks;        /* ProDOS blocks */
    uint32_t data_offset;   /* Data offset */
    uint32_t data_size;     /* Data size */
    uint32_t comment_offset; /* Comment offset */
    uint32_t comment_size;  /* Comment size */
    uint32_t creator_offset; /* Creator data offset */
    uint32_t creator_size;  /* Creator data size */
    uint8_t reserved[16];   /* Reserved */
} uft_2img_header_t;
UFT_PACK_END

/**
 * @brief Check if file is 2IMG format
 */
bool uft_apple_is_2img(const uint8_t *data, size_t size) {
    if (size < 64) return false;
    return (data[0] == '2' && data[1] == 'I' && 
            data[2] == 'M' && data[3] == 'G');
}

/**
 * @brief Parse 2IMG header
 */
int uft_apple_parse_2img(const uint8_t *data, size_t size,
                         uft_apple_order_t *order, size_t *data_offset,
                         size_t *data_size) {
    if (!uft_apple_is_2img(data, size)) {
        return UFT_APPLE_ERR_INVALID;
    }
    
    const uft_2img_header_t *hdr = (const uft_2img_header_t *)data;
    
    if (hdr->header_size < 64) return UFT_APPLE_ERR_INVALID;
    
    switch (hdr->format) {
    case 0:
        if (order) *order = UFT_APPLE_ORDER_DOS;
        break;
    case 1:
        if (order) *order = UFT_APPLE_ORDER_PRODOS;
        break;
    default:
        return UFT_APPLE_ERR_BADTYPE;  /* NIB not supported here */
    }
    
    if (data_offset) *data_offset = hdr->data_offset ? hdr->data_offset : 64;
    if (data_size) *data_size = hdr->data_size ? hdr->data_size : (size - 64);
    
    return 0;
}

/**
 * @brief Create 2IMG header
 */
void uft_apple_create_2img_header(uint8_t *header, uft_apple_order_t order,
                                  uint32_t data_size, uint32_t blocks) {
    memset(header, 0, 64);
    
    uft_2img_header_t *hdr = (uft_2img_header_t *)header;
    
    memcpy(hdr->magic, "2IMG", 4);
    memcpy(hdr->creator, "UFT!", 4);
    hdr->header_size = 64;
    hdr->version = 1;
    hdr->format = (order == UFT_APPLE_ORDER_PRODOS) ? 1 : 0;
    hdr->flags = 0;
    hdr->blocks = blocks;
    hdr->data_offset = 64;
    hdr->data_size = data_size;
}
