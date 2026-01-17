/**
 * @file uft_neogeo.c
 * @brief SNK Neo Geo ROM Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/snk/uft_neogeo.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint32_t read_le32(const uint8_t *data)
{
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

/* ============================================================================
 * Detection
 * ============================================================================ */

bool neogeo_is_neo_format(const uint8_t *data, size_t size)
{
    if (!data || size < NEO_HEADER_SIZE) return false;
    return memcmp(data, NEO_MAGIC, NEO_MAGIC_SIZE) == 0;
}

neogeo_rom_type_t neogeo_detect_chip_type(const char *filename)
{
    if (!filename) return NEO_ROM_P;
    
    /* Find last component */
    const char *name = strrchr(filename, '/');
    if (!name) name = strrchr(filename, '\\');
    name = name ? name + 1 : filename;
    
    /* Check prefix */
    char upper[16] = {0};
    for (int i = 0; i < 15 && name[i]; i++) {
        upper[i] = toupper(name[i]);
    }
    
    if (upper[0] == 'P' || strstr(upper, "-P")) return NEO_ROM_P;
    if (upper[0] == 'S' || strstr(upper, "-S")) return NEO_ROM_S;
    if (upper[0] == 'M' || strstr(upper, "-M")) return NEO_ROM_M;
    if (upper[0] == 'V' || strstr(upper, "-V")) return NEO_ROM_V;
    if (upper[0] == 'C' || strstr(upper, "-C")) return NEO_ROM_C;
    
    return NEO_ROM_P;  /* Default to P-ROM */
}

const char *neogeo_system_name(neogeo_system_t system)
{
    switch (system) {
        case NEO_SYSTEM_MVS:    return "MVS (Arcade)";
        case NEO_SYSTEM_AES:    return "AES (Home)";
        case NEO_SYSTEM_CD:     return "Neo Geo CD";
        case NEO_SYSTEM_CDZ:    return "Neo Geo CDZ";
        default:                return "Unknown";
    }
}

const char *neogeo_region_name(neogeo_region_t region)
{
    switch (region) {
        case NEO_REGION_JAPAN:  return "Japan";
        case NEO_REGION_USA:    return "USA";
        case NEO_REGION_EUROPE: return "Europe";
        case NEO_REGION_ASIA:   return "Asia";
        default:                return "Unknown";
    }
}

const char *neogeo_rom_type_name(neogeo_rom_type_t type)
{
    switch (type) {
        case NEO_ROM_P: return "P-ROM (Program)";
        case NEO_ROM_S: return "S-ROM (Fix/Text)";
        case NEO_ROM_M: return "M-ROM (Z80 Music)";
        case NEO_ROM_V: return "V-ROM (Voice/ADPCM)";
        case NEO_ROM_C: return "C-ROM (Character/Sprite)";
        default:        return "Unknown";
    }
}

/* ============================================================================
 * ROM Operations
 * ============================================================================ */

int neogeo_open(const uint8_t *data, size_t size, neogeo_rom_t *rom)
{
    if (!data || !rom) return -1;
    
    memset(rom, 0, sizeof(neogeo_rom_t));
    
    rom->is_neo_format = neogeo_is_neo_format(data, size);
    
    if (rom->is_neo_format) {
        if (size < NEO_HEADER_SIZE) return -2;
        
        /* Parse NEO header */
        memcpy(rom->header.magic, data, 4);
        rom->header.p_rom_size = read_le32(data + 4);
        rom->header.s_rom_size = read_le32(data + 8);
        rom->header.m_rom_size = read_le32(data + 12);
        rom->header.v_rom_size = read_le32(data + 16);
        rom->header.c_rom_size = read_le32(data + 20);
        rom->header.year = read_le32(data + 24);
        rom->header.genre = read_le32(data + 28);
        rom->header.screenshot = read_le32(data + 32);
        rom->header.ngh = read_le32(data + 36);
        memcpy(rom->header.name, data + 40, 32);
        rom->header.name[32] = '\0';
        memcpy(rom->header.manufacturer, data + 72, 16);
        rom->header.manufacturer[16] = '\0';
        
        /* Calculate offsets */
        rom->p_offset = NEO_HEADER_SIZE;
        rom->s_offset = rom->p_offset + rom->header.p_rom_size;
        rom->m_offset = rom->s_offset + rom->header.s_rom_size;
        rom->v_offset = rom->m_offset + rom->header.m_rom_size;
        rom->c_offset = rom->v_offset + rom->header.v_rom_size;
        
        /* Validate size */
        size_t expected = NEO_HEADER_SIZE + 
                          rom->header.p_rom_size +
                          rom->header.s_rom_size +
                          rom->header.m_rom_size +
                          rom->header.v_rom_size +
                          rom->header.c_rom_size;
        
        if (size < expected) return -3;
    }
    
    /* Copy data */
    rom->data = malloc(size);
    if (!rom->data) return -4;
    
    memcpy(rom->data, data, size);
    rom->size = size;
    rom->owns_data = true;
    
    return 0;
}

