/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_schreibnaht_mfm.c
 * @brief P3-453 — Schreibnaehte in einem IBM-MFM-Bitstrom, aus den
 *        Lueckenwerten gemessen.
 *
 * WARUM DIESER TEST VOR DEM CODE STEHT
 * ------------------------------------
 * Gemessen (MF-1186, `P3-453`): der Baum hat DREI Schreibnaht-Stellen und
 * keine davon traegt fuer einen IBM-MFM-Bitstrom.
 *   · `uff_detect_splices()`            — Zeitluecke im FLUSS, 0 Aufrufer
 *   · `uft_deepread_detect_splice()`    — OTDR-Spuren, 0 Aufrufer
 *   · `G64_DIAG_SPLICE_DETECTED`        — ein ETIKETT, nichts setzt es
 * Beide echten Erkenner brauchen Fluss bzw. OTDR. Fuer den Bitstrom gibt
 * es nichts, und `uft_mfm_sector_t` traegt gemessen KEINE einzige
 * Bitposition — ein Aufrufer kann die Lueckengrenzen also nicht einmal
 * herleiten.
 *
 * WAS HIER GEMESSEN WIRD, UND WAS AUSDRUECKLICH NICHT
 * ---------------------------------------------------
 * Gemessen werden ZAHLEN: wie viele Woerter eine Luecke hat, welches ihr
 * haeufigstes ist, wie viele davon abweichen und WO. Das braucht keine
 * Schwelle.
 *
 * **Nicht** gemessen wird ein URTEIL ueber die Diskette. Die Zulieferung
 * `DiskImageTool-extrakt.zip` liefert dafuer `UFT_SPLICE_MIN_BAD 2`,
 * `UFT_SPLICE_WORDS 2`, `UFT_GAP_MAX_VALUES 8` und die Quoten 0 %/75 % —
 * und sagt ueber die letzten beiden selbst: „die Schwellen sind nicht an
 * einem echten Traeger kalibriert". Das ist S1, und die Folge ist, dass
 * KEINE dieser fuenf Zahlen in den Baum kommt (`P3-451`). Eine Quote, die
 * „sektorweise beschrieben" behauptet, waere eine Aussage ohne Quelle.
 *
 * DIE BENANNTE REFERENZ (EINFRIER-REGEL (a) + (c))
 * ------------------------------------------------
 * `include/uft/uft_mfm_encoder.h` — „IBM System 34 track layout", im Baum
 * seit 2026-04-18, zwei Produktivaufrufer. Seine dokumentierte Lage ist
 * die Quelle fuer jede Zahl hier:
 *
 *   Track prolog:  [Gap4a:80x0x4E] [Sync:12x0x00] [IAM: 3xC2 + FC]
 *                  [Gap1: 50x0x4E]
 *   Je Sektor:     [Sync: 12x0x00] [IDAM: 3xA1 + FE + CHRN + CRC16]
 *                  [Gap2: 22x0x4E + 12x0x00]
 *                  [DAM:  3xA1 + FB + data + CRC16] [Gap3: Nx0x4E]
 *   Track epilog:  [Gap4b: 0x4E bis Spurende]
 *
 * Der Erzeuger ist damit eine ZWEITE Hand gegenueber dem Messcode: die
 * Spur entsteht aus `uft_mfm_encode_track()`, gemessen wird in
 * `uft_mfm_decode_track()`. Ein selbst gebauter Bitstrom aus derselben
 * Funktion, die ihn deutet, waere der geschlossene Kreis von MF-1009
 * (`apridisk`) und MF-1028 (`qrst`).
 *
 * DIE ERWARTUNGEN STEHEN HIER ALS ERWARTUNG, NICHT ALS WISSEN
 * -----------------------------------------------------------
 * MFM ist kontextabhaengig: das Wort fuer 0x4E haengt vom letzten Bit
 * davor ab, also kann das ERSTE Wort eines Fuellbereichs von den
 * folgenden abweichen. Ob `distinct` einer gleichmaessigen Luecke 1 oder
 * 2 ist, sage ich deshalb NICHT voraus — der Test misst es und nagelt
 * den gemessenen Wert fest. Bei A-006 sind drei von vier meiner
 * Erwartungen gefallen; das war kein Zufall, sondern der Normalfall.
 */

