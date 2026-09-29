/* SPDX-License-Identifier: MIT */
/**
 * @file uft_capsimg_helper.c
 * @brief The IPF / CT-Raw helper for UFT: reads an image through the
 *        user-supplied SPS capsimg library and answers in UFT's helper
 *        protocol (docs/specs/capsimg-helper/PROTOCOL.md, version 1).
 *
 * Why a separate program (owner decision 2026-09-29, "SPS v1.02"):
 * capsimg is under the SPS DECODER LIBRARY licence v1.02 (non-commercial,
 * github.com/simonowen/capsimage LICENCE.txt), which does not fit UFT's
 * GPL-2.0-or-later. UFT never links it. This helper is its own program
 * under MIT; it LOADS the CAPSImg library at run time (LoadLibrary /
 * dlopen) from a path the user provides, and talks to UFT through files
 * only. Nothing of capsimg is shipped with UFT or with this helper.
 *
 * Interface facts used here (types, field order, function names, flag
 * bits) are those of capsimg's public API header CapsAPI.h / CapsLib.h —
 * declared anew, not copied; no capsimg code.
 *
 * Call (as uft_ipf_helper.c spawns it):
 *     uft_capsimg_helper <image> <index-file> <blob-file>
 * Environment: UFT_CAPSIMG_LIB = path of CAPSImg.dll / libcapsimage.so
 *              (default: "CAPSImg.dll" next to the helper / system search)
 *
 * The helper invents nothing (PROTOCOL.md §3): a field it cannot determine
 * is 0; a track capsimg cannot lock is still listed with blob_len 0.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
typedef HMODULE lib_t;
static lib_t lib_open(const char *p) { return LoadLibraryA(p); }
static void *lib_sym(lib_t l, const char *n) { return (void *)GetProcAddress(l, n); }
#define CAPS_CC __cdecl
#define DEFAULT_LIB "CAPSImg.dll"
#else
#include <dlfcn.h>
typedef void *lib_t;
static lib_t lib_open(const char *p) { return dlopen(p, RTLD_NOW); }
static void *lib_sym(lib_t l, const char *n) { return dlsym(l, n); }
#define CAPS_CC
#define DEFAULT_LIB "libcapsimage.so.5"
#endif

/* ── the capsimg API, declared from its public header ─────────────── */
#pragma pack(push, 1)
typedef struct {
    uint32_t year, month, day, hour, min, sec, tick;
} caps_datetime_t;
typedef struct {
    uint32_t type, release, revision;
    uint32_t mincylinder, maxcylinder, minhead, maxhead;
    caps_datetime_t crdt;
    uint32_t platform[4];
} caps_image_info_t;
typedef struct {
    uint32_t type, cylinder, head, sectorcnt, sectorsize;
    uint8_t *trackbuf;
    uint32_t tracklen, timelen;
    uint32_t *timebuf;
    int32_t overlap;
    uint32_t startbit, wseed, weakcnt;
} caps_track_info_t2;
#pragma pack(pop)

#define DI_LOCK_INDEX    (1u << 0)
#define DI_LOCK_DENVAR   (1u << 2)
#define DI_LOCK_UPDATEFD (1u << 8)
#define DI_LOCK_TYPE     (1u << 9)
#define DI_LOCK_TRKBIT   (1u << 12)
#define CTIT_FLAG_FLAKEY (1u << 31)

typedef int32_t (CAPS_CC *fn_void_t)(void);
typedef int32_t (CAPS_CC *fn_id_t)(int32_t);
typedef int32_t (CAPS_CC *fn_lockimg_t)(int32_t, char *);
typedef int32_t (CAPS_CC *fn_info_t)(caps_image_info_t *, int32_t);
typedef int32_t (CAPS_CC *fn_locktrk_t)(void *, int32_t, uint32_t, uint32_t, uint32_t);
typedef int32_t (CAPS_CC *fn_unlocktrk_t)(int32_t, uint32_t, uint32_t);

