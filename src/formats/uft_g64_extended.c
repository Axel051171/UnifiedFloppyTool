/**
 * @file uft_g64_extended.c
 * @brief G64 format with error map extension implementation
 * 
 * P1-005: G64 error map support
 */

#include "uft/core/uft_error_compat.h"
#include "uft/formats/uft_g64_extended.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Error Map Functions
 * ============================================================================ */

void g64_error_map_init(g64_error_map_t *map) {
    if (!map) return;
    
    memset(map, 0, sizeof(*map));
    memcpy(map->magic, G64_EXT_MAGIC, 4);
    map->version = G64_EXT_VERSION;
}

void g64_error_map_free(g64_error_map_t *map) {
    if (!map) return;
    
    free(map->errors);
    map->errors = NULL;
    map->error_count = 0;
}

int g64_error_map_add(g64_error_map_t *map,
                      uint8_t track, uint8_t sector,
                      g64_error_type_t type, uint8_t confidence) {
    if (!map) return -1;
    
    /* Grow array if needed */
    uint16_t new_count = map->error_count + 1;
    g64_error_entry_t *new_errors = realloc(map->errors,
                                            new_count * sizeof(g64_error_entry_t));
    if (!new_errors) return -1;
    
    map->errors = new_errors;
    
    /* Add entry */
    g64_error_entry_t *entry = &map->errors[map->error_count];
    memset(entry, 0, sizeof(*entry));
    entry->track = track;
    entry->sector = sector;
    entry->error_type = type;
    entry->confidence = confidence;
    
    map->error_count = new_count;
    map->flags |= G64_FLAG_HAS_ERRORS;
    
    return 0;
}

g64_error_entry_t* g64_error_map_get(const g64_error_map_t *map,
                                     uint8_t track, uint8_t sector) {
    if (!map || !map->errors) return NULL;
    
    for (uint16_t i = 0; i < map->error_count; i++) {
        if (map->errors[i].track == track && 
            map->errors[i].sector == sector) {
            return &map->errors[i];
        }
    }
    
    return NULL;
}

int g64_error_map_count_track(const g64_error_map_t *map, uint8_t track) {
    if (!map || !map->errors) return 0;
    
    int count = 0;
    for (uint16_t i = 0; i < map->error_count; i++) {
        if (map->errors[i].track == track) {
            count++;
        }
    }
    
    return count;
}

/* ============================================================================
 * Error Type Names
 * ============================================================================ */

const char* g64_error_type_name(g64_error_type_t type) {
    switch (type) {
        case G64_ERR_NONE:        return "None";
        case G64_ERR_CRC:         return "CRC Error";
        case G64_ERR_HEADER_CRC:  return "Header CRC";
        case G64_ERR_DATA_CRC:    return "Data CRC";
        case G64_ERR_NO_SYNC:     return "No Sync";
        case G64_ERR_NO_HEADER:   return "No Header";
        case G64_ERR_NO_DATA:     return "No Data";
        case G64_ERR_ID_MISMATCH: return "ID Mismatch";
        case G64_ERR_SECTOR_COUNT:return "Sector Count";
        case G64_ERR_GCR_INVALID: return "Invalid GCR";
        case G64_ERR_TIMING:      return "Timing Error";
        case G64_ERR_WEAK_BITS:   return "Weak Bits";
        default:                  return "Unknown";
    }
}

/* ============================================================================
 * Options
 * ============================================================================ */

void uft_g64_write_options_init(g64_write_options_t *opts) {
    if (!opts) return;
    
    memset(opts, 0, sizeof(*opts));
    opts->include_error_map = true;
    opts->include_metadata = true;
    opts->version = 0;
    opts->force_84_tracks = false;
}

/* ============================================================================
 * G64 Extension I/O
 * ============================================================================ */

/* G64 header structure */
#pragma pack(push, 1)
typedef struct {
    char signature[8];     /* "GCR-1541" */
    uint8_t version;
    uint8_t num_tracks;
    uint16_t max_track_size;
} g64_header_t;
#pragma pack(pop)

