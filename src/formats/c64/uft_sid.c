/**
 * @file uft_sid.c
 * @brief SID Music File Format Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/c64/uft_sid.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint16_t read_be16(const uint8_t *data)
{
    return (data[0] << 8) | data[1];
}

static uint32_t read_be32(const uint8_t *data)
{
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

static void write_be16(uint8_t *data, uint16_t value)
{
    data[0] = (value >> 8) & 0xFF;
    data[1] = value & 0xFF;
}

static void write_be32(uint8_t *data, uint32_t value)
{
    data[0] = (value >> 24) & 0xFF;
    data[1] = (value >> 16) & 0xFF;
    data[2] = (value >> 8) & 0xFF;
    data[3] = value & 0xFF;
}

static void copy_string(char *dest, const uint8_t *src, int len)
{
    memcpy(dest, src, len);
    dest[len] = '\0';
    /* Trim trailing spaces/nulls */
    for (int i = len - 1; i >= 0 && (dest[i] == ' ' || dest[i] == '\0'); i--) {
        dest[i] = '\0';
    }
}

/* ============================================================================
 * Detection & Validation
 * ============================================================================ */

bool sid_detect(const uint8_t *data, size_t size)
{
    if (!data || size < 4) {
        return false;
    }
    return (memcmp(data, SID_MAGIC_PSID, 4) == 0 ||
            memcmp(data, SID_MAGIC_RSID, 4) == 0);
}

bool sid_validate(const uint8_t *data, size_t size)
{
    if (!sid_detect(data, size)) {
        return false;
    }
    
    uint16_t version = read_be16(data + 4);
    if (version < 1 || version > 4) {
        return false;
    }
    
    uint16_t data_offset = read_be16(data + 6);
    if (data_offset < SID_HEADER_V1 || data_offset > size) {
        return false;
    }
    
    /* RSID requires version 2+ */
    if (memcmp(data, SID_MAGIC_RSID, 4) == 0 && version < 2) {
        return false;
    }
    
    return true;
}

/* ============================================================================
 * Image Management
 * ============================================================================ */

int sid_open(const uint8_t *data, size_t size, sid_image_t *image)
{
    if (!data || !image || size < SID_HEADER_V1) {
        return -1;
    }
    
    if (!sid_validate(data, size)) {
        return -2;
    }
    
    memset(image, 0, sizeof(sid_image_t));
    
    /* Copy data */
    image->data = malloc(size);
    if (!image->data) return -3;
    
    memcpy(image->data, data, size);
    image->size = size;
    image->owns_data = true;
    
    /* Parse header */
    memcpy(image->header.magic, data, 4);
    image->header.version = read_be16(data + 4);
    image->header.data_offset = read_be16(data + 6);
    image->header.load_address = read_be16(data + 8);
    image->header.init_address = read_be16(data + 10);
    image->header.play_address = read_be16(data + 12);
    image->header.songs = read_be16(data + 14);
    image->header.start_song = read_be16(data + 16);
    image->header.speed = read_be32(data + 18);
    
    memcpy(image->header.name, data + 22, 32);
    memcpy(image->header.author, data + 54, 32);
    memcpy(image->header.released, data + 86, 32);
    
    /* v2+ fields */
    if (image->header.version >= 2 && image->header.data_offset >= SID_HEADER_V2) {
        image->header.flags = read_be16(data + 118);
        image->header.start_page = data[120];
        image->header.page_length = data[121];
        image->header.second_sid = data[122];
        image->header.third_sid = data[123];
    }
    
    /* Set C64 data pointer */
    image->c64_data = image->data + image->header.data_offset;
    image->c64_data_size = size - image->header.data_offset;
    
    /* Determine actual load address */
    if (image->header.load_address == 0 && image->c64_data_size >= 2) {
        image->actual_load_addr = image->c64_data[0] | (image->c64_data[1] << 8);
        image->c64_data += 2;
        image->c64_data_size -= 2;
    } else {
        image->actual_load_addr = image->header.load_address;
    }
    
    return 0;
}

int sid_load(const char *filename, sid_image_t *image)
{
    if (!filename || !image) return -1;
    
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -2;
    
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(fp);
        return -3;
    }
    
    if (fread(data, 1, size, fp) != size) {
        free(data);
        fclose(fp);
        return -4;
    }
    fclose(fp);
    
    int result = sid_open(data, size, image);
    free(data);
    
    return result;
}

int sid_save(const sid_image_t *image, const char *filename)
{
    if (!image || !filename || !image->data) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -2;
    
    size_t written = fwrite(image->data, 1, image->size, fp);
    fclose(fp);
    
    return (written == image->size) ? 0 : -3;
}

