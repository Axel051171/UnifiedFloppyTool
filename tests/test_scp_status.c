/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_scp_status.c
 * @brief Die Information war da und wurde weggeworfen (MF-804).
 *
 * ── Der Befund ───────────────────────────────────────────────────────────
 *
 * `src/hal/uft_scp_direct.c` bildete **jeden** Fehlerstatus des Geräts
 * auf ein pauschales `UFT_ERR_IO` ab:
 *
 *     if (response[1] != UFT_SCP_PR_OK) {
 *         // Map to generic I/O — caller can read the raw status via a
 *         // future status-query API.
 *         return UFT_ERR_IO;
 *     }
 *
 * Diese API gab es nicht, und das Byte war zu dem Zeitpunkt weg. Die
 * Firmware unterscheidet **zwanzig** Zustände. Für den Benutzer ist der
 * Unterschied zwischen „keine Diskette eingelegt" und „Spur 0 nicht
 * gefunden" der ganze Unterschied zwischen einem Werkzeug und einem
 * Rätsel.
 *
 * ── Was dieser Test bewacht ──────────────────────────────────────────────
 *
 * Nicht die Übersetzung einzelner Texte — die wäre Geschmackssache. Er
 * bewacht die **Unterscheidbarkeit**: jeder der zwanzig Codes muss einen
 * EIGENEN Text haben, und keiner darf auf den Rückfalltext laufen. Wer
 * zwei Codes auf denselben Text legt, hat wieder Information geworfen,
 * nur eine Ebene höher.
 *
 * ── Warum das ohne Gerät geht ────────────────────────────────────────────
 *
 * `uft_scp_status_name()` ist rein. Das ist Absicht: der Gerätepfad ist
 * ohne Hardware nicht erreichbar (MF-310), und eine Aussage, die nur
 * dort geprüft werden könnte, wäre in diesem Baum ungeprüft — also
 * genau die Lage, aus der die fünf fabrizierten Parser kamen.
 *
 * ── Quelle ───────────────────────────────────────────────────────────────
 *
 * `src/samdisk/SuperCardPro.h:9-28` (simonowen/samdisk, MIT, im Baum),
 * Umsetzung der vom Hersteller veröffentlichten *SuperCard Pro SDK v1.7*
 * (cbmstuff.com, Dezember 2015). **Nicht** aus `pySuperCardPro`
 * (GPL-3.0) — es war nicht nötig.
 */
#include "uft/hal/uft_scp_direct.h"

#include <stdio.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-44s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                   _fail++; return; } } while (0)

/* Die zwanzig Codes, wie die Spezifikation sie kennt. Ein einundzwanzigster
 * Wert hätte hier keinen Platz — und genau das prüft Fall 3. */
static const uint8_t CODES[] = {
    UFT_SCP_PR_UNUSED,        UFT_SCP_PR_BAD_COMMAND,
    UFT_SCP_PR_COMMAND_ERR,   UFT_SCP_PR_CHECKSUM,
    UFT_SCP_PR_TIMEOUT,       UFT_SCP_PR_NO_TRK0,
    UFT_SCP_PR_NO_DRIVE_SEL,  UFT_SCP_PR_NO_MOTOR_SEL,
    UFT_SCP_PR_NOT_READY,     UFT_SCP_PR_NO_INDEX,
    UFT_SCP_PR_ZERO_REVS,     UFT_SCP_PR_READ_TOO_LONG,
    UFT_SCP_PR_BAD_LENGTH,    UFT_SCP_PR_BAD_DATA,
    UFT_SCP_PR_BOUNDARY_ODD,  UFT_SCP_PR_WP_ENABLED,
    UFT_SCP_PR_BAD_RAM,       UFT_SCP_PR_NO_DISK,
    UFT_SCP_PR_BAD_BAUD,      UFT_SCP_PR_BAD_CMD_PORT,
    UFT_SCP_PR_OK
};
enum { N_CODES = (int)(sizeof(CODES) / sizeof(CODES[0])) };

/* DER ROTBEWEIS. Vor MF-804 gab es genau EINEN benannten Code; alles
 * andere wurde zu UFT_ERR_IO und war nicht mehr auseinanderzuhalten. */
TEST(jeder_code_hat_einen_eigenen_text)
{
    for (int i = 0; i < N_CODES; i++) {
        const char *t = uft_scp_status_name(CODES[i]);
        if (!t || strcmp(t, "unbekannter SCP-Status") == 0) {
            printf("FAIL: Code 0x%02X ohne eigenen Text\n", CODES[i]);
            _fail++;
            return;
        }
        for (int j = 0; j < i; j++) {
            if (strcmp(t, uft_scp_status_name(CODES[j])) == 0) {
                printf("FAIL: 0x%02X und 0x%02X teilen sich den Text \"%s\"\n",
                       CODES[i], CODES[j], t);
                _fail++;
                return;
            }
        }
    }
}

/* Die zwei Fälle aus der Begründung, wörtlich. Wer sie vertauscht,
 * bricht diesen Test — und genau die Verwechslung war vorher der
 * Normalfall, weil beide dasselbe UFT_ERR_IO lieferten. */
TEST(keine_diskette_und_keine_spur_null_sind_zweierlei)
{
    const char *a = uft_scp_status_name(UFT_SCP_PR_NO_DISK);
    const char *b = uft_scp_status_name(UFT_SCP_PR_NO_TRK0);
    ASSERT(strcmp(a, b) != 0);
    ASSERT(strstr(a, "Diskette") != NULL);
    ASSERT(strstr(b, "Spur 0") != NULL);
}

/* Ein Wert, den die Spezifikation nicht kennt, wird NICHT geraten.
 * 0x14 liegt direkt hinter dem letzten dokumentierten Code (0x13) —
 * also genau dort, wo eine spätere Firmware erweitern würde. */
TEST(ein_unbekannter_wert_wird_nicht_geraten)
{
    ASSERT(strcmp(uft_scp_status_name(0x14), "unbekannter SCP-Status") == 0);
    ASSERT(strcmp(uft_scp_status_name(0xFF), "unbekannter SCP-Status") == 0);
    /* Und der Zahlenwert steht ABSICHTLICH nicht im Text — er gehoert
     * daneben ausgegeben, sonst kann der Aufrufer ihn nicht mehr
     * maschinell weiterreichen. */
    ASSERT(strstr(uft_scp_status_name(0x14), "0x14") == NULL);
}

/* Ohne Kontext gibt es keinen Fehlschlag zu berichten — und schon gar
 * keinen Absturz. */
TEST(letzter_status_ohne_kontext_ist_kein_absturz)
{
    ASSERT(uft_scp_last_status(NULL) == UFT_SCP_PR_OK);
}

int main(void)
{
    printf("test_scp_status (MF-804) — %d Codes aus der SCP SDK v1.7\n",
           N_CODES);
    RUN(jeder_code_hat_einen_eigenen_text);
    RUN(keine_diskette_und_keine_spur_null_sind_zweierlei);
    RUN(ein_unbekannter_wert_wird_nicht_geraten);
    RUN(letzter_status_ohne_kontext_ist_kein_absturz);
    printf("%d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail ? 1 : 0;
}
