/**
 * @file uft_ipf_caps.c
 * @brief CAPS Library Adapter Implementation
 * 
 * Dynamic loading wrapper for the official SPS/CAPS library.
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#include "uft/formats/ipf/uft_ipf_caps.h"
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#define CAPS_LIB_NAME "CAPSImg.dll"
#else
#include <dlfcn.h>
#ifdef __APPLE__
#define CAPS_LIB_NAME "CAPSImage.framework/CAPSImage"
#else
#define CAPS_LIB_NAME "libcapsimage.so.4"
#endif
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * String Tables
 * ═══════════════════════════════════════════════════════════════════════════════ */

static const char *g_platform_names[] = {
    "N/A",           /* 0 */
    "Amiga",         /* 1 */
    "Atari ST",      /* 2 */
    "PC",            /* 3 */
    "Amstrad CPC",   /* 4 */
    "Spectrum",      /* 5 */
    "Sam Coupé",     /* 6 */
    "Archimedes",    /* 7 */
    "C64",           /* 8 */
    "Atari 8-bit"    /* 9 */
};

static const char *g_error_strings[] = {
    "OK",                    /* 0 */
    "Unsupported",           /* 1 */
    "Generic error",         /* 2 */
    "Out of range",          /* 3 */
    "Read only",             /* 4 */
    "Open error",            /* 5 */
    "Type error",            /* 6 */
    "File too short",        /* 7 */
    "Track header error",    /* 8 */
    "Track stream error",    /* 9 */
    "Track data error",      /* 10 */
    "Density header error",  /* 11 */
    "Density stream error",  /* 12 */
    "Density data error",    /* 13 */
    "Incompatible",          /* 14 */
    "Unsupported type",      /* 15 */
    "Bad block type",        /* 16 */
    "Bad block size",        /* 17 */
    "Bad data start",        /* 18 */
    "Buffer too short"       /* 19 */
};

const char *caps_platform_name(caps_platform_t platform) {
    if (platform < 0 || platform > 9) return "Unknown";
    return g_platform_names[platform];
}

const char *caps_error_string(caps_error_t err) {
    if (err < 0 || err > 19) return "Unknown error";
    return g_error_strings[err];
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Dynamic Library Loading
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifdef _WIN32

static void *load_library(const char *path) {
    return (void*)LoadLibraryA(path ? path : CAPS_LIB_NAME);
}

static void unload_library(void *handle) {
    if (handle) FreeLibrary((HMODULE)handle);
}

static void *get_symbol(void *handle, const char *name) {
    return (void*)GetProcAddress((HMODULE)handle, name);
}

#else /* POSIX */

static void *load_library(const char *path) {
    return dlopen(path ? path : CAPS_LIB_NAME, RTLD_NOW | RTLD_LOCAL);
}

static void unload_library(void *handle) {
    if (handle) dlclose(handle);
}

static void *get_symbol(void *handle, const char *name) {
    return dlsym(handle, name);
}

#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Library Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool caps_lib_load(caps_lib_t *lib, const char *path) {
    if (!lib) return false;
    
    memset(lib, 0, sizeof(*lib));
    
    lib->handle = load_library(path);
    if (!lib->handle) {
        return false;
    }
    
    /* Load function pointers */
    lib->Init = (int32_t(*)(void))get_symbol(lib->handle, "CAPSInit");
    lib->Exit = (int32_t(*)(void))get_symbol(lib->handle, "CAPSExit");
    lib->AddImage = (int32_t(*)(void))get_symbol(lib->handle, "CAPSAddImage");
    lib->RemImage = (int32_t(*)(int32_t))get_symbol(lib->handle, "CAPSRemImage");
    lib->LockImage = (int32_t(*)(int32_t, const char*))get_symbol(lib->handle, "CAPSLockImage");
    lib->LockImageMemory = (int32_t(*)(int32_t, uint8_t*, uint32_t, uint32_t))
                            get_symbol(lib->handle, "CAPSLockImageMemory");
    lib->UnlockImage = (int32_t(*)(int32_t))get_symbol(lib->handle, "CAPSUnlockImage");
    lib->LoadImage = (int32_t(*)(int32_t, uint32_t))get_symbol(lib->handle, "CAPSLoadImage");
    lib->GetImageInfo = (int32_t(*)(caps_image_info_t*, int32_t))
                         get_symbol(lib->handle, "CAPSGetImageInfo");
    lib->LockTrack = (int32_t(*)(void*, int32_t, uint32_t, uint32_t, uint32_t))
                      get_symbol(lib->handle, "CAPSLockTrack");
    lib->UnlockTrack = (int32_t(*)(int32_t, uint32_t, uint32_t))
                        get_symbol(lib->handle, "CAPSUnlockTrack");
    lib->UnlockAllTracks = (int32_t(*)(int32_t))get_symbol(lib->handle, "CAPSUnlockAllTracks");
    lib->GetPlatformName = (const char*(*)(uint32_t))get_symbol(lib->handle, "CAPSGetPlatformName");
    lib->GetVersionInfo = (int32_t(*)(caps_version_info_t*, uint32_t))
                           get_symbol(lib->handle, "CAPSGetVersionInfo");
    
    /* Check required functions */
    if (!lib->Init || !lib->Exit || !lib->AddImage || !lib->RemImage ||
        !lib->LockImage || !lib->UnlockImage || !lib->GetImageInfo) {
        unload_library(lib->handle);
        memset(lib, 0, sizeof(*lib));
        return false;
    }
    
    lib->loaded = true;
    return true;
}

void caps_lib_unload(caps_lib_t *lib) {
    if (!lib) return;
    
    if (lib->handle) {
        unload_library(lib->handle);
    }
    
    memset(lib, 0, sizeof(*lib));
}

bool caps_lib_available(void) {
    caps_lib_t lib;
    bool avail = caps_lib_load(&lib, NULL);
    if (avail) {
        caps_lib_unload(&lib);
    }
    return avail;
}

bool caps_lib_get_version(caps_lib_t *lib, caps_version_info_t *vi) {
    if (!lib || !lib->loaded || !lib->GetVersionInfo || !vi) return false;
    return lib->GetVersionInfo(vi, 0) == CAPS_OK;
}
