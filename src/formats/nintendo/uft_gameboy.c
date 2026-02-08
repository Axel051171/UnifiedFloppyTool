/**
 * @file uft_gameboy.c
 * @brief Nintendo Game Boy / Game Boy Advance ROM Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/nintendo/uft_gameboy.h"
#include <stdlib.h>
#include <string.h>

/* Nintendo logo for verification */
static const uint8_t NINTENDO_LOGO[48] = {
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
    0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
    0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
    0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
    0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
    0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
};

/* ============================================================================
 * Detection
 * ============================================================================ */

bool gb_validate_logo(const uint8_t *data, size_t size)
{
    if (!data || size < GB_LOGO_OFFSET + GB_LOGO_SIZE) return false;
    return memcmp(data + GB_LOGO_OFFSET, NINTENDO_LOGO, GB_LOGO_SIZE) == 0;
}

bool gb_detect(const uint8_t *data, size_t size)
{
    if (!data || size < 0x150) return false;
    
    /* Check Nintendo logo */
    if (!gb_validate_logo(data, size)) return false;
    
    /* Verify header checksum */
    uint8_t checksum = gb_calculate_header_checksum(data, size);
    return (checksum == data[0x14D]);
}

bool gba_detect(const uint8_t *data, size_t size)
{
    if (!data || size < 0xC0) return false;
    
    /* Check for ARM branch instruction at start */
    uint32_t entry = data[0] | (data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
    if ((entry & 0xFF000000) != 0xEA000000) return false;  /* ARM B instruction */
    
    /* Check fixed value at 0xB2 */
    if (data[0xB2] != 0x96) return false;
    
    return true;
}

const char *gb_mbc_name(gb_mbc_type_t type)
{
    switch (type) {
        case GB_MBC_ROM_ONLY:       return "ROM ONLY";
        case GB_MBC_MBC1:           return "MBC1";
        case GB_MBC_MBC1_RAM:       return "MBC1+RAM";
        case GB_MBC_MBC1_RAM_BATT:  return "MBC1+RAM+BATTERY";
        case GB_MBC_MBC2:           return "MBC2";
        case GB_MBC_MBC2_BATT:      return "MBC2+BATTERY";
        case GB_MBC_ROM_RAM:        return "ROM+RAM";
        case GB_MBC_ROM_RAM_BATT:   return "ROM+RAM+BATTERY";
        case GB_MBC_MMM01:          return "MMM01";
        case GB_MBC_MMM01_RAM:      return "MMM01+RAM";
        case GB_MBC_MMM01_RAM_BATT: return "MMM01+RAM+BATTERY";
        case GB_MBC_MBC3_TIMER_BATT: return "MBC3+TIMER+BATTERY";
        case GB_MBC_MBC3_TIMER_RAM_BATT: return "MBC3+TIMER+RAM+BATTERY";
        case GB_MBC_MBC3:           return "MBC3";
        case GB_MBC_MBC3_RAM:       return "MBC3+RAM";
        case GB_MBC_MBC3_RAM_BATT:  return "MBC3+RAM+BATTERY";
        case GB_MBC_MBC5:           return "MBC5";
        case GB_MBC_MBC5_RAM:       return "MBC5+RAM";
        case GB_MBC_MBC5_RAM_BATT:  return "MBC5+RAM+BATTERY";
        case GB_MBC_MBC5_RUMBLE:    return "MBC5+RUMBLE";
        case GB_MBC_MBC5_RUMBLE_RAM: return "MBC5+RUMBLE+RAM";
        case GB_MBC_MBC5_RUMBLE_RAM_BATT: return "MBC5+RUMBLE+RAM+BATTERY";
        case GB_MBC_MBC6:           return "MBC6";
        case GB_MBC_MBC7_SENSOR_RUMBLE_RAM_BATT: return "MBC7+SENSOR+RUMBLE+RAM+BATTERY";
        case GB_MBC_POCKET_CAMERA:  return "POCKET CAMERA";
        case GB_MBC_BANDAI_TAMA5:   return "BANDAI TAMA5";
        case GB_MBC_HUC3:           return "HuC3";
        case GB_MBC_HUC1_RAM_BATT:  return "HuC1+RAM+BATTERY";
        default:                    return "Unknown";
    }
}

const char *gb_compat_name(gb_compat_t compat)
{
    switch (compat) {
        case GB_COMPAT_DMG:     return "Game Boy";
        case GB_COMPAT_DMG_CGB: return "Game Boy / Color";
        case GB_COMPAT_CGB_ONLY: return "Game Boy Color Only";
        case GB_COMPAT_SGB:     return "Super Game Boy";
        default:                return "Unknown";
    }
}

const char *gba_save_name(gba_save_type_t type)
{
    switch (type) {
        case GBA_SAVE_NONE:         return "None";
        case GBA_SAVE_EEPROM_512:   return "EEPROM 512B";
        case GBA_SAVE_EEPROM_8K:    return "EEPROM 8KB";
        case GBA_SAVE_SRAM_32K:     return "SRAM 32KB";
        case GBA_SAVE_FLASH_64K:    return "Flash 64KB";
        case GBA_SAVE_FLASH_128K:   return "Flash 128KB";
        default:                    return "Unknown";
    }
}

/* ============================================================================
 * ROM Operations
 * ============================================================================ */

int gb_open(const uint8_t *data, size_t size, gb_rom_t *rom)
{
    if (!data || !rom || size < 0x150) return -1;
    
    memset(rom, 0, sizeof(gb_rom_t));
    
    /* Detect type */
    rom->is_gba = gba_detect(data, size);
    
    if (!rom->is_gba && !gb_detect(data, size)) {
        return -2;  /* Neither GB nor GBA */
    }
    
    /* Copy data */
    rom->data = malloc(size);
    if (!rom->data) return -3;
    
    memcpy(rom->data, data, size);
    rom->size = size;
    rom->owns_data = true;
    
    /* Parse header */
    if (rom->is_gba) {
        memcpy(&rom->gba_header, data, sizeof(gba_header_t));
    } else {
        memcpy(&rom->gb_header, data + GB_HEADER_OFFSET, sizeof(gb_header_t));
    }
    
    rom->header_valid = true;
    return 0;
}

int gb_load(const char *filename, gb_rom_t *rom)
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
    
    int result = gb_open(data, size, rom);
    free(data);
    
    return result;
}

