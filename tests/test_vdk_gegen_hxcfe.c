/**
 * @file test_vdk_gegen_hxcfe.c
 * @brief VDK gegen ein Abbild von fremder Hand (MF-1061)
 *
 * `vdk` stand seit MF-1018 auf **T2**: der Leser war gegen MAMEs
 * `formats/vdk_dsk.cpp` (BSD-3-Clause, Dirk Best) berichtigt und
 * bewacht, aber im Korpus lag kein Dragon-32/64-Abbild, das eine fremde
 * Hand erzeugt hat.
 *
 * ── Der Kanal ist derselbe wie bei MF-1060 ──────────────────────────────
 *
 * hxcfes Modulliste fuehrt `DRAGON3264_VDK;RW` — es kann VDK also
 * schreiben. Was fehlte, war eine Eingabe, deren Geometrie es versteht:
 * ein rohes Sektorabbild traegt sie nicht, und `-rawlist` hat keine
 * Dragon-Anordnung.
 *
 * **ImageDisk traegt sie.** Je Spur stehen Zylinder, Kopf, Sektorzahl
 * und Sektorgroesse ausdruecklich in der Datei, je Sektor dessen Nummer.
 * Der IMD-Traeger ist eigenstaendig in Python geschrieben (nach Dave
 * Dunfields oeffentlicher Beschreibung), ausdruecklich NICHT ueber UFTs
 * `uft_imd.c` — sonst waere die Kette ein geschlossener Kreis
 * (MF-1009 `apridisk`, MF-1028 `qrst`).
 *
 *     hxcfe -finput:<imd> -conv:DRAGON3264_VDK -foutput:<vdk>
 *
 * ── Was hxcfe dabei SELBST entschieden hat ──────────────────────────────
 *
 * Der 12-Byte-Kopf der erzeugten Datei, byteweise:
 *
 *     64 6b 0c 00 10 10 00 00 28 02 00 00
 *     ^^^^^ „dk"      ^^ Version   ^^ Spuren 0x28 = 40
 *           ^^^^^ Kopfgroesse 12      ^^ Seiten 2
 *
 * Das sind genau die Felder, die MAMEs `vdk_dsk.cpp` beschreibt und die
 * MF-1018 in `uft_vdk_plugin.c` festgenagelt hat — und zwar von einer
 * anderen Hand gesetzt. Der Kopf ist damit nicht laenger nur
 * nachgerechnet, sondern an einem Fremderzeugnis abgenommen.
 *
 * ── Der Inhalt ist nachgewiesen UND er unterscheidet ────────────────────
 *
 * Gemessen ausserhalb von UFT, an derselben Datei, mit drei Hypothesen
 * ueber die Spuranordnung:
 *
 *     zylinder-verschraenkt  (c * seiten + h)   1440 von 1440
 *     kopf-dur               (h * zyl + c)        36 von 1440
 *     kopf-dur rueckwaerts   (TI-Regel)           36 von 1440
 *
 * Die erste ist UFTs Formel. Haette hxcfe eine der anderen benutzt, waere
 * das hier ein Befund; haetten alle drei gleich gut gepasst, waere die
 * Datei als Beleg wertlos (Klasse MF-1014/1026/1028). Sie unterscheidet.
 *
 * **Deshalb ist das Fixture ZWEISEITIG.** Eine einseitige VDK haette
 * 720/720 fuer alle drei Hypothesen ergeben, weil bei `seiten == 1` jede
 * Formel in `c` uebergeht — genau der Grund, aus dem der Fehler bei
 * `v9t9` (MF-1027) so lange unbemerkt blieb. Dieselbe Ueberlegung hat
 * bei MF-1060 die SSSD-Datei ausgeschlossen.
 *
 * Dazu: haeufigstes Byte 0x20 mit 1,9 %, also **kein Fuellbyte** — die
 * Falle aus MF-1021, wo hxcfe eine Datei richtiger Groesse lieferte, die
 * zu 100 % aus 0xF6 bestand.
 *
 * ── Was NICHT geprueft ist ──────────────────────────────────────────────
 *
 * * **Das Kompressionsbyte.** MF-1018 sagt ab, wenn es gesetzt ist, weil
 *   gepackte Daten als Sektoren gelesen erfundene Daten waeren; MAME ist
 *   dort stumm. hxcfe setzt es nicht (Byte 11 = 0x00), dieser Weg bleibt
 *   also ungeprueft.
 * * **Ein Kopf groesser als 12 Byte.** MF-1018 hat gemessen, dass ein
 *   256-Byte-Kopf im Format vorgesehen ist und `uint8_t` ihn zu 0
 *   verschluckte; hxcfe schreibt 12.
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

extern const uft_format_plugin_t uft_format_plugin_vdk;

#define KOPF     12u
#define ZYL      40u
#define KOEPFE    2u
#define SPT      18u
#define SGR     256u
#define GESAMT  (KOPF + ZYL * KOEPFE * SPT * SGR)   /* 368 652 */

