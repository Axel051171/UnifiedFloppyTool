/**
 * @file test_fundus_provenance.c
 * @brief Ein Artefakt zitiert seine Herkunftskette (MF-504).
 *
 * **Invarianten zuerst**: diese Datei entstand vor der Bruecke und war
 * rot, bevor es sie gab.
 *
 * ── Die Doppelung, die hier verschwindet ─────────────────────────────────
 *
 * Seit MF-503 fuehren zwei Dinge Herkunft: die Provenienz-Kette
 * (`uft_provenance.h`, was mit den Daten geschah) und der Fundus
 * (`uft_fundus.h`, was gespeichert ist). Beide kennen Bediener und
 * Werkzeug. Zwei Quellen fuer dieselbe Tatsache sind aber kein Komfort,
 * sondern ein Widerspruch in Wartestellung — und genau diese Krankheit
 * hat dieses Projekt in dieser Woche fuenfzehnmal diagnostiziert.
 *
 * Die Bruecke loest das in eine Richtung: **die Kette ist die Quelle, der
 * Fundus zitiert sie.** Wer beim Ablegen Bediener oder Werkzeug selbst
 * angeben will, bekommt keinen stillen Vorrang, sondern eine Absage.
 *
 * ── Und was sie NICHT tut ────────────────────────────────────────────────
 *
 * Eine kaputte Kette wird nicht zitiert. Ein Verweis auf eine Kette, deren
 * Verkettung nicht mehr aufgeht, wuerde sie waschen — das Artefakt saehe
 * belegt aus, und der Beleg waere wertlos.
 */

#include "uft/uft_types.h"
#include "uft/forensic/uft_fundus_provenance.h"   /* zieht beide nach */

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

/** Frisches Verzeichnis — vorher aufraeumen, nicht nur nachher (MF-503). */
static void fresh_dir(char *out, size_t n, const char *tag)
{
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(out, n, "%s/uft_fuprov_%s_%d", d, tag, rand() % 1000000);
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

/** Eine kleine, gueltige Kette bauen. */
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

/** Kopfhash der Kette als Hex. */
static void head_hex(const uft_provenance_chain_t *ch, char out[65])
{
    const uint8_t *h = ch->entries[ch->count - 1].chain_hash;
    for (int i = 0; i < 32; i++) snprintf(out + i * 2, 3, "%02x", h[i]);
    out[64] = 0;
}

/* ── Die Kette wird zitiert ─────────────────────────────────────────── */

TEST(the_artefact_records_the_head_of_its_chain)
{
    /* Der Zweck. Ohne diesen Verweis liegt im Fundus ein Artefakt, dessen
     * Entstehungsweg zwar aufgezeichnet wurde — nur weiss niemand, welche
     * Aufzeichnung dazugehoert. */
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "cite");

    uft_provenance_chain_t *ch = make_chain();
    ASSERT(ch != NULL);

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));

    uft_fundus_meta_t extra;
    memset(&extra, 0, sizeof(extra));
    extra.identifier = "DISK-0815";
    extra.capture_protocol = "forensic_cooldown";

    const uint8_t art[] = "das Artefakt";
    ASSERT(uft_fundus_add_from_chain(&f, art, sizeof(art), "scp", ch,
                                     &extra, NULL, 0));

    char *m = slurp(f.manifest);
    ASSERT(m != NULL);

    char want[65];
    head_hex(ch, want);
    if (!strstr(m, want))
        printf("\n        Kettenhash %s fehlt im Manifest:\n        %s\n",
               want, m);
    ASSERT(strstr(m, want) != NULL);
    ASSERT(strstr(m, "\"chain_hash\"") != NULL);
    ASSERT(strstr(m, "DISK-0815") != NULL);
    free(m);

    uft_fundus_close(&f);
    uft_prov_free(ch);
    wipe(dir);
}

TEST(operator_and_tool_come_from_the_chain_not_the_caller)
{
    /* Die eigentliche Aufloesung der Doppelung: es gibt genau eine Quelle
     * fuer diese Tatsachen, und das ist die Kette. */
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "source");

    uft_provenance_chain_t *ch = make_chain();
    ASSERT(ch != NULL);

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));

    uft_fundus_meta_t extra;
    memset(&extra, 0, sizeof(extra));
    extra.identifier = "X";

    const uint8_t art[] = "a";
    ASSERT(uft_fundus_add_from_chain(&f, art, sizeof(art), "bin", ch,
                                     &extra, NULL, 0));

    char *m = slurp(f.manifest);
    ASSERT(m != NULL);
    /* „pruefstand" hat der Test NIE an den Fundus gegeben — nur an die
     * Kette. Steht es trotzdem im Manifest, kam es von dort. */
    ASSERT(strstr(m, "pruefstand") != NULL);
    ASSERT(strstr(m, "\"tool\"") != NULL);
    free(m);

    uft_fundus_close(&f);
    uft_prov_free(ch);
    wipe(dir);
}

