/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_rev_ausrichtung.c
 * @brief Ohne Ausrichtung meldet der Umdrehungsvergleich alles (MF-950)
 *
 * ── Die Messung, die diesen Test veranlasst hat ──────────────────────
 *
 * MF-949 hat den Umdrehungsvergleich `uft_fuse_revolutions()` gemessen
 * und repariert. Die naechste Frage war, ihn zu speisen: SCP-Dateien
 * tragen Umdrehungen von Haus aus, also ohne Hardware (P3-237).
 *
 * Die Messung am **echten** Abzug `tests/corpus/gw_amigados.scp` — eine
 * Aufnahme von `gw`, also fremder Hand — ergab:
 *
 *     verglichen            101 343 Bits
 *     Umdrehungen uneinig   101 051   (99,71 %)
 *     mittlere Konfidenz    0,5014
 *
 * 99,71 % schwache Bits. Das waere ein Medium, von dem nichts zu retten
 * ist. Die Gegenprobe zeigte etwas anderes:
 *
 *     Fluss-Rohdaten: 50 525 von 50 526 Intervallen GLEICH
 *     bester Bitversatz +1: 0,00 % Abweichung ueber 20 000 Bits
 *
 * Die beiden Umdrehungen tragen **dasselbe Signal**. Sie sind um EIN
 * Bit gegeneinander verschoben, weil der Index-Splice das erste
 * Intervall anders teilt (1975 statt 3950 ns). Ein stellenweiser
 * Vergleich misst dann den Versatz, nicht das Medium.
 *
 * ── Warum das der gefaehrlichste Befund dieser Reihe ist ─────────────
 *
 * Er ist STILL und er ist GROSS. Ein Werkzeug, das 99,71 % einer
 * gesunden Spur als schwach meldet, sagt nichts Falsches ueber ein Bit
 * — es sagt etwas Falsches ueber die ganze Diskette, und zwar
 * zuversichtlich. Genau davor steht „Keine erfundenen Daten".
 *
 * Der Bericht (57. Durchgang), der den Vergleich vorschlug, hat die
 * Ausrichtung nicht erwaehnt; sein Entwurf
 * `uft_detect_weak_bits_cross_revolution(decoded_bits_per_revolution,
 * ...)` vergleicht ebenfalls stellenweise. Das Prinzip stimmt, die
 * Voraussetzung fehlte.
 *
 * ── Was hier geprueft wird ───────────────────────────────────────────
 *
 * 1. ein bekannter Versatz wird gefunden          (synthetisch)
 * 2. bei gleichem Inhalt ist der Versatz 0        (Gegenprobe)
 * 3. unaehnliche Stroeme werden ABGELEHNT, nicht mit dem am wenigsten
 *    schlechten Versatz beantwortet
 * 4. die ausgerichtete Fusion meldet auf identischem Inhalt nichts
 * 5. echte schwache Bits ueberleben die Ausrichtung
 * 6. am ECHTEN Abzug: 99,71 % -> nahe null      (ohne Korpus: Skip)
 */
#include "uft/algorithms/uft_multi_rev_fusion.h"
#include "uft/flux/uft_scp_parser.h"
#include "uft/flux/uft_flux_decoder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ctest wertet 77 als „uebersprungen" (SKIP_RETURN_CODE, MF-598). */
#define SKIP_CODE 77

#ifndef UFT_CORPUS_RESTRICTED_DIR
#define UFT_CORPUS_RESTRICTED_DIR "tests/corpus"
#endif
#define KORPUS_SCP UFT_CORPUS_RESTRICTED_DIR "/gw_amigados.scp"

static int _pass = 0, _fail = 0, _last = 0, _skip = 0;
#define RUN(n)  do { printf("  [TEST] %-46s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                       _fail++; return; } } while (0)

#define BITS   4096u
#define BYTES  (BITS / 8u)

static inline int bit_holen(const uint8_t *b, size_t p)
{
    return (b[p / 8] >> (7 - (p % 8))) & 1;
}

