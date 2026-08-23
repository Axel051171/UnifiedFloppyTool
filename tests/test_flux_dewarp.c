/**
 * @file test_flux_dewarp.c
 * @brief Gleichlauffehler herausrechnen (MF-495).
 *
 * Baustein 2.2 des Mammut-Plans.
 *
 * ── Der Fall, den kein Taktwert loest ────────────────────────────────────
 *
 * Bis hierher hat jede Verbesserung am Lesepfad EINEN besseren Taktwert
 * gesucht: aus dem Medienprofil (MF-471), aus dem Histogramm (MF-488), aus
 * den Sync-Marken (MF-492). Wo die Diskette aber nicht ueberall gleich
 * schnell lief, gibt es keinen richtigen einzelnen Wert — jeder ist fuer
 * einen Teil der Spur falsch. Gemessen an einer Spur, deren erste 35 % um
 * 70 % gedehnt sind: 8 gefundene Sektoren, davon 7 heil.
 *
 * ── Was hier geprueft wird ───────────────────────────────────────────────
 *
 * Vier Dinge, und das vierte ist das wichtigste:
 *
 * 1. Der Entzerrer MISST den Verlauf, statt ihn zu glaetten: eine
 *    gleichmaessige Spur bekommt Spanne 1,0 und bleibt unveraendert.
 * 2. Auf einer Spur mit zwei Geschwindigkeiten holt er heile Sektoren
 *    dazu — nicht bloss gefundene.
 * 3. Er kostet nichts: wo er nicht hilft, gewinnt der unentzerrte
 *    Durchlauf, weil beide gegeneinander antreten.
 * 4. Er erzeugt **nie** einen Sektor mit heiler Pruefsumme und falschem
 *    Inhalt. Eine Stufe, die die Zeitachse veraendert, muss das beweisen,
 *    sonst hat sie im forensischen Lesepfad nichts zu suchen.
 *
 * ── Was er NICHT ist ─────────────────────────────────────────────────────
 *
 * Kein Mittel gegen Zittern. Gemessen ueber je 20 Spuren mit 0…12 %
 * Zittern (1320 Dekodierungen): **ein** heiler Sektor mehr, insgesamt, bei
 * 4 % — und keine einzige Spur schlechter. Das ist Rauschen, kein Gewinn.
 * Dewarp hilft gegen GESCHWINDIGKEITSSCHWANKUNG; wer ihn gegen Zittern
 * einsetzt, bekommt Rechenzeit statt Sektoren.
 *
 * ── Der Startwert entscheidet mit ────────────────────────────────────────
 *
 * Auf der Zwei-Tempo-Spur mit dem gemessenen Startwert (2040 ns aus der
 * Sync-Suche, MF-492): 9 heile Sektoren. Mit 1200 ns — auch ein
 * plausibler Wert, naemlich die Rate der Mehrheit der Abstaende: 3. Der
 * Entzerrer ist also kein Ersatz fuer eine Messung, sondern ihr Abnehmer.
 */

#include "uft/uft_types.h"
#include "uft/flux/uft_dewarp.h"
#include "uft/flux/uft_flux_decoder.h"
#include "flux_gen.h"                 /* tests/flux_gen/amigados */

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-56s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                    _fail++; return; } } while (0)

#define MAXCELLS 200000u

static uint8_t *g_adf;

static uint8_t *make_source_adf(void)
{
    uint8_t *adf = (uint8_t *)calloc(1, UFT_AMIGADOS_ADF_SIZE);
    if (!adf) return NULL;
    uft_amigados_fill_pattern(adf, (size_t)UFT_AMIGADOS_SPT
                                   * UFT_AMIGADOS_SECSZ);
    return adf;
}

static size_t build_intervals(const uint8_t *adf, unsigned cell_ns,
                              double jitter_pct, uint64_t seed,
                              uint32_t *out, size_t cap)
{
    size_t cells = (size_t)(UFT_AMIGADOS_REV_NS / cell_ns);
    if (cells > MAXCELLS) cells = MAXCELLS;
    uint8_t *bits = (uint8_t *)calloc(cells / 8 + 8, 1);
    if (!bits) return 0;
    uft_amigados_cells_t c = { bits, cells, 0, 0 };
    uft_amigados_build_track(&c, adf, 0, NULL);
    size_t n = uft_amigados_cells_to_intervals_jitter(&c, cell_ns, jitter_pct,
                                                      seed, out, cap);
    free(bits);
    return n;
}

