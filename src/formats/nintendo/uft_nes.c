/**
 * @file uft_nes.c
 * @brief Nintendo Entertainment System / Famicom ROM Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/nintendo/uft_nes.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Detection
 * ============================================================================ */

nes_format_t nes_detect_format(const uint8_t *data, size_t size)
{
    if (!data || size < NES_HEADER_SIZE) return NES_FORMAT_UNKNOWN;
    
    /* Check iNES magic */
    if (memcmp(data, NES_MAGIC, NES_MAGIC_SIZE) == 0) {
        /* Check for NES 2.0 */
        if ((data[7] & 0x0C) == 0x08) {
            return NES_FORMAT_NES20;
        }
        return NES_FORMAT_INES;
    }
    
    /* Check UNIF magic */
    if (memcmp(data, UNIF_MAGIC, UNIF_MAGIC_SIZE) == 0) {
        return NES_FORMAT_UNIF;
    }
    
    /* Check FDS magic */
    if (memcmp(data, FDS_MAGIC, 4) == 0) {
        return NES_FORMAT_FDS;
    }
    
    /* Check for headerless FDS (starts with 0x01 *NINTENDO-HVC*) */
    if (size >= 56 && data[0] == 0x01 && 
        memcmp(data + 1, "*NINTENDO-HVC*", 14) == 0) {
        return NES_FORMAT_FDS;
    }
    
    return NES_FORMAT_UNKNOWN;
}

bool nes_is_nes20(const nes_header_t *header)
{
    if (!header) return false;
    return (header->flags7 & 0x0C) == 0x08;
}

bool nes_validate(const uint8_t *data, size_t size)
{
    return nes_detect_format(data, size) != NES_FORMAT_UNKNOWN;
}

const char *nes_format_name(nes_format_t format)
{
    switch (format) {
        case NES_FORMAT_INES:   return "iNES";
        case NES_FORMAT_NES20:  return "NES 2.0";
        case NES_FORMAT_UNIF:   return "UNIF";
        case NES_FORMAT_FDS:    return "FDS";
        default:                return "Unknown";
    }
}

const char *nes_mapper_name(uint16_t mapper)
{
    switch (mapper) {
        case 0:   return "NROM";
        case 1:   return "MMC1 (SxROM)";
        case 2:   return "UxROM";
        case 3:   return "CNROM";
        case 4:   return "MMC3 (TxROM)";
        case 5:   return "MMC5 (ExROM)";
        case 7:   return "AxROM";
        case 9:   return "MMC2 (PxROM)";
        case 10:  return "MMC4 (FxROM)";
        case 11:  return "Color Dreams";
        case 16:  return "Bandai FCG";
        case 18:  return "Jaleco SS88006";
        case 19:  return "Namco 163";
        case 21:  return "VRC4a/VRC4c";
        case 22:  return "VRC2a";
        case 23:  return "VRC2b/VRC4e";
        case 24:  return "VRC6a";
        case 25:  return "VRC4b/VRC4d";
        case 26:  return "VRC6b";
        case 34:  return "BNROM/NINA-001";
        case 64:  return "Tengen RAMBO-1";
        case 66:  return "GxROM/MxROM";
        case 69:  return "Sunsoft FME-7";
        case 71:  return "Codemasters";
        case 79:  return "NINA-03/NINA-06";
        case 85:  return "VRC7";
        case 118: return "TxSROM (MMC3)";
        case 119: return "TQROM (MMC3)";
        case 163: return "Nanjing";
        case 206: return "DxROM/Namcot 118";
        default:  return "Unknown";
    }
}

const char *nes_mirror_name(nes_mirror_t mirror)
{
    switch (mirror) {
        case NES_MIRROR_HORIZONTAL:  return "Horizontal";
        case NES_MIRROR_VERTICAL:    return "Vertical";
        case NES_MIRROR_FOUR_SCREEN: return "Four-screen";
        case NES_MIRROR_SINGLE_0:    return "Single-screen (lower)";
        case NES_MIRROR_SINGLE_1:    return "Single-screen (upper)";
        default:                     return "Unknown";
    }
}

const char *nes_tv_name(nes_tv_t tv)
{
    switch (tv) {
        case NES_TV_NTSC:  return "NTSC";
        case NES_TV_PAL:   return "PAL";
        case NES_TV_DUAL:  return "Dual (NTSC/PAL)";
        case NES_TV_DENDY: return "Dendy";
        default:           return "Unknown";
    }
}

/* ============================================================================
 * Header Parsing
 * ============================================================================ */

uint16_t nes_get_mapper(const nes_header_t *header)
{
    if (!header) return 0;
    
    uint16_t mapper = (header->flags6 >> 4) | (header->flags7 & 0xF0);
    
    /* NES 2.0 extended mapper */
    if (nes_is_nes20(header)) {
        mapper |= ((header->flags8 & 0x0F) << 8);
    }
    
    return mapper;
}

uint32_t nes_get_prg_size(const nes_header_t *header)
{
    if (!header) return 0;
    
    uint32_t size = header->prg_rom_size;
    
    /* NES 2.0 extended size */
    if (nes_is_nes20(header)) {
        uint8_t msb = (header->flags9 & 0x0F);
        if (msb == 0x0F) {
            /* Exponent-multiplier notation */
            uint8_t exp = (header->prg_rom_size >> 2);
            uint8_t mul = (header->prg_rom_size & 0x03) * 2 + 1;
            return (1 << exp) * mul;
        }
        size |= (msb << 8);
    }
    
    return size * NES_PRG_BANK_SIZE;
}

