/**
 * @file test_fundus_roundtrip.c
 * @brief Die Kette weiss, wohin ihr Ergebnis ging (MF-505).
 *
 * **Invarianten zuerst**: diese Datei entstand vor dem Code.
 *
 * ── Die Gegenrichtung ────────────────────────────────────────────────────
 *
 * Seit MF-504 zitiert ein Artefakt seine Herkunftskette: man findet vom
 * Artefakt zur Kette. Umgekehrt ging es nicht — die Kette wusste nicht,
 * in welchem Fundus ihr Ergebnis liegt. Wer eine Kette in der Hand hat,
 * musste den Fundus danach absuchen.
 *
 * ── Warum die Reihenfolge feststeht ──────────────────────────────────────
 *
 * Erst das Artefakt, dann der Ketteneintrag. Andersherum ginge es nicht:
 * der Eintrag muss das Artefakt BENENNEN, und sein Name steht erst nach
 * dem Ablegen fest. Ihn vorher zu reservieren waere moeglich — dann aber
 * behauptete die Kette einen Export, den ein fehlgeschlagenes Schreiben
 * nie ausgefuehrt hat. Eine Kette, die etwas Falsches behauptet, ist
 * schlimmer als eine unvollstaendige.
 *
 * Daraus folgt der wichtigste Fall dieser Datei: schlaegt der
 * Ketteneintrag fehl, **bleibt das Artefakt liegen**. Ein forensisches
 * Werkzeug loescht keine aufgenommenen Daten, um seine Buchfuehrung
 * aufzuraeumen.
 *
 * ── Kein Zirkel ──────────────────────────────────────────────────────────
 *
 * Das Artefakt zeigt auf den Kettenstand, aus dem es entstand; der neue
 * Ketteneintrag zeigt vorwaerts auf das Artefakt. Beides zusammen ist ein
 * Weg, kein Kreis: der zitierte Hash ist der Kopf VOR dem Anhaengen.
 */

#include "uft/uft_types.h"
#include "uft/forensic/uft_fundus_provenance.h"

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
    snprintf(out, n, "%s/uft_furt_%s_%d", d, tag, rand() % 1000000);
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

static uft_provenance_chain_t *make_chain(void)
{
    uft_provenance_chain_t *ch = uft_prov_create();
    if (!ch) return NULL;
    const uint8_t d1[] = "rohfluss";
    const uint8_t d2[] = "dekodiert";
    if (uft_prov_add(ch, UFT_PROV_CAPTURE, d1, sizeof(d1),
                     "Aufnahme Greaseweazle", "pruefstand") != 0 ||
        uft_prov_add(ch, UFT_PROV_DECODE, d2, sizeof(d2),
                     "AmigaDOS dekodiert", "pruefstand") != 0) {
        uft_prov_free(ch);
        return NULL;
    }
    return ch;
}

static void hex_of(const uint8_t *h, char out[65])
{
    for (int i = 0; i < 32; i++) snprintf(out + i * 2, 3, "%02x", h[i]);
    out[64] = 0;
}

/* ── Beide Richtungen ───────────────────────────────────────────────── */

TEST(the_chain_learns_where_its_result_went)
{
    /* Der Zweck. Nach dem Ablegen muss die Kette einen Eintrag mehr
     * haben, und der muss das Artefakt benennen. */
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "fwd");

    uft_provenance_chain_t *ch = make_chain();
    ASSERT(ch != NULL);
    uint32_t before = ch->count;

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));

    uft_fundus_meta_t extra;
    memset(&extra, 0, sizeof(extra));
    extra.identifier = "DISK-0815";

    const uint8_t art[] = "das Artefakt";
    char path[UFT_FUNDUS_PATH_MAX];
    ASSERT(uft_fundus_store_and_record(&f, ch, art, sizeof(art), "scp",
                                       &extra, path, sizeof(path)));

    ASSERT(ch->count == before + 1);
    const uft_prov_entry_t *e = &ch->entries[ch->count - 1];
    ASSERT(e->action == UFT_PROV_EXPORT);
    if (!strstr(e->description, "cap_0001"))
        printf("\n        Beschreibung nennt das Artefakt nicht: %s\n",
               e->description);
    ASSERT(strstr(e->description, "cap_0001") != NULL);

    /* Der Bediener kommt weiter aus der Kette, nicht aus der Luft. */
    ASSERT(strcmp(e->operator_id, "pruefstand") == 0);

    uft_fundus_close(&f);
    uft_prov_free(ch);
    wipe(dir);
}

