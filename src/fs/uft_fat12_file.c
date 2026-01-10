/**
 * @file uft_fat12_file.c
 * @brief FAT12/FAT16 File Operations
 * @version 3.7.0
 * 
 * File extraction, injection, deletion, rename, mkdir/rmdir
 */

#include "uft/fs/uft_fat12.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/*===========================================================================
 * Internal Helpers
 *===========================================================================*/

/** Read little-endian 16-bit */
static inline uint16_t read_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/** Write little-endian 16-bit */
static inline void write_le16(uint8_t *p, uint16_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

/** Write little-endian 32-bit */
static inline void write_le32(uint8_t *p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

/**
 * @brief Get cluster size in bytes
 */
static inline size_t cluster_size(const uft_fat_ctx_t *ctx) {
    return ctx->vol.sectors_per_cluster * UFT_FAT_SECTOR_SIZE;
}

/**
 * @brief Parse path into parent path and filename
 */
static int parse_path(const char *path, uint32_t *parent_cluster, 
                      char *filename, const uft_fat_ctx_t *ctx) {
    if (!path || !filename) return UFT_FAT_ERR_INVALID;
    
    /* Skip leading separator */
    while (*path == '/' || *path == '\\') path++;
    
    if (*path == '\0') {
        return UFT_FAT_ERR_INVALID;  /* Can't operate on root */
    }
    
    /* Find last separator */
    const char *last_sep = NULL;
    const char *p = path;
    while (*p) {
        if (*p == '/' || *p == '\\') last_sep = p;
        p++;
    }
    
    if (last_sep == NULL) {
        /* File in root directory */
        *parent_cluster = 0;
        strncpy(filename, path, UFT_FAT_MAX_LFN);
        filename[UFT_FAT_MAX_LFN] = '\0';
    } else {
        /* File in subdirectory */
        size_t parent_len = last_sep - path;
        char parent_path[UFT_FAT_MAX_PATH];
        strncpy(parent_path, path, parent_len);
        parent_path[parent_len] = '\0';
        
        uft_fat_entry_t parent_entry;
        int rc = uft_fat_find_path(ctx, parent_path, &parent_entry);
        if (rc != 0) return rc;
        
        if (!parent_entry.is_directory) {
            return UFT_FAT_ERR_NOTFOUND;
        }
        
        *parent_cluster = parent_entry.cluster;
        strncpy(filename, last_sep + 1, UFT_FAT_MAX_LFN);
        filename[UFT_FAT_MAX_LFN] = '\0';
    }
    
    return 0;
}

/*===========================================================================
 * File Extraction
 *===========================================================================*/

uint8_t *uft_fat_extract(const uft_fat_ctx_t *ctx, const uft_fat_entry_t *entry,
                         uint8_t *buffer, size_t *size) {
    if (!ctx || !entry || !size) return NULL;
    
    /* Directory? */
    if (entry->is_directory) {
        return NULL;
    }
    
    /* Empty file */
    if (entry->size == 0) {
        *size = 0;
        if (buffer) return buffer;
        return malloc(1);  /* Return valid pointer for 0-byte file */
    }
    
    /* Allocate buffer if needed */
    uint8_t *out = buffer;
    if (!out) {
        out = malloc(entry->size);
        if (!out) return NULL;
    } else if (*size < entry->size) {
        return NULL;  /* Buffer too small */
    }
    
    /* Get cluster chain */
    uft_fat_chain_t chain;
    uft_fat_chain_init(&chain);
    
    int rc = uft_fat_get_chain(ctx, entry->cluster, &chain);
    if (rc != 0) {
        if (!buffer) free(out);
        uft_fat_chain_free(&chain);
        return NULL;
    }
    
    /* Read clusters */
    size_t clust_sz = cluster_size(ctx);
    size_t remaining = entry->size;
    size_t offset = 0;
    uint8_t *clust_buf = malloc(clust_sz);
    
    if (!clust_buf) {
        if (!buffer) free(out);
        uft_fat_chain_free(&chain);
        return NULL;
    }
    
    for (size_t i = 0; i < chain.count && remaining > 0; i++) {
        rc = uft_fat_read_cluster(ctx, chain.clusters[i], clust_buf);
        if (rc != 0) {
            free(clust_buf);
            if (!buffer) free(out);
            uft_fat_chain_free(&chain);
            return NULL;
        }
        
        size_t to_copy = (remaining > clust_sz) ? clust_sz : remaining;
        memcpy(out + offset, clust_buf, to_copy);
        offset += to_copy;
        remaining -= to_copy;
    }
    
    free(clust_buf);
    uft_fat_chain_free(&chain);
    
    *size = entry->size;
    return out;
}

uint8_t *uft_fat_extract_path(const uft_fat_ctx_t *ctx, const char *path,
                              uint8_t *buffer, size_t *size) {
    if (!ctx || !path || !size) return NULL;
    
    uft_fat_entry_t entry;
    int rc = uft_fat_find_path(ctx, path, &entry);
    if (rc != 0) return NULL;
    
    return uft_fat_extract(ctx, &entry, buffer, size);
}

int uft_fat_extract_to_file(const uft_fat_ctx_t *ctx, const char *path,
                            const char *dest_path) {
    if (!ctx || !path || !dest_path) return UFT_FAT_ERR_INVALID;
    
    uft_fat_entry_t entry;
    int rc = uft_fat_find_path(ctx, path, &entry);
    if (rc != 0) return rc;
    
    if (entry.is_directory) {
        return UFT_FAT_ERR_INVALID;
    }
    
    /* Extract to memory */
    size_t size = entry.size;
    uint8_t *data = uft_fat_extract(ctx, &entry, NULL, &size);
    if (!data && entry.size > 0) return UFT_FAT_ERR_IO;
    
    /* Write to file */
    FILE *fp = fopen(dest_path, "wb");
    if (!fp) {
        free(data);
        return UFT_FAT_ERR_IO;
    }
    
    if (size > 0) {
        if (fwrite(data, 1, size, fp) != size) {
            fclose(fp);
            free(data);
            return UFT_FAT_ERR_IO;
        }
    }
    
    fclose(fp);
    free(data);
    return 0;
}

/*===========================================================================
 * File Injection
 *===========================================================================*/

/**
 * @brief Create SFN from filename
 */
static void make_sfn(const char *name, uint8_t sfn[11]) {
    memset(sfn, ' ', 11);
    
    /* Find extension */
    const char *dot = strrchr(name, '.');
    
    /* Base name */
    const char *src = name;
    int i = 0;
    while (*src && src != dot && i < 8) {
        if (*src != ' ' && *src != '.') {
            sfn[i++] = toupper((unsigned char)*src);
        }
        src++;
    }
    
    /* Extension */
    if (dot && dot[1]) {
        src = dot + 1;
        i = 8;
        while (*src && i < 11) {
            if (*src != ' ') {
                sfn[i++] = toupper((unsigned char)*src);
            }
            src++;
        }
    }
}

/**
 * @brief Find or create free directory entry slot
 * @return Entry offset in directory, or -1 on error
 */
static int find_free_dir_slot(uft_fat_ctx_t *ctx, uint32_t dir_cluster,
                              size_t entries_needed, uint32_t *slot_cluster,
                              uint32_t *slot_offset) {
    uint8_t *sector_buf = malloc(UFT_FAT_SECTOR_SIZE);
    if (!sector_buf) return -1;
    
    uint32_t entries_per_sector = UFT_FAT_SECTOR_SIZE / 32;
    size_t consecutive_free = 0;
    uint32_t first_free_cluster = 0;
    uint32_t first_free_offset = 0;
    
    if (dir_cluster == 0) {
        /* Root directory - fixed size */
        for (uint32_t s = 0; s < ctx->vol.root_dir_sectors; s++) {
            int rc = uft_fat_read_root_sector(ctx, s, sector_buf);
            if (rc != 0) {
                free(sector_buf);
                return -1;
            }
            
            for (uint32_t e = 0; e < entries_per_sector; e++) {
                uint8_t first = sector_buf[e * 32];
                
                if (first == UFT_FAT_DIRENT_END || first == UFT_FAT_DIRENT_FREE) {
                    if (consecutive_free == 0) {
                        first_free_cluster = 0;
                        first_free_offset = s * entries_per_sector + e;
                    }
                    consecutive_free++;
                    
                    if (consecutive_free >= entries_needed) {
                        *slot_cluster = first_free_cluster;
                        *slot_offset = first_free_offset;
                        free(sector_buf);
                        return 0;
                    }
                    
                    if (first == UFT_FAT_DIRENT_END) {
                        /* End marker - can use this and following slots */
                        *slot_cluster = first_free_cluster;
                        *slot_offset = first_free_offset;
                        free(sector_buf);
                        return 0;
                    }
                } else {
                    consecutive_free = 0;
                }
            }
        }
    } else {
        /* Subdirectory - can grow */
        uint32_t current = dir_cluster;
        uint32_t entry_index = 0;
        uint32_t entries_per_cluster = (ctx->vol.sectors_per_cluster * UFT_FAT_SECTOR_SIZE) / 32;
        
        uint8_t *cluster_buf = malloc(cluster_size(ctx));
        if (!cluster_buf) {
            free(sector_buf);
            return -1;
        }
        
        while (!uft_fat_cluster_is_eof(ctx, current)) {
            int rc = uft_fat_read_cluster(ctx, current, cluster_buf);
            if (rc != 0) {
                free(cluster_buf);
                free(sector_buf);
                return -1;
            }
            
            for (uint32_t e = 0; e < entries_per_cluster; e++, entry_index++) {
                uint8_t first = cluster_buf[e * 32];
                
                if (first == UFT_FAT_DIRENT_END || first == UFT_FAT_DIRENT_FREE) {
                    if (consecutive_free == 0) {
                        first_free_cluster = current;
                        first_free_offset = e;
                    }
                    consecutive_free++;
                    
                    if (consecutive_free >= entries_needed) {
                        *slot_cluster = first_free_cluster;
                        *slot_offset = first_free_offset;
                        free(cluster_buf);
                        free(sector_buf);
                        return 0;
                    }
                    
                    if (first == UFT_FAT_DIRENT_END) {
                        *slot_cluster = first_free_cluster;
                        *slot_offset = first_free_offset;
                        free(cluster_buf);
                        free(sector_buf);
                        return 0;
                    }
                } else {
                    consecutive_free = 0;
                }
            }
            
            /* Next cluster */
            uint32_t next;
            rc = uft_fat_get_entry(ctx, current, &next);
            if (rc != 0) {
                free(cluster_buf);
                free(sector_buf);
                return -1;
            }
            current = next;
        }
        
        /* Need to allocate new cluster for directory */
        uft_fat_chain_t new_chain;
        uft_fat_chain_init(&new_chain);
        rc = uft_fat_alloc_chain(ctx, 1, &new_chain);
        if (rc != 0 || new_chain.count == 0) {
            free(cluster_buf);
            free(sector_buf);
            uft_fat_chain_free(&new_chain);
            return -1;
        }
        
        /* Link new cluster to directory chain */
        /* Find last cluster */
        current = dir_cluster;
        while (1) {
            uint32_t next;
            uft_fat_get_entry(ctx, current, &next);
            if (uft_fat_cluster_is_eof(ctx, next)) break;
            current = next;
        }
        uft_fat_set_entry(ctx, current, new_chain.clusters[0]);
        
        /* Initialize new cluster (all zeros = end markers) */
        memset(cluster_buf, 0, cluster_size(ctx));
        uft_fat_write_cluster(ctx, new_chain.clusters[0], cluster_buf);
        
        *slot_cluster = new_chain.clusters[0];
        *slot_offset = 0;
        
        free(cluster_buf);
        uft_fat_chain_free(&new_chain);
    }
    
    free(sector_buf);
    return -1;  /* No space */
}

/**
 * @brief Write directory entry at slot
 */
static int write_dir_entry(uft_fat_ctx_t *ctx, uint32_t dir_cluster,
                           uint32_t slot_cluster, uint32_t slot_offset,
                           const uint8_t *sfn, uint8_t attr,
                           uint32_t first_cluster, uint32_t file_size) {
    uint8_t entry[32];
    memset(entry, 0, 32);
    
    /* Name */
    memcpy(entry, sfn, 11);
    
    /* Attributes */
    entry[11] = attr;
    
    /* Timestamps */
    uint16_t fat_time, fat_date;
    uft_fat_from_unix_time(time(NULL), &fat_time, &fat_date);
    write_le16(entry + 14, fat_time);  /* Create time */
    write_le16(entry + 16, fat_date);  /* Create date */
    write_le16(entry + 18, fat_date);  /* Access date */
    write_le16(entry + 22, fat_time);  /* Modify time */
    write_le16(entry + 24, fat_date);  /* Modify date */
    
    /* Cluster */
    write_le16(entry + 26, first_cluster & 0xFFFF);
    write_le16(entry + 20, (first_cluster >> 16) & 0xFFFF);  /* High word for FAT32 compat */
    
    /* Size */
    write_le32(entry + 28, file_size);
    
    /* Write to directory */
    if (dir_cluster == 0) {
        /* Root directory */
        uint32_t sector = slot_offset / (UFT_FAT_SECTOR_SIZE / 32);
        uint32_t offset_in_sector = (slot_offset % (UFT_FAT_SECTOR_SIZE / 32)) * 32;
        
        uint8_t sector_buf[UFT_FAT_SECTOR_SIZE];
        int rc = uft_fat_read_root_sector(ctx, sector, sector_buf);
        if (rc != 0) return rc;
        
        memcpy(sector_buf + offset_in_sector, entry, 32);
        return uft_fat_write_root_sector(ctx, sector, sector_buf);
    } else {
        /* Subdirectory */
        size_t clust_sz = cluster_size(ctx);
        uint8_t *cluster_buf = malloc(clust_sz);
        if (!cluster_buf) return UFT_FAT_ERR_NOMEM;
        
        int rc = uft_fat_read_cluster(ctx, slot_cluster, cluster_buf);
        if (rc != 0) {
            free(cluster_buf);
            return rc;
        }
        
        memcpy(cluster_buf + slot_offset * 32, entry, 32);
        rc = uft_fat_write_cluster(ctx, slot_cluster, cluster_buf);
        
        free(cluster_buf);
        return rc;
    }
}

int uft_fat_inject(uft_fat_ctx_t *ctx, uint32_t dir_cluster,
                   const char *name, const uint8_t *data, size_t size) {
    if (!ctx || !name) return UFT_FAT_ERR_INVALID;
    if (ctx->read_only) return UFT_FAT_ERR_READONLY;
    
    /* Check if file already exists */
    uft_fat_entry_t existing;
    if (uft_fat_find_entry(ctx, dir_cluster, name, &existing) == 0) {
        return UFT_FAT_ERR_EXISTS;
    }
    
    /* Calculate clusters needed */
    size_t clust_sz = cluster_size(ctx);
    size_t clusters_needed = (size + clust_sz - 1) / clust_sz;
    if (size == 0) clusters_needed = 0;
    
    /* Allocate cluster chain */
    uint32_t first_cluster = 0;
    uft_fat_chain_t chain;
    uft_fat_chain_init(&chain);
    
    if (clusters_needed > 0) {
        int rc = uft_fat_alloc_chain(ctx, clusters_needed, &chain);
        if (rc != 0) {
            uft_fat_chain_free(&chain);
            return UFT_FAT_ERR_DISKFULL;
        }
        first_cluster = chain.clusters[0];
        
        /* Write data to clusters */
        uint8_t *clust_buf = malloc(clust_sz);
        if (!clust_buf) {
            uft_fat_free_chain(ctx, first_cluster);
            uft_fat_chain_free(&chain);
            return UFT_FAT_ERR_NOMEM;
        }
        
        size_t remaining = size;
        size_t offset = 0;
        
        for (size_t i = 0; i < chain.count; i++) {
            size_t to_write = (remaining > clust_sz) ? clust_sz : remaining;
            
            memset(clust_buf, 0, clust_sz);
            if (data) {
                memcpy(clust_buf, data + offset, to_write);
            }
            
            rc = uft_fat_write_cluster(ctx, chain.clusters[i], clust_buf);
            if (rc != 0) {
                free(clust_buf);
                uft_fat_free_chain(ctx, first_cluster);
                uft_fat_chain_free(&chain);
                return rc;
            }
            
            offset += to_write;
            remaining -= to_write;
        }
        
        free(clust_buf);
    }
    
    /* Create directory entry */
    uint8_t sfn[11];
    make_sfn(name, sfn);
    
    /* Find free slot (just 1 entry needed for SFN) */
    uint32_t slot_cluster, slot_offset;
    if (find_free_dir_slot(ctx, dir_cluster, 1, &slot_cluster, &slot_offset) != 0) {
        if (first_cluster) uft_fat_free_chain(ctx, first_cluster);
        uft_fat_chain_free(&chain);
        return UFT_FAT_ERR_DISKFULL;
    }
    
    int rc = write_dir_entry(ctx, dir_cluster, slot_cluster, slot_offset,
                             sfn, UFT_FAT_ATTR_ARCHIVE, first_cluster, size);
    
    uft_fat_chain_free(&chain);
    
    if (rc == 0) {
        ctx->modified = true;
    }
    
    return rc;
}

int uft_fat_inject_path(uft_fat_ctx_t *ctx, const char *path,
                        const uint8_t *data, size_t size) {
    if (!ctx || !path) return UFT_FAT_ERR_INVALID;
    
    uint32_t parent_cluster;
    char filename[UFT_FAT_MAX_LFN + 1];
    
    int rc = parse_path(path, &parent_cluster, filename, ctx);
    if (rc != 0) return rc;
    
    return uft_fat_inject(ctx, parent_cluster, filename, data, size);
}

int uft_fat_inject_from_file(uft_fat_ctx_t *ctx, const char *path,
                             const char *src_path) {
    if (!ctx || !path || !src_path) return UFT_FAT_ERR_INVALID;
    
    /* Read source file */
    FILE *fp = fopen(src_path, "rb");
    if (!fp) return UFT_FAT_ERR_IO;
    
    if (fseek(fp, 0, SEEK_END) != 0) { /* seek error */ }
    long file_size = ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) { /* seek error */ }
    if (file_size < 0) {
        fclose(fp);
        return UFT_FAT_ERR_IO;
    }
    
    uint8_t *data = NULL;
    if (file_size > 0) {
        data = malloc(file_size);
        if (!data) {
            fclose(fp);
            return UFT_FAT_ERR_NOMEM;
        }
        
        if (fread(data, 1, file_size, fp) != (size_t)file_size) {
            free(data);
            fclose(fp);
            return UFT_FAT_ERR_IO;
        }
    }
    fclose(fp);
    
    int rc = uft_fat_inject_path(ctx, path, data, file_size);
    
    free(data);
    return rc;
}

/*===========================================================================
 * File Deletion
 *===========================================================================*/

/**
 * @brief Mark directory entry as deleted
 */
static int mark_entry_deleted(uft_fat_ctx_t *ctx, uint32_t dir_cluster,
                              uint32_t entry_index, size_t lfn_entries) {
    if (dir_cluster == 0) {
        /* Root directory */
        uint32_t entries_per_sector = UFT_FAT_SECTOR_SIZE / 32;
        
        /* Mark LFN entries */
        for (size_t i = 0; i < lfn_entries; i++) {
            uint32_t idx = entry_index - lfn_entries + i;
            uint32_t sector = idx / entries_per_sector;
            uint32_t offset = (idx % entries_per_sector) * 32;
            
            uint8_t sector_buf[UFT_FAT_SECTOR_SIZE];
            int rc = uft_fat_read_root_sector(ctx, sector, sector_buf);
            if (rc != 0) return rc;
            
            sector_buf[offset] = UFT_FAT_DIRENT_FREE;
            
            rc = uft_fat_write_root_sector(ctx, sector, sector_buf);
            if (rc != 0) return rc;
        }
        
        /* Mark main entry */
        uint32_t sector = entry_index / entries_per_sector;
        uint32_t offset = (entry_index % entries_per_sector) * 32;
        
        uint8_t sector_buf[UFT_FAT_SECTOR_SIZE];
        int rc = uft_fat_read_root_sector(ctx, sector, sector_buf);
        if (rc != 0) return rc;
        
        sector_buf[offset] = UFT_FAT_DIRENT_FREE;
        
        return uft_fat_write_root_sector(ctx, sector, sector_buf);
    } else {
        /* Subdirectory - need to handle cluster chain */
        /* This is simplified - assumes entries don't span clusters */
        size_t clust_sz = cluster_size(ctx);
        uint32_t entries_per_cluster = clust_sz / 32;
        
        uint8_t *cluster_buf = malloc(clust_sz);
        if (!cluster_buf) return UFT_FAT_ERR_NOMEM;
        
        /* Find the cluster containing this entry */
        uint32_t current = dir_cluster;
        uint32_t remaining_index = entry_index;
        
        while (remaining_index >= entries_per_cluster) {
            uint32_t next;
            int rc = uft_fat_get_entry(ctx, current, &next);
            if (rc != 0 || uft_fat_cluster_is_eof(ctx, next)) {
                free(cluster_buf);
                return UFT_FAT_ERR_INVALID;
            }
            current = next;
            remaining_index -= entries_per_cluster;
        }
        
        int rc = uft_fat_read_cluster(ctx, current, cluster_buf);
        if (rc != 0) {
            free(cluster_buf);
            return rc;
        }
        
        /* Mark as deleted */
        cluster_buf[remaining_index * 32] = UFT_FAT_DIRENT_FREE;
        
        rc = uft_fat_write_cluster(ctx, current, cluster_buf);
        free(cluster_buf);
        return rc;
    }
}

int uft_fat_delete(uft_fat_ctx_t *ctx, const char *path) {
    if (!ctx || !path) return UFT_FAT_ERR_INVALID;
    if (ctx->read_only) return UFT_FAT_ERR_READONLY;
    
    /* Find the file */
    uft_fat_entry_t entry;
    int rc = uft_fat_find_path(ctx, path, &entry);
    if (rc != 0) return rc;
    
    /* Can't delete directories this way */
    if (entry.is_directory) {
        return UFT_FAT_ERR_INVALID;
    }
    
    /* Free cluster chain */
    if (entry.cluster != 0) {
        rc = uft_fat_free_chain(ctx, entry.cluster);
        if (rc != 0) return rc;
    }
    
    /* Mark entry as deleted */
    rc = mark_entry_deleted(ctx, entry.dir_cluster, entry.dir_entry_index, 
                           entry.lfn_count);
    
    if (rc == 0) {
        ctx->modified = true;
    }
    
    return rc;
}

/*===========================================================================
 * File Rename/Move
 *===========================================================================*/

int uft_fat_rename(uft_fat_ctx_t *ctx, const char *old_path, const char *new_path) {
    if (!ctx || !old_path || !new_path) return UFT_FAT_ERR_INVALID;
    if (ctx->read_only) return UFT_FAT_ERR_READONLY;
    
    /* Find source file */
    uft_fat_entry_t entry;
    int rc = uft_fat_find_path(ctx, old_path, &entry);
    if (rc != 0) return rc;
    
    /* Check destination doesn't exist */
    uft_fat_entry_t dest_entry;
    if (uft_fat_find_path(ctx, new_path, &dest_entry) == 0) {
        return UFT_FAT_ERR_EXISTS;
    }
    
    /* Parse new path */
    uint32_t new_parent;
    char new_name[UFT_FAT_MAX_LFN + 1];
    rc = parse_path(new_path, &new_parent, new_name, ctx);
    if (rc != 0) return rc;
    
    /* Same directory? Just update name */
    if (new_parent == entry.dir_cluster) {
        /* Update name in place */
        uint8_t sfn[11];
        make_sfn(new_name, sfn);
        
        if (entry.dir_cluster == 0) {
            uint32_t entries_per_sector = UFT_FAT_SECTOR_SIZE / 32;
            uint32_t sector = entry.dir_entry_index / entries_per_sector;
            uint32_t offset = (entry.dir_entry_index % entries_per_sector) * 32;
            
            uint8_t sector_buf[UFT_FAT_SECTOR_SIZE];
            rc = uft_fat_read_root_sector(ctx, sector, sector_buf);
            if (rc != 0) return rc;
            
            memcpy(sector_buf + offset, sfn, 11);
            
            rc = uft_fat_write_root_sector(ctx, sector, sector_buf);
        } else {
            size_t clust_sz = cluster_size(ctx);
            uint32_t entries_per_cluster = clust_sz / 32;
            
            uint8_t *cluster_buf = malloc(clust_sz);
            if (!cluster_buf) return UFT_FAT_ERR_NOMEM;
            
            uint32_t current = entry.dir_cluster;
            uint32_t remaining = entry.dir_entry_index;
            
            while (remaining >= entries_per_cluster) {
                uint32_t next;
                uft_fat_get_entry(ctx, current, &next);
                if (uft_fat_cluster_is_eof(ctx, next)) {
                    free(cluster_buf);
                    return UFT_FAT_ERR_INVALID;
                }
                current = next;
                remaining -= entries_per_cluster;
            }
            
            rc = uft_fat_read_cluster(ctx, current, cluster_buf);
            if (rc != 0) {
                free(cluster_buf);
                return rc;
            }
            
            memcpy(cluster_buf + remaining * 32, sfn, 11);
            rc = uft_fat_write_cluster(ctx, current, cluster_buf);
            free(cluster_buf);
        }
    } else {
        /* Move to different directory - delete and recreate entry */
        /* Save file data */
        size_t file_size = entry.size;
        uint8_t *data = NULL;
        
        if (!entry.is_directory && file_size > 0) {
            data = uft_fat_extract(ctx, &entry, NULL, &file_size);
            if (!data) return UFT_FAT_ERR_NOMEM;
        }
        
        /* Delete old entry (but keep cluster chain) */
        rc = mark_entry_deleted(ctx, entry.dir_cluster, entry.dir_entry_index,
                               entry.lfn_count);
        if (rc != 0) {
            free(data);
            return rc;
        }
        
        /* Create new entry with same cluster chain */
        uint8_t sfn[11];
        make_sfn(new_name, sfn);
        
        uint32_t slot_cluster, slot_offset;
        if (find_free_dir_slot(ctx, new_parent, 1, &slot_cluster, &slot_offset) != 0) {
            free(data);
            return UFT_FAT_ERR_DISKFULL;
        }
        
        rc = write_dir_entry(ctx, new_parent, slot_cluster, slot_offset,
                             sfn, entry.attributes, entry.cluster, entry.size);
        
        free(data);
    }
    
    if (rc == 0) {
        ctx->modified = true;
    }
    
    return rc;
}

/*===========================================================================
 * Directory Creation/Removal
 *===========================================================================*/

int uft_fat_mkdir(uft_fat_ctx_t *ctx, const char *path) {
    if (!ctx || !path) return UFT_FAT_ERR_INVALID;
    if (ctx->read_only) return UFT_FAT_ERR_READONLY;
    
    /* Parse path */
    uint32_t parent_cluster;
    char dirname[UFT_FAT_MAX_LFN + 1];
    int rc = parse_path(path, &parent_cluster, dirname, ctx);
    if (rc != 0) return rc;
    
    /* Check doesn't exist */
    uft_fat_entry_t existing;
    if (uft_fat_find_entry(ctx, parent_cluster, dirname, &existing) == 0) {
        return UFT_FAT_ERR_EXISTS;
    }
    
    /* Allocate cluster for directory */
    uft_fat_chain_t chain;
    uft_fat_chain_init(&chain);
    rc = uft_fat_alloc_chain(ctx, 1, &chain);
    if (rc != 0 || chain.count == 0) {
        uft_fat_chain_free(&chain);
        return UFT_FAT_ERR_DISKFULL;
    }
    
    uint32_t dir_cluster = chain.clusters[0];
    size_t clust_sz = cluster_size(ctx);
    
    /* Initialize directory cluster with . and .. */
    uint8_t *cluster_buf = calloc(1, clust_sz);
    if (!cluster_buf) {
        uft_fat_free_chain(ctx, dir_cluster);
        uft_fat_chain_free(&chain);
        return UFT_FAT_ERR_NOMEM;
    }
    
    /* . entry (self) */
    uint8_t dot_entry[32];
    memset(dot_entry, ' ', 11);
    dot_entry[0] = '.';
    dot_entry[11] = UFT_FAT_ATTR_DIRECTORY;
    memset(dot_entry + 12, 0, 20);
    write_le16(dot_entry + 26, dir_cluster & 0xFFFF);
    memcpy(cluster_buf, dot_entry, 32);
    
    /* .. entry (parent) */
    uint8_t dotdot_entry[32];
    memset(dotdot_entry, ' ', 11);
    dotdot_entry[0] = '.';
    dotdot_entry[1] = '.';
    dotdot_entry[11] = UFT_FAT_ATTR_DIRECTORY;
    memset(dotdot_entry + 12, 0, 20);
    write_le16(dotdot_entry + 26, parent_cluster & 0xFFFF);
    memcpy(cluster_buf + 32, dotdot_entry, 32);
    
    rc = uft_fat_write_cluster(ctx, dir_cluster, cluster_buf);
    free(cluster_buf);
    
    if (rc != 0) {
        uft_fat_free_chain(ctx, dir_cluster);
        uft_fat_chain_free(&chain);
        return rc;
    }
    
    /* Create directory entry in parent */
    uint8_t sfn[11];
    make_sfn(dirname, sfn);
    
    uint32_t slot_cluster, slot_offset;
    if (find_free_dir_slot(ctx, parent_cluster, 1, &slot_cluster, &slot_offset) != 0) {
        uft_fat_free_chain(ctx, dir_cluster);
        uft_fat_chain_free(&chain);
        return UFT_FAT_ERR_DISKFULL;
    }
    
    rc = write_dir_entry(ctx, parent_cluster, slot_cluster, slot_offset,
                         sfn, UFT_FAT_ATTR_DIRECTORY, dir_cluster, 0);
    
    uft_fat_chain_free(&chain);
    
    if (rc == 0) {
        ctx->modified = true;
    }
    
    return rc;
}

int uft_fat_rmdir(uft_fat_ctx_t *ctx, const char *path) {
    if (!ctx || !path) return UFT_FAT_ERR_INVALID;
    if (ctx->read_only) return UFT_FAT_ERR_READONLY;
    
    /* Find directory */
    uft_fat_entry_t entry;
    int rc = uft_fat_find_path(ctx, path, &entry);
    if (rc != 0) return rc;
    
    if (!entry.is_directory) {
        return UFT_FAT_ERR_INVALID;
    }
    
    /* Check if empty */
    uft_fat_dir_t dir;
    uft_fat_dir_init(&dir);
    rc = uft_fat_read_dir(ctx, entry.cluster, &dir);
    if (rc != 0) {
        uft_fat_dir_free(&dir);
        return rc;
    }
    
    if (dir.count > 0) {
        uft_fat_dir_free(&dir);
        return UFT_FAT_ERR_DIRNOTEMPTY;
    }
    uft_fat_dir_free(&dir);
    
    /* Free cluster chain */
    if (entry.cluster != 0) {
        rc = uft_fat_free_chain(ctx, entry.cluster);
        if (rc != 0) return rc;
    }
    
    /* Delete directory entry */
    rc = mark_entry_deleted(ctx, entry.dir_cluster, entry.dir_entry_index,
                           entry.lfn_count);
    
    if (rc == 0) {
        ctx->modified = true;
    }
    
    return rc;
}

/*===========================================================================
 * Attribute/Time Operations
 *===========================================================================*/

int uft_fat_set_attr(uft_fat_ctx_t *ctx, const char *path, uint8_t attr) {
    if (!ctx || !path) return UFT_FAT_ERR_INVALID;
    if (ctx->read_only) return UFT_FAT_ERR_READONLY;
    
    uft_fat_entry_t entry;
    int rc = uft_fat_find_path(ctx, path, &entry);
    if (rc != 0) return rc;
    
    /* Update attribute byte */
    if (entry.dir_cluster == 0) {
        uint32_t entries_per_sector = UFT_FAT_SECTOR_SIZE / 32;
        uint32_t sector = entry.dir_entry_index / entries_per_sector;
        uint32_t offset = (entry.dir_entry_index % entries_per_sector) * 32;
        
        uint8_t sector_buf[UFT_FAT_SECTOR_SIZE];
        rc = uft_fat_read_root_sector(ctx, sector, sector_buf);
        if (rc != 0) return rc;
        
        sector_buf[offset + 11] = attr;
        
        rc = uft_fat_write_root_sector(ctx, sector, sector_buf);
    } else {
        size_t clust_sz = cluster_size(ctx);
        uint8_t *cluster_buf = malloc(clust_sz);
        if (!cluster_buf) return UFT_FAT_ERR_NOMEM;
        
        /* Find cluster containing entry */
        uint32_t entries_per_cluster = clust_sz / 32;
        uint32_t current = entry.dir_cluster;
        uint32_t remaining = entry.dir_entry_index;
        
        while (remaining >= entries_per_cluster) {
            uint32_t next;
            uft_fat_get_entry(ctx, current, &next);
            if (uft_fat_cluster_is_eof(ctx, next)) {
                free(cluster_buf);
                return UFT_FAT_ERR_INVALID;
            }
            current = next;
            remaining -= entries_per_cluster;
        }
        
        rc = uft_fat_read_cluster(ctx, current, cluster_buf);
        if (rc == 0) {
            cluster_buf[remaining * 32 + 11] = attr;
            rc = uft_fat_write_cluster(ctx, current, cluster_buf);
        }
        free(cluster_buf);
    }
    
    if (rc == 0) {
        ctx->modified = true;
    }
    
    return rc;
}

int uft_fat_set_time(uft_fat_ctx_t *ctx, const char *path, time_t mtime) {
    if (!ctx || !path) return UFT_FAT_ERR_INVALID;
    if (ctx->read_only) return UFT_FAT_ERR_READONLY;
    
    uft_fat_entry_t entry;
    int rc = uft_fat_find_path(ctx, path, &entry);
    if (rc != 0) return rc;
    
    uint16_t fat_time, fat_date;
    uft_fat_from_unix_time(mtime, &fat_time, &fat_date);
    
    if (entry.dir_cluster == 0) {
        uint32_t entries_per_sector = UFT_FAT_SECTOR_SIZE / 32;
        uint32_t sector = entry.dir_entry_index / entries_per_sector;
        uint32_t offset = (entry.dir_entry_index % entries_per_sector) * 32;
        
        uint8_t sector_buf[UFT_FAT_SECTOR_SIZE];
        rc = uft_fat_read_root_sector(ctx, sector, sector_buf);
        if (rc != 0) return rc;
        
        write_le16(sector_buf + offset + 22, fat_time);
        write_le16(sector_buf + offset + 24, fat_date);
        
        rc = uft_fat_write_root_sector(ctx, sector, sector_buf);
    } else {
        size_t clust_sz = cluster_size(ctx);
        uint8_t *cluster_buf = malloc(clust_sz);
        if (!cluster_buf) return UFT_FAT_ERR_NOMEM;
        
        uint32_t entries_per_cluster = clust_sz / 32;
        uint32_t current = entry.dir_cluster;
        uint32_t remaining = entry.dir_entry_index;
        
        while (remaining >= entries_per_cluster) {
            uint32_t next;
            uft_fat_get_entry(ctx, current, &next);
            if (uft_fat_cluster_is_eof(ctx, next)) {
                free(cluster_buf);
                return UFT_FAT_ERR_INVALID;
            }
            current = next;
            remaining -= entries_per_cluster;
        }
        
        rc = uft_fat_read_cluster(ctx, current, cluster_buf);
        if (rc == 0) {
            write_le16(cluster_buf + remaining * 32 + 22, fat_time);
            write_le16(cluster_buf + remaining * 32 + 24, fat_date);
            rc = uft_fat_write_cluster(ctx, current, cluster_buf);
        }
        free(cluster_buf);
    }
    
    if (rc == 0) {
        ctx->modified = true;
    }
    
    return rc;
}
