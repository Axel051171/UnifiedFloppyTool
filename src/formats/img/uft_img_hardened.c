/**
 * @file uft_img_hardened.c
 * @brief PC/DOS IMG Format - HARDENED VERSION
 * 
 * AUDIT FIX: P1_BUG_STATUS.md - IMG Hardened Parser
 * 
 * SECURITY:
 * - All malloc NULL-checked
 * - All fread/fseek return-checked
 * - Size validation for all variants
 * - BPB validation
 */

#include "uft/core/uft_safe_math.h"
#include "uft/uft_format_common.h"
#include "uft/uft_safe.h"
#include "uft/core/uft_platform.h"
#include <string.h>
#include <stdlib.h>

// ============================================================================
// CONSTANTS
// ============================================================================

#define IMG_SECTOR_SIZE     512

// Known disk sizes
typedef struct {
    size_t      size;
    uint8_t     tracks;
    uint8_t     heads;
    uint8_t     sectors;
    const char* name;
} img_geometry_t;

static const img_geometry_t img_geometries[] = {
    { 163840,   40, 1,  8, "5.25\" SS/DD 160K" },
    { 184320,   40, 1,  9, "5.25\" SS/DD 180K" },
    { 327680,   40, 2,  8, "5.25\" DS/DD 320K" },
    { 368640,   40, 2,  9, "5.25\" DS/DD 360K" },
    { 737280,   80, 2,  9, "3.5\" DD 720K" },
    { 1228800,  80, 2, 15, "5.25\" HD 1.2M" },
    { 1474560,  80, 2, 18, "3.5\" HD 1.44M" },
    { 2949120,  80, 2, 36, "3.5\" ED 2.88M" },
    { 0, 0, 0, 0, NULL }
};

// ============================================================================
// INTERNAL STRUCTURES
// ============================================================================

typedef struct {
    FILE*       file;
    size_t      file_size;
    uint8_t     tracks;
    uint8_t     heads;
    uint8_t     sectors;
    bool        read_only;
    bool        has_bpb;
    uint8_t     media_type;
} img_data_t;

// ============================================================================
// BPB VALIDATION
// ============================================================================

typedef struct {
    uint8_t     jmp[3];
    uint8_t     oem[8];
    uint16_t    bytes_per_sector;
    uint8_t     sectors_per_cluster;
    uint16_t    reserved_sectors;
    uint8_t     fat_count;
    uint16_t    root_entries;
    uint16_t    total_sectors;
    uint8_t     media_type;
    uint16_t    sectors_per_fat;
    uint16_t    sectors_per_track;
    uint16_t    heads;
} __attribute__((packed)) bpb_t;

static bool img_validate_bpb(const uint8_t* data, size_t size, img_data_t* priv) {
    if (size < 62) return false;
    
    const bpb_t* bpb = (const bpb_t*)data;
    
    // Check jump instruction
    if (data[0] != 0xEB && data[0] != 0xE9) return false;
    
    // Check bytes per sector
    uint16_t bps = uft_read_le16((uint8_t*)&bpb->bytes_per_sector);
    if (bps != 512 && bps != 1024 && bps != 2048 && bps != 4096) return false;
    
    // Check media type
    uint8_t mt = bpb->media_type;
    if (mt < 0xF0) return false;
    
    // Get geometry from BPB
    uint16_t spt = uft_read_le16((uint8_t*)&bpb->sectors_per_track);
    uint16_t heads = uft_read_le16((uint8_t*)&bpb->heads);
    
    if (spt == 0 || spt > 63 || heads == 0 || heads > 2) return false;
    
    priv->sectors = (uint8_t)spt;
    priv->heads = (uint8_t)heads;
    priv->media_type = mt;
    priv->has_bpb = true;
    
    return true;
}

// ============================================================================
// DETECTION
// ============================================================================

static bool img_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    *confidence = 0;
    
    // Check known sizes
    for (int i = 0; img_geometries[i].size != 0; i++) {
        if (file_size == img_geometries[i].size) {
            *confidence = 60;
            break;
        }
    }
    
    if (*confidence == 0) {
        // Check if divisible by sector size
        if (file_size % IMG_SECTOR_SIZE != 0) return false;
        *confidence = 30;
    }
    
    // Try BPB validation
    if (size >= 62) {
        img_data_t temp = {0};
        if (img_validate_bpb(data, size, &temp)) {
            *confidence = 85;
        }
    }
    
    return *confidence > 0;
}

