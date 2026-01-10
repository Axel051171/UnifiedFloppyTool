/**
 * @file uft_cbm_disk.c
 * @brief Unified CBM Disk Format Handler Implementation
 * 
 * Supports D64/D71/D81 formats with directory reading, file extraction,
 * and tool/protection analysis.
 * 
 * @author UFT Team
 * @version 1.0.0
 */

#include "uft/c64/uft_cbm_disk.h"
#include "uft/c64/uft_fastloader_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ============================================================================
 * Track Layout Tables
 * ============================================================================ */

/* D64/D71 sectors per track (1541/1571) */
static const uint8_t D64_SECTORS[41] = {
    0,
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  /* 1-17 */
    19, 19, 19, 19, 19, 19, 19,                                          /* 18-24 */
    18, 18, 18, 18, 18, 18,                                              /* 25-30 */
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17                               /* 31-40 */
};

/* D64 cumulative track offsets */
static const uint16_t D64_TRACK_OFFSET[41] = {
    0,
    0, 21, 42, 63, 84, 105, 126, 147, 168, 189, 210, 231, 252, 273, 294, 315, 336,
    357, 376, 395, 414, 433, 452, 471,
    490, 508, 526, 544, 562, 580,
    598, 615, 632, 649, 666, 683, 700, 717, 734, 751
};

/* ============================================================================
 * PETSCII Conversion
 * ============================================================================ */

void uft_cbm_petscii_to_ascii(const uint8_t *petscii, size_t len,
                               char *ascii, size_t ascii_cap)
{
    if (!petscii || !ascii || ascii_cap == 0) return;
    
    size_t j = 0;
    for (size_t i = 0; i < len && j < ascii_cap - 1; i++) {
        uint8_t c = petscii[i];
        
        if (c == 0xA0 || c == 0x00) break;  /* End of name */
        
        /* PETSCII to ASCII conversion */
        if (c >= 0x41 && c <= 0x5A) {
            /* Uppercase -> lowercase */
            ascii[j++] = (char)(c + 0x20);
        } else if (c >= 0xC1 && c <= 0xDA) {
            /* Shifted uppercase -> uppercase */
            ascii[j++] = (char)(c - 0x80);
        } else if (c >= 0x20 && c <= 0x7E) {
            ascii[j++] = (char)c;
        } else {
            ascii[j++] = '.';
        }
    }
    ascii[j] = '\0';
}

/* ============================================================================
 * Format Detection
 * ============================================================================ */

uft_cbm_disk_format_t uft_cbm_detect_format(size_t file_size)
{
    switch (file_size) {
        case UFT_D64_SIZE_STD:
        case UFT_D64_SIZE_ERR:
            return UFT_CBM_DISK_D64;
            
        case UFT_D64_SIZE_EXT:
        case UFT_D64_SIZE_EXT_ERR:
            return UFT_CBM_DISK_D64_40;
            
        case UFT_D71_SIZE:
        case UFT_D71_SIZE_ERR:
            return UFT_CBM_DISK_D71;
            
        case UFT_D81_SIZE:
            return UFT_CBM_DISK_D81;
            
        default:
            /* Try to detect by other means */
            if (file_size >= UFT_D64_SIZE_STD && file_size < UFT_D64_SIZE_EXT) {
                return UFT_CBM_DISK_D64;
            }
            return UFT_CBM_DISK_UNKNOWN;
    }
}

const char *uft_cbm_format_name(uft_cbm_disk_format_t format)
{
    switch (format) {
        case UFT_CBM_DISK_D64:    return "D64 (1541)";
        case UFT_CBM_DISK_D64_40: return "D64 Extended (40 tracks)";
        case UFT_CBM_DISK_D71:    return "D71 (1571)";
        case UFT_CBM_DISK_D71_80: return "D71 Extended";
        case UFT_CBM_DISK_D81:    return "D81 (1581)";
        case UFT_CBM_DISK_G64:    return "G64 (GCR stream)";
        case UFT_CBM_DISK_G71:    return "G71 (GCR stream)";
        case UFT_CBM_DISK_D80:    return "D80 (8050)";
        case UFT_CBM_DISK_D82:    return "D82 (8250)";
        default:                  return "Unknown";
    }
}

