/**
 * @file uft_amigados_extended.c
 * @brief Extended AmigaDOS Operations Implementation
 * 
 * Inspired by amitools xdftool
 */

#include "uft/fs/uft_amigados_extended.h"
#include "uft/fs/uft_amigados.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Volume Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_amiga_get_volume_info(uft_amigados_ctx_t *ctx, 
                               uft_amiga_volume_info_t *info) {
    if (!ctx || !info) return -1;
    
    memset(info, 0, sizeof(*info));
    
    /* Get volume name from root block */
    if (ctx->volume_name[0]) {
        strncpy(info->name, ctx->volume_name, sizeof(info->name) - 1);
    }
    
    /* Filesystem type */
    info->dos_type = ctx->dos_type;
    const char *fs_names[] = {"OFS", "FFS", "OFS-INTL", "FFS-INTL", 
                               "OFS-DC", "FFS-DC", "LNFS", "LNFS2"};
    if (ctx->dos_type < 8) {
        strncpy(info->fs_type, fs_names[ctx->dos_type], sizeof(info->fs_type) - 1);
    }
    
    /* Block statistics */
    info->total_blocks = ctx->total_blocks;
    info->used_blocks = ctx->used_blocks;
    info->free_blocks = ctx->total_blocks - ctx->used_blocks;
    
    /* Size calculations */
    info->total_bytes = (uint64_t)info->total_blocks * UFT_AMIGA_BLOCK_SIZE;
    info->used_bytes = (uint64_t)info->used_blocks * UFT_AMIGA_BLOCK_SIZE;
    info->free_bytes = (uint64_t)info->free_blocks * UFT_AMIGA_BLOCK_SIZE;
    
    /* Dates */
    info->creation_date = ctx->creation_date;
    info->modification_date = ctx->modification_date;
    
    /* Bootable status */
    info->is_bootable = ctx->is_bootable;
    
    return 0;
}

