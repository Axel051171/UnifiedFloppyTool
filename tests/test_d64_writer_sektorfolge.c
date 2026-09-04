/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_d64_writer_sektorfolge.c
 * @brief Physische Spurbelegung und CBM-DOS-Vergabe (MF-859, berichtigt MF-861).
 *
 * ── Was MF-859 falsch gemacht hat ────────────────────────────────────
 *
 * MF-859 hat einen echten Fehler behoben — die 21er-Tabelle erzeugte auf
 * 18 von 35 Spuren doppelte und fehlende Sektoren — und dabei die
 * FALSCHE EBENE festgeschrieben: die Vergabereihenfolge des DOS wurde
 * als physisches Soll der Spur geprueft.
 *
 * MF-859 hat die Frage selbst als P3-111 offen gelassen, statt sie zu
 * klaeren. Das war zu wenig. **Ein Rotbeweis, der das Falsche beweist,
 * ist schlimmer als keiner** — er macht den Irrtum haltbar.
 *
 * ── Was gilt ─────────────────────────────────────────────────────────
 *
 * **Ein 1541 legt die Sektoren AUFSTEIGEND auf die Spur** (0, 1, 2, …).
 * Der Versatz 10 (Directory: 3) lebt ausschliesslich in der
 * BLOCKVERGABE des DOS.
 *
 * Belegt durch fuenf unabhaengige Umsetzungen und eine Messung:
 *   ROM 901229-01  Formatierroutine $FC36-$FD1C
 *   OpenCBM        cbmformat.a65 (PrepSec/NxtSec, laeuft auf Hardware)
 *   VICE           fsimage-dxx.c:262
 *   nibtools       fileio.c:760
 *   1541ultimate   disk_image.cc:251
 *   Messung        cbm_fixtures/fdit_uft35.g64, 35/35 Spuren aufsteigend
 *
 * Und der eigene Baum sagt es auch: der ANGEBOTENE Wandlungspfad
 * D64 -> G64 laeuft ueber `uft_d64_g64.c::build_gcr_track()` und schreibt
 * dort seit jeher `for (int s = 0; s < num_sectors; s++)`.
 * `d64_write_track_gcr()` war die einzige Stelle mit einer anderen
 * Meinung — und hat ausserhalb ihres Moduls keinen Aufrufer.
 *
 * ── Die Vergaberegel ist NICHT modular ───────────────────────────────
 *
 * Beim Ueberlauf zieht das DOS die Sektorzahl ab und danach noch eins,
 * sofern das Ergebnis nicht 0 ist (ROM FNDNXT $F189-$F193; gleichlautend
 * `lib1541img cbmdosfs.c:126-134`). Fuer 21 Sektoren:
 *
 *     0, 10, 20,  8, 18,  6, …     statt     0, 10, 20,  9, 19,  8, …
 *
 * Die modulare Fassung entspricht dort dem Schalter
 * `CFF_SIMPLEINTERLEAVE`, nicht dem DOS. MF-859 hatte die modulare.
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

TEST(die_spur_ist_aufsteigend_belegt)
{
    /* DER ROTBEWEIS, berichtigt. Vorher stand hier die Vergabefolge als
     * physisches Soll. */
    for (int spur = 1; spur <= 35; spur++) {
        int n = d64_sectors_per_track(spur);
        ASSERT(n > 0);
        for (int i = 0; i < n; i++) {
            int s = uft_d64_sektor_an_position(spur, i);
            if (s != i) {
                printf("\n      Spur %d Position %d: Sektor %d statt %d\n"
                       "      -> ein 1541 formatiert aufsteigend\n      ",
                       spur, i, s, i);
                _fail++;
                return;
            }
        }
    }
}

TEST(ausserhalb_der_spur_gibt_es_keinen_sektor)
{
    /* Gegenprobe: die Funktion darf nicht einfach die Position
     * durchreichen — jenseits der Sektorzahl gibt es nichts. */
    ASSERT(uft_d64_sektor_an_position(1, 21) == -1);
    ASSERT(uft_d64_sektor_an_position(31, 17) == -1);
    ASSERT(uft_d64_sektor_an_position(1, -1) == -1);
    ASSERT(uft_d64_sektor_an_position(0, 0) == -1);
    /* Spur 36 ist GUELTIG — der Baum fuehrt 40-Spur-Disketten
     * (d64_sectors_per_track akzeptiert 1..40). Die erste Fassung dieses
     * Falls nahm 35 als Obergrenze an und war damit selbst falsch. */
    ASSERT(uft_d64_sektor_an_position(36, 0) == 0);
    ASSERT(uft_d64_sektor_an_position(41, 0) == -1);
}

