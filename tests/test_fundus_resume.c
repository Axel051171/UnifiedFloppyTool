/**
 * @file test_fundus_resume.c
 * @brief Die naechste Sitzung weiss, was die letzte wusste (MF-506).
 *
 * **Invarianten zuerst**: diese Datei entstand vor dem Code.
 *
 * ── Wozu ─────────────────────────────────────────────────────────────────
 *
 * Wer eine Diskette zum zweiten Mal einlegt, soll Kennung, Beschreibung,
 * Notizen und Aufnahme-Rezept nicht neu tippen. Getippte Angaben, die
 * jemand zum dritten Mal eingibt, weichen voneinander ab — und dann steht
 * dieselbe Diskette unter zwei Beschreibungen im Archiv.
 *
 * ── Abweichung vom Plan, ausdruecklich ───────────────────────────────────
 *
 * Der Plan sagt, unterbrochene Aufnahmen wuerden beim Fortsetzen
 * „ergaenzt statt neu nummeriert". Woertlich hiesse das: den bestehenden
 * Eintrag aendern. Das geht nicht — der Fundus haengt an, er ersetzt
 * nicht (FUND-1), und diese Zusicherung ist mehr wert als die Bequem-
 * lichkeit eines einzelnen Eintrags.
 *
 * „Ergaenzt" heisst hier deshalb: die Fortsetzung ist ein **neuer**
 * Eintrag, der auf den unterbrochenen **verweist**. Im Manifest steht
 * danach die ganze Geschichte — Versuch 1 abgebrochen, Versuch 2
 * fortgesetzt —, statt eines Eintrags, dem man nicht mehr ansieht, dass
 * er einmal unvollstaendig war.
 *
 * ── Und was NICHT behauptet wird ─────────────────────────────────────────
 *
 * „Vollstaendig" ist eine Aussage und wird nur geschrieben, wenn jemand
 * sie macht. Eine genullte Angabe behauptet nichts.
 */

#include "uft/uft_types.h"
#include "uft/forensic/uft_fundus.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-56s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                    _fail++; return; } } while (0)

/* ── Hilfen ─────────────────────────────────────────────────────────── */

static void wipe(const char *dir)
{
    char cmd[UFT_FUNDUS_PATH_MAX + 64];
#ifdef _WIN32
    char win[UFT_FUNDUS_PATH_MAX];
    snprintf(win, sizeof(win), "%s", dir);
    for (char *p = win; *p; p++) if (*p == '/') *p = '\\';
    snprintf(cmd, sizeof(cmd), "rmdir /s /q \"%s\" 2>nul", win);
#else
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", dir);
#endif
    if (system(cmd) != 0) { /* schon weg ist auch recht */ }
}

static void fresh_dir(char *out, size_t n, const char *tag)
{
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(out, n, "%s/uft_furesume_%s_%d", d, tag, rand() % 1000000);
    wipe(out);
}

static char *slurp(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    char *b = (char *)malloc((size_t)sz + 1);
    if (!b) { fclose(f); return NULL; }
    size_t got = fread(b, 1, (size_t)sz, f);
    fclose(f);
    b[got] = 0;
    return b;
}

static const uint8_t DATA[] = "aufnahme";

/* ── Wiedererkennen ─────────────────────────────────────────────────── */

TEST(the_newest_entry_for_an_identifier_comes_back)
{
    /* Der Zweck: beim Wiedereinlegen die Angaben der LETZTEN Sitzung, nicht
     * die der ersten. Wer zwischendurch die Beschreibung praezisiert hat,
     * will die praezisere zurueck. */
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "recall");

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));

    uft_fundus_meta_t m;
    memset(&m, 0, sizeof(m));
    m.identifier = "DISK-0007";
    m.description = "erste Beschreibung";
    m.capture_protocol = "sweep";
    ASSERT(uft_fundus_add(&f, DATA, sizeof(DATA), "scp", &m, NULL, 0));

    m.description = "zweite, genauere Beschreibung";
    m.notes = "Rand leicht verzogen";
    ASSERT(uft_fundus_add(&f, DATA, sizeof(DATA), "scp", &m, NULL, 0));

    uft_fundus_recall_t r;
    ASSERT(uft_fundus_recall(&f, "DISK-0007", &r));
    ASSERT(r.found);
    ASSERT(r.seq == 2);
    if (strcmp(r.description, "zweite, genauere Beschreibung") != 0)
        printf("\n        zurueck kam: %s\n", r.description);
    ASSERT(strcmp(r.description, "zweite, genauere Beschreibung") == 0);
    ASSERT(strcmp(r.notes, "Rand leicht verzogen") == 0);
    ASSERT(strcmp(r.capture_protocol, "sweep") == 0);

    uft_fundus_close(&f);
    wipe(dir);
}

