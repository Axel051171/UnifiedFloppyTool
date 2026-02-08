/**
 * @file uft_snes.c
 * @brief Super Nintendo / Super Famicom ROM Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/nintendo/uft_snes.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Detection
 * ============================================================================ */

bool snes_has_copier_header(const uint8_t *data, size_t size)
{
    if (!data) return false;
    
    /* Check if size suggests copier header */
    size_t rom_size = size - SNES_COPIER_HEADER;
    
    /* Valid ROM sizes are powers of 2 (or close) */
    if (size > SNES_COPIER_HEADER && (rom_size % 32768) == 0) {
        /* Additional check: copier headers often have specific patterns */
        return true;
    }
    
    /* Also check by size modulo */
    return (size % 1024) == 512;
}

static int snes_score_header(const uint8_t *data, size_t offset, size_t size)
{
    if (offset + 32 > size) return -1000;
    
    const uint8_t *hdr = data + offset;
    int score = 0;
    
    /* Check checksum complement */
    uint16_t checksum = hdr[28] | (hdr[29] << 8);
    uint16_t complement = hdr[30] | (hdr[31] << 8);
    
    if ((checksum ^ complement) == 0xFFFF) {
        score += 50;
    }
    
    /* Check map mode */
    uint8_t map = hdr[21];
    if ((map & 0x0F) <= 0x05 || map == 0x20 || map == 0x21 || 
        map == 0x23 || map == 0x30 || map == 0x31 || map == 0x35) {
        score += 20;
    }
    
    /* Check ROM size is reasonable */
    uint8_t rom_size_code = hdr[23];
    if (rom_size_code >= 0x08 && rom_size_code <= 0x0D) {
        score += 10;
    }
    
    /* Check SRAM size is reasonable */
    uint8_t sram_code = hdr[24];
    if (sram_code <= 0x08) {
        score += 5;
    }
    
    /* Check region code */
    uint8_t region = hdr[25];
    if (region <= 0x14) {
        score += 5;
    }
    
    /* Title should be printable ASCII or spaces */
    for (int i = 0; i < 21; i++) {
        uint8_t c = hdr[i];
        if ((c >= 0x20 && c <= 0x7E) || c == 0x00) {
            score += 1;
        }
    }
    
    return score;
}

size_t snes_find_header(const uint8_t *data, size_t size, bool has_copier)
{
    if (!data) return 0;
    
    size_t base = has_copier ? SNES_COPIER_HEADER : 0;
    size_t rom_size = size - base;
    
    /* Try different header locations */
    size_t offsets[] = {
        SNES_LOROM_HEADER,
        SNES_HIROM_HEADER,
        SNES_EXLOROM_HEADER,
        SNES_EXHIROM_HEADER
    };
    
    int best_score = -1000;
    size_t best_offset = SNES_LOROM_HEADER;
    
    for (int i = 0; i < 4; i++) {
        if (offsets[i] < rom_size) {
            int score = snes_score_header(data, base + offsets[i], size);
            if (score > best_score) {
                best_score = score;
                best_offset = offsets[i];
            }
        }
    }
    
    return base + best_offset;
}

bool snes_validate(const uint8_t *data, size_t size)
{
    if (!data || size < 32768) return false;
    
    bool has_copier = snes_has_copier_header(data, size);
    size_t hdr_offset = snes_find_header(data, size, has_copier);
    
    int score = snes_score_header(data, hdr_offset, size);
    return score >= 30;
}

const char *snes_mapping_name(snes_mapping_t mapping)
{
    switch (mapping & 0x3F) {
        case 0x20: return "LoROM";
        case 0x21: return "HiROM";
        case 0x23: return "SA-1 ROM";
        case 0x25: return "ExLoROM";
        case 0x30: return "LoROM + FastROM";
        case 0x31: return "HiROM + FastROM";
        case 0x32: return "ExLoROM (SDD-1)";
        case 0x35: return "ExHiROM";
        default:   return "Unknown";
    }
}