int gb_save(const gb_rom_t *rom, const char *filename)
{
    if (!rom || !filename || !rom->data) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -2;
    
    size_t written = fwrite(rom->data, 1, rom->size, fp);
    fclose(fp);
    
    return (written == rom->size) ? 0 : -3;
}

void gb_close(gb_rom_t *rom)
{
    if (!rom) return;
    
    if (rom->owns_data) {
        free(rom->data);
    }
    
    memset(rom, 0, sizeof(gb_rom_t));
}

int gb_get_info(const gb_rom_t *rom, gb_info_t *info)
{
    if (!rom || !info || rom->is_gba) return -1;
    
    memset(info, 0, sizeof(gb_info_t));
    
    /* Title */
    int title_len = (rom->gb_header.cgb_flag & 0x80) ? 11 : 16;
    memcpy(info->title, rom->gb_header.title, title_len);
    info->title[title_len] = '\0';
    
    /* Compatibility */
    if (rom->gb_header.cgb_flag == 0xC0) {
        info->compatibility = GB_COMPAT_CGB_ONLY;
    } else if (rom->gb_header.cgb_flag & 0x80) {
        info->compatibility = GB_COMPAT_DMG_CGB;
    } else if (rom->gb_header.sgb_flag == 0x03) {
        info->compatibility = GB_COMPAT_SGB;
    } else {
        info->compatibility = GB_COMPAT_DMG;
    }
    
    /* MBC */
    info->mbc_type = rom->gb_header.cartridge_type;
    info->mbc_name = gb_mbc_name(info->mbc_type);
    
    /* Sizes */
    info->rom_size = gb_rom_size_bytes(rom->gb_header.rom_size);
    info->ram_size = gb_ram_size_bytes(rom->gb_header.ram_size);
    info->rom_banks = gb_rom_banks(rom->gb_header.rom_size);
    info->ram_banks = (info->ram_size > 8192) ? info->ram_size / 8192 : (info->ram_size > 0 ? 1 : 0);
    
    /* Features */
    info->has_battery = gb_has_battery(info->mbc_type);
    info->has_timer = gb_has_timer(info->mbc_type);
    info->has_rumble = (info->mbc_type >= GB_MBC_MBC5_RUMBLE && 
                        info->mbc_type <= GB_MBC_MBC5_RUMBLE_RAM_BATT);
    
    /* Checksums */
    info->header_checksum = rom->gb_header.header_checksum;
    info->calculated_checksum = gb_calculate_header_checksum(rom->data, rom->size);
    info->header_valid = (info->header_checksum == info->calculated_checksum);
    info->global_checksum = (rom->gb_header.global_checksum >> 8) | 
                            (rom->gb_header.global_checksum << 8);  /* Big-endian */
    
    /* Region */
    info->is_japanese = (rom->gb_header.destination == 0);
    
    /* Licensee */
    if (rom->gb_header.old_licensee == 0x33) {
        info->licensee[0] = rom->gb_header.new_licensee[0];
        info->licensee[1] = rom->gb_header.new_licensee[1];
    } else {
        sprintf(info->licensee, "%02X", rom->gb_header.old_licensee);
    }
    
    return 0;
}

