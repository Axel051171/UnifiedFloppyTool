/**
 * @file test_convert_geometry_override.c
 * @brief Zylinder jenseits des Zielformats, und der Bereich als Vorgabe
 *        (MF-482).
 *
 * Punkt 3.5 der a8rawconv-Gap-Analyse. a8rawconv hat `-g tracks,sides`
 * (1..84 / 1..2, `a8rawconv.cpp:1354-1364`) und benutzt es sowohl beim
 * Sichern als auch beim LESEN eines Abbilds — dort landet es als
 * `forced_tracks` / `forced_sides` in `scp_read()`
 * (`rawdiskscp.cpp:126-127`) und ueberschreibt die abgeleitete Ablage.
 *
 * Zwei Dinge stehen hier zur Pruefung:
 *
 *   1. **Was passiert mit Zylindern, die das Zielformat nicht fassen kann?**
 *      ADF ist auf 80 x 2 festgelegt. Eine Amiga-Diskette mit 82 oder 83
 *      Zylindern ist nichts Exotisches — Zusatzkapazitaet und Kopierschutz
 *      liegen genau dort, und der AmigaDOS-Dekoder laesst seit MF-452
 *      absichtlich Spuren bis 167 zu. Der Wandler kuerzte auf 80 und sagte
 *      es NICHT.
 *   2. **Kann der Aufrufer den Bereich selbst vorgeben?**
 *      `uft_convert_options_t::target_geometry` gibt es seit jeher — mit
 *      dem Kommentar „(0 = auto)" — und **niemand las es je**. Sechster
 *      Eintrag derselben Liste nach MF-471/473/474/479/480.
 */

#include "uft/uft_format_convert.h"
#include "uft/uft_format_plugin.h"
#include "uft/formats/uft_scp_writer.h"
#include "uft/uft_types.h"

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

#define FLEN 64

static void get_temp_path(char *path, size_t size, const char *tag)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_geo_%s_%d", dir, tag, rand() % 100000);
}

/**
 * Eine SCP-Datei mit @p cyls Zylindern x 2 Seiten.
 *
 * Der Flux ist absichtlich kurz und nicht dekodierbar: geprueft wird, was
 * der Wandler ueber den BEREICH sagt, nicht was er aus den Spuren holt.
 * Eine vollstaendige AmigaDOS-Spur je Zylinder waere hier 16 MB Testdaten
 * fuer eine Aussage, die davon nicht abhaengt.
 */
static int write_scp(const char *path, int cyls)
{
    scp_writer_t *w = scp_writer_create(SCP_TYPE_AMIGA, 1);
    if (!w) return -1;

    uint32_t f[FLEN];
    for (int i = 0; i < FLEN; i++) f[i] = 4000u;

    int rc = 0;
    for (int c = 0; c < cyls && rc == 0; c++)
        for (int s = 0; s < 2 && rc == 0; s++)
            rc = scp_writer_add_track(w, c, s, f, FLEN, FLEN * 4000u, 0);

    if (rc == 0) rc = scp_writer_save(w, path);
    scp_writer_free(w);
    return rc;
}

static bool warned_about(const uft_convert_result_t *r, const char *needle)
{
    for (int i = 0; i < r->warning_count; i++)
        if (strstr(r->warnings[i], needle)) return true;
    return false;
}

static void dump_warnings(const uft_convert_result_t *r)
{
    printf("\n");
    for (int i = 0; i < r->warning_count; i++)
        printf("        warn: %s\n", r->warnings[i]);
}

/** SCP -> ADF. @p cyls / @p heads = 0 heisst „keine Vorgabe". */
static uft_error_t convert(const char *scp, const char *out,
                           int cyls, int heads, uft_convert_result_t *res)
{
    uft_convert_options_t o = uft_convert_default_options();
    o.accept_data_loss = true;
    o.target_geometry.cylinders = (uint16_t)cyls;
    o.target_geometry.heads     = (uint16_t)heads;
    memset(res, 0, sizeof(*res));
    return uft_convert_file(scp, out, UFT_FORMAT_ADF, &o, res);
}

/* ────────────────────────────────────────────────────────────────────── */

