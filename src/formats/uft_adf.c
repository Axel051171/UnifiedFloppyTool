/**
 * @file uft_adf.c
 * @brief Amiga ADF Implementation for UFT
 * 
 * @copyright UFT Project 2025
 */

#include "uft/core/uft_error_compat.h"
#include "uft/uft_adf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*============================================================================
 * Big-Endian Conversion (Amiga is big-endian)
 *============================================================================*/

static inline uint32_t be32_to_cpu(uint32_t val) {
    const uint8_t *p = (const uint8_t *)&val;
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

static inline uint32_t cpu_to_be32(uint32_t val) {
    uint32_t result;
    uint8_t *p = (uint8_t *)&result;
    p[0] = (val >> 24) & 0xFF;
    p[1] = (val >> 16) & 0xFF;
    p[2] = (val >> 8) & 0xFF;
    p[3] = val & 0xFF;
    return result;
}

static inline uint16_t be16_to_cpu(uint16_t val) {
    const uint8_t *p = (const uint8_t *)&val;
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

/*============================================================================
 * Volume Structure
 *============================================================================*/

struct uft_adf_volume {
    FILE           *fp;
    void           *mem_data;
    size_t          mem_size;
    bool            readonly;
    bool            is_memory;
    
    uft_adf_density_t density;
    uft_adf_fs_type_t fs_type;
    uint32_t        total_blocks;
    uint32_t        root_block;
    
    /* Cached boot block */
    uint8_t         bootblock[UFT_ADF_BOOTBLOCK_SIZE];
    
    /* Cached root block */
    uint8_t         rootblock_data[UFT_ADF_SECTOR_SIZE];
    
    /* Volume name */
    char            name[UFT_ADF_MAX_NAME + 1];
    
    /* Bitmap (lazy loaded) */
    uint8_t        *bitmap;
    uint32_t        free_blocks;
};

/*============================================================================
 * Directory Iterator
 *============================================================================*/

struct uft_adf_dir_iter {
    uft_adf_volume_t *vol;
    uint32_t        dir_block;
    uint32_t        ht[UFT_ADF_HT_SIZE];
    int             ht_index;
    uint32_t        chain_block;
};

/*============================================================================
 * Amiga Date Conversion
 *============================================================================*/

/* Amiga epoch: January 1, 1978 */
#define AMIGA_EPOCH_DIFF    252460800   /* Seconds from 1970 to 1978 */

time_t uft_adf_to_unix_time(uint32_t days, uint32_t mins, uint32_t ticks) {
    time_t t = AMIGA_EPOCH_DIFF;
    t += (time_t)days * 86400;      /* Days to seconds */
    t += (time_t)mins * 60;          /* Minutes to seconds */
    t += (time_t)ticks / 50;         /* Ticks to seconds (50 ticks/sec) */
    return t;
}

void uft_unix_to_adf_time(time_t t, uint32_t *days, uint32_t *mins,
                          uint32_t *ticks) {
    if (t < AMIGA_EPOCH_DIFF) {
        *days = *mins = *ticks = 0;
        return;
    }
    
    t -= AMIGA_EPOCH_DIFF;
    *days = (uint32_t)(t / 86400);
    t %= 86400;
    *mins = (uint32_t)(t / 60);
    t %= 60;
    *ticks = (uint32_t)(t * 50);
}

/*============================================================================
 * Checksum
 *============================================================================*/

uint32_t uft_adf_checksum(const void *block) {
    const uint32_t *data = (const uint32_t *)block;
    uint32_t sum = 0;
    
    /* Simple 32-bit sum of all longwords (no carry handling) */
    for (int i = 0; i < 128; i++) {
        sum += be32_to_cpu(data[i]);
    }
    
    return -sum;  /* Negate so total becomes 0 */
}

bool uft_adf_verify_checksum(const void *block) {
    const uint32_t *data = (const uint32_t *)block;
    uint32_t sum = 0;
    
    /* Simple 32-bit sum - should be 0 if checksum is correct */
    for (int i = 0; i < 128; i++) {
        sum += be32_to_cpu(data[i]);
    }
    
    return (sum == 0);
}

/*============================================================================
 * Filename Hash
 *============================================================================*/

static const uint8_t intl_toupper[256] = {
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
    0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,
    0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
    0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
    0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
    0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,
    0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,
    0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,
    0x58,0x59,0x5A,0x5B,0x5C,0x5D,0x5E,0x5F,
    0x60,0x41,0x42,0x43,0x44,0x45,0x46,0x47, /* a-g -> A-G */
    0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F, /* h-o -> H-O */
    0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57, /* p-w -> P-W */
    0x58,0x59,0x5A,0x7B,0x7C,0x7D,0x7E,0x7F, /* x-z -> X-Z */
    0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,
    0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,
    0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,
    0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F,
    0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,
    0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,
    0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,
    0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF,
    0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7, /* à-ç -> À-Ç */
    0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,
    0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7, /* ð-× -> Ð-× */
    0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, /* ø-þ -> Ø-Þ */
    0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,
    0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,
    0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xF7,
    0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xFF
};

uint32_t uft_adf_hash_name(const char *name, bool intl) {
    uint32_t hash = (uint32_t)strlen(name);
    
    for (const char *p = name; *p; p++) {
        uint8_t c = (uint8_t)*p;
        if (intl) {
            c = intl_toupper[c];
        } else {
            c = (uint8_t)toupper(c);
        }
        hash = (hash * 13 + c) & 0x7FF;
    }
    
    return hash % UFT_ADF_HT_SIZE;
}

/*============================================================================
 * Protection Bits
 *============================================================================*/

/* Amiga protection bits (active LOW for HSPARWED) */
#define PROT_DELETE     (1 << 0)    /* d - deletable */
#define PROT_EXECUTE    (1 << 1)    /* e - executable */
#define PROT_WRITE      (1 << 2)    /* w - writable */
#define PROT_READ       (1 << 3)    /* r - readable */
#define PROT_ARCHIVE    (1 << 4)    /* a - archived */
#define PROT_PURE       (1 << 5)    /* p - pure */
#define PROT_SCRIPT     (1 << 6)    /* s - script */
#define PROT_HOLD       (1 << 7)    /* h - hold */

char *uft_adf_protect_string(uint32_t protect, char *buffer) {
    /* Format: hsparwed (lowercase = set, dash = not set) */
    buffer[0] = (protect & PROT_HOLD)    ? '-' : 'h';
    buffer[1] = (protect & PROT_SCRIPT)  ? '-' : 's';
    buffer[2] = (protect & PROT_PURE)    ? '-' : 'p';
    buffer[3] = (protect & PROT_ARCHIVE) ? '-' : 'a';
    buffer[4] = (protect & PROT_READ)    ? '-' : 'r';
    buffer[5] = (protect & PROT_WRITE)   ? '-' : 'w';
    buffer[6] = (protect & PROT_EXECUTE) ? '-' : 'e';
    buffer[7] = (protect & PROT_DELETE)  ? '-' : 'd';
    buffer[8] = '\0';
    return buffer;
}

/*============================================================================
 * Filesystem Type Detection
 *============================================================================*/

static uft_adf_fs_type_t detect_fs_type(uint32_t dos_type) {
    switch (dos_type) {
        case UFT_ADF_DOS0: return UFT_ADF_FS_OFS;
        case UFT_ADF_DOS1: return UFT_ADF_FS_FFS;
        case UFT_ADF_DOS2: return UFT_ADF_FS_OFS_INTL;
        case UFT_ADF_DOS3: return UFT_ADF_FS_FFS_INTL;
        case UFT_ADF_DOS4: return UFT_ADF_FS_OFS_DC;
        case UFT_ADF_DOS5: return UFT_ADF_FS_FFS_DC;
        case UFT_ADF_DOS6: return UFT_ADF_FS_OFS_LNFS;
        case UFT_ADF_DOS7: return UFT_ADF_FS_FFS_LNFS;
        default:          return UFT_ADF_FS_UNKNOWN;
    }
}

const char *uft_adf_fs_type_string(uft_adf_fs_type_t type) {
    switch (type) {
        case UFT_ADF_FS_OFS:      return "OFS";
        case UFT_ADF_FS_FFS:      return "FFS";
        case UFT_ADF_FS_OFS_INTL: return "OFS-INTL";
        case UFT_ADF_FS_FFS_INTL: return "FFS-INTL";
        case UFT_ADF_FS_OFS_DC:   return "OFS-DC";
        case UFT_ADF_FS_FFS_DC:   return "FFS-DC";
        case UFT_ADF_FS_OFS_LNFS: return "OFS-LNFS";
        case UFT_ADF_FS_FFS_LNFS: return "FFS-LNFS";
        default:                  return "Unknown";
    }
}

/*============================================================================
 * Density Detection
 *============================================================================*/

int uft_adf_detect_density(size_t size) {
    if (size == UFT_ADF_DD_SIZE) return UFT_ADF_DENSITY_DD;
    if (size == UFT_ADF_HD_SIZE) return UFT_ADF_DENSITY_HD;
    return -1;
}

/*============================================================================
 * Block I/O
 *============================================================================*/

int uft_adf_read_block(uft_adf_volume_t *vol, uint32_t block, void *buffer) {
    if (!vol || !buffer) return -1;
    if (block >= vol->total_blocks) return -1;
    
    size_t offset = (size_t)block * UFT_ADF_SECTOR_SIZE;
    
    if (vol->is_memory) {
        if (offset + UFT_ADF_SECTOR_SIZE > vol->mem_size) return -1;
        memcpy(buffer, (uint8_t *)vol->mem_data + offset, UFT_ADF_SECTOR_SIZE);
        return 0;
    }
    
    if (fseek(vol->fp, (long)offset, SEEK_SET) != 0) return -1;
    if (fread(buffer, 1, UFT_ADF_SECTOR_SIZE, vol->fp) != UFT_ADF_SECTOR_SIZE) {
        return -1;
    }
    
    return 0;
}

int uft_adf_write_block(uft_adf_volume_t *vol, uint32_t block,
                        const void *buffer) {
    if (!vol || !buffer) return -1;
    if (vol->readonly) return -1;
    if (block >= vol->total_blocks) return -1;
    
    size_t offset = (size_t)block * UFT_ADF_SECTOR_SIZE;
    
    if (vol->is_memory) {
        if (offset + UFT_ADF_SECTOR_SIZE > vol->mem_size) return -1;
        memcpy((uint8_t *)vol->mem_data + offset, buffer, UFT_ADF_SECTOR_SIZE);
        return 0;
    }
    
    if (fseek(vol->fp, (long)offset, SEEK_SET) != 0) return -1;
    if (fwrite(buffer, 1, UFT_ADF_SECTOR_SIZE, vol->fp) != UFT_ADF_SECTOR_SIZE) {
        return -1;
    }
    
    return 0;
}

/*============================================================================
 * Volume Open/Close
 *============================================================================*/

static int parse_volume(uft_adf_volume_t *vol) {
    /* Read boot block */
    if (uft_adf_read_block(vol, 0, vol->bootblock) != 0) return -1;
    if (uft_adf_read_block(vol, 1, vol->bootblock + UFT_ADF_SECTOR_SIZE) != 0) {
        return -1;
    }
    
    /* Check DOS signature */
    uint32_t dos_type = be32_to_cpu(*(uint32_t *)vol->bootblock);
    if ((dos_type & 0xFFFFFF00) != 0x444F5300) {
        /* Not "DOS\x" */
        return -1;
    }
    
    vol->fs_type = detect_fs_type(dos_type);
    
    /* Read root block */
    if (uft_adf_read_block(vol, vol->root_block, vol->rootblock_data) != 0) {
        return -1;
    }
    
    /* Verify root block checksum */
    if (!uft_adf_verify_checksum(vol->rootblock_data)) {
        return -1;
    }
    
    /* Parse root block for volume name */
    uint32_t *root = (uint32_t *)vol->rootblock_data;
    
    /* Check block type */
    uint32_t type = be32_to_cpu(root[0]);
    if (type != UFT_ADF_T_HEADER) return -1;
    
    /* Check secondary type (at offset 508) */
    int32_t sec_type = (int32_t)be32_to_cpu(root[127]);
    if (sec_type != UFT_ADF_ST_ROOT) return -1;
    
    /* Extract volume name (BCPL string at offset 432) */
    uint8_t *name_ptr = vol->rootblock_data + 432;
    uint8_t name_len = name_ptr[0];
    if (name_len > UFT_ADF_MAX_NAME) name_len = UFT_ADF_MAX_NAME;
    memcpy(vol->name, name_ptr + 1, name_len);
    vol->name[name_len] = '\0';
    
    return 0;
}

uft_adf_volume_t *uft_adf_open(const char *path, bool readonly) {
    if (!path) return NULL;
    
    FILE *fp = fopen(path, readonly ? "rb" : "r+b");
    if (!fp) {
        /* Try read-only fallback */
        fp = fopen(path, "rb");
        if (!fp) return NULL;
        readonly = true;
    }
    
    /* Get file size */
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    int density = uft_adf_detect_density((size_t)size);
    if (density < 0) {
        fclose(fp);
        return NULL;
    }
    
    uft_adf_volume_t *vol = calloc(1, sizeof(uft_adf_volume_t));
    if (!vol) {
        fclose(fp);
        return NULL;
    }
    
    vol->fp = fp;
    vol->readonly = readonly;
    vol->is_memory = false;
    vol->density = (uft_adf_density_t)density;
    
    if (vol->density == UFT_ADF_DENSITY_DD) {
        vol->total_blocks = UFT_ADF_DD_TOTAL_SECTORS;
        vol->root_block = UFT_ADF_DD_ROOT_BLOCK;
    } else {
        vol->total_blocks = UFT_ADF_HD_TOTAL_SECTORS;
        vol->root_block = UFT_ADF_HD_ROOT_BLOCK;
    }
    
    if (parse_volume(vol) != 0) {
        fclose(fp);
        free(vol);
        return NULL;
    }
    
    return vol;
}

uft_adf_volume_t *uft_adf_open_memory(const void *data, size_t size) {
    if (!data) return NULL;
    
    int density = uft_adf_detect_density(size);
    if (density < 0) return NULL;
    
    uft_adf_volume_t *vol = calloc(1, sizeof(uft_adf_volume_t));
    if (!vol) return NULL;
    
    vol->mem_data = (void *)data;
    vol->mem_size = size;
    vol->readonly = true;  /* Memory buffers are read-only */
    vol->is_memory = true;
    vol->density = (uft_adf_density_t)density;
    
    if (vol->density == UFT_ADF_DENSITY_DD) {
        vol->total_blocks = UFT_ADF_DD_TOTAL_SECTORS;
        vol->root_block = UFT_ADF_DD_ROOT_BLOCK;
    } else {
        vol->total_blocks = UFT_ADF_HD_TOTAL_SECTORS;
        vol->root_block = UFT_ADF_HD_ROOT_BLOCK;
    }
    
    if (parse_volume(vol) != 0) {
        free(vol);
        return NULL;
    }
    
    return vol;
}

void uft_adf_close(uft_adf_volume_t *vol) {
    if (!vol) return;
    
    if (vol->fp) {
        fclose(vol->fp);
    }
    if (vol->bitmap) {
        free(vol->bitmap);
    }
    free(vol);
}

/*============================================================================
 * Volume Information
 *============================================================================*/

int uft_adf_get_info(uft_adf_volume_t *vol, uft_adf_info_t *info) {
    if (!vol || !info) return -1;
    
    memset(info, 0, sizeof(*info));
    
    strncpy(info->name, vol->name, UFT_ADF_MAX_NAME);
    info->density = vol->density;
    info->fs_type = vol->fs_type;
    info->total_blocks = vol->total_blocks;
    
    /* Check bootable */
    uint32_t checksum = be32_to_cpu(*(uint32_t *)(vol->bootblock + 4));
    info->is_bootable = (checksum != 0);
    
    /* Check for boot code */
    info->has_bootcode = false;
    for (int i = 12; i < UFT_ADF_BOOTBLOCK_SIZE; i++) {
        if (vol->bootblock[i] != 0) {
            info->has_bootcode = true;
            break;
        }
    }
    
    /* Parse dates from root block */
    uint32_t *root = (uint32_t *)vol->rootblock_data;
    
    /* Creation date (at offset 484) */
    uint32_t c_days = be32_to_cpu(root[121]);
    uint32_t c_mins = be32_to_cpu(root[122]);
    uint32_t c_ticks = be32_to_cpu(root[123]);
    info->create_time = uft_adf_to_unix_time(c_days, c_mins, c_ticks);
    
    /* Volume modification date (at offset 472) */
    uint32_t v_days = be32_to_cpu(root[118]);
    uint32_t v_mins = be32_to_cpu(root[119]);
    uint32_t v_ticks = be32_to_cpu(root[120]);
    info->modify_time = uft_adf_to_unix_time(v_days, v_mins, v_ticks);
    
    /* Calculate free blocks from bitmap.
     * Bitmap is stored in root block + bitmap extension blocks.
     * Each bit represents one block: 1 = free, 0 = used. */
    uint32_t free_count = 0;
    
    /* Bitmap pages start at root block offset 79 (indices of bitmap blocks) */
    for (int bm = 0; bm < 25; bm++) {
        uint32_t bm_block = be32_to_cpu(root[79 + bm]);
        if (bm_block == 0) break;
        
        /* Read bitmap block */
        uint8_t bm_data[512];
        if (uft_adf_read_block(vol, bm_block, bm_data) == 0) {
            /* Skip first long (checksum), count free bits in remaining longs */
            const uint32_t *bm_words = (const uint32_t *)(bm_data + 4);
            for (int w = 0; w < 127; w++) {
                uint32_t word = be32_to_cpu(bm_words[w]);
                /* Count set bits (popcount) = free blocks */
                while (word) {
                    free_count += (word & 1);
                    word >>= 1;
                }
            }
        }
    }
    
    info->free_blocks = free_count;
    info->used_blocks = vol->total_blocks - free_count;
    
    return 0;
}

bool uft_adf_is_bootable(uft_adf_volume_t *vol) {
    if (!vol) return false;
    
    /* Calculate boot block checksum */
    const uint32_t *data = (const uint32_t *)vol->bootblock;
    uint32_t sum = 0;
    
    for (int i = 0; i < 256; i++) {
        uint32_t val = be32_to_cpu(data[i]);
        uint32_t old = sum;
        sum += val;
        if (sum < old) sum++;
    }
    
    return (sum == 0);
}

/*============================================================================
 * Directory Iteration
 *============================================================================*/

uft_adf_dir_iter_t *uft_adf_opendir(uft_adf_volume_t *vol) {
    return uft_adf_opendir_block(vol, 0);
}

uft_adf_dir_iter_t *uft_adf_opendir_block(uft_adf_volume_t *vol,
                                           uint32_t block) {
    if (!vol) return NULL;
    
    uft_adf_dir_iter_t *iter = calloc(1, sizeof(uft_adf_dir_iter_t));
    if (!iter) return NULL;
    
    iter->vol = vol;
    
    /* Use root block if block is 0 */
    if (block == 0) {
        block = vol->root_block;
    }
    
    iter->dir_block = block;
    
    /* Read directory header block */
    uint8_t header[UFT_ADF_SECTOR_SIZE];
    if (uft_adf_read_block(vol, block, header) != 0) {
        free(iter);
        return NULL;
    }
    
    /* Verify it's a header block */
    uint32_t type = be32_to_cpu(*(uint32_t *)header);
    if (type != UFT_ADF_T_HEADER) {
        free(iter);
        return NULL;
    }
    
    /* Copy hash table (at offset 24, 72 entries) */
    uint32_t *ht_src = (uint32_t *)(header + 24);
    for (int i = 0; i < UFT_ADF_HT_SIZE; i++) {
        iter->ht[i] = be32_to_cpu(ht_src[i]);
    }
    
    iter->ht_index = 0;
    iter->chain_block = 0;
    
    return iter;
}

int uft_adf_readdir(uft_adf_dir_iter_t *iter, uft_adf_entry_t *entry) {
    if (!iter || !entry) return -1;
    
    uint32_t block;
    
    while (1) {
        /* Continue hash chain if active */
        if (iter->chain_block != 0) {
            block = iter->chain_block;
        } else {
            /* Find next non-empty hash slot */
            while (iter->ht_index < UFT_ADF_HT_SIZE &&
                   iter->ht[iter->ht_index] == 0) {
                iter->ht_index++;
            }
            
            if (iter->ht_index >= UFT_ADF_HT_SIZE) {
                return 1;  /* No more entries */
            }
            
            block = iter->ht[iter->ht_index];
            iter->ht_index++;
        }
        
        /* Read header block */
        uint8_t header[UFT_ADF_SECTOR_SIZE];
        if (uft_adf_read_block(iter->vol, block, header) != 0) {
            return -1;
        }
        
        /* Parse entry */
        uint32_t *h = (uint32_t *)header;
        
        memset(entry, 0, sizeof(*entry));
        entry->block = block;
        
        /* Name (BCPL string at offset 432) */
        uint8_t name_len = header[432];
        if (name_len > UFT_ADF_MAX_NAME) name_len = UFT_ADF_MAX_NAME;
        memcpy(entry->name, header + 433, name_len);
        entry->name[name_len] = '\0';
        
        /* Secondary type (at offset 508) */
        int32_t sec_type = (int32_t)be32_to_cpu(h[127]);
        entry->is_dir = (sec_type == UFT_ADF_ST_DIR);
        entry->is_link = (sec_type == UFT_ADF_ST_SOFTLINK ||
                          sec_type == UFT_ADF_ST_HARDLINK);
        
        /* File size (at offset 324) - only valid for files */
        if (!entry->is_dir) {
            entry->size = be32_to_cpu(h[81]);
        }
        
        /* Protection (at offset 320) */
        entry->protect = be32_to_cpu(h[80]);
        
        /* Modification date (at offset 420) */
        uint32_t days = be32_to_cpu(h[105]);
        uint32_t mins = be32_to_cpu(h[106]);
        uint32_t ticks = be32_to_cpu(h[107]);
        entry->mtime = uft_adf_to_unix_time(days, mins, ticks);
        
        /* Comment (BCPL string at offset 328) */
        uint8_t comm_len = header[328];
        if (comm_len > UFT_ADF_MAX_COMMENT) comm_len = UFT_ADF_MAX_COMMENT;
        memcpy(entry->comment, header + 329, comm_len);
        entry->comment[comm_len] = '\0';
        
        /* Hash chain (at offset 496) */
        iter->chain_block = be32_to_cpu(h[124]);
        
        return 0;
    }
}

void uft_adf_closedir(uft_adf_dir_iter_t *iter) {
    free(iter);
}

/*============================================================================
 * Image Creation
 *============================================================================*/

int uft_adf_create(const char *path, uft_adf_density_t density,
                   uft_adf_fs_type_t fs_type, const char *name) {
    if (!path || !name) return -1;
    
    size_t size = (density == UFT_ADF_DENSITY_DD) ?
                   UFT_ADF_DD_SIZE : UFT_ADF_HD_SIZE;
    uint32_t total_blocks = (density == UFT_ADF_DENSITY_DD) ?
                             UFT_ADF_DD_TOTAL_SECTORS : UFT_ADF_HD_TOTAL_SECTORS;
    uint32_t root_block = (density == UFT_ADF_DENSITY_DD) ?
                           UFT_ADF_DD_ROOT_BLOCK : UFT_ADF_HD_ROOT_BLOCK;
    
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    
    /* Write zeros */
    uint8_t zero[UFT_ADF_SECTOR_SIZE] = {0};
    for (uint32_t i = 0; i < total_blocks; i++) {
        if (fwrite(zero, 1, UFT_ADF_SECTOR_SIZE, fp) != UFT_ADF_SECTOR_SIZE) {
            fclose(fp);
            return -1;
        }
    }
    
    /* Create boot block */
    uint8_t bootblock[UFT_ADF_BOOTBLOCK_SIZE] = {0};
    uint32_t dos_type;
    
    switch (fs_type) {
        case UFT_ADF_FS_OFS:      dos_type = UFT_ADF_DOS0; break;
        case UFT_ADF_FS_FFS:      dos_type = UFT_ADF_DOS1; break;
        case UFT_ADF_FS_OFS_INTL: dos_type = UFT_ADF_DOS2; break;
        case UFT_ADF_FS_FFS_INTL: dos_type = UFT_ADF_DOS3; break;
        case UFT_ADF_FS_OFS_DC:   dos_type = UFT_ADF_DOS4; break;
        case UFT_ADF_FS_FFS_DC:   dos_type = UFT_ADF_DOS5; break;
        default:                  dos_type = UFT_ADF_DOS1; break;
    }
    
    *(uint32_t *)bootblock = cpu_to_be32(dos_type);
    /* Checksum and root block remain 0 for non-bootable */
    
    /* Create root block */
    uint8_t rootblock[UFT_ADF_SECTOR_SIZE] = {0};
    uint32_t *root = (uint32_t *)rootblock;
    
    root[0] = cpu_to_be32(UFT_ADF_T_HEADER);    /* Type */
    root[3] = cpu_to_be32(UFT_ADF_HT_SIZE);     /* Hash table size */
    
    /* Bitmap flag (-1 = valid) */
    root[79] = cpu_to_be32(0xFFFFFFFF);
    
    /* Bitmap pages (first one after root) */
    root[80] = cpu_to_be32(root_block + 1);
    
    /* Volume name (BCPL string at offset 432) */
    size_t name_len = strlen(name);
    if (name_len > UFT_ADF_MAX_NAME) name_len = UFT_ADF_MAX_NAME;
    rootblock[432] = (uint8_t)name_len;
    memcpy(rootblock + 433, name, name_len);
    
    /* Current time for dates */
    time_t now = time(NULL);
    uint32_t days, mins, ticks;
    uft_unix_to_adf_time(now, &days, &mins, &ticks);
    
    /* Root alteration date (offset 420) */
    root[105] = cpu_to_be32(days);
    root[106] = cpu_to_be32(mins);
    root[107] = cpu_to_be32(ticks);
    
    /* Volume alteration date (offset 472) */
    root[118] = cpu_to_be32(days);
    root[119] = cpu_to_be32(mins);
    root[120] = cpu_to_be32(ticks);
    
    /* Creation date (offset 484) */
    root[121] = cpu_to_be32(days);
    root[122] = cpu_to_be32(mins);
    root[123] = cpu_to_be32(ticks);
    
    /* Secondary type */
    root[127] = cpu_to_be32(UFT_ADF_ST_ROOT);
    
    /* Calculate checksum (at offset 20) */
    root[5] = 0;  /* Clear checksum field */
    uint32_t checksum = uft_adf_checksum(rootblock);
    root[5] = cpu_to_be32(checksum);
    
    /* Write boot block */
    if (fseek(fp, 0, SEEK_SET) != 0) return -1;
    fwrite(bootblock, 1, UFT_ADF_BOOTBLOCK_SIZE, fp);
    
    /* Write root block */
    if (fseek(fp, (long)root_block * UFT_ADF_SECTOR_SIZE, SEEK_SET) != 0) return -1;
    fwrite(rootblock, 1, UFT_ADF_SECTOR_SIZE, fp);
    
    /* Create bitmap block (all blocks free except boot and root) */
    uint8_t bitmap[UFT_ADF_SECTOR_SIZE] = {0};
    uint32_t *bm = (uint32_t *)bitmap;
    
    /* Set all bits to 1 (free) */
    for (int i = 1; i < 128; i++) {
        bm[i] = 0xFFFFFFFF;
    }
    
    /* Mark boot blocks as used (blocks 0-1) */
    bm[1] &= cpu_to_be32(~0x00000003);
    
    /* Mark root block as used */
    uint32_t rb_word = root_block / 32 + 1;
    uint32_t rb_bit = root_block % 32;
    bm[rb_word] &= cpu_to_be32(~(1 << rb_bit));
    
    /* Mark bitmap block as used */
    uint32_t bb_word = (root_block + 1) / 32 + 1;
    uint32_t bb_bit = (root_block + 1) % 32;
    bm[bb_word] &= cpu_to_be32(~(1 << bb_bit));
    
    /* Calculate bitmap checksum */
    bm[0] = 0;
    checksum = uft_adf_checksum(bitmap);
    bm[0] = cpu_to_be32(checksum);
    
    /* Write bitmap */
    if (fseek(fp, (long)(root_block + 1) * UFT_ADF_SECTOR_SIZE, SEEK_SET) != 0) return -1;
    fwrite(bitmap, 1, UFT_ADF_SECTOR_SIZE, fp);
    if (ferror(fp)) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    
        
    fclose(fp);
return 0;
}

/*============================================================================
 * Self-Test
 *============================================================================*/

#ifdef UFT_ADF_TEST
int main(int argc, char **argv) {
    printf("Amiga ADF Module Test\n");
    printf("=====================\n\n");
    
    if (argc < 2) {
        printf("Usage: %s <adf_file>\n", argv[0]);
        printf("\nCreating test ADF...\n");
        
        if (uft_adf_create("test.adf", UFT_ADF_DENSITY_DD,
                           UFT_ADF_FS_FFS, "TestDisk") == 0) {
            printf("Created test.adf (880KB FFS)\n");
            argv[1] = "test.adf";
        } else {
            printf("Failed to create test ADF\n");
            return 1;
        }
    }
    
    uft_adf_volume_t *vol = uft_adf_open(argv[1], true);
    if (!vol) {
        printf("Failed to open: %s\n", argv[1]);
        return 1;
    }
    
    uft_adf_info_t info;
    if (uft_adf_get_info(vol, &info) == 0) {
        printf("Volume: %s\n", info.name);
        printf("Density: %s\n", info.density == UFT_ADF_DENSITY_DD ? "DD" : "HD");
        printf("FS Type: %s\n", uft_adf_fs_type_string(info.fs_type));
        printf("Blocks: %u\n", info.total_blocks);
        printf("Bootable: %s\n", info.is_bootable ? "Yes" : "No");
    }
    
    printf("\nDirectory listing:\n");
    uft_adf_dir_iter_t *iter = uft_adf_opendir(vol);
    if (iter) {
        uft_adf_entry_t entry;
        while (uft_adf_readdir(iter, &entry) == 0) {
            char prot[9];
            printf("  %s %8u %s %s\n",
                   entry.is_dir ? "DIR" : "   ",
                   entry.size,
                   uft_adf_protect_string(entry.protect, prot),
                   entry.name);
        }
        uft_adf_closedir(iter);
    }
    
    uft_adf_close(vol);
    return 0;
}
#endif