int gba_get_info(const gb_rom_t *rom, gba_info_t *info)
{
    if (!rom || !info || !rom->is_gba) return -1;
    
    memset(info, 0, sizeof(gba_info_t));
    
    memcpy(info->title, rom->gba_header.title, 12);
    info->title[12] = '\0';
    
    memcpy(info->game_code, rom->gba_header.game_code, 4);
    info->game_code[4] = '\0';
    
    memcpy(info->maker_code, rom->gba_header.maker_code, 2);
    info->maker_code[2] = '\0';
    
    info->rom_size = rom->size;
    info->version = rom->gba_header.version;
    
    info->save_type = gba_detect_save_type(rom->data, rom->size);
    info->save_name = gba_save_name(info->save_type);
    
    info->complement = rom->gba_header.complement;
    info->calculated = gba_calculate_complement(rom->data, rom->size);
    info->header_valid = (info->complement == info->calculated);
    
    return 0;
}

/* ============================================================================
 * Checksum
 * ============================================================================ */

uint8_t gb_calculate_header_checksum(const uint8_t *data, size_t size)
{
    if (!data || size < 0x14E) return 0;
    
    uint8_t checksum = 0;
    for (int i = 0x134; i <= 0x14C; i++) {
        checksum = checksum - data[i] - 1;
    }
    return checksum;
}

uint16_t gb_calculate_global_checksum(const uint8_t *data, size_t size)
{
    if (!data) return 0;
    
    uint16_t checksum = 0;
    for (size_t i = 0; i < size; i++) {
        if (i != 0x14E && i != 0x14F) {  /* Skip checksum bytes */
            checksum += data[i];
        }
    }
    return checksum;
}

bool gb_verify_header_checksum(const gb_rom_t *rom)
{
    if (!rom || rom->is_gba) return false;
    return gb_calculate_header_checksum(rom->data, rom->size) == rom->gb_header.header_checksum;
}

uint8_t gba_calculate_complement(const uint8_t *data, size_t size)
{
    if (!data || size < 0xBD) return 0;
    
    uint8_t checksum = 0;
    for (int i = 0xA0; i <= 0xBC; i++) {
        checksum += data[i];
    }
    return -(0x19 + checksum);
}

int gb_fix_header_checksum(gb_rom_t *rom)
{
    if (!rom || !rom->data || rom->is_gba) return -1;
    
    uint8_t checksum = gb_calculate_header_checksum(rom->data, rom->size);
    rom->data[0x14D] = checksum;
    rom->gb_header.header_checksum = checksum;
    
    return 0;
}

/* ============================================================================
 * Save Detection
 * ============================================================================ */

gba_save_type_t gba_detect_save_type(const uint8_t *data, size_t size)
{
    if (!data) return GBA_SAVE_NONE;
    
    /* Search for save type strings in ROM */
    for (size_t i = 0; i + 16 < size; i++) {
        if (memcmp(data + i, "EEPROM_V", 8) == 0) {
            return GBA_SAVE_EEPROM_8K;  /* Default to 8K */
        }
        if (memcmp(data + i, "SRAM_V", 6) == 0 ||
            memcmp(data + i, "SRAM_F_V", 8) == 0) {
            return GBA_SAVE_SRAM_32K;
        }
        if (memcmp(data + i, "FLASH_V", 7) == 0 ||
            memcmp(data + i, "FLASH512_V", 10) == 0) {
            return GBA_SAVE_FLASH_64K;
        }
        if (memcmp(data + i, "FLASH1M_V", 9) == 0) {
            return GBA_SAVE_FLASH_128K;
        }
    }
    
    return GBA_SAVE_NONE;
}

