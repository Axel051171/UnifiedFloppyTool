/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_st_reihenfolge_bericht.c
 * @brief Die Tuer fuer `uft_st_order_messen()` (H-18): Interleave,
 *        Spiralfaktor und TOS-Herkunft im Traegerbericht (Stufe 3).
 *
 * Bis hierher hatte das ganze ST-Ordnungsmodul null produktive Aufrufer.
 * `uft_st_order_befunde()` liest das Modell und schreibt je Kopf einen
 * Befund — aber NUR, wenn zwei Dinge belegt sind:
 *
 *   physische Reihenfolge  jede Spur traegt einen Bitstrom mit bekannter
 *                          Indexlage, und JEDER Sektor nennt seine Bitlage.
 *                          Die Reihenfolge wird aus den Lagen GERECHNET,
 *                          nicht aus der Feldreihenfolge genommen — der Test
 *                          legt die Sektoren deshalb absichtlich RUECKWAERTS
 *                          an. Ein Sektorabbild (.ST/.MSA) traegt keine
 *                          Lagen; dort ist die Reihenfolge normalisiert.
 *   Atari ST               das Plugin, aus dem das Modell stammt, ist ein
 *                          reiner Atari-Behaelter (ST, MSA, STX). Die
 *                          Geometrie allein belegt es NICHT: 80x2x9x512 ist
 *                          ebenso die PC-720K-Diskette, und deren Spiral-
 *                          faktor 0 hiesse hier „TOS 1.0".
 *
 * Faelle:
 *   A  physisch, Spiralfaktor 2, STX          -> Bericht nennt TOS 1.02
 *   B  dieselben Spuren ohne Bitlagen (.ST)   -> keine ST-Zeile
 *   B2 Lagen ohne Index / Index ohne Lagen    -> keine ST-Zeile
 *   C  physisch, aber Plugin HFE              -> keine ST-Zeile
 *   D  eine Spur mit Luecke (H-18), auch als
 *      Vorspur                                -> keine Messung
 *   E  zweiter Aufruf                         -> keine Doppelung
 *   F  Spiralfaktor 2, 2, 3                   -> „uneinheitlich", keine
 *                                                Herkunft
 *
 * B2, D (Vorspur) und F kamen aus der Mutationsmatrix: ohne sie haette
 * das Weglassen der Indexpruefung, der Lagenpruefung, der Vorspurpruefung
 * und der Einigkeitspruefung keinen Test rot gemacht.
 */
