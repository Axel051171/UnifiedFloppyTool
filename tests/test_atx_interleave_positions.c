/**
 * @file test_atx_interleave_positions.c
 * @brief Fehlende Winkelpositionen: das Atari-Layout, nicht gleiche Abstaende
 *        (MF-479).
 *
 * `uft_atx_write()` muss auch Spuren schreiben koennen, die keine gemessenen
 * Winkelpositionen mitbringen — jede Spur, die nicht aus einem ATX stammt,
 * ist so eine. Bis MF-479 verteilte der Schreiber sie dann als `s / n`:
 * gleiche Abstaende, erster Sektor bei 0.
 *
 * So liegt keine Atari-Diskette. Das DOS schreibt SD und ED verschraenkt
 * (etwa 9:1 bzw. 13:1), und jede Spur ist gegen die vorige um rund 8 % einer
 * Umdrehung versetzt, weil der Kopf Zeit zum Umsetzen braucht. Bei ATX ist
 * die Winkelposition kopierschutzrelevant — ein gleichmaessiges Layout sieht
 * plausibel aus und ist eine Form, die es auf dem Medium nicht gibt.
 *
 * `uft_compute_interleave()` (src/core/uft_interleave.c) rechnet dieses
 * Layout seit langem, ist die wortgleiche Portierung von a8rawconvs
 * `compute_interleave` und hatte bis MF-479 **keinen Aufrufer**.
 *
 * Was dieser Test NICHT behauptet: dass die Positionen stimmen. Sie sind
 * gerechnet, nicht gemessen, und `uft_atx_write()` sagt das weiterhin ueber
 * UFT_WARN. Geprueft wird, dass das gerechnete Layout das Atari-Layout ist
 * und nicht das triviale.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_format_common.h"  /* uft_format_add_sector_with_id */
#include "uft/formats/atx.h"
#include "uft/core/uft_interleave.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_atx;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-50s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                    _fail++; return; } } while (0)

#define SEC      128        /* Atari SD */
#define SD_SPT   18         /* Sektoren je Spur, Single Density */
/* ATX zaehlt Winkel in 1/26042 Umdrehung; enger als eine Einheit kann ein
 * Rundlauf nicht sein. */
#define ATX_UNIT (1.0 / 26042.0)

static void get_temp_path(char *path, size_t size, const char *tag)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_atxil_%s_%d.atx", dir, tag, rand() % 100000);
}

static void free_track(uft_track_t *tr)
{
    for (size_t i = 0; i < tr->sector_count; i++) {
        free(tr->sectors[i].data);
        free(tr->sectors[i].weak_mask);
    }
    free(tr->sectors);
    tr->sectors = NULL; tr->sector_count = 0;
}

/** Eine SD-Spur mit 18 Sektoren und OHNE Winkelpositionen. */
static void build_bare_track(uft_track_t *tr, int cyl, int spt, int secsz)
{
    memset(tr, 0, sizeof(*tr));
    uft_track_init(tr, cyl, 0);
    tr->encoding = UFT_ENCODING_FM;

    uint8_t payload[512];
    for (int s = 0; s < spt; s++) {
        memset(payload, (uint8_t)(0x10 + s), (size_t)secsz);
        uft_format_add_sector_with_id(tr, (uint8_t)(s + 1), payload,
                                      (size_t)secsz, 0, 0);
        /* has_angular_position bleibt false — genau der Fall. */
    }
}

/** Spur schreiben, zurueckoeffnen, Positionen einsammeln. */
static bool write_read_positions(const char *path, const uft_track_t *tracks,
                                 size_t track_count, int want_track,
                                 double *out, int out_n)
{
    if (uft_atx_write(path, tracks, track_count, false) != UFT_OK) return false;

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    if (uft_format_plugin_atx.open(&disk, path, true) != UFT_OK) return false;

    uft_track_t back;
    memset(&back, 0, sizeof(back));
    bool ok = (uft_format_plugin_atx.read_track(&disk, want_track, 0, &back)
               == UFT_OK)
              && (int)back.sector_count == out_n;
    if (ok) {
        for (int s = 0; s < out_n; s++) {
            if (!back.sectors[s].has_angular_position) { ok = false; break; }
            out[s] = back.sectors[s].angular_position;
        }
    }
    free_track(&back);
    uft_format_plugin_atx.close(&disk);
    return ok;
}

