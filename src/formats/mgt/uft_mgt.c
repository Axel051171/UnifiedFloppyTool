/**
 * @file uft_mgt.c
 * @brief MGT disk format implementation (+D and DISCiPLE)
 * @version 3.9.0
 * 
 * MGT +D/DISCiPLE format: 80 tracks, DS, 10 sectors, 512 bytes.
 * Reference: libdsk drvmgt.c
 */

#include "uft/formats/uft_mgt.h"
#include "uft/uft_format_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Probe Function
 * ============================================================================ */

bool uft_mgt_probe(const uint8_t *data, size_t size, int *confidence) {
    /* Check for valid MGT sizes */
    if (size != MGT_DISK_SIZE && size != MGT_40_DISK_SIZE) {
        return false;
    }
    
    /* Check directory structure */
    /* Directory is on track 0, side 0, sectors 1-4 */
    int valid_entries = 0;
    int used_entries = 0;
    
    /* Each sector holds 2 directory entries (256 bytes each) */
    for (int s = 0; s < MGT_SECTORS_PER_DIR; s++) {
        size_t sector_offset = s * MGT_SECTOR_SIZE;
        
        for (int e = 0; e < 2; e++) {
            const mgt_dir_entry_t *entry = (const mgt_dir_entry_t *)(data + sector_offset + e * MGT_DIR_ENTRY_SIZE);
            
            /* Check type byte */
            if (entry->type == 0) {
                valid_entries++;  /* Free entry */
            } else if (entry->type >= 1 && entry->type <= 11) {
                valid_entries++;
                used_entries++;
                
                /* Validate filename */
                bool valid_name = true;
                for (int j = 0; j < 10; j++) {
                    char c = entry->filename[j];
                    if (c != 0 && c != ' ' && (c < 32 || c > 126)) {
                        valid_name = false;
                        break;
                    }
                }
                if (!valid_name) valid_entries--;
            }
        }
    }
    
    if (valid_entries >= 4) {
        if (confidence) *confidence = 75;
        return true;
    }
    
    /* Fall back to size-only detection */
    if (confidence) *confidence = 50;
    return true;
}

/* ============================================================================
 * Read Implementation
 * ============================================================================ */

uft_error_t uft_mgt_read_mem(const uint8_t *data, size_t size,
                             uft_disk_image_t **out_disk,
                             mgt_read_result_t *result) {
    if (!data || !out_disk) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Initialize result */
    if (result) {
        memset(result, 0, sizeof(*result));
        result->image_size = size;
    }
    
    /* Determine geometry from size */
    uint8_t cylinders;
    if (size == MGT_DISK_SIZE) {
        cylinders = MGT_CYLINDERS;
    } else if (size == MGT_40_DISK_SIZE) {
        cylinders = MGT_40_CYLINDERS;
    } else {
        if (result) {
            result->error = UFT_ERR_FORMAT;
            result->error_detail = "Invalid MGT disk size";
        }
        return UFT_ERR_FORMAT;
    }
    
    /* Allocate disk image */
    uft_disk_image_t *disk = uft_disk_alloc(cylinders, MGT_HEADS);
    if (!disk) {
        return UFT_ERR_MEMORY;
    }
    
    disk->format = UFT_FMT_RAW;
    snprintf(disk->format_name, sizeof(disk->format_name), "MGT");
    disk->sectors_per_track = MGT_SECTORS;
    disk->bytes_per_sector = MGT_SECTOR_SIZE;
    
    /* Read track data - MGT interleaves sides */
    size_t data_pos = 0;
    
    for (uint8_t c = 0; c < cylinders; c++) {
        for (uint8_t h = 0; h < MGT_HEADS; h++) {
            size_t idx = c * MGT_HEADS + h;
            
            uft_track_t *track = uft_track_alloc(MGT_SECTORS, 0);
            if (!track) {
                uft_disk_free(disk);
                return UFT_ERR_MEMORY;
            }
            
            track->track_num = c;
            track->head = h;
            track->encoding = UFT_ENC_MFM;
            
            for (uint8_t s = 0; s < MGT_SECTORS; s++) {
                uft_sector_t *sect = &track->sectors[s];
                sect->id.cylinder = c;
                sect->id.head = h;
                sect->id.sector = s + MGT_FIRST_SECTOR;
                sect->id.size_code = 2;  /* 512 bytes */
                sect->status = UFT_SECTOR_OK;
                
                sect->data = malloc(MGT_SECTOR_SIZE);
                sect->data_size = MGT_SECTOR_SIZE;
                
                if (sect->data) {
                    memcpy(sect->data, data + data_pos, MGT_SECTOR_SIZE);
                }
                data_pos += MGT_SECTOR_SIZE;
                track->sector_count++;
            }
            
            disk->track_data[idx] = track;
        }
    }
    
    if (result) {
        result->success = true;
        result->cylinders = cylinders;
        result->heads = MGT_HEADS;
        result->sectors = MGT_SECTORS;
        result->sector_size = MGT_SECTOR_SIZE;
    }
    
    *out_disk = disk;
    return UFT_OK;
}

