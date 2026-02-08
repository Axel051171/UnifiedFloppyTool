/**
 * @file uft_atari_dos.c
 * @brief Atari 8-bit DOS Filesystem Implementation
 * @version 4.2.0
 */

#include "uft/uft_atari_dos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * DOS Type Names
 * ═══════════════════════════════════════════════════════════════════════════════ */

static const char *dos_names[] = {
    [UFT_ATARI_DOS_UNKNOWN]   = "Unknown",
    [UFT_ATARI_DOS_1_0]       = "Atari DOS 1.0",
    [UFT_ATARI_DOS_2_0]       = "Atari DOS 2.0",
    [UFT_ATARI_DOS_2_5]       = "Atari DOS 2.5",
    [UFT_ATARI_DOS_3_0]       = "Atari DOS 3.0",
    [UFT_ATARI_DOS_MYDOS]     = "MyDOS 4.x",
    [UFT_ATARI_DOS_SPARTA]    = "SpartaDOS 3.x",
    [UFT_ATARI_DOS_SPARTA_X]  = "SpartaDOS X",
    [UFT_ATARI_DOS_XE]        = "DOS XE",
    [UFT_ATARI_DOS_BIBO]      = "BiboDOS",
    [UFT_ATARI_DOS_TURBO]     = "TurboDOS",
    [UFT_ATARI_DOS_TOP]       = "TOP-DOS",
    [UFT_ATARI_DOS_LITEDOS]   = "LiteDOS",
};

