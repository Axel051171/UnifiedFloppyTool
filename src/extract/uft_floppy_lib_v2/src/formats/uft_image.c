/**
 * @file uft_image.c
 * @brief Disk image format implementation
 */

#include "formats/uft_image.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Format Information Tables
 * ============================================================================ */

static const char* format_names[] = {
    [UFT_IMG_UNKNOWN]   = "Unknown",
    [UFT_IMG_D64]       = "D64 (C64 1541)",
    [UFT_IMG_G64]       = "G64 (C64 GCR)",
    [UFT_IMG_D71]       = "D71 (C128 1571)",
    [UFT_IMG_D81]       = "D81 (C128 1581)",
    [UFT_IMG_D80]       = "D80 (CBM 8050)",
    [UFT_IMG_D82]       = "D82 (CBM 8250)",
    [UFT_IMG_NIB_C64]   = "NIB (C64 Nibble)",
    [UFT_IMG_ADF]       = "ADF (Amiga)",
    [UFT_IMG_ADZ]       = "ADZ (Compressed ADF)",
    [UFT_IMG_DMS]       = "DMS (DiskMasher)",
    [UFT_IMG_FDI]       = "FDI (Amiga)",
    [UFT_IMG_ST]        = "ST (Atari ST)",
    [UFT_IMG_MSA]       = "MSA (Atari Compressed)",
    [UFT_IMG_STX]       = "STX (PASTI)",
    [UFT_IMG_DO]        = "DO (Apple DOS)",
    [UFT_IMG_PO]        = "PO (Apple ProDOS)",
    [UFT_IMG_NIB_APPLE] = "NIB (Apple)",
    [UFT_IMG_WOZ]       = "WOZ (Apple Flux)",
    [UFT_IMG_IMG]       = "IMG (Raw Sector)",
    [UFT_IMG_IMA]       = "IMA (Raw Sector)",
    [UFT_IMG_IMD]       = "IMD (ImageDisk)",
    [UFT_IMG_TD0]       = "TD0 (TeleDisk)",
    [UFT_IMG_DSK]       = "DSK (Generic)",
    [UFT_IMG_FLP]       = "FLP (Raw Floppy)",
    [UFT_IMG_SCP]       = "SCP (SuperCard Pro)",
    [UFT_IMG_KF]        = "KF (KryoFlux)",
    [UFT_IMG_HFE]       = "HFE (HxC Emulator)",
    [UFT_IMG_MFM]       = "MFM (Raw MFM)",
    [UFT_IMG_FLUX]      = "FLUX (Generic)",
};

static const char* format_extensions[] = {
    [UFT_IMG_UNKNOWN]   = "",
    [UFT_IMG_D64]       = "d64",
    [UFT_IMG_G64]       = "g64",
    [UFT_IMG_D71]       = "d71",
    [UFT_IMG_D81]       = "d81",
    [UFT_IMG_D80]       = "d80",
    [UFT_IMG_D82]       = "d82",
    [UFT_IMG_NIB_C64]   = "nib",
    [UFT_IMG_ADF]       = "adf",
    [UFT_IMG_ADZ]       = "adz",
    [UFT_IMG_DMS]       = "dms",
    [UFT_IMG_FDI]       = "fdi",
    [UFT_IMG_ST]        = "st",
    [UFT_IMG_MSA]       = "msa",
    [UFT_IMG_STX]       = "stx",
    [UFT_IMG_DO]        = "do",
    [UFT_IMG_PO]        = "po",
    [UFT_IMG_NIB_APPLE] = "nib",
    [UFT_IMG_WOZ]       = "woz",
    [UFT_IMG_IMG]       = "img",
    [UFT_IMG_IMA]       = "ima",
    [UFT_IMG_IMD]       = "imd",
    [UFT_IMG_TD0]       = "td0",
    [UFT_IMG_DSK]       = "dsk",
    [UFT_IMG_FLP]       = "flp",
    [UFT_IMG_SCP]       = "scp",
    [UFT_IMG_KF]        = "raw",
    [UFT_IMG_HFE]       = "hfe",
    [UFT_IMG_MFM]       = "mfm",
    [UFT_IMG_FLUX]      = "flux",
};