bool uft_g64_has_extension(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return false;
    
    /* Seek to end and check for extension marker */
    if (fseek(fp, -8, SEEK_END) != 0) {
        fclose(fp);
        return false;
    }
    
    char marker[4];
    if (fread(marker, 1, 4, fp) != 4) {
        fclose(fp);
        return false;
    }
    
    fclose(fp);
    return memcmp(marker, G64_EXT_MAGIC, 4) == 0;
}

uft_error_t uft_g64_read_extended(const char *path,
                                  uft_disk_image_t **out_disk,
                                  g64_error_map_t *out_errors,
                                  g64_read_result_t *result) {
    if (!path || !out_disk) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    if (result) memset(result, 0, sizeof(*result));
    if (out_errors) g64_error_map_init(out_errors);
    
    /* Open file */
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        if (result) result->error = UFT_ERR_IO;
        return UFT_ERR_IO;
    }
    
    /* Read header */
    g64_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        if (result) result->error = UFT_ERR_CORRUPT;
        return UFT_ERR_CORRUPT;
    }
    
    /* Validate signature */
    if (memcmp(header.signature, G64_SIGNATURE, 8) != 0) {
        fclose(fp);
        if (result) result->error = UFT_ERR_UNKNOWN_FORMAT;
        return UFT_ERR_UNKNOWN_FORMAT;
    }
    
    uint8_t num_tracks = header.num_tracks;
    if (num_tracks > G64_MAX_TRACKS) {
        num_tracks = G64_MAX_TRACKS;
    }
    
    if (result) {
        result->tracks = num_tracks;
        result->max_track_size = header.max_track_size;
    }
    
    /* Read track offset table */
    uint32_t *track_offsets = malloc(num_tracks * sizeof(uint32_t));
    if (!track_offsets) {
        fclose(fp);
        return UFT_ERR_MEMORY;
    }
    
    if (fread(track_offsets, sizeof(uint32_t), num_tracks, fp) != num_tracks) {
        free(track_offsets);
        fclose(fp);
        return UFT_ERR_CORRUPT;
    }
    
    /* Read speed zone table */
    uint32_t *speed_zones = malloc(num_tracks * sizeof(uint32_t));
    if (!speed_zones) {
        free(track_offsets);
        fclose(fp);
        return UFT_ERR_MEMORY;
    }
    
    /* Seek past track offsets to speed zones */
    if (fseek(fp, 12 + (num_tracks * 4), SEEK_SET) != 0) {
        free(speed_zones);
        free(track_offsets);
        fclose(fp);
        return UFT_ERR_IO;
    }
    if (fread(speed_zones, sizeof(uint32_t), num_tracks, fp) != num_tracks) {
        /* Speed zones are optional */
        memset(speed_zones, 0, num_tracks * sizeof(uint32_t));
    }
    
    /* Create disk image */
    uft_disk_image_t *disk = uft_disk_alloc(num_tracks / 2, 1);
    if (!disk) {
        free(speed_zones);
        free(track_offsets);
        fclose(fp);
        return UFT_ERR_MEMORY;
    }
    
    disk->format = UFT_FMT_G64;
    snprintf(disk->format_name, sizeof(disk->format_name), "G64");
    disk->sectors_per_track = 0;  /* Variable */
    disk->bytes_per_sector = 256;
    
    /* Read tracks */
    for (uint8_t t = 0; t < num_tracks; t += 2) {
        if (track_offsets[t] == 0) continue;
        
        if (fseek(fp, track_offsets[t], SEEK_SET) != 0) {
            continue;  /* Skip unreadable track */
        }
        
        /* Read track size */
        uint16_t track_size;
        if (fread(&track_size, sizeof(track_size), 1, fp) != 1) {
            continue;
        }
        
        if (track_size > G64_MAX_TRACK_SIZE) {
            track_size = G64_MAX_TRACK_SIZE;
        }
        
        /* Allocate track */
        uft_track_t *track = uft_track_alloc(21, track_size * 8);
        if (!track) continue;
        
        track->track_num = t / 2;
        track->head = 0;
        track->encoding = UFT_ENC_GCR_C64;
        
        /* Read track data */
        if (fread(track->raw_data, 1, track_size, fp) != track_size) {
            uft_track_free(track);
            continue;
        }
        
        track->raw_bits = track_size * 8;
        
        /* Set speed zone in metadata */
        if (out_errors) {
            out_errors->track_meta[t].speed_zone = speed_zones[t] & 0x03;
        }
        
        disk->track_data[t / 2] = track;
    }
    
    free(speed_zones);
    free(track_offsets);
    
    /* Check for extension */
    if (uft_g64_has_extension(path) && out_errors) {
        uft_g64_read_error_map(path, out_errors);
        if (result) {
            result->has_extension = true;
            result->total_errors = out_errors->error_count;
        }
    }
    
    fclose(fp);
    
    if (result) {
        result->success = true;
    }
    
    *out_disk = disk;
    return UFT_OK;
}

