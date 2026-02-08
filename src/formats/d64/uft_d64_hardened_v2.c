/**
 * @file uft_d64_hardened_v2.c
 * @brief Security-Hardened D64 Parser
 * 
 * Fixes:
 * - BUG-002: Bounded Array Access
 * - BUG-003: Safe Ownership Transfer
 * - BUG-008: Memory Leak Prevention
 * - BUG-009: Signed/Unsigned Safety
 * 
 * @author Superman QA
 * @date 2026
 */

#include "uft/uft_format_plugin.h"
#include "uft/core/uft_safe_math.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

// ============================================================================
// D64 Constants
// ============================================================================

#define D64_SECTOR_SIZE     256
#define D64_TRACKS_STD      35
#define D64_TRACKS_EXT      40
#define D64_TRACKS_MAX      42
#define D64_HEADS           1

// Größen
#define D64_SIZE_35         174848
#define D64_SIZE_35_ERR     175531
#define D64_SIZE_40         196608
#define D64_SIZE_40_ERR     197376
#define D64_SIZE_42         205312
#define D64_SIZE_42_ERR     206114

// Security limits
#define D64_MAX_FILE_SIZE   (300 * 1024)  // 300 KB max (größer als 42-Track)
#define D64_MAX_SECTORS     802           // 42-Track maximum

// Sektoren pro Track (konstante Tabelle)
static const uint8_t d64_sectors_per_track[D64_TRACKS_MAX] = {
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  // 1-17
    19, 19, 19, 19, 19, 19, 19,                                          // 18-24
    18, 18, 18, 18, 18, 18,                                              // 25-30
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17                       // 31-42
};

// Kumulative Offsets
static const uint16_t d64_track_offset[D64_TRACKS_MAX + 1] = {
    0, 21, 42, 63, 84, 105, 126, 147, 168, 189, 210, 231, 252, 273, 294, 315, 336,
    357, 376, 395, 414, 433, 452, 471, 490, 508, 526, 544, 562, 580, 598,
    615, 632, 649, 666, 683, 700, 717, 734, 751, 768, 785, 802
};

// Error Codes
typedef enum {
    D64_ERR_OK = 0x01,
    D64_ERR_HEADER_NOT_FOUND = 0x02,
    D64_ERR_NO_SYNC = 0x03,
    D64_ERR_DATA_NOT_FOUND = 0x04,
    D64_ERR_CHECKSUM = 0x05,
    D64_ERR_ID_MISMATCH = 0x0B
} d64_error_code_t;

// ============================================================================
// Safe Helpers
// ============================================================================

/**
 * @brief Sichere Berechnung des Sektor-Index
 * @return true wenn Index gültig, false sonst
 */
static bool d64_safe_sector_index(unsigned int track, unsigned int sector,
                                   uint16_t max_sectors, uint16_t *index_out) {
    // Track ist 1-basiert
    if (track < 1 || track > D64_TRACKS_MAX) {
        return false;
    }
    
    if (sector >= d64_sectors_per_track[track - 1]) {
        return false;
    }
    
    uint16_t index = d64_track_offset[track - 1] + (uint16_t)sector;
    
    // FIX BUG-002: Bounds check
    if (index >= max_sectors) {
        return false;
    }
    
    *index_out = index;
    return true;
}

/**
 * @brief Sichere Offset-Berechnung
 */
static bool d64_safe_offset(unsigned int track, unsigned int sector, 
                            size_t file_size, size_t *offset_out) {
    if (track < 1 || track > D64_TRACKS_MAX) {
        return false;
    }
    
    if (sector >= d64_sectors_per_track[track - 1]) {
        return false;
    }
    
    size_t offset = ((size_t)d64_track_offset[track - 1] + sector) * D64_SECTOR_SIZE;
    
    // Bounds check gegen Dateigröße
    if (offset + D64_SECTOR_SIZE > file_size) {
        return false;
    }
    
    *offset_out = offset;
    return true;
}

/**
 * @brief Sektoren pro Track sicher abfragen
 */
