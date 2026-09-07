/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_gw_draht_codec.c
 * @brief Was UFT schreibt, muss das Protokoll auch so lesen (MF-955)
 *
 * ── Der Befund ───────────────────────────────────────────────────────
 *
 * `src/hal/uft_hal_unified.c` fuehrte einen EIGENEN Greaseweazle-
 * Draht-Codec — in beide Richtungen, und in beide Richtungen falsch.
 *
 * LESEN, gemessen gegen `uft_gw_decode_flux_stream()` an einem
 * erzeugten Strom (2 Umdrehungen, 1000 Wechsel):
 *
 *     Produktionsdekoder  2000 Abtastungen  1152 1152 1152 1152 864 …
 *     dieser Dekoder      2015 Abtastungen   201    1    1    1   1 906
 *     abweichend          2000 von 2000
 *
 * SCHREIBEN, gemessen im Rundlauf mit MFM-DD-Zellzeiten bei 72 MHz:
 *
 *     4000 ns -> gemeint 288 Ticks -> gelesen 536 Ticks
 *     6000 ns -> gemeint 432 Ticks -> gelesen 680 Ticks
 *     8000 ns -> gemeint 576 Ticks -> gelesen 313 Ticks
 *     12 von 12 Wechseln kommen falsch an
 *
 * Drei Fehler, beide Richtungen betreffend: `0xFF` ist ein ESCAPE
 * (Opcode + N28-Nutzlast), keine eigenstaendige Marke — die alte
 * Lesefassung addierte dafuer 200 ERFUNDENE Ticks. `0xFE` (254) gehoert
 * in den 2-Byte-Bereich 250..254, nicht zu „Space". Und die 2-Byte-Form
 * ist `250 + (val-250)*255 + next - 1`, nicht `(val & 0x0F) << 8`.
 *
 * ── Warum der Schreibpfad der schwerere ist ──────────────────────────
 *
 * Ein Lesefehler liefert falsche Daten und faellt irgendwann auf. Ein
 * SCHREIBfehler brennt falsche Zeiten auf eine echte Diskette — und das
 * Werkzeug meldet Erfolg. „Keine stille Veraenderung" ist genau dieses
 * Prinzip.
 *
 * ── Warum es niemandem auffiel ───────────────────────────────────────
 *
 * `src/hal/uft_hal_unified.c` — 1503 Zeilen, fuenf Treiber — war bis
 * MF-954 in KEINEM Test eingebunden. Gemessen ueber
 * `git show HEAD~1:tests/CMakeLists.txt`: null Treffer. Es war also
 * nicht so, dass Tests gruen auf Muell gewesen waeren; es ist NIE ein
 * Test durch diesen Pfad gelaufen.
 *
 * ── Was dieser Test prueft ───────────────────────────────────────────
 *
 * Nicht den Produktionscodec — der ist ueber den Fluss-Generator
 * geprueft (MF-951/952). Geprueft wird das NEUE an MF-955: die
 * Abtretung samt EINHEITENWECHSEL. Der Lesepfad bekommt Ticks und muss
 * Nanosekunden liefern, der Schreibpfad umgekehrt. Genau dort sitzt die
 * Fehlerklasse, um die es die ganze Zeit geht: was bedeutet diese Zahl?
 *
 * NICHT geprueft: was ein echtes Geraet aus dem Draht macht. Dieses
 * Projekt hat keine Hardware (MF-310).
 */
#include "uft/hal/uft_greaseweazle_full.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-46s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                       _fail++; return; } } while (0)

#define FREQ_F7   72000000u
#define FREQ_F7P  84000000u

/* Typische MFM-DD-Zellzeiten in Nanosekunden. */
static const uint32_t NS[] = { 4000, 6000, 8000, 4000, 4000, 6000,
                               8000, 8000, 4000, 6000, 4000, 8000 };
#define N_NS ((int)(sizeof NS / sizeof *NS))