const char *uft_cbm_file_type_name(uft_cbm_file_type_t type)
{
    switch (type) {
        case UFT_CBM_FILE_DEL: return "DEL";
        case UFT_CBM_FILE_SEQ: return "SEQ";
        case UFT_CBM_FILE_PRG: return "PRG";
        case UFT_CBM_FILE_USR: return "USR";
        case UFT_CBM_FILE_REL: return "REL";
        case UFT_CBM_FILE_CBM: return "CBM";
        default:               return "???";
    }
}

/* ============================================================================
 * Sector Access
 * ============================================================================ */

uint8_t uft_cbm_sectors_per_track(uft_cbm_disk_format_t format, uint8_t track)
{
    switch (format) {
        case UFT_CBM_DISK_D64:
        case UFT_CBM_DISK_D64_40:
            if (track >= 1 && track <= 40) {
                return D64_SECTORS[track];
            }
            break;
            
        case UFT_CBM_DISK_D71:
        case UFT_CBM_DISK_D71_80:
            /* D71 has two sides, tracks 1-35 on each */
            if (track >= 1 && track <= 35) {
                return D64_SECTORS[track];
            } else if (track >= 36 && track <= 70) {
                return D64_SECTORS[track - 35];
            }
            break;
            
        case UFT_CBM_DISK_D81:
            /* D81 has 40 sectors per track */
            if (track >= 1 && track <= 80) {
                return 40;
            }
            break;
            
        default:
            break;
    }
    return 0;
}

int uft_cbm_sector_offset(uft_cbm_disk_format_t format,
                          uint8_t track, uint8_t sector,
                          uint32_t *offset)
{
    if (!offset) return -1;
    
    uint8_t max_sector = uft_cbm_sectors_per_track(format, track);
    if (max_sector == 0 || sector >= max_sector) {
        return -2;  /* Invalid track/sector */
    }
    
    switch (format) {
        case UFT_CBM_DISK_D64:
        case UFT_CBM_DISK_D64_40:
            if (track >= 1 && track <= 40) {
                *offset = (uint32_t)(D64_TRACK_OFFSET[track] + sector) * UFT_CBM_SECTOR_SIZE;
                return 0;
            }
            break;
            
        case UFT_CBM_DISK_D71:
        case UFT_CBM_DISK_D71_80:
            if (track >= 1 && track <= 35) {
                *offset = (uint32_t)(D64_TRACK_OFFSET[track] + sector) * UFT_CBM_SECTOR_SIZE;
                return 0;
            } else if (track >= 36 && track <= 70) {
                /* Side 2 starts after side 1 */
                uint32_t side1_size = UFT_D64_SECTORS_STD * UFT_CBM_SECTOR_SIZE;
                uint8_t t2 = track - 35;
                *offset = side1_size + (uint32_t)(D64_TRACK_OFFSET[t2] + sector) * UFT_CBM_SECTOR_SIZE;
                return 0;
            }
            break;
            
        case UFT_CBM_DISK_D81:
            if (track >= 1 && track <= 80) {
                *offset = ((uint32_t)(track - 1) * 40 + sector) * UFT_CBM_SECTOR_SIZE;
                return 0;
            }
            break;
            
        default:
            break;
    }
    
    return -3;
}

const uint8_t *uft_cbm_get_sector(const uft_cbm_disk_t *disk,
                                   uint8_t track, uint8_t sector)
{
    if (!disk || !disk->data) return NULL;
    
    uint32_t offset;
    if (uft_cbm_sector_offset(disk->format, track, sector, &offset) != 0) {
        return NULL;
    }
    
    if (offset + UFT_CBM_SECTOR_SIZE > disk->data_size) {
        return NULL;
    }
    
    return disk->data + offset;
}

/* ============================================================================
 * Disk Loading
 * ============================================================================ */

