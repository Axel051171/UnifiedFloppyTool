/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * test_copy_befund_traegt_die_messung.c — ein Befund muss sagen, WELCHE
 * Faehigkeit fehlt, und schweigen, wo nichts gemessen wurde (MF-1332).
 *
 * ── Der Befund, der dazu gefuehrt hat ────────────────────────────────
 * `uft_copy_finding_t` trug bis MF-1332 DREI Felder: `hard`, `id`,
 * `text`. Die Faehigkeit, um die es ging, stand nur im deutschen
 * Fliesstext — obwohl `uft_copy_plan_check()` sie KENNT: bei
 * `"kein_fluss"` steht `UFT_CAP_FLUX_IO` zwei Zeilen ueber dem
 * `befund()`-Aufruf in der eigenen `if`-Bedingung. Ein Verbraucher wie
 * `src/formattab.cpp:2043` musste auf `id` string-matchen, um zu
 * erfahren, was das Geraet koennen muesste.
 *
 * ── Die benannte Referenz ────────────────────────────────────────────
 * `tools/uft-retrace/docs/UFT_INTEGRATION.md` (GPL-2.0-or-later,
 * Eigentuemer-Zulieferung, seit MF-1332 im Baum) verlangt woertlich,
 * dass eine Empfehlung „empfohlene Ebene; benoetigte Faehigkeiten;
 * Verlustmeldung; Messquelle; Konfidenz" liefert, und setzt die
 * Schranke dazu: „Eine Empfehlung darf eine Benutzerauswahl nicht
 * heimlich ueberschreiben."
 *
 * ── Warum ZWEI Zusagen und nicht eine ────────────────────────────────
 * Die zweite ist die wichtigere und die Lehre aus MF-1311: eine Null
 * ist mehrdeutig. `caps_benoetigt == 0` kann „gemessen, braucht nichts"
 * heissen oder „nicht gemessen". Ein Test, der nur prueft, dass die
 * Faehigkeit ankommt, waere auch dann gruen, wenn JEDER Befund
 * `caps_benoetigt_bekannt = true` traegt — und dann behauptete der Baum
 * eine Messung, die nie stattfand.
 *
 * Deshalb prueft dieser Test beide Richtungen:
 *   (1) ein Faehigkeits-Befund NENNT seine Fahne;
 *   (2) ein Befund ohne Messung sagt ausdruecklich „nicht gemessen".
 *
 * ── Und die dritte, die den Stapelmuell faengt ───────────────────────
 * Alle acht Aufrufer im Baum legen ihr `uft_copy_finding_t f[16]`
 * UNINITIALISIERT an (gemessen). Bis MF-1332 schrieb `befund()` drei
 * Felder; jedes weitere waere beim Aufrufer Stapelmuell gewesen. Der
 * Test fuellt sein Feld deshalb ABSICHTLICH mit 0xAA vor und verlangt,
 * dass der Erzeuger es ueberschreibt.
 *
 * ── Mutationsmatrix: 8 von 9, und die neunte ist NACHGEMESSEN ────────
 * Gefangen: die Fahne faellt weg (`befund_caps` -> `befund`), jeder
 * Befund behauptet eine Messung, ein ungemessener Befund empfiehlt doch
 * eine Ebene, eine zweite Fahne wird mitgeschleppt, die Konfidenz gilt
 * als gemessen und ist 0, die Verlustmeldung faellt weg, ein
 * ungemessener Befund erfindet eine Messquelle.
 *
 * DURCHGERUTSCHT ist genau eine: das `memset` in `befund_voll()`
 * entfernen aendert nichts. Der Grund ist gemessen und nicht erzaehlt —
 * eine zweite Mutation nimmt `memset` UND die Zuweisung von `verlust`
 * zugleich weg, und DANN faellt der Test. Beide Sicherungen sind heute
 * **gegenseitig redundant**: jede allein haelt die Zusage, weil direkt
 * hinter dem `memset` alle neun Felder unbedingt zugewiesen werden.
 *
 * Das ist die Gestalt von MF-1031 (dort waren es zwei obere Schranken
 * im 2MG-Leser). Das `memset` bleibt trotzdem stehen, und zwar mit
 * einem Grund, den der Test heute NICHT pruefen kann: es ist die
 * Sicherung fuer das ZEHNTE Feld, das jemand anhaengt und zu setzen
 * vergisst. Eine Redundanz, die man gemessen hat, ist etwas anderes als
 * eine, die man annimmt.
 */

#include <stdio.h>
#include <string.h>

#include "uft/core/uft_copy_plan.h"

static int fehler = 0;

