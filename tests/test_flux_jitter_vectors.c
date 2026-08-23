/**
 * @file test_flux_jitter_vectors.c
 * @brief Synthetische Flux-Vektoren mit Zeitzittern (MF-486).
 *
 * Baustein E des FluxEngine-Umsetzungsplans, der einzige, der ohne
 * FluxEngine-Quelle machbar ist: konstruierte Flussdaten, deren Erwartung
 * per Konstruktion feststeht — damit die Freeze-Regel (MF-363) ohne
 * physische Referenzdiskette bedienbar wird.
 *
 * ── Was hier NICHT als gegeben angenommen wird ───────────────────────────
 *
 * Der Entwurf behauptet: „injizierter Jitter ⇒ MUSS WEAK ergeben". Das ist
 * eine Aussage ueber das Verhalten des Decoders, nicht ueber die Physik, und
 * sie wird hier **gemessen statt geglaubt**. Was der Sweep unten ergibt,
 * steht in KNOWN_ISSUES; die Tests halten das gemessene Verhalten fest.
 *
 * Gemessen wurde ein Sweep ueber den Ausschlag, fuenf Seeds je Stufe, eine
 * AmigaDOS-Spur mit 11 Sektoren:
 *
 *     Jitter   gefunden   CRC falsch   Inhalt falsch
 *      0-15 %    55/55         0             0
 *        20 %    41/55        41            41
 *        25 %    15/55        15            14
 *        30 %     4/55         4             3
 *     >= 35 %     0/55         0             0
 *
 * Drei Bereiche, und der mittlere ist der interessante:
 *
 *   1. Bis etwa 15 % faengt die PLL alles ab — am Ergebnis ist NICHTS zu
 *      sehen. Das ist kein Zufall, das ist ihre Aufgabe.
 *   2. Zwischen 20 und 30 % werden Sektoren gefunden UND sind inhaltlich
 *      falsch. Aber: **jeder einzelne davon traegt eine falsche
 *      Pruefsumme.** Bei 25 % sind es sogar 15 CRC-Fehler bei 14 falschen
 *      Inhalten — die Pruefsumme ist strenger als noetig, also in der
 *      sicheren Richtung.
 *   3. Ab 35 % geht der Sync verloren und es wird gar nichts mehr gefunden.
 *
 * Die forensisch entscheidende Aussage steht in Bereich 2 und ist die
 * Invariante, die dieser Test festhaelt: **falscher Inhalt kommt nie mit
 * gueltiger Pruefsumme durch.** Zeitzittern erzeugt keine still
 * veraenderten Daten.
 *
 * Und die Behauptung des Entwurfs? Aus EINER Spur folgt kein WEAK — es
 * folgt „gefunden, aber Pruefsumme falsch". WEAK entsteht erst, wenn
 * dieselbe Spur MEHRFACH mit verschiedenem Jitter gelesen wird und die
 * Lesungen auseinanderlaufen. Genau das prueft der letzte Test, und die
 * Klasse heisst `MULTIREAD_CLASS_WEAK` — nicht `PROTECTED_CRC`, das der
 * Entwurf nennt; so heisst keine Klasse in diesem Baum.
 *
 * ── Warum Jitter auf die Positionen wirkt, nicht auf die Intervalle ──────
 *
 * Eine verschobene Flanke veraendert ZWEI benachbarte Intervalle
 * gegenlaeufig; ein verlaengertes Intervall nur eines. Wer den Jitter auf
 * Intervalle addiert, laesst die Spur mit der Zeit davonlaufen — das waere
 * kein Jitter, sondern Drehzahlschlupf. Siehe
 * uft_amigados_cells_to_intervals_jitter().
 */

#include "uft/uft_format_convert.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/formats/uft_scp_writer.h"
#include "uft/flux/uft_flux_decoder.h"
#include "uft/recovery/uft_multiread_pipeline.h"
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

#define CELLS  UFT_AMIGADOS_CELLS_PER_REV

/** Eine Spur erzeugen, dekodieren, und zaehlen was herauskam. */
typedef struct {
    int sectors;        /* gefundene Sektoren                */
    int bad_data_crc;   /* davon mit falscher Datenpruefsumme */
    int mismatched;     /* davon mit falschem INHALT          */
} decode_stats_t;

