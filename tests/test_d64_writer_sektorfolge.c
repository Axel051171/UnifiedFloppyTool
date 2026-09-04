/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_d64_writer_sektorfolge.c
 * @brief Jeder Sektor genau einmal — auf JEDER Spur (MF-859).
 *
 * ── Der Fehler ───────────────────────────────────────────────────────
 *
 * `uft_d64_writer.c` fuehrt eine feste Tabelle mit 21 Eintraegen:
 *
 *     static const int standard_interleave[21] = {
 *         0,10,20,9,19,8,18,7,17,6,16,5,15,4,14,3,13,2,12,1,11 };
 *
 * und wendet sie auf JEDE Spur an:
 *
 *     sector = standard_interleave[i % 21];
 *     if (sector >= sector_count) sector = i;
 *
 * Die Tabelle ist fuer 21 Sektoren gerechnet — Zone 0, Spuren 1-17. Eine
 * D64 hat aber vier Zonen: 21, 19, 18 und 17 Sektoren. In den anderen
 * drei greift der Rueckfall, und er ersetzt einen zu grossen Wert durch
 * den LAUFINDEX. Der steht aber spaeter selbst noch in der Tabelle.
 *
 * ── Was dabei wirklich herauskommt ───────────────────────────────────
 *
 * Nicht „sequentiell statt interleavt", sondern DOPPELTE und FEHLENDE
 * Sektoren (nachgerechnet):
 *
 *     Zone       Sektoren   fehlend        doppelt
 *     Spur 1-17     21      keine          keine
 *     Spur 18-24    19      1, 11          2, 4
 *     Spur 25-30    18      1, 11, 12      2, 4, 6
 *     Spur 31-35    17      1, 11, 12      4, 6, 8
 *
 * **18 von 35 Spuren.** Ein echter 1541 faende auf Spur 18-24 die
 * Sektoren 1 und 11 nie — sie stehen nicht auf der Spur.
 *
 * ── Was dieser Test NICHT entscheidet ────────────────────────────────
 *
 * Ob ein 1541 physisch ueberhaupt interleavt formatiert, und nach
 * welcher Regel. Dazu widersprechen sich die Quellen (P3-110). Geprueft
 * wird hier nur die Eigenschaft, die unter JEDER Regel gelten muss:
 * **jeder Sektor der Spur kommt genau einmal vor.**
 *
 * Das ist bewusst schwaecher als eine Sollfolge — und deshalb
 * belastbar.
 */
#include "uft/uft_d64_writer.h"

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

/** Je Zone eine Vertreterspur. */
static const struct { int spur, sektoren; } ZONEN[] = {
    {  1, 21 }, { 18, 19 }, { 25, 18 }, { 31, 17 },
};

/**
 * Prueft die Reihenfolge einer Spur auf Vollstaendigkeit.
 * @return 0 wenn in Ordnung, sonst die Zahl der Maengel.
 */
static int folge_pruefen(int spur, int n)
{
    int *zaehler = (int *)calloc((size_t)n, sizeof(int));
    if (!zaehler) return -1;

    for (int i = 0; i < n; i++) {
        int s = uft_d64_sektor_an_position(spur, i);
        if (s < 0 || s >= n) {
            printf("\n      Spur %d Position %d: Sektor %d liegt ausserhalb "
                   "0..%d\n      ", spur, i, s, n - 1);
            free(zaehler);
            return 1;
        }
        zaehler[s]++;
    }

    int maengel = 0;
    for (int s = 0; s < n; s++) {
        if (zaehler[s] == 0) {
            printf("\n      Spur %d (%d Sektoren): Sektor %d FEHLT",
                   spur, n, s);
            maengel++;
        } else if (zaehler[s] > 1) {
            printf("\n      Spur %d (%d Sektoren): Sektor %d kommt %dmal vor",
                   spur, n, s, zaehler[s]);
            maengel++;
        }
    }
    if (maengel) printf("\n      ");
    free(zaehler);
    return maengel;
}

TEST(jede_zone_traegt_jeden_sektor_genau_einmal)
{
    /* DER ROTBEWEIS. Vor MF-859 fielen drei der vier Zonen durch. */
    int maengel = 0;
    for (size_t z = 0; z < sizeof ZONEN / sizeof *ZONEN; z++)
        maengel += folge_pruefen(ZONEN[z].spur, ZONEN[z].sektoren);
    if (maengel > 0) _fail++;
}

