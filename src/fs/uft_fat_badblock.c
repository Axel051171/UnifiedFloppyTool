/**
 * @file uft_fat_badblock.c
 * @brief FAT Bad Block Management Implementation
 */

#include "uft/fs/uft_fat_badblock.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

/*===========================================================================
 * List Management
 *===========================================================================*/

uft_badblock_list_t *uft_badblock_list_create(void)
{
    uft_badblock_list_t *list = calloc(1, sizeof(uft_badblock_list_t));
    if (!list) return NULL;
    
    list->capacity = 64;
    list->entries = calloc(list->capacity, sizeof(uft_badblock_entry_t));
    if (!list->entries) {
        free(list);
        return NULL;
    }
    
    list->default_unit = UFT_BADBLOCK_SECTOR;
    return list;
}

void uft_badblock_list_destroy(uft_badblock_list_t *list)
{
    if (!list) return;
    free(list->entries);
    free(list);
}

void uft_badblock_list_clear(uft_badblock_list_t *list)
{
    if (!list) return;
    list->count = 0;
}

static int list_grow(uft_badblock_list_t *list)
{
    if (list->count < list->capacity) return 0;
    
    size_t new_cap = list->capacity * 2;
    if (new_cap > UFT_BADBLOCK_MAX_ENTRIES) {
        new_cap = UFT_BADBLOCK_MAX_ENTRIES;
        if (list->count >= new_cap) return -1;
    }
    
    uft_badblock_entry_t *new_entries = realloc(list->entries,
                                                new_cap * sizeof(uft_badblock_entry_t));
    if (!new_entries) return -1;
    
    list->entries = new_entries;
    list->capacity = new_cap;
    return 0;
}

int uft_badblock_list_add(uft_badblock_list_t *list, uint64_t location,
                          uft_badblock_unit_t unit)
{
    if (!list) return -1;
    
    uft_badblock_entry_t entry = {
        .location = location,
        .unit = unit,
        .source = UFT_BADBLOCK_SRC_MANUAL,
        .cluster = 0,
        .marked_in_fat = false
    };
    
    return uft_badblock_list_add_entry(list, &entry);
}

int uft_badblock_list_add_entry(uft_badblock_list_t *list,
                                const uft_badblock_entry_t *entry)
{
    if (!list || !entry) return -1;
    if (list_grow(list) != 0) return -2;
    
    list->entries[list->count++] = *entry;
    return 0;
}

int uft_badblock_list_remove(uft_badblock_list_t *list, size_t index)
{
    if (!list || index >= list->count) return -1;
    
    /* Shift remaining entries */
    memmove(&list->entries[index], &list->entries[index + 1],
            (list->count - index - 1) * sizeof(uft_badblock_entry_t));
    list->count--;
    
    return 0;
}

static int entry_compare(const void *a, const void *b)
{
    const uft_badblock_entry_t *ea = (const uft_badblock_entry_t *)a;
    const uft_badblock_entry_t *eb = (const uft_badblock_entry_t *)b;
    
    if (ea->location < eb->location) return -1;
    if (ea->location > eb->location) return 1;
    return 0;
}

void uft_badblock_list_sort(uft_badblock_list_t *list)
{
    if (!list || list->count < 2) return;
    qsort(list->entries, list->count, sizeof(uft_badblock_entry_t), entry_compare);
}

size_t uft_badblock_list_dedupe(uft_badblock_list_t *list)
{
    if (!list || list->count < 2) return 0;
    
    uft_badblock_list_sort(list);
    
    size_t removed = 0;
    size_t write = 1;
    
    for (size_t read = 1; read < list->count; read++) {
        if (list->entries[read].location != list->entries[write - 1].location ||
            list->entries[read].unit != list->entries[write - 1].unit) {
            if (write != read) {
                list->entries[write] = list->entries[read];
            }
            write++;
        } else {
            removed++;
        }
    }
    
    list->count = write;
    return removed;
}

/*===========================================================================
 * File Import/Export
 *===========================================================================*/