/* Standard geometries */
static const uft_image_geometry_t d64_geometry = {
    .cylinders = 35, .heads = 1, .sectors = 0,  /* Variable */
    .sector_size = 256, .total_size = 174848
};

static const uft_image_geometry_t adf_dd_geometry = {
    .cylinders = 80, .heads = 2, .sectors = 11,
    .sector_size = 512, .total_size = 901120
};

static const uft_image_geometry_t adf_hd_geometry = {
    .cylinders = 80, .heads = 2, .sectors = 22,
    .sector_size = 512, .total_size = 1802240
};

static const uft_image_geometry_t pc_720k_geometry = {
    .cylinders = 80, .heads = 2, .sectors = 9,
    .sector_size = 512, .total_size = 737280
};

static const uft_image_geometry_t pc_1440k_geometry = {
    .cylinders = 80, .heads = 2, .sectors = 18,
    .sector_size = 512, .total_size = 1474560
};

/* ============================================================================
 * Image Handle Structure
 * ============================================================================ */

struct uft_image {
    FILE *file;
    char *filename;
    uft_image_type_t type;
    uft_image_geometry_t geometry;
    uint32_t caps;
    bool writable;
    
    /* Format-specific data */
    void *format_data;
    
    /* Cached data */
    uint8_t *cache;
    size_t cache_size;
    uint8_t cache_track;
    uint8_t cache_head;
    bool cache_valid;
};

/* ============================================================================
 * Magic Bytes Detection
 * ============================================================================ */

/* Magic byte signatures */
static const uint8_t g64_magic[] = { 'G', 'C', 'R', '-', '1', '5', '4', '1' };
static const uint8_t scp_magic[] = { 'S', 'C', 'P' };
static const uint8_t hfe_magic[] = { 'H', 'X', 'C', 'P', 'I', 'C', 'F', 'E' };
static const uint8_t woz_magic[] = { 'W', 'O', 'Z', '1' };
static const uint8_t woz2_magic[] = { 'W', 'O', 'Z', '2' };
static const uint8_t imd_magic[] = { 'I', 'M', 'D', ' ' };
static const uint8_t td0_magic[] = { 'T', 'D' };  /* or "td" */
static const uint8_t dms_magic[] = { 'D', 'M', 'S', '!' };
static const uint8_t stx_magic[] = { 'R', 'S', 'Y', 0x00 };

uft_image_type_t uft_image_detect_magic(const uint8_t *data, size_t len) {
    if (!data || len < 4) return UFT_IMG_UNKNOWN;
    
    /* Check magic signatures */
    if (len >= 8 && memcmp(data, g64_magic, 8) == 0) return UFT_IMG_G64;
    if (len >= 3 && memcmp(data, scp_magic, 3) == 0) return UFT_IMG_SCP;
    if (len >= 8 && memcmp(data, hfe_magic, 8) == 0) return UFT_IMG_HFE;
    if (len >= 4 && memcmp(data, woz_magic, 4) == 0) return UFT_IMG_WOZ;
    if (len >= 4 && memcmp(data, woz2_magic, 4) == 0) return UFT_IMG_WOZ;
    if (len >= 4 && memcmp(data, imd_magic, 4) == 0) return UFT_IMG_IMD;
    if (len >= 2 && (memcmp(data, td0_magic, 2) == 0 || 
                     memcmp(data, "td", 2) == 0)) return UFT_IMG_TD0;
    if (len >= 4 && memcmp(data, dms_magic, 4) == 0) return UFT_IMG_DMS;
    if (len >= 4 && memcmp(data, stx_magic, 4) == 0) return UFT_IMG_STX;
    
    return UFT_IMG_UNKNOWN;
}

