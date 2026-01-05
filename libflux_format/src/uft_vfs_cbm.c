/**
 * @file uft_vfs_cbm.c
 * @brief CBM Filesystem Implementation (D64/D71/D81)
 * 
 * Based on FluxEngine cbmfs.cc by David Given
 * Implements Commodore DOS filesystem access
 * 
 * Supports:
 * - D64 (1541): 35 tracks, 683 blocks
 * - D71 (1571): 70 tracks, 1366 blocks
 * - D81 (1581): 80 tracks, 3200 blocks
 * 
 * Version: 1.0.0
 * Date: 2025-12-30
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "uft/uft_vfs.h"

/*============================================================================
 * CBM FILESYSTEM CONSTANTS
 *============================================================================*/

/* File types */
#define CBM_TYPE_DEL    0
#define CBM_TYPE_SEQ    1
#define CBM_TYPE_PRG    2
#define CBM_TYPE_USR    3
#define CBM_TYPE_REL    4
#define CBM_TYPE_CBM    5   /* D81 partition */
#define CBM_TYPE_DIR    6   /* D81 subdirectory */

/* Flags */
#define CBM_FLAG_LOCKED  0x40
#define CBM_FLAG_CLOSED  0x80
#define CBM_FLAG_SPLAT   0x00  /* File not closed properly */

/* Disk parameters */
#define CBM_DIR_ENTRIES_PER_SECTOR 8
#define CBM_BYTES_PER_DIR_ENTRY   32
#define CBM_DATA_BYTES_PER_SECTOR 254

/* D64 parameters */
#define D64_TRACKS           35
#define D64_BAM_TRACK        18
#define D64_DIR_TRACK        18
#define D64_DIR_SECTOR       1
#define D64_TOTAL_BLOCKS     683

/* D71 parameters */
#define D71_TRACKS           70
#define D71_BAM_TRACK        18
#define D71_DIR_TRACK        18
#define D71_TOTAL_BLOCKS     1366

/* D81 parameters */
#define D81_TRACKS           80
#define D81_BAM_TRACK        40
#define D81_DIR_TRACK        40
#define D81_TOTAL_BLOCKS     3200

/*============================================================================
 * SECTORS PER TRACK TABLE
 *============================================================================*/

/* D64/D71 sectors per track */
static const int g_cbm_sectors_per_track[] = {
    /*  0 */  0,  /* Track 0 doesn't exist */
    /* 1-17 */ 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
    /* 18-24 */ 19, 19, 19, 19, 19, 19, 19,
    /* 25-30 */ 18, 18, 18, 18, 18, 18,
    /* 31-35 */ 17, 17, 17, 17, 17,
    /* 36-52 (D71 side 2) */ 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
    /* 53-59 */ 19, 19, 19, 19, 19, 19, 19,
    /* 60-65 */ 18, 18, 18, 18, 18, 18,
    /* 66-70 */ 17, 17, 17, 17, 17
};

/*============================================================================
 * CBM VFS CONTEXT
 *============================================================================*/

typedef struct cbm_context {
    uft_vfs_context_t base;
    
    /* Disk type */
    int disk_type;          /* 64, 71, or 81 */
    int total_tracks;
    int total_blocks;
    int dir_track;
    int bam_track;
    
    /* BAM */
    uint8_t bam[256 * 4];   /* BAM sectors (max 4 for D81) */
    int bam_sectors;
    
    /* Cached info */
    char disk_name[17];
    char disk_id[6];        /* 2 ID chars + DOS type + null */
    uint8_t dos_version;
    int free_blocks;
} cbm_context_t;

/*============================================================================
 * PETSCII CONVERSION
 *============================================================================*/

static void petscii_to_ascii(char* dst, const uint8_t* src, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++) {
        uint8_t c = src[i];
        
        /* Stop at padding character */
        if (c == 0xA0)
            break;
        
        /* Convert to ASCII */
        if (c >= 0x41 && c <= 0x5A) {
            /* Uppercase -> lowercase */
            dst[i] = c + 0x20;
        } else if (c >= 0xC1 && c <= 0xDA) {
            /* PETSCII uppercase -> ASCII uppercase */
            dst[i] = c - 0x80;
        } else if (c >= 0x20 && c <= 0x7E) {
            dst[i] = c;
        } else {
            dst[i] = '?';
        }
    }
    dst[i] = '\0';
}