static bool decode_with_jitter(const uint8_t *adf, double jitter_pct,
                               uint64_t seed, decode_stats_t *st)
{
    memset(st, 0, sizeof(*st));

    uint8_t  *bits = (uint8_t *)calloc((CELLS + 7) / 8 + 1, 1);
    uint32_t *iv   = (uint32_t *)malloc(CELLS * sizeof(uint32_t));
    if (!bits || !iv) { free(bits); free(iv); return false; }

    uft_amigados_cells_t c = { bits, CELLS, 0, 0 };
    uft_amigados_build_track(&c, adf, 0, NULL);
    size_t n = uft_amigados_cells_to_intervals_jitter(&c, UFT_AMIGADOS_CELL_NS,
                                                      jitter_pct, seed,
                                                      iv, CELLS);
    bool ok = false;
    flux_raw_data_t raw;
    if (n > 0 && flux_raw_from_ns_intervals_indexed(iv, n,
                                                    UFT_AMIGADOS_REV_NS,
                                                    &raw) == FLUX_OK) {
        flux_decoder_options_t o;
        flux_decoder_options_init(&o);
        o.media = UFT_MEDIA_AMIGA_DD;

        flux_decoded_track_t dt;
        memset(&dt, 0, sizeof(dt));
        flux_decode_amiga(&raw, &dt, &o);

        st->sectors      = (int)dt.sector_count;
        st->bad_data_crc = (int)dt.bad_data_crc;
        for (size_t s = 0; s < dt.sector_count; s++) {
            const flux_decoded_sector_t *sec = &dt.sectors[s];
            if (!sec->data || sec->data_size != UFT_AMIGADOS_SECSZ) continue;
            if (sec->sector >= UFT_AMIGADOS_SPT) continue;
            if (memcmp(sec->data,
                       adf + (size_t)sec->sector * UFT_AMIGADOS_SECSZ,
                       UFT_AMIGADOS_SECSZ) != 0)
                st->mismatched++;
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

TEST(zero_jitter_is_the_unjittered_generator)
{
    /* Die Verankerung: mit 0 % muss der Jitter-Weg exakt das liefern, was der
     * bisherige liefert. Sonst prueft alles Folgende zwei Dinge auf einmal. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    uint8_t  *bits = (uint8_t *)calloc((CELLS + 7) / 8 + 1, 1);
    uint32_t *a = (uint32_t *)malloc(CELLS * sizeof(uint32_t));
    uint32_t *b = (uint32_t *)malloc(CELLS * sizeof(uint32_t));
    ASSERT(bits && a && b);

    uft_amigados_cells_t c = { bits, CELLS, 0, 0 };
    uft_amigados_build_track(&c, adf, 0, NULL);

    size_t na = uft_amigados_cells_to_intervals(&c, UFT_AMIGADOS_CELL_NS,
                                                a, CELLS);
    size_t nb = uft_amigados_cells_to_intervals_jitter(&c,
                                                       UFT_AMIGADOS_CELL_NS,
                                                       0.0, 12345, b, CELLS);
    ASSERT(na > 0);
    ASSERT(na == nb);
    ASSERT(memcmp(a, b, na * sizeof(uint32_t)) == 0);

    free(bits); free(a); free(b); free(adf);
}

TEST(the_same_seed_gives_the_same_track)
{
    /* Ohne Reproduzierbarkeit ist kein Befund nachvollziehbar und kein
     * roter Test wieder gruen zu bekommen. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    decode_stats_t s1, s2;
    ASSERT(decode_with_jitter(adf, 12.0, 4711, &s1));
    ASSERT(decode_with_jitter(adf, 12.0, 4711, &s2));
    ASSERT(s1.sectors == s2.sectors);
    ASSERT(s1.bad_data_crc == s2.bad_data_crc);
    ASSERT(s1.mismatched == s2.mismatched);

    /* Der Seed muss auch wirklich WIRKEN — sonst waere die
     * Reproduzierbarkeit oben trivial richtig.
     *
     * Verglichen wird auf der Intervall-Ebene, nicht am Dekodier-Ergebnis:
     * bei 12 % faengt die PLL alles ab, also waeren beide Ergebnisse
     * identisch und der Vergleich stumpf. Die erzeugten Zeiten
     * unterscheiden sich dagegen zwangslaeufig. */
    uint8_t  *bits = (uint8_t *)calloc((CELLS + 7) / 8 + 1, 1);
    uint32_t *a = (uint32_t *)malloc(CELLS * sizeof(uint32_t));
    uint32_t *b = (uint32_t *)malloc(CELLS * sizeof(uint32_t));
    ASSERT(bits && a && b);

    uft_amigados_cells_t c = { bits, CELLS, 0, 0 };
    uft_amigados_build_track(&c, adf, 0, NULL);
    size_t na = uft_amigados_cells_to_intervals_jitter(&c,
                    UFT_AMIGADOS_CELL_NS, 12.0, 4711, a, CELLS);
    size_t nb = uft_amigados_cells_to_intervals_jitter(&c,
                    UFT_AMIGADOS_CELL_NS, 12.0, 9999, b, CELLS);
    ASSERT(na > 0 && na == nb);
    ASSERT(memcmp(a, b, na * sizeof(uint32_t)) != 0);

    free(bits); free(a); free(b); free(adf);
}

TEST(a_pll_absorbs_small_jitter_completely)
{
    /* Was eine PLL ausmacht. Bis zu einem gewissen Ausschlag darf am
     * Ergebnis GAR NICHTS zu sehen sein — kein fehlender Sektor, keine
     * falsche Pruefsumme, kein veraenderter Inhalt. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    for (uint64_t seed = 1; seed <= 5; seed++) {
        decode_stats_t st;
        ASSERT(decode_with_jitter(adf, 10.0, seed, &st));
        if (st.sectors != UFT_AMIGADOS_SPT || st.bad_data_crc || st.mismatched)
            printf("\n        seed=%llu: %d Sektoren, %d CRC-Fehler, "
                   "%d Inhalte falsch\n", (unsigned long long)seed,
                   st.sectors, st.bad_data_crc, st.mismatched);
        ASSERT(st.sectors == UFT_AMIGADOS_SPT);
        ASSERT(st.bad_data_crc == 0);
        ASSERT(st.mismatched == 0);
    }

    free(adf);
}

TEST(corrupted_content_never_passes_the_checksum)
{
    /* DIE Invariante, und der Grund, warum dieser Test existiert.
     *
     * Im Mittelband (20-30 %) werden Sektoren gefunden UND sind inhaltlich
     * falsch. Das ist nicht schoen, aber es ist beherrschbar — solange jeder
     * dieser Sektoren eine falsche Pruefsumme traegt. Waere es anders,
     * koennte Zeitzittern still falsche Daten in ein Abbild schreiben, und
     * genau das darf ein forensisches Werkzeug nicht zulassen.
     *
     * Geprueft wird die Ungleichung, nicht eine Zahl: falscher Inhalt
     * IMPLIZIERT falsche Pruefsumme, also mismatched <= bad_data_crc. Der
     * umgekehrte Fall — Pruefsumme falsch, Inhalt zufaellig richtig — ist
     * erlaubt und kommt vor (bei 25 % gemessen: 15 zu 14). Das ist die
     * sichere Richtung. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    const double band[] = { 20.0, 25.0, 30.0 };
    int any_mismatch = 0;

    for (size_t i = 0; i < sizeof(band) / sizeof(band[0]); i++) {
        for (uint64_t seed = 1; seed <= 5; seed++) {
            decode_stats_t st;
            ASSERT(decode_with_jitter(adf, band[i], seed, &st));

            if (st.mismatched > st.bad_data_crc)
                printf("\n        %.0f%% seed=%llu: %d Inhalte falsch, aber "
                       "nur %d CRC-Fehler\n", band[i],
                       (unsigned long long)seed, st.mismatched,
                       st.bad_data_crc);
            ASSERT(st.mismatched <= st.bad_data_crc);
            any_mismatch += st.mismatched;
        }
    }

    /* Und das Band ist wirklich getroffen: gaebe es hier keine falschen
     * Inhalte, wuerde die Ungleichung oben nichts pruefen. */
    if (any_mismatch == 0)
        printf("\n        kein einziger falscher Inhalt im Band 20-30%%\n");
    ASSERT(any_mismatch > 0);

    free(adf);
}

TEST(heavy_jitter_loses_the_sync_entirely)
{
    /* Der dritte Bereich. Ab etwa 35 % findet der Decoder nichts mehr — der
     * Sektor fehlt, statt falsch zu sein. Eine Luecke ist sichtbar, ein
     * stiller Fehler nicht; das ist die gutartige Art zu scheitern. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    int total_found = 0;
    for (uint64_t seed = 1; seed <= 5; seed++) {
        decode_stats_t st;
        ASSERT(decode_with_jitter(adf, 45.0, seed, &st));
        total_found += st.sectors;
    }
    if (total_found != 0)
        printf("\n        45%% Jitter: %d Sektoren gefunden statt 0\n",
               total_found);
    ASSERT(total_found == 0);

    free(adf);
}

TEST(two_jittered_reads_of_one_track_classify_as_weak)
{
    /* Die Behauptung des Entwurfs, richtiggestellt und geprueft.
     *
     * Aus EINER verzitterten Spur folgt kein WEAK — es folgt „gefunden, aber
     * Pruefsumme falsch". WEAK ist eine Aussage ueber MEHRERE Lesungen
     * derselben Stelle: unterschiedliche Inhalte, und keine davon
     * pruefsummen-geprueft. Genau das entsteht, wenn dieselbe Spur zweimal
     * mit verschiedenem Zittern gelesen wird.
     *
     * Damit ist der Vektorsatz die Positivkontrolle, die der Entwurf haben
     * wollte — nur mit dem Klassennamen, den dieser Baum wirklich hat. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    /* Zwei Lesungen mit demselben Ausschlag, verschiedenen Seeds. */
    uint8_t reads[2][UFT_AMIGADOS_SECSZ];
    int have = 0;

    for (uint64_t seed = 1; seed <= 40 && have < 2; seed++) {
        uint8_t  *bits = (uint8_t *)calloc((CELLS + 7) / 8 + 1, 1);
        uint32_t *iv   = (uint32_t *)malloc(CELLS * sizeof(uint32_t));
        if (!bits || !iv) { free(bits); free(iv); break; }

        uft_amigados_cells_t c = { bits, CELLS, 0, 0 };
        uft_amigados_build_track(&c, adf, 0, NULL);
        size_t n = uft_amigados_cells_to_intervals_jitter(
                       &c, UFT_AMIGADOS_CELL_NS, 22.0, seed, iv, CELLS);

        flux_raw_data_t raw;
        if (n > 0 && flux_raw_from_ns_intervals_indexed(
                         iv, n, UFT_AMIGADOS_REV_NS, &raw) == FLUX_OK) {
            flux_decoder_options_t o;
            flux_decoder_options_init(&o);
            o.media = UFT_MEDIA_AMIGA_DD;

            flux_decoded_track_t dt;
            memset(&dt, 0, sizeof(dt));
            flux_decode_amiga(&raw, &dt, &o);

            /* Sektor 0 mit falscher Pruefsumme — genau das, was das
             * Mittelband liefert. */
            for (size_t s = 0; s < dt.sector_count && have < 2; s++) {
                const flux_decoded_sector_t *sec = &dt.sectors[s];
                if (sec->sector != 0 || !sec->data) continue;
                if (sec->data_size != UFT_AMIGADOS_SECSZ) continue;
                if (sec->data_crc_ok) continue;
                if (have == 1 && memcmp(reads[0], sec->data,
                                        UFT_AMIGADOS_SECSZ) == 0) continue;
                memcpy(reads[have++], sec->data, UFT_AMIGADOS_SECSZ);
            }
            flux_decoded_track_free(&dt);
            flux_raw_free(&raw);
        }
        free(bits); free(iv);
    }

    if (have < 2)
        printf("\n        nur %d verschiedene fehlerhafte Lesungen gefunden\n",
               have);
    ASSERT(have == 2);

    /* Beide durch die Abstimmung: unterschiedlich, keine geprueft => weak. */
    multiread_config_t cfg = multiread_config_default();
    cfg.min_passes = 1;
    cfg.detect_weak_bits = true;
    multiread_ctx_t *mr = multiread_create(&cfg);
    ASSERT(mr != NULL);
    ASSERT(multiread_add_pass(mr, reads[0], UFT_AMIGADOS_SECSZ, 50, false)
           == MULTIREAD_OK);
    ASSERT(multiread_add_pass(mr, reads[1], UFT_AMIGADOS_SECSZ, 50, false)
           == MULTIREAD_OK);

    uint8_t voted[UFT_AMIGADOS_SECSZ];
    multiread_sector_t res;
    memset(&res, 0, sizeof(res));
    ASSERT(multiread_execute(mr, voted, UFT_AMIGADOS_SECSZ, &res)
           == MULTIREAD_OK);

    if (res.class_ != MULTIREAD_CLASS_WEAK)
        printf("\n        Klasse %s statt weak\n",
               multiread_class_name(res.class_));
    ASSERT(res.class_ == MULTIREAD_CLASS_WEAK);

    /* Und nichts davon gilt als wiederhergestellt — keine Lesung war
     * geprueft (MF-466). */
    ASSERT(res.recovered == false);
    ASSERT(res.weak_offset >= 0);

    free(res.weak_mask);
    multiread_destroy(mr);
    free(adf);
}

int main(void)
{
    printf("=== Synthetische Flux-Vektoren mit Jitter (MF-486) ===\n");
    RUN(zero_jitter_is_the_unjittered_generator);
    RUN(the_same_seed_gives_the_same_track);
    RUN(a_pll_absorbs_small_jitter_completely);
    RUN(corrupted_content_never_passes_the_checksum);
    RUN(heavy_jitter_loses_the_sync_entirely);
    RUN(two_jittered_reads_of_one_track_classify_as_weak);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