static void zusage(int ok, const char *was)
{
    printf("  %-62s %s\n", was, ok ? "ok" : "[ROT]");
    if (!ok) fehler++;
}

/* Den Befund mit dieser `id` suchen. Das Feld wird VORHER mit 0xAA
 * gefuellt — wer die neuen Felder nicht setzt, faellt damit auf. */
static int hole(const uft_copy_plan_t *p, uint32_t caps, const char *id,
                uft_copy_finding_t *aus)
{
    uft_copy_finding_t f[16];
    memset(f, 0xAA, sizeof f);
    size_t n = uft_copy_plan_check(p, caps, f, 16);
    if (n > 16) n = 16;
    for (size_t i = 0; i < n; i++) {
        if (f[i].id && strcmp(f[i].id, id) == 0) {
            *aus = f[i];
            return 1;
        }
    }
    return 0;
}

int main(void)
{
    printf("Ein Befund traegt seine Messung (MF-1332)\n");

    /* (1) Flussebene ohne Flussfaehigkeit -> "kein_fluss". */
    uft_copy_plan_t p = uft_copy_plan_default();
    p.level = UFT_COPY_FLUX;

    uft_copy_finding_t f;
    memset(&f, 0, sizeof f);
    if (!hole(&p, 0u, "kein_fluss", &f)) {
        printf("  [ROT] Befund \"kein_fluss\" kam gar nicht — "
               "die Vorbedingung des Tests traegt nicht\n");
        return 1;
    }

    zusage(f.hard, "kein_fluss ist ein harter Befund");

    /* DIE ZEILE: die Fahne steht im `if` daneben und muss beim
     * Aufrufer ankommen. */
    zusage(f.caps_benoetigt_bekannt,
           "kein_fluss: die Faehigkeit gilt als GEMESSEN");
    zusage(f.caps_benoetigt == (uint32_t)UFT_CAP_FLUX_IO,
           "kein_fluss: genannt wird genau UFT_CAP_FLUX_IO");
    zusage(f.verlust != NULL,
           "kein_fluss: die Verlustmeldung ist gefuellt");
    zusage(f.messquelle != NULL,
           "kein_fluss: die Messquelle ist genannt");
    zusage(f.konfidenz_gemessen && f.konfidenz > 0u,
           "kein_fluss: die Konfidenz ist gemessen, nicht 0");

    /* Und sie darf NICHT mehr nennen, als gemeint ist. */
    zusage((f.caps_benoetigt & ~(uint32_t)UFT_CAP_FLUX_IO) == 0u,
           "kein_fluss: keine zusaetzliche Fahne mitgeschleppt");

    /* (2) GEGENPROBE. `beweis_und_eile` ist ein Befund OHNE
     * Faehigkeitsbezug — Beweis-Vorgabe gegen schnelle Strategie. Er
     * muss ausdruecklich „nicht gemessen" sagen. Ohne diese Zusage
     * waere (1) auch dann gruen, wenn jeder Befund eine Messung
     * behauptet. */
    uft_copy_plan_t q = uft_copy_plan_default();
    q.policy   = UFT_POLICY_EVIDENCE;
    q.strategy = UFT_READ_FAST;

    uft_copy_finding_t g;
    memset(&g, 0, sizeof g);
    if (!hole(&q, 0xFFFFFFFFu, "beweis_und_eile", &g)) {
        printf("  [ROT] Befund \"beweis_und_eile\" kam nicht — "
               "die Gegenprobe traegt nicht\n");
        return 1;
    }

    zusage(!g.caps_benoetigt_bekannt,
           "beweis_und_eile: sagt ausdruecklich NICHT gemessen");
    zusage(g.caps_benoetigt == 0u,
           "beweis_und_eile: nennt keine Fahne");
    zusage(!g.konfidenz_gemessen,
           "beweis_und_eile: behauptet keine Konfidenz");
    zusage(g.ebene_empfohlen == UFT_COPY_EBENE_KEINE,
           "beweis_und_eile: empfiehlt keine Ebene");

    /* (3) STAPELMUELL. Das Feld war mit 0xAA vorgefuellt; wenn der
     * Erzeuger nicht nullt, stehen hier 0xAAAAAAAA und ein `bool`,
     * der wahr ist. Diese Zusage faengt genau das. */
    zusage(g.caps_benoetigt != 0xAAAAAAAAu,
           "der Erzeuger ueberschreibt den Eintrag (kein Stapelmuell)");
    zusage(g.verlust == NULL && g.messquelle == NULL,
           "ungemessene Felder sind NULL, nicht Zufall");

    printf("%s\n", fehler ? "FEHLGESCHLAGEN" : "BESTANDEN (0 Fehler)");
    return fehler ? 1 : 0;
}
