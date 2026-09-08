/* SPDX-License-Identifier: MIT */
/**
 * @file test_dmk_rx02_absage.c
 * @brief DMK mit RX02-Bit wird ABGELEHNT, nicht falsch gelesen (MF-972)
 *
 * ── WORUM ES GEHT ────────────────────────────────────────────────────
 *
 * Bit 5 des DMK-Optionsbytes (Kopf-Byte 4) ist eine von Tim Mann
 * definierte Erweiterung. `dmk.h` aus `dmklib 0.2`:
 *
 *     #define DMK_FLAG_SD_BIT     6
 *     #define DMK_FLAG_RX02_BIT   5   / * extension defined by Tim Mann * /
 *     #define DMK_FLAG_SS_BIT     4
 *
 * Sie sagt: die Spuren tragen DEC-RX02 — einen FM-ADRESSKOPF mit
 * M2FM-DATEN.
 *
 * `src/formats/dmk/uft_dmk.c` liest aus diesem Byte NUR Bit 4
 * (einseitig). Das ist fuer die Dichtebits 0x40/0x80 folgenlos, weil
 * der Leser MFM inhaltsbasiert erkennt — an den drei echten
 * `0xA1`-Sync-Bytes vor der Adressmarke.
 *
 * RX02 unterlaeuft genau das: sein Adresskopf IST FM, hat also kein
 * A1. Der Leser haelt die Spur fuer FM, findet die Adressmarke `0xFE`
 * und dekodiert die anschliessenden M2FM-Daten als FM. Ergebnis: Muell
 * mit `UFT_OK`.
 *
 * ── WARUM ABSAGE UND KEIN DEKODER ────────────────────────────────────
 *
 * Ein M2FM-Dekoder waere neuer Decoder-Code und faellt unter die
 * EINFRIER-REGEL (MF-363/498). Die Absage ist die ehrliche Antwort:
 * „erkannt, nicht lesbar" statt stiller Falschdaten. `close()` waere
 * hier der falsche Ort — `open()` hat einen Rueckgabewert.
 *
 * ── WAS DIESER TEST NICHT KANN ───────────────────────────────────────
 *
 * Er baut keine echte RX02-Spur — dafuer braeuchte es einen
 * M2FM-Kodierer, den dieser Baum nicht hat. Er prueft, dass das FLAG
 * beachtet wird, nicht dass RX02-Daten richtig gelesen wuerden. Das
 * waere auch die falsche Zusicherung: sie werden gar nicht gelesen.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_dmk;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-40s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                    _fail++; return; } } while (0)

#define DMK_HDR      16
#define SPUREN       2
#define SPURLAENGE   3200

static void temp_pfad(char *p, size_t n)
{
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(p, n, "%s/uft_dmk_rx02_%d.dmk", d, rand() % 100000);
}

/* Minimales, wohlgeformtes DMK. `optionen` ist Kopf-Byte 4. */
static int baue_dmk(const char *pfad, uint8_t optionen)
{
    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;

    uint8_t kopf[DMK_HDR];
    memset(kopf, 0, sizeof(kopf));
    kopf[0] = 0x00;                      /* nicht schreibgeschuetzt */
    kopf[1] = SPUREN;
    kopf[2] = (uint8_t)(SPURLAENGE & 0xFF);
    kopf[3] = (uint8_t)(SPURLAENGE >> 8);
    kopf[4] = optionen;
    /* Bytes 5..11 bleiben null — der Leser vergibt dafuer Konfidenz. */
    fwrite(kopf, 1, sizeof(kopf), f);

    /* Die Spuren selbst: leere IDAM-Tabelle, Rest Fuellbytes. Der
     * Leser findet darin keine Sektoren, aber die Datei ist gueltig
     * und gross genug fuer seine eigene Groessenrechnung. */
    const int seiten = (optionen & 0x10) ? 1 : 2;
    uint8_t *spur = calloc(1, SPURLAENGE);
    if (!spur) { fclose(f); return 0; }
    memset(spur + 128, 0x4E, SPURLAENGE - 128);   /* Luecken */
    for (int t = 0; t < SPUREN * seiten; t++)
        fwrite(spur, 1, SPURLAENGE, f);
    free(spur);

    fclose(f);
    return 1;
}

/* Die Zeile, die vor MF-972 faellt. */
TEST(rx02_bit_wird_abgelehnt)
{
    char pfad[400];
    temp_pfad(pfad, sizeof(pfad));
    /* 0x10 = einseitig, 0x20 = RX02 */
    ASSERT(baue_dmk(pfad, 0x10 | 0x20));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    uft_error_t rc = uft_format_plugin_dmk.open(&disk, pfad, true);

    if (rc == UFT_OK) {
        printf("\n       geoeffnet statt abgelehnt — die M2FM-Daten"
               " wuerden als FM gelesen\n");
        uft_format_plugin_dmk.close(&disk);
    }
    remove(pfad);
    ASSERT(rc != UFT_OK);
}

/* Und die Gegenprobe: OHNE das Bit muss dieselbe Datei aufgehen.
 * Ohne diesen Fall kaeme eine Absage durch, die JEDE DMK ablehnt. */
TEST(ohne_rx02_bit_wird_geoeffnet)
{
    char pfad[400];
    temp_pfad(pfad, sizeof(pfad));
    ASSERT(baue_dmk(pfad, 0x10));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    uft_error_t rc = uft_format_plugin_dmk.open(&disk, pfad, true);
    if (rc == UFT_OK) uft_format_plugin_dmk.close(&disk);
    remove(pfad);

    ASSERT(rc == UFT_OK);
}

/* Die uebrigen Dichtebits duerfen NICHT mit abgelehnt werden — sie
 * sind fuer diesen Leser folgenlos, weil er MFM an den A1-Sync-Bytes
 * erkennt. Eine zu breite Absage waere ein neuer Fehler. */
TEST(dichtebits_bleiben_folgenlos)
{
    char pfad[400];
    temp_pfad(pfad, sizeof(pfad));
    /* 0x40 = Single Density, 0x80 = Dichtebit ignorieren */
    ASSERT(baue_dmk(pfad, 0x10 | 0x40 | 0x80));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    uft_error_t rc = uft_format_plugin_dmk.open(&disk, pfad, true);
    if (rc == UFT_OK) uft_format_plugin_dmk.close(&disk);
    remove(pfad);

    ASSERT(rc == UFT_OK);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== DMK: RX02-Bit wird abgelehnt (MF-972) ===\n");
    RUN(rx02_bit_wird_abgelehnt);
    RUN(ohne_rx02_bit_wird_geoeffnet);
    RUN(dichtebits_bleiben_folgenlos);
    printf("\n%d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
