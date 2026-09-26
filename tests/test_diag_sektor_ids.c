/* Oberflaechen-Scan: die Sektornummern kommen vom Aufrufer, nicht aus
 * `sec + 1` (Tuerstufe 4, Teil A).
 *
 * Vorzustand, gelesen in src/diag/uft_disc_diagnostics.c: der Scan fragte
 * je Spur fest die Nummern 1..sectors ab (`read_fn(t, s, sec + 1, ...)`).
 * Zwei Formatfamilien, die dieser Baum liest, passen da nicht hinein:
 *
 *   - Amstrad CPC Datenformat: Nummern 0xC1..0xC9 (libdsk `cpcdata`,
 *     dg_secbase = 0xC1; so auch `amstrad-cpc` in der CP/M-Tafel, MF-1039);
 *   - 0-basierte Formate: JV1 0..9 (MF-1016), MYZ80 0..127 (MF-1029).
 *
 * Ein Scan einer CPC-Datendiskette meldete damit JEDEN Sektor als BAD --
 * nicht weil die Diskette schlecht ist, sondern weil nach Nummern gefragt
 * wurde, die es auf ihr nicht gibt. Und die Fehlerliste nannte `sec + 1`,
 * also Nummern, die nie gelesen wurden.
 *
 * Seit Stufe 4 traegt uft_diag_config_t (angehaengt) eine Nummernliste;
 * sector_id_count == 0 laesst das alte Verhalten 1..n stehen.
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

/* Eine CPC-Datendiskette: nur 0xC1..0xC9 stehen darauf. Optional faellt
 * genau ein Sektor aus (Spur/Seite/Nummer), um die Fehlerliste zu pruefen. */
typedef struct {
    int  fremd_gefragt;          /* Anfragen nach Nummern, die es nicht gibt */
    int  kaputt_t, kaputt_s, kaputt_id;
} cpc_t;

static int lies_cpc(int t, int s, int sec, uint8_t *buf, size_t n, void *ud)
{
    cpc_t *c = (cpc_t *)ud;
    if (sec < 0xC1 || sec > 0xC9) { c->fremd_gefragt++; return -1; }
    if (t == c->kaputt_t && s == c->kaputt_s && sec == c->kaputt_id) return -1;
    memset(buf, (uint8_t)sec, n);
    return 0;
}

static uft_diag_config_t cpc_config(int mit_liste)
{
    uft_diag_config_t c;
    memset(&c, 0, sizeof c);
    c.tracks = 4; c.sides = 2; c.sectors = 9; c.sector_size = 512; c.retries = 1;
    if (mit_liste) {
        for (int i = 0; i < 9; i++) c.sector_ids[i] = (uint8_t)(0xC1 + i);
        c.sector_id_count = 9;
    }
    return c;
}

static void t_cpc_ohne_liste_alles_bad(void)
{
    puts("1. CPC-Diskette ohne Nummernliste -- das alte Verhalten, alles BAD");
    uft_diag_config_t c = cpc_config(0);
    uft_diag_ctx_t ctx;
    cpc_t cpc = { 0, -1, -1, -1 };
    if (uft_diag_init(&ctx, &c) != 0) { pruefe("init", 0, NULL); return; }
    int rc = uft_diag_surface_scan(&ctx, lies_cpc, &cpc);
    char h[96];
    snprintf(h, sizeof h, "rc=%d, BAD=%d, GOOD=%d", rc, ctx.bad_sectors, ctx.good_sectors);
    pruefe("72 von 72 BAD (gefragt wurde nach 1..9)",
           rc == 0 && ctx.bad_sectors == 72 && ctx.good_sectors == 0, h);
    snprintf(h, sizeof h, "%d fremde Anfragen", cpc.fremd_gefragt);
    pruefe("jede Anfrage galt einer Nummer, die es nicht gibt",
           cpc.fremd_gefragt == 72, h);
    uft_diag_free(&ctx);
}

static void t_cpc_mit_liste_alles_good(void)
{
    puts("2. CPC-Diskette mit Nummernliste 0xC1..0xC9 -- alles GOOD");
    uft_diag_config_t c = cpc_config(1);
    uft_diag_ctx_t ctx;
    cpc_t cpc = { 0, -1, -1, -1 };
    if (uft_diag_init(&ctx, &c) != 0) { pruefe("init mit Liste", 0, NULL); return; }
    int rc = uft_diag_surface_scan(&ctx, lies_cpc, &cpc);
    char h[96];
    snprintf(h, sizeof h, "rc=%d, BAD=%d, GOOD=%d", rc, ctx.bad_sectors, ctx.good_sectors);
    pruefe("72 von 72 GOOD", rc == 0 && ctx.good_sectors == 72 && ctx.bad_sectors == 0, h);
    snprintf(h, sizeof h, "%d fremde Anfragen", cpc.fremd_gefragt);
    pruefe("keine Anfrage nach einer fremden Nummer", cpc.fremd_gefragt == 0, h);
    uft_diag_free(&ctx);
}

