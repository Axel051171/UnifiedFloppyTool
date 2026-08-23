/**
 * @file test_flux_sync_search.c
 * @brief Sync-Marken ohne PLL finden (MF-492).
 *
 * Baustein 2.1 des Mammut-Plans.
 *
 * ── Die Luecke, gemessen ─────────────────────────────────────────────────
 *
 * Die uebliche Kette ist Flux -> PLL -> Bitstrom -> Sync-Suche. Faellt die
 * PLL aus dem Takt, gibt es keinen Bitstrom, und damit findet niemand mehr
 * eine Marke. Bei einer angenommenen Zellendauer von 0,85 der wahren und
 * 4 % Zittern lieferte `flux_decode_amiga()` **0 von 11** Sektoren — obwohl
 * alle 11 Marken unveraendert im Strom stehen (FLUX-11, MF-487).
 *
 * ── Warum die Suche das kann ─────────────────────────────────────────────
 *
 * Die Marke 0x4489 0x4489 hat die Zellabstaende 4 3 4 3 2 4 3 4 3. Gesucht
 * wird nicht nach Zeiten, sondern nach einer Stelle, an der neun Abstaende
 * zu EINEM gemeinsamen Takt passen — und dieser Takt ist unbekannt und
 * faellt aus den Daten. Deshalb ueberlebt die Suche jede Zellendauer, jede
 * Drehzahl und jede Drift.
 *
 * ── Was hier geprueft wird ───────────────────────────────────────────────
 *
 * Nicht, dass die Suche „funktioniert", sondern fuenf nachpruefbare Dinge:
 * dass sie auf einer sauberen Spur GENAU das findet, was der belegte
 * Dekoder findet; dass sie den Takt MISST statt ihn zu raten; dass beide
 * Darstellungen des Stroms dasselbe sagen; dass sie schweigt, wo nichts
 * ist; und dass der Fund den Dekoder auch erreicht — sonst waere es das
 * naechste Stueck Code, das gebaut, getestet und nie aufgerufen wird.
 */

#include "uft/uft_types.h"
#include "uft/flux/uft_flux_sync_search.h"
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
#define MAXHITS  64

static uint8_t *make_source_adf(void)
{
    uint8_t *adf = (uint8_t *)calloc(1, UFT_AMIGADOS_ADF_SIZE);
    if (!adf) return NULL;
    uft_amigados_fill_pattern(adf, (size_t)UFT_AMIGADOS_SPT
                                   * UFT_AMIGADOS_SECSZ);
    return adf;
}

/**
 * Eine AmigaDOS-Spur als ns-Intervalle.
 *
 * Die Zellenzahl je Umdrehung haengt an der Zellendauer, die Umdrehungsdauer
 * nicht — deshalb wird die Kapazitaet hier gerechnet und nicht angenommen.
 * Eine volle Spur braucht rund 95 400 Zellen; oberhalb von etwa 2100 ns je
 * Zelle passen die 11 Sektoren nicht mehr in eine Umdrehung.
 */
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
 * Wie viele Sektoren liefert der Dekoder?
 *
 * @param bitcell_ns 0 = AUTOMATISCH. Nur dann laeuft der zweite Durchlauf:
 *        eine ausdrueckliche Vorgabe hat Vorrang (MF-471), und eine stille
 *        Korrektur waere genau das, was dieses Werkzeug nicht tut. Ein
 *        Wert != 0 dient hier deshalb als VERGLEICHSMASSSTAB, nicht als
 *        Weg durch die Rettung.
 */
static size_t decode_count(const uint32_t *iv, size_t n, uint32_t bitcell_ns)
{
    flux_raw_data_t raw;
    memset(&raw, 0, sizeof(raw));
    if (flux_raw_from_ns_intervals(iv, n, &raw) != FLUX_OK) return (size_t)-1;

    flux_decoder_options_t o;
    flux_decoder_options_init(&o);
    o.encoding   = FLUX_ENC_AMIGA;
    o.bitcell_ns = bitcell_ns;
    o.use_pll    = true;

    flux_decoded_track_t dt;
    flux_decoded_track_init(&dt);
    flux_decode_amiga(&raw, &dt, &o);
    size_t n_sec = dt.sector_count;
    flux_decoded_track_free(&dt);
    flux_raw_free(&raw);
    return n_sec;
}