uft_image_type_t uft_image_detect_format(const char *filename) {
    if (!filename) return UFT_IMG_UNKNOWN;
    
    /* First try magic bytes */
    FILE *f = fopen(filename, "rb");
    if (f) {
        uint8_t header[16];
        size_t n = fread(header, 1, sizeof(header), f);
        
        /* Get file size */
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fclose(f);
        
        uft_image_type_t type = uft_image_detect_magic(header, n);
        if (type != UFT_IMG_UNKNOWN) return type;
        
        /* Detect by size */
        if (size == 174848 || size == 175531 || size == 196608) return UFT_IMG_D64;
        if (size == 349696) return UFT_IMG_D71;
        if (size == 819200) return UFT_IMG_D81;
        if (size == 901120) return UFT_IMG_ADF;
        if (size == 1802240) return UFT_IMG_ADF;  /* HD */
        if (size == 737280) return UFT_IMG_IMG;   /* 720K */
        if (size == 1474560) return UFT_IMG_IMG;  /* 1.44M */
        if (size == 368640) return UFT_IMG_IMG;   /* 360K */
        if (size == 143360) return UFT_IMG_DO;    /* Apple 5.25" */
    }
    
    /* Try extension */
    const char *ext = strrchr(filename, '.');
    if (ext) {
        ext++;  /* Skip dot */
        for (int i = 1; i < UFT_IMG_COUNT; i++) {
            if (strcasecmp(ext, format_extensions[i]) == 0) {
                return (uft_image_type_t)i;
            }
        }
    }
    
    return UFT_IMG_UNKNOWN;
}

const char* uft_image_type_name(uft_image_type_t type) {
    if (type >= UFT_IMG_COUNT) return "Unknown";
    return format_names[type];
}

const char* uft_image_type_extension(uft_image_type_t type) {
    if (type >= UFT_IMG_COUNT) return "";
    return format_extensions[type];
}

/* ============================================================================
 * Image Open/Close
 * ============================================================================ */

uft_image_t* uft_image_open(const char *filename, const char *mode) {
    if (!filename || !mode) return NULL;
    
    uft_image_type_t type = uft_image_detect_format(filename);
    if (type == UFT_IMG_UNKNOWN) return NULL;
    
    uft_image_t *img = calloc(1, sizeof(uft_image_t));
    if (!img) return NULL;
    
    img->filename = strdup(filename);
    img->type = type;
    img->writable = (strchr(mode, 'w') != NULL || strchr(mode, '+') != NULL);
    
    const char *fmode = img->writable ? "r+b" : "rb";
    img->file = fopen(filename, fmode);
    if (!img->file) {
        free(img->filename);
        free(img);
        return NULL;
    }
    
    /* Set geometry based on type */
    switch (type) {
        case UFT_IMG_D64:
            img->geometry = d64_geometry;
            img->caps = UFT_IMG_CAP_READ | UFT_IMG_CAP_WRITE;
            break;
            
        case UFT_IMG_ADF:
            /* Determine DD or HD by size */
            fseek(img->file, 0, SEEK_END);
            if (ftell(img->file) > 1000000) {
                img->geometry = adf_hd_geometry;
            } else {
                img->geometry = adf_dd_geometry;
            }
            fseek(img->file, 0, SEEK_SET);
            img->caps = UFT_IMG_CAP_READ | UFT_IMG_CAP_WRITE;
            break;
            
        case UFT_IMG_G64:
            img->geometry = d64_geometry;
            img->caps = UFT_IMG_CAP_READ | UFT_IMG_CAP_TIMING;
            break;
            
        case UFT_IMG_SCP:
            img->caps = UFT_IMG_CAP_READ | UFT_IMG_CAP_FLUX | UFT_IMG_CAP_WEAK_BITS;
            break;
            
        case UFT_IMG_IMG:
        case UFT_IMG_IMA:
            fseek(img->file, 0, SEEK_END);
            if (ftell(img->file) >= 1474560) {
                img->geometry = pc_1440k_geometry;
            } else {
                img->geometry = pc_720k_geometry;
            }
            fseek(img->file, 0, SEEK_SET);
            img->caps = UFT_IMG_CAP_READ | UFT_IMG_CAP_WRITE;
            break;
            
        default:
            img->caps = UFT_IMG_CAP_READ;
            break;
    }
    
    return img;
}

