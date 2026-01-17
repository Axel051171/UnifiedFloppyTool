/**
 * @file uft_plugin_manager.c
 * @brief UFT Plugin Manager Implementation
 * @version 4.1.0
 * 
 * Handles loading, unloading, and managing plugins.
 */

#include "uft/plugins/uft_plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define PLUGIN_EXT ".dll"
typedef HMODULE plugin_handle_t;
#define LOAD_LIBRARY(path) LoadLibraryA(path)
#define GET_SYMBOL(h, name) (void*)GetProcAddress(h, name)
#define CLOSE_LIBRARY(h) FreeLibrary(h)
#else
#include <dlfcn.h>
#include <dirent.h>
#ifdef __APPLE__
#define PLUGIN_EXT ".dylib"
#else
#define PLUGIN_EXT ".so"
#endif
typedef void* plugin_handle_t;
#define LOAD_LIBRARY(path) dlopen(path, RTLD_NOW)
#define GET_SYMBOL(h, name) dlsym(h, name)
#define CLOSE_LIBRARY(h) dlclose(h)
#endif

/* ============================================================================
 * Plugin Entry
 * ============================================================================ */

#define MAX_PLUGINS 64

typedef struct {
    char                    path[512];
    plugin_handle_t         handle;
    uft_plugin_info_t*      info;
    void*                   interface;
    uft_plugin_cleanup_fn   cleanup;
    bool                    loaded;
} plugin_entry_t;

static plugin_entry_t g_plugins[MAX_PLUGINS];
static int g_plugin_count = 0;

/* ============================================================================
 * Plugin Manager API
 * ============================================================================ */

int uft_plugin_load(const char *path)
{
    if (!path || g_plugin_count >= MAX_PLUGINS) {
        return -1;
    }
    
    /* Check if already loaded */
    for (int i = 0; i < g_plugin_count; i++) {
        if (strcmp(g_plugins[i].path, path) == 0) {
            printf("Plugin already loaded: %s\n", path);
            return 0;
        }
    }
    
    /* Load the library */
    plugin_handle_t handle = LOAD_LIBRARY(path);
    if (!handle) {
        fprintf(stderr, "Failed to load plugin: %s\n", path);
        return -1;
    }
    
    /* Get init function */
    uft_plugin_init_fn init_fn = (uft_plugin_init_fn)GET_SYMBOL(handle, "uft_plugin_init");
    if (!init_fn) {
        fprintf(stderr, "Plugin missing uft_plugin_init: %s\n", path);
        CLOSE_LIBRARY(handle);
        return -1;
    }
    
    /* Initialize plugin */
    uft_plugin_info_t *info = init_fn();
    if (!info) {
        fprintf(stderr, "Plugin init failed: %s\n", path);
        CLOSE_LIBRARY(handle);
        return -1;
    }
    
    /* Check ABI version */
    if (info->abi_version != UFT_PLUGIN_ABI_VERSION) {
        fprintf(stderr, "Plugin ABI mismatch: %s (expected %d, got %d)\n", 
                path, UFT_PLUGIN_ABI_VERSION, info->abi_version);
        CLOSE_LIBRARY(handle);
        return -1;
    }
    
    /* Get interface */
    uft_plugin_get_interface_fn get_iface = 
        (uft_plugin_get_interface_fn)GET_SYMBOL(handle, "uft_plugin_get_interface");
    void *interface = NULL;
    if (get_iface) {
        interface = get_iface(info->type);
    }
    
    /* Get cleanup function (optional) */
    uft_plugin_cleanup_fn cleanup = 
        (uft_plugin_cleanup_fn)GET_SYMBOL(handle, "uft_plugin_cleanup");
    
    /* Store plugin entry */
    plugin_entry_t *entry = &g_plugins[g_plugin_count++];
    strncpy(entry->path, path, sizeof(entry->path) - 1);
    entry->handle = handle;
    entry->info = info;
    entry->interface = interface;
    entry->cleanup = cleanup;
    entry->loaded = true;
    
    printf("Loaded plugin: %s v%s by %s\n", info->name, info->version, info->author);
    
    return 0;
}

int uft_plugin_unload(const char *name)
{
    for (int i = 0; i < g_plugin_count; i++) {
        if (g_plugins[i].loaded && 
            strcmp(g_plugins[i].info->name, name) == 0) {
            
            /* Call cleanup if available */
            if (g_plugins[i].cleanup) {
                g_plugins[i].cleanup();
            }
            
            /* Close library */
            CLOSE_LIBRARY(g_plugins[i].handle);
            g_plugins[i].loaded = false;
            
            printf("Unloaded plugin: %s\n", name);
            return 0;
        }
    }
    return -1;
}

int uft_plugin_count(void)
{
    int count = 0;
    for (int i = 0; i < g_plugin_count; i++) {
        if (g_plugins[i].loaded) count++;
    }
    return count;
}

const uft_plugin_info_t* uft_plugin_get_info(int index)
{
    int count = 0;
    for (int i = 0; i < g_plugin_count; i++) {
        if (g_plugins[i].loaded) {
            if (count == index) {
                return g_plugins[i].info;
            }
            count++;
        }
    }
    return NULL;
}

const uft_plugin_info_t* uft_plugin_find(const char *name)
{
    for (int i = 0; i < g_plugin_count; i++) {
        if (g_plugins[i].loaded && 
            strcmp(g_plugins[i].info->name, name) == 0) {
            return g_plugins[i].info;
        }
    }
    return NULL;
}

void uft_plugin_list(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                    LOADED PLUGINS (%2d)                        ║\n", uft_plugin_count());
    printf("╠═══════════════════════════════════════════════════════════════╣\n");
    
    for (int i = 0; i < g_plugin_count; i++) {
        if (g_plugins[i].loaded) {
            const uft_plugin_info_t *p = g_plugins[i].info;
            printf("║ %-20s │ %-8s │ %-28s ║\n",
                   p->name, p->version, p->description);
        }
    }
    
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
}

#ifndef _WIN32
int uft_plugin_scan_dir(const char *dir)
{
    DIR *d = opendir(dir);
    if (!d) return -1;
    
    struct dirent *entry;
    int loaded = 0;
    
    while ((entry = readdir(d)) != NULL) {
        const char *ext = strrchr(entry->d_name, '.');
        if (ext && strcmp(ext, PLUGIN_EXT) == 0) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
            if (uft_plugin_load(path) == 0) {
                loaded++;
            }
        }
    }
    
    closedir(d);
    return loaded;
}
#else
int uft_plugin_scan_dir(const char *dir)
{
    WIN32_FIND_DATAA ffd;
    char pattern[512];
    snprintf(pattern, sizeof(pattern), "%s\\*%s", dir, PLUGIN_EXT);
    
    HANDLE hFind = FindFirstFileA(pattern, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return -1;
    
    int loaded = 0;
    do {
        char path[1024];
        snprintf(path, sizeof(path), "%s\\%s", dir, ffd.cFileName);
        if (uft_plugin_load(path) == 0) {
            loaded++;
        }
    } while (FindNextFileA(hFind, &ffd));
    
    FindClose(hFind);
    return loaded;
}
#endif