TEST(cylinders_the_target_cannot_hold_are_reported_not_dropped)
{
    /* 83 Zylinder in ein Format, das 80 fasst. Gekuerzt werden MUSS — ein
     * ADF hat keinen Platz. Verschwiegen werden darf es nicht: „Kein Bit
     * verloren" heisst hier „kein Bit STILL verloren". */
    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "over.scp");
    get_temp_path(out, sizeof(out), "over.adf");
    ASSERT(write_scp(scp, 83) == 0);

    uft_convert_result_t r;
    ASSERT(convert(scp, out, 0, 0, &r) == UFT_OK);

    if (!warned_about(&r, "83")) dump_warnings(&r);
    ASSERT(warned_about(&r, "83"));      /* die Zahl, die die Datei traegt */
    ASSERT(warned_about(&r, "80"));      /* die Zahl, die ADF fasst */

    remove(scp); remove(out);
}

TEST(a_disk_within_the_target_says_nothing_about_truncation)
{
    /* Gegenprobe: 80 Zylinder passen, also darf keine Kuerzungsmeldung
     * kommen. Ohne diesen Test wuerde eine Meldung, die IMMER erscheint,
     * genauso gruen leuchten. */
    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "fit.scp");
    get_temp_path(out, sizeof(out), "fit.adf");
    ASSERT(write_scp(scp, 80) == 0);

    uft_convert_result_t r;
    ASSERT(convert(scp, out, 0, 0, &r) == UFT_OK);
    if (warned_about(&r, "fasst nur")) dump_warnings(&r);
    ASSERT(!warned_about(&r, "fasst nur"));

    remove(scp); remove(out);
}

TEST(an_explicit_range_limits_what_is_read)
{
    /* Die Vorgabe. 80 Zylinder in der Datei, gelesen werden sollen 40 —
     * a8rawconvs `-g 40,2`. Sichtbar wird das an der Zahl der Spuren, die
     * der Wandler ueberhaupt angefasst hat. */
    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "limit.scp");
    get_temp_path(out, sizeof(out), "limit.adf");
    ASSERT(write_scp(scp, 80) == 0);

    uft_convert_result_t full, limited;
    ASSERT(convert(scp, out, 0,  0, &full)    == UFT_OK);
    ASSERT(convert(scp, out, 40, 2, &limited) == UFT_OK);

    int n_full    = full.tracks_converted    + full.tracks_failed;
    int n_limited = limited.tracks_converted + limited.tracks_failed;
    if (n_full != 160 || n_limited != 80)
        printf("\n        ohne Vorgabe %d Spuren, mit 40,2 %d Spuren\n",
               n_full, n_limited);
    ASSERT(n_full == 160);
    ASSERT(n_limited == 80);

    remove(scp); remove(out);
}

TEST(one_side_can_be_selected)
{
    /* `-g 80,1`: nur Seite 0. Genau die Deutung, die Punkt 2.3 als
     * `scp-ss80` fuehrt — hier in Zahlen statt als Name. */
    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "ss.scp");
    get_temp_path(out, sizeof(out), "ss.adf");
    ASSERT(write_scp(scp, 80) == 0);

    uft_convert_result_t r;
    ASSERT(convert(scp, out, 80, 1, &r) == UFT_OK);
    int n = r.tracks_converted + r.tracks_failed;
    if (n != 80) printf("\n        %d Spuren statt 80\n", n);
    ASSERT(n == 80);

    remove(scp); remove(out);
}

TEST(a_range_beyond_the_target_is_refused_out_loud)
{
    /* Overdump in ein Format, das ihn nicht fassen kann: 84 Zylinder nach
     * ADF. Der Wandler darf das nicht stillschweigend auf 80 kuerzen und
     * auch nicht so tun, als haette er 84 geschrieben. */
    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "beyond.scp");
    get_temp_path(out, sizeof(out), "beyond.adf");
    ASSERT(write_scp(scp, 83) == 0);

    uft_convert_result_t r;
    ASSERT(convert(scp, out, 84, 2, &r) == UFT_OK);
    if (!warned_about(&r, "84")) dump_warnings(&r);
    ASSERT(warned_about(&r, "84"));

    remove(scp); remove(out);
}

int main(void)
{
    printf("=== Geometrie: Vorgabe und Ueberhang (MF-482) ===\n");

    if (uft_register_all_formats() != UFT_OK) {
        printf("FAIL: uft_register_all_formats()\n");
        return 1;
    }

    RUN(cylinders_the_target_cannot_hold_are_reported_not_dropped);
    RUN(a_disk_within_the_target_says_nothing_about_truncation);
    RUN(an_explicit_range_limits_what_is_read);
    RUN(one_side_can_be_selected);
    RUN(a_range_beyond_the_target_is_refused_out_loud);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
