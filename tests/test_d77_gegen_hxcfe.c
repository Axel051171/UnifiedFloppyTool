/**
 * @file test_d77_gegen_hxcfe.c
 * @brief D77/D88 gegen einen Container von fremder Hand (MF-1061)
 *
 * `d77` stand auf **T2**: der Leser ist gegen die D88-Beschreibung von
 * pc98.org gepruefte Arbeit und hat mit `test_d88_header_variants` und
 * `test_d88_error_marks` zwei Tests — aber im Korpus lag kein
 * D88-Container, den eine fremde Hand erzeugt hat. Alle bisherigen
 * Pruefdateien baut der Baum selbst.
 *
 * **Das ist bei einem CONTAINER schwerer zu ertragen als bei einem
 * flachen Abbild.** Ein flaches Sektorabbild hat nichts, was man
 * missverstehen koennte; ein D88 hat einen 32-Byte-Kopf, eine Tafel mit
 * 164 Spurzeigern und je Sektor einen 16-Byte-Kopf. Genau dort sass der
 * Fehler bei `apridisk` (MF-1009), bei `qrst` (MF-1028) und bei `pri`
 * (MF-1036) — und in allen dreien war der Rundlauftest gruen, weil
 * Schreiber und Leser Spiegelbilder derselben Erfindung waren.
 *
 * ── Der Kanal ist derselbe wie bei MF-1060 ──────────────────────────────
 *
 * hxcfes Modulliste fuehrt `NEC_D88;RW`. Die Eingabe ist ein
 * **IMD-Traeger** (40 x 2 x 16 x 256, Sektornummern ab 1,
 * selbstbenennende Sektoren nach MF-1020), eigenstaendig in Python
 * geschrieben nach Dave Dunfields oeffentlicher ImageDisk-Beschreibung —
 * ausdruecklich NICHT ueber UFTs `uft_imd.c`, sonst waere die Kette ein
 * geschlossener Kreis.
 *
 *     hxcfe -finput:<imd> -conv:NEC_D88 -foutput:<d77>
 *
 * ── Was hxcfe SELBST gesetzt hat, gemessen an der Datei ─────────────────
 *
 *     Diskettenname (0x00)    „HxCFE"
 *     Schreibschutz (0x1A)    0x00
 *     Medienkennung (0x1B)    0x00   = 2D
 *     Gesamtgroesse (0x1C)    348 848 — und die Datei ist 348 848 Byte
 *     Spurtafel (0x20)        80 von 164 Eintraegen belegt = 40 x 2
 *     Sektorkopf Spur 0       C=0 H=0 R=1 N=1 SPT=16
 *
 * Die Groesse bei 0x1C stimmt mit der tatsaechlichen Dateilaenge
 * ueberein — das ist ein **Beleg am Objekt**, keine Zusicherung: das
 * Feld haette auch falsch sein koennen, und ein Leser, der es ignoriert,
 * haette es nie bemerkt. Dieselbe Art Beweis wie bei MF-869, MF-1013 und
 * MF-1028 (die QRST-Pruefsumme).
 *
 * Der Ueberhang gegenueber der Nutzlast ist **21 168 Byte**: 32 Kopf +
 * 656 Spurtafel + 80 x 16 x 16 Sektorkoepfe = 32 + 656 + 20 480. Auch
 * das geht auf.
 *
 * ── Was NICHT geprueft ist ──────────────────────────────────────────────
 *
 * * **Die anderen Medienkennungen.** hxcfe schreibt 0x00 (2D); 0x10
 *   (2DD) und 0x20 (2HD) bleiben ohne Fremderzeugnis.
 * * **Fehlerhafte Sektoren.** Das deckt `test_d88_error_marks` mit
 *   selbstgebauten Dateien ab; hxcfe schreibt nur fehlerfreie.
 * * **Mehrere Disketten in einer Datei.** D88 sieht das vor, hxcfe
 *   schreibt eine.
 * * **Der Schreibpfad.** Hier wird nur gelesen.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"   /* uft_track_release — implizite Deklaration
                              * ist auf macOS-Clang ein FEHLER */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR ""
#endif

extern const uft_format_plugin_t uft_format_plugin_d77;

