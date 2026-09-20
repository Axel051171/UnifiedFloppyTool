/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_fat_kette_robust.c
 * @brief Der FAT12-Kettenlaeufer auf beschaedigten Disketten (MF-1276)
 *
 * ── WARUM ES DIESEN TEST GIBT ────────────────────────────────────────────
 *
 * `uft_fat_get_chain()` ist die Funktion, an der JEDE FAT12-Extraktion
 * haengt — und gemessen vor MF-1276 rief sie **kein einziger Test**
 * (`git grep -l uft_fat_get_chain -- tests/` : null Treffer). Die
 * Schleifenbremse, die Kettengrenzen und das Verhalten bei einer
 * abgerissenen Kette waren damit unbewacht.
 *
 * ── DER DEFEKT, DEN DIE ERSTE GRUPPE FESTHAELT ───────────────────────────
 *
 * Dort stand:
 *
 *     for (size_t i = 0; i < chain->count && i < 16; i++)
 *         if (chain->clusters[i] == (uint32_t)n) { has_loops = true; ... }
 *
 * Der naechste Cluster wurde nur gegen die ERSTEN SECHZEHN Eintraege
 * gehalten. Eine Schleife, die sich spaeter schliesst, war unsichtbar:
 * `has_loops` blieb false, die Kette lief bis zur 65536er-Bremse, und der
 * Aufrufer bekam zehntausende Cluster ohne jede Warnung.
 *
 * ── HERKUNFT ─────────────────────────────────────────────────────────────
 *
 * Die Verfahren (Schleifenbremse mit voller `seen`-Menge, Bedarfsgrenze
 * aus der Dateigroesse, fortlaufender Rueckfall) stammen aus disk-peek
 * (Joost Yervante Damad, MIT, `js/fat12.js:137-159`) — Verfahren
 * uebernommen, Code neu geschrieben. Ausarbeitung:
 * `neue-ideen/UFT-NN — FAT12 lesen.zip`.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/fs/uft_fat12.h"

static int _fail = 0;
#define CHECK(c, ...) do { if (!(c)) { \
    printf("  FEHLGESCHLAGEN (%s:%d): ", __func__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); _fail++; } } while (0)

/* 360 KB, 40 x 2 x 9 — dieselbe Geometrie wie test_fat_kopien_kette.c. */
#define BPS        512u
#define SPC          2u
#define RESERVED     1u
#define FATS         2u
#define ROOT_ENT   112u
#define TOTAL      720u
#define SPF          2u

#define FAT_START   (RESERVED * BPS)
#define FAT_BYTES   (SPF * BPS)
#define ROOT_BYTES  (ROOT_ENT * 32u)
#define CLUSTER     (SPC * BPS)
#define ABBILD      (TOTAL * BPS)
#define ROOT_START  (FAT_START + FATS * FAT_BYTES)
#define DATA_START  (ROOT_START + ROOT_BYTES)

static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;         p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/** Einen 12-Bit-Eintrag in die FAT schreiben. */
static void fat_set(uint8_t *fat, unsigned cluster, uint16_t wert)
{
    unsigned off = cluster + cluster / 2;
    if (cluster & 1u) {
        fat[off]     = (uint8_t)((fat[off] & 0x0F) | ((wert & 0x0F) << 4));
        fat[off + 1] = (uint8_t)(wert >> 4);
    } else {
        fat[off]     = (uint8_t)(wert & 0xFF);
        fat[off + 1] = (uint8_t)((fat[off + 1] & 0xF0) | ((wert >> 8) & 0x0F));
    }
}

/** Ein leeres, gueltiges 360-KB-FAT12-Abbild. Die Kette legt der Aufrufer. */
static uint8_t *grundabbild(void)
{
    uint8_t *img = (uint8_t *)calloc(1, ABBILD);
    if (!img) return NULL;
    img[0] = 0xEB; img[1] = 0x3C; img[2] = 0x90;
    memcpy(img + 3, "UFTTEST ", 8);
    put16(img + 0x0B, (uint16_t)BPS);
    img[0x0D] = (uint8_t)SPC;
    put16(img + 0x0E, (uint16_t)RESERVED);
    img[0x10] = (uint8_t)FATS;
    put16(img + 0x11, (uint16_t)ROOT_ENT);
    put16(img + 0x13, (uint16_t)TOTAL);
    img[0x15] = 0xFD;
    put16(img + 0x16, (uint16_t)SPF);
    put16(img + 0x18, 9);
    put16(img + 0x1A, 2);
    img[510] = 0x55; img[511] = 0xAA;
    img[FAT_START] = 0xFD; img[FAT_START + 1] = 0xFF; img[FAT_START + 2] = 0xFF;
    return img;
}

