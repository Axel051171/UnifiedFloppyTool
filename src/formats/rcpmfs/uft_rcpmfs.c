/**
 * @file uft_rcpmfs.c
 * @brief RCPMFS (Remote CP/M File System) implementation
 * @version 3.9.0
 * 
 * Multi-disk CP/M container format.
 * Reference: libdsk drvrcpm.c
 */

#include "uft/formats/uft_rcpmfs.h"
#include "uft/uft_format_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static uint32_t read_le32(const uint8_t *p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read_le16(const uint8_t *p) {
    return p[0] | (p[1] << 8);
}

static void write_le32(uint8_t *p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

static void write_le16(uint8_t *p, uint16_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

static uint8_t size_code(uint16_t size) {
    switch (size) {
        case 128:  return 0;
        case 256:  return 1;
        case 512:  return 2;
        case 1024: return 3;
        default:   return 2;
    }
}

/* ============================================================================
 * Options Initialization
 * ============================================================================ */

void uft_rcpmfs_read_options_init(rcpmfs_read_options_t *opts) {
    if (!opts) return;
    memset(opts, 0, sizeof(*opts));
    opts->disk_index = 0;  /* Read first disk by default */
}

void uft_rcpmfs_write_options_init(rcpmfs_write_options_t *opts) {
    if (!opts) return;
    memset(opts, 0, sizeof(*opts));
    opts->compress = false;
}

/* ============================================================================
 * Header Validation
 * ============================================================================ */

bool uft_rcpmfs_validate_header(const rcpmfs_header_t *header) {
    if (!header) return false;
    return memcmp(header->magic, RCPMFS_MAGIC, RCPMFS_MAGIC_LEN) == 0;
}

bool uft_rcpmfs_probe(const uint8_t *data, size_t size, int *confidence) {
    if (!data || size < RCPMFS_HEADER_SIZE) return false;
    
    const rcpmfs_header_t *header = (const rcpmfs_header_t *)data;
    if (uft_rcpmfs_validate_header(header)) {
        if (confidence) *confidence = 95;
        return true;
    }
    
    return false;
}

/* ============================================================================
 * Read Implementation
 * ============================================================================ */

uft_error_t uft_rcpmfs_read_mem(const uint8_t *data, size_t size,
                                uft_disk_image_t **out_disk,
                                const rcpmfs_read_options_t *opts,
                                rcpmfs_read_result_t *result) {
    if (!data || !out_disk || size < RCPMFS_HEADER_SIZE) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Initialize result */
    if (result) {
        memset(result, 0, sizeof(*result));
        result->container_size = size;
    }
    
    /* Validate header */
    const rcpmfs_header_t *header = (const rcpmfs_header_t *)data;
    if (!uft_rcpmfs_validate_header(header)) {
        if (result) {
            result->error = UFT_ERR_FORMAT;
            result->error_detail = "Invalid RCPMFS signature";
        }
        return UFT_ERR_FORMAT;
    }
    
    uint16_t num_disks = read_le16((const uint8_t*)&header->num_disks);
    
    if (result) {
        result->num_disks = num_disks;
        memcpy(result->comment, header->comment, sizeof(header->comment));
    }
    
    /* Default options */
    rcpmfs_read_options_t default_opts;
    if (!opts) {
        uft_rcpmfs_read_options_init(&default_opts);
        opts = &default_opts;
    }
    
    /* Check disk index */
    int disk_idx = opts->disk_index;
    if (disk_idx < 0) disk_idx = 0;
    if (disk_idx >= num_disks) {
        if (result) {
            result->error = UFT_ERR_INVALID_PARAM;
            result->error_detail = "Disk index out of range";
        }
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Read disk entries */
    size_t entry_offset = RCPMFS_HEADER_SIZE;
    
    for (uint16_t i = 0; i < num_disks && i < RCPMFS_MAX_DISKS; i++) {
        if (entry_offset + sizeof(rcpmfs_disk_entry_t) > size) break;
        
        const rcpmfs_disk_entry_t *entry = (const rcpmfs_disk_entry_t *)(data + entry_offset);
        
        if (result) {
            memcpy(result->disks[i].name, entry->name, 16);
            memcpy(result->disks[i].diskdef, entry->diskdef, 16);
            result->disks[i].cylinders = read_le16((const uint8_t*)&entry->cylinders);
            result->disks[i].heads = entry->heads;
            result->disks[i].sectors = entry->sectors;
            result->disks[i].sector_size = read_le16((const uint8_t*)&entry->sector_size);
            result->disks[i].data_size = read_le32((const uint8_t*)&entry->size);
        }
        
        entry_offset += sizeof(rcpmfs_disk_entry_t);
    }
    
    /* Get selected disk entry */
    const rcpmfs_disk_entry_t *selected = (const rcpmfs_disk_entry_t *)
        (data + RCPMFS_HEADER_SIZE + disk_idx * sizeof(rcpmfs_disk_entry_t));
    
    uint32_t disk_offset = read_le32((const uint8_t*)&selected->offset);
    uint32_t disk_size = read_le32((const uint8_t*)&selected->size);
    uint16_t cylinders = read_le16((const uint8_t*)&selected->cylinders);
    uint8_t heads = selected->heads;
    uint8_t sectors = selected->sectors;
    uint16_t sector_size = read_le16((const uint8_t*)&selected->sector_size);
    
    /* Validate */
    if (disk_offset + disk_size > size) {
        if (result) {
            result->error = UFT_ERR_FORMAT;
            result->error_detail = "Disk data extends beyond container";
        }
        return UFT_ERR_FORMAT;
    }
    
    /* Create disk image */
    uft_disk_image_t *disk = uft_disk_alloc(cylinders, heads);
    if (!disk) {
        return UFT_ERR_MEMORY;
    }
    
    disk->format = UFT_FMT_RAW;
    snprintf(disk->format_name, sizeof(disk->format_name), "RCPMFS");
    disk->sectors_per_track = sectors;
    disk->bytes_per_sector = sector_size;
    
    /* Read disk data */
    const uint8_t *disk_data = data + disk_offset;
    size_t data_pos = 0;
    uint8_t sz_code = size_code(sector_size);
    
    for (uint16_t c = 0; c < cylinders; c++) {
        for (uint8_t h = 0; h < heads; h++) {
            size_t idx = c * heads + h;
            
            uft_track_t *track = uft_track_alloc(sectors, 0);
            if (!track) {
                uft_disk_free(disk);
                return UFT_ERR_MEMORY;
            }
            
            track->cylinder = c;
            track->head = h;
            track->encoding = UFT_ENC_MFM;
            
            for (uint8_t s = 0; s < sectors; s++) {
                uft_sector_t *sect = &track->sectors[s];
                sect->id.cylinder = c;
                sect->id.head = h;
                sect->id.sector = s + 1;
                sect->id.size_code = sz_code;
                sect->status = UFT_SECTOR_OK;
                
                sect->data = malloc(sector_size);
                sect->data_size = sector_size;
                
                if (sect->data) {
                    if (data_pos + sector_size <= disk_size) {
                        memcpy(sect->data, disk_data + data_pos, sector_size);
                    } else {
                        memset(sect->data, 0xE5, sector_size);
                    }
                }
                data_pos += sector_size;
                track->sector_count++;
            }
            
            disk->track_data[idx] = track;
        }
    }
    
    if (result) {
        result->success = true;
    }
    
    *out_disk = disk;
    return UFT_OK;
}

uft_error_t uft_rcpmfs_read(const char *path,
                            uft_disk_image_t **out_disk,
                            const rcpmfs_read_options_t *opts,
                            rcpmfs_read_result_t *result) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return UFT_ERR_IO;
    }
    
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(fp);
        return UFT_ERR_MEMORY;
    }
    
    if (fread(data, 1, size, fp) != size) {
        free(data);
        fclose(fp);
        return UFT_ERR_IO;
    }
    fclose(fp);
    
    uft_error_t err = uft_rcpmfs_read_mem(data, size, out_disk, opts, result);
    free(data);
    
    return err;
}

