/**
 * @file uft_n64.c
 * @brief Nintendo 64 ROM Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/nintendo/uft_n64.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint32_t read_be32(const uint8_t *data)
{
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

static void write_be32(uint8_t *data, uint32_t value)
{
    data[0] = (value >> 24) & 0xFF;
    data[1] = (value >> 16) & 0xFF;
    data[2] = (value >> 8) & 0xFF;
    data[3] = value & 0xFF;
}

/* ============================================================================
 * Detection
 * ============================================================================ */

n64_format_t n64_detect_format(const uint8_t *data, size_t size)
{
    if (!data || size < 4) return N64_FORMAT_UNKNOWN;
    
    uint32_t magic = read_be32(data);
    
    switch (magic) {
        case N64_MAGIC_Z64:
            return N64_FORMAT_Z64;
        case N64_MAGIC_V64:
            return N64_FORMAT_V64;
        case N64_MAGIC_N64:
            return N64_FORMAT_N64;
        default:
            return N64_FORMAT_UNKNOWN;
    }
}

const char *n64_format_name(n64_format_t format)
{
    switch (format) {
        case N64_FORMAT_Z64: return "z64 (Big-endian)";
        case N64_FORMAT_V64: return "v64 (Byte-swapped)";
        case N64_FORMAT_N64: return "n64 (Little-endian)";
        default:             return "Unknown";
    }
}

const char *n64_region_name(n64_region_t region)
{
    switch (region) {
        case N64_REGION_NTSC:   return "USA (NTSC)";
        case N64_REGION_PAL:    return "Europe (PAL)";
        case N64_REGION_JAPAN:  return "Japan";
        case N64_REGION_GATEWAY: return "Gateway (NTSC)";
        case N64_REGION_PAL_X:
        case N64_REGION_PAL_Y:  return "Europe (PAL)";
        case N64_REGION_PAL_D:  return "Germany";
        case N64_REGION_PAL_F:  return "France";
        case N64_REGION_PAL_I:  return "Italy";
        case N64_REGION_PAL_S:  return "Spain";
        default:                return "Unknown";
    }
}

const char *n64_cic_name(n64_cic_t cic)
{
    switch (cic) {
        case N64_CIC_6101: return "CIC-6101";
        case N64_CIC_6102: return "CIC-6102";
        case N64_CIC_6103: return "CIC-6103";
        case N64_CIC_6105: return "CIC-6105";
        case N64_CIC_6106: return "CIC-6106";
        case N64_CIC_7101: return "CIC-7101";
        case N64_CIC_7102: return "CIC-7102";
        case N64_CIC_7103: return "CIC-7103";
        case N64_CIC_7105: return "CIC-7105";
        case N64_CIC_7106: return "CIC-7106";
        default:           return "Unknown";
    }
}

const char *n64_save_name(n64_save_type_t type)
{
    switch (type) {
        case N64_SAVE_NONE:         return "None";
        case N64_SAVE_EEPROM_4K:    return "EEPROM 4Kbit (512B)";
        case N64_SAVE_EEPROM_16K:   return "EEPROM 16Kbit (2KB)";
        case N64_SAVE_SRAM_256K:    return "SRAM 256Kbit (32KB)";
        case N64_SAVE_FLASH_1M:     return "Flash 1Mbit (128KB)";
        case N64_SAVE_CONTROLLER:   return "Controller Pak";
        default:                    return "Unknown";
    }
}

bool n64_validate(const uint8_t *data, size_t size)
{
    if (!data || size < N64_HEADER_SIZE) return false;
    return n64_detect_format(data, size) != N64_FORMAT_UNKNOWN;
}

/* ============================================================================
 * Format Conversion
 * ============================================================================ */

void n64_to_z64(uint8_t *data, size_t size, n64_format_t format)
{
    if (!data || format == N64_FORMAT_Z64) return;
    
    if (format == N64_FORMAT_V64) {
        /* Byte-swap: swap every pair of bytes */
        for (size_t i = 0; i < size; i += 2) {
            uint8_t tmp = data[i];
            data[i] = data[i + 1];
            data[i + 1] = tmp;
        }
    } else if (format == N64_FORMAT_N64) {
        /* Word-swap: swap byte order within 32-bit words */
        for (size_t i = 0; i < size; i += 4) {
            uint8_t tmp0 = data[i];
            uint8_t tmp1 = data[i + 1];
            data[i] = data[i + 3];
            data[i + 1] = data[i + 2];
            data[i + 2] = tmp1;
            data[i + 3] = tmp0;
        }
    }
}

void n64_z64_to_v64(const uint8_t *data, size_t size, uint8_t *output)
{
    if (!data || !output) return;
    
    for (size_t i = 0; i < size; i += 2) {
        output[i] = data[i + 1];
        output[i + 1] = data[i];
    }
}

