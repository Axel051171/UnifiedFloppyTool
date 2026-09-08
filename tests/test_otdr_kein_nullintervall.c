/* SPDX-License-Identifier: MIT */
/**
 * @file test_otdr_kein_nullintervall.c
 * @brief Die OTDR-Analyse bekam erfundene 0-ns-Uebergaenge (MF-967)
 *
 * ── WORUM ES GEHT ────────────────────────────────────────────────────
 *
 * Eine SCP-Datei speichert Flusszeiten als 16-Bit-Werte. Ist ein
 * Intervall laenger als 65535 Einheiten, schreibt das Format einen
 * Eintrag `0x0000` als UEBERLAUFMARKE und rechnet 65536 auf den
 * naechsten Eintrag drauf. Die offizielle Spezifikation sagt das
 * woertlich:
 *
 *     „0x0000 […] the time overflowed […] 65536 is added for each
 *      entry"
 *     — SCP Image Specification v1.9, Jim Drew (SuperCard Pro)
 *
 * `src/flux/uft_scp_parser.c` setzt das richtig um: der Ueberlauf wird
 * akkumuliert und auf den naechsten echten Wert addiert. An der Stelle
 * der Marke bleibt ein **0-Platzhalter** stehen — kein Uebergang,
 * sondern eine Luecke in der Zaehlung.
 *
 * Zwei von drei Verbrauchern wissen das und lassen ihn fallen:
 *
 *     src/flux/uft_flux_decoder.c  `if (intervals[i] == 0) continue;`
 *     src/fluxwritejob.cpp         `if (ns == 0) continue;`
 *
 * Der dritte nicht. `otdr_track_load_flux()` machte ein glattes
 * `memcpy`, und damit lief der Platzhalter als ECHTER Uebergang in die
 * Analyse:
 *
 *   1. `otdr_track_histogram()` legt ihn in Bin 0 (`ns / 100`) und
 *      blaeht die Gesamtzahl auf.
 *   2. `otdr_track_analyze()` fuettert ihn in den PLL
 *      (`otdr_pll_feed(&track->pll, 0, …)`) — ein Phasenstoss aus dem
 *      Nichts, dessen Abweichung als echte Messung ins Qualitaets-
 *      profil eingeht.
 *
 * Ein Flussintervall von 0 ns ist physikalisch unmoeglich: zwei
 * Uebergaenge zur selben Zeit gibt es nicht. Das ist keine Ungenauig-
 * keit, sondern eine **erfundene Messung** — und sie trifft genau die
 * Disketten, um derentwillen die OTDR-Analyse existiert: Ueberlaeufe
 * entstehen bei langen Luecken, unformatierten Bereichen und
 * No-Flux-Zonen, also an Kopierschutz-Merkmalen.
 *
 * ── WAS HIER ZUGESICHERT WIRD ────────────────────────────────────────
 *
 *   a) Nach dem Laden enthaelt `flux_ns` KEINE Null.
 *   b) `flux_count` zaehlt nur echte Uebergaenge.
 *   c) `revolution_ns` bleibt unveraendert — die Summe ist dieselbe,
 *      ob man die Nullen mitzaehlt oder nicht. Ohne diese Zusicherung
 *      koennte ein „Fix", der die Zeitachse verkuerzt, durchgehen.
 *   d) Dasselbe gilt fuer die Mehrfach-Ablage `flux_multi[rev]`.
 */

#include "uft/analysis/floppy_otdr.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-42s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                    _fail++; return; } } while (0)

/* Ein Fluss-Feld, wie `uft_scp_read_track()` es liefert: zwei
 * Ueberlaufmarken, deren 65536 bereits im jeweils naechsten Wert
 * stecken. Echte Uebergaenge: 6. Platzhalter: 2. */
static const uint32_t FLUSS[] = {
    2000u,
    4000u,
    0u,                 /* Ueberlaufmarke */
    65536u + 3000u,     /* der lange Eintrag, Ueberlauf schon addiert */
    2000u,
    0u,                 /* zweite Ueberlaufmarke */
    65536u + 1500u,
    2500u
};
#define N_ROH   (sizeof(FLUSS) / sizeof(FLUSS[0]))
#define N_ECHT  6u