static void ascii_to_petscii(uint8_t* dst, const char* src, size_t len)
{
    size_t i;
    for (i = 0; i < len && src[i]; i++) {
        char c = src[i];
        
        if (c >= 'a' && c <= 'z') {
            /* Lowercase -> uppercase PETSCII */
            dst[i] = c - 0x20;
        } else if (c >= 'A' && c <= 'Z') {
            /* Uppercase stays uppercase */
            dst[i] = c;
        } else if (c >= 0x20 && c <= 0x7E) {
            dst[i] = c;
        } else {
            dst[i] = '?';
        }
    }
    
    /* Pad with 0xA0 */
    while (i < len) {
        dst[i++] = 0xA0;
    }
}

/*============================================================================
 * TRACK/SECTOR HELPERS
 *============================================================================*/

/**
 * Get number of sectors for a track
 */
static int cbm_sectors_for_track(cbm_context_t* ctx, int track)
{
    if (ctx->disk_type == 81) {
        return 40;  /* D81 has 40 sectors per track */
    }
    
    if (track < 1 || track > 70)
        return 0;
    
    return g_cbm_sectors_per_track[track];
}

/**
 * Calculate block number from track/sector
 */
static int cbm_track_sector_to_block(cbm_context_t* ctx, int track, int sector)
{
    if (ctx->disk_type == 81) {
        /* D81: 40 sectors per track */
        return (track - 1) * 40 + sector;
    }
    
    /* D64/D71: variable sectors per track */
    int block = 0;
    for (int t = 1; t < track; t++) {
        block += g_cbm_sectors_per_track[t];
    }
    return block + sector;
}

/**
 * Read a sector
 */
static int cbm_read_sector(cbm_context_t* ctx, int track, int sector, uint8_t* buffer)
{
    uft_vfs_sector_interface_t* si = ctx->base.sectors;
    
    /* Convert to physical for D71 */
    int head = 0;
    if (ctx->disk_type == 71 && track > 35) {
        head = 1;
        track -= 35;
    }
    
    return si->read(si->context, track - 1, head, sector, buffer, 256);
}

/**
 * Write a sector
 */
static int cbm_write_sector(cbm_context_t* ctx, int track, int sector, const uint8_t* buffer)
{
    uft_vfs_sector_interface_t* si = ctx->base.sectors;
    
    int head = 0;
    if (ctx->disk_type == 71 && track > 35) {
        head = 1;
        track -= 35;
    }
    
    return si->write(si->context, track - 1, head, sector, buffer, 256);
}

/*============================================================================
 * BAM HANDLING
 *============================================================================*/

/**
 * Read the BAM (Block Allocation Map)
 */
static int cbm_read_bam(cbm_context_t* ctx)
{
    uint8_t sector[256];
    int result;
    
    if (ctx->disk_type == 81) {
        /* D81: BAM is in track 40, sectors 1 and 2 */
        result = cbm_read_sector(ctx, 40, 1, ctx->bam);
        if (result < 0) return result;
        
        result = cbm_read_sector(ctx, 40, 2, ctx->bam + 256);
        if (result < 0) return result;
        
        ctx->bam_sectors = 2;
    } else {
        /* D64/D71: BAM is in track 18, sector 0 */
        result = cbm_read_sector(ctx, 18, 0, ctx->bam);
        if (result < 0) return result;
        
        ctx->bam_sectors = 1;
        
        if (ctx->disk_type == 71) {
            /* D71: Second side BAM in track 53, sector 0 */
            result = cbm_read_sector(ctx, 53, 0, ctx->bam + 256);
            if (result < 0) return result;
            ctx->bam_sectors = 2;
        }
    }
    
    return 0;
}

