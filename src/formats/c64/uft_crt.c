/**
 * @file uft_crt.c
 * @brief CRT Cartridge Image Format Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/c64/uft_crt.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Cartridge Type Names
 * ============================================================================ */

static const char *crt_type_names[] = {
    "Normal cartridge",         /* 0 */
    "Action Replay",            /* 1 */
    "KCS Power Cartridge",      /* 2 */
    "Final Cartridge III",      /* 3 */
    "Simons' BASIC",            /* 4 */
    "Ocean type 1",             /* 5 */
    "Expert Cartridge",         /* 6 */
    "Fun Play, Power Play",     /* 7 */
    "Super Games",              /* 8 */
    "Atomic Power",             /* 9 */
    "Epyx Fastload",            /* 10 */
    "Westermann Learning",      /* 11 */
    "Rex Utility",              /* 12 */
    "Final Cartridge I",        /* 13 */
    "Magic Formel",             /* 14 */
    "C64 Game System",          /* 15 */
    "Warp Speed",               /* 16 */
    "Dinamic",                  /* 17 */
    "Zaxxon, Super Zaxxon",     /* 18 */
    "Magic Desk",               /* 19 */
    "Super Snapshot V5",        /* 20 */
    "Comal-80",                 /* 21 */
};

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

/* ============================================================================
 * Detection & Validation
 * ============================================================================ */

bool crt_detect(const uint8_t *data, size_t size)
{
    if (!data || size < CRT_HEADER_SIZE) {
        return false;
    }
    return memcmp(data, CRT_MAGIC, CRT_MAGIC_LEN) == 0;
}

bool crt_validate(const uint8_t *data, size_t size)
{
    if (!crt_detect(data, size)) {
        return false;
    }
    
    uint32_t header_len = read_be32(data + 16);
    if (header_len < CRT_HEADER_SIZE || header_len > size) {
        return false;
    }
    
    /* Check for at least one CHIP packet */
    if (size > header_len + CRT_CHIP_HEADER_SIZE) {
        if (memcmp(data + header_len, CRT_CHIP_MAGIC, 4) != 0) {
            return false;
        }
    }
    
    return true;
}

/* ============================================================================
 * Image Management
 * ============================================================================ */

int crt_open(const uint8_t *data, size_t size, crt_image_t *image)
{
    if (!data || !image || size < CRT_HEADER_SIZE) {
        return -1;
    }
    
    if (!crt_validate(data, size)) {
        return -2;
    }
    
    memset(image, 0, sizeof(crt_image_t));
    
    /* Copy data */
    image->data = malloc(size);
    if (!image->data) return -3;
    
    memcpy(image->data, data, size);
    image->size = size;
    image->owns_data = true;
    
    /* Parse header */
    memcpy(image->header.magic, data, 16);
    image->header.header_length = read_be32(data + 16);
    image->header.version = read_be16(data + 20);
    image->header.hw_type = read_be16(data + 22);
    image->header.exrom = data[24];
    image->header.game = data[25];
    image->header.subtype = data[26];
    memcpy(image->header.name, data + 32, 32);
    
    /* Count and parse CHIP packets */
    image->chips = calloc(CRT_MAX_CHIPS, sizeof(crt_chip_t));
    if (!image->chips) {
        free(image->data);
        return -4;
    }
    
    size_t offset = image->header.header_length;
    image->num_chips = 0;
    
    while (offset + CRT_CHIP_HEADER_SIZE <= size && image->num_chips < CRT_MAX_CHIPS) {
        if (memcmp(image->data + offset, CRT_CHIP_MAGIC, 4) != 0) {
            break;
        }
        
        crt_chip_t *chip = &image->chips[image->num_chips];
        chip->file_offset = offset;
        
        memcpy(chip->header.magic, image->data + offset, 4);
        chip->header.packet_length = read_be32(image->data + offset + 4);
        chip->header.chip_type = read_be16(image->data + offset + 8);
        chip->header.bank = read_be16(image->data + offset + 10);
        chip->header.load_address = read_be16(image->data + offset + 12);
        chip->header.rom_size = read_be16(image->data + offset + 14);
        
        chip->data = image->data + offset + CRT_CHIP_HEADER_SIZE;
        chip->data_size = chip->header.rom_size;
        
        image->num_chips++;
        offset += chip->header.packet_length;
    }
    
    return 0;
}

int crt_load(const char *filename, crt_image_t *image)
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
    
    int result = crt_open(data, size, image);
    free(data);
    
    return result;
}

