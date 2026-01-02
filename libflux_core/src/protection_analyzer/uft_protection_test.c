/*
 * uft_protection_test.c
 *
 * Simple CLI that:
 *  1) opens .IMG/.ATR/.D64 via protection_analyzer (as a device)
 *  2) runs floppy_analyze_protection()
 *  3) exports either:
 *      - .imd (default)
 *      - .atx (uft stub) if --atx is specified
 *
 * Build:
 *   cc -O2 -Wall -Wextra -std=c11 protection_analyzer.c uft_protection_test.c -o uft_protection_test
 *
 * Usage:
 *   ./uft_protection_test input.atr out.imd
 *   ./uft_protection_test --atx input.atr out.atx
 */

#include "uft/uft_error.h"
#include "protection_analyzer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void print_report(const UFT_ProtectionReport* r) {
    if (!r) { puts("No report."); return; }
    printf("Platform=%u  PrimaryProtection=%u\n", (unsigned)r->platform, (unsigned)r->primaryProtection);
    printf("BadSectors=%u  WeakRegions=%u\n", (unsigned)r->badSectorCount, (unsigned)r->weakRegionCount);

    for (size_t i = 0; i < r->badSectorCount; i++) {
        const UFT_BadSector* b = &r->badSectors[i];
        printf("  BAD  C=%u H=%u S=%u  reason=%u\n",
               (unsigned)b->chs.c, (unsigned)b->chs.h, (unsigned)b->chs.s, (unsigned)b->reason);
    }
    for (size_t i = 0; i < r->weakRegionCount; i++) {
        const UFT_WeakRegion* w = &r->weakRegions[i];
        printf("  WEAK C=%u H=%u S=%u  off=%u len=%u cell_ns=%u jitter_ns=%u seed=%u prot=%u\n",
               (unsigned)w->chs.c, (unsigned)w->chs.h, (unsigned)w->chs.s,
               (unsigned)w->byteOffset, (unsigned)w->byteLength,
               (unsigned)w->cell_ns, (unsigned)w->jitter_ns,
               (unsigned)w->seed, (unsigned)w->protectionId);
    }
}

int main(int argc, char** argv) {
    int want_atx = 0;
    const char* in = NULL;
    const char* out = NULL;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s [--atx] <input.img|input.atr|input.d64> <output.imd|output.atx>\n", argv[0]);
        return 2;
    }

    int argi = 1;
    if (strcmp(argv[argi], "--atx") == 0) { want_atx = 1; argi++; }

    if (argc - argi < 2) {
        fprintf(stderr, "Not enough args.\n");
        return 2;
    }

    in = argv[argi++];
    out = argv[argi++];

    FloppyInterface dev;
    int rc = floppy_open(&dev, in);
    if (rc != 0) { fprintf(stderr, "floppy_open failed: %d\n", rc); return 1; }

    rc = floppy_analyze_protection(&dev);
    if (rc != 0) { fprintf(stderr, "floppy_analyze_protection failed: %d\n", rc); floppy_close(&dev); return 1; }

    const UFT_ProtectionReport* rep = uft_get_last_report(&dev);
    print_report(rep);

    if (want_atx) rc = uft_export_atx_stub(&dev, out);
    else          rc = uft_export_imd(&dev, out);

    if (rc != 0) { fprintf(stderr, "export failed: %d\n", rc); floppy_close(&dev); return 1; }

    printf("Wrote: %s\n", out);
    floppy_close(&dev);
    return 0;
}
