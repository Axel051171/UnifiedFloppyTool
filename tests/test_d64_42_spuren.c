/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_d64_42_spuren.c
 * @brief D64 hat zwei Tueren, und sie waren sich uneinig (MF-871)
 *
 * -- Der Befund -------------------------------------------------------
 *
 * D64 wird in diesem Baum von ZWEI Lesern geoeffnet, beide unter
 * `src/formats/d64/`:
 *
 *   uft_d64_plugin.c      ueber die Format-Registry erreichbar
 *   uft_d64_parser_v3.c   ueber uft_v3_bridge -> uft_advanced_open()
 *
 * Das Plugin nimmt seit MF-350 **acht** Dateigroessen an (35/40/41/42
 * Spuren, je mit und ohne Fehlerkarte) und fuehrt sie in seinem
 * Kommentar sogar ausgeschrieben auf (`uft_d64_plugin.c:34-41`). Der
 * v3-Leser kannte **vier** — 41 und 42 Spuren fielen bei ihm durch.
 *
 * Das ist dasselbe Muster wie MF-870 (TD0) und MF-796 (EDSK), einen
 * Commit spaeter und in einer anderen Formatfamilie: zwei
 * Umsetzungen im selben Verzeichnis, jede in sich schluessig, keine
 * gegen die andere gemessen.
 *
 * -- Ehrlich zur Tragweite --------------------------------------------
 *
 * Die reparierte Tuer hat heute KEINEN Leser: `uft_advanced_open()`
 * hat keinen Aufrufer (gemessen ueber `git ls-files`), und
 * `uft_v3_bridge.c:28` sagt das seit MF-442 selbst. MF-871 bewegt
 * daher KEINE der vier Release-Kennzahlen. Es entfernt einen
 * Auseinanderlauf auf einem mitgebauten, exportierten Pfad — und der
 * naechste Fall dieser Art faellt jetzt hier auf, statt erst beim
 * Benutzer.
 *
 * -- Warum die Zahlen abgeleitet sind ---------------------------------
 *
 * Die Sektorzahl je Spur ist eine Eigenschaft des LAUFWERKS. Sie steht
 * an einer Stelle (`uft_cbm_geometry.c`, gegen ein c1541-Referenzabbild
 * verifiziert) — und `scripts/cbm_zone_gate.py` hat **24 Kopien**
 * davon gefunden.
 *
 * Keine der neuen Groessen ist eingetippt. Dieselbe Formel liefert:
 *
 *     1541, 35 Spuren  683 Bloecke  174848 / 175531
 *     1541, 40 Spuren  768          196608 / 197376
 *     1541, 41 Spuren  785          200960 / 201745
 *     1541, 42 Spuren  802          205312 / 206114
 *
 * Alle acht stimmen byteweise mit denen ueberein, die das Plugin seit
 * MF-350 fuehrt. Dass die Ableitung die BESTEHENDEN, funktionierenden
 * Werte reproduziert, ist die Begruendung fuer die neuen.
 *
 * -- Was das bestehende Tor nicht kann --------------------------------
 *
 * `scripts/cbm_zone_gate.py` prueft, ob die Kopien MITEINANDER
 * uebereinstimmen. Das ist Konsistenz, nicht Richtigkeit: 24 Kopien
 * koennen eintraechtig alle falsch sein.
 *
 * Die Blockzahl ist die AEUSSERE Invariante, die das faengt. Belegt an
 * einem fremden Fall: ein Layout-Generator aus einem Fremdbuendel
 * verwendete `{18,19,21,22}` statt `{17,18,19,21}` und kam damit auf
 * 745 Bloecke bei 35 Spuren. In sich widerspruchsfrei — und diese
 * Pruefung haette es beim ersten Lauf gemeldet.
 */
#include "uft/formats/cbm/uft_cbm_geometry.h"
#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Aus src/formats/d64/uft_d64_parser_v3.c — nicht statisch deklariert;
 * die Groessen werden ueber die oeffentliche Pruefung abgefragt, damit
 * der Test keine Konstanten nachdeklariert (die Falle aus MF-796). */
bool d64_size_is_valid(size_t size, uint8_t *tracks, bool *has_errors);

/* Die ZWEITE Tuer: das ueber die Registry erreichbare Plugin. Beide
 * werden im selben Testbinaer gebunden — nur so laesst sich messen, ob
 * sie sich einig sind. */
extern const uft_format_plugin_t uft_format_plugin_d64;

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-46s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                       _fail++; return; } } while (0)