uft_error_t uft_g64_read_error_map(const char *path,
                                   g64_error_map_t *out_errors) {
    if (!path || !out_errors) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    FILE *fp = fopen(path, "rb");
    if (!fp) return UFT_ERR_IO;
    
    /* Find extension at end of file */
    if (fseek(fp, -8, SEEK_END) != 0) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    long ext_end = ftell(fp);
    if (ext_end < 0) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    
    /* Read extension header offset */
    uint32_t ext_offset;
    if (fread(&ext_offset, sizeof(ext_offset), 1, fp) != 1) {
        fclose(fp);
        return UFT_ERR_CORRUPT;
    }
    
    /* Validate magic */
    char magic[4];
    if (fread(magic, 1, 4, fp) != 4 || 
        memcmp(magic, G64_EXT_MAGIC, 4) != 0) {
        fclose(fp);
        return UFT_ERR_UNKNOWN_FORMAT;
    }
    
    /* Seek to extension start */
    if (fseek(fp, ext_offset, SEEK_SET) != 0) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    
    /* Read extension header */
    uint8_t ext_version;
    uint8_t ext_flags;
    uint16_t error_count;
    
    if (fread(&ext_version, 1, 1, fp) != 1 ||
        fread(&ext_flags, 1, 1, fp) != 1 ||
        fread(&error_count, sizeof(error_count), 1, fp) != 1) {
        fclose(fp);
        return UFT_ERR_CORRUPT;
    }
    
    g64_error_map_init(out_errors);
    out_errors->version = ext_version;
    out_errors->flags = ext_flags;
    
    /* Read error entries */
    if (error_count > 0) {
        out_errors->errors = malloc(error_count * sizeof(g64_error_entry_t));
        if (!out_errors->errors) {
            fclose(fp);
            return UFT_ERR_MEMORY;
        }
        
        for (uint16_t i = 0; i < error_count; i++) {
            g64_error_entry_t *entry = &out_errors->errors[i];
            if (fread(entry, sizeof(g64_error_entry_t), 1, fp) != 1) {
                break;
            }
            out_errors->error_count++;
        }
    }
    
    /* Read track metadata */
    fread(out_errors->track_meta, sizeof(out_errors->track_meta), 1, fp);
    if (ferror(fp)) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    
        
    fclose(fp);
return UFT_OK;
}