static bool amiga_pattern(uft_sync_pattern_t *pat)
{
    const uint16_t pair[2] = { 0x4489, 0x4489 };
    return uft_sync_pattern_from_words(pair, 2, pat);
}

/* ────────────────────────────────────────────────────────────────────── */

TEST(the_amiga_sync_has_the_shape_the_stream_really_shows)
{
    /* Kein abgeleiteter, sondern ein GEMESSENER Wert: derselbe Zellenstrom,
     * den der Generator schreibt, traegt innerhalb der Marke die Abstaende
     * 4 3 4 3 2 4 3 4 3. Steht die Erwartung woanders als in der Messung,
     * faellt das hier auf und nicht erst an der Diskette. */
    uft_sync_pattern_t pat;
    ASSERT(amiga_pattern(&pat));

    static const uint8_t want_k[9] = { 4, 3, 4, 3, 2, 4, 3, 4, 3 };
    ASSERT(pat.k_count == 9);
    for (size_t i = 0; i < 9; i++) {
        if (pat.k[i] != want_k[i])
            printf("\n        k[%zu] = %u statt %u\n", i, pat.k[i], want_k[i]);
        ASSERT(pat.k[i] == want_k[i]);
    }
}

TEST(a_pattern_needs_more_than_a_point_to_have_a_shape)
{
    /* Zwei Abstaende passen auf zu viele Stellen. Die Funktion muss das
     * ablehnen, statt eine Suche zu erlauben, die alles findet. */
    uft_sync_pattern_t pat;
    const uint16_t one = 0x4489;
    ASSERT(uft_sync_pattern_from_words(&one, 1, &pat) == true);   /* 4 Abst. */
    ASSERT(pat.k_count == 4);

    const uint16_t sparse = 0x8001;      /* nur zwei gesetzte Zellen */
    ASSERT(uft_sync_pattern_from_words(&sparse, 1, &pat) == false);
    ASSERT(uft_sync_pattern_from_words(NULL, 2, &pat) == false);
    ASSERT(uft_sync_pattern_from_words(&one, 0, &pat) == false);
}

TEST(on_a_clean_track_it_finds_exactly_what_the_decoder_finds)
{
    /* Der Massstab ist nicht meine Erwartung, sondern der gegen eine echte
     * Aufnahme belegte Dekoder (MF-438). Findet die Suche etwas anderes,
     * ist sie falsch — nicht er. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);
    uint32_t *iv = (uint32_t *)malloc(MAXCELLS * sizeof(uint32_t));
    ASSERT(iv != NULL);

    size_t n = build_intervals(adf, 2000, 0.0, 1, iv, MAXCELLS);
    ASSERT(n > 1000);

    uft_sync_pattern_t pat;
    ASSERT(amiga_pattern(&pat));

    uft_sync_hit_t hits[MAXHITS];
    size_t h = uft_sync_search_intervals(iv, n, &pat, 0.0, hits, MAXHITS);
    size_t sec = decode_count(iv, n, 2000);

    if (h != sec)
        printf("\n        Suche %zu Marken, Dekoder %zu Sektoren\n", h, sec);
    ASSERT(sec == UFT_AMIGADOS_SPT);
    ASSERT(h == sec);

    free(iv); free(adf);
}

TEST(it_measures_the_cell_time_it_does_not_assume_one)
{
    /* Drei verschiedene Zellendauern, dreimal derselbe Aufruf ohne jede
     * Vorgabe. Ein Schaetzer, der immer 2000 sagt, faellt hier durch. */
    static const unsigned cells[] = { 1200, 1800, 2000 };
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);
    uint32_t *iv = (uint32_t *)malloc(MAXCELLS * sizeof(uint32_t));
    ASSERT(iv != NULL);

    uft_sync_pattern_t pat;
    ASSERT(amiga_pattern(&pat));

    for (size_t i = 0; i < sizeof(cells) / sizeof(cells[0]); i++) {
        size_t n = build_intervals(adf, cells[i], 0.0, 1, iv, MAXCELLS);
        ASSERT(n > 1000);

        uft_sync_hit_t hits[MAXHITS];
        size_t h = uft_sync_search_intervals(iv, n, &pat, 0.0, hits, MAXHITS);
        ASSERT(h >= 3);

        for (size_t j = 0; j < h; j++) {
            double rel = hits[j].local_clock / (double)cells[i];
            if (rel < 0.99 || rel > 1.01)
                printf("\n        %u ns -> %.1f ns gemessen\n",
                       cells[i], hits[j].local_clock);
            ASSERT(rel > 0.99 && rel < 1.01);
        }
    }
    free(iv); free(adf);
}

