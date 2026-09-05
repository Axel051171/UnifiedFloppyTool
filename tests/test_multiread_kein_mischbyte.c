/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_multiread_kein_mischbyte.c
 * @brief Das Voting darf keinen Sektor erfinden, den niemand gelesen hat (MF-845).
 *
 * ── Der Fehler ───────────────────────────────────────────────────────────
 *
 * `multiread_execute()` baut den Ausgabepuffer ueber `vote_buffer()` —
 * eine **byteweise unabhaengige** Mehrheitsabstimmung — und ruft
 * `classify_passes()` erst **danach** auf. Die Einstufung konnte den
 * bereits gebauten Puffer also nicht mehr beeinflussen.
 *
 * Bei zwei Lesungen, die BEIDE ihre CRC bestehen und sich trotzdem
 * unterscheiden (`MULTIREAD_CLASS_AMBIGUOUS_GOOD` — der Normalfall bei
 * Fuzzy Bits), ergibt die byteweise Mehrheit eine dritte Bytefolge:
 *
 *     Pass A   01 FF 02 FF 03 FF 04 FF
 *     Pass B   FF 01 FF 02 FF 03 FF 04
 *     Voting   01 01 02 02 03 03 04 04     <- war auf keiner Diskette
 *
 * Bei Gleichstand gewinnt in `vote_byte()` der kleinere Bytewert, und
 * zwar stillschweigend. Das Ergebnis ist weder A noch B.
 *
 * Fuer ein Werkzeug mit dem Grundsatz „Keine erfundenen Daten" ist das
 * die schwerste Fehlerklasse: hier geht nichts verloren, hier ENTSTEHT
 * etwas. Und es landet unveraendert im Zielabbild — `uftc_adf_place_voted()`
 * und `uftc_d64_place_voted()` uebernehmen den Puffer, sobald
 * `res.recovered` gilt, und das ist bei zwei CRC-validen Lesungen
 * trivial erfuellt.
 *
 * ── Das Referenzverhalten stand im eigenen Kommentar ─────────────────────
 *
 * `classify_passes()` traegt seit jeher die Zeile
 *
 *     „a8rawconv behaelt einen und warnt"
 *
 * — genau das Richtige, nur eben nicht getan. Zwei fremde Werkzeuge
 * machen es so:
 *
 *   a8rawconv  `src/a8rawconv/disk.cpp:236-365` (`sift_sectors`) behaelt
 *              bei mehreren gueltigen, aber abweichenden Inhalten EINEN
 *              ganzen Kandidaten und warnt.
 *   FluxEngine `lib/algorithms/readerwriter.cc::collectSectors()`
 *              vergleicht GANZE Kandidaten; zwei `Sector::OK` mit
 *              verschiedenen Daten werden `Sector::CONFLICT`, und mit
 *              `collapse_conflicts=false` bleiben beide Originale.
 *
 * Beide mischen nie auf Byteebene.
 *
 * ── Warum der Test hier steht und nicht im Modul ─────────────────────────
 *
 * -- Nachtrag MF-884: was diesen Waechter traegt, stand nirgends ---------
 *
 * Der Fall unten (`ohne_gueltige_crc_bleibt_das_byte_voting`) laesst das
 * Byte-Voting im WEAK-Zweig absichtlich stehen, und das ist richtig:
 * liegt keine CRC-valide Lesung vor, ist die byteweise Mehrheit eine
 * vertretbare Schaetzung — bei vielen Lesungen eines echt schwachen
 * Sektors sogar die BESSERE Wiederherstellung als irgendeine einzelne
 * Lesung, weil sie die stabilen Bits behaelt.
 *
 * Gemessen (MF-884) ist dabei aber auch: in dieser Klasse ist die
 * Ausgabe fast immer eine Bytefolge, die KEINE Lesung geliefert hat --
 * bei zwei Lesungen ohne CRC in 99,9 % der Faelle, bei drei und vier in
 * 100,0 % (je 20 000 Zufallslaeufe). Grund: jede abweichende
 * Byteposition ist dann ein Gleichstand, und `vote_byte()` behaelt
 * stillschweigend den kleineren Bytewert.
 *
 * Dass das folgenlos bleibt, haengt an EINER Kopplung, die bis MF-884
 * nirgends ausgesprochen war:
 *
 *     recovered = (confidence >= min_confidence) && (good_reads > 0)
 *
 * In der WEAK-Klasse ist `good_reads` per Definition 0 -- sonst waere es
 * keine WEAK-Klasse. Also ist `recovered` dort immer false, und beide
 * Produktionsverbraucher schreiben nur bei `recovered`
 * (`uft_format_convert_flux.c:190` und `:541`). Das Fabrikat erreicht
 * das Zielabbild nie.
 *
 * Zwei getrennte Regeln -- MF-466 (`good_reads > 0`) und die
 * Klassendefinition -- ergeben zusammen eine dritte, die niemand
 * aufgeschrieben hat. Wer MF-466 lockert, macht das Fabrikat still
 * erreichbar. `wer_recovered_meldet_hat_wirklich_gelesen` unten haelt
 * genau diese Kopplung fest; gemessen an 2 000 000 Zufallseingaben:
 * 948 434 mit `recovered`, 380 014 Fabrikate, 0 zugleich.
 *
 * `uft_multiread_pipeline.c` hat am Dateiende einen `#ifdef
 * UFT_UNIT_TESTS`-Block. Gemessen ueber `git ls-files`: **`UFT_UNIT_TESTS`
 * wird im ganzen Baum nirgends definiert** — der Block wird nie
 * uebersetzt und nie ausgefuehrt. Dieselbe Klasse wie die
 * `assert()`-unter-NDEBUG-Tests aus MF-830. Ein Test dort waere ein
 * Test, der nicht laeuft.
 */
