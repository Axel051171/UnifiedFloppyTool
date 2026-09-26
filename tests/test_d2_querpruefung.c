/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_d2_querpruefung.c
 * @brief Spurvergleich im Zentrum: die Klammerregel (Stufe 1 der Tuer).
 *
 * `uft_st_order_messen()` (H-18, MF-1339) kennt die Sektorzahl EINER Spur,
 * nicht die der Diskette: fehlt der LETZTE Sektor ganz, sieht man es der
 * Spur nicht an. Sichtbar wird er erst im Vergleich mit den Nachbarn —
 * eine Ebene hoeher, im Modell. Hier wird festgehalten, was
 * `uft_d2_querpruefung()` dazu sagen darf und was nicht:
 *
 *   A  79 Spuren {1..10}, eine {1..9}          -> genau EIN WARN, „fehlt: 10"
 *   B  zwei benachbarte beschaedigte Spuren     -> ZWEI WARN (die Nachbarregel
 *                                                  des Entwurfs sah hier nichts:
 *                                                  jede Spur hat einen Nachbarn
 *                                                  mit der kleineren Menge)
 *   C  Zonenformate C64 1541/1571, Apple 3,5",
 *      Victor 9000                              -> 0 Befunde
 *   D  Obermenge {1..11}                        -> EIN NOTE, kein Fehler
 *   E  Rand: letzter Zylinder mit Gegenkopf     -> EIN NOTE (nennt die
 *                                                  Bootspur); ohne Gegenkopf 0
 *   F  weder Teil- noch Obermenge, fehlende
 *      Spur in der Klammer                      -> 0 (kein Befund ohne Beleg)
 *   G  zweiter Aufruf                           -> 0 neue Befunde
 *   H  die Pruefung aendert nichts              -> Spurzahl, Sektorzahlen,
 *                                                  Schichten, Merkmale gleich
 *   J  TRS-80 DS DD, SD-Bootspur nur auf C0 H0  -> 0 WARN, 1 NOTE (Waechter)
 *   K  60 Spuren mit derselben Luecke           -> EIN WARN „C10..C69 H0: …"
 *   I  Bruecke: read_track scheitert            -> NOTE TRACK_UNREADABLE je
 *                                                  Lauf (rc woertlich, keine
 *                                                  Behauptung darueber hinaus),
 *                                                  und die Querpruefung macht
 *                                                  daraus KEINE Luecke
 *
 * Die Zonentafeln stehen hier ein ZWEITES Mal und werden nicht aus den
 * Plugins geholt — ein Test, der dieselbe Quelle befragt wie der Pruefling,
 * prueft nichts (Klasse MF-1000):
 *   C64 1541: 17x21, 7x19, 6x18, 5x17 (Sektoren ab 0).
 *   Apple 3,5" GCR: je 16 Spuren 12, 11, 10, 9, 8 (MAME ap_dsk35.cpp,
 *     `ns = 12 - Spur/16`, siehe MF-1031).
 *   Victor 9000: MAME victor9k_dsk.cpp:392-414, je Kopf eine eigene Tafel
 *     (MF-1026); Kopf 1 ist gegen Kopf 0 um eine Zone versetzt — genau das
 *     ist der Fall, an dem eine Randregel mit Gegenkopf-Zeugen ueber mehr
 *     als eine Spur falsch anschlaegt (Kopf 1, Zylinder 75..79).
 */
