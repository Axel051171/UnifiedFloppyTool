/**
 * @file test_convert_cell_adjust.c
 * @brief Der Feineinsteller fuer die Zellendauer, in Prozent (MF-480).
 *
 * Punkt 2.1 der a8rawconv-Gap-Analyse. a8rawconv hat `-p` als Faktor
 * (50…200 %), und der Arbeitsablauf im Handbuch — in kleinen Schritten
 * verstellen, das Verhaeltnis guter zu schlechter Sektoren beobachten — ist
 * dort das zentrale Rettungswerkzeug fuer marginale Disketten.
 *
 * UFT hatte den Wert bereits im Decoder (`flux_decoder_options_t::
 * media_adjust_pct`, MF-471) — und **niemand setzte ihn**. Fuenfter Eintrag
 * derselben Liste nach MF-471/473/474/479: gebaut, geprueft, ohne Wirkung.
 *
 * ── Wann der Einsteller ueberhaupt etwas aendert ─────────────────────────
 *
 * Die Zellendauer wird aus der GEMESSENEN Umdrehung und der NOMINALEN
 * Zellenzahl des Medienprofils abgeleitet (MF-471/475). Stimmt die Messung,
 * aber wurde die Diskette mit einer anderen Datenrate beschrieben als das
 * Profil annimmt, ist die abgeleitete Dauer systematisch daneben. Dagegen
 * hilft kein besserer Decoder, sondern nur ein Wert, den der Aufrufer
 * verstellen kann.
 *
 * **Gemessen, nicht behauptet.** Ein Sweep ueber verschiedene Zellendauern
 * ergab: bis etwa ±30 % faengt die PLL den Versatz von selbst ab, der
 * Einsteller aendert dort NICHTS. Ein Test bei 2080 ns (4 % daneben) haette
 * also gruen geleuchtet, ohne irgendetwas ueber den Einsteller zu sagen.
 *
 * ── Warum der Aufbau so aussieht, wie er aussieht ────────────────────────
 *
 * Der erste Anlauf war eine Diskette mit 3000 ns je Zelle — und
 * **unphysikalisch**: bei 300 min^-1 dauert eine Umdrehung 200 ms, das sind
 * bei 3000 ns nur 66666 Zellen, und elf AmigaDOS-Sektoren brauchen 95392.
 * Eine solche Spur gibt es nicht; der Test lieferte folgerichtig 14 von 22
 * Sektoren, weil die Spuren abgeschnitten waren.
 *
 * Der stimmige Fall geht in die andere Richtung: eine Diskette, die
 * **dichter** beschrieben wurde, als das gewaehlte Profil annimmt — 1000 ns
 * je Zelle, also 200000 Zellen je Umdrehung. Elf Sektoren passen bequem, die
 * Umdrehung ist echte 200 ms, und die aus dem DD-Profil abgeleitete
 * Zellendauer (2000 ns) liegt um den Faktor zwei daneben. Genau der Fall
 * eines falsch gewaehlten Medienprofils — und genau wofuer es den Einsteller
 * gibt. 50 % holt die Diskette vollstaendig zurueck.
 *
 * ── Was dieser Test NICHT zeigt ──────────────────────────────────────────
 *
 * Der synthetische Flux ist jitterfrei. Die Wirkung an einer wirklich
 * marginalen Diskette — dem Fall aus a8rawconvs Handbuch, wo man in
 * Ein-Prozent-Schritten sucht — ist damit **nicht** belegt; dafuer braucht es
 * eine reale Aufnahme im Korpus. Belegt ist: der Wert erreicht den Decoder,
 * er wirkt in beide Richtungen, und ausserhalb 50…200 wird er hoerbar
 * abgelehnt.
 */

#include "uft/uft_format_convert.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/formats/uft_scp_writer.h"
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

/* Eine Diskette, die mit 1000 ns je Zelle beschrieben wurde — halb so viel,
 * wie das DD-Profil aus der gemessenen Umdrehung ableitet (2000 ns). Also
 * doppelt so dicht wie angenommen; der Einsteller muss 50 % sein. */