/**
 * Dekodieren und gegen die QUELLE vergleichen.
 *
 * @param found  gefundene Sektoren
 * @param whole  davon mit heilen Pruefsummen
 * @param wrong  davon mit heiler Pruefsumme und FALSCHEM Inhalt — die Zahl,
 *               die immer 0 sein muss (MF-466: Uebereinstimmung ist keine
 *               Verifikation)
 */
static void decode_truth(const uint32_t *iv, size_t n, uint32_t bitcell_ns,
                         size_t *found, size_t *whole, size_t *wrong)
{
    *found = *whole = *wrong = 0;
    flux_raw_data_t raw;
    memset(&raw, 0, sizeof(raw));
    if (flux_raw_from_ns_intervals(iv, n, &raw) != FLUX_OK) return;

    flux_decoder_options_t o;
    flux_decoder_options_init(&o);
    o.encoding   = FLUX_ENC_AMIGA;
    o.bitcell_ns = bitcell_ns;          /* 0 = automatisch, siehe MF-492 */
    o.use_pll    = true;

    flux_decoded_track_t dt;
    flux_decoded_track_init(&dt);
    flux_decode_amiga(&raw, &dt, &o);

    for (size_t i = 0; i < dt.sector_count; i++) {
        const flux_decoded_sector_t *s = &dt.sectors[i];
        (*found)++;
        if (s->id_crc_ok && s->data_crc_ok) (*whole)++;
        if (s->data_crc_ok && s->data && s->data_size == UFT_AMIGADOS_SECSZ &&
            s->sector < UFT_AMIGADOS_SPT &&
            memcmp(s->data, g_adf + (size_t)s->sector * UFT_AMIGADOS_SECSZ,
                   UFT_AMIGADOS_SECSZ) != 0)
            (*wrong)++;
    }
    flux_decoded_track_free(&dt);
    flux_raw_free(&raw);
}

/* ────────────────────────────────────────────────────────────────────── */

TEST(a_steady_track_is_reported_steady_and_left_alone)
{
    /* Der Wert, den der Mensch zu sehen bekommt („das Medium eiert um X %"),
     * muss auf einer gleichmaessigen Spur 0 sagen. Ein Schaetzer, der auch
     * dort etwas findet, erfindet einen Befund — und die Stufe liefe dann
     * auf jeder gesunden Spur mit. */
    uint32_t *iv = (uint32_t *)malloc(MAXCELLS * sizeof(uint32_t));
    uint32_t *out = (uint32_t *)malloc(MAXCELLS * sizeof(uint32_t));
    ASSERT(iv != NULL && out != NULL);

    size_t n = build_intervals(g_adf, 1200, 0.0, 1, iv, MAXCELLS);
    ASSERT(n > 1000);

    uft_dewarp_result_t r;
    ASSERT(uft_dewarp_intervals(iv, n, 1200.0, 0.0, out, &r));
    if (r.warp_span > 1.01)
        printf("\n        Spanne %.4f auf gleichmaessiger Spur\n", r.warp_span);
    ASSERT(r.warp_span <= 1.01);
    ASSERT(r.ref_clock > 1180.0 && r.ref_clock < 1220.0);

    /* Und der Strom kommt praktisch unveraendert heraus. */
    size_t moved = 0;
    for (size_t i = 0; i < n; i++) {
        long d = (long)out[i] - (long)iv[i];
        if (d < 0) d = -d;
        if (d > 12) moved++;             /* 1 % von 1200 ns */
    }
    if (moved > n / 100)
        printf("\n        %zu von %zu Abstaenden verschoben\n", moved, n);
    ASSERT(moved <= n / 100);

    free(out); free(iv);
}

