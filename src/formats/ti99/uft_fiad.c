/**
 * @file uft_fiad.c
 * @brief FIAD (File In A Directory) Format Implementation for TI-99/4A
 * @version 3.8.0
 * @copyright MIT License
 */

#include "uft/formats/uft_fiad.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal Helpers
 * ═══════════════════════════════════════════════════════════════════════════════ */

static bool is_valid_ti_char(uint8_t c) {
    if (c >= 'A' && c <= 'Z') return true;
    if (c >= 'a' && c <= 'z') return true;
    if (c >= '0' && c <= '9') return true;
    if (c == ' ' || c == '-' || c == '_' || c == '!' || c == '.') return true;
    return false;
}

static void trim_filename(const char *src, char *dst, size_t max_len) {
    size_t len = max_len;
    while (len > 0 && src[len - 1] == ' ') len--;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static void pad_filename(const char *src, char *dst, size_t max_len) {
    size_t i = 0;
    while (i < max_len && src[i]) {
        dst[i] = (char)toupper((unsigned char)src[i]);
        i++;
    }
    while (i < max_len) dst[i++] = ' ';
}

static uft_fiad_type_t flags_to_type(uint8_t flags) {
    if (flags & UFT_FIAD_FLAG_PROGRAM) return UFT_FIAD_TYPE_PROGRAM;
    bool internal = (flags & UFT_FIAD_FLAG_INTERNAL) != 0;
    bool variable = (flags & UFT_FIAD_FLAG_VARIABLE) != 0;
    if (internal && variable) return UFT_FIAD_TYPE_INT_VAR;
    if (internal) return UFT_FIAD_TYPE_INT_FIX;
    if (variable) return UFT_FIAD_TYPE_DIS_VAR;
    return UFT_FIAD_TYPE_DIS_FIX;
}

static uint8_t type_to_flags(uft_fiad_type_t type, bool prot) {
    uint8_t flags = prot ? UFT_FIAD_FLAG_PROTECTED : 0;
    switch (type) {
        case UFT_FIAD_TYPE_PROGRAM: return flags | UFT_FIAD_FLAG_PROGRAM;
        case UFT_FIAD_TYPE_DIS_FIX: return flags;
        case UFT_FIAD_TYPE_DIS_VAR: return flags | UFT_FIAD_FLAG_VARIABLE;
        case UFT_FIAD_TYPE_INT_FIX: return flags | UFT_FIAD_FLAG_INTERNAL;
        case UFT_FIAD_TYPE_INT_VAR: return flags | UFT_FIAD_FLAG_INTERNAL | UFT_FIAD_FLAG_VARIABLE;
    }
    return flags;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool uft_fiad_is_valid(const uint8_t *data, size_t size) {
    if (!data || size < UFT_FIAD_HEADER_SIZE) return false;
    
    /* Validate filename - first char must be non-space */
    if (data[0] == ' ' || data[0] == 0) return false;
    
    /* Check all filename chars are valid */
    bool found_space = false;
    for (int i = 0; i < UFT_FIAD_FILENAME_LEN; i++) {
        if (data[i] == ' ') {
            found_space = true;
        } else if (found_space) {
            return false; /* Non-space after space */
        }
        if (!is_valid_ti_char(data[i])) return false;
    }
    
    /* Check flags are reasonable */
    uint8_t flags = data[12];
    uint8_t valid_flags = UFT_FIAD_FLAG_PROGRAM | UFT_FIAD_FLAG_INTERNAL |
                          UFT_FIAD_FLAG_PROTECTED | UFT_FIAD_FLAG_BACKUP |
                          UFT_FIAD_FLAG_MODIFIED | UFT_FIAD_FLAG_VARIABLE;
    if (flags & ~valid_flags) return false;
    
    /* Get sector count and verify size */
    uint16_t sectors = ((uint16_t)data[14] << 8) | data[15];
    size_t expected = UFT_FIAD_HEADER_SIZE + sectors * UFT_FIAD_SECTOR_SIZE;
    
    /* Allow some tolerance */
    return size >= UFT_FIAD_HEADER_SIZE && (sectors == 0 || size >= expected - UFT_FIAD_SECTOR_SIZE);
}

uft_fiad_error_t uft_fiad_get_info(const uint8_t *data, size_t size,
                                    uft_fiad_info_t *info) {
    if (!data || !info) return UFT_FIAD_ERR_PARAM;
    if (size < UFT_FIAD_HEADER_SIZE) return UFT_FIAD_ERR_SIZE;
    
    const uft_fiad_header_t *hdr = (const uft_fiad_header_t *)data;
    
    memset(info, 0, sizeof(*info));
    trim_filename(hdr->filename, info->filename, UFT_FIAD_FILENAME_LEN);
    
    info->total_sectors = ((uint16_t)hdr->sectors_hi << 8) | hdr->sectors_lo;
    info->num_records = ((uint16_t)hdr->l3_records_hi << 8) | hdr->l3_records_lo;
    info->rec_length = hdr->rec_length;
    info->recs_per_sector = hdr->recs_per_sector;
    info->eof_offset = hdr->eof_offset;
    info->protected = (hdr->flags & UFT_FIAD_FLAG_PROTECTED) != 0;
    info->modified = (hdr->flags & UFT_FIAD_FLAG_MODIFIED) != 0;
    info->type = flags_to_type(hdr->flags);
    
    if (info->total_sectors > 0) {
        info->data_size = (info->total_sectors - 1) * UFT_FIAD_SECTOR_SIZE;
        info->data_size += info->eof_offset ? info->eof_offset : UFT_FIAD_SECTOR_SIZE;
    }
    
    return UFT_FIAD_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * File Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_fiad_error_t uft_fiad_load(uft_fiad_file_t *file, const uint8_t *data, size_t size) {
    if (!file || !data) return UFT_FIAD_ERR_PARAM;
    if (size < UFT_FIAD_HEADER_SIZE) return UFT_FIAD_ERR_SIZE;
    
    memset(file, 0, sizeof(*file));
    memcpy(&file->header, data, UFT_FIAD_HEADER_SIZE);
    
    uint16_t sectors = ((uint16_t)file->header.sectors_hi << 8) | file->header.sectors_lo;
    size_t data_len = sectors * UFT_FIAD_SECTOR_SIZE;
    
    if (data_len > 0) {
        size_t available = size - UFT_FIAD_HEADER_SIZE;
        if (available < data_len) data_len = available;
        
        file->data = malloc(data_len);
        if (!file->data) return UFT_FIAD_ERR_MEMORY;
        memcpy(file->data, data + UFT_FIAD_HEADER_SIZE, data_len);
        file->data_size = data_len;
    }
    
    return UFT_FIAD_OK;
}

uft_fiad_error_t uft_fiad_load_file(uft_fiad_file_t *file, const char *path) {
    if (!file || !path) return UFT_FIAD_ERR_PARAM;
    
    FILE *fp = fopen(path, "rb");
    if (!fp) return UFT_FIAD_ERR_READ;
    
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (size < UFT_FIAD_HEADER_SIZE) {
        fclose(fp);
        return UFT_FIAD_ERR_SIZE;
    }
    
    uint8_t *data = malloc((size_t)size);
    if (!data) { fclose(fp); return UFT_FIAD_ERR_MEMORY; }
    
    if (fread(data, 1, (size_t)size, fp) != (size_t)size) {
        free(data); fclose(fp);
        return UFT_FIAD_ERR_READ;
    }
    fclose(fp);
    
    uft_fiad_error_t ret = uft_fiad_load(file, data, (size_t)size);
    free(data);
    return ret;
}

uft_fiad_error_t uft_fiad_save(const uft_fiad_file_t *file,
                                uint8_t *data, size_t size, size_t *written) {
    if (!file || !written) return UFT_FIAD_ERR_PARAM;
    
    size_t required = UFT_FIAD_HEADER_SIZE + file->data_size;
    *written = required;
    
    if (!data) return UFT_FIAD_OK;
    if (size < required) return UFT_FIAD_ERR_SIZE;
    
    memcpy(data, &file->header, UFT_FIAD_HEADER_SIZE);
    if (file->data && file->data_size > 0) {
        memcpy(data + UFT_FIAD_HEADER_SIZE, file->data, file->data_size);
    }
    
    return UFT_FIAD_OK;
}

uft_fiad_error_t uft_fiad_save_file(const uft_fiad_file_t *file, const char *path) {
    if (!file || !path) return UFT_FIAD_ERR_PARAM;
    
    FILE *fp = fopen(path, "wb");
    if (!fp) return UFT_FIAD_ERR_WRITE;
    
    if (fwrite(&file->header, UFT_FIAD_HEADER_SIZE, 1, fp) != 1) {
        fclose(fp); return UFT_FIAD_ERR_WRITE;
    }
    
    if (file->data && file->data_size > 0) {
        if (fwrite(file->data, file->data_size, 1, fp) != 1) {
            fclose(fp); return UFT_FIAD_ERR_WRITE;
        }
    }
    
    fclose(fp);
    return UFT_FIAD_OK;
}

void uft_fiad_free(uft_fiad_file_t *file) {
    if (!file) return;
    if (file->data) { free(file->data); file->data = NULL; }
    file->data_size = 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Creation
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_fiad_error_t uft_fiad_create(uft_fiad_file_t *file,
                                  const char *filename,
                                  uft_fiad_type_t type,
                                  uint8_t rec_length,
                                  const uint8_t *data, size_t data_size) {
    if (!file || !filename) return UFT_FIAD_ERR_PARAM;
    
    memset(file, 0, sizeof(*file));
    pad_filename(filename, file->header.filename, UFT_FIAD_FILENAME_LEN);
    
    file->header.flags = type_to_flags(type, false);
    file->header.rec_length = rec_length;
    
    if (type != UFT_FIAD_TYPE_PROGRAM && rec_length > 0) {
        bool variable = (type == UFT_FIAD_TYPE_DIS_VAR || type == UFT_FIAD_TYPE_INT_VAR);
        file->header.recs_per_sector = variable ? (255 / (rec_length + 1)) : (256 / rec_length);
    }
    
    if (data && data_size > 0) {
        uint16_t sectors = (uint16_t)((data_size + UFT_FIAD_SECTOR_SIZE - 1) / UFT_FIAD_SECTOR_SIZE);
        size_t padded = sectors * UFT_FIAD_SECTOR_SIZE;
        
        file->data = calloc(1, padded);
        if (!file->data) return UFT_FIAD_ERR_MEMORY;
        memcpy(file->data, data, data_size);
        file->data_size = padded;
        
        file->header.sectors_hi = (sectors >> 8) & 0xFF;
        file->header.sectors_lo = sectors & 0xFF;
        file->header.eof_offset = data_size % UFT_FIAD_SECTOR_SIZE;
    }
    
    return UFT_FIAD_OK;
}

uft_fiad_error_t uft_fiad_create_program(uft_fiad_file_t *file,
                                          const char *filename,
                                          const uint8_t *data, size_t size) {
    return uft_fiad_create(file, filename, UFT_FIAD_TYPE_PROGRAM, 0, data, size);
}

uft_fiad_error_t uft_fiad_create_dis_var80(uft_fiad_file_t *file,
                                            const char *filename,
                                            const char *text) {
    if (!file || !filename) return UFT_FIAD_ERR_PARAM;
    if (!text || !*text) {
        return uft_fiad_create(file, filename, UFT_FIAD_TYPE_DIS_VAR, 80, NULL, 0);
    }
    
    size_t text_len = strlen(text);
    size_t max_size = text_len * 2 + 1024;
    uint8_t *buf = calloc(1, max_size);
    if (!buf) return UFT_FIAD_ERR_MEMORY;
    
    size_t sector_idx = 0, sector_pos = 0;
    uint16_t record_count = 0;
    const char *line = text;
    
    while (*line) {
        const char *eol = strchr(line, '\n');
        size_t line_len = eol ? (size_t)(eol - line) : strlen(line);
        if (line_len > 0 && line[line_len - 1] == '\r') line_len--;
        if (line_len > 80) line_len = 80;
        
        if (sector_pos + 1 + line_len > UFT_FIAD_SECTOR_SIZE) {
            if (sector_pos < UFT_FIAD_SECTOR_SIZE)
                buf[sector_idx * UFT_FIAD_SECTOR_SIZE + sector_pos] = 0xFF;
            sector_idx++;
            sector_pos = 0;
        }
        
        size_t base = sector_idx * UFT_FIAD_SECTOR_SIZE;
        buf[base + sector_pos++] = (uint8_t)line_len;
        if (line_len > 0) {
            memcpy(buf + base + sector_pos, line, line_len);
            sector_pos += line_len;
        }
        record_count++;
        
        line = eol ? eol + 1 : line + strlen(line);
    }
    
    if (sector_pos < UFT_FIAD_SECTOR_SIZE)
        buf[sector_idx * UFT_FIAD_SECTOR_SIZE + sector_pos] = 0xFF;
    
    size_t total_size = (sector_idx + 1) * UFT_FIAD_SECTOR_SIZE;
    
    uft_fiad_error_t ret = uft_fiad_create(file, filename, UFT_FIAD_TYPE_DIS_VAR, 80, buf, total_size);
    if (ret == UFT_FIAD_OK) {
        file->header.l3_records_lo = record_count & 0xFF;
        file->header.l3_records_hi = (record_count >> 8) & 0xFF;
    }
    
    free(buf);
    return ret;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Extraction
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_fiad_error_t uft_fiad_extract(const uft_fiad_file_t *file,
                                   uint8_t *data, size_t size, size_t *extracted) {
    if (!file || !extracted) return UFT_FIAD_ERR_PARAM;
    
    uft_fiad_info_t info;
    uft_fiad_get_info((const uint8_t *)&file->header, UFT_FIAD_HEADER_SIZE + file->data_size, &info);
    
    *extracted = info.data_size;
    if (!data) return UFT_FIAD_OK;
    if (size < info.data_size) return UFT_FIAD_ERR_SIZE;
    
    if (file->data && info.data_size > 0) {
        memcpy(data, file->data, info.data_size);
    }
    
    return UFT_FIAD_OK;
}

uft_fiad_error_t uft_fiad_extract_text(const uft_fiad_file_t *file,
                                        char *text, size_t max_size) {
    if (!file || !text || max_size == 0) return UFT_FIAD_ERR_PARAM;
    
    uft_fiad_type_t type = flags_to_type(file->header.flags);
    if (type != UFT_FIAD_TYPE_DIS_VAR) return UFT_FIAD_ERR_INVALID;
    
    size_t out_pos = 0;
    const uint8_t *ptr = file->data;
    size_t remaining = file->data_size;
    
    while (remaining >= UFT_FIAD_SECTOR_SIZE && out_pos < max_size - 1) {
        size_t pos = 0;
        while (pos < UFT_FIAD_SECTOR_SIZE && out_pos < max_size - 1) {
            uint8_t rec_len = ptr[pos];
            if (rec_len == 0xFF) break;
            pos++;
            if (pos + rec_len > UFT_FIAD_SECTOR_SIZE) break;
            
            size_t copy = (rec_len < max_size - out_pos - 2) ? rec_len : max_size - out_pos - 2;
            memcpy(text + out_pos, ptr + pos, copy);
            out_pos += copy;
            text[out_pos++] = '\n';
            pos += rec_len;
        }
        ptr += UFT_FIAD_SECTOR_SIZE;
        remaining -= UFT_FIAD_SECTOR_SIZE;
    }
    
    text[out_pos] = '\0';
    return UFT_FIAD_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Conversion (FIAD <-> TIFILES)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Forward declaration needed */
struct uft_tifiles_file;

uft_fiad_error_t uft_fiad_to_tifiles(const uft_fiad_file_t *fiad,
                                      struct uft_tifiles_file *tifiles) {
    /* Implementation would require including uft_tifiles.h */
    /* For now, return not implemented */
    (void)fiad;
    (void)tifiles;
    return UFT_FIAD_ERR_PARAM;
}

uft_fiad_error_t uft_tifiles_to_fiad(const struct uft_tifiles_file *tifiles,
                                      uft_fiad_file_t *fiad) {
    (void)tifiles;
    (void)fiad;
    return UFT_FIAD_ERR_PARAM;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utilities
 * ═══════════════════════════════════════════════════════════════════════════════ */

size_t uft_fiad_calc_size(size_t data_size) {
    uint16_t sectors = (uint16_t)((data_size + UFT_FIAD_SECTOR_SIZE - 1) / UFT_FIAD_SECTOR_SIZE);
    return UFT_FIAD_HEADER_SIZE + sectors * UFT_FIAD_SECTOR_SIZE;
}

const char *uft_fiad_type_str(uft_fiad_type_t type) {
    switch (type) {
        case UFT_FIAD_TYPE_PROGRAM: return "PROGRAM";
        case UFT_FIAD_TYPE_DIS_FIX: return "DIS/FIX";
        case UFT_FIAD_TYPE_DIS_VAR: return "DIS/VAR";
        case UFT_FIAD_TYPE_INT_FIX: return "INT/FIX";
        case UFT_FIAD_TYPE_INT_VAR: return "INT/VAR";
    }
    return "UNKNOWN";
}

const char *uft_fiad_strerror(uft_fiad_error_t err) {
    switch (err) {
        case UFT_FIAD_OK:            return "Success";
        case UFT_FIAD_ERR_INVALID:   return "Invalid file";
        case UFT_FIAD_ERR_SIZE:      return "Size mismatch";
        case UFT_FIAD_ERR_READ:      return "Read error";
        case UFT_FIAD_ERR_WRITE:     return "Write error";
        case UFT_FIAD_ERR_MEMORY:    return "Memory error";
        case UFT_FIAD_ERR_PARAM:     return "Invalid parameter";
    }
    return "Unknown error";
}

uft_fiad_type_t uft_fiad_parse_type(uint8_t flags) {
    return flags_to_type(flags);
}

uint8_t uft_fiad_build_flags(uft_fiad_type_t type, bool protected) {
    return type_to_flags(type, protected);
}

bool uft_fiad_validate_filename(const char *filename) {
    if (!filename || *filename == ' ' || *filename == '\0') return false;
    
    size_t len = strlen(filename);
    if (len > UFT_FIAD_FILENAME_LEN) return false;
    
    for (size_t i = 0; i < len; i++) {
        if (!is_valid_ti_char((uint8_t)filename[i])) return false;
    }
    
    return true;
}

void uft_fiad_format_filename(const char *src, char *dst) {
    if (!src || !dst) return;
    pad_filename(src, dst, UFT_FIAD_FILENAME_LEN);
    dst[UFT_FIAD_FILENAME_LEN] = '\0';
}
