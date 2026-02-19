/**
 * @file uft_p00.c
 * @brief P00/S00/U00/R00 PC64 File Format Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/c64/uft_p00.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ============================================================================
 * Detection
 * ============================================================================ */

bool p00_detect(const uint8_t *data, size_t size)
{
    if (!data || size < P00_HEADER_SIZE) {
        return false;
    }
    return memcmp(data, P00_MAGIC, P00_MAGIC_LEN) == 0;
}

bool p00_validate(const uint8_t *data, size_t size)
{
    if (!p00_detect(data, size)) {
        return false;
    }
    
    /* Check null terminator after magic */
    if (data[7] != 0x00) {
        return false;
    }
    
    return true;
}

p00_type_t p00_detect_type_from_name(const char *filename)
{
    if (!filename) return P00_TYPE_UNKNOWN;
    
    const char *ext = strrchr(filename, '.');
    if (!ext) {
        /* Check last 3 chars */
        size_t len = strlen(filename);
        if (len >= 3) {
            ext = filename + len - 3;
        } else {
            return P00_TYPE_UNKNOWN;
        }
    } else {
        ext++;
    }
    
    /* Check extension pattern */
    if (strlen(ext) >= 2) {
        char first = toupper(ext[0]);
        if (isdigit(ext[1]) && isdigit(ext[2])) {
            switch (first) {
                case 'P': return P00_TYPE_PRG;
                case 'S': return P00_TYPE_SEQ;
                case 'U': return P00_TYPE_USR;
                case 'R': return P00_TYPE_REL;
                case 'D': return P00_TYPE_DEL;
            }
        }
    }
    
    return P00_TYPE_UNKNOWN;
}

const char *p00_type_name(p00_type_t type)
{
    switch (type) {
        case P00_TYPE_DEL: return "DEL";
        case P00_TYPE_SEQ: return "SEQ";
        case P00_TYPE_PRG: return "PRG";
        case P00_TYPE_USR: return "USR";
        case P00_TYPE_REL: return "REL";
        default: return "Unknown";
    }
}

const char *p00_type_extension(p00_type_t type)
{
    switch (type) {
        case P00_TYPE_DEL: return "D00";
        case P00_TYPE_SEQ: return "S00";
        case P00_TYPE_PRG: return "P00";
        case P00_TYPE_USR: return "U00";
        case P00_TYPE_REL: return "R00";
        default: return "P00";
    }
}

/* ============================================================================
 * File Operations
 * ============================================================================ */

int p00_open(const uint8_t *data, size_t size, p00_file_t *file)
{
    if (!data || !file || size < P00_HEADER_SIZE) {
        return -1;
    }
    
    if (!p00_validate(data, size)) {
        return -2;
    }
    
    memset(file, 0, sizeof(p00_file_t));
    
    /* Copy data */
    file->data = malloc(size);
    if (!file->data) return -3;
    
    memcpy(file->data, data, size);
    file->size = size;
    file->owns_data = true;
    
    /* Parse header */
    memcpy(file->header.magic, data, 8);
    memcpy(file->header.filename, data + 8, 16);
    file->header.record_size = data[24];
    file->header.padding = data[25];
    
    /* Set data pointers */
    file->file_data = file->data + P00_HEADER_SIZE;
    file->file_data_size = size - P00_HEADER_SIZE;
    
    /* Determine type based on record size or default to PRG */
    if (file->header.record_size > 0) {
        file->type = P00_TYPE_REL;
    } else {
        file->type = P00_TYPE_PRG;  /* Default */
    }
    
    return 0;
}

int p00_load(const char *filename, p00_file_t *file)
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
    
    int result = p00_open(data, size, file);
    if (result == 0) {
        /* Try to detect type from filename */
        p00_type_t detected = p00_detect_type_from_name(filename);
        if (detected != P00_TYPE_UNKNOWN) {
            file->type = detected;
        }
    }
    
    free(data);
    return result;
}

int p00_save(const p00_file_t *file, const char *filename)
{
    if (!file || !filename || !file->data) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -2;
    
    size_t written = fwrite(file->data, 1, file->size, fp);
    fclose(fp);
    
    return (written == file->size) ? 0 : -3;
}

void p00_close(p00_file_t *file)
{
    if (!file) return;
    
    if (file->owns_data) {
        free(file->data);
    }
    
    memset(file, 0, sizeof(p00_file_t));
}

int p00_get_info(const p00_file_t *file, p00_info_t *info)
{
    if (!file || !info) return -1;
    
    memset(info, 0, sizeof(p00_info_t));
    
    info->type = file->type;
    info->record_size = file->header.record_size;
    info->data_size = file->file_data_size;
    
    /* Convert filename */
    p00_petscii_to_ascii((uint8_t*)file->header.filename, info->c64_filename, 16);
    
    /* Get load address for PRG */
    if (file->type == P00_TYPE_PRG && file->file_data_size >= 2) {
        info->load_address = file->file_data[0] | (file->file_data[1] << 8);
    }
    
    return 0;
}

/* ============================================================================
 * Data Access
 * ============================================================================ */

int p00_get_data(const p00_file_t *file, const uint8_t **data, size_t *size)
{
    if (!file || !data || !size) return -1;
    
    *data = file->file_data;
    *size = file->file_data_size;
    return 0;
}

void p00_get_filename(const p00_file_t *file, char *filename)
{
    if (!file || !filename) return;
    
    p00_petscii_to_ascii((uint8_t*)file->header.filename, filename, 16);
}

