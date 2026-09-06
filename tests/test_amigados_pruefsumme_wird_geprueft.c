/* SPDX-License-Identifier: MIT */
/**
 * @file test_amigados_pruefsumme_wird_geprueft.c
 * @brief `verify_checksums` war gesetzt und wirkungslos (MF-934)
 *
 * ── DER BEFUND ───────────────────────────────────────────────────────
 *
 * `uft_amiga_ctx_t` traegt ein Feld `verify_checksums`. Es wird in
 * `uft_amiga_create()` auf `true` gesetzt und laesst sich ueber
 * `uft_amiga_open_buffer(..., opts)` umstellen.
 *
 * Gemessen (MF-934, ueber `git ls-files`): das Feld wird an genau ZWEI
 * Stellen GESCHRIEBEN und an KEINER gelesen. Es entscheidet nichts.
 *
 * Dasselbe gilt fuer die oeffentliche Pruefsummen-API des Dateisystems:
 *
 *     uft_amiga_verify_checksum()   deklariert, definiert, 0 Aufrufer
 *     uft_amiga_update_checksum()   deklariert, definiert, 0 Aufrufer
 *
 * Der Verzeichnispfad liest die Bloecke ohnehin an der API vorbei:
 * `parse_entry()` greift direkt auf `ctx->data + block * BLOCK_SIZE` zu.
 * Ein AmigaDOS-Block mit falscher Pruefsumme wird deshalb wie ein
 * gueltiger ausgegeben — und der Schalter, der das Gegenteil verspricht,
 * ist wirkungslos.
 *
 * Fuer ein Werkzeug mit dem Grundsatz „Keine erfundenen Daten" ist das
 * die teurere Haelfte: nicht ein fehlender Befund, sondern ein
 * ZUGESICHERTER, der nie stattfand.
 *
 * ── DIE REFERENZ ─────────────────────────────────────────────────────
 *
 * Die AmigaDOS-Blockpruefsumme ist die Summe aller 128 Big-Endian-
 * Langworte eines 512-Byte-Blocks; fuer einen gueltigen Block ist sie 0.
 * Im Baum steht sie dreifach:
 *
 *   1. `src/fs/uft_amigados.c`      uft_amiga_verify_checksum()   (0 Aufrufer)
 *   2. `src/fs/uft_amigados_extended.c` amiga_block_checksum_ok() (Validierer)
 *   3. `src/formats/uft_adf.c`      uft_adf_verify_checksum()     (ADF-Plugin)
 *
 * Alle drei rechnen dasselbe. Dieser Test benutzt (1) — die oeffentliche,
 * bis MF-934 tote — und macht sie damit zur benutzten.
 *
 * Der Bootblock hat einen ANDEREN Algorithmus (End-Around-Carry ueber
 * 256 Langworte, Einerkomplement) und ist davon nicht beruehrt; er ist
 * durch die Disassemblierung eines Sektor-Editors von 1986 bestaetigt
 * (`ACS-UtilitiesCompilation4/4`, Offset 0x4dec-0x4e42, UFT-30).
 *
 * ── ROTBEWEIS ────────────────────────────────────────────────────────
 *
 * Vor der Verdrahtung meldet dieser Test einen beschaedigten Block als
 * `checksum_ok == true`.
 */

#include "uft/fs/uft_amigados.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

#define BLK        512u
#define BLOCKS     256u
#define ROOT_BLK   (BLOCKS / 2)

/* Versaetze aus src/fs/uft_amigados.c */
#define O_TYPE        0x000
#define O_HEADER_KEY  0x004
#define O_CHECKSUM    0x014
#define O_HTAB_SIZE   0x00C
#define O_HASHTABLE   0x018
#define O_HASH_CHAIN  0x1F0
#define O_PARENT      0x1F4
#define O_SEC_TYPE    0x1FC
#define O_NAME        432

#define T_HEADER      2
#define ST_ROOT       1
#define ST_FILE       (-3)

static void put_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

static uint32_t get_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void put_bcpl(uint8_t *p, const char *s)
{
    size_t n = strlen(s);
    if (n > 30) n = 30;
    p[0] = (uint8_t)n;
    memcpy(p + 1, s, n);
}