uint32_t nes_get_chr_size(const nes_header_t *header)
{
    if (!header) return 0;
    
    uint32_t size = header->chr_rom_size;
    
    /* NES 2.0 extended size */
    if (nes_is_nes20(header)) {
        uint8_t msb = (header->flags9 >> 4);
        if (msb == 0x0F) {
            /* Exponent-multiplier notation */
            uint8_t exp = (header->chr_rom_size >> 2);
            uint8_t mul = (header->chr_rom_size & 0x03) * 2 + 1;
            return (1 << exp) * mul;
        }
        size |= (msb << 8);
    }
    
    return size * NES_CHR_BANK_SIZE;
}

nes_mirror_t nes_get_mirroring(const nes_header_t *header)
{
    if (!header) return NES_MIRROR_HORIZONTAL;
    
    if (header->flags6 & 0x08) {
        return NES_MIRROR_FOUR_SCREEN;
    }
    
    return (header->flags6 & 0x01) ? NES_MIRROR_VERTICAL : NES_MIRROR_HORIZONTAL;
}

/* ============================================================================
 * ROM Operations
 * ============================================================================ */

int nes_open(const uint8_t *data, size_t size, nes_rom_t *rom)
{
    if (!data || !rom || size < NES_HEADER_SIZE) return -1;
    
    memset(rom, 0, sizeof(nes_rom_t));
    
    rom->format = nes_detect_format(data, size);
    if (rom->format == NES_FORMAT_UNKNOWN) return -2;
    
    /* Copy data */
    rom->data = malloc(size);
    if (!rom->data) return -3;
    
    memcpy(rom->data, data, size);
    rom->size = size;
    rom->owns_data = true;
    
    /* Parse header for iNES/NES 2.0 */
    if (rom->format == NES_FORMAT_INES || rom->format == NES_FORMAT_NES20) {
        memcpy(&rom->header, data, sizeof(nes_header_t));
        
        /* Set up pointers */
        size_t offset = NES_HEADER_SIZE;
        
        /* Trainer */
        if (rom->header.flags6 & 0x04) {
            rom->trainer = rom->data + offset;
            offset += NES_TRAINER_SIZE;
        }
        
        /* PRG ROM */
        rom->prg_rom = rom->data + offset;
        offset += nes_get_prg_size(&rom->header);
        
        /* CHR ROM */
        if (rom->header.chr_rom_size > 0) {
            rom->chr_rom = rom->data + offset;
        }
    }
    
    return 0;
}

int nes_load(const char *filename, nes_rom_t *rom)
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
    
    int result = nes_open(data, size, rom);
    free(data);
    
    return result;
}

void nes_close(nes_rom_t *rom)
{
    if (!rom) return;
    
    if (rom->owns_data) {
        free(rom->data);
    }
    
    memset(rom, 0, sizeof(nes_rom_t));
}

int nes_get_info(const nes_rom_t *rom, nes_info_t *info)
{
    if (!rom || !info) return -1;
    
    memset(info, 0, sizeof(nes_info_t));
    
    info->format = rom->format;
    info->format_name = nes_format_name(rom->format);
    info->file_size = rom->size;
    
    if (rom->format == NES_FORMAT_INES || rom->format == NES_FORMAT_NES20) {
        info->mapper = nes_get_mapper(&rom->header);
        info->mapper_name = nes_mapper_name(info->mapper);
        info->prg_rom_size = nes_get_prg_size(&rom->header);
        info->chr_rom_size = nes_get_chr_size(&rom->header);
        info->prg_banks = info->prg_rom_size / NES_PRG_BANK_SIZE;
        info->chr_banks = info->chr_rom_size / NES_CHR_BANK_SIZE;
        info->mirroring = nes_get_mirroring(&rom->header);
        info->has_battery = (rom->header.flags6 & 0x02) != 0;
        info->has_trainer = (rom->header.flags6 & 0x04) != 0;
        info->is_nes20 = nes_is_nes20(&rom->header);
        
        /* TV system */
        if (info->is_nes20) {
            info->tv_system = rom->header.flags10 & 0x03;
            info->submapper = (rom->header.flags8 >> 4);
        } else {
            info->tv_system = (rom->header.flags9 & 0x01) ? NES_TV_PAL : NES_TV_NTSC;
        }
        
        /* Console type */
        info->console = (rom->header.flags7 & 0x03);
        
        /* CHR RAM if no CHR ROM */
        if (info->chr_rom_size == 0) {
            info->chr_ram_size = 8192;  /* Default 8KB CHR RAM */
        }
    }
    else if (rom->format == NES_FORMAT_FDS) {
        info->prg_rom_size = rom->size;
        info->mapper_name = "FDS";
    }
    
    return 0;
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

void nes_print_info(const nes_rom_t *rom, FILE *fp)
{
    if (!rom || !fp) return;
    
    nes_info_t info;
    nes_get_info(rom, &info);
    
    fprintf(fp, "NES ROM:\n");
    fprintf(fp, "  Format: %s\n", info.format_name);
    fprintf(fp, "  File Size: %zu bytes\n", info.file_size);
    fprintf(fp, "  Mapper: %d (%s)\n", info.mapper, info.mapper_name);
    fprintf(fp, "  PRG ROM: %u KB (%d banks)\n", 
            info.prg_rom_size / 1024, info.prg_banks);
    fprintf(fp, "  CHR ROM: %u KB (%d banks)\n", 
            info.chr_rom_size / 1024, info.chr_banks);
    fprintf(fp, "  Mirroring: %s\n", nes_mirror_name(info.mirroring));
    fprintf(fp, "  Battery: %s\n", info.has_battery ? "Yes" : "No");
    fprintf(fp, "  Trainer: %s\n", info.has_trainer ? "Yes" : "No");
    fprintf(fp, "  TV System: %s\n", nes_tv_name(info.tv_system));
}