uft_image_t* uft_image_create(const char *filename, uft_image_type_t type,
                               const uft_image_geometry_t *geometry) {
    if (!filename || !geometry) return NULL;
    
    uft_image_t *img = calloc(1, sizeof(uft_image_t));
    if (!img) return NULL;
    
    img->filename = strdup(filename);
    img->type = type;
    img->geometry = *geometry;
    img->writable = true;
    
    img->file = fopen(filename, "w+b");
    if (!img->file) {
        free(img->filename);
        free(img);
        return NULL;
    }
    
    /* Pre-allocate file */
    uint8_t zero = 0;
    fseek(img->file, (long)geometry->total_size - 1, SEEK_SET);
    fwrite(&zero, 1, 1, img->file);
    fseek(img->file, 0, SEEK_SET);
    
    img->caps = UFT_IMG_CAP_READ | UFT_IMG_CAP_WRITE;
    
    return img;
}

void uft_image_close(uft_image_t *image) {
    if (image) {
        if (image->file) fclose(image->file);
        free(image->filename);
        free(image->cache);
        free(image->format_data);
        free(image);
    }
}

uft_image_type_t uft_image_get_type(const uft_image_t *image) {
    return image ? image->type : UFT_IMG_UNKNOWN;
}

bool uft_image_get_geometry(const uft_image_t *image, uft_image_geometry_t *geometry) {
    if (!image || !geometry) return false;
    *geometry = image->geometry;
    return true;
}

uint32_t uft_image_get_caps(const uft_image_t *image) {
    return image ? image->caps : 0;
}

/* ============================================================================
 * Track Operations
 * ============================================================================ */

void uft_track_free(uft_track_t *track) {
    if (track) {
        free(track->data);
        free(track->flux);
        free(track->weak_mask);
        memset(track, 0, sizeof(*track));
    }
}

bool uft_image_read_track(uft_image_t *image, uint8_t track, uint8_t head,
                           uft_track_t *data) {
    if (!image || !data || !image->file) return false;
    
    memset(data, 0, sizeof(*data));
    
    switch (image->type) {
        case UFT_IMG_D64: {
            /* D64: Variable sectors per track */
            static const uint8_t sectors_per_track[] = {
                21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,  /* 1-17 */
                19,19,19,19,19,19,19,                                  /* 18-24 */
                18,18,18,18,18,18,                                      /* 25-30 */
                17,17,17,17,17                                          /* 31-35 */
            };
            
            if (track >= 35 || head != 0) return false;
            
            /* Calculate offset */
            size_t offset = 0;
            for (int t = 0; t < track; t++) {
                offset += sectors_per_track[t] * 256;
            }
            
            size_t track_size = (size_t)sectors_per_track[track] * 256;
            data->data = malloc(track_size);
            if (!data->data) return false;
            
            fseek(image->file, (long)offset, SEEK_SET);
            data->data_len = fread(data->data, 1, track_size, image->file);
            data->encoding = 2;  /* GCR */
            data->formatted = true;
            
            return data->data_len == track_size;
        }
        
        case UFT_IMG_ADF:
        case UFT_IMG_IMG:
        case UFT_IMG_IMA: {
            size_t track_size = (size_t)image->geometry.sectors * image->geometry.sector_size;
            size_t offset = (size_t)((track * image->geometry.heads + head) * track_size);
            
            data->data = malloc(track_size);
            if (!data->data) return false;
            
            fseek(image->file, (long)offset, SEEK_SET);
            data->data_len = fread(data->data, 1, track_size, image->file);
            data->encoding = 0;  /* MFM */
            data->formatted = true;
            
            return data->data_len == track_size;
        }
        
        default:
            return false;
    }
}

bool uft_image_write_track(uft_image_t *image, uint8_t track, uint8_t head,
                            const uft_track_t *data) {
    if (!image || !data || !image->file || !image->writable) return false;
    
    switch (image->type) {
        case UFT_IMG_ADF:
        case UFT_IMG_IMG:
        case UFT_IMG_IMA: {
            size_t track_size = (size_t)image->geometry.sectors * image->geometry.sector_size;
            size_t offset = (size_t)((track * image->geometry.heads + head) * track_size);
            
            if (data->data_len != track_size) return false;
            
            fseek(image->file, (long)offset, SEEK_SET);
            size_t written = fwrite(data->data, 1, track_size, image->file);
            fflush(image->file);
            
            return written == track_size;
        }
        
        default:
            return false;
    }
}