TEST(it_finds_the_speed_change_that_is_really_there)
{
    /* Gegenprobe: dieselbe Spur, erste 35 % um 70 % gedehnt. Jetzt MUSS
     * der Schaetzer anschlagen — und zwar in der Groessenordnung, die
     * eingebaut wurde, nicht irgendwie. */
    uint32_t *iv = (uint32_t *)malloc(MAXCELLS * sizeof(uint32_t));
    uint32_t *out = (uint32_t *)malloc(MAXCELLS * sizeof(uint32_t));
    ASSERT(iv != NULL && out != NULL);

    size_t n = build_intervals(g_adf, 1200, 0.0, 5, iv, MAXCELLS);
    ASSERT(n > 5000);
    for (size_t i = 0; i < (n * 35) / 100; i++)
        iv[i] = (uint32_t)(iv[i] * 1.7);

    uft_dewarp_result_t r;
    ASSERT(uft_dewarp_intervals(iv, n, 1200.0, 0.0, out, &r));
    if (r.warp_span < 1.10)
        printf("\n        Spanne nur %.3f\n", r.warp_span);
    ASSERT(r.warp_span >= 1.10);

    /* Nach der Entzerrung ist der Strom gleichmaessig: ein zweiter Durchlauf
     * darueber findet nichts mehr. Das ist die eigentliche Aussage — die
     * Kurve hat den Fehler nicht nur gesehen, sondern entfernt. */
    uft_dewarp_result_t r2;
    ASSERT(uft_dewarp_intervals(out, n, r.ref_clock, 0.0, out, &r2));

    /* Verglichen wird der UEBERSCHUSS ueber 1, nicht die Spanne selbst:
     * eine Spanne ist ein Verhaeltnis und nie kleiner als 1, „auf 60 %
     * gefallen" waere also eine unmoegliche Forderung. */
    const double before = r.warp_span - 1.0;
    const double after  = r2.warp_span - 1.0;
    if (after >= before * 0.5)
        printf("\n        Rest %.3f von %.3f\n", after, before);
    ASSERT(before > 0.10);
    ASSERT(after < before * 0.5);

    free(out); free(iv);
}

TEST(a_track_of_two_speeds_gains_whole_sectors_not_just_finds)
{
    /* Der Zweck der ganzen Stufe, und zwar in heilen Sektoren gemessen.
     * Bis MF-494 wurde an dieser Stelle „gefunden" gezaehlt und „gerettet"
     * geschrieben; hier wird beides getrennt gezaehlt. */
    uint32_t *iv = (uint32_t *)malloc(MAXCELLS * sizeof(uint32_t));
    ASSERT(iv != NULL);

    size_t n = build_intervals(g_adf, 1200, 0.0, 5, iv, MAXCELLS);
    ASSERT(n > 5000);
    for (size_t i = 0; i < (n * 35) / 100; i++)
        iv[i] = (uint32_t)(iv[i] * 1.7);

    /* Ohne Entzerrung: 2000 ns vorgeben sperrt beide Rettungsstufen
     * (MF-471), das ist der ehrliche Vorher-Wert. */
    size_t f0, w0, x0, f1, w1, x1;
    decode_truth(iv, n, 2000, &f0, &w0, &x0);
    decode_truth(iv, n, 0,    &f1, &w1, &x1);

    printf("\n        ohne %zu gef/%zu heil -> mit %zu gef/%zu heil\n",
           f0, w0, f1, w1);
    ASSERT(w0 >= 5);              /* die Ausgangslage ist nicht leer */
    ASSERT(w1 > w0);              /* HEILE Sektoren dazugewonnen */
    ASSERT(x0 == 0 && x1 == 0);   /* und kein einziger still falsch */

    free(iv);
}

TEST(a_ramp_is_not_made_worse_by_the_extra_stages)
{
    /* Die Ueberanpassungs-Falle, an dem Fall geprueft, der sie ausloest:
     * eine gleichmaessig beschleunigende Spur mit Zittern. Der WEGWERF-
     * Prototyp verlor hier einen Sektor (11 -> 10). Die Fassung im Baum tut
     * das nicht mehr — die Ausreisser-Abweisung und der gemessene Startwert
     * haben den Rueckschritt beseitigt.
     *
     * Verlangt wird deshalb nicht, dass die Entzerrung hilft, sondern dass
     * sie nichts kostet. Das ist der Vertrag, den eine Zusatzstufe im
     * Lesepfad einhalten muss. */
    uint32_t *iv = (uint32_t *)malloc(MAXCELLS * sizeof(uint32_t));
    ASSERT(iv != NULL);

    size_t n = build_intervals(g_adf, 1200, 6.0, 3, iv, MAXCELLS);
    ASSERT(n > 5000);
    for (size_t i = 0; i < n; i++)
        iv[i] = (uint32_t)(iv[i] * (1.0 + 0.25 * (double)i / (double)n));

    size_t f0, w0, x0, f1, w1, x1;
    decode_truth(iv, n, 1200, &f0, &w0, &x0);   /* Vorgabe sperrt die Stufen */
    decode_truth(iv, n, 0,    &f1, &w1, &x1);   /* automatisch, mit Stufen */

    if (w1 < w0)
        printf("\n        automatisch %zu heil, mit fester Vorgabe %zu\n",
               w1, w0);
    ASSERT(w0 >= 8);
    ASSERT(w1 >= w0);             /* nichts verloren */
    ASSERT(x0 == 0 && x1 == 0);

    free(iv);
}

