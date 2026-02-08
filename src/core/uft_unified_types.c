/**
 * @file uft_unified_types.c
 * @brief Implementation of unified data types
 */

#include "uft/core/uft_unified_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Error Handling
 * ============================================================================ */

static const struct {
    uft_error_t code;
    const char *str;
    bool recoverable;
} error_table[] = {
    { UFT_OK, "OK", true },
    { UFT_ERR_CRC, "CRC error", true },
    { UFT_ERR_SYNC_LOST, "Sync lost", true },
    { UFT_ERR_NO_DATA, "No data", true },
    { UFT_ERR_WEAK_BITS, "Weak bits", true },
    { UFT_ERR_TIMING, "Timing error", true },
    { UFT_ERR_ID_MISMATCH, "ID mismatch", true },
    { UFT_ERR_DELETED_DATA, "Deleted data", false },
    { UFT_ERR_MISSING_SECTOR, "Missing sector", true },
    { UFT_ERR_INCOMPLETE, "Incomplete", true },
    { UFT_ERR_PLL_UNLOCK, "PLL unlock", true },
    { UFT_ERR_ENCODING, "Encoding error", true },
    { UFT_ERR_WRITE_PROTECT, "Write protected", false },
    { UFT_ERR_VERIFY_FAIL, "Verify failed", true },
    { UFT_ERR_WRITE_FAULT, "Write fault", false },
    { UFT_ERR_TRACK_OVERFLOW, "Track overflow", false },
    { UFT_ERR_PROTECTION, "Protection error", false },
    { UFT_ERR_COPY_DENIED, "Copy denied", false },
    { UFT_ERR_LONG_TRACK, "Long track", false },
    { UFT_ERR_NON_STANDARD, "Non-standard", false },
    { UFT_ERR_UNKNOWN_FORMAT, "Unknown format", false },
    { UFT_ERR_UNSUPPORTED, "Unsupported", false },
    { UFT_ERR_CORRUPT, "Corrupt", false },
    { UFT_ERR_VERSION, "Version mismatch", false },
    { UFT_ERR_IO, "I/O error", true },
    { UFT_ERR_MEMORY, "Memory error", false },
    { UFT_ERR_INVALID_PARAM, "Invalid parameter", false },
    { UFT_ERR_NOT_IMPL, "Not implemented", false },
    { UFT_ERR_TIMEOUT, "Timeout", true },
    { UFT_ERR_CANCELLED, "Cancelled", false },
    { UFT_ERR_BUSY, "Busy", true },
    { UFT_ERR_INTERNAL, "Internal error", false },
};

#define ERROR_TABLE_SIZE (sizeof(error_table) / sizeof(error_table[0]))

const char* uft_error_str(uft_error_t err) {
    for (size_t i = 0; i < ERROR_TABLE_SIZE; i++) {
        if (error_table[i].code == err) {
            return error_table[i].str;
        }
    }
    return "Unknown error";
}

bool uft_error_recoverable(uft_error_t err) {
    for (size_t i = 0; i < ERROR_TABLE_SIZE; i++) {
        if (error_table[i].code == err) {
            return error_table[i].recoverable;
        }
    }
    return false;
}

/* ============================================================================
 * Format Names
 * ============================================================================ */

static const char* format_names[UFT_FMT_MAX] = {
    [UFT_FMT_UNKNOWN] = "Unknown",
    [UFT_FMT_IMG] = "IMG",
    [UFT_FMT_IMA] = "IMA",
    [UFT_FMT_DSK] = "DSK",
    [UFT_FMT_D64] = "D64",
    [UFT_FMT_D71] = "D71",
    [UFT_FMT_D81] = "D81",
    [UFT_FMT_D82] = "D82",
    [UFT_FMT_ADF] = "ADF",
    [UFT_FMT_MSA] = "MSA",
    [UFT_FMT_ST] = "ST",
    [UFT_FMT_ATR] = "ATR",
    [UFT_FMT_XFD] = "XFD",
    [UFT_FMT_G64] = "G64",
    [UFT_FMT_NIB] = "NIB",
    [UFT_FMT_DMK] = "DMK",
    [UFT_FMT_TD0] = "TD0",
    [UFT_FMT_IMD] = "IMD",
    [UFT_FMT_EDSK] = "EDSK",
    [UFT_FMT_HFE] = "HFE",
    [UFT_FMT_IPF] = "IPF",
    [UFT_FMT_FDI] = "FDI",
    [UFT_FMT_CQM] = "CQM",
    [UFT_FMT_SCP] = "SCP",
    [UFT_FMT_A2R] = "A2R",
    [UFT_FMT_WOZ] = "WOZ",
    [UFT_FMT_UFT_KF_RAW] = "KF/RAW",
    [UFT_FMT_GWRAW] = "GWRAW",
    [UFT_FMT_MOOF] = "MOOF",
    [UFT_FMT_D88] = "D88",
    [UFT_FMT_NFD] = "NFD",
    [UFT_FMT_FDD] = "FDD",
    [UFT_FMT_HDM] = "HDM",
};

