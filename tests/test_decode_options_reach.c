/**
 * @file test_decode_options_reach.c
 * @brief Ein Regler wirkt, oder er sagt warum nicht — nie beides nicht (MF-668)
 *
 * Stufe 5 aus `docs/plans/VARIANTEN_UND_FAEHIGKEITEN.md` wollte 38 tote
 * Bedienelemente „verdrahten, ausblenden oder löschen". Die Messung hat
 * die Aufgabe umgeschrieben.
 *
 * ── Was gemessen wurde ───────────────────────────────────────────────────
 *
 * 1. Die Oberfläche baut **nirgends** ein `flux_decoder_options_t`. Nur
 *    vier Kerndateien tun das, intern. Es gab keinen Parameterweg von
 *    einem Dialog zum Dekoder — für kein einziges der 38 Elemente.
 *
 * 2. Von 34 geprüften Bedienelementen der drei „Advanced…"-Dialoge nennen
 *    19 einen Namen, den es im ganzen Baum als Feld nicht gibt:
 *    `preserveGaps`, `preserveSync`, `ignoreBadGCR`, `rawNibble`,
 *    `adaptiveGain`, `lockThreshold`, `weakBitWindow` und andere. Sie sind
 *    nicht unverdrahtet — sie sind erfunden.
 *
 * 3. Der eine Weg, den es gab (MF-480, `decode_cell_adjust_pct`), war
 *    **eine halbe Naht**: er stand in `uftc_convert_scp_to_adf_via_plugin`
 *    und fehlte in `uftc_convert_hfe_to_adf_via_plugin`. Dieselbe
 *    Aufzählungs-Falle wie sechsmal zuvor in diesem Baum — ein Verhalten
 *    an einer Stelle gepflegt, an der zweiten still vergessen.
 *
 * 4. Und die Reparatur, die dabei naheliegt, wäre falsch gewesen:
 *    `media_adjust_pct` wird nur dort gelesen, wo ZEITEN in Zellen
 *    umgerechnet werden. `flux_decode_amiga_bits()` — der Weg für ein
 *    HFE-Abbild — liest aus den Optionen nur die Sync-Muster. Ein HFE ist
 *    ein fertig getakteter Bitstrom; die Zellgrenzen sind beim
 *    Aufzeichnen gefallen. Der Regler ist dort nicht unverdrahtet,
 *    sondern **bedeutungslos**.
 *
 * ── Was dieser Test festhält ─────────────────────────────────────────────
 *
 * Drei Zustände sind möglich, und nur der dritte ist verboten:
 *
 *   * Der Wert gilt und wirkt        — Fluss-Quelle, Wert im Bereich.
 *   * Der Wert gilt nicht, und das wird GESAGT — Bitstrom-Quelle, oder
 *     ein Wert außerhalb 50…200.
 *   * Der Wert gilt nicht und keiner sagt es — genau der Zustand vor
 *     MF-668, und der, den dieser Test rot machen soll, falls er
 *     zurückkommt.
 *
 * Der Test läuft absichtlich über den **HFE**-Pfad, also den, der es
 * vorher nicht konnte. Über SCP wäre er von Anfang an grün gewesen und
 * hätte die Lücke zugedeckt. Das Abbild `gw_amigados.hfe` liegt im freien
 * Korpus, damit der Beweis auch in der CI feuert statt zu schweigen.
 *
 * ── Zurückgenommen ───────────────────────────────────────────────────────
 *
 * Ein zweiter Wert (Zeitgeber-Toleranz für den PLL-Regler) war gebaut und
 * ist wieder ausgebaut: `flux_decoder_options_t.tolerance` wird im ganzen
 * Baum dreimal geschrieben und **nirgends gelesen**. Ein Regler mit
 * Verkabelung und ohne Wirkung ist kein Fortschritt gegenüber einem
 * Regler ohne Verkabelung.
 */

#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_format_convert.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "."
#endif

static int fehler;

#define PRUEFE(bed, ...)                                                   \
    do { if (!(bed)) { printf("  FAIL "); printf(__VA_ARGS__);             \
                       printf("\n"); fehler++; } } while (0)

static uft_error_t wandle(const char *src, const char *dst,
                          double feineinsteller_pct,
                          uft_convert_result_t *res)
{
    uft_convert_options_t o;
    memset(&o, 0, sizeof(o));
    o.decode_cell_adjust_pct = feineinsteller_pct;
    /* HFE -> ADF ist LOSSY-DOCUMENTED: Zeitverhalten und Schwachbits gehen
     * verloren. Das Vorflug-Tor (UFT-A01) bricht ohne ausdrueckliche
     * Zustimmung ab — beim ersten Lauf dieses Tests tat es genau das, und
     * das war richtig. Hier wird zugestimmt, weil gemessen wird. */
    o.accept_data_loss = true;

    memset(res, 0, sizeof(*res));
    remove(dst);
    return uft_convert_file(src, dst, UFT_FORMAT_ADF, &o, res);
}

