/**
 * @file uft_adflib_wrapper.c
 * @brief ADFlib Wrapper Implementation
 */

#include "uft_adflib_wrapper.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static char g_last_error[256] = {0};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Stub Implementation (without ADFlib)
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_USE_ADFLIB

bool uft_adf_is_available(void) {
    return false;
}

const char* uft_adf_last_error(void) {
    return "ADFlib support not compiled in (compile with -DUFT_USE_ADFLIB)";
}

uft_adf_context_t* uft_adf_open(const char *path, bool readonly) {
    (void)path; (void)readonly;
    snprintf(g_last_error, sizeof(g_last_error), 
             "ADFlib not available");
    return NULL;
}

void uft_adf_close(uft_adf_context_t *ctx) { (void)ctx; }

int uft_adf_get_device_info(uft_adf_context_t *ctx, uft_adf_device_info_t *info) {
    (void)ctx; (void)info;
    return -1;
}

int uft_adf_get_volume_info(uft_adf_context_t *ctx, int vol_index, 
                            uft_adf_volume_info_t *info) {
    (void)ctx; (void)vol_index; (void)info;
    return -1;
}

int uft_adf_mount_volume(uft_adf_context_t *ctx, int vol_index) {
    (void)ctx; (void)vol_index;
    return -1;
}

void uft_adf_unmount_volume(uft_adf_context_t *ctx) { (void)ctx; }

int uft_adf_change_dir(uft_adf_context_t *ctx, const char *path) {
    (void)ctx; (void)path;
    return -1;
}

void uft_adf_to_root(uft_adf_context_t *ctx) { (void)ctx; }

const char* uft_adf_get_current_dir(uft_adf_context_t *ctx) {
    (void)ctx;
    return NULL;
}

int uft_adf_list_dir(uft_adf_context_t *ctx, uft_adf_entry_t *entries, 
                     int max_entries) {
    (void)ctx; (void)entries; (void)max_entries;
    return -1;
}

int uft_adf_extract_file(uft_adf_context_t *ctx, const char *adf_path, 
                         const char *local_path) {
    (void)ctx; (void)adf_path; (void)local_path;
    return -1;
}

int uft_adf_extract_all(uft_adf_context_t *ctx, const char *local_dir, 
                        bool recursive) {
    (void)ctx; (void)local_dir; (void)recursive;
    return -1;
}

int uft_adf_add_file(uft_adf_context_t *ctx, const char *local_path, 
                     const char *adf_path) {
    (void)ctx; (void)local_path; (void)adf_path;
    return -1;
}

int uft_adf_delete_file(uft_adf_context_t *ctx, const char *adf_path) {
    (void)ctx; (void)adf_path;
    return -1;
}

int uft_adf_list_deleted(uft_adf_context_t *ctx, uft_adf_entry_t *entries, 
                         int max_entries) {
    (void)ctx; (void)entries; (void)max_entries;
    return -1;
}

int uft_adf_recover_file(uft_adf_context_t *ctx, const char *name) {
    (void)ctx; (void)name;
    return -1;
}

int uft_adf_rebuild_bitmap(uft_adf_context_t *ctx) {
    (void)ctx;
    return -1;
}

int uft_adf_check_consistency(uft_adf_context_t *ctx, int *errors) {
    (void)ctx; (void)errors;
    return -1;
}

const char* uft_adf_fs_type_name(uft_adf_fs_type_t type) {
    switch (type) {
        case UFT_ADF_FS_OFS: return "OFS";
        case UFT_ADF_FS_FFS: return "FFS";
        case UFT_ADF_FS_OFS_INTL: return "OFS-INTL";
        case UFT_ADF_FS_FFS_INTL: return "FFS-INTL";
        case UFT_ADF_FS_OFS_DC: return "OFS-DC";
        case UFT_ADF_FS_FFS_DC: return "FFS-DC";
        default: return "Unknown";
    }
}

#endif /* !UFT_USE_ADFLIB */

