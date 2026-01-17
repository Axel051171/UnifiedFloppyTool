/**
 * @file uft_switch.c
 * @brief Nintendo Switch Container Format Implementation
 * 
 * Based on hactool by SciresM
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/nintendo/uft_switch.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint32_t read_le32(const uint8_t *data)
{
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

static uint64_t read_le64(const uint8_t *data)
{
    return (uint64_t)read_le32(data) | ((uint64_t)read_le32(data + 4) << 32);
}

/* ============================================================================
 * Detection
 * ============================================================================ */

bool xci_detect(const uint8_t *data, size_t size)
{
    if (!data || size < 0x200) return false;
    
    /* XCI header magic is at offset 0x100 */
    uint32_t magic = read_le32(data + 0x100);
    return magic == XCI_MAGIC;
}

bool nsp_detect(const uint8_t *data, size_t size)
{
    if (!data || size < 0x10) return false;
    
    uint32_t magic = read_le32(data);
    return magic == PFS0_MAGIC;
}

bool nca_detect(const uint8_t *data, size_t size)
{
    if (!data || size < 0x400) return false;
    
    /* NCA magic is at offset 0x200 (after signatures) */
    /* But it's usually encrypted, so this is unreliable */
    uint32_t magic = read_le32(data + 0x200);
    
    /* Check for NCA0, NCA2, NCA3 */
    return (magic == 0x3041434E || magic == 0x3241434E || magic == 0x3341434E);
}

const char *xci_cart_size_name(xci_cart_size_t type)
{
    switch (type) {
        case XCI_CART_1GB:  return "1GB";
        case XCI_CART_2GB:  return "2GB";
        case XCI_CART_4GB:  return "4GB";
        case XCI_CART_8GB:  return "8GB";
        case XCI_CART_16GB: return "16GB";
        case XCI_CART_32GB: return "32GB";
        default: return "Unknown";
    }
}

const char *nca_content_type_name(nca_content_type_t type)
{
    switch (type) {
        case NCA_TYPE_PROGRAM:      return "Program";
        case NCA_TYPE_META:         return "Meta";
        case NCA_TYPE_CONTROL:      return "Control";
        case NCA_TYPE_MANUAL:       return "Manual";
        case NCA_TYPE_DATA:         return "Data";
        case NCA_TYPE_PUBLIC_DATA:  return "PublicData";
        default: return "Unknown";
    }
}

/* ============================================================================
 * Container Operations
 * ============================================================================ */

int switch_open(const uint8_t *data, size_t size, switch_ctx_t *ctx)
{
    if (!data || !ctx || size < 0x10) return -1;
    
    memset(ctx, 0, sizeof(switch_ctx_t));
    
    /* Detect format */
    if (xci_detect(data, size)) {
        ctx->is_xci = true;
    } else if (nsp_detect(data, size)) {
        ctx->is_xci = false;
    } else {
        return -2;  /* Unknown format */
    }
    
    /* Copy data */
    ctx->data = malloc(size);
    if (!ctx->data) return -3;
    
    memcpy(ctx->data, data, size);
    ctx->size = size;
    ctx->owns_data = true;
    
    /* Set up header pointers */
    if (ctx->is_xci) {
        ctx->xci_header = (xci_header_t *)(ctx->data);
    } else {
        ctx->pfs0_header = (pfs0_header_t *)(ctx->data);
    }
    
    return 0;
}

int switch_load(const char *filename, switch_ctx_t *ctx)
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
    
    int result = switch_open(data, size, ctx);
    free(data);
    
    return result;
}

void switch_close(switch_ctx_t *ctx)
{
    if (!ctx) return;
    
    if (ctx->owns_data) {
        free(ctx->data);
    }
    
    memset(ctx, 0, sizeof(switch_ctx_t));
}

/* ============================================================================
 * XCI Operations
 * ============================================================================ */

