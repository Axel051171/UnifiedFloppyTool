/**
 * @file test_jv3_gegen_mame.c
 * @brief JV3 gegen MAMEs Satzarithmetik und Tim Manns Beschreibung
 *        (MF-1017).
 *
 * ── Woher die Frage kommt ────────────────────────────────────────────
 *
 * `jv3` stand auf **T3** und hatte keinen eigenen Test. Fuenf Befunde,
 * und die ersten beiden zusammen bedeuteten, dass **jeder Sektor jeder
 * JV3-Datei falsche Bytes lieferte**.
 *
 * Gemessen mit einem Wegwerf-Programm an einer nach MAMEs Arithmetik
 * gebauten Datei — drei Sektoren (Fuellbytes A0, B1, D2) mit einem
 * freien Satz dazwischen (CC):
 *
 *     Spur 0/0   : 2 Sektoren   (Orakel: 3)
 *        Sektor 1: Fuellbyte B1     <- gehoert Sektor 1, nicht Sektor 0
 *        Sektor 1: Fuellbyte CC     <- der FREIE Datenraum
 *
 * **Befund 1:** `JV3_HEADER_SIZE` war `0x2300`. Wirklich sind es
 * `2901*3 + 1 = 0x2200` — das Schreibschutz-Byte ist das letzte Byte
 * des Kopfbereichs (0x21FF), es gibt keine Polsterung. Jeder Sektor
 * wurde damit 256 Byte zu weit gelesen.
 *
 * **Befund 2:** der Verzeichnislauf brach beim ersten freien Satz ab.
 * MAME: „Unused descriptors are FF FF (FC | size). These can be
 * intermixed with valid descriptors." Sein `identify()` ueberspringt
 * sie und schiebt den Datenzeiger um deren Groesse weiter.
 *
 * **Befund 3:** der Freimarker ist `track == 0xFF` **allein**; UFT
 * verlangte Spur UND Sektor auf 0xFF.
 *
 * **Befund 4:** `if (sec_num > 0) sec_num--` vor einem
 * `uft_format_add_sector()`, das 1 addiert — Sektor 0 und Sektor 1
 * landeten beide auf ID 1 (oben zweimal „Sektor 1").
 *
 * **Befund 5:** Dichte und DAM standen in falschen Bits. Bit 7 ist die
 * Dichte, Bits 5-6 sind ein zweistelliger DAM-Kode; der Code las
 * `flags & 0x40` als „deleted", was in Einzeldichte die **0xF9** trifft
 * und nicht die 0xF8.
 *
 * ── Die Orakel ───────────────────────────────────────────────────────
 *
 * **MAME `formats/trs80_dsk.cpp`** (BSD-3-Clause, Dirk Best), in
 * `neue-ideen/formats.zip`, `jv3_format::identify()` woertlich:
 *
 *     const uint32_t header_size = entries * 3 + 1;
 *     uint32_t data_ptr = header_size, last_data = header_size;
 *     if (track < 0xff) { size = 128 << (flag_size ^ 1);
 *                         data_ptr += size; last_data = data_ptr; }
 *     else              { size = 128 << (flag_size ^ 2);
 *                         data_ptr += size; }
 *
 * **Tim Mann, dskspec.html** woertlich: „if a sector is in use, xor'ing
 * its JV3_SIZE field with 1 gives the IBM size code … If a sector is
 * free, xor'ing its JV3_SIZE field with 2 gives its IBM size code."
 *
 * Zwei unabhaengige Haende, dieselbe Regel — und die Pruefdatei hier
 * ist nach dieser Regel gebaut, nicht nach UFTs Code. Jede erwartete
 * Zahl unten ist ausgeschrieben (MF-913).
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

extern const uft_format_plugin_t uft_format_plugin_jv3;

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

#define ENTRIES     2901
#define HDR         (ENTRIES * 3 + 1)       /* 0x2200 */
#define RO_OFF      (ENTRIES * 3)           /* 0x21FF */

/* Ein Satz, wie er in die Pruefdatei geschrieben wird. */
typedef struct {
    uint8_t track;      /* 0xFF = frei */
    uint8_t sector;
    uint8_t flags;
    uint8_t fuell;      /* Fuellbyte des Datenblocks */
} satz_t;