int uft_amiga_relabel(uft_amigados_ctx_t *ctx, const char *new_name) {
    if (!ctx || !new_name) return -1;
    if (strlen(new_name) > 30) return -2;  /* Name too long */
    
    /* Update root block with new name */
    /* Read root block */
    uint8_t block[UFT_AMIGA_BLOCK_SIZE];
    if (uft_amigados_read_block(ctx, ctx->root_block, block) != 0) {
        return -3;
    }
    
    /* Update name (BCPL string at offset 432) */
    size_t name_len = strlen(new_name);
    block[432] = (uint8_t)name_len;
    memcpy(&block[433], new_name, name_len);
    
    /* Update modification date */
    time_t now = time(NULL);
    /* TODO: Convert to Amiga date format */
    
    /* Recalculate checksum */
    uint32_t sum = 0;
    for (int i = 0; i < 128; i++) {
        if (i != 5) {  /* Skip checksum longword */
            sum += ((uint32_t)block[i*4] << 24) |
                   ((uint32_t)block[i*4+1] << 16) |
                   ((uint32_t)block[i*4+2] << 8) |
                   (uint32_t)block[i*4+3];
        }
    }
    sum = -sum;
    block[20] = (sum >> 24) & 0xFF;
    block[21] = (sum >> 16) & 0xFF;
    block[22] = (sum >> 8) & 0xFF;
    block[23] = sum & 0xFF;
    
    /* Write back */
    if (uft_amigados_write_block(ctx, ctx->root_block, block) != 0) {
        return -4;
    }
    
    /* Update context */
    strncpy(ctx->volume_name, new_name, sizeof(ctx->volume_name) - 1);
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Pattern Matching
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Amiga-style pattern matching */
static bool amiga_pattern_match(const char *pattern, const char *str) {
    while (*pattern && *str) {
        if (*pattern == '?' || *pattern == '#') {
            /* Single character match */
            if (*pattern == '#' && pattern[1] == '?') {
                /* #? is same as * */
                pattern++;
            }
            if (*pattern == '?') {
                pattern++;
                str++;
                continue;
            }
        }
        if (*pattern == '*') {
            pattern++;
            if (!*pattern) return true;  /* * at end matches all */
            while (*str) {
                if (amiga_pattern_match(pattern, str)) return true;
                str++;
            }
            return false;
        }
        /* Case-insensitive compare */
        char p = *pattern;
        char s = *str;
        if (p >= 'A' && p <= 'Z') p += 32;
        if (s >= 'A' && s <= 'Z') s += 32;
        if (p != s) return false;
        pattern++;
        str++;
    }
    /* Handle trailing * */
    while (*pattern == '*') pattern++;
    return (*pattern == 0 && *str == 0);
}

/* Recursive directory walker */
static int walk_directory(uft_amigados_ctx_t *ctx,
                          uint32_t dir_block,
                          const char *base_path,
                          const char *pattern,
                          bool recursive,
                          uft_amiga_match_cb callback,
                          void *user_data) {
    uft_amiga_dir_entry_t entries[128];
    int count = 0;
    
    if (uft_amigados_list_dir(ctx, dir_block, entries, 128, &count) != 0) {
        return -1;
    }
    
    for (int i = 0; i < count; i++) {
        char full_path[UFT_AMIGA_MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entries[i].name);
        
        /* Check pattern if provided */
        if (!pattern || amiga_pattern_match(pattern, entries[i].name)) {
            if (callback(&entries[i], full_path, user_data) != 0) {
                return 1;  /* Callback requested stop */
            }
        }
        
        /* Recurse into directories */
        if (recursive && entries[i].type == UFT_AMIGA_ST_USERDIR) {
            walk_directory(ctx, entries[i].header_block, full_path,
                          pattern, recursive, callback, user_data);
        }
    }
    
    return 0;
}

int uft_amiga_find_pattern(uft_amigados_ctx_t *ctx,
                            const char *pattern,
                            bool recursive,
                            uft_amiga_match_cb callback,
                            void *user_data) {
    if (!ctx || !callback) return -1;
    
    return walk_directory(ctx, ctx->root_block, "", 
                          pattern, recursive, callback, user_data);
}

int uft_amiga_list_all(uft_amigados_ctx_t *ctx,
                        uft_amiga_match_cb callback,
                        void *user_data) {
    return uft_amiga_find_pattern(ctx, NULL, true, callback, user_data);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Pack/Unpack Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Callback data for unpack */
typedef struct {
    uft_amigados_ctx_t *ctx;
    const char *host_base;
    const uft_amiga_pack_opts_t *opts;
    int files_extracted;
    int errors;
} unpack_data_t;

static int unpack_callback(const uft_amiga_dir_entry_t *entry,
                            const char *full_path,
                            void *user_data) {
    unpack_data_t *data = (unpack_data_t *)user_data;
    
    char host_path[1024];
    snprintf(host_path, sizeof(host_path), "%s%s", data->host_base, full_path);
    
    if (entry->type == UFT_AMIGA_ST_USERDIR) {
        /* Create directory */
        #ifdef _WIN32
        _mkdir(host_path);
        #else
        mkdir(host_path, 0755);
        #endif
    } else if (entry->type == UFT_AMIGA_ST_FILE) {
        /* Extract file */
        uint8_t *buffer = malloc(entry->size);
        if (!buffer) {
            data->errors++;
            return 0;
        }
        
        size_t bytes_read;
        if (uft_amigados_read_file(data->ctx, entry->header_block, 
                                    buffer, entry->size, &bytes_read) == 0) {
            FILE *fp = fopen(host_path, "wb");
            if (fp) {
                fwrite(buffer, 1, bytes_read, fp);
                fclose(fp);
                data->files_extracted++;
            } else {
                data->errors++;
            }
        } else {
            data->errors++;
        }
        
        free(buffer);
    }
    
    return 0;
}

int uft_amiga_unpack_to_host(uft_amigados_ctx_t *ctx,
                              const char *amiga_path,
                              const char *host_path,
                              const uft_amiga_pack_opts_t *opts) {
    if (!ctx || !host_path) return -1;
    
    /* Create base directory */
    #ifdef _WIN32
    _mkdir(host_path);
    #else
    mkdir(host_path, 0755);
    #endif
    
    unpack_data_t data = {
        .ctx = ctx,
        .host_base = host_path,
        .opts = opts,
        .files_extracted = 0,
        .errors = 0
    };
    
    bool recursive = opts ? opts->recursive : true;
    
    uft_amiga_find_pattern(ctx, NULL, recursive, unpack_callback, &data);
    
    return data.errors > 0 ? -data.errors : data.files_extracted;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Bootblock Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Known boot virus signatures */
static const struct {
    const char *name;
    uint32_t offset;
    uint8_t signature[8];
    size_t sig_len;
} known_viruses[] = {
    {"SCA", 0x3E, {0x53, 0x43, 0x41, 0x20}, 4},
    {"Byte Bandit", 0x48, {0x42, 0x79, 0x74, 0x65, 0x20, 0x42}, 6},
    {"Lamer Exterminator", 0x1A, {0x4C, 0x41, 0x4D, 0x45, 0x52}, 5},
    {NULL, 0, {0}, 0}
};

int uft_amiga_read_bootblock(uft_amigados_ctx_t *ctx, 
                              uft_amiga_bootblock_t *boot) {
    if (!ctx || !boot) return -1;
    
    memset(boot, 0, sizeof(*boot));
    
    /* Read first two blocks */
    if (uft_amigados_read_block(ctx, 0, boot->raw) != 0) return -1;
    if (uft_amigados_read_block(ctx, 1, boot->raw + 512) != 0) return -1;
    
    /* Check DOS magic */
    if (boot->raw[0] == 'D' && boot->raw[1] == 'O' && boot->raw[2] == 'S') {
        boot->valid = true;
        boot->dos_type = boot->raw[3];
    }
    
    /* Calculate checksum */
    uint32_t sum = 0;
    for (int i = 0; i < 256; i++) {
        uint32_t val = ((uint32_t)boot->raw[i*4] << 24) |
                       ((uint32_t)boot->raw[i*4+1] << 16) |
                       ((uint32_t)boot->raw[i*4+2] << 8) |
                       (uint32_t)boot->raw[i*4+3];
        uint32_t old_sum = sum;
        sum += val;
        if (sum < old_sum) sum++;  /* Add carry */
    }
    boot->checksum_valid = (sum == 0xFFFFFFFF);
    
    /* Check for viruses */
    uft_amiga_check_boot_virus(boot);
    
    return 0;
}

int uft_amiga_check_boot_virus(const uft_amiga_bootblock_t *boot) {
    if (!boot || !boot->valid) return 0;
    
    for (int i = 0; known_viruses[i].name != NULL; i++) {
        if (memcmp(&boot->raw[known_viruses[i].offset],
                   known_viruses[i].signature,
                   known_viruses[i].sig_len) == 0) {
            /* Virus found - but we can't modify boot here */
            return 1;
        }
    }
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Validation
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_amiga_validate(uft_amigados_ctx_t *ctx, 
                        uft_amiga_validate_result_t *result) {
    if (!ctx || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    result->valid = true;
    
    char *details = result->details;
    size_t details_len = sizeof(result->details);
    int pos = 0;
    
    /* Validate root block */
    uint8_t block[UFT_AMIGA_BLOCK_SIZE];
    if (uft_amigados_read_block(ctx, ctx->root_block, block) == 0) {
        uint32_t type = ((uint32_t)block[0] << 24) | ((uint32_t)block[1] << 16) |
                        ((uint32_t)block[2] << 8) | block[3];
        if (type == 2) {
            result->rootblock_valid = true;
        } else {
            result->rootblock_valid = false;
            result->error_count++;
            pos += snprintf(details + pos, details_len - pos, 
                           "Root block invalid type: %u\n", type);
        }
    } else {
        result->rootblock_valid = false;
        result->error_count++;
        pos += snprintf(details + pos, details_len - pos, 
                       "Cannot read root block\n");
    }
    
    /* Validate bitmap */
    if (uft_amigados_validate_bitmap(ctx) == 0) {
        result->bitmap_valid = true;
    } else {
        result->bitmap_valid = false;
        result->warning_count++;
        pos += snprintf(details + pos, details_len - pos, 
                       "Bitmap inconsistent\n");
    }
    
    /* Set overall validity */
    result->valid = result->rootblock_valid && 
                    (result->error_count == 0);
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Batch Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_amiga_scan_directory(const char *dir_path,
                              bool recursive,
                              char ***found_paths,
                              int *num_found) {
    if (!dir_path || !found_paths || !num_found) return -1;
    
    *found_paths = NULL;
    *num_found = 0;
    
    int capacity = 64;
    char **paths = malloc(capacity * sizeof(char *));
    if (!paths) return -1;
    
    DIR *dir = opendir(dir_path);
    if (!dir) {
        free(paths);
        return -1;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        
        struct stat st;
        if (stat(full_path, &st) != 0) continue;
        
        if (S_ISDIR(st.st_mode) && recursive) {
            char **sub_paths;
            int sub_count;
            if (uft_amiga_scan_directory(full_path, true, &sub_paths, &sub_count) == 0) {
                for (int i = 0; i < sub_count; i++) {
                    if (*num_found >= capacity) {
                        capacity *= 2;
                        paths = realloc(paths, capacity * sizeof(char *));
                    }
                    paths[(*num_found)++] = sub_paths[i];
                }
                free(sub_paths);
            }
        } else if (S_ISREG(st.st_mode)) {
            /* Check extension */
            const char *ext = strrchr(entry->d_name, '.');
            if (ext && (strcasecmp(ext, ".adf") == 0 || 
                        strcasecmp(ext, ".hdf") == 0 ||
                        strcasecmp(ext, ".dms") == 0)) {
                if (*num_found >= capacity) {
                    capacity *= 2;
                    paths = realloc(paths, capacity * sizeof(char *));
                }
                paths[(*num_found)++] = strdup(full_path);
            }
        }
    }
    
    closedir(dir);
    
    *found_paths = paths;
    return 0;
}

void uft_amiga_free_scan_results(char **paths, int num_paths) {
    if (!paths) return;
    for (int i = 0; i < num_paths; i++) {
        free(paths[i]);
    }
    free(paths);
}