int uft_badblock_import_stream(uft_badblock_list_t *list, FILE *fp,
                               uft_badblock_unit_t unit)
{
    if (!list || !fp) return -1;
    
    char line[256];
    int line_num = 0;
    int entries_added = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        
        /* Skip comments and empty lines */
        char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '#' || *p == '\0' || *p == '\n') continue;
        
        /* Parse number (decimal or hex) */
        char *endp;
        unsigned long long value;
        
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            value = strtoull(p, &endp, 16);
        } else {
            value = strtoull(p, &endp, 10);
        }
        
        if (endp == p) {
            /* Parse error - skip line */
            continue;
        }
        
        uft_badblock_entry_t entry = {
            .location = value,
            .unit = unit,
            .source = UFT_BADBLOCK_SRC_FILE,
            .cluster = 0,
            .marked_in_fat = false
        };
        
        if (uft_badblock_list_add_entry(list, &entry) == 0) {
            entries_added++;
        }
    }
    
    return entries_added;
}

int uft_badblock_import_file(uft_badblock_list_t *list, const char *filename,
                             uft_badblock_unit_t unit)
{
    if (!list || !filename) return -1;
    
    FILE *fp = fopen(filename, "r");
    if (!fp) return -2;
    
    int result = uft_badblock_import_stream(list, fp, unit);
    fclose(fp);
    
    return result;
}

int uft_badblock_import_buffer(uft_badblock_list_t *list, const char *data,
                               size_t size, uft_badblock_unit_t unit)
{
    if (!list || !data) return -1;
    
    /* Create temporary file-like parsing */
    const char *p = data;
    const char *end = data + size;
    int entries_added = 0;
    
    while (p < end) {
        /* Skip whitespace */
        while (p < end && isspace((unsigned char)*p)) p++;
        if (p >= end) break;
        
        /* Skip comments */
        if (*p == '#') {
            while (p < end && *p != '\n') p++;
            continue;
        }
        
        /* Parse number */
        char *endp;
        unsigned long long value;
        
        if (p[0] == '0' && p + 1 < end && (p[1] == 'x' || p[1] == 'X')) {
            value = strtoull(p, &endp, 16);
        } else {
            value = strtoull(p, &endp, 10);
        }
        
        if (endp > p) {
            uft_badblock_entry_t entry = {
                .location = value,
                .unit = unit,
                .source = UFT_BADBLOCK_SRC_FILE,
                .cluster = 0,
                .marked_in_fat = false
            };
            
            if (uft_badblock_list_add_entry(list, &entry) == 0) {
                entries_added++;
            }
            p = endp;
        } else {
            p++;
        }
    }
    
    return entries_added;
}

int uft_badblock_export_stream(const uft_badblock_list_t *list, FILE *fp,
                               uft_badblock_unit_t unit)
{
    if (!list || !fp) return -1;
    
    fprintf(fp, "# Bad block list\n");
    fprintf(fp, "# Unit: %s\n", uft_badblock_unit_str(unit));
    fprintf(fp, "# Count: %zu\n\n", list->count);
    
    for (size_t i = 0; i < list->count; i++) {
        const uft_badblock_entry_t *e = &list->entries[i];
        
        /* Convert to requested unit if needed */
        uint64_t value = e->location;
        
        /* Note: Full conversion would need FAT context */
        if (e->unit == unit) {
            fprintf(fp, "%llu\n", (unsigned long long)value);
        } else {
            /* Output in original unit with comment */
            fprintf(fp, "%llu  # originally %s\n", 
                    (unsigned long long)value,
                    uft_badblock_unit_str(e->unit));
        }
    }
    
    return 0;
}

int uft_badblock_export_file(const uft_badblock_list_t *list, 
                             const char *filename, uft_badblock_unit_t unit)
{
    if (!list || !filename) return -1;
    
    FILE *fp = fopen(filename, "w");
    if (!fp) return -2;
    
    int result = uft_badblock_export_stream(list, fp, unit);
    fclose(fp);
    
    return result;
}

/*===========================================================================
 * FAT Integration
 *===========================================================================*/