TEST(an_overflow_placeholder_survives_as_a_placeholder)
{
    /* Eine 0 im Strom ist eine SCP-Ueberlaufmarke, kein Flusswechsel
     * (MF-438). Wer sie skaliert, macht aus einer Fehlstelle eine Messung. */
    uint32_t in[8]  = { 4000, 0, 6000, 4000, 8000, 4000, 6000, 4000 };
    uint32_t out[8];
    uft_dewarp_result_t r;
    ASSERT(uft_dewarp_intervals(in, 8, 2000.0, 0.0, out, &r));
    ASSERT(out[1] == 0);
    free(NULL);
}

TEST(bad_arguments_are_refused_not_guessed)
{
    uint32_t in[4] = { 4000, 6000, 4000, 8000 };
    uint32_t out[4];
    uft_dewarp_result_t r;
    ASSERT(uft_dewarp_intervals(NULL, 4, 2000.0, 0.0, out, &r) == false);
    ASSERT(uft_dewarp_intervals(in, 4, 2000.0, 0.0, NULL, &r) == false);
    ASSERT(uft_dewarp_intervals(in, 1, 2000.0, 0.0, out, &r) == false);
    ASSERT(uft_dewarp_intervals(in, 4, 0.0, 0.0, out, &r) == false);
    ASSERT(uft_dewarp_intervals(in, 4, -5.0, 0.0, out, &r) == false);
    /* Ein Strom ohne jeden brauchbaren Abstand ergibt keine Kurve. */
    uint32_t junk[4] = { 1, 1, 1, 1 };
    ASSERT(uft_dewarp_intervals(junk, 4, 2000.0, 0.0, out, &r) == false);
}

TEST(it_works_in_place)
{
    /* Der Decoder ruft es mit demselben Puffer fuer Ein- und Ausgabe auf —
     * das muss halten, sonst waere die Verdrahtung ein Speicherfehler. */
    uint32_t *a = (uint32_t *)malloc(MAXCELLS * sizeof(uint32_t));
    uint32_t *b = (uint32_t *)malloc(MAXCELLS * sizeof(uint32_t));
    ASSERT(a != NULL && b != NULL);

    size_t n = build_intervals(g_adf, 1200, 0.0, 5, a, MAXCELLS);
    ASSERT(n > 5000);
    for (size_t i = 0; i < (n * 35) / 100; i++) a[i] = (uint32_t)(a[i] * 1.7);
    memcpy(b, a, n * sizeof(uint32_t));

    uft_dewarp_result_t ra, rb;
    ASSERT(uft_dewarp_intervals(a, n, 1200.0, 0.0, b, &rb));   /* getrennt */
    ASSERT(uft_dewarp_intervals(a, n, 1200.0, 0.0, a, &ra));   /* am Ort */
    ASSERT(memcmp(a, b, n * sizeof(uint32_t)) == 0);
    ASSERT(ra.warp_span == rb.warp_span);

    free(b); free(a);
}

int main(void)
{
    printf("=== Gleichlauffehler herausrechnen (MF-495) ===\n");
    g_adf = make_source_adf();
    if (!g_adf) { printf("kein Speicher\n"); return 1; }

    RUN(a_steady_track_is_reported_steady_and_left_alone);
    RUN(it_finds_the_speed_change_that_is_really_there);
    RUN(a_track_of_two_speeds_gains_whole_sectors_not_just_finds);
    RUN(a_ramp_is_not_made_worse_by_the_extra_stages);
    RUN(an_overflow_placeholder_survives_as_a_placeholder);
    RUN(bad_arguments_are_refused_not_guessed);
    RUN(it_works_in_place);

    free(g_adf);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