#include "uft/uft_types.h"
#include "uft/uft_mfm_encoder.h"
#include "uft/flux/uft_mfm_sector_parser.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-48s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                   _fail++; return; } } while (0)

#define SPT     9u          /* Sektoren je Spur                          */
#define SECSZ   512u        /* 2^(N+7) mit N=2                           */
#define TRACKCAP 16384u     /* Byte Zellenstrom; DD-Lage passt darin      */

static uint8_t  g_cells[TRACKCAP];
static size_t   g_cell_bytes;
static uint8_t  g_pool[SPT * SECSZ * 2u];
static uft_mfm_sector_t g_sec[32];

/* Eine Spur, in EINEM Durchgang geschrieben — das ist der Vergleichsfall.
 * Je Sektor ein eigenes Muster, damit eine Verwechslung auffaellt. */
static size_t spur_bauen(void)
{
    static uint8_t payload[SPT][SECSZ];
    static uft_sector_t secs[SPT];
    unsigned s, i;

    for (s = 0; s < SPT; s++)
        for (i = 0; i < SECSZ; i++)
            payload[s][i] = (uint8_t)(s * 11u + i * 7u + (i >> 4));

    memset(secs, 0, sizeof(secs));
    for (s = 0; s < SPT; s++) {
        secs[s].id.cylinder  = 3;
        secs[s].id.head      = 0;
        secs[s].id.sector    = (uint8_t)(s + 1u);
        secs[s].id.size_code = 2;
        secs[s].data         = payload[s];
        secs[s].data_len     = SECSZ;
        secs[s].data_size    = SECSZ;
    }

    memset(g_cells, 0, sizeof(g_cells));
    {
        uft_mfm_encode_params_t p = UFT_MFM_PARAMS_DEFAULT_DD;
        g_cell_bytes = uft_mfm_encode_track(secs, SPT, 3, 0, &p,
                                            g_cells, sizeof(g_cells));
    }
    return g_cell_bytes;
}

static size_t spur_lesen(void)
{
    memset(g_sec, 0, sizeof(g_sec));
    memset(g_pool, 0, sizeof(g_pool));
    return uft_mfm_decode_track(g_cells, g_cell_bytes * 8u,
                                g_pool, sizeof(g_pool),
                                g_sec, 32u, NULL);
}

/* Ein 16-Bit-Wort an eine Bitstelle schreiben. MSB zuerst, wie der
 * Strom. Damit laesst sich eine Naht NACHSTELLEN, ohne den Messcode zu
 * befragen. */
static void wort_setzen(size_t bitpos, uint16_t w)
{
    unsigned i;
    for (i = 0; i < 16u; i++) {
        const size_t p = bitpos + i;
        const uint8_t bit = (uint8_t)((w >> (15u - i)) & 1u);
        const uint8_t maske = (uint8_t)(1u << (7u - (p & 7u)));
        if (bit) g_cells[p >> 3] |= maske;
        else     g_cells[p >> 3] = (uint8_t)(g_cells[p >> 3] & ~maske);
    }
}

static uint16_t wort_lesen(size_t bitpos)
{
    uint16_t w = 0u;
    unsigned i;
    for (i = 0; i < 16u; i++) {
        const size_t p = bitpos + i;
        w = (uint16_t)((w << 1) | ((g_cells[p >> 3] >> (7u - (p & 7u))) & 1u));
    }
    return w;
}

/* ── Die Lage ueberhaupt: gibt es Bitpositionen? ────────────────────────── */

