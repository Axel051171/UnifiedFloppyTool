/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_kryoflux_schreibweg.c
 * @brief Der KryoFlux-Schreibweg: was da ist, was er prueft (MF-1045)
 *
 * ── Woher dieser Test kommt ──────────────────────────────────────────
 *
 * `docs/CAPABILITIES.md` fuehrte KryoFlux Write als `-` mit der Legende
 * „protokoll-bedingt nicht vorgesehen (z.B. KryoFlux Write — read-only
 * by design)". Das ist eine Aussage ueber das GERAET. Belegt war sie
 * durch eine Zeile ohne Quelle in `.claude/CLAUDE.md`
 * (`// KryoFlux ist read-only`), und `kryoflux_provider_v2.h` berief
 * sich wiederum auf CLAUDE.md — ein Zirkel aus drei Dokumenten und
 * keiner Messung.
 *
 * Gemessen widerspricht der eigene Baum: `src/hal/uft_kryoflux_dtc.c`
 * fuehrt einen vollstaendigen Schreiber von 75 Zeilen
 * (`uft_kf_write_track`), der `dtc -w …` baut und ausfuehrt — und
 * ueber `git ls-files` gemessen ruft ihn NIEMAND.
 *
 * Die Absage stimmte also im Ergebnis (mit einem KryoFlux kann UFT
 * nicht schreiben) und irrte in der Begruendung. Seit MF-1045 sagt die
 * Tafel das so; dieser Test nagelt die Verhaltensseite fest.
 *
 * ── Was hier geprueft wird, und was NICHT ────────────────────────────
 *
 * NICHT geprueft: ob ein KryoFlux schreiben KANN. Das waere eine
 * Messung am Geraet (es gibt keines, MF-310) oder am echten DTC (der
 * liegt nicht im Baum; `tools/hw_simulators/` haelt nur Simulatoren).
 * Diese Frage steht offen als P3-341, und dieser Test beantwortet sie
 * ausdruecklich nicht.
 *
 * Geprueft wird der VERTRAG der vorhandenen Funktionen:
 *
 *   1. `uft_kf_write_supported()` misst NICHT, was sein Name sagt.
 *      Der Rotbeweis dazu braucht keine Hardware — siehe unten.
 *   2. Der Schreiber sagt ab, statt still zu gelingen: NULL-Konfig,
 *      NULL-Fluss, Laenge 0 und „kein DTC" enden alle auf -1.
 *   3. Ohne DTC wird KEIN Unterprozess gestartet, und `last_error`
 *      traegt einen Grund.
 *
 * ── Der Rotbeweis, der ohne Geraet auskommt ──────────────────────────
 *
 * `uft_kf_write_supported()` traegt in seinem eigenen Rumpf das
 * Gestaendnis:
 *
 *     // Check DTC version - write support was added in firmware 3.0+
 *     // For now, assume write is supported if DTC is available
 *     return uft_kf_is_available(config);
 *
 * Die im Kommentar genannte Versionspruefung gibt es nicht, und
 * `uft_kf_is_available()` liefert nur `cfg->dtc_found` — gesetzt von
 * `uft_kf_set_dtc_path()`, das einzig fragt, ob die Datei AUSFUEHRBAR
 * ist. Daraus folgt ein Beweis am Objekt: zeigt man die Konfiguration
 * auf eine beliebige ausfuehrbare Datei — etwa auf DIESES TESTPROGRAMM
 * —, dann antwortet `uft_kf_write_supported()` mit `true`. Es hat nie
 * erfahren, ob dort DTC liegt, geschweige denn welche Fassung.
 *
 * Das ist dieselbe Klasse wie der „Erkenner, der nie nein sagt": eine
 * Frage, deren Antwort nicht von dem abhaengt, wonach sie fragt.
 *
 * Der Schreiber wird dabei bewusst NICHT aufgerufen, wenn der Pfad auf
 * das Testprogramm zeigt — das wuerde `popen()` auf das Testprogramm
 * selbst absetzen.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/hal/uft_kryoflux.h"

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-52s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                       _fail++; return; } } while (0)

/* Pfad dieses Testprogramms — aus main() gesetzt. Er ist garantiert
 * ausfuehrbar und garantiert NICHT DTC. Genau darin liegt der Beweis. */
static const char *g_selbst = NULL;

/* ────────────────────────────────────────────────────────────────────
 * 1. Die Waechter
 * ──────────────────────────────────────────────────────────────────── */

