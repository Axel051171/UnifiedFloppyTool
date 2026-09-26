/* Oberflaechen-Scan: eine Konfiguration ausserhalb der Grenzen wird
 * abgesagt, nicht gelesen (Tuerstufe 4, Teil A).
 *
 * Gelesen in src/diag/uft_disc_diagnostics.c vor dieser Stufe:
 *
 *   - `sector_status[UFT_DIAG_MAX_SECTORS_PER_TRACK]` (36) wurde mit
 *     `sector_status[sec]` fuer sec < config.sectors beschrieben, ohne dass
 *     init oder scan `sectors <= 36` prueften;
 *   - `total_time / ctx->config.sectors` teilte bei sectors == 0 ganzzahlig
 *     durch null;
 *   - bei retries == 0 lief die Leseschleife nie, jeder Sektor galt als BAD,
 *     ohne dass ein einziger Leseversuch stattfand -- ein Befund ohne Messung;
 *   - MAX_TRACKS (84) und MAX_SIDES (2) waren definiert und unbenutzt;
 *   - track_results wird in init fuer tracks x sides angelegt, der Scan
 *     lief aber ueber die AKTUELLE config -- wer sie nach init vergroesserte,
 *     schrieb hinter die Anlage.
 *
 * Rotbeweis: ASan/UBSan gegen den Vorzustand (sectors=37 -> UBSan
 * "index 36 out of bounds", sectors=64 nach init -> ASan
 * heap-buffer-overflow, sectors=0 -> UBSan "division by zero").
 *
 * Ein einzelner Fall laesst sich mit seinem Namen als argv[1] allein
 * starten -- noetig fuer den Rotbeweis, weil der Vorzustand beim
 * Teilen durch null abstuerzt und die Faelle danach sonst nie liefen.
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

typedef struct { long lesungen; } zaehler_t;

static int lies_gut(int t, int s, int sec, uint8_t *buf, size_t n, void *ud)
{
    (void)t; (void)s; (void)sec;
    if (ud) ((zaehler_t *)ud)->lesungen++;
    memset(buf, 0xE5, n);
    return 0;
}

static uft_diag_config_t gueltig(void)
{
    uft_diag_config_t c;
    memset(&c, 0, sizeof c);
    c.tracks = 2; c.sides = 1; c.sectors = 9; c.sector_size = 512; c.retries = 1;
    return c;
}

/* init MUSS absagen; tut es das nicht, laeuft der Scan trotzdem -- damit
 * der Rotbeweis den Folgeschaden unter dem Sanitizer sichtbar macht. */
static void init_muss_absagen(const char *was, uft_diag_config_t cfg)
{
    uft_diag_ctx_t ctx;
    zaehler_t z = { 0 };
    int rc = uft_diag_init(&ctx, &cfg);
    char h[96];
    snprintf(h, sizeof h, "uft_diag_init gab %d, erwartet -1", rc);
    pruefe(was, rc == -1, h);
    if (rc == 0) {
        int sr = uft_diag_surface_scan(&ctx, lies_gut, &z);
        printf("        (Vorzustand: Scan lief trotzdem, rc=%d, %ld Lesungen, "
               "%d BAD)\n", sr, z.lesungen, ctx.bad_sectors);
        uft_diag_free(&ctx);
    }
}

static void f_sektoren_37(void)
{
    uft_diag_config_t c = gueltig();
    c.sectors = UFT_DIAG_MAX_SECTORS_PER_TRACK + 1;   /* 37 */
    init_muss_absagen("sectors = 37 (eins ueber sector_status[36]) wird abgesagt", c);
}

static void f_sektoren_0(void)
{
    uft_diag_config_t c = gueltig();
    c.sectors = 0;
    init_muss_absagen("sectors = 0 wird abgesagt (vorher: Teilen durch null)", c);
}

static void f_sektoren_negativ(void)
{
    uft_diag_config_t c = gueltig();
    c.sectors = -1;
    init_muss_absagen("sectors = -1 wird abgesagt", c);
}

static void f_versuche_0(void)
{
    uft_diag_config_t c = gueltig();
    c.retries = 0;
    init_muss_absagen("retries = 0 wird abgesagt (vorher: alles BAD ohne einen "
                      "Leseversuch)", c);
}

static void f_spuren(void)
{
    uft_diag_config_t c = gueltig();
    c.tracks = 0;
    init_muss_absagen("tracks = 0 wird abgesagt", c);
    c.tracks = 85;       /* MAX_TRACKS ist 84 */
    init_muss_absagen("tracks = 85 (ueber MAX_TRACKS 84) wird abgesagt", c);
}

