/**
 * @file uft_genesis.c
 * @brief Sega Genesis/Mega Drive ROM Image Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/sega/uft_genesis.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

static void trim_string(char *str)
{
    /* Trim trailing spaces */
    int len = strlen(str);
    while (len > 0 && (str[len-1] == ' ' || str[len-1] == '\0')) {
        str[--len] = '\0';
    }
}

/* ============================================================================
 * Detection
 * ============================================================================ */

bool genesis_is_smd(const uint8_t *data, size_t size)
{
    if (!data || size < SMD_HEADER_SIZE + SMD_BLOCK_SIZE) return false;
    
    /* Check SMD markers */
    if (data[8] == 0xAA && data[9] == 0xBB) {
        /* Check block count matches file size */
        int blocks = data[0];
        size_t expected = SMD_HEADER_SIZE + blocks * SMD_BLOCK_SIZE;
        if (size >= expected) {
            return true;
        }
    }
    
    /* Alternative: check if deinterleaving produces valid header */
    /* First block odd bytes should contain "SEGA" */
    if (size >= SMD_HEADER_SIZE + 0x200) {
        /* In SMD, even bytes are at block start, odd at block+8192 */
        const uint8_t *block = data + SMD_HEADER_SIZE;
        if (block[0x80] == 'S' && block[0x81] == 'E' &&
            block[0x82] == 'G' && block[0x83] == 'A') {
            return true;
        }
    }
    
    return false;
}

genesis_format_t genesis_detect_format(const uint8_t *data, size_t size)
{
    if (!data || size < 0x200) return GENESIS_FORMAT_UNKNOWN;
    
    /* Check for SMD format first */
    if (genesis_is_smd(data, size)) {
        return GENESIS_FORMAT_SMD;
    }
    
    /* Check for standard ROM header at $100 */
    if (size >= 0x200) {
        const char *system = (const char *)(data + 0x100);
        
        if (memcmp(system, "SEGA", 4) == 0) {
            /* Check for 32X */
            if (strstr(system, "32X") != NULL) {
                return GENESIS_FORMAT_32X;
            }
            return GENESIS_FORMAT_BIN;
        }
    }
    
    return GENESIS_FORMAT_UNKNOWN;
}

genesis_system_t genesis_detect_system(const uint8_t *data, size_t size)
{
    if (!data || size < 0x200) return GENESIS_TYPE_UNKNOWN;
    
    const char *system = (const char *)(data + 0x100);
    
    if (strncmp(system, "SEGA MEGA DRIVE", 15) == 0) {
        return GENESIS_TYPE_MD;
    }
    if (strncmp(system, "SEGA GENESIS", 12) == 0) {
        return GENESIS_TYPE_GENESIS;
    }
    if (strstr(system, "32X") != NULL) {
        return GENESIS_TYPE_32X;
    }
    if (strstr(system, "MEGA-CD") != NULL || strstr(system, "MEGA CD") != NULL) {
        return GENESIS_TYPE_SCD;
    }
    if (strstr(system, "PICO") != NULL) {
        return GENESIS_TYPE_PICO;
    }
    
    /* Default to Mega Drive if header looks valid */
    if (memcmp(system, "SEGA", 4) == 0) {
        return GENESIS_TYPE_MD;
    }
    
    return GENESIS_TYPE_UNKNOWN;
}

const char *genesis_format_name(genesis_format_t format)
{
    switch (format) {
        case GENESIS_FORMAT_BIN:  return "BIN (Raw Binary)";
        case GENESIS_FORMAT_SMD:  return "SMD (Super Magic Drive)";
        case GENESIS_FORMAT_MD:   return "MD (Mega Drive)";
        case GENESIS_FORMAT_32X:  return "32X ROM";
        default:                  return "Unknown";
    }
}

const char *genesis_system_name(genesis_system_t system)
{
    switch (system) {
        case GENESIS_TYPE_MD:      return "Sega Mega Drive";
        case GENESIS_TYPE_GENESIS: return "Sega Genesis";
        case GENESIS_TYPE_32X:     return "Sega 32X";
        case GENESIS_TYPE_SCD:     return "Sega CD";
        case GENESIS_TYPE_PICO:    return "Sega Pico";
        default:                   return "Unknown";
    }
}

bool genesis_validate(const uint8_t *data, size_t size)
{
    genesis_format_t format = genesis_detect_format(data, size);
    return format != GENESIS_FORMAT_UNKNOWN;
}