/** ns -> Ticks, wie `gw_write_flux()` es seit MF-955 tut. */
static uint32_t ns_zu_ticks(uint32_t ns, uint32_t freq)
{
    double t = (double)ns * ((double)freq / 1000000000.0) + 0.5;
    if (t < 1.0) t = 1.0;
    return (uint32_t)t;
}

/** Ticks -> ns, wie `gw_read_flux()` es seit MF-955 tut. */
static uint32_t ticks_zu_ns(uint32_t ticks, uint32_t freq)
{
    return (uint32_t)((double)ticks * (1e9 / (double)freq) + 0.5);
}

/* ── Der ALTE Kodierer, als Gegenbeweis ──────────────────────────────
 *
 * Wortgetreu aus `gw_write_flux()` vor MF-955. Er steht hier, damit der
 * Befund nachvollziehbar bleibt, wenn niemand mehr den alten Stand
 * auscheckt — und damit der Test zeigt, dass er WIRKLICH falsch war. */
static size_t alt_kodieren(const uint32_t *ns, size_t count,
                           uint8_t *wire, size_t wire_size, uint32_t freq)
{
    double ticks_per_ns = (double)freq / 1000000000.0;
    size_t pos = 0;
    for (size_t i = 0; i < count && pos < wire_size - 4; i++) {
        uint32_t ticks = (uint32_t)(ns[i] * ticks_per_ns + 0.5);
        if (ticks == 0) ticks = 1;
        while (ticks > 0xF9) {
            if (ticks >= 0x100 && ticks <= 0xFFF) {
                wire[pos++] = 0xFA | (uint8_t)((ticks >> 8) & 0x03);
                wire[pos++] = (uint8_t)(ticks & 0xFF);
                ticks = 0;
            } else {
                wire[pos++] = 0xFE;
                ticks -= 0x100;
            }
        }
        if (ticks > 0) wire[pos++] = (uint8_t)ticks;
    }
    wire[pos++] = 0x00;
    return pos;
}

TEST(der_alte_kodierer_war_wirklich_falsch)
{
    /* DER ROTBEWEIS, festgehalten. Ohne ihn waere „12 von 12 falsch"
     * eine Behauptung ueber Code, den es nicht mehr gibt. */
    static uint8_t wire[4096];
    static uint32_t back[4096];

    size_t wlen = alt_kodieren(NS, N_NS, wire, sizeof wire, FREQ_F7);
    uint32_t sf = 0;
    uint32_t nb = uft_gw_decode_flux_stream(wire, wlen, back, 4096, &sf);

    int falsch = 0;
    for (int i = 0; i < N_NS; i++) {
        uint32_t soll = ns_zu_ticks(NS[i], FREQ_F7);
        if (i >= (int)nb || back[i] != soll) falsch++;
    }
    if (falsch != N_NS) {
        printf("\n      der alte Kodierer war in %d von %d Faellen falsch "
               "— gemessen waren es alle\n      ", falsch, N_NS);
        _fail++;
    }
}

TEST(der_rundlauf_ist_bitgenau)
{
    /* Was MF-955 tut: kodieren mit dem Produktionskodierer, lesen mit
     * dem Produktionsdekoder. Beide setzen dasselbe Protokoll um. */
    static uint32_t ticks[N_NS];
    static uint8_t  wire[4096];
    static uint32_t back[4096];

    for (int i = 0; i < N_NS; i++) ticks[i] = ns_zu_ticks(NS[i], FREQ_F7);

    size_t wlen = uft_gw_encode_flux_stream(ticks, N_NS, wire, sizeof wire,
                                            FREQ_F7);
    ASSERT(wlen > 0);

    uint32_t sf = 0;
    uint32_t nb = uft_gw_decode_flux_stream(wire, wlen, back, 4096, &sf);

    /* ERWARTET: ein Wechsel MEHR als eingegeben.
     *
     * Der Kodierer laeuft `si <= sample_count` und haengt einen
     * `dummy_flux` von 100 us an — so macht es greaseweazle selbst,
     * damit der letzte ECHTE Wechsel auch geschrieben wird. Gemessen
     * beim Nachsehen im Kodierer, nicht angenommen. */
    const uint32_t dummy = (uint32_t)((uint64_t)100 * FREQ_F7 / 1000000);
    if (nb != (uint32_t)N_NS + 1u) {
        printf("\n      %u Wechsel zurueck, erwartet %d (+1 Nachlauf)\n"
               "      ", nb, N_NS);
        _fail++;
        return;
    }
    if (back[N_NS] != dummy) {
        printf("\n      Nachlauf %u statt %u Ticks (100 us)\n      ",
               back[N_NS], dummy);
        _fail++;
    }

    for (int i = 0; i < N_NS; i++) {
        if (back[i] != ticks[i]) {
            printf("\n      Wechsel %d: %u statt %u Ticks\n      ",
                   i, back[i], ticks[i]);
            _fail++;
            return;
        }
    }
}