#include "uft/core/uft_disk2.h"
#include "uft/core/uft_disk2_bridge.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0, g_pass = 0;
#define CHECK(c, ...) do { if (c) { g_pass++; } else { g_fail++;              \
    printf("    FAIL @%d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } \
    } while (0)

/* ── Bau eines Modells ─────────────────────────────────────────────────── */

/** Legt Spur (c,h) mit den Sektornummern ids[0..n) an. n == 0: keine Spur. */
static void spur(uft_disk2_t *d, unsigned c, unsigned h,
                 const uint8_t *ids, size_t n) {
    if (!n) return;
    uft_d2_track_t *t = uft_d2_track(d, (uint16_t)c, (uint8_t)h);
    for (size_t i = 0; i < n; ++i) {
        uft_d2_sector_t s;
        memset(&s, 0, sizeof s);
        s.id_cyl = (uint8_t)c; s.id_head = (uint8_t)h; s.id_sec = ids[i];
        s.id_size_code = 2;
        s.idam_bit = s.dam_bit = s.data_end_bit = SIZE_MAX;
        s.origin = UFT_D2_ORIGIN_CONTAINER;
        s.conf = UFT_D2_CONF_UNVERIFIED;
        if (!uft_d2_add_sector(d, t, &s)) { printf("add_sector\n"); exit(2); }
    }
}

/** Spur mit den Nummern erste..erste+n-1. */
static void spur_folge(uft_disk2_t *d, unsigned c, unsigned h,
                       unsigned erste, unsigned n) {
    uint8_t ids[256];
    for (unsigned i = 0; i < n; ++i) ids[i] = (uint8_t)(erste + i);
    spur(d, c, h, ids, n);
}

static size_t zaehle(const uft_disk2_t *d, const char *code,
                     uft_d2_diag_sev_t sev) {
    size_t n = 0;
    for (size_t i = 0; i < uft_d2_diag_count(d); ++i) {
        const uft_d2_diag_t *g = uft_d2_diag_at(d, i);
        if (strcmp(g->code, code) == 0 && g->sev == sev) n++;
    }
    return n;
}

static const uft_d2_diag_t *finde(const uft_disk2_t *d, const char *code,
                                  int c, int h) {
    for (size_t i = 0; i < uft_d2_diag_count(d); ++i) {
        const uft_d2_diag_t *g = uft_d2_diag_at(d, i);
        if (strcmp(g->code, code) == 0 && g->cyl == c && g->head == h) return g;
    }
    return NULL;
}

static void befunde_zeigen(const uft_disk2_t *d) {
    for (size_t i = 0; i < uft_d2_diag_count(d); ++i) {
        const uft_d2_diag_t *g = uft_d2_diag_at(d, i);
        printf("      [%d] %s C%d H%d: %s\n", (int)g->sev, g->code,
               (int)g->cyl, (int)g->head, g->text);
    }
}

/* Die Pruefung darf das Modell nicht veraendern — gemessen, nicht
 * behauptet: vorher und nachher dieselben Zahlen. */
typedef struct {
    size_t spuren, sektoren;
    uint32_t schichten, merkmale;
} stand_t;

static stand_t stand(const uft_disk2_t *d) {
    stand_t s;
    memset(&s, 0, sizeof s);
    s.spuren = uft_d2_track_count(d);
    for (size_t i = 0; i < s.spuren; ++i)
        s.sektoren += uft_d2_track_at(d, i)->sectors.count;
    s.schichten = uft_d2_layers(d);
    s.merkmale = uft_d2_features(d);
    return s;
}

static size_t pruefen(uft_disk2_t *d) {
    const stand_t a = stand(d);
    const size_t n = uft_d2_querpruefung(d);
    const stand_t b = stand(d);
    CHECK(a.spuren == b.spuren && a.sektoren == b.sektoren
          && a.schichten == b.schichten && a.merkmale == b.merkmale,
          "die Pruefung hat das Modell veraendert: Spuren %zu->%zu, "
          "Sektoren %zu->%zu, Schichten 0x%x->0x%x, Merkmale 0x%x->0x%x",
          a.spuren, b.spuren, a.sektoren, b.sektoren, a.schichten,
          b.schichten, a.merkmale, b.merkmale);
    /* Zweiter Aufruf: dieselben Befunde gibt es schon. */
    const size_t vorher = uft_d2_diag_count(d);
    const size_t n2 = uft_d2_querpruefung(d);
    CHECK(n2 == 0u && uft_d2_diag_count(d) == vorher,
          "zweiter Aufruf fuegte %zu Befunde hinzu", n2);
    return n;
}

/* ── A ─────────────────────────────────────────────────────────────────── */

static void a_eine_spur_mit_neun(void) {
    printf("A: 79 Spuren {1..10}, Zylinder 40 {1..9}\n");
    uft_disk2_t *d = uft_d2_create();
    for (unsigned c = 0; c < 80; ++c) spur_folge(d, c, 0, 1, c == 40 ? 9 : 10);
    const size_t n = pruefen(d);
    CHECK(n == 1u, "genau ein neuer Befund erwartet, %zu", n);
    CHECK(zaehle(d, "SEC_GAP_VS_BRACKET", UFT_D2_DIAG_WARN) == 1u,
          "ein WARN SEC_GAP_VS_BRACKET");
    const uft_d2_diag_t *g = finde(d, "SEC_GAP_VS_BRACKET", 40, 0);
    CHECK(g != NULL, "der Befund nennt C40 H0");
    if (g) {
        CHECK(strstr(g->text, "traegt 9") && strstr(g->text, "fehlt: 10"),
              "Text nennt 9 gegen 10 und die fehlende Nummer: %s", g->text);
        CHECK(g->sector == -1, "ganze Spur (sector -1), hat %d", (int)g->sector);
        CHECK(g->layer == UFT_D2_LAYER_SECTORS, "Schicht Sektoren");
    }
    befunde_zeigen(d);
    uft_d2_destroy(d);
}

/* ── B ─────────────────────────────────────────────────────────────────── */

static void b_zwei_benachbarte(void) {
    printf("B: zwei benachbarte beschaedigte Spuren (C40 {1..9}, C41 {1..8})\n");
    uft_disk2_t *d = uft_d2_create();
    for (unsigned c = 0; c < 80; ++c)
        spur_folge(d, c, 0, 1, c == 40 ? 9 : (c == 41 ? 8 : 10));
    const size_t n = pruefen(d);
    CHECK(n == 2u, "zwei Befunde erwartet, %zu", n);
    const uft_d2_diag_t *g40 = finde(d, "SEC_GAP_VS_BRACKET", 40, 0);
    const uft_d2_diag_t *g41 = finde(d, "SEC_GAP_VS_BRACKET", 41, 0);
    CHECK(g40 && strstr(g40->text, "fehlt: 10"), "C40 fehlt 10");
    CHECK(g41 && strstr(g41->text, "fehlt: 9, 10"), "C41 fehlt 9, 10: %s",
          g41 ? g41->text : "(kein Befund)");
    befunde_zeigen(d);
    uft_d2_destroy(d);
}

/* ── C ─────────────────────────────────────────────────────────────────── */

static unsigned c64_spt(unsigned c) {
    return c < 17 ? 21u : c < 24 ? 19u : c < 30 ? 18u : 17u;
}

static const uint8_t VICTOR[2][80] = {
    { 19,19,19,19,
      18,18,18,18,18,18,18,18,18,18,18,18,
      17,17,17,17,17,17,17,17,17,17,17,
      16,16,16,16,16,16,16,16,16,16,16,
      15,15,15,15,15,15,15,15,15,15,
      14,14,14,14,14,14,14,14,14,14,14,14,
      13,13,13,13,13,13,13,13,13,13,13,
      12,12,12,12,12,12,12,12,12 },
    { 18,18,18,18,18,18,18,18,
      17,17,17,17,17,17,17,17,17,17,17,
      16,16,16,16,16,16,16,16,16,16,16,
      15,15,15,15,15,15,15,15,15,15,
      14,14,14,14,14,14,14,14,14,14,14,14,
      13,13,13,13,13,13,13,13,13,13,13,
      12,12,12,12,12,12,12,12,12,12,12,12,
      11,11,11,11,11 }
};

static void c_zonenformate(void) {
    printf("C: Zonenformate bleiben stumm\n");

    uft_disk2_t *d = uft_d2_create();
    for (unsigned c = 0; c < 35; ++c) spur_folge(d, c, 0, 0, c64_spt(c));
    size_t n = pruefen(d);
    CHECK(n == 0u, "C64 1541 (35 Spuren, 4 Zonen): %zu Befunde", n);
    if (n) befunde_zeigen(d);
    uft_d2_destroy(d);

    d = uft_d2_create();
    for (unsigned c = 0; c < 35; ++c)
        for (unsigned h = 0; h < 2; ++h) spur_folge(d, c, h, 0, c64_spt(c));
    n = pruefen(d);
    CHECK(n == 0u, "C64 1571 (zweiseitig, dieselben Zonen): %zu Befunde", n);
    if (n) befunde_zeigen(d);
    uft_d2_destroy(d);

    d = uft_d2_create();
    for (unsigned c = 0; c < 80; ++c)
        for (unsigned h = 0; h < 2; ++h) spur_folge(d, c, h, 0, 12u - c / 16u);
    n = pruefen(d);
    CHECK(n == 0u, "Apple 3,5\" 800K (5 Zonen zu 16 Spuren): %zu Befunde", n);
    if (n) befunde_zeigen(d);
    uft_d2_destroy(d);

    d = uft_d2_create();
    unsigned summe[2] = { 0, 0 };
    for (unsigned c = 0; c < 80; ++c)
        for (unsigned h = 0; h < 2; ++h) {
            spur_folge(d, c, h, 0, VICTOR[h][c]);
            summe[h] += VICTOR[h][c];
        }
    CHECK(summe[0] == 1224u && summe[1] == 1167u,
          "Victor-Tafel abgeschrieben: %u/%u statt 1224/1167", summe[0], summe[1]);
    n = pruefen(d);
    CHECK(n == 0u, "Victor 9000 (zwei versetzte Tafeln): %zu Befunde", n);
    if (n) befunde_zeigen(d);
    uft_d2_destroy(d);
}

/* ── D ─────────────────────────────────────────────────────────────────── */

static void d_obermenge(void) {
    printf("D: Obermenge {1..11} zwischen {1..10}\n");
    uft_disk2_t *d = uft_d2_create();
    for (unsigned c = 0; c < 80; ++c) spur_folge(d, c, 0, 1, c == 40 ? 11 : 10);
    const size_t n = pruefen(d);
    CHECK(n == 1u, "ein Befund, %zu", n);
    CHECK(zaehle(d, "SEC_EXTRA_VS_BRACKET", UFT_D2_DIAG_NOTE) == 1u,
          "ein NOTE SEC_EXTRA_VS_BRACKET (moeglicher Schutz, kein Fehler)");
    CHECK(uft_d2_diag_count_sev(d, UFT_D2_DIAG_WARN) == 0u, "kein WARN");
    const uft_d2_diag_t *g = finde(d, "SEC_EXTRA_VS_BRACKET", 40, 0);
    CHECK(g && strstr(g->text, "zusaetzlich: 11"), "nennt die 11: %s",
          g ? g->text : "(kein Befund)");
    /* The severity alone is not the message: the text must say it may be
     * protection (reviewer mutation X7 dropped the hint and stayed green). */
    CHECK(g && strstr(g->text, "moeglicher Schutz"),
          "nennt den Schutz-Hinweis: %s", g ? g->text : "(kein Befund)");
    befunde_zeigen(d);
    uft_d2_destroy(d);
}

/* ── E ─────────────────────────────────────────────────────────────────── */

static void e_rand(void) {
    printf("E: Rand — letzter Zylinder, mit und ohne Gegenkopf\n");

    /* Zweiseitig: C79 H0 hat 9, C78 H0 und C79 H1 haben 10. Am Rand gibt
     * es nur EINE Klammer; der Befund ist deshalb ein NOTE und nennt die
     * Bootspur als zweite Lesart (siehe G, TRS-80). */
    uft_disk2_t *d = uft_d2_create();
    for (unsigned c = 0; c < 80; ++c)
        for (unsigned h = 0; h < 2; ++h)
            spur_folge(d, c, h, 1, (c == 79 && h == 0) ? 9 : 10);
    size_t n = pruefen(d);
    CHECK(n == 1u, "ein Befund am Rand, %zu", n);
    const uft_d2_diag_t *g = finde(d, "SEC_GAP_VS_BRACKET", 79, 0);
    CHECK(g && strstr(g->text, "fehlt: 10") && strstr(g->text, "H1"),
          "C79 H0 mit Gegenkopf H1 als zweitem Zeugen: %s",
          g ? g->text : "(kein Befund)");
    CHECK(g && g->sev == UFT_D2_DIAG_NOTE,
          "Randbefund ist NOTE, nicht WARN (sev %d)", g ? (int)g->sev : -1);
    CHECK(g && strstr(g->text, "Bootspur"),
          "Randbefund nennt die Bootspur-Lesart: %s", g ? g->text : "(kein Befund)");
    CHECK(uft_d2_diag_count_sev(d, UFT_D2_DIAG_WARN) == 0u,
          "am Rand kein WARN: %zu", uft_d2_diag_count_sev(d, UFT_D2_DIAG_WARN));
    befunde_zeigen(d);
    uft_d2_destroy(d);

    /* Zylinder 0, ebenfalls mit Gegenkopf. */
    d = uft_d2_create();
    for (unsigned c = 0; c < 80; ++c)
        for (unsigned h = 0; h < 2; ++h)
            spur_folge(d, c, h, 1, (c == 0 && h == 1) ? 9 : 10);
    n = pruefen(d);
    CHECK(n == 1u && finde(d, "SEC_GAP_VS_BRACKET", 0, 1) != NULL,
          "C0 H1 mit Gegenkopf H0: %zu Befunde", n);
    uft_d2_destroy(d);

    /* Einseitig: nur EIN Zeuge — kein Befund. */
    d = uft_d2_create();
    for (unsigned c = 0; c < 80; ++c) spur_folge(d, c, 0, 1, c == 79 ? 9 : 10);
    n = pruefen(d);
    CHECK(n == 0u, "einseitig, nur ein Zeuge: %zu Befunde", n);
    uft_d2_destroy(d);

    /* Beide Koepfe am Rand beschaedigt: der Gegenkopf ist kein Zeuge. */
    d = uft_d2_create();
    for (unsigned c = 0; c < 80; ++c)
        for (unsigned h = 0; h < 2; ++h) spur_folge(d, c, h, 1, c == 79 ? 9 : 10);
    n = pruefen(d);
    CHECK(n == 0u, "beide Koepfe am Rand mit 9: %zu Befunde", n);
    uft_d2_destroy(d);
}

/* ── F ─────────────────────────────────────────────────────────────────── */

static void f_ohne_beleg(void) {
    printf("F: kein Befund ohne Beleg\n");

    /* {1..9, 11} gegen {1..10}: weder Teil- noch Obermenge. */
    uft_disk2_t *d = uft_d2_create();
    const uint8_t schief[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 11 };
    for (unsigned c = 0; c < 80; ++c) {
        if (c == 40) spur(d, c, 0, schief, 10);
        else spur_folge(d, c, 0, 1, 10);
    }
    size_t n = pruefen(d);
    CHECK(n == 0u, "unvergleichbare Menge: %zu Befunde", n);
    uft_d2_destroy(d);

    /* Zylinder 40 fehlt, 41 hat 9: die linke Klammer fehlt. */
    d = uft_d2_create();
    for (unsigned c = 0; c < 80; ++c) {
        if (c == 40) continue;
        spur_folge(d, c, 0, 1, c == 41 ? 9 : 10);
    }
    n = pruefen(d);
    CHECK(n == 0u, "fehlende Spur neben der Abweichung: %zu Befunde", n);
    CHECK(uft_d2_diag_count(d) == 0u,
          "eine fehlende Spur ist KEIN Befund dieser Pruefung (%zu)",
          uft_d2_diag_count(d));
    uft_d2_destroy(d);

    /* Klammer links UND rechts vorhanden, dazwischen eine abweichende und
     * eine FEHLENDE Spur: die fehlende unterbricht den Lauf. Wer ueber sie
     * hinwegklammert, meldet sie als „traegt 0; fehlt: 1..10" — eine
     * Spurluecke, die niemand gemessen hat (gefunden von der
     * Mutationsmatrix, M7). */
    d = uft_d2_create();
    for (unsigned c = 0; c < 80; ++c) {
        if (c == 41) continue;
        spur_folge(d, c, 0, 1, c == 40 ? 9 : 10);
    }
    n = pruefen(d);
    CHECK(n == 0u, "Lauf ueber eine fehlende Spur: %zu Befunde", n);
    if (n) befunde_zeigen(d);
    uft_d2_destroy(d);
}

/* ── J: Bootspur auf einer Seite (Waechter) ─────────────────────────────── */

/* TRS-80 Model I, DD, zweiseitig: Spur 0 auf Seite 0 ist in einfacher
 * Dichte geschrieben (Sektoren 0..9), alle anderen Spuren tragen 0..17.
 * Die Diskette ist korrekt formatiert. Die Randregel sieht C0 H0 mit
 * einer Klammer (C1 H0) und dem Gegenkopf (C0 H1) als Zeugen — genau das
 * Bild einer fehlenden Haelfte. Gemessen an der Fassung davor: 1 WARN
 * „fehlt: 10, 11, …, 17" an einer richtigen Diskette (Review Tuer 1+3).
 * Verlangt: KEIN WARN, EIN NOTE, und das NOTE nennt die Bootspur. */
static void j_trs80_bootspur(void) {
    printf("J: TRS-80 DS DD mit SD-Bootspur (C0 H0 0..9, sonst 0..17)\n");
    uft_disk2_t *d = uft_d2_create();
    for (unsigned c = 0; c < 40; ++c)
        for (unsigned h = 0; h < 2; ++h)
            spur_folge(d, c, h, 0, (c == 0 && h == 0) ? 10 : 18);
    const size_t n = pruefen(d);
    CHECK(uft_d2_diag_count_sev(d, UFT_D2_DIAG_WARN) == 0u,
          "richtig formatierte Diskette: %zu WARN",
          uft_d2_diag_count_sev(d, UFT_D2_DIAG_WARN));
    CHECK(n == 1u && zaehle(d, "SEC_GAP_VS_BRACKET", UFT_D2_DIAG_NOTE) == 1u,
          "genau ein NOTE, %zu Befunde", n);
    const uft_d2_diag_t *g = finde(d, "SEC_GAP_VS_BRACKET", 0, 0);
    CHECK(g && strstr(g->text, "Bootspur")
          && strstr(g->text, "fehlt: 10, 11, 12, 13, 14, 15, 16, 17"),
          "C0 H0 nennt die Bootspur und die volle Liste: %s",
          g ? g->text : "(kein Befund)");
    befunde_zeigen(d);
    uft_d2_destroy(d);
}

/* ── K: ein langer Lauf ist EIN Befund ──────────────────────────────────── */

/* C0..9 mit 10, C10..69 mit 9, C70..79 mit 10. Nach der Klammerregel ist
 * jede der 60 Spuren eine Luecke — die Fassung davor schrieb 60 WARN
 * (gemessen, Review Tuer 1+3) und rueckte die Befundliste an ihre Grenze
 * (UFT_D2_MAX_DIAG). Es ist EIN Sachverhalt: ein Befund je Gruppe
 * gleicher Mengen desselben Kopfes. */
static void k_langer_lauf(void) {
    printf("K: C10..C69 mit 9 zwischen Klammern mit 10\n");
    uft_disk2_t *d = uft_d2_create();
    for (unsigned c = 0; c < 80; ++c)
        spur_folge(d, c, 0, 1, (c >= 10 && c <= 69) ? 9 : 10);
    const size_t n = pruefen(d);
    CHECK(n == 1u, "ein Befund fuer den ganzen Lauf, %zu", n);
    CHECK(zaehle(d, "SEC_GAP_VS_BRACKET", UFT_D2_DIAG_WARN) == 1u,
          "genau ein WARN");
    const uft_d2_diag_t *g = finde(d, "SEC_GAP_VS_BRACKET", -1, -1);
    CHECK(g && strcmp(g->text, "C10..C69 H0: tragen 9, Klammer 10 (C9/C70); "
                               "fehlt: 10") == 0,
          "Text nennt Bereich, Kopf, Zahlen und Klammern: %s",
          g ? g->text : "(kein Befund)");
    befunde_zeigen(d);
    uft_d2_destroy(d);

    /* Zwei Gruppen im selben Lauf: C40..41 {1..9}, C42..43 {1..8}. Je
     * Gruppe EIN Befund, keine Zusammenlegung ungleicher Mengen. */
    d = uft_d2_create();
    for (unsigned c = 0; c < 80; ++c)
        spur_folge(d, c, 0, 1, (c == 40 || c == 41) ? 9
                              : (c == 42 || c == 43) ? 8 : 10);
    const size_t n2 = pruefen(d);
    CHECK(n2 == 2u, "zwei Gruppen, zwei Befunde: %zu", n2);
    int a = 0, b = 0;
    for (size_t i = 0; i < uft_d2_diag_count(d); ++i) {
        const char *t = uft_d2_diag_at(d, i)->text;
        if (strcmp(t, "C40..C41 H0: tragen 9, Klammer 10 (C39/C44); "
                      "fehlt: 10") == 0) a++;
        if (strcmp(t, "C42..C43 H0: tragen 8, Klammer 10 (C39/C44); "
                      "fehlt: 9, 10") == 0) b++;
    }
    CHECK(a == 1 && b == 1, "je Gruppe genau ein Befund (%d/%d)", a, b);
    befunde_zeigen(d);
    uft_d2_destroy(d);
}

/* ── I: Bruecke ─────────────────────────────────────────────────────────── */

/* Welche Spuren das Ersatz-Plugin scheitern laesst, und mit welchem
 * Rueckgabewert: g_fehl[c][h] != UFT_OK. */
static uft_error_t g_fehl[7][2];

static uft_error_t fake_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *t) {
    (void)disk;
    memset(t, 0, sizeof *t);
    t->cylinder = cyl; t->head = head;
    if (cyl >= 0 && cyl < 7 && head >= 0 && head < 2
        && g_fehl[cyl][head] != UFT_OK)
        return g_fehl[cyl][head];           /* scheitert */
    const unsigned n = 10;
    t->sectors = calloc(n, sizeof(uft_sector_t));
    t->sector_count = n;
    for (unsigned i = 0; i < n; i++) {
        uft_sector_t *s = &t->sectors[i];
        s->id.cylinder = (uint8_t)cyl; s->id.head = (uint8_t)head;
        s->id.sector = (uint8_t)(i + 1); s->id.size_code = 2;
        s->data = malloc(512); memset(s->data, 0xE5, 512);
        s->data_len = 512; s->data_size = 512; s->data_mark = 0xFB;
    }
    return UFT_OK;
}

