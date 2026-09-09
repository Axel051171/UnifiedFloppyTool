/**
 * @file test_write_gate_unbekannt_verweigert.c
 * @brief Der dreiwertige Schreibschutz — und der Verbraucher, der ihn
 *        richtig behandelt (MF-986, P3-298a).
 *
 * ── Warum diese Datei existiert ──────────────────────────────────────
 *
 * Ein `bool` kann „nicht geschuetzt" und „weiss es nicht" nicht
 * unterscheiden, und diese Verwechslung erlaubt das Schreiben auf ein
 * Original. Der dreiwertige Zustand `uft_write_protect_t` loest das —
 * **aber nur, wenn der Verbraucher den dritten Wert richtig behandelt**.
 * Ein Sensor, der UNBEKANNT liefert, und ein Guard, der daraus „dann
 * eben schreiben" macht, ist der Bool-Fehler mit mehr Aufwand.
 *
 * Genau das war der Bestand vor MF-986: der Diagnose-Schritt endete mit
 *
 *     } else { result->checks_passed |= UFT_CHECK_DRIVE; }
 *
 * Eine genullte `uft_drive_diag_t` — das, was ein Backend ohne Sensor
 * liefert — kam als BESTANDENE Pruefung durch.
 *
 * ── Die vier Faelle, und warum es vier sind ──────────────────────────
 *
 *   1. Sensor UNBEKANNT            -> Schreiben scheitert
 *   2. Sensor UNGESCHUETZT, Ziel nicht freigegeben -> scheitert
 *   3. Sensor GESCHUETZT           -> scheitert, aus der Diagnose heraus
 *   4. Sensor UNGESCHUETZT + Ziel freigegeben      -> **geht durch**
 *
 * Der vierte ist keine Zugabe, er ist die Bedingung dafuer, dass die
 * ersten drei etwas aussagen: ein Tor, das IMMER verweigert, bestuende
 * 1 bis 3 und waere wertlos. Dieselbe Ueberlegung steht in
 * `test_write_gate.c` unter `the_gate_can_actually_say_yes`.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "uft/policy/uft_write_gate.h"

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

#define ADF_LEN 901120u

static const char *temp_dir(void)
{
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    return d;
}

static uint8_t *make_adf(void)
{
    uint8_t *b = (uint8_t *)calloc(1, ADF_LEN);
    if (b) memcpy(b, "DOS\0", 4);
    return b;
}

/* Eine Richtlinie, die die Laufwerksdiagnose wirklich verlangt. */
static uft_write_gate_policy_t diag_policy(void)
{
    uft_write_gate_policy_t p = UFT_GATE_POLICY_IMAGE_ONLY;
    p.require_drive_diag = true;
    return p;
}

/* ── 1. UNBEKANNT fuehrt zu Verweigerung ─────────────────────────────
 *
 * Die Diagnose ist genullt — genau das, was ein Controller liefert, der
 * den Schreibschutz nicht vorab melden kann (SCP, KryoFlux, FC5025
 * nennen ihn in null Dateien; XUM1541, Applesauce und UFI erfahren ihn
 * erst aus einem Schreib*ergebnis*). Vor MF-986 war das ein „ja". */
TEST(unbekannt_verweigert)
{
    uint8_t *img = make_adf();
    ASSERT(img != NULL);

    uft_write_gate_policy_t pol = diag_policy();
    uft_drive_diag_t diag;
    memset(&diag, 0, sizeof(diag));      /* -> write_protect == UFT_WP_UNKNOWN */
    ASSERT(diag.write_protect == UFT_WP_UNKNOWN);

    uft_write_gate_result_t r;
    uft_gate_status_t st = uft_write_gate_precheck_with_diag(
        &pol, img, ADF_LEN, &diag, temp_dir(), "uft_wp_unknown", &r);

    ASSERT(st == UFT_GATE_WRITE_PROTECT_UNKNOWN);
    ASSERT((r.checks_failed & UFT_CHECK_DRIVE) != 0);
    ASSERT((r.checks_passed & UFT_CHECK_DRIVE) == 0);
    /* Die Absage sagt, dass Unbekanntheit keine Erlaubnis ist. */
    ASSERT(strstr(r.decision_reason, "nbekannt") != NULL ||
           strstr(r.decision_reason, "rmittelbar") != NULL);

    free(img);
}

/* ── 2. UNGESCHUETZT ohne Freigabe verweigert ebenfalls ──────────────
 *
 * Der unbequeme Fall. Ein Medium ohne Schreibschutzkerbe ist noch kein
 * freigegebenes Ziel — Pasti ST verweigert aus demselben Grund sogar
 * das LESEN nicht schreibgeschuetzter Quellen (P3-298). */