int uft_cbm_disk_load(const uint8_t *data, size_t size, uft_cbm_disk_t *disk)
{
    if (!data || !disk) return -1;
    
    memset(disk, 0, sizeof(*disk));
    
    disk->format = uft_cbm_detect_format(size);
    if (disk->format == UFT_CBM_DISK_UNKNOWN) {
        return -2;  /* Unknown format */
    }
    
    disk->data = data;
    disk->data_size = size;
    disk->owns_data = 0;
    
    /* Set geometry */
    switch (disk->format) {
        case UFT_CBM_DISK_D64:
            disk->tracks = 35;
            disk->sides = 1;
            disk->total_sectors = UFT_D64_SECTORS_STD;
            break;
            
        case UFT_CBM_DISK_D64_40:
            disk->tracks = 40;
            disk->sides = 1;
            disk->total_sectors = UFT_D64_SECTORS_EXT;
            break;
            
        case UFT_CBM_DISK_D71:
            disk->tracks = 70;
            disk->sides = 2;
            disk->total_sectors = UFT_D71_SECTORS;
            break;
            
        case UFT_CBM_DISK_D81:
            disk->tracks = 80;
            disk->sides = 1;
            disk->total_sectors = UFT_D81_SECTORS;
            break;
            
        default:
            break;
    }
    
    /* Check for error map */
    size_t expected_size = disk->total_sectors * UFT_CBM_SECTOR_SIZE;
    if (size > expected_size) {
        disk->error_map = data + expected_size;
        disk->error_map_size = size - expected_size;
    }
    
    /* Read BAM and directory */
    uft_cbm_read_bam(disk);
    uft_cbm_read_directory(disk);
    
    return 0;
}

int uft_cbm_disk_load_file(const char *filename, uft_cbm_disk_t *disk)
{
    if (!filename || !disk) return -1;
    
    FILE *f = fopen(filename, "rb");
    if (!f) return -2;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0 || size > 1024 * 1024) {  /* Max 1MB */
        fclose(f);
        return -3;
    }
    
    uint8_t *data = malloc((size_t)size);
    if (!data) {
        fclose(f);
        return -4;
    }
    
    if (fread(data, 1, (size_t)size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return -5;
    }
    fclose(f);
    
    int ret = uft_cbm_disk_load(data, (size_t)size, disk);
    if (ret == 0) {
        disk->owns_data = 1;  /* We own this memory now */
    } else {
        free(data);
    }
    
    return ret;
}

void uft_cbm_disk_free(uft_cbm_disk_t *disk)
{
    if (!disk) return;
    
    if (disk->owns_data && disk->data) {
        free((void*)disk->data);
    }
    
    if (disk->directory) {
        free(disk->directory);
    }
    
    memset(disk, 0, sizeof(*disk));
}

/* ============================================================================
 * BAM Reading
 * ============================================================================ */

int uft_cbm_read_bam(uft_cbm_disk_t *disk)
{
    if (!disk || !disk->data) return -1;
    
    memset(&disk->bam, 0, sizeof(disk->bam));
    
    const uint8_t *bam_sector = NULL;
    
    switch (disk->format) {
        case UFT_CBM_DISK_D64:
        case UFT_CBM_DISK_D64_40:
        case UFT_CBM_DISK_D71:
            bam_sector = uft_cbm_get_sector(disk, 18, 0);
            break;
            
        case UFT_CBM_DISK_D81:
            bam_sector = uft_cbm_get_sector(disk, 40, 0);
            break;
            
        default:
            return -2;
    }
    
    if (!bam_sector) return -3;
    
    /* Read disk name and ID */
    if (disk->format == UFT_CBM_DISK_D81) {
        uft_cbm_petscii_to_ascii(bam_sector + 0x04, 16, 
                                  disk->bam.disk_name, sizeof(disk->bam.disk_name));
        memcpy(disk->bam.disk_id, bam_sector + 0x16, 2);
        disk->bam.disk_id[2] = '\0';
        disk->bam.dos_type = bam_sector[0x02];
        disk->bam.dos_version = bam_sector[0x19];
    } else {
        uft_cbm_petscii_to_ascii(bam_sector + 0x90, 16,
                                  disk->bam.disk_name, sizeof(disk->bam.disk_name));
        memcpy(disk->bam.disk_id, bam_sector + 0xA2, 2);
        disk->bam.disk_id[2] = '\0';
        disk->bam.dos_type = bam_sector[0xA5];
        disk->bam.dos_version = bam_sector[0xA6];
    }
    
    /* Count free blocks (D64/D71) */
    if (disk->format != UFT_CBM_DISK_D81) {
        disk->bam.blocks_total = 0;
        disk->bam.blocks_free = 0;
        
        for (uint8_t t = 1; t <= disk->tracks && t <= 40; t++) {
            if (t == 18) continue;  /* Skip directory track */
            
            uint8_t sectors = uft_cbm_sectors_per_track(disk->format, t);
            disk->bam.blocks_total += sectors;
            
            /* BAM entry: offset 4 + (t-1)*4, byte 0 = free count */
            int bam_off = 4 + (t - 1) * 4;
            if (bam_off < 256) {
                disk->bam.track_free[t] = bam_sector[bam_off];
                disk->bam.blocks_free += bam_sector[bam_off];
            }
        }
        
        /* D71 side 2 BAM */
        if (disk->format == UFT_CBM_DISK_D71) {
            const uint8_t *bam2 = uft_cbm_get_sector(disk, 53, 0);
            if (bam2) {
                for (uint8_t t = 36; t <= 70; t++) {
                    if (t == 53) continue;
                    uint8_t sectors = uft_cbm_sectors_per_track(disk->format, t);
                    disk->bam.blocks_total += sectors;
                    /* Side 2 BAM at offset 0 */
                    int bam_off = (t - 36) * 3;
                    if (bam_off < 256) {
                        disk->bam.track_free[t] = bam2[bam_off];
                        disk->bam.blocks_free += bam2[bam_off];
                    }
                }
            }
        }
    } else {
        /* D81 BAM */
        disk->bam.blocks_total = 3160;  /* 80 tracks * 40 - 40 (track 40) */
        disk->bam.blocks_free = 0;
        
        /* Count from BAM sectors */
        const uint8_t *bam1 = uft_cbm_get_sector(disk, 40, 1);
        const uint8_t *bam2 = uft_cbm_get_sector(disk, 40, 2);
        
        if (bam1) {
            for (int i = 0; i < 40; i++) {
                disk->bam.track_free[i + 1] = bam1[0x10 + i * 6];
                disk->bam.blocks_free += bam1[0x10 + i * 6];
            }
        }
        if (bam2) {
            for (int i = 0; i < 40; i++) {
                disk->bam.track_free[i + 41] = bam2[0x10 + i * 6];
                disk->bam.blocks_free += bam2[0x10 + i * 6];
            }
        }
    }
    
    disk->bam.blocks_used = disk->bam.blocks_total - disk->bam.blocks_free;
    
    return 0;
}