TEST(the_artefact_points_at_the_head_before_the_export_entry)
{
    /* Kein Zirkel: das Artefakt zitiert den Stand, AUS DEM es entstand —
     * also den Kopf vor dem Anhaengen. Zitierte es den Kopf danach,
     * muesste dieser das Artefakt enthalten, das ihn zitiert. */
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "acyclic");

    uft_provenance_chain_t *ch = make_chain();
    ASSERT(ch != NULL);

    char head_before[65];
    hex_of(ch->entries[ch->count - 1].chain_hash, head_before);

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));
    const uint8_t art[] = "a";
    ASSERT(uft_fundus_store_and_record(&f, ch, art, sizeof(art), "bin",
                                       NULL, NULL, 0));

    char *m = slurp(f.manifest);
    ASSERT(m != NULL);
    if (!strstr(m, head_before))
        printf("\n        Manifest zitiert nicht den Kopf VOR dem Export\n");
    ASSERT(strstr(m, head_before) != NULL);

    /* Und ausdruecklich NICHT den neuen Kopf. */
    char head_after[65];
    hex_of(ch->entries[ch->count - 1].chain_hash, head_after);
    ASSERT(strcmp(head_before, head_after) != 0);
    ASSERT(strstr(m, head_after) == NULL);
    free(m);

    uft_fundus_close(&f);
    uft_prov_free(ch);
    wipe(dir);
}

TEST(the_chain_still_verifies_afterwards)
{
    /* Ein Eintrag, der die Verkettung bricht, waere schlimmer als kein
     * Eintrag: er machte die ganze Kette wertlos. */
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "verify");

    uft_provenance_chain_t *ch = make_chain();
    ASSERT(ch != NULL);
    ASSERT(uft_prov_verify(ch));

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));
    const uint8_t art[] = "a";
    ASSERT(uft_fundus_store_and_record(&f, ch, art, sizeof(art), "bin",
                                       NULL, NULL, 0));
    ASSERT(uft_prov_verify(ch));

    uft_fundus_close(&f);
    uft_prov_free(ch);
    wipe(dir);
}

TEST(from_the_artefact_one_finds_the_entry_it_came_from)
{
    /* Die Rundreise, die den ganzen Baustein rechtfertigt: Manifest ->
     * Kettenhash -> Eintrag in der Kette -> und der Eintrag danach ist
     * der Export, der auf dasselbe Artefakt zeigt. */
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "trip");

    uft_provenance_chain_t *ch = make_chain();
    ASSERT(ch != NULL);

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));
    const uint8_t art[] = "a";
    ASSERT(uft_fundus_store_and_record(&f, ch, art, sizeof(art), "scp",
                                       NULL, NULL, 0));

    char *m = slurp(f.manifest);
    ASSERT(m != NULL);
    const char *k = strstr(m, "\"chain_hash\":\"");
    ASSERT(k != NULL);
    char cited[65];
    snprintf(cited, sizeof(cited), "%.64s", k + 14);
    free(m);

    /* Den zitierten Eintrag in der Kette suchen. */
    int at = -1;
    for (uint32_t i = 0; i < ch->count; i++) {
        char h[65];
        hex_of(ch->entries[i].chain_hash, h);
        if (strcmp(h, cited) == 0) { at = (int)i; break; }
    }
    if (at < 0) printf("\n        zitierter Hash steht nicht in der Kette\n");
    ASSERT(at >= 0);
    /* Und direkt danach steht der Export auf dieses Artefakt. */
    ASSERT((uint32_t)at + 1 < ch->count);
    ASSERT(ch->entries[at + 1].action == UFT_PROV_EXPORT);
    ASSERT(strstr(ch->entries[at + 1].description, "cap_0001") != NULL);

    uft_fundus_close(&f);
    uft_prov_free(ch);
    wipe(dir);
}

/* ── Der Fehlerfall, der Daten kostet, wenn man ihn falsch macht ────── */

