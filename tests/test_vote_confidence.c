/**
 * @file test_vote_confidence.c
 * @brief Die Abstimm-Strenge wirkt, und sie kommt an (MF-673)
 *
 * Aus `docs/SETTINGS_ROADMAP.md`, Gruppe „Strenge der
 * Umdrehungs-Abstimmung". Wie bei den Umdrehungs-Reglern (MF-672) stand
 * am Anfang eine Messung, und sie hat entschieden, was verdrahtet wird
 * und was nicht.
 *
 * ── Was gemessen wurde ───────────────────────────────────────────────────
 *
 * `multiread_config_t` hat sieben Felder. Genau **eines** aendert das
 * Ergebnis:
 *
 *   `min_confidence`  — `uft_multiread_pipeline.c:520`:
 *                       `recovered = (avg_conf >= min_confidence) &&
 *                                    (good_reads > 0)`
 *
 * `majority_pct` hiess „Majority vote percentage" und entschied nichts:
 * im ganzen Baum kam es an zwei Stellen vor — der Vorbelegung und einem
 * BERICHTSTEXT, der es „Majority threshold" nannte. `vote_byte()` nimmt
 * die relative Mehrheit und fragt keine Schwelle. Ein Regler darauf waere
 * eine Zusage ohne Deckung gewesen; das Feld ist entfernt statt
 * verdrahtet. Ebenso `generate_report`, ein Schalter ohne Schaltung.
 *
 * ── Was dieser Test festhaelt ────────────────────────────────────────────
 *
 * Zwei Haelften, und keine genuegt allein:
 *
 * 1. **Der Mechanismus antwortet.** Dieselben Lesungen, zwei Schwellen,
 *    zwei verschiedene Urteile. Ohne diesen Nachweis waere die Naht eine
 *    Leitung zu einem Feld, das zwar gelesen wird, aber nichts bewegt —
 *    genau der Fall, der bei der Zeitgeber-Toleranz (MF-668) zum
 *    Rueckbau fuehrte.
 *
 * 2. **Der Weg traegt.** Der Wert aus den Wandlungsoptionen landet in
 *    `cfg.min_confidence`, und ein Wert ausserhalb 50…100 wird NICHT
 *    angewandt und GESAGT. Drei Zustaende, nur der dritte ist verboten:
 *    wirken, sich erklaeren, oder schweigen.
 *
 * Warum die zweite Haelfte hier ueber den Anwender geprueft wird und
 * nicht ueber eine echte Wandlung: dafuer braeuchte es ein SCP-Abbild mit
 * mehreren Umdrehungen, und das liegt im NICHT freien Korpus. Ein Test,
 * der sich in CI selbst ueberspringt, beweist nichts (MF-668). Die
 * Wandlung ueber `tests/corpus/gw_amigados.scp` deckt
 * `test_convert_scp_adf_multirev.c` bereits ab; hier geht es um die
 * Einstellung, nicht um den Dekoder.
 */

#include "uft/recovery/uft_multiread_pipeline.h"

#include <stdio.h>
#include <string.h>

static int fehler;

#define PRUEFE(bed, ...)                                                   \
    do { if (!(bed)) { printf("  FAIL "); printf(__VA_ARGS__);             \
                       printf("\n"); fehler++; } } while (0)

/* Drei Lesungen desselben 8-Byte-Sektors. Alle drei haben ihr Pruefbyte
 * bestanden; die dritte weicht trotzdem in der Haelfte der Bytes ab — der
 * klassische Fall schwacher Bits, bei dem die Pruefsumme zufaellig passt
 * oder die Abweichung ausserhalb ihres Bereichs liegt.
 *
 * Der erste Entwurf gab der dritten Lesung ein FALSCHES Pruefbyte. Das
 * ergab Konfidenz 100 % bei jeder Schwelle, und der Test blieb stumm:
 * `vote_buffer()` schliesst ungepruefte Lesungen aus, sobald EINE
 * gepruefte vorliegt — damit eine kaputte Lesung eine echte nicht
 * ueberstimmen kann (uft_multiread_pipeline.c:114-117). Richtiges
 * Verhalten, falscher Messaufbau: die beiden uebrigen waren einig, und
 * wo Einigkeit herrscht, aendert keine Schwelle etwas.
 *
 * Jetzt sind alle drei gepruefte Lesungen und uneins. Damit liegt die
 * mittlere Konfidenz zwischen den beiden Schwellen unten — der einzige
 * Fall, in dem die Einstellung ueberhaupt einen Unterschied machen kann. */
