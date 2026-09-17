/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * fixture_traps.c — Koederdatei fuer tools/substring_audit.py.
 *
 * DIESE DATEI WIRD NICHT GEBAUT. Sie enthaelt absichtlich jede Falle
 * genau einmal, damit der Pruefer gegen eine bekannte Antwort laufen kann:
 * findet er weniger, ist er blind; findet er mehr, meldet er falsch.
 *
 * Der Erwartungswert steht in tests/expected_traps.txt und wird von
 * tests/run_audit_selftest.sh verglichen.
 *
 * Sie liegt absichtlich unter einem Pfad mit "formats" darin, damit die
 * Regel C2 (Magic-Suche im Erkennungspfad) greift.
 */

#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ── Was NICHT gemeldet werden darf ─────────────────────────────────────
 *
 * Die drei Unterklassen aus MF-1171, jede als Koeder:
 */

/* 1. `print(` enthaelt `int(` — ein Regex auf "int(" wuerde hier zuschlagen. */
static void noise_printf(void) { printf("nichts\n"); }

/* 2. In einem KOMMENTAR: strstr(path, ".st") — darf nicht zaehlen.
 *    Ebenso strncmp(a, b, strlen(b)) hier im Text.
 */

/* 3. In einer ZEICHENKETTE: derselbe Text, auch nicht. */
static const char *doc = "benutze nicht strstr(path, \".st\")";

/* 4. C++14-Trennzeichen — ein Lookbehind (?<![\w.]) scheitert daran. */
static const uint8_t mask = 0b0'0101'0101;
static const uint32_t big = 1'000'000;

/* 5. Eine Funktion, deren NAME eine geprüfte Funktion enthaelt. */
static int my_strncmp_wrapper(const char *a, const char *b) {
    return strcmp(a, b);   /* strcmp, nicht strncmp — kein Fund */
}

/* 6. Korrekte Verwendungen, die nicht gemeldet werden dürfen. */
static bool ok_suffix(const char *path) {
    const size_t n = strlen(path);
    return n >= 4u && memcmp(path + n - 4u, ".stx", 4u) == 0;
}
static bool ok_magic(const uint8_t *d, size_t sz) {
    return sz >= 8u && memcmp(d, "SINCLAIR", 8u) == 0;
}
static bool ok_exact(const char *id) { return strcmp(id, "d64") == 0; }

/* ══ C1: Endung wird GESUCHT statt am Ende geprueft ═══════════════════ */

static bool trap_c1_st(const char *path) {
    return strstr(path, ".st") != NULL;          /* C1 sicher */
}

static bool trap_c1_path_var(const char *filename, const char *needle) {
    return strcasestr(filename, needle) != NULL; /* C1 pruefen */
}

/* ══ C2: Magic im ganzen Puffer gesucht ══════════════════════════════ */

static bool trap_c2_sinclair(const char *buf) {
    return strstr(buf, "SINCLAIR") != NULL;      /* C2 sicher */
}

static bool trap_c2_scp(const uint8_t *buf, size_t n) {
    return memmem(buf, n, "SCP", 3u) != NULL;    /* C2 sicher */
}

/* ══ C3: Praefixvergleich statt Gleichheit ═══════════════════════════ */

static bool trap_c3(const char *id, const char *name) {
    return strncmp(id, name, strlen(name)) == 0; /* C3 sicher */
}

static bool trap_c3_lit(const char *id) {
    return strncmp(id, "d8", strlen("d8")) == 0; /* C3 sicher */
}

/* ══ C4: Laenge kuerzer als das Literal ══════════════════════════════ */

static bool trap_c4(const uint8_t *d) {
    return memcmp(d, "SINCLAIR", 4u) == 0;       /* C4 sicher: 4 von 8 */
}

/* ══ C5: Laenge laenger als das Literal ══════════════════════════════ */

static bool trap_c5(const uint8_t *d) {
    return memcmp(d, "SCP", 8u) == 0;            /* C5 sicher: 8 bei 3 */
}

/* ══ C6: sizeof auf einem Zeiger als Laenge ══════════════════════════ */

static bool trap_c6(const char *id, const char *other) {
    return strncmp(id, other, sizeof(other)) == 0; /* C6 pruefen */
}

/* ══ M1: Makro versteckt den Aufruf ══════════════════════════════════ */

#define FIND_IT(h, n) strstr((h), (n))           /* M1 pruefen */

static bool trap_m1(const char *h) {
    return FIND_IT(h, ".do") != NULL;            /* sieht keine Regel */
}

/* Verwendungen, damit der Uebersetzer nicht meckert, falls jemand die
 * Datei doch einmal baut. */
void fixture_traps_use(void) {
    (void)doc; (void)mask; (void)big;
    (void)noise_printf; (void)my_strncmp_wrapper;
    (void)ok_suffix; (void)ok_magic; (void)ok_exact;
    (void)trap_c1_st; (void)trap_c1_path_var;
    (void)trap_c2_sinclair; (void)trap_c2_scp;
    (void)trap_c3; (void)trap_c3_lit;
    (void)trap_c4; (void)trap_c5; (void)trap_c6; (void)trap_m1;
}