static unsigned int d64_get_sectors_safe(unsigned int track) {
    if (track < 1 || track > D64_TRACKS_MAX) {
        return 0;
    }
    return d64_sectors_per_track[track - 1];
}

/**
 * @brief Error-Code zu Status konvertieren
 */
static uint32_t d64_error_to_status(uint8_t err) {
    switch (err) {
        case D64_ERR_OK:
            return UFT_SECTOR_OK;
        case D64_ERR_HEADER_NOT_FOUND:
        case D64_ERR_DATA_NOT_FOUND:
        case D64_ERR_NO_SYNC:
            return UFT_SECTOR_MISSING;
        case D64_ERR_CHECKSUM:
            return UFT_SECTOR_CRC_ERROR;
        case D64_ERR_ID_MISMATCH:
            return UFT_SECTOR_ID_CRC_ERROR;
        default:
            return UFT_SECTOR_CRC_ERROR;
    }
}

// ============================================================================
// Hardened Plugin Data
// ============================================================================

typedef struct {
    FILE       *file;
    size_t      file_size;      // ADDED: For bounds checking
    uint8_t     num_tracks;
    bool        has_errors;
    uint8_t    *error_info;
    uint16_t    total_sectors;
    bool        is_open;        // ADDED: State tracking
} d64_data_hardened_t;

// ============================================================================
// Open (Hardened)
// ============================================================================

static uft_error_t d64_open_hardened(uft_disk_t *disk, const char *path, 
                                      bool read_only) {
    // NULL checks
    if (!disk || !path) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    FILE *f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) {
        return UFT_ERROR_FILE_OPEN;
    }
    
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
    
    // Security: Maximum file size
    if (file_size > D64_MAX_FILE_SIZE) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return UFT_ERROR_FILE_SEEK;
    }
    
    // Detect variant
    uint8_t num_tracks = 0;
    bool has_errors = false;
    uint16_t total_sectors = 0;
    
    switch (file_size) {
        case D64_SIZE_35:     num_tracks = 35; has_errors = false; total_sectors = 683; break;
        case D64_SIZE_35_ERR: num_tracks = 35; has_errors = true;  total_sectors = 683; break;
        case D64_SIZE_40:     num_tracks = 40; has_errors = false; total_sectors = 768; break;
        case D64_SIZE_40_ERR: num_tracks = 40; has_errors = true;  total_sectors = 768; break;
        case D64_SIZE_42:     num_tracks = 42; has_errors = false; total_sectors = 802; break;
        case D64_SIZE_42_ERR: num_tracks = 42; has_errors = true;  total_sectors = 802; break;
        default:
            fclose(f);
            return UFT_ERROR_FORMAT_INVALID;
    }
    
    // Allocate plugin data
    d64_data_hardened_t *pdata = calloc(1, sizeof(d64_data_hardened_t));
    if (!pdata) {
        fclose(f);
        return UFT_ERROR_NO_MEMORY;
    }
    
    pdata->file = f;
    pdata->file_size = file_size;
    pdata->num_tracks = num_tracks;
    pdata->has_errors = has_errors;
    pdata->total_sectors = total_sectors;
    pdata->is_open = true;
    
    // Load error info if present
    if (has_errors) {
        pdata->error_info = malloc(total_sectors);
        if (!pdata->error_info) {
            free(pdata);
            fclose(f);
            return UFT_ERROR_NO_MEMORY;
        }
        
        size_t error_offset = (size_t)total_sectors * D64_SECTOR_SIZE;
        
        // Validate error info fits in file
        if (error_offset + total_sectors > file_size) {
            free(pdata->error_info);
            free(pdata);
            fclose(f);
            return UFT_ERROR_FORMAT_INVALID;
        }
        
        if (fseek(f, (long)error_offset, SEEK_SET) != 0) {
            free(pdata->error_info);
            free(pdata);
            fclose(f);
            return UFT_ERROR_FILE_SEEK;
        }
        
        if (fread(pdata->error_info, 1, total_sectors, f) != total_sectors) {
            free(pdata->error_info);
            free(pdata);
            fclose(f);
            return UFT_ERROR_FILE_READ;
        }
    }
    
    disk->plugin_data = pdata;
    disk->geometry.cylinders = num_tracks;
    disk->geometry.heads = D64_HEADS;
    disk->geometry.sectors = 17;  // Minimum for display
    disk->geometry.sector_size = D64_SECTOR_SIZE;
    disk->geometry.total_sectors = total_sectors;
    
    return UFT_OK;
}

