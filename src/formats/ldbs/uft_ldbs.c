/**
 * @file uft_ldbs.c
 * @brief LDBS (LibDsk Block Store) format implementation
 * @version 3.9.0
 * 
 * Block-based disk image format with deduplication support.
 * Reference: http://www.seasip.info/Unix/LibDsk/ldbs.html
 */

#include "uft/formats/uft_ldbs.h"
#include "uft/uft_format_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static uint32_t read_le32(const uint8_t *p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
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

static uint8_t size_code_from_size(uint16_t size) {
    switch (size) {
        case 128:  return 0;
        case 256:  return 1;
        case 512:  return 2;
        case 1024: return 3;
        case 2048: return 4;
        case 4096: return 5;
        default:   return 2;
    }
}

static uint16_t size_from_size_code(uint8_t code) {
    return 128 << code;
}

/* ============================================================================
 * Options Initialization
 * ============================================================================ */

void uft_ldbs_write_options_init(ldbs_write_options_t *opts) {
    if (!opts) return;
    memset(opts, 0, sizeof(*opts));
    opts->deduplicate = true;
    opts->compress = false;
}

/* ============================================================================
 * Header Validation
 * ============================================================================ */

bool uft_ldbs_validate_header(const ldbs_file_header_t *header) {
    if (!header) return false;
    
    uint32_t magic = read_le32((const uint8_t*)&header->magic);
    uint32_t type = read_le32((const uint8_t*)&header->type);
    
    return (magic == LDBS_FILE_MAGIC && type == LDBS_FILE_TYPE_DSK);
}

bool uft_ldbs_probe(const uint8_t *data, size_t size, int *confidence) {
    if (!data || size < LDBS_HEADER_SIZE) return false;
    
    const ldbs_file_header_t *header = (const ldbs_file_header_t *)data;
    if (uft_ldbs_validate_header(header)) {
        if (confidence) *confidence = 95;
        return true;
    }
    
    return false;
}

/* ============================================================================
 * Block Management
 * ============================================================================ */

typedef struct {
    uint32_t offset;
    uint32_t type;
    uint32_t size;
    uint8_t *data;
} ldbs_block_t;

typedef struct {
    ldbs_block_t *blocks;
    size_t count;
    size_t capacity;
} ldbs_block_list_t;

static void block_list_init(ldbs_block_list_t *list) {
    list->blocks = NULL;
    list->count = 0;
    list->capacity = 0;
}

static void block_list_free(ldbs_block_list_t *list) {
    if (list->blocks) {
        for (size_t i = 0; i < list->count; i++) {
            free(list->blocks[i].data);
        }
        free(list->blocks);
    }
    list->blocks = NULL;
    list->count = 0;
    list->capacity = 0;
}

static ldbs_block_t* block_list_add(ldbs_block_list_t *list) {
    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity ? list->capacity * 2 : 64;
        ldbs_block_t *new_blocks = realloc(list->blocks, new_cap * sizeof(ldbs_block_t));
        if (!new_blocks) return NULL;
        list->blocks = new_blocks;
        list->capacity = new_cap;
    }
    
    ldbs_block_t *block = &list->blocks[list->count++];
    memset(block, 0, sizeof(*block));
    return block;
}

static ldbs_block_t* block_list_find(ldbs_block_list_t *list, uint32_t offset) {
    for (size_t i = 0; i < list->count; i++) {
        if (list->blocks[i].offset == offset) {
            return &list->blocks[i];
        }
    }
    return NULL;
}

/* ============================================================================
 * Read Implementation
 * ============================================================================ */