#include "uft/core/uft_disk2.h"
#include "uft/formats/st/uft_st_order.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0, g_pass = 0;
#define CHECK(c, ...) do { if (c) { g_pass++; } else { g_fail++;              \
    printf("    FAIL @%d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } \
    } while (0)

#define NBITS   100000u
#define SLOT    10000u

static uint8_t g_bits[NBITS / 8u];

/**
 * Spur (c,h) mit den Sektornummern `phys[0..n)` in PHYSISCHER Reihenfolge
 * ab dem Index. Mit `lagen` bekommt jeder Sektor seine Bitlage und die
 * Spur einen Bitstrom mit Index bei Bit 0; angelegt werden die Sektoren
 * RUECKWAERTS, damit nur eine Messung aus den Lagen die Reihenfolge kennt.
 */
/* Wie viel das Modell ueber die Lage weiss. */
enum { OHNE = 0, LAGEN = 1, KEIN_INDEX = 2, INDEX_OHNE_LAGEN = 3 };

static void spur(uft_disk2_t *d, unsigned c, unsigned h, const uint8_t *phys,
                 unsigned n, int lagen) {
    uft_d2_track_t *t = uft_d2_track(d, (uint16_t)c, (uint8_t)h);
    if (lagen != OHNE
        && !uft_d2_set_bitstream(d, t, g_bits, NBITS, NULL, NULL, 0, NULL,
                                 NULL, lagen == KEIN_INDEX ? SIZE_MAX : 0u,
                                 UFT_ENC_MFM, 2000u, UFT_D2_DERIV_NONE)) {
        printf("set_bitstream\n"); exit(2);
    }
    for (unsigned k = n; k-- > 0;) {
        uft_d2_sector_t s;
        memset(&s, 0, sizeof s);
        s.id_cyl = (uint8_t)c; s.id_head = (uint8_t)h; s.id_sec = phys[k];
        s.id_size_code = 2;
        s.idam_bit = (lagen == LAGEN || lagen == KEIN_INDEX)
                   ? 1000u + (size_t)k * SLOT : SIZE_MAX;
        s.dam_bit = s.data_end_bit = SIZE_MAX;
        s.origin = UFT_D2_ORIGIN_CONTAINER;
        s.conf = UFT_D2_CONF_UNVERIFIED;
        if (!uft_d2_add_sector(d, t, &s)) { printf("add_sector\n"); exit(2); }
    }
}

/** Physische Folge einer Spur: Interleave 1, Anfang `anfang`, 1..n. */
static void folge(uint8_t *phys, unsigned n, unsigned anfang) {
    for (unsigned i = 0; i < n; ++i)
        phys[i] = (uint8_t)(1u + ((anfang - 1u + i) % n));
}

/** Eine ganze Diskette mit TOS-1.02-Spirale (Spiralfaktor 2 je Spur). */
static uft_disk2_t *diskette(const char *plugin, unsigned zyl, unsigned koepfe,
                             int lagen) {
    uft_disk2_t *d = uft_d2_create();
    if (plugin) uft_d2_add_meta(d, "Plugin", plugin, UFT_D2_META_SELF);
    for (unsigned c = 0; c < zyl; ++c)
        for (unsigned h = 0; h < koepfe; ++h) {
            uint8_t phys[9];
            /* Nach der Rechnung von `uft_st_spiral_messen()`: die erste
             * Nummer der Spur minus die der Vorspur, modulo 9. */
            folge(phys, 9, 1u + (2u * c) % 9u);
            spur(d, c, h, phys, 9, lagen);
        }
    return d;
}

static void bericht(const uft_disk2_t *d, char *buf, size_t n) {
    const size_t need = uft_d2_report(d, buf, n);
    if (need >= n) { printf("Bericht gekuerzt (%zu)\n", need); exit(2); }
}

static size_t zaehle(const uft_disk2_t *d, const char *code) {
    size_t n = 0;
    for (size_t i = 0; i < uft_d2_diag_count(d); ++i)
        if (strcmp(uft_d2_diag_at(d, i)->code, code) == 0) n++;
    return n;
}

static char g_buf[65536];

static void a_physisch_stx(void) {
    printf("A: physische Reihenfolge, Spiralfaktor 2, Plugin STX\n");
    uft_disk2_t *d = diskette("STX", 80, 2, 1);
    const size_t n = uft_st_order_befunde(d);
    bericht(d, g_buf, sizeof g_buf);
    printf("%s", g_buf);
    CHECK(n == 3u, "drei Befunde (Kopf 0, Kopf 1, Herkunft), %zu", n);
    CHECK(strstr(g_buf, "Kopf 0: Interleave 1, Spiralfaktor 2") != NULL,
          "Kopf 0 mit Interleave 1 und Spiralfaktor 2");
    CHECK(strstr(g_buf, "Kopf 1: Interleave 1, Spiralfaktor 2") != NULL,
          "Kopf 1 ebenso");
    CHECK(strstr(g_buf, "TOS 1.02") != NULL, "die TOS-1.02-Zeile");
    CHECK(strstr(g_buf, "79 Spurpaar") != NULL,
          "die Zahl der gemessenen Spurpaare steht dabei");

    /* E: kein zweites Mal. */
    const size_t vorher = uft_d2_diag_count(d);
    CHECK(uft_st_order_befunde(d) == 0u && uft_d2_diag_count(d) == vorher,
          "zweiter Aufruf fuegte Befunde hinzu");
    uft_d2_destroy(d);
}

static void b_sektorabbild(void) {
    printf("B: dieselben Spuren ohne Bitlagen (Sektorabbild, Plugin ST)\n");
    uft_disk2_t *d = diskette("ST", 80, 2, 0);
    const size_t n = uft_st_order_befunde(d);
    bericht(d, g_buf, sizeof g_buf);
    CHECK(n == 0u, "kein Befund, %zu", n);
    CHECK(strstr(g_buf, "Spiralfaktor") == NULL && strstr(g_buf, "TOS") == NULL,
          "keine ST-Zeile:\n%s", g_buf);
    uft_d2_destroy(d);
}

static void b2_lage_ohne_anker(void) {
    printf("B2: Bitstrom ohne Indexlage / Index ohne Sektorlagen\n");
    /* Lagen, aber kein Index: ohne Spuranfang kein Spiralfaktor. */
    uft_disk2_t *d = diskette("STX", 80, 1, KEIN_INDEX);
    CHECK(uft_st_order_befunde(d) == 0u, "ohne Indexlage kein Befund");
    uft_d2_destroy(d);
    /* Index, aber Sektoren ohne Lage: die Feldreihenfolge ist keine
     * Messung (hier sogar absichtlich rueckwaerts). */
    d = diskette("STX", 80, 1, INDEX_OHNE_LAGEN);
    CHECK(uft_st_order_befunde(d) == 0u, "ohne Sektorlagen kein Befund");
    uft_d2_destroy(d);

    /* EIN Sektor ohne Lage, bzw. zwei Sektoren an derselben Lage, auf
     * Zylinder 40: diese Spur misst nicht, also fallen genau ihre zwei
     * Paare weg — 77 statt 79. Der Sektor ist der physisch LETZTE
     * (Feldplatz 0, weil rueckwaerts angelegt): eine Sortierung, die ihn
     * trotzdem mitnaehme, stellte ihn ans Ende und wuerde richtig
     * messen — genau deshalb faellt sie hier auf (Mutationen S4, S8). */
    for (int fall = 0; fall < 2; ++fall) {
        d = diskette("STX", 80, 1, LAGEN);
        uft_d2_track_t *t = uft_d2_track(d, 40, 0);
        if (fall == 0) t->sectors.items[0].idam_bit = SIZE_MAX;
        else t->sectors.items[0].idam_bit = t->sectors.items[1].idam_bit;
        CHECK(uft_st_order_befunde(d) == 2u, "Kopf 0 und Herkunft");
        bericht(d, g_buf, sizeof g_buf);
        CHECK(strstr(g_buf, "aus 77 Spurpaaren") != NULL,
              "%s: Zylinder 40 misst nicht, 77 Paare:\n%s",
              fall == 0 ? "ohne Lage" : "doppelte Lage", g_buf);
        uft_d2_destroy(d);
    }
}

static void c_nicht_atari(void) {
    printf("C: physisch, aber Plugin HFE — Atari nicht belegt\n");
    uft_disk2_t *d = diskette("HFE", 80, 2, 1);
    CHECK(uft_st_order_befunde(d) == 0u, "kein Befund");
    uft_d2_destroy(d);

    d = diskette(NULL, 80, 2, 1);
    CHECK(uft_st_order_befunde(d) == 0u, "ohne Plugin-Angabe kein Befund");
    uft_d2_destroy(d);
}

static void d_luecke(void) {
    printf("D: Luecke in der Sektorfolge (H-18)\n");
    uft_disk2_t *d = uft_d2_create();
    uft_d2_add_meta(d, "Plugin", "STX", UFT_D2_META_SELF);
    const uint8_t voll[9]   = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    const uint8_t lueckig[9] = { 3, 4, 5, 6, 7, 8, 10, 1, 2 };   /* 9 fehlt */
    spur(d, 0, 0, voll, 9, 1);
    spur(d, 1, 0, lueckig, 9, 1);
    const size_t n = uft_st_order_befunde(d);
    bericht(d, g_buf, sizeof g_buf);
    CHECK(n == 0u, "keine Messung, %zu Befunde", n);
    CHECK(strstr(g_buf, "Spiralfaktor") == NULL, "keine ST-Zeile:\n%s", g_buf);
    CHECK(zaehle(d, "ST_REIHENFOLGE") == 0u, "kein ST_REIHENFOLGE");
    uft_d2_destroy(d);

    /* Umgekehrt: die VORSPUR hat die Luecke. Ihr erster Sektor ist dann
     * kein belegter Spuranfang, also kein Paar. */
    d = uft_d2_create();
    uft_d2_add_meta(d, "Plugin", "STX", UFT_D2_META_SELF);
    const uint8_t versetzt[9] = { 3, 4, 5, 6, 7, 8, 9, 1, 2 };
    spur(d, 0, 0, lueckig, 9, LAGEN);
    spur(d, 1, 0, versetzt, 9, LAGEN);
    CHECK(uft_st_order_befunde(d) == 0u, "lueckige Vorspur: kein Paar");
    uft_d2_destroy(d);
}

static void f_uneinheitlich(void) {
    printf("F: Spiralfaktor nicht einheitlich\n");
    uft_disk2_t *d = uft_d2_create();
    uft_d2_add_meta(d, "Plugin", "STX", UFT_D2_META_SELF);
    /* Anfaenge 1, 3, 5, 8: Spiralfaktoren 2, 2, 3. */
    const unsigned anfang[4] = { 1, 3, 5, 8 };
    for (unsigned c = 0; c < 4; ++c) {
        uint8_t phys[9];
        folge(phys, 9, anfang[c]);
        spur(d, c, 0, phys, 9, LAGEN);
    }
    const size_t n = uft_st_order_befunde(d);
    bericht(d, g_buf, sizeof g_buf);
    CHECK(n == 1u, "ein Befund, %zu", n);
    CHECK(strstr(g_buf, "uneinheitlich") != NULL, "sagt uneinheitlich:\n%s", g_buf);
    CHECK(strstr(g_buf, "TOS") == NULL, "keine Herkunft:\n%s", g_buf);
    uft_d2_destroy(d);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== test_st_reihenfolge_bericht ===\n\n");
    a_physisch_stx();
    b_sektorabbild();
    b2_lage_ohne_anker();
    c_nicht_atari();
    d_luecke();
    f_uneinheitlich();
    printf("\n%d bestanden, %d fehlgeschlagen\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