/** Einen Verzeichniseintrag legen. */
static void datei_eintragen(uint8_t *img, unsigned slot, const char *name11,
                            unsigned start, uint32_t groesse)
{
    uint8_t *e = img + ROOT_START + slot * 32u;
    memcpy(e, name11, 11);
    e[11] = 0x20;
    put16(e + 26, (uint16_t)start);
    put32(e + 28, groesse);
}

static uft_fat_ctx_t *oeffnen(uint8_t *img)
{
    uft_fat_ctx_t *ctx = uft_fat_create();
    if (!ctx) return NULL;
    if (uft_fat_open(ctx, img, ABBILD, false) != UFT_FAT_OK) {
        uft_fat_destroy(ctx);
        return NULL;
    }
    return ctx;
}

/* ═══════════ 1. Die Schleife JENSEITS des alten 16er-Fensters ═══════ */

static void t1_schleife_spaet(void)
{
    printf("Test 1: eine Schleife hinter dem 16. Cluster wird gesehen\n");
    uint8_t *img = grundabbild();
    CHECK(img != NULL, "Abbild");
    if (!img) return;
    uint8_t *f1 = img + FAT_START;

    /* 2 -> 3 -> ... -> 40, dann 40 -> 20: der Ring schliesst sich weit
     * hinter dem alten Fenster. */
    for (unsigned c = 2; c < 40; c++) fat_set(f1, c, (uint16_t)(c + 1));
    fat_set(f1, 40, 20);
    memcpy(img + FAT_START + FAT_BYTES, f1, FAT_BYTES);

    uft_fat_ctx_t *ctx = oeffnen(img);
    CHECK(ctx != NULL, "oeffnen");
    if (ctx) {
        uft_fat_chain_t k;
        uft_fat_chain_init(&k);
        CHECK(uft_fat_get_chain(ctx, 2u, &k) == UFT_FAT_OK, "Kette gelesen");
        CHECK(k.has_loops,
              "die Schleife 40 -> 20 MUSS erkannt werden; vor MF-1276 blieb "
              "has_loops false und die Kette lief bis zur 65536er-Bremse");
        CHECK(k.loop_at == 20u,
              "und es steht dabei, WO sie sich schliesst: erwartet 20, war %u",
              (unsigned)k.loop_at);
        CHECK(k.count == 39u,
              "gesammelt werden die Cluster 2..40, also 39 — nicht mehr; "
              "gezaehlt %zu", k.count);
        CHECK(!k.complete, "und vollstaendig ist sie nicht");
        printf("    Ring 40 -> 20 nach 39 Clustern: erkannt, loop_at=%u\n",
               (unsigned)k.loop_at);
        uft_fat_chain_free(&k);
        uft_fat_destroy(ctx);
    }
    free(img);
}

/* ═══════════ 2. Die Schleife INNERHALB des Fensters — kein Rueckschritt ═ */

static void t2_schleife_frueh(void)
{
    printf("Test 2: die kurze Schleife wird weiterhin gesehen\n");
    uint8_t *img = grundabbild();
    CHECK(img != NULL, "Abbild");
    if (!img) return;
    uint8_t *f1 = img + FAT_START;
    fat_set(f1, 2, 3); fat_set(f1, 3, 2);     /* 2 -> 3 -> 2 */
    memcpy(img + FAT_START + FAT_BYTES, f1, FAT_BYTES);

    uft_fat_ctx_t *ctx = oeffnen(img);
    CHECK(ctx != NULL, "oeffnen");
    if (ctx) {
        uft_fat_chain_t k;
        uft_fat_chain_init(&k);
        uft_fat_get_chain(ctx, 2u, &k);
        CHECK(k.has_loops, "2 -> 3 -> 2 ist eine Schleife");
        CHECK(k.loop_at == 2u, "sie schliesst sich bei 2, war %u",
              (unsigned)k.loop_at);
        CHECK(k.count == 2u, "und sie hat zwei Glieder, gezaehlt %zu", k.count);
        printf("    Ring 2 -> 3 -> 2: erkannt, zwei Glieder\n");
        uft_fat_chain_free(&k);
        uft_fat_destroy(ctx);
    }
    free(img);
}

