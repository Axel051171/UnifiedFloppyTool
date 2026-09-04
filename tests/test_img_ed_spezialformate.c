/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_img_ed_spezialformate.c
 * @brief ED-Spezialformate 41-44 SpT — und wer sie vorher beanspruchte
 *        (MF-872)
 *
 * -- Der Befund --------------------------------------------------------
 *
 * `known_geometries[]` in `src/formats/img/uft_img.c` endete bei 36
 * Sektoren je Spur (2.88 MB). Die ED-Spezialformate mit 41 bis 44 SpT
 * fehlten.
 *
 * Das war nicht bloss eine Luecke. Gemessen ueber den ECHTEN
 * Erkennungsweg — alle 137 registrierten Sonden gegen einen Puffer mit
 * gueltigem PC-Bootsektor — beanspruchte diese vier Groessen **allein
 * `DMK`, mit Konfidenz 55**: ein TRS-80-Format fuer ein PC-Abbild, und
 * 55 liegt im Band „Struktur gelesen" (MF-729).
 *
 * -- Die Referenz ------------------------------------------------------
 *
 * Zwei unabhaengige Quellen, keine davon der eigene Baum:
 *
 *   FLOPFIX `version.txt:40`
 *     „Sind die Spezialformate aktiviert, so sind nun auch die
 *      ED-Formate mit 41, 42, 43 und 44 Sektoren zugaenglich."
 *
 *   FreeDOS FORMAT 0.92 (GPL-2-only — es wurden ausschliesslich Zahlen
 *   entnommen, kein Code) fuehrt mit FD3360/FD3486 bis 42 SpT und deckt
 *   damit die untere Haelfte unabhaengig ab.
 *
 * -- Warum die Zahlen abgeleitet sind ----------------------------------
 *
 * Keine der vier ist abgeschrieben. SpT x 2 Seiten x 80 Spuren x 512:
 *
 *     36 SpT -> 2949120   (die BESTEHENDE Zeile)
 *     41 SpT -> 3358720
 *     42 SpT -> 3440640
 *     43 SpT -> 3522560
 *     44 SpT -> 3604480
 *
 * Dass dieselbe Rechnung die bestehende, funktionierende Zeile
 * reproduziert, ist die Begruendung fuer die vier neuen.
 *
 * -- Was vor dem Eintragen gemessen wurde ------------------------------
 *
 * MF-784 hat zweimal gezeigt, dass Groessengleichheit keine
 * Geometriegleichheit ist (`v9t9` und `cpm` gegen gw-Formate). Deshalb
 * zuerst: keine der vier Zahlen kommt sonst irgendwo im Baum vor, und
 * der generische Rueckfall in `img_detect_geometry()` faengt sie nicht
 * — `sectors_options[] = {18,9,15,36,21,8,10}` kennt 41-44 nicht, und
 * bei 8 oder 10 SpT lieferte er Zylinderzahlen weit ueber der Grenze 84.
 */
#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

uft_error_t uft_register_all_formats(void);

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-44s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                       _fail++; return; } } while (0)

#define SEITEN     2
#define SPUREN    80
#define SEKTORGR 512

/* Die ED-Reihe. 36 ist die bestehende Zeile und steht als Gegenprobe
 * mit drin: die Rechnung muss sie ebenso treffen wie die neuen. */
static const int ED_SPT[] = { 36, 41, 42, 43, 44 };
#define ED_N (sizeof ED_SPT / sizeof ED_SPT[0])

static size_t ed_groesse(int spt)
{
    return (size_t)spt * SEITEN * SPUREN * SEKTORGR;
}

/* Ein Puffer, wie ihn ein echtes PC-Abbild am Anfang hat. Ein
 * Nullpuffer waere fuer diese Frage NICHT repraesentativ: `dim_probe()`
 * liest data[0] als Medienkennung, und 0x00 IST eine gueltige (2HD). */
static unsigned char *bootpuffer(size_t n)
{
    unsigned char *b = calloc(1, n);
    if (!b || n < 512) { free(b); return NULL; }
    b[0] = 0xEB; b[1] = 0x3C; b[2] = 0x90;
    memcpy(b + 3, "MSDOS5.0", 8);
    b[11] = 0x00; b[12] = 0x02;       /* 512 Byte je Sektor */
    b[510] = 0x55; b[511] = 0xAA;     /* Bootkennung        */
    return b;
}

static const uft_format_plugin_t *plugin_namens(const char *name)
{
    size_t n = uft_registered_format_plugin_count();
    for (size_t i = 0; i < n; i++) {
        const uft_format_plugin_t *p = uft_registered_format_plugin_at(i);
        if (p && p->name && strcmp(p->name, name) == 0) return p;
    }
    return NULL;
}