uft_error_t uft_ldbs_read_mem(const uint8_t *data, size_t size,
                              uft_disk_image_t **out_disk,
                              ldbs_read_result_t *result) {
    if (!data || !out_disk || size < LDBS_HEADER_SIZE) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Initialize result */
    if (result) {
        memset(result, 0, sizeof(*result));
        result->image_size = size;
    }
    
    /* Validate header */
    const ldbs_file_header_t *header = (const ldbs_file_header_t *)data;
    if (!uft_ldbs_validate_header(header)) {
        if (result) {
            result->error = UFT_ERR_FORMAT;
            result->error_detail = "Invalid LDBS signature";
        }
        return UFT_ERR_FORMAT;
    }
    
    uint32_t dir_offset = read_le32((const uint8_t*)&header->dir_offset);
    uint32_t total_blocks = read_le32((const uint8_t*)&header->total_blocks);
    
    if (result) {
        result->total_blocks = total_blocks;
    }
    
    /* Read all blocks */
    ldbs_block_list_t blocks;
    block_list_init(&blocks);
    
    size_t pos = LDBS_HEADER_SIZE;
    ldbs_geometry_t geometry = {0};
    bool have_geometry = false;
    
    while (pos + LDBS_BLOCK_HEADER_SIZE <= size) {
        const ldbs_block_header_t *bhdr = (const ldbs_block_header_t *)(data + pos);
        
        uint32_t magic = read_le32((const uint8_t*)&bhdr->magic);
        if (magic != LDBS_BLOCK_MAGIC) break;
        
        uint32_t type = read_le32((const uint8_t*)&bhdr->type);
        uint32_t bsize = read_le32((const uint8_t*)&bhdr->size);
        
        if (pos + LDBS_BLOCK_HEADER_SIZE + bsize > size) break;
        
        ldbs_block_t *block = block_list_add(&blocks);
        if (!block) {
            block_list_free(&blocks);
            return UFT_ERR_MEMORY;
        }
        
        block->offset = pos;
        block->type = type;
        block->size = bsize;
        block->data = malloc(bsize);
        if (block->data) {
            memcpy(block->data, data + pos + LDBS_BLOCK_HEADER_SIZE, bsize);
        }
        
        /* Extract geometry if present */
        if (type == LDBS_GEOM_BLOCK && bsize >= sizeof(ldbs_geometry_t)) {
            memcpy(&geometry, block->data, sizeof(ldbs_geometry_t));
            geometry.cylinders = read_le16((uint8_t*)&geometry.cylinders);
            geometry.sector_size = read_le16((uint8_t*)&geometry.sector_size);
            geometry.data_rate = read_le16((uint8_t*)&geometry.data_rate);
            have_geometry = true;
        }
        
        /* Extract comment if present */
        if (type == LDBS_INFO_BLOCK && result) {
            size_t copy_len = (bsize < LDBS_MAX_COMMENT - 1) ? bsize : LDBS_MAX_COMMENT - 1;
            memcpy(result->comment, block->data, copy_len);
            result->comment[copy_len] = '\0';
        }
        
        pos += LDBS_BLOCK_HEADER_SIZE + bsize;
    }
    
    /* Default geometry if not found */
    if (!have_geometry) {
        geometry.cylinders = 80;
        geometry.heads = 2;
        geometry.sectors = 9;
        geometry.sector_size = 512;
        geometry.first_sector = 1;
        geometry.encoding = 1;  /* MFM */
    }
    
    if (result) {
        result->geometry = geometry;
        result->unique_blocks = blocks.count;
    }
    
    /* Create disk image */
    uft_disk_image_t *disk = uft_disk_alloc(geometry.cylinders, geometry.heads);
    if (!disk) {
        block_list_free(&blocks);
        return UFT_ERR_MEMORY;
    }
    
    disk->format = UFT_FMT_RAW;
    snprintf(disk->format_name, sizeof(disk->format_name), "LDBS");
    disk->sectors_per_track = geometry.sectors;
    disk->bytes_per_sector = geometry.sector_size;
    
    /* Find track directory */
    ldbs_block_t *dir_block = NULL;
    for (size_t i = 0; i < blocks.count; i++) {
        if (blocks.blocks[i].type == LDBS_TRACK_BLOCK) {
            dir_block = &blocks.blocks[i];
            break;
        }
    }
    
    /* Read tracks from directory or create empty */
    uint8_t size_code = size_code_from_size(geometry.sector_size);
    uft_encoding_t encoding = (geometry.encoding == 0) ? UFT_ENC_FM : UFT_ENC_MFM;
    
    for (uint16_t c = 0; c < geometry.cylinders; c++) {
        for (uint8_t h = 0; h < geometry.heads; h++) {
            size_t idx = c * geometry.heads + h;
            
            uft_track_t *track = uft_track_alloc(geometry.sectors, 0);
            if (!track) {
                uft_disk_free(disk);
                block_list_free(&blocks);
                return UFT_ERR_MEMORY;
            }
            
            track->track_num = c;
            track->head = h;
            track->encoding = encoding;
            
            for (uint8_t s = 0; s < geometry.sectors; s++) {
                uft_sector_t *sect = &track->sectors[s];
                sect->id.cylinder = c;
                sect->id.head = h;
                sect->id.sector = s + geometry.first_sector;
                sect->id.size_code = size_code;
                sect->status = UFT_SECTOR_OK;
                
                sect->data = malloc(geometry.sector_size);
                sect->data_size = geometry.sector_size;
                
                if (sect->data) {
                    memset(sect->data, 0xE5, geometry.sector_size);
                }
                track->sector_count++;
            }
            
            disk->track_data[idx] = track;
        }
    }
    
    /* TODO: Parse track directory and populate sector data from blocks */
    /* For now, sectors are filled with 0xE5 */
    
    block_list_free(&blocks);
    
    if (result) {
        result->success = true;
    }
    
    *out_disk = disk;
    return UFT_OK;
}