const char *uft_atari_dos_name(uft_atari_dos_type_t type)
{
    if (type >= 0 && type <= UFT_ATARI_DOS_LITEDOS) {
        const char *name = dos_names[type];
        return name ? name : "Unknown";
    }
    return "Unknown";
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * DOS Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_atari_dos_type_t uft_atari_detect_dos(const uint8_t *data, size_t size)
{
    if (!data || size < 384) return UFT_ATARI_DOS_UNKNOWN;
    
    /* Check for SpartaDOS signature in boot sector */
    if (size >= 32 && memcmp(data + 6, "AZALON", 6) == 0) {
        return UFT_ATARI_DOS_SPARTA;
    }
    
    /* Read VTOC at sector 360 (offset 360 * 128 for SD) */
    size_t vtoc_offset = 360 * 128;
    if (size > vtoc_offset + 10) {
        uint8_t dos_code = data[vtoc_offset];
        
        /* DOS 2.0 uses code 0x02 */
        if (dos_code == 0x02) {
            /* Check for DOS 2.5 extended VTOC */
            if (size > 1024 * 128 + 128) {
                return UFT_ATARI_DOS_2_5;
            }
            return UFT_ATARI_DOS_2_0;
        }
        
        /* MyDOS uses modified VTOC */
        if (dos_code == 0x02 && size > 720 * 128) {
            /* Check for MyDOS extended directory structure */
            size_t dir_offset = 361 * 128;
            for (int i = 0; i < 64; i++) {
                uint8_t flags = data[dir_offset + i * 16];
                if (flags == 0x10) { /* Subdirectory flag */
                    return UFT_ATARI_DOS_MYDOS;
                }
            }
        }
    }
    
    /* Default to DOS 2.0 for standard sizes */
    if (size == 92160 || size == 133120 || size == 183936) {
        return UFT_ATARI_DOS_2_0;
    }
    
    return UFT_ATARI_DOS_UNKNOWN;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Disk Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_atari_disk_open(const char *filename, uft_atari_disk_t *disk)
{
    if (!filename || !disk) return -1;
    
    FILE *f = fopen(filename, "rb");
    if (!f) return -1;
    
    /* Get file size */
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    long size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    if (size <= 0) {
        fclose(f);
        return -1;
    }
    
    /* Allocate and read data */
    uint8_t *data = (uint8_t*)malloc(size);
    if (!data) {
        fclose(f);
        return -1;
    }
    
    if (fread(data, 1, size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return -1;
    }
    fclose(f);
    
    int result = uft_atari_disk_open_mem(data, size, disk);
    if (result < 0) {
        free(data);
        return result;
    }
    
    disk->filename = strdup(filename);
    return 0;
}

int uft_atari_disk_open_mem(const uint8_t *data, size_t size, uft_atari_disk_t *disk)
{
    if (!data || !disk || size < 128) return -1;
    
    memset(disk, 0, sizeof(*disk));
    
    /* Detect DOS type */
    disk->dos_type = uft_atari_detect_dos(data, size);
    
    /* Determine density from size */
    if (size <= 92160) {
        disk->density = UFT_ATARI_DENSITY_SD;
        disk->sector_size = 128;
        disk->total_sectors = 720;
    } else if (size <= 133120) {
        disk->density = UFT_ATARI_DENSITY_ED;
        disk->sector_size = 128;
        disk->total_sectors = 1040;
    } else if (size <= 183936) {
        disk->density = UFT_ATARI_DENSITY_DD;
        disk->sector_size = 256;
        disk->total_sectors = 720;
    } else {
        disk->density = UFT_ATARI_DENSITY_QD;
        disk->sector_size = 256;
        disk->total_sectors = size / 256;
    }
    
    /* Copy data */
    disk->data = (uint8_t*)malloc(size);
    if (!disk->data) return -1;
    memcpy(disk->data, data, size);
    disk->data_size = size;
    
    /* Parse VTOC */
    size_t vtoc_offset;
    if (disk->sector_size == 128) {
        vtoc_offset = (UFT_ATARI_VTOC_SECTOR - 1) * 128;
    } else {
        /* DD: first 3 sectors are 128 bytes (boot), rest are 256 */
        vtoc_offset = 3 * 128 + (UFT_ATARI_VTOC_SECTOR - 4) * 256;
    }
    
    if (vtoc_offset + sizeof(uft_atari_vtoc_t) <= size) {
        disk->vtoc = (uft_atari_vtoc_t*)(disk->data + vtoc_offset);
        disk->free_sectors = disk->vtoc->free_sectors;
    }
    
    /* Parse directory */
    disk->directory = (uft_atari_dirent_t*)calloc(UFT_ATARI_DIR_ENTRIES, 
                                                   sizeof(uft_atari_dirent_t));
    if (disk->directory) {
        size_t dir_offset;
        if (disk->sector_size == 128) {
            dir_offset = (UFT_ATARI_DIR_SECTOR - 1) * 128;
        } else {
            dir_offset = 3 * 128 + (UFT_ATARI_DIR_SECTOR - 4) * 256;
        }
        
        disk->dir_entry_count = 0;
        for (int i = 0; i < UFT_ATARI_DIR_ENTRIES && dir_offset + 16 <= size; i++) {
            memcpy(&disk->directory[i], disk->data + dir_offset, 16);
            if (disk->directory[i].flags & UFT_ATARI_FLAG_INUSE) {
                disk->dir_entry_count++;
            }
            dir_offset += 16;
        }
    }
    
    return 0;
}

int uft_atari_disk_create(uft_atari_disk_t *disk,
                          uft_atari_dos_type_t dos_type,
                          uft_atari_density_t density)
{
    if (!disk) return -1;
    
    memset(disk, 0, sizeof(*disk));
    disk->dos_type = dos_type;
    disk->density = density;
    
    /* Set geometry based on density */
    switch (density) {
        case UFT_ATARI_DENSITY_SD:
            disk->sector_size = 128;
            disk->total_sectors = 720;
            disk->data_size = 720 * 128;
            break;
        case UFT_ATARI_DENSITY_ED:
            disk->sector_size = 128;
            disk->total_sectors = 1040;
            disk->data_size = 1040 * 128;
            break;
        case UFT_ATARI_DENSITY_DD:
            disk->sector_size = 256;
            disk->total_sectors = 720;
            disk->data_size = 3 * 128 + 717 * 256;  /* Boot sectors are SD */
            break;
        default:
            return -1;
    }
    
    disk->data = (uint8_t*)calloc(1, disk->data_size);
    if (!disk->data) return -1;
    
    /* Initialize VTOC */
    size_t vtoc_offset = (UFT_ATARI_VTOC_SECTOR - 1) * disk->sector_size;
    disk->vtoc = (uft_atari_vtoc_t*)(disk->data + vtoc_offset);
    disk->vtoc->dos_code = 0x02;
    disk->vtoc->total_sectors = disk->total_sectors;
    disk->vtoc->free_sectors = disk->total_sectors - 
                               (UFT_ATARI_BOOT_SECTORS + 1 + UFT_ATARI_DIR_SECTORS);
    
    /* Mark system sectors as used in bitmap */
    memset(disk->vtoc->bitmap, 0xFF, sizeof(disk->vtoc->bitmap));
    for (int i = 0; i < UFT_ATARI_BOOT_SECTORS + 1 + UFT_ATARI_DIR_SECTORS; i++) {
        int byte = i / 8;
        int bit = i % 8;
        disk->vtoc->bitmap[byte] &= ~(1 << bit);
    }
    
    /* Allocate directory */
    disk->directory = (uft_atari_dirent_t*)calloc(UFT_ATARI_DIR_ENTRIES,
                                                   sizeof(uft_atari_dirent_t));
    disk->dir_entry_count = 0;
    disk->modified = true;
    
    return 0;
}

int uft_atari_disk_save(const uft_atari_disk_t *disk, const char *filename)
{
    if (!disk || !disk->data || !filename) return -1;
    
    FILE *f = fopen(filename, "wb");
    if (!f) return -1;
    
    size_t written = fwrite(disk->data, 1, disk->data_size, f);
    fclose(f);
    
    return (written == disk->data_size) ? 0 : -1;
}

void uft_atari_disk_close(uft_atari_disk_t *disk)
{
    if (!disk) return;
    
    free(disk->data);
    free(disk->directory);
    free(disk->filename);
    memset(disk, 0, sizeof(*disk));
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Sector Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_atari_read_sector(const uft_atari_disk_t *disk, 
                          uint16_t sector, uint8_t *buffer)
{
    if (!disk || !disk->data || !buffer) return -1;
    if (sector < 1 || sector > disk->total_sectors) return -1;
    
    size_t offset;
    size_t size;
    
    if (disk->density == UFT_ATARI_DENSITY_DD && sector > 3) {
        /* DD: first 3 sectors are 128 bytes */
        offset = 3 * 128 + (sector - 4) * 256;
        size = 256;
    } else {
        offset = (sector - 1) * disk->sector_size;
        size = disk->sector_size;
    }
    
    if (offset + size > disk->data_size) return -1;
    
    memcpy(buffer, disk->data + offset, size);
    return (int)size;
}

int uft_atari_write_sector(uft_atari_disk_t *disk,
                           uint16_t sector, const uint8_t *data)
{
    if (!disk || !disk->data || !data) return -1;
    if (sector < 1 || sector > disk->total_sectors) return -1;
    
    size_t offset;
    size_t size;
    
    if (disk->density == UFT_ATARI_DENSITY_DD && sector > 3) {
        offset = 3 * 128 + (sector - 4) * 256;
        size = 256;
    } else {
        offset = (sector - 1) * disk->sector_size;
        size = disk->sector_size;
    }
    
    if (offset + size > disk->data_size) return -1;
    
    memcpy(disk->data + offset, data, size);
    disk->modified = true;
    return (int)size;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Directory Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_atari_dir_count(const uft_atari_disk_t *disk)
{
    if (!disk || !disk->directory) return 0;
    
    int count = 0;
    for (int i = 0; i < UFT_ATARI_DIR_ENTRIES; i++) {
        if (disk->directory[i].flags & UFT_ATARI_FLAG_INUSE) {
            count++;
        }
    }
    return count;
}

int uft_atari_dir_get(const uft_atari_disk_t *disk, int index,
                      uft_atari_dirent_t *entry)
{
    if (!disk || !disk->directory || !entry) return -1;
    if (index < 0 || index >= UFT_ATARI_DIR_ENTRIES) return -1;
    
    *entry = disk->directory[index];
    return 0;
}

int uft_atari_dir_find(const uft_atari_disk_t *disk, const char *name)
{
    if (!disk || !disk->directory || !name) return -1;
    
    char name8[9] = {0};
    char ext3[4] = {0};
    uft_atari_filename_to_native(name, name8, ext3);
    
    for (int i = 0; i < UFT_ATARI_DIR_ENTRIES; i++) {
        if (!(disk->directory[i].flags & UFT_ATARI_FLAG_INUSE)) continue;
        if (disk->directory[i].flags & UFT_ATARI_FLAG_DELETED) continue;
        
        if (memcmp(disk->directory[i].filename, name8, 8) == 0 &&
            memcmp(disk->directory[i].extension, ext3, 3) == 0) {
            return i;
        }
    }
    return -1;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Filename Conversion
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_atari_filename_to_native(const char *input, char *name8, char *ext3)
{
    if (!input || !name8 || !ext3) return;
    
    memset(name8, ' ', 8);
    memset(ext3, ' ', 3);
    
    int i = 0;
    int j = 0;
    
    /* Copy filename */
    while (input[i] && input[i] != '.' && j < 8) {
        name8[j++] = toupper((unsigned char)input[i++]);
    }
    
    /* Skip to extension */
    if (input[i] == '.') i++;
    
    /* Copy extension */
    j = 0;
    while (input[i] && j < 3) {
        ext3[j++] = toupper((unsigned char)input[i++]);
    }
}

void uft_atari_filename_from_native(const char *name8, const char *ext3,
                                    char *output, size_t output_size)
{
    if (!name8 || !ext3 || !output || output_size < 13) return;
    
    int i = 0;
    
    /* Copy filename, trimming trailing spaces */
    int len = 8;
    while (len > 0 && name8[len-1] == ' ') len--;
    for (int j = 0; j < len && i < (int)output_size - 1; j++) {
        output[i++] = name8[j];
    }
    
    /* Add dot and extension if present */
    len = 3;
    while (len > 0 && ext3[len-1] == ' ') len--;
    if (len > 0) {
        output[i++] = '.';
        for (int j = 0; j < len && i < (int)output_size - 1; j++) {
            output[i++] = ext3[j];
        }
    }
    
    output[i] = '\0';
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * File Size Calculation
 * ═══════════════════════════════════════════════════════════════════════════════ */

uint32_t uft_atari_file_size(const uft_atari_disk_t *disk, int dir_index)
{
    if (!disk || !disk->data || dir_index < 0) return 0;
    if (dir_index >= UFT_ATARI_DIR_ENTRIES) return 0;
    
    const uft_atari_dirent_t *entry = &disk->directory[dir_index];
    if (!(entry->flags & UFT_ATARI_FLAG_INUSE)) return 0;
    
    /* Follow sector chain and count bytes */
    uint32_t total = 0;
    uint16_t sector = entry->start_sector;
    int max_iterations = disk->total_sectors;
    
    while (sector > 0 && sector <= disk->total_sectors && max_iterations-- > 0) {
        uint8_t sector_data[256];
        if (uft_atari_read_sector(disk, sector, sector_data) < 0) break;
        
        if (disk->sector_size == 128) {
            /* SD: last 3 bytes are link info */
            uint8_t bytes_used = sector_data[127];
            if (bytes_used == 0) bytes_used = 125;
            total += bytes_used;
            
            /* Get next sector */
            uint16_t next = ((sector_data[125] & 0x03) << 8) | sector_data[126];
            if (next == 0 || next == sector) break;
            sector = next;
        } else {
            /* DD: different link format */
            uint8_t bytes_used = sector_data[255];
            if (bytes_used == 0) bytes_used = 253;
            total += bytes_used;
            
            uint16_t next = ((sector_data[254] & 0x03) << 8) | sector_data[253];
            if (next == 0 || next == sector) break;
            sector = next;
        }
    }
    
    return total;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Disk Info
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_atari_disk_info(const uft_atari_disk_t *disk, char *buffer, size_t size)
{
    if (!disk || !buffer || size < 100) return;
    
    snprintf(buffer, size,
        "DOS: %s\n"
        "Density: %s\n"
        "Sectors: %u × %u bytes\n"
        "Free: %u sectors\n"
        "Files: %d\n",
        uft_atari_dos_name(disk->dos_type),
        disk->density == UFT_ATARI_DENSITY_SD ? "Single" :
        disk->density == UFT_ATARI_DENSITY_ED ? "Enhanced" :
        disk->density == UFT_ATARI_DENSITY_DD ? "Double" : "Quad",
        disk->total_sectors, disk->sector_size,
        disk->free_sectors,
        uft_atari_dir_count(disk));
}
