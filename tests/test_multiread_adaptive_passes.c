/* SPDX-License-Identifier: MIT */
/**
 * @file test_multiread_adaptive_passes.c
 * @brief `adaptive_passes` aendert nichts — und der Bericht sagt es (MF-975)
 *
 * ── WORUM ES GEHT ────────────────────────────────────────────────────
 *
 * `multiread_config_t` hat drei Schalter, die der forensische Bericht
 * dem Benutzer auflistet. Gemessen in
 * `src/recovery/uft_multiread_pipeline.c`:
 *
 *     min_confidence     :712   echte Entscheidung
 *     detect_weak_bits   :604   echte Verzweigung
 *     adaptive_passes    :93 (Vorgabe), :907 (Berichtszeile) — SONST NICHTS
 *
 * Der Header verspricht „Increase passes on failure". Die Leseschleife
 * ist aber fest:
 *
 *     for (pass = 0; pass < max_passes; pass++) { ...
 *         if (successful_reads >= min_passes) break; }
 *
 * `adaptive_passes` kommt darin nicht vor. Es wird genau einmal
 * gelesen — um im Bericht als „Adaptive passes: yes" zu erscheinen.
 *
 * ── WARUM DAS SCHLIMMER IST ALS EIN TOTES FELD ───────────────────────
 *
 * Ein Feld, das niemand liest, ist Bestand. Dieses hier wird gelesen und
 * BERICHTET — in einem forensischen Bericht, also genau dem Dokument,
 * fuer das dieses Werkzeug existiert. Es sagt dem Benutzer eine
 * Faehigkeit zu, die es nicht gibt.
 *
 * Das Tor `scripts/audit_setting_wiring.py` kann diese Klasse
 * bauartbedingt nicht sehen: es misst, OB ein Feld gelesen wird, nicht
 * ob das Lesen etwas bewirkt. Seine Grenze steht in seinem Kopf.
 *
 * ── WAS HIER ZUGESICHERT WIRD ────────────────────────────────────────
 *
 *   a) Die Zahl der Leseversuche ist mit `adaptive_passes = true` und
 *      mit `false` GLEICH — gemessen an einem `read_callback`, der
 *      seine Aufrufe zaehlt und immer fehlschlaegt (der Fall, in dem
 *      „increase passes on failure" wirken muesste).
 *   b) Der Bericht behauptet keine Adaptivitaet mehr.
 *
 * (a) ist eine CHARAKTERISIERUNG, kein Wunsch: sie faellt an dem Tag,
 * an dem jemand die Adaptivitaet wirklich baut — und genau dann gehoert
 * auch die Berichtszeile zurueck. Der Weg dahin steht als offener Punkt
 * (`RetryPolicy`-Regel „seit letzter Aenderung", UFT-59).
 */

#include "uft/recovery/uft_multiread_pipeline.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-44s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                    _fail++; return; } } while (0)

/* Ein Leser, der IMMER fehlschlaegt und mitzaehlt. Genau der Fall, in
 * dem „increase passes on failure" etwas tun muesste. */
static int g_aufrufe;

static int leser_faellt_immer(void *ud, uint8_t track, uint8_t head,
                              uint8_t *data, size_t *len)
{
    (void)ud; (void)track; (void)head; (void)data;
    g_aufrufe++;
    *len = 0;
    return -1;
}

static int zaehle_versuche(bool adaptiv)
{
    multiread_config_t cfg = multiread_config_default();
    cfg.adaptive_passes = adaptiv;
    cfg.read_callback   = leser_faellt_immer;
    cfg.user_data       = NULL;

    multiread_ctx_t *ctx = multiread_create(&cfg);
    if (!ctx) return -1;

    g_aufrufe = 0;
    multiread_track_t erg;
    memset(&erg, 0, sizeof(erg));
    (void)multiread_track(ctx, 0, 0, &erg);

    multiread_destroy(ctx);
    return g_aufrufe;
}

TEST(adaptive_passes_aendert_die_versuchszahl_nicht)
{
    const int mit  = zaehle_versuche(true);
    const int ohne = zaehle_versuche(false);

    ASSERT(mit > 0);
    ASSERT(ohne > 0);

    if (mit != ohne)
        printf("\n       mit=%d, ohne=%d — die Einstellung WIRKT jetzt;"
               " dann gehoert die Berichtszeile zurueck\n", mit, ohne);
    ASSERT(mit == ohne);
}

/* Der Bericht darf keine Faehigkeit zusagen, die die Schleife nicht
 * hat. Diese Zusicherung faellt vor MF-975. */
TEST(der_bericht_behauptet_keine_adaptivitaet)
{
    multiread_config_t cfg = multiread_config_default();
    cfg.adaptive_passes = true;
    cfg.read_callback   = leser_faellt_immer;

    multiread_ctx_t *ctx = multiread_create(&cfg);
    ASSERT(ctx != NULL);

    multiread_track_t erg;
    memset(&erg, 0, sizeof(erg));
    (void)multiread_track(ctx, 0, 0, &erg);

    char *bericht = multiread_generate_report(ctx, &erg, 1);
    if (!bericht) { multiread_destroy(ctx); ASSERT(bericht != NULL); }

    const bool sagt_adaptiv = (strstr(bericht, "Adaptive passes: yes") != NULL);
    if (sagt_adaptiv)
        printf("\n       Bericht meldet \"Adaptive passes: yes\","
               " die Schleife ist aber fest\n");

    free(bericht);
    multiread_destroy(ctx);
    ASSERT(!sagt_adaptiv);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== multiread: adaptive_passes ist wirkungslos (MF-975) ===\n");
    RUN(adaptive_passes_aendert_die_versuchszahl_nicht);
    RUN(der_bericht_behauptet_keine_adaptivitaet);
    printf("\n%d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