/* ═══════════ 3. Die abgerissene Kette ══════════════════════════════ */

static void t3_abgerissen(void)
{
    printf("Test 3: eine Kette, die nicht bis zum Dateiende traegt\n");
    uint8_t *img = grundabbild();
    CHECK(img != NULL, "Abbild");
    if (!img) return;
    uint8_t *f1 = img + FAT_START;

    /* Die Datei braucht VIER Cluster; die Kette traegt zwei und endet. */
    fat_set(f1, 2, 3);
    fat_set(f1, 3, 0xFFF);
    memcpy(img + FAT_START + FAT_BYTES, f1, FAT_BYTES);
    datei_eintragen(img, 0, "KAPUTT  BIN", 2u, 4u * CLUSTER);
    for (unsigned c = 2; c <= 5; c++)
        memset(img + DATA_START + (c - 2) * CLUSTER, (int)c, CLUSTER);

    uft_fat_ctx_t *ctx = oeffnen(img);
    CHECK(ctx != NULL, "oeffnen");
    if (ctx) {
        uft_fat_chain_t k;
        uft_fat_chain_init(&k);
        CHECK(uft_fat_get_chain_sized(ctx, 2u, 4u * CLUSTER, &k) == UFT_FAT_OK,
              "Kette mit Groesse gelesen");
        CHECK(k.needed == 4u, "vier Cluster verlangt die Groesse, %zu", k.needed);
        CHECK(k.from_chain == 2u,
              "die Kette selbst trug ZWEI — das ist die Zahl, die vorher "
              "nirgends stand; gezaehlt %zu", k.from_chain);
        CHECK(k.count == 4u, "aufgefuellt auf vier, %zu", k.count);
        CHECK(k.status == UFT_FAT_CHAIN_CONTIG,
              "und der Zustand sagt, dass die letzten zwei GERATEN sind: %s",
              uft_fat_chain_status_name(k.status));
        CHECK(k.count >= 4u && k.clusters[2] == 4u && k.clusters[3] == 5u,
              "fortlaufend ab dem letzten erreichten Cluster");
        printf("    2 von 4 Clustern aus der Kette, 2 fortlaufend: %s\n",
               uft_fat_chain_status_name(k.status));
        uft_fat_chain_free(&k);

        /* Der Produktivpfad: `uft_fat_extract()` fragt seit MF-1276 MIT
         * der Groesse und bekommt deshalb den Rueckfall. Ohne ihn kaeme
         * eine halbe Datei zurueck. */
        uft_fat_entry_t e;
        memset(&e, 0, sizeof(e));
        e.cluster = 2u;
        e.size = 4u * CLUSTER;
        size_t n = 0;
        uint8_t *daten = uft_fat_extract(ctx, &e, NULL, &n);
        CHECK(daten != NULL, "extrahiert");
        CHECK(n == 4u * CLUSTER,
              "der Aufrufer bekommt die volle Groesse (%zu statt %u) — vorher "
              "die Haelfte", n, 4u * CLUSTER);
        if (daten && n == 4u * CLUSTER)
            CHECK(daten[0] == 2 && daten[CLUSTER] == 3
                  && daten[2u * CLUSTER] == 4 && daten[3u * CLUSTER] == 5,
                  "und zwar die Cluster 2,3 aus der Kette und 4,5 fortlaufend");
        free(daten);
        uft_fat_destroy(ctx);
    }
    free(img);
}

/* ═══════════ 4. Die unsinnige Groesse ══════════════════════════════ */

