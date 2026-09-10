/**
 * @file test_apridisk_schreibt_in_die_datei.c
 * @brief `apridisk_write_track()` muss die Datei erreichen — und die
 *        Datei muss MAMEs Satzaufbau tragen (MF-1009).
 *
 * ── Woher die Frage kommt ────────────────────────────────────────────
 *
 * **MF-930** fand elf Plugins mit einem vollstaendigen, aber
 * unerreichbaren Dateischreiber. `apridisk` war darunter **der
 * schaerfste Fall**: der Rueckweg stand woertlich im Quelltext —
 * „Call flush/close to persist changes" — und es gab ihn nicht.
 *
 * `apridisk` ist der **vierte**, der verdrahtet wird (nach `opus`
 * MF-931, `cfi` MF-1004, `mgt` MF-1006). Verzeichnet als **P3-204**.
 *
 * ── Und der erste, bei dem der Schreiber erst REPARIERT werden musste ─
 *
 * MF-1009 hat gemessen, dass Leser UND Schreiber dasselbe **erfundene**
 * Satzformat benutzten:
 *
 *   - Typkonstanten ohne die obere Haelfte `0xE31D`, und SEKTOR und
 *     KOMMENTAR zusaetzlich vertauscht
 *   - `compression`/`header_size` als 32 Bit statt 16
 *   - ein Sektor-Deskriptor HINTER dem Kopf, `header_size` = 16 + 8
 *
 * Eine so erzeugte Datei war von keinem Werkzeug lesbar, **auch nicht
 * von UFT selbst**. Ein Rundlauf haette sie trotzdem bestanden: falsch
 * geschrieben, falsch gelesen, gleiches Ergebnis. Genau die
 * Selbstbestaetigung aus MF-992.
 *
 * **Deshalb prueft Fall 5 die ROHEN Bytes** des ersten Satzkopfs gegen
 * MAMEs Feldlagen (`apridisk.cpp:load()`). Das ist der Teil, den ein
 * Rundlauf durch den eigenen Leser nicht leisten kann.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "uft/formats/uft_apridisk.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

static int gruen = 0;
static int rot = 0;

static void pruefe(const char *name, int bedingung, const char *hinweis)
{
    if (bedingung) {
        printf("  [OK]   %s\n", name);
        gruen++;
    } else {
        printf("  [ROT]  %s%s%s\n", name, hinweis ? "  -- " : "",
               hinweis ? hinweis : "");
        rot++;
    }
}

extern const uft_format_plugin_t uft_format_plugin_apridisk;

/* Werte aus dem Orakel, hier eigenstaendig ausgeschrieben (MF-913: ein
 * Test darf nicht dieselbe Konstante pruefen, die er belegt). */
#define O_TYP_SEKTOR     0xE31D0001u
#define O_ROH            0x9E90u
#define O_SEKTORGROESSE  512
#define O_KOPFGROESSE    128

#define SPUREN     2
#define SEKTOREN   9
#define NEUER_WERT 0x3C
#define PRUEF_ZYL  1
#define PRUEF_SEK  4          /* 1-basiert */

static void le16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)(v >> 8);
}