TEST(der_leser_nennt_die_stellen_im_strom)
{
    size_t n, i;
    ASSERT(spur_bauen() > 0u);
    n = spur_lesen();
    ASSERT(n == SPT);

    for (i = 0; i < n; i++) {
        /* Die Marke liegt VOR den Daten, und beide liegen im Strom. */
        ASSERT(g_sec[i].id_sync_bit < g_sec[i].data_start_bit);
        ASSERT(g_sec[i].data_start_bit < g_cell_bytes * 8u);
        /* Und sie laufen vorwaerts: Sektor i+1 liegt hinter i. */
        if (i > 0u)
            ASSERT(g_sec[i].id_sync_bit > g_sec[i - 1u].data_start_bit);
    }
}

TEST(die_luecken_liegen_zwischen_marke_und_daten)
{
    size_t n, i;
    ASSERT(spur_bauen() > 0u);
    n = spur_lesen();
    ASSERT(n == SPT);

    for (i = 0; i < n; i++) {
        /* Gap 2 liegt hinter der ID-CRC und vor der DAM-Sync. */
        ASSERT(g_sec[i].gap2.start_bit < g_sec[i].gap2.end_bit);
        ASSERT(g_sec[i].gap2.end_bit <= g_sec[i].data_start_bit);
        ASSERT(g_sec[i].gap2.start_bit > g_sec[i].id_sync_bit);
        /* Die Vorlaufluecke endet an der eigenen Marke. */
        ASSERT(g_sec[i].lead_gap.end_bit == g_sec[i].id_sync_bit);
        ASSERT(g_sec[i].lead_gap.start_bit < g_sec[i].lead_gap.end_bit);
    }
}

/* ── Der Vergleichsfall: eine Spur aus EINEM Durchgang ──────────────────── */

TEST(eine_frisch_formatierte_spur_hat_keine_abweichung)
{
    size_t n, i;
    unsigned mit_abweichung = 0u, beurteilbar = 0u, taktzelle = 0u;

    ASSERT(spur_bauen() > 0u);
    n = spur_lesen();
    ASSERT(n == SPT);

    /* Gap 2 ist die Luecke, die VOLLSTAENDIG im Sektor liegt: laut
     * Encoder-Kopf 22x0x4E + 12x0x00. Die 12 Nullwoerter sind der
     * DAM-Vorlauf und gehoeren NICHT zum Fuellteil. */
    for (i = 0; i < n; i++) {
        const uft_mfm_gap_t *g = &g_sec[i].gap2;
        ASSERT(g->sync_nulls == 12u);      /* Quelle: Encoder-Kopf Z. 15 */
        ASSERT(g->words == 22u);           /* der Fuellteil, ohne Vorlauf */
        /* 0x9254 ist das MFM-Wort des Fuellbytes 0x4E — gemessen, und es
         * ist in JEDER Luecke das haeufigste. */
        ASSERT(g->dominant_word == 0x9254u);

        /* Die REGEL statt der Zahl: es gibt genau ein Wort mehr als das
         * dominante, wenn die Taktzelle es erklaert — und keines sonst. */
        ASSERT(g->distinct == 1u + (uint32_t)g->leading_clock_only);
        if (g->leading_clock_only) {
            /* Und die Erklaerung wird geprueft, nicht geglaubt: das
             * erste Wort unterscheidet sich NUR in Bit 15, und das letzte
             * Bit vor der Luecke ist eine 1. */
            const uint16_t w0 = wort_lesen(g->start_bit);
            ASSERT((uint16_t)(w0 ^ g->dominant_word) == 0x8000u);
            ASSERT(((g_cells[(g->start_bit - 1u) >> 3]
                     >> (7u - ((g->start_bit - 1u) & 7u))) & 1u) == 1u);
            taktzelle++;
        }
        if (g->distinct > 0u) {
            beurteilbar++;
            if (g->deviating > 0u) mit_abweichung++;
        }
    }
    ASSERT(beurteilbar == SPT);
    /* Der Punkt: eine in einem Durchgang geschriebene Spur weicht
     * NIRGENDS unerklaert ab. Faellt diese Zusage, ist entweder der
     * Messcode falsch oder der Encoder schreibt nicht, was sein Kopf
     * sagt. */
    ASSERT(mit_abweichung == 0u);
    /* Die gemessene Zahl, auf GENAU dieser Nutzlast: 5 der 9 CRCs enden
     * auf eine 1. Sie steht hier, damit eine Aenderung am Encoder
     * auffaellt — und sie steht ZULETZT, weil die Summe nichts ueber die
     * Verteilung sagt (MF-1026), die Regel oben dagegen schon. */
    printf("\n    Taktzellen-Faelle: %u von %u    ", taktzelle, (unsigned)n);
    ASSERT(taktzelle == 5u);
}

