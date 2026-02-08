/**
 * @file uft_disk.c
 * @brief Unified Disk Implementation (P2-ARCH-005)
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#include "uft/core/uft_disk.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

uft_disk_unified_t* uft_disk_create(void)
{
    uft_disk_unified_t *disk = calloc(1, sizeof(uft_disk_unified_t));
    if (!disk) return NULL;
    
    disk->type = UFT_DTYPE_UNKNOWN;
    disk->encoding = UFT_DISK_ENC_UNKNOWN;
    
    return disk;
}

void uft_disk_free(uft_disk_unified_t *disk)
{
    if (!disk) return;
    
    /* Free all tracks */
    for (int i = 0; i < UFT_DISK_MAX_TRACKS; i++) {
        if (disk->tracks[i]) {
            uft_track_base_free(disk->tracks[i]);
        }
    }
    
    free(disk->raw_data);
    free(disk);
}

uft_disk_unified_t* uft_disk_clone(const uft_disk_unified_t *src)
{
    if (!src) return NULL;
    
    uft_disk_unified_t *dst = uft_disk_create();
    if (!dst) return NULL;
    
    /* Copy scalar/fixed fields */
    memcpy(dst->name, src->name, sizeof(dst->name));
    memcpy(dst->source_path, src->source_path, sizeof(dst->source_path));
    memcpy(dst->format_name, src->format_name, sizeof(dst->format_name));
    
    dst->type = src->type;
    dst->flags = src->flags;
    dst->encoding = src->encoding;
    dst->geometry = src->geometry;
    dst->track_count = src->track_count;
    
    dst->total_sectors = src->total_sectors;
    dst->good_sectors = src->good_sectors;
    dst->bad_sectors = src->bad_sectors;
    dst->missing_sectors = src->missing_sectors;
    dst->overall_quality = src->overall_quality;
    
    dst->protection_type = src->protection_type;
    memcpy(dst->protection_name, src->protection_name, sizeof(dst->protection_name));
    
    memcpy(dst->metadata, src->metadata, sizeof(dst->metadata));
    dst->metadata_count = src->metadata_count;
    
    dst->created_time = src->created_time;
    dst->modified_time = src->modified_time;
    
    /* Clone tracks */
    for (int i = 0; i < UFT_DISK_MAX_TRACKS; i++) {
        if (src->tracks[i]) {
            dst->tracks[i] = uft_track_base_clone(src->tracks[i]);
        }
    }
    
    /* Clone raw data */
    if (src->raw_data && src->raw_size > 0) {
        dst->raw_data = malloc(src->raw_size);
        if (dst->raw_data) {
            memcpy(dst->raw_data, src->raw_data, src->raw_size);
            dst->raw_size = src->raw_size;
        }
    }
    
    return dst;
}

/*===========================================================================
 * Track Management
 *===========================================================================*/

static int track_index(uint8_t cyl, uint8_t head)
{
    if (head > 1) return -1;
    int idx = cyl * 2 + head;
    if (idx >= UFT_DISK_MAX_TRACKS) return -1;
    return idx;
}

int uft_disk_add_track(uft_disk_unified_t *disk, uft_track_base_t *track)
{
    if (!disk || !track) return -1;
    
    int idx = track_index(track->cylinder, track->head);
    if (idx < 0) return -1;
    
    /* Free existing track */
    if (disk->tracks[idx]) {
        uft_track_base_free(disk->tracks[idx]);
    }
    
    disk->tracks[idx] = track;
    
    /* Update track count */
    int max_idx = 0;
    for (int i = 0; i < UFT_DISK_MAX_TRACKS; i++) {
        if (disk->tracks[i]) {
            max_idx = i + 1;
        }
    }
    disk->track_count = (uint8_t)max_idx;
    
    return 0;
}

uft_track_base_t* uft_disk_get_track(uft_disk_unified_t *disk, 
                                      uint8_t cyl, uint8_t head)
{
    if (!disk) return NULL;
    
    int idx = track_index(cyl, head);
    if (idx < 0) return NULL;
    
    return disk->tracks[idx];
}

int uft_disk_remove_track(uft_disk_unified_t *disk, uint8_t cyl, uint8_t head)
{
    if (!disk) return -1;
    
    int idx = track_index(cyl, head);
    if (idx < 0) return -1;
    
    if (disk->tracks[idx]) {
        uft_track_base_free(disk->tracks[idx]);
        disk->tracks[idx] = NULL;
    }
    
    return 0;
}

/*===========================================================================
 * Sector Access
 *===========================================================================*/

