/**
 * @file test_convert_table_has_dispatch.c
 * @brief Was die Wandlungstabelle anbietet, muss der Verteiler auch koennen
 *
 * ── Woher die Frage kommt ────────────────────────────────────────────────
 *
 * MF-565 fand einen Wandlungspfad, den nie ein Test angefasst hatte: er
 * lieferte eine leere Diskette und meldete Erfolg. Gefunden wurde er durch
 * Zufall, beim Bau eines Rotbeweises fuer etwas anderes.
 *
 * Die systematische Fassung derselben Frage lautet: **welche Paare bietet
 * das Werkzeug an, ohne sie zu koennen?** Drei Listen muessen dafuer
 * zusammenpassen, und sie liegen in drei Dateien:
 *
 *   `uft_format_convert_tables.c`    was ANGEBOTEN wird
 *   `uft_roundtrip.c`                welches URTEIL das Paar traegt
 *   `uft_format_convert_dispatch.c`  was tatsaechlich GERUFEN wird
 *
 * Faellt das dritte aus, kommt der Benutzer bis zum Verteiler durch und
 * bekommt dort „dispatch not yet implemented" — nachdem Tabelle und
 * Preflight ihm gesagt haben, der Weg sei gangbar.
 *
 * ── Warum das gemessen und nicht gelesen wird ────────────────────────────
 *
 * Der erste Anlauf war ein Skript, das den Verteiler nach
 * `src_format == X && dst_format == Y` durchsuchte. Es meldete **24 Paare
 * ohne Verteiler** — darunter `SCP->ADF` und `IMG->HFE`, die beide
 * nachweislich laufen und gemessene Rundlaeufe haben (MF-473, MF-539).
 *
 * Der Verteiler gruppiert naemlich:
 *
 *     if (src_format == UFT_FORMAT_SCP &&
 *         (dst_format == UFT_FORMAT_ADF || dst_format == UFT_FORMAT_IMG))
 *
 * Ein Regex ueber fremden Quelltext ist eine Vermutung. Dieser Test fragt
 * stattdessen den **laufenden Wandler**: fuer jedes Paar der Tabelle
 * einmal `uft_convert_memory()`, und dann wird die Antwort angesehen.
 *
 * ── Wie unterschieden wird ───────────────────────────────────────────────
 *
 * Die Eingabe ist absichtlich Unsinn — ein paar Byte Fuellung. Kein Paar
 * kann damit erfolgreich wandeln, und das ist der Punkt: es geht nicht um
 * das Ergebnis, sondern darum, WIE WEIT der Aufruf kommt.
 *
 *   Preflight weist ab        das Paar wird gar nicht erst angeboten —
 *                             in Ordnung, so ist es gedacht (MF-263)
 *   Formatfehler o.ae.        der Verteiler hat das Paar und ist am
 *                             Unsinn gescheitert — in Ordnung
 *   „dispatch not yet
 *    implemented"             Tabelle sagt ja, Verteiler sagt nein
 *                             -> genau der gesuchte Widerspruch
 *
 * Ein Paar, das in der Rundlauf-Matrix ein Urteil traegt (und damit vom
 * Preflight durchgelassen wird) UND beim Verteiler auflaeuft, ist der
 * schwerste Fall: dem Benutzer wurde zweimal zugesagt, dass es geht.
 */

#include "uft/uft_core.h"
#include "uft/uft_format_convert.h"
#include "uft/uft_format_plugin.h"
#include "uft/core/uft_roundtrip.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int failures;

/** Die Meldung, die der Rueckfall des Verteilers ausgibt. */
#define DISPATCH_MISSING  "dispatch not yet implemented"