static inline void bit_setzen(uint8_t *b, size_t p, int w)
{
    if (w) b[p / 8] |=  (uint8_t)(1u << (7 - (p % 8)));
    else   b[p / 8] &= (uint8_t)~(1u << (7 - (p % 8)));
}

/** Deterministischer Musterstrom — kein rand(), damit der Test wiederholbar ist. */
static void muster_bauen(uint8_t *b, size_t bits, uint32_t saat)
{
    uint32_t z = saat ? saat : 1u;
    for (size_t i = 0; i < bits; i++) {
        z ^= z << 13; z ^= z >> 17; z ^= z << 5;   /* xorshift32 */
        bit_setzen(b, i, (int)(z & 1u));
    }
}

/** Kopiert @p src um @p d Bits verschoben nach @p dst. */
static void verschieben(const uint8_t *src, size_t bits, long d, uint8_t *dst)
{
    memset(dst, 0, (bits + 7) / 8);
    for (size_t i = 0; i < bits; i++) {
        long j = (long)i - d;
        if (j >= 0 && (size_t)j < bits)
            bit_setzen(dst, i, bit_holen(src, (size_t)j));
    }
}

TEST(ein_bekannter_versatz_wird_gefunden)
{
    /* DER ROTBEWEIS in seiner kleinsten Form. */
    static const long VERSATZ[] = { +1, -1, +7, -13 };

    uint8_t a[BYTES], b[BYTES];
    muster_bauen(a, BITS, 0xC0FFEEu);

    for (size_t k = 0; k < sizeof VERSATZ / sizeof *VERSATZ; k++) {
        verschieben(a, BITS, VERSATZ[k], b);

        uft_rev_ausrichtung_t aus;
        memset(&aus, 0, sizeof aus);
        /* Der Vergleichsbereich muss den Rand meiden — dort steht beim
         * Verschieben Fuellung, kein Inhalt. */
        bool ok = uft_revolutionen_ausrichten(a, BITS, b, BITS, 32, &aus);

        if (!ok || aus.versatz != VERSATZ[k]) {
            printf("\n      Versatz %+ld: gefunden %+ld (ok=%d, "
                   "Abweichung %.4f)\n      ",
                   VERSATZ[k], aus.versatz, (int)ok, aus.abweichung);
            _fail++;
            return;
        }
        if (!aus.verlaesslich) {
            printf("\n      Versatz %+ld gefunden, aber als unverlaesslich "
                   "gemeldet (Abweichung %.4f)\n      ",
                   VERSATZ[k], aus.abweichung);
            _fail++;
            return;
        }
    }
}

TEST(gleicher_inhalt_ergibt_versatz_null)
{
    /* Gegenprobe: eine Ausrichtung, die IMMER etwas verschiebt, waere
     * genauso falsch wie gar keine. */
    uint8_t a[BYTES];
    muster_bauen(a, BITS, 0x1234u);

    uft_rev_ausrichtung_t aus;
    memset(&aus, 0, sizeof aus);
    ASSERT(uft_revolutionen_ausrichten(a, BITS, a, BITS, 32, &aus) == true);

    if (aus.versatz != 0 || aus.abweichung > 0.0001) {
        printf("\n      Versatz %+ld, Abweichung %.4f — erwartet 0 / 0\n"
               "      ", aus.versatz, aus.abweichung);
        _fail++;
    }
}

