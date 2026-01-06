/**
 * @file uft_d64_prg.c
 * @brief D64 PRG File Manipulation Implementation
 * @version 1.0.0
 * 
 * Based on "The Little Black Book" forensic techniques.
 * 
 * SPDX-License-Identifier: MIT
 */

#include <uft/cbm/uft_d64_prg.h>
#include <uft/cbm/uft_d64_layout.h>
#include <uft/cbm/uft_d64_bam.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

#define DIR_ENTRY_SIZE      32
#define DIR_ENTRIES_PER_SECTOR  8
#define SECTOR_DATA_SIZE    254     /* 256 - 2 bytes for T/S link */
#define MAX_CHAIN_SECTORS   2000    /* Loop guard */

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Convert PETSCII filename to ASCII for comparison
 */
static void petscii_filename_to_ascii(char* out, const uint8_t* petscii, size_t len)
{
    size_t w = 0;
    for (size_t i = 0; i < len && w < 16; i++) {
        uint8_t b = petscii[i];
        if (b == 0xA0) break;  /* Padding = end of name */
        
        if (b >= 0x41 && b <= 0x5A) {
            out[w++] = (char)b;  /* A-Z */
        } else if (b >= 0xC1 && b <= 0xDA) {
            out[w++] = (char)(b - 0x80);  /* Shifted -> uppercase */
        } else if (b >= 0x20 && b <= 0x7E) {
            out[w++] = (char)b;
        } else {
            out[w++] = '?';
        }
    }
    out[w] = '\0';
}

/**
 * @brief Compare filename (case-insensitive)
 */
static bool filename_match(const char* a, const char* b)
{
    if (!a || !b) return false;
    
    while (*a && *b) {
        char ca = (char)toupper((unsigned char)*a);
        char cb = (char)toupper((unsigned char)*b);
        if (ca != cb) return false;
        a++;
        b++;
    }
    return *a == *b;
}

/**
 * @brief Find directory entry by filename
 */