#include "uft/recovery/uft_multiread_pipeline.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

#define N 8u
static const uint8_t PASS_A[N] = { 0x01, 0xFF, 0x02, 0xFF, 0x03, 0xFF, 0x04, 0xFF };
static const uint8_t PASS_B[N] = { 0xFF, 0x01, 0xFF, 0x02, 0xFF, 0x03, 0xFF, 0x04 };

static void zeige(const char *name, const uint8_t *b)
{
    printf("\n      %-8s", name);
    for (unsigned i = 0; i < N; i++) printf(" %02X", b[i]);
}

TEST(zwei_crc_valide_lesungen_ergeben_keinen_dritten_inhalt)
{
    /* DER ROTBEWEIS. */
    multiread_config_t cfg = multiread_config_default();
    cfg.min_passes = 2;   /* Vorgabe ist 3; zwei Lesungen genuegen hier */
    multiread_ctx_t *ctx = multiread_create(&cfg);
    ASSERT(ctx != NULL);

    ASSERT(multiread_add_pass(ctx, PASS_A, N, 100, true) == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, PASS_B, N, 100, true) == MULTIREAD_OK);

    uint8_t out[N];
    memset(out, 0, sizeof out);
    multiread_sector_t res;
    memset(&res, 0, sizeof res);
    ASSERT(multiread_execute(ctx, out, N, &res) == MULTIREAD_OK);

    /* Die Einstufung muss den Fall benennen — das tat sie schon vorher. */
    ASSERT(res.class_ == MULTIREAD_CLASS_AMBIGUOUS_GOOD);
    ASSERT(res.distinct_contents == 2);

    /* Und die AUSGABE muss einer der beiden BEOBACHTETEN Lesungen
     * entsprechen. Vorher kam hier 01 01 02 02 03 03 04 04 heraus. */
    bool ist_a = (memcmp(out, PASS_A, N) == 0);
    bool ist_b = (memcmp(out, PASS_B, N) == 0);
    if (!ist_a && !ist_b) {
        zeige("Pass A", PASS_A);
        zeige("Pass B", PASS_B);
        zeige("Ausgabe", out);
        printf("\n      -> die Ausgabe ist WEDER A NOCH B\n      ");
        _fail++;
        free(res.weak_mask);
        multiread_destroy(ctx);
        return;
    }

    free(res.weak_mask);
    multiread_destroy(ctx);
}