TEST(null_konfig_wird_ueberall_abgelehnt)
{
    /* Gemessen und benannt statt behauptet (MF-1031-Gestalt): die
     * beiden folgenden Zusicherungen sind GEGENSEITIG REDUNDANT.
     * `uft_kf_write_supported()` traegt einen eigenen NULL-Waechter,
     * und sein Rumpf ist `return uft_kf_is_available(config);` —
     * welches bei Zeile 756 selbst mit `if (!cfg) return false;`
     * beginnt. Nimmt man einen der beiden Waechter weg, haelt der
     * andere die Zusage; erst beide weg lassen sie fallen.
     *
     * In der Mutationsmatrix rutscht „M1 NULL-Waechter weg" deshalb
     * durch, und das ist kein Loch im Test, sondern eine Eigenschaft
     * des Codes. Sie steht hier, damit niemand sie fuer ein Loch
     * haelt und den Test „repariert". */
    ASSERT(uft_kf_write_supported(NULL) == false);
    ASSERT(uft_kf_is_available(NULL) == false);

    const uint32_t fluss[4] = { 100, 100, 100, 100 };
    ASSERT(uft_kf_write_track(NULL, 0, 0, fluss, 4) == -1);
}

TEST(schreibzusage_ist_dieselbe_frage_wie_verfuegbarkeit)
{
    /* BERICHTIGT waehrend der Abnahme. Hier stand
     * `frische_konfig_kennt_kein_dtc` mit der Begruendung, `create()`
     * kalloziere und `dtc_found` sei deshalb false — „der Zustand ist
     * also deterministisch, dieser Test haengt nicht davon ab, ob auf
     * der Baumaschine zufaellig ein DTC installiert ist".
     *
     * Das war falsch, und eine Zusicherung hat es gefangen:
     * `uft_kf_config_create()` ruft in seiner vorletzten Zeile
     * `find_dtc_executable(cfg)` — es SUCHT. Auf einer Maschine mit
     * installiertem DTC waere die frische Konfiguration also
     * verfuegbar gewesen und der Test rot, ohne dass irgendetwas
     * kaputt ist.
     *
     * Was hier statt dessen geprueft wird, gilt in BEIDEN Faellen und
     * ist die eigentliche Aussage: die beiden Fragen sind
     * ununterscheidbar. */
    uft_kf_config_t *cfg = uft_kf_config_create();
    ASSERT(cfg != NULL);

    ASSERT(uft_kf_write_supported(cfg) == uft_kf_is_available(cfg));

    uft_kf_config_destroy(cfg);
}

TEST(ein_pfad_ins_leere_macht_nichts_verfuegbar)
{
    uft_kf_config_t *cfg = uft_kf_config_create();
    ASSERT(cfg != NULL);

    ASSERT(uft_kf_set_dtc_path(cfg, "/kein/dtc/an/diesem/ort/dtc") == -1);
    ASSERT(uft_kf_is_available(cfg) == false);
    ASSERT(uft_kf_write_supported(cfg) == false);

    uft_kf_config_destroy(cfg);
}

/* ────────────────────────────────────────────────────────────────────
 * 2. Der Kern: die Frage misst nicht, wonach sie fragt
 * ──────────────────────────────────────────────────────────────────── */

TEST(schreibzusage_haengt_nur_an_der_ausfuehrbarkeit)
{
    ASSERT(g_selbst != NULL);

    uft_kf_config_t *cfg = uft_kf_config_create();
    ASSERT(cfg != NULL);

    /* Wir zeigen auf DIESES Testprogramm. Es ist ausfuehrbar, und es
     * ist mit Sicherheit nicht DTC. */
    ASSERT(uft_kf_set_dtc_path(cfg, g_selbst) == 0);

    /* Und `uft_kf_write_supported()` sagt: ja, schreiben geht.
     *
     * Das ist der ganze Befund. Die Funktion hat nicht gefragt, ob
     * dort DTC liegt, nicht welche Fassung, und nicht ob diese Fassung
     * schreiben kann — sie hat gefragt, ob die Datei ausfuehrbar ist.
     * Faellt diese Zusicherung eines Tages, dann WEIL jemand eine
     * echte Pruefung eingebaut hat; dann gehoert dieser Test
     * umgeschrieben statt repariert. */
    ASSERT(uft_kf_write_supported(cfg) == true);

    /* Und der Beleg, dass es wirklich dieselbe Frage ist: */
    ASSERT(uft_kf_write_supported(cfg) == uft_kf_is_available(cfg));

    /* Bewusst KEIN uft_kf_write_track() hier — das wuerde popen() auf
     * dieses Testprogramm absetzen. */
    uft_kf_config_destroy(cfg);
}

/* ────────────────────────────────────────────────────────────────────
 * 3. Der Schreiber sagt ab, statt still zu gelingen
 * ──────────────────────────────────────────────────────────────────── */