TEST(alle_35_spuren_nicht_nur_die_vertreter)
{
    /* Gegenprobe 1: vier Vertreterspuren koennten zufaellig durchgehen.
     * Geprueft wird die ganze Diskette. */
    int maengel = 0;
    for (int spur = 1; spur <= 35; spur++) {
        int n = d64_sectors_per_track(spur);
        if (n <= 0) { printf("\n      Spur %d: %d Sektoren\n      ", spur, n);
                      _fail++; return; }
        maengel += folge_pruefen(spur, n);
    }
    if (maengel > 0) _fail++;
}

TEST(zone_null_bleibt_wie_sie_war)
{
    /* Gegenprobe 2: die Spuren 1-17 waren KORREKT. Eine Reparatur, die
     * sie mitaendert, waere ein Rueckschritt — und sie ist die einzige
     * Zone, deren Reihenfolge im Baum je jemand aufgeschrieben hat. */
    static const int ERWARTET[21] = {
        0, 10, 20, 9, 19, 8, 18, 7, 17, 6, 16, 5, 15, 4, 14, 3, 13, 2, 12, 1, 11
    };
    for (int i = 0; i < 21; i++) {
        int s = uft_d64_sektor_an_position(1, i);
        if (s != ERWARTET[i]) {
            printf("\n      Spur 1 Position %d: %d statt %d\n      ",
                   i, s, ERWARTET[i]);
            _fail++;
            return;
        }
    }
}

TEST(die_directory_spur_hat_ihren_eigenen_versatz)
{
    /* Die reale 1541-DOS schaltet fuer Spur 18 eigens um: `secinc` steht
     * global auf 10 (`dskintsf.src`), und `nxdrbk` (`tst4.src`) sichert
     * den Wert, setzt 3, vergibt und setzt zurueck. Eine eigene Routine
     * dafuer — keine Fussnote.
     *
     * Zweite Hand: `lib1541img` (Felix Palmen) fuehrt unabhaengig
     *     .dirInterleave = 3, .fileInterleave = 10
     * (`src/lib/1541img/cbmdosfs.c:21-22`).
     *
     * Geprueft wird die FOLGE des Versatzes, nicht die Zahl selbst: bei
     * Versatz 3 liegen aufeinanderfolgende Positionen drei Sektoren
     * auseinander, bei 10 zehn. */
    const int n = d64_sectors_per_track(18);
    ASSERT(n == 19);

    int s0 = uft_d64_sektor_an_position(18, 0);
    int s1 = uft_d64_sektor_an_position(18, 1);
    int d = s1 - s0;
    if (d < 0) d += n;

    if (d != 3) {
        printf("\n      Spur 18: Abstand %d statt 3 "
               "(Sektor %d dann %d)\n      ", d, s0, s1);
        _fail++;
    }
}

TEST(eine_datenspur_gleicher_groesse_behaelt_zehn)
{
    /* Gegenprobe 3: die Sonderregel gilt NUR fuer Spur 18. Spur 19 hat
     * dieselben 19 Sektoren und muss bei Versatz 10 bleiben — sonst
     * waere „Directory-Sonderfall" in Wahrheit „Zonen-Sonderfall". */
    const int n = d64_sectors_per_track(19);
    ASSERT(n == 19);

    int s0 = uft_d64_sektor_an_position(19, 0);
    int s1 = uft_d64_sektor_an_position(19, 1);
    int d = s1 - s0;
    if (d < 0) d += n;

    if (d != 10) {
        printf("\n      Spur 19: Abstand %d statt 10\n      ", d);
        _fail++;
    }
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== D64-Writer: jeder Sektor genau einmal (MF-859) ===\n");
    RUN(jede_zone_traegt_jeden_sektor_genau_einmal);
    RUN(alle_35_spuren_nicht_nur_die_vertreter);
    RUN(zone_null_bleibt_wie_sie_war);
    RUN(die_directory_spur_hat_ihren_eigenen_versatz);
    RUN(eine_datenspur_gleicher_groesse_behaelt_zehn);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