/* ============================================================================
 * Directory Reading
 * ============================================================================ */

int uft_cbm_read_directory(uft_cbm_disk_t *disk)
{
    if (!disk || !disk->data) return -1;
    
    /* Free existing directory */
    if (disk->directory) {
        free(disk->directory);
        disk->directory = NULL;
    }
    disk->dir_count = 0;
    
    /* Allocate directory */
    disk->directory = calloc(UFT_CBM_MAX_DIR_ENTRIES, sizeof(uft_cbm_dir_entry_t));
    if (!disk->directory) return -2;
    
    /* Get first directory sector */
    uint8_t dir_track, dir_sector;
    
    switch (disk->format) {
        case UFT_CBM_DISK_D64:
        case UFT_CBM_DISK_D64_40:
        case UFT_CBM_DISK_D71:
            dir_track = 18;
            dir_sector = 1;
            break;
            
        case UFT_CBM_DISK_D81:
            dir_track = 40;
            dir_sector = 3;
            break;
            
        default:
            return -3;
    }
    
    /* Read directory chain */
    int sectors_read = 0;
    
    while (dir_track != 0 && sectors_read < 20) {
        const uint8_t *sector = uft_cbm_get_sector(disk, dir_track, dir_sector);
        if (!sector) break;
        
        /* 8 directory entries per sector */
        for (int i = 0; i < 8 && disk->dir_count < UFT_CBM_MAX_DIR_ENTRIES; i++) {
            const uint8_t *entry = sector + (i * 32);
            
            /* Check if entry is used */
            uint8_t file_type = entry[2] & 0x07;
            if (file_type == 0 && entry[3] == 0 && entry[4] == 0) {
                continue;  /* Empty slot */
            }
            
            uft_cbm_dir_entry_t *de = &disk->directory[disk->dir_count];
            
            /* File type */
            de->type = (uft_cbm_file_type_t)file_type;
            de->flags = entry[2] & 0xF8;
            
            /* Filename */
            uft_cbm_petscii_to_ascii(entry + 5, 16, 
                                      de->filename, sizeof(de->filename));
            
            /* Start track/sector */
            de->start_track = entry[3];
            de->start_sector = entry[4];
            
            /* Block count */
            de->blocks = (uint16_t)(entry[30] | (entry[31] << 8));
            
            /* REL file info */
            if (de->type == UFT_CBM_FILE_REL) {
                de->side_track = entry[21];
                de->side_sector = entry[22];
                de->record_length = entry[23];
            }
            
            /* GEOS info */
            if (entry[24] != 0) {
                de->is_geos = 1;
                de->geos_type = entry[24];
                de->geos_struct = entry[25];
            }
            
            /* Calculate actual size (blocks * 254, approximately) */
            de->size_bytes = de->blocks * 254;
            
            disk->dir_count++;
        }
        
        /* Next sector in chain */
        dir_track = sector[0];
        dir_sector = sector[1];
        sectors_read++;
    }
    
    return (int)disk->dir_count;
}