TEST(schreiber_lehnt_leere_eingaben_ab)
{
    /* Dass hier -1 herauskommt, reicht als Zusicherung NICHT — das
     * kam im ersten Lauf der Mutationsmatrix heraus, wo „NULL-Fluss
     * erlaubt" und „Laenge 0 erlaubt" beide DURCHRUTSCHTEN.
     *
     * Der Grund ist eine Reihenfolge: faellt der Eingangswaechter weg,
     * greift eine Zeile spaeter das DTC-Tor, und weil eine frische
     * Konfiguration kein DTC kennt, kommt wieder -1. Zwei verschiedene
     * Absagen, dasselbe Ergebnis.
     *
     * Unterscheiden lassen sie sich an `last_error`: der Waechter
     * kehrt STILL zurueck (`return -1;` ohne snprintf), das DTC-Tor
     * SCHREIBT einen Grund. Also wird hier nicht nur der Rueckgabewert
     * geprueft, sondern dass der Fehlertext UNBERUEHRT bleibt.
     *
     * Denselben Fall ueber einen erreichbaren Schreiber zu isolieren
     * waere der falsche Weg: dafuer muesste `dtc_path` auf eine
     * ausfuehrbare Datei zeigen, und `uft_kf_write_track()` setzt dann
     * `popen()` darauf ab — bei `argv[0]` also auf dieses
     * Testprogramm, rekursiv. */
    uft_kf_config_t *cfg = uft_kf_config_create();
    ASSERT(cfg != NULL);

    /* Deterministisch in den Zustand „kein DTC" bringen — NICHT auf
     * `create()` verlassen, das sucht (siehe oben). */
    ASSERT(uft_kf_set_dtc_path(cfg, "/kein/dtc/an/diesem/ort/dtc") == -1);

    char vorher[512];
    strncpy(vorher, uft_kf_get_error(cfg), sizeof(vorher) - 1);
    vorher[sizeof(vorher) - 1] = '\0';
    ASSERT(strstr(vorher, "not found") != NULL);   /* wir wissen, was dasteht */

    const uint32_t fluss[4] = { 100, 100, 100, 100 };

    ASSERT(uft_kf_write_track(cfg, 0, 0, NULL,  4) == -1);
    ASSERT(strcmp(uft_kf_get_error(cfg), vorher) == 0);  /* unberuehrt */

    ASSERT(uft_kf_write_track(cfg, 0, 0, fluss, 0) == -1);
    ASSERT(strcmp(uft_kf_get_error(cfg), vorher) == 0);  /* unberuehrt */

    uft_kf_config_destroy(cfg);
}

TEST(ohne_dtc_wird_abgesagt_mit_grund)
{
    uft_kf_config_t *cfg = uft_kf_config_create();
    ASSERT(cfg != NULL);

    /* Erzwungen, nicht angenommen: `create()` sucht nach DTC, also
     * setzen wir den Pfad selbst ins Leere. */
    ASSERT(uft_kf_set_dtc_path(cfg, "/kein/dtc/an/diesem/ort/dtc") == -1);
    ASSERT(uft_kf_is_available(cfg) == false);

    const uint32_t fluss[8] = { 100, 120, 100, 140, 100, 120, 100, 140 };

    /* Kein DTC -> kein Unterprozess, kein Erfolg, und ein Grund. */
    ASSERT(uft_kf_write_track(cfg, 0, 0, fluss, 8) == -1);

    const char *grund = uft_kf_get_error(cfg);
    ASSERT(grund != NULL);
    ASSERT(grund[0] != '\0');

    /* „Keine stille Veraenderung": der Fehlertext muss den Grund
     * nennen, nicht bloss irgendeinen Text tragen. */
    ASSERT(strstr(grund, "not supported") != NULL ||
           strstr(grund, "not available") != NULL);

    uft_kf_config_destroy(cfg);
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    g_selbst = (argc > 0 && argv && argv[0]) ? argv[0] : NULL;

    printf("=== KryoFlux-Schreibweg: Waechter (MF-1045) ===\n");
    RUN(null_konfig_wird_ueberall_abgelehnt);
    RUN(schreibzusage_ist_dieselbe_frage_wie_verfuegbarkeit);
    RUN(ein_pfad_ins_leere_macht_nichts_verfuegbar);

    printf("\n=== Der Kern: die Frage misst nicht, wonach sie fragt ===\n");
    RUN(schreibzusage_haengt_nur_an_der_ausfuehrbarkeit);

    printf("\n=== Der Schreiber sagt ab, statt still zu gelingen ===\n");
    RUN(schreiber_lehnt_leere_eingaben_ab);
    RUN(ohne_dtc_wird_abgesagt_mit_grund);

    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    printf("\nNICHT geprueft: ob ein KryoFlux schreiben KANN. Dieses\n"
           "Projekt hat keine Hardware (MF-310), und der echte DTC liegt\n"
           "nicht im Baum. Die Frage steht als P3-341. Dass UFT es heute\n"
           "nicht kann, ist dagegen gemessen: der Schreiber in\n"
           "src/hal/uft_kryoflux_dtc.c hat im ganzen Baum keinen\n"
           "Aufrufer — dafuer steht Tor 67\n"
           "(scripts/audit_faehigkeitszusage_gedeckt.py).\n");
    return _fail == 0 ? 0 : 1;
}
