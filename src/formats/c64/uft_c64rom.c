/**
 * @file uft_c64rom.c
 * @brief C64 ROM Image Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/c64/uft_c64rom.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * CRC32 Implementation
 * ============================================================================ */

static uint32_t crc32_table[256];
static bool crc32_initialized = false;

static void init_crc32_table(void)
{
    if (crc32_initialized) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
        crc32_table[i] = crc;
    }
    crc32_initialized = true;
}

uint32_t c64rom_crc32(const uint8_t *data, size_t size)
{
    init_crc32_table();
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

/* ============================================================================
 * Detection
 * ============================================================================ */

c64rom_type_t c64rom_detect_type(size_t size)
{
    switch (size) {
        case C64ROM_BASIC_SIZE:
            /* 8KB could be BASIC or KERNAL - need content check */
            return C64ROM_TYPE_BASIC;
        case C64ROM_CHAR_SIZE:
            return C64ROM_TYPE_CHAR;
        case C64ROM_COMBINED_SIZE:
            return C64ROM_TYPE_COMBINED;
        case C64ROM_FULL_SIZE:
            return C64ROM_TYPE_FULL;
        default:
            return C64ROM_TYPE_UNKNOWN;
    }
}

c64rom_version_t c64rom_detect_version(const uint8_t *data, size_t size)
{
    if (!data || size < 8192) return C64ROM_VER_UNKNOWN;
    
    /* Check for JiffyDOS signature in KERNAL */
    const uint8_t *kernal = data;
    if (size >= C64ROM_COMBINED_SIZE) {
        kernal = data + C64ROM_BASIC_SIZE;  /* Skip BASIC */
    }
    
    /* JiffyDOS has "JIFFYDOS" string typically */
    for (size_t i = 0; i < 8192 - 8; i++) {
        if (memcmp(kernal + i, "JIFFYDOS", 8) == 0 ||
            memcmp(kernal + i, "JiffyDOS", 8) == 0) {
            return C64ROM_VER_JIFFYDOS;
        }
    }
    
    /* Check for Dolphin DOS */
    for (size_t i = 0; i + 7 < 8192; i++) {
        if (memcmp(kernal + i, "DOLPHIN", 7) == 0) {
            return C64ROM_VER_DOLPHINDOS;
        }
    }
    
    /* Check for SpeedDOS */
    for (size_t i = 0; i + 5 < 8192; i++) {
        if (memcmp(kernal + i, "SPEED", 5) == 0) {
            return C64ROM_VER_SPEEDDOS;
        }
    }
    
    /* Check CRC for original ROMs */
    uint32_t crc = c64rom_crc32(kernal, 8192);
    
    /* Known original KERNAL CRCs */
    if (crc == 0xDBE3E7C7 || crc == 0x39065497 || crc == 0x7360B296) {
        return C64ROM_VER_ORIGINAL;
    }
    
    return C64ROM_VER_CUSTOM;
}

const char *c64rom_type_name(c64rom_type_t type)
{
    switch (type) {
        case C64ROM_TYPE_BASIC:     return "BASIC ROM";
        case C64ROM_TYPE_KERNAL:    return "KERNAL ROM";
        case C64ROM_TYPE_CHAR:      return "Character ROM";
        case C64ROM_TYPE_COMBINED:  return "Combined BASIC+KERNAL";
        case C64ROM_TYPE_FULL:      return "Full ROM Set";
        default:                    return "Unknown";
    }
}

const char *c64rom_version_name(c64rom_version_t version)
{
    switch (version) {
        case C64ROM_VER_ORIGINAL:   return "Original Commodore";
        case C64ROM_VER_JIFFYDOS:   return "JiffyDOS";
        case C64ROM_VER_DOLPHINDOS: return "Dolphin DOS";
        case C64ROM_VER_SPEEDDOS:   return "SpeedDOS";
        case C64ROM_VER_PRODOS:     return "Professional DOS";
        case C64ROM_VER_EXOS:       return "EXOS";
        case C64ROM_VER_CUSTOM:     return "Custom/Modified";
        default:                    return "Unknown";
    }
}

bool c64rom_validate(const uint8_t *data, size_t size)
{
    if (!data) return false;
    
    c64rom_type_t type = c64rom_detect_type(size);
    if (type == C64ROM_TYPE_UNKNOWN) return false;
    
    /* For KERNAL, check reset vector points to valid address */
    if (type == C64ROM_TYPE_COMBINED || type == C64ROM_TYPE_FULL ||
        size == C64ROM_KERNAL_SIZE) {
        const uint8_t *kernal = data;
        if (size >= C64ROM_COMBINED_SIZE) {
            kernal = data + C64ROM_BASIC_SIZE;
        }
        
        /* Reset vector at $FFFC-$FFFD (offset 0x1FFC-0x1FFD in KERNAL) */
        uint16_t reset = kernal[0x1FFC] | (kernal[0x1FFD] << 8);
        
        /* Should point to KERNAL space ($E000-$FFFF) */
        if (reset < 0xE000) return false;
    }
    
    return true;
}

/* ============================================================================
 * ROM Operations
 * ============================================================================ */

int c64rom_open(const uint8_t *data, size_t size, c64rom_image_t *rom)
{
    if (!data || !rom) return -1;
    
    c64rom_type_t type = c64rom_detect_type(size);
    if (type == C64ROM_TYPE_UNKNOWN) return -2;
    
    memset(rom, 0, sizeof(c64rom_image_t));
    
    rom->data = malloc(size);
    if (!rom->data) return -3;
    
    memcpy(rom->data, data, size);
    rom->size = size;
    rom->type = type;
    rom->owns_data = true;
    
    /* Set up pointers based on type */
    switch (type) {
        case C64ROM_TYPE_BASIC:
            rom->basic = rom->data;
            break;
        case C64ROM_TYPE_KERNAL:
            rom->kernal = rom->data;
            break;
        case C64ROM_TYPE_CHAR:
            rom->charrom = rom->data;
            break;
        case C64ROM_TYPE_COMBINED:
            rom->basic = rom->data;
            rom->kernal = rom->data + C64ROM_BASIC_SIZE;
            break;
        case C64ROM_TYPE_FULL:
            rom->basic = rom->data;
            rom->kernal = rom->data + C64ROM_BASIC_SIZE;
            rom->charrom = rom->data + C64ROM_COMBINED_SIZE;
            break;
        default:
            break;
    }
    
    rom->version = c64rom_detect_version(data, size);
    
    return 0;
}

int c64rom_load(const char *filename, c64rom_image_t *rom)
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
    
    int result = c64rom_open(data, size, rom);
    free(data);
    
    return result;
}