int crt_save(const crt_image_t *image, const char *filename)
{
    if (!image || !filename || !image->data) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -2;
    
    size_t written = fwrite(image->data, 1, image->size, fp);
    fclose(fp);
    
    return (written == image->size) ? 0 : -3;
}

void crt_close(crt_image_t *image)
{
    if (!image) return;
    
    if (image->owns_data) {
        free(image->data);
    }
    free(image->chips);
    
    memset(image, 0, sizeof(crt_image_t));
}

/* ============================================================================
 * Cartridge Info
 * ============================================================================ */

int crt_get_info(const crt_image_t *image, crt_info_t *info)
{
    if (!image || !info) return -1;
    
    memset(info, 0, sizeof(crt_info_t));
    
    /* Copy name, remove trailing spaces */
    memcpy(info->name, image->header.name, 32);
    info->name[32] = '\0';
    for (int i = 31; i >= 0 && (info->name[i] == ' ' || info->name[i] == '\0'); i--) {
        info->name[i] = '\0';
    }
    
    info->type = image->header.hw_type;
    info->version = image->header.version;
    info->exrom = image->header.exrom;
    info->game = image->header.game;
    info->num_chips = image->num_chips;
    
    /* Calculate total ROM size and max bank */
    int max_bank = 0;
    for (int i = 0; i < image->num_chips; i++) {
        info->total_rom_size += image->chips[i].data_size;
        if (image->chips[i].header.bank > max_bank) {
            max_bank = image->chips[i].header.bank;
        }
    }
    info->num_banks = max_bank + 1;
    
    return 0;
}

const char *crt_type_name(crt_type_t type)
{
    if (type < sizeof(crt_type_names) / sizeof(crt_type_names[0])) {
        return crt_type_names[type];
    }
    
    switch (type) {
        case CRT_TYPE_EASYFLASH: return "EasyFlash";
        case CRT_TYPE_RETRO_REPLAY: return "Retro Replay";
        case CRT_TYPE_MMC64: return "MMC64";
        case CRT_TYPE_IDE64: return "IDE64";
        case CRT_TYPE_GMOD2: return "GMod2";
        default: return "Unknown";
    }
}

void crt_get_name(const crt_image_t *image, char *name)
{
    if (!image || !name) return;
    
    memcpy(name, image->header.name, 32);
    name[32] = '\0';
    
    /* Trim trailing spaces */
    for (int i = 31; i >= 0 && name[i] == ' '; i--) {
        name[i] = '\0';
    }
}

/* ============================================================================
 * CHIP Operations
 * ============================================================================ */

int crt_get_chip_count(const crt_image_t *image)
{
    return image ? image->num_chips : 0;
}

int crt_get_chip(const crt_image_t *image, int index, crt_chip_t *chip)
{
    if (!image || !chip || index < 0 || index >= image->num_chips) {
        return -1;
    }
    
    *chip = image->chips[index];
    return 0;
}

int crt_extract_rom(const crt_image_t *image, uint8_t *buffer,
                    size_t buffer_size, size_t *extracted)
{
    if (!image || !buffer) return -1;
    
    size_t total = 0;
    
    for (int i = 0; i < image->num_chips && total < buffer_size; i++) {
        size_t to_copy = image->chips[i].data_size;
        if (total + to_copy > buffer_size) {
            to_copy = buffer_size - total;
        }
        
        memcpy(buffer + total, image->chips[i].data, to_copy);
        total += to_copy;
    }
    
    if (extracted) *extracted = total;
    return 0;
}

/* ============================================================================
 * CRT Creation
 * ============================================================================ */

int crt_create(const char *name, crt_type_t type, uint8_t exrom, uint8_t game,
               crt_image_t *image)
{
    if (!image) return -1;
    
    memset(image, 0, sizeof(crt_image_t));
    
    /* Allocate initial buffer */
    image->data = calloc(1, CRT_HEADER_SIZE + 1024);
    if (!image->data) return -2;
    
    image->size = CRT_HEADER_SIZE;
    image->owns_data = true;
    
    /* Write header */
    memcpy(image->data, CRT_MAGIC, CRT_MAGIC_LEN);
    write_be32(image->data + 16, CRT_HEADER_SIZE);
    write_be16(image->data + 20, 0x0100);  /* Version 1.0 */
    write_be16(image->data + 22, type);
    image->data[24] = exrom;
    image->data[25] = game;
    
    if (name) {
        size_t len = strlen(name);
        if (len > 32) len = 32;
        memcpy(image->data + 32, name, len);
    }
    
    /* Parse back into header struct */
    memcpy(image->header.magic, image->data, 16);
    image->header.header_length = CRT_HEADER_SIZE;
    image->header.version = 0x0100;
    image->header.hw_type = type;
    image->header.exrom = exrom;
    image->header.game = game;
    if (name) {
        strncpy(image->header.name, name, 32);
    }
    
    /* Allocate chips array */
    image->chips = calloc(CRT_MAX_CHIPS, sizeof(crt_chip_t));
    if (!image->chips) {
        free(image->data);
        return -3;
    }
    
    return 0;
}