TEST(unaehnliche_stroeme_werden_abgelehnt)
{
    /* DER WICHTIGSTE FALL. Zwei unabhaengige Muster haben keinen
     * gemeinsamen Versatz. Eine Ausrichtung, die trotzdem den am
     * wenigsten schlechten liefert, waere eine erfundene Aussage — und
     * die nachfolgende Fusion baute darauf auf.
     *
     * Bei zufaelligen Stroemen liegt die Abweichung bei rund 50 %,
     * unabhaengig vom Versatz. */
    uint8_t a[BYTES], b[BYTES];
    muster_bauen(a, BITS, 0xAAAA1111u);
    muster_bauen(b, BITS, 0x5555EEEEu);

    uft_rev_ausrichtung_t aus;
    memset(&aus, 0, sizeof aus);
    bool ok = uft_revolutionen_ausrichten(a, BITS, b, BITS, 32, &aus);

    if (ok && aus.verlaesslich) {
        printf("\n      zwei unabhaengige Stroeme wurden mit Versatz %+ld "
               "ausgerichtet (Abweichung %.4f)\n      ",
               aus.versatz, aus.abweichung);
        _fail++;
    }
    /* Die gemessene Abweichung muss trotzdem berichtet werden — sie ist
     * der Grund fuer die Ablehnung und gehoert dem Aufrufer. */
    if (aus.abweichung < 0.30 || aus.abweichung > 0.70) {
        printf("\n      Abweichung %.4f — bei unabhaengigen Stroemen "
               "sind rund 0,5 zu erwarten\n      ", aus.abweichung);
        _fail++;
    }
}

/**
 * Baut eine Spur, die vorne eine periodische Luecke traegt und ihren
 * unterscheidenden Inhalt erst ab @p unique_ab.
 *
 * Das ist der Normalfall einer echten Spur, nicht ein Sonderfall:
 * Gap-Bytes sind periodisch und machen den groessten Teil aus, die
 * Sektorkoepfe stehen dazwischen.
 */
static void spur_mit_luecke_bauen(uint8_t *b, size_t bits,
                                  size_t unique_ab, uint32_t saat)
{
    uint32_t z = saat ? saat : 1u;
    for (size_t i = 0; i < bits; i++) {
        if (i < unique_ab) {
            bit_setzen(b, i, (int)(i & 1u));          /* 0101… */
        } else {
            z ^= z << 13; z ^= z >> 17; z ^= z << 5;
            bit_setzen(b, i, (int)(z & 1u));
        }
    }
}

TEST(der_inhalt_darf_ueberall_auf_der_spur_liegen)
{
    /* Diesen Fall hatte die MUTATIONSPROBE nicht — und deshalb blieb
     * „Fenster an den Rand legen" gruen.
     *
     * Bis MF-953 verglich die Ausrichtung ein Fenster aus der MITTE
     * (12,5 % bis 62,5 %). Liegt die unterscheidende Stelle dahinter,
     * sieht sie nur gleichfoermiges Muster, und dort passen viele
     * Versaetze gleich gut. Gemessen:
     *
     *     Inhalt in der Mitte        Versatz +2  richtig
     *     Inhalt im letzten Viertel  Versatz +0  FALSCH
     *     Inhalt im letzten Achtel   Versatz +0  FALSCH
     *
     * Und zwar mit Abweichung 0,0000 — also als VERLAESSLICH gemeldet.
     * Ein zuversichtlich falscher Versatz geht in die Fusion und erzeugt
     * dort erfundene Befunde (MF-950).
     *
     * Meine bisherigen Pruefmuster waren gleichmaessig zufaellig, also
     * ueberall unterscheidend — sie konnten den Fehler nicht sehen. */
    static const size_t AB[] = { 0u, BITS / 4u, BITS / 2u,
                                 BITS * 3u / 4u, BITS * 7u / 8u };

    for (size_t k = 0; k < sizeof AB / sizeof *AB; k++) {
        uint8_t a[BYTES], b[BYTES];
        spur_mit_luecke_bauen(a, BITS, AB[k], 0xC0FFEEu);
        verschieben(a, BITS, +2, b);

        uft_rev_ausrichtung_t aus;
        memset(&aus, 0, sizeof aus);
        bool ok = uft_revolutionen_ausrichten(a, BITS, b, BITS, 32, &aus);

        if (!ok || aus.versatz != +2) {
            printf("\n      Inhalt ab %zu von %u: Versatz %+ld statt +2 "
                   "(Abweichung %.4f, %s)\n      ",
                   AB[k], BITS, aus.versatz, aus.abweichung,
                   aus.verlaesslich ? "verlaesslich" : "unverlaesslich");
            _fail++;
            return;
        }
    }
}

