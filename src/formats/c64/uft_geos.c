/**
 * @file uft_geos.c
 * @brief GEOS Filesystem Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/c64/uft_geos.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Type Names
 * ============================================================================ */

static const char *geos_type_names[] = {
    "Non-GEOS",         /* 0x00 */
    "BASIC",            /* 0x01 */
    "Assembler",        /* 0x02 */
    "Data",             /* 0x03 */
    "System",           /* 0x04 */
    "Desk Accessory",   /* 0x05 */
    "Application",      /* 0x06 */
    "Printer Driver",   /* 0x07 */
    "Input Driver",     /* 0x08 */
    "Disk Driver",      /* 0x09 */
    "Boot Loader",      /* 0x0A */
    "Temporary",        /* 0x0B */
    "Auto-Exec",        /* 0x0C */
    "Input Driver 128", /* 0x0D */
    "Numerator",        /* 0x0E */
    "Font"              /* 0x0F */
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint16_t read_le16(const uint8_t *data)
{
    return data[0] | (data[1] << 8);
}

static void write_le16(uint8_t *data, uint16_t value)
{
    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;
}

static void copy_petscii(char *dest, const uint8_t *src, int len)
{
    for (int i = 0; i < len; i++) {
        uint8_t c = src[i];
        if (c == 0xA0 || c == 0x00) {
            dest[i] = '\0';
            return;
        } else if (c >= 0xC1 && c <= 0xDA) {
            dest[i] = c - 0x80;  /* Uppercase */
        } else if (c >= 0x41 && c <= 0x5A) {
            dest[i] = c;
        } else if (c >= 0x20 && c < 0x7F) {
            dest[i] = c;
        } else {
            dest[i] = '_';
        }
    }
    dest[len] = '\0';
}

/* ============================================================================
 * GEOS Detection
 * ============================================================================ */

bool geos_is_geos_file(const uint8_t *dir_entry)
{
    if (!dir_entry) return false;
    
    /* Check for GEOS file type marker in byte 24 */
    /* In extended directory entry, byte 24 contains GEOS type */
    uint8_t file_type = dir_entry[2] & 0x0F;
    
    /* USR files with specific markers are GEOS */
    if (file_type == 0x03) {  /* USR */
        /* Check for GEOS marker */
        return (dir_entry[24] != 0x00);
    }
    
    return false;
}

const char *geos_type_name(uint8_t type)
{
    if (type <= 0x0F) {
        return geos_type_names[type];
    }
    return "Unknown";
}

const char *geos_structure_name(uint8_t structure)
{
    switch (structure) {
        case GEOS_STRUCT_SEQ:  return "Sequential";
        case GEOS_STRUCT_VLIR: return "VLIR";
        default: return "Unknown";
    }
}

/* ============================================================================
 * Info Block
 * ============================================================================ */

int geos_parse_info(const uint8_t *data, geos_info_t *info)
{
    if (!data || !info) return -1;
    
    memset(info, 0, sizeof(geos_info_t));
    
    /* Info block ID at offset 0-2 */
    memcpy(info->info_id, data, 3);
    
    /* Icon at offset 3-68 */
    info->icon.width = data[3];
    info->icon.height = data[4];
    if (info->icon.width == 3 && info->icon.height == 21) {
        memcpy(info->icon.data, data + 5, 63);
    }
    
    /* File type info at offset 68-73 */
    info->dos_type = data[68];
    info->geos_type = data[69];
    info->structure = data[70];
    info->load_address = read_le16(data + 71);
    info->end_address = read_le16(data + 73);
    info->exec_address = read_le16(data + 75);
    
    /* Class name at offset 77 (20 bytes) */
    copy_petscii(info->class_name, data + 77, 20);
    
    /* Author at offset 97 (20 bytes) */
    copy_petscii(info->author, data + 97, 20);
    
    /* Parent at offset 117 (20 bytes) */
    copy_petscii(info->parent_name, data + 117, 20);
    
    /* Application at offset 137 (20 bytes) */
    copy_petscii(info->application, data + 137, 20);
    
    /* Version at offset 157 (4 bytes) */
    memcpy(info->version, data + 157, 4);
    
    /* Created timestamp at offset 161 */
    info->created.year = data[161];
    info->created.month = data[162];
    info->created.day = data[163];
    info->created.hour = data[164];
    info->created.minute = data[165];
    
    /* Modified timestamp at offset 166 - if present */
    if (data[166] != 0) {
        info->modified.year = data[166];
        info->modified.month = data[167];
        info->modified.day = data[168];
        info->modified.hour = data[169];
        info->modified.minute = data[170];
    }
    
    /* Description at offset 160 (up to 96 bytes) */
    /* Actually stored starting at a different offset depending on format */
    
    return 0;
}

int geos_write_info(const geos_info_t *info, uint8_t *data)
{
    if (!info || !data) return -1;
    
    memset(data, 0, GEOS_INFO_BLOCK_SIZE);
    
    /* Info block ID */
    data[0] = 0x00;
    data[1] = 0xFF;
    data[2] = 0x00;
    
    /* Icon */
    data[3] = 3;   /* Width in bytes */
    data[4] = 21;  /* Height in pixels */
    memcpy(data + 5, info->icon.data, 63);
    
    /* File type info */
    data[68] = info->dos_type;
    data[69] = info->geos_type;
    data[70] = info->structure;
    write_le16(data + 71, info->load_address);
    write_le16(data + 73, info->end_address);
    write_le16(data + 75, info->exec_address);
    
    /* Class name */
    if (info->class_name[0]) {
        strncpy((char*)data + 77, info->class_name, 20);
    }
    
    /* Author */
    if (info->author[0]) {
        strncpy((char*)data + 97, info->author, 20);
    }
    
    /* Timestamps */
    data[161] = info->created.year;
    data[162] = info->created.month;
    data[163] = info->created.day;
    data[164] = info->created.hour;
    data[165] = info->created.minute;
    
    return 0;
}

void geos_format_timestamp(const geos_timestamp_t *ts, char *buffer)
{
    if (!ts || !buffer) return;
    
    int year = 1900 + ts->year;
    if (ts->year < 80) year += 100;  /* Y2K handling */
    
    snprintf(buffer, 20, "%04d-%02d-%02d %02d:%02d",
             year, ts->month, ts->day, ts->hour, ts->minute);
}

/* ============================================================================
 * VLIR Handling
 * ============================================================================ */

int geos_parse_vlir_index(const uint8_t *data, geos_vlir_record_t *records,
                          int *num_records)
{
    if (!data || !records || !num_records) return -1;
    
    int count = 0;
    
    for (int i = 0; i < GEOS_MAX_VLIR_RECORDS; i++) {
        records[i].track = data[i * 2];
        records[i].sector = data[i * 2 + 1];
        records[i].size = 0;
        records[i].data = NULL;
        
        if (records[i].track == 0x00 && records[i].sector == 0x00) {
            /* Empty record */
        } else if (records[i].track == 0x00 && records[i].sector != 0x00) {
            /* Last sector, sector byte is size */
            records[i].size = records[i].sector;
        } else if (records[i].track == 0xFF) {
            /* Deleted record */
        } else {
            /* Valid record */
            count++;
        }
    }
    
    *num_records = count;
    return 0;
}

int geos_write_vlir_index(const geos_vlir_record_t *records, int num_records,
                          uint8_t *data)
{
    if (!records || !data) return -1;
    
    memset(data, 0, 254);
    
    for (int i = 0; i < num_records && i < GEOS_MAX_VLIR_RECORDS; i++) {
        data[i * 2] = records[i].track;
        data[i * 2 + 1] = records[i].sector;
    }
    
    return 0;
}

bool geos_vlir_record_empty(const geos_vlir_record_t *record)
{
    if (!record) return true;
    return (record->track == 0x00 && record->sector == 0x00);
}

bool geos_vlir_record_deleted(const geos_vlir_record_t *record)
{
    if (!record) return false;
    return (record->track == 0xFF);
}

/* ============================================================================
 * GEOS File Operations
 * ============================================================================ */

int geos_file_create(const char *filename, uint8_t type, bool is_vlir,
                     geos_file_t *file)
{
    if (!file) return -1;
    
    memset(file, 0, sizeof(geos_file_t));
    
    if (filename) {
        strncpy(file->filename, filename, 16);
    }
    
    file->info.geos_type = type;
    file->info.structure = is_vlir ? GEOS_STRUCT_VLIR : GEOS_STRUCT_SEQ;
    file->info.dos_type = 0x83;  /* USR file */
    file->is_vlir = is_vlir;
    
    /* Set default icon */
    file->info.icon.width = 3;
    file->info.icon.height = 21;
    geos_get_default_icon(type, &file->info.icon);
    
    return 0;
}

void geos_file_free(geos_file_t *file)
{
    if (!file) return;
    
    if (file->records) {
        for (int i = 0; i < file->num_records; i++) {
            free(file->records[i].data);
        }
        free(file->records);
    }
    
    free(file->seq_data);
    
    memset(file, 0, sizeof(geos_file_t));
}

void geos_file_set_icon(geos_file_t *file, const uint8_t *icon_data)
{
    if (!file || !icon_data) return;
    
    file->info.icon.width = 3;
    file->info.icon.height = 21;
    memcpy(file->info.icon.data, icon_data, 63);
}

void geos_file_set_description(geos_file_t *file, const char *class_name,
                               const char *author, const char *description)
{
    if (!file) return;
    
    if (class_name) {
        strncpy(file->info.class_name, class_name, 19);
    }
    if (author) {
        strncpy(file->info.author, author, 19);
    }
    if (description) {
        strncpy(file->info.description, description, 95);
    }
}

/* ============================================================================
 * CVT Format
 * ============================================================================ */

bool geos_cvt_detect(const uint8_t *data, size_t size)
{
    if (!data || size < CVT_MAGIC_LEN + 256) {
        return false;
    }
    
    return (memcmp(data, CVT_MAGIC, CVT_MAGIC_LEN) == 0);
}

int geos_cvt_parse(const uint8_t *data, size_t size, geos_file_t *file)
{
    if (!data || !file || size < CVT_MAGIC_LEN + 256) {
        return -1;
    }
    
    if (!geos_cvt_detect(data, size)) {
        return -2;
    }
    
    memset(file, 0, sizeof(geos_file_t));
    
    /* CVT header is 30 bytes, then directory entry, then info block */
    const uint8_t *dir_entry = data + 30;
    const uint8_t *info_block = data + 30 + 30;
    
    /* Parse filename from directory entry */
    copy_petscii(file->filename, dir_entry + 5, 16);
    
    /* Parse info block */
    geos_parse_info(info_block, &file->info);
    
    file->is_vlir = (file->info.structure == GEOS_STRUCT_VLIR);
    
    /* Data follows info block */
    size_t data_start = 30 + 30 + 256;
    if (data_start < size) {
        file->seq_size = size - data_start;
        file->seq_data = malloc(file->seq_size);
        if (file->seq_data) {
            memcpy(file->seq_data, data + data_start, file->seq_size);
        }
    }
    
    return 0;
}

int geos_cvt_create(const geos_file_t *file, uint8_t *cvt_data,
                    size_t max_size, size_t *cvt_size)
{
    if (!file || !cvt_data) return -1;
    
    /* Calculate required size */
    size_t required = CVT_MAGIC_LEN + 2 + 30 + 256;  /* Header + dir + info */
    if (file->seq_data) {
        required += file->seq_size;
    }
    
    if (required > max_size) return -2;
    
    memset(cvt_data, 0, required);
    
    /* CVT magic */
    memcpy(cvt_data, CVT_MAGIC, CVT_MAGIC_LEN);
    
    /* Signature bytes */
    cvt_data[CVT_MAGIC_LEN] = 0x00;
    cvt_data[CVT_MAGIC_LEN + 1] = 0x00;
    
    /* Directory entry at offset 30 */
    uint8_t *dir = cvt_data + 30;
    dir[0] = 0x00;  /* Track */
    dir[1] = 0x00;  /* Sector */
    dir[2] = 0x83;  /* USR file type */
    dir[3] = 0x01;  /* Info track */
    dir[4] = 0x01;  /* Info sector */
    
    /* Copy filename */
    for (int i = 0; i < 16 && file->filename[i]; i++) {
        dir[5 + i] = file->filename[i];
    }
    
    /* GEOS type info in extended directory */
    dir[24] = file->info.geos_type;
    dir[25] = file->info.structure;
    
    /* Info block at offset 60 */
    geos_write_info(&file->info, cvt_data + 60);
    
    /* Data at offset 316 */
    if (file->seq_data) {
        memcpy(cvt_data + 316, file->seq_data, file->seq_size);
    }
    
    if (cvt_size) *cvt_size = required;
    return 0;
}

int geos_cvt_load(const char *filename, geos_file_t *file)
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
    
    int result = geos_cvt_parse(data, size, file);
    free(data);
    
    return result;
}