#define D64_SEKTORGROESSE 256

/* Die bekannten Kennzahlen. Sie sind der Bezug, nicht das Ergebnis. */
#define BLOCKS_35 683
#define BLOCKS_40 768
#define BLOCKS_42 802

TEST(die_zonentabelle_ergibt_die_bekannten_blockzahlen)
{
    /* DIE AEUSSERE INVARIANTE. Ohne sie ist die Zonentabelle nur mit
     * sich selbst im Einklang. */
    struct { int spuren; int soll; } F[] = {
        { 35, BLOCKS_35 }, { 40, BLOCKS_40 }, { 42, BLOCKS_42 },
    };
    for (size_t i = 0; i < sizeof F / sizeof F[0]; i++) {
        int ist = uft_cbm_total_blocks(UFT_CBM_1541, F[i].spuren);
        if (ist != F[i].soll) {
            printf("\n      %d Spuren: %d Bloecke, erwartet %d\n"
                   "      -> die Sektorzahlen je Zone pruefen\n      ",
                   F[i].spuren, ist, F[i].soll);
            _fail++;
            return;
        }
    }
}

TEST(die_d64_groessen_folgen_aus_der_zonentabelle)
{
    /* Die vier BESTEHENDEN Groessen werden aus derselben Formel
     * nachgerechnet wie die zwei neuen. Stimmt die Formel fuer die
     * bestehenden, traegt sie auch die neuen. */
    struct { int spuren; size_t roh; size_t mit_karte; } F[] = {
        { 35, 174848u, 175531u },
        { 40, 196608u, 197376u },
        { 41, 200960u, 201745u },
        { 42, 205312u, 206114u },
    };
    for (size_t i = 0; i < sizeof F / sizeof F[0]; i++) {
        int b = uft_cbm_total_blocks(UFT_CBM_1541, F[i].spuren);
        size_t roh = (size_t)b * D64_SEKTORGROESSE;
        size_t mit = roh + (size_t)b;
        if (roh != F[i].roh || mit != F[i].mit_karte) {
            printf("\n      %d Spuren: %zu / %zu, erwartet %zu / %zu\n      ",
                   F[i].spuren, roh, mit, F[i].roh, F[i].mit_karte);
            _fail++;
            return;
        }
    }
}

TEST(die_directory_spur_hat_19_sektoren)
{
    /* Gegenprobe an einem Einzelwert, damit „Summe stimmt" nicht durch
     * zwei sich aufhebende Fehler zustande kommen kann. */
    ASSERT(uft_cbm_sectors_per_track(UFT_CBM_1541, 18) == 19);
    ASSERT(uft_cbm_sectors_per_track(UFT_CBM_1541,  1) == 21);
    ASSERT(uft_cbm_sectors_per_track(UFT_CBM_1541, 35) == 17);
    ASSERT(uft_cbm_sectors_per_track(UFT_CBM_1541, 42) == 17);
}

TEST(es_gibt_keine_22_sektor_zone)
{
    /* Der fremde Layout-Generator kam auf 745 Bloecke, weil er eine
     * 22-Sektor-Zone annahm. Ein 1541 hat keine. */
    for (int t = 1; t <= 42; t++) {
        int s = uft_cbm_sectors_per_track(UFT_CBM_1541, t);
        if (s < 17 || s > 21) {
            printf("\n      Spur %d: %d Sektoren — ausserhalb 17..21\n      ",
                   t, s);
            _fail++;
            return;
        }
    }
}

TEST(jenseits_von_42_gibt_es_keine_spur)
{
    /* Die belegte Obergrenze. Werkzeuge der Zeit erlaubten die EINGABE
     * von mehr — „Copy+ 45 Tracks" prueft gegen 45 statt 40, und die
     * beiden Fassungen unterscheiden sich in genau zwei ASCII-Ziffern.
     * Das sagt etwas ueber das Werkzeug, nichts ueber das Medium. */
    ASSERT(uft_cbm_max_track(UFT_CBM_1541) == 42);
    ASSERT(uft_cbm_total_blocks(UFT_CBM_1541, 43) == 0);
    ASSERT(uft_cbm_total_blocks(UFT_CBM_1541, 45) == 0);
    ASSERT(uft_cbm_total_blocks(UFT_CBM_1541,  0) == 0);
}

/* Die acht Groessen, die ein D64 haben darf. Die Liste ist die
 * ERWARTUNG, gegen die BEIDE Tueren geprueft werden — sie steht hier
 * einmal, nicht je Tuer einmal. Genau das war der Fehler. */