/* ────────────────────────────────────────────────────────────────────── */

TEST(a_track_without_positions_gets_the_interleaved_layout)
{
    /* Die Kernaussage. Ohne gemessene Positionen muss das Ergebnis das sein,
     * was uft_compute_interleave() rechnet — und ausdruecklich NICHT s/n. */
    char path[512];
    get_temp_path(path, sizeof(path), "sd");

    uft_track_t tr;
    build_bare_track(&tr, 0, SD_SPT, SEC);

    double got[SD_SPT];
    ASSERT(write_read_positions(path, &tr, 1, 0, got, SD_SPT));

    float want[SD_SPT];
    ASSERT(uft_compute_interleave(want, SD_SPT, SEC, false, 0, 0,
                                  UFT_INTERLEAVE_AUTO) == UFT_OK);

    int off_by_trivial = 0;
    for (int s = 0; s < SD_SPT; s++) {
        double d = got[s] - (double)want[s];
        if (d < 0) d = -d;
        if (d >= ATX_UNIT) {
            printf("\n        s=%2d got=%.5f want=%.5f\n", s, got[s],
                   (double)want[s]);
        }
        ASSERT(d < ATX_UNIT);

        /* Zaehlen, wie weit das Ergebnis vom trivialen Layout abweicht. */
        double triv = (double)s / (double)SD_SPT;
        double dt = got[s] - triv;
        if (dt < 0) dt = -dt;
        if (dt >= ATX_UNIT) off_by_trivial++;
    }

    /* Wenn die Verschraenkung wirkt, stimmt fast nichts mit s/n ueberein.
     * Nur der erste Platz faellt zusammen (beide bei 0). */
    if (off_by_trivial < SD_SPT - 2)
        printf("\n        nur %d von %d Positionen weichen von s/n ab\n",
               off_by_trivial, SD_SPT);
    ASSERT(off_by_trivial >= SD_SPT - 2);

    free_track(&tr);
    remove(path);
}

TEST(consecutive_tracks_are_offset_against_each_other)
{
    /* Der 8-%-Spurversatz. Zwei Spuren mit identischem Inhalt duerfen NICHT
     * dieselben Positionen bekommen — sonst fehlt die Kopf-Umsetzzeit, und
     * das Abbild behauptet eine Diskette, auf der alle Spuren gleichzeitig
     * am Index beginnen. */
    char path[512];
    get_temp_path(path, sizeof(path), "skew");

    uft_track_t tr[3];
    for (int c = 0; c < 3; c++) build_bare_track(&tr[c], c, SD_SPT, SEC);

    double p0[SD_SPT], p1[SD_SPT];
    ASSERT(write_read_positions(path, tr, 3, 0, p0, SD_SPT));
    ASSERT(write_read_positions(path, tr, 3, 1, p1, SD_SPT));

    /* Erster Sektor: Spur 1 liegt um ~0.08 spaeter als Spur 0. */
    double delta = p1[0] - p0[0];
    if (delta < 0) delta += 1.0;
    if (delta < 0.07 || delta > 0.09)
        printf("\n        Spurversatz %.4f statt ~0.08\n", delta);
    ASSERT(delta > 0.07 && delta < 0.09);

    for (int c = 0; c < 3; c++) free_track(&tr[c]);
    remove(path);
}

