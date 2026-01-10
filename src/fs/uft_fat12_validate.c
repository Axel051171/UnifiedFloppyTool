/**
 * @file uft_fat12_validate.c
 * @brief FAT12/FAT16 Validation and Repair
 * @version 3.7.0
 * 
 * Filesystem validation, cross-link detection, repair, deleted file recovery
 */

#include "uft/fs/uft_fat12.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 * Internal Structures
 *===========================================================================*/

/** Cluster usage map */
typedef struct {
    uint8_t *map;           /**< Bitmap: 1 bit per cluster */
    uint32_t *refs;         /**< Reference count per cluster */
    size_t count;           /**< Number of clusters */
} cluster_map_t;

/*===========================================================================
 * Cluster Map Operations
 *===========================================================================*/

static cluster_map_t *cluster_map_create(size_t count) {
    cluster_map_t *map = calloc(1, sizeof(cluster_map_t));
    if (!map) return NULL;
    
    map->count = count;
    map->map = calloc((count + 7) / 8, 1);
    map->refs = calloc(count, sizeof(uint32_t));
    
    if (!map->map || !map->refs) {
        free(map->map);
        free(map->refs);
        free(map);
        return NULL;
    }
    
    return map;
}

static void cluster_map_destroy(cluster_map_t *map) {
    if (!map) return;
    free(map->map);
    free(map->refs);
    free(map);
}

static void cluster_map_set(cluster_map_t *map, uint32_t cluster) {
    if (cluster >= map->count) return;
    map->map[cluster / 8] |= (1 << (cluster % 8));
    map->refs[cluster]++;
}

static bool cluster_map_test(const cluster_map_t *map, uint32_t cluster) {
    if (cluster >= map->count) return false;
    return (map->map[cluster / 8] & (1 << (cluster % 8))) != 0;
}

static uint32_t cluster_map_refcount(const cluster_map_t *map, uint32_t cluster) {
    if (cluster >= map->count) return 0;
    return map->refs[cluster];
}

/*===========================================================================
 * Validation Issue Management
 *===========================================================================*/

void uft_fat_validation_init(uft_fat_validation_t *val) {
    if (!val) return;
    memset(val, 0, sizeof(*val));
}

void uft_fat_validation_free(uft_fat_validation_t *val) {
    if (!val) return;
    free(val->issues);
    memset(val, 0, sizeof(*val));
}

/**
 * @brief Add issue to validation result
 */
static int add_issue(uft_fat_validation_t *val, uft_fat_severity_t severity,
                     uint32_t cluster, const char *fmt, ...) {
    /* Grow array if needed */
    if (val->issue_count >= val->issue_capacity) {
        size_t new_cap = val->issue_capacity == 0 ? 16 : val->issue_capacity * 2;
        uft_fat_issue_t *new_issues = realloc(val->issues, new_cap * sizeof(uft_fat_issue_t));
        if (!new_issues) return -1;
        val->issues = new_issues;
        val->issue_capacity = new_cap;
    }
    
    uft_fat_issue_t *issue = &val->issues[val->issue_count++];
    issue->severity = severity;
    issue->cluster = cluster;
    
    va_list args;
    va_start(args, fmt);
    vsnprintf(issue->message, sizeof(issue->message), fmt, args);
    va_end(args);
    
    return 0;
}

/*===========================================================================
 * Directory Tree Scanning
 *===========================================================================*/

typedef struct {
    const uft_fat_ctx_t *ctx;
    cluster_map_t *map;
    uft_fat_validation_t *val;
    int depth;
} scan_ctx_t;

/**
 * @brief Mark cluster chain as used
 */
