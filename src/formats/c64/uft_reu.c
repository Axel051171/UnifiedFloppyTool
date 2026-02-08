/**
 * @file uft_reu.c
 * @brief REU and GeoRAM Image Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/c64/uft_reu.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Type Names
 * ============================================================================ */

static const struct {
    reu_type_t type;
    const char *name;
    size_t size;
} reu_types[] = {
    { REU_TYPE_1700,    "Commodore 1700 (128KB)",   REU_SIZE_1700 },
    { REU_TYPE_1764,    "Commodore 1764 (256KB)",   REU_SIZE_1764 },
    { REU_TYPE_1750,    "Commodore 1750 (512KB)",   REU_SIZE_1750 },
    { REU_TYPE_1MB,     "REU 1MB",                  REU_SIZE_1MB },
    { REU_TYPE_2MB,     "REU 2MB",                  REU_SIZE_2MB },
    { REU_TYPE_4MB,     "REU 4MB",                  REU_SIZE_4MB },
    { REU_TYPE_8MB,     "REU 8MB",                  REU_SIZE_8MB },
    { REU_TYPE_16MB,    "REU 16MB",                 REU_SIZE_16MB },
    { REU_TYPE_GEORAM,  "GeoRAM",                   GEORAM_SIZE_512K },
    { REU_TYPE_BBGRAM,  "BBGRam",                   128 * 1024 },
    { REU_TYPE_UNKNOWN, "Unknown", 0 }
};

/* ============================================================================
 * Image Management
 * ============================================================================ */

int reu_create(reu_type_t type, reu_image_t *image)
{
    size_t size = reu_type_size(type);
    if (size == 0 || !image) return -1;
    
    return reu_create_sized(size, image);
}

int reu_create_sized(size_t size, reu_image_t *image)
{
    if (!image || size == 0) return -1;
    
    memset(image, 0, sizeof(reu_image_t));
    
    image->data = calloc(1, size);
    if (!image->data) return -2;
    
    image->size = size;
    image->type = reu_detect_type(size);
    image->owns_data = true;
    
    return 0;
}

int reu_open(const uint8_t *data, size_t size, reu_image_t *image)
{
    if (!data || !image || size == 0) return -1;
    
    memset(image, 0, sizeof(reu_image_t));
    
    image->data = malloc(size);
    if (!image->data) return -2;
    
    memcpy(image->data, data, size);
    image->size = size;
    image->type = reu_detect_type(size);
    image->owns_data = true;
    
    return 0;
}

int reu_load(const char *filename, reu_image_t *image)
{
    if (!filename || !image) return -1;
    
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -2;
    
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (size == 0) {
        fclose(fp);
        return -3;
    }
    
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(fp);
        return -4;
    }
    
    if (fread(data, 1, size, fp) != size) {
        free(data);
        fclose(fp);
        return -5;
    }
    fclose(fp);
    
    memset(image, 0, sizeof(reu_image_t));
    image->data = data;
    image->size = size;
    image->type = reu_detect_type(size);
    image->owns_data = true;
    
    return 0;
}

int reu_save(const reu_image_t *image, const char *filename)
{
    if (!image || !filename || !image->data) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -2;
    
    size_t written = fwrite(image->data, 1, image->size, fp);
    fclose(fp);
    
    return (written == image->size) ? 0 : -3;
}

void reu_close(reu_image_t *image)
{
    if (!image) return;
    
    if (image->owns_data) {
        free(image->data);
    }
    
    memset(image, 0, sizeof(reu_image_t));
}

/* ============================================================================
 * REU Info
 * ============================================================================ */

int reu_get_info(const reu_image_t *image, reu_info_t *info)
{
    if (!image || !info) return -1;
    
    memset(info, 0, sizeof(reu_info_t));
    
    info->type = image->type;
    info->size = image->size;
    info->num_pages = image->size / REU_PAGE_SIZE;
    info->num_banks = image->size / (64 * 1024);
    info->is_georam = (image->type == REU_TYPE_GEORAM);
    
    return 0;
}

const char *reu_type_name(reu_type_t type)
{
    for (size_t i = 0; i < sizeof(reu_types)/sizeof(reu_types[0]); i++) {
        if (reu_types[i].type == type) {
            return reu_types[i].name;
        }
    }
    return "Unknown";
}

reu_type_t reu_detect_type(size_t size)
{
    switch (size) {
        case REU_SIZE_1700:     return REU_TYPE_1700;
        case REU_SIZE_1764:     return REU_TYPE_1764;
        case REU_SIZE_1750:     return REU_TYPE_1750;
        case REU_SIZE_1MB:      return REU_TYPE_1MB;
        case REU_SIZE_2MB:      return REU_TYPE_2MB;
        case REU_SIZE_4MB:      return REU_TYPE_4MB;
        case REU_SIZE_8MB:      return REU_TYPE_8MB;
        case REU_SIZE_16MB:     return REU_TYPE_16MB;
        default:
            /* Check if power of 2 and >= 128KB */
            if (size >= 128*1024 && (size & (size - 1)) == 0) {
                return REU_TYPE_1750;  /* Generic REU */
            }
            return REU_TYPE_UNKNOWN;
    }
}

size_t reu_type_size(reu_type_t type)
{
    for (size_t i = 0; i < sizeof(reu_types)/sizeof(reu_types[0]); i++) {
        if (reu_types[i].type == type) {
            return reu_types[i].size;
        }
    }
    return 0;
}

/* ============================================================================
 * Memory Access
 * ============================================================================ */