const uft_cbm_dir_entry_t *uft_cbm_get_entry(const uft_cbm_disk_t *disk, size_t index)
{
    if (!disk || !disk->directory || index >= disk->dir_count) {
        return NULL;
    }
    return &disk->directory[index];
}

int uft_cbm_find_file(const uft_cbm_disk_t *disk, const char *name,
                      uft_cbm_dir_entry_t *entry)
{
    if (!disk || !disk->directory || !name || !entry) return -1;
    
    for (size_t i = 0; i < disk->dir_count; i++) {
        if (strcasecmp(disk->directory[i].filename, name) == 0) {
            *entry = disk->directory[i];
            return (int)i;
        }
    }
    
    return -1;  /* Not found */
}

/* ============================================================================
 * Directory Formatting
 * ============================================================================ */

size_t uft_cbm_format_directory(const uft_cbm_disk_t *disk, char *out, size_t out_cap)
{
    if (!disk || !out || out_cap == 0) return 0;
    
    size_t pos = 0;
    
    #define APPEND(...) do { \
        int n = snprintf(out + pos, out_cap - pos, __VA_ARGS__); \
        if (n > 0 && (size_t)n < out_cap - pos) pos += (size_t)n; \
    } while(0)
    
    /* Header */
    APPEND("0 \"%-16s\" %s\n", disk->bam.disk_name, disk->bam.disk_id);
    
    /* Entries */
    for (size_t i = 0; i < disk->dir_count; i++) {
        const uft_cbm_dir_entry_t *e = &disk->directory[i];
        
        if (e->type == UFT_CBM_FILE_DEL && !(e->flags & 0x80)) {
            continue;  /* Skip truly deleted files */
        }
        
        APPEND("%-5u \"%-16s\" %s%s\n",
               e->blocks,
               e->filename,
               uft_cbm_file_type_name(e->type),
               (e->flags & 0x40) ? "<" : "");  /* Locked */
    }
    
    /* Footer */
    APPEND("%u BLOCKS FREE.\n", disk->bam.blocks_free);
    
    #undef APPEND
    
    return pos;
}

/* ============================================================================
 * File Extraction
 * ============================================================================ */

int uft_cbm_extract_file(const uft_cbm_disk_t *disk,
                         const uft_cbm_dir_entry_t *entry,
                         uft_cbm_file_t *file)
{
    if (!disk || !entry || !file) return -1;
    
    memset(file, 0, sizeof(*file));
    file->entry = *entry;
    
    /* Calculate maximum size (blocks * 256) */
    size_t max_size = (size_t)entry->blocks * UFT_CBM_SECTOR_SIZE;
    
    file->data = malloc(max_size);
    if (!file->data) return -2;
    
    /* Follow sector chain */
    uint8_t track = entry->start_track;
    uint8_t sector = entry->start_sector;
    size_t total = 0;
    int sectors_read = 0;
    
    while (track != 0 && sectors_read < 1000) {
        const uint8_t *sec = uft_cbm_get_sector(disk, track, sector);
        if (!sec) break;
        
        uint8_t next_track = sec[0];
        uint8_t next_sector = sec[1];
        
        size_t data_len;
        if (next_track == 0) {
            /* Last sector - next_sector contains bytes used */
            data_len = next_sector ? next_sector - 1 : 254;
        } else {
            data_len = 254;
        }
        
        if (total + data_len > max_size) {
            data_len = max_size - total;
        }
        
        memcpy(file->data + total, sec + 2, data_len);
        total += data_len;
        
        track = next_track;
        sector = next_sector;
        sectors_read++;
    }
    
    file->data_size = total;
    
    /* Analyze if PRG */
    if (entry->type == UFT_CBM_FILE_PRG && total >= 2) {
        uft_c64_prg_analyze(file->data, total, &file->prg_info);
        /* Scan the payload (skip 2-byte load address) */
        if (file->prg_info.view.payload && file->prg_info.view.payload_size > 0) {
            uft_cbm_drive_scan_payload(file->prg_info.view.payload,
                                       file->prg_info.view.payload_size,
                                       &file->scan_result);
        }
    }
    
    return 0;
}