static int mark_chain(scan_ctx_t *sctx, uint32_t start, uint32_t expected_size,
                      const char *owner) {
    if (start == 0) return 0;
    
    uint32_t current = start;
    uint32_t count = 0;
    uint32_t max_clusters = (expected_size + (512 * sctx->ctx->vol.sectors_per_cluster) - 1) /
                           (512 * sctx->ctx->vol.sectors_per_cluster);
    if (expected_size == 0) max_clusters = 0xFFFFFFFF;  /* Directories have no size limit */
    
    while (!uft_fat_cluster_is_eof(sctx->ctx, current)) {
        /* Bounds check */
        if (current < UFT_FAT_FIRST_CLUSTER || current > sctx->ctx->vol.last_cluster) {
            add_issue(sctx->val, UFT_FAT_SEV_ERROR, current,
                     "Invalid cluster %u in chain for %s", current, owner);
            return -1;
        }
        
        /* Cross-link detection */
        if (cluster_map_test(sctx->map, current - UFT_FAT_FIRST_CLUSTER)) {
            uint32_t refs = cluster_map_refcount(sctx->map, current - UFT_FAT_FIRST_CLUSTER);
            add_issue(sctx->val, UFT_FAT_SEV_ERROR, current,
                     "Cross-linked cluster %u (refs=%u) in %s", current, refs + 1, owner);
            sctx->val->cross_linked_clusters++;
            sctx->val->repairable = false;
        }
        
        cluster_map_set(sctx->map, current - UFT_FAT_FIRST_CLUSTER);
        count++;
        
        /* Chain too long? */
        if (count > max_clusters + 1) {
            add_issue(sctx->val, UFT_FAT_SEV_WARNING, current,
                     "Chain longer than expected for %s (%u clusters for %u bytes)",
                     owner, count, expected_size);
        }
        
        /* Loop detection */
        if (count > sctx->ctx->vol.data_clusters + 10) {
            add_issue(sctx->val, UFT_FAT_SEV_FATAL, current,
                     "Possible loop in cluster chain for %s", owner);
            return -1;
        }
        
        /* Next cluster */
        uint32_t next;
        int rc = uft_fat_get_entry(sctx->ctx, current, &next);
        if (rc != 0) return rc;
        
        /* Bad cluster? */
        if (uft_fat_cluster_is_bad(sctx->ctx, next)) {
            add_issue(sctx->val, UFT_FAT_SEV_WARNING, next,
                     "Bad cluster marker in chain for %s", owner);
            sctx->val->bad_clusters++;
            break;
        }
        
        current = next;
    }
    
    return 0;
}

/**
 * @brief Recursively scan directory tree
 */
static int scan_directory(scan_ctx_t *sctx, uint32_t cluster, const char *path) {
    if (sctx->depth > 32) {
        add_issue(sctx->val, UFT_FAT_SEV_ERROR, cluster,
                 "Directory nesting too deep at %s", path);
        return -1;
    }
    
    uft_fat_dir_t dir;
    uft_fat_dir_init(&dir);
    
    int rc = uft_fat_read_dir(sctx->ctx, cluster, &dir);
    if (rc != 0) {
        add_issue(sctx->val, UFT_FAT_SEV_ERROR, cluster,
                 "Cannot read directory %s", path);
        uft_fat_dir_free(&dir);
        return rc;
    }
    
    for (size_t i = 0; i < dir.count; i++) {
        const uft_fat_entry_t *entry = &dir.entries[i];
        char fullpath[UFT_FAT_MAX_PATH];
        
        snprintf(fullpath, sizeof(fullpath), "%s%s%s",
                path, (strcmp(path, "/") == 0) ? "" : "/",
                entry->has_lfn ? entry->lfn : entry->sfn);
        
        if (entry->is_directory) {
            /* Mark directory cluster chain */
            if (entry->cluster != 0) {
                mark_chain(sctx, entry->cluster, 0, fullpath);
                
                /* Recurse */
                sctx->depth++;
                scan_directory(sctx, entry->cluster, fullpath);
                sctx->depth--;
            }
        } else {
            /* Mark file cluster chain */
            mark_chain(sctx, entry->cluster, entry->size, fullpath);
            
            /* Verify size vs chain length */
            if (entry->size > 0 && entry->cluster == 0) {
                add_issue(sctx->val, UFT_FAT_SEV_ERROR, 0,
                         "File %s has size %u but no cluster", fullpath, entry->size);
            }
        }
    }
    
    uft_fat_dir_free(&dir);
    return 0;
}

