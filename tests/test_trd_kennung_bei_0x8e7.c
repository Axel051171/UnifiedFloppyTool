/**
 * @file test_trd_kennung_bei_0x8e7.c
 * @brief The TRD probe looks for the TR-DOS ID where TR-DOS puts it:
 *        0x8E7, not 0x227 (MF-1405).
 *
 * `trd_probe()` in src/formats/trd/uft_trd.c gave its "real signature"
 * (confidence 92) for `data[0x227] == 0x10`, with the comment "TR-DOS disk
 * info at track 0, sector 9 (offset 0x800)". 0x227 is not in that sector:
 * it is byte 0x27 of the THIRD sector, inside the catalogue. The ID byte
 * lives at 0x800 + 0xE7 = 0x8E7.
 *
 * References, all independent of this probe:
 *   - SAMdisk `src/samdisk/scl.cpp` (MIT, in the tree): `pb[231] = 0x10`
 *     in the info sector, i.e. 0x8E7;
 *   - HxC `scl_loader.c` (GPL-2, read only): template byte 0x10 at the
 *     same place — both tabulated in the head of
 *     src/formats/scl/uft_scl_plugin.c (MF-1014);
 *   - two real TR-DOS images, measured 2026-09-27 from neue-ideen/1/:
 *     `WDC11sorc.trd` (wdc1.zip) and `betadisk/betadisk_dsdd.trd`
 *     (ZX-Spectrum-FD-Images-main.zip), both 655 360 bytes:
 *     [0x8E7] = 0x10, [0x8E3] = 0x16, and [0x227] = 0x00 in BOTH.
 *     With the old check neither real disk earned its ID; both got the
 *     size-only 45.
 *
 * The images are not committed (their redistribution is not measured);
 * the test builds the info sector the two oracles describe.
 */
#include "uft/uft_format_plugin.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_trd;

#define TRD_640K 655360u

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { gruen++; }
    else    { printf("  [ROT] %s -- %s\n", was, hinweis); rot++; }
}

static int sonde(const uint8_t *d, size_t n, int *conf)
{
    *conf = -1;
    return uft_format_plugin_trd.probe(d, n, TRD_640K, conf) ? 1 : 0;
}

int main(void)
{
    puts("=== TRD-Sonde: TR-DOS-Kennung bei 0x8E7 (MF-1405) ===");
    uint8_t *d = (uint8_t *)calloc(1, TRD_640K);
    if (!d) return 1;
    char h[160];
    int conf;

    /* 1: the info sector as SAMdisk and HxC write it. */
    d[0x8E3] = 0x16;              /* geometry: 80 tracks, 2 sides */
    d[0x8E7] = 0x10;              /* TR-DOS ID */
    int ja = sonde(d, TRD_640K, &conf);
    snprintf(h, sizeof h, "probe=%d confidence=%d", ja, conf);
    pruefe("TR-DOS-Infosatz (0x8E7 = 0x10) traegt die Kennung (>= 80)",
           ja && conf >= 80, h);

    /* 2: 0x10 in the catalogue at 0x227, nothing at 0x8E7 — that is a
     *    file entry byte, not an ID, and must not earn one. */
    memset(d, 0, TRD_640K);
    d[0x227] = 0x10;
    ja = sonde(d, TRD_640K, &conf);
    snprintf(h, sizeof h, "probe=%d confidence=%d", ja, conf);
    pruefe("0x10 bei 0x227 (Katalog) ist KEINE Kennung (< 50)",
           ja && conf < 50, h);

    /* 3: probe sees only the first 0x800 bytes -> no claim beyond size. */
    memset(d, 0, TRD_640K);
    d[0x8E7] = 0x10;
    ja = sonde(d, 0x800, &conf);
    snprintf(h, sizeof h, "probe=%d confidence=%d", ja, conf);
    pruefe("Puffer endet vor 0x8E7: kein Anspruch ueber die Groesse",
           ja && conf < 50, h);

    free(d);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
