/**
 * @file test_fundus.c
 * @brief Die Zusicherungen des Aufnahme-Speichers (MF-503).
 *
 * Baustein 1.3 des Mammut-Plans, **Invarianten zuerst**: diese Datei
 * entstand vor der Implementierung und war rot, bevor es sie gab.
 *
 * Ein Archiv laesst sich nicht gegen ein Oracle pruefen — es gibt kein
 * Fremdwerkzeug, das denselben Fundus baut. Was sich pruefen laesst, sind
 * seine Zusicherungen, und die sind hier die Tests:
 *
 *   - angehaengt, nie ersetzt (Bytes UND Manifest)
 *   - Nummern werden nie wiederverwendet
 *   - ohne UFT pruefbar (sha256sum-Format)
 *   - Beschaedigung wird gefunden, nicht uebersehen
 *   - keine erfundenen Angaben
 */

#include "uft/uft_types.h"
#include "uft/forensic/uft_fundus.h"
#include "uft/core/uft_sha256.h"

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

static void temp_dir(char *out, size_t n, const char *tag)
{
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(out, n, "%s/uft_fundus_%s_%d", d, tag, rand() % 1000000);
}

static uint8_t *slurp(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    uint8_t *b = (uint8_t *)malloc((size_t)sz + 1);
    if (!b) { fclose(f); return NULL; }
    size_t got = fread(b, 1, (size_t)sz, f);
    fclose(f);
    b[got] = 0;
    if (len) *len = got;
    return b;
}

/**
 * Verzeichnis samt Inhalt entfernen.
 *
 * Unter Windows braucht `rmdir` RUECKWAERTS-Schraegstriche; mit den
 * Vorwaerts-Schraegstrichen, die der Rest des Tests benutzt, tut es
 * stillschweigend nichts. Genau daran ist die erste Fassung dieser Datei
 * gescheitert — die Verzeichnisse ueberlebten den Lauf, und weil `rand()`
 * ungesaet immer dieselbe Folge liefert, fand der naechste Lauf sie
 * wieder vor. Die Tests waren damit von ihrer eigenen Aufraeumroutine
 * abhaengig, und ein Rotbeweis meldete "auch nach Wiederherstellung rot".
 */
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

/**
 * Ein frisches Verzeichnis fuer einen Test.
 *
 * Raeumt VORHER auf, nicht nur nachher: ein abgebrochener Lauf darf den
 * naechsten nicht beeinflussen. Ein Test, der nur mit sauberer Umgebung
 * besteht, prueft die Umgebung mit.
 */
static void fresh_dir(char *out, size_t n, const char *tag)
{
    temp_dir(out, n, tag);
    wipe(out);
}

static const uft_fundus_meta_t META = {
    "DISK-0001", "Testaufnahme", "keine Besonderheiten",
    "pruefstand", "forensic_cooldown", "uft-test 1.0"
};

/* ── Anhaengend, nie ersetzend ──────────────────────────────────────── */

TEST(a_second_capture_leaves_the_first_untouched)
{
    /* Die Grundzusicherung. Wer ein Archiv fuehrt, muss sich darauf
     * verlassen koennen, dass eine spaetere Aufnahme eine fruehere nicht
     * anfasst — sonst ist es kein Archiv, sondern ein Arbeitsverzeichnis. */
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "append");

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));

    const uint8_t a[] = "erste Aufnahme";
    char p1[UFT_FUNDUS_PATH_MAX];
    ASSERT(uft_fundus_add(&f, a, sizeof(a), "scp", &META, p1, sizeof(p1)));

    size_t n1 = 0;
    uint8_t *before = slurp(p1, &n1);
    ASSERT(before != NULL);

    const uint8_t b[] = "zweite Aufnahme, laenger als die erste";
    ASSERT(uft_fundus_add(&f, b, sizeof(b), "scp", &META, NULL, 0));

    size_t n2 = 0;
    uint8_t *after = slurp(p1, &n2);
    ASSERT(after != NULL);
    ASSERT(n1 == n2);
    ASSERT(memcmp(before, after, n1) == 0);

    free(before); free(after);
    uft_fundus_close(&f);
    wipe(dir);
}

