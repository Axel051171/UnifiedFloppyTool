/**
 * @file uft_vsf.c
 * @brief VICE Snapshot Format (VSF) Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/c64/uft_vsf.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint32_t read_le32(const uint8_t *data)
{
    return data[0] | (data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

/* ============================================================================
 * Detection
 * ============================================================================ */

bool vsf_detect(const uint8_t *data, size_t size)
{
    if (!data || size < VSF_MAGIC_LEN) {
        return false;
    }
    return memcmp(data, VSF_MAGIC, VSF_MAGIC_LEN) == 0;
}

bool vsf_validate(const uint8_t *data, size_t size)
{
    if (!vsf_detect(data, size)) {
        return false;
    }
    
    /* Need at least header */
    if (size < 37) {  /* 19 + 2 + 16 */
        return false;
    }
    
    return true;
}

vsf_machine_t vsf_get_machine_type(const char *name)
{
    if (!name) return VSF_MACHINE_UNKNOWN;
    
    if (strncmp(name, "C64", 3) == 0) return VSF_MACHINE_C64;
    if (strncmp(name, "C128", 4) == 0) return VSF_MACHINE_C128;
    if (strncmp(name, "VIC20", 5) == 0 || strncmp(name, "VIC-20", 6) == 0)
        return VSF_MACHINE_VIC20;
    if (strncmp(name, "PET", 3) == 0) return VSF_MACHINE_PET;
    if (strncmp(name, "PLUS4", 5) == 0 || strncmp(name, "PLUS/4", 6) == 0)
        return VSF_MACHINE_PLUS4;
    if (strncmp(name, "CBM", 3) == 0) return VSF_MACHINE_CBM2;
    
    return VSF_MACHINE_UNKNOWN;
}

const char *vsf_machine_name(vsf_machine_t machine)
{
    switch (machine) {
        case VSF_MACHINE_C64:   return "Commodore 64";
        case VSF_MACHINE_C128:  return "Commodore 128";
        case VSF_MACHINE_VIC20: return "VIC-20";
        case VSF_MACHINE_PET:   return "PET";
        case VSF_MACHINE_PLUS4: return "Plus/4";
        case VSF_MACHINE_CBM2:  return "CBM-II";
        default: return "Unknown";
    }
}

/* ============================================================================
 * Snapshot Operations
 * ============================================================================ */

int vsf_open(const uint8_t *data, size_t size, vsf_snapshot_t *snapshot)
{
    if (!data || !snapshot || size < 37) {
        return -1;
    }
    
    if (!vsf_validate(data, size)) {
        return -2;
    }
    
    memset(snapshot, 0, sizeof(vsf_snapshot_t));
    
    /* Copy data */
    snapshot->data = malloc(size);
    if (!snapshot->data) return -3;
    
    memcpy(snapshot->data, data, size);
    snapshot->size = size;
    snapshot->owns_data = true;
    
    /* Parse header */
    memcpy(snapshot->header.magic, data, 19);
    snapshot->header.version_major = data[19];
    snapshot->header.version_minor = data[20];
    memcpy(snapshot->header.machine, data + 21, 16);
    
    /* Determine machine type */
    snapshot->machine = vsf_get_machine_type(snapshot->header.machine);
    
    /* Allocate module array */
    snapshot->modules = calloc(VSF_MAX_MODULES, sizeof(vsf_module_t));
    if (!snapshot->modules) {
        free(snapshot->data);
        return -4;
    }
    
    /* Parse modules */
    size_t offset = 37;  /* After header */
    snapshot->num_modules = 0;
    
    while (offset + 22 <= size && snapshot->num_modules < VSF_MAX_MODULES) {
        vsf_module_t *mod = &snapshot->modules[snapshot->num_modules];
        
        /* Read module header */
        memcpy(mod->name, data + offset, 16);
        mod->name[16] = '\0';
        
        /* Trim trailing spaces */
        for (int i = 15; i >= 0 && mod->name[i] == ' '; i--) {
            mod->name[i] = '\0';
        }
        
        mod->version_major = data[offset + 16];
        mod->version_minor = data[offset + 17];
        mod->length = read_le32(data + offset + 18);
        mod->offset = offset + 22;
        mod->data = snapshot->data + mod->offset;
        
        /* Validate module length */
        if (mod->offset + mod->length > size) {
            break;
        }
        
        snapshot->num_modules++;
        offset = mod->offset + mod->length;
    }
    
    return 0;
}

int vsf_load(const char *filename, vsf_snapshot_t *snapshot)
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
    
    int result = vsf_open(data, size, snapshot);
    free(data);
    
    return result;
}

void vsf_close(vsf_snapshot_t *snapshot)
{
    if (!snapshot) return;
    
    if (snapshot->owns_data) {
        free(snapshot->data);
    }
    free(snapshot->modules);
    
    memset(snapshot, 0, sizeof(vsf_snapshot_t));
}

int vsf_get_info(const vsf_snapshot_t *snapshot, vsf_info_t *info)
{
    if (!snapshot || !info) return -1;
    
    memset(info, 0, sizeof(vsf_info_t));
    
    info->machine = snapshot->machine;
    memcpy(info->machine_name, snapshot->header.machine, 16);
    info->machine_name[16] = '\0';
    info->version_major = snapshot->header.version_major;
    info->version_minor = snapshot->header.version_minor;
    info->num_modules = snapshot->num_modules;
    
    return 0;
}

/* ============================================================================
 * Module Operations
 * ============================================================================ */

int vsf_get_module_count(const vsf_snapshot_t *snapshot)
{
    return snapshot ? snapshot->num_modules : 0;
}