#define TRUE_CELL_NS   1000u
#define NUDGE_PCT      50.0
/* Zellen je Umdrehung bei dieser Zellendauer. */
#define CELLS_PER_REV  (UFT_AMIGADOS_REV_NS / TRUE_CELL_NS)
#define NTRACKS        2
#define ALL_SECTORS    (NTRACKS * UFT_AMIGADOS_SPT)
#define MAX_INTERVALS  CELLS_PER_REV

static void get_temp_path(char *path, size_t size, const char *tag)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_cadj_%s_%d", dir, tag, rand() % 100000);
}

/**
 * Eine SCP-Datei schreiben, deren Spuren mit @p cell_ns je Zelle
 * aufgezeichnet sind — die Umdrehungsdauer bleibt bei 200 ms, weil der
 * Index nicht davon abhaengt, wie dicht geschrieben wurde.
 */
static int write_scp(const char *path, const uint8_t *adf, unsigned cell_ns)
{
    size_t cells = UFT_AMIGADOS_REV_NS / cell_ns;
    uint8_t  *bits = (uint8_t *)calloc((cells + 7) / 8 + 1, 1);
    uint32_t *iv   = (uint32_t *)malloc(cells * sizeof(uint32_t));
    if (!bits || !iv) { free(bits); free(iv); return -1; }

    scp_writer_t *w = scp_writer_create(SCP_TYPE_AMIGA, 1);
    if (!w) { free(bits); free(iv); return -1; }

    int rc = 0;
    for (int track = 0; track < NTRACKS && rc == 0; track++) {
        uft_amigados_cells_t c = { bits, cells, 0, 0 };
        uft_amigados_build_track(&c, adf, (uint8_t)track, NULL);
        size_t n = uft_amigados_cells_to_intervals(&c, cell_ns, iv, cells);
        rc = scp_writer_add_track(w, track / 2, track % 2, iv, n,
                                  UFT_AMIGADOS_REV_NS, 0);
    }
    if (rc == 0) rc = scp_writer_save(w, path);

    scp_writer_free(w);
    free(bits);
    free(iv);
    return rc;
}

static uint8_t *make_source_adf(void)
{
    uint8_t *adf = (uint8_t *)calloc(1, UFT_AMIGADOS_ADF_SIZE);
    if (!adf) return NULL;
    uft_amigados_fill_pattern(adf, (size_t)ALL_SECTORS * UFT_AMIGADOS_SECSZ);
    return adf;
}

/** SCP -> ADF mit gegebenem Feineinsteller. 0 heisst „nicht gesetzt". */
static uft_error_t convert(const char *scp, const char *out, double pct,
                           uft_convert_result_t *res)
{
    uft_convert_options_t o = uft_convert_default_options();
    o.accept_data_loss = true;          /* Flux -> Sektor ist verlustbehaftet */
    o.decode_cell_adjust_pct = pct;
    memset(res, 0, sizeof(*res));
    return uft_convert_file(scp, out, UFT_FORMAT_ADF, &o, res);
}

static bool warned_about(const uft_convert_result_t *r, const char *needle)
{
    for (int i = 0; i < r->warning_count; i++)
        if (strstr(r->warnings[i], needle)) return true;
    return false;
}

/* ────────────────────────────────────────────────────────────────────── */