TEST(the_manifest_only_grows_at_the_end)
{
    /* Deshalb eine Zeile je Eintrag und kein JSON-Array: ein Array liesse
     * sich nicht anhaengen, ohne die Datei neu zu schreiben. Geprueft wird
     * genau das — die alten Bytes muessen Praefix der neuen sein. */
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "grow");

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));
    const uint8_t d[] = "x";
    ASSERT(uft_fundus_add(&f, d, sizeof(d), "bin", &META, NULL, 0));

    size_t n1 = 0;
    uint8_t *m1 = slurp(f.manifest, &n1);
    ASSERT(m1 != NULL && n1 > 0);

    ASSERT(uft_fundus_add(&f, d, sizeof(d), "bin", &META, NULL, 0));
    size_t n2 = 0;
    uint8_t *m2 = slurp(f.manifest, &n2);
    ASSERT(m2 != NULL);

    if (n2 <= n1 || memcmp(m1, m2, n1) != 0)
        printf("\n        Manifest wurde umgeschrieben (%zu -> %zu)\n",
               n1, n2);
    ASSERT(n2 > n1);
    ASSERT(memcmp(m1, m2, n1) == 0);

    free(m1); free(m2);
    uft_fundus_close(&f);
    wipe(dir);
}

TEST(a_reopened_fundus_continues_the_numbering)
{
    /* Eine unterbrochene Sitzung darf nicht bei 1 anfangen — sonst
     * ueberschriebe die naechste Aufnahme die erste. */
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "resume");

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));
    const uint8_t d[] = "x";
    char p1[UFT_FUNDUS_PATH_MAX];
    ASSERT(uft_fundus_add(&f, d, sizeof(d), "bin", &META, p1, sizeof(p1)));
    ASSERT(uft_fundus_add(&f, d, sizeof(d), "bin", &META, NULL, 0));
    uft_fundus_close(&f);

    uft_fundus_t g;
    ASSERT(uft_fundus_open(dir, &g));
    if (g.next_seq < 3)
        printf("\n        naechste Nummer %u, erwartet mindestens 3\n",
               g.next_seq);
    ASSERT(g.next_seq >= 3);

    char p3[UFT_FUNDUS_PATH_MAX];
    ASSERT(uft_fundus_add(&g, d, sizeof(d), "bin", &META, p3, sizeof(p3)));
    ASSERT(strcmp(p1, p3) != 0);     /* auf keinen Fall derselbe Name */

    uft_fundus_close(&g);
    wipe(dir);
}

TEST(a_deleted_artefact_does_not_free_its_number)
{
    /* Wer von aussen loescht, darf keine Nummer freigeben: zwei
     * verschiedene Aufnahmen unter einem Namen waeren nicht mehr
     * auseinanderzuhalten. Das Manifest, nicht das Verzeichnis, fuehrt
     * die Nummern. */
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "gap");

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));
    const uint8_t d[] = "x";
    char p1[UFT_FUNDUS_PATH_MAX], p2[UFT_FUNDUS_PATH_MAX];
    ASSERT(uft_fundus_add(&f, d, sizeof(d), "bin", &META, p1, sizeof(p1)));
    ASSERT(uft_fundus_add(&f, d, sizeof(d), "bin", &META, p2, sizeof(p2)));
    uft_fundus_close(&f);

    ASSERT(remove(p1) == 0);          /* jemand raeumt auf */

    uft_fundus_t g;
    ASSERT(uft_fundus_open(dir, &g));
    char p3[UFT_FUNDUS_PATH_MAX];
    ASSERT(uft_fundus_add(&g, d, sizeof(d), "bin", &META, p3, sizeof(p3)));
    ASSERT(strcmp(p3, p1) != 0);
    ASSERT(strcmp(p3, p2) != 0);
    uft_fundus_close(&g);
    wipe(dir);
}

/* ── Ohne UFT pruefbar ──────────────────────────────────────────────── */