static int find_dir_entry(const uft_d64_image_t* img, const char* filename,
                          uint8_t* out_track, uint8_t* out_sector,
                          size_t* out_entry_idx, uint8_t* out_entry)
{
    if (!img || !filename) return -1;
    
    /* Read BAM to get directory start */
    const uint8_t* bam = uft_d64_get_sector_const(img, 18, 0);
    if (!bam) return -1;
    
    uint8_t dir_track = bam[0];
    uint8_t dir_sector = bam[1];
    
    /* If BAM points to itself (unusual), start at sector 1 */
    if (dir_track == 18 && dir_sector == 0) {
        dir_sector = 1;
    }
    
    uint8_t t = dir_track;
    uint8_t s = dir_sector;
    uint32_t guard = 0;
    
    while (t != 0 && guard++ < MAX_CHAIN_SECTORS) {
        const uint8_t* sector = uft_d64_get_sector_const(img, t, s);
        if (!sector) return -1;
        
        for (size_t e = 0; e < DIR_ENTRIES_PER_SECTOR; e++) {
            const uint8_t* entry = sector + (e * DIR_ENTRY_SIZE);
            uint8_t ftype = entry[2];
            
            /* Skip deleted entries */
            if ((ftype & 0x0F) == 0) continue;
            
            /* Compare filename */
            char entry_name[17];
            petscii_filename_to_ascii(entry_name, &entry[5], 16);
            
            if (filename_match(entry_name, filename)) {
                if (out_track) *out_track = t;
                if (out_sector) *out_sector = s;
                if (out_entry_idx) *out_entry_idx = e;
                if (out_entry) memcpy(out_entry, entry, DIR_ENTRY_SIZE);
                return 0;
            }
        }
        
        /* Next directory sector */
        uint8_t next_t = sector[0];
        uint8_t next_s = sector[1];
        
        if (next_t == 0) break;
        t = next_t;
        s = next_s;
    }
    
    return -1;  /* Not found */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PRG Information Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_d64_prg_get_info(const uft_d64_image_t* img, const char* filename,
                          uft_d64_prg_info_t* info)
{
    if (!img || !filename || !info) return -1;
    
    memset(info, 0, sizeof(*info));
    
    uint8_t entry[DIR_ENTRY_SIZE];
    int rc = find_dir_entry(img, filename, NULL, NULL, NULL, entry);
    if (rc != 0) return -1;
    
    /* Parse directory entry */
    petscii_filename_to_ascii(info->filename, &entry[5], 16);
    info->file_type = entry[2];
    info->start_track = entry[3];
    info->start_sector = entry[4];
    info->size_blocks = (uint16_t)entry[30] | ((uint16_t)entry[31] << 8);
    
    info->is_closed = (entry[2] & UFT_D64_FTYPE_CLOSED) != 0;
    info->is_locked = (entry[2] & UFT_D64_FTYPE_LOCKED) != 0;
    
    /* Get load address if PRG type */
    if ((entry[2] & 0x0F) == UFT_D64_FTYPE_PRG) {
        const uint8_t* first = uft_d64_get_sector_const(img, 
                                                         info->start_track,
                                                         info->start_sector);
        if (first) {
            info->load_address = (uint16_t)first[2] | ((uint16_t)first[3] << 8);
            info->is_basic = (info->load_address == UFT_C64_BASIC_START);
        }
    }
    
    /* Calculate actual size */
    info->size_bytes = uft_d64_get_chain_size(img, info->start_track, 
                                               info->start_sector);
    
    return 0;
}

int uft_d64_prg_get_load_address(const uft_d64_image_t* img, 
                                  const char* filename, uint16_t* addr)
{
    if (!img || !filename || !addr) return -1;
    
    uft_d64_prg_info_t info;
    int rc = uft_d64_prg_get_info(img, filename, &info);
    if (rc != 0) return rc;
    
    if ((info.file_type & 0x0F) != UFT_D64_FTYPE_PRG) {
        return -1;  /* Not a PRG file */
    }
    
    *addr = info.load_address;
    return 0;
}

bool uft_d64_prg_is_basic(const uft_d64_image_t* img, const char* filename)
{
    if (!img || !filename) return false;
    
    uft_d64_prg_info_t info;
    if (uft_d64_prg_get_info(img, filename, &info) != 0) return false;
    
    if ((info.file_type & 0x0F) != UFT_D64_FTYPE_PRG) return false;
    if (info.load_address != UFT_C64_BASIC_START) return false;
    
    /* Could add BASIC token validation here for more certainty */
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PRG Modification Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_d64_prg_set_load_address(uft_d64_image_t* img, 
                                  const char* filename, uint16_t addr)
{
    if (!img || !filename) return -1;
    
    /* Find file */
    uint8_t entry[DIR_ENTRY_SIZE];
    int rc = find_dir_entry(img, filename, NULL, NULL, NULL, entry);
    if (rc != 0) return -1;
    
    /* Verify it's a PRG file */
    if ((entry[2] & 0x0F) != UFT_D64_FTYPE_PRG) return -1;
    
    /* Get first sector */
    uint8_t* first = uft_d64_get_sector(img, entry[3], entry[4]);
    if (!first) return -1;
    
    /* Set load address (bytes 2-3 of first sector) */
    first[2] = (uint8_t)(addr & 0xFF);
    first[3] = (uint8_t)((addr >> 8) & 0xFF);
    
    img->modified = true;
    return 0;
}

int uft_d64_prg_patch(uft_d64_image_t* img, const char* filename,
                       size_t offset, const uint8_t* data, size_t len)
{
    if (!img || !filename || !data || len == 0) return -1;
    
    /* Find file */
    uint8_t entry[DIR_ENTRY_SIZE];
    int rc = find_dir_entry(img, filename, NULL, NULL, NULL, entry);
    if (rc != 0) return -1;
    
    uint8_t t = entry[3];
    uint8_t s = entry[4];
    
    /* Skip to correct position in chain */
    size_t pos = 0;
    size_t bytes_written = 0;
    uint32_t guard = 0;
    
    /* Account for load address (first 2 bytes of first sector's data) */
    offset += 2;
    
    while (t != 0 && bytes_written < len && guard++ < MAX_CHAIN_SECTORS) {
        uint8_t* sector = uft_d64_get_sector(img, t, s);
        if (!sector) return -1;
        
        uint8_t next_t = sector[0];
        uint8_t next_s = sector[1];
        size_t data_len = (next_t == 0) ? next_s : SECTOR_DATA_SIZE;
        
        /* Check if we need to write in this sector */
        if (pos + data_len > offset) {
            size_t start = (offset > pos) ? (offset - pos) : 0;
            size_t end = data_len;
            
            for (size_t i = start; i < end && bytes_written < len; i++) {
                sector[2 + i] = data[bytes_written++];
            }
        }
        
        pos += data_len;
        t = next_t;
        s = next_s;
    }
    
    if (bytes_written < len) return -1;  /* Couldn't write all data */
    
    img->modified = true;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * File Chain Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_d64_iterate_chain(const uft_d64_image_t* img,
                           uint8_t start_track, uint8_t start_sector,
                           uft_d64_chain_callback_t callback, void* user_data)
{
    if (!img || !callback) return -1;
    if (start_track == 0) return -1;
    
    uint8_t t = start_track;
    uint8_t s = start_sector;
    uint32_t guard = 0;
    
    while (t != 0 && guard++ < MAX_CHAIN_SECTORS) {
        const uint8_t* sector = uft_d64_get_sector_const(img, t, s);
        if (!sector) return -1;
        
        uint8_t next_t = sector[0];
        uint8_t next_s = sector[1];
        
        /* Data starts at offset 2, length is 254 or sector[1] for last sector */
        size_t data_len = SECTOR_DATA_SIZE;
        if (next_t == 0) {
            /* Last sector: sector[1] contains bytes used (0 means 256?) */
            data_len = (next_s == 0) ? SECTOR_DATA_SIZE : next_s;
            if (data_len > SECTOR_DATA_SIZE) data_len = SECTOR_DATA_SIZE;
        }
        
        int rc = callback(t, s, sector + 2, data_len, user_data);
        if (rc != 0) return rc;
        
        t = next_t;
        s = next_s;
    }
    
    return 0;
}

/* Size calculation callback */
typedef struct {
    size_t total;
} size_ctx_t;

static int size_callback(uint8_t track, uint8_t sector,
                         const uint8_t* data, size_t data_len, void* user)
{
    (void)track; (void)sector; (void)data;
    size_ctx_t* ctx = (size_ctx_t*)user;
    ctx->total += data_len;
    return 0;
}

size_t uft_d64_get_chain_size(const uft_d64_image_t* img,
                               uint8_t start_track, uint8_t start_sector)
{
    size_ctx_t ctx = { .total = 0 };
    uft_d64_iterate_chain(img, start_track, start_sector, size_callback, &ctx);
    return ctx.total;
}

/* Read callback */
typedef struct {
    uint8_t* buffer;
    size_t buffer_size;
    size_t written;
} read_ctx_t;

static int read_callback(uint8_t track, uint8_t sector,
                         const uint8_t* data, size_t data_len, void* user)
{
    (void)track; (void)sector;
    read_ctx_t* ctx = (read_ctx_t*)user;
    
    size_t to_copy = data_len;
    if (ctx->written + to_copy > ctx->buffer_size) {
        to_copy = ctx->buffer_size - ctx->written;
    }
    
    if (to_copy > 0) {
        memcpy(ctx->buffer + ctx->written, data, to_copy);
        ctx->written += to_copy;
    }
    
    return 0;
}

int uft_d64_prg_read(const uft_d64_image_t* img, const char* filename,
                      uint8_t* buffer, size_t buffer_size, size_t* bytes_read)
{
    if (!img || !filename || !buffer || buffer_size == 0) return -1;
    
    uft_d64_prg_info_t info;
    int rc = uft_d64_prg_get_info(img, filename, &info);
    if (rc != 0) return rc;
    
    read_ctx_t ctx = {
        .buffer = buffer,
        .buffer_size = buffer_size,
        .written = 0
    };
    
    rc = uft_d64_iterate_chain(img, info.start_track, info.start_sector,
                                read_callback, &ctx);
    
    if (bytes_read) *bytes_read = ctx.written;
    return rc;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Pattern Search Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const uint8_t* pattern;
    size_t pat_len;
    uft_d64_ts_position_t* result;
    bool found;
    size_t file_offset;
} find_ctx_t;

static int find_callback(uint8_t track, uint8_t sector,
                         const uint8_t* data, size_t data_len, void* user)
{
    find_ctx_t* ctx = (find_ctx_t*)user;
    if (ctx->found) return 0;
    
    for (size_t i = 0; i + ctx->pat_len <= data_len; i++) {
        if (memcmp(data + i, ctx->pattern, ctx->pat_len) == 0) {
            ctx->result->track = track;
            ctx->result->sector = sector;
            ctx->result->offset = (uint16_t)(i + 2);  /* +2 for T/S link */
            ctx->found = true;
            return 0;
        }
    }
    
    ctx->file_offset += data_len;
    return 0;
}

int uft_d64_prg_find_pattern(const uft_d64_image_t* img, const char* filename,
                              const uint8_t* pattern, size_t pat_len,
                              uft_d64_ts_position_t* position)
{
    if (!img || !filename || !pattern || pat_len == 0 || !position) return -1;
    
    uft_d64_prg_info_t info;
    int rc = uft_d64_prg_get_info(img, filename, &info);
    if (rc != 0) return rc;
    
    find_ctx_t ctx = {
        .pattern = pattern,
        .pat_len = pat_len,
        .result = position,
        .found = false,
        .file_offset = 0
    };
    
    rc = uft_d64_iterate_chain(img, info.start_track, info.start_sector,
                                find_callback, &ctx);
    if (rc != 0) return rc;
    
    return ctx.found ? 0 : 1;  /* 0=found, 1=not found */
}

int uft_d64_prg_replace_pattern(uft_d64_image_t* img, const char* filename,
                                 const uint8_t* pattern, size_t pat_len,
                                 const uint8_t* replacement, size_t rep_len,
                                 size_t* count)
{
    if (!img || !filename || !pattern || !replacement) return -1;
    if (pat_len == 0 || pat_len != rep_len) return -1;
    
    uft_d64_prg_info_t info;
    int rc = uft_d64_prg_get_info(img, filename, &info);
    if (rc != 0) return rc;
    
    uint8_t t = info.start_track;
    uint8_t s = info.start_sector;
    size_t replacements = 0;
    uint32_t guard = 0;
    
    while (t != 0 && guard++ < MAX_CHAIN_SECTORS) {
        uint8_t* sector = uft_d64_get_sector(img, t, s);
        if (!sector) break;
        
        uint8_t next_t = sector[0];
        uint8_t next_s = sector[1];
        size_t data_len = (next_t == 0) ? 
                          ((next_s == 0) ? SECTOR_DATA_SIZE : next_s) : 
                          SECTOR_DATA_SIZE;
        
        /* Search and replace in this sector */
        for (size_t i = 0; i + pat_len <= data_len; i++) {
            if (memcmp(sector + 2 + i, pattern, pat_len) == 0) {
                memcpy(sector + 2 + i, replacement, rep_len);
                replacements++;
                i += pat_len - 1;  /* Skip past replacement */
            }
        }
        
        t = next_t;
        s = next_s;
    }
    
    if (replacements > 0) {
        img->modified = true;
    }
    
    if (count) *count = replacements;
    return 0;
}