/*===========================================================================
 * Main Validation
 *===========================================================================*/

int uft_fat_validate(const uft_fat_ctx_t *ctx, uft_fat_validation_t *val) {
    if (!ctx || !ctx->data || !val) return UFT_FAT_ERR_INVALID;
    
    uft_fat_validation_init(val);
    val->repairable = true;  /* Assume repairable until proven otherwise */
    
    /* Create cluster usage map */
    cluster_map_t *map = cluster_map_create(ctx->vol.data_clusters);
    if (!map) return UFT_FAT_ERR_NOMEM;
    
    val->total_clusters = ctx->vol.data_clusters;
    
    /* Scan FAT for statistics */
    for (uint32_t c = UFT_FAT_FIRST_CLUSTER; c <= ctx->vol.last_cluster; c++) {
        uint32_t entry;
        if (uft_fat_get_entry(ctx, c, &entry) == 0) {
            if (uft_fat_cluster_is_free(ctx, c)) {
                val->free_clusters++;
            } else if (uft_fat_cluster_is_bad(ctx, entry)) {
                val->bad_clusters++;
            } else {
                val->used_clusters++;
            }
        }
    }
    
    /* Scan directory tree */
    scan_ctx_t sctx = {
        .ctx = ctx,
        .map = map,
        .val = val,
        .depth = 0
    };
    
    /* Mark root directory as used (for subdirectories) */
    if (ctx->vol.fat_type == UFT_FAT_TYPE_FAT12 || ctx->vol.fat_type == UFT_FAT_TYPE_FAT16) {
        /* Root directory is in fixed area, not clusters */
    }
    
    scan_directory(&sctx, 0, "/");
    
    /* Find lost clusters (allocated in FAT but not referenced) */
    for (uint32_t c = UFT_FAT_FIRST_CLUSTER; c <= ctx->vol.last_cluster; c++) {
        uint32_t entry;
        if (uft_fat_get_entry(ctx, c, &entry) == 0) {
            if (!uft_fat_cluster_is_free(ctx, c) && !uft_fat_cluster_is_bad(ctx, entry)) {
                /* Cluster is allocated */
                if (!cluster_map_test(map, c - UFT_FAT_FIRST_CLUSTER)) {
                    val->lost_clusters++;
                    add_issue(val, UFT_FAT_SEV_WARNING, c,
                             "Lost cluster %u (allocated but unreferenced)", c);
                }
            }
        }
    }
    
    /* Compare FAT copies */
    int fat_diff = uft_fat_compare_fats(ctx);
    if (fat_diff > 0) {
        val->fat_copies_differ = true;
        add_issue(val, UFT_FAT_SEV_WARNING, 0,
                 "FAT copies differ (%d differences)", fat_diff);
    }
    
    /* Set overall validity */
    val->valid = (val->issue_count == 0) ||
                 (val->cross_linked_clusters == 0 && val->lost_clusters == 0);
    
    cluster_map_destroy(map);
    return 0;
}

/*===========================================================================
 * FAT Comparison
 *===========================================================================*/

int uft_fat_compare_fats(const uft_fat_ctx_t *ctx) {
    if (!ctx || !ctx->data) return -1;
    if (ctx->vol.num_fats < 2) return 0;
    
    uint32_t fat_bytes = ctx->vol.fat_size * UFT_FAT_SECTOR_SIZE;
    const uint8_t *fat1 = ctx->data + ctx->vol.fat_start_sector * UFT_FAT_SECTOR_SIZE;
    const uint8_t *fat2 = fat1 + fat_bytes;
    
    int differences = 0;
    for (uint32_t i = 0; i < fat_bytes; i++) {
        if (fat1[i] != fat2[i]) {
            differences++;
        }
    }
    
    return differences;
}