static uint64_t summe(const uint32_t *v, size_t n)
{
    uint64_t s = 0;
    for (size_t i = 0; i < n; i++) s += v[i];
    return s;
}

TEST(kein_nullintervall_in_der_analyse)
{
    otdr_track_t *tr = otdr_track_create(0, 0);
    ASSERT(tr != NULL);

    otdr_track_load_flux(tr, FLUSS, (uint32_t)N_ROH, 0);

    /* (b) nur echte Uebergaenge */
    if (tr->flux_count != N_ECHT)
        printf("\n       flux_count=%u, erwartet %u\n",
               (unsigned)tr->flux_count, (unsigned)N_ECHT);
    ASSERT(tr->flux_count == N_ECHT);

    /* (a) keine Null im Feld — die Zeile, die vor MF-967 faellt */
    ASSERT(tr->flux_ns != NULL);
    for (uint32_t i = 0; i < tr->flux_count; i++) {
        if (tr->flux_ns[i] == 0)
            printf("\n       flux_ns[%u] == 0 — erfundener Uebergang\n",
                   (unsigned)i);
        ASSERT(tr->flux_ns[i] != 0);
    }

    /* (e) und es sind DIESELBEN Werte, in derselben Reihenfolge.
     *
     * Ohne diese Zusicherung kaeme ein Filter durch, der richtig ZAEHLT
     * und schief LIEST — gemessen: die Mutation „richtige Anzahl,
     * verfaelschter Inhalt" blieb ohne sie gruen. Zaehlen ist nicht
     * Lesen. */
    uint32_t k = 0;
    for (uint32_t i = 0; i < N_ROH; i++) {
        if (FLUSS[i] == 0) continue;
        if (tr->flux_ns[k] != FLUSS[i])
            printf("\n       flux_ns[%u]=%u, erwartet %u (Quelle %u)\n",
                   (unsigned)k, (unsigned)tr->flux_ns[k],
                   (unsigned)FLUSS[i], (unsigned)i);
        ASSERT(tr->flux_ns[k] == FLUSS[i]);
        k++;
    }
    ASSERT(k == tr->flux_count);

    otdr_track_free(tr);
}

TEST(die_umdrehungsdauer_bleibt_gleich)
{
    otdr_track_t *tr = otdr_track_create(0, 0);
    ASSERT(tr != NULL);

    otdr_track_load_flux(tr, FLUSS, (uint32_t)N_ROH, 0);

    /* (c) Nullen wegzulassen darf die Zeitachse NICHT verkuerzen.
     * Ein „Fix", der stattdessen die langen Eintraege wegwirft, waere
     * schlimmer als der Fehler. */
    const uint64_t soll = summe(FLUSS, N_ROH);
    if ((uint64_t)tr->revolution_ns != soll)
        printf("\n       revolution_ns=%llu, erwartet %llu\n",
               (unsigned long long)tr->revolution_ns,
               (unsigned long long)soll);
    ASSERT((uint64_t)tr->revolution_ns == soll);

    otdr_track_free(tr);
}

TEST(auch_die_mehrfach_ablage_ist_sauber)
{
    otdr_track_t *tr = otdr_track_create(0, 0);
    ASSERT(tr != NULL);

    /* (d) Umdrehung 2 — der Zweig, der NICHT `rev == 0` ist. Er hat
     * seine eigene Kopie, und die wurde bis MF-967 ebenfalls roh
     * uebernommen. */
    otdr_track_load_flux(tr, FLUSS, (uint32_t)N_ROH, 2);

    ASSERT(tr->flux_multi[2] != NULL);
    if (tr->flux_multi_count[2] != N_ECHT)
        printf("\n       flux_multi_count[2]=%u, erwartet %u\n",
               (unsigned)tr->flux_multi_count[2], (unsigned)N_ECHT);
    ASSERT(tr->flux_multi_count[2] == N_ECHT);
    for (uint32_t i = 0; i < tr->flux_multi_count[2]; i++)
        ASSERT(tr->flux_multi[2][i] != 0);

    otdr_track_free(tr);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== OTDR: kein erfundenes 0-ns-Intervall (MF-967) ===\n");
    RUN(kein_nullintervall_in_der_analyse);
    RUN(die_umdrehungsdauer_bleibt_gleich);
    RUN(auch_die_mehrfach_ablage_ist_sauber);
    printf("\n%d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
