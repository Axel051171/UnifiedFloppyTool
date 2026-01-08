/**
 * @file ipfinfo_caps.c
 * @brief IPF Info Tool using CAPS Library
 * 
 * Displays detailed information about IPF files using the
 * official SPS CAPS library for track decoding.
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

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

/* CAPS type definitions */
typedef uint8_t  CapsUByte;
typedef int32_t  CapsLong;
typedef uint32_t CapsULong;

#define CAPS_MAXPLATFORM 4
#define CAPS_MTRS 5

/* CAPS structures */
struct CapsDateTimeExt {
    CapsULong year, month, day, hour, min, sec, tick;
};

struct CapsImageInfo {
    CapsULong type, release, revision;
    CapsULong mincylinder, maxcylinder;
    CapsULong minhead, maxhead;
    struct CapsDateTimeExt crdt;
    CapsULong platform[CAPS_MAXPLATFORM];
};

struct CapsTrackInfoT2 {
    CapsULong type, cylinder, head;
    CapsULong sectorcnt, sectorsize;
    CapsUByte *trackbuf;
    CapsULong tracklen, timelen;
    CapsULong *timebuf;
    CapsLong overlap;
    CapsULong startbit, wseed, weakcnt;
};

struct CapsVersionInfo {
    CapsULong type, release, revision, flag;
};

/* Function pointers */
typedef CapsLong (*fn_Init)(void);
typedef CapsLong (*fn_Exit)(void);
typedef CapsLong (*fn_AddImage)(void);
typedef CapsLong (*fn_RemImage)(CapsLong);
typedef CapsLong (*fn_LockImage)(CapsLong, const char*);
typedef CapsLong (*fn_UnlockImage)(CapsLong);
typedef CapsLong (*fn_GetImageInfo)(struct CapsImageInfo*, CapsLong);
typedef CapsLong (*fn_LockTrack)(void*, CapsLong, CapsULong, CapsULong, CapsULong);
typedef CapsLong (*fn_UnlockTrack)(CapsLong, CapsULong, CapsULong);
typedef CapsLong (*fn_UnlockAllTracks)(CapsLong);
typedef const char* (*fn_GetPlatformName)(CapsULong);
typedef CapsLong (*fn_GetVersionInfo)(struct CapsVersionInfo*, CapsULong);

/* Global library handle */
static void *g_lib = NULL;
static fn_Init CAPSInit;
static fn_Exit CAPSExit;
static fn_AddImage CAPSAddImage;
static fn_RemImage CAPSRemImage;
static fn_LockImage CAPSLockImage;
static fn_UnlockImage CAPSUnlockImage;
static fn_GetImageInfo CAPSGetImageInfo;
static fn_LockTrack CAPSLockTrack;
static fn_UnlockTrack CAPSUnlockTrack;
static fn_UnlockAllTracks CAPSUnlockAllTracks;
static fn_GetPlatformName CAPSGetPlatformName;
static fn_GetVersionInfo CAPSGetVersionInfo;

/* Lock flags */
#define DI_LOCK_TYPE    (1UL << 9)
#define DI_LOCK_TRKBIT  (1UL << 12)

static bool load_caps_library(const char *path) {
#ifdef _WIN32
    g_lib = (void*)LoadLibraryA(path ? path : CAPS_LIB_NAME);
    #define GET_SYM(name) (fn_##name)GetProcAddress((HMODULE)g_lib, "CAPS" #name)
#else
    g_lib = dlopen(path ? path : CAPS_LIB_NAME, RTLD_NOW);
    #define GET_SYM(name) (fn_##name)dlsym(g_lib, "CAPS" #name)
#endif
    
    if (!g_lib) {
        fprintf(stderr, "Failed to load CAPS library\n");
        return false;
    }
    
    CAPSInit = GET_SYM(Init);
    CAPSExit = GET_SYM(Exit);
    CAPSAddImage = GET_SYM(AddImage);
    CAPSRemImage = GET_SYM(RemImage);
    CAPSLockImage = GET_SYM(LockImage);
    CAPSUnlockImage = GET_SYM(UnlockImage);
    CAPSGetImageInfo = GET_SYM(GetImageInfo);
    CAPSLockTrack = GET_SYM(LockTrack);
    CAPSUnlockTrack = GET_SYM(UnlockTrack);
    CAPSUnlockAllTracks = GET_SYM(UnlockAllTracks);
    CAPSGetPlatformName = GET_SYM(GetPlatformName);
    CAPSGetVersionInfo = GET_SYM(GetVersionInfo);
    
    if (!CAPSInit || !CAPSExit || !CAPSAddImage || !CAPSRemImage ||
        !CAPSLockImage || !CAPSUnlockImage || !CAPSGetImageInfo) {
        fprintf(stderr, "Missing required CAPS functions\n");
        return false;
    }
    
    return true;
}

static const char *track_type_name(CapsULong type) {
    switch (type & 0xFF) {
        case 0: return "N/A";
        case 1: return "Noise";
        case 2: return "Auto";
        case 3: return "Variable";
        default: return "Unknown";
    }
}

