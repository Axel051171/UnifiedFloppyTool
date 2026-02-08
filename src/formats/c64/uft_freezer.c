/**
 * @file uft_freezer.c
 * @brief C64 Freezer Cartridge Snapshot Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/c64/uft_freezer.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Detection
 * ============================================================================ */

freezer_type_t freezer_detect(const uint8_t *data, size_t size)
{
    if (!data || size < 1024) return FREEZER_TYPE_UNKNOWN;
    
    /* Check for Retro Replay FRZ format */
    if (size > 6 && memcmp(data, "C64FRZ", 6) == 0) {
        return FREEZER_TYPE_RR;
    }
    
    /* Check for Action Replay by typical size and structure */
    /* AR snapshots are typically 66816-67328 bytes */
    if (size >= 66816 && size <= 67584) {
        /* Check for typical AR header patterns */
        /* First bytes usually contain CPU state */
        uint8_t potential_status = data[4];
        if ((potential_status & 0x20) != 0) {  /* Bit 5 always set */
            return FREEZER_TYPE_AR;
        }
    }
    
    /* Check for Final Cartridge III */
    if (size >= 65536 + 1024 && data[0] == 0xFC && data[1] == 0x03) {
        return FREEZER_TYPE_FC3;
    }
    
    /* Check for Super Snapshot */
    if (size >= 65536 && data[0] == 'S' && data[1] == 'S') {
        return FREEZER_TYPE_SS;
    }
    
    /* Check for Nordic Power */
    if (size >= 65536 && data[0] == 'N' && data[1] == 'P') {
        return FREEZER_TYPE_NP;
    }
    
    /* Generic - has at least 64KB (RAM) */
    if (size >= FREEZER_RAM_SIZE) {
        return FREEZER_TYPE_GENERIC;
    }
    
    return FREEZER_TYPE_UNKNOWN;
}

const char *freezer_type_name(freezer_type_t type)
{
    switch (type) {
        case FREEZER_TYPE_AR:      return "Action Replay";
        case FREEZER_TYPE_RR:      return "Retro Replay";
        case FREEZER_TYPE_FC3:     return "Final Cartridge III";
        case FREEZER_TYPE_SS:      return "Super Snapshot";
        case FREEZER_TYPE_NP:      return "Nordic Power";
        case FREEZER_TYPE_GENERIC: return "Generic Snapshot";
        default:                   return "Unknown";
    }
}

bool freezer_validate(const uint8_t *data, size_t size)
{
    return freezer_detect(data, size) != FREEZER_TYPE_UNKNOWN;
}

/* ============================================================================
 * Parsing Functions
 * ============================================================================ */

/**
 * @brief Parse Action Replay format
 */
static int parse_action_replay(freezer_snapshot_t *snapshot)
{
    const uint8_t *data = snapshot->data;
    size_t size = snapshot->size;
    
    if (size < 256 + FREEZER_RAM_SIZE) return -1;
    
    /* Header structure (varies by AR version):
     * 0x00-0x0F: CPU state
     * 0x10-0x5F: VIC-II registers  
     * 0x60-0x6F: CIA1
     * 0x70-0x7F: CIA2
     * 0x80-0x47F: Color RAM (1024 bytes)
     * 0x480+: Main RAM (65536 bytes)
     */
    
    /* CPU state */
    snapshot->state.cpu.a = data[0];
    snapshot->state.cpu.x = data[1];
    snapshot->state.cpu.y = data[2];
    snapshot->state.cpu.sp = data[3];
    snapshot->state.cpu.status = data[4];
    snapshot->state.cpu.pc = data[5] | (data[6] << 8);
    snapshot->state.cpu.port = data[7];
    snapshot->state.cpu.port_dir = data[8];
    
    /* VIC-II registers */
    memcpy(snapshot->state.vic.regs, data + 0x10, 64);
    snapshot->state.vic.bank = (snapshot->state.cia2.pra & 0x03) ^ 0x03;
    
    /* CIA1 */
    snapshot->state.cia1.pra = data[0x60];
    snapshot->state.cia1.prb = data[0x61];
    snapshot->state.cia1.ddra = data[0x62];
    snapshot->state.cia1.ddrb = data[0x63];
    snapshot->state.cia1.timer_a = data[0x64] | (data[0x65] << 8);
    snapshot->state.cia1.timer_b = data[0x66] | (data[0x67] << 8);
    snapshot->state.cia1.cra = data[0x6E];
    snapshot->state.cia1.crb = data[0x6F];
    
    /* CIA2 */
    snapshot->state.cia2.pra = data[0x70];
    snapshot->state.cia2.prb = data[0x71];
    snapshot->state.cia2.ddra = data[0x72];
    snapshot->state.cia2.ddrb = data[0x73];
    snapshot->state.cia2.timer_a = data[0x74] | (data[0x75] << 8);
    snapshot->state.cia2.timer_b = data[0x76] | (data[0x77] << 8);
    snapshot->state.cia2.cra = data[0x7E];
    snapshot->state.cia2.crb = data[0x7F];
    
    /* Color RAM */
    memcpy(snapshot->state.colorram, data + 0x80, FREEZER_COLORRAM_SIZE);
    
    /* Main RAM */
    size_t ram_offset = 0x80 + FREEZER_COLORRAM_SIZE;
    if (ram_offset + FREEZER_RAM_SIZE <= size) {
        memcpy(snapshot->state.ram, data + ram_offset, FREEZER_RAM_SIZE);
    }
    
    return 0;
}