int uft_cbm_extract_file_by_name(const uft_cbm_disk_t *disk, const char *name,
                                  uft_cbm_file_t *file)
{
    uft_cbm_dir_entry_t entry;
    if (uft_cbm_find_file(disk, name, &entry) < 0) {
        return -1;
    }
    return uft_cbm_extract_file(disk, &entry, file);
}

void uft_cbm_file_free(uft_cbm_file_t *file)
{
    if (!file) return;
    if (file->data) {
        free(file->data);
    }
    memset(file, 0, sizeof(*file));
}

int uft_cbm_extract_all_prg(const uft_cbm_disk_t *disk,
                             uft_cbm_file_t *files, size_t max_files)
{
    if (!disk || !files) return 0;
    
    int count = 0;
    
    for (size_t i = 0; i < disk->dir_count && (size_t)count < max_files; i++) {
        const uft_cbm_dir_entry_t *e = &disk->directory[i];
        
        if (e->type == UFT_CBM_FILE_PRG) {
            if (uft_cbm_extract_file(disk, e, &files[count]) == 0) {
                count++;
            }
        }
    }
    
    return count;
}

/* ============================================================================
 * Disk Analysis
 * ============================================================================ */

int uft_cbm_analyze_disk(uft_cbm_disk_t *disk, uft_cbm_disk_analysis_t *analysis)
{
    if (!disk || !analysis) return -1;
    
    memset(analysis, 0, sizeof(*analysis));
    analysis->format = disk->format;
    analysis->valid = 1;
    
    /* Count file types */
    for (size_t i = 0; i < disk->dir_count; i++) {
        const uft_cbm_dir_entry_t *e = &disk->directory[i];
        
        analysis->total_files++;
        
        switch (e->type) {
            case UFT_CBM_FILE_PRG: analysis->prg_count++; break;
            case UFT_CBM_FILE_SEQ: analysis->seq_count++; break;
            case UFT_CBM_FILE_DEL: analysis->deleted_count++; break;
            default: analysis->other_count++; break;
        }
    }
    
    /* Scan PRG files for tools */
    uft_cbm_file_t prg_files[32];
    int prg_count = uft_cbm_extract_all_prg(disk, prg_files, 32);
    
    for (int i = 0; i < prg_count; i++) {
        if (prg_files[i].scan_result.score >= 10) {
            analysis->has_copy_tools = 1;
            analysis->tool_score += prg_files[i].scan_result.score;
        }
        
        if (prg_files[i].scan_result.is_nibbler) {
            analysis->has_copy_tools = 1;
        }
        
        /* Signature scan */
        uft_sig_result_t sig_result;
        if (uft_sig_scan(prg_files[i].prg_info.view.payload,
                         prg_files[i].prg_info.view.payload_size,
                         prg_files[i].prg_info.view.load_addr,
                         &sig_result) > 0) {
            
            for (int j = 0; j < sig_result.match_count && analysis->tool_count < 8; j++) {
                /* Add tool name if not duplicate */
                const char *name = sig_result.matches[j].entry->name;
                int found = 0;
                for (int k = 0; k < analysis->tool_count; k++) {
                    if (strcmp(analysis->tool_names[k], name) == 0) {
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    strncpy(analysis->tool_names[analysis->tool_count++],
                            name, 31);
                }
                
                if (sig_result.matches[j].entry->category == UFT_SIG_CAT_FASTLOADER) {
                    analysis->has_fastloaders = 1;
                }
                if (sig_result.matches[j].entry->category == UFT_SIG_CAT_PROTECTION) {
                    analysis->has_protection = 1;
                }
            }
        }
        
        uft_cbm_file_free(&prg_files[i]);
    }
    
    return 0;
}

uint32_t uft_cbm_disk_checksum(const uft_cbm_disk_t *disk)
{
    if (!disk || !disk->data) return 0;
    
    /* Simple CRC32-like checksum */
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < disk->data_size; i++) {
        crc ^= disk->data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    
    return ~crc;
}
