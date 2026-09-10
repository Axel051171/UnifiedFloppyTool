/**
 * @file test_vdk_gegen_mame.c
 * @brief VDK gegen MAMEs `vdk_dsk.cpp` (MF-1018).
 *
 * ── Woher die Frage kommt ────────────────────────────────────────────
 *
 * `vdk` stand auf **T3**. Drei Befunde, alle gemessen an Pruefdateien,
 * deren Spuren und Sektoren sich selbst benennen (Fuellbyte = 0x40 +
 * Spur, Byte 1 = Sektornummer, Byte 2 = Kopf).
 *
 * **Befund 1 — die Kopfgroesse lag in einem `uint8_t`.**
 *
 *     typedef struct { FILE *file; uint8_t hdr_size, tracks, sides; }
 *     p->hdr_size = uft_read_le16(hdr + 2);
 *
 * Bei einem Kopf von exakt 0x0100 Byte wird daraus 0. Gemessen:
 *
 *     Sektor-ID 1, Byte 0 = 64, Byte 1 = 6B
 *     Orakel   : ID 1, Byte 0 = 40, Byte 1 = 01
 *
 * `64 6B` ist `'d' 'k'` — **die eigene Kennung der Datei wurde als
 * Sektordaten ausgegeben.**
 *
 * **Befund 2 — eine Geometrie, die die Datei verneint.** Stand im Kopf
 * eine 0, machte der Leser daraus 35 Zylinder. MAME nimmt `header[8]`
 * und `header[9]` woertlich.
 *
 * **Befund 3 — das Kompressionsbyte wurde ignoriert.** `hdr[11]` sagt,
 * ob die Sektordaten gepackt sind; gesetzt las UFT sie trotzdem als
 * Sektorabzug und meldete Erfolg. **Hier ist das Orakel selbst stumm**
 * — MAMEs `load()` prueft das Byte nicht. Abgesagt wird es hier
 * trotzdem, weil gepackte Daten als Sektoren gelesen erfundene Daten
 * sind.
 *
 * ── Das Orakel ───────────────────────────────────────────────────────
 *
 * **MAME `formats/vdk_dsk.cpp`** (BSD-3-Clause, Dirk Best), in
 * `neue-ideen/formats.zip`, woertlich:
 *
 *     uint8_t header[0x100];
 *     read(io, header, 0x100);
 *     int const header_size = header[3] * 0x100 + header[2];
 *     int const track_count = header[8];
 *     int const head_count  = header[9];
 *     io.seek(header_size, SEEK_SET);
 *     ... SECTOR_COUNT = 18, SECTOR_SIZE = 256, FIRST_SECTOR_ID = 1
 *
 * MAME liest **0x100 Byte** Kopf, bevor es die Kopfgroesse benutzt —
 * ein Kopf bis 256 Byte ist im Format also vorgesehen.
 *
 * ── Und was gemessen wurde und STIMMT ────────────────────────────────
 *
 * Die Sektor-IDs: `uft_format_add_sector()` addiert 1 zu `s = 0..17`
 * und ergibt **1..18**, genau MAMEs `FIRST_SECTOR_ID = 1`. Anders als
 * bei `jv1` (MF-1016) ist das hier richtig — deshalb wird es geprueft
 * und **nicht** angefasst.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

extern const uft_format_plugin_t uft_format_plugin_vdk;

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

#define SPT 18
#define SS  256

/* Baut eine VDK nach MAMEs Kopfaufbau. Spur t ist mit 0x40+t gefuellt,
 * Byte 1 traegt die Sektornummer (1-basiert), Byte 2 den Kopf. */
