/**
 * @file test_mgt_gegen_mame.c
 * @brief MGT Feld fuer Feld gegen MAMEs `coupedsk.cpp` (MF-1006).
 *
 * ── Warum dieser Test die Uebereinstimmung FESTNAGELT ───────────────
 *
 * `mgt` stand auf **T3** — und anders als bei `cfi` (MF-1004) hat der
 * Feldabgleich hier **keinen Fehler** gefunden: UFTs Leser stimmt mit
 * dem Orakel ueberein. Uebereinstimmung reicht fuer T2 nur, wenn sie
 * BEWACHT ist statt behauptet; genau das tut dieser Test.
 *
 * Referenz: MAME, `src/lib/formats/coupedsk.cpp` — `mgt_format::load()`
 * und `::save()`, BSD-3-Clause, Olivier Galibert. Beigesteuert vom
 * Eigentuemer als `neue-ideen/formats.zip`; kein MAME-Code uebernommen.
 * Der Kanal ist derselbe wie in MF-614 (MAME als benannte Referenz).
 *
 * ── Der Abgleich, Feld fuer Feld ────────────────────────────────────
 *
 *   Groesse            819200            = 80*2*10*512     BEIDE
 *   Zylinder           80                                  BEIDE
 *   Koepfe             2                                   BEIDE
 *   Sektoren je Spur   10                                  BEIDE
 *   Sektorgroesse      512                                 BEIDE
 *   Sektor-IDs         1..10   (MAME: `sectors[i].sector_id = i + 1`)
 *                              (UFT:  `s + MGT_FIRST_SECTOR`, = 1)
 *   Spurversatz        MAME: `(track*2 + head) * track_size`
 *                      UFT:  `c * MGT_HEADS + h`, Spurgroesse 5120
 *                      -> dieselbe Anordnung, seitenverschraenkt
 *
 * ── Zwei benannte Unterschiede, KEIN Fehler ─────────────────────────
 *
 * **(a) 737280 (9 Sektoren je Spur).** MAMEs `load()` kennt sie
 * (`sector_count = (size == 737280) ? 9 : 10`), aber in seinem
 * `identify()` steht der Vergleich `size == 737280` **auskommentiert**
 * — geprueft wird dort allein `size == 819200`.
 *
 * MAME erkennt die Variante also nie, es koennte sie nur laden, wenn
 * jemand das Format erzwingt. UFT weist 737280 ab — und das ist nach
 * **MF-729** richtig: 737280 ist die Groesse von PC-720K und
 * Atari-ST-DS; sie als MGT zu beanspruchen waere ein Fehlalarm. MAMEs
 * auskommentierte Zeile ist genau dafuer ein Beleg. Fall 4 haelt das
 * fest.
 *
 * **(b) 409600 (40 Zylinder).** UFT kennt sie, MAME nicht. Kein
 * Widerspruch — UFT ist hier breiter. Fall 5 haelt sie fest, damit
 * niemand sie fuer einen Zufall haelt.
 *
 * ── Und was das fuer P3-312 heisst ──────────────────────────────────
 *
 * P3-312 fuehrte „UFTs MGT-Leser kennt nur EINE von zwei
 * Spuranordnungen" und nannte als Blocker, dass `src/samdisk/SAMCoupe.h`
 * fehlt. **MAME rechnet dieselbe Anordnung wie UFT** und kennt die
 * IMG-Variante ebenfalls nicht — sie ist eine SAMdisk-Besonderheit.
 * UFT liest MGT richtig; der Eintrag verliert seine Schaerfe.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "uft/formats/uft_mgt.h"
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

/* Jede Spur bekommt ein eigenes Byte, abgeleitet aus (c,h) nach der
 * ANORDNUNG DES ORAKELS: `(track*2 + head)`. Eine vertauschte Rechnung
 * im Leser trifft damit sicher die falsche Spur. */
static uint8_t marke(int c, int h)
{
    return (uint8_t)((c * 2 + h) & 0xFF);
}

static uint8_t *baue_mgt(int cylinders, int sektoren, size_t *out_size)
{
    const size_t spurgroesse = (size_t)sektoren * 512;
    const size_t n = (size_t)cylinders * 2 * spurgroesse;
    uint8_t *p = (uint8_t *)malloc(n);
    if (!p) return NULL;
    for (int c = 0; c < cylinders; c++)
        for (int h = 0; h < 2; h++)
            memset(p + ((size_t)c * 2 + h) * spurgroesse,
                   marke(c, h), spurgroesse);
    *out_size = n;
    return p;
}