void sid_close(sid_image_t *image)
{
    if (!image) return;
    
    if (image->owns_data) {
        free(image->data);
    }
    
    memset(image, 0, sizeof(sid_image_t));
}

/* ============================================================================
 * SID Info
 * ============================================================================ */

int sid_get_info(const sid_image_t *image, sid_info_t *info)
{
    if (!image || !info) return -1;
    
    memset(info, 0, sizeof(sid_info_t));
    
    info->type = (memcmp(image->header.magic, SID_MAGIC_RSID, 4) == 0) ?
                 SID_TYPE_RSID : SID_TYPE_PSID;
    info->version = image->header.version;
    
    copy_string(info->name, (uint8_t*)image->header.name, 32);
    copy_string(info->author, (uint8_t*)image->header.author, 32);
    copy_string(info->released, (uint8_t*)image->header.released, 32);
    
    info->load_address = image->actual_load_addr;
    info->init_address = image->header.init_address;
    info->play_address = image->header.play_address;
    info->songs = image->header.songs;
    info->start_song = image->header.start_song;
    
    if (image->header.version >= 2) {
        info->clock = (image->header.flags >> 2) & 0x03;
        info->sid_model = (image->header.flags >> 4) & 0x03;
    }
    
    info->data_size = image->c64_data_size;
    info->end_address = image->actual_load_addr + image->c64_data_size - 1;
    
    return 0;
}

void sid_get_name(const sid_image_t *image, char *name)
{
    if (!image || !name) return;
    copy_string(name, (uint8_t*)image->header.name, 32);
}

void sid_get_author(const sid_image_t *image, char *author)
{
    if (!image || !author) return;
    copy_string(author, (uint8_t*)image->header.author, 32);
}

void sid_get_released(const sid_image_t *image, char *released)
{
    if (!image || !released) return;
    copy_string(released, (uint8_t*)image->header.released, 32);
}

bool sid_song_uses_cia(const sid_image_t *image, int song)
{
    if (!image || song < 0 || song >= 32) return false;
    return (image->header.speed & (1 << song)) != 0;
}

/* ============================================================================
 * Data Extraction
 * ============================================================================ */

int sid_get_c64_data(const sid_image_t *image, const uint8_t **data, size_t *size)
{
    if (!image || !data || !size) return -1;
    
    *data = image->c64_data;
    *size = image->c64_data_size;
    return 0;
}

int sid_extract_prg(const sid_image_t *image, uint8_t *prg_data,
                    size_t max_size, size_t *extracted)
{
    if (!image || !prg_data) return -1;
    
    size_t prg_size = 2 + image->c64_data_size;
    if (prg_size > max_size) return -2;
    
    /* Write load address */
    prg_data[0] = image->actual_load_addr & 0xFF;
    prg_data[1] = (image->actual_load_addr >> 8) & 0xFF;
    
    /* Write data */
    memcpy(prg_data + 2, image->c64_data, image->c64_data_size);
    
    if (extracted) *extracted = prg_size;
    return 0;
}

int sid_save_prg(const sid_image_t *image, const char *filename)
{
    if (!image || !filename) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -2;
    
    /* Write load address */
    uint8_t load[2] = {
        image->actual_load_addr & 0xFF,
        (image->actual_load_addr >> 8) & 0xFF
    };
    fwrite(load, 1, 2, fp);
    
    /* Write data */
    fwrite(image->c64_data, 1, image->c64_data_size, fp);
    fclose(fp);
    
    return 0;
}

/* ============================================================================
 * SID Creation
 * ============================================================================ */

int sid_create(sid_type_t type, uint16_t version, sid_image_t *image)
{
    if (!image || version < 1 || version > 4) return -1;
    if (type == SID_TYPE_RSID && version < 2) return -2;
    
    memset(image, 0, sizeof(sid_image_t));
    
    uint16_t header_size = (version >= 2) ? SID_HEADER_V2 : SID_HEADER_V1;
    
    image->data = calloc(1, header_size + 1024);
    if (!image->data) return -3;
    
    image->size = header_size;
    image->owns_data = true;
    
    /* Write magic */
    memcpy(image->data, (type == SID_TYPE_RSID) ? SID_MAGIC_RSID : SID_MAGIC_PSID, 4);
    memcpy(image->header.magic, image->data, 4);
    
    /* Write version and offset */
    write_be16(image->data + 4, version);
    write_be16(image->data + 6, header_size);
    
    image->header.version = version;
    image->header.data_offset = header_size;
    image->header.songs = 1;
    image->header.start_song = 1;
    
    write_be16(image->data + 14, 1);  /* songs */
    write_be16(image->data + 16, 1);  /* start_song */
    
    return 0;
}

