/**
 * @file test_convert_output_verify.c
 * @brief Der Wandler sieht sein eigenes Ergebnis nach (MF-489).
 *
 * Zweimal hat dieser Baum ein Abbild aus lauter Nullen als **erfolgreiche**
 * Wandlung gemeldet:
 *
 *   MF-437  HFE->ADF: der IBM-Parser fand auf einer AmigaDOS-Spur nichts,
 *           der calloc'te Puffer blieb null, `tracks_converted++` lief
 *           trotzdem.
 *   MF-438  SCP->ADF: derselbe Fehler eine Schicht tiefer.
 *
 * Beide Male fiel es erst auf, als jemand die Datei aufmachte. Beide Male
 * haette eine einzige Frage am Ende gereicht: *traegt das, was ich gerade
 * geschrieben habe, ueberhaupt ein Dateisystem?*
 *
 * ── Der Erkenner lag die ganze Zeit daneben ──────────────────────────────
 *
 * `src/detect/mfm/` — 3609 Zeilen, 43 exportierte Symbole, Erkennung fuer
 * FAT12/16, AmigaDOS OFS/FFS/PFS, CP/M, Atari ST, MSX, CBM — mit
 * Konfidenz-Bewertung und Kandidatenliste. `mfm_detect.c` wird nur von
 * `uft_mfm_detect_bridge.c` gerufen, und die Bruecke von **niemandem**.
 *
 * Neunter Eintrag derselben Liste in dieser Woche, und der groesste nach
 * Symbolzahl.
 *
 * ── Was der Test zusichert ───────────────────────────────────────────────
 *
 * Nicht, dass ein unerkanntes Dateisystem ein Fehler waere — eine leere oder
 * fremdformatierte Diskette gibt es. Sondern dass der Wandler an dieser
 * Stelle den Mund aufmacht, statt „erfolgreich" zu melden und zu schweigen.
 */

#include "uft/uft_format_convert.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/formats/uft_scp_writer.h"
#include "uft/detect/uft_mfm_detect_bridge.h"
#include "flux_gen.h"                 /* tests/flux_gen/amigados */

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-52s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                    _fail++; return; } } while (0)

#define CELLS    UFT_AMIGADOS_CELLS_PER_REV
#define NTRACKS  2

static void get_temp_path(char *path, size_t size, const char *tag)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_outv_%s_%d", dir, tag, rand() % 100000);
}

/**
 * Ein ADF, das wirklich nach AmigaDOS aussieht.
 *
 * `uft_amigados_fill_pattern()` allein erzeugt nur Sektoren — der Erkenner
 * braucht die Bootblock-Kennung `DOS\0`. Ohne sie waere die Positivprobe
 * unten keine: sie wuerde denselben Weg nehmen wie die Negativprobe.
 */
static uint8_t *make_amigados_adf(void)
{
    uint8_t *adf = (uint8_t *)calloc(1, UFT_AMIGADOS_ADF_SIZE);
    if (!adf) return NULL;
    uft_amigados_fill_pattern(adf, (size_t)NTRACKS * UFT_AMIGADOS_SPT
                                   * UFT_AMIGADOS_SECSZ);
    memcpy(adf, "DOS\0", 4);          /* OFS-Bootblock */
    return adf;
}

/** SCP mit echtem, dekodierbarem AmigaDOS-Flux. */
static int write_scp_real(const char *path, const uint8_t *adf)
{
    uint8_t  *bits = (uint8_t *)calloc((CELLS + 7) / 8 + 1, 1);
    uint32_t *iv   = (uint32_t *)malloc(CELLS * sizeof(uint32_t));
    if (!bits || !iv) { free(bits); free(iv); return -1; }

    scp_writer_t *w = scp_writer_create(SCP_TYPE_AMIGA, 1);
    if (!w) { free(bits); free(iv); return -1; }

    int rc = 0;
    for (int track = 0; track < NTRACKS && rc == 0; track++) {
        uft_amigados_cells_t c = { bits, CELLS, 0, 0 };
        uft_amigados_build_track(&c, adf, (uint8_t)track, NULL);
        size_t n = uft_amigados_cells_to_intervals(&c, UFT_AMIGADOS_CELL_NS,
                                                   iv, CELLS);
        rc = scp_writer_add_track(w, track / 2, track % 2, iv, n,
                                  UFT_AMIGADOS_REV_NS, 0);
    }
    if (rc == 0) rc = scp_writer_save(w, path);
    scp_writer_free(w);
    free(bits); free(iv);
    return rc;
}

/** SCP mit Flux, aus dem kein Sektor faellt — Ergebnis: ein Abbild aus
 *  lauter Nullen, also genau der Fall aus MF-437/438. */
static int write_scp_undecodable(const char *path)
{
    scp_writer_t *w = scp_writer_create(SCP_TYPE_AMIGA, 1);
    if (!w) return -1;

    uint32_t f[64];
    for (int i = 0; i < 64; i++) f[i] = 4000u;

    int rc = 0;
    for (int track = 0; track < NTRACKS && rc == 0; track++)
        rc = scp_writer_add_track(w, track / 2, track % 2, f, 64,
                                  64u * 4000u, 0);
    if (rc == 0) rc = scp_writer_save(w, path);
    scp_writer_free(w);
    return rc;
}

