/**
 * @file test_pll_regression.c
 * @brief Der Fangbereich der PLL, als Netz gegen stille Verschlechterung
 *        (MF-487).
 *
 * Baustein E Punkt 1 des FluxEngine-Plans: „jede Aenderung an
 * `uft_flux_pll.h` laeuft gegen den Vektorsatz".
 *
 * ── Warum es das braucht ─────────────────────────────────────────────────
 *
 * Bis hier pruefte **nichts die PLL direkt**. `test_flux_index_wiring`
 * prueft, welche Zellendauer GEWAEHLT wird (MF-475). `test_media_profile`
 * prueft die Rechnung dahinter. Die Wandlungstests pruefen das Ende der
 * Kette. Wer den Regelkreis selbst verstellt — Verstaerkung, Fenster,
 * Einrastverhalten — bekommt von keinem dieser Tests eine Antwort, solange
 * am Ende zufaellig noch genug herauskommt.
 *
 * ── Wie die PLL isoliert wird ────────────────────────────────────────────
 *
 * Der Flux wird mit einer WAHREN Zellendauer erzeugt und mit einer
 * ANGENOMMENEN dekodiert. Ihr Verhaeltnis ist der Fehler, den die PLL
 * ausregeln muss — und sonst wirkt nichts mit: kein Medienprofil, keine
 * gemessene Umdrehung, keine Indexmarken. `opts.bitcell_ns` gesetzt heisst
 * „diese Zahl gilt" (MF-471); damit steht der Startwert fest und der Test
 * misst den Regelkreis statt der Zellendauer-Wahl.
 *
 * ── Der gemessene Fangbereich ────────────────────────────────────────────
 *
 *     Annahme/Wahr   Sektoren   eingerastete Bitrate
 *         0.40-0.70     0/11     (kein Einrasten)
 *         0.80         11/11     520833
 *         0.90-1.20    11/11     500000   <- exakt 1e9/2000
 *         1.30         11/11     480769
 *         1.40         11/11     446429
 *         1.50         11/11     416667
 *         1.80-2.00     0/11     (kein Einrasten)
 *
 * Zwei Dinge, die man vorher nicht wusste:
 *
 *   1. Der Bereich ist **asymmetrisch** — nach oben zieht die PLL bis +50 %,
 *      nach unten nur bis -20 %. Das ist kein Fehler, sondern eine
 *      Eigenschaft, und sie gehoert festgehalten, damit sie nicht
 *      unbemerkt verschwindet.
 *   2. Dekodieren und Einrasten sind nicht dasselbe. Zwischen 1.30 und 1.50
 *      kommen alle elf Sektoren heraus, obwohl die gemeldete Periode
 *      deutlich danebenliegt: die Korrektur je Zelle traegt weiter, wo der
 *      Mittelwert schon abgedriftet ist.
 *
 * ── Versatz und Zittern addieren sich, und zwar einseitig ────────────────
 *
 * Der erste Anlauf dieses Tests verlangte, dass die Kombination „nicht
 * schlechter als ihre Teile" sei. Das war Wunschdenken; die Messung
 * (Sektoren von 11, schlechtester von fuenf Seeds):
 *
 *     Annahme/Wahr |   0%    4%    8%   12%   16%   Zittern
 *             0.85 |   11     0     0     0    10
 *             0.90 |   11    10    10    10    11
 *             0.95 |   11    10    10    11    11
 *             1.00 |   11    10    11    11    11
 *             1.10 |   11    11    11    11    11
 *             1.20 |   11    11    11    11    11
 *             1.30 |   11    11    11    11    11
 *
 * Drei Ablesungen:
 *
 *   - **Unterhalb des Nennwerts ist der Fangbereich bruechig.** Bei 0.85
 *     genuegen 4 % Zittern, um die Spur ganz zu verlieren, obwohl beide
 *     Stoerungen einzeln folgenlos sind.
 *   - **Oberhalb ueberlebt der SYNC alles.** Ab 1.10 werden immer alle elf
 *     gefunden, egal wie stark gezittert wird. Wer eine Zellendauer
 *     schaetzen muss, schaetzt also besser zu GROSS als zu klein — eine
 *     Betriebsempfehlung, die aus dieser Tabelle folgt und sonst nirgends
 *     steht.
 *
 *     **Gefunden heisst aber nicht fehlerfrei.** Das uebersah der erste
 *     Anlauf dieses Tests: bei 1.10 mit 16 % Zittern kommen alle elf
 *     Sektoren, und einer traegt einen verfaelschten Inhalt — korrekt als
 *     CRC-Fehler markiert. Die Tabelle misst das Ueberleben des Syncs,
 *     nicht die Unversehrtheit der Nutzdaten. Der Sweep, der sie erzeugt
 *     hat, zaehlte nur Sektoren; er mass die falsche Groesse.
 *   - **Der Verlauf ist nicht monoton.** Bei 0.85 liefert 16 % Zittern mehr
 *     als 4 %. Eine Zusicherung der Form „mehr Stoerung, nie besser" waere
 *     also falsch.
 *
 * Deshalb sichert dieser Test die Kombination nur dort zu, wo sie robust
 * gemessen ist (>= 1.10), und verlangt sonst nur das, was ueberall gilt:
 * keine stille Verfaelschung.
 *
 * ── Schwellen mit Abstand, nicht auf der Kante ───────────────────────────
 *
 * Die Tests pruefen den Bereich NICHT an seinen Raendern. Eine Schwelle auf
 * der gemessenen Grenze waere auf einer anderen Plattform, einem anderen
 * Compiler oder nach einer harmlosen Umstellung rot — und ein Test, der aus
 * Rundungsgruenden rot wird, wird abgeschaltet statt gelesen.
 *
 * Geprueft wird tief INNERHALB (0.90 … 1.30 muss gehen) und weit AUSSERHALB
 * (0.50 und 2.00 duerfen nicht gehen). Die Streifen 0.70-0.90 und 1.30-1.80
 * bleiben absichtlich ohne Zusicherung: wo genau die Kante liegt, weiss
 * dieser Test nicht, und er tut nicht so.
 */

