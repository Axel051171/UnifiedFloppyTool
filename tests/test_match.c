/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_match.c
 * @brief Waechter fuer uft_match — die sicheren Ersatzfunktionen.
 *
 * Jede Gruppe prueft die Falle UND den Ersatz: erst, dass `strstr` hier
 * zuschlaegt, dann, dass `uft_suffix_eq` es nicht tut. Ohne die erste
 * Haelfte beweist die zweite nichts.
 */

#include "uft/util/uft_match.h"

#include <stdio.h>
#include <string.h>

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { \
    printf("  FEHLGESCHLAGEN (%s:%d): ", __func__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); g_fail++; } } while (0)

/* ═══════════ 1. Endungen: die .st-Falle ═════════════════════════════ */

static void t1_suffix(void) {
    printf("Test 1: Endung am Ende pruefen, nicht suchen\n");

    /* Die Falle, in echten Pfaden. */
    static const char *const traps[] = {
        "C:\\Users\\Axel\\Bestand\\abzug.scp",
        "/home/axel/bestand/x.img",
        "D:\\Testdaten\\st-serie\\disk.d64",
        "arbeitsstand.imd",
        NULL
    };

    for (unsigned i = 0; traps[i]; ++i) {
        /* strstr trifft — das ist die Falle. */
        const bool old = (strstr(traps[i], ".st") != NULL) ||
                         (strstr(traps[i], "st") != NULL);
        /* uft_suffix_eq trifft nicht. */
        const bool new_ = uft_suffix_eq(traps[i], ".st");
        CHECK(!new_, "\"%s\" ist keine .st-Datei", traps[i]);
        if (old && !new_)
            printf("    \"%s\": strstr JA, suffix_eq nein\n", traps[i]);
    }

    /* Und was treffen muss, trifft. */
    CHECK(uft_suffix_eq("disk.st", ".st"), "disk.st ist eine .st-Datei");
    CHECK(uft_suffix_eq("DISK.ST", ".st"), "Gross- und Kleinschreibung");
    CHECK(uft_suffix_eq("/pfad/mit punkt.im/x.st", ".st"),
          "auch bei einem Punkt im Verzeichnisnamen");

    /* .st darf NICHT auf .stx treffen — die Verwechslung, die den ganzen
     * Aufwand rechtfertigt. */
    CHECK(!uft_suffix_eq("disk.stx", ".st"), ".stx ist nicht .st");
    CHECK(uft_suffix_eq("disk.stx", ".stx"), ".stx ist .stx");

    /* Ein Pfad, der NUR die Endung ist, ist kein Treffer. */
    CHECK(!uft_suffix_eq(".st", ".st"),
          "\".st\" allein ist ein Name, keine Datei mit Endung");

    /* Schranken. */
    CHECK(!uft_suffix_eq(NULL, ".st"), "NULL");
    CHECK(!uft_suffix_eq("x.st", NULL), "NULL");
    CHECK(!uft_suffix_eq("a", ".st"), "kuerzer als die Endung");
    CHECK(!uft_suffix_eq("x.st", ""), "leere Endung");
}

/* ═══════════ 2. Endungsliste: Reihenfolge zaehlt ════════════════════ */