static uft_error_t convert(const char *scp, const char *out,
                           uft_convert_result_t *res)
{
    uft_convert_options_t o = uft_convert_default_options();
    o.accept_data_loss = true;
    memset(res, 0, sizeof(*res));
    return uft_convert_file(scp, out, UFT_FORMAT_ADF, &o, res);
}

static bool warned_about(const uft_convert_result_t *r, const char *needle)
{
    for (int i = 0; i < r->warning_count && i < 8; i++)
        if (strstr(r->warnings[i], needle)) return true;
    return false;
}

static void dump_warnings(const uft_convert_result_t *r)
{
    printf("\n");
    for (int i = 0; i < r->warning_count && i < 8; i++)
        printf("        warn: %s\n", r->warnings[i]);
}

/* ────────────────────────────────────────────────────────────────────── */

TEST(an_image_with_no_filesystem_is_called_out)
{
    /* DIE Probe. Nicht dekodierbarer Flux ergibt ein Abbild aus lauter
     * Nullen — und der Wandler meldete dafuer bis MF-489 „erfolgreich"
     * ohne ein weiteres Wort. */
    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "empty.scp");
    get_temp_path(out, sizeof(out), "empty.adf");
    ASSERT(write_scp_undecodable(scp) == 0);

    uft_convert_result_t r;
    ASSERT(convert(scp, out, &r) == UFT_OK);

    if (!warned_about(&r, "kein erkennbares Dateisystem")) dump_warnings(&r);
    ASSERT(warned_about(&r, "kein erkennbares Dateisystem"));

    /* Und das Ergebnis ist wirklich leer — sonst pruefte die Meldung oben
     * etwas anderes als gedacht. */
    ASSERT(r.sectors_converted == 0);

    remove(scp); remove(out);
}

TEST(a_real_amigados_image_is_recognised_and_named)
{
    /* Die Gegenprobe. Ohne sie wuerde eine Meldung, die IMMER erscheint,
     * genauso gruen leuchten. */
    uint8_t *adf = make_amigados_adf();
    ASSERT(adf != NULL);

    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "real.scp");
    get_temp_path(out, sizeof(out), "real.adf");
    ASSERT(write_scp_real(scp, adf) == 0);

    uft_convert_result_t r;
    ASSERT(convert(scp, out, &r) == UFT_OK);
    ASSERT(r.sectors_converted == NTRACKS * UFT_AMIGADOS_SPT);

    if (warned_about(&r, "kein erkennbares Dateisystem")) dump_warnings(&r);
    ASSERT(!warned_about(&r, "kein erkennbares Dateisystem"));

    if (!warned_about(&r, "Amiga OFS")) dump_warnings(&r);
    ASSERT(warned_about(&r, "Dateisystem im Ergebnis"));
    ASSERT(warned_about(&r, "Amiga OFS"));

    free(adf); remove(scp); remove(out);
}

TEST(the_reported_filesystem_is_the_one_in_the_written_file)
{
    /* Kreuzprobe: was der Wandler MELDET, muss dem entsprechen, was in der
     * geschriebenen Datei wirklich steht. Sonst koennte die Meldung aus
     * einer anderen Quelle stammen als dem Ergebnis — und genau solche
     * Verwechslungen sucht dieser Baum. */
    uint8_t *adf = make_amigados_adf();
    ASSERT(adf != NULL);

    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "cross.scp");
    get_temp_path(out, sizeof(out), "cross.adf");
    ASSERT(write_scp_real(scp, adf) == 0);

    uft_convert_result_t r;
    ASSERT(convert(scp, out, &r) == UFT_OK);

    /* Die geschriebene Datei unabhaengig nachsehen. */
    FILE *f = fopen(out, "rb");
    ASSERT(f != NULL);
    uint8_t *got = (uint8_t *)malloc(UFT_AMIGADOS_ADF_SIZE);
    ASSERT(got != NULL);
    size_t n = fread(got, 1, UFT_AMIGADOS_ADF_SIZE, f);
    fclose(f);
    ASSERT(n == UFT_AMIGADOS_ADF_SIZE);

    uft_mfm_detect_info_t info;
    memset(&info, 0, sizeof(info));
    ASSERT(uft_mfmd_detect_image(got, n, &info) == UFT_MFMD_OK);
    ASSERT(info.num_candidates >= 1);
    ASSERT(info.fs_name != NULL);

    if (!warned_about(&r, info.fs_name)) {
        printf("\n        Datei sagt \"%s\"\n", info.fs_name);
        dump_warnings(&r);
    }
    ASSERT(warned_about(&r, info.fs_name));

    uft_mfmd_free(&info);
    free(got); free(adf); remove(scp); remove(out);
}

int main(void)
{
    printf("=== Der Wandler sieht sein eigenes Ergebnis nach (MF-489) ===\n");

    if (uft_register_all_formats() != UFT_OK) {
        printf("FAIL: uft_register_all_formats()\n");
        return 1;
    }

    RUN(an_image_with_no_filesystem_is_called_out);
    RUN(a_real_amigados_image_is_recognised_and_named);
    RUN(the_reported_filesystem_is_the_one_in_the_written_file);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