const char *snes_chip_name(snes_chip_t chip)
{
    switch (chip) {
        case SNES_CHIP_NONE:      return "ROM only";
        case SNES_CHIP_RAM:       return "ROM + RAM";
        case SNES_CHIP_SRAM:      return "ROM + SRAM";
        case SNES_CHIP_DSP:       return "ROM + DSP";
        case SNES_CHIP_DSP_RAM:   return "ROM + DSP + RAM";
        case SNES_CHIP_DSP_SRAM:  return "ROM + DSP + SRAM";
        case SNES_CHIP_FX:        return "SuperFX";
        case SNES_CHIP_FX_RAM:    return "SuperFX + RAM";
        case SNES_CHIP_FX_SRAM:   return "SuperFX + SRAM";
        case SNES_CHIP_FX2:       return "SuperFX2";
        case SNES_CHIP_OBC1:      return "OBC1";
        case SNES_CHIP_SA1:       return "SA-1";
        case SNES_CHIP_SA1_SRAM:  return "SA-1 + SRAM";
        case SNES_CHIP_SDD1:      return "S-DD1";
        case SNES_CHIP_SDD1_SRAM: return "S-DD1 + SRAM";
        case SNES_CHIP_SPC7110:   return "SPC7110";
        case SNES_CHIP_ST018:     return "ST018";
        case SNES_CHIP_CX4:       return "Cx4";
        default:                  return "Unknown";
    }
}

const char *snes_region_name(snes_region_t region)
{
    switch (region) {
        case SNES_REGION_JAPAN:       return "Japan";
        case SNES_REGION_USA:         return "USA";
        case SNES_REGION_EUROPE:      return "Europe";
        case SNES_REGION_SWEDEN:      return "Sweden";
        case SNES_REGION_FINLAND:     return "Finland";
        case SNES_REGION_DENMARK:     return "Denmark";
        case SNES_REGION_FRANCE:      return "France";
        case SNES_REGION_NETHERLANDS: return "Netherlands";
        case SNES_REGION_SPAIN:       return "Spain";
        case SNES_REGION_GERMANY:     return "Germany";
        case SNES_REGION_ITALY:       return "Italy";
        case SNES_REGION_CHINA:       return "China";
        case SNES_REGION_KOREA:       return "Korea";
        case SNES_REGION_INTERNATIONAL: return "International";
        case SNES_REGION_CANADA:      return "Canada";
        case SNES_REGION_BRAZIL:      return "Brazil";
        case SNES_REGION_AUSTRALIA:   return "Australia";
        default:                      return "Unknown";
    }
}

/* ============================================================================
 * ROM Operations
 * ============================================================================ */

int snes_open(const uint8_t *data, size_t size, snes_rom_t *rom)
{
    if (!data || !rom || size < 32768) return -1;
    
    memset(rom, 0, sizeof(snes_rom_t));
    
    /* Check for copier header */
    rom->has_copier_header = snes_has_copier_header(data, size);
    
    /* Find internal header */
    rom->header_offset = snes_find_header(data, size, rom->has_copier_header);
    
    /* Validate */
    if (!snes_validate(data, size)) return -2;
    
    /* Copy data */
    rom->data = malloc(size);
    if (!rom->data) return -3;
    
    memcpy(rom->data, data, size);
    rom->size = size;
    rom->owns_data = true;
    
    /* Parse header */
    const uint8_t *hdr = rom->data + rom->header_offset;
    memcpy(rom->header.title, hdr, 21);
    rom->header.map_mode = hdr[21];
    rom->header.rom_type = hdr[22];
    rom->header.rom_size = hdr[23];
    rom->header.sram_size = hdr[24];
    rom->header.region = hdr[25];
    rom->header.developer_id = hdr[26];
    rom->header.version = hdr[27];
    rom->header.checksum_comp = hdr[28] | (hdr[29] << 8);
    rom->header.checksum = hdr[30] | (hdr[31] << 8);
    
    return 0;
}

int snes_load(const char *filename, snes_rom_t *rom)
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
    
    int result = snes_open(data, size, rom);
    free(data);
    
    return result;
}

void snes_close(snes_rom_t *rom)
{
    if (!rom) return;
    
    if (rom->owns_data) {
        free(rom->data);
    }
    
    memset(rom, 0, sizeof(snes_rom_t));
}

int snes_get_info(const snes_rom_t *rom, snes_info_t *info)
{
    if (!rom || !info) return -1;
    
    memset(info, 0, sizeof(snes_info_t));
    
    /* Title */
    memcpy(info->title, rom->header.title, 21);
    info->title[21] = '\0';
    /* Trim trailing spaces */
    for (int i = 20; i >= 0 && info->title[i] == ' '; i--) {
        info->title[i] = '\0';
    }
    
    info->file_size = rom->size;
    info->rom_size = rom->has_copier_header ? rom->size - SNES_COPIER_HEADER : rom->size;
    info->has_copier_header = rom->has_copier_header;
    
    info->mapping = rom->header.map_mode;
    info->mapping_name = snes_mapping_name(rom->header.map_mode);
    
    info->chip = rom->header.rom_type;
    info->chip_name = snes_chip_name(rom->header.rom_type);
    
    /* SRAM size */
    if (rom->header.sram_size > 0) {
        info->sram_size = 1024 << rom->header.sram_size;
    }
    
    info->region = rom->header.region;
    info->region_name = snes_region_name(rom->header.region);
    info->version = rom->header.version;
    
    info->checksum = rom->header.checksum;
    info->calculated = snes_calculate_checksum(rom);
    info->checksum_valid = (info->checksum == info->calculated);
    
    info->is_hirom = (rom->header.map_mode & 0x01) != 0;
    info->is_fastrom = (rom->header.map_mode & 0x10) != 0;
    
    return 0;
}