int xci_get_info(const switch_ctx_t *ctx, xci_info_t *info)
{
    if (!ctx || !info || !ctx->is_xci) return -1;
    
    memset(info, 0, sizeof(xci_info_t));
    
    xci_header_t *hdr = ctx->xci_header;
    
    info->cart_size = hdr->cart_size;
    info->cart_type = (xci_cart_size_t)hdr->cart_type;
    
    /* Parse HFS0 root partition to find sub-partitions */
    if (hdr->hfs0_offset > 0 && hdr->hfs0_offset < ctx->size) {
        uint8_t *hfs0 = ctx->data + hdr->hfs0_offset;
        uint32_t magic = read_le32(hfs0);
        
        if (magic == HFS0_MAGIC) {
            uint32_t num_files = read_le32(hfs0 + 4);
            uint32_t string_table_size = read_le32(hfs0 + 8);
            
            info->num_partitions = (num_files > 4) ? 4 : num_files;
            
            uint8_t *string_table = hfs0 + 16 + num_files * 0x40;
            
            for (int i = 0; i < info->num_partitions && i < 4; i++) {
                uint8_t *entry = hfs0 + 16 + i * 0x40;
                
                uint64_t offset = read_le64(entry);
                uint64_t size = read_le64(entry + 8);
                uint32_t str_offset = read_le32(entry + 16);
                
                info->partitions[i].offset = hdr->hfs0_offset + 
                    16 + num_files * 0x40 + string_table_size + offset;
                info->partitions[i].size = size;
                
                /* Copy partition name */
                const char *name = (const char *)(string_table + str_offset);
                strncpy(info->partitions[i].name, name, 63);
            }
        }
    }
    
    return 0;
}

int xci_get_partition_count(const switch_ctx_t *ctx)
{
    if (!ctx || !ctx->is_xci) return 0;
    
    xci_info_t info;
    if (xci_get_info(ctx, &info) != 0) return 0;
    
    return info.num_partitions;
}

int xci_get_partition(const switch_ctx_t *ctx, int index,
                      xci_partition_info_t *info)
{
    if (!ctx || !info || !ctx->is_xci) return -1;
    
    xci_info_t xci_info;
    if (xci_get_info(ctx, &xci_info) != 0) return -2;
    
    if (index < 0 || index >= xci_info.num_partitions) return -3;
    
    *info = xci_info.partitions[index];
    return 0;
}

int xci_list_files(const switch_ctx_t *ctx, const char *partition,
                   switch_file_entry_t *files, int max_files)
{
    if (!ctx || !partition || !files || !ctx->is_xci) return -1;
    
    xci_info_t info;
    if (xci_get_info(ctx, &info) != 0) return -2;
    
    /* Find partition */
    int part_idx = -1;
    for (int i = 0; i < info.num_partitions; i++) {
        if (strcmp(info.partitions[i].name, partition) == 0) {
            part_idx = i;
            break;
        }
    }
    
    if (part_idx < 0) return -3;
    
    /* Parse HFS0 in partition */
    uint64_t part_offset = info.partitions[part_idx].offset;
    if (part_offset >= ctx->size) return -4;
    
    uint8_t *hfs0 = ctx->data + part_offset;
    uint32_t magic = read_le32(hfs0);
    
    if (magic != HFS0_MAGIC) return -5;
    
    uint32_t num_files = read_le32(hfs0 + 4);
    uint32_t string_table_size = read_le32(hfs0 + 8);
    
    int count = (num_files > (uint32_t)max_files) ? max_files : num_files;
    
    uint8_t *string_table = hfs0 + 16 + num_files * 0x40;
    uint64_t data_offset = part_offset + 16 + num_files * 0x40 + string_table_size;
    
    for (int i = 0; i < count; i++) {
        uint8_t *entry = hfs0 + 16 + i * 0x40;
        
        files[i].offset = data_offset + read_le64(entry);
        files[i].size = read_le64(entry + 8);
        
        uint32_t str_offset = read_le32(entry + 16);
        const char *name = (const char *)(string_table + str_offset);
        strncpy(files[i].name, name, 255);
    }
    
    return count;
}

/* ============================================================================
 * NSP Operations
 * ============================================================================ */

