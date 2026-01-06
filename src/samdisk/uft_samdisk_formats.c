/**
 * @file uft_samdisk_formats.c
 * @brief SAMdisk System-Specific Format Support
 * 
 * EXT4-015 Part 2: System-specific disk formats
 * 
 * Features:
 * - SAM Coupé disk format
 * - Spectrum +3 format
 * - Amstrad CPC/PCW formats
 * - MSX disk format
 * - Enterprise format
 */

#include "uft/samdisk/uft_samdisk_formats.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "uft/uft_compiler.h"

/*===========================================================================
 * SAM Coupé Format
 *===========================================================================*/

/* SAM Coupé disk geometry */
#define SAM_TRACKS          80
#define SAM_SIDES           2
#define SAM_SECTORS         10
#define SAM_SECTOR_SIZE     512

/* SAM DOS directory structure */
#define SAM_DIR_TRACK       4
#define SAM_DIR_ENTRIES     80
#define SAM_DIR_ENTRY_SIZE  256

typedef struct {
    uint8_t     status;         /* 00: file status */
    uint8_t     filename[10];   /* 01-0A: filename */
    uint8_t     sectors_lo;     /* 0B: sector count low */
    uint8_t     sectors_hi;     /* 0C: sector count high */
    uint8_t     start_track;    /* 0D: starting track */
    uint8_t     start_sector;   /* 0E: starting sector */
    uint8_t     type_info[11];  /* 0F-19: type specific info */
    uint8_t     mgts_flags;     /* 1A: MGTS flags */
    uint8_t     reserved[5];    /* 1B-1F: reserved */
    uint8_t     extra[224];     /* 20-FF: extra space in 256-byte entry */
} sam_dir_entry_t;
UFT_PACK_END

int uft_sam_read_directory(const uint8_t *image, size_t size,
                           uft_sam_file_t *files, size_t max_files,
                           size_t *count)
{
    if (!image || !files || !count) return -1;
    
    *count = 0;
    
    /* Calculate directory location */
    size_t track_size = SAM_SECTORS * SAM_SECTOR_SIZE * SAM_SIDES;
    size_t dir_offset = SAM_DIR_TRACK * track_size;
    
    if (dir_offset + SAM_DIR_ENTRIES * 32 > size) return -1;
    
    for (int i = 0; i < SAM_DIR_ENTRIES && *count < max_files; i++) {
        size_t entry_offset = dir_offset + i * 32;  /* 32-byte compact entry */
        const uint8_t *entry = image + entry_offset;
        
        /* Check if entry is used */
        if (entry[0] == 0 || entry[0] == 0xE5) continue;
        
        uft_sam_file_t *f = &files[*count];
        memset(f, 0, sizeof(*f));
        
        /* Copy filename (10 chars) */
        memcpy(f->name, entry + 1, 10);
        f->name[10] = '\0';
        
        /* Trim trailing spaces */
        for (int j = 9; j >= 0 && f->name[j] == ' '; j--) {
            f->name[j] = '\0';
        }
        
        f->sectors = entry[11] | (entry[12] << 8);
        f->start_track = entry[13];
        f->start_sector = entry[14];
        f->type = entry[15];
        f->size = f->sectors * SAM_SECTOR_SIZE;
        
        (*count)++;
    }
    
    return 0;
}

int uft_sam_read_file(const uint8_t *image, size_t size,
                      const uft_sam_file_t *file,
                      uint8_t *buffer, size_t *buf_size)
{
    if (!image || !file || !buffer || !buf_size) return -1;
    
    size_t needed = file->sectors * SAM_SECTOR_SIZE;
    if (*buf_size < needed) {
        *buf_size = needed;
        return -1;
    }
    
    int track = file->start_track;
    int sector = file->start_sector;
    size_t offset = 0;
    
    for (int s = 0; s < file->sectors; s++) {
        /* Calculate sector offset */
        int side = (sector > 10) ? 1 : 0;
        int sec = (sector > 10) ? sector - 10 : sector;
        
        size_t sec_offset = track * SAM_SECTORS * SAM_SECTOR_SIZE * SAM_SIDES;
        sec_offset += side * SAM_SECTORS * SAM_SECTOR_SIZE;
        sec_offset += (sec - 1) * SAM_SECTOR_SIZE;
        
        if (sec_offset + SAM_SECTOR_SIZE > size) break;
        
        memcpy(buffer + offset, image + sec_offset, SAM_SECTOR_SIZE);
        offset += SAM_SECTOR_SIZE;
        
        /* Next sector (simple sequential) */
        sector++;
        if (sector > 20) {
            sector = 1;
            track++;
        }
    }
    
    *buf_size = offset;
    return 0;
}