static void t2_suffix_list(void) {
    printf("Test 2: in einer Endungsliste zaehlt SUFFIX, nicht Praefix\n");

    /* ── EINE KORREKTUR AN MIR SELBST ────────────────────────────────
     *
     * Meine erste Testfassung nahm { ".st", ".stx", ".st0" } als "falsch
     * sortiert" an — weil ".st" ein PRAEFIX von ".stx" ist. Der Test
     * fiel um, und der Code hatte recht: gegen Praefixe ist ein
     * Suffixvergleich von sich aus immun.
     *
     *   uft_suffix_eq("disk.stx", ".st")  ->  false
     *   denn die letzten drei Zeichen sind "stx", nicht ".st"
     *
     * Gefaehrlich ist der umgekehrte Fall: ein Eintrag, der SUFFIX eines
     * anderen ist.
     *
     *   uft_suffix_eq("x.d64", "d64")     ->  true
     *   steht "d64" vor ".d64" in der Liste, gewinnt es immer
     *
     * Genau das prueft uft_suffix_list_ok(), und genau das steht jetzt
     * hier. */

    /* Harmlos: gegenseitige PRAEFIXE sind kein Problem. */
    static const char *const fine[] = { ".st", ".stx", ".st0", NULL };
    CHECK(uft_suffix_list_ok(fine, NULL),
          "gegenseitige Praefixe sind fuer einen Suffixvergleich harmlos");
    CHECK(!uft_suffix_eq("disk.stx", ".st"),
          "und der Beweis: \".st\" trifft \"disk.stx\" nicht");

    /* Gefaehrlich: "d64" ist ein SUFFIX von ".d64". */
    static const char *const bad[] = { "d64", ".d64", NULL };
    size_t bad_at = 999u;
    CHECK(!uft_suffix_list_ok(bad, &bad_at),
          "\"d64\" vor \".d64\" MUSS auffallen");
    CHECK(bad_at == 0u, "und zwar bei Eintrag 0, war %zu", bad_at);
    printf("    { \"d64\", \".d64\" } -> Eintrag %zu steht zu frueh\n",
           bad_at);

    /* Der Schaden, wenn man es nicht prueft. */
    size_t idx = 999u;
    CHECK(uft_suffix_eq_any("abzug.d64", bad, &idx), "trifft");
    CHECK(idx == 0u,
          "die kuerzere Endung gewinnt — \"abzug.d64\" wird als \"d64\" "
          "erkannt, nicht als \".d64\", war %zu", idx);

    /* Richtig sortiert: die laengere zuerst. */
    static const char *const good[] = { ".d64", "d64", NULL };
    CHECK(uft_suffix_list_ok(good, NULL), "so geht die Liste durch");
    CHECK(uft_suffix_eq_any("abzug.d64", good, &idx) && idx == 0u,
          "und die laengere trifft, war %zu", idx);
    printf("    Umgedreht: \".d64\" gewinnt — laengste Endung zuerst\n");

    /* Der Regelfall aus dem Formatvorrat bleibt sauber. */
    static const char *const real[] = {
        ".stx", ".st0", ".st", ".fdi", ".fd", ".img", ".imd", ".im", NULL
    };
    CHECK(uft_suffix_list_ok(real, &bad_at),
          "die echte Liste muss durchgehen, Eintrag %zu stoert", bad_at);
    CHECK(uft_suffix_eq_any("x.stx", real, &idx) && idx == 0u, ".stx");
    CHECK(uft_suffix_eq_any("x.st", real, &idx) && idx == 2u, ".st");
    CHECK(!uft_suffix_eq_any("x.d64", real, &idx),
          "keine der ST-Endungen trifft eine .d64");
}

/* ═══════════ 3. Magic an fester Stelle ══════════════════════════════ */

