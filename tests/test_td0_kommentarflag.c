/* SPDX-License-Identifier: MIT */
/**
 * @file test_td0_kommentarflag.c
 * @brief TD0: der Kommentarblock haengt am FLAG, nicht an der Version (MF-971)
 *
 * ── WORUM ES GEHT ────────────────────────────────────────────────────
 *
 * Der TeleDisk-Kopf ist 12 Byte lang. Byte 7 (`bTrackDensity`) traegt in
 * **Bit 7** die Angabe, ob ein Kommentarblock folgt. Vier voneinander
 * unabhaengige Quellen sagen dasselbe:
 *
 *   1. `src/samdisk/td0.cpp:28` — und das ist die Referenz, mit der
 *      `docs/VERIFICATION_TIERS.md` TD0 auf **T2** fuehrt:
 *
 *          // Optional comment block, present if bit 7 is set in
 *          // bTrackDensity above
 *
 *      `td0.cpp:208`: `if (th.bTrackDensity & 0x80)`
 *
 *   2. `src/formats/td0/uft_td0_lzss.c:469` — der SCHWESTERLESER im
 *      selben Verzeichnis: `if (img->header.stepping & 0x80)`
 *
 *   3. `libdisk/teledisk.c:341` (KCemu 0.5.1): dieselbe Pruefung
 *
 *   4. Die Kopfbelegung selbst: Byte 7 ist `bTrackDensity`, und die
 *      Werte 0/1/2 belegen nur die unteren Bits.
 *
 * Das REGISTRIERTE Plugin `src/formats/td0/uft_td0.c` prueft stattdessen
 * `pdata->version >= 0x10` — einen Versionsschwellwert. Es liest
 * `header[7]` gar nicht.
 *
 * ── WAS DAS ANRICHTET ────────────────────────────────────────────────
 *
 * Die Version steht in Byte 4 und ist bei TeleDisk 1.1+ regelmaessig
 * >= 0x10. Eine solche Datei OHNE Kommentar laesst das Plugin
 * trotzdem 10 Byte lesen und danach `com_len` Byte ueberspringen —
 * wobei `com_len` aus dem gelesen wird, was tatsaechlich dort steht:
 * SPURDATEN. `data_start` landet an einer beliebigen Stelle, und die
 * Geometrie-Abtastung laeuft ins Leere.
 *
 * Die Gegenrichtung ist genauso falsch: eine Datei mit Version < 0x10
 * UND gesetztem Kommentarbit wird nicht uebersprungen — dann beginnt
 * die Spursuche im Kommentarkopf.
 *
 * ── WAS HIER GEBAUT WIRD ─────────────────────────────────────────────
 *
 * Minimale, UNKOMPRIMIERTE TD0-Dateien ("TD"-Kennung). Zwei Spuren zu
 * neun Sektoren, Sektorflags `0x10` (kein Datenblock), Abschluss
 * `0xFF`. Erwartet: 2 Zylinder, 1 Kopf, 9 Sektoren.
 *
 * Die Wahrheit des Tests ist nicht der Inhalt, sondern die GEOMETRIE:
 * sie wurde hineingeschrieben und muss zurueckkommen.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_td0;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                    _fail++; return; } } while (0)

#define SPUREN    2
#define SEKTOREN  9

static void temp_pfad(char *p, size_t n)
{
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(p, n, "%s/uft_td0_flag_%d.td0", d, rand() % 100000);
}

/* Baut eine TD0.
 *
 * @param version         Byte 4 des Kopfes
 * @param trackdensity    Byte 7 — Bit 7 ist das Kommentarflag
 * @param mit_kommentar   schreibt tatsaechlich einen Kommentarblock
 */