// ============================================================================
// OPEN
// ============================================================================

static uft_error_t img_open(uft_disk_t* disk, const char* path, bool read_only) {
    UFT_REQUIRE_NOT_NULL2(disk, path);
    
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    // Get file size
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return UFT_ERROR_FILE_SEEK;
    }
    
    long pos = ftell(f);
    if (pos < 0) {
        fclose(f);
        return UFT_ERROR_FILE_SEEK;
    }
    
    size_t file_size = (size_t)pos;
    
    if (file_size < IMG_SECTOR_SIZE) {
        fclose(f);
        return UFT_ERROR_FORMAT;
    }
    
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return UFT_ERROR_FILE_SEEK;
    }
    
    // Allocate private data
    img_data_t* priv = calloc(1, sizeof(img_data_t));
    if (!priv) {
        fclose(f);
        return UFT_ERROR_MEMORY;
    }
    
    priv->file = f;
    priv->file_size = file_size;
    priv->read_only = read_only;
    
    // Read first sector for BPB
    uint8_t boot[IMG_SECTOR_SIZE];
    if (fread(boot, 1, IMG_SECTOR_SIZE, f) != IMG_SECTOR_SIZE) {
        free(priv);
        fclose(f);
        return UFT_ERROR_FILE_READ;
    }
    
    // Try BPB first
    if (!img_validate_bpb(boot, IMG_SECTOR_SIZE, priv)) {
        // Fall back to size detection
        bool found = false;
        for (int i = 0; img_geometries[i].size != 0; i++) {
            if (file_size == img_geometries[i].size) {
                priv->tracks = img_geometries[i].tracks;
                priv->heads = img_geometries[i].heads;
                priv->sectors = img_geometries[i].sectors;
                found = true;
                break;
            }
        }
        
        if (!found) {
            // Guess geometry
            size_t total_sectors = file_size / IMG_SECTOR_SIZE;
            priv->sectors = 18;
            priv->heads = 2;
            priv->tracks = (uint8_t)(total_sectors / (priv->sectors * priv->heads));
            if (priv->tracks == 0) priv->tracks = 80;
        }
    } else {
        // Calculate tracks from BPB
        size_t total_sectors = file_size / IMG_SECTOR_SIZE;
        priv->tracks = (uint8_t)(total_sectors / ((size_t)priv->sectors * priv->heads));
    }
    
    // Set disk fields
    disk->private_data = priv;
    disk->format = UFT_FORMAT_IMG;
    disk->geometry.cylinders = priv->tracks;
    disk->geometry.heads = priv->heads;
    disk->geometry.sectors_per_track = priv->sectors;
    disk->geometry.sector_size = IMG_SECTOR_SIZE;
    disk->read_only = read_only;
    
    return UFT_OK;
}

// ============================================================================
// READ TRACK
// ============================================================================