/* Die Vorlaufluecke bestaetigt die Encoder-Lage byteweise — von der
 * LESESEITE her, also mit der zweiten Hand. */
TEST(die_vorlaufluecke_bestaetigt_die_encoder_lage)
{
    size_t n, i;
    ASSERT(spur_bauen() > 0u);
    n = spur_lesen();
    ASSERT(n == SPT);

    /* Sektor 1: der Spurvorlauf. 80 (Gap4a) + 12 (Sync) + 4 (IAM: 3xC2
     * + FC) + 50 (Gap1) = 146 Woerter, dann die 12 Sync-Nullen vor der
     * IDAM. Jede Zahl aus dem Encoder-Kopf; die Summe ist gemessen. */
    ASSERT(g_sec[0].lead_gap.words == 146u);
    ASSERT(g_sec[0].lead_gap.sync_nulls == 12u);
    /* Und er ist KEIN gleichmaessiger Zwischenraum — er traegt die
     * IAM-Marke. Das gehoert gemeldet, nicht geglaettet (D6). */
    ASSERT(g_sec[0].lead_gap.distinct > 2u);
    ASSERT(g_sec[0].lead_gap.deviating > 0u);

    /* Sektoren 2..9: Gap 3 des Vorgaengers. DD-Vorgabe ist 40
     * (`UFT_MFM_PARAMS_DEFAULT_DD`), dazu 12 Sync-Nullen. */
    for (i = 1; i < n; i++) {
        ASSERT(g_sec[i].lead_gap.words == 40u);
        ASSERT(g_sec[i].lead_gap.sync_nulls == 12u);
        ASSERT(g_sec[i].lead_gap.dominant_word == 0x9254u);
        ASSERT(g_sec[i].lead_gap.deviating == 0u);
    }
}

/* ── Die Gegenrichtung: eine nachgestellte Naht, und NUR dort ───────────── */

TEST(eine_nachgestellte_naht_wird_genau_dort_gefunden)
{
    size_t n, i, ziel_bit;
    uint16_t fremd;
    unsigned getroffen = 0u, falsch_alarm = 0u;

    ASSERT(spur_bauen() > 0u);
    n = spur_lesen();
    ASSERT(n == SPT);

    /* Ein Wort aus dem DATENbereich von Sektor 0 — vom Encoder selbst
     * erzeugt, also ein zulaessiges MFM-Wort, und sicher nicht das
     * Fuellwort. Genau so sieht die Stelle aus, an der ein Laufwerk beim
     * Nachschreiben eines Sektors das Schreibtor abschaltet. */
    fremd = wort_lesen(g_sec[0].data_start_bit + 4u * 16u);
    ASSERT(fremd != g_sec[4].gap2.dominant_word);

    /* In die Mitte des Fuellteils von Sektor 5, damit die Grenzwoerter
     * selbst unberuehrt bleiben. */
    ziel_bit = g_sec[4].gap2.start_bit + 10u * 16u;
    wort_setzen(ziel_bit, fremd);

    n = spur_lesen();
    ASSERT(n == SPT);

    for (i = 0; i < n; i++) {
        const uft_mfm_gap_t *g = &g_sec[i].gap2;
        if (g->deviating == 0u) continue;
        if (i == 4u) {
            getroffen++;
            ASSERT(g->deviating == 1u);
            ASSERT(g->first_deviating_bit == ziel_bit);
            ASSERT(g->last_deviating_bit  == ziel_bit);
            /* Zwei Woerter plus, falls vorhanden, das durch die
             * Taktregel erklaerte erste — die Regel, nicht die Zahl. */
            ASSERT(g->distinct == 2u + (uint32_t)g->leading_clock_only);
        } else {
            printf("\n    Falschalarm in S%u (abw=%u) ",
                   (unsigned)(i + 1u), g->deviating);
            falsch_alarm++;
        }
    }
    /* Beides zusammen ist die Aussage, und die Summe allein waere sie
     * nicht (MF-1026): getroffen UND kein Falschalarm. */
    ASSERT(getroffen == 1u);
    ASSERT(falsch_alarm == 0u);
}