const char* uft_format_name(uft_format_id_t format) {
    if (format < UFT_FMT_MAX) {
        return format_names[format] ? format_names[format] : "Unknown";
    }
    return "Invalid";
}

static const char* encoding_names[] = {
    [UFT_ENC_UNKNOWN] = "Unknown",
    [UFT_ENC_FM] = "FM",
    [UFT_ENC_MFM] = "MFM",
    [UFT_ENC_M2FM] = "M2FM",
    [UFT_ENC_GCR_C64] = "GCR (C64)",
    [UFT_ENC_GCR_APPLE] = "GCR (Apple)",
    [UFT_ENC_GCR_MAC] = "GCR (Mac)",
    [UFT_ENC_AMIGA_MFM] = "Amiga MFM",
};

const char* uft_encoding_name(uint8_t encoding) {
    if (encoding < sizeof(encoding_names)/sizeof(encoding_names[0])) {
        return encoding_names[encoding] ? encoding_names[encoding] : "Unknown";
    }
    return "Invalid";
}

/* ============================================================================
 * Memory Management - Sector
 * ============================================================================ */

uft_sector_t* uft_sector_alloc(size_t data_len) {
    uft_sector_t *sector = calloc(1, sizeof(uft_sector_t));
    if (!sector) return NULL;
    
    if (data_len > 0) {
        sector->data = calloc(1, data_len);
        if (!sector->data) {
            free(sector);
            return NULL;
        }
        sector->data_len = data_len;
    }
    
    return sector;
}

void uft_sector_free(uft_sector_t *sector) {
    if (!sector) return;
    
    free(sector->data);
    free(sector->confidence);
    free(sector->weak_mask);
    free(sector->timing_ns);
    free(sector);
}

int uft_sector_copy(uft_sector_t *dest, const uft_sector_t *src) {
    if (!dest || !src) return -1;
    
    dest->id = src->id;
    dest->crc_stored = src->crc_stored;
    dest->crc_calculated = src->crc_calculated;
    dest->crc_valid = src->crc_valid;
    dest->error = src->error;
    dest->retry_count = src->retry_count;
    dest->bit_offset = src->bit_offset;
    dest->byte_offset = src->byte_offset;
    
    /* Copy data */
    if (src->data && src->data_len > 0) {
        dest->data = malloc(src->data_len);
        if (!dest->data) return -2;
        memcpy(dest->data, src->data, src->data_len);
        dest->data_len = src->data_len;
    }
    
    /* Copy confidence */
    if (src->confidence && src->data_len > 0) {
        dest->confidence = malloc(src->data_len);
        if (dest->confidence) {
            memcpy(dest->confidence, src->confidence, src->data_len);
        }
    }
    
    /* Copy weak mask */
    if (src->weak_mask && src->data_len > 0) {
        dest->weak_mask = malloc(src->data_len);
        if (dest->weak_mask) {
            memcpy(dest->weak_mask, src->weak_mask, src->data_len);
        }
    }
    
    /* Copy timing */
    if (src->timing_ns && src->timing_count > 0) {
        dest->timing_ns = malloc(src->timing_count * sizeof(double));
        if (dest->timing_ns) {
            memcpy(dest->timing_ns, src->timing_ns, 
                   src->timing_count * sizeof(double));
            dest->timing_count = src->timing_count;
        }
    }
    
    return 0;
}

/* ============================================================================
 * Memory Management - Track
 * ============================================================================ */

uft_track_t* uft_track_alloc(size_t max_sectors, size_t max_raw_bits) {
    uft_track_t *track = calloc(1, sizeof(uft_track_t));
    if (!track) return NULL;
    
    track->owns_data = true;
    
    if (max_sectors > 0) {
        track->sectors = calloc(max_sectors, sizeof(uft_sector_t));
        if (!track->sectors) {
            free(track);
            return NULL;
        }
        track->sector_capacity = max_sectors;
    }
    
    if (max_raw_bits > 0) {
        size_t raw_bytes = (max_raw_bits + 7) / 8;
        track->raw_data = calloc(1, raw_bytes);
        if (!track->raw_data) {
            free(track->sectors);
            free(track);
            return NULL;
        }
        track->raw_capacity = raw_bytes;
    }
    
    return track;
}