static void t3_magic(void) {
    printf("Test 3: Magic an der Stelle pruefen, die die Spec nennt\n");

    /* Ein Abbild, das ein SCL-Archiv ENTHAELT — bei Versatz 1024. Genau
     * der Fall, den a2til fuer NuFX beschreibt. */
    uint8_t img[4096];
    memset(img, 0, sizeof(img));
    memcpy(img, "SCP", 3u);                    /* das echte Format */
    memcpy(img + 1024, "SINCLAIR", 8u);        /* eingebettetes Archiv */

    /* Die Falle: suchen findet das Archiv und haelt das Abbild dafuer. */
    size_t where = 0u;
    CHECK(uft_magic_search(img, sizeof(img), "SINCLAIR", 8u,
                           "Koeder", &where),
          "gesucht wird es gefunden");
    CHECK(where == 1024u, "bei 1024, war %zu", where);

    /* Der Ersatz: an Versatz 0 steht kein SINCLAIR. */
    CHECK(!UFT_MAGIC_AT(img, sizeof(img), 0u, "SINCLAIR"),
          "an der richtigen Stelle steht es NICHT — und das ist die "
          "Aussage, die zaehlt");
    CHECK(UFT_MAGIC_AT(img, sizeof(img), 0u, "SCP"),
          "dafuer steht dort SCP");
    printf("    Archiv bei 1024 gefunden, bei 0 nicht — ein Abbild, das "
           "ein\n    Archiv enthaelt, ist kein Archiv\n");

    /* Ueberlaufsicherheit: Versatz nahe SIZE_MAX darf nicht umlaufen. */
    CHECK(!uft_magic_at(img, sizeof(img), (size_t)-4, "SCP", 3u),
          "Versatz nahe SIZE_MAX muss abgewiesen werden, nicht umlaufen");
    CHECK(!uft_magic_at(img, 2u, 0u, "SCP", 3u),
          "Puffer kuerzer als die Kennung");
    CHECK(!uft_magic_at(img, sizeof(img), sizeof(img) - 1u, "SCP", 3u),
          "Kennung reicht ueber das Ende");
}

/* ═══════════ 4. Format-IDs: der Praefixvergleich ════════════════════ */

static void t4_ids(void) {
    printf("Test 4: Format-IDs GANZ vergleichen\n");

    /* Die echten Praefixpaare aus dem Formatvorrat. */
    static const char *const ids[] = {
        "d64", "d67", "d71", "d80", "d81", "d82",
        "g64", "g71", "scp", "scl", "st", "stx", "fd", "fdi",
        "img", "imd", "im", "po", "do", "d13"
    };
    const size_t n = sizeof(ids) / sizeof(ids[0]);

    /* Die Falle: strncmp mit strlen des Suchbegriffs. */
    const char *needle = "st";
    unsigned prefix_hits = 0u;
    for (size_t i = 0; i < n; ++i)
        if (strncmp(ids[i], needle, strlen(needle)) == 0) prefix_hits++;
    CHECK(prefix_hits >= 2u,
          "\"st\" muss mit Praefixvergleich MEHRERE treffen, traf %u",
          prefix_hits);

    /* Der Ersatz: genau einer. */
    unsigned exact_hits = 0u;
    for (size_t i = 0; i < n; ++i)
        if (uft_id_eq(ids[i], needle)) exact_hits++;
    CHECK(exact_hits == 1u, "genau einer erwartet, %u", exact_hits);
    printf("    \"st\": Praefixvergleich trifft %u, uft_id_eq trifft %u\n",
           prefix_hits, exact_hits);

    CHECK(uft_id_find("stx", ids, n) != (size_t)-1, "stx ist drin");
    CHECK(uft_id_find("st0", ids, n) == (size_t)-1, "st0 nicht");
    CHECK(uft_id_eq("SCP", "scp"), "Gross- und Kleinschreibung");
    CHECK(!uft_id_eq("scp", "scp2"), "scp ist nicht scp2");
    CHECK(!uft_id_eq("", "scp"), "leer");
    CHECK(!uft_id_eq(NULL, "scp"), "NULL");
}

/* ═══════════ 5. Die Praefixpaare auflisten ══════════════════════════ */

static unsigned g_pairs = 0u;
static void on_pair(const char *a, const char *b, void *ctx) {
    (void)ctx;
    g_pairs++;
    if (g_pairs <= 6u) printf("      \"%s\" ist Praefix von \"%s\"\n", a, b);
}