int c64rom_save(const c64rom_image_t *rom, const char *filename)
{
    if (!rom || !filename || !rom->data) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -2;
    
    size_t written = fwrite(rom->data, 1, rom->size, fp);
    fclose(fp);
    
    return (written == rom->size) ? 0 : -3;
}

void c64rom_close(c64rom_image_t *rom)
{
    if (!rom) return;
    
    if (rom->owns_data) {
        free(rom->data);
    }
    
    memset(rom, 0, sizeof(c64rom_image_t));
}

int c64rom_get_info(const c64rom_image_t *rom, c64rom_info_t *info)
{
    if (!rom || !info) return -1;
    
    memset(info, 0, sizeof(c64rom_info_t));
    
    info->type = rom->type;
    info->version = rom->version;
    info->version_name = c64rom_version_name(rom->version);
    info->size = rom->size;
    info->crc32 = c64rom_crc32(rom->data, rom->size);
    info->has_basic = (rom->basic != NULL);
    info->has_kernal = (rom->kernal != NULL);
    info->has_char = (rom->charrom != NULL);
    
    return 0;
}

/* ============================================================================
 * ROM Access
 * ============================================================================ */

const uint8_t *c64rom_get_basic(const c64rom_image_t *rom)
{
    return rom ? rom->basic : NULL;
}