static void t4_groesse_unsinnig(void)
{
    printf("Test 4: eine Groessenangabe von 4 GB sprengt nichts\n");
    uint8_t *img = grundabbild();
    CHECK(img != NULL, "Abbild");
    if (!img) return;
    uint8_t *f1 = img + FAT_START;
    fat_set(f1, 2, 0xFFF);
    memcpy(img + FAT_START + FAT_BYTES, f1, FAT_BYTES);

    uft_fat_ctx_t *ctx = oeffnen(img);
    CHECK(ctx != NULL, "oeffnen");
    if (ctx) {
        uft_fat_chain_t k;
        uft_fat_chain_init(&k);
        CHECK(uft_fat_get_chain_sized(ctx, 2u, 0xFFFFFFFFu, &k) == UFT_FAT_OK,
              "Kette gelesen");
        /* Die Diskette hat rund 354 Datencluster; mehr kann niemand
         * liefern, egal was im Verzeichniseintrag steht. */
        CHECK(k.count < 400u,
              "auf die Cluster der Diskette begrenzt, geliefert %zu", k.count);
        CHECK(k.needed > k.count,
              "und der Bedarf bleibt sichtbar groesser als das Gelieferte "
              "(%zu > %zu)", k.needed, k.count);
        CHECK(k.status == UFT_FAT_CHAIN_CONTIG || k.status == UFT_FAT_CHAIN_SHORT,
              "der Zustand sagt, dass es nicht reicht: %s",
              uft_fat_chain_status_name(k.status));
        printf("    4 GB verlangt, %zu Cluster geliefert, Zustand: %s\n",
               k.count, uft_fat_chain_status_name(k.status));
        uft_fat_chain_free(&k);
        uft_fat_destroy(ctx);
    }
    free(img);
}

/* ═══════════ 5. Die Kette, die WEITER laeuft als die Datei ═════════
 *
 * Nachgetragen, weil der Rotbeweis es verlangt hat: die Mutation „keine
 * Deckelung auf den Bedarf" ist beim ersten Lauf NICHT gefallen. Die
 * Gruppe 4 deckt sie nicht ab — dort endet die Kette sofort, und die
 * Obergrenze, die dann greift, ist die Diskette und nicht der Bedarf.
 *
 * Der Fall, der die Deckelung wirklich braucht, ist der umgekehrte: eine
 * FAT, die zehn Cluster verkettet, und ein Verzeichniseintrag, der zwei
 * Cluster Groesse nennt. Ohne Deckelung bekaeme der Aufrufer zehn. */
static void t5_kette_laenger_als_datei(void)
{
    printf("Test 5: eine Kette, die laenger ist als die Datei\n");
    uint8_t *img = grundabbild();
    CHECK(img != NULL, "Abbild");
    if (!img) return;
    uint8_t *f1 = img + FAT_START;

    for (unsigned c = 2; c < 11; c++) fat_set(f1, c, (uint16_t)(c + 1));
    fat_set(f1, 11, 0xFFF);            /* 2 -> 3 -> ... -> 11, zehn Glieder */
    memcpy(img + FAT_START + FAT_BYTES, f1, FAT_BYTES);

    uft_fat_ctx_t *ctx = oeffnen(img);
    CHECK(ctx != NULL, "oeffnen");
    if (ctx) {
        /* Ohne Groesse: die ganze Kette, zehn Glieder. */
        uft_fat_chain_t voll;
        uft_fat_chain_init(&voll);
        uft_fat_get_chain(ctx, 2u, &voll);
        CHECK(voll.count == 10u,
              "ohne Groessenangabe traegt die Kette zehn Glieder, %zu",
              voll.count);
        uft_fat_chain_free(&voll);

        /* MIT Groesse: zwei Cluster, und keiner mehr. */
        uft_fat_chain_t k;
        uft_fat_chain_init(&k);
        CHECK(uft_fat_get_chain_sized(ctx, 2u, 2u * CLUSTER, &k) == UFT_FAT_OK,
              "Kette mit Groesse gelesen");
        CHECK(k.needed == 2u, "zwei Cluster verlangt die Groesse, %zu", k.needed);
        CHECK(k.count == 2u,
              "und mehr darf der Aufrufer nicht bekommen, auch wenn die Kette "
              "weiterlaeuft; geliefert %zu", k.count);
        CHECK(k.status == UFT_FAT_CHAIN_OK,
              "das ist kein Schaden, sondern eine Datei in einer laengeren "
              "Kette: %s", uft_fat_chain_status_name(k.status));
        printf("    Kette 10 Glieder, Datei 2 Cluster: %zu geliefert\n", k.count);
        uft_fat_chain_free(&k);
        uft_fat_destroy(ctx);
    }
    free(img);
}

int main(void)
{
    printf("=== test_fat_kette_robust (MF-1276) ===\n\n");
    t1_schleife_spaet();
    t2_schleife_frueh();
    t3_abgerissen();
    t4_groesse_unsinnig();
    t5_kette_laenger_als_datei();
    printf("\n%s (%d Fehler)\n", _fail ? "FEHLGESCHLAGEN" : "BESTANDEN", _fail);
    return _fail ? 1 : 0;
}