#include "uft/uft_types.h"
#include "uft/flux/uft_flux_decoder.h"
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

#define CELLS      UFT_AMIGADOS_CELLS_PER_REV
#define TRUE_CELL  UFT_AMIGADOS_CELL_NS          /* 2000 ns */
#define NOMINAL_BITRATE (1.0e9 / (double)TRUE_CELL)   /* 500000 */

typedef struct {
    int    sectors;
    int    bad_data_crc;
    int    mismatched;      /* gefunden, aber Inhalt falsch */
    double avg_bitrate;     /* worauf die PLL eingerastet ist */
} pll_result_t;

/**
 * Flux mit @p true_cell_ns erzeugen, mit @p assumed_cell_ns dekodieren.
 */
static bool decode_at(const uint8_t *adf, unsigned true_cell_ns,
                      double assumed_cell_ns, double jitter_pct,
                      uint64_t seed, pll_result_t *r)
{
    memset(r, 0, sizeof(*r));

    uint8_t  *bits = (uint8_t *)calloc((CELLS + 7) / 8 + 1, 1);
    uint32_t *iv   = (uint32_t *)malloc(CELLS * sizeof(uint32_t));
    if (!bits || !iv) { free(bits); free(iv); return false; }

    uft_amigados_cells_t c = { bits, CELLS, 0, 0 };
    uft_amigados_build_track(&c, adf, 0, NULL);
    size_t n = uft_amigados_cells_to_intervals_jitter(&c, true_cell_ns,
                                                      jitter_pct, seed,
                                                      iv, CELLS);
    bool ok = false;
    flux_raw_data_t raw;
    if (n > 0 && flux_raw_from_ns_intervals(iv, n, &raw) == FLUX_OK) {
        flux_decoder_options_t o;
        flux_decoder_options_init(&o);
        o.bitcell_ns = assumed_cell_ns;   /* die Annahme, die geregelt wird */

        flux_decoded_track_t dt;
        memset(&dt, 0, sizeof(dt));
        flux_decode_amiga(&raw, &dt, &o);

        r->sectors      = (int)dt.sector_count;
        r->bad_data_crc = (int)dt.bad_data_crc;
        r->avg_bitrate  = dt.avg_bitrate;
        for (size_t s = 0; s < dt.sector_count; s++) {
            const flux_decoded_sector_t *sec = &dt.sectors[s];
            if (!sec->data || sec->data_size != UFT_AMIGADOS_SECSZ) continue;
            if (sec->sector >= UFT_AMIGADOS_SPT) continue;
            if (memcmp(sec->data,
                       adf + (size_t)sec->sector * UFT_AMIGADOS_SECSZ,
                       UFT_AMIGADOS_SECSZ) != 0)
                r->mismatched++;
        }
        flux_decoded_track_free(&dt);
        flux_raw_free(&raw);
        ok = true;
    }
    free(bits); free(iv);
    return ok;
}