int neogeo_load(const char *filename, neogeo_rom_t *rom)
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
    
    int result = neogeo_open(data, size, rom);
    free(data);
    
    return result;
}

void neogeo_close(neogeo_rom_t *rom)
{
    if (!rom) return;
    
    if (rom->owns_data) {
        free(rom->data);
    }
    
    memset(rom, 0, sizeof(neogeo_rom_t));
}

int neogeo_get_info(const neogeo_rom_t *rom, neogeo_info_t *info)
{
    if (!rom || !info) return -1;
    
    memset(info, 0, sizeof(neogeo_info_t));
    
    info->is_neo_format = rom->is_neo_format;
    info->total_size = rom->size;
    
    if (rom->is_neo_format) {
        strncpy(info->name, rom->header.name, 63);
        strncpy(info->manufacturer, rom->header.manufacturer, 31);
        info->ngh = rom->header.ngh;
        info->year = rom->header.year;
        info->p_size = rom->header.p_rom_size;
        info->s_size = rom->header.s_rom_size;
        info->m_size = rom->header.m_rom_size;
        info->v_size = rom->header.v_rom_size;
        info->c_size = rom->header.c_rom_size;
    }
    
    /* Default to MVS for .neo format */
    info->system = rom->is_neo_format ? NEO_SYSTEM_MVS : NEO_SYSTEM_UNKNOWN;
    
    return 0;
}

/* ============================================================================
 * ROM Access
 * ============================================================================ */

const uint8_t *neogeo_get_prom(const neogeo_rom_t *rom, size_t *size)
{
    if (!rom || !rom->is_neo_format) return NULL;
    if (size) *size = rom->header.p_rom_size;
    return rom->data + rom->p_offset;
}

const uint8_t *neogeo_get_srom(const neogeo_rom_t *rom, size_t *size)
{
    if (!rom || !rom->is_neo_format) return NULL;
    if (size) *size = rom->header.s_rom_size;
    return rom->data + rom->s_offset;
}

const uint8_t *neogeo_get_mrom(const neogeo_rom_t *rom, size_t *size)
{
    if (!rom || !rom->is_neo_format) return NULL;
    if (size) *size = rom->header.m_rom_size;
    return rom->data + rom->m_offset;
}

const uint8_t *neogeo_get_vrom(const neogeo_rom_t *rom, size_t *size)
{
    if (!rom || !rom->is_neo_format) return NULL;
    if (size) *size = rom->header.v_rom_size;
    return rom->data + rom->v_offset;
}

const uint8_t *neogeo_get_crom(const neogeo_rom_t *rom, size_t *size)
{
    if (!rom || !rom->is_neo_format) return NULL;
    if (size) *size = rom->header.c_rom_size;
    return rom->data + rom->c_offset;
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

void neogeo_print_info(const neogeo_rom_t *rom, FILE *fp)
{
    if (!rom || !fp) return;
    
    neogeo_info_t info;
    neogeo_get_info(rom, &info);
    
    fprintf(fp, "Neo Geo ROM:\n");
    fprintf(fp, "  Format: %s\n", info.is_neo_format ? ".neo container" : "Raw ROM");
    fprintf(fp, "  Total Size: %zu bytes (%.1f MB)\n",
            info.total_size, info.total_size / 1048576.0);
    
    if (info.is_neo_format) {
        fprintf(fp, "  Name: %s\n", info.name);
        fprintf(fp, "  Manufacturer: %s\n", info.manufacturer);
        fprintf(fp, "  NGH: %03d\n", info.ngh);
        fprintf(fp, "  Year: %d\n", info.year);
        fprintf(fp, "  P-ROM: %zu KB\n", info.p_size / 1024);
        fprintf(fp, "  S-ROM: %zu KB\n", info.s_size / 1024);
        fprintf(fp, "  M-ROM: %zu KB\n", info.m_size / 1024);
        fprintf(fp, "  V-ROM: %zu KB\n", info.v_size / 1024);
        fprintf(fp, "  C-ROM: %zu KB\n", info.c_size / 1024);
    }
}
