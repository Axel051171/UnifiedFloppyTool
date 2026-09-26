/* Oberflaechen-Scan: ein Fehler bleibt auf seiner Seite, und ein zeitweise
 * unlesbarer Sektor 10 schrumpft keine Geometrie (H-18, MF-1339).
 *
 * NEGATIVE REFERENZ: CERTIFY 1.0 (Dan Moore/Dave Small, Antic 1988),
 * `certify.lzh!CERTIFY.C` Z. 173-194, gesichtet in
 * docs/research/ATARI_COPY_TOOLS_2026-09-23.md: dort liest der Fehlerzweig
 * von Seite 1 erneut Seite 0, und EIN Leseversuch auf Sektor 10 entscheidet
 * 9 oder 10 Sektoren je Spur.
 *
 * uft_diag_surface_scan() nimmt die Geometrie aus der Konfiguration und
 * wiederholt auf derselben Seite -- richtig, aber jeder vorhandene Test lief
 * mit `sides = 1`. Produktive Aufrufer: 0 (nur Tests). Die ST-Haelfte steht
 * in tests/test_ein_sektor_entscheidet_keine_geometrie.c.
 */
#include "uft/diag/uft_disc_diagnostics.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) {
        printf("  [ok ] %s\n", was);
        gruen++;
    } else {
        printf("  [ROT] %s%s%s\n", was, hinweis ? " -- " : "", hinweis ? hinweis : "");
        rot++;
    }
}

/* --- 1. Oberflaechen-Scan: Seite 0 gut, Seite 1 kaputt ------------------ */

static int lies_seite1_spur5_kaputt(int track, int side, int sector,
                                    uint8_t *buf, size_t n, void *ud)
{
    (void)sector; (void)ud;
    memset(buf, 0xE5, n);
    return (track == 5 && side == 1) ? -1 : 0;
}

static void t_seitenfehler_bleibt_auf_seite_1(void)
{
    puts("1. Seite 0 gut, Seite 1 fehlerhaft -- der Fehler steht auf Seite 1");
    uft_diag_config_t cfg = { .tracks = 10, .sides = 2, .sectors = 9,
                              .sector_size = 512, .retries = 2 };
    uft_diag_ctx_t ctx;
    if (uft_diag_init(&ctx, &cfg) != 0) { pruefe("init", 0, NULL); return; }
    int rc = uft_diag_surface_scan(&ctx, lies_seite1_spur5_kaputt, NULL);
    pruefe("Scan laeuft", rc == 0, NULL);

    uft_bad_sector_t liste[64];
    size_t n = 64;
    uft_diag_get_bad_sectors(&ctx, liste, &n);
    char h[80];
    snprintf(h, sizeof h, "%zu Eintraege", n);
    pruefe("genau neun schlechte Sektoren", n == 9, h);
    int alle_seite1 = (n > 0);
    for (size_t i = 0; i < n && i < 64; i++)
        if (liste[i].side != 1 || liste[i].track != 5) alle_seite1 = 0;
    pruefe("alle auf Spur 5, Seite 1", alle_seite1, NULL);

    const uft_diag_track_result_t *s0 = &ctx.track_results[5 * 2 + 0];
    const uft_diag_track_result_t *s1 = &ctx.track_results[5 * 2 + 1];
    pruefe("Spur 5 Seite 0 ohne Fehler", s0->side == 0 && s0->bad_sectors == 0, NULL);
    pruefe("Spur 5 Seite 1 mit neun Fehlern", s1->side == 1 && s1->bad_sectors == 9, NULL);
    uft_diag_free(&ctx);
}

/* --- 2. Oberflaechen-Scan: Sektor 10 zeitweise unlesbar ----------------- */

typedef struct { int versuche[80][2]; } zaehler_t;

static int lies_sektor10_erst_beim_zweiten_mal(int track, int side, int sector,
                                               uint8_t *buf, size_t n, void *ud)
{
    zaehler_t *z = (zaehler_t *)ud;
    memset(buf, 0xE5, n);
    if (sector != 10) return 0;
    return (z->versuche[track][side]++ == 0) ? -1 : 0;
}

static void t_zeitweiser_sektor10_schrumpft_nichts(void)
{
    puts("2. Sektor 10 beim ersten Versuch unlesbar -- 10 bleiben 10");
    uft_diag_config_t cfg = { .tracks = 4, .sides = 2, .sectors = 10,
                              .sector_size = 512, .retries = 3 };
    uft_diag_ctx_t ctx;
    if (uft_diag_init(&ctx, &cfg) != 0) { pruefe("init", 0, NULL); return; }
    zaehler_t *z = calloc(1, sizeof *z);
    int rc = uft_diag_surface_scan(&ctx, lies_sektor10_erst_beim_zweiten_mal, z);
    pruefe("Scan laeuft", rc == 0, NULL);
    char h[80];
    snprintf(h, sizeof h, "total=%d", ctx.total_sectors);
    pruefe("80 Sektoren gezaehlt (4 x 2 x 10)", ctx.total_sectors == 80, h);
    pruefe("kein Sektor schlecht", ctx.bad_sectors == 0, NULL);
    snprintf(h, sizeof h, "weak=%d", ctx.weak_sectors);
    pruefe("acht schwache Sektoren, je Spur und Seite Sektor 10", ctx.weak_sectors == 8, h);
    pruefe("Sektor 10 auf Spur 3 Seite 1 ist WEAK",
           ctx.track_results[3 * 2 + 1].sector_status[9] == UFT_SECTOR_WEAK, NULL);
    free(z);
    uft_diag_free(&ctx);
}

int main(void)
{
    puts("=== Oberflaechen-Scan: Seite und Sektor 10 (H-18) ===");
    t_seitenfehler_bleibt_auf_seite_1();
    t_zeitweiser_sektor10_schrumpft_nichts();
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