static void t5_prefix_pairs(void) {
    printf("Test 5: welche IDs sind Praefixe anderer?\n");

    static const char *const ids[] = {
        "d64", "d67", "d71", "d80", "d81", "d82",
        "g64", "g71", "scp", "scl", "st", "stx", "fd", "fdi",
        "img", "imd", "im", "po", "do", "d13"
    };
    const size_t n = sizeof(ids) / sizeof(ids[0]);

    g_pairs = 0u;
    const size_t found = uft_id_prefix_pairs(ids, n, on_pair, NULL);
    CHECK(found >= 4u,
          "mindestens vier Paare erwartet (st/stx, fd/fdi, im/img, "
          "im/imd), gefunden %zu", found);
    CHECK(found == g_pairs, "Rueckruf und Zaehler muessen uebereinstimmen");
    printf("    %zu Paare — an jeder Stelle, die sie mit einem "
           "Praefixvergleich\n    prueft, ist der Code falsch\n", found);
}

/* ═══════════ 6. Literalvergleich ════════════════════════════════════ */

static void t6_literals(void) {
    printf("Test 6: Laenge aus dem Literal, nicht aus einer Zahl\n");

    const uint8_t d[] = "SINCLAIR\0\0\0";

    /* Die drei Laengenfehler, und dass UFT_LIT_EQ sie nicht machen kann. */
    CHECK(memcmp(d, "SINCLAIR", 4u) == 0,
          "der halbe Vergleich geht durch — das ist die Falle");
    CHECK(UFT_LIT_EQ(d, sizeof(d), 0u, "SINCLAIR"),
          "der ganze auch, aber ohne Zahl im Aufruf");
    CHECK(!UFT_LIT_EQ(d, sizeof(d), 0u, "SINCLAIS"),
          "ein falsches Zeichen muss auffallen");
    CHECK(!UFT_LIT_EQ(d, 4u, 0u, "SINCLAIR"),
          "zu kleiner Puffer muss abgewiesen werden, nicht ueberlesen");

    /* Und der Befund mit Fundstelle. */
    size_t diff = 999u;
    CHECK(!uft_bytes_eq("SINCLAIR", "SINCLAIS", 8u, &diff), "abweichend");
    CHECK(diff == 7u, "ab Byte 7, war %zu", diff);
    CHECK(uft_bytes_eq("SCP", "SCP", 3u, &diff), "gleich");
    printf("    \"Kennung weicht ab Byte %zu ab\" ist eine Aussage, "
           "\"falsch\" ist keine\n", (size_t)7u);
}

/* ═══════════ 7. Endung herausholen ══════════════════════════════════ */

static void t7_ext(void) {
    printf("Test 7: die Endung finden, auch bei Punkten im Pfad\n");

    const char *e;
    e = uft_path_ext("/pfad/zu/disk.scp");
    CHECK(e && strcmp(e, "scp") == 0, "scp, war '%s'", e ? e : "(NULL)");

    e = uft_path_ext("/pfad.mit.punkt/datei");
    CHECK(e == NULL,
          "ein Punkt im VERZEICHNIS ist keine Endung, war '%s'",
          e ? e : "(NULL)");

    e = uft_path_ext("C:\\Bestand\\v1.2\\abzug.imd");
    CHECK(e && strcmp(e, "imd") == 0, "imd trotz Punkt im Pfad, war '%s'",
          e ? e : "(NULL)");

    CHECK(uft_path_ext("/pfad/.versteckt") == NULL,
          "ein fuehrender Punkt ist ein versteckter Name");
    CHECK(uft_path_ext("ohnepunkt") == NULL, "keine Endung");
    CHECK(uft_path_ext("endetmitpunkt.") == NULL, "leere Endung");
    CHECK(uft_path_ext(NULL) == NULL, "NULL");
    printf("    \"/pfad.mit.punkt/datei\" hat keine Endung — der letzte "
           "Punkt\n    allein genuegt nicht\n");
}

int main(void) {
    printf("=== test_match ===\n\n");
    t1_suffix();
    t2_suffix_list();
    t3_magic();
    t4_ids();
    t5_prefix_pairs();
    t6_literals();
    t7_ext();
    printf("\n%s (%d Fehler)\n", g_fail ? "FEHLGESCHLAGEN" : "BESTANDEN",
           g_fail);
    return g_fail ? 1 : 0;
}