size_t uft_badblock_read_from_fat(uft_badblock_list_t *list,
                                  const uft_fat_ctx_t *ctx)
{
    if (!list || !ctx) return 0;
    
    const uft_fat_volume_t *vol = uft_fat_get_volume(ctx);
    if (!vol) return 0;
    
    size_t found = 0;
    
    for (uint32_t cluster = 2; cluster < vol->data_clusters + 2; cluster++) {
        if (uft_fat_cluster_is_bad(ctx, cluster)) {
            uft_badblock_entry_t entry = {
                .location = cluster,
                .unit = UFT_BADBLOCK_CLUSTER,
                .source = UFT_BADBLOCK_SRC_FAT,
                .cluster = cluster,
                .marked_in_fat = true
            };
            
            if (uft_badblock_list_add_entry(list, &entry) == 0) {
                found++;
            }
        }
    }
    
    return found;
}

size_t uft_badblock_mark_in_fat(const uft_badblock_list_t *list,
                                uft_fat_ctx_t *ctx,
                                uft_badblock_stats_t *stats)
{
    if (!list || !ctx) return 0;
    
    if (stats) memset(stats, 0, sizeof(*stats));
    
    size_t marked = 0;
    
    for (size_t i = 0; i < list->count; i++) {
        const uft_badblock_entry_t *e = &list->entries[i];
        
        /* Convert to cluster */
        uint32_t cluster = 0;
        
        switch (e->unit) {
            case UFT_BADBLOCK_CLUSTER:
                cluster = (uint32_t)e->location;
                break;
            case UFT_BADBLOCK_SECTOR:
                cluster = uft_badblock_sector_to_cluster(ctx, e->location);
                break;
            case UFT_BADBLOCK_BYTE_OFFSET:
                cluster = uft_badblock_offset_to_cluster(ctx, e->location);
                break;
            case UFT_BADBLOCK_BLOCK_1K:
                cluster = uft_badblock_block_to_cluster(ctx, e->location);
                break;
        }
        
        if (cluster >= 2) {
            /* Check if already bad */
            if (uft_fat_cluster_is_bad(ctx, cluster)) {
                if (stats) stats->already_marked++;
            } else {
                /* Mark as bad */
                uint32_t bad_value;
                const uft_fat_volume_t *vol = uft_fat_get_volume(ctx);
                if (vol->fat_type == UFT_FAT_TYPE_FAT12) {
                    bad_value = UFT_FAT12_BAD;
                } else if (vol->fat_type == UFT_FAT_TYPE_FAT16) {
                    bad_value = UFT_FAT16_BAD;
                } else {
                    bad_value = 0x0FFFFFF7;  /* FAT32 */
                }
                
                if (uft_fat_set_entry(ctx, cluster, bad_value) == 0) {
                    marked++;
                }
            }
            
            if (stats) {
                stats->total_bad++;
                stats->clusters_affected++;
            }
        }
    }
    
    if (stats) {
        stats->in_data_area = marked;
        stats->needs_marking = marked;
    }
    
    return marked;
}

size_t uft_badblock_unmark_in_fat(const uft_badblock_list_t *list,
                                  uft_fat_ctx_t *ctx)
{
    if (!list || !ctx) return 0;
    
    size_t unmarked = 0;
    
    for (size_t i = 0; i < list->count; i++) {
        const uft_badblock_entry_t *e = &list->entries[i];
        
        uint32_t cluster = 0;
        if (e->unit == UFT_BADBLOCK_CLUSTER) {
            cluster = (uint32_t)e->location;
        } else {
            cluster = e->cluster;
        }
        
        if (cluster >= 2 && uft_fat_cluster_is_bad(ctx, cluster)) {
            /* Mark as free */
            uint32_t free_value = 0;  /* Works for FAT12/16/32 */
            if (uft_fat_set_entry(ctx, cluster, free_value) == 0) {
                unmarked++;
            }
        }
    }
    
    return unmarked;
}