TEST(a_full_chain_keeps_the_artefact_and_says_so)
{
    /* Schlaegt der Ketteneintrag fehl, ist das Artefakt schon geschrieben.
     * Es dann zu loeschen waere Buchfuehrung auf Kosten von Daten — ein
     * forensisches Werkzeug tut das nicht. Der Aufrufer bekommt `false`
     * und findet die Aufnahme trotzdem im Fundus. */
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "full");

    uft_provenance_chain_t *ch = make_chain();
    ASSERT(ch != NULL);
    /* Bis an die Kapazitaetsgrenze fuellen. */
    const uint8_t d[] = "x";
    while (ch->count < UFT_PROV_MAX_ENTRIES)
        ASSERT(uft_prov_add(ch, UFT_PROV_ANALYZE, d, sizeof(d),
                            "fuellen", "pruefstand") == 0);
    ASSERT(ch->count == UFT_PROV_MAX_ENTRIES);

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));
    const uint8_t art[] = "wertvolle Aufnahme";
    char path[UFT_FUNDUS_PATH_MAX];
    path[0] = 0;
    ASSERT(uft_fundus_store_and_record(&f, ch, art, sizeof(art), "scp",
                                       NULL, path, sizeof(path)) == false);

    /* Trotzdem abgelegt — und auffindbar. */
    ASSERT(path[0] != 0);
    char *got = slurp(path);
    if (!got) printf("\n        Artefakt wurde geloescht: %s\n", path);
    ASSERT(got != NULL);
    ASSERT(memcmp(got, art, sizeof(art)) == 0);
    free(got);

    /* Und der Fundus weiss davon. */
    uft_fundus_verify_t r;
    ASSERT(uft_fundus_verify(&f, &r));
    ASSERT(r.checked == 1 && r.ok == 1);

    uft_fundus_close(&f);
    uft_prov_free(ch);
    wipe(dir);
}

TEST(a_broken_chain_leaves_nothing_behind_at_all)
{
    /* Hier ist der Abbruch VOR dem Schreiben, also darf auch nichts
     * liegenbleiben — der Unterschied zum Fall darueber. */
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "brk");

    uft_provenance_chain_t *ch = make_chain();
    ASSERT(ch != NULL);
    ch->entries[0].data_size ^= 0xFFu;
    ASSERT(uft_prov_verify(ch) == false);
    uint32_t before = ch->count;

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));
    const uint8_t art[] = "a";
    ASSERT(uft_fundus_store_and_record(&f, ch, art, sizeof(art), "bin",
                                       NULL, NULL, 0) == false);

    char *m = slurp(f.manifest);
    ASSERT(m != NULL);
    ASSERT(m[0] == 0);              /* kein Eintrag */
    free(m);
    ASSERT(ch->count == before);    /* und kein Ketteneintrag */

    uft_fundus_close(&f);
    uft_prov_free(ch);
    wipe(dir);
}

TEST(bad_arguments_are_refused_not_guessed)
{
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "args");

    uft_provenance_chain_t *ch = make_chain();
    ASSERT(ch != NULL);
    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));
    const uint8_t art[] = "a";

    /* Gegenprobe zuerst. */
    ASSERT(uft_fundus_store_and_record(&f, ch, art, sizeof(art), "bin",
                                       NULL, NULL, 0) == true);

    ASSERT(uft_fundus_store_and_record(NULL, ch, art, sizeof(art), "bin",
                                       NULL, NULL, 0) == false);
    ASSERT(uft_fundus_store_and_record(&f, NULL, art, sizeof(art), "bin",
                                       NULL, NULL, 0) == false);
    ASSERT(uft_fundus_store_and_record(&f, ch, NULL, 5, "bin",
                                       NULL, NULL, 0) == false);
    ASSERT(uft_fundus_store_and_record(&f, ch, art, 0, "bin",
                                       NULL, NULL, 0) == false);

    uft_fundus_close(&f);
    uft_prov_free(ch);
    wipe(dir);
}

int main(void)
{
    printf("=== Die Kette weiss, wohin ihr Ergebnis ging (MF-505) ===\n");
    RUN(the_chain_learns_where_its_result_went);
    RUN(the_artefact_points_at_the_head_before_the_export_entry);
    RUN(the_chain_still_verifies_afterwards);
    RUN(from_the_artefact_one_finds_the_entry_it_came_from);
    RUN(a_full_chain_keeps_the_artefact_and_says_so);
    RUN(a_broken_chain_leaves_nothing_behind_at_all);
    RUN(bad_arguments_are_refused_not_guessed);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
