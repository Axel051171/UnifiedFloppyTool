/**
 * @file uft_spectrum.c
 * @brief ZX Spectrum Tape and Disk Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/sinclair/uft_spectrum.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Detection
 * ============================================================================ */

spec_format_t spec_detect_format(const uint8_t *data, size_t size)
{
    if (!data || size < 16) return SPEC_FORMAT_UNKNOWN;
    
    /* Check TZX magic */
    if (size >= TZX_MAGIC_SIZE && memcmp(data, TZX_MAGIC, TZX_MAGIC_SIZE) == 0) {
        return SPEC_FORMAT_TZX;
    }
    
    /* Check for SNA (fixed sizes) */
    if (size == 49179 || size == 131103 || size == 147487) {
        return SPEC_FORMAT_SNA;
    }
    
    /* Check for Z80 */
    if (size >= 30) {
        /* Z80 v1: PC at offset 6-7 is non-zero */
        uint16_t pc = data[6] | (data[7] << 8);
        if (pc != 0) {
            /* Could be Z80 v1 */
            return SPEC_FORMAT_Z80;
        }
        /* Z80 v2/v3: PC==0, additional header at offset 30 */
        if (size >= 32) {
            uint16_t extra_len = data[30] | (data[31] << 8);
            if (extra_len == 23 || extra_len == 54 || extra_len == 55) {
                return SPEC_FORMAT_Z80;
            }
        }
    }
    
    /* Check for TAP */
    if (size >= 21) {
        /* TAP: first block should be header (19 bytes + flag) */
        uint16_t block_len = data[0] | (data[1] << 8);
        if (block_len == 19 && data[2] == TAP_BLOCK_HEADER) {
            return SPEC_FORMAT_TAP;
        }
    }
    
    /* Check for +3 DSK */
    if (size >= 256 && memcmp(data, "MV - CPC", 8) == 0) {
        return SPEC_FORMAT_DSK;
    }
    if (size >= 256 && memcmp(data, "EXTENDED", 8) == 0) {
        return SPEC_FORMAT_DSK;
    }
    
    return SPEC_FORMAT_UNKNOWN;
}

const char *spec_format_name(spec_format_t format)
{
    switch (format) {
        case SPEC_FORMAT_TAP: return "TAP (Raw tape)";
        case SPEC_FORMAT_TZX: return "TZX (Extended tape)";
        case SPEC_FORMAT_Z80: return "Z80 (Snapshot)";
        case SPEC_FORMAT_SNA: return "SNA (Snapshot)";
        case SPEC_FORMAT_DSK: return "DSK (+3 Disk)";
        default:              return "Unknown";
    }
}

const char *spec_model_name(spec_model_t model)
{
    switch (model) {
        case SPEC_MODEL_48K:   return "ZX Spectrum 48K";
        case SPEC_MODEL_128K:  return "ZX Spectrum 128K";
        case SPEC_MODEL_PLUS2: return "ZX Spectrum +2";
        case SPEC_MODEL_PLUS2A: return "ZX Spectrum +2A";
        case SPEC_MODEL_PLUS3: return "ZX Spectrum +3";
        default:               return "Unknown";
    }
}

/* ============================================================================
 * File Operations
 * ============================================================================ */

int spec_open(const uint8_t *data, size_t size, spec_file_t *file)
{
    if (!data || !file || size < 16) return -1;
    
    memset(file, 0, sizeof(spec_file_t));
    
    file->format = spec_detect_format(data, size);
    if (file->format == SPEC_FORMAT_UNKNOWN) return -2;
    
    file->data = malloc(size);
    if (!file->data) return -3;
    
    memcpy(file->data, data, size);
    file->size = size;
    file->owns_data = true;
    
    return 0;
}

int spec_load(const char *filename, spec_file_t *file)
{
    if (!filename || !file) return -1;
    
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
    
    int result = spec_open(data, size, file);
    free(data);
    
    return result;
}