/* ── D6: „nicht beurteilbar" ist nicht „keine Naht" ─────────────────────── */

TEST(eine_unbeurteilbare_luecke_sagt_das_statt_null)
{
    size_t n, i, from;
    ASSERT(spur_bauen() > 0u);
    n = spur_lesen();
    ASSERT(n == SPT);

    /* Den GANZEN Fuellteil von Sektor 2 mit lauter verschiedenen Woertern
     * ueberschreiben — aus dem Datenbereich, also zulaessiges MFM. Ein
     * Bereich ohne haeufigstes Wort ist keine Luecke, und „0 Abweichungen"
     * waere dort die falsche Antwort. */
    from = g_sec[1].gap2.start_bit;
    for (i = 0; i < 22u; i++)
        wort_setzen(from + i * 16u,
                    wort_lesen(g_sec[0].data_start_bit + (8u + i) * 16u));

    n = spur_lesen();
    ASSERT(n == SPT);

    /* Viele verschiedene Woerter: `distinct` sagt es, und `deviating`
     * bleibt nicht bei 0 stehen, als waere alles in Ordnung. */
    ASSERT(g_sec[1].gap2.distinct > 2u);
    ASSERT(g_sec[1].gap2.deviating > 0u);
    /* Und die Nachbarn sind unberuehrt — die Messung greift nicht ueber. */
    ASSERT(g_sec[0].gap2.deviating == 0u);
    ASSERT(g_sec[2].gap2.deviating == 0u);
}

/* ── Anti-Tautologie: ohne Strom gibt es keine Zahlen ───────────────────── */

TEST(ohne_luecke_werden_keine_zahlen_erfunden)
{
    uint8_t klein[64];
    uft_mfm_sector_t s[2];
    size_t n;

    memset(klein, 0, sizeof(klein));
    memset(s, 0xFF, sizeof(s));
    n = uft_mfm_decode_track(klein, sizeof(klein) * 8u,
                             g_pool, sizeof(g_pool), s, 2u, NULL);
    /* Ein Nullpuffer traegt keine Sync-Marke, also keinen Sektor — und
     * damit auch keine Luecke. Der Punkt ist, dass hier nichts gemeldet
     * wird, was nicht dasteht. */
    ASSERT(n == 0u);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== P3-453: Schreibnaehte aus den Lueckenwerten (IBM MFM) ===\n");
    printf("--- Die Stellen im Strom ---\n");
    RUN(der_leser_nennt_die_stellen_im_strom);
    RUN(die_luecken_liegen_zwischen_marke_und_daten);
    printf("--- Der Vergleichsfall: ein Durchgang ---\n");
    RUN(eine_frisch_formatierte_spur_hat_keine_abweichung);
    RUN(die_vorlaufluecke_bestaetigt_die_encoder_lage);
    printf("--- Die Gegenrichtung: eine nachgestellte Naht ---\n");
    RUN(eine_nachgestellte_naht_wird_genau_dort_gefunden);
    printf("--- D6 und Anti-Tautologie ---\n");
    RUN(eine_unbeurteilbare_luecke_sagt_das_statt_null);
    RUN(ohne_luecke_werden_keine_zahlen_erfunden);
    printf("\nErgebnis: %d bestanden, %d gefallen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