void uft_track_free(uft_track_t *track) {
    if (!track) return;
    
    if (track->owns_data) {
        for (size_t i = 0; i < track->sector_count; i++) {
            free(track->sectors[i].data);
            free(track->sectors[i].confidence);
            free(track->sectors[i].weak_mask);
            free(track->sectors[i].timing_ns);
        }
        free(track->sectors);
        free(track->raw_data);
        free(track->flux_times);
        free(track->confidence);
        free(track->weak_mask);
        
        if (track->revisions) {
            for (size_t i = 0; i < track->revision_count; i++) {
                free(track->revisions[i].data);
            }
            free(track->revisions);
        }
    }
    
    free(track);
}

int uft_track_copy(uft_track_t *dest, const uft_track_t *src) {
    if (!dest || !src) return -1;
    
    dest->track_num = src->track_num;
    dest->head = src->head;
    dest->encoding = src->encoding;
    dest->error = src->error;
    dest->quality = src->quality;
    dest->complete = src->complete;
    dest->protected = src->protected;
    dest->rotation_ns = src->rotation_ns;
    dest->data_rate = src->data_rate;
    dest->owns_data = true;
    
    /* Copy raw data */
    if (src->raw_data && src->raw_capacity > 0) {
        dest->raw_data = malloc(src->raw_capacity);
        if (!dest->raw_data) return -2;
        memcpy(dest->raw_data, src->raw_data, src->raw_capacity);
        dest->raw_bits = src->raw_bits;
        dest->raw_capacity = src->raw_capacity;
    }
    
    /* Copy sectors */
    if (src->sectors && src->sector_count > 0) {
        dest->sectors = calloc(src->sector_count, sizeof(uft_sector_t));
        if (!dest->sectors) return -3;
        
        for (size_t i = 0; i < src->sector_count; i++) {
            if (uft_sector_copy(&dest->sectors[i], &src->sectors[i]) != 0) {
                return -4;
            }
        }
        dest->sector_count = src->sector_count;
        dest->sector_capacity = src->sector_count;
    }
    
    return 0;
}

/* ============================================================================
 * Memory Management - Disk
 * ============================================================================ */

uft_disk_image_t* uft_disk_alloc(uint16_t tracks, uint8_t heads) {
    uft_disk_image_t *disk = calloc(1, sizeof(uft_disk_image_t));
    if (!disk) return NULL;
    
    disk->owns_data = true;
    disk->tracks = tracks;
    disk->heads = heads;
    
    size_t total_tracks = (size_t)tracks * heads;
    if (total_tracks > 0) {
        disk->track_data = calloc(total_tracks, sizeof(uft_track_t*));
        if (!disk->track_data) {
            free(disk);
            return NULL;
        }
        disk->track_count = total_tracks;
    }
    
    return disk;
}

void uft_disk_free(uft_disk_image_t *disk) {
    if (!disk) return;
    
    if (disk->owns_data) {
        for (size_t i = 0; i < disk->track_count; i++) {
            uft_track_free(disk->track_data[i]);
        }
        free(disk->track_data);
        free(disk->source_path);
    }
    
    free(disk);
}

/* ============================================================================
 * Comparison
 * ============================================================================ */

int uft_disk_compare(const uft_disk_image_t *a, 
                     const uft_disk_image_t *b,
                     uft_compare_result_t *result) {
    if (!a || !b || !result) return -1;
    
    *result = UFT_CMP_IDENTICAL;
    
    /* Compare geometry */
    if (a->tracks != b->tracks || a->heads != b->heads ||
        a->sectors_per_track != b->sectors_per_track ||
        a->bytes_per_sector != b->bytes_per_sector) {
        *result |= UFT_CMP_GEOMETRY_DIFFERS;
    }
    
    /* Compare format */
    if (a->format != b->format) {
        *result |= UFT_CMP_METADATA_DIFFERS;
    }
    
    /* Compare track data */
    size_t min_tracks = (a->track_count < b->track_count) ? 
                        a->track_count : b->track_count;
    
    for (size_t t = 0; t < min_tracks; t++) {
        uft_track_t *ta = a->track_data[t];
        uft_track_t *tb = b->track_data[t];
        
        if (!ta && !tb) continue;
        if (!ta || !tb) {
            *result |= UFT_CMP_DATA_DIFFERS;
            continue;
        }
        
        /* Compare sectors */
        if (ta->sector_count != tb->sector_count) {
            *result |= UFT_CMP_DATA_DIFFERS;
            continue;
        }
        
        for (size_t s = 0; s < ta->sector_count; s++) {
            uft_sector_t *sa = &ta->sectors[s];
            uft_sector_t *sb = &tb->sectors[s];
            
            if (sa->data_len != sb->data_len) {
                *result |= UFT_CMP_DATA_DIFFERS;
                continue;
            }
            
            if (sa->data && sb->data) {
                if (memcmp(sa->data, sb->data, sa->data_len) != 0) {
                    *result |= UFT_CMP_DATA_DIFFERS;
                }
            }
        }
    }
    
    return 0;
}
