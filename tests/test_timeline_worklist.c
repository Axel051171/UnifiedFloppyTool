/**
 * @file test_timeline_worklist.c
 * @brief Die Nicht-Verschlimmerungs-Garantie, vor dem Umbau (MF-564)
 *
 * ── Worum es geht ────────────────────────────────────────────────────────
 *
 * Der Plan (§2.3.1) will, dass die Recovery-Stufen scheibenweise arbeiten
 * und harte Scheiben teurer behandeln. Heute laufen sie ueber die GANZE
 * Spur; die Scheibenkarte aus MF-501 wird nur zum MELDEN benutzt
 * (`uft_timeline_fraction()` im Wandler), nie zum Steuern.
 *
 * Ein solcher Umbau kann auf genau zwei Arten schiefgehen, und beide sind
 * still:
 *
 *   1. Eine Scheibe, die schon **sauber dekodiert** war, wird noch einmal
 *      angefasst — und kommt schlechter zurueck. Eine Recovery-Stufe, die
 *      an gutem Material dreht, macht es hoechstens kaputt.
 *   2. Das scheibenweise Ergebnis weicht auf einer **sauberen Spur** vom
 *      Ganzspur-Ergebnis ab. Dann hat der Umbau etwas veraendert, wo es
 *      nichts zu verbessern gab.
 *
 * Beides faellt nicht auf, wenn man es nachher misst: eine Spur, die
 * vorher 11 Sektoren hatte und nachher 11, sieht gleich aus, auch wenn es
 * andere 11 sind.
 *
 * ── Warum dieser Test VOR dem Umbau steht ────────────────────────────────
 *
 * Er prueft nicht das Steuern — das gibt es noch nicht. Er prueft den
 * **Baustein**, den das Steuern braucht, und die beiden Zusagen, die es
 * einhalten muss:
 *
 *      uft_timeline_work_list()  welche Scheiben Arbeit brauchen
 *
 * Zusage 1: **Eine DECODED-Scheibe steht nie darin.** Wer die Liste
 *           abarbeitet, kann gutes Material gar nicht erst anfassen.
 * Zusage 2: **Auf einer sauberen Spur ist die Liste leer.** Damit ist das
 *           scheibenweise Vorgehen dort ein Nichts-Tun, und das Ergebnis
 *           ist per Konstruktion identisch mit dem Ganzspur-Ergebnis. Das
 *           ist die Nicht-Verschlimmerungs-Garantie in ihrem Grundfall,
 *           und sie gilt, bevor eine Zeile Steuerlogik geschrieben ist.
 * Zusage 3: **Arbeitsliste plus DECODED ergibt alle Scheiben.** Sonst
 *           vergisst das Steuern etwas, ohne es zu melden.
 *
 * Wer den Umbau macht, hat damit einen Schiedsrichter, der schon stand,
 * bevor es etwas zu beschoenigen gab.
 */

#include "uft/flux/uft_decode_timeline.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond, ...)                                                   \
    do {                                                                   \
        if (cond) { printf("  ok   " __VA_ARGS__); printf("\n"); }         \
        else { printf("  FAIL " __VA_ARGS__); printf("\n"); failures++; }  \
    } while (0)

/** Baut eine Karte von Hand — der Test soll die LISTE pruefen, nicht den
 *  Dekoder. */
static void make_map(uft_decode_timeline_t *t, const uft_slice_status_t *st,
                     size_t n)
{
    memset(t, 0, sizeof(*t));
    t->slices = calloc(n, sizeof(uft_decode_slice_t));
    t->count = n;
    t->bit_count = n * 100;
    for (size_t i = 0; i < n; i++) {
        t->slices[i].first_bit = i * 100;
        t->slices[i].end_bit   = (i + 1) * 100;
        t->slices[i].status    = st[i];
    }
}