static void le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);  p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF); p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static uint32_t le32_lies(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t le16_lies(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint8_t marke(int spur, int sektor)
{
    return (uint8_t)(0x40 + spur * 16 + sektor);
}

/** Baut eine echte APRIDISK-Datei nach MAMEs load(). */
static int baue(const char *pfad)
{
    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;

    uint8_t kopf[O_KOPFGROESSE];
    memset(kopf, 0, sizeof(kopf));
    memcpy(kopf, "ACT Apricot disk image\032\004", 24);
    int ok = (fwrite(kopf, 1, sizeof(kopf), f) == sizeof(kopf));

    uint8_t sk[16];
    uint8_t daten[O_SEKTORGROESSE];
    for (int spur = 0; spur < SPUREN && ok; spur++) {
        for (int s = 1; s <= SEKTOREN && ok; s++) {
            memset(sk, 0, sizeof(sk));
            le32(sk + 0, O_TYP_SEKTOR);
            le16(sk + 4, O_ROH);
            le16(sk + 6, 16);
            le32(sk + 8, O_SEKTORGROESSE);
            sk[12] = 0;                       /* Kopf   */
            sk[13] = (uint8_t)s;              /* Sektor */
            le16(sk + 14, (uint16_t)spur);    /* Spur   */
            ok = (fwrite(sk, 1, sizeof(sk), f) == sizeof(sk));
            if (!ok) break;
            memset(daten, marke(spur, s), sizeof(daten));
            ok = (fwrite(daten, 1, sizeof(daten), f) == sizeof(daten));
        }
    }
    ok = (fclose(f) == 0) && ok;
    return ok;
}

static void setze_pfad(uft_disk_t *d, const char *pfad)
{
    snprintf(d->path_buf, sizeof(d->path_buf), "%s", pfad);
    d->path = d->path_buf;
}

static void spur_frei(uft_track_t *tr)
{
    if (!tr) return;
    for (uint8_t s = 0; s < tr->sector_count; s++)
        free(tr->sectors[s].data);
    free(tr->sectors);
    memset(tr, 0, sizeof(*tr));
}

int main(void)
{
    printf("=== apridisk_write_track erreicht die Datei (MF-1009) ===\n");

    char pfad[512];
    const char *tmp = getenv("TEMP");
    if (!tmp) tmp = getenv("TMPDIR");
    if (!tmp) tmp = ".";
    snprintf(pfad, sizeof(pfad), "%s/uft_mf1009_apridisk.dsk", tmp);

    if (!baue(pfad)) {
        printf("  [ROT]  Pruefdatei liess sich nicht schreiben\n");
        return 1;
    }

    pruefe("das Plugin hat ein write_track",
           uft_format_plugin_apridisk.write_track != NULL, NULL);
    if (!uft_format_plugin_apridisk.write_track) {
        remove(pfad);
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    setze_pfad(&disk, pfad);

    uft_error_t rc = uft_format_plugin_apridisk.open(&disk, pfad, false);
    pruefe("die Pruefdatei laesst sich oeffnen", rc == UFT_OK, NULL);
    if (rc != UFT_OK) {
        remove(pfad);
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    rc = uft_format_plugin_apridisk.read_track(&disk, PRUEF_ZYL, 0, &t);
    if (rc == UFT_OK && t.sector_count >= PRUEF_SEK
        && t.sectors[PRUEF_SEK - 1].data) {
        memset(t.sectors[PRUEF_SEK - 1].data, NEUER_WERT,
               t.sectors[PRUEF_SEK - 1].data_size);
    }

    uft_error_t wrc =
        uft_format_plugin_apridisk.write_track(&disk, PRUEF_ZYL, 0, &t);
    char h[200];
    snprintf(h, sizeof(h),
             "write_track meldete %d -- vor MF-1009 stand hier "
             "UFT_ERROR_NOT_SUPPORTED, davor UFT_OK ohne Tat", (int)wrc);
    pruefe("write_track meldet Erfolg", wrc == UFT_OK, h);

    spur_frei(&t);
    uft_format_plugin_apridisk.close(&disk);

    /* ── zuruecklesen ─────────────────────────────────────────────── */
    uft_disk_t d2;
    memset(&d2, 0, sizeof(d2));
    setze_pfad(&d2, pfad);
    rc = uft_format_plugin_apridisk.open(&d2, pfad, true);
    pruefe("die geschriebene Datei laesst sich neu oeffnen", rc == UFT_OK,
           "der Schreiber hat die Datei unbrauchbar gemacht");

    if (rc == UFT_OK) {
        uft_track_t t2;
        memset(&t2, 0, sizeof(t2));
        uft_error_t r2 =
            uft_format_plugin_apridisk.read_track(&d2, PRUEF_ZYL, 0, &t2);
        int traegt = (r2 == UFT_OK && t2.sector_count >= PRUEF_SEK
                      && t2.sectors[PRUEF_SEK - 1].data
                      && t2.sectors[PRUEF_SEK - 1].data[0] == NEUER_WERT);
        snprintf(h, sizeof(h), "Sektor (%d,0,%d) traegt 0x%02X statt 0x%02X",
                 PRUEF_ZYL, PRUEF_SEK,
                 (r2 == UFT_OK && t2.sector_count >= PRUEF_SEK
                  && t2.sectors[PRUEF_SEK - 1].data)
                     ? (unsigned)t2.sectors[PRUEF_SEK - 1].data[0] : 0u,
                 (unsigned)NEUER_WERT);
        pruefe("die Aenderung steht in der DATEI", traegt, h);

        /* Gegenprobe: ein Nachbarsektor derselben Spur ist heil. */
        int nachbar = (r2 == UFT_OK && t2.sector_count >= 1
                       && t2.sectors[0].data
                       && t2.sectors[0].data[0] == marke(PRUEF_ZYL, 1));
        pruefe("der Nachbarsektor derselben Spur ist heil", nachbar,
               "der Schreibindex trifft den falschen Sektor");
        spur_frei(&t2);
        uft_format_plugin_apridisk.close(&d2);
    }

    /* ── die ROHE Datei nach MAMEs Satzlauf abgehen ───────────────── */
    {
        /* MAMEs `load()`: `file_offset = APR_HEADER_SIZE;` dann
         * `while (file_offset < file_size)` — Kopf lesen,
         * `file_offset += header_size`, Daten, `file_offset +=
         * data_size`. Genau das wird hier nachgefahren. Es prueft die
         * SCHRITTWEITEN mit: liegt `header_size` oder `data_size`
         * falsch, laeuft der Lauf aus dem Tritt und findet den
         * geaenderten Sektor nicht mehr.
         *
         * Der erste Satz ist ein ERZEUGER-Satz (0xE31D0003) — das
         * schreibt der Schreiber zu Recht, und MAMEs Leser
         * ueberspringt unbekannte Typen. Meine erste Fassung nahm den
         * ersten Satz fuer einen Sektor und war deshalb rot; der Test
         * war zu streng, nicht der Code falsch. */
        uint8_t *inhalt = NULL;
        long n = 0;
        FILE *f = fopen(pfad, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            n = ftell(f);
            fseek(f, 0, SEEK_SET);
            inhalt = (uint8_t *)malloc((size_t)(n > 0 ? n : 1));
            if (inhalt && fread(inhalt, 1, (size_t)n, f) != (size_t)n) {
                free(inhalt);
                inhalt = NULL;
            }
            fclose(f);
        }

        int sektoren = 0, erzeuger = 0, gefunden = 0, aufbau_ok = 1;
        char wo[110] = "";
        if (inhalt && n > O_KOPFGROESSE) {
            long pos = O_KOPFGROESSE;
            while (pos + 16 <= n) {
                const uint8_t *sk = inhalt + pos;
                uint32_t typ = le32_lies(sk + 0);
                uint16_t komp = le16_lies(sk + 4);
                uint16_t kg   = le16_lies(sk + 6);
                uint32_t dg   = le32_lies(sk + 8);
                uint8_t  kopf = sk[12];
                uint8_t  sekt = sk[13];
                uint16_t spur = le16_lies(sk + 14);

                if (kg != 16) {
                    aufbau_ok = 0;
                    snprintf(wo, sizeof(wo),
                             "Kopfgroesse %u bei Versatz %ld, erwartet 16",
                             (unsigned)kg, pos);
                    break;
                }
                pos += kg;

                if (typ == O_TYP_SEKTOR) {
                    sektoren++;
                    if (komp != O_ROH && komp != 0x3E5Au) {
                        aufbau_ok = 0;
                        snprintf(wo, sizeof(wo),
                                 "Kompression 0x%04X bei Spur %u Sektor %u",
                                 (unsigned)komp, (unsigned)spur,
                                 (unsigned)sekt);
                        break;
                    }
                    if (spur == PRUEF_ZYL && kopf == 0 && sekt == PRUEF_SEK
                        && pos + (long)dg <= n) {
                        /* Beide Wege sind richtig, und welcher genommen
                         * wird, ist bestimmt: der Zielsektor ist mit
                         * EINEM Byte gefuellt, also packt ihn
                         * `apridisk_make_fill()` zu drei Byte. Meine
                         * erste Fassung verlangte „roh" und war deshalb
                         * rot — der Schreiber hatte recht, die Zusage
                         * nicht. Geprueft wird jetzt beides, womit der
                         * Packpfad mitbelegt ist. */
                        if (komp == 0x3E5Au) {
                            gefunden = (dg == 3
                                        && le16_lies(inhalt + pos)
                                           == O_SEKTORGROESSE
                                        && inhalt[pos + 2] == NEUER_WERT);
                        } else if (komp == O_ROH) {
                            gefunden = (dg == O_SEKTORGROESSE
                                        && inhalt[pos] == NEUER_WERT);
                        }
                        if (!gefunden)
                            snprintf(wo, sizeof(wo),
                                     "Zielsektor: Komp=0x%04X dg=%u "
                                     "Byte0=0x%02X Byte2=0x%02X",
                                     (unsigned)komp, (unsigned)dg,
                                     (unsigned)inhalt[pos],
                                     (pos + 2 < n)
                                         ? (unsigned)inhalt[pos + 2] : 0u);
                    }
                } else if (typ == 0xE31D0003u) {
                    erzeuger++;
                }

                pos += dg;
            }
        }
        free(inhalt);

        snprintf(h, sizeof(h),
                 "%d Sektor- und %d Erzeuger-Saetze gelaufen%s%s",
                 sektoren, erzeuger, wo[0] ? "; " : "", wo);
        pruefe("die erzeugte Datei laesst sich nach MAMEs Satzlauf abgehen",
               aufbau_ok && sektoren == SPUREN * SEKTOREN, h);
        pruefe("und der geaenderte Sektor steht dort in der Datei "
               "(roh oder als Fuelllauf)",
               gefunden, wo[0] ? wo : "Zielsektor nicht gefunden");
    }

    remove(pfad);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot == 0 ? 0 : 1;
}