TEST(both_representations_of_the_same_stream_agree)
{
    /* `flux_raw_data_t` haelt kumulierte Zeiten, die Erzeuger liefern
     * Intervalle. Die Verwechslung hat MF-438 einmal gekostet; hier steht
     * sie unter Beobachtung. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);
    uint32_t *iv = (uint32_t *)malloc(MAXCELLS * sizeof(uint32_t));
    uint32_t *tr = (uint32_t *)malloc(MAXCELLS * sizeof(uint32_t));
    ASSERT(iv != NULL && tr != NULL);

    size_t n = build_intervals(adf, 2000, 0.0, 1, iv, MAXCELLS);
    ASSERT(n > 1000);
    uint32_t t = 0;
    for (size_t i = 0; i < n; i++) { t += iv[i]; tr[i] = t; }

    uft_sync_pattern_t pat;
    ASSERT(amiga_pattern(&pat));

    uft_sync_hit_t a[MAXHITS], b[MAXHITS];
    size_t ha = uft_sync_search_intervals(iv, n, &pat, 0.0, a, MAXHITS);
    size_t hb = uft_sync_search_transitions(tr, n, &pat, 0.0, b, MAXHITS);
    ASSERT(ha >= 3);
    ASSERT(ha == hb);
    for (size_t i = 0; i < ha; i++) {
        ASSERT(a[i].index == b[i].index);
        double d = a[i].local_clock - b[i].local_clock;
        if (d < 0) d = -d;
        ASSERT(d < 1e-6);
    }
    free(tr); free(iv); free(adf);
}

TEST(noise_gets_no_answer_at_all)
{
    /* Neun Abstaende, die zufaellig zu einem gemeinsamen Takt passen, gibt
     * es — aber selten. Faende die Suche hier viel, waere sie ein
     * Zufallsgenerator mit Nachkommastellen und jede Rettung, die auf ihr
     * aufbaut, waere erfunden. */
    uint32_t *iv = (uint32_t *)malloc(20000 * sizeof(uint32_t));
    ASSERT(iv != NULL);
    uint64_t s = 0x2545F4914F6CDD1DULL;
    for (size_t i = 0; i < 20000; i++) {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        iv[i] = 1000u + (uint32_t)(s % 7000u);      /* gleichverteilt, kein MFM */
    }
    uft_sync_pattern_t pat;
    ASSERT(amiga_pattern(&pat));

    uft_sync_hit_t hits[MAXHITS];
    size_t h = uft_sync_search_intervals(iv, 20000, &pat, 0.0, hits, MAXHITS);
    if (h > 2) printf("\n        %zu Treffer im Rauschen\n", h);
    ASSERT(h <= 2);
    free(iv);
}

TEST(an_overflow_placeholder_breaks_the_hit_that_contains_it)
{
    /* Eine 0 im Intervallstrom ist eine SCP-Ueberlaufmarke, kein
     * Flusswechsel (MF-438). Der Taktabgleich muss sie verwerfen: es gibt
     * keinen Takt, zu dem 0 und k >= 1 zugleich passen. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);
    uint32_t *iv = (uint32_t *)malloc(MAXCELLS * sizeof(uint32_t));
    ASSERT(iv != NULL);
    size_t n = build_intervals(adf, 2000, 0.0, 1, iv, MAXCELLS);
    ASSERT(n > 1000);

    uft_sync_pattern_t pat;
    ASSERT(amiga_pattern(&pat));
    uft_sync_hit_t hits[MAXHITS];
    size_t before = uft_sync_search_intervals(iv, n, &pat, 0.0, hits, MAXHITS);
    ASSERT(before >= 3);

    iv[hits[0].index + 2] = 0;      /* mitten in die erste Fundstelle */
    uft_sync_hit_t after_hits[MAXHITS];
    size_t after = uft_sync_search_intervals(iv, n, &pat, 0.0,
                                             after_hits, MAXHITS);
    ASSERT(after == before - 1);
    ASSERT(after == 0 || after_hits[0].index != hits[0].index);

    free(iv); free(adf);
}