TEST(enhanced_density_gets_a_different_interleave_than_single)
{
    /* 26 Sektoren (ED) fuehren auf einen anderen Verschraenkungsfaktor als
     * 18 (SD). Waere die Rechnung nicht wirklich eingebunden, saehen beide
     * gleich aus — s/n haengt nur an n. */
    char path[512];
    get_temp_path(path, sizeof(path), "ed");

    uft_track_t tr;
    build_bare_track(&tr, 0, 26, SEC);

    double got[26];
    ASSERT(write_read_positions(path, &tr, 1, 0, got, 26));

    float want[26];
    ASSERT(uft_compute_interleave(want, 26, SEC, false, 0, 0,
                                  UFT_INTERLEAVE_AUTO) == UFT_OK);
    for (int s = 0; s < 26; s++) {
        double d = got[s] - (double)want[s];
        if (d < 0) d = -d;
        ASSERT(d < ATX_UNIT);
    }

    free_track(&tr);
    remove(path);
}

TEST(measured_positions_are_never_overwritten)
{
    /* Die Gegenrichtung, und die wichtigere: liegt eine gemessene Position
     * vor, darf die Rechnung sie nicht anfassen. Sonst haette MF-479 aus
     * einer ungenauen Angabe eine falsche gemacht. */
    char path[512];
    get_temp_path(path, sizeof(path), "keep");

    uft_track_t tr;
    build_bare_track(&tr, 0, SD_SPT, SEC);

    /* Nur die Sektoren 0 und 5 sind gemessen. */
    tr.sectors[0].angular_position = 0.013; tr.sectors[0].has_angular_position = true;
    tr.sectors[5].angular_position = 0.617; tr.sectors[5].has_angular_position = true;

    double got[SD_SPT];
    ASSERT(write_read_positions(path, &tr, 1, 0, got, SD_SPT));

    ASSERT(got[0] > 0.013 - ATX_UNIT && got[0] < 0.013 + ATX_UNIT);
    ASSERT(got[5] > 0.617 - ATX_UNIT && got[5] < 0.617 + ATX_UNIT);

    /* Die uebrigen kommen aus der Rechnung. */
    float want[SD_SPT];
    ASSERT(uft_compute_interleave(want, SD_SPT, SEC, false, 0, 0,
                                  UFT_INTERLEAVE_AUTO) == UFT_OK);
    for (int s = 1; s < SD_SPT; s++) {
        if (s == 5) continue;
        double d = got[s] - (double)want[s];
        if (d < 0) d = -d;
        ASSERT(d < ATX_UNIT);
    }

    free_track(&tr);
    remove(path);
}

TEST(phantom_sectors_keep_distinct_positions)
{
    /* Zwei Sektoren mit derselben Nummer ohne gemessene Position: sie
     * duerfen nicht auf denselben Platz fallen. Deshalb ist der Index in die
     * Verschraenkungstabelle die LISTENPOSITION und nicht die Sektornummer. */
    char path[512];
    get_temp_path(path, sizeof(path), "phantom");

    uft_track_t tr;
    memset(&tr, 0, sizeof(tr));
    uft_track_init(&tr, 0, 0);
    tr.encoding = UFT_ENCODING_FM;

    uint8_t a[SEC], b[SEC];
    memset(a, 0x11, sizeof(a));
    memset(b, 0x22, sizeof(b));
    uft_format_add_sector_with_id(&tr, 7, a, SEC, 0, 0);
    uft_format_add_sector_with_id(&tr, 7, b, SEC, 0, 0);
    uft_format_add_sector_with_id(&tr, 8, a, SEC, 0, 0);

    double got[3];
    ASSERT(write_read_positions(path, &tr, 1, 0, got, 3));

    double d01 = got[0] - got[1]; if (d01 < 0) d01 = -d01;
    ASSERT(d01 >= ATX_UNIT);

    free_track(&tr);
    remove(path);
}

int main(void)
{
    printf("=== ATX: Ersatz-Winkelpositionen sind das Atari-Layout (MF-479) ===\n");
    RUN(a_track_without_positions_gets_the_interleaved_layout);
    RUN(consecutive_tracks_are_offset_against_each_other);
    RUN(enhanced_density_gets_a_different_interleave_than_single);
    RUN(measured_positions_are_never_overwritten);
    RUN(phantom_sectors_keep_distinct_positions);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