uft_error_t uft_g64_write_extended(const uft_disk_image_t *disk,
                                   const char *path,
                                   const g64_error_map_t *errors,
                                   const g64_write_options_t *opts) {
    if (!disk || !path) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    g64_write_options_t default_opts;
    if (!opts) {
        uft_g64_write_options_init(&default_opts);
        opts = &default_opts;
    }
    
    FILE *fp = fopen(path, "wb");
    if (!fp) return UFT_ERR_IO;
    
    /* Determine track count */
    uint8_t num_tracks = opts->force_84_tracks ? 84 : 
                         (disk->tracks * 2);  /* Half-tracks */
    if (num_tracks > G64_MAX_TRACKS) {
        num_tracks = G64_MAX_TRACKS;
    }
    
    /* Write header */
    g64_header_t header = {0};
    memcpy(header.signature, G64_SIGNATURE, 8);
    header.version = opts->version;
    header.num_tracks = num_tracks;
    header.max_track_size = G64_MAX_TRACK_SIZE;
    
    fwrite(&header, sizeof(header), 1, fp);
    
    /* Calculate track data start */
    size_t table_size = num_tracks * 4 * 2;  /* Offsets + speeds */
    size_t data_start = 12 + table_size;
    
    /* Build offset table */
    uint32_t *offsets = calloc(num_tracks, sizeof(uint32_t));
    uint32_t *speeds = calloc(num_tracks, sizeof(uint32_t));
    
    if (!offsets || !speeds) {
        free(offsets);
        free(speeds);
        fclose(fp);
        return UFT_ERR_MEMORY;
    }
    
    size_t current_offset = data_start;
    
    for (uint8_t t = 0; t < num_tracks; t += 2) {
        uint8_t track_idx = t / 2;
        if (track_idx >= disk->track_count || !disk->track_data[track_idx]) {
            continue;
        }
        
        uft_track_t *track = disk->track_data[track_idx];
        if (!track->raw_data || track->raw_bits == 0) continue;
        
        offsets[t] = (uint32_t)current_offset;
        
        /* Speed zone based on track number */
        if (track_idx <= 17) speeds[t] = 3;
        else if (track_idx <= 24) speeds[t] = 2;
        else if (track_idx <= 30) speeds[t] = 1;
        else speeds[t] = 0;
        
        /* Override from error map if available */
        if (errors && errors->track_meta[t].speed_zone) {
            speeds[t] = errors->track_meta[t].speed_zone;
        }
        
        size_t track_bytes = (track->raw_bits + 7) / 8;
        current_offset += 2 + track_bytes;  /* size + data */
    }
    
    /* Write offset table */
    fwrite(offsets, sizeof(uint32_t), num_tracks, fp);
    
    /* Write speed table */
    fwrite(speeds, sizeof(uint32_t), num_tracks, fp);
    
    /* Write track data */
    for (uint8_t t = 0; t < num_tracks; t += 2) {
        if (offsets[t] == 0) continue;
        
        uint8_t track_idx = t / 2;
        uft_track_t *track = disk->track_data[track_idx];
        
        size_t track_bytes = (track->raw_bits + 7) / 8;
        uint16_t track_size = (uint16_t)track_bytes;
        
        if (fseek(fp, offsets[t], SEEK_SET) != 0) {
            continue;  /* Skip unwritable position */
        }
        if (fwrite(&track_size, sizeof(track_size), 1, fp) != 1) {
            continue;
        }
        (void)fwrite(track->raw_data, 1, track_bytes, fp);
    }
    
    free(offsets);
    free(speeds);
    
    /* Write extension if requested */
    if (opts->include_error_map && errors && errors->error_count > 0) {
        long ext_offset = ftell(fp);
        if (ext_offset < 0) {
            fclose(fp);
            return UFT_ERR_IO;
        }
        
        /* Extension header */
        uint8_t ext_version = G64_EXT_VERSION;
        uint8_t ext_flags = errors->flags;
        uint16_t error_count = errors->error_count;
        
        fwrite(&ext_version, 1, 1, fp);
        fwrite(&ext_flags, 1, 1, fp);
        fwrite(&error_count, sizeof(error_count), 1, fp);
        
        /* Error entries */
        fwrite(errors->errors, sizeof(g64_error_entry_t), 
               errors->error_count, fp);
        
        /* Track metadata */
        if (opts->include_metadata) {
            fwrite(errors->track_meta, sizeof(errors->track_meta), 1, fp);
        }
        
        /* Extension footer */
        uint32_t ext_off_val = (uint32_t)ext_offset;
        fwrite(&ext_off_val, sizeof(ext_off_val), 1, fp);
        fwrite(G64_EXT_MAGIC, 4, 1, fp);
    }
    if (ferror(fp)) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    
        
    fclose(fp);
return UFT_OK;
}