const uint8_t *c64rom_get_kernal(const c64rom_image_t *rom)
{
    return rom ? rom->kernal : NULL;
}

const uint8_t *c64rom_get_charrom(const c64rom_image_t *rom)
{
    return rom ? rom->charrom : NULL;
}

int c64rom_extract(const c64rom_image_t *rom, c64rom_type_t type,
                   uint8_t *buffer, size_t max_size, size_t *extracted)
{
    if (!rom || !buffer) return -1;
    
    const uint8_t *src = NULL;
    size_t size = 0;
    
    switch (type) {
        case C64ROM_TYPE_BASIC:
            src = rom->basic;
            size = C64ROM_BASIC_SIZE;
            break;
        case C64ROM_TYPE_KERNAL:
            src = rom->kernal;
            size = C64ROM_KERNAL_SIZE;
            break;
        case C64ROM_TYPE_CHAR:
            src = rom->charrom;
            size = C64ROM_CHAR_SIZE;
            break;
        default:
            return -2;
    }
    
    if (!src) return -3;
    if (max_size < size) return -4;
    
    memcpy(buffer, src, size);
    if (extracted) *extracted = size;
    
    return 0;
}

/* ============================================================================
 * KERNAL Analysis
 * ============================================================================ */

int c64rom_get_vectors(const c64rom_image_t *rom, c64rom_vectors_t *vectors)
{
    if (!rom || !vectors || !rom->kernal) return -1;
    
    const uint8_t *k = rom->kernal;
    
    /* Hardware vectors at end of KERNAL */
    vectors->nmi   = k[0x1FFA] | (k[0x1FFB] << 8);
    vectors->reset = k[0x1FFC] | (k[0x1FFD] << 8);
    vectors->irq   = k[0x1FFE] | (k[0x1FFF] << 8);
    
    /* Software vectors (jump table at $FF81+) */
    /* These are JMP instructions, extract targets */
    vectors->open   = k[0x1FC0 + 1] | (k[0x1FC0 + 2] << 8);  /* $FFC0 */
    vectors->close  = k[0x1FC3 + 1] | (k[0x1FC3 + 2] << 8);  /* $FFC3 */
    vectors->chkin  = k[0x1FC6 + 1] | (k[0x1FC6 + 2] << 8);  /* $FFC6 */
    vectors->chkout = k[0x1FC9 + 1] | (k[0x1FC9 + 2] << 8);  /* $FFC9 */
    vectors->clrchn = k[0x1FCC + 1] | (k[0x1FCC + 2] << 8);  /* $FFCC */
    vectors->chrin  = k[0x1FCF + 1] | (k[0x1FCF + 2] << 8);  /* $FFCF */
    vectors->chrout = k[0x1FD2 + 1] | (k[0x1FD2 + 2] << 8);  /* $FFD2 */
    vectors->load   = k[0x1FD5 + 1] | (k[0x1FD5 + 2] << 8);  /* $FFD5 */
    vectors->save   = k[0x1FD8 + 1] | (k[0x1FD8 + 2] << 8);  /* $FFD8 */
    
    vectors->brk = vectors->irq;  /* BRK uses same vector */
    
    return 0;
}

bool c64rom_is_jiffydos(const c64rom_image_t *rom)
{
    return rom && rom->version == C64ROM_VER_JIFFYDOS;
}

/* ============================================================================
 * ROM Creation/Patching
 * ============================================================================ */