TEST(setting_a_chain_owned_field_by_hand_is_refused)
{
    /* Kein stiller Vorrang. Wer denselben Wert an zwei Stellen angeben
     * kann, wird es irgendwann verschieden tun — und dann gibt es keinen
     * Weg mehr zu entscheiden, welcher gilt. Deshalb eine Absage und
     * kein Ueberschreiben. */
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "refuse");

    uft_provenance_chain_t *ch = make_chain();
    ASSERT(ch != NULL);

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));
    const uint8_t art[] = "a";

    uft_fundus_meta_t bad;
    memset(&bad, 0, sizeof(bad));
    bad.operator_id = "jemand anderes";
    ASSERT(uft_fundus_add_from_chain(&f, art, sizeof(art), "bin", ch,
                                     &bad, NULL, 0) == false);

    memset(&bad, 0, sizeof(bad));
    bad.tool = "ein anderes Werkzeug";
    ASSERT(uft_fundus_add_from_chain(&f, art, sizeof(art), "bin", ch,
                                     &bad, NULL, 0) == false);

    /* Und die Absage hinterlaesst nichts: kein Artefakt, kein Eintrag. */
    char *m = slurp(f.manifest);
    ASSERT(m != NULL);
    ASSERT(m[0] == 0);
    free(m);
    ASSERT(f.next_seq == 1);

    /* Gegenprobe: ohne die strittigen Felder geht es. */
    uft_fundus_meta_t good;
    memset(&good, 0, sizeof(good));
    good.identifier = "OK";
    ASSERT(uft_fundus_add_from_chain(&f, art, sizeof(art), "bin", ch,
                                     &good, NULL, 0) == true);

    uft_fundus_close(&f);
    uft_prov_free(ch);
    wipe(dir);
}

/* ── Eine kaputte Kette wird nicht gewaschen ────────────────────────── */

TEST(a_broken_chain_is_not_quoted)
{
    /* Der forensische Kern. Ein Verweis auf eine Kette, deren Verkettung
     * nicht mehr aufgeht, wuerde sie waschen: das Artefakt saehe belegt
     * aus, und der Beleg waere wertlos. */
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "broken");

    uft_provenance_chain_t *ch = make_chain();
    ASSERT(ch != NULL);
    ASSERT(uft_prov_verify(ch) == true);      /* vorher heil */

    /* Einen Eintrag nachtraeglich veraendern — genau das, wogegen eine
     * Hash-Kette gebaut ist. */
    ch->entries[0].data_size ^= 0xFFu;
    ASSERT(uft_prov_verify(ch) == false);     /* die Kette merkt es */

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));
    const uint8_t art[] = "a";
    uft_fundus_meta_t extra;
    memset(&extra, 0, sizeof(extra));
    extra.identifier = "X";

    ASSERT(uft_fundus_add_from_chain(&f, art, sizeof(art), "bin", ch,
                                     &extra, NULL, 0) == false);

    char *m = slurp(f.manifest);
    ASSERT(m != NULL);
    ASSERT(m[0] == 0);        /* nichts abgelegt */
    free(m);

    uft_fundus_close(&f);
    uft_prov_free(ch);
    wipe(dir);
}

TEST(an_empty_chain_has_nothing_to_quote)
{
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "empty");

    uft_provenance_chain_t *ch = uft_prov_create();
    ASSERT(ch != NULL);
    ASSERT(ch->count == 0);

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));
    const uint8_t art[] = "a";
    ASSERT(uft_fundus_add_from_chain(&f, art, sizeof(art), "bin", ch,
                                     NULL, NULL, 0) == false);

    uft_fundus_close(&f);
    uft_prov_free(ch);
    wipe(dir);
}

/* ── Ohne Kette bleibt alles wie vorher ─────────────────────────────── */

TEST(the_plain_path_claims_no_chain)
{
    /* Der Fundus muss ohne Kette benutzbar bleiben — und dann darf er
     * keinen Kettenverweis behaupten. */
    char dir[UFT_FUNDUS_PATH_MAX];
    fresh_dir(dir, sizeof(dir), "plain");

    uft_fundus_t f;
    ASSERT(uft_fundus_open(dir, &f));

    uft_fundus_meta_t m;
    memset(&m, 0, sizeof(m));
    m.identifier = "OHNE-KETTE";
    m.operator_id = "von Hand";      /* hier ist das erlaubt */

    const uint8_t art[] = "a";
    ASSERT(uft_fundus_add(&f, art, sizeof(art), "bin", &m, NULL, 0));

    char *txt = slurp(f.manifest);
    ASSERT(txt != NULL);
    ASSERT(strstr(txt, "OHNE-KETTE") != NULL);
    ASSERT(strstr(txt, "von Hand") != NULL);
    if (strstr(txt, "chain_hash"))
        printf("\n        Kettenverweis ohne Kette: %s\n", txt);
    ASSERT(strstr(txt, "chain_hash") == NULL);
    free(txt);

    uft_fundus_close(&f);
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

    /* Gegenprobe zuerst: die gueltige Form muss durchgehen, sonst waere
     * „lehnt Unbrauchbares ab" von „lehnt alles ab" nicht zu trennen. */
    ASSERT(uft_fundus_add_from_chain(&f, art, sizeof(art), "bin", ch,
                                     NULL, NULL, 0) == true);

    ASSERT(uft_fundus_add_from_chain(NULL, art, sizeof(art), "bin", ch,
                                     NULL, NULL, 0) == false);
    ASSERT(uft_fundus_add_from_chain(&f, NULL, 5, "bin", ch,
                                     NULL, NULL, 0) == false);
    ASSERT(uft_fundus_add_from_chain(&f, art, 0, "bin", ch,
                                     NULL, NULL, 0) == false);
    ASSERT(uft_fundus_add_from_chain(&f, art, sizeof(art), "bin", NULL,
                                     NULL, NULL, 0) == false);

    uft_fundus_close(&f);
    uft_prov_free(ch);
    wipe(dir);
}

int main(void)
{
    printf("=== Artefakt zitiert seine Herkunftskette (MF-504) ===\n");
    RUN(the_artefact_records_the_head_of_its_chain);
    RUN(operator_and_tool_come_from_the_chain_not_the_caller);
    RUN(setting_a_chain_owned_field_by_hand_is_refused);
    RUN(a_broken_chain_is_not_quoted);
    RUN(an_empty_chain_has_nothing_to_quote);
    RUN(the_plain_path_claims_no_chain);
    RUN(bad_arguments_are_refused_not_guessed);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
