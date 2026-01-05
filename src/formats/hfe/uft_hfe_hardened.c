/**
 * @file uft_hfe_hardened.c
 * @brief HxC HFE Format - HARDENED VERSION
 * 
 * AUDIT FIX: P1_BUG_STATUS.md - HFE Hardened Parser
 * 
 * SECURITY:
 * - All malloc NULL-checked
 * - All fread/fseek return-checked
 * - Header validation
 * - Track offset bounds checking
 */

#include "uft/core/uft_safe_math.h"
#include "uft/uft_format_common.h"
#include "uft/uft_safe.h"
#include "uft/core/uft_platform.h"
#include <string.h>
#include <stdlib.h>

// ============================================================================
// HFE HEADER (512 bytes)
// ============================================================================

#define HFE_HEADER_SIZE         512
#define HFE_TRACK_TABLE_OFFSET  512
#define HFE_BLOCK_SIZE          512

#pragma pack(push, 1)
typedef struct {
    uint8_t  signature[8];      // "HXCPICFE"
    uint8_t  format_revision;   // 0
    uint8_t  number_of_tracks;
    uint8_t  number_of_sides;
    uint8_t  track_encoding;    // 0=ISOIBM_MFM, 1=Amiga_MFM, 2=ISOIBM_FM, etc.
    uint16_t bit_rate;          // kbps (LE)
    uint16_t floppy_rpm;        // (LE)
    uint8_t  floppy_interface_mode;
    uint8_t  reserved;
    uint16_t track_list_offset; // Offset to track table in blocks (LE)
    uint8_t  write_allowed;
    uint8_t  single_step;
    uint8_t  track0s0_altencoding;
    uint8_t  track0s0_encoding;
    uint8_t  track0s1_altencoding;
    uint8_t  track0s1_encoding;
} hfe_header_t;

typedef struct {
    uint16_t offset;    // Track data offset in blocks (LE)
    uint16_t length;    // Track length in bytes (LE)
} hfe_track_entry_t;
#pragma pack(pop)

// ============================================================================
// INTERNAL STRUCTURES
// ============================================================================

typedef struct {
    FILE*           file;
    size_t          file_size;
    hfe_header_t    header;
    hfe_track_entry_t* track_table;
    size_t          track_table_size;
    bool            read_only;
} hfe_data_t;

// ============================================================================
// DETECTION
// ============================================================================

static bool hfe_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    (void)file_size;
    *confidence = 0;
    
    if (size < HFE_HEADER_SIZE) return false;
    
    // Check signature
    if (memcmp(data, "HXCPICFE", 8) != 0) {
        // Check for HFE v3
        if (memcmp(data, "HXCHFEV3", 8) == 0) {
            *confidence = 98;
            return true;
        }
        return false;
    }
    
    const hfe_header_t* hdr = (const hfe_header_t*)data;
    
    // Validate fields
    if (hdr->number_of_tracks == 0 || hdr->number_of_tracks > 166) return false;
    if (hdr->number_of_sides == 0 || hdr->number_of_sides > 2) return false;
    
    *confidence = 95;
    return true;
}

// ============================================================================
// OPEN
// ============================================================================

static uft_error_t hfe_open(uft_disk_t* disk, const char* path, bool read_only) {
    UFT_REQUIRE_NOT_NULL2(disk, path);
    
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    // Get file size
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return UFT_ERROR_FILE_SEEK;
    }
    
    long pos = ftell(f);
    if (pos < 0 || pos < HFE_HEADER_SIZE) {
        fclose(f);
        return UFT_ERROR_FORMAT;
    }
    
    size_t file_size = (size_t)pos;
    
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return UFT_ERROR_FILE_SEEK;
    }
    
    // Allocate private data
    hfe_data_t* priv = calloc(1, sizeof(hfe_data_t));
    if (!priv) {
        fclose(f);
        return UFT_ERROR_MEMORY;
    }
    
    priv->file = f;
    priv->file_size = file_size;
    priv->read_only = read_only;
    
    // Read header
    if (fread(&priv->header, 1, sizeof(hfe_header_t), f) != sizeof(hfe_header_t)) {
        free(priv);
        fclose(f);
        return UFT_ERROR_FILE_READ;
    }
    
    // Validate header
    if (memcmp(priv->header.signature, "HXCPICFE", 8) != 0 &&
        memcmp(priv->header.signature, "HXCHFEV3", 8) != 0) {
        free(priv);
        fclose(f);
        return UFT_ERROR_FORMAT;
    }
    
    if (priv->header.number_of_tracks == 0 || priv->header.number_of_tracks > 166 ||
        priv->header.number_of_sides == 0 || priv->header.number_of_sides > 2) {
        free(priv);
        fclose(f);
        return UFT_ERROR_FORMAT;
    }
    
    // Read track table
    uint16_t table_offset = uft_read_le16((uint8_t*)&priv->header.track_list_offset);
    size_t table_pos = (size_t)table_offset * HFE_BLOCK_SIZE;
    
    if (table_pos >= file_size) {
        free(priv);
        fclose(f);
        return UFT_ERROR_FORMAT;
    }
    
    if (fseek(f, (long)table_pos, SEEK_SET) != 0) {
        free(priv);
        fclose(f);
        return UFT_ERROR_FILE_SEEK;
    }
    
    priv->track_table_size = (size_t)priv->header.number_of_tracks;
    priv->track_table = calloc(priv->track_table_size, sizeof(hfe_track_entry_t));
    if (!priv->track_table) {
        free(priv);
        fclose(f);
        return UFT_ERROR_MEMORY;
    }
    
    size_t table_bytes = priv->track_table_size * sizeof(hfe_track_entry_t);
    if (fread(priv->track_table, 1, table_bytes, f) != table_bytes) {
        free(priv->track_table);
        free(priv);
        fclose(f);
        return UFT_ERROR_FILE_READ;
    }
    
    // Set disk fields
    disk->private_data = priv;
    disk->format = UFT_FORMAT_HFE;
    disk->geometry.cylinders = priv->header.number_of_tracks;
    disk->geometry.heads = priv->header.number_of_sides;
    disk->geometry.sectors_per_track = 0;  // Flux format
    disk->geometry.sector_size = 0;
    disk->read_only = read_only;
    
    return UFT_OK;
}