int geos_cvt_save(const geos_file_t *file, const char *filename)
{
    if (!file || !filename) return -1;
    
    /* Estimate size */
    size_t max_size = 316 + (file->seq_data ? file->seq_size : 0) + 1024;
    uint8_t *cvt_data = malloc(max_size);
    if (!cvt_data) return -2;
    
    size_t cvt_size;
    int ret = geos_cvt_create(file, cvt_data, max_size, &cvt_size);
    if (ret != 0) {
        free(cvt_data);
        return ret;
    }
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        free(cvt_data);
        return -3;
    }
    
    fwrite(cvt_data, 1, cvt_size, fp);
    fclose(fp);
    free(cvt_data);
    
    return 0;
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

void geos_print_info(const geos_file_t *file, FILE *fp)
{
    if (!file || !fp) return;
    
    fprintf(fp, "GEOS File: %s\n", file->filename);
    fprintf(fp, "  Type: %s\n", geos_type_name(file->info.geos_type));
    fprintf(fp, "  Structure: %s\n", geos_structure_name(file->info.structure));
    fprintf(fp, "  Class: %s\n", file->info.class_name);
    fprintf(fp, "  Author: %s\n", file->info.author);
    fprintf(fp, "  Load: $%04X, End: $%04X, Exec: $%04X\n",
            file->info.load_address, file->info.end_address,
            file->info.exec_address);
    
    char ts[32];
    geos_format_timestamp(&file->info.created, ts);
    fprintf(fp, "  Created: %s\n", ts);
    
    if (file->is_vlir) {
        fprintf(fp, "  VLIR Records: %d\n", file->num_records);
    } else {
        fprintf(fp, "  Data Size: %zu bytes\n", file->seq_size);
    }
}

void geos_print_icon(const geos_icon_t *icon, FILE *fp)
{
    if (!icon || !fp || icon->width != 3 || icon->height != 21) return;
    
    for (int y = 0; y < 21; y++) {
        for (int x = 0; x < 24; x++) {
            int byte_idx = y * 3 + (x / 8);
            int bit_idx = 7 - (x % 8);
            
            if (icon->data[byte_idx] & (1 << bit_idx)) {
                fprintf(fp, "##");
            } else {
                fprintf(fp, "  ");
            }
        }
        fprintf(fp, "\n");
    }
}

void geos_get_default_icon(uint8_t type, geos_icon_t *icon)
{
    if (!icon) return;
    
    icon->width = 3;
    icon->height = 21;
    memset(icon->data, 0, 63);
    
    /* Simple default: filled rectangle with border */
    for (int y = 0; y < 21; y++) {
        icon->data[y * 3 + 0] = (y == 0 || y == 20) ? 0xFF : 0x80;
        icon->data[y * 3 + 1] = (y == 0 || y == 20) ? 0xFF : 0x00;
        icon->data[y * 3 + 2] = (y == 0 || y == 20) ? 0xFF : 0x01;
    }
}