static int baue_td0(const char *pfad, uint8_t version, uint8_t trackdensity,
                    int mit_kommentar)
{
    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;

    uint8_t kopf[12];
    memset(kopf, 0, sizeof(kopf));
    kopf[0] = 'T'; kopf[1] = 'D';   /* unkomprimiert */
    kopf[2] = 0;                    /* Datentraegerfolge */
    kopf[3] = 0;                    /* Pruefkennung     */
    kopf[4] = version;
    kopf[5] = 0;                    /* Quelldichte      */
    kopf[6] = 3;                    /* Laufwerkstyp 720K */
    kopf[7] = trackdensity;
    kopf[8] = 0;                    /* DOS-Modus        */
    kopf[9] = 1;                    /* Seiten           */
    fwrite(kopf, 1, sizeof(kopf), f);

    if (mit_kommentar) {
        /* CRC(2) + Laenge(2) + Datum(6) = 10 Byte Kopf, dann der Text. */
        const char *text = "UFT Pruefkommentar";
        uint16_t len = (uint16_t)strlen(text);
        uint8_t ck[10];
        memset(ck, 0, sizeof(ck));
        ck[2] = (uint8_t)(len & 0xFF);
        ck[3] = (uint8_t)(len >> 8);
        fwrite(ck, 1, sizeof(ck), f);
        fwrite(text, 1, len, f);
    }

    for (uint8_t c = 0; c < SPUREN; c++) {
        uint8_t th[4] = { SEKTOREN, c, 0, 0 };
        fwrite(th, 1, sizeof(th), f);
        for (uint8_t s = 0; s < SEKTOREN; s++) {
            /* Flags 0x10: der Sektor traegt keinen Datenblock. Damit
             * bleibt die Datei klein und die Abtastung eindeutig. */
            uint8_t sh[6] = { c, 0, (uint8_t)(s + 1), 2, 0x10, 0 };
            fwrite(sh, 1, sizeof(sh), f);
        }
    }
    uint8_t ende[4] = { 0xFF, 0, 0, 0 };
    fwrite(ende, 1, sizeof(ende), f);

    fclose(f);
    return 1;
}

static void pruefe_geometrie(const char *pfad, const char *fall)
{
    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    uft_error_t rc = uft_format_plugin_td0.open(&disk, pfad, true);
    if (rc != UFT_OK) {
        printf("\n       [%s] open() = %d\n", fall, (int)rc);
        _fail++;
        return;
    }
    if (disk.geometry.cylinders != SPUREN ||
        disk.geometry.sectors != SEKTOREN) {
        printf("\n       [%s] %u Zylinder / %u Sektoren, erwartet %d / %d\n",
               fall, (unsigned)disk.geometry.cylinders,
               (unsigned)disk.geometry.sectors, SPUREN, SEKTOREN);
        _fail++;
    }
    uft_format_plugin_td0.close(&disk);
}

/* ─────────────────────────────────────────────────────────────────────
 *  Version >= 0x10, KEIN Kommentarflag — der haeufige Fall.
 * ───────────────────────────────────────────────────────────────────── */
TEST(neue_version_ohne_kommentar_wird_nicht_uebersprungen)
{
    char pfad[400];
    temp_pfad(pfad, sizeof(pfad));
    /* Version 0x15 (TeleDisk 2.1), Spurdichte 0 -> Bit 7 GELOESCHT. */
    ASSERT(baue_td0(pfad, 0x15, 0x00, /*mit_kommentar*/0));
    pruefe_geometrie(pfad, "v>=0x10 ohne Kommentar");
    remove(pfad);
}

/* Kommentarflag gesetzt UND Kommentar vorhanden -> muss uebersprungen
 * werden. Ohne diesen Fall koennte ein "Fix" durchkommen, der den
 * Kommentar NIE ueberspringt. */
TEST(gesetztes_flag_ueberspringt_den_kommentar)
{
    char pfad[400];
    temp_pfad(pfad, sizeof(pfad));
    ASSERT(baue_td0(pfad, 0x15, 0x80, /*mit_kommentar*/1));
    pruefe_geometrie(pfad, "Flag gesetzt, Kommentar da");
    remove(pfad);
}

/* Alte Version MIT Kommentarflag. Der Versionsschwellwert wuerde hier
 * NICHT ueberspringen — das Flag verlangt es aber. */
TEST(alte_version_mit_flag_ueberspringt_trotzdem)
{
    char pfad[400];
    temp_pfad(pfad, sizeof(pfad));
    ASSERT(baue_td0(pfad, 0x09, 0x80, /*mit_kommentar*/1));
    pruefe_geometrie(pfad, "v<0x10 mit Flag");
    remove(pfad);
}

/* Und die untere Gegenprobe: alte Version, kein Flag, kein Kommentar. */
TEST(alte_version_ohne_flag_bleibt_unveraendert)
{
    char pfad[400];
    temp_pfad(pfad, sizeof(pfad));
    ASSERT(baue_td0(pfad, 0x09, 0x00, /*mit_kommentar*/0));
    pruefe_geometrie(pfad, "v<0x10 ohne Flag");
    remove(pfad);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== TD0: Kommentarflag statt Versionsschwelle (MF-971) ===\n");
    RUN(neue_version_ohne_kommentar_wird_nicht_uebersprungen);
    RUN(gesetztes_flag_ueberspringt_den_kommentar);
    RUN(alte_version_mit_flag_ueberspringt_trotzdem);
    RUN(alte_version_ohne_flag_bleibt_unveraendert);
    printf("\n%d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
