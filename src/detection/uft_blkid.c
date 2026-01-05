/**
 * @file uft_blkid.c
 * @brief Filesystem and Partition Detection Implementation
 * @version 4.2.0
 */

#include "uft/uft_blkid.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Magic Database
 * ═══════════════════════════════════════════════════════════════════════════════ */

static const uft_blkid_magic_t magic_db[] = {
    /* ext2/3/4 */
    {
        .type = UFT_BLKID_EXT2,
        .name = "ext2",
        .description = "Linux ext2 filesystem",
        .offset = 0x438,
        .magic = (const uint8_t[]){0x53, 0xEF},
        .magic_len = 2,
        .priority = 80,
        .min_size = 2048
    },
    
    /* NTFS */
    {
        .type = UFT_BLKID_NTFS,
        .name = "ntfs",
        .description = "Windows NTFS filesystem",
        .offset = 3,
        .magic = (const uint8_t[]){'N','T','F','S',' ',' ',' ',' '},
        .magic_len = 8,
        .priority = 85,
        .min_size = 512
    },
    
    /* exFAT */
    {
        .type = UFT_BLKID_EXFAT,
        .name = "exfat",
        .description = "Microsoft exFAT filesystem",
        .offset = 3,
        .magic = (const uint8_t[]){'E','X','F','A','T',' ',' ',' '},
        .magic_len = 8,
        .priority = 85,
        .min_size = 512
    },
    
    /* XFS */
    {
        .type = UFT_BLKID_XFS,
        .name = "xfs",
        .description = "SGI XFS filesystem",
        .offset = 0,
        .magic = (const uint8_t[]){'X','F','S','B'},
        .magic_len = 4,
        .priority = 80,
        .min_size = 512
    },
    
    /* Btrfs */
    {
        .type = UFT_BLKID_BTRFS,
        .name = "btrfs",
        .description = "Btrfs filesystem",
        .offset = 0x10040,
        .magic = (const uint8_t[]){'_','B','H','R','f','S','_','M'},
        .magic_len = 8,
        .priority = 80,
        .min_size = 0x10048
    },
    
    /* HFS+ */
    {
        .type = UFT_BLKID_HFS_PLUS,
        .name = "hfsplus",
        .description = "Apple HFS+ filesystem",
        .offset = 0x400,
        .magic = (const uint8_t[]){'H','+'},
        .magic_len = 2,
        .priority = 80,
        .min_size = 0x402
    },
    
    /* ISO9660 */
    {
        .type = UFT_BLKID_ISO9660,
        .name = "iso9660",
        .description = "ISO 9660 CD-ROM filesystem",
        .offset = 0x8001,
        .magic = (const uint8_t[]){'C','D','0','0','1'},
        .magic_len = 5,
        .priority = 85,
        .min_size = 0x8006
    },
    
    /* UDF */
    {
        .type = UFT_BLKID_UDF,
        .name = "udf",
        .description = "Universal Disk Format",
        .offset = 0x8001,
        .magic = (const uint8_t[]){'B','E','A','0','1'},
        .magic_len = 5,
        .priority = 85,
        .min_size = 0x8006
    },
    
    /* Amiga OFS */
    {
        .type = UFT_BLKID_AMIGA_OFS,
        .name = "amiga_ofs",
        .description = "Amiga Old File System",
        .offset = 0,
        .magic = (const uint8_t[]){'D','O','S', 0x00},
        .magic_len = 4,
        .priority = 75,
        .min_size = 512
    },
    
    /* Amiga FFS */
    {
        .type = UFT_BLKID_AMIGA_FFS,
        .name = "amiga_ffs",
        .description = "Amiga Fast File System",
        .offset = 0,
        .magic = (const uint8_t[]){'D','O','S', 0x01},
        .magic_len = 4,
        .priority = 75,
        .min_size = 512
    },
    
    /* MBR partition table */
    {
        .type = UFT_BLKID_PART_MBR,
        .name = "mbr",
        .description = "DOS/MBR partition table",
        .offset = 0x1FE,
        .magic = (const uint8_t[]){0x55, 0xAA},
        .magic_len = 2,
        .priority = 50,
        .min_size = 512
    },
    
    /* GPT */
    {
        .type = UFT_BLKID_PART_GPT,
        .name = "gpt",
        .description = "GUID Partition Table",
        .offset = 0x200,
        .magic = (const uint8_t[]){'E','F','I',' ','P','A','R','T'},
        .magic_len = 8,
        .priority = 90,
        .min_size = 0x208
    },
    
    /* ATR */
    {
        .type = UFT_BLKID_IMG_ATR,
        .name = "atr",
        .description = "Atari disk image",
        .offset = 0,
        .magic = (const uint8_t[]){0x96, 0x02},
        .magic_len = 2,
        .priority = 70,
        .min_size = 16
    },
    
    /* HFE */
    {
        .type = UFT_BLKID_IMG_HFE,
        .name = "hfe",
        .description = "HxC Floppy Emulator image",
        .offset = 0,
        .magic = (const uint8_t[]){'H','X','C','P','I','C','F','E'},
        .magic_len = 8,
        .priority = 90,
        .min_size = 512
    },
    
    /* SCP */
    {
        .type = UFT_BLKID_IMG_SCP,
        .name = "scp",
        .description = "SuperCard Pro flux image",
        .offset = 0,
        .magic = (const uint8_t[]){'S','C','P'},
        .magic_len = 3,
        .priority = 90,
        .min_size = 16
    },
    
    /* Linux swap */
    {
        .type = UFT_BLKID_SWAP_LINUX,
        .name = "swap",
        .description = "Linux swap space",
        .offset = 4086,
        .magic = (const uint8_t[]){'S','W','A','P','S','P','A','C','E','2'},
        .magic_len = 10,
        .priority = 80,
        .min_size = 4096
    },
    
    /* LUKS */
    {
        .type = UFT_BLKID_LUKS,
        .name = "luks",
        .description = "LUKS encrypted volume",
        .offset = 0,
        .magic = (const uint8_t[]){'L','U','K','S', 0xBA, 0xBE},
        .magic_len = 6,
        .priority = 95,
        .min_size = 512
    },
    
    /* Terminator */
    {
        .type = UFT_BLKID_UNKNOWN,
        .name = NULL
    }
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Type Names
 * ═══════════════════════════════════════════════════════════════════════════════ */

static const struct {
    uft_blkid_type_t type;
    const char *name;
    const char *description;
} type_names[] = {
    {UFT_BLKID_UNKNOWN, "unknown", "Unknown format"},
    {UFT_BLKID_EXT2, "ext2", "Linux ext2"},
    {UFT_BLKID_EXT3, "ext3", "Linux ext3"},
    {UFT_BLKID_EXT4, "ext4", "Linux ext4"},
    {UFT_BLKID_FAT12, "fat12", "FAT12"},
    {UFT_BLKID_FAT16, "fat16", "FAT16"},
    {UFT_BLKID_FAT32, "fat32", "FAT32"},
    {UFT_BLKID_EXFAT, "exfat", "exFAT"},
    {UFT_BLKID_NTFS, "ntfs", "NTFS"},
    {UFT_BLKID_XFS, "xfs", "XFS"},
    {UFT_BLKID_BTRFS, "btrfs", "Btrfs"},
    {UFT_BLKID_ZFS, "zfs", "ZFS"},
    {UFT_BLKID_HFS, "hfs", "HFS"},
    {UFT_BLKID_HFS_PLUS, "hfsplus", "HFS+"},
    {UFT_BLKID_APFS, "apfs", "APFS"},
    {UFT_BLKID_ISO9660, "iso9660", "ISO9660"},
    {UFT_BLKID_UDF, "udf", "UDF"},
    {UFT_BLKID_AMIGA_OFS, "amiga_ofs", "Amiga OFS"},
    {UFT_BLKID_AMIGA_FFS, "amiga_ffs", "Amiga FFS"},
    {UFT_BLKID_CPM, "cpm", "CP/M"},
    {UFT_BLKID_PART_MBR, "mbr", "MBR"},
    {UFT_BLKID_PART_GPT, "gpt", "GPT"},
    {UFT_BLKID_PART_APM, "apm", "Apple Partition Map"},
    {UFT_BLKID_IMG_ADF, "adf", "Amiga Disk File"},
    {UFT_BLKID_IMG_D64, "d64", "Commodore D64"},
    {UFT_BLKID_IMG_ATR, "atr", "Atari ATR"},
    {UFT_BLKID_IMG_HFE, "hfe", "HxC HFE"},
    {UFT_BLKID_IMG_SCP, "scp", "SuperCard Pro"},
    {UFT_BLKID_LUKS, "luks", "LUKS"},
    {UFT_BLKID_SWAP_LINUX, "swap", "Linux Swap"},
    {0, NULL, NULL}
};

const char *uft_blkid_type_name(uft_blkid_type_t type)
{
    for (int i = 0; type_names[i].name; i++) {
        if (type_names[i].type == type) {
            return type_names[i].name;
        }
    }
    return "unknown";
}

const char *uft_blkid_type_description(uft_blkid_type_t type)
{
    for (int i = 0; type_names[i].name; i++) {
        if (type_names[i].type == type) {
            return type_names[i].description;
        }
    }
    return "Unknown format";
}

uft_blkid_type_t uft_blkid_type_by_name(const char *name)
{
    if (!name) return UFT_BLKID_UNKNOWN;
    
    for (int i = 0; type_names[i].name; i++) {
        if (strcasecmp(type_names[i].name, name) == 0) {
            return type_names[i].type;
        }
    }
    return UFT_BLKID_UNKNOWN;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Probe Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

struct uft_blkid_probe {
    const uint8_t *data;
    size_t size;
    uint64_t offset;
    uint64_t device_size;
    uint32_t flags;
    
    /* Results */
    uft_blkid_result_t results[16];
    int result_count;
    
    /* Partitions */
    uft_blkid_partition_t partitions[128];
    int partition_count;
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Probe API
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_blkid_probe_t *uft_blkid_new_probe(void)
{
    uft_blkid_probe_t *probe = (uft_blkid_probe_t*)calloc(1, sizeof(uft_blkid_probe_t));
    if (probe) {
        probe->flags = UFT_BLKID_FLAG_ALL;
    }
    return probe;
}

uft_blkid_probe_t *uft_blkid_new_probe_from_filename(const char *filename)
{
    if (!filename) return NULL;
    
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    long size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    if (size <= 0 || size > 100 * 1024 * 1024) {  /* Limit to 100MB for probe */
        fclose(f);
        return NULL;
    }
    
    uint8_t *data = (uint8_t*)malloc(size);
    if (!data) {
        fclose(f);
        return NULL;
    }
    
    if (fread(data, 1, size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    
    uft_blkid_probe_t *probe = uft_blkid_new_probe();
    if (!probe) {
        free(data);
        return NULL;
    }
    
    probe->data = data;
    probe->size = size;
    probe->device_size = size;
    
    return probe;
}

int uft_blkid_set_data(uft_blkid_probe_t *probe,
                       const uint8_t *data,
                       size_t size)
{
    if (!probe || !data) return -1;
    
    probe->data = data;
    probe->size = size;
    if (probe->device_size == 0) {
        probe->device_size = size;
    }
    
    return 0;
}

void uft_blkid_set_flags(uft_blkid_probe_t *probe, uint32_t flags)
{
    if (probe) probe->flags = flags;
}

void uft_blkid_free_probe(uft_blkid_probe_t *probe)
{
    if (!probe) return;
    /* Note: we don't own the data pointer unless we loaded from file */
    free(probe);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool uft_blkid_check_magic(const uint8_t *data, size_t size,
                           const uft_blkid_magic_t *magic)
{
    if (!data || !magic || !magic->magic) return false;
    if (size < magic->offset + magic->magic_len) return false;
    if (size < (size_t)magic->min_size) return false;
    
    const uint8_t *ptr = data + magic->offset;
    
    if (magic->mask) {
        for (size_t i = 0; i < magic->magic_len; i++) {
            if ((ptr[i] & magic->mask[i]) != (magic->magic[i] & magic->mask[i])) {
                return false;
            }
        }
    } else {
        if (memcmp(ptr, magic->magic, magic->magic_len) != 0) {
            return false;
        }
    }
    
    /* Check secondary magic if present */
    if (magic->magic2 && magic->magic2_len > 0) {
        if (size < magic->magic2_offset + magic->magic2_len) return false;
        if (memcmp(data + magic->magic2_offset, magic->magic2, magic->magic2_len) != 0) {
            return false;
        }
    }
    
    return true;
}

static int detect_fat_type(const uint8_t *data, size_t size)
{
    if (size < 512) return UFT_BLKID_UNKNOWN;
    
    /* Check boot signature */
    if (data[510] != 0x55 || data[511] != 0xAA) return UFT_BLKID_UNKNOWN;
    
    /* Check for FAT signature */
    if (memcmp(data + 54, "FAT12", 5) == 0) return UFT_BLKID_FAT12;
    if (memcmp(data + 54, "FAT16", 5) == 0) return UFT_BLKID_FAT16;
    if (memcmp(data + 82, "FAT32", 5) == 0) return UFT_BLKID_FAT32;
    
    /* Calculate from BPB */
    uint16_t bytes_per_sector = data[11] | (data[12] << 8);
    uint8_t sectors_per_cluster = data[13];
    uint16_t reserved_sectors = data[14] | (data[15] << 8);
    uint8_t fat_count = data[16];
    uint16_t root_entries = data[17] | (data[18] << 8);
    uint16_t total_sectors_16 = data[19] | (data[20] << 8);
    uint16_t sectors_per_fat_16 = data[22] | (data[23] << 8);
    uint32_t total_sectors_32 = data[32] | (data[33] << 8) | 
                                 (data[34] << 16) | (data[35] << 24);
    
    if (bytes_per_sector == 0 || sectors_per_cluster == 0) {
        return UFT_BLKID_UNKNOWN;
    }
    
    uint32_t total_sectors = total_sectors_16 ? total_sectors_16 : total_sectors_32;
    uint32_t sectors_per_fat = sectors_per_fat_16;
    if (sectors_per_fat == 0) {
        sectors_per_fat = data[36] | (data[37] << 8) | 
                          (data[38] << 16) | (data[39] << 24);
    }
    
    uint32_t root_sectors = ((root_entries * 32) + bytes_per_sector - 1) / bytes_per_sector;
    uint32_t data_sectors = total_sectors - reserved_sectors - 
                            (fat_count * sectors_per_fat) - root_sectors;
    uint32_t clusters = data_sectors / sectors_per_cluster;
    
    if (clusters < 4085) return UFT_BLKID_FAT12;
    if (clusters < 65525) return UFT_BLKID_FAT16;
    return UFT_BLKID_FAT32;
}

int uft_blkid_do_probe(uft_blkid_probe_t *probe)
{
    if (!probe || !probe->data) return -1;
    
    probe->result_count = 0;
    
    /* Check all magics */
    for (int i = 0; magic_db[i].name != NULL; i++) {
        const uft_blkid_magic_t *magic = &magic_db[i];
        
        /* Check flags */
        if ((magic->type >= UFT_BLKID_EXT2 && magic->type < UFT_BLKID_PART_MBR) &&
            !(probe->flags & UFT_BLKID_FLAG_FILESYSTEMS)) continue;
        if ((magic->type >= UFT_BLKID_PART_MBR && magic->type < UFT_BLKID_IMG_ADF) &&
            !(probe->flags & UFT_BLKID_FLAG_PARTITIONS)) continue;
        if ((magic->type >= UFT_BLKID_IMG_ADF && magic->type < UFT_BLKID_LVM2) &&
            !(probe->flags & UFT_BLKID_FLAG_IMAGES)) continue;
        
        if (uft_blkid_check_magic(probe->data, probe->size, magic)) {
            if (probe->result_count < 16) {
                uft_blkid_result_t *r = &probe->results[probe->result_count];
                memset(r, 0, sizeof(*r));
                r->type = magic->type;
                r->name = magic->name;
                r->confidence = magic->priority;
                probe->result_count++;
            }
        }
    }
    
    /* Special handling for FAT */
    if (probe->flags & UFT_BLKID_FLAG_FILESYSTEMS) {
        int fat_type = detect_fat_type(probe->data, probe->size);
        if (fat_type != UFT_BLKID_UNKNOWN) {
            if (probe->result_count < 16) {
                uft_blkid_result_t *r = &probe->results[probe->result_count];
                memset(r, 0, sizeof(*r));
                r->type = fat_type;
                r->name = uft_blkid_type_name(fat_type);
                r->confidence = 80;
                probe->result_count++;
            }
        }
    }
    
    /* Special handling for D64 (size-based) */
    if (probe->flags & UFT_BLKID_FLAG_IMAGES) {
        if (probe->size == 174848 || probe->size == 175531 ||
            probe->size == 196608 || probe->size == 197376) {
            if (probe->result_count < 16) {
                uft_blkid_result_t *r = &probe->results[probe->result_count];
                memset(r, 0, sizeof(*r));
                r->type = UFT_BLKID_IMG_D64;
                r->name = "d64";
                r->confidence = 75;
                probe->result_count++;
            }
        }
        
        /* ADF (size-based) */
        if (probe->size == 901120 || probe->size == 1802240) {
            if (probe->result_count < 16) {
                uft_blkid_result_t *r = &probe->results[probe->result_count];
                memset(r, 0, sizeof(*r));
                r->type = UFT_BLKID_IMG_ADF;
                r->name = "adf";
                r->confidence = 70;
                probe->result_count++;
            }
        }
    }
    
    return probe->result_count > 0 ? 0 : -1;
}

int uft_blkid_get_result(const uft_blkid_probe_t *probe,
                         uft_blkid_result_t *result)
{
    if (!probe || !result || probe->result_count == 0) return -1;
    
    /* Return highest confidence result */
    int best = 0;
    for (int i = 1; i < probe->result_count; i++) {
        if (probe->results[i].confidence > probe->results[best].confidence) {
            best = i;
        }
    }
    
    *result = probe->results[best];
    return 0;
}

int uft_blkid_get_results(const uft_blkid_probe_t *probe,
                          uft_blkid_result_t *results,
                          int max_results)
{
    if (!probe || !results) return 0;
    
    int count = probe->result_count;
    if (count > max_results) count = max_results;
    
    memcpy(results, probe->results, count * sizeof(uft_blkid_result_t));
    return count;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Simple API
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_blkid_detect(const uint8_t *data, size_t size,
                     uft_blkid_result_t *result)
{
    if (!data || !result) return -1;
    
    uft_blkid_probe_t *probe = uft_blkid_new_probe();
    if (!probe) return -1;
    
    uft_blkid_set_data(probe, data, size);
    
    int ret = uft_blkid_do_probe(probe);
    if (ret == 0) {
        uft_blkid_get_result(probe, result);
    }
    
    uft_blkid_free_probe(probe);
    return ret;
}

int uft_blkid_detect_file(const char *filename, uft_blkid_result_t *result)
{
    if (!filename || !result) return -1;
    
    uft_blkid_probe_t *probe = uft_blkid_new_probe_from_filename(filename);
    if (!probe) return -1;
    
    int ret = uft_blkid_do_probe(probe);
    if (ret == 0) {
        uft_blkid_get_result(probe, result);
    }
    
    uft_blkid_free_probe(probe);
    return ret;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Magic Database Access
 * ═══════════════════════════════════════════════════════════════════════════════ */

const uft_blkid_magic_t *uft_blkid_get_magics(void)
{
    return magic_db;
}

int uft_blkid_get_magic_count(void)
{
    int count = 0;
    while (magic_db[count].name != NULL) count++;
    return count;
}

int uft_blkid_score(const uint8_t *data, size_t size, uft_blkid_type_t type)
{
    if (!data) return 0;
    
    for (int i = 0; magic_db[i].name != NULL; i++) {
        if (magic_db[i].type == type) {
            if (uft_blkid_check_magic(data, size, &magic_db[i])) {
                return magic_db[i].priority;
            }
        }
    }
    
    return 0;
}