static void t_fehlerliste_nennt_die_echte_nummer(void)
{
    puts("3. Ein ausgefallener Sektor 0xC5 auf Spur 2 Seite 1 -- die Liste nennt ihn");
    uft_diag_config_t c = cpc_config(1);
    uft_diag_ctx_t ctx;
    cpc_t cpc = { 0, 2, 1, 0xC5 };
    if (uft_diag_init(&ctx, &c) != 0) { pruefe("init", 0, NULL); return; }
    uft_diag_surface_scan(&ctx, lies_cpc, &cpc);
    uft_bad_sector_t liste[8];
    size_t n = 8;
    int rc = uft_diag_get_bad_sectors(&ctx, liste, &n);
    char h[128];
    snprintf(h, sizeof h, "rc=%d, n=%zu", rc, n);
    pruefe("genau ein Eintrag", rc == 0 && n == 1, h);
    if (n >= 1) {
        snprintf(h, sizeof h, "Spur %d Seite %d Sektor 0x%02X",
                 liste[0].track, liste[0].side, (unsigned)liste[0].sector);
        pruefe("Spur 2, Seite 1, Sektor 0xC5 (nicht Position+1 = 5)",
               liste[0].track == 2 && liste[0].side == 1 && liste[0].sector == 0xC5, h);
    }
    pruefe("sector_status[] bleibt nach Position gefuehrt (Stelle 4 = 0xC5)",
           ctx.track_results[2 * 2 + 1].sector_status[4] == UFT_DIAG_SECTOR_BAD &&
           ctx.track_results[2 * 2 + 1].sector_status[3] == UFT_DIAG_SECTOR_GOOD, NULL);
    uft_diag_free(&ctx);
}

/* JV1: Nummern 0..9 auf einer Seite (MF-1016). */
static int lies_null_basiert(int t, int s, int sec, uint8_t *buf, size_t n, void *ud)
{
    (void)t; (void)s; (void)ud;
    if (sec < 0 || sec > 9) return -1;
    memset(buf, 0, n);
    return 0;
}

static void t_null_basiert(void)
{
    puts("4. 0-basierte Nummern 0..9 (JV1)");
    uft_diag_config_t c;
    memset(&c, 0, sizeof c);
    c.tracks = 3; c.sides = 1; c.sectors = 10; c.sector_size = 256; c.retries = 1;
    uft_diag_ctx_t ctx;

    if (uft_diag_init(&ctx, &c) != 0) { pruefe("init", 0, NULL); return; }
    uft_diag_surface_scan(&ctx, lies_null_basiert, NULL);
    char h[96];
    snprintf(h, sizeof h, "BAD=%d", ctx.bad_sectors);
    pruefe("ohne Liste: Nummer 10 fehlt je Spur -- 3 BAD", ctx.bad_sectors == 3, h);
    uft_diag_free(&ctx);

    for (int i = 0; i < 10; i++) c.sector_ids[i] = (uint8_t)i;
    c.sector_id_count = 10;
    if (uft_diag_init(&ctx, &c) != 0) { pruefe("init mit Liste 0..9", 0, NULL); return; }
    uft_diag_surface_scan(&ctx, lies_null_basiert, NULL);
    snprintf(h, sizeof h, "GOOD=%d BAD=%d", ctx.good_sectors, ctx.bad_sectors);
    pruefe("mit Liste 0..9: 30 von 30 GOOD", ctx.good_sectors == 30 && ctx.bad_sectors == 0, h);
    uft_diag_free(&ctx);
}

static void t_liste_passt_nicht(void)
{
    puts("5. Eine Liste, die nicht zur Sektorzahl passt, wird abgesagt");
    uft_diag_config_t c = cpc_config(1);
    uft_diag_ctx_t ctx;

    c.sector_id_count = 8;                 /* sectors ist 9 */
    int rc = uft_diag_init(&ctx, &c);
    if (rc == 0) uft_diag_free(&ctx);
    pruefe("8 Nummern fuer 9 Sektoren: abgesagt", rc == -1, NULL);

    c = cpc_config(1);
    c.sector_ids[8] = 0xC1;                /* 0xC1 doppelt */
    rc = uft_diag_init(&ctx, &c);
    if (rc == 0) uft_diag_free(&ctx);
    pruefe("doppelte Nummer 0xC1: abgesagt (ein Sektor, zwei Plaetze)", rc == -1, NULL);

    c = cpc_config(1);
    c.sector_id_count = -1;
    rc = uft_diag_init(&ctx, &c);
    if (rc == 0) uft_diag_free(&ctx);
    pruefe("negative Anzahl: abgesagt", rc == -1, NULL);
}

int main(void)
{
    puts("=== Oberflaechen-Scan: Sektornummern vom Aufrufer ===");
    t_cpc_ohne_liste_alles_bad();
    t_cpc_mit_liste_alles_good();
    t_fehlerliste_nennt_die_echte_nummer();
    t_null_basiert();
    t_liste_passt_nicht();
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
