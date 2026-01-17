/**
 * @file uft_t64.c
 * @brief T64 Tape Image Format Implementation
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#include "uft/formats/c64/uft_t64.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ============================================================================
 * Static Data
 * ============================================================================ */

static const char *type_names[] = {
    "DEL", "SEQ", "PRG", "USR", "REL"
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Convert PETSCII to ASCII
 */
void t64_petscii_to_ascii(const char *petscii, char *ascii, int len)
{
    for (int i = 0; i < len; i++) {
        uint8_t c = (uint8_t)petscii[i];
        
        if (c == 0x00 || c == 0x20) {
            /* Stop on null or space (T64 uses 0x20 padding) */
            ascii[i] = '\0';
            return;
        } else if (c >= 0x41 && c <= 0x5A) {
            ascii[i] = c;  /* A-Z */
        } else if (c >= 0xC1 && c <= 0xDA) {
            ascii[i] = c - 0x80;  /* Shifted letters */
        } else if (c >= 0x20 && c <= 0x7E) {
            ascii[i] = c;
        } else {
            ascii[i] = '_';
        }
    }
    ascii[len] = '\0';
}

/**
 * @brief Convert ASCII to PETSCII
 */
void t64_ascii_to_petscii(const char *ascii, char *petscii, int len)
{
    int i;
    for (i = 0; i < len && ascii[i]; i++) {
        char c = ascii[i];
        if (c >= 'a' && c <= 'z') {
            petscii[i] = c - 32;  /* Uppercase */
        } else if (c >= 'A' && c <= 'Z') {
            petscii[i] = c;
        } else {
            petscii[i] = c;
        }
    }
    /* Pad with spaces (0x20) as per T64 spec */
    for (; i < len; i++) {
        petscii[i] = 0x20;
    }
}

/**
 * @brief Read 16-bit little-endian value
 */
static uint16_t read_u16(const uint8_t *data)
{
    return data[0] | (data[1] << 8);
}

/**
 * @brief Read 32-bit little-endian value
 */
static uint32_t read_u32(const uint8_t *data)
{
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

/**
 * @brief Write 16-bit little-endian value
 */
static void write_u16(uint8_t *data, uint16_t value)
{
    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;
}

/**
 * @brief Write 32-bit little-endian value
 */
static void write_u32(uint8_t *data, uint32_t value)
{
    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;
    data[2] = (value >> 16) & 0xFF;
    data[3] = (value >> 24) & 0xFF;
}

/* ============================================================================
 * Format Detection & Validation
 * ============================================================================ */

/**
 * @brief Detect T64 format
 */
bool t64_detect(const uint8_t *data, size_t size)
{
    if (!data || size < T64_HEADER_SIZE + T64_DIR_ENTRY_SIZE) {
        return false;
    }
    
    /* Check for T64 magic strings */
    if (memcmp(data, "C64", 3) == 0 && 
        (memcmp(data, T64_MAGIC_V1, 19) == 0 || 
         memcmp(data, T64_MAGIC_V2, 20) == 0)) {
        return true;
    }
    
    return false;
}

/**
 * @brief Validate T64 format
 */
bool t64_validate(const uint8_t *data, size_t size)
{
    if (!t64_detect(data, size)) {
        return false;
    }
    
    /* Check header values */
    uint16_t max_entries = read_u16(data + 34);
    uint16_t used_entries = read_u16(data + 36);
    
    if (max_entries == 0 || max_entries > T64_MAX_ENTRIES) {
        return false;
    }
    
    if (used_entries > max_entries) {
        return false;
    }
    
    /* Check minimum size */
    size_t min_size = T64_HEADER_SIZE + max_entries * T64_DIR_ENTRY_SIZE;
    if (size < min_size) {
        return false;
    }
    
    return true;
}

/* ============================================================================
 * Image Management
 * ============================================================================ */

/**
 * @brief Open T64 from data
 */
int t64_open(const uint8_t *data, size_t size, t64_image_t *image)
{
    if (!data || !image || size < T64_HEADER_SIZE) {
        return -1;
    }
    
    if (!t64_validate(data, size)) {
        return -2;
    }
    
    memset(image, 0, sizeof(t64_image_t));
    
    /* Copy data */
    image->data = malloc(size);
    if (!image->data) return -3;
    
    memcpy(image->data, data, size);
    image->size = size;
    image->owns_data = true;
    
    /* Parse header */
    memcpy(image->header.magic, data, 32);
    image->header.version = read_u16(data + 32);
    image->header.max_entries = read_u16(data + 34);
    image->header.used_entries = read_u16(data + 36);
    image->header.reserved = read_u16(data + 38);
    memcpy(image->header.tape_name, data + 40, 24);
    
    /* Parse directory entries */
    image->entries = calloc(image->header.max_entries, sizeof(t64_dir_entry_t));
    if (!image->entries) {
        free(image->data);
        return -4;
    }
    
    const uint8_t *dir = data + T64_HEADER_SIZE;
    image->num_entries = 0;
    
    for (int i = 0; i < image->header.max_entries; i++) {
        const uint8_t *entry = dir + i * T64_DIR_ENTRY_SIZE;
        
        image->entries[i].entry_type = entry[0];
        image->entries[i].c1541_type = entry[1];
        image->entries[i].start_addr = read_u16(entry + 2);
        image->entries[i].end_addr = read_u16(entry + 4);
        image->entries[i].reserved1 = read_u16(entry + 6);
        image->entries[i].offset = read_u32(entry + 8);
        image->entries[i].reserved2 = read_u32(entry + 12);
        memcpy(image->entries[i].filename, entry + 16, 16);
        
        if (image->entries[i].entry_type != T64_TYPE_FREE) {
            image->num_entries++;
        }
    }
    
    return 0;
}

/**
 * @brief Load T64 from file
 */
int t64_load(const char *filename, t64_image_t *image)
{
    if (!filename || !image) return -1;
    
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -2;
    
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (size < T64_HEADER_SIZE) {
        fclose(fp);
        return -3;
    }
    
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(fp);
        return -4;
    }
    
    if (fread(data, 1, size, fp) != size) {
        free(data);
        fclose(fp);
        return -5;
    }
    fclose(fp);
    
    int result = t64_open(data, size, image);
    free(data);
    
    return result;
}

/**
 * @brief Save T64 to file
 */
int t64_save(const t64_image_t *image, const char *filename)
{
    if (!image || !filename || !image->data) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -2;
    
    size_t written = fwrite(image->data, 1, image->size, fp);
    fclose(fp);
    
    return (written == image->size) ? 0 : -3;
}

/**
 * @brief Close T64 image
 */
void t64_close(t64_image_t *image)
{
    if (!image) return;
    
    if (image->owns_data) {
        free(image->data);
    }
    free(image->entries);
    
    memset(image, 0, sizeof(t64_image_t));
}

/* ============================================================================
 * Directory Operations
 * ============================================================================ */

/**
 * @brief Get file count
 */
int t64_get_file_count(const t64_image_t *image)
{
    if (!image) return 0;
    return image->num_entries;
}

/**
 * @brief Get file info by index
 */
int t64_get_file_info(const t64_image_t *image, int index, t64_file_info_t *info)
{
    if (!image || !info || index < 0) return -1;
    
    int file_index = 0;
    
    for (int i = 0; i < image->header.max_entries; i++) {
        if (image->entries[i].entry_type != T64_TYPE_FREE) {
            if (file_index == index) {
                t64_petscii_to_ascii(image->entries[i].filename, info->filename, 16);
                info->entry_type = image->entries[i].entry_type;
                info->c1541_type = image->entries[i].c1541_type;
                info->start_addr = image->entries[i].start_addr;
                info->end_addr = image->entries[i].end_addr;
                info->data_offset = image->entries[i].offset;
                info->data_size = info->end_addr - info->start_addr;
                info->entry_index = i;
                return 0;
            }
            file_index++;
        }
    }
    
    return -2;  /* Index out of range */
}

/**
 * @brief Find file by name
 */
int t64_find_file(const t64_image_t *image, const char *filename,
                  t64_file_info_t *info)
{
    if (!image || !filename) return -1;
    
    char petscii_name[17];
    t64_ascii_to_petscii(filename, petscii_name, 16);
    
    for (int i = 0; i < image->header.max_entries; i++) {
        if (image->entries[i].entry_type != T64_TYPE_FREE) {
            if (memcmp(image->entries[i].filename, petscii_name, 16) == 0) {
                if (info) {
                    t64_petscii_to_ascii(image->entries[i].filename, info->filename, 16);
                    info->entry_type = image->entries[i].entry_type;
                    info->c1541_type = image->entries[i].c1541_type;
                    info->start_addr = image->entries[i].start_addr;
                    info->end_addr = image->entries[i].end_addr;
                    info->data_offset = image->entries[i].offset;
                    info->data_size = info->end_addr - info->start_addr;
                    info->entry_index = i;
                }
                return 0;
            }
        }
    }
    
    return -1;  /* Not found */
}

/**
 * @brief Get tape name
 */
void t64_get_tape_name(const t64_image_t *image, char *name)
{
    if (!image || !name) return;
    t64_petscii_to_ascii(image->header.tape_name, name, 24);
}

/* ============================================================================
 * File Extraction
 * ============================================================================ */

/**
 * @brief Extract file by name
 */
int t64_extract_file(const t64_image_t *image, const char *filename,
                     t64_file_t *file)
{
    if (!image || !filename || !file) return -1;
    
    t64_file_info_t info;
    if (t64_find_file(image, filename, &info) != 0) {
        return -2;  /* Not found */
    }
    
    memset(file, 0, sizeof(t64_file_t));
    file->info = info;
    
    /* Validate offset and size */
    if (info.data_offset + info.data_size > image->size) {
        return -3;
    }
    
    /* Allocate and copy data */
    file->data = malloc(info.data_size);
    if (!file->data) return -4;
    
    memcpy(file->data, image->data + info.data_offset, info.data_size);
    file->data_size = info.data_size;
    
    return 0;
}

/**
 * @brief Extract file by index
 */
int t64_extract_by_index(const t64_image_t *image, int index, t64_file_t *file)
{
    if (!image || !file || index < 0) return -1;
    
    t64_file_info_t info;
    if (t64_get_file_info(image, index, &info) != 0) {
        return -2;
    }
    
    memset(file, 0, sizeof(t64_file_t));
    file->info = info;
    
    if (info.data_offset + info.data_size > image->size) {
        return -3;
    }
    
    file->data = malloc(info.data_size);
    if (!file->data) return -4;
    
    memcpy(file->data, image->data + info.data_offset, info.data_size);
    file->data_size = info.data_size;
    
    return 0;
}

/**
 * @brief Extract all files
 */
int t64_extract_all(const t64_image_t *image, t64_file_t *files, int max_files)
{
    if (!image || !files || max_files <= 0) return 0;
    
    int count = 0;
    
    for (int i = 0; i < max_files && i < image->num_entries; i++) {
        if (t64_extract_by_index(image, i, &files[count]) == 0) {
            count++;
        }
    }
    
    return count;
}

/**
 * @brief Save extracted file
 */
int t64_save_file(const t64_file_t *file, const char *path, bool include_header)
{
    if (!file || !file->data) return -1;
    
    char filepath[512];
    
    if (path) {
        strncpy(filepath, path, sizeof(filepath) - 1);
    } else {
        snprintf(filepath, sizeof(filepath), "%s.prg", file->info.filename);
    }
    
    FILE *fp = fopen(filepath, "wb");
    if (!fp) return -2;
    
    /* Optionally write load address header */
    if (include_header) {
        uint8_t header[2];
        header[0] = file->info.start_addr & 0xFF;
        header[1] = (file->info.start_addr >> 8) & 0xFF;
        fwrite(header, 1, 2, fp);
    }
    
    size_t written = fwrite(file->data, 1, file->data_size, fp);
    fclose(fp);
    
    return (written == file->data_size) ? 0 : -3;
}

/**
 * @brief Free extracted file
 */
void t64_free_file(t64_file_t *file)
{
    if (file) {
        free(file->data);
        file->data = NULL;
        file->data_size = 0;
    }
}

/* ============================================================================
 * T64 Creation
 * ============================================================================ */

/**
 * @brief Create new T64
 */
int t64_create(const char *tape_name, int max_entries, t64_image_t *image)
{
    if (!image || max_entries <= 0 || max_entries > T64_MAX_ENTRIES) {
        return -1;
    }
    
    memset(image, 0, sizeof(t64_image_t));
    
    /* Calculate size */
    size_t header_size = T64_HEADER_SIZE + max_entries * T64_DIR_ENTRY_SIZE;
    
    image->data = calloc(1, header_size);
    if (!image->data) return -2;
    
    image->size = header_size;
    image->owns_data = true;
    
    /* Write magic */
    memcpy(image->data, T64_MAGIC_V1, strlen(T64_MAGIC_V1));
    
    /* Write header */
    write_u16(image->data + 32, 0x0100);  /* Version */
    write_u16(image->data + 34, max_entries);
    write_u16(image->data + 36, 0);  /* Used entries */
    write_u16(image->data + 38, 0);  /* Reserved */
    
    /* Set tape name */
    if (tape_name) {
        t64_ascii_to_petscii(tape_name, (char *)(image->data + 40), 24);
    } else {
        memset(image->data + 40, 0x20, 24);
    }
    
    /* Copy header to struct */
    memcpy(image->header.magic, image->data, 32);
    image->header.version = 0x0100;
    image->header.max_entries = max_entries;
    image->header.used_entries = 0;
    memcpy(image->header.tape_name, image->data + 40, 24);
    
    /* Allocate directory entries */
    image->entries = calloc(max_entries, sizeof(t64_dir_entry_t));
    if (!image->entries) {
        free(image->data);
        return -3;
    }
    
    return 0;
}

/**
 * @brief Add file to T64
 */
int t64_add_file(t64_image_t *image, const char *filename,
                 const uint8_t *data, size_t size,
                 uint16_t start_addr, uint8_t file_type)
{
    if (!image || !filename || !data || size == 0) return -1;
    
    /* Find free entry */
    int entry_idx = -1;
    for (int i = 0; i < image->header.max_entries; i++) {
        if (image->entries[i].entry_type == T64_TYPE_FREE) {
            entry_idx = i;
            break;
        }
    }
    
    if (entry_idx < 0) return -2;  /* No free entries */
    
    /* Calculate new file offset */
    uint32_t new_offset = image->size;
    
    /* Expand data buffer */
    uint8_t *new_data = realloc(image->data, image->size + size);
    if (!new_data) return -3;
    
    image->data = new_data;
    memcpy(image->data + new_offset, data, size);
    image->size += size;
    
    /* Update directory entry */
    image->entries[entry_idx].entry_type = T64_TYPE_TAPE;
    image->entries[entry_idx].c1541_type = file_type;
    image->entries[entry_idx].start_addr = start_addr;
    image->entries[entry_idx].end_addr = start_addr + size;
    image->entries[entry_idx].offset = new_offset;
    t64_ascii_to_petscii(filename, image->entries[entry_idx].filename, 16);
    
    /* Write entry to data */
    uint8_t *entry = image->data + T64_HEADER_SIZE + entry_idx * T64_DIR_ENTRY_SIZE;
    entry[0] = T64_TYPE_TAPE;
    entry[1] = file_type;
    write_u16(entry + 2, start_addr);
    write_u16(entry + 4, start_addr + size);
    write_u16(entry + 6, 0);
    write_u32(entry + 8, new_offset);
    write_u32(entry + 12, 0);
    t64_ascii_to_petscii(filename, (char *)(entry + 16), 16);
    
    /* Update header */
    image->header.used_entries++;
    image->num_entries++;
    write_u16(image->data + 36, image->header.used_entries);
    
    return 0;
}

/**
 * @brief Add PRG file from disk
 */
int t64_add_prg_file(t64_image_t *image, const char *path, const char *c64_name)
{
    if (!image || !path) return -1;
    
    FILE *fp = fopen(path, "rb");
    if (!fp) return -2;
    
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (size < 2) {
        fclose(fp);
        return -3;
    }
    
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(fp);
        return -4;
    }
    
    if (fread(data, 1, size, fp) != size) {
        free(data);
        fclose(fp);
        return -5;
    }
    fclose(fp);
    
    /* Extract load address from first 2 bytes */
    uint16_t load_addr = data[0] | (data[1] << 8);
    
    /* Generate filename if not provided */
    char filename[17];
    if (c64_name) {
        strncpy(filename, c64_name, 16);
        filename[16] = '\0';
    } else {
        /* Extract from path */
        const char *name = strrchr(path, '/');
        if (!name) name = strrchr(path, '\\');
        name = name ? name + 1 : path;
        
        int i;
        for (i = 0; i < 16 && name[i] && name[i] != '.'; i++) {
            filename[i] = toupper((unsigned char)name[i]);
        }
        for (; i < 16; i++) filename[i] = ' ';
        filename[16] = '\0';
    }
    
    /* Add file (skip load address bytes since they're in header) */
    int result = t64_add_file(image, filename, data + 2, size - 2, 
                              load_addr, T64_C1541_PRG);
    
    free(data);
    return result;
}

