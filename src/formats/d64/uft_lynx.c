/**
 * @file uft_lynx.c
 * @brief Lynx Archive Format Implementation
 * @version 1.0.0
 * 
 * Based on the Lynx archiver format by Will Corley (1986).
 * Reference implementation from lib1541img.
 * 
 * SPDX-License-Identifier: MIT
 */

#include <uft/cbm/uft_lynx.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Standard Lynx BASIC loader header.
 * This is the self-extractor stub that displays:
 * "USE LYNX TO DISSOLVE THIS FILE"
 */
static const uint8_t LYNX_HEADER[] = {
    0x01, 0x08, 0x5B, 0x08, 0x0A, 0x00, 0x97, 0x35,
    0x33, 0x32, 0x38, 0x30, 0x2C, 0x30, 0x3A, 0x97,
    0x35, 0x33, 0x32, 0x38, 0x31, 0x2C, 0x30, 0x3A,
    0x97, 0x36, 0x34, 0x36, 0x2C, 0xC2, 0x28, 0x31,
    0x36, 0x32, 0x29, 0x3A, 0x99, 0x22, 0x93, 0x11,
    0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x22,
    0x3A, 0x99, 0x22, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x55, 0x53, 0x45, 0x20, 0x4C, 0x59, 0x4E, 0x58,
    0x20, 0x54, 0x4F, 0x20, 0x44, 0x49, 0x53, 0x53,
    0x4F, 0x4C, 0x56, 0x45, 0x20, 0x54, 0x48, 0x49,
    0x53, 0x20, 0x46, 0x49, 0x4C, 0x45, 0x22, 0x3A,
    0x89, 0x31, 0x30, 0x00, 0x00, 0x00, 0x0d, 0x20,
    0x20, 0x20, 0x20, 0x2a, 0x4c, 0x59, 0x4e, 0x58,
    0x20, 0x41, 0x52, 0x43, 0x48, 0x49, 0x56, 0x45,
    0x20, 0x42, 0x59, 0x20, 0x45, 0x58, 0x43, 0x45,
    0x53, 0x53, 0x21, 0x0d, 0x20
};

/** File type characters in Lynx directory: DEL, SEQ, PRG, USR, REL */
static const uint8_t LYNX_FILETYPE_CHARS[] = { 0x00, 'S', 'P', 'U', 'R' };

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parse a PETSCII decimal number
 */
static int parse_petscii_num(uint16_t* num, size_t* pos,
                             const uint8_t* data, size_t size)
{
    size_t p = *pos;
    
    /* Skip leading spaces */
    while (p < size && data[p] == 0x20) p++;
    
    /* Must start with digit 1-9 */
    if (p >= size || data[p] < 0x31 || data[p] > 0x39) return -1;
    
    uint16_t n = data[p++] - 0x30;
    if (p >= size) return -1;
    
    /* Parse remaining digits (up to 3 total for values 1-999) */
    for (int i = 0; i < 2 && p < size; i++) {
        if (data[p] >= 0x30 && data[p] <= 0x39) {
            n = n * 10 + (data[p++] - 0x30);
        } else {
            break;
        }
    }
    
    /* Can't have more digits */
    if (p < size && data[p] >= 0x30 && data[p] <= 0x39) return -1;
    
    /* Skip trailing spaces */
    while (p < size && data[p] == 0x20) p++;
    
    /* Must end with CR */
    if (p >= size || data[p] != 0x0D) return -1;
    p++;
    
    if (p >= size) return -1;
    
    *pos = p;
    *num = n;
    return 0;
}

/**
 * @brief Format a number as PETSCII decimal
 */
static int format_petscii_num(uint8_t* buf, size_t maxlen, unsigned num)
{
    int len = snprintf((char*)buf, maxlen, "%u", num);
    if (len < 1 || (size_t)len >= maxlen) return -1;
    
    /* Ensure ASCII digits (should already be, but be safe) */
    for (int i = 0; i < len; i++) {
        if (buf[i] >= '0' && buf[i] <= '9') {
            buf[i] = (uint8_t)(0x30 + (buf[i] - '0'));
        }
    }
    
    return len;
}