/* Groesse nach MAME: benutzt ^1, frei ^2, dann 128 << code */
static uint16_t groesse(uint8_t flags, int benutzt)
{
    uint8_t code = (uint8_t)((flags & 3) ^ (benutzt ? 1 : 2));
    return (uint16_t)(128u << code);
}

/* Baut eine JV3 nach MAMEs Arithmetik. Gibt die Gesamtgroesse zurueck,
 * 0 wenn der Puffer nicht reicht. */
static size_t baue_jv3(uint8_t *b, size_t kap, const satz_t *s, int n,
                       uint8_t ro_flag, int daten_kuerzen)
{
    if (kap < HDR) return 0;
    memset(b, 0xFF, ENTRIES * 3);       /* alle Saetze frei */
    b[RO_OFF] = ro_flag;

    size_t daten = 0;
    for (int i = 0; i < n; i++)
        daten += groesse(s[i].flags, s[i].track != 0xFF);
    if (HDR + daten > kap) return 0;

    size_t off = HDR;
    for (int i = 0; i < n; i++) {
        b[i * 3 + 0] = s[i].track;
        b[i * 3 + 1] = s[i].sector;
        b[i * 3 + 2] = s[i].flags;
        uint16_t sz = groesse(s[i].flags, s[i].track != 0xFF);
        memset(b + off, s[i].fuell, sz);
        off += sz;
    }
    size_t gesamt = HDR + daten;
    if (daten_kuerzen && gesamt > 256) gesamt -= 256;
    return gesamt;
}

static int schreibe(const char *pfad, const uint8_t *b, size_t n)
{
    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;
    int ok = (fwrite(b, 1, n, f) == n);
    fclose(f);
    return ok;
}

static void frei(uft_track_t *t)
{
    if (!t) return;
    free(t->sectors);
    memset(t, 0, sizeof(*t));
}

/* Sucht in der Spur den Sektor mit der ID `id`. */
static const uft_sector_t *finde(const uft_track_t *t, uint8_t id)
{
    for (size_t i = 0; i < t->sector_count; i++)
        if (t->sectors[i].id.sector == id) return &t->sectors[i];
    return NULL;
}