TEST(die_groessen_folgen_aus_der_rechnung)
{
    /* Die bestehende Zeile als Anker, damit die Rechnung belegt ist und
     * nicht bloss behauptet. */
    static const size_t SOLL[] = {
        2949120u, 3358720u, 3440640u, 3522560u, 3604480u,
    };
    for (size_t i = 0; i < ED_N; i++) {
        size_t g = ed_groesse(ED_SPT[i]);
        if (g != SOLL[i]) {
            printf("\n      %d SpT: %zu, erwartet %zu\n      ",
                   ED_SPT[i], g, SOLL[i]);
            _fail++;
            return;
        }
    }
}

TEST(img_erkennt_alle_fuenf_ed_groessen)
{
    /* DER ROTBEWEIS. Vor MF-872 faellt dieser Fall fuer VIER der fuenf
     * Groessen um — nur 36 SpT war eingetragen. */
    const uft_format_plugin_t *img = plugin_namens("IMG");
    ASSERT(img != NULL);
    ASSERT(img->probe != NULL);

    unsigned char *b = bootpuffer(65536);
    ASSERT(b != NULL);

    for (size_t i = 0; i < ED_N; i++) {
        size_t g = ed_groesse(ED_SPT[i]);
        int k = 0;
        if (!img->probe(b, 65536, g, &k)) {
            printf("\n      %d SpT (%zu Byte) von IMG abgewiesen\n      ",
                   ED_SPT[i], g);
            _fail++;
            free(b);
            return;
        }
        /* Mit gueltigem Bootsektor gehoert die Konfidenz ins obere Band
         * — Merkmal getroffen, nicht bloss die Groesse (MF-729). */
        if (k < 80) {
            printf("\n      %d SpT: Konfidenz %d, erwartet >= 80\n      ",
                   ED_SPT[i], k);
            _fail++;
            free(b);
            return;
        }
    }
    free(b);
}

TEST(img_schlaegt_dmk_auf_den_ed_groessen)
{
    /* Der eigentliche Schaden war nicht die Luecke, sondern wer sie
     * fuellte: `DMK` beanspruchte alle vier mit 55. Dass IMG jetzt
     * erkennt, hilft nur, wenn es auch gewinnt. */
    const uft_format_plugin_t *img = plugin_namens("IMG");
    const uft_format_plugin_t *dmk = plugin_namens("DMK");
    ASSERT(img != NULL);
    if (!dmk) return;   /* DMK nicht registriert — dann gibt es nichts
                         * zu schlagen; kein Fehler. */

    unsigned char *b = bootpuffer(65536);
    ASSERT(b != NULL);

    for (size_t i = 1; i < ED_N; i++) {     /* die vier neuen */
        size_t g = ed_groesse(ED_SPT[i]);
        int ki = 0, kd = 0;
        bool oi = img->probe(b, 65536, g, &ki);
        bool od = dmk->probe(b, 65536, g, &kd);
        if (!oi || (od && kd >= ki)) {
            printf("\n      %d SpT: IMG=%d(%d), DMK=%d(%d) — IMG "
                   "gewinnt nicht\n      ",
                   ED_SPT[i], (int)oi, ki, (int)od, kd);
            _fail++;
            free(b);
            return;
        }
    }
    free(b);
}

TEST(eine_groesse_daneben_wird_nicht_ed)
{
    /* Ohne diesen Fall waere eine Tabelle, die alles annimmt, ebenso
     * "gruen". Ein Sektor mehr oder weniger ist kein ED-Format. */
    const uft_format_plugin_t *img = plugin_namens("IMG");
    ASSERT(img != NULL);
    unsigned char *b = bootpuffer(65536);
    ASSERT(b != NULL);

    for (size_t i = 1; i < ED_N; i++) {
        size_t g = ed_groesse(ED_SPT[i]);
        int k = 0;
        /* +512: keine der Zahlen, und der generische Rueckfall darf sie
         * auch nicht ueber eine Zylinderzahl <= 84 einfangen. */
        if (img->probe(b, 65536, g + SEKTORGR, &k)) {
            printf("\n      %zu Byte (%d SpT + 1 Sektor) angenommen, "
                   "Konfidenz %d\n      ", g + SEKTORGR, ED_SPT[i], k);
            _fail++;
            free(b);
            return;
        }
    }
    free(b);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (uft_register_all_formats() != UFT_OK) {
        printf("uft_register_all_formats() fehlgeschlagen\n");
        return 1;
    }
    printf("=== IMG: ED-Spezialformate 41-44 SpT (MF-872) ===\n");
    RUN(die_groessen_folgen_aus_der_rechnung);
    RUN(img_erkennt_alle_fuenf_ed_groessen);
    RUN(img_schlaegt_dmk_auf_den_ed_groessen);
    RUN(eine_groesse_daneben_wird_nicht_ed);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