/*===========================================================================
 * Spectrum +3 Format
 *===========================================================================*/

/* +3DOS geometry */
#define P3_TRACKS           40
#define P3_SIDES            1
#define P3_SECTORS          9
#define P3_SECTOR_SIZE      512
#define P3_BLOCK_SIZE       1024
#define P3_DIR_BLOCKS       2

/* +3DOS header */
typedef struct {
    uint8_t     signature[8];   /* "PLUS3DOS" */
    uint8_t     soft_eof;       /* 0x1A */
    uint8_t     issue;          /* Issue number */
    uint8_t     version;        /* Version number */
    uint8_t     length[4];      /* File length (LE) */
    uint8_t     basic_length[2]; /* BASIC length */
    uint8_t     basic_line[2];  /* BASIC start line */
    uint8_t     basic_offset[2]; /* BASIC prog offset */
    uint8_t     unused[104];    /* Unused */
    uint8_t     checksum;       /* Header checksum */
} p3dos_header_t;
UFT_PACK_END

int uft_p3_validate_header(const uint8_t *data, size_t size)
{
    if (!data || size < sizeof(p3dos_header_t)) return -1;
    
    const p3dos_header_t *hdr = (const p3dos_header_t *)data;
    
    if (memcmp(hdr->signature, "PLUS3DOS", 8) != 0) return -1;
    if (hdr->soft_eof != 0x1A) return -1;
    
    /* Verify checksum */
    uint8_t sum = 0;
    for (int i = 0; i < 127; i++) {
        sum += data[i];
    }
    
    if (sum != hdr->checksum) return -1;
    
    return 0;
}

int uft_p3_read_directory(const uint8_t *image, size_t size,
                          uft_p3_file_t *files, size_t max_files,
                          size_t *count)
{
    if (!image || !files || !count) return -1;
    
    *count = 0;
    
    /* +3DOS uses CP/M-like directory at start of disk */
    size_t dir_offset = P3_BLOCK_SIZE;  /* Skip system track */
    
    for (int i = 0; i < 64 && *count < max_files; i++) {
        const uint8_t *entry = image + dir_offset + i * 32;
        
        /* Check user number (0-15 = valid, E5 = deleted) */
        if (entry[0] > 15) continue;
        
        uft_p3_file_t *f = &files[*count];
        memset(f, 0, sizeof(*f));
        
        f->user = entry[0];
        
        /* Filename (8 + 3) */
        memcpy(f->name, entry + 1, 8);
        f->name[8] = '.';
        memcpy(f->name + 9, entry + 9, 3);
        f->name[12] = '\0';
        
        /* Strip high bits and trailing spaces */
        for (int j = 0; j < 12; j++) {
            f->name[j] &= 0x7F;
            if (f->name[j] == ' ') f->name[j] = '\0';
        }
        
        f->extent = entry[12];
        f->records = entry[15];
        f->size = f->records * 128;
        
        /* Get allocation blocks */
        for (int b = 0; b < 16; b++) {
            f->blocks[b] = entry[16 + b];
        }
        
        (*count)++;
    }
    
    return 0;
}

/*===========================================================================
 * Amstrad CPC Format
 *===========================================================================*/

/* CPC disk formats */
#define CPC_DATA_TRACKS     40
#define CPC_DATA_SECTORS    9
#define CPC_DATA_SIZE       512

#define CPC_SYS_TRACKS      40
#define CPC_SYS_SECTORS     9
#define CPC_SYS_SIZE        512

/* AMSDOS header */
typedef struct {
    uint8_t     user;           /* User number */
    uint8_t     filename[8];    /* Filename */
    uint8_t     extension[3];   /* Extension */
    uint8_t     extent_lo;      /* Extent low */
    uint8_t     reserved[2];    /* Reserved */
    uint8_t     extent_hi;      /* Extent high */
    uint8_t     records;        /* Record count */
    uint8_t     blocks[16];     /* Block pointers */
} amsdos_dir_t;
UFT_PACK_END

/* AMSDOS file header (in file) */
typedef struct {
    uint8_t     user;           /* User number (always 0) */
    uint8_t     filename[11];   /* 8.3 filename */
    uint8_t     unused[6];
    uint8_t     type;           /* File type */
    uint8_t     unused2[2];
    uint8_t     load_addr[2];   /* Load address */
    uint8_t     unused3;
    uint8_t     length[2];      /* Logical length */
    uint8_t     exec_addr[2];   /* Execution address */
    uint8_t     unused4[36];
    uint8_t     real_length[3]; /* Real file length (24-bit) */
    uint8_t     checksum[2];    /* Header checksum */
    uint8_t     unused5[59];
} amsdos_file_header_t;
UFT_PACK_END