/**
 * Check if a block is free
 */
static bool cbm_block_is_free(cbm_context_t* ctx, int track, int sector)
{
    if (ctx->disk_type == 81) {
        /* D81: Each track has 5 bytes in BAM (free count + 4 bitmap bytes) */
        int offset = 0x10 + (track - 1) * 6;
        if (track > 40) {
            offset = 0x10 + (track - 41) * 6;
            if (offset > 256) return false;
        }
        
        int byte_offset = sector / 8;
        int bit_offset = sector % 8;
        return (ctx->bam[offset + 1 + byte_offset] & (1 << bit_offset)) != 0;
    } else {
        /* D64/D71: Each track has 4 bytes (free count + 3 bitmap bytes) */
        int offset = 4 + (track - 1) * 4;
        if (ctx->disk_type == 71 && track > 35) {
            /* Second side BAM */
            offset = 4 + (track - 36) * 4;
            return (ctx->bam[256 + offset + 1 + sector / 8] & (1 << (sector % 8))) != 0;
        }
        return (ctx->bam[offset + 1 + sector / 8] & (1 << (sector % 8))) != 0;
    }
}

/**
 * Count free blocks
 */
static int cbm_count_free_blocks(cbm_context_t* ctx)
{
    int free = 0;
    
    if (ctx->disk_type == 81) {
        /* D81: Free count in BAM header */
        for (int t = 1; t <= 80; t++) {
            if (t == 40) continue;  /* Skip directory track */
            int offset = (t <= 40) ? (0x10 + (t - 1) * 6) : (0x10 + (t - 41) * 6);
            free += ctx->bam[offset];
        }
    } else {
        /* D64/D71: Free count per track in BAM */
        for (int t = 1; t <= ctx->total_tracks; t++) {
            if (t == 18) continue;  /* Skip directory track */
            
            int offset;
            if (t <= 35) {
                offset = 4 + (t - 1) * 4;
                free += ctx->bam[offset];
            } else if (ctx->disk_type == 71) {
                offset = 4 + (t - 36) * 4;
                free += ctx->bam[256 + offset];
            }
        }
    }
    
    return free;
}

/*============================================================================
 * DIRECTORY HANDLING
 *============================================================================*/

/**
 * Parse a directory entry
 */
static void cbm_parse_dir_entry(const uint8_t* entry, uft_vfs_dirent_t* dirent)
{
    memset(dirent, 0, sizeof(*dirent));
    
    dirent->file_type = entry[2] & 0x0F;
    dirent->attributes = 0;
    
    if (entry[2] & CBM_FLAG_LOCKED)
        dirent->attributes |= UFT_VATTR_LOCKED;
    if (!(entry[2] & CBM_FLAG_CLOSED))
        dirent->attributes |= UFT_VATTR_SPLAT;
    
    dirent->start_track = entry[3];
    dirent->start_sector = entry[4];
    
    /* Filename */
    petscii_to_ascii(dirent->name, entry + 5, 16);
    
    /* REL file info */
    dirent->record_length = entry[21];
    
    /* Block count */
    dirent->blocks = entry[30] | (entry[31] << 8);
    
    /* Approximate size (254 bytes per block) */
    dirent->size = dirent->blocks * CBM_DATA_BYTES_PER_SECTOR;
}

/**
 * Internal directory iterator state
 */
typedef struct cbm_dir_iter {
    cbm_context_t* ctx;
    int track;
    int sector;
    int entry_index;
    uint8_t sector_data[256];
} cbm_dir_iter_t;

/*============================================================================
 * VFS API IMPLEMENTATION
 *============================================================================*/

/**
 * Detect CBM filesystem type
 */
