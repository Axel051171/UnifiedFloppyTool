/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * test_adfcopy_firmware_protokoll.c — P3-188 / MF-922.
 *
 * ══════════════════════════════════════════════════════════════════════
 *  WAS DIESER TEST AENDERT
 * ══════════════════════════════════════════════════════════════════════
 *
 * MF-915 hat die falschen Faehigkeitszusagen zu ADFCopy berichtigt und
 * die Protokoll-Unvereinbarkeit BESCHRIEBEN. Beschrieben ist nicht
 * gemessen: der Befund ADFC-1 stand als Prosa da, und die vorhandene
 * Testkette konnte ihn nicht sehen, weil ihr Emulator
 * (`firmware_state_machine.c`) UFTs eigene ANNAHME nachbildet — ein
 * Kommandobyte, ein Statusbyte. Der Treiber wurde also gegen ein
 * Modell seiner selbst bestaetigt.
 *
 * Seit MF-922 liegt daneben `firmware_line_protocol.c`: die
 * VEROEFFENTLICHTE Firmware, Zeile fuer Zeile mit Fundstelle. Damit
 * wird aus „ungeprueft" **pruefbar** — ohne Geraet, ohne
 * Bench-Sitzung, und ohne die EINFRIER-REGEL zu beugen: hier wird kein
 * Treiber umgebaut, hier wird gemessen.
 *
 * ── Die drei Aussagen ───────────────────────────────────────────────
 *
 *   1. Der Emulator antwortet wie die Firmware — auf TEXTZEILEN.
 *   2. UFTs Bytekommando erzeugt NICHTS. Es landet im Textpuffer und
 *      wartet dort auf ein '\n', das nie kommt.
 *   3. Die Gegenprobe zu 2: derselbe Emulator antwortet sehr wohl,
 *      wenn man ihn richtig anspricht. Das Schweigen liegt am
 *      Protokoll, nicht an einem toten Modell.
 *
 * Ohne 3 waere 2 wertlos — ein Emulator, der nie etwas sagt, „belegt"
 * jede Behauptung.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "emulators/adfcopy/firmware_line_protocol.h"

/* Das Byte, das UFTs Treiber fuer GET_STATUS sendet.
 * Quelle: src/hardware_providers/adfcopy_provider_v2.h — „cmdGetStatus()
 * -> ADFC_CMD_GET_STATUS (0x0B), reads 1-byte status bitmask". */
#define UFT_ADFC_CMD_GET_STATUS 0x0B

static int g_ok = 0;
static void ok(const char *n) { g_ok++; printf("  OK  %s\n", n); }

static void sende(adfw_line_t *fw, const char *s)
{
    adfw_line_rx(fw, (const uint8_t *)s, strlen(s));
}

static size_t hole(adfw_line_t *fw, char *buf, size_t cap)
{
    size_t n = adfw_line_tx(fw, (uint8_t *)buf, cap - 1);
    buf[n] = '\0';
    return n;
}

/* ═══════ 1. Der Emulator spricht wie die Firmware ═════════════════ */

static void t_firmware_antwortet(void)
{
    adfw_line_t *fw = adfw_line_create();
    assert(fw != NULL);
    char buf[512];

    /* Ohne Diskette: main.cpp:502-507 */
    adfw_line_set_disk(fw, false);
    sende(fw, "index\n");
    hole(fw, buf, sizeof buf);
    if (strcmp(buf, "NO DISK\n") != 0) {
        printf("  FEHLER: 'index' ohne Diskette -> '%s' statt 'NO DISK'\n", buf);
        assert(0);
    }

    /* Mit Diskette: "%d microseconds\nOK\n" */
    adfw_line_set_disk(fw, true);
    sende(fw, "index\n");
    hole(fw, buf, sizeof buf);
    if (!strstr(buf, "microseconds") || !strstr(buf, "OK")) {
        printf("  FEHLER: 'index' mit Diskette -> '%s'\n", buf);
        assert(0);
    }

    /* main.cpp:550 — dskcng: Serial.println(diskChange()) */
    sende(fw, "dskcng\n");
    hole(fw, buf, sizeof buf);
    assert(strcmp(buf, "1\n") == 0);

    /* Argumenttrennung am ERSTEN Leerzeichen (main.cpp:105-108) */
    sende(fw, "goto 41\n");
    assert(adfw_line_track(fw) == 41);
    assert(hole(fw, buf, sizeof buf) == 0);   /* goto antwortet nicht */

    /* MF-922, aus dem Mutations-Gegenbeweis gelernt: mit nur EINEM
     * Leerzeichen sind „am ersten" und „am letzten" nicht zu
     * unterscheiden — die Mutation `strchr -> strrchr` blieb gruen.
     * Die Firmware trennt am ERSTEN (`inputString.indexOf(" ")`,
     * main.cpp:105), also muss hier eine Zeile mit ZWEI Leerzeichen
     * stehen. Bei „am letzten" waere `cmd` == "goto 7" und traefe
     * keinen der 67 Vergleiche — die Spur bliebe auf 41. */
    sende(fw, "goto 7 rest\n");
    if (adfw_line_track(fw) != 7) {
        printf("  FEHLER: 'goto 7 rest' -> Spur %d statt 7; die Zeile "
               "wird nicht am ERSTEN Leerzeichen getrennt\n",
               adfw_line_track(fw));
        assert(0);
    }

    /* read: Rahmenzeilen wie in der Firmware */
    sende(fw, "read 12\n");
    hole(fw, buf, sizeof buf);
    if (!strstr(buf, "Reading Track 12")) {
        printf("  FEHLER: 'read 12' -> '%s'\n", buf);
        assert(0);
    }

    /* CR vor LF wird entfernt (main.cpp:101-102) — ein Wirt, der
     * "\r\n" sendet, muss trotzdem verstanden werden. */
    sende(fw, "dskcng\r\n");
    hole(fw, buf, sizeof buf);
    assert(strcmp(buf, "1\n") == 0);

    /* Unbekanntes Kommando: KEINE Antwort, aber gezaehlt. */
    unsigned vorher = adfw_line_unknown(fw);
    sende(fw, "gibtesnicht\n");
    assert(adfw_line_unknown(fw) == vorher + 1);
    assert(hole(fw, buf, sizeof buf) == 0);

    adfw_line_destroy(fw);
    ok("Emulator antwortet wie die Firmware (Zeilen, Text, Argumente)");
}