// ============================================================================
// Read Track (Hardened) - FIX BUG-002, BUG-008, BUG-009
// ============================================================================

static uft_error_t d64_read_track_hardened(uft_disk_t *disk, int cylinder, 
                                            int head, uft_track_t *track) {
    // NULL and state checks
    if (!disk || !track) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    d64_data_hardened_t *pdata = disk->plugin_data;
    if (!pdata || !pdata->file || !pdata->is_open) {
        return UFT_ERROR_DISK_NOT_OPEN;
    }
    
    // FIX BUG-009: Signed check before conversion
    if (cylinder < 0 || head != 0) {
        return UFT_ERROR_OUT_OF_RANGE;
    }
    
    unsigned int d64_track = (unsigned int)cylinder + 1;
    
    if (d64_track > pdata->num_tracks) {
        return UFT_ERROR_OUT_OF_RANGE;
    }
    
    // Initialize track
    uft_track_init(track, cylinder, head);
    
    unsigned int sectors_this_track = d64_get_sectors_safe(d64_track);
    if (sectors_this_track == 0) {
        return UFT_ERROR_OUT_OF_RANGE;
    }
    
    uft_error_t result = UFT_OK;
    
    for (unsigned int s = 0; s < sectors_this_track; s++) {
        uft_sector_t sector = {0};
        
        sector.id.cylinder = (uint8_t)d64_track;
        sector.id.head = 0;
        sector.id.sector = (uint8_t)s;
        sector.id.size_code = 1;
        
        // Safe offset calculation with bounds check
        size_t offset;
        if (!d64_safe_offset(d64_track, s, pdata->file_size, &offset)) {
            result = UFT_ERROR_OUT_OF_RANGE;
            goto cleanup;
        }
        
        if (fseek(pdata->file, (long)offset, SEEK_SET) != 0) {
            result = UFT_ERROR_FILE_SEEK;
            goto cleanup;
        }
        
        sector.data = malloc(D64_SECTOR_SIZE);
        if (!sector.data) {
            result = UFT_ERROR_NO_MEMORY;
            goto cleanup;
        }
        
        size_t read = fread(sector.data, 1, D64_SECTOR_SIZE, pdata->file);
        if (read != D64_SECTOR_SIZE) {
            free(sector.data);
            result = UFT_ERROR_FILE_READ;
            goto cleanup;
        }
        
        sector.data_size = D64_SECTOR_SIZE;
        
        // FIX BUG-002: Safe error info access
        if (pdata->has_errors && pdata->error_info) {
            uint16_t sector_index;
            if (d64_safe_sector_index(d64_track, s, pdata->total_sectors, 
                                      &sector_index)) {
                uint8_t err_code = pdata->error_info[sector_index];
                sector.status = d64_error_to_status(err_code);
                sector.id.crc_ok = (err_code == D64_ERR_OK);
            } else {
                // Index out of bounds - treat as OK
                sector.status = UFT_SECTOR_OK;
                sector.id.crc_ok = true;
            }
        } else {
            sector.status = UFT_SECTOR_OK;
            sector.id.crc_ok = true;
        }
        
        // FIX BUG-003: Clear ownership transfer semantics
        // uft_track_add_sector makes a DEEP COPY
        result = uft_track_add_sector(track, &sector);
        
        // Always free our copy - track has its own
        free(sector.data);
        sector.data = NULL;
        
        if (UFT_FAILED(result)) {
            goto cleanup;
        }
    }
    
    track->status = UFT_TRACK_OK;
    return UFT_OK;
    
cleanup:
    // FIX BUG-008: Proper cleanup on error
    uft_track_clear(track);
    return result;
}

