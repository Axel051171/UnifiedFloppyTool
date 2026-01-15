/**
 * @file uft_fat_atari.c
 * @brief Atari ST FAT Filesystem Implementation
 */

#include "uft/fs/uft_fat_atari.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

/*===========================================================================
 * Standard Atari Geometries
 *===========================================================================*/

const uft_atari_geometry_t uft_atari_std_formats[] = {
    {
        .name = "Atari ST SS/DD 9 sectors",
        .type = UFT_ATARI_FMT_SS_DD_9,
        .sectors = 720,
        .spt = 9,
        .sides = 1,
        .tracks = 80,
        .dir_entries = 112,
        .fat_sectors = 5,
        .spc = 2,
        .media = 0xF8,
        .is_standard = true
    },
    {
        .name = "Atari ST DS/DD 9 sectors",
        .type = UFT_ATARI_FMT_DS_DD_9,
        .sectors = 1440,
        .spt = 9,
        .sides = 2,
        .tracks = 80,
        .dir_entries = 112,
        .fat_sectors = 5,
        .spc = 2,
        .media = 0xF9,
        .is_standard = true
    },
    {
        .name = "Atari ST DS/DD 10 sectors",
        .type = UFT_ATARI_FMT_DS_DD_10,
        .sectors = 1600,
        .spt = 10,
        .sides = 2,
        .tracks = 80,
        .dir_entries = 112,
        .fat_sectors = 5,
        .spc = 2,
        .media = 0xF9,
        .is_standard = false
    },
    {
        .name = "Atari ST DS/DD 11 sectors (Twister)",
        .type = UFT_ATARI_FMT_DS_DD_11,
        .sectors = 1760,
        .spt = 11,
        .sides = 2,
        .tracks = 80,
        .dir_entries = 112,
        .fat_sectors = 5,
        .spc = 2,
        .media = 0xF9,
        .is_standard = false
    },
    {
        .name = "Atari ST DS/HD 18 sectors",
        .type = UFT_ATARI_FMT_DS_HD_18,
        .sectors = 2880,
        .spt = 18,
        .sides = 2,
        .tracks = 80,
        .dir_entries = 224,
        .fat_sectors = 9,
        .spc = 2,
        .media = 0xF0,
        .is_standard = true
    },
    {
        .name = "Atari ST DS/ED 36 sectors",
        .type = UFT_ATARI_FMT_DS_ED_36,
        .sectors = 5760,
        .spt = 36,
        .sides = 2,
        .tracks = 80,
        .dir_entries = 240,
        .fat_sectors = 12,
        .spc = 2,
        .media = 0xF0,
        .is_standard = false
    },
    /* Sentinel */
    { NULL, UFT_ATARI_FMT_UNKNOWN, 0, 0, 0, 0, 0, 0, 0, 0, false }
};

const size_t uft_atari_std_format_count = 
    sizeof(uft_atari_std_formats) / sizeof(uft_atari_std_formats[0]) - 1;

/*===========================================================================
 * Serial Number
 *===========================================================================*/

uint32_t uft_atari_generate_serial(void)
{
    return uft_atari_serial_from_time((uint32_t)time(NULL));
}

uint32_t uft_atari_serial_from_time(uint32_t timestamp)
{
    /*
     * Atari serial number format (24-bit):
     * Based on random and time components
     * High bit indicates Atari format
     */
    uint32_t serial = timestamp ^ (timestamp >> 16);
    serial = (serial & 0xFFFFFF) | UFT_ATARI_SERIAL_FLAG;
    
    /* Add some randomness from lower bits */
    serial ^= (timestamp << 8) & 0xFF0000;
    
    return serial & 0xFFFFFF;  /* 24-bit only */
}

bool uft_atari_is_atari_serial(uint32_t serial)
{
    /* Check for Atari serial pattern */
    /* Atari serials often have specific patterns based on time */
    return (serial != 0) && (serial <= 0xFFFFFF);
}

void uft_atari_set_serial(uft_atari_bootsect_t *boot, uint32_t serial)
{
    if (!boot) return;
    
    /* 24-bit serial stored in 3 bytes, big-endian */
    boot->serial[0] = (serial >> 16) & 0xFF;
    boot->serial[1] = (serial >> 8) & 0xFF;
    boot->serial[2] = serial & 0xFF;
}