void sid_set_metadata(sid_image_t *image, const char *name,
                      const char *author, const char *released)
{
    if (!image) return;
    
    if (name) {
        memset(image->header.name, 0, 32);
        strncpy(image->header.name, name, 32);
        memcpy(image->data + 22, image->header.name, 32);
    }
    
    if (author) {
        memset(image->header.author, 0, 32);
        strncpy(image->header.author, author, 32);
        memcpy(image->data + 54, image->header.author, 32);
    }
    
    if (released) {
        memset(image->header.released, 0, 32);
        strncpy(image->header.released, released, 32);
        memcpy(image->data + 86, image->header.released, 32);
    }
}

void sid_set_addresses(sid_image_t *image, uint16_t load,
                       uint16_t init, uint16_t play)
{
    if (!image) return;
    
    image->header.load_address = load;
    image->header.init_address = init;
    image->header.play_address = play;
    
    write_be16(image->data + 8, load);
    write_be16(image->data + 10, init);
    write_be16(image->data + 12, play);
}

void sid_set_songs(sid_image_t *image, uint16_t songs, uint16_t start_song)
{
    if (!image) return;
    
    image->header.songs = songs;
    image->header.start_song = start_song;
    
    write_be16(image->data + 14, songs);
    write_be16(image->data + 16, start_song);
}

int sid_set_data(sid_image_t *image, const uint8_t *data, size_t size)
{
    if (!image || !data || size == 0) return -1;
    
    size_t new_size = image->header.data_offset + size;
    uint8_t *new_data = realloc(image->data, new_size);
    if (!new_data) return -2;
    
    image->data = new_data;
    memcpy(image->data + image->header.data_offset, data, size);
    image->size = new_size;
    
    image->c64_data = image->data + image->header.data_offset;
    image->c64_data_size = size;
    image->actual_load_addr = image->header.load_address;
    
    return 0;
}

int sid_from_prg(const uint8_t *prg_data, size_t prg_size,
                 const char *name, const char *author,
                 uint16_t init, uint16_t play, sid_image_t *image)
{
    if (!prg_data || prg_size < 3 || !image) return -1;
    
    /* Extract load address from PRG */
    uint16_t load = prg_data[0] | (prg_data[1] << 8);
    
    int ret = sid_create(SID_TYPE_PSID, 2, image);
    if (ret != 0) return ret;
    
    sid_set_metadata(image, name ? name : "Unknown", 
                     author ? author : "Unknown", "");
    sid_set_addresses(image, load, init ? init : load, play);
    
    /* Add data without load address */
    return sid_set_data(image, prg_data + 2, prg_size - 2);
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

const char *sid_clock_name(uint8_t clock)
{
    switch (clock) {
        case SID_CLOCK_PAL:  return "PAL";
        case SID_CLOCK_NTSC: return "NTSC";
        case SID_CLOCK_ANY:  return "PAL/NTSC";
        default: return "Unknown";
    }
}

const char *sid_model_name(uint8_t model)
{
    switch (model) {
        case SID_MODEL_6581: return "6581";
        case SID_MODEL_8580: return "8580";
        case SID_MODEL_ANY:  return "6581/8580";
        default: return "Unknown";
    }
}

uint16_t sid_decode_address(uint8_t addr)
{
    if (addr == SID_ADDR_NONE) return 0;
    if (addr <= 7) return 0xD400 + (addr * 0x20);
    if (addr <= 0x10) return 0xDE00 + ((addr - 8) * 0x20);
    return 0;
}

void sid_print_info(const sid_image_t *image, FILE *fp)
{
    if (!image || !fp) return;
    
    sid_info_t info;
    sid_get_info(image, &info);
    
    fprintf(fp, "SID File Information:\n");
    fprintf(fp, "  Type: %s v%d\n", 
            info.type == SID_TYPE_RSID ? "RSID" : "PSID", info.version);
    fprintf(fp, "  Name: %s\n", info.name);
    fprintf(fp, "  Author: %s\n", info.author);
    fprintf(fp, "  Released: %s\n", info.released);
    fprintf(fp, "  Load: $%04X, Init: $%04X, Play: $%04X\n",
            info.load_address, info.init_address, info.play_address);
    fprintf(fp, "  Songs: %d, Default: %d\n", info.songs, info.start_song);
    fprintf(fp, "  Data: %zu bytes ($%04X-$%04X)\n",
            info.data_size, info.load_address, info.end_address);
    
    if (info.version >= 2) {
        fprintf(fp, "  Clock: %s, Model: %s\n",
                sid_clock_name(info.clock), sid_model_name(info.sid_model));
    }
}
