/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * fixture_code_a.c — Koeder fuer code_audit.py, Datei 1 von 2.
 *
 * DIESE DATEI WIRD NICHT GEBAUT. Sie enthaelt jede Falle genau einmal.
 *
 * K4 braucht ZWEI Dateien, sonst gibt es nichts zu doppeln — die
 * Gegenstuecke stehen in fixture_code_b.c. Genau das ist der Punkt der
 * Regel: eine Konstante in einer Datei ist eine Definition, in zwei ist
 * sie Doppelhaltung.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

/* ══ K4: Konstanten, die auch in fixture_code_b.c stehen ═════════════ */

/* 12500 und 6250 — der belegte uft_hfe.c-Fall. */
static uint16_t track_len_guess(unsigned sectors) {
    const uint16_t bitrate = (sectors > 10u) ? 500u : 250u;
    return (bitrate >= 500u) ? 12500u : 6250u;
}

/* 0x4E — das MFM-Fuellbyte, in drei Dateien waere es "sicher". */
static const uint8_t gap_fill = 0x4Eu;

/* 737280 — die neunfach beanspruchte Groesse. */
static int is_720k(size_t sz) { return sz == 737280u; }

/* ══ Was NICHT gemeldet werden darf ══════════════════════════════════ */

/* Sektorgroessen und Rundungen stehen absichtlich nicht in der Liste:
 * sie kommen berechtigt ueberall vor. */
static const uint16_t sector_size = 512u;
static const uint8_t  align       = 16u;
static const unsigned both        = 2u;

/* Eine Konstante, die nur HIER steht, ist eine Definition. */
static const uint32_t only_here = 1802240u;   /* Amiga HD, nur hier */

/* ══ K6: Warnung hinter der letzten Schleife ═════════════════════════ */

struct item { const char *name; int flag; };

static size_t report_bad(const struct item *items, size_t count,
                         char *buf, size_t buflen) {
    size_t n = 0;
    int r;

    r = snprintf(buf, buflen, "Eintraege: %zu\n", count);
    if (r > 0) n += (size_t)r;

    for (size_t i = 0; i < count; ++i) {
        if (n >= buflen) break;
        r = snprintf(buf + n, buflen - n, "  %s\n", items[i].name);
        if (r > 0) n += (size_t)r;
    }

    /* K6 pruefen: diese Warnung faellt bei vielen Eintraegen weg. */
    if (count > 10u) {
        r = snprintf(buf + n, buflen - n,
                     "WARNUNG: die Liste ist unvollstaendig\n");
        if (r > 0) n += (size_t)r;
    }
    return n;
}

/* Richtig gemacht: die Warnung steht VOR der Schleife. Kein Fund. */
static size_t report_good(const struct item *items, size_t count,
                          char *buf, size_t buflen) {
    size_t n = 0;
    int r;

    if (count > 10u) {
        r = snprintf(buf, buflen, "WARNUNG: gekuerzt\n");
        if (r > 0) n += (size_t)r;
    }
    for (size_t i = 0; i < count; ++i) {
        if (n >= buflen) break;
        r = snprintf(buf + n, buflen - n, "  %s\n", items[i].name);
        if (r > 0) n += (size_t)r;
    }
    return n;
}

/* Kein begrenzter Puffer -> kein Fund, auch mit Warnung nach der Schleife. */
static void report_unbounded(const struct item *items, size_t count) {
    for (size_t i = 0; i < count; ++i) printf("  %s\n", items[i].name);
    printf("WARNUNG: nur zur Anzeige\n");
}

/* ══ P3: #undef vor spaetem Gebrauch ═════════════════════════════════ */

static size_t two_part_report(char *buf, size_t buflen, int flag) {
    size_t n = 0;
    int r;
    #define APP(...) do { if (n + 1u < buflen) {                         \
        r = snprintf(buf + n, buflen - n, __VA_ARGS__);                  \
        if (r > 0) { n += (size_t)r; } } } while (0)

    if (!flag) {
        APP("nichts zu berichten\n");
        #undef APP          /* P3 sicher: APP wird unten noch gebraucht */
        return n;
    }

    APP("etwas zu berichten\n");   /* hier ist APP schon weg */
    return n;
}

void fixture_code_a_use(void) {
    (void)track_len_guess; (void)gap_fill; (void)is_720k;
    (void)sector_size; (void)align; (void)both; (void)only_here;
    (void)report_bad; (void)report_good; (void)report_unbounded;
    (void)two_part_report;
}