uint32_t uft_atari_get_serial(const uft_atari_bootsect_t *boot)
{
    if (!boot) return 0;
    
    return ((uint32_t)boot->serial[0] << 16) |
           ((uint32_t)boot->serial[1] << 8) |
           (uint32_t)boot->serial[2];
}

/*===========================================================================
 * Boot Sector Checksum
 *===========================================================================*/

uint16_t uft_atari_calc_checksum(const uft_atari_bootsect_t *boot)
{
    if (!boot) return 0;
    
    const uint16_t *words = (const uint16_t *)boot;
    uint32_t sum = 0;
    
    /* Sum all 256 words (512 bytes) except the checksum itself */
    for (int i = 0; i < 255; i++) {
        /* Big-endian on Atari */
        uint16_t word = (words[i] >> 8) | (words[i] << 8);
        sum += word;
    }
    
    return (uint16_t)(sum & 0xFFFF);
}

void uft_atari_make_bootable(uft_atari_bootsect_t *boot)
{
    if (!boot) return;
    
    /* Calculate current sum without checksum field */
    uint16_t current_sum = uft_atari_calc_checksum(boot);
    
    /* Checksum value needed to make total = 0x1234 */
    uint16_t checksum = (UFT_ATARI_BOOT_CHECKSUM - current_sum) & 0xFFFF;
    
    /* Store in big-endian */
    boot->checksum = (checksum >> 8) | (checksum << 8);
}

void uft_atari_make_non_bootable(uft_atari_bootsect_t *boot)
{
    if (!boot) return;
    
    /* Set checksum to invalid value */
    boot->checksum = 0;
}

bool uft_atari_is_bootable(const uft_atari_bootsect_t *boot)
{
    if (!boot) return false;
    
    const uint16_t *words = (const uint16_t *)boot;
    uint32_t sum = 0;
    
    /* Sum all 256 words including checksum */
    for (int i = 0; i < 256; i++) {
        uint16_t word = (words[i] >> 8) | (words[i] << 8);
        sum += word;
    }
    
    return (sum & 0xFFFF) == UFT_ATARI_BOOT_CHECKSUM;
}

/*===========================================================================
 * Format Detection
 *===========================================================================*/

bool uft_atari_detect(const uint8_t *data, size_t size)
{
    if (!data || size < 512) return false;
    
    const uft_atari_bootsect_t *boot = (const uft_atari_bootsect_t *)data;
    
    /* Check for Atari-specific characteristics */
    
    /* 1. Branch instruction at start (BRA.S = 0x60xx) */
    uint16_t bra = (boot->bra >> 8) | (boot->bra << 8);  /* Swap endian */
    if ((bra & 0xFF00) != 0x6000) {
        /* Could also be 0xE9 (JMP) for PC-compatible */
        if (data[0] != 0xEB && data[0] != 0xE9) {
            return false;
        }
    }
    
    /* 2. Check typical Atari geometry */
    if (boot->sectors_per_cluster != 2) {
        /* Atari strongly prefers 2 sectors per cluster */
        return false;
    }
    
    /* 3. Check sector size */
    if (boot->bytes_per_sector != 512) {
        /* Standard Atari uses 512 bytes/sector for floppies */
        return false;
    }
    
    /* 4. Check for standard Atari disk sizes */
    uint16_t total = boot->total_sectors;
    bool valid_size = (total == 720) ||   /* SS/DD 9 */
                      (total == 1440) ||  /* DS/DD 9 */
                      (total == 1600) ||  /* DS/DD 10 */
                      (total == 1760) ||  /* DS/DD 11 */
                      (total == 2880);    /* DS/HD 18 */
    
    if (!valid_size && size <= 2 * 1024 * 1024) {
        return false;  /* Unknown floppy size */
    }
    
    /* 5. Media descriptor should be valid */
    if (boot->media_type != 0xF8 && boot->media_type != 0xF9 && 
        boot->media_type != 0xF0) {
        return false;
    }
    
    return true;
}

