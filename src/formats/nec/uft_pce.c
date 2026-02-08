/**
 * @file uft_pce.c
 * @brief NEC TurboGrafx-16 / PC Engine ROM Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/nec/uft_pce.h"
#include <stdlib.h>
#include <string.h>

/* CRC32 table */
static uint32_t crc32_table[256];
static bool crc32_init = false;

static void init_crc32_table(void)
{
    if (crc32_init) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        }
        crc32_table[i] = c;
    }
    crc32_init = true;
}

/* ============================================================================
 * Detection
 * ============================================================================ */

bool pce_has_header(const uint8_t *data, size_t size)
{
    if (!data || size < 512) return false;
    
    /* Check if size - 512 is a valid ROM size */
    size_t rom_size = size - 512;
    
    /* Valid PCE ROM sizes are typically powers of 2 or specific sizes */
    if (rom_size == PCE_SIZE_256K || rom_size == PCE_SIZE_384K ||
        rom_size == PCE_SIZE_512K || rom_size == PCE_SIZE_768K ||
        rom_size == PCE_SIZE_1M || rom_size == PCE_SIZE_2M) {
        
        /* Check header signature (some emulators use specific patterns) */
        /* Most common: all zeros or specific marker */
        bool all_zero = true;
        for (int i = 0; i < 16 && all_zero; i++) {
            if (data[i] != 0) all_zero = false;
        }
        
        if (all_zero) return true;
        
        /* Check for "HES" or other known header markers */
        if (memcmp(data, "HES", 3) == 0) return true;
    }
    
    return false;
}

pce_type_t pce_detect_type(const uint8_t *data, size_t size)
{
    if (!data || size < 8192) return PCE_TYPE_UNKNOWN;
    
    size_t offset = pce_has_header(data, size) ? 512 : 0;
    size_t rom_size = size - offset;
    
    /* Check for SuperGrafx - look for specific patterns */
    /* SuperGrafx games use bank $68-$7F for extra VRAM */
    
    /* Street Fighter II is 2.5MB */
    if (rom_size > PCE_SIZE_2M) {
        return PCE_TYPE_SF2;
    }
    
    /* Populous uses special mapper */
    /* Check for Populous signature */
    const uint8_t *rom = data + offset;
    if (rom_size >= PCE_SIZE_384K) {
        /* Populous check - it has specific pattern */
        /* This is simplified; real detection would use CRC database */
    }
    
    return PCE_TYPE_HUCARD;
}

pce_region_t pce_detect_region(const uint8_t *data, size_t size)
{
    /* PCE doesn't have clear region markers in ROM */
    /* Would need CRC database for accurate detection */
    return PCE_REGION_UNKNOWN;
}

const char *pce_type_name(pce_type_t type)
{
    switch (type) {
        case PCE_TYPE_HUCARD:       return "HuCard";
        case PCE_TYPE_SUPERGRAFX:   return "SuperGrafx";
        case PCE_TYPE_POPULOUS:     return "Populous";
        case PCE_TYPE_SF2:          return "Street Fighter II";
        case PCE_TYPE_CDROM:        return "CD-ROM System";
        default:                    return "Unknown";
    }
}

const char *pce_region_name(pce_region_t region)
{
    switch (region) {
        case PCE_REGION_JAPAN:  return "Japan (PC Engine)";
        case PCE_REGION_USA:    return "USA (TurboGrafx-16)";
        default:                return "Unknown";
    }
}

bool pce_validate(const uint8_t *data, size_t size)
{
    if (!data) return false;
    
    size_t offset = pce_has_header(data, size) ? 512 : 0;
    size_t rom_size = size - offset;
    
    /* Check for valid ROM size */
    if (rom_size < 8192) return false;
    if (rom_size > 4 * 1024 * 1024) return false;  /* Max 4MB */
    
    /* PCE ROMs should have reset vector in valid range */
    /* Reset vector at end of ROM */
    
    return true;
}

/* ============================================================================
 * ROM Operations
 * ============================================================================ */