static void print_image_info(const char *filename, bool verbose) {
    CapsLong id = CAPSAddImage();
    if (id < 0) {
        fprintf(stderr, "Failed to add image slot\n");
        return;
    }
    
    CapsLong err = CAPSLockImage(id, filename);
    if (err != 0) {
        fprintf(stderr, "Failed to lock image: error %d\n", (int)err);
        CAPSRemImage(id);
        return;
    }
    
    struct CapsImageInfo cii;
    memset(&cii, 0, sizeof(cii));
    
    if (CAPSGetImageInfo(&cii, id) != 0) {
        fprintf(stderr, "Failed to get image info\n");
        CAPSUnlockImage(id);
        CAPSRemImage(id);
        return;
    }
    
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("IPF Image Analysis (via CAPS Library)\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    printf("File:        %s\n", filename);
    printf("Type:        %s\n", cii.type == 1 ? "Floppy Disk" : "Unknown");
    printf("Release:     %u.%u\n", (unsigned)cii.release, (unsigned)cii.revision);
    printf("Cylinders:   %u - %u (%u total)\n", 
           (unsigned)cii.mincylinder, (unsigned)cii.maxcylinder,
           (unsigned)(cii.maxcylinder - cii.mincylinder + 1));
    printf("Heads:       %u - %u\n", (unsigned)cii.minhead, (unsigned)cii.maxhead);
    printf("Created:     %04u-%02u-%02u %02u:%02u:%02u\n",
           (unsigned)cii.crdt.year, (unsigned)cii.crdt.month, (unsigned)cii.crdt.day,
           (unsigned)cii.crdt.hour, (unsigned)cii.crdt.min, (unsigned)cii.crdt.sec);
    
    printf("Platforms:   ");
    for (int i = 0; i < CAPS_MAXPLATFORM; i++) {
        if (cii.platform[i] != 0) {
            const char *name = CAPSGetPlatformName ? 
                               CAPSGetPlatformName(cii.platform[i]) : "?";
            printf("%s ", name);
        }
    }
    printf("\n");
    
    /* Track analysis */
    if (verbose) {
        printf("\n── Track Details ────────────────────────────────────────────────────\n");
        printf("Cyl Head Type     Sectors Bits      Overlap Weak\n");
        printf("─── ──── ──────── ─────── ───────── ─────── ────\n");
        
        unsigned total_bits = 0;
        unsigned total_sectors = 0;
        unsigned weak_tracks = 0;
        
        for (CapsULong cyl = cii.mincylinder; cyl <= cii.maxcylinder; cyl++) {
            for (CapsULong head = cii.minhead; head <= cii.maxhead; head++) {
                struct CapsTrackInfoT2 ti;
                memset(&ti, 0, sizeof(ti));
                ti.type = 2;  /* Request T2 structure */
                
                CapsLong r = CAPSLockTrack(&ti, id, cyl, head, DI_LOCK_TYPE | DI_LOCK_TRKBIT);
                if (r == 0) {
                    printf("%3u  %u   %-8s %7u %9u %7d %4u\n",
                           (unsigned)cyl, (unsigned)head,
                           track_type_name(ti.type),
                           (unsigned)ti.sectorcnt,
                           (unsigned)ti.tracklen,
                           (int)ti.overlap,
                           (unsigned)ti.weakcnt);
                    
                    total_bits += ti.tracklen;
                    total_sectors += ti.sectorcnt;
                    if (ti.weakcnt > 0) weak_tracks++;
                    
                    CAPSUnlockTrack(id, cyl, head);
                }
            }
        }
        
        printf("\n── Summary ──────────────────────────────────────────────────────────\n");
        printf("Total tracks:  %u\n", 
               (unsigned)((cii.maxcylinder - cii.mincylinder + 1) * 
                         (cii.maxhead - cii.minhead + 1)));
        printf("Total sectors: %u\n", total_sectors);
        printf("Total bits:    %u (%.1f KB)\n", total_bits, total_bits / 8.0 / 1024.0);
        printf("Weak tracks:   %u\n", weak_tracks);
    }
    
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    
    CAPSUnlockAllTracks(id);
    CAPSUnlockImage(id);
    CAPSRemImage(id);
}

int main(int argc, char **argv) {
    bool verbose = false;
    const char *filename = NULL;
    const char *libpath = NULL;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            libpath = argv[++i];
        } else if (argv[i][0] != '-') {
            filename = argv[i];
        }
    }
    
    if (!filename) {
        printf("IPF Info Tool (CAPS Library Edition)\n");
        printf("Usage: %s [-v] [-l libpath] <file.ipf>\n", argv[0]);
        printf("  -v, --verbose   Show track details\n");
        printf("  -l <path>       Path to CAPS library\n");
        return 1;
    }
    
    if (!load_caps_library(libpath)) {
        return 1;
    }
    
    if (CAPSInit() != 0) {
        fprintf(stderr, "Failed to initialize CAPS library\n");
        return 1;
    }
    
    /* Show library version */
    if (CAPSGetVersionInfo) {
        struct CapsVersionInfo vi;
        if (CAPSGetVersionInfo(&vi, 0) == 0) {
            printf("CAPS Library v%u.%u\n\n", (unsigned)vi.release, (unsigned)vi.revision);
        }
    }
    
    print_image_info(filename, verbose);
    
    CAPSExit();
    return 0;
}