/* ============================================================================
 * Sector Operations
 * ============================================================================ */

ssize_t uft_image_read_sector(uft_image_t *image, uint8_t track, uint8_t head,
                               uint8_t sector, uint8_t *data, size_t data_len) {
    if (!image || !data || !image->file) return -1;
    
    size_t sector_size = image->geometry.sector_size;
    if (data_len < sector_size) return -1;
    
    switch (image->type) {
        case UFT_IMG_D64: {
            /* D64: Complex sector mapping */
            static const uint8_t sectors_per_track[] = {
                21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
                19,19,19,19,19,19,19,
                18,18,18,18,18,18,
                17,17,17,17,17
            };
            
            if (track >= 35 || sector >= sectors_per_track[track]) return -1;
            
            size_t offset = 0;
            for (int t = 0; t < track; t++) {
                offset += sectors_per_track[t] * 256;
            }
            offset += (size_t)sector * 256;
            
            fseek(image->file, (long)offset, SEEK_SET);
            return (ssize_t)fread(data, 1, 256, image->file);
        }
        
        case UFT_IMG_ADF:
        case UFT_IMG_IMG:
        case UFT_IMG_IMA: {
            if (sector >= image->geometry.sectors) return -1;
            
            size_t offset = (size_t)(track * image->geometry.heads + head) * 
                            image->geometry.sectors * sector_size +
                            (size_t)sector * sector_size;
            
            fseek(image->file, (long)offset, SEEK_SET);
            return (ssize_t)fread(data, 1, sector_size, image->file);
        }
        
        default:
            return -1;
    }
}

ssize_t uft_image_write_sector(uft_image_t *image, uint8_t track, uint8_t head,
                                uint8_t sector, const uint8_t *data, size_t data_len) {
    if (!image || !data || !image->file || !image->writable) return -1;
    
    size_t sector_size = image->geometry.sector_size;
    if (data_len < sector_size) return -1;
    
    switch (image->type) {
        case UFT_IMG_ADF:
        case UFT_IMG_IMG:
        case UFT_IMG_IMA: {
            if (sector >= image->geometry.sectors) return -1;
            
            size_t offset = (size_t)(track * image->geometry.heads + head) * 
                            image->geometry.sectors * sector_size +
                            (size_t)sector * sector_size;
            
            fseek(image->file, (long)offset, SEEK_SET);
            size_t written = fwrite(data, 1, sector_size, image->file);
            fflush(image->file);
            return (ssize_t)written;
        }
        
        default:
            return -1;
    }
}

/* ============================================================================
 * D64 Functions
 * ============================================================================ */

size_t uft_d64_read_directory(uft_image_t *image, uft_d64_dir_entry_t *entries,
                               size_t max_entries) {
    if (!image || !entries || image->type != UFT_IMG_D64) return 0;
    
    size_t count = 0;
    uint8_t sector[256];
    
    /* Directory starts at track 18, sector 1 */
    int dir_track = 18;
    int dir_sector = 1;
    
    while (dir_sector != 0 && count < max_entries) {
        if (uft_image_read_sector(image, (uint8_t)(dir_track - 1), 0, 
                                   (uint8_t)dir_sector, sector, 256) < 0) {
            break;
        }
        
        /* 8 directory entries per sector */
        for (int i = 0; i < 8 && count < max_entries; i++) {
            uint8_t *entry = &sector[i * 32];
            
            /* Skip deleted/empty entries */
            if (entry[2] == 0) continue;
            
            uft_d64_dir_entry_t *de = &entries[count];
            de->file_type = entry[2];
            de->start_track = entry[3];
            de->start_sector = entry[4];
            
            /* Copy filename (16 chars, PETSCII) */
            memcpy(de->name, &entry[5], 16);
            de->name[16] = '\0';
            
            /* Trim trailing 0xA0 (shifted space) */
            for (int j = 15; j >= 0 && de->name[j] == (char)0xA0; j--) {
                de->name[j] = '\0';
            }
            
            de->blocks = (uint16_t)(entry[30] | (entry[31] << 8));
            
            count++;
        }
        
        /* Next directory sector */
        dir_track = sector[0];
        dir_sector = sector[1];
    }
    
    return count;
}