static uft_vfs_type_t cbm_detect(uft_vfs_sector_interface_t* sectors)
{
    uint8_t sector[256];
    
    /* Try reading track 18, sector 0 (D64/D71 BAM) */
    if (sectors->read(sectors->context, 17, 0, 0, sector, 256) == 0) {
        /* Check for valid DOS type */
        uint8_t dos = sector[2];
        if (dos == 0x41 || dos == 0x00) {  /* 'A' or 0 */
            /* Check for D71 by looking at track 53 */
            if (sectors->read(sectors->context, 52, 1, 0, sector, 256) == 0) {
                if (sector[2] == 0x41 || sector[2] == 0x00) {
                    return UFT_VFS_CBM;  /* D71 */
                }
            }
            return UFT_VFS_CBM;  /* D64 */
        }
    }
    
    /* Try D81: track 40, sector 0 */
    if (sectors->read(sectors->context, 39, 0, 0, sector, 256) == 0) {
        if (sector[2] == 0x44) {  /* 'D' for D81 */
            return UFT_VFS_CBM;
        }
    }
    
    return UFT_VFS_UNKNOWN;
}

/**
 * Mount CBM filesystem
 */
int uft_vfs_cbm_mount(uft_vfs_context_t* vfs_ctx, uft_vfs_sector_interface_t* sectors)
{
    cbm_context_t* ctx = (cbm_context_t*)calloc(1, sizeof(cbm_context_t));
    if (!ctx) return UFT_VFS_ERR_NOMEM;
    
    ctx->base.type = UFT_VFS_CBM;
    ctx->base.sectors = sectors;
    
    /* Determine disk type by geometry */
    if (sectors->tracks >= 80) {
        ctx->disk_type = 81;
        ctx->total_tracks = 80;
        ctx->total_blocks = D81_TOTAL_BLOCKS;
        ctx->dir_track = D81_DIR_TRACK;
        ctx->bam_track = D81_BAM_TRACK;
    } else if (sectors->tracks >= 70) {
        ctx->disk_type = 71;
        ctx->total_tracks = 70;
        ctx->total_blocks = D71_TOTAL_BLOCKS;
        ctx->dir_track = D71_DIR_TRACK;
        ctx->bam_track = D71_BAM_TRACK;
    } else {
        ctx->disk_type = 64;
        ctx->total_tracks = 35;
        ctx->total_blocks = D64_TOTAL_BLOCKS;
        ctx->dir_track = D64_DIR_TRACK;
        ctx->bam_track = D64_BAM_TRACK;
    }
    
    /* Read BAM */
    int result = cbm_read_bam(ctx);
    if (result < 0) {
        free(ctx);
        return result;
    }
    
    /* Extract disk info */
    ctx->dos_version = ctx->bam[2];
    petscii_to_ascii(ctx->disk_name, ctx->bam + 0x90, 16);
    
    /* Disk ID */
    ctx->disk_id[0] = ctx->bam[0xA2];
    ctx->disk_id[1] = ctx->bam[0xA3];
    ctx->disk_id[2] = ctx->bam[0xA5];
    ctx->disk_id[3] = ctx->bam[0xA6];
    ctx->disk_id[4] = '\0';
    
    ctx->free_blocks = cbm_count_free_blocks(ctx);
    
    /* Copy to base structure */
    strncpy(ctx->base.info.label, ctx->disk_name, sizeof(ctx->base.info.label) - 1);
    ctx->base.info.type = UFT_VFS_CBM;
    ctx->base.info.total_blocks = ctx->total_blocks;
    ctx->base.info.free_blocks = ctx->free_blocks;
    ctx->base.info.block_size = 254;
    ctx->base.info.dos_version = ctx->dos_version;
    
    ctx->base.fs_data = ctx;
    
    /* Copy context back */
    memcpy(vfs_ctx, &ctx->base, sizeof(uft_vfs_context_t));
    vfs_ctx->fs_data = ctx;
    
    return UFT_VFS_OK;
}

/**
 * Unmount CBM filesystem
 */
int uft_vfs_cbm_unmount(uft_vfs_context_t* ctx)
{
    if (ctx->fs_data) {
        free(ctx->fs_data);
        ctx->fs_data = NULL;
    }
    return UFT_VFS_OK;
}

/**
 * Get filesystem info
 */