static struct {
    fn_void_t init, exit_, add;
    fn_id_t rem, unlockimg;
    fn_lockimg_t lockimg;
    fn_info_t info;
    fn_locktrk_t locktrk;
    fn_unlocktrk_t unlocktrk;
} caps;

/* Answer with an ERROR line and END into the index, and a non-zero code
 * (PROTOCOL.md §3, H3/H5: no TRACK line). */
static int absage(const char *index, int code, const char *grund)
{
    FILE *f = index ? fopen(index, "wb") : NULL;
    if (f) {
        fprintf(f, "UFT-IPF-HELPER 1\nERROR %s\nEND\n", grund);
        fclose(f);
    }
    fprintf(stderr, "uft_capsimg_helper: %s\n", grund);
    return code;
}

static int laden(const char *pfad)
{
    lib_t l = lib_open(pfad);
    if (!l) return 0;
    caps.init      = (fn_void_t)lib_sym(l, "CAPSInit");
    caps.exit_     = (fn_void_t)lib_sym(l, "CAPSExit");
    caps.add       = (fn_void_t)lib_sym(l, "CAPSAddImage");
    caps.rem       = (fn_id_t)lib_sym(l, "CAPSRemImage");
    caps.lockimg   = (fn_lockimg_t)lib_sym(l, "CAPSLockImage");
    caps.unlockimg = (fn_id_t)lib_sym(l, "CAPSUnlockImage");
    caps.info      = (fn_info_t)lib_sym(l, "CAPSGetImageInfo");
    caps.locktrk   = (fn_locktrk_t)lib_sym(l, "CAPSLockTrack");
    caps.unlocktrk = (fn_unlocktrk_t)lib_sym(l, "CAPSUnlockTrack");
    return caps.init && caps.exit_ && caps.add && caps.rem && caps.lockimg &&
           caps.unlockimg && caps.info && caps.locktrk && caps.unlocktrk;
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "Aufruf: %s <abbild> <index-datei> <beilage>\n", argv[0]);
        return 2;
    }
    const char *bild = argv[1], *index = argv[2], *beilage = argv[3];
    const char *libpfad = getenv("UFT_CAPSIMG_LIB");
    if (!libpfad || !*libpfad) libpfad = DEFAULT_LIB;
    if (!laden(libpfad))
        return absage(index, 3, "capsimg library not loadable (UFT_CAPSIMG_LIB)");

    if (caps.init() != 0) return absage(index, 4, "CAPSInit failed");
    int32_t id = caps.add();
    if (id < 0) { caps.exit_(); return absage(index, 4, "CAPSAddImage failed"); }

    int rc = 0;
    char grund[160] = "";
    FILE *blob = NULL, *idx = NULL;
    int32_t e = caps.lockimg(id, (char *)bild);
    if (e != 0) {
        snprintf(grund, sizeof grund, "capsimg rejects the image (CAPSLockImage %d)", (int)e);
        rc = 5;
        goto fertig;
    }
    caps_image_info_t ii;
    memset(&ii, 0, sizeof ii);
    if (caps.info(&ii, id) != 0 || ii.maxcylinder < ii.mincylinder ||
        ii.maxhead < ii.minhead || ii.maxcylinder > 254 || ii.maxhead > 1) {
        snprintf(grund, sizeof grund, "CAPSGetImageInfo failed or geometry out of range");
        rc = 5;
        goto fertig;
    }
    blob = fopen(beilage, "wb");
    if (!blob) {
        snprintf(grund, sizeof grund, "blob file not writable");
        rc = 6;
        goto fertig;
    }
    /* The index is built in memory first; it is written only when every
     * track has been handled, so a failure never leaves a partial answer
     * that ends with END (H4). */
    size_t cap = 4096, len = 0;
    char *text = malloc(cap);
    if (!text) { snprintf(grund, sizeof grund, "out of memory"); rc = 7; goto fertig; }
