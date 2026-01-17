/**
 * @file uft_atari.c
 * @brief Atari 2600/7800/5200/Lynx ROM Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/atari/uft_atari.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint32_t read_be32(const uint8_t *data)
{
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

static uint16_t read_le16(const uint8_t *data)
{
    return data[0] | (data[1] << 8);
}

/* ============================================================================
 * Detection
 * ============================================================================ */

bool atari_is_a78(const uint8_t *data, size_t size)
{
    if (!data || size < A78_HEADER_SIZE) return false;
    return (memcmp(data + A78_MAGIC_OFFSET, A78_MAGIC, A78_MAGIC_SIZE) == 0);
}

bool atari_is_lynx(const uint8_t *data, size_t size)
{
    if (!data || size < LYNX_HEADER_SIZE) return false;
    return (memcmp(data, LYNX_MAGIC, LYNX_MAGIC_SIZE) == 0);
}

atari_console_t atari_detect_console(const uint8_t *data, size_t size)
{
    if (!data || size < 1024) return ATARI_CONSOLE_UNKNOWN;
    
    /* Check for headers first */
    if (atari_is_a78(data, size)) {
        return ATARI_CONSOLE_7800;
    }
    
    if (atari_is_lynx(data, size)) {
        return ATARI_CONSOLE_LYNX;
    }
    
    /* 5200 cartridges are typically 16KB-32KB */
    /* And have specific reset vectors */
    if (size >= 16384 && size <= 32768) {
        /* Check for 5200 signature at end */
        /* Last 6 bytes often contain reset vector to $BFF0-$BFFF range */
        /* This is heuristic - 5200 detection is not 100% reliable */
    }
    
    /* Default to 2600 for common sizes */
    if (size == A26_SIZE_2K || size == A26_SIZE_4K ||
        size == A26_SIZE_8K || size == A26_SIZE_16K ||
        size == A26_SIZE_32K) {
        return ATARI_CONSOLE_2600;
    }
    
    /* Larger ROMs might be 7800 without header */
    if (size > 32768) {
        return ATARI_CONSOLE_7800;
    }
    
    return ATARI_CONSOLE_2600;  /* Default */
}

a26_bankswitch_t a26_detect_bankswitch(const uint8_t *data, size_t size)
{
    if (!data) return A26_BANK_UNKNOWN;
    
    /* Simple detection based on size */
    switch (size) {
        case A26_SIZE_2K:
        case A26_SIZE_4K:
            return A26_BANK_NONE;
        case A26_SIZE_8K:
            /* Could be F8, FE, E0 - check hotspots */
            /* F8: $1FF8/$1FF9 */
            /* Check for F8 pattern */
            for (size_t i = 0; i < size - 2; i++) {
                if ((data[i] == 0x8D || data[i] == 0xAD) &&
                    (data[i+1] == 0xF8 || data[i+1] == 0xF9) &&
                    data[i+2] == 0x1F) {
                    return A26_BANK_F8;
                }
            }
            return A26_BANK_F8;  /* Default for 8K */
        case A26_SIZE_16K:
            return A26_BANK_F6;
        case A26_SIZE_32K:
            return A26_BANK_F4;
        default:
            if (size == 12288) return A26_BANK_FA;  /* 12K CBS RAM Plus */
            return A26_BANK_UNKNOWN;
    }
}

const char *atari_console_name(atari_console_t console)
{
    switch (console) {
        case ATARI_CONSOLE_2600: return "Atari 2600 (VCS)";
        case ATARI_CONSOLE_5200: return "Atari 5200 (SuperSystem)";
        case ATARI_CONSOLE_7800: return "Atari 7800 (ProSystem)";
        case ATARI_CONSOLE_LYNX: return "Atari Lynx";
        default:                 return "Unknown";
    }
}

