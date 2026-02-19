/**
 * @file uft_m2i.c
 * @brief M2I Tape Image Format Implementation
 * @version 4.1.1
 * 
 * M2I (Mastering 2 Image) is a tape image format that stores
 * the directory structure and file contents of a Commodore tape.
 * Unlike TAP format which stores raw tape signals, M2I stores
 * the logical file data.
 * 
 * File Structure:
 * - Header: "M2I\x00" signature + version
 * - Directory entries with file metadata
 * - File data blocks
 * 
 * Reference: VICE emulator, 64Copy
 */

#include "uft/formats/uft_m2i.h"
#include "uft/core/uft_unified_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * M2I Format Constants
 * ============================================================================ */

#define M2I_SIGNATURE       "M2I\x00"
#define M2I_SIGNATURE_LEN   4
#define M2I_VERSION         1

#define M2I_MAX_ENTRIES     256
#define M2I_FILENAME_LEN    16
#define M2I_ENTRY_SIZE      32

/* File types (same as CBM DOS) */
#define M2I_TYPE_DEL        0x00
#define M2I_TYPE_SEQ        0x01
#define M2I_TYPE_PRG        0x02
#define M2I_TYPE_USR        0x03
#define M2I_TYPE_REL        0x04

/* Entry flags */
#define M2I_FLAG_LOCKED     0x40
#define M2I_FLAG_CLOSED     0x80

/* ============================================================================
 * Data Structures
 * ============================================================================ */

typedef struct {
    char signature[4];      /* "M2I\0" */
    uint8_t version;        /* Format version */
    uint8_t entry_count;    /* Number of directory entries */
    uint16_t reserved;      /* Reserved for future use */
} m2i_header_t;

