/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_d64_schreiber_symmetrie.c
 * @brief Was der D64-Leser annimmt, muss der D64-Schreiber erzeugen (MF-908)
 *
 * ── Was gemessen wurde ────────────────────────────────────────────────────
 *
 * Seit MF-871 nimmt der Leser VIER Spurzahlen an — `d64_is_valid_size()`
 * in `src/formats/d64/uft_d64_parser_v3.c` kennt 35, 40, 41 und 42, je
 * mit und ohne Fehlerkarte.
 *
 * Der Schreiber kennt ZWEI. `d64_create()` in
 * `src/formats/c64/uft_d64_g64.c` beginnt mit
 *
 *     if (num_tracks != 35 && num_tracks != 40) return NULL;
 *
 * Der Baum kann also D64-Abbilder LESEN, die er nicht SCHREIBEN kann.
 *
 * ── Warum das kein Schoenheitsfehler ist ──────────────────────────────────
 *
 * Die Wandlung G64 -> D64 steht in `src/core/uft_roundtrip.c` als
 * ANGEBOTENER Pfad mit dem Urteil `UFT_RT_LOSSY_DOCUMENTED` und einer
 * Verlustliste, die sich vollstaendig gibt: „680 von 683 Sektoren
 * bitgleich; ab: Spur 17/0, Spur 18/0 (BAM), Spur 18/1 (Verzeichnis);
 * GCR-Kodierung und Fehlerinfo gehen bauartbedingt verloren".
 *
 * Die Spurabschneidung steht NICHT darin. Und sie ist real: der Wandler
 * sondiert die Spuren 36..40 (`uft_cbm_d64_decode_via_plugin()`), sieht
 * 41 und 42 also nie, und koennte sie selbst dann nicht ablegen, weil
 * `d64_create()` sie ablehnt. Eine G64 mit 42 Spuren verliert zwei —
 * still, auf einem Pfad, dessen Verlustliste den Anspruch erhebt,
 * vollstaendig zu sein.
 *
 * ── Woher die Zahlen kommen ───────────────────────────────────────────────
 *
 * NICHT aus einer neuen Tabelle. `uft_cbm_total_blocks(UFT_CBM_1541, n)`
 * rechnet sie aus der Zonenaufteilung, die seit MF-434 der SSOT ist —
 * dieselbe Quelle, aus der die bestehenden Konstanten stammen. Der Test
 * prueft das mit: 35 -> 683 und 40 -> 768 muessen die vorhandenen
 * `D64_BLOCKS_*` treffen, sonst ist die Herleitung falsch und nicht die
 * Konstante.
 *
 * ── Zwei Tueren, eine Antwort ─────────────────────────────────────────────
 *
 * Der Test treibt BEIDE echten Tueren an, keine nachgebaute:
 *   `d64_create()`      — der Schreiber, oeffentlich im Header
 *   `d64_size_is_valid()` — die schmale Pruef-Tuer aus MF-871
 * und haelt sie gegeneinander. Ein Nachbau der Strukturen waere der
 * Fehler aus MF-796 (EDSK: Typen von Hand nachdeklariert, 40 gegen 32
 * Byte je Sektor, Plugin lieferte still keinen Sektor).
 */

#include "uft/formats/c64/uft_d64_g64.h"
#include "uft/formats/cbm/uft_cbm_geometry.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Die schmale Pruef-Tuer des Lesers (MF-871). Bewusst nur diese eine
 * Funktion, kein Aufbau — siehe den Kommentar an ihrer Definition. */
bool d64_size_is_valid(size_t size, uint8_t *tracks, bool *has_errors);

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-42s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

/* Die Spurzahlen, die der LESER annimmt (MF-871). */
static const int kLesbar[] = { 35, 40, 41, 42 };
static const int kLesbarAnzahl = 4;

/* ─────────────────────────────────────────────────────────────────────────
 *  1. Die Blockzahlen der bestehenden Konstanten kommen aus dem SSOT.
 *
 *  Zuerst die Herleitung pruefen, dann darauf bauen. Waere sie falsch,
 *  waeren alle weiteren Zahlen dieses Tests wertlos.
 * ───────────────────────────────────────────────────────────────────────── */