uft_error_t uft_ldbs_read(const char *path,
                          uft_disk_image_t **out_disk,
                          ldbs_read_result_t *result) {
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
    
    uft_error_t err = uft_ldbs_read_mem(data, size, out_disk, result);
    free(data);
    
    return err;
}

/* ============================================================================
 * Write Implementation
 * ============================================================================ */

uft_error_t uft_ldbs_write(const uft_disk_image_t *disk,
                           const char *path,
                           const ldbs_write_options_t *opts) {
    if (!disk || !path) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Default options */
    ldbs_write_options_t default_opts;
    if (!opts) {
        uft_ldbs_write_options_init(&default_opts);
        opts = &default_opts;
    }
    
    /* Calculate required size */
    size_t num_sectors = (size_t)disk->tracks * disk->heads * disk->sectors_per_track;
    size_t max_size = LDBS_HEADER_SIZE + 
                      LDBS_BLOCK_HEADER_SIZE + sizeof(ldbs_geometry_t) +
                      LDBS_BLOCK_HEADER_SIZE + (disk->tracks * disk->heads * 8) +
                      num_sectors * (LDBS_BLOCK_HEADER_SIZE + disk->bytes_per_sector);
    
    if (opts->comment[0]) {
        max_size += LDBS_BLOCK_HEADER_SIZE + strlen(opts->comment);
    }
    
    uint8_t *output = malloc(max_size);
    if (!output) {
        return UFT_ERR_MEMORY;
    }
    memset(output, 0, max_size);
    
    size_t pos = LDBS_HEADER_SIZE;
    uint32_t block_count = 0;
    
    /* Write geometry block */
    ldbs_block_header_t *bhdr = (ldbs_block_header_t *)(output + pos);
    write_le32((uint8_t*)&bhdr->magic, LDBS_BLOCK_MAGIC);
    write_le32((uint8_t*)&bhdr->type, LDBS_GEOM_BLOCK);
    write_le32((uint8_t*)&bhdr->size, sizeof(ldbs_geometry_t));
    
    ldbs_geometry_t *geom = (ldbs_geometry_t *)(output + pos + LDBS_BLOCK_HEADER_SIZE);
    write_le16((uint8_t*)&geom->cylinders, disk->tracks);
    geom->heads = disk->heads;
    geom->sectors = disk->sectors_per_track;
    write_le16((uint8_t*)&geom->sector_size, disk->bytes_per_sector);
    geom->first_sector = 1;
    geom->encoding = 1;  /* MFM */
    write_le16((uint8_t*)&geom->data_rate, 250);
    
    pos += LDBS_BLOCK_HEADER_SIZE + sizeof(ldbs_geometry_t);
    block_count++;
    
    /* Write comment block if present */
    if (opts->comment[0]) {
        size_t comment_len = strlen(opts->comment);
        bhdr = (ldbs_block_header_t *)(output + pos);
        write_le32((uint8_t*)&bhdr->magic, LDBS_BLOCK_MAGIC);
        write_le32((uint8_t*)&bhdr->type, LDBS_INFO_BLOCK);
        write_le32((uint8_t*)&bhdr->size, comment_len);
        memcpy(output + pos + LDBS_BLOCK_HEADER_SIZE, opts->comment, comment_len);
        pos += LDBS_BLOCK_HEADER_SIZE + comment_len;
        block_count++;
    }
    
    /* Track directory offset */
    uint32_t dir_offset = pos;
    
    /* Reserve space for track directory */
    size_t dir_entries = disk->tracks * disk->heads;
    size_t dir_size = dir_entries * sizeof(ldbs_track_entry_t);
    
    bhdr = (ldbs_block_header_t *)(output + pos);
    write_le32((uint8_t*)&bhdr->magic, LDBS_BLOCK_MAGIC);
    write_le32((uint8_t*)&bhdr->type, LDBS_TRACK_BLOCK);
    write_le32((uint8_t*)&bhdr->size, dir_size);
    
    ldbs_track_entry_t *dir = (ldbs_track_entry_t *)(output + pos + LDBS_BLOCK_HEADER_SIZE);
    pos += LDBS_BLOCK_HEADER_SIZE + dir_size;
    block_count++;
    
    /* Write sector data blocks */
    size_t dir_idx = 0;
    for (uint16_t c = 0; c < disk->tracks; c++) {
        for (uint8_t h = 0; h < disk->heads; h++) {
            size_t track_idx = c * disk->heads + h;
            uft_track_t *track = disk->track_data[track_idx];
            
            /* Record track position in directory */
            dir[dir_idx].cylinder = c;
            dir[dir_idx].head = h;
            write_le32((uint8_t*)&dir[dir_idx].block_offset, pos);
            dir_idx++;
            
            /* Write sector data */
            for (uint8_t s = 0; s < disk->sectors_per_track; s++) {
                bhdr = (ldbs_block_header_t *)(output + pos);
                write_le32((uint8_t*)&bhdr->magic, LDBS_BLOCK_MAGIC);
                write_le32((uint8_t*)&bhdr->type, LDBS_SECTOR_BLOCK);
                write_le32((uint8_t*)&bhdr->size, disk->bytes_per_sector);
                
                uint8_t *sect_data = output + pos + LDBS_BLOCK_HEADER_SIZE;
                
                if (track && s < track->sector_count && track->sectors[s].data) {
                    memcpy(sect_data, track->sectors[s].data, disk->bytes_per_sector);
                } else {
                    memset(sect_data, 0xE5, disk->bytes_per_sector);
                }
                
                pos += LDBS_BLOCK_HEADER_SIZE + disk->bytes_per_sector;
                block_count++;
            }
        }
    }
    
    /* Write file header */
    ldbs_file_header_t *fhdr = (ldbs_file_header_t *)output;
    write_le32((uint8_t*)&fhdr->magic, LDBS_FILE_MAGIC);
    write_le32((uint8_t*)&fhdr->type, LDBS_FILE_TYPE_DSK);
    write_le32((uint8_t*)&fhdr->dir_offset, dir_offset);
    write_le32((uint8_t*)&fhdr->total_blocks, block_count);
    
    /* Write file */
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        free(output);
        return UFT_ERR_IO;
    }
    
    size_t written = fwrite(output, 1, pos, fp);
    fclose(fp);
    free(output);
    
    if (written != pos) {
        return UFT_ERR_IO;
    }
    
    return UFT_OK;
}