/* ============================================================================
 * Write Implementation
 * ============================================================================ */

uft_error_t uft_rcpmfs_write(const uft_disk_image_t *disk,
                             const char *path,
                             const char *disk_name,
                             const char *diskdef_name,
                             const rcpmfs_write_options_t *opts) {
    if (!disk || !path) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Calculate disk data size */
    size_t disk_data_size = (size_t)disk->tracks * disk->heads *
                            disk->sectors_per_track * disk->bytes_per_sector;
    
    /* Calculate total container size */
    size_t total_size = RCPMFS_HEADER_SIZE + sizeof(rcpmfs_disk_entry_t) + disk_data_size;
    
    uint8_t *output = malloc(total_size);
    if (!output) {
        return UFT_ERR_MEMORY;
    }
    memset(output, 0, total_size);
    
    /* Build header */
    rcpmfs_header_t *header = (rcpmfs_header_t *)output;
    memcpy(header->magic, RCPMFS_MAGIC, RCPMFS_MAGIC_LEN);
    header->version = RCPMFS_VERSION;
    header->flags = 0;
    write_le16((uint8_t*)&header->num_disks, 1);
    write_le32((uint8_t*)&header->total_size, total_size);
    
    if (opts && opts->comment[0]) {
        strncpy(header->comment, opts->comment, sizeof(header->comment) - 1);
    }
    
    /* Build disk entry */
    rcpmfs_disk_entry_t *entry = (rcpmfs_disk_entry_t *)(output + RCPMFS_HEADER_SIZE);
    if (disk_name) {
        strncpy(entry->name, disk_name, sizeof(entry->name) - 1);
    } else {
        strcpy(entry->name, "DISK_A");
    }
    if (diskdef_name) {
        strncpy(entry->diskdef, diskdef_name, sizeof(entry->diskdef) - 1);
    }
    write_le32((uint8_t*)&entry->offset, RCPMFS_HEADER_SIZE + sizeof(rcpmfs_disk_entry_t));
    write_le32((uint8_t*)&entry->size, disk_data_size);
    write_le16((uint8_t*)&entry->cylinders, disk->tracks);
    entry->heads = disk->heads;
    entry->sectors = disk->sectors_per_track;
    write_le16((uint8_t*)&entry->sector_size, disk->bytes_per_sector);
    
    /* Write disk data */
    uint8_t *disk_data = output + RCPMFS_HEADER_SIZE + sizeof(rcpmfs_disk_entry_t);
    size_t data_pos = 0;
    
    for (uint16_t c = 0; c < disk->tracks; c++) {
        for (uint8_t h = 0; h < disk->heads; h++) {
            size_t idx = c * disk->heads + h;
            uft_track_t *track = disk->track_data[idx];
            
            for (uint8_t s = 0; s < disk->sectors_per_track; s++) {
                if (track && s < track->sector_count && track->sectors[s].data) {
                    memcpy(disk_data + data_pos, track->sectors[s].data,
                           disk->bytes_per_sector);
                } else {
                    memset(disk_data + data_pos, 0xE5, disk->bytes_per_sector);
                }
                data_pos += disk->bytes_per_sector;
            }
        }
    }
    
    /* Write file */
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        free(output);
        return UFT_ERR_IO;
    }
    
    size_t written = fwrite(output, 1, total_size, fp);
    fclose(fp);
    free(output);
    
    if (written != total_size) {
        return UFT_ERR_IO;
    }
    
    return UFT_OK;
}