TEST(a_stream_far_off_the_profile_needs_no_nudge_any_more)
{
    /* Diese Pruefung stand bis MF-492 andersherum da: die abgeleitete Dauer
     * liegt um den Faktor zwei daneben, die PLL rastet nicht ein, es kommt
     * KEIN Sektor heraus — das war die Ausgangslage, die den Feineinsteller
     * ueberhaupt noetig machte.
     *
     * Seit MF-492 misst der automatische Pfad die Zellendauer an den
     * Sync-Marken selbst und holt die Diskette ohne Zutun. Die alte
     * Erwartung war kein Vertrag, sondern eine PROTOKOLLIERTE SCHWAECHE;
     * sie hat sich erledigt, und der Test sagt jetzt, was gilt.
     *
     * Der Feineinsteller ist damit nicht ueberfluessig: er wirkt weiterhin
     * in allen Decodern und hat weiterhin Vorrang, wenn er gesetzt ist —
     * das pruefen die beiden folgenden Tests. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "off.scp");
    get_temp_path(out, sizeof(out), "off.adf");
    ASSERT(write_scp(scp, adf, TRUE_CELL_NS) == 0);

    uft_convert_result_t r;
    ASSERT(convert(scp, out, 0.0, &r) == UFT_OK);
    if (r.sectors_converted != ALL_SECTORS)
        printf("\n        %d von %d Sektoren ohne Einsteller\n",
               r.sectors_converted, ALL_SECTORS);
    ASSERT(r.sectors_converted == ALL_SECTORS);

    free(adf); remove(scp); remove(out);
}

TEST(the_nudge_recovers_the_whole_disk)
{
    /* Dieselbe Datei, ein Prozentwert — und die Diskette ist vollstaendig
     * da. Das ist die Verdrahtung: der Wert erreicht den Decoder und
     * bestimmt seine Startperiode. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "fix.scp");
    get_temp_path(out, sizeof(out), "fix.adf");
    ASSERT(write_scp(scp, adf, TRUE_CELL_NS) == 0);

    uft_convert_result_t r;
    ASSERT(convert(scp, out, NUDGE_PCT, &r) == UFT_OK);
    if (r.sectors_converted != ALL_SECTORS)
        printf("\n        %d von %d Sektoren bei %.0f%%\n",
               r.sectors_converted, ALL_SECTORS, NUDGE_PCT);
    ASSERT(r.sectors_converted == ALL_SECTORS);

    /* Und der Inhalt stimmt, nicht nur die Zahl. */
    FILE *f = fopen(out, "rb");
    ASSERT(f != NULL);
    uint8_t *got = (uint8_t *)malloc(UFT_AMIGADOS_ADF_SIZE);
    ASSERT(got != NULL);
    size_t n = fread(got, 1, UFT_AMIGADOS_ADF_SIZE, f);
    fclose(f);
    ASSERT(n == UFT_AMIGADOS_ADF_SIZE);
    ASSERT(memcmp(got, adf, (size_t)ALL_SECTORS * UFT_AMIGADOS_SECSZ) == 0);

    free(got); free(adf); remove(scp); remove(out);
}

TEST(a_wrong_nudge_makes_a_good_stream_unreadable)
{
    /* Die Gegenrichtung, und der Grund, warum das kein Standardwert ist:
     * derselbe Einsteller auf einer gesunden Diskette macht sie unlesbar.
     * Ein Rettungswerkzeug, kein Verbesserer. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "good.scp");
    get_temp_path(out, sizeof(out), "good.adf");
    ASSERT(write_scp(scp, adf, UFT_AMIGADOS_CELL_NS) == 0);   /* 2000 ns */

    uft_convert_result_t ok, bad;
    ASSERT(convert(scp, out, 0.0, &ok) == UFT_OK);
    ASSERT(ok.sectors_converted == ALL_SECTORS);

    ASSERT(convert(scp, out, NUDGE_PCT, &bad) == UFT_OK);
    ASSERT(bad.sectors_converted == 0);

    free(adf); remove(scp); remove(out);
}

TEST(an_out_of_range_nudge_is_refused_out_loud)
{
    /* Ausserhalb 50…200 lehnt das Medienprofil den Wert ab und der Decoder
     * faellt still auf 100 zurueck. Still ist falsch: wer einen Wert
     * vorgibt, muss erfahren, dass er nicht benutzt wurde — sonst dreht er
     * an einem Knopf, der nicht verbunden ist. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "range.scp");
    get_temp_path(out, sizeof(out), "range.adf");
    ASSERT(write_scp(scp, adf, UFT_AMIGADOS_CELL_NS) == 0);

    uft_convert_result_t r;
    ASSERT(convert(scp, out, 400.0, &r) == UFT_OK);
    ASSERT(warned_about(&r, "ausserhalb"));

    /* Und er verhaelt sich wie „nicht gesetzt", nicht wie 400. */
    ASSERT(r.sectors_converted == ALL_SECTORS);

    free(adf); remove(scp); remove(out);
}