static uft_error_t img_read_track(uft_disk_t* disk, int cyl, int head,
                                   uft_track_t* track, uft_track_read_options_t* opts) {
    UFT_REQUIRE_NOT_NULL2(disk, track);
    UFT_REQUIRE_NOT_NULL(disk->private_data);
    (void)opts;
    
    img_data_t* priv = (img_data_t*)disk->private_data;
    
    // Bounds check
    if (cyl < 0 || cyl >= priv->tracks) return UFT_ERROR_BOUNDS;
    if (head < 0 || head >= priv->heads) return UFT_ERROR_BOUNDS;
    
    // Calculate offset
    size_t track_offset = (size_t)(cyl * priv->heads + head) * priv->sectors * IMG_SECTOR_SIZE;
    
    // Bounds check on file
    size_t track_size = (size_t)priv->sectors * IMG_SECTOR_SIZE;
    if (track_offset + track_size > priv->file_size) {
        return UFT_ERROR_BOUNDS;
    }
    
    // Seek
    if (fseek(priv->file, (long)track_offset, SEEK_SET) != 0) {
        return UFT_ERROR_FILE_SEEK;
    }
    
    // Allocate sectors
    track->sectors = calloc((size_t)priv->sectors, sizeof(uft_sector_t));
    if (!track->sectors) {
        return UFT_ERROR_MEMORY;
    }
    
    track->sector_count = priv->sectors;
    track->cylinder = cyl;
    track->head = head;
    
    // Read sectors
    for (int s = 0; s < priv->sectors; s++) {
        track->sectors[s].data = malloc(IMG_SECTOR_SIZE);
        if (!track->sectors[s].data) {
            for (int j = 0; j < s; j++) free(track->sectors[j].data);
            free(track->sectors);
            track->sectors = NULL;
            track->sector_count = 0;
            return UFT_ERROR_MEMORY;
        }
        
        if (fread(track->sectors[s].data, 1, IMG_SECTOR_SIZE, priv->file) != IMG_SECTOR_SIZE) {
            for (int j = 0; j <= s; j++) free(track->sectors[j].data);
            free(track->sectors);
            track->sectors = NULL;
            track->sector_count = 0;
            return UFT_ERROR_FILE_READ;
        }
        
        track->sectors[s].size = IMG_SECTOR_SIZE;
        track->sectors[s].id.cylinder = cyl;
        track->sectors[s].id.head = head;
        track->sectors[s].id.sector = s + 1;  // 1-based
        track->sectors[s].id.size_code = 2;
        track->sectors[s].status = UFT_SECTOR_OK;
    }
    
    return UFT_OK;
}

// ============================================================================
// WRITE TRACK
// ============================================================================

static uft_error_t img_write_track(uft_disk_t* disk, const uft_track_t* track,
                                    uft_track_write_options_t* opts) {
    UFT_REQUIRE_NOT_NULL2(disk, track);
    UFT_REQUIRE_NOT_NULL(disk->private_data);
    (void)opts;
    
    img_data_t* priv = (img_data_t*)disk->private_data;
    
    if (priv->read_only) return UFT_ERROR_READ_ONLY;
    
    if (track->cylinder < 0 || track->cylinder >= priv->tracks) return UFT_ERROR_BOUNDS;
    if (track->head < 0 || track->head >= priv->heads) return UFT_ERROR_BOUNDS;
    if (track->sector_count != priv->sectors) return UFT_ERROR_FORMAT;
    
    size_t track_offset = (size_t)(track->cylinder * priv->heads + track->head) * 
                          priv->sectors * IMG_SECTOR_SIZE;
    
    if (fseek(priv->file, (long)track_offset, SEEK_SET) != 0) {
        return UFT_ERROR_FILE_SEEK;
    }
    
    for (int s = 0; s < track->sector_count; s++) {
        if (!track->sectors[s].data || track->sectors[s].size != IMG_SECTOR_SIZE) {
            return UFT_ERROR_FORMAT;
        }
        if (fwrite(track->sectors[s].data, 1, IMG_SECTOR_SIZE, priv->file) != IMG_SECTOR_SIZE) {
            return UFT_ERROR_FILE_WRITE;
        }
    }
    
    fflush(priv->file);
    return UFT_OK;
}

// ============================================================================
// CLOSE
// ============================================================================

static void img_close(uft_disk_t* disk) {
    if (!disk || !disk->private_data) return;
    
    img_data_t* priv = (img_data_t*)disk->private_data;
    if (priv->file) fclose(priv->file);
    free(priv);
    disk->private_data = NULL;
}

// ============================================================================
// PLUGIN
// ============================================================================

static const uft_format_plugin_t img_hardened_plugin = {
    .name = "IMG (Hardened)",
    .format = UFT_FORMAT_IMG,
    .extensions = "img,ima,dsk,vfd",
    .probe = img_probe,
    .open = img_open,
    .close = img_close,
    .read_track = img_read_track,
    .write_track = img_write_track,
    .create = NULL,
    .get_info = NULL,
    .capabilities = UFT_CAP_READ | UFT_CAP_WRITE
};

const uft_format_plugin_t* uft_img_hardened_get_plugin(void) {
    return &img_hardened_plugin;
}
