/**
 * @file uft_3ds.c
 * @brief Nintendo 3DS Container Format Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/nintendo/uft_3ds.h"
#include <stdlib.h>
#include <string.h>

/* Media unit size (512 bytes) */
#define MEDIA_UNIT_SIZE     0x200

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint32_t read_le32(const uint8_t *data)
{
    return data[0] | (data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static uint64_t read_le64(const uint8_t *data)
{
    return (uint64_t)read_le32(data) | ((uint64_t)read_le32(data + 4) << 32);
}

/* ============================================================================
 * Detection
 * ============================================================================ */

bool n3ds_detect_cci(const uint8_t *data, size_t size)
{
    if (!data || size < 0x200) return false;
    
    /* NCSD magic at offset 0x100 */
    uint32_t magic = read_le32(data + 0x100);
    return magic == NCSD_MAGIC;
}

bool n3ds_detect_cia(const uint8_t *data, size_t size)
{
    if (!data || size < 0x20) return false;
    
    /* CIA header size check */
    uint32_t header_size = read_le32(data);
    return (header_size == 0x2020);
}

bool n3ds_detect_ncch(const uint8_t *data, size_t size)
{
    if (!data || size < 0x200) return false;
    
    /* NCCH magic at offset 0x100 */
    uint32_t magic = read_le32(data + 0x100);
    return magic == NCCH_MAGIC;
}

bool n3ds_is_encrypted(const ncch_header_t *ncch)
{
    if (!ncch) return true;
    
    /* Flag 7 bit 0: no crypto */
    /* Flag 3: crypto method (0=fixed, 1-10=keyslot) */
    return !((ncch->flags[7] & 0x04) != 0);
}

/* ============================================================================
 * Container Operations
 * ============================================================================ */

int n3ds_open(const uint8_t *data, size_t size, n3ds_ctx_t *ctx)
{
    if (!data || !ctx || size < 0x200) return -1;
    
    memset(ctx, 0, sizeof(n3ds_ctx_t));
    
    /* Detect format */
    if (n3ds_detect_cci(data, size)) {
        ctx->is_cci = true;
    } else if (n3ds_detect_cia(data, size)) {
        ctx->is_cci = false;
    } else if (n3ds_detect_ncch(data, size)) {
        /* Standalone NCCH */
        ctx->is_cci = false;
    } else {
        return -2;
    }
    
    /* Copy data */
    ctx->data = malloc(size);
    if (!ctx->data) return -3;
    
    memcpy(ctx->data, data, size);
    ctx->size = size;
    ctx->owns_data = true;
    
    /* Set up header pointers */
    if (ctx->is_cci) {
        ctx->ncsd = (ncsd_header_t *)ctx->data;
        
        /* First partition NCCH */
        if (ctx->ncsd->partitions[0].size > 0) {
            size_t ncch_offset = ctx->ncsd->partitions[0].offset * MEDIA_UNIT_SIZE;
            if (ncch_offset + 0x200 <= size) {
                ctx->ncch = (ncch_header_t *)(ctx->data + ncch_offset);
            }
        }
    } else if (n3ds_detect_cia(data, size)) {
        ctx->cia = (cia_header_t *)ctx->data;
        
        /* Find NCCH after CIA header + certificate + ticket + TMD */
        uint32_t cert_offset = ((ctx->cia->header_size + CIA_ALIGN - 1) / CIA_ALIGN) * CIA_ALIGN;
        uint32_t ticket_offset = cert_offset + ((ctx->cia->cert_size + CIA_ALIGN - 1) / CIA_ALIGN) * CIA_ALIGN;
        uint32_t tmd_offset = ticket_offset + ((ctx->cia->ticket_size + CIA_ALIGN - 1) / CIA_ALIGN) * CIA_ALIGN;
        uint32_t content_offset = tmd_offset + ((ctx->cia->tmd_size + CIA_ALIGN - 1) / CIA_ALIGN) * CIA_ALIGN;
        
        if (content_offset + 0x200 <= size) {
            if (n3ds_detect_ncch(ctx->data + content_offset, size - content_offset)) {
                ctx->ncch = (ncch_header_t *)(ctx->data + content_offset);
            }
        }
    } else {
        /* Standalone NCCH */
        ctx->ncch = (ncch_header_t *)ctx->data;
    }
    
    return 0;
}

int n3ds_load(const char *filename, n3ds_ctx_t *ctx)
{
    if (!filename || !ctx) return -1;
    
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
    
    int result = n3ds_open(data, size, ctx);
    free(data);
    
    return result;
}

void n3ds_close(n3ds_ctx_t *ctx)
{
    if (!ctx) return;
    
    if (ctx->owns_data) {
        free(ctx->data);
    }
    
    memset(ctx, 0, sizeof(n3ds_ctx_t));
}

int n3ds_get_info(const n3ds_ctx_t *ctx, n3ds_info_t *info)
{
    if (!ctx || !info) return -1;
    
    memset(info, 0, sizeof(n3ds_info_t));
    
    info->is_cci = ctx->is_cci;
    info->file_size = ctx->size;
    
    if (ctx->ncch) {
        info->title_id = ctx->ncch->program_id;
        memcpy(info->product_code, ctx->ncch->product_code, 16);
        info->product_code[16] = '\0';
        info->maker_code = ctx->ncch->maker_code;
        info->has_exefs = (ctx->ncch->exefs_size > 0);
        info->has_romfs = (ctx->ncch->romfs_size > 0);
        info->encrypted = n3ds_is_encrypted(ctx->ncch);
    }
    
    if (ctx->is_cci && ctx->ncsd) {
        info->num_partitions = 0;
        for (int i = 0; i < 8; i++) {
            if (ctx->ncsd->partitions[i].size > 0) {
                info->num_partitions++;
            }
        }
    } else {
        info->num_partitions = 1;
    }
    
    return 0;
}

/* ============================================================================
 * Partition Access
 * ============================================================================ */

int n3ds_get_partition_count(const n3ds_ctx_t *ctx)
{
    if (!ctx) return 0;
    
    if (!ctx->is_cci || !ctx->ncsd) return 1;
    
    int count = 0;
    for (int i = 0; i < 8; i++) {
        if (ctx->ncsd->partitions[i].size > 0) {
            count++;
        }
    }
    
    return count;
}

int n3ds_get_partition(const n3ds_ctx_t *ctx, int index, ncch_header_t *ncch)
{
    if (!ctx || !ncch) return -1;
    
    if (!ctx->is_cci || !ctx->ncsd) {
        if (index == 0 && ctx->ncch) {
            memcpy(ncch, ctx->ncch, sizeof(ncch_header_t));
            return 0;
        }
        return -2;
    }
    
    if (index < 0 || index >= 8) return -3;
    if (ctx->ncsd->partitions[index].size == 0) return -4;
    
    size_t offset = ctx->ncsd->partitions[index].offset * MEDIA_UNIT_SIZE;
    if (offset + sizeof(ncch_header_t) > ctx->size) return -5;
    
    memcpy(ncch, ctx->data + offset, sizeof(ncch_header_t));
    return 0;
}

int n3ds_get_partition_bounds(const n3ds_ctx_t *ctx, int index,
                               size_t *offset, size_t *size)
{
    if (!ctx) return -1;
    
    if (!ctx->is_cci || !ctx->ncsd) {
        if (index == 0) {
            if (offset) *offset = 0;
            if (size) *size = ctx->size;
            return 0;
        }
        return -2;
    }
    
    if (index < 0 || index >= 8) return -3;
    if (ctx->ncsd->partitions[index].size == 0) return -4;
    
    if (offset) *offset = ctx->ncsd->partitions[index].offset * MEDIA_UNIT_SIZE;
    if (size) *size = ctx->ncsd->partitions[index].size * MEDIA_UNIT_SIZE;
    
    return 0;
}

/* ============================================================================
 * ExeFS Access
 * ============================================================================ */

static exefs_header_t *get_exefs_header(const n3ds_ctx_t *ctx)
{
    if (!ctx || !ctx->ncch) return NULL;
    if (ctx->ncch->exefs_size == 0) return NULL;
    
    /* Calculate ExeFS offset */
    size_t ncch_base = 0;
    if (ctx->is_cci && ctx->ncsd) {
        ncch_base = ctx->ncsd->partitions[0].offset * MEDIA_UNIT_SIZE;
    }
    
    size_t exefs_offset = ncch_base + ctx->ncch->exefs_offset * MEDIA_UNIT_SIZE;
    if (exefs_offset + sizeof(exefs_header_t) > ctx->size) return NULL;
    
    return (exefs_header_t *)(ctx->data + exefs_offset);
}

int n3ds_exefs_file_count(const n3ds_ctx_t *ctx)
{
    exefs_header_t *exefs = get_exefs_header(ctx);
    if (!exefs) return 0;
    
    int count = 0;
    for (int i = 0; i < 10; i++) {
        if (exefs->files[i].name[0] != '\0' && exefs->files[i].size > 0) {
            count++;
        }
    }
    
    return count;
}

int n3ds_exefs_get_file(const n3ds_ctx_t *ctx, int index,
                         char *name, size_t *size)
{
    exefs_header_t *exefs = get_exefs_header(ctx);
    if (!exefs) return -1;
    
    int current = 0;
    for (int i = 0; i < 10; i++) {
        if (exefs->files[i].name[0] != '\0' && exefs->files[i].size > 0) {
            if (current == index) {
                if (name) {
                    memcpy(name, exefs->files[i].name, 8);
                }
                if (size) {
                    *size = exefs->files[i].size;
                }
                return 0;
            }
            current++;
        }
    }
    
    return -2;
}

int n3ds_exefs_extract(const n3ds_ctx_t *ctx, const char *name,
                        uint8_t *buffer, size_t max_size, size_t *extracted)
{
    if (!ctx || !name || !buffer) return -1;
    
    exefs_header_t *exefs = get_exefs_header(ctx);
    if (!exefs) return -2;
    
    /* Find file */
    for (int i = 0; i < 10; i++) {
        if (strncmp(exefs->files[i].name, name, 8) == 0 && 
            exefs->files[i].size > 0) {
            
            /* Calculate data offset */
            size_t ncch_base = 0;
            if (ctx->is_cci && ctx->ncsd) {
                ncch_base = ctx->ncsd->partitions[0].offset * MEDIA_UNIT_SIZE;
            }
            
            size_t exefs_base = ncch_base + ctx->ncch->exefs_offset * MEDIA_UNIT_SIZE;
            size_t file_offset = exefs_base + 0x200 + exefs->files[i].offset;
            size_t file_size = exefs->files[i].size;
            
            if (file_offset + file_size > ctx->size) return -3;
            
            size_t to_copy = (file_size > max_size) ? max_size : file_size;
            memcpy(buffer, ctx->data + file_offset, to_copy);
            
            if (extracted) *extracted = to_copy;
            return 0;
        }
    }
    
    return -4;  /* Not found */
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

void n3ds_title_id_str(uint64_t title_id, char *buffer)
{
    if (!buffer) return;
    snprintf(buffer, 20, "%016llX", (unsigned long long)title_id);
}

void n3ds_print_info(const n3ds_ctx_t *ctx, FILE *fp)
{
    if (!ctx || !fp) return;
    
    n3ds_info_t info;
    n3ds_get_info(ctx, &info);
    
    fprintf(fp, "Nintendo 3DS Image:\n");
    fprintf(fp, "  Format: %s\n", info.is_cci ? "CCI/3DS (Gamecard)" : "CIA/NCCH");
    fprintf(fp, "  File Size: %zu bytes (%.1f MB)\n", 
            info.file_size, info.file_size / 1048576.0);
    
    if (info.product_code[0]) {
        fprintf(fp, "  Product Code: %s\n", info.product_code);
    }
    
    char title_str[20];
    n3ds_title_id_str(info.title_id, title_str);
    fprintf(fp, "  Title ID: %s\n", title_str);
    fprintf(fp, "  Maker Code: %04X\n", info.maker_code);
    fprintf(fp, "  Partitions: %d\n", info.num_partitions);
    fprintf(fp, "  ExeFS: %s\n", info.has_exefs ? "Yes" : "No");
    fprintf(fp, "  RomFS: %s\n", info.has_romfs ? "Yes" : "No");
    fprintf(fp, "  Encrypted: %s\n", info.encrypted ? "Yes" : "No");
}

void n3ds_print_exefs(const n3ds_ctx_t *ctx, FILE *fp)
{
    if (!ctx || !fp) return;
    
    exefs_header_t *exefs = get_exefs_header(ctx);
    if (!exefs) {
        fprintf(fp, "No ExeFS found\n");
        return;
    }
    
    fprintf(fp, "ExeFS Contents:\n");
    
    for (int i = 0; i < 10; i++) {
        if (exefs->files[i].name[0] != '\0' && exefs->files[i].size > 0) {
            char name[9] = {0};
            memcpy(name, exefs->files[i].name, 8);
            fprintf(fp, "  %-8s  offset=0x%08X  size=%u bytes\n",
                    name, exefs->files[i].offset, exefs->files[i].size);
        }
    }
}