const char *a26_bankswitch_name(a26_bankswitch_t type)
{
    switch (type) {
        case A26_BANK_NONE:      return "None (2K/4K)";
        case A26_BANK_F8:        return "F8 (8K Atari)";
        case A26_BANK_F6:        return "F6 (16K Atari)";
        case A26_BANK_F4:        return "F4 (32K Atari)";
        case A26_BANK_FE:        return "FE (8K Activision)";
        case A26_BANK_E0:        return "E0 (8K Parker Bros)";
        case A26_BANK_E7:        return "E7 (16K M-Network)";
        case A26_BANK_3F:        return "3F (Tigervision)";
        case A26_BANK_FA:        return "FA (12K CBS RAM Plus)";
        case A26_BANK_CV:        return "CV (Commavid)";
        case A26_BANK_UA:        return "UA (UA Ltd)";
        case A26_BANK_SUPERCHIP: return "SuperChip (+128B RAM)";
        case A26_BANK_AR:        return "AR (Supercharger)";
        case A26_BANK_DPC:       return "DPC (Pitfall II)";
        default:                 return "Unknown";
    }
}

const char *a78_controller_name(a78_controller_t type)
{
    switch (type) {
        case A78_CTRL_NONE:          return "None";
        case A78_CTRL_JOYSTICK:      return "7800 Joystick";
        case A78_CTRL_LIGHTGUN:      return "Light Gun";
        case A78_CTRL_PADDLE:        return "Paddle";
        case A78_CTRL_TRAKBALL:      return "Trak-Ball";
        case A78_CTRL_2600_JOYSTICK: return "2600 Joystick";
        case A78_CTRL_2600_DRIVING:  return "2600 Driving";
        case A78_CTRL_2600_KEYBOARD: return "2600 Keyboard";
        case A78_CTRL_ST_MOUSE:      return "ST Mouse";
        case A78_CTRL_AMIGA_MOUSE:   return "Amiga Mouse";
        default:                     return "Unknown";
    }
}

/* ============================================================================
 * ROM Operations
 * ============================================================================ */

int atari_open(const uint8_t *data, size_t size, atari_rom_t *rom)
{
    if (!data || !rom || size < 1024) return -1;
    
    memset(rom, 0, sizeof(atari_rom_t));
    
    /* Detect console */
    rom->console = atari_detect_console(data, size);
    if (rom->console == ATARI_CONSOLE_UNKNOWN) return -2;
    
    /* Check for headers */
    if (rom->console == ATARI_CONSOLE_7800 && atari_is_a78(data, size)) {
        rom->has_header = true;
        rom->header_size = A78_HEADER_SIZE;
        
        /* Parse A78 header */
        rom->a78_header.version = data[0];
        memcpy(rom->a78_header.magic, data + 1, 9);
        memcpy(rom->a78_header.title, data + 17, 32);
        rom->a78_header.rom_size = read_be32(data + 49);
        rom->a78_header.cart_type = (data[53] << 8) | data[54];
        rom->a78_header.controller1 = data[55];
        rom->a78_header.controller2 = data[56];
        rom->a78_header.tv_type = data[57];
        rom->a78_header.save_type = data[58];
    }
    else if (rom->console == ATARI_CONSOLE_LYNX && atari_is_lynx(data, size)) {
        rom->has_header = true;
        rom->header_size = LYNX_HEADER_SIZE;
        
        /* Parse Lynx header */
        memcpy(rom->lynx_header.magic, data, 4);
        rom->lynx_header.page_size_bank0 = read_le16(data + 4);
        rom->lynx_header.page_size_bank1 = read_le16(data + 6);
        rom->lynx_header.version = read_le16(data + 8);
        memcpy(rom->lynx_header.title, data + 10, 32);
        memcpy(rom->lynx_header.manufacturer, data + 42, 16);
        rom->lynx_header.rotation = data[58];
    }
    
    /* Copy data */
    rom->data = malloc(size);
    if (!rom->data) return -3;
    
    memcpy(rom->data, data, size);
    rom->size = size;
    rom->owns_data = true;
    
    return 0;
}

int atari_load(const char *filename, atari_rom_t *rom)
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
    
    int result = atari_open(data, size, rom);
    free(data);
    
    return result;
}

void atari_close(atari_rom_t *rom)
{
    if (!rom) return;
    
    if (rom->owns_data) {
        free(rom->data);
    }
    
    memset(rom, 0, sizeof(atari_rom_t));
}