int uft_badblock_analyze(const uft_badblock_list_t *list,
                         const uft_fat_ctx_t *ctx,
                         uft_badblock_stats_t *stats)
{
    if (!list || !ctx || !stats) return -1;
    
    memset(stats, 0, sizeof(*stats));
    
    const uft_fat_volume_t *vol = uft_fat_get_volume(ctx);
    if (!vol) return -2;
    
    for (size_t i = 0; i < list->count; i++) {
        const uft_badblock_entry_t *e = &list->entries[i];
        
        stats->total_bad++;
        
        /* Determine location in filesystem */
        uint64_t sector = 0;
        
        switch (e->unit) {
            case UFT_BADBLOCK_SECTOR:
                sector = e->location;
                break;
            case UFT_BADBLOCK_BYTE_OFFSET:
                sector = e->location / vol->bytes_per_sector;
                break;
            case UFT_BADBLOCK_BLOCK_1K:
                sector = (e->location * 1024) / vol->bytes_per_sector;
                break;
            case UFT_BADBLOCK_CLUSTER:
                /* Calculate sector from cluster */
                if (e->location >= 2) {
                    sector = vol->data_start_sector + 
                             (e->location - 2) * vol->sectors_per_cluster;
                }
                break;
        }
        
        /* Classify by region */
        if (sector < vol->reserved_sectors) {
            stats->in_reserved++;
        } else if (sector < vol->fat_start_sector + vol->fat_size * vol->num_fats) {
            stats->in_fat++;
        } else if (sector < vol->data_start_sector) {
            stats->in_root_dir++;
        } else {
            stats->in_data_area++;
            
            /* Check if already marked */
            uint32_t cluster = 2 + (sector - vol->data_start_sector) / vol->sectors_per_cluster;
            if (uft_fat_cluster_is_bad(ctx, cluster)) {
                stats->already_marked++;
            } else {
                stats->needs_marking++;
            }
        }
        
        stats->bytes_affected += vol->bytes_per_sector;
    }
    
    stats->clusters_affected = stats->in_data_area;
    
    return 0;
}

/*===========================================================================
 * Conversion
 *===========================================================================*/

uint32_t uft_badblock_sector_to_cluster(const uft_fat_ctx_t *ctx, 
                                        uint64_t sector)
{
    if (!ctx) return 0;
    
    const uft_fat_volume_t *vol = uft_fat_get_volume(ctx);
    if (!vol) return 0;
    
    if (sector < vol->data_start_sector) return 0;  /* In system area */
    
    uint64_t data_sector = sector - vol->data_start_sector;
    return 2 + (uint32_t)(data_sector / vol->sectors_per_cluster);
}

uint32_t uft_badblock_offset_to_cluster(const uft_fat_ctx_t *ctx,
                                        uint64_t offset)
{
    if (!ctx) return 0;
    
    const uft_fat_volume_t *vol = uft_fat_get_volume(ctx);
    if (!vol) return 0;
    
    return uft_badblock_sector_to_cluster(ctx, offset / vol->bytes_per_sector);
}

uint32_t uft_badblock_block_to_cluster(const uft_fat_ctx_t *ctx,
                                       uint64_t block)
{
    if (!ctx) return 0;
    
    const uft_fat_volume_t *vol = uft_fat_get_volume(ctx);
    if (!vol) return 0;
    
    /* 1KB blocks as used by mkfs.fat */
    uint64_t byte_offset = block * 1024;
    return uft_badblock_offset_to_cluster(ctx, byte_offset);
}

int uft_badblock_cluster_to_sectors(const uft_fat_ctx_t *ctx,
                                    uint32_t cluster,
                                    uint64_t *first_sector,
                                    uint32_t *sector_count)
{
    if (!ctx || cluster < 2) return -1;
    
    const uft_fat_volume_t *vol = uft_fat_get_volume(ctx);
    if (!vol) return -2;
    
    if (first_sector) {
        *first_sector = vol->data_start_sector + 
                        (uint64_t)(cluster - 2) * vol->sectors_per_cluster;
    }
    if (sector_count) {
        *sector_count = vol->sectors_per_cluster;
    }
    
    return 0;
}

/*===========================================================================
 * Utilities
 *===========================================================================*/