/* ============================================================================
 * List Function
 * ============================================================================ */

uft_error_t uft_rcpmfs_list(const char *path,
                            rcpmfs_disk_info_t *disks,
                            size_t max_disks,
                            size_t *num_disks) {
    rcpmfs_read_result_t result;
    uft_disk_image_t *disk = NULL;
    
    rcpmfs_read_options_t opts;
    uft_rcpmfs_read_options_init(&opts);
    
    /* Just read to get disk list */
    uft_error_t err = uft_rcpmfs_read(path, &disk, &opts, &result);
    if (disk) {
        uft_disk_free(disk);
    }
    
    if (err != UFT_OK && err != UFT_ERR_FORMAT) {
        return err;
    }
    
    if (result.num_disks > 0 && disks && max_disks > 0) {
        size_t copy_count = (result.num_disks < max_disks) ? result.num_disks : max_disks;
        memcpy(disks, result.disks, copy_count * sizeof(rcpmfs_disk_info_t));
    }
    
    if (num_disks) {
        *num_disks = result.num_disks;
    }
    
    return UFT_OK;
}

/* ============================================================================
 * Format Plugin Registration
 * ============================================================================ */

static bool rcpmfs_probe_plugin(const uint8_t *data, size_t size,
                                size_t file_size, int *confidence) {
    (void)file_size;
    return uft_rcpmfs_probe(data, size, confidence);
}

static uft_error_t rcpmfs_open(uft_disk_t *disk, const char *path, bool read_only) {
    (void)read_only;
    uft_disk_image_t *image = NULL;
    uft_error_t err = uft_rcpmfs_read(path, &image, NULL, NULL);
    if (err == UFT_OK && image) {
        disk->plugin_data = image;
        disk->geometry.cylinders = image->tracks;
        disk->geometry.heads = image->heads;
        disk->geometry.sectors = image->sectors_per_track;
        disk->geometry.sector_size = image->bytes_per_sector;
    }
    return err;
}

static void rcpmfs_close(uft_disk_t *disk) {
    if (disk && disk->plugin_data) {
        uft_disk_free((uft_disk_image_t*)disk->plugin_data);
        disk->plugin_data = NULL;
    }
}

static uft_error_t rcpmfs_read_track(uft_disk_t *disk, int cyl, int head,
                                      uft_track_t *track) {
    uft_disk_image_t *image = (uft_disk_image_t*)disk->plugin_data;
    if (!image || !track) return UFT_ERR_INVALID_PARAM;
    
    size_t idx = cyl * image->heads + head;
    if (idx >= (size_t)(image->tracks * image->heads)) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    uft_track_t *src = image->track_data[idx];
    if (!src) return UFT_ERR_INVALID_PARAM;
    
    track->cylinder = cyl;
    track->head = head;
    track->sector_count = src->sector_count;
    track->encoding = src->encoding;
    
    for (uint8_t s = 0; s < src->sector_count; s++) {
        track->sectors[s] = src->sectors[s];
        if (src->sectors[s].data) {
            track->sectors[s].data = malloc(src->sectors[s].data_size);
            if (track->sectors[s].data) {
                memcpy(track->sectors[s].data, src->sectors[s].data,
                       src->sectors[s].data_size);
            }
        }
    }
    
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_rcpmfs = {
    .name = "RCPMFS",
    .description = "Remote CP/M File System Container",
    .extensions = "rcpmfs,rcpm",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = rcpmfs_probe_plugin,
    .open = rcpmfs_open,
    .close = rcpmfs_close,
    .read_track = rcpmfs_read_track,
};

UFT_REGISTER_FORMAT_PLUGIN(rcpmfs)
