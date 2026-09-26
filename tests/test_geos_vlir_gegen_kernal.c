/* GEOS-VLIR-Record-Block: 00/00 ist das Ende, 00/FF ein leerer Platz
 * (H-12, MF-1338).
 *
 * DREI BELEGE, einer davon von fremder Hand:
 *   1. docs/format_specs/commodore/GEOS.TXT:170-182 — "When a T/S link of
 *      $00/$00 is encountered, we are at the end of the RECORD block. If
 *      the T/S link is a $00/$FF, then the record is not available."
 *   2. docs/format_specs/commodore/CVT.TXT:119-122 — ein 00/FF-Record
 *      existiert nicht, sein Eintrag bleibt aber im Record-Block stehen.
 *      (1 und 2 stammen aus derselben Sammlung, also EINE Hand.)
 *   3. Der GEOS-2.0-Kernal (Berkeley Softworks), rekonstruierter Quelltext
 *      von Michael Steil, github mist64/geos, kernal/files/files10.s —
 *      NUR GELESEN, nichts uebernommen (Kanal Spec):
 *        _OpenRecordFile zaehlt die Plaetze ab Byte 2 und haelt beim ersten
 *        00/00 an; ein 00/FF zaehlt als Platz.
 *        _WriteRecord mit Laenge 0 gibt die Kette frei und schreibt 00/FF.
 *        _ReadRecord liest bei Spur 0 nichts.
 *        _DeleteRecord schiebt die Tafel zusammen — einen Loeschvermerk
 *        gibt es nicht.
 *        Hoechstens $7F = 127 Plaetze.
 *
 * VERTRAG: `data` zeigt auf die 254 Byte HINTER der eigenen Verkettung des
 * Record-Blocks (Sektor + 2), also auf Platz 0.
 */
#include "uft/formats/c64/uft_geos.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) {
        printf("  [ok ] %s\n", was);
        gruen++;
    } else {
        printf("  [ROT] %s%s%s\n", was, hinweis ? " -- " : "", hinweis ? hinweis : "");
        rot++;
    }
}

static void setze(uint8_t *tafel, int platz, uint8_t spur, uint8_t sektor)
{
    tafel[platz * 2] = spur;
    tafel[platz * 2 + 1] = sektor;
}

static void t_beispiel_aus_geos_txt(void)
{
    puts("1. Die Beispieltafel aus GEOS.TXT: sechs Records, dann das Ende");
    uint8_t tafel[254];
    memset(tafel, 0, sizeof tafel);
    /* GEOS.TXT: hinter 00 FF folgen 08 00 09 04 09 03 0A 0A 0B 11 0F 11 00 00 */
    setze(tafel, 0, 0x08, 0x00);
    setze(tafel, 1, 0x09, 0x04);
    setze(tafel, 2, 0x09, 0x03);
    setze(tafel, 3, 0x0A, 0x0A);
    setze(tafel, 4, 0x0B, 0x11);
    setze(tafel, 5, 0x0F, 0x11);
    geos_vlir_record_t r[GEOS_MAX_VLIR_RECORDS];
    int n = -1;
    int rc = geos_parse_vlir_index(tafel, r, &n);
    char h[80];
    snprintf(h, sizeof h, "rc=%d, Plaetze=%d", rc, n);
    pruefe("sechs Plaetze, rc 0", rc == 0 && n == 6, h);
    pruefe("Platz 5 ist Spur 15 Sektor 17", r[5].track == 0x0F && r[5].sector == 0x11, NULL);
}