// ============================================================================
// READ TRACK (Flux Data)
// ============================================================================

static uft_error_t hfe_read_track(uft_disk_t* disk, int cyl, int head,
                                   uft_track_t* track, uft_track_read_options_t* opts) {
    UFT_REQUIRE_NOT_NULL2(disk, track);
    UFT_REQUIRE_NOT_NULL(disk->private_data);
    (void)opts;
    
    hfe_data_t* priv = (hfe_data_t*)disk->private_data;
    
    // Bounds check
    if (cyl < 0 || (size_t)cyl >= priv->track_table_size) return UFT_ERROR_BOUNDS;
    if (head < 0 || head >= priv->header.number_of_sides) return UFT_ERROR_BOUNDS;
    
    // Get track entry
    hfe_track_entry_t* entry = &priv->track_table[cyl];
    uint16_t offset_blocks = uft_read_le16((uint8_t*)&entry->offset);
    uint16_t length = uft_read_le16((uint8_t*)&entry->length);
    
    size_t track_offset = (size_t)offset_blocks * HFE_BLOCK_SIZE;
    
    // Bounds check
    if (track_offset + length > priv->file_size) {
        return UFT_ERROR_BOUNDS;
    }
    
    // HFE interleaves side 0 and side 1 data in 256-byte blocks
    size_t side_length = length / 2;
    
    // Allocate raw data for this side
    track->raw_data = malloc(side_length);
    if (!track->raw_data) {
        return UFT_ERROR_MEMORY;
    }
    
    // Seek to track data
    if (fseek(priv->file, (long)track_offset, SEEK_SET) != 0) {
        free(track->raw_data);
        track->raw_data = NULL;
        return UFT_ERROR_FILE_SEEK;
    }
    
    // Read interleaved data
    uint8_t* buffer = malloc(length);
    if (!buffer) {
        free(track->raw_data);
        track->raw_data = NULL;
        return UFT_ERROR_MEMORY;
    }
    
    if (fread(buffer, 1, length, priv->file) != length) {
        free(buffer);
        free(track->raw_data);
        track->raw_data = NULL;
        return UFT_ERROR_FILE_READ;
    }
    
    // De-interleave
    size_t out_pos = 0;
    for (size_t i = 0; i < length; i += 512) {
        size_t block_offset = (head == 0) ? i : i + 256;
        size_t copy_size = (side_length - out_pos < 256) ? (side_length - out_pos) : 256;
        
        if (block_offset + copy_size <= length) {
            memcpy(track->raw_data + out_pos, buffer + block_offset, copy_size);
            out_pos += copy_size;
        }
    }
    
    free(buffer);
    
    track->raw_size = side_length;
    track->cylinder = cyl;
    track->head = head;
    track->sector_count = 0;
    track->sectors = NULL;
    
    return UFT_OK;
}

// ============================================================================
// CLOSE
// ============================================================================

static void hfe_close(uft_disk_t* disk) {
    if (!disk || !disk->private_data) return;
    
    hfe_data_t* priv = (hfe_data_t*)disk->private_data;
    
    if (priv->track_table) free(priv->track_table);
    if (priv->file) fclose(priv->file);
    
    free(priv);
    disk->private_data = NULL;
}

// ============================================================================
// PLUGIN REGISTRATION
// ============================================================================

static const uft_format_plugin_t hfe_hardened_plugin = {
    .name = "HFE (Hardened)",
    .format = UFT_FORMAT_HFE,
    .extensions = "hfe",
    .probe = hfe_probe,
    .open = hfe_open,
    .close = hfe_close,
    .read_track = hfe_read_track,
    .write_track = NULL,  // TODO: Implement
    .create = NULL,
    .get_info = NULL,
    .capabilities = UFT_CAP_READ | UFT_CAP_FLUX
};

const uft_format_plugin_t* uft_hfe_hardened_get_plugin(void) {
    return &hfe_hardened_plugin;
}