TEST(ungeschuetzt_ohne_freigabe_verweigert)
{
    uint8_t *img = make_adf();
    ASSERT(img != NULL);

    uft_write_gate_policy_t pol = diag_policy();
    ASSERT(pol.target_media_released == false);   /* Vorgabe ist: nicht frei */

    uft_drive_diag_t diag;
    memset(&diag, 0, sizeof(diag));
    diag.write_protect = UFT_WP_UNPROTECTED;

    uft_write_gate_result_t r;
    uft_gate_status_t st = uft_write_gate_precheck_with_diag(
        &pol, img, ADF_LEN, &diag, temp_dir(), "uft_wp_frei", &r);

    ASSERT(st == UFT_GATE_TARGET_NOT_RELEASED);
    ASSERT((r.checks_failed & UFT_CHECK_DRIVE) != 0);
    ASSERT(r.override_required == true);   /* hier IST etwas abzuwaegen */

    free(img);
}

/* ── 3. GESCHUETZT verweigert, und die Diagnose traegt die Absage ────
 *
 * Beide Kanaele muessen dasselbe sagen: das alte Flag und der neue
 * dreiwertige Zustand. Ein Guard, der nur den eigenen Vorgabewert
 * liest, wuerde hier zufaellig richtig liegen — darum wird beides
 * einzeln geprueft. */
TEST(geschuetzt_verweigert_aus_der_diagnose)
{
    uint8_t *img = make_adf();
    ASSERT(img != NULL);
    uft_write_gate_policy_t pol = diag_policy();
    pol.target_media_released = true;   /* selbst MIT Freigabe: nein */

    /* (a) ueber den dreiwertigen Zustand */
    uft_drive_diag_t d1;
    memset(&d1, 0, sizeof(d1));
    d1.write_protect = UFT_WP_PROTECTED;
    uft_write_gate_result_t r1;
    uft_gate_status_t s1 = uft_write_gate_precheck_with_diag(
        &pol, img, ADF_LEN, &d1, temp_dir(), "uft_wp_prot_a", &r1);
    ASSERT(s1 == UFT_GATE_DRIVE_UNSAFE);
    ASSERT(r1.override_required == false);   /* kein Ueberstimmen */

    /* (b) ueber das alte Flag — dieselbe Antwort */
    uft_drive_diag_t d2;
    memset(&d2, 0, sizeof(d2));
    d2.flags = UFT_DRIVE_DIAG_WRITE_PROTECT;
    uft_write_gate_result_t r2;
    uft_gate_status_t s2 = uft_write_gate_precheck_with_diag(
        &pol, img, ADF_LEN, &d2, temp_dir(), "uft_wp_prot_b", &r2);
    ASSERT(s2 == UFT_GATE_DRIVE_UNSAFE);

    free(img);
}

/* ── 4. Der Gegenzweig: es MUSS auch ja sagen koennen ────────────────
 *
 * Ohne diesen Fall bestuende ein Tor, das immer verweigert, die drei
 * oberen — und waere wertlos. */
TEST(ungeschuetzt_und_freigegeben_geht_durch)
{
    uint8_t *img = make_adf();
    ASSERT(img != NULL);

    uft_write_gate_policy_t pol = diag_policy();
    pol.target_media_released = true;

    uft_drive_diag_t diag;
    memset(&diag, 0, sizeof(diag));
    diag.write_protect = UFT_WP_UNPROTECTED;

    uft_write_gate_result_t r;
    uft_gate_status_t st = uft_write_gate_precheck_with_diag(
        &pol, img, ADF_LEN, &diag, temp_dir(), "uft_wp_ok", &r);

    if (st != UFT_GATE_OK)
        printf("\n        Status %d: %s\n", (int)st, r.decision_reason);
    ASSERT(st == UFT_GATE_OK);
    ASSERT((r.checks_passed & UFT_CHECK_DRIVE) != 0);
    ASSERT(r.checks_failed == 0);

    free(img);
}

int main(void)
{
    printf("=== Schreibtor: unbekannt ist keine Erlaubnis (MF-986) ===\n");
    RUN(unbekannt_verweigert);
    RUN(ungeschuetzt_ohne_freigabe_verweigert);
    RUN(geschuetzt_verweigert_aus_der_diagnose);
    RUN(ungeschuetzt_und_freigegeben_geht_durch);
    printf("\n%d Pruefungen gruen, %d rot\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
