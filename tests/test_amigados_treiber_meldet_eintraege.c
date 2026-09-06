/* SPDX-License-Identifier: MIT */
/**
 * @file test_amigados_treiber_meldet_eintraege.c
 * @brief `readdir` las die Anzahl NACH dem Freigeben — immer 0 (MF-935)
 *
 * ── DER BEFUND ───────────────────────────────────────────────────────
 *
 * `src/fs/uft_fs_amigados_driver.c`, `amigados_readdir()`, schloss so:
 *
 *     uft_amiga_free_dir(&dir);
 *     *entries = out;
 *     *count   = dir.count;      <-- HIER
 *     return UFT_OK;
 *
 * `uft_amiga_free_dir()` setzt `dir->count = dir->capacity = 0`. Die
 * Anzahl wurde also GENULLT, bevor sie gelesen wurde.
 *
 * Folge: der Treiber gibt einen korrekt gefuellten Eintragsblock zurueck
 * UND meldet dazu die Anzahl **0** — mit `UFT_OK`. Wer darueber
 * iteriert, sieht ein LEERES Verzeichnis, egal was auf der Diskette
 * steht. Die Daten sind da, sie sind nur unerreichbar.
 *
 * Das ist derselbe Grundfehler wie MF-928 (G64 meldete Erfolg fuer die
 * halbe Diskette) und MF-930 (Schreiberfolg ohne Tat): eine
 * Erfolgsmeldung, hinter der die Arbeit fehlt.
 *
 * Was es besonders macht: `amigados_driver` ist der EINZIGE registrierte
 * `uft_fs_driver_t` im Baum (gemessen MF-935 ueber `git ls-files`), und
 * er registriert sich per `__attribute__((constructor))` selbst. Es gab
 * also keinen zweiten Treiber, an dem der Unterschied aufgefallen waere,
 * und keinen Test, der diesen Weg lief.
 *
 * ── ROTBEWEIS ────────────────────────────────────────────────────────
 *
 * Vor MF-935 meldet dieser Test 0 Eintraege fuer ein Abbild mit zweien.
 */

#include "uft/uft_integration.h"
#include "uft/uft_error.h"
#include "uft/uft_format_plugin.h"
#include "uft/fs/uft_amigados.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-44s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

#define BLK        512u
#define BLOCKS     256u
#define ROOT_BLK   (BLOCKS / 2)

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

static void setze_pruefsumme(uint8_t *block)
{
    put_be32(block + O_CHECKSUM, 0);
    uint32_t sum = 0;
    for (size_t i = 0; i < 128; i++) sum += get_be32(block + i * 4);
    put_be32(block + O_CHECKSUM, (uint32_t)(0u - sum));
}

/* Wurzel -> "EINS" -> "ZWEI". Bootblock mit gueltiger Pruefsumme, damit
 * `amigados_probe()` die hohe Konfidenz (90) vergibt und der Treiber
 * ueber `uft_fs_mount_auto()` wirklich gewaehlt wird. */
