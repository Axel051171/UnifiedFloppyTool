/**
 * @file test_fundus_walk.c
 * @brief Alle Aufnahmen einer Diskette wiederfinden (MF-561)
 *
 * ── Die Luecke, gemessen ─────────────────────────────────────────────────
 *
 * Der Fundus (FUND-1) kann anhaengen, nachrechnen und sich erinnern:
 *
 *      uft_fundus_add()      ein Artefakt anhaengen
 *      uft_fundus_verify()   alle Eintraege nachrechnen
 *      uft_fundus_recall()   was die letzte Sitzung ueber DIESE Diskette wusste
 *
 * `recall` liefert **einen** Eintrag — den juengsten. Das ist richtig fuer
 * seinen Zweck (fortsetzen, wo man aufgehoert hat), reicht aber fuer zwei
 * notierte Bausteine nicht:
 *
 *   * **Multi-Capture-Overlay** braucht ALLE Aufnahmen derselben Diskette,
 *     um sie uebereinanderzulegen. Wer nur die juengste bekommt, kann
 *     nichts ueberlagern.
 *   * **Die Mining-Schleife** braucht ALLE Eintraege, um sie erneut durch
 *     eine verbesserte Dekodierung zu schicken.
 *
 * `verify()` laeuft zwar ueber das Manifest, gibt aber nichts heraus — es
 * meldet nur eine Bilanz.
 *
 * **Es gibt keine Aufzaehlung.** Das ist die Luecke, und dieser Test misst
 * sie: drei Aufnahmen derselben Diskette anhaengen, und zeigen, dass der
 * bestehende Weg nur eine davon zurueckgibt.
 *
 * ── Was danach gilt ──────────────────────────────────────────────────────
 *
 *      uft_fundus_walk()        jeden Eintrag, in Manifest-Reihenfolge
 *      uft_fundus_collect_for() alle Eintraege EINER Diskette, aelteste
 *                               zuerst — genau das, was Overlay braucht
 *
 * Beide lesen mit denselben Helfern wie `recall` (`line_str_field`,
 * `line_uint_field`). Ein zweiter Manifest-Parser waere genau die
 * Geschwister-Bauart, die in dieser Sitzung siebenmal aufgefallen ist
 * (MF-526, 550, 554, 555, 559, 560).
 *
 * ── Warum das ausserhalb des Moratoriums liegt ───────────────────────────
 *
 * Der Fundus ist Infrastruktur unter `src/forensic/`, kein Format- und
 * kein Decoder-Code. Die EINFRIER-REGEL (MF-363/498) betrifft ihn nicht.
 * Geprueft wird trotzdem nach derselben Regel: Rotbeweis zuerst, jede Zahl
 * gemessen.
 */

#include "uft/forensic/uft_fundus.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond, ...)                                                   \
    do {                                                                   \
        if (cond) { printf("  ok   " __VA_ARGS__); printf("\n"); }         \
        else { printf("  FAIL " __VA_ARGS__); printf("\n"); failures++; }  \
    } while (0)

static const char *DIR = "uft_fundus_walk_test";

static void cleanup(void)
{
    /* Der Fundus legt Dateien im Verzeichnis ab; wir raeumen grob auf,
     * damit ein zweiter Lauf nicht auf Resten aufsetzt. */
    char cmd[512];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "rmdir /s /q \"%s\" 2>nul", DIR);
#else
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", DIR);
#endif
    int rc = system(cmd);
    (void)rc;
}

/** Sammelt, was `walk` liefert. */
typedef struct {
    int      n;
    unsigned seq[16];
    char     ident[16][64];
} collected_t;