int c64rom_create(const uint8_t *basic, const uint8_t *kernal,
                  const uint8_t *charrom, c64rom_image_t *rom)
{
    if (!rom) return -1;
    if (!basic && !kernal && !charrom) return -2;
    
    memset(rom, 0, sizeof(c64rom_image_t));
    
    /* Determine size and type */
    size_t size = 0;
    if (basic) size += C64ROM_BASIC_SIZE;
    if (kernal) size += C64ROM_KERNAL_SIZE;
    if (charrom) size += C64ROM_CHAR_SIZE;
    
    rom->data = calloc(1, size);
    if (!rom->data) return -3;
    
    rom->size = size;
    rom->owns_data = true;
    
    /* Copy ROM parts */
    size_t offset = 0;
    if (basic) {
        memcpy(rom->data + offset, basic, C64ROM_BASIC_SIZE);
        rom->basic = rom->data + offset;
        offset += C64ROM_BASIC_SIZE;
    }
    if (kernal) {
        memcpy(rom->data + offset, kernal, C64ROM_KERNAL_SIZE);
        rom->kernal = rom->data + offset;
        offset += C64ROM_KERNAL_SIZE;
    }
    if (charrom) {
        memcpy(rom->data + offset, charrom, C64ROM_CHAR_SIZE);
        rom->charrom = rom->data + offset;
    }
    
    /* Set type */
    if (basic && kernal && charrom) {
        rom->type = C64ROM_TYPE_FULL;
    } else if (basic && kernal) {
        rom->type = C64ROM_TYPE_COMBINED;
    } else if (kernal) {
        rom->type = C64ROM_TYPE_KERNAL;
    } else if (basic) {
        rom->type = C64ROM_TYPE_BASIC;
    } else {
        rom->type = C64ROM_TYPE_CHAR;
    }
    
    rom->version = c64rom_detect_version(rom->data, rom->size);
    
    return 0;
}

int c64rom_patch(c64rom_image_t *rom, uint16_t address, uint8_t value)
{
    if (!rom || !rom->data) return -1;
    if (address >= rom->size) return -2;
    
    rom->data[address] = value;
    rom->version = C64ROM_VER_CUSTOM;  /* Mark as modified */
    
    return 0;
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

void c64rom_print_info(const c64rom_image_t *rom, FILE *fp)
{
    if (!rom || !fp) return;
    
    c64rom_info_t info;
    c64rom_get_info(rom, &info);
    
    fprintf(fp, "C64 ROM Image:\n");
    fprintf(fp, "  Type: %s\n", c64rom_type_name(info.type));
    fprintf(fp, "  Version: %s\n", info.version_name);
    fprintf(fp, "  Size: %zu bytes\n", info.size);
    fprintf(fp, "  CRC32: %08X\n", info.crc32);
    fprintf(fp, "  Contains:\n");
    if (info.has_basic) fprintf(fp, "    - BASIC ROM (8KB)\n");
    if (info.has_kernal) fprintf(fp, "    - KERNAL ROM (8KB)\n");
    if (info.has_char) fprintf(fp, "    - Character ROM (4KB)\n");
    
    if (rom->kernal) {
        c64rom_vectors_t vectors;
        c64rom_get_vectors(rom, &vectors);
        fprintf(fp, "\n");
        c64rom_print_vectors(&vectors, fp);
    }
}

void c64rom_print_vectors(const c64rom_vectors_t *vectors, FILE *fp)
{
    if (!vectors || !fp) return;
    
    fprintf(fp, "KERNAL Vectors:\n");
    fprintf(fp, "  Hardware:\n");
    fprintf(fp, "    NMI:    $%04X\n", vectors->nmi);
    fprintf(fp, "    RESET:  $%04X\n", vectors->reset);
    fprintf(fp, "    IRQ:    $%04X\n", vectors->irq);
    fprintf(fp, "  I/O Routines:\n");
    fprintf(fp, "    OPEN:   $%04X\n", vectors->open);
    fprintf(fp, "    CLOSE:  $%04X\n", vectors->close);
    fprintf(fp, "    CHRIN:  $%04X\n", vectors->chrin);
    fprintf(fp, "    CHROUT: $%04X\n", vectors->chrout);
    fprintf(fp, "    LOAD:   $%04X\n", vectors->load);
    fprintf(fp, "    SAVE:   $%04X\n", vectors->save);
}