/**
 * @brief Find Lynx header after BASIC stub
 */
static int find_lynx_header(size_t* sigpos, uint8_t* dirblocks, size_t* dirpos,
                            const uint8_t* data, size_t size)
{
    size_t pos = *sigpos;
    
    /* Skip leading spaces */
    while (pos < size && data[pos] == 0x20) pos++;
    if (pos >= size) return -1;
    
    /* Parse directory block count (1-2 digits) */
    if (data[pos] < 0x31 || data[pos] > 0x39) return -1;
    uint8_t blocks = data[pos++] - 0x30;
    
    if (pos >= size) return -1;
    if (data[pos] >= 0x30 && data[pos] <= 0x39) {
        blocks = blocks * 10 + (data[pos++] - 0x30);
        if (pos >= size) return -1;
    }
    if (data[pos] >= 0x30 && data[pos] <= 0x39) return -1; /* Too many digits */
    
    /* Skip spaces to signature */
    while (pos < size && data[pos] == 0x20) pos++;
    
    size_t sig_start = pos;
    
    /* Find end of signature line (CR) */
    while (pos < size && data[pos] != 0x0D) {
        if (data[pos] == 0x00) return -1; /* Unexpected null */
        pos++;
    }
    
    if (pos >= size - 5) return -1;
    pos++; /* Skip CR */
    
    *sigpos = sig_start;
    *dirblocks = blocks;
    *dirpos = pos;
    return 0;
}

/**
 * @brief Find the header structure in archive data
 */
static int find_header(size_t* sigpos, uint8_t* dirblocks, size_t* dirpos,
                       const uint8_t* data, size_t size)
{
    if (size < 255) return -1;
    
    /* Check for BASIC program structure */
    size_t base = (size_t)data[0] | ((size_t)data[1] << 8);
    size_t next = (size_t)data[2] | ((size_t)data[3] << 8);
    
    size_t pos = 0;
    
    if (next > base) {
        pos = next - base + 2;
        
        if (pos <= size - 5 && data[pos - 1] == 0x00) {
            /* Walk through BASIC lines */
            while (pos < size - 1 && (data[pos] || data[pos + 1])) {
                size_t nextline = (size_t)data[pos] | ((size_t)data[pos + 1] << 8);
                if (nextline <= next) return -1;
                next = nextline;
                pos = next - base + 2;
                if (pos > size - 5) return -1;
                if (data[pos - 1]) return -1;
            }
            pos += 2;
            if (pos >= size || data[pos++] != 0x0D) {
                pos = 0; /* Try without BASIC parsing */
            }
        } else {
            pos = 0;
        }
    }
    
    /* Find Lynx-specific header */
    if (find_lynx_header(&pos, dirblocks, dirpos, data, size) < 0) {
        return -1;
    }
    
    *sigpos = pos;
    return 0;
}

/**
 * @brief Convert PETSCII to ASCII (simplified)
 */
static void petscii_to_ascii(char* out, const uint8_t* petscii, size_t len)
{
    size_t w = 0;
    for (size_t i = 0; i < len && w < 16; i++) {
        uint8_t b = petscii[i];
        if (b == 0xA0) break; /* Shifted space = padding */
        
        if (b >= 0x41 && b <= 0x5A) {
            out[w++] = (char)b; /* A-Z */
        } else if (b >= 0xC1 && b <= 0xDA) {
            out[w++] = (char)(b - 0x80); /* Shifted -> uppercase */
        } else if (b >= 0x20 && b <= 0x7E) {
            out[w++] = (char)b;
        } else {
            out[w++] = '?';
        }
    }
    out[w] = '\0';
}