static void t_luecke_bleibt_ein_platz(void)
{
    puts("2. Eine Luecke (00/FF) zwischen gueltigen Records");
    uint8_t tafel[254];
    memset(tafel, 0, sizeof tafel);
    setze(tafel, 0, 0x08, 0x00);
    setze(tafel, 1, 0x00, 0xFF);    /* leerer Record -- Kernal _WriteRecord(0) */
    setze(tafel, 2, 0x09, 0x04);
    geos_vlir_record_t r[GEOS_MAX_VLIR_RECORDS];
    int n = -1;
    int rc = geos_parse_vlir_index(tafel, r, &n);
    char h[80];
    /* Der Kernal zaehlt 00/FF als Platz. Wuerde er nicht mitgezaehlt,
     * verschoebe sich jede folgende Recordnummer um eins. */
    snprintf(h, sizeof h, "rc=%d, Plaetze=%d", rc, n);
    pruefe("drei Plaetze -- die Luecke zaehlt mit, die Nummern bleiben stabil",
           rc == 0 && n == 3, h);
    pruefe("Platz 1 ist leer", geos_vlir_record_empty(&r[1]), NULL);
    snprintf(h, sizeof h, "size=%zu", r[1].size);
    pruefe("00/FF ist keine Groessenangabe (size 0, nicht 255)", r[1].size == 0, h);
    pruefe("Platz 2 ist Spur 9 Sektor 4, nicht leer",
           r[2].track == 9 && r[2].sector == 4 && !geos_vlir_record_empty(&r[2]), NULL);
}

static void t_ende_ist_ende(void)
{
    puts("3. 00/00 ist das ENDE -- was dahinter steht, ist kein Record");
    uint8_t tafel[254];
    memset(tafel, 0, sizeof tafel);
    setze(tafel, 0, 0x08, 0x00);
    /* Platz 1 = 00/00: Ende */
    setze(tafel, 2, 0x09, 0x04);    /* Nichtnull-Bytes hinter dem Ende */
    geos_vlir_record_t r[GEOS_MAX_VLIR_RECORDS];
    memset(r, 0x5A, sizeof r);
    int n = -1;
    int rc = geos_parse_vlir_index(tafel, r, &n);
    char h[80];
    snprintf(h, sizeof h, "Plaetze=%d", n);
    pruefe("ein Platz -- der Lauf haelt beim ersten 00/00 an", n == 1, h);
    snprintf(h, sizeof h, "rc=%d", rc);
    pruefe("rc 1 meldet: hinter dem Ende stehen Nichtnull-Bytes", rc == 1, h);
    pruefe("hinter dem Ende wird nichts als Record ausgegeben",
           r[2].track == 0 && r[2].sector == 0 && r[2].data == NULL, NULL);
}

static void t_kein_loeschvermerk(void)
{
    puts("4. GEOS kennt keinen Loeschvermerk");
    geos_vlir_record_t ff = {0xFF, 0x00, 0, NULL};
    geos_vlir_record_t leer = {0x00, 0xFF, 0, NULL};
    /* _DeleteRecord schiebt die Tafel zusammen; eine Spur 0xFF als
     * "geloescht" zu lesen, war eine Erfindung. */
    pruefe("FF/00 gilt nicht als geloescht", !geos_vlir_record_deleted(&ff), NULL);
    pruefe("00/FF gilt nicht als geloescht", !geos_vlir_record_deleted(&leer), NULL);
}

static void t_volle_tafel_und_rundlauf(void)
{
    puts("5. 127 Plaetze ohne Ende-Marke, und der Rundlauf erhaelt jede Luecke");
    uint8_t tafel[254];
    for (int i = 0; i < 127; i++) {
        if (i % 3 == 1)
            setze(tafel, i, 0x00, 0xFF);
        else
            setze(tafel, i, (uint8_t)(1 + i % 35), (uint8_t)(i % 17));
    }
    geos_vlir_record_t r[GEOS_MAX_VLIR_RECORDS];
    int n = -1;
    int rc = geos_parse_vlir_index(tafel, r, &n);
    char h[80];
    snprintf(h, sizeof h, "rc=%d, Plaetze=%d", rc, n);
    pruefe("127 Plaetze", rc == 0 && n == 127, h);

    uint8_t zurueck[254];
    memset(zurueck, 0xAA, sizeof zurueck);
    rc = geos_write_vlir_index(r, n, zurueck);
    pruefe("geos_write_vlir_index schreibt", rc == 0, NULL);
    pruefe("byteidentisch zurueck -- jede 00/FF-Luecke an ihrem Platz",
           memcmp(tafel, zurueck, sizeof tafel) == 0, NULL);
}

int main(void)
{
    puts("=== GEOS-VLIR-Record-Block gegen GEOS.TXT, CVT.TXT und den Kernal (H-12) ===");
    t_beispiel_aus_geos_txt();
    t_luecke_bleibt_ein_platz();
    t_ende_ist_ende();
    t_kein_loeschvermerk();
    t_volle_tafel_und_rundlauf();
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