static uint8_t *baue_abbild(size_t *size_out)
{
    const size_t size = (size_t)BLOCKS * BLK;
    uint8_t *img = (uint8_t *)calloc(1, size);
    if (!img) return NULL;

    memcpy(img, "DOS", 3);
    img[3] = 0x00;

    /* Bootblock-Pruefsumme: 256 Langworte, End-Around-Carry,
     * Einerkomplement, Feld an Versatz 4 ausgelassen. Anderer
     * Algorithmus als die Blockpruefsumme — bestaetigt durch
     * Disassemblierung eines Sektor-Editors von 1986 (UFT-30,
     * ACS-UtilitiesCompilation4/4, Offset 0x4dec-0x4e42). */
    put_be32(img + 8, ROOT_BLK);
    {
        uint32_t sum = 0;
        for (int i = 0; i < 256; i++) {
            if (i == 1) continue;
            uint32_t v = get_be32(img + i * 4);
            uint32_t prev = sum;
            sum += v;
            if (sum < prev) sum++;
        }
        put_be32(img + 4, ~sum);
    }

    uint8_t *root = img + (size_t)ROOT_BLK * BLK;
    put_be32(root + O_TYPE, T_HEADER);
    put_be32(root + O_HEADER_KEY, 0);
    put_be32(root + O_HTAB_SIZE, 72);
    put_be32(root + O_SEC_TYPE, (uint32_t)ST_ROOT);
    put_bcpl(root + O_NAME, "TREIBERTEST");
    put_be32(root + O_HASHTABLE + 0 * 4, 100);
    setze_pruefsumme(root);

    uint8_t *a = img + 100u * BLK;
    put_be32(a + O_TYPE, T_HEADER);
    put_be32(a + O_HEADER_KEY, 100);
    put_be32(a + O_SEC_TYPE, (uint32_t)ST_FILE);
    put_be32(a + O_PARENT, ROOT_BLK);
    put_be32(a + O_HASH_CHAIN, 101);
    put_bcpl(a + O_NAME, "EINS");
    setze_pruefsumme(a);

    uint8_t *b = img + 101u * BLK;
    put_be32(b + O_TYPE, T_HEADER);
    put_be32(b + O_HEADER_KEY, 101);
    put_be32(b + O_SEC_TYPE, (uint32_t)ST_FILE);
    put_be32(b + O_PARENT, ROOT_BLK);
    put_be32(b + O_HASH_CHAIN, 0);
    put_bcpl(b + O_NAME, "ZWEI");
    setze_pruefsumme(b);

    *size_out = size;
    return img;
}

/* Der Pruefstand muss selbst stimmen: die FS-Ebene sieht zwei Eintraege.
 * Ohne diese Zusicherung koennte der Treibertest aus dem falschen Grund
 * rot sein (leeres Abbild statt kaputter Treiber). */
TEST(die_fs_ebene_sieht_zwei_eintraege)
{
    size_t size = 0;
    uint8_t *img = baue_abbild(&size);
    ASSERT(img != NULL);

    uft_amiga_ctx_t *ctx = uft_amiga_create();
    ASSERT(ctx != NULL);
    ASSERT(uft_amiga_open_buffer(ctx, img, size, false, NULL) == 0);

    uft_amiga_dir_t dir;
    memset(&dir, 0, sizeof(dir));
    ASSERT(uft_amiga_load_dir_path(ctx, "", &dir) == 0);
    ASSERT(dir.count == 2);

    uft_amiga_free_dir(&dir);
    uft_amiga_close(ctx);
    uft_amiga_destroy(ctx);
    free(img);
}

/* ─────────────────────────────────────────────────────────────────────
 *  DIE ZEILE: was die FS-Ebene sieht, muss der Treiber auch melden.
 * ───────────────────────────────────────────────────────────────────── */
TEST(der_treiber_meldet_dieselbe_anzahl)
{
    size_t size = 0;
    uint8_t *img = baue_abbild(&size);
    ASSERT(img != NULL);

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.image_data = img;
    disk.image_size = size;

    uft_filesystem_t *fs = NULL;
    const uft_fs_driver_t *drv = NULL;
    uft_error_t r = uft_fs_mount_auto(&disk, &fs, &drv);
    ASSERT(r == UFT_OK);
    ASSERT(fs != NULL);
    ASSERT(drv != NULL);
    ASSERT(drv->readdir != NULL);

    uft_dirent_t *eintraege = NULL;
    size_t anzahl = 12345;              /* erkennbarer Vorgabewert */
    ASSERT(drv->readdir(fs, "", &eintraege, &anzahl) == UFT_OK);

    if (anzahl != 2) {
        printf("  (Treiber meldet %u Eintraege, FS-Ebene sieht 2)\n",
               (unsigned)anzahl);
    }
    ASSERT(anzahl == 2);
    ASSERT(eintraege != NULL);

    /* Und die Namen kommen auch an — sonst waere die Anzahl richtig und
     * der Inhalt trotzdem leer. */
    int eins = 0, zwei = 0;
    for (size_t i = 0; i < anzahl; i++) {
        if (strcmp(eintraege[i].name, "EINS") == 0) eins++;
        if (strcmp(eintraege[i].name, "ZWEI") == 0) zwei++;
    }
    ASSERT(eins == 1);
    ASSERT(zwei == 1);

    free(eintraege);
    if (drv->unmount) drv->unmount(fs);
    free(img);
}