/**
 * @brief Remove file from T64
 */
int t64_remove_file(t64_image_t *image, const char *filename)
{
    if (!image || !filename) return -1;
    
    t64_file_info_t info;
    if (t64_find_file(image, filename, &info) != 0) {
        return -2;  /* Not found */
    }
    
    /* Mark entry as free */
    image->entries[info.entry_index].entry_type = T64_TYPE_FREE;
    
    /* Update in data */
    uint8_t *entry = image->data + T64_HEADER_SIZE + 
                     info.entry_index * T64_DIR_ENTRY_SIZE;
    entry[0] = T64_TYPE_FREE;
    
    /* Update header */
    image->header.used_entries--;
    image->num_entries--;
    write_u16(image->data + 36, image->header.used_entries);
    
    return 0;
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

/**
 * @brief Get file type name
 */
const char *t64_type_name(uint8_t c1541_type)
{
    if (c1541_type < 5) {
        return type_names[c1541_type];
    }
    return "???";
}

/**
 * @brief Print directory
 */
void t64_print_directory(const t64_image_t *image, FILE *fp)
{
    if (!image || !fp) return;
    
    char tape_name[25];
    t64_get_tape_name(image, tape_name);
    
    fprintf(fp, "T64 Tape Image: \"%s\"\n", tape_name);
    fprintf(fp, "Version: %04X, Entries: %d/%d\n\n",
            image->header.version,
            image->header.used_entries,
            image->header.max_entries);
    
    fprintf(fp, "  # Type  Start   End     Size  Filename\n");
    fprintf(fp, "--- ----  ------  ------  ----  ----------------\n");
    
    int file_num = 0;
    for (int i = 0; i < image->header.max_entries; i++) {
        if (image->entries[i].entry_type != T64_TYPE_FREE) {
            char filename[17];
            t64_petscii_to_ascii(image->entries[i].filename, filename, 16);
            
            fprintf(fp, "%3d %s   $%04X   $%04X   %4d  \"%s\"\n",
                    file_num++,
                    t64_type_name(image->entries[i].c1541_type),
                    image->entries[i].start_addr,
                    image->entries[i].end_addr,
                    image->entries[i].end_addr - image->entries[i].start_addr,
                    filename);
        }
    }
}