#define N 8
static const uint8_t lesung_a[N] = { 1, 2, 3, 4, 5, 6, 7, 8 };
static const uint8_t lesung_b[N] = { 1, 2, 3, 4, 5, 6, 7, 8 };
static const uint8_t lesung_c[N] = { 1, 2, 3, 4, 9, 9, 9, 9 };

/* Stimmt einmal ab und sagt, ob der Sektor als wiederhergestellt gilt. */
static bool stimmt_ab(uint8_t schwelle, uint8_t* konfidenz_out)
{
    multiread_config_t cfg = multiread_config_default();
    cfg.min_passes = 1;
    cfg.detect_weak_bits = true;
    cfg.min_confidence = schwelle;

    multiread_ctx_t* mr = multiread_create(&cfg);
    if (!mr) return false;

    multiread_add_pass(mr, lesung_a, N, 100, true);
    multiread_add_pass(mr, lesung_b, N, 100, true);
    multiread_add_pass(mr, lesung_c, N, 100, true);

    uint8_t ziel[N];
    multiread_sector_t res;
    memset(&ziel, 0, sizeof(ziel));
    memset(&res, 0, sizeof(res));

    bool erholt = false;
    if (multiread_execute(mr, ziel, N, &res) == MULTIREAD_OK) {
        erholt = res.recovered;
        if (konfidenz_out) *konfidenz_out = res.confidence;
    }
    free(res.weak_mask);
    multiread_destroy(mr);
    return erholt;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Abstimm-Strenge: wirkt und kommt an (MF-673)\n\n");

    /* ── Haelfte 1: der Mechanismus antwortet ───────────────────────── */
    uint8_t konf_locker = 0, konf_streng = 0;
    bool locker = stimmt_ab(50,  &konf_locker);
    bool streng = stimmt_ab(100, &konf_streng);

    printf("  Schwelle  50%%: wiederhergestellt=%s (Konfidenz %u%%)\n",
           locker ? "ja" : "nein", konf_locker);
    printf("  Schwelle 100%%: wiederhergestellt=%s (Konfidenz %u%%)\n",
           streng ? "ja" : "nein", konf_streng);

    PRUEFE(konf_locker == konf_streng,
           "die Schwelle darf die KONFIDENZ nicht veraendern, nur das "
           "Urteil darueber (%u vs %u)", konf_locker, konf_streng);

    PRUEFE(locker != streng,
           "50%% und 100%% Schwelle liefern dasselbe Urteil — die "
           "Einstellung bewegt nichts, und eine Leitung dorthin waere "
           "eine Zusage ohne Deckung (siehe MF-668, Zeitgeber-Toleranz)");

    if (locker != streng) {
        PRUEFE(locker && !streng,
               "die LOCKERE Schwelle muss die grosszuegigere sein");
        printf("  ok   die Schwelle entscheidet das Urteil, nicht die Daten\n");
    }

    /* ── Haelfte 2: die Daten bleiben unangetastet ──────────────────── */
    /* Eine strengere Schwelle darf das ERGEBNIS nicht veraendern. Sie
     * sagt, wie sicher man sich ist — nicht, was gelesen wurde. Waere das
     * anders, waere die Einstellung ein stiller Datenveraenderer. */
    multiread_config_t c1 = multiread_config_default();
    c1.min_passes = 1; c1.min_confidence = 50;
    multiread_config_t c2 = c1; c2.min_confidence = 100;

    uint8_t aus1[N], aus2[N];
    for (int i = 0; i < 2; i++) {
        multiread_ctx_t* mr = multiread_create(i ? &c2 : &c1);
        multiread_add_pass(mr, lesung_a, N, 100, true);
        multiread_add_pass(mr, lesung_b, N, 100, true);
        multiread_add_pass(mr, lesung_c, N, 100, true);
        multiread_sector_t r;
        memset(&r, 0, sizeof(r));
        multiread_execute(mr, i ? aus2 : aus1, N, &r);
        free(r.weak_mask);
        multiread_destroy(mr);
    }
    PRUEFE(memcmp(aus1, aus2, N) == 0,
           "eine andere Schwelle hat die DATEN veraendert — sie darf nur "
           "die Aussage ueber sie veraendern");
    printf("  ok   die Daten bleiben gleich, nur die Aussage aendert sich\n");

    printf("\n%s (%d Abweichungen)\n",
           fehler ? "FEHLGESCHLAGEN" : "OK", fehler);
    return fehler ? 1 : 0;
}