/* ============================================================================
 * Checksum
 * ============================================================================ */

uint16_t snes_calculate_checksum(const snes_rom_t *rom)
{
    if (!rom || !rom->data) return 0;
    
    size_t base = rom->has_copier_header ? SNES_COPIER_HEADER : 0;
    size_t rom_size = rom->size - base;
    const uint8_t *data = rom->data + base;
    
    uint32_t sum = 0;
    
    /* Calculate sum of all bytes */
    for (size_t i = 0; i < rom_size; i++) {
        sum += data[i];
    }
    
    /* Mirror smaller ROMs to fill expected size */
    size_t expected = 1;
    while (expected < rom_size) expected <<= 1;
    
    if (expected > rom_size) {
        size_t mirror = rom_size;
        size_t remaining = expected - rom_size;
        while (remaining > 0) {
            size_t chunk = (remaining > mirror) ? mirror : remaining;
            for (size_t i = 0; i < chunk; i++) {
                sum += data[i % rom_size];
            }
            remaining -= chunk;
        }
    }
    
    return (uint16_t)(sum & 0xFFFF);
}

bool snes_verify_checksum(const snes_rom_t *rom)
{
    if (!rom) return false;
    return snes_calculate_checksum(rom) == rom->header.checksum;
}

int snes_fix_checksum(snes_rom_t *rom)
{
    if (!rom || !rom->data) return -1;
    
    uint16_t checksum = snes_calculate_checksum(rom);
    uint16_t complement = checksum ^ 0xFFFF;
    
    uint8_t *hdr = rom->data + rom->header_offset;
    hdr[28] = complement & 0xFF;
    hdr[29] = (complement >> 8) & 0xFF;
    hdr[30] = checksum & 0xFF;
    hdr[31] = (checksum >> 8) & 0xFF;
    
    rom->header.checksum = checksum;
    rom->header.checksum_comp = complement;
    
    return 0;
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

const uint8_t *snes_get_rom_data(const snes_rom_t *rom)
{
    if (!rom || !rom->data) return NULL;
    return rom->data + (rom->has_copier_header ? SNES_COPIER_HEADER : 0);
}

size_t snes_get_rom_size(const snes_rom_t *rom)
{
    if (!rom) return 0;
    return rom->size - (rom->has_copier_header ? SNES_COPIER_HEADER : 0);
}

int snes_strip_header(snes_rom_t *rom)
{
    if (!rom || !rom->data || !rom->has_copier_header) return -1;
    
    size_t new_size = rom->size - SNES_COPIER_HEADER;
    memmove(rom->data, rom->data + SNES_COPIER_HEADER, new_size);
    
    uint8_t *new_data = realloc(rom->data, new_size);
    if (new_data) {
        rom->data = new_data;
    }
    
    rom->size = new_size;
    rom->header_offset -= SNES_COPIER_HEADER;
    rom->has_copier_header = false;
    
    return 0;
}

void snes_print_info(const snes_rom_t *rom, FILE *fp)
{
    if (!rom || !fp) return;
    
    snes_info_t info;
    snes_get_info(rom, &info);
    
    fprintf(fp, "SNES ROM:\n");
    fprintf(fp, "  Title: %s\n", info.title);
    fprintf(fp, "  File Size: %zu bytes (%.2f Mbit)\n", 
            info.file_size, info.rom_size / 131072.0);
    fprintf(fp, "  Copier Header: %s\n", info.has_copier_header ? "Yes" : "No");
    fprintf(fp, "  Mapping: %s\n", info.mapping_name);
    fprintf(fp, "  Chip: %s\n", info.chip_name);
    fprintf(fp, "  SRAM: %u KB\n", info.sram_size / 1024);
    fprintf(fp, "  Region: %s\n", info.region_name);
    fprintf(fp, "  Version: 1.%d\n", info.version);
    fprintf(fp, "  Checksum: %04X (%s)\n", 
            info.checksum, info.checksum_valid ? "VALID" : "INVALID");
}