static uint8_t *make_source_adf(void)
{
    uint8_t *adf = (uint8_t *)calloc(1, UFT_AMIGADOS_ADF_SIZE);
    if (!adf) return NULL;
    uft_amigados_fill_pattern(adf, (size_t)UFT_AMIGADOS_SPT
                                   * UFT_AMIGADOS_SECSZ);
    return adf;
}

/* ────────────────────────────────────────────────────────────────────── */

TEST(without_an_offset_the_pll_lands_on_the_true_cell_time)
{
    /* Die Verankerung. Stimmt die Annahme, muss die eingerastete Periode
     * exakt die wahre sein — das ist der Nullpunkt, gegen den jede
     * Abweichung unten gemessen wird. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    pll_result_t r;
    ASSERT(decode_at(adf, TRUE_CELL, (double)TRUE_CELL, 0.0, 1, &r));

    if (r.sectors != UFT_AMIGADOS_SPT)
        printf("\n        %d von %d Sektoren\n", r.sectors, UFT_AMIGADOS_SPT);
    ASSERT(r.sectors == UFT_AMIGADOS_SPT);
    ASSERT(r.bad_data_crc == 0);
    ASSERT(r.mismatched == 0);

    double err = r.avg_bitrate / NOMINAL_BITRATE - 1.0;
    if (err < 0) err = -err;
    if (err > 0.01)
        printf("\n        Bitrate %.0f statt %.0f (%.2f %% daneben)\n",
               r.avg_bitrate, NOMINAL_BITRATE, err * 100.0);
    ASSERT(err < 0.01);

    free(adf);
}

TEST(the_pll_pulls_in_offsets_from_minus_ten_to_plus_thirty_percent)
{
    /* Der Kern der Regression. Diese Punkte liegen tief im gemessenen
     * Fangbereich (0.80 … 1.50); wer ihn verengt, faellt hier auf.
     *
     * Die Raender selbst werden bewusst NICHT geprueft — siehe Dateikopf. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    const double inside[] = { 0.90, 1.00, 1.10, 1.20, 1.30 };
    for (size_t i = 0; i < sizeof(inside) / sizeof(inside[0]); i++) {
        pll_result_t r;
        ASSERT(decode_at(adf, TRUE_CELL, TRUE_CELL * inside[i], 0.0, 1, &r));
        if (r.sectors != UFT_AMIGADOS_SPT || r.mismatched)
            printf("\n        Verhaeltnis %.2f: %d Sektoren, %d Inhalte "
                   "falsch\n", inside[i], r.sectors, r.mismatched);
        ASSERT(r.sectors == UFT_AMIGADOS_SPT);
        ASSERT(r.bad_data_crc == 0);
        ASSERT(r.mismatched == 0);
    }

    free(adf);
}

TEST(far_outside_the_range_nothing_is_decoded_rather_than_something_wrong)
{
    /* Die wichtigere Haelfte. Eine PLL, die weit ausserhalb ihres Bereichs
     * auf eine Oberwelle einrastet, liefert Sektoren — falsche. Genau das
     * darf nicht passieren: lieber nichts als etwas Erfundenes.
     *
     * 0.50 und 2.00 liegen weit jenseits der gemessenen Kanten (0.70-0.80
     * bzw. 1.50-1.80), der Abstand ist also gross. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    const double outside[] = { 0.50, 2.00 };
    for (size_t i = 0; i < sizeof(outside) / sizeof(outside[0]); i++) {
        pll_result_t r;
        ASSERT(decode_at(adf, TRUE_CELL, TRUE_CELL * outside[i], 0.0, 1, &r));
        if (r.sectors != 0)
            printf("\n        Verhaeltnis %.2f: %d Sektoren gefunden, "
                   "%d Inhalte falsch\n", outside[i], r.sectors, r.mismatched);
        ASSERT(r.sectors == 0);
    }

    free(adf);
}

TEST(no_combination_of_offset_and_jitter_corrupts_silently)
{
    /* Das Netz ueber die ganze Flaeche: Versatz mal Zittern.
     *
     * Geprueft wird die Invariante aus MF-486, jetzt auch ueber die
     * Versatz-Achse — falscher Inhalt IMPLIZIERT falsche Pruefsumme. Sie
     * ist die eine Zusicherung, die ueberall gelten muss, auch dort, wo der
     * Test ueber Sektorzahlen nichts behauptet. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    const double ratios[]  = { 0.70, 0.85, 1.00, 1.15, 1.45, 1.70 };
    const double jitters[] = { 0.0, 8.0, 18.0, 28.0 };

    for (size_t i = 0; i < sizeof(ratios) / sizeof(ratios[0]); i++) {
        for (size_t j = 0; j < sizeof(jitters) / sizeof(jitters[0]); j++) {
            for (uint64_t seed = 1; seed <= 3; seed++) {
                pll_result_t r;
                ASSERT(decode_at(adf, TRUE_CELL, TRUE_CELL * ratios[i],
                                 jitters[j], seed, &r));
                if (r.mismatched > r.bad_data_crc)
                    printf("\n        %.2f / %.0f%% / seed %llu: "
                           "%d Inhalte falsch, nur %d CRC-Fehler\n",
                           ratios[i], jitters[j],
                           (unsigned long long)seed,
                           r.mismatched, r.bad_data_crc);
                ASSERT(r.mismatched <= r.bad_data_crc);
            }
        }
    }

    free(adf);
}

TEST(above_the_nominal_cell_time_the_sync_survives_any_jitter)
{
    /* Die robuste Haelfte, und die eigentliche Zusicherung dieses Tests:
     * ab Versatz 1.10 werden alle elf Sektoren GEFUNDEN, auch unter
     * kraeftigem Zittern. Wer den Fangbereich verengt, faellt hier auf.
     *
     * Gefunden heisst nicht fehlerfrei: bei 16 % Zittern kann ein Sektor
     * einen verfaelschten Inhalt tragen. Zugesichert ist deshalb, dass so
     * einer MARKIERT ist — nicht, dass es ihn nicht gibt.
     *
     * Die andere Haelfte (unterhalb des Nennwerts) wird bewusst NICHT
     * zugesichert — dort genuegen 4 % Zittern, um bei 0.85 die ganze Spur
     * zu verlieren. Siehe Tabelle im Dateikopf. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    const double ratios[]  = { 1.10, 1.20, 1.30 };
    const double jitters[] = { 4.0, 8.0, 12.0, 16.0 };

    for (size_t i = 0; i < sizeof(ratios) / sizeof(ratios[0]); i++) {
        for (size_t j = 0; j < sizeof(jitters) / sizeof(jitters[0]); j++) {
            for (uint64_t seed = 1; seed <= 5; seed++) {
                pll_result_t r;
                ASSERT(decode_at(adf, TRUE_CELL, TRUE_CELL * ratios[i],
                                 jitters[j], seed, &r));
                if (r.sectors != UFT_AMIGADOS_SPT
                    || r.mismatched > r.bad_data_crc)
                    printf("\n        %.2f mit %.0f%% Zittern, seed %llu: "
                           "%d Sektoren, %d Inhalte falsch, %d CRC-Fehler\n",
                           ratios[i], jitters[j], (unsigned long long)seed,
                           r.sectors, r.mismatched, r.bad_data_crc);
                ASSERT(r.sectors == UFT_AMIGADOS_SPT);
                ASSERT(r.mismatched <= r.bad_data_crc);
            }
        }
    }

    free(adf);
}

TEST(below_the_nominal_cell_time_jitter_takes_sectors_but_never_truth)
{
    /* Die bruechige Haelfte, ehrlich festgehalten statt weggelassen.
     *
     * Unterhalb des Nennwerts kostet Zittern Sektoren — bei 0.85 gehen sie
     * schon bei 4 % vollstaendig verloren. Zugesichert wird deshalb NICHT,
     * wie viele ankommen, sondern das, was in jedem Fall gelten muss: was
     * ankommt, ist nicht still verfaelscht.
     *
     * Ein Test, der hier eine Sektorzahl fordern wuerde, waere entweder
     * falsch oder muesste die Messung Zeile fuer Zeile einfrieren — und
     * damit bei der kleinsten Rundungsdifferenz rot werden. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    const double ratios[]  = { 0.85, 0.90, 0.95, 1.00 };
    const double jitters[] = { 4.0, 8.0, 12.0, 16.0 };

    for (size_t i = 0; i < sizeof(ratios) / sizeof(ratios[0]); i++) {
        for (size_t j = 0; j < sizeof(jitters) / sizeof(jitters[0]); j++) {
            for (uint64_t seed = 1; seed <= 5; seed++) {
                pll_result_t r;
                ASSERT(decode_at(adf, TRUE_CELL, TRUE_CELL * ratios[i],
                                 jitters[j], seed, &r));
                if (r.mismatched > r.bad_data_crc)
                    printf("\n        %.2f / %.0f%% / seed %llu: %d Inhalte "
                           "falsch, nur %d CRC-Fehler\n", ratios[i],
                           jitters[j], (unsigned long long)seed,
                           r.mismatched, r.bad_data_crc);
                ASSERT(r.mismatched <= r.bad_data_crc);
            }
        }
    }

    free(adf);
}

TEST(switching_the_pll_off_freezes_the_period_in_every_decoder)
{
    /* Die Verdrahtungsprobe, und sie hat beim Schreiben dieses Tests einen
     * echten Fehler gefunden (MF-487).
     *
     * Geprueft wird in BEIDE Richtungen, und nur so faellt der Fehler auf:
     *
     *   - `use_pll = false`  =>  die gemeldete Bitrate ist EXAKT die
     *     Startperiode. Nichts wird nachgeregelt.
     *   - `use_pll = true`   =>  sie ist es NICHT. Die Regelung arbeitet.
     *
     * `flux_decode_fm()` uebernahm als einziger weder `opts->use_pll` noch
     * `opts->pll_gain`. Und weil `flux_pll_init()` mit `memset(...,0)`
     * beginnt, blieb `use_pll` dort auf FALSE: **der FM-Pfad lief nie mit
     * Regelung.** Eine Pruefung nur auf `use_pll = false` haette das nicht
     * gesehen — beide Faelle sehen dort gleich aus. Genau daran ist die
     * erste Fassung dieses Tests gescheitert.
     *
     * Geprueft wird mit AmigaDOS-Flux fuer alle Decoder. Dass die
     * IBM-/GCR-Decoder darin keine Sektoren finden, ist gleichgueltig und
     * sogar nuetzlich: geprueft wird der Regelkreis, nicht der Sektorparser. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    uint8_t  *bits = (uint8_t *)calloc((CELLS + 7) / 8 + 1, 1);
    uint32_t *iv   = (uint32_t *)malloc(CELLS * sizeof(uint32_t));
    ASSERT(bits && iv);

    uft_amigados_cells_t c = { bits, CELLS, 0, 0 };
    uft_amigados_build_track(&c, adf, 0, NULL);
    /* Mit Zittern, sonst haette die Regelung ohnehin nichts zu tun und der
     * Test koennte den Unterschied nicht sehen. */
    size_t n = uft_amigados_cells_to_intervals_jitter(&c, TRUE_CELL, 12.0, 3,
                                                      iv, CELLS);
    ASSERT(n > 0);

    flux_raw_data_t raw;
    ASSERT(flux_raw_from_ns_intervals(iv, n, &raw) == FLUX_OK);

    /* Ein Startwert, der NICHT der wahren Zellendauer entspricht: liefe die
     * Regelung trotz Abschaltung, zoege sie ihn in Richtung Wahrheit und der
     * Vergleich unten schlaege an. */
    const double start_ns = TRUE_CELL * 1.20;

    struct { const char *name;
             flux_status_t (*fn)(const flux_raw_data_t *,
                                 flux_decoded_track_t *,
                                 const flux_decoder_options_t *); }
    decoders[] = {
        { "mfm",       flux_decode_mfm       },
        { "fm",        flux_decode_fm        },
        { "gcr_c64",   flux_decode_gcr_c64   },
        { "gcr_apple", flux_decode_gcr_apple },
        { "amiga",     flux_decode_amiga     },
    };

    const double frozen = 1.0e9 / start_ns;

    for (size_t i = 0; i < sizeof(decoders) / sizeof(decoders[0]); i++) {
        /* Aus: die Periode darf sich nicht bewegen. */
        flux_decoder_options_t off;
        flux_decoder_options_init(&off);
        off.bitcell_ns = start_ns;
        off.use_pll = false;

        flux_decoded_track_t dt_off;
        memset(&dt_off, 0, sizeof(dt_off));
        decoders[i].fn(&raw, &dt_off, &off);

        double err = dt_off.avg_bitrate / frozen - 1.0;
        if (err < 0) err = -err;
        if (err > 1e-9)
            printf("\n        %s: use_pll=false, Bitrate %.3f statt %.3f\n",
                   decoders[i].name, dt_off.avg_bitrate, frozen);
        ASSERT(err <= 1e-9);
        flux_decoded_track_free(&dt_off);

        /* An: sie MUSS sich bewegen. Diese Richtung findet den Decoder, der
         * die Option gar nicht erst uebernimmt. */
        flux_decoder_options_t on;
        flux_decoder_options_init(&on);
        on.bitcell_ns = start_ns;
        on.use_pll = true;

        flux_decoded_track_t dt_on;
        memset(&dt_on, 0, sizeof(dt_on));
        decoders[i].fn(&raw, &dt_on, &on);

        double moved = dt_on.avg_bitrate / frozen - 1.0;
        if (moved < 0) moved = -moved;
        if (moved <= 1e-9)
            printf("\n        %s: use_pll=true, Bitrate steht bei %.3f — die "
                   "Regelung wurde nicht eingeschaltet\n",
                   decoders[i].name, dt_on.avg_bitrate);
        ASSERT(moved > 1e-9);
        flux_decoded_track_free(&dt_on);
    }

    flux_raw_free(&raw);
    free(bits); free(iv); free(adf);
}