/* ═══════════════════════════════════════════════════════════════════════════════
 * ADFlib Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifdef UFT_USE_ADFLIB

#include <adflib.h>

struct uft_adf_context {
    struct AdfDevice *dev;
    struct AdfVolume *vol;
    int current_vol;
    char current_path[512];
    bool readonly;
};

static bool g_adflib_initialized = false;

static void ensure_init(void) {
    if (!g_adflib_initialized) {
        adfLibInit();
        g_adflib_initialized = true;
    }
}

bool uft_adf_is_available(void) {
    return true;
}

const char* uft_adf_last_error(void) {
    return g_last_error;
}

uft_adf_context_t* uft_adf_open(const char *path, bool readonly) {
    ensure_init();
    
    uft_adf_context_t *ctx = calloc(1, sizeof(uft_adf_context_t));
    if (!ctx) {
        snprintf(g_last_error, sizeof(g_last_error), "Memory allocation failed");
        return NULL;
    }
    
    ctx->readonly = readonly;
    ctx->current_vol = -1;
    
    ADF_ACCESS_MODE mode = readonly ? ADF_ACCESS_MODE_READONLY : ADF_ACCESS_MODE_READWRITE;
    ctx->dev = adfDevOpen(path, mode);
    
    if (!ctx->dev) {
        snprintf(g_last_error, sizeof(g_last_error), "Failed to open device: %s", path);
        free(ctx);
        return NULL;
    }
    
    if (adfDevMount(ctx->dev) != ADF_RC_OK) {
        snprintf(g_last_error, sizeof(g_last_error), "Failed to mount device");
        adfDevClose(ctx->dev);
        free(ctx);
        return NULL;
    }
    
    return ctx;
}

void uft_adf_close(uft_adf_context_t *ctx) {
    if (!ctx) return;
    
    if (ctx->vol) {
        adfVolUnMount(ctx->vol);
    }
    if (ctx->dev) {
        adfDevUnMount(ctx->dev);
        adfDevClose(ctx->dev);
    }
    free(ctx);
}

int uft_adf_get_device_info(uft_adf_context_t *ctx, uft_adf_device_info_t *info) {
    if (!ctx || !ctx->dev || !info) return -1;
    
    memset(info, 0, sizeof(*info));
    info->cylinders = ctx->dev->cylinders;
    info->heads = ctx->dev->heads;
    info->sectors = ctx->dev->sectors;
    info->num_volumes = ctx->dev->nVol;
    info->total_blocks = ctx->dev->cylinders * ctx->dev->heads * ctx->dev->sectors;
    info->has_rdb = (ctx->dev->devType == ADF_DEVTYPE_HARDDISK);
    
    return 0;
}

int uft_adf_get_volume_info(uft_adf_context_t *ctx, int vol_index, 
                            uft_adf_volume_info_t *info) {
    if (!ctx || !ctx->dev || !info) return -1;
    if (vol_index < 0 || vol_index >= ctx->dev->nVol) return -1;
    
    /* Temporarily mount volume to get info */
    struct AdfVolume *vol = adfVolMount(ctx->dev, vol_index, ADF_ACCESS_MODE_READONLY);
    if (!vol) return -1;
    
    memset(info, 0, sizeof(*info));
    strncpy(info->name, vol->volName, sizeof(info->name) - 1);
    info->fs_type = (uft_adf_fs_type_t)vol->dosType;
    info->num_blocks = vol->lastBlock - vol->firstBlock + 1;
    info->root_block = vol->rootBlock;
    info->is_bootable = vol->bootCode;
    
    /* Calculate free blocks */
    info->free_blocks = adfCountFreeBlocks(vol);
    
    /* Get creation date */
    info->year = vol->coDays;  /* Simplified - need proper conversion */
    
    adfVolUnMount(vol);
    return 0;
}

int uft_adf_mount_volume(uft_adf_context_t *ctx, int vol_index) {
    if (!ctx || !ctx->dev) return -1;
    if (vol_index < 0 || vol_index >= ctx->dev->nVol) return -1;
    
    if (ctx->vol) {
        adfVolUnMount(ctx->vol);
        ctx->vol = NULL;
    }
    
    ADF_ACCESS_MODE mode = ctx->readonly ? ADF_ACCESS_MODE_READONLY : ADF_ACCESS_MODE_READWRITE;
    ctx->vol = adfVolMount(ctx->dev, vol_index, mode);
    
    if (!ctx->vol) {
        snprintf(g_last_error, sizeof(g_last_error), "Failed to mount volume %d", vol_index);
        return -1;
    }
    
    ctx->current_vol = vol_index;
    strcpy(ctx->current_path, "/");
    return 0;
}

void uft_adf_unmount_volume(uft_adf_context_t *ctx) {
    if (ctx && ctx->vol) {
        adfVolUnMount(ctx->vol);
        ctx->vol = NULL;
        ctx->current_vol = -1;
    }
}