int main(void)
{
    printf("=== MGT gegen MAME coupedsk.cpp (MF-1006) ===\n");

    /* ── 1./2./3.: die 819200-Diskette ─────────────────────────────── */
    size_t n = 0;
    uint8_t *datei = baue_mgt(80, 10, &n);
    if (!datei) { printf("  [ROT]  kein Speicher\n"); return 1; }

    char h[200];
    snprintf(h, sizeof(h), "gebaut: %zu Byte, erwartet 819200", n);
    pruefe("die Pruefdatei hat die Groesse aus dem Orakel", n == 819200, h);

    uft_disk_image_t *disk = NULL;
    mgt_read_result_t erg;
    uft_error_t rc = uft_mgt_read_mem(datei, n, &disk, &erg);
    pruefe("sie laesst sich oeffnen", rc == UFT_OK && disk != NULL,
           "ohne geoeffnete Datei sagt der Rest nichts");
    if (rc != UFT_OK || !disk) {
        free(datei);
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }

    snprintf(h, sizeof(h), "gemessen %ux%ux%u, %u Byte/Sektor",
             disk->tracks, disk->heads, disk->sectors_per_track,
             disk->bytes_per_sector);
    pruefe("Geometrie wie im Orakel: 80 x 2 x 10 x 512",
           disk->tracks == 80 && disk->heads == 2
           && disk->sectors_per_track == 10
           && disk->bytes_per_sector == 512, h);

    /* Sektor-IDs: MAME setzt `sector_id = i + 1`. */
    uft_track_t *t00 = disk->track_data[0];
    int ids_ok = (t00 && t00->sector_count == 10);
    if (ids_ok)
        for (int s = 0; s < 10; s++)
            if (t00->sectors[s].id.sector != s + 1) { ids_ok = 0; break; }
    pruefe("Sektor-IDs laufen 1..10 (MAME: sector_id = i + 1)", ids_ok,
           "eine 0-basierte Numerierung waere eine stille Falschaussage");

    /* Die Anordnung. (0,1) und (1,0) unterscheiden
     * `c*heads+h` von `h*cyls+c` — beide Zellen liegen bei einer
     * vertauschten Rechnung falsch. Dazu die letzte Spur, weil ein
     * Abschneiden am Ende sonst unbemerkt bliebe. */
    struct { int c, h; } proben[] = {
        { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 }, { 40, 1 }, { 79, 1 }
    };
    int anordnung_ok = 1;
    char wo[64] = "";
    for (size_t i = 0; i < sizeof(proben) / sizeof(proben[0]); i++) {
        int c = proben[i].c, hh = proben[i].h;
        uft_track_t *tr = disk->track_data[(size_t)c * 2 + hh];
        if (!tr || tr->sector_count == 0 || !tr->sectors[0].data
            || tr->sectors[0].data[0] != marke(c, hh)) {
            anordnung_ok = 0;
            snprintf(wo, sizeof(wo), "Spur (%d,%d) traegt 0x%02X statt 0x%02X",
                     c, hh,
                     (tr && tr->sector_count && tr->sectors[0].data)
                         ? (unsigned)tr->sectors[0].data[0] : 0u,
                     (unsigned)marke(c, hh));
            break;
        }
    }
    pruefe("Spurversatz wie im Orakel: (Zylinder*2 + Kopf) * 5120",
           anordnung_ok, wo[0] ? wo : NULL);

    uft_disk_free(disk);
    free(datei);

    /* ── 4.: 737280 wird ABGEWIESEN ───────────────────────────────── */
    {
        size_t n9 = 0;
        uint8_t *d9 = baue_mgt(80, 9, &n9);
        if (d9) {
            uft_disk_image_t *disk9 = NULL;
            uft_error_t rc9 = uft_mgt_read_mem(d9, n9, &disk9, NULL);
            snprintf(h, sizeof(h),
                     "%zu Byte wurden angenommen (rc=%d) -- das ist die "
                     "Groesse von PC-720K und Atari-ST-DS; MAMEs identify() "
                     "hat die Zeile auskommentiert", n9, (int)rc9);
            pruefe("737280 (9 Sektoren) wird abgewiesen", rc9 != UFT_OK, h);
            if (disk9) uft_disk_free(disk9);
            free(d9);
        }
    }

    /* ── 5.: 409600 (40 Zylinder) — UFTs eigene Variante ──────────── */
    {
        size_t n40 = 0;
        uint8_t *d40 = baue_mgt(40, 10, &n40);
        if (d40) {
            uft_disk_image_t *disk40 = NULL;
            uft_error_t rc40 = uft_mgt_read_mem(d40, n40, &disk40, NULL);
            int ok = (rc40 == UFT_OK && disk40 && disk40->tracks == 40
                      && disk40->heads == 2);
            snprintf(h, sizeof(h),
                     "%zu Byte, rc=%d -- MAME kennt diese Variante nicht, "
                     "UFT ist hier breiter", n40, (int)rc40);
            pruefe("409600 (40 Zylinder) wird angenommen", ok, h);
            if (disk40) uft_disk_free(disk40);
            free(d40);
        }
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot == 0 ? 0 : 1;
}