uft_sector_unified_t* uft_disk_get_sector(uft_disk_unified_t *disk,
                                           uint8_t cyl, uint8_t head, 
                                           uint8_t sector)
{
    if (!disk) return NULL;
    
    uft_track_base_t *track = uft_disk_get_track(disk, cyl, head);
    if (!track) return NULL;
    
    /* Search for sector in track */
    /* Note: This assumes tracks have sector arrays, which track_base supports */
    uft_sector_base_t *sec = uft_track_base_find_sector(track, sector);
    
    /* Convert track_base sector to unified sector.
     * These types have compatible layouts for basic data access.
     * Both contain data pointer, size, and sector ID fields. */
    if (sec && sec->data && sec->size > 0) {
        /* The sector_base_t fields map to sector_unified_t for read access.
         * This is a read-only view - modifications need proper conversion. */
        return (uft_sector_unified_t *)(void *)sec;
    }
    return NULL;
}

int uft_disk_read_sector(uft_disk_unified_t *disk,
                         uint8_t cyl, uint8_t head, uint8_t sector,
                         uint8_t *buffer, size_t size)
{
    if (!disk || !buffer) return -1;
    
    /* For raw sector images, calculate offset directly */
    if (disk->raw_data && disk->geometry.sector_size > 0) {
        size_t sec_size = disk->geometry.sector_size;
        size_t spt = disk->geometry.sectors;
        size_t heads = disk->geometry.heads;
        
        if (spt == 0 || heads == 0) return -1;
        
        size_t offset = ((cyl * heads + head) * spt + sector) * sec_size;
        
        if (offset + sec_size > disk->raw_size) return -1;
        if (size < sec_size) return -1;
        
        memcpy(buffer, disk->raw_data + offset, sec_size);
        return (int)sec_size;
    }
    
    /* For track-based access */
    uft_sector_unified_t *sec = uft_disk_get_sector(disk, cyl, head, sector);
    if (!sec || !sec->data) return -1;
    
    size_t copy_size = sec->data_size;
    if (copy_size > size) copy_size = size;
    
    memcpy(buffer, sec->data, copy_size);
    return (int)copy_size;
}

/*===========================================================================
 * Metadata
 *===========================================================================*/

int uft_disk_set_meta(uft_disk_unified_t *disk, 
                      const char *key, const char *value)
{
    if (!disk || !key) return -1;
    
    /* Look for existing key */
    for (int i = 0; i < disk->metadata_count; i++) {
        if (strcmp(disk->metadata[i].key, key) == 0) {
            if (value) {
                snprintf(disk->metadata[i].value, 
                         sizeof(disk->metadata[i].value), "%s", value);
            } else {
                disk->metadata[i].value[0] = '\0';
            }
            return 0;
        }
    }
    
    /* Add new entry */
    if (disk->metadata_count >= UFT_DISK_MAX_METADATA) return -1;
    
    snprintf(disk->metadata[disk->metadata_count].key,
             sizeof(disk->metadata[0].key), "%s", key);
    if (value) {
        snprintf(disk->metadata[disk->metadata_count].value,
                 sizeof(disk->metadata[0].value), "%s", value);
    }
    disk->metadata_count++;
    
    return 0;
}

const char* uft_disk_get_meta(uft_disk_unified_t *disk, const char *key)
{
    if (!disk || !key) return NULL;
    
    for (int i = 0; i < disk->metadata_count; i++) {
        if (strcmp(disk->metadata[i].key, key) == 0) {
            return disk->metadata[i].value;
        }
    }
    
    return NULL;
}

/*===========================================================================
 * Statistics
 *===========================================================================*/

void uft_disk_update_stats(uft_disk_unified_t *disk)
{
    if (!disk) return;
    
    disk->total_sectors = 0;
    disk->good_sectors = 0;
    disk->bad_sectors = 0;
    disk->missing_sectors = 0;
    
    for (int i = 0; i < UFT_DISK_MAX_TRACKS; i++) {
        if (disk->tracks[i]) {
            uft_track_base_update_stats(disk->tracks[i]);
            
            disk->total_sectors += disk->tracks[i]->sectors_expected;
            disk->good_sectors += disk->tracks[i]->sectors_good;
            disk->bad_sectors += disk->tracks[i]->sectors_bad;
            disk->missing_sectors += (disk->tracks[i]->sectors_expected - 
                                      disk->tracks[i]->sectors_found);
        }
    }
    
    disk->overall_quality = uft_disk_calc_quality(disk);
    
    /* Update flags */
    if (disk->bad_sectors > 0) {
        disk->flags |= UFT_DF_BAD_SECTORS;
    }
}