/* ============================================================================
 * ROM Operations
 * ============================================================================ */

static void parse_header(const uint8_t *data, genesis_header_t *header)
{
    const uint8_t *hdr = data + GENESIS_HEADER_OFFSET;
    
    memcpy(header->system, hdr, 16);
    header->system[15] = '\0';
    
    memcpy(header->copyright, hdr + 0x10, 16);
    header->copyright[15] = '\0';
    
    memcpy(header->title_domestic, hdr + 0x20, 48);
    header->title_domestic[47] = '\0';
    
    memcpy(header->title_overseas, hdr + 0x50, 48);
    header->title_overseas[47] = '\0';
    
    memcpy(header->serial, hdr + 0x80, 14);
    header->serial[13] = '\0';
    
    header->checksum = read_be16(hdr + 0x8E);
    
    memcpy(header->io_support, hdr + 0x90, 16);
    header->io_support[15] = '\0';
    
    header->rom_start = read_be32(hdr + 0xA0);
    header->rom_end = read_be32(hdr + 0xA4);
    header->ram_start = read_be32(hdr + 0xA8);
    header->ram_end = read_be32(hdr + 0xAC);
    
    memcpy(header->sram_info, hdr + 0xB0, 12);
    header->sram_info[11] = '\0';
    
    memcpy(header->modem_info, hdr + 0xBC, 12);
    header->modem_info[11] = '\0';
    
    memcpy(header->memo, hdr + 0xC8, 40);
    header->memo[39] = '\0';
    
    memcpy(header->region, hdr + 0xF0, 16);
    header->region[15] = '\0';
    
    /* Trim strings */
    trim_string(header->title_domestic);
    trim_string(header->title_overseas);
    trim_string(header->serial);
}

int genesis_open(const uint8_t *data, size_t size, genesis_rom_t *rom)
{
    if (!data || !rom || size < 0x200) return -1;
    
    memset(rom, 0, sizeof(genesis_rom_t));
    
    rom->format = genesis_detect_format(data, size);
    if (rom->format == GENESIS_FORMAT_UNKNOWN) return -2;
    
    /* Convert SMD to BIN if needed */
    if (rom->format == GENESIS_FORMAT_SMD) {
        size_t bin_size = size - SMD_HEADER_SIZE;
        rom->data = malloc(bin_size);
        if (!rom->data) return -3;
        
        if (genesis_smd_to_bin(data, size, rom->data, bin_size, &rom->size) != 0) {
            free(rom->data);
            return -4;
        }
    } else {
        rom->data = malloc(size);
        if (!rom->data) return -3;
        
        memcpy(rom->data, data, size);
        rom->size = size;
    }
    
    rom->owns_data = true;
    rom->system = genesis_detect_system(rom->data, rom->size);
    
    /* Parse header */
    if (rom->size >= 0x200) {
        parse_header(rom->data, &rom->header);
    }
    
    return 0;
}

int genesis_load(const char *filename, genesis_rom_t *rom)
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
    
    int result = genesis_open(data, size, rom);
    free(data);
    
    return result;
}

int genesis_save(const genesis_rom_t *rom, const char *filename,
                 genesis_format_t format)
{
    if (!rom || !filename || !rom->data) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -2;
    
    size_t written;
    
    if (format == GENESIS_FORMAT_SMD) {
        /* Convert to SMD */
        size_t smd_size = SMD_HEADER_SIZE + rom->size;
        uint8_t *smd_data = malloc(smd_size);
        if (!smd_data) {
            fclose(fp);
            return -3;
        }
        
        if (genesis_bin_to_smd(rom->data, rom->size, smd_data, smd_size, &smd_size) != 0) {
            free(smd_data);
            fclose(fp);
            return -4;
        }
        
        written = fwrite(smd_data, 1, smd_size, fp);
        free(smd_data);
    } else {
        written = fwrite(rom->data, 1, rom->size, fp);
    }
    
    fclose(fp);
    return (written > 0) ? 0 : -5;
}

void genesis_close(genesis_rom_t *rom)
{
    if (!rom) return;
    
    if (rom->owns_data) {
        free(rom->data);
    }
    
    memset(rom, 0, sizeof(genesis_rom_t));
}