/* ------------------------------------------------------------------ */
/* Die Messung erreicht den Menschen (MF-496)                          */
/* ------------------------------------------------------------------ */

TEST(the_measured_cell_time_is_reported_as_a_nudge_suggestion)
{
    /* Der Punkt dieses Bausteins. Der Decoder MISST die Zellendauer seit
     * MF-492 und benutzt sie auch — aber der Mensch am Feineinsteller sah
     * davon nichts und musste raten, obwohl das Werkzeug die Antwort schon
     * hatte.
     *
     * Die Diskette hier ist mit 1000 ns je Zelle beschrieben, das Profil
     * leitet 2000 ns ab. Die richtige Antwort ist also 50 % — und genau
     * die muss in der Meldung stehen, nicht bloss „irgendwas stimmt
     * nicht". */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "rep.scp");
    get_temp_path(out, sizeof(out), "rep.adf");
    ASSERT(write_scp(scp, adf, TRUE_CELL_NS) == 0);

    uft_convert_result_t r;
    ASSERT(convert(scp, out, 0.0, &r) == UFT_OK);

    if (!warned_about(&r, "Feineinsteller")) {
        printf("\n        keine Meldung zur Zellendauer; %d Warnungen:\n",
               r.warning_count);
        for (int i = 0; i < r.warning_count && i < 8; i++)
            printf("          %s\n", r.warnings[i]);
    }
    ASSERT(warned_about(&r, "Feineinsteller"));

    /* Und die Zahl muss stimmen. 50 % ist der Wert, den der naechste Test
     * unabhaengig davon als richtig belegt — eine Meldung mit falscher Zahl
     * waere schlimmer als keine. */
    ASSERT(warned_about(&r, "50 %"));

    free(adf); remove(scp); remove(out);
}

TEST(a_disk_that_matches_its_profile_gets_no_such_advice)
{
    /* Gegenprobe, ohne die die vorige nichts wert waere: eine Diskette, die
     * mit der abgeleiteten Zellendauer beschrieben wurde, darf KEINEN
     * Vorschlag bekommen. Ein Werkzeug, das immer etwas zu meckern hat,
     * wird ignoriert. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "match.scp");
    get_temp_path(out, sizeof(out), "match.adf");
    /* 2000 ns = genau das, was das DD-Profil aus 200 ms ableitet. */
    ASSERT(write_scp(scp, adf, 2000) == 0);

    uft_convert_result_t r;
    ASSERT(convert(scp, out, 0.0, &r) == UFT_OK);
    if (warned_about(&r, "Feineinsteller"))
        printf("\n        unnoetiger Vorschlag bei passender Diskette\n");
    ASSERT(!warned_about(&r, "Feineinsteller"));
    ASSERT(!warned_about(&r, "Gleichlauffehler"));

    free(adf); remove(scp); remove(out);
}

int main(void)
{
    printf("=== Zellendauer-Feineinsteller in Prozent (MF-480) ===\n");

    if (uft_register_all_formats() != UFT_OK) {
        printf("FAIL: uft_register_all_formats()\n");
        return 1;
    }

    RUN(a_stream_far_off_the_profile_needs_no_nudge_any_more);
    RUN(the_nudge_recovers_the_whole_disk);
    RUN(a_wrong_nudge_makes_a_good_stream_unreadable);
    RUN(an_out_of_range_nudge_is_refused_out_loud);

    /* MF-496: die Messung erreicht den Menschen */
    RUN(the_measured_cell_time_is_reported_as_a_nudge_suggestion);
    RUN(a_disk_that_matches_its_profile_gets_no_such_advice);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