ssize_t uft_d64_read_file(uft_image_t *image, const char *name,
                           uint8_t *data, size_t max_len) {
    if (!image || !name || !data) return -1;
    
    /* Find file in directory */
    uft_d64_dir_entry_t entries[144];
    size_t dir_count = uft_d64_read_directory(image, entries, 144);
    
    uft_d64_dir_entry_t *found = NULL;
    for (size_t i = 0; i < dir_count; i++) {
        if (strcasecmp(entries[i].name, name) == 0) {
            found = &entries[i];
            break;
        }
    }
    
    if (!found) return -1;
    
    /* Follow chain and read data */
    size_t total = 0;
    int track = found->start_track;
    int sector = found->start_sector;
    uint8_t buf[256];
    
    while (track != 0 && total < max_len) {
        if (uft_image_read_sector(image, (uint8_t)(track - 1), 0,
                                   (uint8_t)sector, buf, 256) < 0) {
            break;
        }
        
        int next_track = buf[0];
        int next_sector = buf[1];
        
        size_t data_bytes;
        if (next_track == 0) {
            /* Last sector - next_sector contains byte count */
            data_bytes = (size_t)(next_sector - 1);
        } else {
            data_bytes = 254;
        }
        
        if (total + data_bytes > max_len) {
            data_bytes = max_len - total;
        }
        
        memcpy(data + total, buf + 2, data_bytes);
        total += data_bytes;
        
        track = next_track;
        sector = next_sector;
    }
    
    return (ssize_t)total;
}

/* ============================================================================
 * ADF Functions
 * ============================================================================ */

bool uft_adf_read_info(uft_image_t *image, uft_adf_info_t *info) {
    if (!image || !info || image->type != UFT_IMG_ADF) return false;
    
    memset(info, 0, sizeof(*info));
    
    /* Read boot block (block 0) */
    uint8_t boot[512];
    if (uft_image_read_sector(image, 0, 0, 0, boot, 512) < 0) {
        return false;
    }
    
    /* Check for DOS signature */
    if (memcmp(boot, "DOS", 3) != 0) {
        return false;
    }
    
    /* Filesystem type */
    info->is_ffs = (boot[3] & 1) != 0;
    info->is_intl = (boot[3] & 2) != 0;
    info->is_dircache = (boot[3] & 4) != 0;
    
    /* Root block is at middle of disk */
    info->root_block = 880;  /* DD disk */
    info->bitmap_block = 881;
    
    /* Read root block for volume name */
    uint8_t root[512];
    if (uft_image_read_sector(image, 880 / 11, 0, 880 % 11, root, 512) < 0) {
        return false;
    }
    
    /* Volume name at offset 0x1B1 */
    uint8_t name_len = root[0x1B0];
    if (name_len > 30) name_len = 30;
    memcpy(info->disk_name, &root[0x1B1], name_len);
    info->disk_name[name_len] = '\0';
    
    return true;
}

/* ============================================================================
 * Conversion
 * ============================================================================ */

bool uft_image_convert(uft_image_t *src, const char *dest_filename,
                        uft_image_type_t dest_type) {
    if (!src || !dest_filename) return false;
    
    /* Only support sector-based conversions for now */
    if (!(src->caps & UFT_IMG_CAP_READ)) return false;
    
    uft_image_t *dest = uft_image_create(dest_filename, dest_type, &src->geometry);
    if (!dest) return false;
    
    bool success = true;
    uft_track_t track = {0};
    
    for (uint8_t t = 0; t < src->geometry.cylinders && success; t++) {
        for (uint8_t h = 0; h < src->geometry.heads && success; h++) {
            if (uft_image_read_track(src, t, h, &track)) {
                success = uft_image_write_track(dest, t, h, &track);
                uft_track_free(&track);
            }
        }
    }
    
    uft_image_close(dest);
    return success;
}