int nsp_get_info(const switch_ctx_t *ctx, nsp_info_t *info)
{
    if (!ctx || !info || ctx->is_xci) return -1;
    
    memset(info, 0, sizeof(nsp_info_t));
    
    pfs0_header_t *hdr = ctx->pfs0_header;
    
    info->num_files = hdr->num_files;
    info->total_size = ctx->size;
    
    return 0;
}

int nsp_get_file_count(const switch_ctx_t *ctx)
{
    if (!ctx || ctx->is_xci) return 0;
    return ctx->pfs0_header->num_files;
}

int nsp_get_file(const switch_ctx_t *ctx, int index,
                 switch_file_entry_t *entry)
{
    if (!ctx || !entry || ctx->is_xci) return -1;
    
    pfs0_header_t *hdr = ctx->pfs0_header;
    
    if (index < 0 || (uint32_t)index >= hdr->num_files) return -2;
    
    /* File entries start after header */
    pfs0_file_entry_t *file_entry = (pfs0_file_entry_t *)(ctx->data + 16 + 
                                                          index * sizeof(pfs0_file_entry_t));
    
    /* String table after all file entries */
    char *string_table = (char *)(ctx->data + 16 + 
                                  hdr->num_files * sizeof(pfs0_file_entry_t));
    
    /* Data starts after string table */
    uint64_t data_offset = 16 + hdr->num_files * sizeof(pfs0_file_entry_t) + 
                           hdr->string_table_size;
    
    entry->offset = data_offset + file_entry->offset;
    entry->size = file_entry->size;
    strncpy(entry->name, string_table + file_entry->string_offset, 255);
    
    return 0;
}

int nsp_extract_file(const switch_ctx_t *ctx, int index,
                     uint8_t *buffer, size_t max_size, size_t *extracted)
{
    if (!ctx || !buffer || ctx->is_xci) return -1;
    
    switch_file_entry_t entry;
    if (nsp_get_file(ctx, index, &entry) != 0) return -2;
    
    if (entry.offset + entry.size > ctx->size) return -3;
    
    size_t to_copy = (entry.size > max_size) ? max_size : entry.size;
    memcpy(buffer, ctx->data + entry.offset, to_copy);
    
    if (extracted) *extracted = to_copy;
    return 0;
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

void xci_print_info(const switch_ctx_t *ctx, FILE *fp)
{
    if (!ctx || !fp || !ctx->is_xci) return;
    
    xci_info_t info;
    if (xci_get_info(ctx, &info) != 0) return;
    
    fprintf(fp, "Nintendo Switch XCI:\n");
    fprintf(fp, "  Cartridge Size: %s\n", xci_cart_size_name(info.cart_type));
    fprintf(fp, "  File Size: %zu bytes\n", ctx->size);
    fprintf(fp, "  Partitions: %d\n", info.num_partitions);
    
    for (int i = 0; i < info.num_partitions; i++) {
        fprintf(fp, "    %d: %s (offset 0x%lX, size %lu bytes)\n",
                i, info.partitions[i].name,
                (unsigned long)info.partitions[i].offset,
                (unsigned long)info.partitions[i].size);
    }
}

void nsp_print_info(const switch_ctx_t *ctx, FILE *fp)
{
    if (!ctx || !fp || ctx->is_xci) return;
    
    nsp_info_t info;
    if (nsp_get_info(ctx, &info) != 0) return;
    
    fprintf(fp, "Nintendo Switch NSP:\n");
    fprintf(fp, "  File Size: %lu bytes\n", (unsigned long)info.total_size);
    fprintf(fp, "  Files: %d\n", info.num_files);
    
    for (int i = 0; i < info.num_files && i < 20; i++) {
        switch_file_entry_t entry;
        if (nsp_get_file(ctx, i, &entry) == 0) {
            fprintf(fp, "    %d: %s (%lu bytes)\n", 
                    i, entry.name, (unsigned long)entry.size);
        }
    }
    
    if (info.num_files > 20) {
        fprintf(fp, "    ... and %d more files\n", info.num_files - 20);
    }
}

void switch_title_id_str(uint64_t title_id, char *buffer)
{
    if (!buffer) return;
    sprintf(buffer, "%016llX", (unsigned long long)title_id);
}