/**
 * @brief Convert ASCII to PETSCII with padding
 */
static void ascii_to_petscii(uint8_t* out, const char* ascii, size_t maxlen)
{
    size_t len = ascii ? strlen(ascii) : 0;
    
    for (size_t i = 0; i < maxlen; i++) {
        if (i < len) {
            char c = ascii[i];
            if (c >= 'a' && c <= 'z') {
                out[i] = (uint8_t)(c - 'a' + 0xC1); /* Shifted lowercase */
            } else if (c >= 'A' && c <= 'Z') {
                out[i] = (uint8_t)c;
            } else if (c >= 0x20 && c <= 0x7E) {
                out[i] = (uint8_t)c;
            } else {
                out[i] = 0xA0;
            }
        } else {
            out[i] = 0xA0; /* Padding */
        }
    }
}

/**
 * @brief Case-insensitive string comparison
 */
static bool str_eq_nocase(const char* a, const char* b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
        a++;
        b++;
    }
    return *a == *b;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Detection Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

bool uft_lynx_detect(const uint8_t* data, size_t size)
{
    if (!data || size < 255) return false;
    
    size_t sigpos;
    uint8_t dirblocks;
    size_t dirpos;
    
    return find_header(&sigpos, &dirblocks, &dirpos, data, size) == 0;
}