int uft_fat_sync_fats(uft_fat_ctx_t *ctx) {
    if (!ctx || !ctx->data) return UFT_FAT_ERR_INVALID;
    if (ctx->read_only) return UFT_FAT_ERR_READONLY;
    if (ctx->vol.num_fats < 2) return 0;
    
    uint32_t fat_bytes = ctx->vol.fat_size * UFT_FAT_SECTOR_SIZE;
    uint8_t *fat1 = ctx->data + ctx->vol.fat_start_sector * UFT_FAT_SECTOR_SIZE;
    uint8_t *fat2 = fat1 + fat_bytes;
    
    memcpy(fat2, fat1, fat_bytes);
    ctx->modified = true;
    
    return 0;
}

/*===========================================================================
 * Repair Operations
 *===========================================================================*/

int uft_fat_repair(uft_fat_ctx_t *ctx, const uft_fat_validation_t *val) {
    if (!ctx || !val) return UFT_FAT_ERR_INVALID;
    if (ctx->read_only) return UFT_FAT_ERR_READONLY;
    
    int fixed = 0;
    
    /* Sync FAT copies */
    if (val->fat_copies_differ) {
        if (uft_fat_sync_fats(ctx) == 0) {
            fixed++;
        }
    }
    
    /* Note: Cross-link repair requires user decision on which file to keep */
    /* Lost cluster recovery would create files in a FOUND.000 directory */
    
    return fixed;
}

int uft_fat_rebuild_fat(uft_fat_ctx_t *ctx) {
    if (!ctx || !ctx->data) return UFT_FAT_ERR_INVALID;
    if (ctx->read_only) return UFT_FAT_ERR_READONLY;
    
    /* This is a complex operation that requires:
     * 1. Clear FAT (mark all as free)
     * 2. Mark reserved clusters
     * 3. Scan directory tree and rebuild chains
     * 
     * For safety, we don't implement full rebuild - it's too risky
     */
    
    return UFT_FAT_ERR_INVALID;  /* Not implemented */
}

/*===========================================================================
 * Deleted File Recovery
 *===========================================================================*/

int uft_fat_find_deleted(const uft_fat_ctx_t *ctx, uft_fat_dir_t *dir,
                         uft_fat_recover_callback_t callback, void *user_data) {
    if (!ctx || !callback) return UFT_FAT_ERR_INVALID;
    
    /* If no directory specified, scan root */
    uint32_t cluster = dir ? dir->cluster : 0;
    int found = 0;
    
    /* Read directory including deleted entries */
    uint8_t sector[UFT_FAT_SECTOR_SIZE];
    uint32_t entries_per_sector = UFT_FAT_SECTOR_SIZE / 32;
    
    if (cluster == 0) {
        /* Root directory */
        for (uint32_t s = 0; s < ctx->vol.root_dir_sectors; s++) {
            int rc = uft_fat_read_root_sector(ctx, s, sector);
            if (rc != 0) continue;
            
            for (uint32_t e = 0; e < entries_per_sector; e++) {
                uint8_t *raw = sector + e * 32;
                
                if (raw[0] == UFT_FAT_DIRENT_END) goto done;
                if (raw[0] != UFT_FAT_DIRENT_FREE) continue;
                
                /* Skip LFN entries */
                if ((raw[11] & UFT_FAT_ATTR_LFN_MASK) == UFT_FAT_ATTR_LFN) continue;
                
                /* Build entry */
                uft_fat_entry_t entry;
                memset(&entry, 0, sizeof(entry));
                entry.is_deleted = true;
                
                /* Recover name (first char is unknown) */
                entry.sfn[0] = '?';
                for (int i = 1; i < 8 && raw[i] != ' '; i++) {
                    entry.sfn[i] = raw[i];
                }
                if (raw[8] != ' ') {
                    size_t len = strlen(entry.sfn);
                    entry.sfn[len++] = '.';
                    for (int i = 8; i < 11 && raw[i] != ' '; i++) {
                        entry.sfn[len++] = raw[i];
                    }
                }
                
                entry.attributes = raw[11];
                entry.is_directory = (raw[11] & UFT_FAT_ATTR_DIRECTORY) != 0;
                entry.cluster = raw[26] | ((uint32_t)raw[27] << 8);
                entry.size = raw[28] | ((uint32_t)raw[29] << 8) |
                            ((uint32_t)raw[30] << 16) | ((uint32_t)raw[31] << 24);
                
                /* Check if recoverable */
                bool can_recover = true;
                if (entry.cluster != 0 && entry.size > 0) {
                    /* Check if first cluster is free */
                    if (!uft_fat_cluster_is_free(ctx, entry.cluster)) {
                        can_recover = false;
                    }
                }
                
                callback(&entry, can_recover, user_data);
                found++;
            }
        }
    }
    
done:
    return found;
}