static const struct { size_t groesse; uint8_t spuren; bool fehler; }
GUELTIG[] = {
    { 174848u, 35, false }, { 175531u, 35, true },
    { 196608u, 40, false }, { 197376u, 40, true },
    { 200960u, 41, false }, { 201745u, 41, true },
    { 205312u, 42, false }, { 206114u, 42, true },
};
#define GUELTIG_N (sizeof GUELTIG / sizeof GUELTIG[0])

TEST(beide_d64_tueren_nehmen_dieselben_groessen)
{
    /* DER EIGENTLICHE BEFUND, und der Rotbeweis zugleich.
     *
     * D64 hat ZWEI Leser im selben Verzeichnis:
     *
     *   uft_d64_plugin.c      ueber die Registry erreichbar
     *   uft_d64_parser_v3.c   ueber uft_v3_bridge -> uft_advanced_open
     *
     * Das Plugin nimmt seit MF-350 alle acht Groessen; der v3-Leser
     * kannte vier. Vor MF-871 faellt dieser Fall fuer VIER der acht
     * Groessen um (41 und 42, je mit und ohne Fehlerkarte).
     *
     * Beide Richtungen werden geprueft: keine Tuer darf mehr annehmen
     * als die andere. Eine Pruefung, die nur eine Richtung kennt, waere
     * gruen, sobald eine Tuer alles durchlaesst. */
    for (size_t i = 0; i < GUELTIG_N; i++) {
        size_t g = GUELTIG[i].groesse;
        uint8_t spuren = 0;
        bool fehler = false;
        int konfidenz = 0;

        bool v3 = d64_size_is_valid(g, &spuren, &fehler);
        bool pl = uft_format_plugin_d64.probe(NULL, 0, g, &konfidenz);

        if (v3 != pl) {
            printf("\n      %zu Byte: v3=%d, Plugin=%d — die Tueren "
                   "sind sich uneinig\n      ", g, (int)v3, (int)pl);
            _fail++;
            return;
        }
        if (!v3) {
            printf("\n      %zu Byte (%u Spuren) von BEIDEN abgewiesen"
                   "\n      ", g, GUELTIG[i].spuren);
            _fail++;
            return;
        }
        if (spuren != GUELTIG[i].spuren || fehler != GUELTIG[i].fehler) {
            printf("\n      %zu Byte: %u Spuren/%d, erwartet %u/%d\n      ",
                   g, spuren, (int)fehler,
                   GUELTIG[i].spuren, (int)GUELTIG[i].fehler);
            _fail++;
            return;
        }
    }
}

TEST(beide_tueren_weisen_dasselbe_ab)
{
    /* Die Gegenprobe. Ohne sie waeren zwei Tueren, die BEIDE alles
     * annehmen, ebenso "einig" wie zwei richtige. */
    static const size_t UNGUELTIG[] = {
        0u, 1u, 12345u,
        174847u, 175532u,          /* je ein Byte neben 35 Spuren   */
        200959u, 201746u,          /* je ein Byte neben 41 Spuren   */
        205311u, 205313u, 206115u, /* je ein Byte neben 42 Spuren   */
        218112u,                   /* die 45 Spuren aus der Eingabemaske */
    };
    for (size_t i = 0; i < sizeof UNGUELTIG / sizeof UNGUELTIG[0]; i++) {
        uint8_t spuren; bool fehler; int konfidenz = 0;
        bool v3 = d64_size_is_valid(UNGUELTIG[i], &spuren, &fehler);
        bool pl = uft_format_plugin_d64.probe(NULL, 0, UNGUELTIG[i],
                                              &konfidenz);
        if (v3 || pl) {
            printf("\n      %zu Byte angenommen: v3=%d, Plugin=%d\n      ",
                   UNGUELTIG[i], (int)v3, (int)pl);
            _fail++;
            return;
        }
    }
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== D64: zwei Tueren, eine Groessenmenge (MF-871) ===\n");
    RUN(die_zonentabelle_ergibt_die_bekannten_blockzahlen);
    RUN(die_d64_groessen_folgen_aus_der_zonentabelle);
    RUN(die_directory_spur_hat_19_sektoren);
    RUN(es_gibt_keine_22_sektor_zone);
    RUN(jenseits_von_42_gibt_es_keine_spur);
    RUN(beide_d64_tueren_nehmen_dieselben_groessen);
    RUN(beide_tueren_weisen_dasselbe_ab);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