TEST(einigkeit_bleibt_wie_bisher)
{
    /* Gegenprobe 1: sind sich alle Lesungen einig, aendert sich nichts.
     * Ohne diesen Fall koennte der Fix schlicht immer den ersten Pass
     * durchreichen und der Rotbeweis waere trotzdem gruen. */
    multiread_config_t cfg = multiread_config_default();
    cfg.min_passes = 2;   /* Vorgabe ist 3; zwei Lesungen genuegen hier */
    multiread_ctx_t *ctx = multiread_create(&cfg);
    ASSERT(ctx != NULL);
    ASSERT(multiread_add_pass(ctx, PASS_A, N, 100, true) == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, PASS_A, N, 100, true) == MULTIREAD_OK);

    uint8_t out[N];
    multiread_sector_t res;
    memset(&res, 0, sizeof res);
    ASSERT(multiread_execute(ctx, out, N, &res) == MULTIREAD_OK);
    ASSERT(res.class_ == MULTIREAD_CLASS_STABLE_GOOD);
    ASSERT(memcmp(out, PASS_A, N) == 0);

    free(res.weak_mask);
    multiread_destroy(ctx);
}

TEST(ohne_gueltige_crc_bleibt_das_byte_voting)
{
    /* Gegenprobe 2: liegt KEINE CRC-valide Lesung vor, ist die byteweise
     * Mehrheit weiterhin richtig — sie ist dann ausdruecklich ein
     * Schaetzwert und ueber `has_weak_bits` als solcher gekennzeichnet.
     * Der Fix darf diesen Zweig NICHT anfassen.
     *
     * Drei Lesungen, die mittlere Stelle unterscheidet sich: die Mehrheit
     * traegt, und das Ergebnis ist hier zufaellig auch ein beobachteter
     * Inhalt — geprueft wird, dass die Klasse WEAK bleibt und das Voting
     * greift. */
    static const uint8_t C1[4] = { 0xAA, 0xBB, 0xCC, 0xDD };
    static const uint8_t C2[4] = { 0xAA, 0xBB, 0xCC, 0xDD };
    static const uint8_t C3[4] = { 0xAA, 0xBB, 0x11, 0xDD };

    multiread_config_t cfg = multiread_config_default();
    cfg.min_passes = 2;   /* Vorgabe ist 3; zwei Lesungen genuegen hier */
    multiread_ctx_t *ctx = multiread_create(&cfg);
    ASSERT(ctx != NULL);
    ASSERT(multiread_add_pass(ctx, C1, 4, 100, false) == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, C2, 4, 100, false) == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, C3, 4,  80, false) == MULTIREAD_OK);

    uint8_t out[4];
    multiread_sector_t res;
    memset(&res, 0, sizeof res);
    ASSERT(multiread_execute(ctx, out, 4, &res) == MULTIREAD_OK);
    ASSERT(res.class_ == MULTIREAD_CLASS_WEAK);
    ASSERT(out[2] == 0xCC);            /* Mehrheit, nicht 0x11 */
    ASSERT(res.has_weak_bits);         /* und ehrlich gekennzeichnet */

    free(res.weak_mask);
    multiread_destroy(ctx);
}

TEST(die_kennzahlen_bleiben_gefuellt)
{
    /* Gegenprobe 3: auch im AMBIGUOUS-Zweig muessen Konfidenz und
     * Weak-Maske weiter entstehen — sie kommen aus den Lesungen, nicht
     * aus der Ausgabe, und der Aufrufer braucht sie. */
    multiread_config_t cfg = multiread_config_default();
    cfg.min_passes = 2;   /* Vorgabe ist 3; zwei Lesungen genuegen hier */
    multiread_ctx_t *ctx = multiread_create(&cfg);
    ASSERT(ctx != NULL);
    ASSERT(multiread_add_pass(ctx, PASS_A, N, 100, true) == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, PASS_B, N, 100, true) == MULTIREAD_OK);

    uint8_t out[N];
    multiread_sector_t res;
    memset(&res, 0, sizeof res);
    ASSERT(multiread_execute(ctx, out, N, &res) == MULTIREAD_OK);

    ASSERT(res.total_reads == 2);
    ASSERT(res.good_reads  == 2);
    ASSERT(res.has_weak_bits);         /* alle acht Stellen weichen ab */
    ASSERT(res.data_len == N);

    free(res.weak_mask);
    multiread_destroy(ctx);
}