int pce_open(const uint8_t *data, size_t size, pce_rom_t *rom)
{
    if (!data || !rom || size < 8192) return -1;
    
    memset(rom, 0, sizeof(pce_rom_t));
    
    if (!pce_validate(data, size)) return -2;
    
    rom->data = malloc(size);
    if (!rom->data) return -3;
    
    memcpy(rom->data, data, size);
    rom->size = size;
    rom->owns_data = true;
    rom->has_header = pce_has_header(data, size);
    rom->type = pce_detect_type(data, size);
    
    return 0;
}

int pce_load(const char *filename, pce_rom_t *rom)
{
    if (!filename || !rom) return -1;
    
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
    
    int result = pce_open(data, size, rom);
    free(data);
    
    return result;
}

void pce_close(pce_rom_t *rom)
{
    if (!rom) return;
    
    if (rom->owns_data) {
        free(rom->data);
    }
    
    memset(rom, 0, sizeof(pce_rom_t));
}

int pce_get_info(const pce_rom_t *rom, pce_info_t *info)
{
    if (!rom || !info) return -1;
    
    memset(info, 0, sizeof(pce_info_t));
    
    info->type = rom->type;
    info->file_size = rom->size;
    info->has_header = rom->has_header;
    info->rom_size = pce_get_rom_size(rom);
    info->region = pce_detect_region(rom->data, rom->size);
    
    /* Calculate CRC32 of ROM data (without header) */
    const uint8_t *rom_data = pce_get_rom_data(rom);
    size_t rom_size = pce_get_rom_size(rom);
    info->crc32 = pce_calc_crc32(rom_data, rom_size);
    
    /* Determine mapper */
    if (info->type == PCE_TYPE_SF2) {
        info->mapper = PCE_MAPPER_SF2;
    } else if (info->type == PCE_TYPE_POPULOUS) {
        info->mapper = PCE_MAPPER_POPULOUS;
    } else {
        info->mapper = PCE_MAPPER_NONE;
    }
    
    return 0;
}

const uint8_t *pce_get_rom_data(const pce_rom_t *rom)
{
    if (!rom || !rom->data) return NULL;
    return rom->data + (rom->has_header ? 512 : 0);
}

size_t pce_get_rom_size(const pce_rom_t *rom)
{
    if (!rom) return 0;
    return rom->size - (rom->has_header ? 512 : 0);
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

uint32_t pce_calc_crc32(const uint8_t *data, size_t size)
{
    if (!data) return 0;
    
    init_crc32_table();
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return crc ^ 0xFFFFFFFF;
}

int pce_strip_header(pce_rom_t *rom)
{
    if (!rom || !rom->has_header) return -1;
    
    size_t new_size = rom->size - 512;
    uint8_t *new_data = malloc(new_size);
    if (!new_data) return -2;
    
    memcpy(new_data, rom->data + 512, new_size);
    
    if (rom->owns_data) {
        free(rom->data);
    }
    
    rom->data = new_data;
    rom->size = new_size;
    rom->has_header = false;
    rom->owns_data = true;
    
    return 0;
}

void pce_print_info(const pce_rom_t *rom, FILE *fp)
{
    if (!rom || !fp) return;
    
    pce_info_t info;
    pce_get_info(rom, &info);
    
    fprintf(fp, "PC Engine / TurboGrafx-16 ROM:\n");
    fprintf(fp, "  Type: %s\n", pce_type_name(info.type));
    fprintf(fp, "  Region: %s\n", pce_region_name(info.region));
    fprintf(fp, "  File Size: %zu bytes (%.1f KB)\n",
            info.file_size, info.file_size / 1024.0);
    fprintf(fp, "  ROM Size: %zu bytes (%.1f KB)\n",
            info.rom_size, info.rom_size / 1024.0);
    fprintf(fp, "  Has Header: %s\n", info.has_header ? "Yes" : "No");
    fprintf(fp, "  CRC32: %08X\n", info.crc32);
}