TEST(auch_mit_luecke_am_ENDE_wird_ausgerichtet)
{
    /* Der Spiegelfall zu `der_inhalt_darf_ueberall_auf_der_spur_liegen`.
     * Die Mutationsprobe „nur das letzte Viertel vergleichen" blieb
     * gruen, weil dort ueberall Inhalt lag.
     *
     * Eine Spur kann ihren Inhalt aber auch VORNE tragen und hinten
     * eine Luecke — beides kommt vor, und der Vergleich darf sich auf
     * keine der beiden Seiten verlassen. */
    static const size_t BIS[] = { BITS, BITS * 3u / 4u, BITS / 2u,
                                  BITS / 4u, BITS / 8u };

    for (size_t k = 0; k < sizeof BIS / sizeof *BIS; k++) {
        uint8_t a[BYTES], b[BYTES];
        /* Inhalt bis BIS[k], danach gleichfoermiges 0101. */
        uint32_t z = 0xBEEF77u;
        for (size_t i = 0; i < BITS; i++) {
            if (i < BIS[k]) {
                z ^= z << 13; z ^= z >> 17; z ^= z << 5;
                bit_setzen(a, i, (int)(z & 1u));
            } else {
                bit_setzen(a, i, (int)(i & 1u));
            }
        }
        verschieben(a, BITS, +2, b);

        uft_rev_ausrichtung_t aus;
        memset(&aus, 0, sizeof aus);
        bool ok = uft_revolutionen_ausrichten(a, BITS, b, BITS, 32, &aus);

        if (!ok || aus.versatz != +2) {
            printf("\n      Inhalt bis %zu von %u: Versatz %+ld statt +2 "
                   "(Abweichung %.4f)\n      ",
                   BIS[k], BITS, aus.versatz, aus.abweichung);
            _fail++;
            return;
        }
    }
}

TEST(jeder_versatz_sieht_dieselbe_menge_bits)
{
    /* Die Rand-Einrueckung ist keine unbelegte Vorsorge, sondern eine
     * ZUSICHERUNG — und hier steht sie als solche.
     *
     * Verglichen wird von `max_versatz` bis `kurz - max_versatz`. Fuer
     * jeden Versatz in diesem Bereich liegt jede Position beider Stroeme
     * im gueltigen Bereich, also vergleicht JEDER Versatz exakt
     * `kurz - 2*max_versatz` Bits. Kein Versatz kann gewinnen, weil er
     * weniger Beweis sehen musste.
     *
     * GEMESSEN, und das gehoert dazu: ohne die Einrueckung schlug in 120
     * Faellen (3 Laengen x 40 Saaten) KEINER fehl. Ihre Notwendigkeit
     * ist also nicht durch einen Fehlerfall belegt. Sie kostet nichts
     * (64 von 101 343 Bits) und kann — anders als das Mittelfenster,
     * das MF-953 entfernt hat — keine Beweisstelle ausschliessen.
     * Deshalb bleibt sie, und deshalb wird ihre Eigenschaft hier
     * geprueft statt behauptet. */
    static const long MAXV[] = { 8, 32, 64 };

    uint8_t a[BYTES], b[BYTES];
    muster_bauen(a, BITS, 0x99AA55u);
    verschieben(a, BITS, +3, b);

    for (size_t k = 0; k < sizeof MAXV / sizeof *MAXV; k++) {
        uft_rev_ausrichtung_t aus;
        memset(&aus, 0, sizeof aus);
        ASSERT(uft_revolutionen_ausrichten(a, BITS, b, BITS,
                                           MAXV[k], &aus) == true);

        const size_t soll = (size_t)BITS - 2u * (size_t)MAXV[k];
        if (aus.verglichen != soll) {
            printf("\n      max_versatz %ld: %zu Bits verglichen, "
                   "zugesagt %zu\n      ",
                   MAXV[k], aus.verglichen, soll);
            _fail++;
            return;
        }
    }
}