int uft_fat_recover_file(const uft_fat_ctx_t *ctx, const uft_fat_entry_t *entry,
                         uint8_t *output, size_t *size) {
    if (!ctx || !entry || !output || !size) return UFT_FAT_ERR_INVALID;
    if (!entry->is_deleted) return UFT_FAT_ERR_INVALID;
    
    if (entry->size == 0) {
        *size = 0;
        return 0;
    }
    
    if (*size < entry->size) return UFT_FAT_ERR_INVALID;
    
    /* For deleted files, clusters may be overwritten.
     * Best effort: read contiguous clusters starting from entry->cluster
     */
    size_t cluster_bytes = ctx->vol.sectors_per_cluster * UFT_FAT_SECTOR_SIZE;
    size_t clusters_needed = (entry->size + cluster_bytes - 1) / cluster_bytes;
    
    uint8_t *cluster_buf = malloc(cluster_bytes);
    if (!cluster_buf) return UFT_FAT_ERR_NOMEM;
    
    size_t offset = 0;
    size_t remaining = entry->size;
    uint32_t cluster = entry->cluster;
    
    for (size_t i = 0; i < clusters_needed && remaining > 0; i++) {
        /* Check cluster is in valid range and free */
        if (cluster < UFT_FAT_FIRST_CLUSTER || cluster > ctx->vol.last_cluster) {
            break;
        }
        
        if (!uft_fat_cluster_is_free(ctx, cluster)) {
            /* Cluster reused - stop here */
            break;
        }
        
        int rc = uft_fat_read_cluster(ctx, cluster, cluster_buf);
        if (rc != 0) {
            free(cluster_buf);
            return rc;
        }
        
        size_t to_copy = (remaining > cluster_bytes) ? cluster_bytes : remaining;
        memcpy(output + offset, cluster_buf, to_copy);
        offset += to_copy;
        remaining -= to_copy;
        
        cluster++;  /* Assume contiguous allocation */
    }
    
    free(cluster_buf);
    *size = offset;
    
    return (remaining == 0) ? 0 : UFT_FAT_ERR_IO;  /* Partial recovery if data lost */
}

/*===========================================================================
 * JSON Report Generation
 *===========================================================================*/

size_t uft_fat_to_json(const uft_fat_ctx_t *ctx, char *buffer, size_t size) {
    if (!ctx || !buffer || size == 0) return 0;
    
    size_t written = 0;
    
    written += snprintf(buffer + written, size - written,
        "{\n"
        "  \"fat_type\": \"FAT%d\",\n"
        "  \"bytes_per_sector\": %u,\n"
        "  \"sectors_per_cluster\": %u,\n"
        "  \"reserved_sectors\": %u,\n"
        "  \"num_fats\": %u,\n"
        "  \"root_entries\": %u,\n"
        "  \"total_sectors\": %u,\n"
        "  \"fat_sectors\": %u,\n"
        "  \"data_clusters\": %u,\n"
        "  \"label\": \"%s\",\n"
        "  \"oem_name\": \"%s\",\n"
        "  \"serial\": \"0x%08X\"\n"
        "}",
        ctx->vol.fat_type,
        ctx->vol.bytes_per_sector,
        ctx->vol.sectors_per_cluster,
        ctx->vol.reserved_sectors,
        ctx->vol.num_fats,
        ctx->vol.root_entry_count,
        ctx->vol.total_sectors,
        ctx->vol.fat_size,
        ctx->vol.data_clusters,
        ctx->vol.label,
        ctx->vol.oem_name,
        ctx->vol.serial
    );
    
    return written;
}
