/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_spur_freigabe_zaehlt.c
 * @brief Rotbeweis zu MF-1132 — `uft_track_release()` liess das
 *        Sektorfeld liegen, und das waren 27 der 29 ASan-Lecks
 *
 * ── Der Befund ────────────────────────────────────────────────────────────
 *
 * `uft_track_add_sector()` (`src/core/uft_format_plugin.c:476`) legt je
 * Sektor VIER Bloecke an — `data`, `confidence_map`, `weak_mask`,
 * `timing_ns` — plus das Sektorfeld selbst. `uft_track_release()` gab
 * alles davon nur unter `track->owns_data` frei, und `add_sector` setzt
 * diese Fahne NIE.
 *
 * Gemessen im ASan-Lauf der CI: 29 LeakSanitizer-Zusammenfassungen,
 * 22,9 MB, davon 27 Meldungen und 22,3 MB ueber diesen einen Weg. Die
 * Fahne wird im ganzen Baum fuer eine Spur nur an zwei Stellen gesetzt,
 * beide fruehere Reparaturen derselben Klasse (MF-595, MF-599).
 *
 * ── Warum nicht einfach `owns_data = true` ────────────────────────────────
 *
 * Weil `owns_data` EINE Fahne fuer NEUN Eigentuemerschaften ist:
 * Sektorpuffer, Sektorfeld, `raw_data`, `flux`, `flux_times`,
 * `confidence`, `weak_mask` und die `revisions`. Sie an der
 * Sektorstelle zu setzen behauptet Eigentum an acht Dingen, die
 * `add_sector` nicht angelegt hat — ein `free()` auf einen geliehenen
 * Zeiger.
 *
 * Gemessen ist dagegen, dass `uft_track_t::sectors` im ganzen Baum an
 * genau ZWEI Stellen geschrieben wird (`uft_track_alloc()` mit `calloc`,
 * `uft_track_add_sector()` mit `realloc`). Es ist nie geliehen, seine
 * Freigabe braucht also kein Tor — und der zweite Aufraeumer derselben
 * Struktur, `uft_track_cleanup()`, sagt das seit langem: er gibt
 * Sektoren und Sektorfeld BEDINGUNGSLOS frei. Zwei Aufraeumer, dasselbe
 * Feld, zwei Antworten.
 *
 * ── Warum dieser Test `--wrap` benutzt ────────────────────────────────────
 *
 * Ein Leck ist ohne Sanitizer nicht sichtbar, und die lokale
 * MinGW-Kette hat kein ASan. Ein Test, der das Leck nur BESCHREIBT,
 * waere ein Rotbeweis, der nicht feuert — nach der EINFRIER-REGEL nicht
 * ausreichend.
 *
 * Deshalb zaehlt dieser Test selbst: `-Wl,--wrap=malloc` & Co. fuehren
 * jede Anforderung und jede Freigabe ueber eigene Funktionen, und die
 * Bilanz ist danach eine Zahl. Das ist exakt und braucht kein ASan.
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"

/* ══════════════════════════════════════════════════════════════════════
 * Die Waage
 * ══════════════════════════════════════════════════════════════════════ */
void *__real_malloc(size_t n);
void *__real_calloc(size_t n, size_t m);
void *__real_realloc(void *p, size_t n);
void  __real_free(void *p);

static long g_offen;       /* Anforderungen minus Freigaben */
static long g_anforderung;
static long g_freigabe;
static int  g_zaehlen;     /* nur im Messabschnitt zaehlen */

void *__wrap_malloc(size_t n) {
    void *p = __real_malloc(n);
    if (g_zaehlen && p) { g_offen++; g_anforderung++; }
    return p;
}

void *__wrap_calloc(size_t n, size_t m) {
    void *p = __real_calloc(n, m);
    if (g_zaehlen && p) { g_offen++; g_anforderung++; }
    return p;
}

void *__wrap_realloc(void *p, size_t n) {
    void *q = __real_realloc(p, n);
    if (g_zaehlen) {
        /* realloc(NULL, n) ist eine Anforderung; realloc(p, n) ersetzt
         * einen bestehenden Block, die Bilanz bleibt gleich. */
        if (!p && q) { g_offen++; g_anforderung++; }
    }
    return q;
}

void __wrap_free(void *p) {
    if (g_zaehlen && p) { g_offen--; g_freigabe++; }
    __real_free(p);
}

static void waage_start(void) {
    g_offen = 0; g_anforderung = 0; g_freigabe = 0; g_zaehlen = 1;
}

static long waage_ende(void) {
    g_zaehlen = 0;
    return g_offen;
}

/* ══════════════════════════════════════════════════════════════════════ */
static int gruen = 0;
static int rot = 0;

#define ZUSAGE(bed, text)                                                   \
    do {                                                                    \
        if (bed) { gruen++; printf("   [ok ] %s\n", (text)); }               \
        else     { rot++;   printf("   [ROT] %s\n", (text)); }               \
        assert(bed);                                                        \
    } while (0)

/* Ein Sektor mit ALLEN VIER Puffern, damit der Test alle vier
 * Freigabewege trifft und nicht nur `data`. */
