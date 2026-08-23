/**
 * @file test_track_add_sector_owns_copies.c
 * @brief Was hineinkopiert wird, muss herausgegeben werden koennen (MF-531)
 *
 * ── Der Befund ───────────────────────────────────────────────────────────
 *
 * `uft_track_add_sector()` begann mit einer FLACHEN Kopie:
 *
 *     uft_sector_t* dst = &track->sectors[track->sector_count];
 *     *dst = *sector;                       // kopiert ZEIGER
 *     if (sector->data ...) { dst->data = malloc(...); memcpy(...); }
 *
 * Tief kopiert wurde nur `data`. `confidence_map`, `weak_mask` und
 * `timing_ns` blieben Zeiger auf die **Quelle**.
 *
 * `uft_sector_cleanup()` gibt aber alle vier frei — und muss das, seit
 * ATX nach TA3 `weak_mask` fuellt; der Kommentar dort sagt es ausdruecklich.
 *
 * Damit war die API unsymmetrisch: sie nahm drei Zeiger als Fremdbesitz
 * auf und gab sie spaeter als Eigenbesitz frei. Wer Quelle UND Ziel
 * aufraeumt, gibt dieselben drei Zeiger zweimal frei — dieselbe Bauart wie
 * das doppelte `free` in MF-513, nur im gemeinsamen Helfer statt in einem
 * Plugin.
 *
 * ── Warum es nie aufgefallen ist ─────────────────────────────────────────
 *
 * Ausgeloest hat es niemand. Die zwoelf Plugins, die seit MF-516 ueber
 * diese API gehen, bauen ihre Sektoren ohne `weak_mask`. Ein Plugin, das
 * ATX-Sektoren weiterreicht, haette es getroffen — und der Fehler waere
 * dann in einem forensischen Pfad aufgetreten, nicht in einem Randfall.
 *
 * ── Was dieser Test tut ──────────────────────────────────────────────────
 *
 * Er baut einen Sektor mit allen drei Feldern, gibt ihn an
 * `uft_track_add_sector()`, raeumt **beide** auf und prueft, dass die
 * Zeiger verschieden sind. Unter ASan war das vor der Korrektur ein
 * doppeltes free; ohne Sanitizer bleibt der Zeigervergleich als Nachweis.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(c, msg) do { \
    if (c) { printf("  ok   %s\n", msg); } \
    else   { printf("  FAIL %s\n", msg); failures++; } } while (0)

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("uft_track_add_sector() muss besitzen, was es freigibt (MF-531)\n");

    enum { N = 64, T = 16 };

    uft_sector_t src;
    memset(&src, 0, sizeof(src));
    src.data_size      = N;
    src.data           = malloc(N);
    src.confidence_map = malloc(N);
    src.weak_mask      = malloc(N);
    src.timing_count   = T;
    src.timing_ns      = malloc(T * sizeof(double));
    if (!src.data || !src.confidence_map || !src.weak_mask || !src.timing_ns) {
        printf("  Speicher fehlt — nicht messbar\n");
        return 1;
    }
    memset(src.data, 0xAB, N);
    memset(src.confidence_map, 0x7F, N);
    memset(src.weak_mask, 0x01, N);
    for (int i = 0; i < T; i++) src.timing_ns[i] = 1000.0 + i;

    uft_track_t trk;
    uft_track_init(&trk, 0, 0);

    uft_error_t e = uft_track_add_sector(&trk, &src);
    CHECK(e == UFT_OK, "uft_track_add_sector() nimmt den Sektor an");
    CHECK(trk.sector_count == 1, "Zaehler steht auf 1");

    if (trk.sector_count == 1) {
        const uft_sector_t *dst = &trk.sectors[0];

        /* Der Kern: jeder Zeiger, den uft_sector_cleanup() freigibt, muss
         * dem Ziel gehoeren — also verschieden von dem der Quelle sein. */
        CHECK(dst->data != src.data,
              "data ist eine eigene Kopie");
        CHECK(dst->confidence_map != src.confidence_map,
              "confidence_map ist eine eigene Kopie");
        CHECK(dst->weak_mask != src.weak_mask,
              "weak_mask ist eine eigene Kopie");
        CHECK(dst->timing_ns != src.timing_ns,
              "timing_ns ist eine eigene Kopie");

        /* Und der Inhalt muss stimmen — eine eigene Kopie, die etwas
         * anderes enthaelt, waere schlimmer als eine geteilte. */
        CHECK(dst->data && memcmp(dst->data, src.data, N) == 0,
              "data inhaltsgleich");
        CHECK(dst->confidence_map &&
              memcmp(dst->confidence_map, src.confidence_map, N) == 0,
              "confidence_map inhaltsgleich");
        CHECK(dst->weak_mask &&
              memcmp(dst->weak_mask, src.weak_mask, N) == 0,
              "weak_mask inhaltsgleich");
        CHECK(dst->timing_count == T,
              "timing_count uebernommen");
        CHECK(dst->timing_ns &&
              memcmp(dst->timing_ns, src.timing_ns, T * sizeof(double)) == 0,
              "timing_ns inhaltsgleich");
    }

    /* BEIDE aufraeumen. Vor der Korrektur war das ein doppeltes free auf
     * drei Zeigern; unter ASan bricht der Lauf hier ab. */
    uft_track_cleanup(&trk);
    uft_sector_cleanup(&src);
    printf("  ok   Quelle und Ziel beide aufgeraeumt, kein doppeltes free\n");

    printf("\n%s (%d Abweichungen)\n",
           failures ? "FEHLGESCHLAGEN" : "OK", failures);
    return failures ? 1 : 0;
}