/* Ein leeres Verzeichnis muss weiterhin 0 melden — sonst waere der Fix
 * eine Zahl, die immer stimmt. */
TEST(leeres_verzeichnis_meldet_null)
{
    size_t size = 0;
    uint8_t *img = baue_abbild(&size);
    ASSERT(img != NULL);

    /* Hash-Slot 0 leeren und Wurzel-Pruefsumme nachziehen. */
    uint8_t *root = img + (size_t)ROOT_BLK * BLK;
    put_be32(root + O_HASHTABLE + 0 * 4, 0);
    setze_pruefsumme(root);

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.image_data = img;
    disk.image_size = size;

    uft_filesystem_t *fs = NULL;
    const uft_fs_driver_t *drv = NULL;
    ASSERT(uft_fs_mount_auto(&disk, &fs, &drv) == UFT_OK);

    uft_dirent_t *eintraege = NULL;
    size_t anzahl = 12345;
    ASSERT(drv->readdir(fs, "", &eintraege, &anzahl) == UFT_OK);
    ASSERT(anzahl == 0);

    free(eintraege);
    if (drv->unmount) drv->unmount(fs);
    free(img);
}

/* EIN Eintrag — und das ist der Fall, den die Mutationsmatrix erzwungen
 * hat: eine fest verdrahtete `*count = 2` blieb gruen, weil der
 * Leer-Fall im Treiber VOR der fraglichen Zeile zurueckkehrt
 * (`if (dir.count == 0) return UFT_OK;`) und `*count` dort auf seinem
 * Anfangswert 0 stehen bleibt. Zwei Punkte reichen nicht, um eine
 * Konstante von einer Messung zu unterscheiden — es braucht einen
 * dritten. */
TEST(ein_einziger_eintrag_meldet_eins)
{
    size_t size = 0;
    uint8_t *img = baue_abbild(&size);
    ASSERT(img != NULL);

    /* Die Kette nach "EINS" kappen und dessen Pruefsumme nachziehen. */
    uint8_t *a = img + 100u * BLK;
    put_be32(a + O_HASH_CHAIN, 0);
    setze_pruefsumme(a);

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.image_data = img;
    disk.image_size = size;

    uft_filesystem_t *fs = NULL;
    const uft_fs_driver_t *drv = NULL;
    ASSERT(uft_fs_mount_auto(&disk, &fs, &drv) == UFT_OK);

    uft_dirent_t *eintraege = NULL;
    size_t anzahl = 12345;
    ASSERT(drv->readdir(fs, "", &eintraege, &anzahl) == UFT_OK);
    ASSERT(anzahl == 1);
    ASSERT(eintraege != NULL);
    ASSERT(strcmp(eintraege[0].name, "EINS") == 0);

    free(eintraege);
    if (drv->unmount) drv->unmount(fs);
    free(img);
}

int main(void)
{
    printf("=== AmigaDOS-Treiber meldet seine Eintraege (MF-935) ===\n");
    RUN(die_fs_ebene_sieht_zwei_eintraege);
    RUN(der_treiber_meldet_dieselbe_anzahl);
    RUN(ein_einziger_eintrag_meldet_eins);
    RUN(leeres_verzeichnis_meldet_null);
    printf("\n%d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
