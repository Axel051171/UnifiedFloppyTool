/**
 * @file uft_amigados_core.c
 * @brief AmigaDOS Core Implementation
 * 
 * Context management, detection, block access, and utilities.
 */

#include "uft/fs/uft_amigados.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Internal Helpers
 *===========================================================================*/

static inline uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static inline int32_t read_be32s(const uint8_t *p)
{
    return (int32_t)read_be32(p);
}

static inline uint16_t read_be16(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

static inline void write_be32(uint8_t *p, uint32_t v)
{
    p[0] = (v >> 24) & 0xFF;
    p[1] = (v >> 16) & 0xFF;
    p[2] = (v >> 8) & 0xFF;
    p[3] = v & 0xFF;
}

static inline void write_be16(uint8_t *p, uint16_t v)
{
    p[0] = (v >> 8) & 0xFF;
    p[1] = v & 0xFF;
}

/* Read BCPL string (length-prefixed) */
static void read_bcpl_string(const uint8_t *src, char *dst, size_t max_len)
{
    size_t len = src[0];
    if (len > max_len - 1) len = max_len - 1;
    memcpy(dst, src + 1, len);
    dst[len] = '\0';
}

/* Write BCPL string */
static void write_bcpl_string(uint8_t *dst, const char *src, size_t max_len)
{
    size_t len = strlen(src);
    if (len > max_len - 1) len = max_len - 1;
    dst[0] = (uint8_t)len;
    memcpy(dst + 1, src, len);
    /* Pad with zeros */
    for (size_t i = len + 1; i < max_len; i++) {
        dst[i] = 0;
    }
}

/* Amiga epoch: 1978-01-01 00:00:00 UTC */
#define AMIGA_EPOCH_OFFSET  252460800LL  /* Seconds from 1970 to 1978 */

/*===========================================================================
 * Checksum Functions
 *===========================================================================*/

uint32_t uft_amiga_block_checksum(const uint8_t *block)
{
    uint32_t sum = 0;
    for (int i = 0; i < UFT_AMIGA_BLOCK_SIZE; i += 4) {
        sum += read_be32(block + i);
    }
    return sum;  /* Should be 0 for valid block */
}

void uft_amiga_update_checksum(uint8_t *block)
{
    /* Clear checksum field (offset 20) */
    write_be32(block + 20, 0);
    
    /* Calculate sum */
    uint32_t sum = 0;
    for (int i = 0; i < UFT_AMIGA_BLOCK_SIZE; i += 4) {
        sum += read_be32(block + i);
    }
    
    /* Store negated sum */
    write_be32(block + 20, (uint32_t)(-(int32_t)sum));
}

bool uft_amiga_verify_checksum(const uint8_t *block)
{
    return uft_amiga_block_checksum(block) == 0;
}

uint32_t uft_amiga_bootblock_checksum(const uint8_t *boot)
{
    uint32_t sum = 0;
    for (int i = 0; i < 1024; i += 4) {
        uint32_t val = read_be32(boot + i);
        uint32_t prev = sum;
        sum += val;
        if (sum < prev) sum++;  /* Handle overflow */
    }
    return sum;
}

/*===========================================================================
 * Time Conversion
 *===========================================================================*/

time_t uft_amiga_to_unix_time(uint32_t days, uint32_t mins, uint32_t ticks)
{
    time_t t = AMIGA_EPOCH_OFFSET;
    t += (time_t)days * 86400;
    t += (time_t)mins * 60;
    t += (time_t)ticks / 50;
    return t;
}

void uft_amiga_from_unix_time(time_t unix_time, uint32_t *days,
                              uint32_t *mins, uint32_t *ticks)
{
    time_t t = unix_time - AMIGA_EPOCH_OFFSET;
    if (t < 0) t = 0;
    
    *days = (uint32_t)(t / 86400);
    t %= 86400;
    *mins = (uint32_t)(t / 60);
    t %= 60;
    *ticks = (uint32_t)(t * 50);
}

/*===========================================================================
 * Protection Bits
 *===========================================================================*/

void uft_amiga_protection_str(uint32_t prot, char *buffer)
{
    /* HSPARWED - note: bits are inverted for RWED */
    buffer[0] = (prot & UFT_AMIGA_PROT_HOLD) ? 'h' : '-';
    buffer[1] = (prot & UFT_AMIGA_PROT_SCRIPT) ? 's' : '-';
    buffer[2] = (prot & UFT_AMIGA_PROT_PURE) ? 'p' : '-';
    buffer[3] = (prot & UFT_AMIGA_PROT_ARCHIVE) ? 'a' : '-';
    /* RWED: 0 = allowed, 1 = denied */
    buffer[4] = (prot & UFT_AMIGA_PROT_READ) ? '-' : 'r';
    buffer[5] = (prot & UFT_AMIGA_PROT_WRITE) ? '-' : 'w';
    buffer[6] = (prot & UFT_AMIGA_PROT_EXECUTE) ? '-' : 'e';
    buffer[7] = (prot & UFT_AMIGA_PROT_DELETE) ? '-' : 'd';
    buffer[8] = '\0';
}

uint32_t uft_amiga_parse_protection(const char *str)
{
    uint32_t prot = 0;
    if (!str) return 0;
    
    for (const char *p = str; *p; p++) {
        switch (*p) {
            case 'h': case 'H': prot |= UFT_AMIGA_PROT_HOLD; break;
            case 's': case 'S': prot |= UFT_AMIGA_PROT_SCRIPT; break;
            case 'p': case 'P': prot |= UFT_AMIGA_PROT_PURE; break;
            case 'a': case 'A': prot |= UFT_AMIGA_PROT_ARCHIVE; break;
            case 'r': case 'R': /* readable = bit clear */ break;
            case 'w': case 'W': /* writable = bit clear */ break;
            case 'e': case 'E': /* executable = bit clear */ break;
            case 'd': case 'D': /* deletable = bit clear */ break;
        }
    }
    
    /* Set denied bits for unspecified RWED */
    if (!strchr(str, 'r') && !strchr(str, 'R')) prot |= UFT_AMIGA_PROT_READ;
    if (!strchr(str, 'w') && !strchr(str, 'W')) prot |= UFT_AMIGA_PROT_WRITE;
    if (!strchr(str, 'e') && !strchr(str, 'E')) prot |= UFT_AMIGA_PROT_EXECUTE;
    if (!strchr(str, 'd') && !strchr(str, 'D')) prot |= UFT_AMIGA_PROT_DELETE;
    
    return prot;
}

/*===========================================================================
 * Filesystem Type Helpers
 *===========================================================================*/

const char *uft_amiga_fs_type_str(uft_amiga_fs_type_t fs_type)
{
    switch (fs_type) {
        case UFT_AMIGA_FS_OFS:      return "OFS (DOS0)";
        case UFT_AMIGA_FS_FFS:      return "FFS (DOS1)";
        case UFT_AMIGA_FS_OFS_INTL: return "OFS+INTL (DOS2)";
        case UFT_AMIGA_FS_FFS_INTL: return "FFS+INTL (DOS3)";
        case UFT_AMIGA_FS_OFS_DC:   return "OFS+DirCache (DOS4)";
        case UFT_AMIGA_FS_FFS_DC:   return "FFS+DirCache (DOS5)";
        case UFT_AMIGA_FS_OFS_LNFS: return "OFS+LongNames (DOS6)";
        case UFT_AMIGA_FS_FFS_LNFS: return "FFS+LongNames (DOS7)";
        default:                    return "Unknown";
    }
}

uft_amiga_options_t uft_amiga_default_options(void)
{
    uft_amiga_options_t opt = {
        .verify_checksums = true,
        .auto_fix = false,
        .preserve_dates = true,
        .follow_links = true,
        .interleave = 1
    };
    return opt;
}

/*===========================================================================
 * Detection
 *===========================================================================*/

int uft_amiga_detect(const uint8_t *data, size_t size, uft_amiga_detect_t *result)
{
    if (!data || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    /* Minimum size: 2 boot blocks + root block area */
    if (size < 3 * UFT_AMIGA_BLOCK_SIZE) return -1;
    
    /* Check DOS type in bootblock */
    if (data[0] != 'D' || data[1] != 'O' || data[2] != 'S') {
        return -1;  /* Not AmigaDOS */
    }
    
    uint8_t dos_num = data[3];
    if (dos_num > 7) return -1;  /* Invalid DOS type */
    
    /* Parse DOS type */
    result->fs_type = (uft_amiga_fs_type_t)dos_num;
    result->is_ffs = (dos_num & 0x01) != 0;
    result->is_intl = (dos_num >= 2 && dos_num <= 5) ||
                      (dos_num >= 6 && dos_num <= 7);
    result->is_dircache = (dos_num == 4 || dos_num == 5);
    result->is_longnames = (dos_num >= 6);
    
    snprintf(result->dos_type, sizeof(result->dos_type), "DOS%d", dos_num);
    
    /* Calculate total blocks */
    result->total_blocks = (uint32_t)(size / UFT_AMIGA_BLOCK_SIZE);
    
    /* Root block is in the middle */
    result->root_block = result->total_blocks / 2;
    
    /* Verify bootblock checksum */
    result->bootblock_checksum = uft_amiga_bootblock_checksum(data);
    result->bootblock_valid = (result->bootblock_checksum == 0);
    
    /* Verify root block */
    if (result->root_block * UFT_AMIGA_BLOCK_SIZE + UFT_AMIGA_BLOCK_SIZE <= size) {
        const uint8_t *root = data + result->root_block * UFT_AMIGA_BLOCK_SIZE;
        
        uint32_t type = read_be32(root);
        int32_t sec_type = read_be32s(root + 508);
        
        if (type == UFT_AMIGA_T_SHORT && sec_type == UFT_AMIGA_ST_ROOT) {
            result->is_valid = true;
        }
    }
    
    return result->is_valid ? 0 : -1;
}

bool uft_amiga_is_adf(const char *filename)
{
    if (!filename) return false;
    
    FILE *f = fopen(filename, "rb");
    if (!f) return false;
    
    uint8_t header[4];
    size_t n = fread(header, 1, 4, f);
    fclose(f);
    
    if (n < 4) return false;
    
    return (header[0] == 'D' && header[1] == 'O' &&
            header[2] == 'S' && header[3] <= 7);
}

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

uft_amiga_ctx_t *uft_amiga_create(void)
{
    uft_amiga_ctx_t *ctx = calloc(1, sizeof(uft_amiga_ctx_t));
    if (!ctx) return NULL;
    
    ctx->verify_checksums = true;
    return ctx;
}

void uft_amiga_destroy(uft_amiga_ctx_t *ctx)
{
    if (!ctx) return;
    
    if (ctx->owns_data && ctx->data) {
        free(ctx->data);
    }
    
    free(ctx);
}

void uft_amiga_close(uft_amiga_ctx_t *ctx)
{
    if (!ctx) return;
    
    if (ctx->owns_data && ctx->data) {
        free(ctx->data);
        ctx->data = NULL;
    }
    
    ctx->is_valid = false;
    ctx->size = 0;
}

static int parse_root_block(uft_amiga_ctx_t *ctx)
{
    if (!ctx || !ctx->data) return -1;
    
    const uint8_t *root = ctx->data + ctx->root_block * UFT_AMIGA_BLOCK_SIZE;
    
    /* Verify type */
    uint32_t type = read_be32(root);
    int32_t sec_type = read_be32s(root + 508);
    
    if (type != UFT_AMIGA_T_SHORT || sec_type != UFT_AMIGA_ST_ROOT) {
        return -1;
    }
    
    /* Verify checksum */
    if (ctx->verify_checksums && !uft_amiga_verify_checksum(root)) {
        return -1;
    }
    
    /* Parse bitmap block pointers (offset 316-392, 25 entries) */
    ctx->bitmap_count = 0;
    for (int i = 0; i < UFT_AMIGA_MAX_BITMAP_BLOCKS; i++) {
        uint32_t bm_block = read_be32(root + 316 + i * 4);
        if (bm_block != 0 && bm_block < ctx->total_blocks) {
            ctx->bitmap_blocks[ctx->bitmap_count++] = bm_block;
        }
    }
    
    /* Volume name at offset 432 (BCPL string) */
    read_bcpl_string(root + 432, ctx->volume_name, UFT_AMIGA_MAX_FILENAME_LFS + 1);
    
    /* Timestamps */
    ctx->disk_days = read_be32(root + 420);
    ctx->disk_mins = read_be32(root + 424);
    ctx->disk_ticks = read_be32(root + 428);
    ctx->last_modified = uft_amiga_to_unix_time(ctx->disk_days,
                                                 ctx->disk_mins,
                                                 ctx->disk_ticks);
    
    /* Creation date (offset 484-496) */
    uint32_t c_days = read_be32(root + 484);
    uint32_t c_mins = read_be32(root + 488);
    uint32_t c_ticks = read_be32(root + 492);
    ctx->creation_date = uft_amiga_to_unix_time(c_days, c_mins, c_ticks);
    
    ctx->is_valid = true;
    return 0;
}

int uft_amiga_open_buffer(uft_amiga_ctx_t *ctx, uint8_t *data, size_t size,
                          bool copy, const uft_amiga_options_t *options)
{
    if (!ctx || !data || size < 3 * UFT_AMIGA_BLOCK_SIZE) return -1;
    
    /* Close any existing image */
    uft_amiga_close(ctx);
    
    /* Set options */
    if (options) {
        ctx->verify_checksums = options->verify_checksums;
        ctx->auto_fix = options->auto_fix;
        ctx->preserve_dates = options->preserve_dates;
    }
    
    /* Detect filesystem */
    uft_amiga_detect_t detect;
    if (uft_amiga_detect(data, size, &detect) != 0) {
        return -1;
    }
    
    /* Copy or reference data */
    if (copy) {
        ctx->data = malloc(size);
        if (!ctx->data) return -1;
        memcpy(ctx->data, data, size);
        ctx->owns_data = true;
    } else {
        ctx->data = data;
        ctx->owns_data = false;
    }
    
    ctx->size = size;
    ctx->fs_type = detect.fs_type;
    ctx->is_ffs = detect.is_ffs;
    ctx->is_intl = detect.is_intl;
    ctx->is_dircache = detect.is_dircache;
    ctx->is_longnames = detect.is_longnames;
    ctx->total_blocks = detect.total_blocks;
    ctx->root_block = detect.root_block;
    
    /* Parse root block */
    if (parse_root_block(ctx) != 0) {
        uft_amiga_close(ctx);
        return -1;
    }
    
    return 0;
}

int uft_amiga_open_file(uft_amiga_ctx_t *ctx, const char *filename,
                        const uft_amiga_options_t *options)
{
    if (!ctx || !filename) return -1;
    
    FILE *f = fopen(filename, "rb");
    if (!f) return -1;
    
    /* Get file size */
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    long fsize = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    if (fsize < 0 || fsize > 100 * 1024 * 1024) {  /* Max 100MB */
        fclose(f);
        return -1;
    }
    
    /* Allocate and read */
    uint8_t *data = malloc((size_t)fsize);
    if (!data) {
        fclose(f);
        return -1;
    }
    
    size_t n = fread(data, 1, (size_t)fsize, f);
    fclose(f);
    
    if (n != (size_t)fsize) {
        free(data);
        return -1;
    }
    
    int ret = uft_amiga_open_buffer(ctx, data, (size_t)fsize, false, options);
    if (ret != 0) {
        free(data);
        return ret;
    }
    
    ctx->owns_data = true;  /* We allocated the buffer */
    return 0;
}

int uft_amiga_save(const uft_amiga_ctx_t *ctx, const char *filename)
{
    if (!ctx || !filename || !ctx->data) return -1;
    
    FILE *f = fopen(filename, "wb");
    if (!f) return -1;
    
    size_t n = fwrite(ctx->data, 1, ctx->size, f);
    fclose(f);
    
    return (n == ctx->size) ? 0 : -1;
}

/*===========================================================================
 * Block Access
 *===========================================================================*/

int uft_amiga_read_block(const uft_amiga_ctx_t *ctx, uint32_t block_num,
                         uint8_t *buffer)
{
    if (!ctx || !ctx->data || !buffer) return -1;
    if (block_num >= ctx->total_blocks) return -1;
    
    size_t offset = (size_t)block_num * UFT_AMIGA_BLOCK_SIZE;
    memcpy(buffer, ctx->data + offset, UFT_AMIGA_BLOCK_SIZE);
    
    return 0;
}

int uft_amiga_write_block(uft_amiga_ctx_t *ctx, uint32_t block_num,
                          const uint8_t *buffer)
{
    if (!ctx || !ctx->data || !buffer) return -1;
    if (block_num >= ctx->total_blocks) return -1;
    
    size_t offset = (size_t)block_num * UFT_AMIGA_BLOCK_SIZE;
    memcpy(ctx->data + offset, buffer, UFT_AMIGA_BLOCK_SIZE);
    ctx->modified = true;
    
    return 0;
}

/*===========================================================================
 * Bootblock Functions
 *===========================================================================*/

int uft_amiga_read_bootblock(const uft_amiga_ctx_t *ctx,
                             uint8_t *block0, uint8_t *block1)
{
    if (!ctx || !ctx->data) return -1;
    
    if (block0) {
        memcpy(block0, ctx->data, UFT_AMIGA_BLOCK_SIZE);
    }
    if (block1) {
        memcpy(block1, ctx->data + UFT_AMIGA_BLOCK_SIZE, UFT_AMIGA_BLOCK_SIZE);
    }
    
    return 0;
}

int uft_amiga_write_bootblock(uft_amiga_ctx_t *ctx,
                              const uint8_t *block0, const uint8_t *block1)
{
    if (!ctx || !ctx->data) return -1;
    
    if (block0) {
        memcpy(ctx->data, block0, UFT_AMIGA_BLOCK_SIZE);
    }
    if (block1) {
        memcpy(ctx->data + UFT_AMIGA_BLOCK_SIZE, block1, UFT_AMIGA_BLOCK_SIZE);
    }
    ctx->modified = true;
    
    return 0;
}

bool uft_amiga_is_bootable(const uft_amiga_ctx_t *ctx)
{
    if (!ctx || !ctx->data) return false;
    
    /* Check for valid bootblock checksum */
    uint32_t checksum = uft_amiga_bootblock_checksum(ctx->data);
    if (checksum != 0) return false;
    
    /* Check for executable code (not just empty DOS header) */
    /* Bootable disks have code after the DOS header */
    for (int i = 12; i < 1024; i++) {
        if (ctx->data[i] != 0) return true;
    }
    
    return false;
}

int uft_amiga_make_bootable(uft_amiga_ctx_t *ctx)
{
    if (!ctx || !ctx->data) return -1;
    
    /* Standard Amiga 1.3 bootblock code */
    static const uint8_t boot_code[] = {
        0x43, 0xFA, 0x00, 0x18,  /* LEA.L    $00000018(PC),A1 */
        0x4E, 0xAE, 0xFF, 0xA0,  /* JSR      -$60(A6) [DoIO] */
        0x4A, 0x80,              /* TST.L    D0 */
        0x67, 0x0A,              /* BEQ.S    $0000001E */
        0x20, 0x40,              /* MOVEA.L  D0,A0 */
        0x20, 0x68, 0x00, 0x16,  /* MOVEA.L  $0016(A0),A0 */
        0x70, 0x00,              /* MOVEQ    #$00,D0 */
        0x4E, 0x75,              /* RTS */
        0x00, 0x00, 0x00, 0x00,  /* (padding) */
        0x00, 0x00, 0x03, 0x70   /* (track info) */
    };
    
    /* Preserve DOS type */
    uint8_t dos_type = ctx->data[3];
    
    /* Clear bootblock */
    memset(ctx->data + 4, 0, 1024 - 4);
    
    /* Write boot code at offset 12 */
    memcpy(ctx->data + 12, boot_code, sizeof(boot_code));
    
    /* Fix checksum */
    /* Clear checksum field at offset 4-7 */
    write_be32(ctx->data + 4, 0);
    
    /* Calculate checksum */
    uint32_t sum = 0;
    for (int i = 0; i < 1024; i += 4) {
        uint32_t val = read_be32(ctx->data + i);
        uint32_t prev = sum;
        sum += val;
        if (sum < prev) sum++;
    }
    
    /* Store complement */
    write_be32(ctx->data + 4, ~sum);
    
    ctx->modified = true;
    return 0;
}

/*===========================================================================
 * Hash Function
 *===========================================================================*/

uint32_t uft_amiga_hash_name(const char *name, bool intl)
{
    if (!name) return 0;
    
    uint32_t hash = (uint32_t)strlen(name);
    
    for (const char *p = name; *p; p++) {
        unsigned char c = (unsigned char)*p;
        
        if (intl) {
            /* International mode: extended case folding */
            if (c >= 'a' && c <= 'z') {
                c -= 32;
            } else if (c >= 0xE0 && c <= 0xFE && c != 0xF7) {
                c -= 32;  /* Accented lowercase to uppercase */
            }
        } else {
            /* Standard: ASCII case insensitive only */
            if (c >= 'a' && c <= 'z') {
                c -= 32;
            }
        }
        
        hash = (hash * 13 + c) & 0x7FF;
    }
    
    return hash % UFT_AMIGA_HASH_SIZE;
}

/*===========================================================================
 * JSON Report
 *===========================================================================*/

int uft_amiga_report_json(const uft_amiga_ctx_t *ctx, char *buffer, size_t size)
{
    if (!ctx || !buffer || size == 0) return -1;
    
    uft_amiga_bitmap_info_t bm_info = {0};
    uft_amiga_get_bitmap_info(ctx, &bm_info);
    
    char prot_str[16];
    uft_amiga_protection_str(0, prot_str);
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"filesystem\": \"%s\",\n"
        "  \"valid\": %s,\n"
        "  \"volume_name\": \"%s\",\n"
        "  \"total_blocks\": %u,\n"
        "  \"free_blocks\": %u,\n"
        "  \"used_blocks\": %u,\n"
        "  \"root_block\": %u,\n"
        "  \"features\": {\n"
        "    \"ffs\": %s,\n"
        "    \"international\": %s,\n"
        "    \"dircache\": %s,\n"
        "    \"longnames\": %s\n"
        "  },\n"
        "  \"size_bytes\": %zu,\n"
        "  \"bootable\": %s\n"
        "}",
        uft_amiga_fs_type_str(ctx->fs_type),
        ctx->is_valid ? "true" : "false",
        ctx->volume_name,
        ctx->total_blocks,
        bm_info.free_blocks,
        bm_info.used_blocks,
        ctx->root_block,
        ctx->is_ffs ? "true" : "false",
        ctx->is_intl ? "true" : "false",
        ctx->is_dircache ? "true" : "false",
        ctx->is_longnames ? "true" : "false",
        ctx->size,
        uft_amiga_is_bootable(ctx) ? "true" : "false"
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}