TEST(an_unknown_disk_invents_nothing)
{
    /* Wo nichts war, darf nichts zurueckkommen — auch keine leeren
     * Zeichenketten, die spaeter als „steht so im Archiv" gelesen werden. */
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "unknown");

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));

    uft_fundus_meta_t m;
    memset(&m, 0, sizeof(m));
    m.identifier = "DISK-A";
    m.description = "etwas";
    ASSERT(uft_fundus_add(&f, DATA, sizeof(DATA), "bin", &m, NULL, 0));

    uft_fundus_recall_t r;
    ASSERT(uft_fundus_recall(&f, "DISK-B", &r));
    ASSERT(r.found == false);
    ASSERT(r.seq == 0);
    ASSERT(r.description[0] == 0);
    ASSERT(r.notes[0] == 0);

    uft_fundus_close(&f);
    wipe(dir);
}

TEST(a_longer_identifier_is_not_the_same_disk)
{
    /* „DISK-1" und „DISK-10" sind zwei Disketten. Ein Vergleich, der nur
     * nach dem Vorkommen sucht, wuerde sie verwechseln — und dann traegt
     * eine Diskette die Notizen einer anderen. */
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "prefix");

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));

    uft_fundus_meta_t m;
    memset(&m, 0, sizeof(m));
    m.identifier = "DISK-10";
    m.description = "die zehnte";
    ASSERT(uft_fundus_add(&f, DATA, sizeof(DATA), "bin", &m, NULL, 0));

    uft_fundus_recall_t r;
    ASSERT(uft_fundus_recall(&f, "DISK-1", &r));
    if (r.found) printf("\n        DISK-1 fand DISK-10: %s\n", r.description);
    ASSERT(r.found == false);

    /* Und die richtige wird gefunden. */
    ASSERT(uft_fundus_recall(&f, "DISK-10", &r));
    ASSERT(r.found);
    ASSERT(strcmp(r.description, "die zehnte") == 0);

    uft_fundus_close(&f);
    wipe(dir);
}

TEST(escaped_text_survives_the_round_trip)
{
    /* Freitext mit Anfuehrungszeichen und Zeilenumbruch wird beim
     * Schreiben maskiert (FUND-1). Kommt er unmaskiert zurueck, ist die
     * Wiedererkennung eine Verfaelschung — und der Bediener uebernimmt
     * beim naechsten Mal einen Text, der nie so dastand. */
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "escape");

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));

    const char *tricky = "Rand \"leicht\" verzogen\nzweite Zeile\tTab";
    uft_fundus_meta_t m;
    memset(&m, 0, sizeof(m));
    m.identifier = "DISK-ESC";
    m.notes = tricky;
    ASSERT(uft_fundus_add(&f, DATA, sizeof(DATA), "bin", &m, NULL, 0));

    uft_fundus_recall_t r;
    ASSERT(uft_fundus_recall(&f, "DISK-ESC", &r));
    ASSERT(r.found);
    if (strcmp(r.notes, tricky) != 0)
        printf("\n        zurueck: %s\n        erwartet: %s\n",
               r.notes, tricky);
    ASSERT(strcmp(r.notes, tricky) == 0);

    uft_fundus_close(&f);
    wipe(dir);
}

/* ── Unterbrochen und fortgesetzt ───────────────────────────────────── */

TEST(nothing_claims_completeness_unless_someone_says_so)
{
    /* Eine genullte Angabe behauptet nichts. Waere „vollstaendig" der
     * Standardwert, truege jede Aufnahme eine Aussage, die niemand
     * gemacht hat. */
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "silent");

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));

    uft_fundus_meta_t m;
    memset(&m, 0, sizeof(m));
    m.identifier = "DISK-Q";
    ASSERT(m.state == UFT_FUNDUS_STATE_UNSPECIFIED);
    ASSERT(uft_fundus_add(&f, DATA, sizeof(DATA), "bin", &m, NULL, 0));

    char *txt = slurp(f.manifest);
    ASSERT(txt != NULL);
    if (strstr(txt, "state"))
        printf("\n        Zustand behauptet ohne Angabe: %s\n", txt);
    ASSERT(strstr(txt, "state") == NULL);
    free(txt);

    uft_fundus_recall_t r;
    ASSERT(uft_fundus_recall(&f, "DISK-Q", &r));
    ASSERT(r.found);
    ASSERT(r.state == UFT_FUNDUS_STATE_UNSPECIFIED);

    uft_fundus_close(&f);
    wipe(dir);
}