static void f_seiten(void)
{
    uft_diag_config_t c = gueltig();
    c.sides = 0;
    init_muss_absagen("sides = 0 wird abgesagt", c);
    c.sides = 3;
    init_muss_absagen("sides = 3 (ueber MAX_SIDES 2) wird abgesagt", c);
}

static void f_sektorgroesse(void)
{
    uft_diag_config_t c = gueltig();
    c.sector_size = 0;
    init_muss_absagen("sector_size = 0 wird abgesagt", c);
}

static void f_grenzfaelle_bleiben_gueltig(void)
{
    /* Die Grenzen selbst sind erlaubt: 84 Spuren, 2 Seiten, 36 Sektoren,
     * 1 Versuch. Eine zu enge Pruefung waere eine neue stille Absage. */
    uft_diag_config_t c = gueltig();
    c.tracks = 84; c.sides = 2; c.sectors = UFT_DIAG_MAX_SECTORS_PER_TRACK;
    c.retries = 1;
    uft_diag_ctx_t ctx;
    zaehler_t z = { 0 };
    pruefe("84 x 2 x 36 mit einem Versuch wird angenommen",
           uft_diag_init(&ctx, &c) == 0, NULL);
    int rc = uft_diag_surface_scan(&ctx, lies_gut, &z);
    char h[96];
    snprintf(h, sizeof h, "rc=%d, %ld Lesungen", rc, z.lesungen);
    pruefe("und liest 6048 Sektoren je genau einmal",
           rc == 0 && z.lesungen == 84L * 2 * 36 && ctx.good_sectors == 6048, h);
    pruefe("Sektor 36 der letzten Spur steht als GOOD",
           ctx.track_results[83 * 2 + 1].sector_status[35] == UFT_DIAG_SECTOR_GOOD,
           NULL);
    uft_diag_free(&ctx);
}

/* Der Scan prueft selbst: ctx ist oeffentlich, config laesst sich nach init
 * aendern. Ohne eigene Pruefung schriebe der Scan hinter die Anlage. */
static void f_config_nach_init_vergroessert(void)
{
    uft_diag_config_t c = gueltig();
    c.tracks = 1; c.sides = 1;
    uft_diag_ctx_t ctx;
    zaehler_t z = { 0 };
    if (uft_diag_init(&ctx, &c) != 0) { pruefe("init (1x1x9)", 0, NULL); return; }

    ctx.config.sectors = 64;   /* sector_status[40] liegt HINTER der Anlage */
    int rc = uft_diag_surface_scan(&ctx, lies_gut, &z);
    char h[96];
    snprintf(h, sizeof h, "rc=%d, %ld Lesungen", rc, z.lesungen);
    pruefe("Scan sagt ab, wenn sectors nach init auf 64 steigt", rc == -1, h);
    pruefe("... und liest dabei nichts", z.lesungen == 0, h);

    uft_bad_sector_t liste[8];
    size_t n = 8;
    int br = uft_diag_get_bad_sectors(&ctx, liste, &n);
    snprintf(h, sizeof h, "rc=%d", br);
    pruefe("get_bad_sectors sagt bei sectors = 64 ebenfalls ab", br == -1, h);

    ctx.config.sectors = 9;
    ctx.config.tracks = 3;     /* angelegt ist 1 x 1 */
    z.lesungen = 0;
    rc = uft_diag_surface_scan(&ctx, lies_gut, &z);
    snprintf(h, sizeof h, "rc=%d, %ld Lesungen", rc, z.lesungen);
    pruefe("Scan sagt ab, wenn tracks nach init ueber die Anlage steigt",
           rc == -1 && z.lesungen == 0, h);
    n = 8;
    br = uft_diag_get_bad_sectors(&ctx, liste, &n);
    snprintf(h, sizeof h, "rc=%d", br);
    pruefe("get_bad_sectors ebenso", br == -1, h);

    ctx.config.tracks = 1;
    ctx.config.retries = 0;
    z.lesungen = 0;
    rc = uft_diag_surface_scan(&ctx, lies_gut, &z);
    snprintf(h, sizeof h, "rc=%d, %d BAD", rc, ctx.bad_sectors);
    pruefe("Scan sagt bei retries = 0 nach init ab, statt alles BAD zu melden",
           rc == -1 && ctx.bad_sectors == 0, h);
    uft_diag_free(&ctx);
}