uint8_t reu_read_byte(const reu_image_t *image, uint32_t address)
{
    if (!image || !image->data || address >= image->size) {
        return 0xFF;
    }
    return image->data[address];
}

void reu_write_byte(reu_image_t *image, uint32_t address, uint8_t value)
{
    if (!image || !image->data || address >= image->size) {
        return;
    }
    image->data[address] = value;
    image->modified = true;
}

size_t reu_read_block(const reu_image_t *image, uint32_t address,
                      uint8_t *buffer, size_t size)
{
    if (!image || !image->data || !buffer || address >= image->size) {
        return 0;
    }
    
    size_t available = image->size - address;
    size_t to_read = (size < available) ? size : available;
    
    memcpy(buffer, image->data + address, to_read);
    return to_read;
}

size_t reu_write_block(reu_image_t *image, uint32_t address,
                       const uint8_t *buffer, size_t size)
{
    if (!image || !image->data || !buffer || address >= image->size) {
        return 0;
    }
    
    size_t available = image->size - address;
    size_t to_write = (size < available) ? size : available;
    
    memcpy(image->data + address, buffer, to_write);
    image->modified = true;
    return to_write;
}

int reu_read_page(const reu_image_t *image, int bank, int page, uint8_t *buffer)
{
    if (!image || !buffer) return -1;
    
    uint32_t address = (bank * 65536) + (page * 256);
    if (address + 256 > image->size) return -2;
    
    memcpy(buffer, image->data + address, 256);
    return 0;
}

int reu_write_page(reu_image_t *image, int bank, int page, const uint8_t *buffer)
{
    if (!image || !buffer) return -1;
    
    uint32_t address = (bank * 65536) + (page * 256);
    if (address + 256 > image->size) return -2;
    
    memcpy(image->data + address, buffer, 256);
    image->modified = true;
    return 0;
}

/* ============================================================================
 * GeoRAM Emulation
 * ============================================================================ */

int georam_create(size_t size, reu_image_t *image)
{
    if (!image) return -1;
    
    /* Validate GeoRAM size (must be 512KB-4MB, power of 2) */
    if (size < GEORAM_SIZE_512K || size > GEORAM_SIZE_4MB) {
        return -2;
    }
    
    int ret = reu_create_sized(size, image);
    if (ret == 0) {
        image->type = REU_TYPE_GEORAM;
    }
    return ret;
}

uint8_t georam_read(const reu_image_t *image, const georam_state_t *state,
                    uint8_t offset)
{
    if (!image || !state || !image->data) return 0xFF;
    
    /* GeoRAM address: block * 16KB + page * 256 + offset */
    uint32_t address = (state->block * GEORAM_BLOCK_SIZE) + 
                       (state->page * GEORAM_PAGE_SIZE) + offset;
    
    if (address >= image->size) {
        /* Wrap around */
        address %= image->size;
    }
    
    return image->data[address];
}

void georam_write(reu_image_t *image, const georam_state_t *state,
                  uint8_t offset, uint8_t value)
{
    if (!image || !state || !image->data) return;
    
    uint32_t address = (state->block * GEORAM_BLOCK_SIZE) + 
                       (state->page * GEORAM_PAGE_SIZE) + offset;
    
    if (address >= image->size) {
        address %= image->size;
    }
    
    image->data[address] = value;
    image->modified = true;
}

void georam_set_block(georam_state_t *state, uint8_t block)
{
    if (state) {
        state->block = block;
    }
}

void georam_set_page(georam_state_t *state, uint8_t page)
{
    if (state) {
        state->page = page;
    }
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

void reu_fill(reu_image_t *image, uint8_t pattern)
{
    if (image && image->data) {
        memset(image->data, pattern, image->size);
        image->modified = true;
    }
}

void reu_clear(reu_image_t *image)
{
    reu_fill(image, 0);
}

int reu_compare(const reu_image_t *image1, const reu_image_t *image2)
{
    if (!image1 || !image2) return -1;
    if (!image1->data || !image2->data) return -2;
    if (image1->size != image2->size) return 1;
    
    return memcmp(image1->data, image2->data, image1->size);
}

void reu_print_info(const reu_image_t *image, FILE *fp)
{
    if (!image || !fp) return;
    
    reu_info_t info;
    reu_get_info(image, &info);
    
    fprintf(fp, "REU/GeoRAM Image\n");
    fprintf(fp, "  Type: %s\n", reu_type_name(info.type));
    fprintf(fp, "  Size: %zu bytes (%zuKB)\n", info.size, info.size / 1024);
    fprintf(fp, "  Pages: %zu\n", info.num_pages);
    fprintf(fp, "  Banks: %zu\n", info.num_banks);
    fprintf(fp, "  GeoRAM: %s\n", info.is_georam ? "Yes" : "No");
}

void reu_dump(const reu_image_t *image, FILE *fp, uint32_t start, size_t length)
{
    if (!image || !fp || !image->data) return;
    
    if (start >= image->size) return;
    if (start + length > image->size) {
        length = image->size - start;
    }
    
    for (size_t i = 0; i < length; i += 16) {
        fprintf(fp, "%06X:", (unsigned)(start + i));
        
        for (size_t j = 0; j < 16 && i + j < length; j++) {
            fprintf(fp, " %02X", image->data[start + i + j]);
        }
        
        fprintf(fp, "  ");
        
        for (size_t j = 0; j < 16 && i + j < length; j++) {
            uint8_t c = image->data[start + i + j];
            fprintf(fp, "%c", (c >= 32 && c < 127) ? c : '.');
        }
        
        fprintf(fp, "\n");
    }
}