int crt_add_chip(crt_image_t *image, uint16_t bank, uint16_t load_address,
                 const uint8_t *data, size_t size, uint16_t chip_type)
{
    if (!image || !data || size == 0) return -1;
    if (image->num_chips >= CRT_MAX_CHIPS) return -2;
    
    size_t packet_len = CRT_CHIP_HEADER_SIZE + size;
    size_t new_size = image->size + packet_len;
    
    /* Expand buffer */
    uint8_t *new_data = realloc(image->data, new_size);
    if (!new_data) return -3;
    
    image->data = new_data;
    
    /* Write CHIP header */
    uint8_t *chip_ptr = image->data + image->size;
    memcpy(chip_ptr, CRT_CHIP_MAGIC, 4);
    write_be32(chip_ptr + 4, packet_len);
    write_be16(chip_ptr + 8, chip_type);
    write_be16(chip_ptr + 10, bank);
    write_be16(chip_ptr + 12, load_address);
    write_be16(chip_ptr + 14, size);
    
    /* Write ROM data */
    memcpy(chip_ptr + CRT_CHIP_HEADER_SIZE, data, size);
    
    /* Update chip info */
    crt_chip_t *chip = &image->chips[image->num_chips];
    chip->file_offset = image->size;
    memcpy(chip->header.magic, CRT_CHIP_MAGIC, 4);
    chip->header.packet_length = packet_len;
    chip->header.chip_type = chip_type;
    chip->header.bank = bank;
    chip->header.load_address = load_address;
    chip->header.rom_size = size;
    chip->data = chip_ptr + CRT_CHIP_HEADER_SIZE;
    chip->data_size = size;
    
    image->num_chips++;
    image->size = new_size;
    
    return 0;
}

int crt_create_8k(const char *name, const uint8_t *rom_data, crt_image_t *image)
{
    if (!rom_data || !image) return -1;
    
    int ret = crt_create(name, CRT_TYPE_NORMAL, 0, 1, image);
    if (ret != 0) return ret;
    
    /* Add 8K chip at $8000 */
    return crt_add_chip(image, 0, 0x8000, rom_data, 8192, CRT_ROM_TYPE_ROM);
}

int crt_create_16k(const char *name, const uint8_t *rom_data, crt_image_t *image)
{
    if (!rom_data || !image) return -1;
    
    int ret = crt_create(name, CRT_TYPE_NORMAL, 0, 0, image);
    if (ret != 0) return ret;
    
    /* Add ROML at $8000 */
    ret = crt_add_chip(image, 0, 0x8000, rom_data, 8192, CRT_ROM_TYPE_ROM);
    if (ret != 0) return ret;
    
    /* Add ROMH at $A000 */
    return crt_add_chip(image, 0, 0xA000, rom_data + 8192, 8192, CRT_ROM_TYPE_ROM);
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

void crt_print_info(const crt_image_t *image, FILE *fp)
{
    if (!image || !fp) return;
    
    crt_info_t info;
    crt_get_info(image, &info);
    
    fprintf(fp, "CRT Cartridge Image\n");
    fprintf(fp, "  Name: %s\n", info.name);
    fprintf(fp, "  Type: %s (%d)\n", crt_type_name(info.type), info.type);
    fprintf(fp, "  Version: %d.%d\n", info.version >> 8, info.version & 0xFF);
    fprintf(fp, "  EXROM: %d, GAME: %d\n", info.exrom, info.game);
    fprintf(fp, "  CHIPs: %d\n", info.num_chips);
    fprintf(fp, "  Banks: %d\n", info.num_banks);
    fprintf(fp, "  Total ROM: %zu bytes\n", info.total_rom_size);
    
    fprintf(fp, "\nCHIP Packets:\n");
    for (int i = 0; i < image->num_chips; i++) {
        crt_chip_t *chip = &image->chips[i];
        fprintf(fp, "  %2d: Bank %2d, $%04X, %5d bytes, Type %d\n",
                i, chip->header.bank, chip->header.load_address,
                chip->header.rom_size, chip->header.chip_type);
    }
}