static int baue_vdk(const char *pfad, size_t kopf, uint8_t tracks,
                    uint8_t sides, uint8_t kompression, uint8_t flags)
{
    size_t daten = (size_t)tracks * sides * SPT * SS;
    uint8_t *b = (uint8_t *)calloc(1, kopf + daten + 1);
    if (!b) return 0;
    b[0] = 'd'; b[1] = 'k';
    b[2] = (uint8_t)(kopf & 0xFF);
    b[3] = (uint8_t)(kopf >> 8);
    b[4] = 0x10; b[5] = 0x10; b[6] = 'M'; b[7] = 0x01;
    b[8] = tracks; b[9] = sides; b[10] = flags; b[11] = kompression;
    /* Der Kopf jenseits von 12 Byte ist Fuellwerk — 0x99, damit man es
     * im Sektorinhalt erkennt, falls es dort auftaucht. */
    for (size_t i = 12; i < kopf; i++) b[i] = 0x99;

    for (int t = 0; t < tracks; t++)
        for (int h = 0; h < sides; h++)
            for (int s = 0; s < SPT; s++) {
                uint8_t *sek = b + kopf
                             + ((size_t)t * sides + h) * SPT * SS
                             + (size_t)s * SS;
                memset(sek, (uint8_t)(0x40 + t), SS);
                sek[1] = (uint8_t)(s + 1);
                sek[2] = (uint8_t)h;
            }
    FILE *f = fopen(pfad, "wb");
    if (!f) { free(b); return 0; }
    int ok = (fwrite(b, 1, kopf + daten, f) == kopf + daten);
    fclose(f);
    free(b);
    return ok;
}

static void frei(uft_track_t *t)
{
    if (!t) return;
    free(t->sectors);
    memset(t, 0, sizeof(*t));
}