float uft_disk_calc_quality(const uft_disk_unified_t *disk)
{
    if (!disk || disk->total_sectors == 0) return 0.0f;
    
    return (float)disk->good_sectors / (float)disk->total_sectors;
}

/*===========================================================================
 * Info
 *===========================================================================*/

const char* uft_disk_type_name(uft_disk_type_t type)
{
    switch (type) {
        case UFT_DTYPE_525_SS_SD:   return "5.25\" SS/SD";
        case UFT_DTYPE_525_SS_DD:   return "5.25\" SS/DD";
        case UFT_DTYPE_525_DS_DD:   return "5.25\" DS/DD";
        case UFT_DTYPE_525_DS_HD:   return "5.25\" DS/HD";
        case UFT_DTYPE_525_DS_QD:   return "5.25\" DS/QD";
        case UFT_DTYPE_35_SS_DD:    return "3.5\" SS/DD";
        case UFT_DTYPE_35_DS_DD:    return "3.5\" DS/DD (720KB)";
        case UFT_DTYPE_35_DS_HD:    return "3.5\" DS/HD (1.44MB)";
        case UFT_DTYPE_35_DS_ED:    return "3.5\" DS/ED (2.88MB)";
        case UFT_DTYPE_8_SS_SD:     return "8\" SS/SD";
        case UFT_DTYPE_8_DS_SD:     return "8\" DS/SD";
        case UFT_DTYPE_8_DS_DD:     return "8\" DS/DD";
        case UFT_DTYPE_HARD_SECTOR: return "Hard-Sectored";
        case UFT_DTYPE_CUSTOM:      return "Custom";
        default:                    return "Unknown";
    }
}

const char* uft_disk_flags_str(uint16_t flags)
{
    static char buf[256];
    buf[0] = '\0';
    size_t pos = 0;
    
    if (flags & UFT_DF_READ_ONLY)   pos += snprintf(buf+pos, sizeof(buf)-pos, "ro,");
    if (flags & UFT_DF_MODIFIED)    pos += snprintf(buf+pos, sizeof(buf)-pos, "mod,");
    if (flags & UFT_DF_PROTECTED)   pos += snprintf(buf+pos, sizeof(buf)-pos, "prot,");
    if (flags & UFT_DF_BAD_SECTORS) pos += snprintf(buf+pos, sizeof(buf)-pos, "bad,");
    if (flags & UFT_DF_FLUX_SOURCE) pos += snprintf(buf+pos, sizeof(buf)-pos, "flux,");
    if (flags & UFT_DF_MULTI_REV)   pos += snprintf(buf+pos, sizeof(buf)-pos, "mrev,");
    if (flags & UFT_DF_FORENSIC)    pos += snprintf(buf+pos, sizeof(buf)-pos, "forensic,");
    
    if (pos > 0 && buf[pos-1] == ',') buf[pos-1] = '\0';
    
    return buf;
}

int uft_disk_get_info(const uft_disk_unified_t *disk, 
                      char *buffer, size_t size)
{
    if (!disk || !buffer || size == 0) return -1;
    
    return snprintf(buffer, size,
        "Disk: %s\n"
        "Type: %s\n"
        "Format: %s\n"
        "Geometry: %d cyl × %d heads × %d spt\n"
        "Sectors: %u total, %u good, %u bad\n"
        "Quality: %.1f%%\n"
        "Flags: [%s]",
        disk->name[0] ? disk->name : "(unnamed)",
        uft_disk_type_name(disk->type),
        disk->format_name[0] ? disk->format_name : "Unknown",
        disk->geometry.cylinders, disk->geometry.heads, disk->geometry.sectors,
        disk->total_sectors, disk->good_sectors, disk->bad_sectors,
        disk->overall_quality * 100.0f,
        uft_disk_flags_str(disk->flags));
}

/*===========================================================================
 * I/O Helpers
 *===========================================================================*/

int uft_disk_alloc_raw(uft_disk_unified_t *disk, size_t size)
{
    if (!disk || size == 0) return -1;
    
    free(disk->raw_data);
    disk->raw_data = calloc(1, size);
    if (!disk->raw_data) return -1;
    
    disk->raw_size = size;
    disk->flags |= UFT_DF_SECTOR_IMAGE;
    
    return 0;
}

int uft_disk_set_geometry(uft_disk_unified_t *disk,
                          uint8_t cyls, uint8_t heads, 
                          uint8_t spt, uint16_t sector_size)
{
    if (!disk) return -1;
    
    disk->geometry.cylinders = cyls;
    disk->geometry.heads = heads;
    disk->geometry.sectors = spt;
    disk->geometry.sector_size = sector_size;
    disk->geometry.rpm = 300;  /* Default */
    
    return 0;
}