int genesis_get_info(const genesis_rom_t *rom, genesis_info_t *info)
{
    if (!rom || !info) return -1;
    
    memset(info, 0, sizeof(genesis_info_t));
    
    info->format = rom->format;
    info->system = rom->system;
    info->rom_size = rom->size;
    
    /* Use overseas title if available, else domestic */
    if (rom->header.title_overseas[0]) {
        strncpy(info->title, rom->header.title_overseas, 48);
    } else {
        strncpy(info->title, rom->header.title_domestic, 48);
    }
    
    strncpy(info->serial, rom->header.serial, 14);
    strncpy(info->copyright, rom->header.copyright, 16);
    
    info->checksum = rom->header.checksum;
    info->calculated_checksum = genesis_calculate_checksum(rom->data, rom->size);
    info->checksum_valid = (info->checksum == info->calculated_checksum);
    
    info->regions = genesis_parse_regions(rom->header.region);
    
    /* Check for SRAM */
    if (rom->header.sram_info[0] == 'R' && rom->header.sram_info[1] == 'A') {
        info->has_sram = true;
        /* Parse SRAM addresses from header */
        const uint8_t *hdr = rom->data + GENESIS_HEADER_OFFSET;
        info->sram_start = read_be32(hdr + 0xB4);
        info->sram_end = read_be32(hdr + 0xB8);
    }
    
    return 0;
}

/* ============================================================================
 * Format Conversion
 * ============================================================================ */

int genesis_smd_to_bin(const uint8_t *smd_data, size_t smd_size,
                       uint8_t *bin_data, size_t max_size, size_t *bin_size)
{
    if (!smd_data || !bin_data) return -1;
    if (smd_size < SMD_HEADER_SIZE + SMD_BLOCK_SIZE) return -2;
    
    int blocks = smd_data[0];
    if (blocks == 0) {
        blocks = (smd_size - SMD_HEADER_SIZE) / SMD_BLOCK_SIZE;
    }
    
    size_t output_size = blocks * SMD_BLOCK_SIZE;
    if (max_size < output_size) return -3;
    
    /* Deinterleave blocks */
    for (int b = 0; b < blocks; b++) {
        const uint8_t *block = smd_data + SMD_HEADER_SIZE + b * SMD_BLOCK_SIZE;
        uint8_t *out = bin_data + b * SMD_BLOCK_SIZE;
        
        /* SMD interleaving: even bytes first, then odd bytes */
        for (int i = 0; i < 8192; i++) {
            out[i * 2 + 1] = block[i];          /* Odd bytes */
            out[i * 2] = block[i + 8192];       /* Even bytes */
        }
    }
    
    if (bin_size) *bin_size = output_size;
    return 0;
}

int genesis_bin_to_smd(const uint8_t *bin_data, size_t bin_size,
                       uint8_t *smd_data, size_t max_size, size_t *smd_size)
{
    if (!bin_data || !smd_data) return -1;
    
    /* Round up to block boundary */
    int blocks = (bin_size + SMD_BLOCK_SIZE - 1) / SMD_BLOCK_SIZE;
    size_t output_size = SMD_HEADER_SIZE + blocks * SMD_BLOCK_SIZE;
    
    if (max_size < output_size) return -2;
    
    /* Clear header */
    memset(smd_data, 0, SMD_HEADER_SIZE);
    smd_data[0] = blocks;
    smd_data[1] = 0x03;
    smd_data[8] = 0xAA;
    smd_data[9] = 0xBB;
    smd_data[10] = 0x06;  /* Genesis */
    
    /* Interleave blocks */
    for (int b = 0; b < blocks; b++) {
        const uint8_t *in = bin_data + b * SMD_BLOCK_SIZE;
        uint8_t *block = smd_data + SMD_HEADER_SIZE + b * SMD_BLOCK_SIZE;
        
        size_t block_bytes = SMD_BLOCK_SIZE;
        if (b == blocks - 1) {
            block_bytes = bin_size - b * SMD_BLOCK_SIZE;
        }
        
        memset(block, 0, SMD_BLOCK_SIZE);
        
        for (size_t i = 0; i < block_bytes / 2; i++) {
            block[i] = in[i * 2 + 1];           /* Odd bytes first */
            block[i + 8192] = in[i * 2];        /* Even bytes second */
        }
    }
    
    if (smd_size) *smd_size = output_size;
    return 0;
}

/* ============================================================================
 * Checksum
 * ============================================================================ */