TEST(die_fusion_lehnt_unausrichtbare_umdrehungen_ab)
{
    /* Diesen Fall hat die MUTATIONSPROBE gefunden.
     *
     * `unaehnliche_stroeme_werden_abgelehnt` prueft nur die MESSUNG.
     * Ob die FUSION das Urteil auch beachtet, stand nirgends: die
     * Mutation, die `!aus.verlaesslich` aus der Bedingung strich, blieb
     * gruen.
     *
     * Das ist der Wachposten, auf den es ankommt. Ohne ihn vergleicht
     * die Fusion auf einer geratenen Ausrichtung — und liefert dann
     * genau die Befunde, die es nicht gibt (99,71 % auf einer gesunden
     * Spur, siehe Kopf). */
    uint8_t a[BYTES], b[BYTES];
    muster_bauen(a, BITS, 0x11112222u);
    muster_bauen(b, BITS, 0x33334444u);

    const uint8_t *revs[2] = { a, b };
    const size_t   len[2]  = { BITS, BITS };

    uft_fused_bitstream_t f;
    memset(&f, 0, sizeof f);
    if (uft_fuse_revolutions_ausgerichtet(revs, len, 2, 32, NULL, &f)) {
        printf("\n      zwei unabhaengige Stroeme wurden fusioniert: "
               "%zu von %zu uneinig\n"
               "      -> ein Befund auf geratener Ausrichtung\n      ",
               f.disagreement_count, f.bit_count);
        uft_fused_bitstream_free(&f);
        _fail++;
    }
}

TEST(ausgerichtet_meldet_identischer_inhalt_nichts)
{
    /* Der Fall aus der Korpus-Messung, synthetisch nachgestellt: gleicher
     * Inhalt, um ein Bit verschoben. Ohne Ausrichtung meldete das
     * beinahe jedes Bit als schwach. */
    uint8_t a[BYTES], b[BYTES];
    muster_bauen(a, BITS, 0xBEEF0001u);
    verschieben(a, BITS, +1, b);

    const uint8_t *revs[2] = { a, b };
    const size_t   len[2]  = { BITS, BITS };

    /* Erst OHNE Ausrichtung — das ist der Vorzustand. */
    uft_fused_bitstream_t roh;
    memset(&roh, 0, sizeof roh);
    ASSERT(uft_fuse_revolutions_laengen(revs, len, 2, NULL, &roh) == true);
    const double quote_roh =
        (double)roh.disagreement_count / (double)roh.bit_count;
    uft_fused_bitstream_free(&roh);

    /* Dann MIT. */
    uft_fused_bitstream_t f;
    memset(&f, 0, sizeof f);
    ASSERT(uft_fuse_revolutions_ausgerichtet(revs, len, 2, 32, NULL, &f)
           == true);
    const double quote = (double)f.disagreement_count / (double)f.bit_count;

    if (quote > 0.001) {
        printf("\n      ausgerichtet immer noch %.2f %% uneinig "
               "(unausgerichtet %.2f %%)\n      ",
               100.0 * quote, 100.0 * quote_roh);
        _fail++;
    }
    /* Und die Gegenrichtung: der Vorzustand MUSS schlecht gewesen sein,
     * sonst belegt dieser Test nichts. */
    if (quote_roh < 0.10) {
        printf("\n      unausgerichtet nur %.2f %% uneinig — der Test "
               "belegt dann nichts\n      ", 100.0 * quote_roh);
        _fail++;
    }
    uft_fused_bitstream_free(&f);
}

