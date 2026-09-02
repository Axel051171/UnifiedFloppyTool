/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_gw_trk0.c
 * @brief Eine Prüfung, die beim Kaputtgehen „gut" meldete (MF-799).
 *
 * ── Der Befund ───────────────────────────────────────────────────────────
 *
 * `uft_gw_get_pin()` gab `bool` zurück und `false` für **jeden**
 * Fehlerfall. `uft_gw_seek()` liest den Pin invertiert:
 *
 *     bool trk0 = !uft_gw_get_pin(device, 26);
 *
 * Scheiterte GET_PIN, kam `false`, `!false` ist `true`, und die
 * Spur-0-Prüfung galt als **bestanden**. Die einzige Absicherung gegen
 * einen dekalibrierten Kopf konnte nur dann „in Ordnung" sagen, wenn sie
 * selbst kaputt war.
 *
 * Zweite Hälfte desselben Befunds: die Prüfung lief nur im
 * `if (cylinder == 0)`-Zweig. keirf prüft beidseitig —
 * `error.check(cyl < 0 or (cyl == 0) == trk0, ...)`. Fehlt die zweite
 * Richtung, meldet ein Laufwerk, das seine Position verloren hat, auf
 * jedem Zylinder /TRK0 und liest achtzigmal dieselbe Spur, ohne dass
 * irgendetwas auffällt.
 *
 * ── Warum das ohne Gerät prüfbar ist ─────────────────────────────────────
 *
 * Nicht durch einen Emulator: `tests/emulators/greaseweazle/` ist eine
 * eigenständige Zustandsmaschine und treibt die echte HAL nicht. Statt
 * einen Transport nachzubauen, ist die **Entscheidung** aus dem
 * Gerätepfad herausgezogen — `uft_gw_trk0_stimmig()` ist rein. Damit ist
 * sie prüfbar, statt in einem Pfad zu verschwinden, den nur eine
 * Hardware-Sitzung erreicht (und die gibt es hier nicht, MF-310).
 *
 * ── Referenz ─────────────────────────────────────────────────────────────
 *
 * keirf/greaseweazle v1.23, `greaseweazle/tools/../usb.py::seek()`,
 * Commit `a0ae343d7e2603b9f3fdc0149ef8e89de5399f58`, Unlicense.
 * Im Oracle-Register als `gw`.
 */
#include "uft/hal/uft_greaseweazle_full.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-46s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                   _fail++; return; } } while (0)

/* ── DER ROTBEWEIS, erste Hälfte ─────────────────────────────────────────
 *
 * Zylinder 40 mit anliegendem /TRK0 ist ein dekalibrierter Kopf. Die
 * alte Prüfung sah diesen Fall nie an. */
TEST(trk0_auf_zylinder_40_ist_ein_befund)
{
    ASSERT(uft_gw_trk0_stimmig(0, true));     /* Spur 0, /TRK0 da   -> ok */
    ASSERT(!uft_gw_trk0_stimmig(0, false));   /* Spur 0, /TRK0 weg  -> Fehler */
    ASSERT(uft_gw_trk0_stimmig(40, false));   /* Spur 40, kein TRK0 -> ok */
    ASSERT(!uft_gw_trk0_stimmig(40, true));   /* <- DER FALL, der durchfiel */
    ASSERT(!uft_gw_trk0_stimmig(79, true));
}

/* Flippy: negative Zylinder sind ausgenommen, wie in der Referenz.
 * Panasonic-Laufwerke melden beim Einwaertsschritt von -1 kein /TRK0. */
TEST(negative_zylinder_sind_ausgenommen)
{
    ASSERT(uft_gw_trk0_stimmig(-1, true));
    ASSERT(uft_gw_trk0_stimmig(-1, false));
}

/* ── DER ROTBEWEIS, zweite Hälfte ────────────────────────────────────────
 *
 * Ohne Gerät ist `device == NULL` der billigste erzwingbare Fehlschlag.
 * Die alte Fassung gab dafür `false` zurück — nicht von einem echten
 * Low-Pegel zu unterscheiden. Die neue muss einen Fehlercode liefern
 * UND den Ausgabewert unangetastet lassen. */
TEST(ein_fehlschlag_ist_kein_pegel)
{
    bool pegel = true;               /* Wachposten: darf nicht beschrieben
                                        werden, wenn der Aufruf scheitert */
    int r = uft_gw_get_pin(NULL, 26, &pegel);

    ASSERT(r != UFT_GW_OK);
    ASSERT(r == UFT_GW_ERR_NOT_CONNECTED);
    ASSERT(pegel == true);           /* unberuehrt */

    /* Und ohne Ausgabeziel ist der Aufruf ein Programmierfehler, kein
     * stiller Erfolg. */
    ASSERT(uft_gw_get_pin(NULL, 26, NULL) != UFT_GW_OK);
}

/* Die Gegenprobe zur Umkehrung: `trk0` entsteht als `!pegel`. Wer die
 * Invertierung vergisst, dreht jede Aussage um — deshalb hier
 * ausbuchstabiert, welcher Pegel welche Bedeutung hat.
 *
 * /TRK0 ist AKTIV LOW: Pegel 0 heisst „Kopf steht ueber Spur 0". */
TEST(die_umkehrung_steht_ausbuchstabiert_da)
{
    const bool pegel_low = false, pegel_high = true;
    ASSERT(uft_gw_trk0_stimmig(0, !pegel_low));     /* low  -> trk0 -> Spur 0 */
    ASSERT(!uft_gw_trk0_stimmig(0, !pegel_high));   /* high -> kein trk0 */
}

int main(void)
{
    printf("test_gw_trk0 (MF-799)\n");
    RUN(trk0_auf_zylinder_40_ist_ein_befund);
    RUN(negative_zylinder_sind_ausgenommen);
    RUN(ein_fehlschlag_ist_kein_pegel);
    RUN(die_umkehrung_steht_ausbuchstabiert_da);
    printf("%d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail ? 1 : 0;
}