void spec_close(spec_file_t *file)
{
    if (!file) return;
    
    if (file->owns_data) {
        free(file->data);
    }
    
    memset(file, 0, sizeof(spec_file_t));
}

int spec_get_info(const spec_file_t *file, spec_info_t *info)
{
    if (!file || !info) return -1;
    
    memset(info, 0, sizeof(spec_info_t));
    
    info->format = file->format;
    info->format_name = spec_format_name(file->format);
    info->file_size = file->size;
    
    switch (file->format) {
        case SPEC_FORMAT_SNA:
            if (file->size == 49179) {
                info->model = SPEC_MODEL_48K;
            } else {
                info->model = SPEC_MODEL_128K;
            }
            break;
            
        case SPEC_FORMAT_Z80:
            if (file->data[6] != 0 || file->data[7] != 0) {
                info->z80_version = Z80_VERSION_1;
                info->model = SPEC_MODEL_48K;
            } else {
                uint16_t extra = file->data[30] | (file->data[31] << 8);
                if (extra == 23) {
                    info->z80_version = Z80_VERSION_2;
                } else {
                    info->z80_version = Z80_VERSION_3;
                }
                /* Model from byte 34 */
                if (file->size > 34) {
                    uint8_t hw = file->data[34];
                    if (hw < 3) info->model = SPEC_MODEL_48K;
                    else if (hw < 6) info->model = SPEC_MODEL_128K;
                    else info->model = SPEC_MODEL_PLUS2;
                }
            }
            info->is_compressed = (file->data[12] & 0x20) != 0;
            break;
            
        case SPEC_FORMAT_TAP:
        case SPEC_FORMAT_TZX:
            info->block_count = spec_tap_block_count(file);
            info->model = SPEC_MODEL_48K;
            break;
            
        default:
            break;
    }
    
    info->model_name = spec_model_name(info->model);
    
    return 0;
}

/* ============================================================================
 * TAP Operations
 * ============================================================================ */

int spec_tap_block_count(const spec_file_t *file)
{
    if (!file || file->format != SPEC_FORMAT_TAP) return 0;
    
    int count = 0;
    size_t offset = 0;
    
    while (offset + 2 < file->size) {
        uint16_t block_len = file->data[offset] | (file->data[offset + 1] << 8);
        if (offset + 2 + block_len > file->size) break;
        
        count++;
        offset += 2 + block_len;
    }
    
    return count;
}

int spec_tap_get_block(const spec_file_t *file, int index,
                       const uint8_t **data, size_t *size, uint8_t *flag)
{
    if (!file || file->format != SPEC_FORMAT_TAP) return -1;
    
    int current = 0;
    size_t offset = 0;
    
    while (offset + 2 < file->size) {
        uint16_t block_len = file->data[offset] | (file->data[offset + 1] << 8);
        if (offset + 2 + block_len > file->size) return -2;
        
        if (current == index) {
            if (data) *data = file->data + offset + 3;
            if (size) *size = block_len - 1;
            if (flag) *flag = file->data[offset + 2];
            return 0;
        }
        
        current++;
        offset += 2 + block_len;
    }
    
    return -3;
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

void spec_print_info(const spec_file_t *file, FILE *fp)
{
    if (!file || !fp) return;
    
    spec_info_t info;
    spec_get_info(file, &info);
    
    fprintf(fp, "ZX Spectrum File:\n");
    fprintf(fp, "  Format: %s\n", info.format_name);
    fprintf(fp, "  Size: %zu bytes\n", info.file_size);
    fprintf(fp, "  Model: %s\n", info.model_name);
    
    if (info.format == SPEC_FORMAT_Z80) {
        fprintf(fp, "  Z80 Version: %d\n", info.z80_version);
        fprintf(fp, "  Compressed: %s\n", info.is_compressed ? "Yes" : "No");
    }
    
    if (info.block_count > 0) {
        fprintf(fp, "  Blocks: %d\n", info.block_count);
    }
}