int main(void)
{
    printf("=== VDK gegen MAMEs vdk_dsk.cpp (MF-1018) ===\n");

    char pfad[512];
    const char *tmp = getenv("TEMP");
    if (!tmp) tmp = getenv("TMPDIR");
    if (!tmp) tmp = ".";
    snprintf(pfad, sizeof(pfad), "%s/uft_mf1018.vdk", tmp);

    char h[240];

    /* ── Der Normalfall: 12-Byte-Kopf ────────────────────────────────── */
    {
        if (!baue_vdk(pfad, 12, 40, 1, 0, 0)) {
            printf("  [ROT]  Pruefdatei liess sich nicht schreiben\n");
            return 1;
        }
        uft_disk_t d;
        memset(&d, 0, sizeof(d));
        uft_error_t rc = uft_format_plugin_vdk.open(&d, pfad, true);
        snprintf(h, sizeof(h), "open=%d, %u Zyl, %u Koepfe, %u Sek, %u Byte",
                 (int)rc, d.geometry.cylinders, d.geometry.heads,
                 d.geometry.sectors, d.geometry.sector_size);
        pruefe("ein 12-Byte-Kopf: 40 Zylinder, 1 Kopf, 18 x 256",
               rc == UFT_OK && d.geometry.cylinders == 40
               && d.geometry.heads == 1 && d.geometry.sectors == 18
               && d.geometry.sector_size == 256, h);

        if (rc == UFT_OK) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            uft_error_t r = uft_format_plugin_vdk.read_track(&d, 0, 0, &t);
            /* MAME: FIRST_SECTOR_ID = 1 -> IDs 1..18 */
            int ids_ok = (t.sector_count == SPT);
            for (size_t i = 0; i < t.sector_count && ids_ok; i++)
                if (t.sectors[i].id.sector != (uint8_t)(i + 1)) ids_ok = 0;
            snprintf(h, sizeof(h), "rc=%d, %zu Sektoren, erste IDs %u %u %u",
                     (int)r, (size_t)t.sector_count,
                     t.sector_count > 0 ? t.sectors[0].id.sector : 0,
                     t.sector_count > 1 ? t.sectors[1].id.sector : 0,
                     t.sector_count > 2 ? t.sectors[2].id.sector : 0);
            pruefe("die Sektor-IDs sind 1..18 (MAME FIRST_SECTOR_ID = 1)",
                   ids_ok, h);

            /* Und der Inhalt gehoert zur Nummer. */
            int inhalt_ok = (t.sector_count == SPT);
            for (size_t i = 0; i < t.sector_count && inhalt_ok; i++)
                if (t.sectors[i].data[0] != 0x40
                    || t.sectors[i].data[1] != (uint8_t)(i + 1)) inhalt_ok = 0;
            snprintf(h, sizeof(h), "Sektor 1: Byte 0 = %02X, Byte 1 = %02X "
                     "(erwartet 40 / 01)",
                     t.sector_count ? t.sectors[0].data[0] : 0,
                     t.sector_count ? t.sectors[0].data[1] : 0);
            pruefe("und jeder traegt SEINE Bytes", inhalt_ok, h);
            frei(&t);

            /* Die letzte Spur. */
            memset(&t, 0, sizeof(t));
            r = uft_format_plugin_vdk.read_track(&d, 39, 0, &t);
            snprintf(h, sizeof(h), "rc=%d, Byte 0 = %02X (erwartet 67)",
                     (int)r, t.sector_count ? t.sectors[0].data[0] : 0);
            pruefe("Spur 39 ist die letzte und liegt richtig",
                   r == UFT_OK && t.sector_count == SPT
                   && t.sectors[0].data[0] == (uint8_t)(0x40 + 39), h);
            frei(&t);

            /* Schranken — vorher fehlten sie auf BEIDEN Seiten. */
            memset(&t, 0, sizeof(t));
            r = uft_format_plugin_vdk.read_track(&d, 40, 0, &t);
            pruefe("Spur 40 wird abgewiesen", r != UFT_OK, NULL);
            frei(&t);
            memset(&t, 0, sizeof(t));
            r = uft_format_plugin_vdk.read_track(&d, 0, 1, &t);
            pruefe("Kopf 1 wird abgewiesen (die Datei sagt 1 Seite)",
                   r != UFT_OK, NULL);
            frei(&t);
            memset(&t, 0, sizeof(t));
            r = uft_format_plugin_vdk.read_track(&d, -1, 0, &t);
            pruefe("Spur -1 wird abgewiesen (MF-519)", r != UFT_OK, NULL);
            frei(&t);

            uft_format_plugin_vdk.close(&d);
        }
        remove(pfad);
    }

    /* ── Befund 1: ein Kopf von exakt 0x0100 Byte ───────────────────── */
    {
        baue_vdk(pfad, 256, 2, 1, 0, 0);
        uft_disk_t d;
        memset(&d, 0, sizeof(d));
        uft_error_t rc = uft_format_plugin_vdk.open(&d, pfad, true);
        pruefe("eine VDK mit 256-Byte-Kopf laesst sich oeffnen",
               rc == UFT_OK, NULL);
        if (rc == UFT_OK) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            uft_format_plugin_vdk.read_track(&d, 0, 0, &t);
            snprintf(h, sizeof(h), "Byte 0 = %02X, Byte 1 = %02X "
                     "(erwartet 40 / 01; 64 6B waere \"dk\", die Kennung)",
                     t.sector_count ? t.sectors[0].data[0] : 0,
                     t.sector_count ? t.sectors[0].data[1] : 0);
            pruefe("und ihre Sektoren sind Sektoren, nicht der Dateikopf",
                   t.sector_count == SPT && t.sectors[0].data[0] == 0x40
                   && t.sectors[0].data[1] == 0x01, h);

            /* Das Kopf-Fuellwerk 0x99 darf in keinem Sektor stehen —
             * und geprueft wird JEDES Byte, nicht nur das erste.
             *
             * MF-1018: die erste Fassung sah nur `data[0]` an. Bei der
             * Mutation „Kopfgroesse zurueck auf uint8_t" beginnt der
             * Leser bei Versatz 0, und dort steht `'d'`, nicht 0x99 —
             * die Zusicherung blieb also gruen, obwohl das Fuellwerk
             * ab Byte 12 des ersten Sektors sehr wohl drinstand. Eine
             * Zusicherung, die nur eine Stelle abtastet, ist eine
             * Zusicherung, die kaum feuern kann. */
            int neunundneunzig = 0;
            for (size_t i = 0; i < t.sector_count; i++)
                for (int k = 0; k < SS; k++)
                    if (t.sectors[i].data[k] == 0x99) neunundneunzig++;
            snprintf(h, sizeof(h), "%d Byte 0x99 in den Sektoren",
                     neunundneunzig);
            pruefe("das Kopf-Fuellwerk (0x99) taucht in keinem Sektorbyte auf",
                   neunundneunzig == 0, h);
            frei(&t);
            uft_format_plugin_vdk.close(&d);
        }
        remove(pfad);
    }

    /* ── Zwei Seiten: Spur aussen, Kopf innen ────────────────────────── */
    {
        baue_vdk(pfad, 12, 3, 2, 0, 0);
        uft_disk_t d;
        memset(&d, 0, sizeof(d));
        if (uft_format_plugin_vdk.open(&d, pfad, true) == UFT_OK) {
            snprintf(h, sizeof(h), "%u Zyl, %u Koepfe, %u gesamt",
                     d.geometry.cylinders, d.geometry.heads,
                     d.geometry.total_sectors);
            pruefe("3 Zylinder x 2 Seiten = 108 Sektoren",
                   d.geometry.cylinders == 3 && d.geometry.heads == 2
                   && d.geometry.total_sectors == 108, h);

            uft_track_t a, b2;
            memset(&a, 0, sizeof(a));
            memset(&b2, 0, sizeof(b2));
            uft_format_plugin_vdk.read_track(&d, 1, 0, &a);
            uft_format_plugin_vdk.read_track(&d, 1, 1, &b2);
            snprintf(h, sizeof(h),
                     "Spur1/K0: %02X %02X   Spur1/K1: %02X %02X "
                     "(erwartet 41 00 / 41 01)",
                     a.sector_count ? a.sectors[0].data[0] : 0,
                     a.sector_count ? a.sectors[0].data[2] : 0,
                     b2.sector_count ? b2.sectors[0].data[0] : 0,
                     b2.sector_count ? b2.sectors[0].data[2] : 0);
            pruefe("die Spuren liegen Zylinder aussen / Kopf innen",
                   a.sector_count == SPT && b2.sector_count == SPT
                   && a.sectors[0].data[0] == 0x41 && a.sectors[0].data[2] == 0
                   && b2.sectors[0].data[0] == 0x41
                   && b2.sectors[0].data[2] == 1, h);
            frei(&a); frei(&b2);
            uft_format_plugin_vdk.close(&d);
        } else {
            pruefe("3 Zylinder x 2 Seiten = 108 Sektoren", 0, "open schlug fehl");
        }
        remove(pfad);
    }

    /* ── Gegenproben ────────────────────────────────────────────────── */
    {
        struct { const char *was; size_t kopf; uint8_t tr, si, ko; } f[] = {
            { "Spuren = 0 im Kopf wird abgewiesen, nicht auf 35 gehoben",
              12, 0, 1, 0 },
            { "Seiten = 0 im Kopf wird abgewiesen",
              12, 40, 0, 0 },
            { "ein gesetztes Kompressionsbyte wird abgewiesen — gepackte "
              "Daten sind keine Sektoren",
              12, 2, 1, 1 },
            { "eine Kopfgroesse unter 12 wird abgewiesen",
              8, 2, 1, 0 },
        };
        for (size_t i = 0; i < sizeof(f)/sizeof(f[0]); i++) {
            /* Bei kopf < 12 muss von Hand gebaut werden. */
            if (f[i].kopf < 12) {
                uint8_t b[12 + SPT * SS * 2];
                memset(b, 0, sizeof(b));
                b[0] = 'd'; b[1] = 'k';
                b[2] = (uint8_t)f[i].kopf; b[3] = 0;
                b[8] = f[i].tr; b[9] = f[i].si;
                FILE *fp = fopen(pfad, "wb");
                if (fp) { fwrite(b, 1, sizeof(b), fp); fclose(fp); }
            } else {
                baue_vdk(pfad, f[i].kopf, f[i].tr, f[i].si, f[i].ko, 0);
            }
            uft_disk_t d;
            memset(&d, 0, sizeof(d));
            uft_error_t r = uft_format_plugin_vdk.open(&d, pfad, true);
            snprintf(h, sizeof(h), "open=%d", (int)r);
            pruefe(f[i].was, r != UFT_OK, h);
            if (r == UFT_OK) uft_format_plugin_vdk.close(&d);
            remove(pfad);
        }
    }

    /* ── Die Sonde bleibt, wie sie war ───────────────────────────────── */
    {
        uint8_t kopf[16];
        memset(kopf, 0, sizeof(kopf));
        kopf[0] = 'd'; kopf[1] = 'k';
        int conf = -1;
        bool p = uft_format_plugin_vdk.probe(kopf, sizeof(kopf),
                                            sizeof(kopf), &conf);
        snprintf(h, sizeof(h), "%s, Konfidenz %d", p ? "ja" : "nein", conf);
        pruefe("die Sonde nimmt die Kennung \"dk\" mit hoher Konfidenz "
               "(MF-729: 80..100 = Merkmal getroffen)",
               p && conf >= 80, h);
        /* Die Pruefung der Kopffelder gehoert ins Oeffnen, nicht in die
         * Sonde — `tests/test_disk_open_fuzz.c` verlangt ausdruecklich,
         * dass die Sonde eine Datei mit dieser Kennung annimmt. */
        kopf[0] = 'X';
        conf = -1;
        p = uft_format_plugin_vdk.probe(kopf, sizeof(kopf), sizeof(kopf),
                                        &conf);
        pruefe("und weist eine falsche Kennung ab", !p, NULL);
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