/**
 * @brief Parse Retro Replay FRZ format
 */
static int parse_retro_replay(freezer_snapshot_t *snapshot)
{
    const uint8_t *data = snapshot->data;
    size_t size = snapshot->size;
    
    /* Verify header */
    if (size < 16 || memcmp(data, "C64FRZ", 6) != 0) {
        return -1;
    }
    
    uint8_t version = data[6];
    (void)version;
    
    /* RR FRZ format has structured sections */
    size_t offset = 16;
    
    /* CPU section */
    if (offset + 10 > size) return -2;
    snapshot->state.cpu.a = data[offset++];
    snapshot->state.cpu.x = data[offset++];
    snapshot->state.cpu.y = data[offset++];
    snapshot->state.cpu.sp = data[offset++];
    snapshot->state.cpu.status = data[offset++];
    snapshot->state.cpu.pc = data[offset] | (data[offset+1] << 8);
    offset += 2;
    snapshot->state.cpu.port = data[offset++];
    snapshot->state.cpu.port_dir = data[offset++];
    offset++; /* padding */
    
    /* VIC-II section */
    if (offset + 64 > size) return -3;
    memcpy(snapshot->state.vic.regs, data + offset, 64);
    offset += 64;
    
    /* SID section */
    if (offset + 32 > size) return -4;
    memcpy(snapshot->state.sid.regs, data + offset, 32);
    offset += 32;
    
    /* CIA sections */
    if (offset + 32 > size) return -5;
    snapshot->state.cia1.pra = data[offset++];
    snapshot->state.cia1.prb = data[offset++];
    snapshot->state.cia1.ddra = data[offset++];
    snapshot->state.cia1.ddrb = data[offset++];
    snapshot->state.cia1.timer_a = data[offset] | (data[offset+1] << 8);
    offset += 2;
    snapshot->state.cia1.timer_b = data[offset] | (data[offset+1] << 8);
    offset += 2;
    offset += 4; /* TOD */
    snapshot->state.cia1.sdr = data[offset++];
    snapshot->state.cia1.icr = data[offset++];
    snapshot->state.cia1.cra = data[offset++];
    snapshot->state.cia1.crb = data[offset++];
    
    /* CIA2 similar */
    snapshot->state.cia2.pra = data[offset++];
    snapshot->state.cia2.prb = data[offset++];
    snapshot->state.cia2.ddra = data[offset++];
    snapshot->state.cia2.ddrb = data[offset++];
    snapshot->state.cia2.timer_a = data[offset] | (data[offset+1] << 8);
    offset += 2;
    snapshot->state.cia2.timer_b = data[offset] | (data[offset+1] << 8);
    offset += 2;
    offset += 4; /* TOD */
    snapshot->state.cia2.sdr = data[offset++];
    snapshot->state.cia2.icr = data[offset++];
    snapshot->state.cia2.cra = data[offset++];
    snapshot->state.cia2.crb = data[offset++];
    
    /* Color RAM */
    if (offset + FREEZER_COLORRAM_SIZE > size) return -6;
    memcpy(snapshot->state.colorram, data + offset, FREEZER_COLORRAM_SIZE);
    offset += FREEZER_COLORRAM_SIZE;
    
    /* Main RAM */
    if (offset + FREEZER_RAM_SIZE > size) return -7;
    memcpy(snapshot->state.ram, data + offset, FREEZER_RAM_SIZE);
    
    return 0;
}

