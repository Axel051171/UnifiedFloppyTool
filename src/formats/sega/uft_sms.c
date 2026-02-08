/**
 * @file uft_sms.c
 * @brief Sega Master System / Game Gear ROM Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/sega/uft_sms.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Detection
 * ============================================================================ */

bool sms_find_header(const uint8_t *data, size_t size, uint32_t *offset)
{
    if (!data) return false;
    
    /* Check common header locations */
    uint32_t offsets[] = {
        SMS_HEADER_OFFSET_7FF0,
        SMS_HEADER_OFFSET_3FF0,
        SMS_HEADER_OFFSET_1FF0
    };
    
    for (int i = 0; i < 3; i++) {
        if (size >= offsets[i] + SMS_HEADER_SIZE) {
            if (memcmp(data + offsets[i], SMS_SIGNATURE, SMS_SIGNATURE_SIZE) == 0) {
                if (offset) *offset = offsets[i];
                return true;
            }
        }
    }
    
    return false;
}

sms_console_t sms_detect_console(const uint8_t *data, size_t size)
{
    if (!data || size < 8192) return SMS_CONSOLE_UNKNOWN;
    
    uint32_t offset;
    if (sms_find_header(data, size, &offset)) {
        /* Check region byte for GG vs SMS */
        uint8_t region = data[offset + 15] & 0x0F;
        
        switch (region) {
            case 5:
            case 6:
            case 7:
                return SMS_CONSOLE_GG;
            case 3:
            case 4:
                return SMS_CONSOLE_SMS;
        }
    }
    
    /* Heuristic: small ROMs might be SG-1000 */
    if (size <= 32768) {
        /* SG-1000 typically uses different patterns */
        return SMS_CONSOLE_SMS;  /* Default to SMS */
    }
    
    return SMS_CONSOLE_SMS;
}

sms_mapper_t sms_detect_mapper(const uint8_t *data, size_t size)
{
    if (!data) return SMS_MAPPER_NONE;
    
    /* ROMs 48KB or smaller don't need mapper */
    if (size <= 48 * 1024) {
        return SMS_MAPPER_NONE;
    }
    
    /* Codemasters detection: check for specific patterns */
    /* Codemasters ROMs often have bank select at $0000 */
    if (size >= 128 * 1024) {
        /* Check for Codemasters signature patterns */
        /* They typically have specific reset vector patterns */
    }
    
    /* Default to Sega mapper for larger ROMs */
    return SMS_MAPPER_SEGA;
}

const char *sms_console_name(sms_console_t console)
{
    switch (console) {
        case SMS_CONSOLE_SG1000: return "SG-1000";
        case SMS_CONSOLE_SC3000: return "SC-3000";
        case SMS_CONSOLE_SMS:    return "Master System";
        case SMS_CONSOLE_GG:     return "Game Gear";
        default:                 return "Unknown";
    }
}

const char *sms_region_name(sms_region_t region)
{
    switch (region) {
        case SMS_REGION_SMS_JAPAN:  return "SMS Japan";
        case SMS_REGION_SMS_EXPORT: return "SMS Export";
        case SMS_REGION_GG_JAPAN:   return "GG Japan";
        case SMS_REGION_GG_EXPORT:  return "GG Export";
        case SMS_REGION_GG_INTL:    return "GG International";
        default:                    return "Unknown";
    }
}

const char *sms_mapper_name(sms_mapper_t mapper)
{
    switch (mapper) {
        case SMS_MAPPER_NONE:        return "None";
        case SMS_MAPPER_SEGA:        return "Sega";
        case SMS_MAPPER_CODEMASTERS: return "Codemasters";
        case SMS_MAPPER_KOREAN:      return "Korean";
        case SMS_MAPPER_MSX:         return "MSX";
        case SMS_MAPPER_NEMESIS:     return "Nemesis";
        case SMS_MAPPER_JANGGUN:     return "Janggun";
        case SMS_MAPPER_4PAK:        return "4-Pak";
        default:                     return "Unknown";
    }
}

bool sms_validate(const uint8_t *data, size_t size)
{
    if (!data) return false;
    
    /* Minimum size for a valid ROM */
    if (size < 8192) return false;
    
    /* Maximum size */
    if (size > 4 * 1024 * 1024) return false;
    
    /* Check for header (not all ROMs have one) */
    /* Just validate size is reasonable */
    return true;
}

/* ============================================================================
 * ROM Operations
 * ============================================================================ */

int sms_open(const uint8_t *data, size_t size, sms_rom_t *rom)
{
    if (!data || !rom) return -1;
    
    memset(rom, 0, sizeof(sms_rom_t));
    
    if (!sms_validate(data, size)) return -2;
    
    /* Find header */
    rom->has_header = sms_find_header(data, size, &rom->header_offset);
    
    if (rom->has_header) {
        const uint8_t *hdr = data + rom->header_offset;
        
        memcpy(rom->header.signature, hdr, 8);
        rom->header.reserved[0] = hdr[8];
        rom->header.reserved[1] = hdr[9];
        rom->header.checksum = hdr[10] | (hdr[11] << 8);
        rom->header.product_code[0] = hdr[12];
        rom->header.product_code[1] = hdr[13];
        rom->header.product_code[2] = hdr[14] >> 4;
        rom->header.version_region = hdr[15];
        rom->header.size_code = hdr[14] & 0x0F;
    }
    
    /* Detect console */
    rom->console = sms_detect_console(data, size);
    
    /* Copy data */
    rom->data = malloc(size);
    if (!rom->data) return -3;
    
    memcpy(rom->data, data, size);
    rom->size = size;
    rom->owns_data = true;
    
    return 0;
}