int uft_adf_change_dir(uft_adf_context_t *ctx, const char *path) {
    if (!ctx || !ctx->vol || !path) return -1;
    
    if (adfChangeDir(ctx->vol, path) != ADF_RC_OK) {
        snprintf(g_last_error, sizeof(g_last_error), "Directory not found: %s", path);
        return -1;
    }
    
    /* Update current path */
    if (path[0] == '/') {
        strncpy(ctx->current_path, path, sizeof(ctx->current_path) - 1);
    } else {
        size_t len = strlen(ctx->current_path);
        if (len > 1) {
            strncat(ctx->current_path, "/", sizeof(ctx->current_path) - len - 1);
        }
        strncat(ctx->current_path, path, sizeof(ctx->current_path) - strlen(ctx->current_path) - 1);
    }
    
    return 0;
}

void uft_adf_to_root(uft_adf_context_t *ctx) {
    if (ctx && ctx->vol) {
        adfToRootDir(ctx->vol);
        strcpy(ctx->current_path, "/");
    }
}

const char* uft_adf_get_current_dir(uft_adf_context_t *ctx) {
    return ctx ? ctx->current_path : NULL;
}

int uft_adf_list_dir(uft_adf_context_t *ctx, uft_adf_entry_t *entries, 
                     int max_entries) {
    if (!ctx || !ctx->vol || !entries || max_entries <= 0) return -1;
    
    struct AdfList *list = adfGetDirEnt(ctx->vol, ctx->vol->curDirPtr);
    if (!list) return 0;
    
    int count = 0;
    struct AdfList *cell = list;
    
    while (cell && count < max_entries) {
        struct AdfEntry *entry = (struct AdfEntry*)cell->content;
        uft_adf_entry_t *out = &entries[count];
        
        memset(out, 0, sizeof(*out));
        strncpy(out->name, entry->name, sizeof(out->name) - 1);
        out->size = entry->size;
        out->sector = entry->sector;
        
        switch (entry->type) {
            case ADF_ST_FILE: out->type = UFT_ADF_ENTRY_FILE; break;
            case ADF_ST_DIR: out->type = UFT_ADF_ENTRY_DIR; break;
            case ADF_ST_LFILE: out->type = UFT_ADF_ENTRY_HARDLINK; break;
            case ADF_ST_LDIR: out->type = UFT_ADF_ENTRY_HARDLINK; break;
            case ADF_ST_LSOFT: out->type = UFT_ADF_ENTRY_SOFTLINK; break;
            default: out->type = UFT_ADF_ENTRY_FILE; break;
        }
        
        if (entry->comment) {
            strncpy(out->comment, entry->comment, sizeof(out->comment) - 1);
        }
        
        count++;
        cell = cell->next;
    }
    
    adfFreeDirList(list);
    return count;
}

int uft_adf_extract_file(uft_adf_context_t *ctx, const char *adf_path, 
                         const char *local_path) {
    if (!ctx || !ctx->vol || !adf_path || !local_path) return -1;
    
    struct AdfFile *file = adfFileOpen(ctx->vol, adf_path, ADF_FILE_MODE_READ);
    if (!file) {
        snprintf(g_last_error, sizeof(g_last_error), "Cannot open file: %s", adf_path);
        return -1;
    }
    
    FILE *out = fopen(local_path, "wb");
    if (!out) {
        adfFileClose(file);
        snprintf(g_last_error, sizeof(g_last_error), "Cannot create file: %s", local_path);
        return -1;
    }
    
    uint8_t buf[512];
    while (!adfFileAtEOF(file)) {
        int n = adfFileRead(file, sizeof(buf), buf);
        if (n > 0) {
            fwrite(buf, 1, n, out);
        }
    }
    
    fclose(out);
    adfFileClose(file);
    return 0;
}

int uft_adf_rebuild_bitmap(uft_adf_context_t *ctx) {
    if (!ctx || !ctx->vol) return -1;
    if (ctx->readonly) {
        snprintf(g_last_error, sizeof(g_last_error), "Volume is read-only");
        return -1;
    }
    
    /* ADFlib function to rebuild bitmap */
    if (adfReconstructBitmap(ctx->vol) != ADF_RC_OK) {
        snprintf(g_last_error, sizeof(g_last_error), "Bitmap rebuild failed");
        return -1;
    }
    
    return 0;
}

const char* uft_adf_fs_type_name(uft_adf_fs_type_t type) {
    switch (type) {
        case UFT_ADF_FS_OFS: return "OFS";
        case UFT_ADF_FS_FFS: return "FFS";
        case UFT_ADF_FS_OFS_INTL: return "OFS-INTL";
        case UFT_ADF_FS_FFS_INTL: return "FFS-INTL";
        case UFT_ADF_FS_OFS_DC: return "OFS-DC";
        case UFT_ADF_FS_FFS_DC: return "FFS-DC";
        default: return "Unknown";
    }
}

/* Additional functions would be implemented similarly... */

#endif /* UFT_USE_ADFLIB */