static void sektor_fuellen(uft_sector_t *s, int nr) {
    memset(s, 0, sizeof(*s));
    s->data_size = 256;
    s->data_len = 256;
    s->data = malloc(256);
    s->confidence_map = malloc(256);
    s->weak_mask = malloc(256);
    s->timing_count = 8;
    s->timing_ns = malloc(8 * sizeof(double));
    if (s->data) memset(s->data, 0x40 | (nr & 0x0F), 256);
    if (s->confidence_map) memset(s->confidence_map, 0xFF, 256);
    if (s->weak_mask) memset(s->weak_mask, 0x00, 256);
    if (s->timing_ns) {
        for (int i = 0; i < 8; i++) s->timing_ns[i] = 2000.0 + i;
    }
}

static void sektor_leeren(uft_sector_t *s) {
    free(s->data);
    free(s->confidence_map);
    free(s->weak_mask);
    free(s->timing_ns);
    memset(s, 0, sizeof(*s));
}

/* ══════════════════════════════════════════════════════════════════════
 * 1) add_sector + release muss ausgeglichen sein
 * ══════════════════════════════════════════════════════════════════════ */
static void bilanz_release(void) {
    const int N = 21;          /* eine CBM-Zone */

    uft_track_t t;
    memset(&t, 0, sizeof(t));

    waage_start();

    uft_error_t rc = uft_track_alloc(&t, (size_t)N);
    ZUSAGE(rc == UFT_OK, "uft_track_alloc() gelingt");

    for (int i = 0; i < N; i++) {
        uft_sector_t s;
        sektor_fuellen(&s, i);
        rc = uft_track_add_sector(&t, &s);
        sektor_leeren(&s);                 /* die Vorlage gehoert uns */
        if (rc != UFT_OK) break;
    }
    ZUSAGE(rc == UFT_OK, "alle 21 Sektoren angenommen");
    ZUSAGE(t.sector_count == (size_t)N, "sector_count stimmt");

    /* Der gemessene Kern des Befunds: die Fahne wird nicht gesetzt. Das
     * ist KEIN Fehler mehr — seit MF-1132 haengt die Freigabe des
     * Sektorfelds nicht daran. Festgenagelt wird es, damit eine
     * Aenderung an `add_sector` auffaellt. */
    ZUSAGE(t.owns_data == false,
           "add_sector setzt owns_data NICHT (gemessen, festgenagelt)");

    const long vor_release = g_offen;
    uft_track_release(&t);
    const long offen = waage_ende();

    printf("   Anforderungen=%ld Freigaben=%ld offen_vor_release=%ld "
           "offen_danach=%ld\n", g_anforderung, g_freigabe,
           vor_release, offen);

    ZUSAGE(offen == 0,
           "nach uft_track_release() ist die Bilanz AUSGEGLICHEN — "
           "vor MF-1132 blieben hier 21*4+1 Bloecke offen");
    ZUSAGE(t.sectors == NULL && t.sector_count == 0,
           "die Spur ist danach leer und wieder benutzbar");
}

/* ══════════════════════════════════════════════════════════════════════
 * 2) Zweimal freigeben bleibt sicher
 * ══════════════════════════════════════════════════════════════════════ */
static void doppelt_freigeben(void) {
    uft_track_t t;
    memset(&t, 0, sizeof(t));

    waage_start();
    uft_track_alloc(&t, 4);
    for (int i = 0; i < 4; i++) {
        uft_sector_t s;
        sektor_fuellen(&s, i);
        uft_track_add_sector(&t, &s);
        sektor_leeren(&s);
    }
    uft_track_release(&t);
    uft_track_release(&t);          /* darf nicht abstuerzen */
    const long offen = waage_ende();

    ZUSAGE(offen == 0, "zweimal freigeben bleibt ausgeglichen");
    ZUSAGE(t.sectors == NULL, "und die Spur bleibt leer");
}

/* ══════════════════════════════════════════════════════════════════════
 * 3) Die geliehenen Zeiger bleiben UNBERUEHRT
 * ══════════════════════════════════════════════════════════════════════
 *
 * Der Grund, warum MF-1132 nicht `owns_data = true` gesetzt hat: unter
 * dieser Fahne gibt `release()` auch `raw_data`, `flux`, `confidence`,
 * `weak_mask` und die `revisions` frei. Eine Spur mit
 * `owns_data == false` und einem GELIEHENEN `raw_data` darf diesen
 * Zeiger behalten.
 */
static void geliehenes_bleibt(void) {
    static uint8_t geliehen[64];     /* statisch — nie malloc, nie free */

    uft_track_t t;
    memset(&t, 0, sizeof(t));

    waage_start();
    uft_track_alloc(&t, 2);
    for (int i = 0; i < 2; i++) {
        uft_sector_t s;
        sektor_fuellen(&s, i);
        uft_track_add_sector(&t, &s);
        sektor_leeren(&s);
    }

    t.raw_data = geliehen;           /* geliehen, NICHT uebernommen */
    t.raw_size = sizeof(geliehen);
    t.owns_data = false;

    uft_track_release(&t);
    const long offen = waage_ende();

    ZUSAGE(offen == 0,
           "die Sektoren sind frei, obwohl owns_data false ist");
    /* Waere `raw_data` freigegeben worden, haette die Waage -1 gemeldet
     * (eine Freigabe ohne Anforderung) oder der Lauf waere abgestuerzt. */
    ZUSAGE(offen >= 0,
           "und der geliehene Zeiger wurde NICHT freigegeben");
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("MF-1132 — die Freigabe der Spur, gezaehlt\n");

    printf("\n1) add_sector + release: Bilanz\n");
    bilanz_release();

    printf("\n2) zweimal freigeben\n");
    doppelt_freigeben();

    printf("\n3) geliehene Zeiger bleiben unberuehrt\n");
    geliehenes_bleibt();

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