/* Die Pruefsumme SETZEN — Feld nullen, alles summieren, Negatives
 * eintragen. Bewusst hier von Hand statt ueber uft_amiga_update_checksum():
 * ein Pruefstand, der die zu pruefende Funktion zum Erzeugen benutzt,
 * misst nur sich selbst. */
static void setze_pruefsumme(uint8_t *block)
{
    put_be32(block + O_CHECKSUM, 0);
    uint32_t sum = 0;
    for (size_t i = 0; i < 128; i++) sum += get_be32(block + i * 4);
    put_be32(block + O_CHECKSUM, (uint32_t)(0u - sum));
}

/**
 * Wurzel (Block 128) -> Hash-Slot 0 -> Block 100 "GUT" -> Block 101 "BOESE"
 *
 * Alle Bloecke bekommen eine GUELTIGE Pruefsumme. Ist `kaputt` wahr, wird
 * danach ein Nutzbyte in Block 101 veraendert, OHNE die Pruefsumme
 * nachzuziehen — genau der Zustand, den ein Lesefehler oder eine
 * Manipulation hinterlaesst.
 */
static uint8_t *baue_abbild(int kaputt, size_t *size_out)
{
    const size_t size = (size_t)BLOCKS * BLK;
    uint8_t *img = (uint8_t *)calloc(1, size);
    if (!img) return NULL;

    memcpy(img, "DOS", 3);
    img[3] = 0x00;                      /* OFS */

    uint8_t *root = img + (size_t)ROOT_BLK * BLK;
    put_be32(root + O_TYPE, T_HEADER);
    put_be32(root + O_HEADER_KEY, 0);
    put_be32(root + O_HTAB_SIZE, 72);
    put_be32(root + O_SEC_TYPE, (uint32_t)ST_ROOT);
    put_bcpl(root + O_NAME, "PRUEFTEST");
    put_be32(root + O_HASHTABLE + 0 * 4, 100);
    setze_pruefsumme(root);

    uint8_t *a = img + 100u * BLK;
    put_be32(a + O_TYPE, T_HEADER);
    put_be32(a + O_HEADER_KEY, 100);
    put_be32(a + O_SEC_TYPE, (uint32_t)ST_FILE);
    put_be32(a + O_PARENT, ROOT_BLK);
    put_be32(a + O_HASH_CHAIN, 101);
    put_bcpl(a + O_NAME, "GUT");
    setze_pruefsumme(a);

    uint8_t *b = img + 101u * BLK;
    put_be32(b + O_TYPE, T_HEADER);
    put_be32(b + O_HEADER_KEY, 101);
    put_be32(b + O_SEC_TYPE, (uint32_t)ST_FILE);
    put_be32(b + O_PARENT, ROOT_BLK);
    put_be32(b + O_HASH_CHAIN, 0);
    put_bcpl(b + O_NAME, "BOESE");
    setze_pruefsumme(b);

    if (kaputt) {
        /* Ein Byte im Namensfeld kippen, Pruefsumme NICHT nachziehen.
         * Der Block bleibt strukturell lesbar — nur seine Pruefsumme
         * stimmt nicht mehr. */
        b[O_NAME + 1] ^= 0x20;
    }

    *size_out = size;
    return img;
}

/* Der Pruefstand muss selbst stimmen: ein sauber gebautes Abbild traegt
 * gueltige Pruefsummen, ein verfaelschtes nicht. Ohne diese Zusicherung
 * koennte der eigentliche Test aus dem falschen Grund gruen sein. */
TEST(der_pruefstand_baut_was_er_behauptet)
{
    size_t n = 0;
    uint8_t *gut = baue_abbild(0, &n);
    ASSERT(gut != NULL);
    ASSERT(uft_amiga_verify_checksum(gut + 101u * BLK) == true);
    free(gut);

    uint8_t *schlecht = baue_abbild(1, &n);
    ASSERT(schlecht != NULL);
    ASSERT(uft_amiga_verify_checksum(schlecht + 101u * BLK) == false);
    free(schlecht);
}