int vsf_get_module(const vsf_snapshot_t *snapshot, int index, vsf_module_t *module)
{
    if (!snapshot || !module || index < 0 || index >= snapshot->num_modules) {
        return -1;
    }
    
    *module = snapshot->modules[index];
    return 0;
}

int vsf_find_module(const vsf_snapshot_t *snapshot, const char *name,
                    vsf_module_t *module)
{
    if (!snapshot || !name) return -1;
    
    for (int i = 0; i < snapshot->num_modules; i++) {
        if (strcmp(snapshot->modules[i].name, name) == 0) {
            if (module) *module = snapshot->modules[i];
            return 0;
        }
    }
    
    return -1;  /* Not found */
}

int vsf_get_module_data(const vsf_snapshot_t *snapshot, const char *name,
                        const uint8_t **data, size_t *size)
{
    if (!snapshot || !name || !data || !size) return -1;
    
    vsf_module_t module;
    if (vsf_find_module(snapshot, name, &module) != 0) {
        return -1;
    }
    
    *data = module.data;
    *size = module.length;
    return 0;
}

/* ============================================================================
 * State Extraction
 * ============================================================================ */

int vsf_get_cpu_state(const vsf_snapshot_t *snapshot, vsf_cpu_state_t *state)
{
    if (!snapshot || !state) return -1;
    
    const uint8_t *data;
    size_t size;
    
    if (vsf_get_module_data(snapshot, VSF_MODULE_MAINCPU, &data, &size) != 0) {
        return -2;
    }
    
    if (size < 10) return -3;
    
    memset(state, 0, sizeof(vsf_cpu_state_t));
    
    /* VICE MAINCPU module format (may vary by version) */
    /* Typically: CLK, A, X, Y, SP, PC, Status, IRQ, NMI, etc. */
    /* Skip clock (4 bytes typically) */
    size_t offset = 4;
    
    if (size >= offset + 8) {
        state->a = data[offset + 0];
        state->x = data[offset + 1];
        state->y = data[offset + 2];
        state->sp = data[offset + 3];
        state->pc = data[offset + 4] | (data[offset + 5] << 8);
        state->status = data[offset + 6];
    }
    
    return 0;
}

int vsf_extract_memory(const vsf_snapshot_t *snapshot, uint8_t *memory,
                       size_t max_size, size_t *extracted)
{
    if (!snapshot || !memory) return -1;
    
    const uint8_t *data;
    size_t size;
    
    /* Try C64MEM module */
    if (vsf_get_module_data(snapshot, VSF_MODULE_C64MEM, &data, &size) != 0) {
        /* Try generic "MEM" module */
        if (vsf_get_module_data(snapshot, "MEM", &data, &size) != 0) {
            return -2;
        }
    }
    
    /* C64MEM usually has 64KB of RAM */
    size_t mem_offset = 0;
    size_t mem_size = size;
    
    /* Skip header bytes if present */
    if (size > 65536) {
        mem_offset = size - 65536;
        mem_size = 65536;
    }
    
    if (mem_size > max_size) {
        mem_size = max_size;
    }
    
    memcpy(memory, data + mem_offset, mem_size);
    
    if (extracted) *extracted = mem_size;
    return 0;
}

int vsf_extract_colorram(const vsf_snapshot_t *snapshot, uint8_t *colorram,
                         size_t *size)
{
    if (!snapshot || !colorram) return -1;
    
    const uint8_t *data;
    size_t mod_size;
    
    if (vsf_get_module_data(snapshot, VSF_MODULE_VIC_II, &data, &mod_size) != 0) {
        return -2;
    }
    
    /* Color RAM is usually at the end of VIC-II module */
    /* Or in a separate COLORRAM module */
    if (mod_size >= 1024) {
        /* Extract last 1KB as color RAM (simplified) */
        size_t offset = mod_size - 1024;
        memcpy(colorram, data + offset, 1024);
        if (size) *size = 1024;
        return 0;
    }
    
    return -3;
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

void vsf_list_modules(const vsf_snapshot_t *snapshot, FILE *fp)
{
    if (!snapshot || !fp) return;
    
    fprintf(fp, "Modules (%d):\n", snapshot->num_modules);
    
    for (int i = 0; i < snapshot->num_modules; i++) {
        vsf_module_t *mod = &snapshot->modules[i];
        fprintf(fp, "  %2d: %-16s v%d.%d  %6u bytes\n",
                i, mod->name, mod->version_major, mod->version_minor,
                mod->length);
    }
}

void vsf_print_info(const vsf_snapshot_t *snapshot, FILE *fp)
{
    if (!snapshot || !fp) return;
    
    vsf_info_t info;
    vsf_get_info(snapshot, &info);
    
    fprintf(fp, "VICE Snapshot File:\n");
    fprintf(fp, "  Version: %d.%d\n", info.version_major, info.version_minor);
    fprintf(fp, "  Machine: %s (%s)\n", info.machine_name,
            vsf_machine_name(info.machine));
    fprintf(fp, "  Modules: %d\n", info.num_modules);
    fprintf(fp, "  File Size: %zu bytes\n", snapshot->size);
    
    fprintf(fp, "\n");
    vsf_list_modules(snapshot, fp);
    
    /* Try to show CPU state */
    vsf_cpu_state_t cpu;
    if (vsf_get_cpu_state(snapshot, &cpu) == 0) {
        fprintf(fp, "\nCPU State:\n");
        fprintf(fp, "  A=$%02X X=$%02X Y=$%02X SP=$%02X\n",
                cpu.a, cpu.x, cpu.y, cpu.sp);
        fprintf(fp, "  PC=$%04X Status=$%02X\n", cpu.pc, cpu.status);
    }
}
