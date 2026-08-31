/**
 * @file test_probe_verdict_bands.c
 * @brief Drei falsche Sieger werden drei ehrliche Mehrdeutigkeiten (MF-729)
 *
 * ── Was falsch war ──────────────────────────────────────────────────────
 *
 * `uft_probe_ranking.tied` zaehlte Bewerber mit **derselben Zahl**.
 * Verschiedene Zahlen hiessen `tied = 1` — „eindeutig". Da die Zahlen
 * aber ohne gemeinsame Skala vergeben wurden (MF-728: 35 bis 85 fuer
 * dieselbe Erkenntnis „die Groesse passt"), behauptete das Wissen, das
 * keine Sonde hatte.
 *
 * Gemessen waren es drei falsche Sieger auf sehr gaengigen Groessen:
 *
 *     163840  PC 160K   -> TRD (82)   Spectrum TR-DOS schlaegt das PC-Abbild
 *     819200  Mac 800K  -> D81 (80)   Commodore schlaegt Macintosh
 *     368640  PC 360K   -> MSX (75)   MSX schlaegt das PC-Abbild
 *
 * Keines dieser Plugins hatte mehr erkannt als „die Groesse passt".
 *
 * ── Was jetzt gilt ──────────────────────────────────────────────────────
 *
 *     eindeutig  <=>  genau EIN Bewerber im Band des Gewinners
 *                     UND das Band ist mindestens „Struktur gelesen" (50)
 *
 * Alles andere ist **mehrdeutig**, und dann ist die Kandidatenliste die
 * Antwort — nicht der Sieger. Das verallgemeinert FMT-17 (MF-724) vom
 * Sonderfall `do`/`po` auf alle kopflosen Formate.
 *
 * Die Baender und die zwei Eichungen stehen im Kopf von
 * `include/uft/uft_format_plugin.h`.
 *
 * ── Was dieser Test NICHT prueft ────────────────────────────────────────
 *
 * Ob die Konfidenzen der einzelnen Plugins richtig sind. Das messen die
 * beiden Eichgeraete: `test_probe_confidence_on_zeros.c` (nichts ueber
 * 49 auf Nullen) und `test_probe_confidence_on_random.c` (wer 50..79
 * will, muss Zufall abweisen).
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

static const char *urteil_name(uft_probe_verdict_t v)
{
    switch (v) {
    case UFT_PROBE_VERDICT_EINDEUTIG:   return "eindeutig";
    case UFT_PROBE_VERDICT_MEHRDEUTIG:  return "MEHRDEUTIG";
    default:                            return "kein Anspruch";
    }
}

static const char *band_name(uft_probe_band_t b)
{
    switch (b) {
    case UFT_PROBE_BAND_MAGIC:  return "Merkmal";
    case UFT_PROBE_BAND_STRUCT: return "Struktur";
    case UFT_PROBE_BAND_SIZE:   return "Groesse";
    default:                    return "—";
    }
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Urteil statt Zahlenwettrennen (MF-729)\n\n");

    uft_error_t rc = uft_register_all_formats();
    PRUEFE(rc == UFT_OK && uft_registered_format_plugin_count() > 100,
           "die Registry ist nicht gefuellt (%zu) — dann misst dieser "
           "Test nichts (MF-447)",
           uft_registered_format_plugin_count());

    const size_t n = 4096;
    uint8_t *nullen = calloc(1, n);
    if (!nullen) { printf("kein Speicher\n"); return 2; }

    /* Die drei Faelle aus MF-728, plus zwei Gegenproben. */
    static const struct {
        size_t      groesse;
        const char *was;
        bool        soll_mehrdeutig;
    } FAELLE[] = {
        {  163840, "PC 160K   (war: TRD 82)",  true  },
        {  819200, "Mac 800K  (war: D81 80)",  true  },
        {  368640, "PC 360K   (war: MSX 75)",  true  },
        {  143360, "Apple 140K (do/po)",       true  },
        {   92160, "TI-99 90K (fuenffach)",    true  },
    };

    printf("  %-26s %-8s %-9s %-12s %s\n",
           "Groesse", "Sieger", "Konf/Band", "Urteil", "im Band");
    for (size_t i = 0; i < sizeof(FAELLE) / sizeof(FAELLE[0]); i++) {
        uft_probe_ranking_t r;
        memset(&r, 0, sizeof(r));
        (void)uft_probe_buffer_ranked(nullen, n, FAELLE[i].groesse, &r);

        printf("  %-26s %-8s %3d/%-5s %-12s %zu von %zu\n",
               FAELLE[i].was,
               r.winner ? r.winner->name : "—",
               r.confidence, band_name(r.band),
               urteil_name(r.verdict), r.band_claimants, r.claimants);

        if (FAELLE[i].soll_mehrdeutig) {
            PRUEFE(r.verdict == UFT_PROBE_VERDICT_MEHRDEUTIG,
                   "%s: Urteil ist '%s' statt MEHRDEUTIG. Auf einem "
                   "Nullpuffer kann keine Sonde etwas erkannt haben — "
                   "ein eindeutiges Urteil waere eine Behauptung ohne "
                   "Grundlage", FAELLE[i].was, urteil_name(r.verdict));
            PRUEFE(r.band <= UFT_PROBE_BAND_SIZE,
                   "%s: der Gewinner liegt im Band '%s' (Konfidenz %d) — "
                   "auf Nullen darf nichts ueber %d melden (Eichung 1)",
                   FAELLE[i].was, band_name(r.band), r.confidence,
                   UFT_PROBE_CONF_STRUCT_MIN - 1);
        }
    }

    /* ── Gegenprobe: ein echtes Erkennungsmerkmal MUSS eindeutig sein ─
     *
     * Ohne sie wuerde dieser Test auch dann gruen, wenn das Urteil
     * IMMER mehrdeutig lautet — und ein Erkenner, der nie etwas
     * erkennt, ist so falsch wie einer, der immer rät. */
    uint8_t woz[512];
    memset(woz, 0, sizeof(woz));
    memcpy(woz, "WOZ2", 4);
    woz[4] = 0xFF; woz[5] = 0x0A; woz[6] = 0x0D; woz[7] = 0x0A;
    uft_probe_ranking_t rw;
    memset(&rw, 0, sizeof(rw));
    (void)uft_probe_buffer_ranked(woz, sizeof(woz), sizeof(woz), &rw);
    printf("\n  %-26s %-8s %3d/%-5s %-12s %zu von %zu\n",
           "WOZ2-Kopf (Gegenprobe)",
           rw.winner ? rw.winner->name : "—",
           rw.confidence, band_name(rw.band),
           urteil_name(rw.verdict), rw.band_claimants, rw.claimants);
    PRUEFE(rw.winner && strcmp(rw.winner->name, "WOZ") == 0,
           "ein WOZ2-Kopf wird nicht mehr von WOZ gewonnen");
    PRUEFE(rw.verdict == UFT_PROBE_VERDICT_EINDEUTIG,
           "ein echtes Erkennungsmerkmal ergibt kein eindeutiges Urteil "
           "('%s') — dann urteilt die Rangfolge immer mehrdeutig und "
           "sagt damit gar nichts", urteil_name(rw.verdict));
    PRUEFE(rw.band == UFT_PROBE_BAND_MAGIC,
           "der WOZ-Kopf landet im Band '%s' statt 'Merkmal'",
           band_name(rw.band));

    free(nullen);

    printf("\n  Was die gruene Ampel heisst: wo nichts erkannt wurde, "
           "sagt das Urteil\n"
           "  MEHRDEUTIG — und wo ein Merkmal traf, sagt es eindeutig. "
           "Der Sieger ist\n"
           "  in beiden Faellen derselbe wie vorher; geaendert hat sich, "
           "was ueber ihn\n"
           "  behauptet wird.\n"
           "\n"
           "  Was sie NICHT heisst: dass die Kandidatenliste schon "
           "irgendwo ankommt.\n"
           "  Oberflaeche und Kommandozeile muessen sie noch anbieten "
           "(FMT-21).\n");

    printf("\n%s (%d Abweichungen)\n", fehler ? "ROT" : "GRUEN", fehler);
    return fehler ? 1 : 0;
}