#define ZYL       40u
#define KOEPFE     2u
#define SPT       16u
#define SGR      256u
#define NUTZLAST (ZYL * KOEPFE * SPT * SGR)   /* 327 680 */
#define DATEI    348848u
#define NAMENSLAENGE 17u

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung, const char *detail)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s - %s\n", was, detail ? detail : ""); }
}

static void soll_inhalt(unsigned c, unsigned h, unsigned s, uint8_t *b)
{
    char k[24];
    unsigned i;
    snprintf(k, sizeof k, "UFT-K C%02u H%u S%02u ", c, h, s);
    memcpy(b, k, NAMENSLAENGE);
    for (i = NAMENSLAENGE; i < SGR; i++)
        b[i] = (uint8_t)((c * 37u + h * 101u + s * 7u
                          + (i - NAMENSLAENGE) * 3u) & 0xFFu);
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int main(void)
{
    const uft_format_plugin_t *p = &uft_format_plugin_d77;
    uft_disk_t disk;
    char pfad[600], det[300], erster[200];
    uint8_t kopf[0x20], soll[SGR];
    FILE *f;
    long gr;
    unsigned c, s, gesehen = 0, gleich = 0, falsch = 0, id_falsch = 0;
    int h, konf, ok;
    size_t gelesen;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("D77/D88 gegen einen hxcfe-Container - MF-1061\n");
    printf("=============================================\n");

    if (UFT_CORPUS_DIR[0] == '\0') {
        printf("SKIP: UFT_CORPUS_DIR nicht gesetzt - ohne Korpus prueft "
               "dieser Test nichts.\n");
        return 77;      /* SKIP_RETURN_CODE, seit MF-598 */
    }
    snprintf(pfad, sizeof pfad, "%s/hxcfe_uftk_nec_2d.d77", UFT_CORPUS_DIR);
    f = fopen(pfad, "rb");
    if (!f) { printf("SKIP: %s fehlt.\n", pfad); return 77; }
    fseek(f, 0, SEEK_END);
    gr = ftell(f);
    fseek(f, 0, SEEK_SET);
    gelesen = fread(kopf, 1, sizeof kopf, f);
    fclose(f);

    snprintf(det, sizeof det, "%ld Byte, gelesen %u Kopfbyte",
             gr, (unsigned)gelesen);
    pruefe("der Container ist 348 848 Byte gross",
           gr == (long)DATEI && gelesen == sizeof kopf, det);

    /* Beleg am Objekt: das Groessenfeld steht IN der Datei und stimmt
     * mit ihrer tatsaechlichen Laenge ueberein. Ein Leser, der es
     * ignoriert, haette einen Fehler dort nie bemerkt. */
    snprintf(det, sizeof det, "Feld 0x1C = %u, Datei = %ld",
             le32(kopf + 0x1C), gr);
    pruefe("das Groessenfeld bei 0x1C nennt genau die Dateilaenge",
           le32(kopf + 0x1C) == (uint32_t)gr, det);

    snprintf(det, sizeof det, "0x%02X", kopf[0x1B]);
    pruefe("die Medienkennung bei 0x1B ist 0x00 (2D)", kopf[0x1B] == 0x00,
           det);

    snprintf(det, sizeof det, "Ueberhang %u Byte (32 Kopf + 656 Spurtafel "
             "+ %u Sektorkoepfe)",
             (unsigned)(DATEI - NUTZLAST), ZYL * KOEPFE * SPT * 16u);
    pruefe("der Ueberhang geht auf: 32 + 656 + 80 x 16 x 16 = 21 168",
           DATEI - NUTZLAST == 32u + 656u + ZYL * KOEPFE * SPT * 16u, det);

    /* Die Sonde GENAU so rufen, wie der Produktionspfad es tut:
     * `src/core/uft_format_plugin.c:365-366` liest hoechstens
     * UFT_PROBE_BUFFER_SIZE Byte und uebergibt die WIRKLICHE
     * Dateigroesse als drittes Argument.
     *
     * Der erste Entwurf gab hier nur die 32 Kopfbytes weiter und bekam
     * `probe=0` — was wie ein Befund aussah und keiner war: `d77_probe`
     * verlangt `size >= 0x2A0`, weil es die SPURZEIGERTAFEL braucht,
     * nicht bloss den Kopf. Ein zu kleiner Puffer im Test ist keine
     * Aussage ueber das Plugin (Lehre MF-1029/MF-1059: die
     * Groessenquelle gehoert nachgebaut, nicht erraten). */
    {
        size_t pgr = ((size_t)gr < (size_t)UFT_PROBE_BUFFER_SIZE)
                         ? (size_t)gr : (size_t)UFT_PROBE_BUFFER_SIZE;
        uint8_t *puffer = (uint8_t *)malloc(pgr);
        if (!puffer) { printf("kein Speicher\n"); return 1; }
        f = fopen(pfad, "rb");
        if (!f || fread(puffer, 1, pgr, f) != pgr) {
            if (f) fclose(f);
            free(puffer);
            pruefe("Sondenpuffer liess sich lesen", 0, pfad);
            printf("\n%d gruen, %d rot\n", gruen, rot);
            return 1;
        }
        fclose(f);
        konf = -1;
        ok = p->probe(puffer, pgr, (size_t)gr, &konf) ? 1 : 0;
        snprintf(det, sizeof det, "probe=%d konf=%d (Puffer %u Byte)",
                 ok, konf, (unsigned)pgr);
        pruefe("die Sonde nimmt den fremden Container auf dem "
               "Produktionspfad an", ok && konf >= 50, det);
        free(puffer);
    }

    memset(&disk, 0, sizeof disk);
    if (p->open(&disk, pfad, true) != UFT_OK) {
        pruefe("open liest den hxcfe-Container", 0, pfad);
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }

    snprintf(det, sizeof det, "%d x %d x %d x %d",
             disk.geometry.cylinders, disk.geometry.heads,
             disk.geometry.sectors, disk.geometry.sector_size);
    pruefe("Geometrie 40 x 2 x 16 x 256 aus der fremden Spurtafel",
           disk.geometry.cylinders == (int)ZYL
           && disk.geometry.heads == (int)KOEPFE
           && disk.geometry.sectors == (int)SPT
           && disk.geometry.sector_size == (int)SGR, det);

    erster[0] = 0;
    for (c = 0; c < ZYL; c++) {
        for (h = 0; h < (int)KOEPFE; h++) {
            uft_track_t t;
            memset(&t, 0, sizeof t);
            if (p->read_track(&disk, (int)c, h, &t) != UFT_OK) {
                falsch += SPT;
                if (!erster[0])
                    snprintf(erster, sizeof erster,
                             "Spur %u/%d nicht lesbar", c, h);
                continue;
            }
            for (s = 0; s < t.sector_count; s++) {
                const uft_sector_t *sec = &t.sectors[s];
                size_t len = sec->data_len ? sec->data_len : sec->data_size;
                gesehen++;
                soll_inhalt(c, (unsigned)h, s, soll);
                if (sec->data && len == SGR
                    && memcmp(sec->data, soll, SGR) == 0) gleich++;
                else {
                    falsch++;
                    if (!erster[0])
                        snprintf(erster, sizeof erster,
                                 "Spur %u/%d Sektor %u traegt '%.17s'",
                                 c, h, s,
                                 sec->data ? (const char *)sec->data
                                           : "(NULL)");
                }
                /* Der Traeger hat 1..16 geschrieben; der Sektorkopf in
                 * der D88 traegt R=1 fuer den ersten. */
                if ((unsigned)sec->id.sector != s + 1u) id_falsch++;
            }
            uft_track_release(&t);
        }
    }
    p->close(&disk);

    snprintf(det, sizeof det, "%u gesehen, %u gleich, %u falsch%s%s",
             gesehen, gleich, falsch, erster[0] ? " - " : "", erster);
    pruefe("1280 von 1280 Sektoren byteidentisch, jeder an der Stelle, "
           "die die fremde Spurtafel ihm zuweist",
           gesehen == ZYL * KOEPFE * SPT && gleich == gesehen
           && falsch == 0, det);

    snprintf(det, sizeof det, "%u Sektoren mit falscher Nummer", id_falsch);
    pruefe("die Sektornummern kommen aus dem Sektorkopf und sind 1..16",
           id_falsch == 0, det);

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