void n64_z64_to_n64(const uint8_t *data, size_t size, uint8_t *output)
{
    if (!data || !output) return;
    
    for (size_t i = 0; i < size; i += 4) {
        output[i] = data[i + 3];
        output[i + 1] = data[i + 2];
        output[i + 2] = data[i + 1];
        output[i + 3] = data[i];
    }
}

/* ============================================================================
 * ROM Operations
 * ============================================================================ */

int n64_open(const uint8_t *data, size_t size, n64_rom_t *rom)
{
    if (!data || !rom || size < N64_HEADER_SIZE) return -1;
    
    memset(rom, 0, sizeof(n64_rom_t));
    
    rom->original_format = n64_detect_format(data, size);
    if (rom->original_format == N64_FORMAT_UNKNOWN) return -2;
    
    /* Allocate and copy data */
    rom->data = malloc(size);
    if (!rom->data) return -3;
    
    memcpy(rom->data, data, size);
    rom->size = size;
    rom->owns_data = true;
    
    /* Convert to z64 format */
    n64_to_z64(rom->data, rom->size, rom->original_format);
    
    /* Parse header (now in big-endian z64 format) */
    const uint8_t *hdr = rom->data;
    
    rom->header.pi_bsd_dom1 = read_be32(hdr + 0x00);
    rom->header.clock_rate = read_be32(hdr + 0x04);
    rom->header.boot_address = read_be32(hdr + 0x08);
    rom->header.release = read_be32(hdr + 0x0C);
    rom->header.crc1 = read_be32(hdr + 0x10);
    rom->header.crc2 = read_be32(hdr + 0x14);
    memcpy(&rom->header.reserved1, hdr + 0x18, 8);
    memcpy(rom->header.title, hdr + 0x20, 20);
    rom->header.reserved2 = read_be32(hdr + 0x34);
    rom->header.media_format = read_be32(hdr + 0x38);
    rom->header.game_id[0] = hdr[0x3C];
    rom->header.game_id[1] = hdr[0x3D];
    rom->header.region = hdr[0x3E];
    rom->header.version = hdr[0x3F];
    
    rom->header_valid = true;
    return 0;
}

int n64_load(const char *filename, n64_rom_t *rom)
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
    
    int result = n64_open(data, size, rom);
    free(data);
    
    return result;
}

int n64_save(const n64_rom_t *rom, const char *filename)
{
    return n64_save_as(rom, filename, N64_FORMAT_Z64);
}

int n64_save_as(const n64_rom_t *rom, const char *filename, n64_format_t format)
{
    if (!rom || !filename || !rom->data) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -2;
    
    uint8_t *output = rom->data;
    uint8_t *converted = NULL;
    
    if (format != N64_FORMAT_Z64) {
        converted = malloc(rom->size);
        if (!converted) {
            fclose(fp);
            return -3;
        }
        
        if (format == N64_FORMAT_V64) {
            n64_z64_to_v64(rom->data, rom->size, converted);
        } else {
            n64_z64_to_n64(rom->data, rom->size, converted);
        }
        output = converted;
    }
    
    size_t written = fwrite(output, 1, rom->size, fp);
    fclose(fp);
    
    free(converted);
    
    return (written == rom->size) ? 0 : -4;
}

void n64_close(n64_rom_t *rom)
{
    if (!rom) return;
    
    if (rom->owns_data) {
        free(rom->data);
    }
    
    memset(rom, 0, sizeof(n64_rom_t));
}

int n64_get_info(const n64_rom_t *rom, n64_info_t *info)
{
    if (!rom || !info) return -1;
    
    memset(info, 0, sizeof(n64_info_t));
    
    info->format = rom->original_format;
    info->format_name = n64_format_name(rom->original_format);
    info->rom_size = rom->size;
    
    /* Title (trim trailing spaces) */
    memcpy(info->title, rom->header.title, 20);
    info->title[20] = '\0';
    for (int i = 19; i >= 0 && info->title[i] == ' '; i--) {
        info->title[i] = '\0';
    }
    
    /* Game ID and code */
    info->game_id[0] = rom->header.game_id[0];
    info->game_id[1] = rom->header.game_id[1];
    info->game_id[2] = '\0';
    
    /* Full code: N + game_id + region (e.g., NSME) */
    info->full_code[0] = 'N';
    info->full_code[1] = rom->header.game_id[0];
    info->full_code[2] = rom->header.game_id[1];
    info->full_code[3] = rom->header.region;
    info->full_code[4] = '\0';
    
    info->region = rom->header.region;
    info->region_name = n64_region_name(info->region);
    info->version = rom->header.version;
    
    /* CRCs */
    info->crc1 = rom->header.crc1;
    info->crc2 = rom->header.crc2;
    n64_calculate_crc(rom, &info->calc_crc1, &info->calc_crc2);
    info->crc_valid = (info->crc1 == info->calc_crc1 && info->crc2 == info->calc_crc2);
    
    /* CIC */
    info->cic = n64_detect_cic(rom);
    info->cic_name = n64_cic_name(info->cic);
    
    /* Save type (would need database lookup for accuracy) */
    info->save_type = N64_SAVE_NONE;
    info->save_name = n64_save_name(info->save_type);
    
    return 0;
}

