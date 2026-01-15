/**
 * @file uft_tifiles.c
 * @brief TIFILES Format Implementation for TI-99/4A
 * @version 3.8.0
 * @copyright MIT License
 */

#include "uft/formats/uft_tifiles.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

static const uint8_t TIFILES_SIG[8] = { 0x07, 'T', 'I', 'F', 'I', 'L', 'E', 'S' };

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal Helpers
 * ═══════════════════════════════════════════════════════════════════════════════ */

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

static uft_tifiles_type_t flags_to_type(uint8_t flags) {
    if (flags & UFT_TIFILES_FLAG_PROGRAM) return UFT_TIFILES_TYPE_PROGRAM;
    bool internal = (flags & UFT_TIFILES_FLAG_INTERNAL) != 0;
    bool variable = (flags & UFT_TIFILES_FLAG_VARIABLE) != 0;
    if (internal && variable) return UFT_TIFILES_TYPE_INT_VAR;
    if (internal) return UFT_TIFILES_TYPE_INT_FIX;
    if (variable) return UFT_TIFILES_TYPE_DIS_VAR;
    return UFT_TIFILES_TYPE_DIS_FIX;
}

static uint8_t type_to_flags(uft_tifiles_type_t type, bool prot) {
    uint8_t flags = prot ? UFT_TIFILES_FLAG_PROTECTED : 0;
    switch (type) {
        case UFT_TIFILES_TYPE_PROGRAM: return flags | UFT_TIFILES_FLAG_PROGRAM;
        case UFT_TIFILES_TYPE_DIS_FIX: return flags;
        case UFT_TIFILES_TYPE_DIS_VAR: return flags | UFT_TIFILES_FLAG_VARIABLE;
        case UFT_TIFILES_TYPE_INT_FIX: return flags | UFT_TIFILES_FLAG_INTERNAL;
        case UFT_TIFILES_TYPE_INT_VAR: return flags | UFT_TIFILES_FLAG_INTERNAL | UFT_TIFILES_FLAG_VARIABLE;
    }
    return flags;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool uft_tifiles_is_valid(const uint8_t *data, size_t size) {
    if (!data || size < UFT_TIFILES_HEADER_SIZE) return false;
    if (memcmp(data, TIFILES_SIG, 8) != 0) return false;
    
    const uft_tifiles_header_t *hdr = (const uft_tifiles_header_t *)data;
    uint16_t sectors = ((uint16_t)hdr->sectors_hi << 8) | hdr->sectors_lo;
    size_t expected = UFT_TIFILES_HEADER_SIZE + (sectors * UFT_TIFILES_SECTOR_SIZE);
    
    return size >= expected - UFT_TIFILES_SECTOR_SIZE;
}

uft_tifiles_error_t uft_tifiles_get_info(const uint8_t *data, size_t size,
                                          uft_tifiles_info_t *info) {
    if (!data || !info) return UFT_TIFILES_ERR_PARAM;
    if (!uft_tifiles_is_valid(data, size)) return UFT_TIFILES_ERR_SIGNATURE;
    
    const uft_tifiles_header_t *hdr = (const uft_tifiles_header_t *)data;
    
    memset(info, 0, sizeof(*info));
    trim_filename(hdr->filename, info->filename, UFT_TIFILES_FILENAME_LEN);
    
    info->total_sectors = ((uint16_t)hdr->sectors_hi << 8) | hdr->sectors_lo;
    info->num_records = ((uint16_t)hdr->num_records_hi << 8) | hdr->num_records_lo;
    info->rec_length = hdr->rec_length;
    info->recs_per_sector = hdr->recs_per_sector;
    info->eof_offset = hdr->eof_offset;
    info->protected = (hdr->flags & UFT_TIFILES_FLAG_PROTECTED) != 0;
    info->modified = (hdr->flags & UFT_TIFILES_FLAG_MODIFIED) != 0;
    info->type = flags_to_type(hdr->flags);
    
    if (info->total_sectors > 0) {
        info->data_size = (info->total_sectors - 1) * UFT_TIFILES_SECTOR_SIZE;
        info->data_size += info->eof_offset ? info->eof_offset : UFT_TIFILES_SECTOR_SIZE;
    }
    
    return UFT_TIFILES_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * File Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_tifiles_error_t uft_tifiles_load(uft_tifiles_file_t *file,
                                      const uint8_t *data, size_t size) {
    if (!file || !data) return UFT_TIFILES_ERR_PARAM;
    if (!uft_tifiles_is_valid(data, size)) return UFT_TIFILES_ERR_SIGNATURE;
    
    memset(file, 0, sizeof(*file));
    memcpy(&file->header, data, UFT_TIFILES_HEADER_SIZE);
    
    uint16_t sectors = ((uint16_t)file->header.sectors_hi << 8) | file->header.sectors_lo;
    size_t data_len = sectors * UFT_TIFILES_SECTOR_SIZE;
    
    if (data_len > 0) {
        size_t available = size - UFT_TIFILES_HEADER_SIZE;
        if (available < data_len) data_len = available;
        
        file->data = malloc(data_len);
        if (!file->data) return UFT_TIFILES_ERR_MEMORY;
        memcpy(file->data, data + UFT_TIFILES_HEADER_SIZE, data_len);
        file->data_size = data_len;
    }
    
    return UFT_TIFILES_OK;
}

uft_tifiles_error_t uft_tifiles_load_file(uft_tifiles_file_t *file, const char *path) {
    if (!file || !path) return UFT_TIFILES_ERR_PARAM;
    
    FILE *fp = fopen(path, "rb");
    if (!fp) return UFT_TIFILES_ERR_READ;
    
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (size < UFT_TIFILES_HEADER_SIZE) {
        fclose(fp);
        return UFT_TIFILES_ERR_SIZE;
    }
    
    uint8_t *data = malloc((size_t)size);
    if (!data) { fclose(fp); return UFT_TIFILES_ERR_MEMORY; }
    
    if (fread(data, 1, (size_t)size, fp) != (size_t)size) {
        free(data); fclose(fp);
        return UFT_TIFILES_ERR_READ;
    }
    fclose(fp);
    
    uft_tifiles_error_t ret = uft_tifiles_load(file, data, (size_t)size);
    free(data);
    return ret;
}

uft_tifiles_error_t uft_tifiles_save(const uft_tifiles_file_t *file,
                                      uint8_t *data, size_t size, size_t *written) {
    if (!file || !written) return UFT_TIFILES_ERR_PARAM;
    
    size_t required = UFT_TIFILES_HEADER_SIZE + file->data_size;
    *written = required;
    
    if (!data) return UFT_TIFILES_OK;
    if (size < required) return UFT_TIFILES_ERR_SIZE;
    
    memcpy(data, &file->header, UFT_TIFILES_HEADER_SIZE);
    if (file->data && file->data_size > 0) {
        memcpy(data + UFT_TIFILES_HEADER_SIZE, file->data, file->data_size);
    }
    
    return UFT_TIFILES_OK;
}

uft_tifiles_error_t uft_tifiles_save_file(const uft_tifiles_file_t *file, const char *path) {
    if (!file || !path) return UFT_TIFILES_ERR_PARAM;
    
    FILE *fp = fopen(path, "wb");
    if (!fp) return UFT_TIFILES_ERR_WRITE;
    
    if (fwrite(&file->header, UFT_TIFILES_HEADER_SIZE, 1, fp) != 1) {
        fclose(fp); return UFT_TIFILES_ERR_WRITE;
    }
    
    if (file->data && file->data_size > 0) {
        if (fwrite(file->data, file->data_size, 1, fp) != 1) {
            fclose(fp); return UFT_TIFILES_ERR_WRITE;
        }
    }
    
    fclose(fp);
    return UFT_TIFILES_OK;
}

void uft_tifiles_free(uft_tifiles_file_t *file) {
    if (!file) return;
    if (file->data) { free(file->data); file->data = NULL; }
    file->data_size = 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Creation
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_tifiles_error_t uft_tifiles_create(uft_tifiles_file_t *file,
                                        const char *filename,
                                        uft_tifiles_type_t type,
                                        uint8_t rec_length,
                                        const uint8_t *data, size_t data_size) {
    if (!file || !filename) return UFT_TIFILES_ERR_PARAM;
    
    memset(file, 0, sizeof(*file));
    memcpy(file->header.signature, TIFILES_SIG, 8);
    pad_filename(filename, file->header.filename, UFT_TIFILES_FILENAME_LEN);
    
    file->header.flags = type_to_flags(type, false);
    file->header.rec_length = rec_length;
    
    if (type != UFT_TIFILES_TYPE_PROGRAM && rec_length > 0) {
        bool variable = (type == UFT_TIFILES_TYPE_DIS_VAR || type == UFT_TIFILES_TYPE_INT_VAR);
        file->header.recs_per_sector = variable ? (255 / (rec_length + 1)) : (256 / rec_length);
    }
    
    if (data && data_size > 0) {
        uint16_t sectors = (uint16_t)((data_size + UFT_TIFILES_SECTOR_SIZE - 1) / UFT_TIFILES_SECTOR_SIZE);
        size_t padded = sectors * UFT_TIFILES_SECTOR_SIZE;
        
        file->data = calloc(1, padded);
        if (!file->data) return UFT_TIFILES_ERR_MEMORY;
        memcpy(file->data, data, data_size);
        file->data_size = padded;
        
        file->header.sectors_hi = (sectors >> 8) & 0xFF;
        file->header.sectors_lo = sectors & 0xFF;
        file->header.eof_offset = data_size % UFT_TIFILES_SECTOR_SIZE;
    }
    
    return UFT_TIFILES_OK;
}

uft_tifiles_error_t uft_tifiles_create_program(uft_tifiles_file_t *file,
                                                const char *filename,
                                                const uint8_t *data, size_t size) {
    return uft_tifiles_create(file, filename, UFT_TIFILES_TYPE_PROGRAM, 0, data, size);
}

uft_tifiles_error_t uft_tifiles_create_dis_var80(uft_tifiles_file_t *file,
                                                  const char *filename,
                                                  const char *text) {
    if (!file || !filename) return UFT_TIFILES_ERR_PARAM;
    if (!text || !*text) {
        return uft_tifiles_create(file, filename, UFT_TIFILES_TYPE_DIS_VAR, 80, NULL, 0);
    }
    
    size_t text_len = strlen(text);
    size_t max_size = text_len * 2 + 1024;
    uint8_t *buf = calloc(1, max_size);
    if (!buf) return UFT_TIFILES_ERR_MEMORY;
    
    size_t sector_idx = 0, sector_pos = 0;
    uint16_t record_count = 0;
    const char *line = text;
    
    while (*line) {
        const char *eol = strchr(line, '\n');
        size_t line_len = eol ? (size_t)(eol - line) : strlen(line);
        if (line_len > 0 && line[line_len - 1] == '\r') line_len--;
        if (line_len > 80) line_len = 80;
        
        if (sector_pos + 1 + line_len > UFT_TIFILES_SECTOR_SIZE) {
            if (sector_pos < UFT_TIFILES_SECTOR_SIZE)
                buf[sector_idx * UFT_TIFILES_SECTOR_SIZE + sector_pos] = 0xFF;
            sector_idx++;
            sector_pos = 0;
        }
        
        size_t base = sector_idx * UFT_TIFILES_SECTOR_SIZE;
        buf[base + sector_pos++] = (uint8_t)line_len;
        if (line_len > 0) {
            memcpy(buf + base + sector_pos, line, line_len);
            sector_pos += line_len;
        }
        record_count++;
        
        line = eol ? eol + 1 : line + strlen(line);
    }
    
    if (sector_pos < UFT_TIFILES_SECTOR_SIZE)
        buf[sector_idx * UFT_TIFILES_SECTOR_SIZE + sector_pos] = 0xFF;
    
    size_t total_size = (sector_idx + 1) * UFT_TIFILES_SECTOR_SIZE;
    
    uft_tifiles_error_t ret = uft_tifiles_create(file, filename, UFT_TIFILES_TYPE_DIS_VAR, 80, buf, total_size);
    if (ret == UFT_TIFILES_OK) {
        file->header.num_records_lo = record_count & 0xFF;
        file->header.num_records_hi = (record_count >> 8) & 0xFF;
    }
    
    free(buf);
    return ret;
}

uft_tifiles_error_t uft_tifiles_create_dis_fix(uft_tifiles_file_t *file,
                                                const char *filename,
                                                uint8_t rec_length,
                                                const uint8_t *data, size_t size) {
    return uft_tifiles_create(file, filename, UFT_TIFILES_TYPE_DIS_FIX, rec_length, data, size);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Extraction
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_tifiles_error_t uft_tifiles_extract(const uft_tifiles_file_t *file,
                                         uint8_t *data, size_t size, size_t *extracted) {
    if (!file || !extracted) return UFT_TIFILES_ERR_PARAM;
    
    uft_tifiles_info_t info;
    uft_tifiles_error_t ret = uft_tifiles_get_info((const uint8_t *)&file->header, 
                                                    UFT_TIFILES_HEADER_SIZE + file->data_size, &info);
    if (ret != UFT_TIFILES_OK) return ret;
    
    *extracted = info.data_size;
    if (!data) return UFT_TIFILES_OK;
    if (size < info.data_size) return UFT_TIFILES_ERR_SIZE;
    
    if (file->data && info.data_size > 0) {
        memcpy(data, file->data, info.data_size);
    }
    
    return UFT_TIFILES_OK;
}

uft_tifiles_error_t uft_tifiles_extract_text(const uft_tifiles_file_t *file,
                                              char *text, size_t max_size) {
    if (!file || !text || max_size == 0) return UFT_TIFILES_ERR_PARAM;
    
    uft_tifiles_type_t type = flags_to_type(file->header.flags);
    if (type != UFT_TIFILES_TYPE_DIS_VAR) return UFT_TIFILES_ERR_INVALID;
    
    size_t out_pos = 0;
    const uint8_t *ptr = file->data;
    size_t remaining = file->data_size;
    
    while (remaining >= UFT_TIFILES_SECTOR_SIZE && out_pos < max_size - 1) {
        size_t pos = 0;
        while (pos < UFT_TIFILES_SECTOR_SIZE && out_pos < max_size - 1) {
            uint8_t rec_len = ptr[pos];
            if (rec_len == 0xFF) break;
            pos++;
            if (pos + rec_len > UFT_TIFILES_SECTOR_SIZE) break;
            
            size_t copy = (rec_len < max_size - out_pos - 2) ? rec_len : max_size - out_pos - 2;
            memcpy(text + out_pos, ptr + pos, copy);
            out_pos += copy;
            text[out_pos++] = '\n';
            pos += rec_len;
        }
        ptr += UFT_TIFILES_SECTOR_SIZE;
        remaining -= UFT_TIFILES_SECTOR_SIZE;
    }
    
    text[out_pos] = '\0';
    return UFT_TIFILES_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utilities
 * ═══════════════════════════════════════════════════════════════════════════════ */

size_t uft_tifiles_calc_size(size_t data_size) {
    uint16_t sectors = (uint16_t)((data_size + UFT_TIFILES_SECTOR_SIZE - 1) / UFT_TIFILES_SECTOR_SIZE);
    return UFT_TIFILES_HEADER_SIZE + sectors * UFT_TIFILES_SECTOR_SIZE;
}

const char *uft_tifiles_type_str(uft_tifiles_type_t type) {
    switch (type) {
        case UFT_TIFILES_TYPE_PROGRAM: return "PROGRAM";
        case UFT_TIFILES_TYPE_DIS_FIX: return "DIS/FIX";
        case UFT_TIFILES_TYPE_DIS_VAR: return "DIS/VAR";
        case UFT_TIFILES_TYPE_INT_FIX: return "INT/FIX";
        case UFT_TIFILES_TYPE_INT_VAR: return "INT/VAR";
    }
    return "UNKNOWN";
}

const char *uft_tifiles_strerror(uft_tifiles_error_t err) {
    switch (err) {
        case UFT_TIFILES_OK:            return "Success";
        case UFT_TIFILES_ERR_INVALID:   return "Invalid file";
        case UFT_TIFILES_ERR_SIGNATURE: return "Invalid signature";
        case UFT_TIFILES_ERR_SIZE:      return "Size mismatch";
        case UFT_TIFILES_ERR_READ:      return "Read error";
        case UFT_TIFILES_ERR_WRITE:     return "Write error";
        case UFT_TIFILES_ERR_MEMORY:    return "Memory error";
        case UFT_TIFILES_ERR_PARAM:     return "Invalid parameter";
    }
    return "Unknown error";
}

uft_tifiles_type_t uft_tifiles_parse_type(uint8_t flags) {
    return flags_to_type(flags);
}

uint8_t uft_tifiles_build_flags(uft_tifiles_type_t type, bool protected) {
    return type_to_flags(type, protected);
}