/**
 * @brief Parse generic/unknown format
 */
static int parse_generic(freezer_snapshot_t *snapshot)
{
    /* Assume raw RAM dump */
    if (snapshot->size < FREEZER_RAM_SIZE) return -1;
    
    memcpy(snapshot->state.ram, snapshot->data, FREEZER_RAM_SIZE);
    
    /* Color RAM if present */
    if (snapshot->size >= FREEZER_RAM_SIZE + FREEZER_COLORRAM_SIZE) {
        memcpy(snapshot->state.colorram, 
               snapshot->data + FREEZER_RAM_SIZE,
               FREEZER_COLORRAM_SIZE);
    }
    
    /* Set reasonable CPU defaults */
    snapshot->state.cpu.sp = 0xFF;
    snapshot->state.cpu.status = 0x24;  /* IRQ disabled, bit 5 set */
    snapshot->state.cpu.port = 0x37;
    snapshot->state.cpu.port_dir = 0x2F;
    
    /* Get reset vector as PC */
    snapshot->state.cpu.pc = snapshot->state.ram[0xFFFC] |
                             (snapshot->state.ram[0xFFFD] << 8);
    
    return 0;
}

/* ============================================================================
 * Snapshot Operations
 * ============================================================================ */

int freezer_open(const uint8_t *data, size_t size, freezer_snapshot_t *snapshot)
{
    if (!data || !snapshot || size < 1024) return -1;
    
    memset(snapshot, 0, sizeof(freezer_snapshot_t));
    
    snapshot->type = freezer_detect(data, size);
    if (snapshot->type == FREEZER_TYPE_UNKNOWN) return -2;
    
    /* Copy data */
    snapshot->data = malloc(size);
    if (!snapshot->data) return -3;
    
    memcpy(snapshot->data, data, size);
    snapshot->size = size;
    snapshot->owns_data = true;
    
    /* Parse based on type */
    int result;
    switch (snapshot->type) {
        case FREEZER_TYPE_AR:
            result = parse_action_replay(snapshot);
            break;
        case FREEZER_TYPE_RR:
            result = parse_retro_replay(snapshot);
            break;
        case FREEZER_TYPE_FC3:
        case FREEZER_TYPE_SS:
        case FREEZER_TYPE_NP:
            result = parse_action_replay(snapshot);  /* Similar format */
            break;
        default:
            result = parse_generic(snapshot);
            break;
    }
    
    snapshot->valid = (result == 0);
    return result;
}

int freezer_load(const char *filename, freezer_snapshot_t *snapshot)
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
    
    int result = freezer_open(data, size, snapshot);
    free(data);
    
    return result;
}

int freezer_save(const freezer_snapshot_t *snapshot, const char *filename,
                 freezer_type_t type)
{
    if (!snapshot || !filename || !snapshot->valid) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -2;
    
    size_t written = 0;
    
    if (type == FREEZER_TYPE_RR) {
        /* Write Retro Replay FRZ format */
        uint8_t header[16] = "C64FRZ";
        header[6] = RR_FRZ_VERSION;
        fwrite(header, 1, 16, fp);
        written += 16;
        
        /* CPU */
        uint8_t cpu[10];
        cpu[0] = snapshot->state.cpu.a;
        cpu[1] = snapshot->state.cpu.x;
        cpu[2] = snapshot->state.cpu.y;
        cpu[3] = snapshot->state.cpu.sp;
        cpu[4] = snapshot->state.cpu.status;
        cpu[5] = snapshot->state.cpu.pc & 0xFF;
        cpu[6] = snapshot->state.cpu.pc >> 8;
        cpu[7] = snapshot->state.cpu.port;
        cpu[8] = snapshot->state.cpu.port_dir;
        cpu[9] = 0;
        fwrite(cpu, 1, 10, fp);
        written += 10;
        
        /* VIC */
        fwrite(snapshot->state.vic.regs, 1, 64, fp);
        written += 64;
        
        /* SID */
        fwrite(snapshot->state.sid.regs, 1, 32, fp);
        written += 32;
        
        /* CIAs */
        /* ... simplified for brevity */
        uint8_t cia[32] = {0};
        fwrite(cia, 1, 32, fp);
        written += 32;
        
        /* Color RAM */
        fwrite(snapshot->state.colorram, 1, FREEZER_COLORRAM_SIZE, fp);
        written += FREEZER_COLORRAM_SIZE;
        
        /* RAM */
        fwrite(snapshot->state.ram, 1, FREEZER_RAM_SIZE, fp);
        written += FREEZER_RAM_SIZE;
    } else {
        /* Write raw format */
        fwrite(snapshot->data, 1, snapshot->size, fp);
        written = snapshot->size;
    }
    
    fclose(fp);
    return (written > 0) ? 0 : -3;
}