#define PUT(...) do { \
        int n_ = snprintf(text + len, cap - len, __VA_ARGS__); \
        if (n_ < 0) { rc = 7; break; } \
        if ((size_t)n_ >= cap - len) { \
            cap = cap * 2 + (size_t)n_; \
            char *t_ = realloc(text, cap); \
            if (!t_) { rc = 7; break; } \
            text = t_; \
            n_ = snprintf(text + len, cap - len, __VA_ARGS__); \
        } \
        len += (size_t)n_; \
    } while (0)

    PUT("UFT-IPF-HELPER 1\nBLOB %s\nCYLS %u\nHEADS %u\nPLATFORM %u\n", beilage,
        (unsigned)(ii.maxcylinder + 1), (unsigned)(ii.maxhead + 1), (unsigned)ii.platform[0]);
    uint64_t off = 0;
    for (uint32_t c = ii.mincylinder; c <= ii.maxcylinder && rc == 0; c++) {
        for (uint32_t h = ii.minhead; h <= ii.maxhead && rc == 0; h++) {
            caps_track_info_t2 ti;
            memset(&ti, 0, sizeof ti);
            ti.type = 2;   /* ask for the T2 block (DI_LOCK_TYPE) */
            int32_t le = caps.locktrk(&ti, id, c, h,
                                      DI_LOCK_INDEX | DI_LOCK_DENVAR |
                                      DI_LOCK_UPDATEFD | DI_LOCK_TYPE |
                                      DI_LOCK_TRKBIT);
            if (le != 0) {
                /* A lock ERROR is not an empty track. Measured: an IPF cut
                 * to 20 000 bytes keeps every track header, and capsimg
                 * answers rc 2 for 163 of 164 tracks — listing them as
                 * "content unknown" would hide that the file is broken.
                 * H4: refuse the whole answer. */
                snprintf(grund, sizeof grund,
                         "track %u/%u: CAPSLockTrack %d (image damaged or truncated)",
                         (unsigned)c, (unsigned)h, (int)le);
                rc = 5;
                break;
            }
            if (!ti.trackbuf || ti.tracklen == 0) {
                /* rc 0, no cells: an unformatted track — it exists, it
                 * carries nothing (measured: cylinders 4..83 of the
                 * 4-cylinder disk-analyse IPF) */
                PUT("TRACK %u %u 0 0 0 0 0\n", (unsigned)c, (unsigned)h);
                caps.unlocktrk(id, c, h);
                continue;
            }
            uint32_t bits = ti.tracklen;              /* DI_LOCK_TRKBIT: in bits */
            uint32_t bytes = (bits + 7u) / 8u;
            if (fwrite(ti.trackbuf, 1, bytes, blob) != bytes) {
                snprintf(grund, sizeof grund, "blob write failed");
                rc = 6;
                caps.unlocktrk(id, c, h);
                break;
            }
            uint32_t flags = (ti.type & CTIT_FLAG_FLAKEY) ? 1u : 0u;
            /* density: capsimg does not hand out the file's density code,
             * so it is reported as 0 = not determined (PROTOCOL.md §3). */
            PUT("TRACK %u %u %u 0 %u %llu %u\n", (unsigned)c, (unsigned)h,
                (unsigned)bits, (unsigned)flags, (unsigned long long)off, (unsigned)bytes);
            off += bytes;
            caps.unlocktrk(id, c, h);
        }
    }
    if (rc == 0) PUT("END\n");
    if (rc == 0) {
        idx = fopen(index, "wb");
        if (!idx || fwrite(text, 1, len, idx) != len) {
            snprintf(grund, sizeof grund, "index file not writable");
            rc = 6;
        }
    } else if (!grund[0]) {
        snprintf(grund, sizeof grund, "out of memory");
    }
    free(text);

fertig:
    if (idx) fclose(idx);
    if (blob) fclose(blob);
    caps.unlockimg(id);
    caps.rem(id);
    caps.exit_();
    if (rc != 0) return absage(index, rc, grund);
    return 0;
}