TEST(a_uniform_gap_yields_marks_not_one_per_interval)
{
    /* Der Fall, an dem sich zeigt, ob gezaehlt wird was da ist oder was
     * passt: 0xAAAA ist ein Muster aus lauter gleichen Abstaenden — genau
     * das, was ein MFM-Gap aus Null-Bytes im Strom hinterlaesst. Ein
     * gleichmaessiger Strom passt darauf an JEDER Stelle.
     *
     * Ohne das Ueberspringen der Fundstelle meldet die Suche hier einen
     * Treffer je Intervall: derselbe Gap, tausendfach gezaehlt. Das ist
     * dieselbe Falle wie MF-454 im Amiga-Decoder — Kandidaten zaehlen
     * statt Marken. */
    const uint16_t word = 0xAAAA;
    uft_sync_pattern_t pat;
    ASSERT(uft_sync_pattern_from_words(&word, 1, &pat));
    ASSERT(pat.k_count == 7);            /* 8 gesetzte Zellen -> 7 Abstaende */
    for (size_t i = 0; i < pat.k_count; i++) ASSERT(pat.k[i] == 2);

    enum { N = 700 };
    uint32_t *iv = (uint32_t *)malloc(N * sizeof(uint32_t));
    ASSERT(iv != NULL);
    for (size_t i = 0; i < N; i++) iv[i] = 4000;   /* 2 Zellen a 2000 ns */

    uft_sync_hit_t hits[MAXHITS];
    size_t h = uft_sync_search_intervals(iv, N, &pat, 0.0, hits, MAXHITS);
    ASSERT(h >= 2);

    /* Keine zwei Funde duerfen sich ueberlappen. */
    for (size_t i = 1; i < h; i++) {
        if (hits[i].index < hits[i - 1].index + pat.k_count)
            printf("\n        Fund %zu bei %zu ueberlappt Fund bei %zu\n",
                   i, hits[i].index, hits[i - 1].index);
        ASSERT(hits[i].index >= hits[i - 1].index + pat.k_count);
    }
    free(iv);
}

TEST(a_good_track_is_not_touched_by_the_second_pass)
{
    /* Wo der erste Durchlauf alles findet, darf der zweite gar nicht erst
     * laufen. Ein Rettungspfad, der auch die gesunden Faelle anfasst, ist
     * eine neue Fehlerquelle statt einer Rettung. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);
    uint32_t *iv = (uint32_t *)malloc(MAXCELLS * sizeof(uint32_t));
    ASSERT(iv != NULL);
    size_t n = build_intervals(adf, 2000, 0.0, 1, iv, MAXCELLS);
    ASSERT(n > 1000);
    ASSERT(decode_count(iv, n, 2000) == UFT_AMIGADOS_SPT);
    free(iv); free(adf);
}

TEST(an_empty_track_stays_empty)
{
    /* Die Gegenprobe zum zweiten Versuch: wo keine Marken sind, darf er
     * nichts erfinden. Ein Rettungspfad, der aus Rauschen Sektoren macht,
     * waere schlimmer als gar keiner. */
    uint32_t *iv = (uint32_t *)malloc(20000 * sizeof(uint32_t));
    ASSERT(iv != NULL);
    uint64_t s = 0x9E3779B97F4A7C15ULL;
    for (size_t i = 0; i < 20000; i++) {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        iv[i] = 1000u + (uint32_t)(s % 7000u);
    }
    size_t sec = decode_count(iv, 20000, 1700);
    if (sec != 0) printf("\n        %zu Sektoren aus Rauschen\n", sec);
    ASSERT(sec == 0);
    free(iv);
}