void freezer_close(freezer_snapshot_t *snapshot)
{
    if (!snapshot) return;
    
    if (snapshot->owns_data) {
        free(snapshot->data);
    }
    
    memset(snapshot, 0, sizeof(freezer_snapshot_t));
}

int freezer_get_info(const freezer_snapshot_t *snapshot, freezer_info_t *info)
{
    if (!snapshot || !info) return -1;
    
    memset(info, 0, sizeof(freezer_info_t));
    
    info->type = snapshot->type;
    info->type_name = freezer_type_name(snapshot->type);
    info->file_size = snapshot->size;
    info->entry_point = snapshot->state.cpu.pc;
    info->has_colorram = true;
    info->has_io = (snapshot->type != FREEZER_TYPE_GENERIC);
    
    return 0;
}

/* ============================================================================
 * State Access
 * ============================================================================ */

int freezer_get_cpu(const freezer_snapshot_t *snapshot, freezer_cpu_t *cpu)
{
    if (!snapshot || !cpu || !snapshot->valid) return -1;
    *cpu = snapshot->state.cpu;
    return 0;
}

int freezer_get_vic(const freezer_snapshot_t *snapshot, freezer_vic_t *vic)
{
    if (!snapshot || !vic || !snapshot->valid) return -1;
    *vic = snapshot->state.vic;
    return 0;
}

int freezer_get_sid(const freezer_snapshot_t *snapshot, freezer_sid_t *sid)
{
    if (!snapshot || !sid || !snapshot->valid) return -1;
    *sid = snapshot->state.sid;
    return 0;
}

int freezer_get_cia(const freezer_snapshot_t *snapshot, int cia_num,
                    freezer_cia_t *cia)
{
    if (!snapshot || !cia || !snapshot->valid) return -1;
    if (cia_num != 1 && cia_num != 2) return -2;
    
    *cia = (cia_num == 1) ? snapshot->state.cia1 : snapshot->state.cia2;
    return 0;
}

int freezer_get_ram(const freezer_snapshot_t *snapshot, uint16_t address,
                    uint8_t *buffer, size_t size)
{
    if (!snapshot || !buffer || !snapshot->valid) return -1;
    if ((size_t)address + size > FREEZER_RAM_SIZE) return -2;
    
    memcpy(buffer, snapshot->state.ram + address, size);
    return 0;
}

int freezer_get_colorram(const freezer_snapshot_t *snapshot, uint8_t *colorram)
{
    if (!snapshot || !colorram || !snapshot->valid) return -1;
    memcpy(colorram, snapshot->state.colorram, FREEZER_COLORRAM_SIZE);
    return 0;
}

/* ============================================================================
 * State Modification
 * ============================================================================ */

int freezer_set_cpu(freezer_snapshot_t *snapshot, const freezer_cpu_t *cpu)
{
    if (!snapshot || !cpu || !snapshot->valid) return -1;
    snapshot->state.cpu = *cpu;
    return 0;
}

int freezer_set_ram(freezer_snapshot_t *snapshot, uint16_t address,
                    const uint8_t *buffer, size_t size)
{
    if (!snapshot || !buffer || !snapshot->valid) return -1;
    if ((size_t)address + size > FREEZER_RAM_SIZE) return -2;
    
    memcpy(snapshot->state.ram + address, buffer, size);
    return 0;
}

/* ============================================================================
 * Conversion
 * ============================================================================ */