uft_atari_format_t uft_atari_identify_format(const uint8_t *data, size_t size)
{
    if (!data || size < 512) return UFT_ATARI_FMT_UNKNOWN;
    
    const uft_atari_bootsect_t *boot = (const uft_atari_bootsect_t *)data;
    
    uint16_t total = boot->total_sectors;
    uint8_t spt = boot->sectors_per_track;
    uint8_t sides = boot->num_heads;
    
    /* Match against known formats */
    for (size_t i = 0; i < uft_atari_std_format_count; i++) {
        const uft_atari_geometry_t *g = &uft_atari_std_formats[i];
        
        if (g->sectors == total && g->spt == spt && g->sides == sides) {
            return g->type;
        }
    }
    
    /* Check size-based detection */
    const uft_atari_geometry_t *geom = uft_atari_geometry_from_size(size);
    if (geom) {
        return geom->type;
    }
    
    return UFT_ATARI_FMT_CUSTOM;
}

const uft_atari_geometry_t *uft_atari_get_geometry(uft_atari_format_t format)
{
    for (size_t i = 0; i < uft_atari_std_format_count; i++) {
        if (uft_atari_std_formats[i].type == format) {
            return &uft_atari_std_formats[i];
        }
    }
    return NULL;
}

const uft_atari_geometry_t *uft_atari_geometry_from_size(size_t size)
{
    /* Match by total bytes */
    for (size_t i = 0; i < uft_atari_std_format_count; i++) {
        const uft_atari_geometry_t *g = &uft_atari_std_formats[i];
        size_t expected = (size_t)g->sectors * 512;
        
        if (size == expected) {
            return g;
        }
    }
    return NULL;
}

/*===========================================================================
 * Formatting
 *===========================================================================*/

int uft_atari_format(uint8_t *data, size_t size, uft_atari_format_t format,
                     const char *label)
{
    const uft_atari_geometry_t *geom = uft_atari_get_geometry(format);
    if (!geom) return -1;
    
    return uft_atari_format_custom(data, size, geom, label);
}

int uft_atari_format_custom(uint8_t *data, size_t size,
                            const uft_atari_geometry_t *geom,
                            const char *label)
{
    if (!data || !geom) return -1;
    
    size_t required = (size_t)geom->sectors * 512;
    if (size < required) return -2;
    
    /* Clear disk */
    memset(data, 0, required);
    
    uft_atari_bootsect_t *boot = (uft_atari_bootsect_t *)data;
    
    /* Branch instruction (BRA.S to boot code) */
    boot->bra = 0x601C;  /* BRA.S $1E (big-endian: 0x601C) */
    
    /* OEM name */
    memcpy(boot->oem, "Loader", 6);
    
    /* Serial number */
    uft_atari_set_serial(boot, uft_atari_generate_serial());
    
    /* BPB */
    boot->bytes_per_sector = 512;
    boot->sectors_per_cluster = geom->spc;
    boot->reserved_sectors = 1;
    boot->num_fats = 2;
    boot->root_entries = geom->dir_entries;
    boot->total_sectors = geom->sectors;
    boot->media_type = geom->media;
    boot->fat_sectors = geom->fat_sectors;
    boot->sectors_per_track = geom->spt;
    boot->num_heads = geom->sides;
    boot->hidden_sectors = 0;
    
    /* No checksum initially (non-bootable) */
    boot->checksum = 0;
    
    /* Initialize FATs */
    uint32_t fat1_offset = boot->reserved_sectors * 512;
    uint32_t fat2_offset = fat1_offset + boot->fat_sectors * 512;
    
    /* FAT[0] = media type, FAT[1] = 0xFFF (EOF) */
    data[fat1_offset] = boot->media_type;
    data[fat1_offset + 1] = 0xFF;
    data[fat1_offset + 2] = 0xFF;
    
    /* Copy to second FAT */
    memcpy(data + fat2_offset, data + fat1_offset, boot->fat_sectors * 512);
    
    /* Volume label in root directory */
    if (label && label[0]) {
        uint32_t root_offset = fat2_offset + boot->fat_sectors * 512;
        
        /* Volume label entry */
        memset(data + root_offset, ' ', 11);
        size_t len = strlen(label);
        if (len > 11) len = 11;
        memcpy(data + root_offset, label, len);
        data[root_offset + 11] = 0x08;  /* Volume label attribute */
    }
    
    return 0;
}