/* Ein unversehrtes Verzeichnis: beide Eintraege gelten. */
TEST(unversehrte_eintraege_gelten)
{
    size_t size = 0;
    uint8_t *img = baue_abbild(0, &size);
    ASSERT(img != NULL);

    uft_amiga_ctx_t *ctx = uft_amiga_create();
    ASSERT(ctx != NULL);
    ASSERT(uft_amiga_open_buffer(ctx, img, size, false, NULL) == 0);

    uft_amiga_dir_t dir;
    memset(&dir, 0, sizeof(dir));
    ASSERT(uft_amiga_load_dir_path(ctx, "", &dir) == 0);
    ASSERT(dir.count == 2);
    ASSERT(dir.bad_checksums == 0);
    for (size_t i = 0; i < dir.count; i++) {
        ASSERT(dir.entries[i].checksum_ok == true);
    }

    uft_amiga_free_dir(&dir);
    uft_amiga_close(ctx);
    uft_amiga_destroy(ctx);
    free(img);
}

/* ─────────────────────────────────────────────────────────────────────
 *  DIE ZEILE: ein Block mit falscher Pruefsumme wird als solcher
 *  ausgewiesen — und trotzdem AUSGEGEBEN.
 *
 *  Beides gehoert zusammen. Ihn wegzulassen waere „Kein Bit verloren"
 *  verletzt; ihn unmarkiert auszugeben waere „Keine erfundenen Daten"
 *  verletzt. Der forensische Weg ist: zeigen UND kennzeichnen.
 * ───────────────────────────────────────────────────────────────────── */
TEST(beschaedigter_block_wird_gekennzeichnet_nicht_verschwiegen)
{
    size_t size = 0;
    uint8_t *img = baue_abbild(1, &size);
    ASSERT(img != NULL);

    uft_amiga_ctx_t *ctx = uft_amiga_create();
    ASSERT(ctx != NULL);
    ASSERT(uft_amiga_open_buffer(ctx, img, size, false, NULL) == 0);

    uft_amiga_dir_t dir;
    memset(&dir, 0, sizeof(dir));
    ASSERT(uft_amiga_load_dir_path(ctx, "", &dir) == 0);

    /* Kein Bit verloren: der Eintrag ist da. */
    ASSERT(dir.count == 2);

    /* Keine erfundenen Daten: er ist gekennzeichnet. */
    int schlechte = 0, gute = 0;
    for (size_t i = 0; i < dir.count; i++) {
        if (dir.entries[i].checksum_ok) gute++; else schlechte++;
    }
    if (schlechte != 1) {
        printf("  (gekennzeichnet: %d von 2, erwartet 1)\n", schlechte);
    }
    ASSERT(schlechte == 1);
    ASSERT(gute == 1);
    ASSERT(dir.bad_checksums == 1);

    uft_amiga_free_dir(&dir);
    uft_amiga_close(ctx);
    uft_amiga_destroy(ctx);
    free(img);
}

/* Der Schalter muss WIRKEN — in beide Richtungen. Ist er aus, wird nicht
 * geprueft, und dann darf auch nichts gekennzeichnet sein: eine
 * Kennzeichnung ohne Pruefung waere eine erfundene Aussage. */
TEST(abgeschalteter_schalter_kennzeichnet_nichts)
{
    size_t size = 0;
    uint8_t *img = baue_abbild(1, &size);
    ASSERT(img != NULL);

    uft_amiga_ctx_t *ctx = uft_amiga_create();
    ASSERT(ctx != NULL);

    uft_amiga_options_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.verify_checksums = false;
    ASSERT(uft_amiga_open_buffer(ctx, img, size, false, &opts) == 0);

    uft_amiga_dir_t dir;
    memset(&dir, 0, sizeof(dir));
    ASSERT(uft_amiga_load_dir_path(ctx, "", &dir) == 0);
    ASSERT(dir.count == 2);
    ASSERT(dir.bad_checksums == 0);

    uft_amiga_free_dir(&dir);
    uft_amiga_close(ctx);
    uft_amiga_destroy(ctx);
    free(img);
}

int main(void)
{
    printf("=== AmigaDOS: die Pruefsumme wird geprueft (MF-934) ===\n");
    RUN(der_pruefstand_baut_was_er_behauptet);
    RUN(unversehrte_eintraege_gelten);
    RUN(beschaedigter_block_wird_gekennzeichnet_nicht_verschwiegen);
    RUN(abgeschalteter_schalter_kennzeichnet_nichts);
    printf("\n%d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