uint16_t genesis_calculate_checksum(const uint8_t *data, size_t size)
{
    if (!data || size < 0x200) return 0;
    
    uint16_t checksum = 0;
    
    /* Checksum covers $200 to end of ROM */
    for (size_t i = 0x200; i < size; i += 2) {
        checksum += read_be16(data + i);
    }
    
    return checksum;
}

bool genesis_verify_checksum(const genesis_rom_t *rom)
{
    if (!rom || !rom->data) return false;
    
    uint16_t calculated = genesis_calculate_checksum(rom->data, rom->size);
    return calculated == rom->header.checksum;
}

int genesis_fix_checksum(genesis_rom_t *rom)
{
    if (!rom || !rom->data || rom->size < 0x200) return -1;
    
    uint16_t checksum = genesis_calculate_checksum(rom->data, rom->size);
    
    /* Write checksum at $18E */
    write_be16(rom->data + 0x18E, checksum);
    
    /* Update cached header */
    rom->header.checksum = checksum;
    
    return 0;
}

/* ============================================================================
 * Region
 * ============================================================================ */

genesis_region_t genesis_parse_regions(const char *region_str)
{
    genesis_region_t regions = 0;
    
    if (!region_str) return regions;
    
    for (int i = 0; region_str[i] && i < 16; i++) {
        char c = toupper(region_str[i]);
        switch (c) {
            case 'J': regions |= GENESIS_REGION_JAPAN; break;
            case 'U': regions |= GENESIS_REGION_USA; break;
            case 'E': regions |= GENESIS_REGION_EUROPE; break;
            case '1': regions |= GENESIS_REGION_JAPAN; break;
            case '4': regions |= GENESIS_REGION_USA; break;
            case '8': regions |= GENESIS_REGION_USA | GENESIS_REGION_EUROPE; break;
            case 'A': case 'F': regions |= GENESIS_REGION_WORLD; break;
        }
    }
    
    return regions;
}

void genesis_region_string(genesis_region_t region, char *buffer, size_t size)
{
    if (!buffer || size == 0) return;
    
    buffer[0] = '\0';
    
    if (region & GENESIS_REGION_JAPAN) {
        strncat(buffer, "Japan ", size - strlen(buffer) - 1);
    }
    if (region & GENESIS_REGION_USA) {
        strncat(buffer, "USA ", size - strlen(buffer) - 1);
    }
    if (region & GENESIS_REGION_EUROPE) {
        strncat(buffer, "Europe ", size - strlen(buffer) - 1);
    }
    
    /* Trim trailing space */
    int len = strlen(buffer);
    if (len > 0 && buffer[len-1] == ' ') {
        buffer[len-1] = '\0';
    }
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

void genesis_print_info(const genesis_rom_t *rom, FILE *fp)
{
    if (!rom || !fp) return;
    
    genesis_info_t info;
    genesis_get_info(rom, &info);
    
    fprintf(fp, "Sega Genesis/Mega Drive ROM:\n");
    fprintf(fp, "  Format: %s\n", genesis_format_name(info.format));
    fprintf(fp, "  System: %s\n", genesis_system_name(info.system));
    fprintf(fp, "  Title: %s\n", info.title);
    fprintf(fp, "  Serial: %s\n", info.serial);
    fprintf(fp, "  Copyright: %s\n", info.copyright);
    fprintf(fp, "  ROM Size: %u bytes (%.1f KB)\n", 
            info.rom_size, info.rom_size / 1024.0);
    fprintf(fp, "  Checksum: $%04X (calculated: $%04X) %s\n",
            info.checksum, info.calculated_checksum,
            info.checksum_valid ? "[OK]" : "[MISMATCH]");
    
    char region_str[64];
    genesis_region_string(info.regions, region_str, sizeof(region_str));
    fprintf(fp, "  Regions: %s\n", region_str[0] ? region_str : "Unknown");
    
    if (info.has_sram) {
        fprintf(fp, "  SRAM: $%06X - $%06X (%u bytes)\n",
                info.sram_start, info.sram_end,
                info.sram_end - info.sram_start + 1);
    }
}

const char *genesis_get_title(const genesis_rom_t *rom, bool overseas)
{
    if (!rom) return NULL;
    
    if (overseas && rom->header.title_overseas[0]) {
        return rom->header.title_overseas;
    }
    return rom->header.title_domestic;
}