uft_error_t uft_g64_append_error_map(const char *path,
                                     const g64_error_map_t *errors) {
    if (!path || !errors) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Check if already has extension */
    if (uft_g64_has_extension(path)) {
        /* Would need to replace - not supported yet */
        return UFT_ERR_UNSUPPORTED;
    }
    
    FILE *fp = fopen(path, "ab");
    if (!fp) return UFT_ERR_IO;
    
    long ext_offset = ftell(fp);
    
    /* Write extension */
    uint8_t ext_version = G64_EXT_VERSION;
    fwrite(&ext_version, 1, 1, fp);
    fwrite(&errors->flags, 1, 1, fp);
    fwrite(&errors->error_count, sizeof(errors->error_count), 1, fp);
    
    if (errors->error_count > 0) {
        fwrite(errors->errors, sizeof(g64_error_entry_t),
               errors->error_count, fp);
    }
    
    fwrite(errors->track_meta, sizeof(errors->track_meta), 1, fp);
    
    /* Footer */
    uint32_t ext_off_val = (uint32_t)ext_offset;
    fwrite(&ext_off_val, sizeof(ext_off_val), 1, fp);
    fwrite(G64_EXT_MAGIC, 4, 1, fp);
    if (ferror(fp)) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    
        
    fclose(fp);
return UFT_OK;
}

/* ============================================================================
 * Conversion
 * ============================================================================ */

uft_error_t g64_build_error_map(const uft_disk_image_t *disk,
                                g64_error_map_t *out_map) {
    if (!disk || !out_map) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    g64_error_map_init(out_map);
    
    for (size_t t = 0; t < disk->track_count; t++) {
        uft_track_t *track = disk->track_data[t];
        if (!track) continue;
        
        /* Set track metadata */
        out_map->track_meta[t * 2].encoding = track->encoding;
        out_map->track_meta[t * 2].quality = track->quality;
        
        /* Scan sectors for errors */
        for (size_t s = 0; s < track->sector_count; s++) {
            uft_sector_t *sector = &track->sectors[s];
            
            g64_error_type_t err_type = G64_ERR_NONE;
            
            if (sector->id.status & UFT_SECTOR_CRC_ERROR) {
                err_type = G64_ERR_CRC;
            } else if (sector->id.status & UFT_SECTOR_MISSING) {
                err_type = G64_ERR_NO_DATA;
            } else if (sector->id.status & UFT_SECTOR_WEAK) {
                err_type = G64_ERR_WEAK_BITS;
                out_map->flags |= G64_FLAG_HAS_WEAK_BITS;
            }
            
            if (err_type != G64_ERR_NONE) {
                g64_error_map_add(out_map, (uint8_t)t, sector->id.sector,
                                  err_type, 255);
            }
        }
    }
    
    return UFT_OK;
}

void g64_apply_error_map(uft_disk_image_t *disk,
                         const g64_error_map_t *map) {
    if (!disk || !map) return;
    
    for (uint16_t i = 0; i < map->error_count; i++) {
        g64_error_entry_t *err = &map->errors[i];
        
        if (err->track >= disk->track_count) continue;
        
        uft_track_t *track = disk->track_data[err->track];
        if (!track) continue;
        
        /* Find sector */
        for (size_t s = 0; s < track->sector_count; s++) {
            if (track->sectors[s].id.sector == err->sector) {
                /* Apply error flag */
                switch (err->error_type) {
                    case G64_ERR_CRC:
                    case G64_ERR_HEADER_CRC:
                    case G64_ERR_DATA_CRC:
                        track->sectors[s].id.status |= UFT_SECTOR_CRC_ERROR;
                        break;
                    case G64_ERR_NO_DATA:
                    case G64_ERR_NO_HEADER:
                        track->sectors[s].id.status |= UFT_SECTOR_MISSING;
                        break;
                    case G64_ERR_WEAK_BITS:
                        track->sectors[s].id.status |= UFT_SECTOR_WEAK;
                        break;
                    default:
                        break;
                }
                break;
            }
        }
    }
}