/* ═══════ 2. UFTs Bytekommando erzeugt NICHTS ══════════════════════ */

static void t_bytekommando_verhallt(void)
{
    adfw_line_t *fw = adfw_line_create();
    assert(fw != NULL);
    adfw_line_set_disk(fw, true);
    char buf[256];

    /* Genau das, was `send_cmd(tx, ADFC_CMD_GET_STATUS)` auf die
     * Leitung legt: ein einzelnes Byte 0x0B. */
    const uint8_t byte = UFT_ADFC_CMD_GET_STATUS;
    adfw_line_rx(fw, &byte, 1);

    /* Die Firmware hat noch keine Zeile gesehen — das Byte liegt im
     * Puffer und wartet auf sein '\n'. */
    if (adfw_line_lines_seen(fw) != 0) {
        printf("  FEHLER: die Firmware hat %u Zeilen ausgewertet, "
               "obwohl nur ein Byte ohne Zeilenende kam\n",
               adfw_line_lines_seen(fw));
        assert(0);
    }
    if (adfw_line_pending(fw) != 1) {
        printf("  FEHLER: %zu Byte haengen im Puffer, erwartet ist 1\n",
               adfw_line_pending(fw));
        assert(0);
    }

    /* Und es kommt NICHTS zurueck. Der Treiber wartet auf ein
     * Statusbyte, das nie gesendet wird — genau der
     * „GET_STATUS response timeout", den ein Benutzer heute sieht. */
    size_t n = hole(fw, buf, sizeof buf);
    if (n != 0) {
        printf("  FEHLER: %zu Byte Antwort auf ein rohes Kommandobyte "
               "('%s') — dann ist das Protokollmodell doch vereinbar\n",
               n, buf);
        assert(0);
    }

    /* Auch die ganze Bytefolge, die der Treiber im schlimmsten Fall
     * absetzt, aendert daran nichts, solange kein '\n' dabei ist. */
    const uint8_t folge[] = { 0x01, 0x02, 0x0B, 0x10, 0xFF };
    adfw_line_rx(fw, folge, sizeof folge);
    assert(adfw_line_lines_seen(fw) == 0);
    assert(hole(fw, buf, sizeof buf) == 0);

    adfw_line_destroy(fw);
    ok("UFTs Bytekommando 0x0B -> 0 Zeilen, 0 Byte Antwort (ADFC-1)");
}

/* ═══════ 3. Gegenprobe: derselbe Emulator KANN antworten ══════════ */

static void t_gegenprobe(void)
{
    adfw_line_t *fw = adfw_line_create();
    assert(fw != NULL);
    adfw_line_set_disk(fw, true);
    char buf[256];

    /* Erst das Byte (nichts), dann ein '\n' — die angesammelte Zeile
     * "\x0b" trifft keinen der 67 Vergleiche und bleibt unbeantwortet.
     * Danach ein richtiges Kommando: jetzt kommt eine Antwort.
     *
     * Damit ist belegt, dass das Schweigen in Pruefung 2 am PROTOKOLL
     * liegt und nicht daran, dass dieser Emulator ueberhaupt nichts
     * sagt. Ohne diese Gegenprobe waere Pruefung 2 wertlos. */
    const uint8_t byte = UFT_ADFC_CMD_GET_STATUS;
    adfw_line_rx(fw, &byte, 1);
    sende(fw, "\n");
    assert(adfw_line_lines_seen(fw) == 1);
    assert(adfw_line_unknown(fw) == 1);
    assert(hole(fw, buf, sizeof buf) == 0);

    sende(fw, "index\n");
    size_t n = hole(fw, buf, sizeof buf);
    if (n == 0) {
        printf("  FEHLER: der Emulator antwortet auch auf 'index' nicht "
               "— dann belegt Pruefung 2 gar nichts\n");
        assert(0);
    }
    assert(strstr(buf, "OK") != NULL);

    adfw_line_destroy(fw);
    ok("Gegenprobe: derselbe Emulator antwortet auf eine echte Zeile");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("test_adfcopy_firmware_protokoll — P3-188 (MF-922)\n");
    /* Reihenfolge mit Absicht: erst die beiden Pruefungen, um die es
     * geht, dann die breite Verhaltensprobe.
     *
     * MF-922, aus dem Mutations-Gegenbeweis gelernt: lief die breite
     * Probe zuerst, fielen ALLE Mutationen schon dort — und ob die
     * Gegenprobe (3) ihren eigenen Zweck erfuellt, liess sich gar
     * nicht mehr beobachten. Eine Matrix, in der jede Mutation an
     * derselben Stelle faellt, misst nur diese eine Stelle. */
    t_bytekommando_verhallt();
    t_gegenprobe();
    t_firmware_antwortet();
    printf("%d/%d Pruefungen gruen\n", g_ok, g_ok);
    return 0;
}