/* ============================================================================
 * CRC Calculation (N64 CRC algorithm)
 * ============================================================================ */

#define ROL(i, b) (((i) << (b)) | ((i) >> (32 - (b))))

static uint32_t crc_table[256];
static bool crc_init = false;

static void init_crc_table(void)
{
    if (crc_init) return;
    
    for (int i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            if (c & 1) {
                c = 0xEDB88320 ^ (c >> 1);
            } else {
                c >>= 1;
            }
        }
        crc_table[i] = c;
    }
    crc_init = true;
}

int n64_calculate_crc(const n64_rom_t *rom, uint32_t *crc1, uint32_t *crc2)
{
    if (!rom || !crc1 || !crc2 || rom->size < 0x101000) return -1;
    
    /* N64 uses a special CRC algorithm based on the boot code */
    /* Simplified implementation */
    
    uint32_t seed = 0xF8CA4DDC;  /* CIC-6102 seed */
    
    uint32_t t1 = seed;
    uint32_t t2 = seed;
    uint32_t t3 = seed;
    uint32_t t4 = seed;
    uint32_t t5 = seed;
    uint32_t t6 = seed;
    
    for (size_t i = 0x1000; i < 0x101000; i += 4) {
        uint32_t d = read_be32(rom->data + i);
        
        if ((t6 + d) < t6) t4++;
        
        t6 += d;
        t3 ^= d;
        
        uint32_t r = ROL(d, (d & 0x1F));
        t5 += r;
        
        if (t2 > d) {
            t2 ^= r;
        } else {
            t2 ^= t6 ^ d;
        }
        
        t1 += read_be32(rom->data + 0x750 + (i & 0xFF)) ^ d;
    }
    
    *crc1 = t6 ^ t4 ^ t3;
    *crc2 = t5 ^ t2 ^ t1;
    
    return 0;
}

bool n64_verify_crc(const n64_rom_t *rom)
{
    if (!rom) return false;
    
    uint32_t crc1, crc2;
    if (n64_calculate_crc(rom, &crc1, &crc2) != 0) return false;
    
    return (crc1 == rom->header.crc1 && crc2 == rom->header.crc2);
}

int n64_fix_crc(n64_rom_t *rom)
{
    if (!rom || !rom->data) return -1;
    
    uint32_t crc1, crc2;
    if (n64_calculate_crc(rom, &crc1, &crc2) != 0) return -2;
    
    /* Write CRCs to header */
    write_be32(rom->data + 0x10, crc1);
    write_be32(rom->data + 0x14, crc2);
    
    rom->header.crc1 = crc1;
    rom->header.crc2 = crc2;
    
    return 0;
}

n64_cic_t n64_detect_cic(const n64_rom_t *rom)
{
    if (!rom || rom->size < 0x1000) return N64_CIC_UNKNOWN;
    
    /* Calculate checksum of boot code (0x40-0x1000) */
    init_crc_table();
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0x40; i < 0x1000; i++) {
        crc = crc_table[(crc ^ rom->data[i]) & 0xFF] ^ (crc >> 8);
    }
    crc ^= 0xFFFFFFFF;
    
    /* Match against known CIC checksums */
    switch (crc) {
        case 0x6170A4A1: return N64_CIC_6101;
        case 0x90BB6CB5: return N64_CIC_6102;
        case 0x0B050EE0: return N64_CIC_6103;
        case 0x98BC2C86: return N64_CIC_6105;
        case 0xACC8580A: return N64_CIC_6106;
        default:         return N64_CIC_UNKNOWN;
    }
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

void n64_print_info(const n64_rom_t *rom, FILE *fp)
{
    if (!rom || !fp) return;
    
    n64_info_t info;
    n64_get_info(rom, &info);
    
    fprintf(fp, "Nintendo 64 ROM:\n");
    fprintf(fp, "  Title: %s\n", info.title);
    fprintf(fp, "  Game Code: %s\n", info.full_code);
    fprintf(fp, "  Region: %s\n", info.region_name);
    fprintf(fp, "  Version: %d.%d\n", info.version >> 4, info.version & 0xF);
    fprintf(fp, "  Original Format: %s\n", info.format_name);
    fprintf(fp, "  ROM Size: %zu KB (%.1f MB)\n", 
            info.rom_size / 1024, info.rom_size / 1048576.0);
    fprintf(fp, "  CRC: %08X / %08X %s\n", 
            info.crc1, info.crc2, info.crc_valid ? "(VALID)" : "(INVALID)");
    fprintf(fp, "  CIC: %s\n", info.cic_name);
}
