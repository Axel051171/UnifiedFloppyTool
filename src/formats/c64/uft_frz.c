/**
 * @file uft_frz.c
 * @brief C64 Freezer Cartridge Snapshot Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/c64/uft_frz.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Detection
 * ============================================================================ */

frz_type_t frz_detect_type(const uint8_t *data, size_t size)
{
    if (!data || size < 1024) return FRZ_TYPE_UNKNOWN;
    
    /* Action Replay detection by size and structure */
    if (size >= 66816 && size <= 67584) {
        /* Check for typical AR memory layout */
        /* AR snapshots usually have specific patterns */
        return FRZ_TYPE_AR5;
    }
    
    if (size >= 67584 && size <= 68096) {
        return FRZ_TYPE_AR6;
    }
    
    /* Final Cartridge III - often starts with specific header */
    if (size >= 65792 && data[0] == 0xFC && data[1] == 0x03) {
        return FRZ_TYPE_FC3;
    }
    
    /* Super Snapshot V5 */
    if (size >= 66560 && data[0] == 'S' && data[1] == 'S') {
        return FRZ_TYPE_SS5;
    }
    
    /* Retro Replay */
    if (size >= 65536 + 1024 && data[0] == 'R' && data[1] == 'R') {
        return FRZ_TYPE_RR;
    }
    
    /* Generic - just RAM + minimal state */
    if (size >= 65536) {
        return FRZ_TYPE_GENERIC;
    }
    
    return FRZ_TYPE_UNKNOWN;
}

const char *frz_type_name(frz_type_t type)
{
    switch (type) {
        case FRZ_TYPE_AR4:      return "Action Replay MK4";
        case FRZ_TYPE_AR5:      return "Action Replay MK5";
        case FRZ_TYPE_AR6:      return "Action Replay MK6";
        case FRZ_TYPE_FC3:      return "Final Cartridge III";
        case FRZ_TYPE_SS5:      return "Super Snapshot V5";
        case FRZ_TYPE_RR:       return "Retro Replay";
        case FRZ_TYPE_NP:       return "Nordic Power";
        case FRZ_TYPE_GENERIC:  return "Generic Snapshot";
        default:                return "Unknown";
    }
}

bool frz_validate(const uint8_t *data, size_t size)
{
    return frz_detect_type(data, size) != FRZ_TYPE_UNKNOWN;
}

/* ============================================================================
 * Snapshot Operations
 * ============================================================================ */

/**
 * @brief Parse Action Replay format snapshot
 */
static int parse_action_replay(frz_snapshot_t *snapshot)
{
    if (snapshot->size < 65536 + 256) return -1;
    
    /* AR format: Header (256 bytes) + RAM (64KB) + Color RAM (1KB) */
    const uint8_t *hdr = snapshot->data;
    
    /* Parse CPU state from header */
    snapshot->state.cpu.a = hdr[0];
    snapshot->state.cpu.x = hdr[1];
    snapshot->state.cpu.y = hdr[2];
    snapshot->state.cpu.sp = hdr[3];
    snapshot->state.cpu.status = hdr[4];
    snapshot->state.cpu.pc = hdr[5] | (hdr[6] << 8);
    snapshot->state.cpu.port = hdr[7];
    snapshot->state.cpu.port_dir = hdr[8];
    
    /* Parse VIC-II registers */
    if (snapshot->size >= 256 + 64) {
        memcpy(snapshot->state.vic.regs, hdr + 16, FRZ_VIC_REGS);
    }
    
    /* Parse SID registers */
    if (snapshot->size >= 256 + 64 + 32) {
        memcpy(snapshot->state.sid.regs, hdr + 80, FRZ_SID_REGS);
    }
    
    /* Parse CIA registers */
    if (snapshot->size >= 256 + 64 + 32 + 32) {
        memcpy(snapshot->state.cia1.regs, hdr + 112, FRZ_CIA_REGS);
        memcpy(snapshot->state.cia2.regs, hdr + 128, FRZ_CIA_REGS);
    }
    
    /* Copy RAM */
    memcpy(snapshot->state.ram, snapshot->data + 256, FRZ_RAM_SIZE);
    
    /* Copy Color RAM if present */
    if (snapshot->size >= 256 + FRZ_RAM_SIZE + FRZ_COLORRAM_SIZE) {
        memcpy(snapshot->state.colorram, 
               snapshot->data + 256 + FRZ_RAM_SIZE, 
               FRZ_COLORRAM_SIZE);
    }
    
    return 0;
}

/**
 * @brief Parse generic format (just RAM)
 */
static int parse_generic(frz_snapshot_t *snapshot)
{
    if (snapshot->size < FRZ_RAM_SIZE) return -1;
    
    /* Copy RAM */
    memcpy(snapshot->state.ram, snapshot->data, FRZ_RAM_SIZE);
    
    /* Color RAM if present */
    if (snapshot->size >= FRZ_RAM_SIZE + FRZ_COLORRAM_SIZE) {
        memcpy(snapshot->state.colorram,
               snapshot->data + FRZ_RAM_SIZE,
               FRZ_COLORRAM_SIZE);
    }
    
    /* Set default CPU state */
    snapshot->state.cpu.sp = 0xFF;
    snapshot->state.cpu.status = 0x20;
    snapshot->state.cpu.pc = snapshot->data[0xFFFC] | 
                             (snapshot->data[0xFFFD] << 8);
    
    return 0;
}