/* Die Anlagegrenze EXAKT: tracks x sides == angelegt + 1. Der Fall oben
 * (3 gegen 1) laesst eine Pruefung durch, die sich um eins verzaehlt
 * (`need <= count + 1` -- so von der Mutationsmatrix gefunden); diese
 * schriebe genau einen Eintrag hinter die Heap-Anlage. */
static void f_anlage_plus_eins(void)
{
    uft_diag_config_t c = gueltig();
    uft_diag_ctx_t ctx;
    zaehler_t z = { 0 };
    uft_bad_sector_t liste[8];
    size_t n;
    int rc, br;
    char h[96];

    /* Spuren: angelegt 2 x 1, danach 3 x 1 */
    c.tracks = 2; c.sides = 1;
    if (uft_diag_init(&ctx, &c) != 0) { pruefe("init (2x1x9)", 0, NULL); return; }
    snprintf(h, sizeof h, "track_results_count=%zu", ctx.track_results_count);
    pruefe("init legt 2 x 1 = 2 Eintraege an", ctx.track_results_count == 2, h);
    ctx.config.tracks = 3;
    rc = uft_diag_surface_scan(&ctx, lies_gut, &z);
    snprintf(h, sizeof h, "rc=%d, %ld Lesungen", rc, z.lesungen);
    pruefe("tracks 2 -> 3 nach init (3 = angelegt + 1): Scan sagt ab, 0 Lesungen",
           rc == -1 && z.lesungen == 0, h);
    n = 8;
    br = uft_diag_get_bad_sectors(&ctx, liste, &n);
    snprintf(h, sizeof h, "rc=%d", br);
    pruefe("... get_bad_sectors ebenso", br == -1, h);

    /* Gegenprobe an der Grenze selbst: 1 x 2 == 2 angelegt wird angenommen
     * (eine Pruefung mit `<` statt `<=` saegte hier ab). */
    ctx.config.tracks = 1; ctx.config.sides = 2;
    z.lesungen = 0;
    rc = uft_diag_surface_scan(&ctx, lies_gut, &z);
    snprintf(h, sizeof h, "rc=%d, %ld Lesungen", rc, z.lesungen);
    pruefe("Gegenprobe: 1 x 2 = genau angelegt wird gelesen (18 Lesungen)",
           rc == 0 && z.lesungen == 18, h);
    uft_diag_free(&ctx);

    /* Seiten: angelegt 1 x 1, danach 1 x 2 bei gleicher Spurzahl */
    c.tracks = 1; c.sides = 1;
    if (uft_diag_init(&ctx, &c) != 0) { pruefe("init (1x1x9)", 0, NULL); return; }
    ctx.config.sides = 2;
    z.lesungen = 0;
    rc = uft_diag_surface_scan(&ctx, lies_gut, &z);
    snprintf(h, sizeof h, "rc=%d, %ld Lesungen", rc, z.lesungen);
    pruefe("sides 1 -> 2 nach init bei 1 Spur (2 = angelegt + 1): Scan sagt ab, "
           "0 Lesungen", rc == -1 && z.lesungen == 0, h);
    n = 8;
    br = uft_diag_get_bad_sectors(&ctx, liste, &n);
    snprintf(h, sizeof h, "rc=%d", br);
    pruefe("... get_bad_sectors ebenso", br == -1, h);
    uft_diag_free(&ctx);
}

typedef struct { const char *name; void (*fn)(void); } fall_t;

static const fall_t FAELLE[] = {
    { "grenzfaelle",  f_grenzfaelle_bleiben_gueltig },
    { "versuche0",    f_versuche_0 },
    { "spuren",       f_spuren },
    { "seiten",       f_seiten },
    { "groesse",      f_sektorgroesse },
    { "negativ",      f_sektoren_negativ },
    { "sektoren37",   f_sektoren_37 },
    { "nachinit",     f_config_nach_init_vergroessert },
    { "anlageplus1",  f_anlage_plus_eins },
    { "sektoren0",    f_sektoren_0 },      /* zuletzt: Vorzustand stuerzt ab */
};

int main(int argc, char **argv)
{
    puts("=== Oberflaechen-Scan: Grenzen der Konfiguration ===");
    size_t n = sizeof FAELLE / sizeof FAELLE[0];
    int gelaufen = 0;
    for (size_t i = 0; i < n; i++) {
        if (argc > 1 && strcmp(argv[1], FAELLE[i].name) != 0) continue;
        printf("- %s\n", FAELLE[i].name);
        FAELLE[i].fn();
        gelaufen++;
    }
    if (gelaufen == 0) {
        printf("  [ROT] kein Fall heisst '%s'\n", argc > 1 ? argv[1] : "");
        return 1;
    }
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