int uft_vfs_cbm_get_info(uft_vfs_context_t* ctx, uft_vfs_info_t* info)
{
    cbm_context_t* cbm = (cbm_context_t*)ctx->fs_data;
    if (!cbm) return UFT_VFS_ERR_BADFS;
    
    *info = ctx->info;
    return UFT_VFS_OK;
}

/**
 * Open directory for reading
 */
void* uft_vfs_cbm_opendir(uft_vfs_context_t* ctx, const char* path)
{
    cbm_context_t* cbm = (cbm_context_t*)ctx->fs_data;
    if (!cbm) return NULL;
    
    /* Only root directory supported */
    if (path && path[0] != '\0' && strcmp(path, "/") != 0) {
        return NULL;
    }
    
    cbm_dir_iter_t* iter = (cbm_dir_iter_t*)calloc(1, sizeof(cbm_dir_iter_t));
    if (!iter) return NULL;
    
    iter->ctx = cbm;
    iter->track = cbm->dir_track;
    iter->sector = D64_DIR_SECTOR;
    iter->entry_index = 0;
    
    /* Read first directory sector */
    if (cbm_read_sector(cbm, iter->track, iter->sector, iter->sector_data) < 0) {
        free(iter);
        return NULL;
    }
    
    return iter;
}

/**
 * Read next directory entry
 */
int uft_vfs_cbm_readdir(void* handle, uft_vfs_dirent_t* entry)
{
    cbm_dir_iter_t* iter = (cbm_dir_iter_t*)handle;
    if (!iter) return UFT_VFS_ERR_BADFS;
    
    while (iter->track != 0) {
        /* Check all 8 entries in current sector */
        while (iter->entry_index < CBM_DIR_ENTRIES_PER_SECTOR) {
            const uint8_t* dir_entry = iter->sector_data + (iter->entry_index * CBM_BYTES_PER_DIR_ENTRY);
            iter->entry_index++;
            
            /* Skip empty/deleted entries */
            if (dir_entry[2] == 0)
                continue;
            
            /* Parse entry */
            cbm_parse_dir_entry(dir_entry, entry);
            return UFT_VFS_OK;
        }
        
        /* Move to next directory sector */
        int next_track = iter->sector_data[0];
        int next_sector = iter->sector_data[1];
        
        if (next_track == 0) {
            iter->track = 0;
            break;
        }
        
        iter->track = next_track;
        iter->sector = next_sector;
        iter->entry_index = 0;
        
        if (cbm_read_sector(iter->ctx, iter->track, iter->sector, iter->sector_data) < 0) {
            return UFT_VFS_ERR_IO;
        }
    }
    
    return 1;  /* End of directory */
}

/**
 * Close directory
 */
void uft_vfs_cbm_closedir(void* handle)
{
    free(handle);
}

/**
 * Read file contents
 */
int uft_vfs_cbm_read_file(uft_vfs_context_t* ctx,
                          const char* path,
                          uint8_t* buffer,
                          size_t size,
                          size_t* bytes_read)
{
    cbm_context_t* cbm = (cbm_context_t*)ctx->fs_data;
    if (!cbm) return UFT_VFS_ERR_BADFS;
    
    *bytes_read = 0;
    
    /* Find file in directory */
    void* dir = uft_vfs_cbm_opendir(ctx, "/");
    if (!dir) return UFT_VFS_ERR_NOTFOUND;
    
    uft_vfs_dirent_t entry;
    bool found = false;
    
    while (uft_vfs_cbm_readdir(dir, &entry) == 0) {
        if (strcasecmp(entry.name, path) == 0) {
            found = true;
            break;
        }
    }
    
    uft_vfs_cbm_closedir(dir);
    
    if (!found) return UFT_VFS_ERR_NOTFOUND;
    
    /* REL files not supported */
    if (entry.file_type == CBM_TYPE_REL) {
        return UFT_VFS_ERR_BADTYPE;
    }
    
    /* Follow file chain */
    int track = entry.start_track;
    int sector = entry.start_sector;
    uint8_t sector_data[256];
    size_t pos = 0;
    
    while (track != 0 && pos < size) {
        if (cbm_read_sector(cbm, track, sector, sector_data) < 0) {
            return UFT_VFS_ERR_IO;
        }
        
        int next_track = sector_data[0];
        int next_sector = sector_data[1];
        
        size_t data_len;
        if (next_track == 0) {
            /* Last sector: next_sector is byte count */
            data_len = next_sector - 1;
        } else {
            data_len = CBM_DATA_BYTES_PER_SECTOR;
        }
        
        if (pos + data_len > size) {
            data_len = size - pos;
        }
        
        memcpy(buffer + pos, sector_data + 2, data_len);
        pos += data_len;
        
        track = next_track;
        sector = next_sector;
    }
    
    *bytes_read = pos;
    return UFT_VFS_OK;
}