TEST(an_interrupted_capture_says_so_and_is_recalled_as_such)
{
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "interrupt");

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));

    uft_fundus_meta_t m;
    memset(&m, 0, sizeof(m));
    m.identifier = "DISK-INT";
    m.description = "abgebrochen nach Spur 40";
    m.state = UFT_FUNDUS_STATE_INTERRUPTED;
    ASSERT(uft_fundus_add(&f, DATA, sizeof(DATA), "scp", &m, NULL, 0));

    char *txt = slurp(f.manifest);
    ASSERT(txt != NULL);
    ASSERT(strstr(txt, "interrupted") != NULL);
    free(txt);

    uft_fundus_recall_t r;
    ASSERT(uft_fundus_recall(&f, "DISK-INT", &r));
    ASSERT(r.found);
    ASSERT(r.state == UFT_FUNDUS_STATE_INTERRUPTED);

    uft_fundus_close(&f);
    wipe(dir);
}

TEST(a_continuation_is_a_new_entry_that_points_back)
{
    /* Die Abweichung vom Plan, ausgeschrieben. „Ergaenzt" heisst NICHT
     * „den alten Eintrag aendern" — das waere gegen die Anhaenge-
     * Zusicherung. Es heisst: ein neuer Eintrag mit einem Verweis, und im
     * Manifest steht danach die ganze Geschichte. */
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "continue");

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));

    uft_fundus_meta_t m;
    memset(&m, 0, sizeof(m));
    m.identifier = "DISK-C";
    m.state = UFT_FUNDUS_STATE_INTERRUPTED;
    char p1[UFT_FUNDUS_PATH_MAX];
    ASSERT(uft_fundus_add(&f, DATA, sizeof(DATA), "scp", &m, p1, sizeof(p1)));

    size_t n1 = 0;
    char *m1 = slurp(f.manifest);
    ASSERT(m1 != NULL);
    n1 = strlen(m1);

    /* Fortsetzen. */
    uft_fundus_recall_t r;
    ASSERT(uft_fundus_recall(&f, "DISK-C", &r));
    ASSERT(r.found && r.state == UFT_FUNDUS_STATE_INTERRUPTED);

    uft_fundus_meta_t c;
    memset(&c, 0, sizeof(c));
    c.identifier = "DISK-C";
    c.state = UFT_FUNDUS_STATE_COMPLETE;
    c.continues_seq = r.seq;
    char p2[UFT_FUNDUS_PATH_MAX];
    ASSERT(uft_fundus_add(&f, DATA, sizeof(DATA), "scp", &c, p2, sizeof(p2)));

    /* Neue Nummer, nicht dieselbe. */
    ASSERT(strcmp(p1, p2) != 0);

    char *m2 = slurp(f.manifest);
    ASSERT(m2 != NULL);
    /* Das Manifest ist nur gewachsen — der alte Eintrag steht unveraendert
     * am Anfang. */
    ASSERT(strlen(m2) > n1);
    ASSERT(memcmp(m1, m2, n1) == 0);
    ASSERT(strstr(m2, "\"continues\":1") != NULL);
    free(m1); free(m2);

    /* Und die Wiedererkennung liefert jetzt die FORTSETZUNG. */
    ASSERT(uft_fundus_recall(&f, "DISK-C", &r));
    ASSERT(r.seq == 2);
    ASSERT(r.state == UFT_FUNDUS_STATE_COMPLETE);
    ASSERT(r.continues_seq == 1);

    uft_fundus_close(&f);
    wipe(dir);
}

TEST(bad_arguments_are_refused_not_guessed)
{
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "args");

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));
    uft_fundus_meta_t m;
    memset(&m, 0, sizeof(m));
    m.identifier = "D";
    ASSERT(uft_fundus_add(&f, DATA, sizeof(DATA), "bin", &m, NULL, 0));

    uft_fundus_recall_t r;
    ASSERT(uft_fundus_recall(&f, "D", &r) == true);     /* Gegenprobe */
    ASSERT(uft_fundus_recall(NULL, "D", &r) == false);
    ASSERT(uft_fundus_recall(&f, NULL, &r) == false);
    ASSERT(uft_fundus_recall(&f, "D", NULL) == false);
    ASSERT(uft_fundus_recall(&f, "", &r) == false);

    uft_fundus_close(&f);
    wipe(dir);
}

int main(void)
{
    printf("=== Die naechste Sitzung weiss, was die letzte wusste (MF-506) ===\n");
    RUN(the_newest_entry_for_an_identifier_comes_back);
    RUN(an_unknown_disk_invents_nothing);
    RUN(a_longer_identifier_is_not_the_same_disk);
    RUN(escaped_text_survives_the_round_trip);
    RUN(nothing_claims_completeness_unless_someone_says_so);
    RUN(an_interrupted_capture_says_so_and_is_recalled_as_such);
    RUN(a_continuation_is_a_new_entry_that_points_back);
    RUN(bad_arguments_are_refused_not_guessed);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