bool gb_has_battery(gb_mbc_type_t type)
{
    switch (type) {
        case GB_MBC_MBC1_RAM_BATT:
        case GB_MBC_MBC2_BATT:
        case GB_MBC_ROM_RAM_BATT:
        case GB_MBC_MMM01_RAM_BATT:
        case GB_MBC_MBC3_TIMER_BATT:
        case GB_MBC_MBC3_TIMER_RAM_BATT:
        case GB_MBC_MBC3_RAM_BATT:
        case GB_MBC_MBC5_RAM_BATT:
        case GB_MBC_MBC5_RUMBLE_RAM_BATT:
        case GB_MBC_MBC7_SENSOR_RUMBLE_RAM_BATT:
        case GB_MBC_HUC1_RAM_BATT:
            return true;
        default:
            return false;
    }
}

bool gb_has_timer(gb_mbc_type_t type)
{
    return (type == GB_MBC_MBC3_TIMER_BATT || 
            type == GB_MBC_MBC3_TIMER_RAM_BATT);
}

/* ============================================================================
 * Size Conversion
 * ============================================================================ */

size_t gb_rom_size_bytes(uint8_t code)
{
    if (code <= 0x08) {
        return 32768 << code;  /* 32KB * 2^code */
    }
    return 0;
}

size_t gb_ram_size_bytes(uint8_t code)
{
    switch (code) {
        case 0x00: return 0;
        case 0x01: return 2048;
        case 0x02: return 8192;
        case 0x03: return 32768;
        case 0x04: return 131072;
        case 0x05: return 65536;
        default: return 0;
    }
}

int gb_rom_banks(uint8_t code)
{
    if (code <= 0x08) {
        return 2 << code;
    }
    return 0;
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

void gb_print_info(const gb_rom_t *rom, FILE *fp)
{
    if (!rom || !fp || rom->is_gba) return;
    
    gb_info_t info;
    gb_get_info(rom, &info);
    
    fprintf(fp, "Game Boy ROM:\n");
    fprintf(fp, "  Title: %s\n", info.title);
    fprintf(fp, "  Compatibility: %s\n", gb_compat_name(info.compatibility));
    fprintf(fp, "  Cartridge: %s\n", info.mbc_name);
    fprintf(fp, "  ROM Size: %zu KB (%d banks)\n", info.rom_size / 1024, info.rom_banks);
    fprintf(fp, "  RAM Size: %zu KB\n", info.ram_size / 1024);
    fprintf(fp, "  Features:");
    if (info.has_battery) fprintf(fp, " Battery");
    if (info.has_timer) fprintf(fp, " Timer");
    if (info.has_rumble) fprintf(fp, " Rumble");
    if (!info.has_battery && !info.has_timer && !info.has_rumble) fprintf(fp, " None");
    fprintf(fp, "\n");
    fprintf(fp, "  Header Checksum: %02X (%s)\n", 
            info.header_checksum, info.header_valid ? "VALID" : "INVALID");
    fprintf(fp, "  Region: %s\n", info.is_japanese ? "Japan" : "World");
    fprintf(fp, "  Licensee: %s\n", info.licensee);
}

void gba_print_info(const gb_rom_t *rom, FILE *fp)
{
    if (!rom || !fp || !rom->is_gba) return;
    
    gba_info_t info;
    gba_get_info(rom, &info);
    
    fprintf(fp, "Game Boy Advance ROM:\n");
    fprintf(fp, "  Title: %s\n", info.title);
    fprintf(fp, "  Game Code: AGB-%s\n", info.game_code);
    fprintf(fp, "  Maker Code: %s\n", info.maker_code);
    fprintf(fp, "  ROM Size: %zu KB (%.1f MB)\n", 
            info.rom_size / 1024, info.rom_size / 1048576.0);
    fprintf(fp, "  Version: %d\n", info.version);
    fprintf(fp, "  Save Type: %s\n", info.save_name);
    fprintf(fp, "  Header: %s\n", info.header_valid ? "VALID" : "INVALID");
}