TEST(echte_schwache_bits_ueberleben_die_ausrichtung)
{
    /* Die Ausrichtung darf nicht ALLES wegerklaeren. Gleicher Inhalt,
     * um ein Bit verschoben, PLUS drei echte Widersprueche. */
    uint8_t a[BYTES], b[BYTES];
    muster_bauen(a, BITS, 0x0BADC0DEu);
    verschieben(a, BITS, +1, b);

    static const size_t STELLEN[3] = { 500, 1200, 3000 };
    for (int i = 0; i < 3; i++)
        bit_setzen(b, STELLEN[i], !bit_holen(b, STELLEN[i]));

    const uint8_t *revs[2] = { a, b };
    const size_t   len[2]  = { BITS, BITS };

    uft_fused_bitstream_t f;
    memset(&f, 0, sizeof f);
    ASSERT(uft_fuse_revolutions_ausgerichtet(revs, len, 2, 32, NULL, &f)
           == true);

    if (f.disagreement_count != 3) {
        printf("\n      %zu Widersprueche gemeldet, drei eingebaut\n      ",
               f.disagreement_count);
        _fail++;
    }
    uft_fused_bitstream_free(&f);
}

/* ── Der Fall, der zaehlt: eine ECHTE Aufnahme ───────────────────────
 *
 * Alles oben ist synthetisch — von mir gebaut, von mir gelesen. Der
 * Beleg, der traegt, kommt von fremder Hand: `gw_amigados.scp` ist eine
 * Aufnahme von `gw` (Greaseweazle), zwei Umdrehungen einer
 * AmigaDOS-Diskette.
 *
 * `tests/corpus/` ist ungetrackt (32 MB). Ohne Korpus ueberspringt sich
 * dieser Fall BENANNT — nicht stillschweigend. */

/** SCP-Intervalle (ns, 0 = Ueberlaufplatzhalter) -> absolute ns. */
static size_t intervalle_zu_absolut(const uint32_t *iv, uint32_t n,
                                    uint32_t *out, size_t max)
{
    uint64_t t = 0;
    size_t   k = 0;
    for (uint32_t i = 0; i < n && k < max; i++) {
        if (iv[i] == 0u) continue;          /* kein Uebergang hier */
        t += iv[i];
        if (t > 0xFFFFFFFFull) break;
        out[k++] = (uint32_t)t;
    }
    return k;
}

