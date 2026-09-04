/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_a2r_hilfsfunktionen_leben.c
 * @brief Die Selbsttests des A2R-Parsers, erstmals ausgefuehrt (MF-851).
 *
 * Die Faelle standen als `#ifdef UFT_UNIT_TESTS`-Block am Ende von
 * `src/parsers/a2r/uft_a2r_parser.c`. `UFT_UNIT_TESTS` wird im ganzen
 * Baum nirgends definiert — der Block wurde nie uebersetzt (P3-89).
 *
 * ── Grenze des Hebens ────────────────────────────────────────────────
 *
 * Zwei Zusagen des Originals sind hier NICHT enthalten:
 *
 *     assert(read_le16(buf) == 0x1234);
 *     assert(read_le32(buf) == 0x56781234);
 *
 * `read_le16`/`read_le32` sind `static inline` in der
 * Uebersetzungseinheit (uft_a2r_parser.c:98,103). Ein Test von aussen
 * kommt nicht heran. Sie werden mittelbar geprueft: jede Kopfangabe,
 * die dieser Parser liest, geht durch sie hindurch.
 *
 * Das ist die Kehrseite solcher Bloecke — sie erreichen, was sonst
 * niemand sieht, und genau deshalb duerfen sie nicht die EINZIGE
 * Pruefung sein: dieser hier lief nie.
 */
#include "uft/parsers/uft_a2r_parser.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-44s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                       _fail++; return; } } while (0)

TEST(fehlertexte)
{
    ASSERT(strcmp(a2r_error_string(A2R_OK), "OK") == 0);
    ASSERT(strcmp(a2r_error_string(A2R_ERR_BAD_MAGIC),
                  "Invalid A2R signature") == 0);
}

TEST(laufwerkstypen)
{
    ASSERT(strcmp(a2r_disk_type_string(1),
                  "5.25\" Single-Sided (Disk II)") == 0);
}

TEST(viertelspur_hin_und_zurueck)
{
    /* Apple-II-Laufwerke fahren in Vierteln; A2R zaehlt sie fortlaufend.
     * 35 = Spur 8, Viertel 3. */
    uint8_t track = 0, quarter = 0;
    a2r_quarter_to_track(35, &track, &quarter);
    ASSERT(track == 8);
    ASSERT(quarter == 3);
    ASSERT(a2r_track_to_quarter(8, 3) == 35);
}

TEST(viertelspur_ist_umkehrbar)
{
    /* GEGENPROBE, im Original nicht vorhanden: EIN Wertepaar beweist
     * keine Umrechnung. Ueber den ganzen Bereich muss Hin- und
     * Rueckweg dasselbe ergeben. */
    for (unsigned q = 0; q < 160; q++) {
        uint8_t t = 0, v = 0;
        a2r_quarter_to_track((uint8_t)q, &t, &v);
        if (a2r_track_to_quarter(t, v) != q) {
            printf("\n      Viertel %u -> Spur %u/%u -> %u\n      ",
                   q, t, v, a2r_track_to_quarter(t, v));
            _fail++;
            return;
        }
    }
}

TEST(umdrehungszeit_in_upm)
{
    /* 200 ms je Umdrehung sind 300 UPM. */
    double rpm = a2r_duration_to_rpm(200000.0);
    ASSERT(fabs(rpm - 300.0) < 0.1);
}

TEST(nicht_vorhandene_datei)
{
    ASSERT(a2r_is_valid_file("/nonexistent.a2r") == false);
    ASSERT(a2r_get_file_version("/nonexistent.a2r") == 0);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== A2R: Selbsttests erstmals ausgefuehrt (MF-851) ===\n");
    RUN(fehlertexte);
    RUN(laufwerkstypen);
    RUN(viertelspur_hin_und_zurueck);
    RUN(viertelspur_ist_umkehrbar);
    RUN(umdrehungszeit_in_upm);
    RUN(nicht_vorhandene_datei);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