TEST(die_vergabe_folgt_der_dos_regel_nicht_der_modularen)
{
    /* ROM FNDNXT $F189-$F193: beim Ueberlauf Sektorzahl abziehen, dann
     * noch eins, sofern nicht 0. Gleichlautend lib1541img
     * cbmdosfs.c:126-134.
     *
     * Fuer 21 Sektoren ergibt das 0, 10, 20, 8, 18, 6, … — die modulare
     * Fassung ergaebe 0, 10, 20, 9, 19, 8, … und war MF-859s Stand. */
    static const int ERWARTET[6] = { 0, 10, 20, 8, 18, 6 };
    for (int i = 0; i < 6; i++) {
        int s = uft_d64_vergabe_an_position(1, i);
        if (s != ERWARTET[i]) {
            printf("\n      Spur 1 Vergabe %d: %d statt %d\n"
                   "      -> das ist die modulare Regel, nicht die des "
                   "DOS\n      ", i, s, ERWARTET[i]);
            _fail++;
            return;
        }
    }
}

TEST(die_vergabe_trifft_jeden_sektor_genau_einmal)
{
    /* Der urspruengliche Befund aus MF-859 bleibt gueltig: die alte
     * 21er-Tabelle erzeugte doppelte und fehlende Sektoren. Auf der
     * Vergabeebene muss jede Spur weiterhin eine vollstaendige
     * Permutation liefern. */
    for (int spur = 1; spur <= 35; spur++) {
        int n = d64_sectors_per_track(spur);
        int *z = (int *)calloc((size_t)n, sizeof(int));
        ASSERT(z != NULL);

        for (int i = 0; i < n; i++) {
            int s = uft_d64_vergabe_an_position(spur, i);
            if (s < 0 || s >= n) {
                printf("\n      Spur %d Vergabe %d: %d liegt ausserhalb\n"
                       "      ", spur, i, s);
                free(z); _fail++; return;
            }
            z[s]++;
        }
        for (int s = 0; s < n; s++) {
            if (z[s] != 1) {
                printf("\n      Spur %d (%d Sekt): Sektor %d kommt %dmal "
                       "vor\n      ", spur, n, s, z[s]);
                free(z); _fail++; return;
            }
        }
        free(z);
    }
}

TEST(die_directory_spur_hat_ihren_eigenen_versatz)
{
    /* ROM: SECINC steht global auf 10 ($EBCD), NXDRBK ($D497) setzt 3
     * fuer die Directory-Spur — eine eigene Routine dafuer. Zweite Hand:
     * lib1541img `.dirInterleave = 3`, `.fileInterleave = 10`. */
    const int n = d64_sectors_per_track(18);
    ASSERT(n == 19);

    int d = uft_d64_vergabe_an_position(18, 1)
          - uft_d64_vergabe_an_position(18, 0);
    if (d < 0) d += n;
    if (d != 3) {
        printf("\n      Spur 18: Abstand %d statt 3\n      ", d);
        _fail++;
    }
}

TEST(eine_datenspur_gleicher_groesse_behaelt_zehn)
{
    /* Gegenprobe: die Sonderregel gilt NUR fuer Spur 18. Spur 19 hat
     * dieselben 19 Sektoren — sonst waere „Directory-Sonderfall" in
     * Wahrheit „Zonen-Sonderfall". */
    const int n = d64_sectors_per_track(19);
    ASSERT(n == 19);

    int d = uft_d64_vergabe_an_position(19, 1)
          - uft_d64_vergabe_an_position(19, 0);
    if (d < 0) d += n;
    if (d != 10) {
        printf("\n      Spur 19: Abstand %d statt 10\n      ", d);
        _fail++;
    }
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== D64: Spurbelegung und Vergabe (MF-861) ===\n");
    RUN(die_spur_ist_aufsteigend_belegt);
    RUN(ausserhalb_der_spur_gibt_es_keinen_sektor);
    RUN(die_vergabe_folgt_der_dos_regel_nicht_der_modularen);
    RUN(die_vergabe_trifft_jeden_sektor_genau_einmal);
    RUN(die_directory_spur_hat_ihren_eigenen_versatz);
    RUN(eine_datenspur_gleicher_groesse_behaelt_zehn);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
