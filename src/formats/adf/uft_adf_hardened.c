/**
 * @file uft_adf_hardened.c
 * @brief Amiga ADF Format - HARDENED VERSION
 * 
 * AUDIT FIX: P1_BUG_STATUS.md - ADF Hardened Parser
 * 
 * SECURITY:
 * - All malloc NULL-checked
 * - All fread/fseek return-checked
 * - Bounds validation on all sector access
 * - Checksum verification
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

#define ADF_SECTOR_SIZE     512
#define ADF_SECTORS_TRACK   11
#define ADF_HEADS           2
#define ADF_TRACKS_DD       80
#define ADF_TRACKS_HD       160

#define ADF_SIZE_DD         (ADF_SECTOR_SIZE * ADF_SECTORS_TRACK * ADF_HEADS * ADF_TRACKS_DD)  // 901120
#define ADF_SIZE_HD         (ADF_SIZE_DD * 2)  // 1802240

#define ADF_BOOTBLOCK_SIZE  1024
#define ADF_ROOTBLOCK_SECTOR  880

// Filesystem types
#define ADF_FS_OFS          0
#define ADF_FS_FFS          1
#define ADF_FS_INTL         2
#define ADF_FS_DIRCACHE     4

// ============================================================================
// INTERNAL STRUCTURES
// ============================================================================

typedef struct {
    FILE*       file;
    size_t      file_size;
    uint8_t     tracks;
    uint8_t     heads;
    uint8_t     fs_type;
    bool        is_hd;
    bool        read_only;
    uint32_t    rootblock_checksum;
    bool        checksum_valid;
} adf_data_t;

// ============================================================================
// CHECKSUM CALCULATION
// ============================================================================

static uint32_t adf_bootblock_checksum(const uint8_t* data, size_t size) {
    if (size < 4) return 0;
    
    uint32_t sum = 0;
    for (size_t i = 0; i < size; i += 4) {
        if (i == 4) continue;  // Skip checksum field
        uint32_t val = uft_read_be32(data + i);
        uint32_t old = sum;
        sum += val;
        if (sum < old) sum++;  // Carry
    }
    return ~sum;
}

static uint32_t adf_block_checksum(const uint8_t* data) {
    uint32_t sum = 0;
    for (int i = 0; i < 128; i++) {
        if (i == 5) continue;  // Skip checksum at offset 20
        uint32_t val = uft_read_be32(data + i * 4);
        sum += val;
    }
    return -sum;
}

// ============================================================================
// DETECTION
// ============================================================================

static bool adf_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    *confidence = 0;
    
    // Size check
    if (file_size != ADF_SIZE_DD && file_size != ADF_SIZE_HD) {
        return false;
    }
    
    *confidence = 60;  // Size match
    
    // Need at least bootblock for validation
    if (size < ADF_BOOTBLOCK_SIZE) {
        return true;
    }
    
    // Check bootblock type
    const uint8_t* boot = data;
    if (boot[0] == 'D' && boot[1] == 'O' && boot[2] == 'S') {
        *confidence = 85;
        
        // Verify bootblock checksum
        uint32_t stored = uft_read_be32(boot + 4);
        uint32_t calc = adf_bootblock_checksum(boot, ADF_BOOTBLOCK_SIZE);
        if (stored == calc) {
            *confidence = 95;
        }
        
        // Check filesystem type
        uint8_t fs = boot[3];
        if (fs <= 7) {
            *confidence += 2;
        }
    }
    
    return true;
}

// ============================================================================
// OPEN
// ============================================================================

static uft_error_t adf_open(uft_disk_t* disk, const char* path, bool read_only) {
    UFT_REQUIRE_NOT_NULL2(disk, path);
    
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    // Get file size safely
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
    
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return UFT_ERROR_FILE_SEEK;
    }
    
    // Validate size
    bool is_hd = false;
    uint8_t tracks = ADF_TRACKS_DD;
    
    if (file_size == ADF_SIZE_DD) {
        is_hd = false;
        tracks = ADF_TRACKS_DD;
    } else if (file_size == ADF_SIZE_HD) {
        is_hd = true;
        tracks = ADF_TRACKS_HD;
    } else {
        fclose(f);
        return UFT_ERROR_FORMAT;
    }
    
    // Allocate private data
    adf_data_t* priv = calloc(1, sizeof(adf_data_t));
    if (!priv) {
        fclose(f);
        return UFT_ERROR_MEMORY;
    }
    
    priv->file = f;
    priv->file_size = file_size;
    priv->tracks = tracks;
    priv->heads = ADF_HEADS;
    priv->is_hd = is_hd;
    priv->read_only = read_only;
    
    // Read bootblock to detect filesystem
    uint8_t bootblock[ADF_BOOTBLOCK_SIZE];
    if (fread(bootblock, 1, ADF_BOOTBLOCK_SIZE, f) != ADF_BOOTBLOCK_SIZE) {
        free(priv);
        fclose(f);
        return UFT_ERROR_FILE_READ;
    }
    
    if (bootblock[0] == 'D' && bootblock[1] == 'O' && bootblock[2] == 'S') {
        priv->fs_type = bootblock[3] & 0x07;
        
        // Verify checksum
        uint32_t stored = uft_read_be32(bootblock + 4);
        uint32_t calc = adf_bootblock_checksum(bootblock, ADF_BOOTBLOCK_SIZE);
        priv->checksum_valid = (stored == calc);
    }
    
    // Set disk fields
    disk->private_data = priv;
    disk->format = UFT_FORMAT_ADF;
    disk->geometry.cylinders = tracks;
    disk->geometry.heads = ADF_HEADS;
    disk->geometry.sectors_per_track = is_hd ? 22 : ADF_SECTORS_TRACK;
    disk->geometry.sector_size = ADF_SECTOR_SIZE;
    disk->read_only = read_only;
    
    return UFT_OK;
}

// ============================================================================
// READ TRACK
// ============================================================================

static uft_error_t adf_read_track(uft_disk_t* disk, int cyl, int head,
                                   uft_track_t* track, uft_track_read_options_t* opts) {
    UFT_REQUIRE_NOT_NULL2(disk, track);
    UFT_REQUIRE_NOT_NULL(disk->private_data);
    (void)opts;
    
    adf_data_t* priv = (adf_data_t*)disk->private_data;
    
    // Bounds check
    if (cyl < 0 || cyl >= priv->tracks) return UFT_ERROR_BOUNDS;
    if (head < 0 || head >= priv->heads) return UFT_ERROR_BOUNDS;
    
    int sectors = priv->is_hd ? 22 : ADF_SECTORS_TRACK;
    
    // Calculate offset
    size_t track_offset = (size_t)(cyl * priv->heads + head) * sectors * ADF_SECTOR_SIZE;
    
    // Bounds check on file
    if (track_offset + (size_t)(sectors * ADF_SECTOR_SIZE) > priv->file_size) {
        return UFT_ERROR_BOUNDS;
    }
    
    // Seek
    if (fseek(priv->file, (long)track_offset, SEEK_SET) != 0) {
        return UFT_ERROR_FILE_SEEK;
    }
    
    // Allocate sectors
    track->sectors = calloc((size_t)sectors, sizeof(uft_sector_t));
    if (!track->sectors) {
        return UFT_ERROR_MEMORY;
    }
    
    track->sector_count = sectors;
    track->cylinder = cyl;
    track->head = head;
    
    // Read sectors
    for (int s = 0; s < sectors; s++) {
        track->sectors[s].data = malloc(ADF_SECTOR_SIZE);
        if (!track->sectors[s].data) {
            // Cleanup
            for (int j = 0; j < s; j++) {
                free(track->sectors[j].data);
            }
            free(track->sectors);
            track->sectors = NULL;
            track->sector_count = 0;
            return UFT_ERROR_MEMORY;
        }
        
        if (fread(track->sectors[s].data, 1, ADF_SECTOR_SIZE, priv->file) != ADF_SECTOR_SIZE) {
            // Cleanup
            for (int j = 0; j <= s; j++) {
                free(track->sectors[j].data);
            }
            free(track->sectors);
            track->sectors = NULL;
            track->sector_count = 0;
            return UFT_ERROR_FILE_READ;
        }
        
        track->sectors[s].size = ADF_SECTOR_SIZE;
        track->sectors[s].id.cylinder = cyl;
        track->sectors[s].id.head = head;
        track->sectors[s].id.sector = s;
        track->sectors[s].id.size_code = 2;  // 512 bytes
        track->sectors[s].status = UFT_SECTOR_OK;
    }
    
    return UFT_OK;
}

// ============================================================================
// WRITE TRACK
// ============================================================================

static uft_error_t adf_write_track(uft_disk_t* disk, const uft_track_t* track,
                                    uft_track_write_options_t* opts) {
    UFT_REQUIRE_NOT_NULL2(disk, track);
    UFT_REQUIRE_NOT_NULL(disk->private_data);
    (void)opts;
    
    adf_data_t* priv = (adf_data_t*)disk->private_data;
    
    if (priv->read_only) return UFT_ERROR_READ_ONLY;
    
    // Bounds check
    if (track->cylinder < 0 || track->cylinder >= priv->tracks) return UFT_ERROR_BOUNDS;
    if (track->head < 0 || track->head >= priv->heads) return UFT_ERROR_BOUNDS;
    
    int expected_sectors = priv->is_hd ? 22 : ADF_SECTORS_TRACK;
    if (track->sector_count != expected_sectors) return UFT_ERROR_FORMAT;
    
    // Calculate offset
    size_t track_offset = (size_t)(track->cylinder * priv->heads + track->head) * 
                          expected_sectors * ADF_SECTOR_SIZE;
    
    // Seek
    if (fseek(priv->file, (long)track_offset, SEEK_SET) != 0) {
        return UFT_ERROR_FILE_SEEK;
    }
    
    // Write sectors
    for (int s = 0; s < track->sector_count; s++) {
        if (!track->sectors[s].data || track->sectors[s].size != ADF_SECTOR_SIZE) {
            return UFT_ERROR_FORMAT;
        }
        
        if (fwrite(track->sectors[s].data, 1, ADF_SECTOR_SIZE, priv->file) != ADF_SECTOR_SIZE) {
            return UFT_ERROR_FILE_WRITE;
        }
    }
    
    fflush(priv->file);
    return UFT_OK;
}

// ============================================================================
// CLOSE
// ============================================================================

static void adf_close(uft_disk_t* disk) {
    if (!disk || !disk->private_data) return;
    
    adf_data_t* priv = (adf_data_t*)disk->private_data;
    
    if (priv->file) {
        fclose(priv->file);
    }
    
    free(priv);
    disk->private_data = NULL;
}

// ============================================================================
// PLUGIN REGISTRATION
// ============================================================================

static const uft_format_plugin_t adf_hardened_plugin = {
    .name = "ADF (Hardened)",
    .format = UFT_FORMAT_ADF,
    .extensions = "adf,adz",
    .probe = adf_probe,
    .open = adf_open,
    .close = adf_close,
    .read_track = adf_read_track,
    .write_track = adf_write_track,
    .create = NULL,  // TODO
    .get_info = NULL,
    .capabilities = UFT_CAP_READ | UFT_CAP_WRITE
};

const uft_format_plugin_t* uft_adf_hardened_get_plugin(void) {
    return &adf_hardened_plugin;
}