TEST(the_sidecar_is_what_sha256sum_expects)
{
    /* Ein Archiv, dessen Integritaet nur das erzeugende Programm
     * bestaetigen kann, ist als Archiv wenig wert. Geprueft wird das
     * Format Zeichen fuer Zeichen: 64 Hex, zwei Leerzeichen, Dateiname. */
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "sidecar");

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));
    const uint8_t d[] = "Inhalt mit bekannter Pruefsumme";
    char p[UFT_FUNDUS_PATH_MAX];
    ASSERT(uft_fundus_add(&f, d, sizeof(d), "bin", &META, p, sizeof(p)));

    char side[UFT_FUNDUS_PATH_MAX + 8];
    snprintf(side, sizeof(side), "%s.sha256", p);
    size_t n = 0;
    char *txt = (char *)slurp(side, &n);
    ASSERT(txt != NULL);

    /* 64 Hexziffern */
    ASSERT(n > 64 + 2);
    for (int i = 0; i < 64; i++) {
        char c = txt[i];
        ASSERT((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }
    /* zwei Leerzeichen (sha256sum, Binaermodus: Leerzeichen + Stern
     * waere der andere zulaessige Fall; hier gilt der Textmodus nicht,
     * weil Artefakte binaer sind) */
    ASSERT(txt[64] == ' ');
    ASSERT(txt[65] == ' ' || txt[65] == '*');
    /* Danach ein Dateiname ohne Verzeichnisanteil — sonst laesst sich die
     * Datei nicht neben ihrer Pruefsumme verschieben. */
    ASSERT(strchr(txt + 66, '/') == NULL);
    ASSERT(strchr(txt + 66, '\\') == NULL);

    /* Und die Pruefsumme muss stimmen. */
    uint8_t want[32];
    uft_sha256(d, sizeof(d), want);
    char hex[65];
    for (int i = 0; i < 32; i++) snprintf(hex + i * 2, 3, "%02x", want[i]);
    if (strncmp(txt, hex, 64) != 0)
        printf("\n        Sidecar %.64s\n        erwartet %s\n", txt, hex);
    ASSERT(strncmp(txt, hex, 64) == 0);

    free(txt);
    uft_fundus_close(&f);
    wipe(dir);
}

/* ── Beschaedigung wird gefunden ────────────────────────────────────── */

TEST(verify_passes_on_an_untouched_fundus)
{
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "vok");

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));
    const uint8_t d[] = "unversehrt";
    for (int i = 0; i < 3; i++)
        ASSERT(uft_fundus_add(&f, d, sizeof(d), "bin", &META, NULL, 0));

    uft_fundus_verify_t r;
    ASSERT(uft_fundus_verify(&f, &r));
    if (r.ok != 3 || r.mismatched || r.missing)
        printf("\n        %d geprueft, %d ok, %d falsch, %d fehlt\n",
               r.checked, r.ok, r.mismatched, r.missing);
    ASSERT(r.checked == 3);
    ASSERT(r.ok == 3);
    ASSERT(r.mismatched == 0 && r.missing == 0);
    ASSERT(r.first_bad[0] == 0);

    uft_fundus_close(&f);
    wipe(dir);
}

TEST(a_single_flipped_byte_is_found)
{
    /* Der Fall, fuer den ein Archiv Pruefsummen fuehrt. Ein Bit-Dreher
     * durch alternde Datentraeger sieht nach nichts aus — er muss
     * trotzdem auffallen, und zwar benannt. */
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "flip");

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));
    const uint8_t d[] = "Aufnahme, die spaeter kippt";
    char p[UFT_FUNDUS_PATH_MAX];
    ASSERT(uft_fundus_add(&f, d, sizeof(d), "bin", &META, p, sizeof(p)));
    ASSERT(uft_fundus_add(&f, d, sizeof(d), "bin", &META, NULL, 0));

    FILE *fp = fopen(p, "r+b");
    ASSERT(fp != NULL);
    ASSERT(fseek(fp, 3, SEEK_SET) == 0);
    fputc(0x00, fp);
    fclose(fp);

    uft_fundus_verify_t r;
    ASSERT(uft_fundus_verify(&f, &r));
    ASSERT(r.checked == 2);
    ASSERT(r.mismatched == 1);
    ASSERT(r.ok == 1);
    ASSERT(r.first_bad[0] != 0);      /* und es steht drin, welcher */

    uft_fundus_close(&f);
    wipe(dir);
}

TEST(a_missing_artefact_is_not_the_same_as_a_broken_one)
{
    /* Zwei verschiedene Befunde mit zwei verschiedenen Abhilfen: eine
     * fehlende Datei sucht man, eine verfaelschte liest man neu ein. Sie
     * zusammenzufassen waere bequem und falsch. */
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "gone");

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));
    const uint8_t d[] = "verschwindet gleich";
    char p[UFT_FUNDUS_PATH_MAX];
    ASSERT(uft_fundus_add(&f, d, sizeof(d), "bin", &META, p, sizeof(p)));
    ASSERT(remove(p) == 0);

    uft_fundus_verify_t r;
    ASSERT(uft_fundus_verify(&f, &r));
    ASSERT(r.checked == 1);
    ASSERT(r.missing == 1);
    ASSERT(r.mismatched == 0);
    ASSERT(r.ok == 0);
    /* Und WELCHE Datei fehlt, muss dastehen. Ein Bericht, der nur eine
     * Zahl nennt, zwingt zum Suchen — bei einem Archiv mit tausend
     * Aufnahmen ist das der Unterschied zwischen Befund und Ahnung.
     * Ein Rotbeweis hat gezeigt, dass diese Zusicherung im Fehlt-Fall
     * ungeprueft war, waehrend sie im Kaputt-Fall geprueft wurde. */
    ASSERT(r.first_bad[0] != 0);
    ASSERT(strstr(r.first_bad, "cap_") != NULL);

    uft_fundus_close(&f);
    wipe(dir);
}

/* ── Keine erfundenen Angaben ───────────────────────────────────────── */