int freezer_extract_prg(const freezer_snapshot_t *snapshot,
                        uint16_t start_addr, uint16_t end_addr,
                        uint8_t *prg_data, size_t max_size, size_t *extracted)
{
    if (!snapshot || !prg_data || !snapshot->valid) return -1;
    if (end_addr <= start_addr) return -2;
    
    size_t data_size = end_addr - start_addr;
    size_t total_size = data_size + 2;  /* + load address */
    
    if (total_size > max_size) return -3;
    
    /* Load address (little endian) */
    prg_data[0] = start_addr & 0xFF;
    prg_data[1] = (start_addr >> 8) & 0xFF;
    
    /* Copy data */
    memcpy(prg_data + 2, snapshot->state.ram + start_addr, data_size);
    
    if (extracted) *extracted = total_size;
    return 0;
}

int freezer_extract_screen(const freezer_snapshot_t *snapshot,
                           uint8_t *screen, uint8_t *colors)
{
    if (!snapshot || !screen || !snapshot->valid) return -1;
    
    /* Get screen base from VIC */
    uint8_t d018 = snapshot->state.vic.regs[0x18];
    uint16_t screen_base = ((d018 >> 4) & 0x0F) * 0x400;
    
    /* Add VIC bank offset */
    uint16_t bank = (snapshot->state.cia2.pra & 0x03) ^ 0x03;
    screen_base += bank * 0x4000;
    
    /* Copy screen (1000 bytes = 40x25) */
    memcpy(screen, snapshot->state.ram + screen_base, 1000);
    
    /* Copy colors if requested */
    if (colors) {
        memcpy(colors, snapshot->state.colorram, 1000);
    }
    
    return 0;
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

void freezer_print_info(const freezer_snapshot_t *snapshot, FILE *fp)
{
    if (!snapshot || !fp) return;
    
    freezer_info_t info;
    freezer_get_info(snapshot, &info);
    
    fprintf(fp, "Freezer Snapshot:\n");
    fprintf(fp, "  Type: %s\n", info.type_name);
    fprintf(fp, "  File Size: %zu bytes\n", info.file_size);
    fprintf(fp, "  Entry Point: $%04X\n", info.entry_point);
    fprintf(fp, "  Has Color RAM: %s\n", info.has_colorram ? "Yes" : "No");
    fprintf(fp, "  Has I/O State: %s\n", info.has_io ? "Yes" : "No");
    
    if (snapshot->valid) {
        fprintf(fp, "\n");
        freezer_print_cpu(&snapshot->state.cpu, fp);
    }
}

void freezer_print_cpu(const freezer_cpu_t *cpu, FILE *fp)
{
    if (!cpu || !fp) return;
    
    fprintf(fp, "CPU State:\n");
    fprintf(fp, "  A=$%02X  X=$%02X  Y=$%02X  SP=$%02X\n",
            cpu->a, cpu->x, cpu->y, cpu->sp);
    fprintf(fp, "  PC=$%04X  Status=$%02X [%c%c-%c%c%c%c%c]\n",
            cpu->pc, cpu->status,
            (cpu->status & 0x80) ? 'N' : 'n',
            (cpu->status & 0x40) ? 'V' : 'v',
            (cpu->status & 0x10) ? 'B' : 'b',
            (cpu->status & 0x08) ? 'D' : 'd',
            (cpu->status & 0x04) ? 'I' : 'i',
            (cpu->status & 0x02) ? 'Z' : 'z',
            (cpu->status & 0x01) ? 'C' : 'c');
    fprintf(fp, "  Port=$%02X  Direction=$%02X\n",
            cpu->port, cpu->port_dir);
}

void freezer_print_vic(const freezer_vic_t *vic, FILE *fp)
{
    if (!vic || !fp) return;
    
    fprintf(fp, "VIC-II State:\n");
    fprintf(fp, "  Screen: $%04X\n", ((vic->regs[0x18] >> 4) & 0x0F) * 0x400);
    fprintf(fp, "  Charset: $%04X\n", ((vic->regs[0x18] & 0x0E) >> 1) * 0x800);
    fprintf(fp, "  Border: $%X  Background: $%X\n",
            vic->regs[0x20] & 0x0F, vic->regs[0x21] & 0x0F);
    fprintf(fp, "  Control: $%02X $%02X (", vic->regs[0x11], vic->regs[0x16]);
    
    if (vic->regs[0x11] & 0x20) fprintf(fp, "BITMAP ");
    if (vic->regs[0x16] & 0x10) fprintf(fp, "MULTICOLOR ");
    if (vic->regs[0x11] & 0x10) fprintf(fp, "38ROWS ");
    fprintf(fp, ")\n");
}