int sms_load(const char *filename, sms_rom_t *rom)
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
    
    int result = sms_open(data, size, rom);
    free(data);
    
    return result;
}

void sms_close(sms_rom_t *rom)
{
    if (!rom) return;
    
    if (rom->owns_data) {
        free(rom->data);
    }
    
    memset(rom, 0, sizeof(sms_rom_t));
}

int sms_get_info(const sms_rom_t *rom, sms_info_t *info)
{
    if (!rom || !info) return -1;
    
    memset(info, 0, sizeof(sms_info_t));
    
    info->console = rom->console;
    info->file_size = rom->size;
    info->has_header = rom->has_header;
    info->header_offset = rom->header_offset;
    info->mapper = sms_detect_mapper(rom->data, rom->size);
    
    if (rom->has_header) {
        info->checksum = rom->header.checksum;
        info->calc_checksum = sms_calc_checksum(rom->data, rom->size);
        info->checksum_valid = (info->checksum == info->calc_checksum);
        
        info->region = rom->header.version_region & 0x0F;
        info->version = (rom->header.version_region >> 4) & 0x0F;
        
        /* Product code (BCD) */
        info->product_code = 
            (rom->header.product_code[2] * 10000) +
            ((rom->header.product_code[1] >> 4) * 1000) +
            ((rom->header.product_code[1] & 0x0F) * 100) +
            ((rom->header.product_code[0] >> 4) * 10) +
            (rom->header.product_code[0] & 0x0F);
        
        info->rom_size = sms_size_from_code(rom->header.size_code);
    }
    
    return 0;
}

/* ============================================================================
 * Checksum
 * ============================================================================ */

uint16_t sms_calc_checksum(const uint8_t *data, size_t size)
{
    if (!data) return 0;
    
    uint16_t checksum = 0;
    
    /* Find header location to exclude from checksum */
    uint32_t header_offset;
    bool has_header = sms_find_header(data, size, &header_offset);
    
    /* Sum all bytes except header area */
    size_t check_end = has_header ? header_offset : size;
    
    for (size_t i = 0; i < check_end; i++) {
        checksum += data[i];
    }
    
    return checksum;
}

bool sms_verify_checksum(const sms_rom_t *rom)
{
    if (!rom || !rom->has_header) return false;
    
    uint16_t calc = sms_calc_checksum(rom->data, rom->size);
    return (calc == rom->header.checksum);
}

int sms_fix_checksum(sms_rom_t *rom)
{
    if (!rom || !rom->has_header) return -1;
    
    uint16_t checksum = sms_calc_checksum(rom->data, rom->size);
    
    /* Write checksum to header */
    rom->data[rom->header_offset + 10] = checksum & 0xFF;
    rom->data[rom->header_offset + 11] = (checksum >> 8) & 0xFF;
    rom->header.checksum = checksum;
    
    return 0;
}

/* ============================================================================
 * Size Conversion
 * ============================================================================ */

size_t sms_size_from_code(uint8_t code)
{
    switch (code & 0x0F) {
        case 0x0A: return 8 * 1024;
        case 0x0B: return 16 * 1024;
        case 0x0C: return 32 * 1024;
        case 0x0D: return 48 * 1024;
        case 0x0E: return 64 * 1024;
        case 0x0F: return 128 * 1024;
        case 0x00: return 256 * 1024;
        case 0x01: return 512 * 1024;
        case 0x02: return 1024 * 1024;
        default:   return 0;
    }
}

uint8_t sms_code_from_size(size_t size)
{
    if (size <= 8 * 1024) return 0x0A;
    if (size <= 16 * 1024) return 0x0B;
    if (size <= 32 * 1024) return 0x0C;
    if (size <= 48 * 1024) return 0x0D;
    if (size <= 64 * 1024) return 0x0E;
    if (size <= 128 * 1024) return 0x0F;
    if (size <= 256 * 1024) return 0x00;
    if (size <= 512 * 1024) return 0x01;
    return 0x02;  /* 1MB */
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

void sms_print_info(const sms_rom_t *rom, FILE *fp)
{
    if (!rom || !fp) return;
    
    sms_info_t info;
    sms_get_info(rom, &info);
    
    fprintf(fp, "%s ROM:\n", sms_console_name(info.console));
    fprintf(fp, "  File Size: %zu bytes (%.1f KB)\n",
            info.file_size, info.file_size / 1024.0);
    fprintf(fp, "  Has Header: %s\n", info.has_header ? "Yes" : "No");
    fprintf(fp, "  Mapper: %s\n", sms_mapper_name(info.mapper));
    
    if (info.has_header) {
        fprintf(fp, "  Header Offset: 0x%04X\n", info.header_offset);
        fprintf(fp, "  Region: %s\n", sms_region_name(info.region));
        fprintf(fp, "  Product Code: %05u\n", info.product_code);
        fprintf(fp, "  Version: %d\n", info.version);
        fprintf(fp, "  Checksum: %04X (%s)\n", 
                info.checksum, info.checksum_valid ? "VALID" : "INVALID");
    }
}