TEST(die_mehrheit_gewinnt_nicht_die_erste_lesung)
{
    /* MF-860, Verfeinerung von MF-845.
     *
     * MF-845 nahm „die erste ueberlebende Lesung". Bei DREI Lesungen,
     * von denen ZWEI uebereinstimmen, kann die erste die
     * Einzelgaengerin sein — dann setzt sich eine Minderheit durch.
     *
     * Referenz: a8rawconv `src/a8rawconv/disk.cpp:300-308` gruppiert
     * nach Inhalt und nimmt die haeufigste Gruppe („find the most
     * popular"). `best_sector` zeigt dabei immer auf ein wirklich
     * gelesenes Objekt.
     *
     * Aufbau: Pass A steht ALLEIN, Pass B kommt ZWEIMAL. Alle drei
     * bestehen ihre CRC. Erwartet wird B. */
    multiread_config_t cfg = multiread_config_default();
    cfg.min_passes = 2;
    multiread_ctx_t *ctx = multiread_create(&cfg);
    ASSERT(ctx != NULL);

    ASSERT(multiread_add_pass(ctx, PASS_A, N, 100, true) == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, PASS_B, N, 100, true) == MULTIREAD_OK);
    ASSERT(multiread_add_pass(ctx, PASS_B, N, 100, true) == MULTIREAD_OK);

    uint8_t out[N];
    memset(out, 0, sizeof out);
    multiread_sector_t res;
    memset(&res, 0, sizeof res);
    ASSERT(multiread_execute(ctx, out, N, &res) == MULTIREAD_OK);

    ASSERT(res.class_ == MULTIREAD_CLASS_AMBIGUOUS_GOOD);

    /* ZWEI verschiedene Inhalte, nicht drei. Die alte Zaehlung verglich
     * jede Lesung gegen den Bezug und kam bei A, B, B auf 3. */
    if (res.distinct_contents != 2) {
        printf("\n      distinct_contents = %u, erwartet 2\n"
               "      -> gezaehlt wird, wie viele vom Bezug abweichen,\n"
               "         nicht wie viele Inhalte es gibt\n      ",
               res.distinct_contents);
        _fail++;
    }

    if (memcmp(out, PASS_B, N) != 0) {
        zeige("Pass A", PASS_A);
        zeige("Pass B", PASS_B);
        zeige("Ausgabe", out);
        printf("\n      -> B kam ZWEIMAL vor, A einmal — die Mehrheit "
               "haette gewinnen muessen\n      ");
        _fail++;
    }

    free(res.weak_mask);
    multiread_destroy(ctx);
}

/* -- MF-884 -----------------------------------------------------------
 *
 * Die tragende Zusage, erstmals ausgesprochen und geprueft:
 *
 *     Meldet `multiread_execute()` `recovered`, dann ist `output` eine
 *     Bytefolge, die MINDESTENS EINE Lesung wirklich geliefert hat.
 *
 * Sie gilt heute, aber nicht weil jemand sie geschrieben haette -- sie
 * faellt aus zwei anderen Regeln heraus (siehe Dateikopf). Deshalb wird
 * sie nicht an einem Beispiel geprueft, sondern an einem Streifzug durch
 * den Eingaberaum: der Fall, der sie bricht, waere genau der, den ein
 * Beispiel nicht trifft.
 *
 * Gegenprobe von Hand (MF-884): faellt in `multiread_execute()` die
 * Bedingung `good_reads > 0` weg, meldet dieser Test binnen weniger
 * hundert Runden eine Verletzung. */