uint16_t uft_atari_calc_sector_size(uint64_t total_size)
{
    /* GEMDOS limit: 16-bit sector count (65535 sectors max) */
    uint64_t sectors_512 = total_size / 512;
    
    if (sectors_512 <= 65535) return 512;
    
    /* Need larger logical sectors */
    if (total_size / 1024 <= 65535) return 1024;
    if (total_size / 2048 <= 65535) return 2048;
    if (total_size / 4096 <= 65535) return 4096;
    return 8192;  /* Maximum supported */
}

int uft_atari_convert_from_pc(uint8_t *data, size_t size)
{
    if (!data || size < 512) return -1;
    
    uft_atari_bootsect_t *atari_boot = (uft_atari_bootsect_t *)data;
    
    /* Set Atari serial number */
    uft_atari_set_serial(atari_boot, uft_atari_generate_serial());
    
    /* Atari prefers 2 sectors per cluster */
    /* Note: This could break existing data! Only use on fresh format */
    if (atari_boot->sectors_per_cluster != 2) {
        /* Warning: cannot safely change SPC on existing disk */
        return -2;
    }
    
    /* Clear PC-specific boot code */
    memset(atari_boot->boot_code, 0, sizeof(atari_boot->boot_code));
    
    /* Set OEM to Atari-style */
    memcpy(atari_boot->oem, "Loader", 6);
    
    /* Ensure non-bootable */
    uft_atari_make_non_bootable(atari_boot);
    
    return 0;
}

/*===========================================================================
 * AHDI Partitions
 *===========================================================================*/

bool uft_ahdi_detect(const uint8_t *data, size_t size)
{
    if (!data || size < 512) return false;
    
    const uft_ahdi_root_t *root = (const uft_ahdi_root_t *)data;
    
    /* Check for valid partition entries */
    int valid_count = 0;
    
    for (int i = 0; i < 4; i++) {
        const uft_ahdi_part_t *part = &root->parts[i];
        
        if (part->flag == 0x00) continue;  /* Empty slot */
        if (part->flag != 0x01 && part->flag != 0x81) {
            return false;  /* Invalid flag */
        }
        
        /* Check partition type */
        if (memcmp(part->id, UFT_AHDI_TYPE_GEM, 3) == 0 ||
            memcmp(part->id, UFT_AHDI_TYPE_BGM, 3) == 0 ||
            memcmp(part->id, UFT_AHDI_TYPE_XGM, 3) == 0 ||
            memcmp(part->id, UFT_AHDI_TYPE_RAW, 3) == 0) {
            valid_count++;
        }
    }
    
    return valid_count > 0;
}

const uft_ahdi_root_t *uft_ahdi_get_root(const uint8_t *data)
{
    if (!data) return NULL;
    return (const uft_ahdi_root_t *)data;
}

int uft_ahdi_count_partitions(const uft_ahdi_root_t *root)
{
    if (!root) return 0;
    
    int count = 0;
    for (int i = 0; i < 4; i++) {
        if (root->parts[i].flag == 0x01 || root->parts[i].flag == 0x81) {
            count++;
        }
    }
    return count;
}

int uft_ahdi_get_partition(const uft_ahdi_root_t *root, int index,
                           uint32_t *start, uint32_t *size, char *type)
{
    if (!root || index < 0 || index >= 4) return -1;
    
    const uft_ahdi_part_t *part = &root->parts[index];
    
    if (part->flag != 0x01 && part->flag != 0x81) {
        return -2;  /* Empty or invalid */
    }
    
    /* Values are big-endian on Atari */
    if (start) {
        *start = ((part->start >> 24) & 0xFF) |
                 ((part->start >> 8) & 0xFF00) |
                 ((part->start << 8) & 0xFF0000) |
                 ((part->start << 24) & 0xFF000000);
    }
    if (size) {
        *size = ((part->size >> 24) & 0xFF) |
                ((part->size >> 8) & 0xFF00) |
                ((part->size << 8) & 0xFF0000) |
                ((part->size << 24) & 0xFF000000);
    }
    if (type) {
        memcpy(type, part->id, 3);
        type[3] = '\0';
    }
    
    return 0;
}