int main(void)
{
    printf("=== JV3 gegen MAMEs Satzarithmetik (MF-1017) ===\n");

    char pfad[512];
    const char *tmp = getenv("TEMP");
    if (!tmp) tmp = getenv("TMPDIR");
    if (!tmp) tmp = ".";
    snprintf(pfad, sizeof(pfad), "%s/uft_mf1017.jv3", tmp);

    char h[240];
    static uint8_t b[HDR + 8192];

    /* ── Der Kern: Datenanfang, freie Saetze, Sektornummern ─────────── */
    {
        /* Freier Satz mit flags&3 = 0 -> 128 << (0^2) = **512** Byte.
         * Damit ist die XOR-2-Regel wirklich geprueft: mit der
         * XOR-1-Regel waeren es 256, und der Sektor dahinter laege
         * 256 Byte falsch. */
        const satz_t s[] = {
            { 0x00, 0, 0x00, 0xA0 },    /* benutzt, 256 B */
            { 0x00, 1, 0x00, 0xB1 },    /* benutzt, 256 B */
            { 0xFF, 5, 0x00, 0xCC },    /* FREI, 512 B, Sektor != 0xFF */
            { 0x00, 2, 0x00, 0xD2 },    /* benutzt, 256 B */
        };
        size_t n = baue_jv3(b, sizeof(b), s, 4, 0x00, 0);
        int ok = (n > 0) && schreibe(pfad, b, n);
        snprintf(h, sizeof(h), "%zu Byte (Kopf 0x%X + 256+256+512+256)",
                 n, (unsigned)HDR);
        pruefe("die Pruefdatei ist nach MAMEs Arithmetik gebaut",
               ok && n == HDR + 256 + 256 + 512 + 256, h);

        uft_disk_t d;
        memset(&d, 0, sizeof(d));
        uft_error_t rc = uft_format_plugin_jv3.open(&d, pfad, true);
        snprintf(h, sizeof(h), "open=%d, %u Zyl, %u Koepfe, %u Sektoren",
                 (int)rc, d.geometry.cylinders, d.geometry.heads,
                 d.geometry.total_sectors);
        pruefe("sie laesst sich oeffnen und fuehrt DREI Sektoren",
               rc == UFT_OK && d.geometry.total_sectors == 3, h);

        if (rc == UFT_OK) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            uft_error_t r = uft_format_plugin_jv3.read_track(&d, 0, 0, &t);
            snprintf(h, sizeof(h), "rc=%d, %zu Sektoren", (int)r,
                     (size_t)t.sector_count);
            pruefe("Spur 0/0 liefert drei Sektoren — der freie Satz ist "
                   "uebersprungen, nicht das Ende", r == UFT_OK
                   && t.sector_count == 3, h);

            const uft_sector_t *s0 = finde(&t, 0);
            const uft_sector_t *s1 = finde(&t, 1);
            const uft_sector_t *s2 = finde(&t, 2);
            snprintf(h, sizeof(h), "IDs 0/1/2 gefunden: %d/%d/%d",
                     s0 != NULL, s1 != NULL, s2 != NULL);
            pruefe("die Sektornummern sind 0, 1 und 2 — jede genau einmal",
                   s0 && s1 && s2, h);

            if (s0 && s1 && s2) {
                snprintf(h, sizeof(h),
                         "Sektor 0 -> %02X, 1 -> %02X, 2 -> %02X "
                         "(erwartet A0 / B1 / D2)",
                         s0->data[0], s1->data[0], s2->data[0]);
                pruefe("und jeder traegt SEINE Bytes, nicht die des "
                       "naechsten Blocks",
                       s0->data[0] == 0xA0 && s1->data[0] == 0xB1
                       && s2->data[0] == 0xD2, h);

                /* Das Fuellbyte des freien Raums darf nirgends auftauchen. */
                int cc = 0;
                for (size_t i = 0; i < t.sector_count; i++)
                    if (t.sectors[i].data[0] == 0xCC) cc++;
                pruefe("der freie Datenraum (CC) wird nicht als Sektor "
                       "ausgegeben", cc == 0, NULL);
            }
            frei(&t);
            uft_format_plugin_jv3.close(&d);
        }
        remove(pfad);
    }

    /* ── Groessen: die vier IBM-Kodes auf der benutzten Seite ───────── */
    {
        /* benutzt: 128 << ((flags&3) ^ 1)
         *   flags&3=1 -> 128    flags&3=0 -> 256
         *   flags&3=3 -> 512    flags&3=2 -> 1024 */
        const satz_t s[] = {
            { 0x00, 0, 0x01, 0x11 },    /* 128  */
            { 0x00, 1, 0x00, 0x22 },    /* 256  */
            { 0x00, 2, 0x03, 0x33 },    /* 512  */
            { 0x00, 3, 0x02, 0x44 },    /* 1024 */
        };
        size_t n = baue_jv3(b, sizeof(b), s, 4, 0x00, 0);
        schreibe(pfad, b, n);
        snprintf(h, sizeof(h), "%zu Byte (Kopf + 128+256+512+1024)", n);
        pruefe("eine Datei mit allen vier Sektorgroessen",
               n == HDR + 128 + 256 + 512 + 1024, h);

        uft_disk_t d;
        memset(&d, 0, sizeof(d));
        if (uft_format_plugin_jv3.open(&d, pfad, true) == UFT_OK) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            uft_format_plugin_jv3.read_track(&d, 0, 0, &t);
            const uint16_t soll[4] = { 128, 256, 512, 1024 };
            const uint8_t fuell[4] = { 0x11, 0x22, 0x33, 0x44 };
            int gut = (t.sector_count == 4);
            for (uint8_t i = 0; i < 4 && gut; i++) {
                const uft_sector_t *s2 = finde(&t, i);
                if (!s2 || s2->data_len != soll[i]
                    || s2->data[0] != fuell[i]) gut = 0;
            }
            snprintf(h, sizeof(h), "%zu Sektoren, Laengen %u/%u/%u/%u",
                     (size_t)t.sector_count,
                     t.sector_count > 0 ? (unsigned)t.sectors[0].data_len : 0,
                     t.sector_count > 1 ? (unsigned)t.sectors[1].data_len : 0,
                     t.sector_count > 2 ? (unsigned)t.sectors[2].data_len : 0,
                     t.sector_count > 3 ? (unsigned)t.sectors[3].data_len : 0);
            pruefe("128/256/512/1024 stimmen, und jeder Block liegt richtig",
                   gut, h);
            frei(&t);
            uft_format_plugin_jv3.close(&d);
        } else {
            pruefe("128/256/512/1024 stimmen, und jeder Block liegt richtig",
                   0, "open schlug fehl");
        }
        remove(pfad);
    }

    /* ── DAM und CRC-Flag ───────────────────────────────────────────── */
    {
        /* MAMEs Tafel:  0xF8 „deleted" = 0x60 (SD) / 0x20 (DD)
         *               0xF9            = 0x40 (SD)
         *               0xFB „normal"   = 0x00
         * Der alte Code las `flags & 0x40` als „deleted" — das trifft
         * in Einzeldichte die 0xF9. */
        const satz_t s[] = {
            { 0x00, 0, 0x00,        0xA1 },  /* SD, DAM 0xFB normal    */
            { 0x00, 1, 0x60,        0xA2 },  /* SD, DAM 0xF8 DELETED   */
            { 0x00, 2, 0x40,        0xA3 },  /* SD, DAM 0xF9 (nicht F8)*/
            { 0x00, 3, 0x80 | 0x20, 0xA4 },  /* DD, DAM 0xF8 DELETED   */
            { 0x00, 4, 0x08,        0xA5 },  /* CRC-Fehler             */
        };
        size_t n = baue_jv3(b, sizeof(b), s, 5, 0x00, 0);
        schreibe(pfad, b, n);

        uft_disk_t d;
        memset(&d, 0, sizeof(d));
        if (uft_format_plugin_jv3.open(&d, pfad, true) == UFT_OK) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            uft_format_plugin_jv3.read_track(&d, 0, 0, &t);
            const uft_sector_t *n0 = finde(&t, 0), *d1 = finde(&t, 1);
            const uft_sector_t *u2 = finde(&t, 2), *d3 = finde(&t, 3);
            const uft_sector_t *c4 = finde(&t, 4);
            int alle = (n0 && d1 && u2 && d3 && c4);
            snprintf(h, sizeof(h), "%zu von 5 Sektoren gefunden",
                     (size_t)t.sector_count);
            pruefe("alle fuenf Flag-Faelle liegen vor", alle, h);
            if (alle) {
                snprintf(h, sizeof(h),
                         "normal=%d  0x60=%d  0x40=%d  DD|0x20=%d",
                         n0->deleted, d1->deleted, u2->deleted, d3->deleted);
                pruefe("„deleted\" ist DAM 0xF8: 0x60 in SD und 0x20 in DD "
                       "— 0x40 ist es NICHT",
                       !n0->deleted && d1->deleted && !u2->deleted
                       && d3->deleted, h);
                pruefe("das CRC-Flag (Bit 3) macht den Sektor schlecht",
                       !c4->id.crc_ok || !c4->data_crc_ok, NULL);
            }
            frei(&t);
            uft_format_plugin_jv3.close(&d);
        } else {
            pruefe("alle fuenf Flag-Faelle liegen vor", 0, "open schlug fehl");
        }
        remove(pfad);
    }

    /* ── Seite 1 ────────────────────────────────────────────────────── */
    {
        const satz_t s[] = {
            { 0x05, 0, 0x00,        0x50 },   /* Spur 5, Seite 0 */
            { 0x05, 0, 0x00 | 0x10, 0x51 },   /* Spur 5, Seite 1 */
        };
        size_t n = baue_jv3(b, sizeof(b), s, 2, 0x00, 0);
        schreibe(pfad, b, n);
        uft_disk_t d;
        memset(&d, 0, sizeof(d));
        if (uft_format_plugin_jv3.open(&d, pfad, true) == UFT_OK) {
            snprintf(h, sizeof(h), "%u Zyl, %u Koepfe",
                     d.geometry.cylinders, d.geometry.heads);
            pruefe("Bit 4 ist die Seite: 6 Zylinder, 2 Koepfe",
                   d.geometry.cylinders == 6 && d.geometry.heads == 2, h);

            uft_track_t t0, t1;
            memset(&t0, 0, sizeof(t0));
            memset(&t1, 0, sizeof(t1));
            uft_format_plugin_jv3.read_track(&d, 5, 0, &t0);
            uft_format_plugin_jv3.read_track(&d, 5, 1, &t1);
            snprintf(h, sizeof(h), "Seite 0 -> %02X, Seite 1 -> %02X "
                     "(erwartet 50 / 51)",
                     t0.sector_count ? t0.sectors[0].data[0] : 0,
                     t1.sector_count ? t1.sectors[0].data[0] : 0);
            pruefe("und beide Seiten liefern ihre eigenen Bytes",
                   t0.sector_count == 1 && t1.sector_count == 1
                   && t0.sectors[0].data[0] == 0x50
                   && t1.sectors[0].data[0] == 0x51, h);
            frei(&t0); frei(&t1);
            uft_format_plugin_jv3.close(&d);
        } else {
            pruefe("Bit 4 ist die Seite: 6 Zylinder, 2 Koepfe", 0,
                   "open schlug fehl");
        }
        remove(pfad);
    }

    /* ── Gegenproben ────────────────────────────────────────────────── */
    {
        const satz_t gut[] = { { 0x00, 0, 0x00, 0x77 } };
        struct { const char *was; satz_t s; uint8_t ro; int kurz; } f[] = {
            { "ein Satz mit dem non-IBM-Bit wird benannt abgewiesen",
              { 0x00, 0, 0x04, 0x77 }, 0x00, 0 },
            { "Spur 96 wird abgewiesen (MAME MAX_TRACKS)",
              { 96,   0, 0x00, 0x77 }, 0x00, 0 },
            { "Sektor 19 wird abgewiesen (MAME MAX_SECTORS)",
              { 0x00, 19, 0x00, 0x77 }, 0x00, 0 },
            { "ein Schreibschutz-Byte ungleich 00/FF wird abgewiesen",
              { 0x00, 0, 0x00, 0x77 }, 0x42, 0 },
            { "fehlende Sektordaten werden abgewiesen",
              { 0x00, 0, 0x00, 0x77 }, 0x00, 1 },
        };
        for (size_t i = 0; i < sizeof(f)/sizeof(f[0]); i++) {
            /* Bei der Kurz-Probe braucht es zwei Saetze, damit 256 Byte
             * fehlen koennen. */
            satz_t s[2] = { f[i].s, { 0x00, 1, 0x00, 0x88 } };
            int n_s = f[i].kurz ? 2 : 1;
            size_t n = baue_jv3(b, sizeof(b), s, n_s, f[i].ro, f[i].kurz);
            schreibe(pfad, b, n);
            uft_disk_t d;
            memset(&d, 0, sizeof(d));
            uft_error_t r = uft_format_plugin_jv3.open(&d, pfad, true);
            snprintf(h, sizeof(h), "open=%d", (int)r);
            pruefe(f[i].was, r != UFT_OK, h);
            if (r == UFT_OK) uft_format_plugin_jv3.close(&d);
            remove(pfad);
        }
        (void)gut;
    }

    /* ── Die Sonde ──────────────────────────────────────────────────── */
    {
        /* Zehn Sektoren, damit die „not even one track's worth"-Schranke
         * nicht greift. */
        satz_t s[10];
        for (int i = 0; i < 10; i++) {
            s[i].track = 0; s[i].sector = (uint8_t)i;
            s[i].flags = 0x00; s[i].fuell = (uint8_t)(0x60 + i);
        }
        size_t n = baue_jv3(b, sizeof(b), s, 10, 0x00, 0);
        int conf = -1;
        bool p = uft_format_plugin_jv3.probe(b, n, n, &conf);
        snprintf(h, sizeof(h), "%s, Konfidenz %d", p ? "ja" : "nein", conf);
        pruefe("die Sonde nimmt eine genau passende JV3 mit hoher "
               "Konfidenz", p && conf >= 80, h);

        /* DMK-Unterscheidung: MAME weist ab, wenn die Bytes 5..15 alle
         * null sind. */
        uint8_t vorher[11];
        memcpy(vorher, b + 5, 11);
        memset(b + 5, 0, 11);
        conf = -1;
        p = uft_format_plugin_jv3.probe(b, n, n, &conf);
        pruefe("und weist eine DMK-Datei ab (Bytes 5..15 alle null)",
               !p, NULL);
        memcpy(b + 5, vorher, 11);

        /* Doppelt vergebener Sektor. */
        b[3] = 0; b[4] = 0; b[5] = 0x00;   /* Satz 1 = Satz 0 */
        conf = -1;
        p = uft_format_plugin_jv3.probe(b, n, n, &conf);
        pruefe("und einen doppelt vergebenen Sektor (MAME prueft das)",
               !p, NULL);
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
