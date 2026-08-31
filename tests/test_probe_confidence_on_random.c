/**
 * @file test_probe_confidence_on_random.c
 * @brief Eichung 2 — wer Struktur behauptet, muss Zufall abweisen (MF-729)
 *
 * ── Die Regel ───────────────────────────────────────────────────────────
 *
 * Eichung 1 (`test_probe_confidence_on_zeros.c`) faengt Sonden, die auf
 * einem **Nullpuffer** mehr melden, als die Groesse hergibt. Sie faengt
 * aber nicht den naechsten Trick: eine Pruefung, die *irgendetwas* am
 * Inhalt ansieht und dabei so weit gefasst ist, dass sie fast immer
 * zustimmt.
 *
 * Der Anlass ist konkret. `trd_probe()` enthielt bis MF-729:
 *
 *     if (data[0x227] == 0x10)         *confidence = 92;   // echte Signatur
 *     else if (data[0x8E4] <= 128)     *confidence = 82;   // <-- dies
 *
 * Die zweite Bedingung trifft auf **die Haelfte aller Bytewerte** zu.
 * Sie liest den Inhalt — und sagt nichts. Auf Nullen faellt sie auf,
 * auf Zufall zur Haelfte nicht.
 *
 * Darum: **wer 50..79 beansprucht, muss Zufall abweisen.** Gemessen an
 * 100 zufaelligen Puffern je Groesse darf eine Sonde hoechstens **5 mal**
 * ins Band „Struktur gelesen" oder hoeher rutschen. Eine
 * Strukturpruefung, die Zufall zur Haelfte durchlaesst, ist keine — sie
 * ist eine Bereichspruefung mit gutem Namen.
 *
 * ── Warum 95 von 100 und nicht 100 von 100 ──────────────────────────────
 *
 * Ein echtes Erkennungsmerkmal kann zufaellig auftreten; bei vier festen
 * Bytes ist die Wahrscheinlichkeit rund 2^-32 je Puffer, bei zwei Bytes
 * schon 2^-16. Eine Sonde mit einem kurzen Magic darf also gelegentlich
 * anschlagen, ohne fehlerhaft zu sein. Fuenf Prozent ist die Schwelle,
 * ab der es kein Zufall mehr ist, sondern Bauart.
 *
 * ── Bestimmtheit ────────────────────────────────────────────────────────
 *
 * Der Zufall ist ein fester Generator mit festem Startwert. Derselbe
 * Lauf ergibt dieselben Puffer — ein Tor, das bei jedem Durchgang etwas
 * anderes misst, ist kein Tor.
 */

#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

uft_error_t uft_register_all_formats(void);

static int fehler = 0;

#define PRUEFE(bed, ...) do {                                            \
    if (!(bed)) { printf("  FAIL "); printf(__VA_ARGS__);                \
                  printf("\n"); fehler++; }                              \
} while (0)

#define LAEUFE      100u    /* Puffer je Groesse            */
#define ERLAUBT       5u    /* davon duerfen anschlagen     */
#define PUFFER    65536u    /* voller Sondenpuffer          */

/* xorshift64* — fester Generator, damit der Lauf wiederholbar ist. */
static uint64_t rng_state;
static uint64_t rng_next(void)
{
    uint64_t x = rng_state;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    rng_state = x;
    return x * 0x2545F4914F6CDD1Dull;
}

static const size_t GROESSEN[] = {
     92160u,  143360u,  163840u,  174848u,  184320u,  204800u,  232960u,
    327680u,  368640u,  409600u,  655360u,  737280u,  819200u,  901120u,
   1228800u, 1474560u,
};

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Eichung 2: wer Struktur behauptet, muss Zufall abweisen "
           "(MF-729)\n\n");

    uft_error_t rc = uft_register_all_formats();
    size_t anzahl = uft_registered_format_plugin_count();
    const size_t n_gr = sizeof(GROESSEN) / sizeof(GROESSEN[0]);
    printf("  Registry: rc=%d, %zu Plugins · %zu Groessen · %u Puffer je "
           "Groesse\n\n", rc, anzahl, n_gr, LAEUFE);
    PRUEFE(rc == UFT_OK && anzahl > 100,
           "die Registry ist nicht gefuellt (%zu) — dann misst dieser "
           "Test nichts (MF-447)", anzahl);

    uint8_t *buf = malloc(PUFFER);
    /* Ein Treffer je (Plugin, Groesse) waere zu fein aufgeloest; gezaehlt
     * wird je Plugin ueber alle Groessen, und die Schwelle skaliert
     * mit. */
    size_t *treffer = calloc(anzahl, sizeof(size_t));
    if (!buf || !treffer) { printf("kein Speicher\n"); return 2; }

    rng_state = 0x9E3779B97F4A7C15ull;   /* fester Startwert */

    for (size_t g = 0; g < n_gr; g++) {
        for (unsigned r = 0; r < LAEUFE; r++) {
            for (size_t o = 0; o + 8 <= PUFFER; o += 8) {
                uint64_t v = rng_next();
                memcpy(buf + o, &v, 8);
            }
            for (size_t i = 0; i < anzahl; i++) {
                const uft_format_plugin_t *p =
                    uft_registered_format_plugin_at(i);
                if (!p || !p->probe) continue;
                int k = -1;
                if (!p->probe(buf, PUFFER, GROESSEN[g], &k)) continue;
                if (k >= UFT_PROBE_CONF_STRUCT_MIN) treffer[i]++;
            }
        }
    }

    const size_t schwelle = (size_t)ERLAUBT * n_gr;
    size_t verstoesse = 0;
    for (size_t i = 0; i < anzahl; i++) {
        if (treffer[i] <= schwelle) continue;
        const uft_format_plugin_t *p = uft_registered_format_plugin_at(i);
        double q = 100.0 * (double)treffer[i] / (double)(LAEUFE * n_gr);
        printf("  VERSTOSS  %-10s %5zu von %5u Zufallspuffern "
               "(%.1f %%) im Band >= %d\n",
               p && p->name ? p->name : "?", treffer[i],
               (unsigned)(LAEUFE * n_gr), q, UFT_PROBE_CONF_STRUCT_MIN);
        verstoesse++;
    }

    free(buf);
    free(treffer);

    printf("\n  Schwelle: hoechstens %u %% der Puffer (= %zu von %u)\n",
           ERLAUBT, schwelle, (unsigned)(LAEUFE * n_gr));
    printf("  Verstoesse: %zu\n", verstoesse);

    PRUEFE(verstoesse == 0,
           "%zu Sonde(n) stimmen bei mehr als %u %% zufaelliger Puffer "
           "ins Band 'Struktur gelesen' zu. Eine Pruefung, die Zufall so "
           "oft durchlaesst, ist keine Strukturpruefung — sie ist eine "
           "Bereichspruefung mit gutem Namen (der trd-Fall aus MF-729)",
           verstoesse, ERLAUBT);

    printf("\n  Was die gruene Ampel heisst: keine Sonde erschleicht sich "
           "das Band\n"
           "  'Struktur gelesen' mit einer Bedingung, die auf Zufall "
           "meistens zutrifft.\n"
           "  Was sie NICHT heisst: dass die Strukturpruefungen RICHTIG "
           "sind — nur,\n"
           "  dass sie ueberhaupt trennen.\n");

    printf("\n%s (%d Abweichungen)\n", fehler ? "ROT" : "GRUEN", fehler);
    return fehler ? 1 : 0;
}
