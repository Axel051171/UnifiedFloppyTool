/**
 * @file uft_opus.c
 * @brief OPUS Discovery disk format implementation
 * @version 3.9.0
 * 
 * OPUS Discovery for ZX Spectrum: 40 tracks, SS, 18 sectors, 256 bytes.
 * Reference: libdsk drvopus.c
 */

#include "uft/formats/uft_opus.h"
#include "uft/uft_format_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Probe Function
 * ============================================================================ */

bool uft_opus_probe(const uint8_t *data, size_t size, int *confidence) {
    /* Check exact file size */
    if (size != OPUS_DISK_SIZE) {
        return false;
    }
    
    /* Check directory structure on track 0 */
    /* Directory entries start at sector 1 */
    const uint8_t *dir_start = data + OPUS_SECTOR_SIZE;  /* Skip sector 0 */
    
    int valid_entries = 0;
    int used_entries = 0;
    
    for (int i = 0; i < OPUS_DIR_ENTRIES; i++) {
        const opus_dir_entry_t *entry = (const opus_dir_entry_t *)(dir_start + i * OPUS_DIR_ENTRY_SIZE);
        
        /* Check status byte */
        if (entry->status == 0 || entry->status == 1) {
            valid_entries++;
            if (entry->status == 1) {
                used_entries++;
                
                /* Validate filename - should be ASCII printable or space */
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
    
    /* Require at least some valid directory structure */
    if (valid_entries >= 10) {
        if (confidence) *confidence = 70;
        return true;
    }
    
    /* Fall back to size-only detection */
    if (confidence) *confidence = 40;
    return true;
}

/* ============================================================================
 * Read Implementation
 * ============================================================================ */

uft_error_t uft_opus_read_mem(const uint8_t *data, size_t size,
                              uft_disk_image_t **out_disk,
                              opus_read_result_t *result) {
    if (!data || !out_disk) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Initialize result */
    if (result) {
        memset(result, 0, sizeof(*result));
        result->image_size = size;
    }
    
    /* Validate size */
    if (size != OPUS_DISK_SIZE) {
        if (result) {
            result->error = UFT_ERR_FORMAT;
            result->error_detail = "Invalid OPUS disk size";
        }
        return UFT_ERR_FORMAT;
    }
    
    /* Allocate disk image */
    uft_disk_image_t *disk = uft_disk_alloc(OPUS_CYLINDERS, OPUS_HEADS);
    if (!disk) {
        return UFT_ERR_MEMORY;
    }
    
    disk->format = UFT_FMT_RAW;
    snprintf(disk->format_name, sizeof(disk->format_name), "OPUS");
    disk->sectors_per_track = OPUS_SECTORS;
    disk->bytes_per_sector = OPUS_SECTOR_SIZE;
    
    /* Read track data */
    size_t data_pos = 0;
    
    for (uint8_t c = 0; c < OPUS_CYLINDERS; c++) {
        uft_track_t *track = uft_track_alloc(OPUS_SECTORS, 0);
        if (!track) {
            uft_disk_free(disk);
            return UFT_ERR_MEMORY;
        }
        
        track->cylinder = c;
        track->head = 0;
        track->encoding = UFT_ENC_MFM;
        
        for (uint8_t s = 0; s < OPUS_SECTORS; s++) {
            uft_sector_t *sect = &track->sectors[s];
            sect->id.cylinder = c;
            sect->id.head = 0;
            sect->id.sector = s + OPUS_FIRST_SECTOR;
            sect->id.size_code = 1;  /* 256 bytes */
            sect->status = UFT_SECTOR_OK;
            
            sect->data = malloc(OPUS_SECTOR_SIZE);
            sect->data_size = OPUS_SECTOR_SIZE;
            
            if (sect->data) {
                memcpy(sect->data, data + data_pos, OPUS_SECTOR_SIZE);
            }
            data_pos += OPUS_SECTOR_SIZE;
            track->sector_count++;
        }
        
        disk->track_data[c] = track;
    }
    
    if (result) {
        result->success = true;
        result->cylinders = OPUS_CYLINDERS;
        result->heads = OPUS_HEADS;
        result->sectors = OPUS_SECTORS;
        result->sector_size = OPUS_SECTOR_SIZE;
    }
    
    *out_disk = disk;
    return UFT_OK;
}

uft_error_t uft_opus_read(const char *path,
                          uft_disk_image_t **out_disk,
                          opus_read_result_t *result) {
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
    
    uft_error_t err = uft_opus_read_mem(data, size, out_disk, result);
    free(data);
    
    return err;
}

/* ============================================================================
 * Write Implementation
 * ============================================================================ */

uft_error_t uft_opus_write(const uft_disk_image_t *disk,
                           const char *path) {
    if (!disk || !path) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Create output buffer */
    uint8_t *output = malloc(OPUS_DISK_SIZE);
    if (!output) {
        return UFT_ERR_MEMORY;
    }
    memset(output, 0xE5, OPUS_DISK_SIZE);
    
    /* Write track data */
    size_t data_pos = 0;
    
    for (uint8_t c = 0; c < OPUS_CYLINDERS && c < disk->tracks; c++) {
        uft_track_t *track = disk->track_data[c];
        
        for (uint8_t s = 0; s < OPUS_SECTORS; s++) {
            if (track && s < track->sector_count && track->sectors[s].data) {
                size_t copy_size = (track->sectors[s].data_size < OPUS_SECTOR_SIZE) ?
                                   track->sectors[s].data_size : OPUS_SECTOR_SIZE;
                memcpy(output + data_pos, track->sectors[s].data, copy_size);
            }
            data_pos += OPUS_SECTOR_SIZE;
        }
    }
    
    /* Write file */
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        free(output);
        return UFT_ERR_IO;
    }
    
    size_t written = fwrite(output, 1, OPUS_DISK_SIZE, fp);
    fclose(fp);
    free(output);
    
    return (written == OPUS_DISK_SIZE) ? UFT_OK : UFT_ERR_IO;
}

/* ============================================================================
 * Directory Functions
 * ============================================================================ */

uft_error_t uft_opus_read_directory(const uft_disk_image_t *disk,
                                    opus_dir_entry_t *entries,
                                    size_t max_entries,
                                    size_t *entry_count) {
    if (!disk || !entries || max_entries == 0) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Directory is on track 0, starting at sector 1 */
    uft_track_t *track0 = disk->track_data[0];
    if (!track0) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    size_t count = 0;
    size_t sector_offset = 0;
    
    for (int s = 1; s < track0->sector_count && count < max_entries; s++) {
        if (!track0->sectors[s].data) continue;
        
        /* Each sector holds multiple directory entries */
        int entries_per_sector = track0->sectors[s].data_size / OPUS_DIR_ENTRY_SIZE;
        
        for (int e = 0; e < entries_per_sector && count < max_entries; e++) {
            opus_dir_entry_t *src = (opus_dir_entry_t *)(track0->sectors[s].data + 
                                                         e * OPUS_DIR_ENTRY_SIZE);
            
            if (src->status == 1) {  /* Used entry */
                memcpy(&entries[count], src, sizeof(opus_dir_entry_t));
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

static bool opus_probe_plugin(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    (void)file_size;
    return uft_opus_probe(data, size, confidence);
}

static uft_error_t opus_open(uft_disk_t *disk, const char *path, bool read_only) {
    (void)read_only;
    uft_disk_image_t *image = NULL;
    uft_error_t err = uft_opus_read(path, &image, NULL);
    if (err == UFT_OK && image) {
        disk->plugin_data = image;
        disk->geometry.cylinders = image->tracks;
        disk->geometry.heads = image->heads;
        disk->geometry.sectors = image->sectors_per_track;
        disk->geometry.sector_size = image->bytes_per_sector;
    }
    return err;
}

static void opus_close(uft_disk_t *disk) {
    if (disk && disk->plugin_data) {
        uft_disk_free((uft_disk_image_t*)disk->plugin_data);
        disk->plugin_data = NULL;
    }
}

static uft_error_t opus_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track) {
    uft_disk_image_t *image = (uft_disk_image_t*)disk->plugin_data;
    if (!image || !track || head != 0) return UFT_ERR_INVALID_PARAM;
    
    if (cyl >= image->tracks) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    uft_track_t *src = image->track_data[cyl];
    if (!src) return UFT_ERR_INVALID_PARAM;
    
    track->cylinder = cyl;
    track->head = 0;
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

const uft_format_plugin_t uft_format_plugin_opus = {
    .name = "OPUS",
    .description = "OPUS Discovery (ZX Spectrum)",
    .extensions = "opd,opus",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = opus_probe_plugin,
    .open = opus_open,
    .close = opus_close,
    .read_track = opus_read_track,
};

UFT_REGISTER_FORMAT_PLUGIN(opus)