TEST(ssot_deckt_die_bestehenden_konstanten)
{
    ASSERT(uft_cbm_total_blocks(UFT_CBM_1541, 35) == D64_BLOCKS_35);
    ASSERT(uft_cbm_total_blocks(UFT_CBM_1541, 40) == D64_BLOCKS_40);
    /* Und die beiden, um die es geht — gerechnet, nicht gesetzt. */
    ASSERT(uft_cbm_total_blocks(UFT_CBM_1541, 41) == 785);
    ASSERT(uft_cbm_total_blocks(UFT_CBM_1541, 42) == 802);
}

/* ─────────────────────────────────────────────────────────────────────────
 *  2. Der Schreiber erzeugt jede Spurzahl, die der Leser annimmt.
 *
 *  Heute rot fuer 41 und 42: `d64_create()` gibt NULL zurueck.
 * ───────────────────────────────────────────────────────────────────────── */
TEST(schreiber_kennt_was_der_leser_kennt)
{
    for (int i = 0; i < kLesbarAnzahl; i++) {
        const int n = kLesbar[i];
        d64_image_t *img = d64_create(n);
        if (!img) {
            printf("\n         d64_create(%d) lieferte NULL, "
                   "der Leser nimmt %d aber an", n, n);
            ASSERT(img != NULL);
            return;
        }
        ASSERT(img->num_tracks == n);
        ASSERT(img->num_blocks == uft_cbm_total_blocks(UFT_CBM_1541, n));
        d64_free(img);
    }
}

/* ─────────────────────────────────────────────────────────────────────────
 *  3. Und der Leser nimmt an, was der Schreiber erzeugt (die Gegenrichtung).
 *
 *  Ein Abbild, das dieser Baum schreibt und dann nicht wieder aufmachen
 *  kann, waere schlimmer als eines, das er gar nicht schreibt.
 * ───────────────────────────────────────────────────────────────────────── */
TEST(leser_nimmt_an_was_der_schreiber_erzeugt)
{
    for (int i = 0; i < kLesbarAnzahl; i++) {
        const int n = kLesbar[i];
        d64_image_t *img = d64_create(n);
        if (!img) continue;          /* Pruefung 2 meldet das bereits */

        uint8_t *daten = NULL;
        size_t   groesse = 0;
        ASSERT(d64_save_buffer(img, &daten, &groesse, false) == 0);
        ASSERT(daten != NULL);

        uint8_t spuren = 0;
        bool    fehlerkarte = true;
        ASSERT(d64_size_is_valid(groesse, &spuren, &fehlerkarte));
        ASSERT(spuren == n);
        ASSERT(fehlerkarte == false);

        free(daten);
        d64_free(img);
    }
}

/* ─────────────────────────────────────────────────────────────────────────
 *  4. WAECHTER: was kein gueltiges D64 ist, erzeugt der Schreiber nicht.
 *
 *  36..39 sind keine D64-Ausdehnungen — der Leser weist sie ab, der
 *  Schreiber muss es auch. Sonst waere die Symmetrie in die falsche
 *  Richtung hergestellt: ein Erkenner, der nicht "nein" sagen kann
 *  (MF-729), nur eben auf der Schreibseite.
 * ───────────────────────────────────────────────────────────────────────── */
TEST(waechter_ungueltige_spurzahl_wird_abgelehnt)
{
    const int ungueltig[] = { 0, 1, 34, 36, 37, 38, 39, 43, 70, 255 };
    for (unsigned i = 0; i < sizeof(ungueltig) / sizeof(ungueltig[0]); i++) {
        d64_image_t *img = d64_create(ungueltig[i]);
        if (img) {
            printf("\n         d64_create(%d) lieferte ein Abbild",
                   ungueltig[i]);
            d64_free(img);
            ASSERT(false);
            return;
        }
        /* Und der Leser weist die entsprechende Groesse ebenfalls ab. */
        const int bl = uft_cbm_total_blocks(UFT_CBM_1541, ungueltig[i]);
        if (bl > 0) {
            uint8_t spuren = 0; bool fk = false;
            ASSERT(!d64_size_is_valid((size_t)bl * 256, &spuren, &fk));
        }
    }
}

int main(void)
{
    printf("D64: der Schreiber kennt, was der Leser kennt (MF-908)\n");
    RUN(ssot_deckt_die_bestehenden_konstanten);
    RUN(schreiber_kennt_was_der_leser_kennt);
    RUN(leser_nimmt_an_was_der_schreiber_erzeugt);
    RUN(waechter_ungueltige_spurzahl_wird_abgelehnt);
    printf("\n  %d bestanden, %d gefallen\n", _pass, _fail);
    return _fail ? 1 : 0;
}
