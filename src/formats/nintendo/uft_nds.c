/**
 * @file uft_nds.c
 * @brief Nintendo DS / DSi ROM Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/nintendo/uft_nds.h"
#include <stdlib.h>
#include <string.h>

/* CRC16 table for NDS */
static const uint16_t crc16_table[256] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

static uint16_t calc_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc16_table[(crc ^ data[i]) & 0xFF];
    }
    return crc;
}

/* ============================================================================
 * Detection
 * ============================================================================ */

bool nds_validate(const uint8_t *data, size_t size)
{
    if (!data || size < NDS_HEADER_SIZE) return false;
    
    /* Check for valid game code (4 printable chars) */
    for (int i = 12; i < 16; i++) {
        if (data[i] < 0x20 || data[i] > 0x7E) return false;
    }
    
    /* Verify header CRC */
    uint16_t crc = calc_crc16(data, 0x15E);
    uint16_t stored = data[0x15E] | (data[0x15F] << 8);
    
    return (crc == stored);
}

const char *nds_unit_name(nds_unit_t unit)
{
    switch (unit) {
        case NDS_UNIT_NDS:      return "Nintendo DS";
        case NDS_UNIT_NDS_DSI:  return "Nintendo DS (DSi Enhanced)";
        case NDS_UNIT_DSI:      return "Nintendo DSi";
        default:               return "Unknown";
    }
}

/* ============================================================================
 * ROM Operations
 * ============================================================================ */

int nds_open(const uint8_t *data, size_t size, nds_rom_t *rom)
{
    if (!data || !rom || size < NDS_HEADER_SIZE) return -1;
    
    memset(rom, 0, sizeof(nds_rom_t));
    
    /* Copy data */
    rom->data = malloc(size);
    if (!rom->data) return -3;
    
    memcpy(rom->data, data, size);
    rom->size = size;
    rom->owns_data = true;
    
    /* Parse header */
    memcpy(&rom->header, data, sizeof(nds_header_t));
    
    return 0;
}

int nds_load(const char *filename, nds_rom_t *rom)
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
    
    int result = nds_open(data, size, rom);
    free(data);
    
    return result;
}

void nds_close(nds_rom_t *rom)
{
    if (!rom) return;
    
    if (rom->owns_data) {
        free(rom->data);
    }
    
    memset(rom, 0, sizeof(nds_rom_t));
}

int nds_get_info(const nds_rom_t *rom, nds_info_t *info)
{
    if (!rom || !info) return -1;
    
    memset(info, 0, sizeof(nds_info_t));
    
    /* Title */
    memcpy(info->title, rom->header.title, 12);
    info->title[12] = '\0';
    
    /* Game code */
    memcpy(info->game_code, rom->header.game_code, 4);
    info->game_code[4] = '\0';
    
    /* Maker code */
    memcpy(info->maker_code, rom->header.maker_code, 2);
    info->maker_code[2] = '\0';
    
    info->unit = rom->header.unit_code;
    info->unit_name = nds_unit_name(rom->header.unit_code);
    
    info->file_size = rom->size;
    info->total_size = rom->header.total_size;
    info->capacity = 128 * 1024 << rom->header.device_capacity;
    info->version = rom->header.version;
    
    info->is_dsi_enhanced = (rom->header.unit_code == NDS_UNIT_NDS_DSI);
    info->is_dsi_exclusive = (rom->header.unit_code == NDS_UNIT_DSI);
    
    info->arm9_size = rom->header.arm9_size;
    info->arm7_size = rom->header.arm7_size;
    
    info->has_icon = (rom->header.icon_offset != 0);
    
    info->header_crc = rom->header.header_crc;
    info->header_valid = nds_verify_header_crc(rom);
    
    return 0;
}

/* ============================================================================
 * CRC
 * ============================================================================ */

uint16_t nds_calculate_header_crc(const uint8_t *data, size_t size)
{
    if (!data || size < 0x160) return 0;
    return calc_crc16(data, 0x15E);
}

bool nds_verify_header_crc(const nds_rom_t *rom)
{
    if (!rom || !rom->data) return false;
    
    uint16_t calc = nds_calculate_header_crc(rom->data, rom->size);
    return (calc == rom->header.header_crc);
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

void nds_print_info(const nds_rom_t *rom, FILE *fp)
{
    if (!rom || !fp) return;
    
    nds_info_t info;
    nds_get_info(rom, &info);
    
    fprintf(fp, "Nintendo DS ROM:\n");
    fprintf(fp, "  Title: %s\n", info.title);
    fprintf(fp, "  Game Code: %s\n", info.game_code);
    fprintf(fp, "  Maker Code: %s\n", info.maker_code);
    fprintf(fp, "  Unit: %s\n", info.unit_name);
    fprintf(fp, "  File Size: %zu bytes (%.1f MB)\n", 
            info.file_size, info.file_size / 1048576.0);
    fprintf(fp, "  Capacity: %u MB\n", info.capacity / 1048576);
    fprintf(fp, "  Version: %d\n", info.version);
    fprintf(fp, "  ARM9 Size: %u bytes\n", info.arm9_size);
    fprintf(fp, "  ARM7 Size: %u bytes\n", info.arm7_size);
    fprintf(fp, "  Has Icon: %s\n", info.has_icon ? "Yes" : "No");
    fprintf(fp, "  Header CRC: %04X (%s)\n", 
            info.header_crc, info.header_valid ? "VALID" : "INVALID");
}