TEST(the_same_input_gives_the_same_answer)
{
    /* Ohne das ist jede Schwelle oben wertlos: ein Regelkreis, der bei
     * gleicher Eingabe zweimal Verschiedenes liefert, laesst sich nicht
     * gegen eine Grenze pruefen. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    pll_result_t a, b;
    ASSERT(decode_at(adf, TRUE_CELL, TRUE_CELL * 1.15, 18.0, 7, &a));
    ASSERT(decode_at(adf, TRUE_CELL, TRUE_CELL * 1.15, 18.0, 7, &b));

    ASSERT(a.sectors == b.sectors);
    ASSERT(a.bad_data_crc == b.bad_data_crc);
    ASSERT(a.mismatched == b.mismatched);
    ASSERT(a.avg_bitrate == b.avg_bitrate);

    free(adf);
}

int main(void)
{
    printf("=== PLL-Regression: Fangbereich und Einrasten (MF-487) ===\n");
    RUN(without_an_offset_the_pll_lands_on_the_true_cell_time);
    RUN(the_pll_pulls_in_offsets_from_minus_ten_to_plus_thirty_percent);
    RUN(far_outside_the_range_nothing_is_decoded_rather_than_something_wrong);
    RUN(no_combination_of_offset_and_jitter_corrupts_silently);
    RUN(above_the_nominal_cell_time_the_sync_survives_any_jitter);
    RUN(below_the_nominal_cell_time_jitter_takes_sectors_but_never_truth);
    RUN(switching_the_pll_off_freezes_the_period_in_every_decoder);
    RUN(the_same_input_gives_the_same_answer);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