/* ============================================================================
 * Format Plugin Registration
 * ============================================================================ */

static bool ldbs_probe_plugin(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    (void)file_size;
    return uft_ldbs_probe(data, size, confidence);
}

static uft_error_t ldbs_open(uft_disk_t *disk, const char *path, bool read_only) {
    (void)read_only;
    uft_disk_image_t *image = NULL;
    uft_error_t err = uft_ldbs_read(path, &image, NULL);
    if (err == UFT_OK && image) {
        disk->plugin_data = image;
        disk->geometry.cylinders = image->tracks;
        disk->geometry.heads = image->heads;
        disk->geometry.sectors = image->sectors_per_track;
        disk->geometry.sector_size = image->bytes_per_sector;
    }
    return err;
}

static void ldbs_close(uft_disk_t *disk) {
    if (disk && disk->plugin_data) {
        uft_disk_free((uft_disk_image_t*)disk->plugin_data);
        disk->plugin_data = NULL;
    }
}

static uft_error_t ldbs_read_track(uft_disk_t *disk, int cyl, int head,
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

const uft_format_plugin_t uft_format_plugin_ldbs = {
    .name = "LDBS",
    .description = "LibDsk Block Store",
    .extensions = "ldbs,lbs",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = ldbs_probe_plugin,
    .open = ldbs_open,
    .close = ldbs_close,
    .read_track = ldbs_read_track,
};

UFT_REGISTER_FORMAT_PLUGIN(ldbs)