int uft_cpc_read_directory(const uint8_t *image, size_t size,
                           uft_cpc_file_t *files, size_t max_files,
                           size_t *count)
{
    if (!image || !files || !count) return -1;
    
    *count = 0;
    
    /* Directory is in first 2 tracks on side 0 */
    size_t dir_offset = 0;  /* Track 0, sector 0 */
    
    for (int i = 0; i < 64 && *count < max_files; i++) {
        const amsdos_dir_t *entry = (const amsdos_dir_t *)(image + dir_offset + i * 32);
        
        if (entry->user > 15) continue;  /* 0xE5 = deleted */
        
        uft_cpc_file_t *f = &files[*count];
        memset(f, 0, sizeof(*f));
        
        f->user = entry->user;
        
        /* Copy filename */
        memcpy(f->name, entry->filename, 8);
        f->name[8] = '.';
        memcpy(f->name + 9, entry->extension, 3);
        f->name[12] = '\0';
        
        /* Clean up name */
        for (int j = 0; j < 12; j++) {
            if (f->name[j] < 32 || f->name[j] > 126) {
                f->name[j] = '\0';
            }
        }
        
        f->extent = entry->extent_lo | (entry->extent_hi << 8);
        f->records = entry->records;
        f->size = f->records * 128;
        
        (*count)++;
    }
    
    return 0;
}

/*===========================================================================
 * MSX Disk Format
 *===========================================================================*/

/* MSX disk geometry */
#define MSX_TRACKS          80
#define MSX_SIDES           2
#define MSX_SECTORS         9
#define MSX_SECTOR_SIZE     512

/* MSX-DOS boot sector */
typedef struct {
    uint8_t     jump[3];        /* Jump instruction */
    uint8_t     oem[8];         /* OEM name */
    uint8_t     bytes_sec[2];   /* Bytes per sector */
    uint8_t     sec_cluster;    /* Sectors per cluster */
    uint8_t     reserved[2];    /* Reserved sectors */
    uint8_t     fats;           /* Number of FATs */
    uint8_t     root_entries[2]; /* Root directory entries */
    uint8_t     total_sec[2];   /* Total sectors */
    uint8_t     media;          /* Media descriptor */
    uint8_t     fat_sectors[2]; /* Sectors per FAT */
    uint8_t     sec_track[2];   /* Sectors per track */
    uint8_t     heads[2];       /* Number of heads */
    uint8_t     hidden[4];      /* Hidden sectors */
} msx_boot_t;
UFT_PACK_END

int uft_msx_parse_boot(const uint8_t *image, size_t size, uft_msx_info_t *info)
{
    if (!image || !info || size < 512) return -1;
    
    memset(info, 0, sizeof(*info));
    
    const msx_boot_t *boot = (const msx_boot_t *)image;
    
    info->bytes_per_sector = boot->bytes_sec[0] | (boot->bytes_sec[1] << 8);
    info->sectors_per_cluster = boot->sec_cluster;
    info->reserved_sectors = boot->reserved[0] | (boot->reserved[1] << 8);
    info->num_fats = boot->fats;
    info->root_entries = boot->root_entries[0] | (boot->root_entries[1] << 8);
    info->total_sectors = boot->total_sec[0] | (boot->total_sec[1] << 8);
    info->media_type = boot->media;
    info->fat_sectors = boot->fat_sectors[0] | (boot->fat_sectors[1] << 8);
    
    /* Determine format */
    switch (info->media_type) {
        case 0xF8: info->format_name = "Single-sided 80 track"; break;
        case 0xF9: info->format_name = "Double-sided 80 track"; break;
        case 0xFA: info->format_name = "Single-sided 80 track"; break;
        case 0xFB: info->format_name = "Double-sided 80 track"; break;
        case 0xFC: info->format_name = "Single-sided 40 track"; break;
        case 0xFD: info->format_name = "Double-sided 40 track"; break;
        case 0xFE: info->format_name = "Single-sided 40 track 8 sector"; break;
        case 0xFF: info->format_name = "Double-sided 40 track 8 sector"; break;
        default:   info->format_name = "Unknown"; break;
    }
    
    return 0;
}

/*===========================================================================
 * Enterprise Format
 *===========================================================================*/

/* Enterprise 64/128 disk geometry */
#define EP_TRACKS           80
#define EP_SIDES            2
#define EP_SECTORS          9
#define EP_SECTOR_SIZE      512

/* EXDOS directory entry */
typedef struct {
    uint8_t     status;         /* Entry status */
    uint8_t     filename[8];    /* Filename */
    uint8_t     extension[3];   /* Extension */
    uint8_t     attributes;     /* File attributes */
    uint8_t     unused[2];
    uint8_t     extent;         /* Extent number */
    uint8_t     records;        /* Records in extent */
    uint8_t     blocks[16];     /* Block allocation */
} exdos_dir_t;
UFT_PACK_END

