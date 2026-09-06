/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file firmware_line_protocol.c
 * @brief Der ADF-Drive, wie er wirklich spricht. Begruendung im Kopf.
 *
 * Verhalten nach der Firmware `Niteto/ADF-Drive-Firmware` (GPL-3.0),
 * geklont nach `tools/uft-scout/work/ADF-Drive-Firmware/`. GELESEN,
 * nicht uebernommen — eigenstaendige Umsetzung, jede Aussage mit
 * Fundstelle. MF-922 (P3-188).
 */
#include "firmware_line_protocol.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define ADFW_LINE_MAX  200      /* inputString.reserve(200), main.cpp:832 */
#define ADFW_OUT_MAX   4096

struct adfw_line {
    char     zeile[ADFW_LINE_MAX + 1];
    size_t   zeile_len;
    char     aus[ADFW_OUT_MAX];
    size_t   aus_len;
    size_t   aus_pos;
    bool     disk;
    int      track;
    unsigned lines_seen;
    unsigned unknown;
};

static void sende(adfw_line_t *fw, const char *fmt, ...)
{
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    size_t len = (size_t)n < sizeof tmp ? (size_t)n : sizeof tmp - 1;
    if (fw->aus_len + len > ADFW_OUT_MAX) len = ADFW_OUT_MAX - fw->aus_len;
    memcpy(fw->aus + fw->aus_len, tmp, len);
    fw->aus_len += len;
}

/* Zerlegung wie main.cpp:101-113: LF und CR entfernen, dann am ERSTEN
 * Leerzeichen in cmd und param trennen. */
static void zerlege(char *zeile, const char **cmd, const char **param)
{
    for (char *p = zeile; *p; p++)
        if (*p == '\n' || *p == '\r') *p = '\0';

    *cmd = zeile;
    *param = "";
    char *sp = strchr(zeile, ' ');
    if (sp && sp != zeile) {
        *sp = '\0';
        *param = sp + 1;
    }
}

static void werte_aus(adfw_line_t *fw)
{
    fw->lines_seen++;
    const char *cmd = NULL, *param = NULL;
    zerlege(fw->zeile, &cmd, &param);

    if (cmd[0] == '\0') return;          /* leere Zeile: kein Vergleich trifft */

    /* main.cpp:502 — index */
    if (strcmp(cmd, "index") == 0) {
        if (fw->disk) {
            /* Serial.printf("%d microseconds\nOK\n", measureRPM())
             * 200000 us = 300 U/min, der Nennwert einer Amiga-Diskette. */
            sende(fw, "%d microseconds\nOK\n", 200000);
        } else {
            sende(fw, "NO DISK\n");      /* Serial.println("NO DISK") */
        }
        return;
    }

    /* main.cpp:550 — dskcng: Serial.println(diskChange()) */
    if (strcmp(cmd, "dskcng") == 0) {
        sende(fw, "%d\n", fw->disk ? 1 : 0);
        return;
    }

    /* main.cpp:442 — goto: KEINE Antwort. Das ist kein Versehen des
     * Emulators, sondern der Firmware-Rumpf:
     *     busy(true); gotoLogicTrack(param.toInt());
     * Fuer einen Treiber, der auf jedes Kommando ein Statusbyte
     * erwartet, ist genau das toedlich. */
    if (strcmp(cmd, "goto") == 0) {
        fw->track = atoi(param);
        return;
    }

    /* main.cpp:719 — init: ebenfalls ohne Antwort. */
    if (strcmp(cmd, "init") == 0) {
        fw->track = -1;
        return;
    }

    /* main.cpp:631 — busy/nobusy: ohne Antwort. */
    if (strcmp(cmd, "busy") == 0 || strcmp(cmd, "nobusy") == 0) {
        return;
    }

    /* main.cpp — read: Serial.printf("Reading Track %d\n", ...), dann
     * die Spurdaten, dann eine Positionszeile. Die Nutzdaten sind hier
     * NICHT nachgebildet (siehe Kopf); die Rahmenzeilen schon, denn um
     * sie geht es bei der Protokollform. */
    if (strcmp(cmd, "read") == 0) {
        int t = atoi(param);
        fw->track = t;
        sende(fw, "Reading Track %d\n", t);
        sende(fw, "Track: %d Side: %d Dir: %d CurrentTrack: %d "
                  "LogicalTrack: %d\n", t / 2, t % 2, 0, t, t);
        return;
    }

    /* main.cpp — write: "Writing Track %d\n" ... "OK" bzw.
     * "Write failed!". Ohne Diskette scheitert es. */
    if (strcmp(cmd, "write") == 0) {
        int t = atoi(param);
        fw->track = t;
        sende(fw, "Writing Track %d\n", t);
        sende(fw, fw->disk ? "OK\n" : "Write failed!\n");
        return;
    }

    /* Alles andere trifft keinen der 67 Vergleiche — die Firmware
     * antwortet GAR NICHT. */
    fw->unknown++;
}

/* ─────────────────────────── Schnittstelle ──────────────────────── */

adfw_line_t *adfw_line_create(void)
{
    adfw_line_t *fw = calloc(1, sizeof *fw);
    if (fw) fw->track = -1;
    return fw;
}

void adfw_line_destroy(adfw_line_t *fw) { free(fw); }

void adfw_line_set_disk(adfw_line_t *fw, bool present)
{
    if (fw) fw->disk = present;
}

void adfw_line_rx(adfw_line_t *fw, const uint8_t *data, size_t n)
{
    if (!fw || !data) return;
    for (size_t i = 0; i < n; i++) {
        char c = (char)data[i];
        if (fw->zeile_len < ADFW_LINE_MAX) {
            fw->zeile[fw->zeile_len++] = c;
        }
        /* DAS ist der Kern: erst '\n' loest die Auswertung aus. */
        if (c == '\n') {
            fw->zeile[fw->zeile_len] = '\0';
            werte_aus(fw);
            fw->zeile_len = 0;
        }
    }
}

size_t adfw_line_tx(adfw_line_t *fw, uint8_t *out, size_t cap)
{
    if (!fw || !out || cap == 0) return 0;
    size_t da = fw->aus_len - fw->aus_pos;
    size_t n = da < cap ? da : cap;
    memcpy(out, fw->aus + fw->aus_pos, n);
    fw->aus_pos += n;
    if (fw->aus_pos == fw->aus_len) { fw->aus_len = 0; fw->aus_pos = 0; }
    return n;
}

unsigned adfw_line_lines_seen(const adfw_line_t *fw) { return fw ? fw->lines_seen : 0; }
unsigned adfw_line_unknown(const adfw_line_t *fw)    { return fw ? fw->unknown : 0; }
size_t   adfw_line_pending(const adfw_line_t *fw)    { return fw ? fw->zeile_len : 0; }
int      adfw_line_track(const adfw_line_t *fw)      { return fw ? fw->track : -1; }