TEST(what_the_caller_did_not_say_is_not_written)
{
    /* „Unbekannter Bediener" ist eine andere Aussage als „Bediener:
     * unbekannt". Ein Archiv, das Luecken mit Standardwerten fuellt,
     * erzeugt Herkunftsangaben, die niemand gemacht hat. */
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "sparse");

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));

    uft_fundus_meta_t m;
    memset(&m, 0, sizeof(m));
    m.identifier = "NUR-KENNUNG";

    const uint8_t d[] = "x";
    ASSERT(uft_fundus_add(&f, d, sizeof(d), "bin", &m, NULL, 0));

    size_t n = 0;
    char *txt = (char *)slurp(f.manifest, &n);
    ASSERT(txt != NULL);
    ASSERT(strstr(txt, "NUR-KENNUNG") != NULL);
    if (strstr(txt, "operator") != NULL)
        printf("\n        Manifest: %s\n", txt);
    ASSERT(strstr(txt, "operator") == NULL);
    ASSERT(strstr(txt, "unknown") == NULL);
    ASSERT(strstr(txt, "unbekannt") == NULL);
    free(txt);

    /* Auch ganz ohne Angaben muss es gehen — dann steht eben nur, was
     * der Fundus selbst weiss. */
    ASSERT(uft_fundus_add(&f, d, sizeof(d), "bin", NULL, NULL, 0));

    uft_fundus_close(&f);
    wipe(dir);
}

TEST(a_quote_in_a_note_does_not_break_the_manifest)
{
    /* Freitext enthaelt irgendwann Anfuehrungszeichen oder einen
     * Zeilenumbruch. Bricht das die Zeile, ist ab da das ganze Manifest
     * unlesbar — und ein Archiv mit unlesbarem Manifest ist ein Haufen
     * Dateien. */
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "quote");

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));

    uft_fundus_meta_t m;
    memset(&m, 0, sizeof(m));
    m.identifier = "Q";
    m.notes = "Rand \"leicht\" verzogen\nzweite Zeile\\ende";

    const uint8_t d[] = "x";
    ASSERT(uft_fundus_add(&f, d, sizeof(d), "bin", &m, NULL, 0));
    ASSERT(uft_fundus_add(&f, d, sizeof(d), "bin", &m, NULL, 0));

    size_t n = 0;
    char *txt = (char *)slurp(f.manifest, &n);
    ASSERT(txt != NULL);

    /* Genau zwei Zeilen — der Umbruch im Freitext darf keine dritte
     * erzeugt haben. */
    int lines = 0;
    for (size_t i = 0; i < n; i++) if (txt[i] == '\n') lines++;
    if (lines != 2) printf("\n        %d Zeilen statt 2:\n%s\n", lines, txt);
    ASSERT(lines == 2);
    free(txt);

    uft_fundus_close(&f);
    wipe(dir);
}

/* ── Randfaelle ─────────────────────────────────────────────────────── */

TEST(bad_arguments_are_refused_not_guessed)
{
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "args");

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f) == true);      /* Gegenprobe zuerst */

    const uint8_t d[] = "x";
    ASSERT(uft_fundus_add(NULL, d, sizeof(d), "bin", &META, NULL, 0) == false);
    ASSERT(uft_fundus_add(&f, NULL, 10, "bin", &META, NULL, 0) == false);
    ASSERT(uft_fundus_add(&f, d, 0, "bin", &META, NULL, 0) == false);

    uft_fundus_verify_t r;
    ASSERT(uft_fundus_verify(NULL, &r) == false);
    ASSERT(uft_fundus_verify(&f, NULL) == false);

    ASSERT(uft_fundus_open(NULL, &f) == false);
    ASSERT(uft_fundus_open(dir, NULL) == false);

    uft_fundus_close(&f);
    uft_fundus_close(NULL);
    wipe(dir);
}

int main(void)
{
    printf("=== Aufnahme-Speicher mit Herkunft (MF-503) ===\n");
    RUN(a_second_capture_leaves_the_first_untouched);
    RUN(the_manifest_only_grows_at_the_end);
    RUN(a_reopened_fundus_continues_the_numbering);
    RUN(a_deleted_artefact_does_not_free_its_number);
    RUN(the_sidecar_is_what_sha256sum_expects);
    RUN(verify_passes_on_an_untouched_fundus);
    RUN(a_single_flipped_byte_is_found);
    RUN(a_missing_artefact_is_not_the_same_as_a_broken_one);
    RUN(what_the_caller_did_not_say_is_not_written);
    RUN(a_quote_in_a_note_does_not_break_the_manifest);
    RUN(bad_arguments_are_refused_not_guessed);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