int atari_get_info(const atari_rom_t *rom, atari_info_t *info)
{
    if (!rom || !info) return -1;
    
    memset(info, 0, sizeof(atari_info_t));
    
    info->console = rom->console;
    info->console_name = atari_console_name(rom->console);
    info->file_size = rom->size;
    info->has_header = rom->has_header;
    info->rom_size = rom->size - rom->header_size;
    
    switch (rom->console) {
        case ATARI_CONSOLE_2600:
            info->bankswitch = a26_detect_bankswitch(
                rom->data + rom->header_size, info->rom_size);
            info->bankswitch_name = a26_bankswitch_name(info->bankswitch);
            break;
            
        case ATARI_CONSOLE_7800:
            if (rom->has_header) {
                memcpy(info->title, rom->a78_header.title, 32);
                info->title[32] = '\0';
                /* Trim */
                for (int i = 31; i >= 0 && info->title[i] == ' '; i--) {
                    info->title[i] = '\0';
                }
                
                info->controller1 = rom->a78_header.controller1;
                info->controller2 = rom->a78_header.controller2;
                info->is_pal = (rom->a78_header.tv_type == 1);
                info->has_pokey = (rom->a78_header.cart_type & 0x01) != 0;
                
                /* Cart type */
                if (rom->a78_header.cart_type & 0x02) {
                    info->cart_type = A78_TYPE_SUPERGAME_RAM;
                } else if (rom->a78_header.cart_type & 0x04) {
                    info->cart_type = A78_TYPE_SUPERGAME_ROM;
                } else if (rom->a78_header.cart_type & 0x08) {
                    info->cart_type = A78_TYPE_ABSOLUTE;
                } else if (rom->a78_header.cart_type & 0x10) {
                    info->cart_type = A78_TYPE_ACTIVISION;
                }
            }
            break;
            
        case ATARI_CONSOLE_LYNX:
            if (rom->has_header) {
                memcpy(info->title, rom->lynx_header.title, 32);
                info->title[32] = '\0';
                info->rotation = rom->lynx_header.rotation;
            }
            break;
            
        default:
            break;
    }
    
    return 0;
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

const uint8_t *atari_get_rom_data(const atari_rom_t *rom)
{
    if (!rom || !rom->data) return NULL;
    return rom->data + rom->header_size;
}

size_t atari_get_rom_size(const atari_rom_t *rom)
{
    if (!rom) return 0;
    return rom->size - rom->header_size;
}

void atari_print_info(const atari_rom_t *rom, FILE *fp)
{
    if (!rom || !fp) return;
    
    atari_info_t info;
    atari_get_info(rom, &info);
    
    fprintf(fp, "%s ROM:\n", info.console_name);
    fprintf(fp, "  File Size: %zu bytes (%.1f KB)\n",
            info.file_size, info.file_size / 1024.0);
    fprintf(fp, "  ROM Size: %zu bytes (%.1f KB)\n",
            info.rom_size, info.rom_size / 1024.0);
    fprintf(fp, "  Has Header: %s\n", info.has_header ? "Yes" : "No");
    
    if (info.title[0]) {
        fprintf(fp, "  Title: %s\n", info.title);
    }
    
    switch (info.console) {
        case ATARI_CONSOLE_2600:
            fprintf(fp, "  Bankswitching: %s\n", info.bankswitch_name);
            break;
            
        case ATARI_CONSOLE_7800:
            fprintf(fp, "  TV Type: %s\n", info.is_pal ? "PAL" : "NTSC");
            fprintf(fp, "  POKEY: %s\n", info.has_pokey ? "Yes" : "No");
            fprintf(fp, "  Controller 1: %s\n", a78_controller_name(info.controller1));
            fprintf(fp, "  Controller 2: %s\n", a78_controller_name(info.controller2));
            break;
            
        case ATARI_CONSOLE_LYNX:
            fprintf(fp, "  Rotation: %d\n", info.rotation);
            break;
            
        default:
            break;
    }
}