/** @return 0 gemessen, SKIP_CODE ohne Korpus, 1 bei Fehlschlag. */
static int echter_abzug(void)
{
    enum { MAX_BITS = 512u * 1024u, MAX_TR = 4u * 1024u * 1024u };

    FILE *pruef = fopen(KORPUS_SCP, "rb");
    if (!pruef) {
        printf("  [TEST] %-46s ... SKIP\n", "der_echte_abzug_faellt_von_99_auf_0");
        printf("      %s fehlt (tests/corpus/ ist ungetrackt).\n", KORPUS_SCP);
        printf("      Beschaffung: tests/corpus_manifest/manifest.json\n");
        return SKIP_CODE;
    }
    fclose(pruef);

    uft_scp_ctx_t ctx;
    memset(&ctx, 0, sizeof ctx);
    if (uft_scp_open(&ctx, KORPUS_SCP) != 0) return 1;

    uft_scp_track_data_t td;
    memset(&td, 0, sizeof td);
    if (uft_scp_read_track(&ctx, 0, &td) != 0 || !td.valid ||
        td.revolution_count < 2) {
        printf("  Spur 0 nicht mit zwei Umdrehungen lesbar\n");
        return 1;
    }

    uint32_t *tr = (uint32_t *)malloc(sizeof(uint32_t) * MAX_TR);
    uint8_t  *b[2] = { NULL, NULL };
    size_t    len[2] = { 0, 0 };
    if (!tr) return 1;

    for (int r = 0; r < 2; r++) {
        size_t anz = intervalle_zu_absolut(td.revolutions[r].flux_data,
                                           td.revolutions[r].flux_count,
                                           tr, MAX_TR);
        flux_raw_data_t f;
        memset(&f, 0, sizeof f);
        f.transitions      = tr;
        f.transition_count = (uint32_t)anz;
        f.sample_rate      = 1000000000u;      /* 1 Tick = 1 ns */

        b[r] = (uint8_t *)calloc(MAX_BITS / 8u + 1u, 1);
        size_t bc = MAX_BITS;
        flux_pll_t pll;
        flux_pll_init(&pll, FLUX_MFM_DD_BITCELL_NS);
        pll.use_pll = true;
        flux_to_bitstream(&f, b[r], &bc, FLUX_MFM_DD_BITCELL_NS, &pll);
        len[r] = bc;
    }

    const uint8_t *revs[2] = { b[0], b[1] };
    int fehler = 0;

    /* Der Vorzustand — er MUSS schlecht sein, sonst belegt der Fall nichts. */
    uft_fused_bitstream_t roh;
    memset(&roh, 0, sizeof roh);
    double quote_roh = 0.0;
    if (uft_fuse_revolutions_laengen(revs, len, 2, NULL, &roh)) {
        quote_roh = (double)roh.disagreement_count / (double)roh.bit_count;
        uft_fused_bitstream_free(&roh);
    }

    uft_fused_bitstream_t f;
    memset(&f, 0, sizeof f);
    double quote = 1.0;
    if (uft_fuse_revolutions_ausgerichtet(revs, len, 2, 64, NULL, &f)) {
        quote = (double)f.disagreement_count / (double)f.bit_count;
        uft_fused_bitstream_free(&f);
    } else {
        printf("      ausgerichtete Fusion lehnte den echten Abzug ab\n");
        fehler = 1;
    }

    printf("  [TEST] %-46s ... ", "der_echte_abzug_faellt_von_99_auf_0");
    if (quote_roh < 0.50) {
        printf("\n      unausgerichtet nur %.2f %% uneinig — gemessen waren "
               "99,71 %%\n      ", 100.0 * quote_roh);
        fehler = 1;
    }
    if (quote > 0.01) {
        printf("\n      ausgerichtet %.2f %% uneinig — gemessen war 0,00 %%\n"
               "      ", 100.0 * quote);
        fehler = 1;
    }
    if (fehler) {
        printf("FAIL\n");
    } else {
        printf("OK  (%.2f %% -> %.2f %%)\n",
               100.0 * quote_roh, 100.0 * quote);
    }

    free(b[0]); free(b[1]); free(tr);
    uft_scp_free_track(&td);
    return fehler;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Umdrehungen ausrichten, bevor man sie vergleicht (MF-950) ===\n");
    RUN(ein_bekannter_versatz_wird_gefunden);
    RUN(gleicher_inhalt_ergibt_versatz_null);
    RUN(unaehnliche_stroeme_werden_abgelehnt);
    RUN(der_inhalt_darf_ueberall_auf_der_spur_liegen);
    RUN(auch_mit_luecke_am_ENDE_wird_ausgerichtet);
    RUN(jeder_versatz_sieht_dieselbe_menge_bits);
    RUN(die_fusion_lehnt_unausrichtbare_umdrehungen_ab);
    RUN(ausgerichtet_meldet_identischer_inhalt_nichts);
    RUN(echte_schwache_bits_ueberleben_die_ausrichtung);

    const int korpus = echter_abzug();
    if (korpus == SKIP_CODE)      _skip++;
    else if (korpus != 0)         _fail++;
    else                          _pass++;

    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen, %d uebersprungen\n",
           _pass, _fail, _skip);

    /* Ohne Korpus: 77, damit ctest den Lauf BENANNT als uebersprungen
     * fuehrt statt ihn gruen zu melden. Die synthetischen Faelle sind
     * dann trotzdem gelaufen; ist einer davon rot, gewinnt der Fehler. */
    if (_fail) return 1;
    return (korpus == SKIP_CODE) ? SKIP_CODE : 0;
}