bool uft_badblock_in_data_area(const uft_fat_ctx_t *ctx, uint64_t location,
                               uft_badblock_unit_t unit)
{
    if (!ctx) return false;
    
    const uft_fat_volume_t *vol = uft_fat_get_volume(ctx);
    if (!vol) return false;
    
    uint64_t sector = 0;
    
    switch (unit) {
        case UFT_BADBLOCK_SECTOR:
            sector = location;
            break;
        case UFT_BADBLOCK_BYTE_OFFSET:
            sector = location / vol->bytes_per_sector;
            break;
        case UFT_BADBLOCK_BLOCK_1K:
            sector = (location * 1024) / vol->bytes_per_sector;
            break;
        case UFT_BADBLOCK_CLUSTER:
            return location >= 2 && location < vol->data_clusters + 2;
    }
    
    return sector >= vol->data_start_sector;
}

const char *uft_badblock_unit_str(uft_badblock_unit_t unit)
{
    switch (unit) {
        case UFT_BADBLOCK_SECTOR:      return "sector";
        case UFT_BADBLOCK_CLUSTER:     return "cluster";
        case UFT_BADBLOCK_BYTE_OFFSET: return "byte";
        case UFT_BADBLOCK_BLOCK_1K:    return "1KB-block";
        default:                        return "unknown";
    }
}

const char *uft_badblock_source_str(uft_badblock_source_t source)
{
    switch (source) {
        case UFT_BADBLOCK_SRC_MANUAL: return "manual";
        case UFT_BADBLOCK_SRC_FILE:   return "file";
        case UFT_BADBLOCK_SRC_SCAN:   return "scan";
        case UFT_BADBLOCK_SRC_FAT:    return "FAT";
        default:                       return "unknown";
    }
}

void uft_badblock_print_summary(const uft_badblock_list_t *list, FILE *fp)
{
    if (!list || !fp) return;
    
    fprintf(fp, "Bad Block List Summary:\n");
    fprintf(fp, "  Total entries: %zu\n", list->count);
    
    size_t by_unit[4] = {0};
    size_t by_source[4] = {0};
    
    for (size_t i = 0; i < list->count; i++) {
        const uft_badblock_entry_t *e = &list->entries[i];
        if (e->unit < 4) by_unit[e->unit]++;
        if (e->source < 4) by_source[e->source]++;
    }
    
    fprintf(fp, "  By unit:\n");
    fprintf(fp, "    Sectors:    %zu\n", by_unit[UFT_BADBLOCK_SECTOR]);
    fprintf(fp, "    Clusters:   %zu\n", by_unit[UFT_BADBLOCK_CLUSTER]);
    fprintf(fp, "    Byte offs:  %zu\n", by_unit[UFT_BADBLOCK_BYTE_OFFSET]);
    fprintf(fp, "    1KB blocks: %zu\n", by_unit[UFT_BADBLOCK_BLOCK_1K]);
    
    fprintf(fp, "  By source:\n");
    fprintf(fp, "    Manual:     %zu\n", by_source[UFT_BADBLOCK_SRC_MANUAL]);
    fprintf(fp, "    File:       %zu\n", by_source[UFT_BADBLOCK_SRC_FILE]);
    fprintf(fp, "    Scan:       %zu\n", by_source[UFT_BADBLOCK_SRC_SCAN]);
    fprintf(fp, "    FAT:        %zu\n", by_source[UFT_BADBLOCK_SRC_FAT]);
}

void uft_badblock_print_stats(const uft_badblock_stats_t *stats, FILE *fp)
{
    if (!stats || !fp) return;
    
    fprintf(fp, "Bad Block Analysis:\n");
    fprintf(fp, "  Total bad:        %zu\n", stats->total_bad);
    fprintf(fp, "  In data area:     %zu\n", stats->in_data_area);
    fprintf(fp, "  In reserved:      %zu\n", stats->in_reserved);
    fprintf(fp, "  In FAT:           %zu\n", stats->in_fat);
    fprintf(fp, "  In root dir:      %zu\n", stats->in_root_dir);
    fprintf(fp, "  Already marked:   %zu\n", stats->already_marked);
    fprintf(fp, "  Needs marking:    %zu\n", stats->needs_marking);
    fprintf(fp, "  Bytes affected:   %llu\n", (unsigned long long)stats->bytes_affected);
    fprintf(fp, "  Clusters affected: %u\n", stats->clusters_affected);
}