int frz_open(const uint8_t *data, size_t size, frz_snapshot_t *snapshot)
{
    if (!data || !snapshot || size < 1024) return -1;
    
    memset(snapshot, 0, sizeof(frz_snapshot_t));
    
    snapshot->type = frz_detect_type(data, size);
    if (snapshot->type == FRZ_TYPE_UNKNOWN) return -2;
    
    /* Copy data */
    snapshot->data = malloc(size);
    if (!snapshot->data) return -3;
    
    memcpy(snapshot->data, data, size);
    snapshot->size = size;
    snapshot->owns_data = true;
    
    /* Parse based on type */
    int result;
    switch (snapshot->type) {
        case FRZ_TYPE_AR4:
        case FRZ_TYPE_AR5:
        case FRZ_TYPE_AR6:
            result = parse_action_replay(snapshot);
            break;
        case FRZ_TYPE_FC3:
        case FRZ_TYPE_SS5:
        case FRZ_TYPE_RR:
            result = parse_action_replay(snapshot);  /* Similar format */
            break;
        default:
            result = parse_generic(snapshot);
            break;
    }
    
    if (result == 0) {
        snapshot->state_valid = true;
    }
    
    return result;
}

int frz_load(const char *filename, frz_snapshot_t *snapshot)
{
    if (!filename || !snapshot) return -1;
    
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
    
    int result = frz_open(data, size, snapshot);
    free(data);
    
    return result;
}

int frz_save(const frz_snapshot_t *snapshot, const char *filename)
{
    if (!snapshot || !filename || !snapshot->data) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -2;
    
    size_t written = fwrite(snapshot->data, 1, snapshot->size, fp);
    fclose(fp);
    
    return (written == snapshot->size) ? 0 : -3;
}

void frz_close(frz_snapshot_t *snapshot)
{
    if (!snapshot) return;
    
    if (snapshot->owns_data) {
        free(snapshot->data);
    }
    
    memset(snapshot, 0, sizeof(frz_snapshot_t));
}

int frz_get_info(const frz_snapshot_t *snapshot, frz_info_t *info)
{
    if (!snapshot || !info) return -1;
    
    memset(info, 0, sizeof(frz_info_t));
    
    info->type = snapshot->type;
    info->type_name = frz_type_name(snapshot->type);
    info->file_size = snapshot->size;
    info->has_extended_state = (snapshot->type >= FRZ_TYPE_AR5);
    
    if (snapshot->state_valid) {
        info->start_address = snapshot->state.cpu.pc;
    }
    
    return 0;
}

/* ============================================================================
 * State Access
 * ============================================================================ */

int frz_get_cpu(const frz_snapshot_t *snapshot, frz_cpu_t *cpu)
{
    if (!snapshot || !cpu || !snapshot->state_valid) return -1;
    *cpu = snapshot->state.cpu;
    return 0;
}

int frz_get_vic(const frz_snapshot_t *snapshot, frz_vic_t *vic)
{
    if (!snapshot || !vic || !snapshot->state_valid) return -1;
    *vic = snapshot->state.vic;
    return 0;
}

int frz_get_sid(const frz_snapshot_t *snapshot, frz_sid_t *sid)
{
    if (!snapshot || !sid || !snapshot->state_valid) return -1;
    *sid = snapshot->state.sid;
    return 0;
}

int frz_get_ram(const frz_snapshot_t *snapshot, uint8_t *ram)
{
    if (!snapshot || !ram || !snapshot->state_valid) return -1;
    memcpy(ram, snapshot->state.ram, FRZ_RAM_SIZE);
    return 0;
}

int frz_get_colorram(const frz_snapshot_t *snapshot, uint8_t *colorram)
{
    if (!snapshot || !colorram || !snapshot->state_valid) return -1;
    memcpy(colorram, snapshot->state.colorram, FRZ_COLORRAM_SIZE);
    return 0;
}

uint8_t frz_peek(const frz_snapshot_t *snapshot, uint16_t address)
{
    if (!snapshot || !snapshot->state_valid) return 0;
    return snapshot->state.ram[address];
}

/* ============================================================================
 * Conversion
 * ============================================================================ */

int frz_extract_prg(const frz_snapshot_t *snapshot, uint16_t start_addr,
                    uint16_t end_addr, uint8_t *prg, size_t max_size,
                    size_t *prg_size)
{
    if (!snapshot || !prg || !snapshot->state_valid) return -1;
    if (end_addr <= start_addr) return -2;
    
    size_t data_len = end_addr - start_addr;
    size_t total_len = data_len + 2;  /* + load address */
    
    if (total_len > max_size) return -3;
    
    /* Write load address */
    prg[0] = start_addr & 0xFF;
    prg[1] = (start_addr >> 8) & 0xFF;
    
    /* Copy data */
    memcpy(prg + 2, snapshot->state.ram + start_addr, data_len);
    
    if (prg_size) *prg_size = total_len;
    return 0;
}