TEST(the_median_of_the_finds_is_what_counts_not_the_first_one)
{
    /* Eine reale Diskette laeuft nicht ueberall gleich schnell. Liegt der
     * ERSTE Fund in einem verzogenen Abschnitt, verzoege er als Massstab
     * die ganze Spur; der Mittelwert liesse sich anteilig mitziehen. Der
     * Median bleibt bei dem, was die Mehrheit der Marken zeigt.
     *
     * Geprueft wird hier die Auswahlregel selbst und nicht eine Spur, die
     * so gebaut waere, dass sie gerade diese Regel braucht — eine solche
     * Spur waere ein Test, der zum Code passt statt umgekehrt. */
    uft_sync_hit_t h[5];
    memset(h, 0, sizeof(h));
    const double clock[5] = { 2040.0, 1200.0, 1201.0, 1199.0, 1200.0 };
    for (size_t i = 0; i < 5; i++) h[i].local_clock = clock[i];

    double med = uft_sync_median_clock(h, 5);
    if (med < 1199.0 || med > 1201.0)
        printf("\n        Median %.1f — der Ausreisser hat entschieden\n", med);
    ASSERT(med >= 1199.0 && med <= 1201.0);

    /* Gerade Anzahl: Mittel der beiden mittleren Werte. */
    ASSERT(uft_sync_median_clock(h, 4) > 1199.0);
    ASSERT(uft_sync_median_clock(h, 4) < 1621.0);

    /* Randfaelle, die nicht knallen duerfen. */
    ASSERT(uft_sync_median_clock(h, 1) == 2040.0);
    ASSERT(uft_sync_median_clock(NULL, 5) == 0.0);
    ASSERT(uft_sync_median_clock(h, 0) == 0.0);
}

TEST(the_decoder_finds_sectors_the_pll_alone_never_sees)
{
    /* Der eigentliche Punkt, am automatischen Pfad gemessen — und mit dem
     * Wort, das die Messung hergibt: GEFUNDEN, nicht gerettet.
     *
     * Eine Spur mit 1200 ns je Zelle und 20 % Zittern: das Histogramm
     * (MF-488) verweigert hier die Auskunft — bei diesem Zittern sieht der
     * Strom nicht mehr nach sauberem MFM aus —, und der Nennwert 2000 ns
     * liegt mit dem Verhaeltnis 1,67 ausserhalb des gemessenen PLL-Fang-
     * bereichs von 0,80…1,50 (MF-487). Ohne die Sync-Suche kommt hier
     * **kein einziger** Sektor zurueck.
     *
     * Ehrlich dazu (MF-494): keiner der gefundenen Sektoren traegt bei
     * diesem Zittern eine heile Pruefsumme. Die Sektorpositionen sind ein
     * forensischer Befund, die Daten sind es nicht. Wer hier „gerettet"
     * liest, liest mehr, als gemessen wurde — deshalb heisst der Test
     * jetzt so, wie er heisst. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);
    uint32_t *iv = (uint32_t *)malloc(MAXCELLS * sizeof(uint32_t));
    ASSERT(iv != NULL);

    size_t n = build_intervals(adf, 1200, 20.0, 7, iv, MAXCELLS);
    ASSERT(n > 1000);

    /* Ohne Rettung: der Nennwert ausdruecklich vorgegeben — eine Vorgabe
     * sperrt den zweiten Durchlauf, das ist hier genau der Vergleichsfall. */
    size_t without = decode_count(iv, n, 2000);
    size_t with    = decode_count(iv, n, 0);
    if (!(without == 0 && with > 0))
        printf("\n        ohne Rettung %zu, mit %zu\n", without, with);
    ASSERT(without == 0);
    ASSERT(with >= 5);

    free(iv); free(adf);
}

int main(void)
{
    printf("=== Sync-Marken ohne PLL finden (MF-492) ===\n");
    RUN(the_amiga_sync_has_the_shape_the_stream_really_shows);
    RUN(a_pattern_needs_more_than_a_point_to_have_a_shape);
    RUN(on_a_clean_track_it_finds_exactly_what_the_decoder_finds);
    RUN(it_measures_the_cell_time_it_does_not_assume_one);
    RUN(both_representations_of_the_same_stream_agree);
    RUN(noise_gets_no_answer_at_all);
    RUN(an_overflow_placeholder_breaks_the_hit_that_contains_it);
    RUN(a_uniform_gap_yields_marks_not_one_per_interval);
    RUN(the_median_of_the_finds_is_what_counts_not_the_first_one);
    RUN(the_decoder_finds_sectors_the_pll_alone_never_sees);
    RUN(a_good_track_is_not_touched_by_the_second_pass);
    RUN(an_empty_track_stays_empty);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