typedef struct {
    char filename[M2I_FILENAME_LEN];  /* PETSCII filename, padded with 0xA0 */
    uint8_t file_type;                /* File type + flags */
    uint8_t reserved1;
    uint16_t start_address;           /* Load address for PRG files */
    uint32_t file_size;               /* Size in bytes */
    uint32_t data_offset;             /* Offset to file data in image */
    uint8_t reserved2[6];
} m2i_entry_t;

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static uint16_t read_le16(const uint8_t *p) {
    return p[0] | (p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void write_le16(uint8_t *p, uint16_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

static void write_le32(uint8_t *p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

static const char* m2i_type_name(uint8_t type) {
    switch (type & 0x07) {
        case M2I_TYPE_DEL: return "DEL";
        case M2I_TYPE_SEQ: return "SEQ";
        case M2I_TYPE_PRG: return "PRG";
        case M2I_TYPE_USR: return "USR";
        case M2I_TYPE_REL: return "REL";
        default: return "???";
    }
}

/* Convert PETSCII filename to ASCII */
static void petscii_to_ascii(const char *petscii, char *ascii, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uint8_t c = (uint8_t)petscii[i];
        if (c == 0xA0 || c == 0x00) {
            ascii[i] = '\0';
            break;
        } else if (c >= 0x41 && c <= 0x5A) {
            ascii[i] = c;  /* A-Z unchanged */
        } else if (c >= 0xC1 && c <= 0xDA) {
            ascii[i] = c - 0x80;  /* Shifted letters */
        } else if (c >= 0x20 && c <= 0x7E) {
            ascii[i] = c;  /* Printable ASCII */
        } else {
            ascii[i] = '?';
        }
    }
    ascii[len - 1] = '\0';
}

/* ============================================================================
 * Probe Function
 * ============================================================================ */

bool uft_m2i_probe(const uint8_t *data, size_t size, int *confidence) {
    if (!data || size < sizeof(m2i_header_t)) {
        return false;
    }
    
    /* Check signature */
    if (memcmp(data, M2I_SIGNATURE, M2I_SIGNATURE_LEN) == 0) {
        if (confidence) *confidence = 95;
        return true;
    }
    
    return false;
}

/* ============================================================================
 * Read Functions
 * ============================================================================ */

int uft_m2i_read(const char *path, m2i_image_t **out) {
    if (!path || !out) return UFT_ERR_INVALID_PARAM;
    
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERR_FILE_OPEN;
    
    /* Read header */
    m2i_header_t header;
    if (fread(&header, 1, sizeof(header), f) != sizeof(header)) {
        fclose(f);
        return UFT_ERR_FILE_READ;
    }
    
    /* Verify signature */
    if (memcmp(header.signature, M2I_SIGNATURE, M2I_SIGNATURE_LEN) != 0) {
        fclose(f);
        return UFT_ERC_FORMAT;
    }
    
    /* Allocate image */
    m2i_image_t *img = calloc(1, sizeof(m2i_image_t));
    if (!img) {
        fclose(f);
        return UFT_ERR_MEMORY;
    }
    
    img->version = header.version;
    img->entry_count = header.entry_count;
    
    if (img->entry_count > 0) {
        img->entries = calloc(img->entry_count, sizeof(m2i_file_entry_t));
        if (!img->entries) {
            free(img);
            fclose(f);
            return UFT_ERR_MEMORY;
        }
        
        /* Read directory entries */
        for (int i = 0; i < img->entry_count; i++) {
            m2i_entry_t entry;
            if (fread(&entry, 1, sizeof(entry), f) != sizeof(entry)) {
                break;
            }
            
            petscii_to_ascii(entry.filename, img->entries[i].filename, 
                           M2I_FILENAME_LEN + 1);
            img->entries[i].file_type = entry.file_type & 0x07;
            img->entries[i].locked = (entry.file_type & M2I_FLAG_LOCKED) != 0;
            img->entries[i].start_address = read_le16((uint8_t*)&entry.start_address);
            img->entries[i].file_size = read_le32((uint8_t*)&entry.file_size);
            img->entries[i].data_offset = read_le32((uint8_t*)&entry.data_offset);
        }
    }
    
    fclose(f);
    *out = img;
    return UFT_OK;
}

int uft_m2i_extract_file(const char *m2i_path, int index, 
                          const char *output_path) {
    if (!m2i_path || !output_path) return UFT_ERR_INVALID_PARAM;
    
    m2i_image_t *img = NULL;
    int err = uft_m2i_read(m2i_path, &img);
    if (err != UFT_OK) return err;
    
    if (index < 0 || index >= img->entry_count) {
        uft_m2i_free(img);
        return UFT_ERR_INVALID_PARAM;
    }
    
    m2i_file_entry_t *entry = &img->entries[index];
    
    /* Open M2I file for reading data */
    FILE *fin = fopen(m2i_path, "rb");
    if (!fin) {
        uft_m2i_free(img);
        return UFT_ERR_FILE_OPEN;
    }
    
    /* Seek to file data */
    fseek(fin, entry->data_offset, SEEK_SET);
    
    /* Create output file */
    FILE *fout = fopen(output_path, "wb");
    if (!fout) {
        fclose(fin);
        uft_m2i_free(img);
        return UFT_ERR_IO;
    }
    
    /* For PRG files, write load address first */
    if (entry->file_type == M2I_TYPE_PRG) {
        uint8_t addr[2];
        write_le16(addr, entry->start_address);
        fwrite(addr, 1, 2, fout);
    }
    
    /* Copy file data */
    uint8_t buffer[4096];
    size_t remaining = entry->file_size;
    while (remaining > 0) {
        size_t to_read = remaining > sizeof(buffer) ? sizeof(buffer) : remaining;
        size_t read = fread(buffer, 1, to_read, fin);
        if (read == 0) break;
        fwrite(buffer, 1, read, fout);
        remaining -= read;
    }
    
    fclose(fout);
    fclose(fin);
    uft_m2i_free(img);
    
    return UFT_OK;
}

/* ============================================================================
 * Write Functions
 * ============================================================================ */

int uft_m2i_create(m2i_image_t **out) {
    if (!out) return UFT_ERR_INVALID_PARAM;
    
    m2i_image_t *img = calloc(1, sizeof(m2i_image_t));
    if (!img) return UFT_ERR_MEMORY;
    
    img->version = M2I_VERSION;
    img->entry_count = 0;
    img->entries = NULL;
    
    *out = img;
    return UFT_OK;
}

int uft_m2i_add_file(m2i_image_t *img, const char *filename,
                      uint8_t file_type, uint16_t start_addr,
                      const uint8_t *data, size_t data_len) {
    if (!img || !filename || !data) return UFT_ERR_INVALID_PARAM;
    if (img->entry_count >= M2I_MAX_ENTRIES) return -20;
    
    /* Expand entries array */
    m2i_file_entry_t *new_entries = realloc(img->entries,
        (img->entry_count + 1) * sizeof(m2i_file_entry_t));
    if (!new_entries) return UFT_ERR_MEMORY;
    img->entries = new_entries;
    
    m2i_file_entry_t *entry = &img->entries[img->entry_count];
    memset(entry, 0, sizeof(*entry));
    
    strncpy(entry->filename, filename, M2I_FILENAME_LEN);
    entry->file_type = file_type;
    entry->start_address = start_addr;
    entry->file_size = data_len;
    
    /* Allocate and copy data */
    entry->data = malloc(data_len);
    if (!entry->data) return UFT_ERR_MEMORY;
    memcpy(entry->data, data, data_len);
    
    img->entry_count++;
    return UFT_OK;
}

int uft_m2i_write(const char *path, const m2i_image_t *img) {
    if (!path || !img) return UFT_ERR_INVALID_PARAM;
    
    FILE *f = fopen(path, "wb");
    if (!f) return UFT_ERR_IO;
    
    /* Write header */
    m2i_header_t header;
    memcpy(header.signature, M2I_SIGNATURE, M2I_SIGNATURE_LEN);
    header.version = img->version;
    header.entry_count = img->entry_count;
    header.reserved = 0;
    fwrite(&header, 1, sizeof(header), f);
    
    /* Calculate data offset (after header + all entries) */
    uint32_t data_offset = sizeof(m2i_header_t) + 
                           img->entry_count * M2I_ENTRY_SIZE;
    
    /* Write directory entries */
    for (int i = 0; i < img->entry_count; i++) {
        m2i_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        
        /* Pad filename with 0xA0 (PETSCII space) */
        memset(entry.filename, 0xA0, M2I_FILENAME_LEN);
        size_t name_len = strlen(img->entries[i].filename);
        if (name_len > M2I_FILENAME_LEN) name_len = M2I_FILENAME_LEN;
        memcpy(entry.filename, img->entries[i].filename, name_len);
        
        entry.file_type = img->entries[i].file_type;
        if (img->entries[i].locked) entry.file_type |= M2I_FLAG_LOCKED;
        entry.file_type |= M2I_FLAG_CLOSED;
        
        write_le16((uint8_t*)&entry.start_address, img->entries[i].start_address);
        write_le32((uint8_t*)&entry.file_size, img->entries[i].file_size);
        write_le32((uint8_t*)&entry.data_offset, data_offset);
        
        fwrite(&entry, 1, sizeof(entry), f);
        data_offset += img->entries[i].file_size;
    }
    
    /* Write file data */
    for (int i = 0; i < img->entry_count; i++) {
        if (img->entries[i].data && img->entries[i].file_size > 0) {
            fwrite(img->entries[i].data, 1, img->entries[i].file_size, f);
        }
    }
    if (ferror(f)) {
        fclose(f);
        return UFT_ERR_IO;
    }
    
        
    fclose(f);
return UFT_OK;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

void uft_m2i_free(m2i_image_t *img) {
    if (!img) return;
    
    if (img->entries) {
        for (int i = 0; i < img->entry_count; i++) {
            if (img->entries[i].data) {
                free(img->entries[i].data);
            }
        }
        free(img->entries);
    }
    free(img);
}

int uft_m2i_get_info(const char *path, char *buf, size_t buf_size) {
    if (!path || !buf) return UFT_ERR_INVALID_PARAM;
    
    m2i_image_t *img = NULL;
    int err = uft_m2i_read(path, &img);
    if (err != UFT_OK) return err;
    
    size_t offset = 0;
    offset += snprintf(buf + offset, buf_size - offset,
        "Format: M2I (Tape Image)\n"
        "Version: %d\n"
        "Files: %d\n\n"
        "Directory:\n",
        img->version, img->entry_count);
    
    for (int i = 0; i < img->entry_count && offset < buf_size; i++) {
        m2i_file_entry_t *e = &img->entries[i];
        offset += snprintf(buf + offset, buf_size - offset,
            "  %2d: %-16s  %s%s  %5u bytes  $%04X\n",
            i, e->filename,
            m2i_type_name(e->file_type),
            e->locked ? "<" : " ",
            (unsigned)e->file_size,
            e->start_address);
    }
    
    uft_m2i_free(img);
    return UFT_OK;
}

/* ============================================================================
 * Conversion Functions
 * ============================================================================ */

int uft_m2i_to_t64(const char *m2i_path, const char *t64_path) {
    if (!m2i_path || !t64_path) return UFT_ERR_INVALID_PARAM;
    
    /* Read M2I */
    m2i_image_t *img = NULL;
    int err = uft_m2i_read(m2i_path, &img);
    if (err != UFT_OK) return err;
    
    /* T64 conversion would go here */
    /* For now, just placeholder */
    
    uft_m2i_free(img);
    return UFT_OK;
}