uft_error_t uft_mgt_read(const char *path,
                         uft_disk_image_t **out_disk,
                         mgt_read_result_t *result) {
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
    
    uft_error_t err = uft_mgt_read_mem(data, size, out_disk, result);
    free(data);
    
    return err;
}

/* ============================================================================
 * Write Implementation
 * ============================================================================ */

uft_error_t uft_mgt_write(const uft_disk_image_t *disk,
                          const char *path) {
    if (!disk || !path) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Calculate output size */
    size_t disk_size = (size_t)disk->tracks * disk->heads * MGT_TRACK_SIZE;
    
    uint8_t *output = malloc(disk_size);
    if (!output) {
        return UFT_ERR_MEMORY;
    }
    memset(output, 0xE5, disk_size);
    
    /* Write track data */
    size_t data_pos = 0;
    
    for (uint16_t c = 0; c < disk->tracks; c++) {
        for (uint8_t h = 0; h < disk->heads; h++) {
            size_t idx = c * disk->heads + h;
            uft_track_t *track = disk->track_data[idx];
            
            for (uint8_t s = 0; s < MGT_SECTORS; s++) {
                if (track && s < track->sector_count && track->sectors[s].data) {
                    memcpy(output + data_pos, track->sectors[s].data, MGT_SECTOR_SIZE);
                }
                data_pos += MGT_SECTOR_SIZE;
            }
        }
    }
    
    /* Write file */
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        free(output);
        return UFT_ERR_IO;
    }
    
    size_t written = fwrite(output, 1, disk_size, fp);
    fclose(fp);
    free(output);
    
    return (written == disk_size) ? UFT_OK : UFT_ERR_IO;
}

/* ============================================================================
 * Directory Functions
 * ============================================================================ */

uft_error_t uft_mgt_read_directory(const uft_disk_image_t *disk,
                                   mgt_dir_entry_t *entries,
                                   size_t max_entries,
                                   size_t *entry_count) {
    if (!disk || !entries || max_entries == 0) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Directory is on track 0, side 0, sectors 1-4 */
    uft_track_t *track0 = disk->track_data[0];
    if (!track0) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    size_t count = 0;
    
    for (int s = 0; s < MGT_SECTORS_PER_DIR && count < max_entries; s++) {
        if (s >= track0->sector_count || !track0->sectors[s].data) continue;
        
        for (int e = 0; e < 2 && count < max_entries; e++) {
            mgt_dir_entry_t *src = (mgt_dir_entry_t *)(track0->sectors[s].data + 
                                                       e * MGT_DIR_ENTRY_SIZE);
            
            if (src->type >= 1 && src->type <= 11) {
                memcpy(&entries[count], src, sizeof(mgt_dir_entry_t));
                count++;
            }
        }
    }
    
    if (entry_count) {
        *entry_count = count;
    }
    
    return UFT_OK;
}

/* ============================================================================
 * Format Plugin Registration
 * ============================================================================ */

static bool mgt_probe_plugin(const uint8_t *data, size_t size,
                             size_t file_size, int *confidence) {
    (void)file_size;
    return uft_mgt_probe(data, size, confidence);
}

static uft_error_t mgt_open(uft_disk_t *disk, const char *path, bool read_only) {
    (void)read_only;
    uft_disk_image_t *image = NULL;
    uft_error_t err = uft_mgt_read(path, &image, NULL);
    if (err == UFT_OK && image) {
        disk->plugin_data = image;
        disk->geometry.cylinders = image->tracks;
        disk->geometry.heads = image->heads;
        disk->geometry.sectors = image->sectors_per_track;
        disk->geometry.sector_size = image->bytes_per_sector;
    }
    return err;
}

static void mgt_close(uft_disk_t *disk) {
    if (disk && disk->plugin_data) {
        uft_disk_free((uft_disk_image_t*)disk->plugin_data);
        disk->plugin_data = NULL;
    }
}

static uft_error_t mgt_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track) {
    uft_disk_image_t *image = (uft_disk_image_t*)disk->plugin_data;
    if (!image || !track) return UFT_ERR_INVALID_PARAM;
    
    size_t idx = cyl * image->heads + head;
    if (idx >= (size_t)(image->tracks * image->heads)) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    uft_track_t *src = image->track_data[idx];
    if (!src) return UFT_ERR_INVALID_PARAM;
    
    track->track_num = cyl;
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

const uft_format_plugin_t uft_format_plugin_mgt = {
    .name = "MGT",
    .description = "MGT +D/DISCiPLE (ZX Spectrum)",
    .extensions = "mgt,img",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = mgt_probe_plugin,
    .open = mgt_open,
    .close = mgt_close,
    .read_track = mgt_read_track,
};

UFT_REGISTER_FORMAT_PLUGIN(mgt)