int uft_lynx_detect_confidence(const uint8_t* data, size_t size)
{
    if (!data || size < 100) return 0;
    
    int confidence = 0;
    
    /* Check load address ($0801 = BASIC start) */
    if (data[0] == 0x01 && data[1] == 0x08) {
        confidence += 20;
    }
    
    /* Check for "LYNX" text somewhere in first 256 bytes */
    for (size_t i = 0; i < size && i < 256 - 4; i++) {
        if ((data[i] == 'L' || data[i] == 'l' || data[i] == 0x4C) &&
            (data[i+1] == 'Y' || data[i+1] == 'y' || data[i+1] == 0x59) &&
            (data[i+2] == 'N' || data[i+2] == 'n' || data[i+2] == 0x4E) &&
            (data[i+3] == 'X' || data[i+3] == 'x' || data[i+3] == 0x58)) {
            confidence += 40;
            break;
        }
    }
    
    /* Try to parse as Lynx */
    size_t sigpos;
    uint8_t dirblocks;
    size_t dirpos;
    
    if (find_header(&sigpos, &dirblocks, &dirpos, data, size) == 0) {
        confidence += 40;
        
        /* Check if we can parse file count */
        uint16_t numfiles;
        size_t pos = dirpos;
        if (parse_petscii_num(&numfiles, &pos, data, size) == 0) {
            if (numfiles > 0 && numfiles <= UFT_LYNX_MAX_FILES) {
                confidence += 20;
            }
        }
    }
    
    return confidence > 100 ? 100 : confidence;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Archive Reading Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_lynx_open(const uint8_t* data, size_t size, uft_lynx_archive_t* archive)
{
    if (!data || !archive) return -1;
    
    memset(archive, 0, sizeof(*archive));
    archive->data = data;
    archive->data_size = size;
    
    /* Find header */
    size_t sigpos;
    uint8_t dirblocks;
    size_t pos;
    
    if (find_header(&sigpos, &dirblocks, &pos, data, size) < 0) {
        return -1;
    }
    
    /* Extract signature */
    size_t siglen = 0;
    while (sigpos + siglen < size && data[sigpos + siglen] != 0x0D && siglen < 79) {
        siglen++;
    }
    memcpy(archive->info.signature, data + sigpos, siglen);
    archive->info.signature[siglen] = '\0';
    archive->info.dir_blocks = dirblocks;
    
    /* Parse file count */
    uint16_t numfiles;
    if (parse_petscii_num(&numfiles, &pos, data, size) < 0) {
        return -1;
    }
    
    if (numfiles == 0 || numfiles > UFT_LYNX_MAX_FILES) {
        return -1;
    }
    
    archive->info.file_count = numfiles;
    
    /* Allocate entry array */
    archive->entries = (uft_lynx_entry_t*)calloc(numfiles, sizeof(uft_lynx_entry_t));
    if (!archive->entries) {
        return -1;
    }
    archive->entry_count = numfiles;
    
    /* Parse directory entries */
    for (uint16_t i = 0; i < numfiles; i++) {
        uft_lynx_entry_t* entry = &archive->entries[i];
        
        /* Parse filename (up to CR) */
        uint8_t namelen = 0;
        while (pos + namelen < size && data[pos + namelen] != 0x0D && namelen < 16) {
            namelen++;
        }
        
        if (pos + namelen + 1 >= size) {
            uft_lynx_close(archive);
            return -1;
        }
        
        /* Trim trailing padding */
        while (namelen > 0 && data[pos + namelen - 1] == 0xA0) {
            namelen--;
        }
        
        memcpy(entry->name_petscii, data + pos, namelen);
        entry->name_len = namelen;
        petscii_to_ascii(entry->name, data + pos, namelen);
        
        pos += namelen;
        while (pos < size && data[pos] != 0x0D) pos++;
        if (pos >= size) { uft_lynx_close(archive); return -1; }
        pos++; /* Skip CR */
        
        /* Parse block count */
        if (parse_petscii_num(&entry->blocks, &pos, data, size) < 0) {
            uft_lynx_close(archive);
            return -1;
        }
        
        /* Parse file type character */
        if (pos >= size || data[pos] == 0) {
            uft_lynx_close(archive);
            return -1;
        }
        
        uint8_t type_char = data[pos++];
        entry->type = UFT_LYNX_PRG; /* Default */
        
        for (int t = 1; t < 5; t++) {
            if (LYNX_FILETYPE_CHARS[t] == type_char) {
                entry->type = (uft_lynx_filetype_t)t;
                break;
            }
        }
        
        if (pos >= size || data[pos] != 0x0D) {
            uft_lynx_close(archive);
            return -1;
        }
        pos++; /* Skip CR */
        
        /* For REL files, parse record length */
        if (entry->type == UFT_LYNX_REL) {
            uint16_t reclen;
            if (parse_petscii_num(&reclen, &pos, data, size) < 0) {
                uft_lynx_close(archive);
                return -1;
            }
            entry->record_len = (uint8_t)reclen;
        }
        
        /* Parse last sector usage */
        uint16_t lsu;
        if (parse_petscii_num(&lsu, &pos, data, size) < 0) {
            /* Last file may not have LSU */
            if (i < numfiles - 1) {
                uft_lynx_close(archive);
                return -1;
            }
            entry->last_sector_usage = 0;
            entry->size = 0; /* Will be calculated from remaining data */
        } else {
            entry->last_sector_usage = (uint8_t)lsu;
            if (entry->blocks > 0) {
                entry->size = (size_t)(entry->blocks - 1) * UFT_LYNX_BLOCK_SIZE + 
                              (lsu > 0 ? lsu - 1 : UFT_LYNX_BLOCK_SIZE);
            }
        }
    }
    
    /* Calculate data offsets */
    size_t data_pos = (size_t)dirblocks * UFT_LYNX_BLOCK_SIZE;
    
    for (uint16_t i = 0; i < numfiles; i++) {
        uft_lynx_entry_t* entry = &archive->entries[i];
        
        /* Align to block boundary */
        size_t pad = UFT_LYNX_BLOCK_SIZE - (pos % UFT_LYNX_BLOCK_SIZE);
        if (pad < UFT_LYNX_BLOCK_SIZE) {
            pos += pad;
        }
        
        /* For REL files, skip side sectors */
        if (entry->type == UFT_LYNX_REL) {
            uint8_t sidesects = (entry->blocks / 120) + (entry->blocks % 120 ? 1 : 0);
            pos += (size_t)sidesects * UFT_LYNX_BLOCK_SIZE;
        }
        
        entry->data_offset = pos;
        
        /* Calculate size for last file if not set */
        if (i == numfiles - 1 && entry->size == 0) {
            if (pos < size) {
                entry->size = size - pos;
                if (entry->size > (size_t)entry->blocks * UFT_LYNX_BLOCK_SIZE) {
                    entry->size = (size_t)entry->blocks * UFT_LYNX_BLOCK_SIZE;
                }
            }
        }
        
        pos += entry->size;
    }
    
    archive->info.total_size = size;
    archive->info.is_valid = true;
    
    return 0;
}

void uft_lynx_close(uft_lynx_archive_t* archive)
{
    if (!archive) return;
    
    free(archive->entries);
    archive->entries = NULL;
    archive->entry_count = 0;
    
    if (archive->owns_data) {
        free((void*)archive->data);
    }
    archive->data = NULL;
    archive->data_size = 0;
    
    memset(&archive->info, 0, sizeof(archive->info));
}

const uft_lynx_info_t* uft_lynx_get_info(const uft_lynx_archive_t* archive)
{
    return archive ? &archive->info : NULL;
}

uint16_t uft_lynx_get_file_count(const uft_lynx_archive_t* archive)
{
    return archive ? archive->entry_count : 0;
}

const uft_lynx_entry_t* uft_lynx_get_entry(const uft_lynx_archive_t* archive,
                                           uint16_t index)
{
    if (!archive || index >= archive->entry_count) return NULL;
    return &archive->entries[index];
}

int uft_lynx_find_file(const uft_lynx_archive_t* archive, const char* name)
{
    if (!archive || !name) return -1;
    
    for (uint16_t i = 0; i < archive->entry_count; i++) {
        if (str_eq_nocase(archive->entries[i].name, name)) {
            return (int)i;
        }
    }
    
    return -1;
}

int uft_lynx_extract_file(const uft_lynx_archive_t* archive,
                          uint16_t index,
                          uint8_t* buffer,
                          size_t buf_size,
                          size_t* out_size)
{
    if (!archive || !buffer || index >= archive->entry_count) return -1;
    
    const uft_lynx_entry_t* entry = &archive->entries[index];
    
    if (entry->data_offset + entry->size > archive->data_size) {
        return -1;
    }
    
    size_t copy_size = entry->size;
    if (copy_size > buf_size) {
        copy_size = buf_size;
    }
    
    memcpy(buffer, archive->data + entry->data_offset, copy_size);
    
    if (out_size) *out_size = copy_size;
    return 0;
}

int uft_lynx_extract_file_alloc(const uft_lynx_archive_t* archive,
                                uint16_t index,
                                uint8_t** out_data,
                                size_t* out_size)
{
    if (!archive || !out_data || index >= archive->entry_count) return -1;
    
    const uft_lynx_entry_t* entry = &archive->entries[index];
    
    *out_data = (uint8_t*)malloc(entry->size);
    if (!*out_data) return -1;
    
    int rc = uft_lynx_extract_file(archive, index, *out_data, entry->size, out_size);
    if (rc != 0) {
        free(*out_data);
        *out_data = NULL;
    }
    
    return rc;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Archive Creation Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

size_t uft_lynx_estimate_size(const uft_lynx_file_t* files, uint16_t file_count)
{
    if (!files || file_count == 0) return 0;
    
    /* Header */
    size_t size = sizeof(LYNX_HEADER);
    
    /* File count line */
    size += 10;
    
    /* Directory entries */
    for (uint16_t i = 0; i < file_count; i++) {
        size += 16 + 2;  /* Name + CR */
        size += 5 + 2;   /* Blocks + CR */
        size += 1 + 1;   /* Type + CR */
        if (files[i].type == UFT_LYNX_REL) {
            size += 5;   /* Record length */
        }
        size += 5;       /* LSU */
    }
    
    /* Padding to block boundary */
    size = ((size + UFT_LYNX_BLOCK_SIZE - 1) / UFT_LYNX_BLOCK_SIZE) * UFT_LYNX_BLOCK_SIZE;
    
    /* File data */
    for (uint16_t i = 0; i < file_count; i++) {
        size_t file_size = files[i].size;
        size_t blocks = (file_size + UFT_LYNX_BLOCK_SIZE - 1) / UFT_LYNX_BLOCK_SIZE;
        if (blocks == 0) blocks = 1;
        
        /* REL files have side sectors */
        if (files[i].type == UFT_LYNX_REL) {
            uint8_t sidesects = (uint8_t)((blocks / 120) + (blocks % 120 ? 1 : 0));
            size += (size_t)sidesects * UFT_LYNX_BLOCK_SIZE;
        }
        
        size += blocks * UFT_LYNX_BLOCK_SIZE;
    }
    
    return size;
}

int uft_lynx_create(const uft_lynx_file_t* files,
                    uint16_t file_count,
                    const char* signature,
                    uint8_t** out_data,
                    size_t* out_size)
{
    if (!files || file_count == 0 || !out_data || !out_size) return -1;
    if (file_count > UFT_LYNX_MAX_FILES) return -1;
    
    if (!signature) signature = UFT_LYNX_DEFAULT_SIGNATURE;
    
    /* Estimate and allocate buffer */
    size_t est_size = uft_lynx_estimate_size(files, file_count);
    uint8_t* data = (uint8_t*)calloc(est_size + 1024, 1); /* Extra safety margin */
    if (!data) return -1;
    
    size_t pos = 0;
    
    /* Write BASIC header */
    memcpy(data + pos, LYNX_HEADER, sizeof(LYNX_HEADER));
    pos = sizeof(LYNX_HEADER);
    
    /* Calculate directory blocks (we'll update this later) */
    size_t dir_blocks_pos = 0x60; /* Position of block count in header */
    
    /* Write file count */
    uint8_t numbuf[8];
    int len = format_petscii_num(numbuf, sizeof(numbuf), file_count);
    if (len < 0) { free(data); return -1; }
    memcpy(data + pos, numbuf, (size_t)len);
    pos += (size_t)len;
    data[pos++] = 0x20; /* Space */
    data[pos++] = 0x0D; /* CR */
    
    /* Write directory entries */
    for (uint16_t i = 0; i < file_count; i++) {
        const uft_lynx_file_t* file = &files[i];
        
        /* Filename (16 chars, padded with 0xA0) */
        uint8_t name_pet[16];
        ascii_to_petscii(name_pet, file->name, 16);
        memcpy(data + pos, name_pet, 16);
        pos += 16;
        data[pos++] = 0x0D;
        
        /* Block count */
        uint16_t blocks = (uint16_t)((file->size + UFT_LYNX_BLOCK_SIZE - 1) / UFT_LYNX_BLOCK_SIZE);
        if (blocks == 0) blocks = 1;
        
        data[pos++] = 0x20; /* Leading space */
        len = format_petscii_num(numbuf, sizeof(numbuf), blocks);
        if (len < 0) { free(data); return -1; }
        memcpy(data + pos, numbuf, (size_t)len);
        pos += (size_t)len;
        data[pos++] = 0x20;
        data[pos++] = 0x0D;
        
        /* File type */
        uint8_t type_char = LYNX_FILETYPE_CHARS[file->type];
        if (type_char == 0) type_char = 'P'; /* Default to PRG */
        data[pos++] = type_char;
        data[pos++] = 0x0D;
        
        /* Record length for REL files */
        if (file->type == UFT_LYNX_REL) {
            data[pos++] = 0x20;
            len = format_petscii_num(numbuf, sizeof(numbuf), file->record_len);
            if (len < 0) { free(data); return -1; }
            memcpy(data + pos, numbuf, (size_t)len);
            pos += (size_t)len;
            data[pos++] = 0x20;
            data[pos++] = 0x0D;
        }
        
        /* Last sector usage */
        data[pos++] = 0x20;
        unsigned lsu = (unsigned)(file->size % UFT_LYNX_BLOCK_SIZE);
        if (lsu == 0 && file->size > 0) lsu = UFT_LYNX_BLOCK_SIZE;
        lsu++; /* Lynx LSU is 1-based */
        len = format_petscii_num(numbuf, sizeof(numbuf), lsu);
        if (len < 0) { free(data); return -1; }
        memcpy(data + pos, numbuf, (size_t)len);
        pos += (size_t)len;
        data[pos++] = 0x20;
        data[pos++] = 0x0D;
    }
    
    /* Pad directory to block boundary */
    size_t pad = UFT_LYNX_BLOCK_SIZE - (pos % UFT_LYNX_BLOCK_SIZE);
    if (pad < UFT_LYNX_BLOCK_SIZE) {
        memset(data + pos, 0, pad);
        pos += pad;
    }
    
    /* Update directory block count in header */
    uint8_t dir_blocks = (uint8_t)(pos / UFT_LYNX_BLOCK_SIZE);
    len = format_petscii_num(numbuf, sizeof(numbuf), dir_blocks);
    if (len >= 1) {
        data[dir_blocks_pos] = numbuf[0];
        if (len >= 2) data[dir_blocks_pos + 1] = numbuf[1];
    }
    
    /* Write file data */
    for (uint16_t i = 0; i < file_count; i++) {
        const uft_lynx_file_t* file = &files[i];
        
        /* REL files need side sectors (simplified - just empty placeholders) */
        if (file->type == UFT_LYNX_REL) {
            uint16_t blocks = (uint16_t)((file->size + UFT_LYNX_BLOCK_SIZE - 1) / UFT_LYNX_BLOCK_SIZE);
            uint8_t sidesects = (uint8_t)((blocks / 120) + (blocks % 120 ? 1 : 0));
            pos += (size_t)sidesects * UFT_LYNX_BLOCK_SIZE;
        }
        
        /* Copy file data */
        if (file->data && file->size > 0) {
            memcpy(data + pos, file->data, file->size);
        }
        pos += file->size;
        
        /* Pad to block boundary (except last file) */
        if (i < file_count - 1) {
            pad = UFT_LYNX_BLOCK_SIZE - (pos % UFT_LYNX_BLOCK_SIZE);
            if (pad < UFT_LYNX_BLOCK_SIZE) {
                memset(data + pos, 0, pad);
                pos += pad;
            }
        }
    }
    
    *out_data = data;
    *out_size = pos;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

const char* uft_lynx_type_name(uft_lynx_filetype_t type)
{
    switch (type) {
        case UFT_LYNX_DEL: return "DEL";
        case UFT_LYNX_SEQ: return "SEQ";
        case UFT_LYNX_PRG: return "PRG";
        case UFT_LYNX_USR: return "USR";
        case UFT_LYNX_REL: return "REL";
        default: return "???";
    }
}

uft_lynx_filetype_t uft_lynx_type_from_d64(uint8_t d64_type)
{
    switch (d64_type & 0x0F) {
        case 0x00: return UFT_LYNX_DEL;
        case 0x01: return UFT_LYNX_SEQ;
        case 0x02: return UFT_LYNX_PRG;
        case 0x03: return UFT_LYNX_USR;
        case 0x04: return UFT_LYNX_REL;
        default: return UFT_LYNX_PRG;
    }
}

uint8_t uft_lynx_type_to_d64(uft_lynx_filetype_t lynx_type)
{
    return (uint8_t)lynx_type | 0x80; /* Add "closed" flag */
}