static bool gather(const uft_fundus_entry_t *e, void *user)
{
    collected_t *c = (collected_t *)user;
    if (c->n >= 16) return false;
    c->seq[c->n] = e->seq;
    snprintf(c->ident[c->n], sizeof(c->ident[0]), "%s", e->identifier);
    c->n++;
    return true;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Alle Aufnahmen einer Diskette wiederfinden (MF-561) ===\n");

    cleanup();

    uft_fundus_t f;
    if (!uft_fundus_open(DIR, &f)) {
        printf("  Fundus nicht anlegbar\n");
        return 2;
    }

    /* Drei Aufnahmen DERSELBEN Diskette, dazwischen eine fremde. */
    static const uint8_t d1[] = { 1, 2, 3, 4 };
    static const uint8_t d2[] = { 5, 6, 7, 8, 9 };
    static const uint8_t d3[] = { 10, 11 };
    static const uint8_t other[] = { 99 };

    uft_fundus_meta_t m;
    memset(&m, 0, sizeof(m));
    m.identifier = "DISK-A";
    m.tool = "test";
    m.state = UFT_FUNDUS_STATE_INTERRUPTED;
    CHECK(uft_fundus_add(&f, d1, sizeof(d1), "scp", &m, NULL, 0),
          "erste Aufnahme angehaengt");

    memset(&m, 0, sizeof(m));
    m.identifier = "DISK-B";
    m.tool = "test";
    CHECK(uft_fundus_add(&f, other, sizeof(other), "scp", &m, NULL, 0),
          "fremde Diskette dazwischen");

    memset(&m, 0, sizeof(m));
    m.identifier = "DISK-A";
    m.tool = "test";
    m.state = UFT_FUNDUS_STATE_INTERRUPTED;
    CHECK(uft_fundus_add(&f, d2, sizeof(d2), "scp", &m, NULL, 0),
          "zweite Aufnahme angehaengt");

    memset(&m, 0, sizeof(m));
    m.identifier = "DISK-A";
    m.tool = "test";
    m.state = UFT_FUNDUS_STATE_COMPLETE;
    CHECK(uft_fundus_add(&f, d3, sizeof(d3), "scp", &m, NULL, 0),
          "dritte Aufnahme angehaengt");

    /* ── Die Luecke, wie sie vor MF-561 war ─────────────────────────── */
    uft_fundus_recall_t r;
    memset(&r, 0, sizeof(r));
    CHECK(uft_fundus_recall(&f, "DISK-A", &r), "recall lief");
    CHECK(r.found, "recall kennt DISK-A");
    printf("       recall liefert Eintrag %u — von DREI Aufnahmen.\n"
           "       Fuer ein Overlay reicht das nicht.\n", r.seq);

    /* ── Was jetzt geht ─────────────────────────────────────────────── */
    collected_t all;
    memset(&all, 0, sizeof(all));
    CHECK(uft_fundus_walk(&f, gather, &all), "walk lief");
    printf("       walk sah %d Eintraege\n", all.n);
    CHECK(all.n == 4, "walk sieht alle vier Eintraege (3x DISK-A, 1x DISK-B)");

    /* Reihenfolge: Manifest-Reihenfolge, also aufsteigend. */
    int ordered = 1;
    for (int i = 1; i < all.n; i++)
        if (all.seq[i] <= all.seq[i - 1]) ordered = 0;
    CHECK(ordered, "walk liefert in aufsteigender Reihenfolge");

    uft_fundus_entry_t got[8];
    size_t n_got = 0;
    CHECK(uft_fundus_collect_for(&f, "DISK-A", got, 8, &n_got),
          "collect_for lief");
    printf("       collect_for(DISK-A) fand %zu Aufnahmen\n", n_got);
    CHECK(n_got == 3, "genau die drei Aufnahmen von DISK-A");

    int only_a = 1;
    for (size_t i = 0; i < n_got; i++)
        if (strcmp(got[i].identifier, "DISK-A") != 0) only_a = 0;
    CHECK(only_a, "keine fremde Diskette dabei");

    int asc = 1;
    for (size_t i = 1; i < n_got; i++)
        if (got[i].seq <= got[i - 1].seq) asc = 0;
    CHECK(asc, "aelteste zuerst — die Reihenfolge, die ein Overlay braucht");

    /* Der Zustand muss mitkommen: Overlay will wissen, welche Aufnahme
     * vollstaendig war. */
    int has_complete = 0;
    for (size_t i = 0; i < n_got; i++)
        if (got[i].state == UFT_FUNDUS_STATE_COMPLETE) has_complete = 1;
    CHECK(has_complete, "der Zustand der Aufnahme kommt mit");

    /* Und der Pfad, sonst kann niemand die Daten lesen. */
    int has_path = 1;
    for (size_t i = 0; i < n_got; i++)
        if (got[i].file[0] == '\0') has_path = 0;
    CHECK(has_path, "jeder Eintrag nennt seine Datei");

    /* Ein Abbruch aus dem Rueckruf muss halten. */
    collected_t two;
    memset(&two, 0, sizeof(two));
    two.n = 14;   /* nach zwei weiteren ist Schluss */
    uft_fundus_walk(&f, gather, &two);
    CHECK(two.n == 16, "der Rueckruf kann abbrechen");

    uft_fundus_close(&f);
    cleanup();

    printf("\n%s (%d Abweichungen)\n",
           failures ? "FEHLGESCHLAGEN" : "OK", failures);
    return failures ? 1 : 0;
}