static void free_map(uft_decode_timeline_t *t)
{
    free(t->slices);
    t->slices = NULL;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Die Nicht-Verschlimmerungs-Garantie (MF-564) ===\n");

    size_t idx[32];

    /* ── Zusage 2: saubere Spur, leere Liste ───────────────────────────
     *
     * Zuerst, weil sie die wichtigste ist: solange die Liste hier leer
     * bleibt, kann das scheibenweise Vorgehen auf gesunden Spuren
     * nichts kaputt machen. */
    {
        uft_slice_status_t all_ok[8];
        for (int i = 0; i < 8; i++) all_ok[i] = UFT_SLICE_DECODED;

        uft_decode_timeline_t t;
        make_map(&t, all_ok, 8);

        size_t n = uft_timeline_work_list(&t, idx, 32);
        printf("       saubere Spur: %zu Scheiben auf der Liste\n", n);
        CHECK(n == 0,
              "auf einer sauberen Spur ist nichts zu tun — das Ergebnis "
              "ist per Konstruktion dasselbe");
        free_map(&t);
    }

    /* ── Zusage 1: DECODED steht nie darin ─────────────────────────────── */
    {
        uft_slice_status_t mixed[9] = {
            UFT_SLICE_DECODED,  UFT_SLICE_DAMAGED,  UFT_SLICE_DECODED,
            UFT_SLICE_UNTOUCHED, UFT_SLICE_DECODED, UFT_SLICE_DAMAGED,
            UFT_SLICE_DECODED,  UFT_SLICE_UNTOUCHED, UFT_SLICE_DECODED,
        };
        uft_decode_timeline_t t;
        make_map(&t, mixed, 9);

        size_t n = uft_timeline_work_list(&t, idx, 32);
        printf("       gemischte Spur: %zu von 9 Scheiben brauchen Arbeit\n",
               n);
        CHECK(n == 4, "die vier nicht-dekodierten Scheiben");

        int touched_decoded = 0;
        for (size_t i = 0; i < n; i++)
            if (t.slices[idx[i]].status == UFT_SLICE_DECODED)
                touched_decoded = 1;
        CHECK(!touched_decoded,
              "keine einzige DECODED-Scheibe auf der Liste — gutes "
              "Material wird gar nicht erst angefasst");

        /* Zusage 3: nichts vergessen. */
        size_t decoded = 0;
        for (size_t i = 0; i < t.count; i++)
            if (t.slices[i].status == UFT_SLICE_DECODED) decoded++;
        CHECK(n + decoded == t.count,
              "Arbeitsliste plus DECODED ergibt alle Scheiben — nichts "
              "faellt hinten runter");

        /* Aufsteigend, damit ein Abarbeiter den Strom vorwaerts liest
         * statt zu springen. */
        int asc = 1;
        for (size_t i = 1; i < n; i++)
            if (idx[i] <= idx[i - 1]) asc = 0;
        CHECK(asc, "aufsteigend, also in Stromrichtung");
        free_map(&t);
    }

    /* ── Eine Spur ganz ohne Erfolg ────────────────────────────────────── */
    {
        uft_slice_status_t all_bad[5];
        for (int i = 0; i < 5; i++) all_bad[i] = UFT_SLICE_DAMAGED;

        uft_decode_timeline_t t;
        make_map(&t, all_bad, 5);
        size_t n = uft_timeline_work_list(&t, idx, 32);
        CHECK(n == 5, "auf einer ganz kaputten Spur steht alles auf der Liste");
        free_map(&t);
    }

    /* ── Ein zu kleines Feld darf nicht luegen ─────────────────────────── */
    {
        uft_slice_status_t all_bad[10];
        for (int i = 0; i < 10; i++) all_bad[i] = UFT_SLICE_DAMAGED;

        uft_decode_timeline_t t;
        make_map(&t, all_bad, 10);

        size_t small[3];
        size_t n = uft_timeline_work_list(&t, small, 3);
        printf("       Feld fuer 3, gebraucht werden 10 -> Antwort %zu\n", n);
        CHECK(n == 10,
              "die Antwort nennt die WIRKLICHE Zahl, nicht die "
              "geschriebene — eine Zahl, die weniger meldet als da ist, "
              "waere die stille Kuerzung aus MF-550");
        free_map(&t);
    }

    /* ── Unbrauchbare Argumente ────────────────────────────────────────── */
    {
        uft_decode_timeline_t t;
        uft_slice_status_t one[1] = { UFT_SLICE_DAMAGED };
        make_map(&t, one, 1);
        CHECK(uft_timeline_work_list(NULL, idx, 32) == 0,
              "ohne Karte keine Liste");
        CHECK(uft_timeline_work_list(&t, NULL, 32) == 1,
              "ohne Feld nur zaehlen — das ist die Art, die Groesse zu "
              "erfragen");
        free_map(&t);
    }

    printf("\n%s (%d Abweichungen)\n",
           failures ? "FEHLGESCHLAGEN" : "OK", failures);
    return failures ? 1 : 0;
}