/* Setzt die Zylinder von..bis auf den Koepfen `maske` auf `rc`. */
static void fehl(int von, int bis, unsigned maske, uft_error_t rc) {
    for (int c = von; c <= bis; ++c)
        for (int h = 0; h < 2; ++h)
            if (maske & (1u << h)) g_fehl[c][h] = rc;
}

/* Ein Lauf der Bruecke ueber das Ersatz-Plugin (7 Zylinder, `koepfe`
 * Koepfe, Fehlschlaege aus g_fehl). Setzt g_fehl danach zurueck. */
static uft_disk2_t *bruecke_mit(unsigned koepfe, size_t *gescheitert) {
    uft_format_plugin_t fake;
    memset(&fake, 0, sizeof fake);
    fake.name = "fake";
    fake.read_track = fake_read_track;
    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    disk.geometry.cylinders = 7;
    disk.geometry.heads = koepfe;

    uft_disk2_t *d = uft_d2_create();
    uft_d2_bridge_stats_t st;
    CHECK(uft_d2_from_disk(d, &disk, &fake, &st), "Bruecke");
    *gescheitert = st.tracks_failed;
    memset(g_fehl, 0, sizeof g_fehl);
    return d;
}

static void i_bruecke(void) {
    printf("I: Bruecke — Spur 3 scheitert\n");
    size_t f = 0;
    fehl(3, 3, 1u, UFT_ERROR_IO);
    uft_disk2_t *d = bruecke_mit(1u, &f);
    CHECK(f == 1u, "eine Spur gescheitert: %zu", f);
    const uft_d2_diag_t *g = finde(d, "TRACK_UNREADABLE", 3, 0);
    /* NOTE, nicht WARN: der Rueckgabewert sagt nicht, OB die Spur
     * unlesbar ist — an D88/D77 heisst derselbe Fehler „im Behaelter
     * unformatiert" (gemessen: tests/test_d2_bruecke_am_korpus.c). */
    CHECK(g != NULL && g->sev == UFT_D2_DIAG_NOTE,
          "Befund TRACK_UNREADABLE NOTE mit C3 H0 (die Summe TRACKS_FAILED "
          "allein nennt die Spur nicht)");
    CHECK(g && strstr(g->text, "rc=") && !strstr(g->text, "nicht leer"),
          "Text nennt rc und behauptet nichts darueber hinaus: %s",
          g ? g->text : "(kein Befund)");
    CHECK(zaehle(d, "TRACKS_FAILED", UFT_D2_DIAG_WARN) == 1u,
          "die Summe bleibt daneben stehen");
    const size_t n = pruefen(d);
    CHECK(n == 0u, "die Querpruefung macht aus der gescheiterten Spur keine "
          "Luecke: %zu Befunde", n);
    befunde_zeigen(d);
    uft_d2_destroy(d);

    printf("I2: Bruecke — Zylinder 3..5 scheitern, einseitig\n");
    fehl(3, 5, 1u, UFT_ERROR_IO);
    d = bruecke_mit(1u, &f);
    CHECK(f == 3u, "drei Spuren gescheitert: %zu", f);
    CHECK(zaehle(d, "TRACK_UNREADABLE", UFT_D2_DIAG_NOTE) == 1u
          && zaehle(d, "TRACK_UNREADABLE", UFT_D2_DIAG_WARN) == 0u,
          "EIN NOTE fuer den Lauf, nicht drei");
    g = finde(d, "TRACK_UNREADABLE", -1, -1);
    static const char kopf[] = "C3..C5 H0: read_track von \"fake\" lieferte rc=";
    CHECK(g && strncmp(g->text, kopf, sizeof kopf - 1u) == 0,
          "Bereich im Text: %s", g ? g->text : "(kein Befund)");
    befunde_zeigen(d);
    uft_d2_destroy(d);

    printf("I3: Bruecke — Zylinder 3..5 scheitern auf beiden Koepfen\n");
    fehl(3, 5, 3u, UFT_ERROR_IO);
    d = bruecke_mit(2u, &f);
    CHECK(f == 6u, "sechs Spuren gescheitert: %zu", f);
    CHECK(zaehle(d, "TRACK_UNREADABLE", UFT_D2_DIAG_NOTE) == 1u,
          "EIN NOTE fuer beide Koepfe");
    g = finde(d, "TRACK_UNREADABLE", -1, -1);
    CHECK(g && strncmp(g->text, "C3..C5 H0/H1: ", 14) == 0,
          "beide Koepfe im Text: %s", g ? g->text : "(kein Befund)");
    uft_d2_destroy(d);

    /* Ein Lauf endet, wo sich die Kopfmenge ODER der Rueckgabewert
     * aendert: C2 H0 allein, C3..C4 H0/H1, C5..C6 H0/H1 mit anderem rc
     * — drei Laeufe, drei Befunde. */
    printf("I4: Bruecke — Kopfmenge und Rueckgabewert trennen Laeufe\n");
    fehl(2, 2, 1u, UFT_ERROR_IO);
    fehl(3, 4, 3u, UFT_ERROR_IO);
    fehl(5, 6, 3u, UFT_ERROR_INVALID_ARG);
    d = bruecke_mit(2u, &f);
    CHECK(f == 9u, "neun Spuren gescheitert: %zu", f);
    CHECK(zaehle(d, "TRACK_UNREADABLE", UFT_D2_DIAG_NOTE) == 3u,
          "drei Laeufe, drei NOTE: %zu",
          zaehle(d, "TRACK_UNREADABLE", UFT_D2_DIAG_NOTE));
    CHECK(finde(d, "TRACK_UNREADABLE", 2, 0) != NULL,
          "C2 H0 allein steht in den Feldern");
    int c34 = 0, c56 = 0;
    for (size_t i = 0; i < uft_d2_diag_count(d); ++i) {
        const char *t = uft_d2_diag_at(d, i)->text;
        if (strncmp(t, "C3..C4 H0/H1: ", 14) == 0) c34++;
        if (strncmp(t, "C5..C6 H0/H1: ", 14) == 0) c56++;
    }
    CHECK(c34 == 1 && c56 == 1, "C3..C4 und C5..C6 getrennt (%d/%d)", c34, c56);
    befunde_zeigen(d);
    uft_d2_destroy(d);

    /* Both heads of ONE cylinder fail with DIFFERENT return codes: the
     * finding must name both, not the first one for both heads (reviewer
     * mutation X4, `if (rc_gleich)` -> `if (1)`, stayed green). */
    printf("I5: Bruecke — zwei Koepfe, zwei Rueckgabewerte\n");
    fehl(3, 3, 1u, UFT_ERROR_IO);
    fehl(3, 3, 2u, UFT_ERROR_INVALID_ARG);
    d = bruecke_mit(2u, &f);
    CHECK(f == 2u, "zwei Spuren gescheitert: %zu", f);
    char erwartet[32];
    snprintf(erwartet, sizeof erwartet, "rc=%d/%d",
             (int)UFT_ERROR_IO, (int)UFT_ERROR_INVALID_ARG);
    g = finde(d, "TRACK_UNREADABLE", -1, -1);
    CHECK(g && strstr(g->text, erwartet),
          "beide Rueckgabewerte (%s): %s", erwartet,
          g ? g->text : "(kein Befund)");
    befunde_zeigen(d);
    uft_d2_destroy(d);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== test_d2_querpruefung ===\n\n");
    a_eine_spur_mit_neun();
    b_zwei_benachbarte();
    c_zonenformate();
    d_obermenge();
    e_rand();
    f_ohne_beleg();
    j_trs80_bootspur();
    k_langer_lauf();
    i_bruecke();
    printf("\n%d bestanden, %d fehlgeschlagen\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