/* Die Kennung des Traegers ist 17 Byte lang: „UFT-K " 6 + „C00 " 4 +
 * „H0 " 3 + „S00 " 4. Bei MF-1060 kostete ein Versatz um eins hier einen
 * vollstaendig roten Lauf, dessen Meldung das RICHTIGE Byte zeigte. */
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

int main(void)
{
    const uft_format_plugin_t *p = &uft_format_plugin_vdk;
    uft_disk_t disk;
    char pfad[600], det[300], erster[200];
    uint8_t kopf[KOPF], soll[SGR];
    FILE *f;
    long gr;
    unsigned c, s, gesehen = 0, gleich = 0, falsch = 0, id_falsch = 0;
    int h, konf, ok;
    size_t gelesen;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("VDK gegen ein hxcfe-Erzeugnis - MF-1061\n");
    printf("=======================================\n");

    if (UFT_CORPUS_DIR[0] == '\0') {
        printf("SKIP: UFT_CORPUS_DIR nicht gesetzt - ohne Korpus prueft "
               "dieser Test nichts.\n");
        return 77;      /* SKIP_RETURN_CODE, seit MF-598 */
    }
    snprintf(pfad, sizeof pfad, "%s/hxcfe_uftk_dragon_ds.vdk",
             UFT_CORPUS_DIR);
    f = fopen(pfad, "rb");
    if (!f) { printf("SKIP: %s fehlt.\n", pfad); return 77; }
    fseek(f, 0, SEEK_END);
    gr = ftell(f);
    fseek(f, 0, SEEK_SET);
    gelesen = fread(kopf, 1, KOPF, f);
    fclose(f);

    snprintf(det, sizeof det, "%ld Byte, gelesen %u Kopfbyte",
             gr, (unsigned)gelesen);
    pruefe("368 652 Byte = 12 Kopf + 40 x 2 x 18 x 256",
           gr == (long)GESAMT && gelesen == KOPF, det);

    /* Die Kopffelder, die eine FREMDE Hand gesetzt hat. */
    snprintf(det, sizeof det, "%02x %02x", kopf[0], kopf[1]);
    pruefe("Kennung \"dk\" (0x6B64 LE) steht bei Versatz 0",
           kopf[0] == 0x64 && kopf[1] == 0x6B, det);

    snprintf(det, sizeof det, "%u", (unsigned)(kopf[2] | (kopf[3] << 8)));
    pruefe("Kopfgroesse 12 steht als LE16 bei Versatz 2",
           (kopf[2] | (kopf[3] << 8)) == (int)KOPF, det);

    snprintf(det, sizeof det, "Spuren %u, Seiten %u", kopf[8], kopf[9]);
    pruefe("Spurzahl 40 bei Versatz 8, Seitenzahl 2 bei Versatz 9",
           kopf[8] == ZYL && kopf[9] == KOEPFE, det);

    snprintf(det, sizeof det, "Byte 11 = 0x%02x", kopf[11]);
    pruefe("das Kompressionsbyte ist 0 - dieser Weg bleibt ungeprueft",
           kopf[11] == 0, det);

    konf = -1;
    ok = p->probe(kopf, KOPF, (size_t)gr, &konf) ? 1 : 0;
    snprintf(det, sizeof det, "probe=%d konf=%d", ok, konf);
    pruefe("die Sonde erkennt das Fremderzeugnis an seiner Kennung",
           ok && konf >= 80, det);

    memset(&disk, 0, sizeof disk);
    if (p->open(&disk, pfad, true) != UFT_OK) {
        pruefe("open liest das hxcfe-Abbild", 0, pfad);
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }

    snprintf(det, sizeof det, "%d x %d x %d x %d",
             disk.geometry.cylinders, disk.geometry.heads,
             disk.geometry.sectors, disk.geometry.sector_size);
    pruefe("Geometrie 40 x 2 x 18 x 256 aus dem fremden Kopf",
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
                /* MAME: FIRST_SECTOR_ID = 1. Der Traeger hat 1..18
                 * geschrieben, hxcfe hat sie uebernommen. */
                if ((unsigned)sec->id.sector != s + 1u) id_falsch++;
            }
            uft_track_release(&t);
        }
    }
    p->close(&disk);

    snprintf(det, sizeof det, "%u gesehen, %u gleich, %u falsch%s%s",
             gesehen, gleich, falsch, erster[0] ? " - " : "", erster);
    pruefe("1440 von 1440 Sektoren byteidentisch, zylinder-verschraenkt "
           "abgelegt",
           gesehen == ZYL * KOEPFE * SPT && gleich == gesehen
           && falsch == 0, det);

    snprintf(det, sizeof det, "%u Sektoren mit falscher Nummer", id_falsch);
    pruefe("die Sektornummern sind 1..18, wie MAMEs FIRST_SECTOR_ID sagt",
           id_falsch == 0, det);

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