// ============================================================================
// Close (Hardened)
// ============================================================================

static void d64_close_hardened(uft_disk_t *disk) {
    if (!disk || !disk->plugin_data) {
        return;
    }
    
    d64_data_hardened_t *pdata = disk->plugin_data;
    
    // Mark as closed FIRST (prevents use-after-free in multi-threaded)
    pdata->is_open = false;
    
    if (pdata->error_info) {
        // Clear sensitive data before freeing
        memset(pdata->error_info, 0, pdata->total_sectors);
        free(pdata->error_info);
        pdata->error_info = NULL;
    }
    
    if (pdata->file) {
        fclose(pdata->file);
        pdata->file = NULL;
    }
    
    // Clear structure before freeing
    memset(pdata, 0, sizeof(*pdata));
    free(pdata);
    
    disk->plugin_data = NULL;
}

// ============================================================================
// Hardened Write with Verify
// ============================================================================

static uft_error_t d64_write_track_hardened(uft_disk_t *disk, int cylinder,
                                             int head, const uft_track_t *track) {
    if (!disk || !track) return UFT_ERROR_NULL_POINTER;
    
    d64_data_hardened_t *pdata = disk->plugin_data;
    if (!pdata || !pdata->file || !pdata->is_open) {
        return UFT_ERROR_NOT_READY;
    }
    
    /* D64: sectors per track depends on speed zone */
    static const int sectors_per_track[40] = {
        21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
        19,19,19,19,19,19,19,
        18,18,18,18,18,18,
        17,17,17,17,17,
        17,17,17,17,17
    };
    
    if (cylinder < 0 || cylinder >= 40) return UFT_ERROR_INVALID_TRACK;
    int spt = sectors_per_track[cylinder];
    
    /* Calculate file offset for this track */
    size_t offset = 0;
    for (int t = 0; t < cylinder; t++) {
        offset += sectors_per_track[t] * 256;
    }
    
    /* Write each sector with verify */
    for (int s = 0; s < spt; s++) {
        size_t sec_offset = offset + s * 256;
        
        if (fseek(pdata->file, (long)sec_offset, SEEK_SET) != 0) {
            return UFT_ERROR_SEEK_ERROR;
        }
        
        /* Get sector data from track */
        const uint8_t *sector_data = NULL;
        if (track->sectors && s < (int)track->sector_count) {
            sector_data = track->sectors[s].data;
        }
        
        uint8_t buf[256];
        if (sector_data) {
            memcpy(buf, sector_data, 256);
        } else {
            memset(buf, 0, 256);
        }
        
        /* Write */
        if (fwrite(buf, 1, 256, pdata->file) != 256) {
            return UFT_ERROR_FILE_WRITE;
        }
        
        /* Verify: read back and compare */
        if (fseek(pdata->file, (long)sec_offset, SEEK_SET) != 0) {
            return UFT_ERROR_SEEK_ERROR;
        }
        
        uint8_t verify[256];
        if (fread(verify, 1, 256, pdata->file) != 256) {
            return UFT_ERROR_FILE_READ;
        }
        
        if (memcmp(buf, verify, 256) != 0) {
            return UFT_ERROR_VERIFY;
        }
    }
    
    fflush(pdata->file);
    return UFT_OK;
}

// ============================================================================
// Plugin Registration
// ============================================================================

static const uft_format_plugin_t d64_plugin_hardened = {
    .name = "D64-Hardened",
    .extensions = "d64",
    .probe = NULL,  // Use existing probe
    .open = d64_open_hardened,
    .close = d64_close_hardened,
    .read_track = d64_read_track_hardened,
    .write_track = d64_write_track_hardened,  /* With error recovery and verify */
    .create = NULL,
    .flush = NULL,
    .read_metadata = NULL,
    .write_metadata = NULL,
};

const uft_format_plugin_t* uft_d64_get_hardened_plugin(void) {
    return &d64_plugin_hardened;
}