int uft_ep_read_directory(const uint8_t *image, size_t size,
                          uft_ep_file_t *files, size_t max_files,
                          size_t *count)
{
    if (!image || !files || !count) return -1;
    
    *count = 0;
    
    /* EXDOS directory location depends on format */
    size_t dir_offset = EP_SECTOR_SIZE;  /* After boot sector */
    
    for (int i = 0; i < 128 && *count < max_files; i++) {
        const exdos_dir_t *entry = (const exdos_dir_t *)(image + dir_offset + i * 32);
        
        if (entry->status == 0 || entry->status == 0xE5) continue;
        
        uft_ep_file_t *f = &files[*count];
        memset(f, 0, sizeof(*f));
        
        /* Copy filename */
        memcpy(f->name, entry->filename, 8);
        f->name[8] = '.';
        memcpy(f->name + 9, entry->extension, 3);
        f->name[12] = '\0';
        
        f->attributes = entry->attributes;
        f->extent = entry->extent;
        f->records = entry->records;
        f->size = f->records * 128;
        
        (*count)++;
    }
    
    return 0;
}

/*===========================================================================
 * Generic Format Detection
 *===========================================================================*/

uft_samdisk_system_t uft_samdisk_detect_system(const uint8_t *image, size_t size)
{
    if (!image || size < 512) return UFT_SYSTEM_UNKNOWN;
    
    /* Check for MSX boot sector */
    const msx_boot_t *boot = (const msx_boot_t *)image;
    if (boot->bytes_sec[0] == 0x00 && boot->bytes_sec[1] == 0x02) {
        if (boot->media >= 0xF8 && boot->media <= 0xFF) {
            return UFT_SYSTEM_MSX;
        }
    }
    
    /* Check for +3DOS */
    if (size >= 512 + sizeof(p3dos_header_t)) {
        if (memcmp(image + 512, "PLUS3DOS", 8) == 0) {
            return UFT_SYSTEM_SPECTRUM_P3;
        }
    }
    
    /* Check for SAM Coupé (by disk size) */
    size_t sam_size = SAM_TRACKS * SAM_SIDES * SAM_SECTORS * SAM_SECTOR_SIZE;
    if (size == sam_size) {
        return UFT_SYSTEM_SAM_COUPE;
    }
    
    /* Check for Amstrad CPC (DATA format marker) */
    if (size >= 512) {
        /* CPC system disks have specific boot code */
        if (image[0] == 0x00 || image[0] == 0x01) {
            /* Could be CPC DATA or SYSTEM format */
            return UFT_SYSTEM_CPC;
        }
    }
    
    return UFT_SYSTEM_UNKNOWN;
}

const char *uft_samdisk_system_name(uft_samdisk_system_t system)
{
    switch (system) {
        case UFT_SYSTEM_SAM_COUPE:    return "SAM Coupé";
        case UFT_SYSTEM_SPECTRUM_P3:  return "Spectrum +3";
        case UFT_SYSTEM_CPC:          return "Amstrad CPC";
        case UFT_SYSTEM_PCW:          return "Amstrad PCW";
        case UFT_SYSTEM_MSX:          return "MSX";
        case UFT_SYSTEM_ENTERPRISE:   return "Enterprise";
        default:                      return "Unknown";
    }
}

/*===========================================================================
 * Report
 *===========================================================================*/

int uft_samdisk_system_report(const uint8_t *image, size_t size,
                              char *buffer, size_t buf_size)
{
    if (!image || !buffer) return -1;
    
    uft_samdisk_system_t system = uft_samdisk_detect_system(image, size);
    
    int written = snprintf(buffer, buf_size,
        "{\n"
        "  \"system\": \"%s\",\n"
        "  \"image_size\": %zu,\n",
        uft_samdisk_system_name(system),
        size
    );
    
    if (system == UFT_SYSTEM_MSX) {
        uft_msx_info_t info;
        if (uft_msx_parse_boot(image, size, &info) == 0) {
            written += snprintf(buffer + written, buf_size - written,
                "  \"msx_format\": \"%s\",\n"
                "  \"bytes_per_sector\": %d,\n"
                "  \"sectors_per_cluster\": %d,\n"
                "  \"total_sectors\": %d,\n",
                info.format_name,
                info.bytes_per_sector,
                info.sectors_per_cluster,
                info.total_sectors
            );
        }
    }
    
    written += snprintf(buffer + written, buf_size - written, "}\n");
    
    return (written > 0 && (size_t)written < buf_size) ? 0 : -1;
}