static uint32_t mf884_z = 2463534242u;
static uint32_t mf884_wurf(void)
{
    mf884_z ^= mf884_z << 13;
    mf884_z ^= mf884_z >> 17;
    mf884_z ^= mf884_z << 5;
    return mf884_z & 0x7FFFFFFFu;
}

TEST(wer_recovered_meldet_hat_wirklich_gelesen)
{
    enum { LEN = 12, MAXP = 5, RUNDEN = 20000 };
    uint8_t eingabe[MAXP][LEN];
    bool    crc[MAXP];

    long recovered_n = 0, fabrikat_n = 0, verletzt = 0;
    mf884_z = 2463534242u;                 /* fester Startwert */

    for (int runde = 0; runde < RUNDEN; runde++) {
        int anzahl = 2 + (int)(mf884_wurf() % (MAXP - 1));
        /* Kleines Alphabet, damit Uebereinstimmungen ueberhaupt
         * vorkommen -- mit 256 Werten waere jede Lesung einzigartig. */
        unsigned alphabet = 2 + mf884_wurf() % 3;
        for (int i = 0; i < anzahl; i++) {
            crc[i] = (mf884_wurf() % 100) < 40;
            for (int k = 0; k < LEN; k++)
                eingabe[i][k] = (uint8_t)(mf884_wurf() % alphabet);
        }

        multiread_config_t cfg = multiread_config_default();
        cfg.min_passes = 1;
        multiread_ctx_t *ctx = multiread_create(&cfg);
        ASSERT(ctx != NULL);
        for (int i = 0; i < anzahl; i++)
            ASSERT(multiread_add_pass(ctx, eingabe[i], LEN, 100, crc[i])
                   == MULTIREAD_OK);

        uint8_t out[LEN];
        multiread_sector_t res;
        memset(&res, 0, sizeof res);
        if (multiread_execute(ctx, out, LEN, &res) == MULTIREAD_OK) {
            bool beobachtet = false;
            for (int i = 0; i < anzahl; i++)
                if (memcmp(out, eingabe[i], LEN) == 0) {
                    beobachtet = true;
                    break;
                }
            if (res.recovered) recovered_n++;
            if (!beobachtet)   fabrikat_n++;

            if (res.recovered && !beobachtet) {
                if (verletzt == 0)
                    printf("\n      Runde %d: recovered=JA, aber die "
                           "Ausgabe stammt aus KEINER Lesung\n"
                           "      klasse=%s good_reads=%u conf=%u\n      ",
                           runde, multiread_class_name(res.class_),
                           res.good_reads, res.confidence);
                verletzt++;
            }
        }
        free(res.weak_mask);
        multiread_destroy(ctx);
    }

    /* Der Streifzug muss BEIDE Seiten wirklich erreicht haben -- sonst
     * waere ein gruener Lauf nur ein Lauf, der nichts geprueft hat. */
    if (recovered_n == 0 || fabrikat_n == 0) {
        printf("\n      Streifzug untauglich: recovered=%ld, "
               "Fabrikate=%ld -- beides muss vorkommen\n      ",
               recovered_n, fabrikat_n);
        _fail++;
        return;
    }

    if (verletzt != 0) {
        printf("      %ld Verletzung(en) in %d Runden "
               "(recovered=%ld, Fabrikate=%ld)\n      ",
               verletzt, RUNDEN, recovered_n, fabrikat_n);
        _fail++;
    }
}

int main(void)
{
    printf("=== Multi-Read: kein erfundenes Mischbyte (MF-845) ===\n");
    RUN(zwei_crc_valide_lesungen_ergeben_keinen_dritten_inhalt);
    RUN(einigkeit_bleibt_wie_bisher);
    RUN(ohne_gueltige_crc_bleibt_das_byte_voting);
    RUN(die_kennzahlen_bleiben_gefuellt);
    RUN(die_mehrheit_gewinnt_nicht_die_erste_lesung);
    RUN(wer_recovered_meldet_hat_wirklich_gelesen);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