int frz_to_vsf(const frz_snapshot_t *snapshot, uint8_t *vsf_data,
               size_t max_size, size_t *vsf_size)
{
    if (!snapshot || !vsf_data || !snapshot->state_valid) return -1;
    
    /* Minimal VSF: header (37) + MAINCPU module + C64MEM module */
    size_t needed = 37 + 22 + 32 + 22 + FRZ_RAM_SIZE;
    if (max_size < needed) return -2;
    
    memset(vsf_data, 0, needed);
    
    /* VSF header */
    memcpy(vsf_data, "VICE Snapshot File\032", 19);
    vsf_data[19] = 1;  /* Version major */
    vsf_data[20] = 1;  /* Version minor */
    memcpy(vsf_data + 21, "C64", 3);
    
    /* MAINCPU module */
    uint8_t *mod = vsf_data + 37;
    memcpy(mod, "MAINCPU", 7);
    mod[16] = 1;  /* Version */
    mod[17] = 0;
    mod[18] = 32; /* Length */
    mod[19] = 0;
    mod[20] = 0;
    mod[21] = 0;
    
    /* CPU data */
    uint8_t *cpu = mod + 22;
    cpu[4] = snapshot->state.cpu.a;
    cpu[5] = snapshot->state.cpu.x;
    cpu[6] = snapshot->state.cpu.y;
    cpu[7] = snapshot->state.cpu.sp;
    cpu[8] = snapshot->state.cpu.pc & 0xFF;
    cpu[9] = snapshot->state.cpu.pc >> 8;
    cpu[10] = snapshot->state.cpu.status;
    
    /* C64MEM module */
    uint8_t *mem_mod = mod + 22 + 32;
    memcpy(mem_mod, "C64MEM", 6);
    mem_mod[16] = 1;
    mem_mod[17] = 0;
    mem_mod[18] = (FRZ_RAM_SIZE) & 0xFF;
    mem_mod[19] = (FRZ_RAM_SIZE >> 8) & 0xFF;
    mem_mod[20] = 0;
    mem_mod[21] = 0;
    
    /* RAM data */
    memcpy(mem_mod + 22, snapshot->state.ram, FRZ_RAM_SIZE);
    
    if (vsf_size) *vsf_size = needed;
    return 0;
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

void frz_print_info(const frz_snapshot_t *snapshot, FILE *fp)
{
    if (!snapshot || !fp) return;
    
    frz_info_t info;
    frz_get_info(snapshot, &info);
    
    fprintf(fp, "Freezer Snapshot:\n");
    fprintf(fp, "  Type: %s\n", info.type_name);
    fprintf(fp, "  File Size: %zu bytes\n", info.file_size);
    fprintf(fp, "  Extended State: %s\n", info.has_extended_state ? "Yes" : "No");
    
    if (snapshot->state_valid) {
        fprintf(fp, "  Start Address: $%04X\n", info.start_address);
        fprintf(fp, "\n");
        frz_print_cpu(&snapshot->state.cpu, fp);
    }
}

void frz_print_cpu(const frz_cpu_t *cpu, FILE *fp)
{
    if (!cpu || !fp) return;
    
    fprintf(fp, "CPU State:\n");
    fprintf(fp, "  A=$%02X X=$%02X Y=$%02X SP=$%02X\n",
            cpu->a, cpu->x, cpu->y, cpu->sp);
    fprintf(fp, "  PC=$%04X Status=$%02X [%c%c%c%c%c%c%c%c]\n",
            cpu->pc, cpu->status,
            (cpu->status & 0x80) ? 'N' : '-',
            (cpu->status & 0x40) ? 'V' : '-',
            (cpu->status & 0x20) ? '-' : '-',
            (cpu->status & 0x10) ? 'B' : '-',
            (cpu->status & 0x08) ? 'D' : '-',
            (cpu->status & 0x04) ? 'I' : '-',
            (cpu->status & 0x02) ? 'Z' : '-',
            (cpu->status & 0x01) ? 'C' : '-');
    fprintf(fp, "  Port=$%02X Dir=$%02X\n", cpu->port, cpu->port_dir);
}

void frz_hexdump(const frz_snapshot_t *snapshot, uint16_t start,
                 uint16_t length, FILE *fp)
{
    if (!snapshot || !fp || !snapshot->state_valid) return;
    
    for (uint16_t addr = start; addr < start + length; addr += 16) {
        fprintf(fp, "%04X: ", addr);
        
        for (int i = 0; i < 16 && addr + i < start + length; i++) {
            fprintf(fp, "%02X ", snapshot->state.ram[addr + i]);
        }
        
        fprintf(fp, " ");
        for (int i = 0; i < 16 && addr + i < start + length; i++) {
            uint8_t c = snapshot->state.ram[addr + i];
            fprintf(fp, "%c", (c >= 32 && c < 127) ? c : '.');
        }
        
        fprintf(fp, "\n");
    }
}
