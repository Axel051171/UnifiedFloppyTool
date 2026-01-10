/**
 * @file uft_fat12_dir.c
 * @brief FAT12/FAT16 Directory Operations
 * @version 3.7.0
 * 
 * Directory parsing, entry finding, LFN assembly, iteration
 */

#include "uft/fs/uft_fat12.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*===========================================================================
 * Internal Helpers
 *===========================================================================*/

/** Read little-endian 16-bit */
static inline uint16_t read_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/** Read little-endian 32-bit */
static inline uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
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

/** Case-insensitive name compare for 8.3 */
static int sfn_compare(const char *a, const char *b) {
    while (*a && *b) {
        char ca = toupper((unsigned char)*a);
        char cb = toupper((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

/*===========================================================================
 * Directory Entry Parsing
 *===========================================================================*/

/**
 * @brief Parse short filename from directory entry
 */
static void parse_sfn(const uint8_t *raw, char sfn[13]) {
    int i, j = 0;
    
    /* Name part (8 bytes) */
    for (i = 0; i < 8 && raw[i] != ' '; i++) {
        /* Handle 0x05 -> 0xE5 (Kanji lead byte) */
        sfn[j++] = (raw[i] == 0x05) ? (char)0xE5 : raw[i];
    }
    
    /* Extension part (3 bytes) */
    if (raw[8] != ' ') {
        sfn[j++] = '.';
        for (i = 8; i < 11 && raw[i] != ' '; i++) {
            sfn[j++] = raw[i];
        }
    }
    
    sfn[j] = '\0';
}

/**
 * @brief Parse LFN entry characters to buffer
 */
static void parse_lfn_chars(const uft_fat_lfn_t *lfn, uint16_t *chars) {
    /* Characters in LFN entry: 5 + 6 + 2 = 13 per entry */
    memcpy(&chars[0], lfn->name1, 10);   /* chars 1-5 */
    memcpy(&chars[5], lfn->name2, 12);   /* chars 6-11 */
    memcpy(&chars[11], lfn->name3, 4);   /* chars 12-13 */
}

/**
 * @brief Convert UCS-2 to UTF-8
 */
static size_t ucs2_to_utf8(const uint16_t *src, size_t src_len, char *dst, size_t dst_size) {
    size_t out = 0;
    
    for (size_t i = 0; i < src_len && out < dst_size - 1; i++) {
        uint16_t c = src[i];
        
        /* End marker */
        if (c == 0x0000 || c == 0xFFFF) break;
        
        if (c < 0x80) {
            dst[out++] = (char)c;
        } else if (c < 0x800) {
            if (out + 1 >= dst_size) break;
            dst[out++] = 0xC0 | (c >> 6);
            dst[out++] = 0x80 | (c & 0x3F);
        } else {
            if (out + 2 >= dst_size) break;
            dst[out++] = 0xE0 | (c >> 12);
            dst[out++] = 0x80 | ((c >> 6) & 0x3F);
            dst[out++] = 0x80 | (c & 0x3F);
        }
    }
    
    dst[out] = '\0';
    return out;
}

/**
 * @brief Parse single directory entry
 */
static bool parse_dirent(const uint8_t *raw, uft_fat_entry_t *entry,
                         const uint16_t *lfn_buffer, size_t lfn_chars) {
    const uft_fat_dirent_t *de = (const uft_fat_dirent_t *)raw;
    
    memset(entry, 0, sizeof(*entry));
    
    /* Check first byte */
    if (de->name[0] == UFT_FAT_DIRENT_END) {
        return false;  /* End of directory */
    }
    
    /* Deleted entry */
    entry->is_deleted = (de->name[0] == UFT_FAT_DIRENT_FREE);
    
    /* Skip LFN entries (handled separately) */
    if ((de->attributes & UFT_FAT_ATTR_LFN_MASK) == UFT_FAT_ATTR_LFN) {
        return false;
    }
    
    /* Volume label */
    if (de->attributes & UFT_FAT_ATTR_VOLUME_ID) {
        entry->is_volume_label = true;
        memcpy(entry->sfn, de->name, 11);
        entry->sfn[11] = '\0';
        return true;
    }
    
    /* Parse short filename */
    parse_sfn(de->name, entry->sfn);
    
    /* Long filename from accumulated LFN buffer */
    if (lfn_chars > 0) {
        ucs2_to_utf8(lfn_buffer, lfn_chars, entry->lfn, UFT_FAT_MAX_LFN + 1);
        entry->has_lfn = true;
    } else {
        strncpy(entry->lfn, entry->sfn, UFT_FAT_MAX_LFN);
    }
    
    /* Attributes */
    entry->attributes = de->attributes;
    entry->is_directory = (de->attributes & UFT_FAT_ATTR_DIRECTORY) != 0;
    
    /* Cluster */
    entry->cluster = read_le16((const uint8_t *)&de->cluster_lo);
    if (de->cluster_hi) {
        entry->cluster |= ((uint32_t)read_le16((const uint8_t *)&de->cluster_hi) << 16);
    }
    
    /* Size (0 for directories) */
    entry->size = read_le32((const uint8_t *)&de->file_size);
    
    /* Timestamps */
    entry->create_time = uft_fat_to_unix_time(
        read_le16((const uint8_t *)&de->create_time),
        read_le16((const uint8_t *)&de->create_date)
    );
    entry->modify_time = uft_fat_to_unix_time(
        read_le16((const uint8_t *)&de->modify_time),
        read_le16((const uint8_t *)&de->modify_date)
    );
    entry->access_time = uft_fat_to_unix_time(
        0,
        read_le16((const uint8_t *)&de->access_date)
    );
    
    return true;
}

/*===========================================================================
 * Directory Lifecycle
 *===========================================================================*/

void uft_fat_dir_init(uft_fat_dir_t *dir) {
    if (!dir) return;
    memset(dir, 0, sizeof(*dir));
}

void uft_fat_dir_free(uft_fat_dir_t *dir) {
    if (!dir) return;
    if (dir->entries) {
        free(dir->entries);
    }
    memset(dir, 0, sizeof(*dir));
}

/**
 * @brief Grow directory entry array
 */
static int dir_grow(uft_fat_dir_t *dir) {
    size_t new_cap = dir->capacity == 0 ? 32 : dir->capacity * 2;
    uft_fat_entry_t *new_entries = realloc(dir->entries, new_cap * sizeof(uft_fat_entry_t));
    if (!new_entries) return UFT_FAT_ERR_NOMEM;
    dir->entries = new_entries;
    dir->capacity = new_cap;
    return 0;
}

/*===========================================================================
 * Directory Reading
 *===========================================================================*/

/**
 * @brief Read directory entries from root directory
 */
static int read_root_dir(const uft_fat_ctx_t *ctx, uft_fat_dir_t *dir) {
    uint8_t sector[UFT_FAT_SECTOR_SIZE];
    uint16_t lfn_buffer[256];
    size_t lfn_chars = 0;
    uint8_t expected_seq = 0;
    uint8_t lfn_checksum = 0;
    uint32_t lfn_start_index = 0;
    
    dir->cluster = 0;
    dir->path[0] = '/';
    dir->path[1] = '\0';
    
    uint32_t entries_per_sector = UFT_FAT_SECTOR_SIZE / 32;
    uint32_t entry_index = 0;
    
    for (uint32_t s = 0; s < ctx->vol.root_dir_sectors; s++) {
        int rc = uft_fat_read_root_sector(ctx, s, sector);
        if (rc != 0) return rc;
        
        for (uint32_t e = 0; e < entries_per_sector; e++, entry_index++) {
            const uint8_t *raw = sector + e * 32;
            
            /* End of directory */
            if (raw[0] == UFT_FAT_DIRENT_END) {
                return 0;
            }
            
            /* Skip empty entries */
            if (raw[0] == UFT_FAT_DIRENT_FREE) {
                lfn_chars = 0;
                expected_seq = 0;
                continue;
            }
            
            /* LFN entry */
            if ((raw[11] & UFT_FAT_ATTR_LFN_MASK) == UFT_FAT_ATTR_LFN) {
                const uft_fat_lfn_t *lfn = (const uft_fat_lfn_t *)raw;
                uint8_t seq = lfn->sequence;
                
                /* First (last physical) LFN entry */
                if (seq & UFT_FAT_LFN_LAST) {
                    expected_seq = seq & UFT_FAT_LFN_SEQ_MASK;
                    lfn_checksum = lfn->checksum;
                    lfn_chars = expected_seq * 13;
                    if (lfn_chars > 255) lfn_chars = 255;
                    memset(lfn_buffer, 0xFF, sizeof(lfn_buffer));
                    lfn_start_index = entry_index;
                }
                
                /* Validate sequence */
                uint8_t this_seq = seq & UFT_FAT_LFN_SEQ_MASK;
                if (this_seq == expected_seq && lfn->checksum == lfn_checksum) {
                    /* Extract characters */
                    uint16_t chars[13];
                    parse_lfn_chars(lfn, chars);
                    
                    size_t offset = (this_seq - 1) * 13;
                    for (int i = 0; i < 13 && offset + i < 255; i++) {
                        lfn_buffer[offset + i] = chars[i];
                    }
                    expected_seq--;
                } else {
                    /* Sequence error, reset */
                    lfn_chars = 0;
                    expected_seq = 0;
                }
                continue;
            }
            
            /* Regular entry */
            if (dir->count >= dir->capacity) {
                int rc = dir_grow(dir);
                if (rc != 0) return rc;
            }
            
            uft_fat_entry_t *entry = &dir->entries[dir->count];
            if (parse_dirent(raw, entry, lfn_buffer, (expected_seq == 0) ? lfn_chars : 0)) {
                /* Verify LFN checksum */
                if (entry->has_lfn) {
                    uint8_t calc_sum = uft_fat_lfn_checksum((const char *)raw);
                    if (calc_sum != lfn_checksum) {
                        /* Checksum mismatch, discard LFN */
                        entry->has_lfn = false;
                        strncpy(entry->lfn, entry->sfn, UFT_FAT_MAX_LFN);
                    } else {
                        entry->lfn_start_index = lfn_start_index;
                        entry->lfn_count = (uint8_t)((entry_index - lfn_start_index));
                    }
                }
                
                entry->dir_cluster = 0;
                entry->dir_entry_index = entry_index;
                
                if (!entry->is_volume_label) {
                    dir->count++;
                }
            }
            
            /* Reset LFN state */
            lfn_chars = 0;
            expected_seq = 0;
        }
    }
    
    return 0;
}

/**
 * @brief Read directory entries from cluster chain
 */
static int read_subdir(const uft_fat_ctx_t *ctx, uint32_t cluster, uft_fat_dir_t *dir) {
    uint8_t *cluster_buf = malloc(ctx->vol.sectors_per_cluster * UFT_FAT_SECTOR_SIZE);
    if (!cluster_buf) return UFT_FAT_ERR_NOMEM;
    
    uint16_t lfn_buffer[256];
    size_t lfn_chars = 0;
    uint8_t expected_seq = 0;
    uint8_t lfn_checksum = 0;
    uint32_t lfn_start_index = 0;
    
    dir->cluster = cluster;
    
    uint32_t entries_per_cluster = (ctx->vol.sectors_per_cluster * UFT_FAT_SECTOR_SIZE) / 32;
    uint32_t entry_index = 0;
    uint32_t current = cluster;
    
    while (!uft_fat_cluster_is_eof(ctx, current)) {
        /* Read cluster */
        int rc = uft_fat_read_cluster(ctx, current, cluster_buf);
        if (rc != 0) {
            free(cluster_buf);
            return rc;
        }
        
        for (uint32_t e = 0; e < entries_per_cluster; e++, entry_index++) {
            const uint8_t *raw = cluster_buf + e * 32;
            
            /* End of directory */
            if (raw[0] == UFT_FAT_DIRENT_END) {
                free(cluster_buf);
                return 0;
            }
            
            /* Skip empty entries */
            if (raw[0] == UFT_FAT_DIRENT_FREE) {
                lfn_chars = 0;
                expected_seq = 0;
                continue;
            }
            
            /* LFN entry */
            if ((raw[11] & UFT_FAT_ATTR_LFN_MASK) == UFT_FAT_ATTR_LFN) {
                const uft_fat_lfn_t *lfn = (const uft_fat_lfn_t *)raw;
                uint8_t seq = lfn->sequence;
                
                if (seq & UFT_FAT_LFN_LAST) {
                    expected_seq = seq & UFT_FAT_LFN_SEQ_MASK;
                    lfn_checksum = lfn->checksum;
                    lfn_chars = expected_seq * 13;
                    if (lfn_chars > 255) lfn_chars = 255;
                    memset(lfn_buffer, 0xFF, sizeof(lfn_buffer));
                    lfn_start_index = entry_index;
                }
                
                uint8_t this_seq = seq & UFT_FAT_LFN_SEQ_MASK;
                if (this_seq == expected_seq && lfn->checksum == lfn_checksum) {
                    uint16_t chars[13];
                    parse_lfn_chars(lfn, chars);
                    
                    size_t offset = (this_seq - 1) * 13;
                    for (int i = 0; i < 13 && offset + i < 255; i++) {
                        lfn_buffer[offset + i] = chars[i];
                    }
                    expected_seq--;
                } else {
                    lfn_chars = 0;
                    expected_seq = 0;
                }
                continue;
            }
            
            /* Regular entry */
            if (dir->count >= dir->capacity) {
                rc = dir_grow(dir);
                if (rc != 0) {
                    free(cluster_buf);
                    return rc;
                }
            }
            
            uft_fat_entry_t *entry = &dir->entries[dir->count];
            if (parse_dirent(raw, entry, lfn_buffer, (expected_seq == 0) ? lfn_chars : 0)) {
                if (entry->has_lfn) {
                    uint8_t calc_sum = uft_fat_lfn_checksum((const char *)raw);
                    if (calc_sum != lfn_checksum) {
                        entry->has_lfn = false;
                        strncpy(entry->lfn, entry->sfn, UFT_FAT_MAX_LFN);
                    } else {
                        entry->lfn_start_index = lfn_start_index;
                        entry->lfn_count = (uint8_t)((entry_index - lfn_start_index));
                    }
                }
                
                entry->dir_cluster = cluster;
                entry->dir_entry_index = entry_index;
                
                /* Skip . and .. entries */
                if (entry->sfn[0] != '.') {
                    dir->count++;
                }
            }
            
            lfn_chars = 0;
            expected_seq = 0;
        }
        
        /* Next cluster */
        uint32_t next;
        int rc2 = uft_fat_get_entry(ctx, current, &next);
        if (rc2 != 0) {
            free(cluster_buf);
            return rc2;
        }
        current = next;
    }
    
    free(cluster_buf);
    return 0;
}

int uft_fat_read_dir(const uft_fat_ctx_t *ctx, uint32_t cluster, uft_fat_dir_t *dir) {
    if (!ctx || !ctx->data || !dir) return UFT_FAT_ERR_INVALID;
    
    uft_fat_dir_init(dir);
    
    if (cluster == 0) {
        return read_root_dir(ctx, dir);
    } else {
        return read_subdir(ctx, cluster, dir);
    }
}

/*===========================================================================
 * Path Resolution
 *===========================================================================*/

/**
 * @brief Resolve path to directory cluster
 */
static int resolve_dir_path(const uft_fat_ctx_t *ctx, const char *path,
                            uint32_t *cluster_out, char *last_component) {
    if (!ctx || !path) return UFT_FAT_ERR_INVALID;
    
    /* Start at root */
    uint32_t cluster = 0;
    
    /* Skip leading separator */
    while (*path == '/' || *path == '\\') path++;
    
    if (*path == '\0') {
        /* Root directory */
        if (cluster_out) *cluster_out = 0;
        if (last_component) last_component[0] = '\0';
        return 0;
    }
    
    /* Tokenize path */
    char pathbuf[UFT_FAT_MAX_PATH];
    strncpy(pathbuf, path, UFT_FAT_MAX_PATH - 1);
    pathbuf[UFT_FAT_MAX_PATH - 1] = '\0';
    
    char *saveptr = NULL;
    char *token = strtok_r(pathbuf, "/\\", &saveptr);
    char *next_token = NULL;
    
    while (token) {
        next_token = strtok_r(NULL, "/\\", &saveptr);
        
        /* If no more tokens, this is the last component */
        if (next_token == NULL) {
            if (last_component) {
                strncpy(last_component, token, UFT_FAT_MAX_SFN);
            }
            if (cluster_out) *cluster_out = cluster;
            return 0;
        }
        
        /* Find this component in current directory */
        uft_fat_entry_t entry;
        int rc = uft_fat_find_entry(ctx, cluster, token, &entry);
        if (rc != 0) return rc;
        
        /* Must be a directory */
        if (!entry.is_directory) {
            return UFT_FAT_ERR_NOTFOUND;
        }
        
        cluster = entry.cluster;
        token = next_token;
    }
    
    if (cluster_out) *cluster_out = cluster;
    if (last_component) last_component[0] = '\0';
    return 0;
}

int uft_fat_read_dir_path(const uft_fat_ctx_t *ctx, const char *path, uft_fat_dir_t *dir) {
    if (!ctx || !path || !dir) return UFT_FAT_ERR_INVALID;
    
    /* Resolve full path */
    uint32_t cluster = 0;
    
    /* Skip leading separator */
    const char *p = path;
    while (*p == '/' || *p == '\\') p++;
    
    if (*p == '\0') {
        /* Root directory */
        return uft_fat_read_dir(ctx, 0, dir);
    }
    
    /* Find the directory */
    uft_fat_entry_t entry;
    int rc = uft_fat_find_path(ctx, path, &entry);
    if (rc != 0) return rc;
    
    if (!entry.is_directory) {
        return UFT_FAT_ERR_INVALID;
    }
    
    rc = uft_fat_read_dir(ctx, entry.cluster, dir);
    if (rc == 0) {
        strncpy(dir->path, path, UFT_FAT_MAX_PATH - 1);
        dir->path[UFT_FAT_MAX_PATH - 1] = '\0';
    }
    return rc;
}

/*===========================================================================
 * Entry Finding
 *===========================================================================*/

int uft_fat_find_entry(const uft_fat_ctx_t *ctx, uint32_t cluster,
                       const char *name, uft_fat_entry_t *entry) {
    if (!ctx || !name || !entry) return UFT_FAT_ERR_INVALID;
    
    uft_fat_dir_t dir;
    uft_fat_dir_init(&dir);
    
    int rc = uft_fat_read_dir(ctx, cluster, &dir);
    if (rc != 0) {
        uft_fat_dir_free(&dir);
        return rc;
    }
    
    /* Search for name (case-insensitive) */
    for (size_t i = 0; i < dir.count; i++) {
        /* Check SFN */
        if (sfn_compare(dir.entries[i].sfn, name) == 0) {
            *entry = dir.entries[i];
            uft_fat_dir_free(&dir);
            return 0;
        }
        
        /* Check LFN */
        if (dir.entries[i].has_lfn) {
            if (sfn_compare(dir.entries[i].lfn, name) == 0) {
                *entry = dir.entries[i];
                uft_fat_dir_free(&dir);
                return 0;
            }
        }
    }
    
    uft_fat_dir_free(&dir);
    return UFT_FAT_ERR_NOTFOUND;
}

int uft_fat_find_path(const uft_fat_ctx_t *ctx, const char *path, uft_fat_entry_t *entry) {
    if (!ctx || !path || !entry) return UFT_FAT_ERR_INVALID;
    
    /* Handle empty path or root */
    const char *p = path;
    while (*p == '/' || *p == '\\') p++;
    
    if (*p == '\0') {
        /* Root directory - create fake entry */
        memset(entry, 0, sizeof(*entry));
        strncpy(entry->sfn, "/", 13); entry->sfn[12] = '\0';
        strncpy(entry->lfn, "/", 255); entry->lfn[254] = '\0';
        entry->is_directory = true;
        entry->cluster = 0;
        return 0;
    }
    
    /* Resolve path to parent directory + last component */
    uint32_t parent_cluster;
    char filename[UFT_FAT_MAX_LFN + 1];
    
    int rc = resolve_dir_path(ctx, path, &parent_cluster, filename);
    if (rc != 0) return rc;
    
    /* If no filename, this is a directory reference */
    if (filename[0] == '\0') {
        memset(entry, 0, sizeof(*entry));
        entry->is_directory = true;
        entry->cluster = parent_cluster;
        return 0;
    }
    
    /* Find in parent directory */
    return uft_fat_find_entry(ctx, parent_cluster, filename, entry);
}

/*===========================================================================
 * Directory Iteration
 *===========================================================================*/

int uft_fat_foreach_entry(const uft_fat_ctx_t *ctx, uint32_t cluster,
                          uft_fat_dir_callback_t callback, void *user_data) {
    if (!ctx || !callback) return UFT_FAT_ERR_INVALID;
    
    uft_fat_dir_t dir;
    uft_fat_dir_init(&dir);
    
    int rc = uft_fat_read_dir(ctx, cluster, &dir);
    if (rc != 0) {
        uft_fat_dir_free(&dir);
        return rc;
    }
    
    for (size_t i = 0; i < dir.count; i++) {
        int cb_rc = callback(&dir.entries[i], user_data);
        if (cb_rc != 0) {
            uft_fat_dir_free(&dir);
            return cb_rc;
        }
    }
    
    uft_fat_dir_free(&dir);
    return 0;
}

/**
 * @brief Recursive file iteration context
 */
typedef struct {
    const uft_fat_ctx_t *ctx;
    uft_fat_dir_callback_t callback;
    void *user_data;
    char path[UFT_FAT_MAX_PATH];
    int depth;
} foreach_ctx_t;

/**
 * @brief Recursive file iteration helper
 */
static int foreach_recursive(foreach_ctx_t *fctx, uint32_t cluster) {
    if (fctx->depth > 32) return UFT_FAT_ERR_INVALID;  /* Prevent deep recursion */
    
    uft_fat_dir_t dir;
    uft_fat_dir_init(&dir);
    
    int rc = uft_fat_read_dir(fctx->ctx, cluster, &dir);
    if (rc != 0) {
        uft_fat_dir_free(&dir);
        return rc;
    }
    
    size_t path_len = strlen(fctx->path);
    
    for (size_t i = 0; i < dir.count; i++) {
        const uft_fat_entry_t *entry = &dir.entries[i];
        
        /* Call callback for all entries */
        int cb_rc = fctx->callback(entry, fctx->user_data);
        if (cb_rc != 0) {
            uft_fat_dir_free(&dir);
            return cb_rc;
        }
        
        /* Recurse into subdirectories */
        if (entry->is_directory && entry->cluster != 0) {
            /* Build path */
            size_t name_len = strlen(entry->has_lfn ? entry->lfn : entry->sfn);
            if (path_len + name_len + 2 < UFT_FAT_MAX_PATH) {
                if (path_len > 1) {
                    fctx->path[path_len] = '/';
                    strncpy(fctx->path + path_len + 1, 
                           entry->has_lfn ? entry->lfn : entry->sfn,
                           UFT_FAT_MAX_PATH - path_len - 2);
                } else {
                    strncpy(fctx->path + path_len,
                           entry->has_lfn ? entry->lfn : entry->sfn,
                           UFT_FAT_MAX_PATH - path_len - 1);
                }
                
                fctx->depth++;
                rc = foreach_recursive(fctx, entry->cluster);
                fctx->depth--;
                fctx->path[path_len] = '\0';
                
                if (rc != 0) {
                    uft_fat_dir_free(&dir);
                    return rc;
                }
            }
        }
    }
    
    uft_fat_dir_free(&dir);
    return 0;
}

int uft_fat_foreach_file(const uft_fat_ctx_t *ctx, uint32_t cluster,
                         uft_fat_dir_callback_t callback, void *user_data) {
    if (!ctx || !callback) return UFT_FAT_ERR_INVALID;
    
    foreach_ctx_t fctx;
    fctx.ctx = ctx;
    fctx.callback = callback;
    fctx.user_data = user_data;
    fctx.path[0] = '/';
    fctx.path[1] = '\0';
    fctx.depth = 0;
    
    return foreach_recursive(&fctx, cluster);
}

/*===========================================================================
 * Directory Printing
 *===========================================================================*/

void uft_fat_print_dir(const uft_fat_ctx_t *ctx, uint32_t cluster, FILE *fp) {
    if (!ctx) return;
    if (!fp) fp = stdout;
    
    uft_fat_dir_t dir;
    uft_fat_dir_init(&dir);
    
    if (uft_fat_read_dir(ctx, cluster, &dir) != 0) {
        fprintf(fp, "Error reading directory\n");
        return;
    }
    
    fprintf(fp, "Directory of cluster %u:\n", cluster);
    fprintf(fp, "%-12s %8s  %-19s  %s\n", "Name", "Size", "Modified", "Attr");
    fprintf(fp, "%-12s %8s  %-19s  %s\n", "----", "----", "--------", "----");
    
    for (size_t i = 0; i < dir.count; i++) {
        const uft_fat_entry_t *e = &dir.entries[i];
        
        char attr_str[8];
        uft_fat_attr_to_string(e->attributes, attr_str);
        
        char time_str[20];
        struct tm *tm = localtime(&e->modify_time);
        if (tm) {
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm);
        } else {
            memset(time_str, ' ', 19); time_str[19] = '\0';
        }
        
        const char *name = e->has_lfn ? e->lfn : e->sfn;
        
        if (e->is_directory) {
            fprintf(fp, "%-12s %8s  %-19s  %s\n", name, "<DIR>", time_str, attr_str);
        } else {
            fprintf(fp, "%-12s %8u  %-19s  %s\n", name, e->size, time_str, attr_str);
        }
    }
    
    fprintf(fp, "%zu file(s)\n", dir.count);
    
    uft_fat_dir_free(&dir);
}

/**
 * @brief Tree printing context
 */
typedef struct {
    FILE *fp;
    int depth;
} tree_ctx_t;

/**
 * @brief Tree printing callback
 */
static int tree_callback(const uft_fat_entry_t *entry, void *user_data) {
    tree_ctx_t *tctx = user_data;
    
    /* Indent */
    for (int i = 0; i < tctx->depth; i++) {
        fprintf(tctx->fp, "  ");
    }
    
    const char *name = entry->has_lfn ? entry->lfn : entry->sfn;
    
    if (entry->is_directory) {
        fprintf(tctx->fp, "[%s]\n", name);
        tctx->depth++;
    } else {
        fprintf(tctx->fp, "%s (%u bytes)\n", name, entry->size);
    }
    
    return 0;
}

void uft_fat_print_tree(const uft_fat_ctx_t *ctx, FILE *fp) {
    if (!ctx) return;
    if (!fp) fp = stdout;
    
    tree_ctx_t tctx = { .fp = fp, .depth = 0 };
    fprintf(fp, "/\n");
    
    uft_fat_foreach_file(ctx, 0, tree_callback, &tctx);
}

/*===========================================================================
 * Utilities
 *===========================================================================*/

char *uft_fat_entry_to_string(const uft_fat_entry_t *entry, char *buffer) {
    if (!entry || !buffer) return buffer;
    
    char attr_str[8];
    uft_fat_attr_to_string(entry->attributes, attr_str);
    
    const char *name = entry->has_lfn ? entry->lfn : entry->sfn;
    
    if (entry->is_directory) {
        snprintf(buffer, 80, "%-32s <DIR> %s", name, attr_str);
    } else {
        snprintf(buffer, 80, "%-32s %8u %s", name, entry->size, attr_str);
    }
    
    return buffer;
}

char *uft_fat_attr_to_string(uint8_t attr, char *buffer) {
    if (!buffer) return buffer;
    
    buffer[0] = (attr & UFT_FAT_ATTR_READONLY)  ? 'R' : '-';
    buffer[1] = (attr & UFT_FAT_ATTR_HIDDEN)    ? 'H' : '-';
    buffer[2] = (attr & UFT_FAT_ATTR_SYSTEM)    ? 'S' : '-';
    buffer[3] = (attr & UFT_FAT_ATTR_VOLUME_ID) ? 'V' : '-';
    buffer[4] = (attr & UFT_FAT_ATTR_DIRECTORY) ? 'D' : '-';
    buffer[5] = (attr & UFT_FAT_ATTR_ARCHIVE)   ? 'A' : '-';
    buffer[6] = '\0';
    
    return buffer;
}