uint16_t p00_get_load_address(const p00_file_t *file)
{
    if (!file || file->type != P00_TYPE_PRG || file->file_data_size < 2) {
        return 0;
    }
    
    return file->file_data[0] | (file->file_data[1] << 8);
}

/* ============================================================================
 * Creation
 * ============================================================================ */

int p00_create(p00_type_t type, const char *c64_filename,
               const uint8_t *data, size_t size,
               uint8_t record_size, p00_file_t *file)
{
    if (!file || !data || size == 0) return -1;
    
    memset(file, 0, sizeof(p00_file_t));
    
    size_t total_size = P00_HEADER_SIZE + size;
    file->data = calloc(1, total_size);
    if (!file->data) return -2;
    
    file->size = total_size;
    file->owns_data = true;
    file->type = type;
    
    /* Write header */
    memcpy(file->data, "C64File", 7);
    file->data[7] = 0x00;
    
    /* Convert and write filename */
    if (c64_filename) {
        p00_ascii_to_petscii(c64_filename, file->data + 8, 16);
    }
    
    file->data[24] = record_size;
    file->data[25] = 0x00;
    
    /* Copy data */
    memcpy(file->data + P00_HEADER_SIZE, data, size);
    
    /* Parse header back */
    memcpy(file->header.magic, file->data, 8);
    memcpy(file->header.filename, file->data + 8, 16);
    file->header.record_size = record_size;
    
    file->file_data = file->data + P00_HEADER_SIZE;
    file->file_data_size = size;
    
    return 0;
}

int p00_from_prg(const char *c64_filename, const uint8_t *prg_data,
                 size_t prg_size, p00_file_t *file)
{
    return p00_create(P00_TYPE_PRG, c64_filename, prg_data, prg_size, 0, file);
}

int p00_extract_prg(const p00_file_t *file, uint8_t *prg_data,
                    size_t max_size, size_t *extracted)
{
    if (!file || !prg_data) return -1;
    
    if (file->file_data_size > max_size) return -2;
    
    memcpy(prg_data, file->file_data, file->file_data_size);
    
    if (extracted) *extracted = file->file_data_size;
    return 0;
}

/* ============================================================================
 * Conversion
 * ============================================================================ */

void p00_make_pc_filename(const char *c64_filename, char *pc_filename,
                          p00_type_t type)
{
    if (!c64_filename || !pc_filename) return;
    
    /* Copy and sanitize filename */
    int j = 0;
    for (int i = 0; c64_filename[i] && i < 16 && j < 240; i++) {
        char c = c64_filename[i];
        /* Replace invalid PC filename characters */
        if (c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            pc_filename[j++] = '_';
        } else if (c >= 32 && c < 127) {
            pc_filename[j++] = c;
        } else {
            pc_filename[j++] = '_';
        }
    }
    
    /* Add extension */
    pc_filename[j++] = '.';
    const char *ext = p00_type_extension(type);
    strncpy(pc_filename + j, ext, 255 - j);
    pc_filename[255] = '\0';
}

void p00_petscii_to_ascii(const uint8_t *petscii, char *ascii, int len)
{
    if (!petscii || !ascii) return;
    
    for (int i = 0; i < len; i++) {
        uint8_t c = petscii[i];
        
        if (c == 0xA0 || c == 0x00) {
            /* Shifted space or null - end of string */
            ascii[i] = '\0';
            return;
        } else if (c >= 0xC1 && c <= 0xDA) {
            /* Shifted uppercase */
            ascii[i] = c - 0x80;
        } else if (c >= 0x41 && c <= 0x5A) {
            /* Uppercase letters (display as lowercase) */
            ascii[i] = c + 0x20;
        } else if (c >= 0x61 && c <= 0x7A) {
            /* Lowercase (graphics in PETSCII) */
            ascii[i] = c - 0x20;
        } else if (c >= 0x20 && c < 0x7F) {
            ascii[i] = c;
        } else {
            ascii[i] = '_';
        }
    }
    ascii[len] = '\0';
}

void p00_ascii_to_petscii(const char *ascii, uint8_t *petscii, int len)
{
    if (!ascii || !petscii) return;
    
    memset(petscii, 0xA0, len);  /* Fill with shifted space */
    
    for (int i = 0; ascii[i] && i < len; i++) {
        char c = ascii[i];
        
        if (c >= 'a' && c <= 'z') {
            /* Lowercase -> uppercase PETSCII */
            petscii[i] = c - 0x20;
        } else if (c >= 'A' && c <= 'Z') {
            /* Uppercase -> uppercase PETSCII */
            petscii[i] = c;
        } else if (c >= 0x20 && c < 0x7F) {
            petscii[i] = c;
        } else {
            petscii[i] = 0xA0;
        }
    }
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

void p00_print_info(const p00_file_t *file, FILE *fp)
{
    if (!file || !fp) return;
    
    p00_info_t info;
    p00_get_info(file, &info);
    
    fprintf(fp, "PC64 File:\n");
    fprintf(fp, "  Type: %s\n", p00_type_name(info.type));
    fprintf(fp, "  C64 Filename: \"%s\"\n", info.c64_filename);
    fprintf(fp, "  Data Size: %zu bytes\n", info.data_size);
    
    if (info.type == P00_TYPE_PRG && info.data_size >= 2) {
        fprintf(fp, "  Load Address: $%04X\n", info.load_address);
        fprintf(fp, "  PRG Size: %zu bytes\n", info.data_size - 2);
    }
    
    if (info.type == P00_TYPE_REL && info.record_size > 0) {
        fprintf(fp, "  Record Size: %d bytes\n", info.record_size);
    }
}