/* Sucht eine Warnung, die den Feineinsteller beim Namen nennt. */
static const char *warnung_zum_regler(const uft_convert_result_t *r)
{
    for (int i = 0; i < r->warning_count; i++)
        if (strstr(r->warnings[i], "Feineinsteller")) return r->warnings[i];
    return NULL;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (uft_register_all_formats() != UFT_OK) {
        printf("FEHLER: Registry\n");
        return 1;
    }
    printf("Dekoder-Regler: wirken oder sich erklaeren (MF-668)\n\n");

    char src[1024];
    snprintf(src, sizeof(src), "%s/gw_amigados.hfe", UFT_CORPUS_DIR);
    FILE *f = fopen(src, "rb");
    if (!f) {
        printf("FEHLER: %s fehlt.\n", src);
        printf("Diese Datei liegt im FREIEN Korpus und ist im Baum. Fehlt\n");
        printf("sie, ist der Baum kaputt, nicht der Test — ein\n");
        printf("uebersprungener Beweis beweist nichts.\n");
        return 1;
    }
    fclose(f);

    uft_convert_result_t r;
    const char *w;

    /* Grundlage: ohne Vorgabe laeuft die Wandlung und sagt nichts ueber
     * einen Regler, den niemand angefasst hat. */
    uft_error_t e = wandle(src, "uft_opt_null.adf", 0.0, &r);
    PRUEFE(e == UFT_OK, "Wandlung ohne Vorgabe scheiterte (rc=%d)", (int)e);
    PRUEFE(r.sectors_converted > 0,
           "Wandlung ohne Vorgabe lieferte 0 Sektoren — der Beweis liefe "
           "ueber einen toten Pfad und wuerde nichts messen");
    PRUEFE(warnung_zum_regler(&r) == NULL,
           "ohne Vorgabe darf keine Regler-Warnung erscheinen");
    printf("  ok   ohne Vorgabe: %d Sektoren, keine Regler-Warnung\n",
           r.sectors_converted);
    remove("uft_opt_null.adf");

    /* Der Kern: ein Wert INNERHALB der Grenzen, auf einer Bitstrom-Quelle.
     *
     * Vor MF-668 passierte hier nichts und wurde nichts gesagt. Das ist
     * die schlimmere Haelfte von beidem: der Benutzer glaubt, seine Zahl
     * habe gewirkt. Jetzt muss die Wandlung durchlaufen UND erklaeren. */
    e = wandle(src, "uft_opt_gueltig.adf", 150.0, &r);
    PRUEFE(e == UFT_OK, "Wandlung mit 150%% scheiterte (rc=%d)", (int)e);
    w = warnung_zum_regler(&r);
    PRUEFE(w != NULL,
           "150%% ist ein GUELTIGER Wert, der auf einem getakteten Bitstrom "
           "nichts bewirken kann — das muss gesagt werden, nicht "
           "verschwiegen");
    if (w) {
        PRUEFE(strstr(w, "Bitstrom") != NULL,
               "die Warnung muss den Grund nennen (Bitstrom), nicht nur "
               "dass etwas nicht ging: '%s'", w);
        printf("  ok   150%% auf Bitstrom -> erklaert\n");
        printf("       \"%s\"\n", w);
    }
    remove("uft_opt_gueltig.adf");

    /* Ein Wert AUSSERHALB der Grenzen. Auch hier gilt: sagen, nicht
     * schweigen. Auf dem Bitstrom-Pfad greift der Bitstrom-Grund zuerst —
     * beides sind Gruende, keiner davon ist Stille. */
    e = wandle(src, "uft_opt_wild.adf", 900.0, &r);
    PRUEFE(e == UFT_OK, "Wandlung mit 900%% scheiterte (rc=%d)", (int)e);
    w = warnung_zum_regler(&r);
    PRUEFE(w != NULL, "900%% darf nicht stillschweigend verschwinden");
    /* Die Erfolgsmeldung gehoert HINTER die Pruefung, nicht neben sie:
     * beim Sabotage-Gegencheck stand sonst "ok" direkt unter "FAIL". */
    if (w) printf("  ok   900%% -> abgelehnt und gemeldet\n");
    remove("uft_opt_wild.adf");

    /* Und das Ergebnis bleibt unversehrt: eine nicht angewandte Vorgabe
     * darf die Daten nicht anfassen. Sonst waere die Warnung ehrlich und
     * das Abbild trotzdem verdorben. */
    uft_convert_result_t ohne, mit;
    wandle(src, "uft_opt_a.adf", 0.0,   &ohne);
    wandle(src, "uft_opt_b.adf", 150.0, &mit);
    PRUEFE(ohne.sectors_converted == mit.sectors_converted,
           "eine NICHT angewandte Vorgabe hat das Ergebnis veraendert "
           "(%d vs %d Sektoren) — dann war sie doch angewandt",
           ohne.sectors_converted, mit.sectors_converted);
    printf("  ok   nicht angewandt heisst auch: nichts veraendert (%d = %d)\n",
           ohne.sectors_converted, mit.sectors_converted);
    remove("uft_opt_a.adf");
    remove("uft_opt_b.adf");

    printf("\n%s (%d Abweichungen)\n",
           fehler ? "FEHLGESCHLAGEN" : "OK", fehler);
    return fehler ? 1 : 0;
}