static bool has_warning(const uft_convert_result_t *r, const char *needle)
{
    for (int i = 0; i < r->warning_count && i < 16; i++)
        if (strstr(r->warnings[i], needle)) return true;
    return false;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Angebot und Verteiler muessen zusammenpassen ===\n");

    /* Unsinn als Eingabe: es geht um die Reichweite des Aufrufs, nicht um
     * ein Ergebnis. Gross genug, dass ein Parser den Kopf lesen kann. */
    static uint8_t junk[4096];
    for (size_t i = 0; i < sizeof(junk); i++) junk[i] = (uint8_t)(i * 31 + 7);

    int offered = 0, dispatched = 0, refused = 0, missing = 0;
    int missing_with_verdict = 0;

    for (int s = 0; s < UFT_FORMAT_MAX; s++) {
        const uft_conversion_path_t *paths[64];
        int n = uft_convert_list_targets((uft_format_t)s, paths, 64);
        if (n <= 0) continue;

        for (int i = 0; i < n; i++) {
            uft_format_t src = paths[i]->source;
            uft_format_t dst = paths[i]->target;
            if (src == dst) continue;          /* Identitaet, eigener Fall */

            offered++;

            uint8_t *out = NULL;
            size_t out_size = 0;
            uft_convert_options_ext_t o;
            uft_convert_result_t r;
            memset(&o, 0, sizeof(o));
            memset(&r, 0, sizeof(r));
            /* Damit das Preflight-Tor nur an der UNGEPRUEFT-Schranke
             * abweist und nicht schon am Verlust-Einverstaendnis. */
            o.accept_data_loss = true;

            uft_convert_memory(junk, sizeof(junk), src, &out, &out_size,
                               dst, &o, &r);
            free(out);

            if (has_warning(&r, DISPATCH_MISSING)) {
                missing++;
                /* Kam der Aufruf ueberhaupt bis zum Verteiler, hat das
                 * Preflight-Tor ihn durchgelassen — also traegt das Paar
                 * ein Urteil in der Rundlauf-Matrix. */
                missing_with_verdict++;
                printf("  FEHLT  %s -> %s: Tabelle bietet an, Verteiler "
                       "hat keinen Zweig\n",
                       uft_format_get_name(src), uft_format_get_name(dst));
            } else if (has_warning(&r, "Preflight ABORT")) {
                refused++;
            } else {
                dispatched++;
            }
#ifdef UFT_DISPATCH_TRACE
            printf("  TRACE %s->%s err=%d matrix=%s warn=%d: %s\n",
                   uft_format_get_name(src), uft_format_get_name(dst),
                   (int)r.error,
                   uft_roundtrip_status_short(
                       uft_roundtrip_status((uft_format_id_t)src,
                                            (uft_format_id_t)dst)),
                   r.warning_count,
                   r.warning_count > 0 ? r.warnings[0] : "(keine)");
#endif
        }
    }

    printf("\n  angeboten in der Tabelle : %d Paare\n", offered);
    printf("  vom Verteiler bedient    : %d\n", dispatched);
    printf("  vom Preflight abgewiesen : %d (UNGEPRUEFT — so gedacht)\n",
           refused);
    printf("  ohne Verteiler-Zweig     : %d\n", missing);

    /* ── Die Schranke ───────────────────────────────────────────────────
     *
     * Ein Paar, das die Tabelle anbietet und der Verteiler nicht kennt,
     * ist eine Zusage ohne Deckung. Der Rueckfall meldet es ehrlich —
     * aber erst, nachdem Tabelle und Preflight dem Benutzer gesagt haben,
     * der Weg sei gangbar. */
    if (missing > 0) {
        printf("\n  FAIL: %d Paare stehen im Angebot, ohne dass es einen "
               "Wandler gibt.\n        Der Benutzer erfaehrt es erst beim "
               "Verteiler — nachdem\n        Tabelle und Preflight ihm "
               "zugesagt haben, dass es geht.\n", missing);
        failures++;
    }

    /* Und die Gegenprobe: der Test misst nur dann etwas, wenn ueberhaupt
     * Paare durchgelaufen sind. */
    if (offered < 20) {
        printf("  FAIL: nur %d Paare geprueft — die Tabelle fuehrt "
               "mehr als 40.\n        Dieser Test misst so gut wie "
               "nichts.\n", offered);
        failures++;
    }

    /* ── Das Tor muss auch ohne Dateipfade greifen ──────────────────────
     *
     * `uft_convert_memory()` hat keine Dateien; es reicht `NULL, NULL` an
     * `uft_preflight_check()` weiter. Das darf die PRUEFUNG nicht
     * abschalten — nur die Nebendatei (`.loss.json`) kann ohne Ziel nicht
     * geschrieben werden, das Urteil sehr wohl.
     *
     * Gemessen wird an einem Paar, das die Matrix ausdruecklich UNMOEGLICH
     * nennt, mit der Begruendung „synthesising flux would be fabrication":
     * IMG -> SCP. Ein Wandler, der das ausfuehrt, erfindet Timing, das die
     * Quelle nie hatte. */
    {
        uft_convert_result_t r;
        uint8_t *out = NULL;
        size_t out_size = 0;
        uft_convert_options_ext_t o;
        memset(&o, 0, sizeof(o));
        memset(&r, 0, sizeof(r));
        /* Ausdruecklich OHNE Einverstaendnis — und selbst mit waere
         * UNMOEGLICH nicht verhandelbar. */
        o.accept_data_loss = false;

        uft_roundtrip_status_t st =
            uft_roundtrip_status((uft_format_id_t)UFT_FORMAT_IMG,
                                 (uft_format_id_t)UFT_FORMAT_SCP);
        uft_error_t e = uft_convert_memory(junk, sizeof(junk),
                                           UFT_FORMAT_IMG, &out, &out_size,
                                           UFT_FORMAT_SCP, &o, &r);
        free(out);

        printf("\n  IMG->SCP: Matrix sagt %s, Wandler antwortet %d "
               "(%zu Byte)\n", uft_roundtrip_status_string(st), (int)e,
               out_size);

        if (st != UFT_RT_IMPOSSIBLE) {
            printf("  FAIL: dieser Teil misst nichts — die Matrix fuehrt "
                   "IMG->SCP nicht mehr als UNMOEGLICH.\n");
            failures++;
        } else if (e == UFT_OK) {
            printf("  FAIL: das Preflight-Tor laesst im Speicher-Modus ein "
                   "UNMOEGLICH-Paar durch.\n        Die Matrix nennt es "
                   "woertlich Fabrikation; der Wandler tut es und meldet "
                   "Erfolg.\n");
            failures++;
        } else {
            printf("  ok   das Tor greift auch ohne Dateipfade\n");
        }
    }

    printf("\n%s (%d Abweichungen)\n",
           failures ? "FEHLGESCHLAGEN" : "OK", failures);
    return failures ? 1 : 0;
}