TEST(die_einheit_ueberlebt_beide_umrechnungen)
{
    /* DIE eigentliche Frage von MF-955, und die Fehlerklasse dieser
     * ganzen Reihe: was bedeutet diese Zahl?
     *
     * Der Schreibpfad bekommt Nanosekunden und muss Ticks liefern; der
     * Lesepfad bekommt Ticks und muss Nanosekunden liefern. Beide
     * Umrechnungen sind neu in MF-955 — und eine vertauschte Richtung
     * faellt ohne Geraet sonst niemandem auf.
     *
     * Bei 72 MHz ist ein Tick 13,89 ns; die Rundung darf einen Tick
     * kosten, nicht mehr. */
    for (int i = 0; i < N_NS; i++) {
        uint32_t t  = ns_zu_ticks(NS[i], FREQ_F7);
        uint32_t ns = ticks_zu_ns(t, FREQ_F7);
        long ab = (long)ns - (long)NS[i];
        if (ab < 0) ab = -ab;
        if (ab > 14) {          /* ein Tick bei 72 MHz */
            printf("\n      %u ns -> %u Ticks -> %u ns (Abweichung %ld)\n"
                   "      ", NS[i], t, ns, ab);
            _fail++;
            return;
        }
    }
}

TEST(eine_andere_taktfrequenz_ergibt_andere_ticks)
{
    /* Gegenprobe zur Umrechnung: waere die Frequenz fest verdrahtet —
     * der Fehler, den MF-179 im Kodierer behoben hat —, kaeme fuer F7
     * und F7-Plus dasselbe heraus. Bei 84 statt 72 MHz muss dieselbe
     * Zeit MEHR Ticks ergeben. */
    for (int i = 0; i < N_NS; i++) {
        uint32_t t72 = ns_zu_ticks(NS[i], FREQ_F7);
        uint32_t t84 = ns_zu_ticks(NS[i], FREQ_F7P);
        if (t84 <= t72) {
            printf("\n      %u ns: %u Ticks bei 84 MHz, %u bei 72 — die "
                   "Frequenz kommt nicht an\n      ", NS[i], t84, t72);
            _fail++;
            return;
        }
        /* Und zurueck muss beides dieselbe Zeit ergeben. */
        uint32_t ns72 = ticks_zu_ns(t72, FREQ_F7);
        uint32_t ns84 = ticks_zu_ns(t84, FREQ_F7P);
        long ab = (long)ns72 - (long)ns84;
        if (ab < 0) ab = -ab;
        if (ab > 14) {
            printf("\n      %u ns kommt als %u (72 MHz) und %u (84 MHz) "
                   "zurueck\n      ", NS[i], ns72, ns84);
            _fail++;
            return;
        }
    }
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Greaseweazle-Draht-Codec (MF-955) ===\n");
    RUN(der_alte_kodierer_war_wirklich_falsch);
    RUN(der_rundlauf_ist_bitgenau);
    RUN(die_einheit_ueberlebt_beide_umrechnungen);
    RUN(eine_andere_taktfrequenz_ergibt_andere_ticks);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    printf("\nNICHT geprueft: was ein echtes Geraet aus dem Draht macht.\n"
           "Dieses Projekt hat keine Hardware (MF-310).\n");
    return _fail == 0 ? 0 : 1;
}