/**
 * Get CBM file type string
 */
const char* uft_vfs_cbm_type_string(uint8_t type)
{
    switch (type & 0x0F) {
        case CBM_TYPE_DEL: return "DEL";
        case CBM_TYPE_SEQ: return "SEQ";
        case CBM_TYPE_PRG: return "PRG";
        case CBM_TYPE_USR: return "USR";
        case CBM_TYPE_REL: return "REL";
        case CBM_TYPE_CBM: return "CBM";
        case CBM_TYPE_DIR: return "DIR";
        default: return "???";
    }
}

/**
 * Get CBM disk ID
 */
void uft_vfs_cbm_get_id(uft_vfs_context_t* ctx, char* id)
{
    cbm_context_t* cbm = (cbm_context_t*)ctx->fs_data;
    if (cbm) {
        memcpy(id, cbm->disk_id, 5);
    } else {
        id[0] = '\0';
    }
}

/**
 * Read CBM BAM
 */
int uft_vfs_cbm_read_bam(uft_vfs_context_t* ctx, uint8_t* bam, size_t size)
{
    cbm_context_t* cbm = (cbm_context_t*)ctx->fs_data;
    if (!cbm) return UFT_VFS_ERR_BADFS;
    
    size_t copy_size = cbm->bam_sectors * 256;
    if (copy_size > size) copy_size = size;
    
    memcpy(bam, cbm->bam, copy_size);
    return (int)copy_size;
}

/*============================================================================
 * VFS TYPE NAME
 *============================================================================*/

const char* uft_vfs_type_name(uft_vfs_type_t type)
{
    switch (type) {
        case UFT_VFS_FAT12: return "FAT12";
        case UFT_VFS_FAT16: return "FAT16";
        case UFT_VFS_CPM: return "CP/M";
        case UFT_VFS_CPM3: return "CP/M Plus";
        case UFT_VFS_CBM: return "CBM DOS";
        case UFT_VFS_GEOS: return "GEOS";
        case UFT_VFS_DOS33: return "Apple DOS 3.3";
        case UFT_VFS_PRODOS: return "ProDOS";
        case UFT_VFS_HFS: return "HFS";
        case UFT_VFS_DFS: return "Acorn DFS";
        case UFT_VFS_ADFS: return "ADFS";
        case UFT_VFS_OFS: return "Amiga OFS";
        case UFT_VFS_FFS: return "Amiga FFS";
        case UFT_VFS_BROTHER: return "Brother";
        case UFT_VFS_LIF: return "HP LIF";
        case UFT_VFS_ROLAND: return "Roland";
        case UFT_VFS_SMAKY: return "Smaky 6";
        case UFT_VFS_ZDOS: return "Z-DOS";
        case UFT_VFS_MICRODOS: return "MicroDOS";
        case UFT_VFS_PHILE: return "Philips Phile";
        default: return "Unknown";
    }
}

/*============================================================================
 * PETSCII CONVERSION (VFS API)
 *============================================================================*/

void uft_vfs_petscii_to_ascii(char* dst, const uint8_t* src, size_t len)
{
    petscii_to_ascii(dst, src, len);
}

void uft_vfs_ascii_to_petscii(uint8_t* dst, const char* src, size_t len)
{
    ascii_to_petscii(dst, src, len);
}
