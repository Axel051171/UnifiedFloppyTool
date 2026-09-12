/**
 * @file test_ti99_gegen_hxcfe.c
 * @brief Beide TI-99-Leser gegen ein Abbild von fremder Hand (MF-1060)
 *
 * ── Warum es dieses Fixture vorher nicht gab ────────────────────────────
 *
 * MF-1021 hat genau das versucht und ist gescheitert — und der Befund war
 * damals wertvoll: hxcfe meldete Erfolg, erzeugte 184 320 Byte (die
 * richtige Groesse) und die Datei war zu **100 % 0xF6**, das
 * Formatier-Fuellbyte. Null Mustertreffer. Daraus wurde die Regel
 * „ein erzeugtes Fixture ist erst dann ein Beleg, wenn sein INHALT
 * nachgewiesen ist", und `v9t9` blieb ohne Fremdabbild.
 *
 * Die Ursache stand dabei schon da: *„sein RAW-Lader erkennt die
 * TI-Geometrie nicht und sein Layout-Verzeichnis hat keinen TI-Eintrag."*
 * Gemessen am 2026-09-12 stimmt beides — `hxcfe -rawlist` fuehrt keine
 * Anordnung fuer TI-99.
 *
 * **Aber das ist eine Aussage ueber den benutzten Aufruf, nicht ueber das
 * Werkzeug.** Dieselbe Gestalt wie MF-1033, wo „libdsk erzeugt keine
 * Fixtures" sich als fehlender Schalter `-format <name>` herausstellte,
 * und wie MF-1024 (`kfx`).
 *
 * Der fehlende Kanal ist hier ein **Traegerformat, das seine Geometrie
 * selbst mitbringt**: ImageDisk nennt je Spur Zylinder, Kopf,
 * Sektorzahl, Sektorgroesse und je Sektor dessen Nummer ausdruecklich.
 * Ein Lader, der IMD liest, braucht keine Layout-Tabelle. hxcfes
 * `IMD_IMG` liest, `TI994A_V9T9` schreibt — beide `RW` in seiner
 * Modulliste.
 *
 * ── Wie das Fixture entstanden ist ──────────────────────────────────────
 *
 *   1. Eine IMD-Datei, 40 x 2 x 9 x 256, mit selbstbenennenden Sektoren
 *      (MF-1020), geschrieben von einem eigenstaendigen Python-Schreiber
 *      nach Dave Dunfields oeffentlicher ImageDisk-Beschreibung —
 *      ausdruecklich NICHT ueber UFTs `uft_imd.c`, sonst waere die Kette
 *      ein geschlossener Kreis (MF-1009 `apridisk`, MF-1028 `qrst`).
 *   2. `hxcfe -finput:<imd> -conv:TI994A_V9T9 -foutput:<v9t9>`
 *
 * hxcfe legt die Spuren dabei **selbst** an. Das ist der ganze Punkt.
 *
 * ── Der Inhalt ist nachgewiesen, und er UNTERSCHEIDET ───────────────────
 *
 * Gemessen an der erzeugten Datei, mit einer Rechnung ausserhalb von UFT:
 *
 *     haeufigstes Byte           0x20 mit  1,9 %   (kein Fuellbyte)
 *     Mustertreffer „UFT-K C"         720 von 720
 *     nach MAMEs TI-Regel gleich      720
 *     Gegenprobe LINEAR gleich         18
 *
 * Die letzte Zeile ist die wichtigste: haette hxcfe die Spuren linear
 * abgelegt, waere die Datei als Beleg wertlos, weil sie zwischen der
 * richtigen und der falschen Anordnung nicht unterscheidet. Sie tut es —
 * und die **18** sind kein Zufallsrest, sondern genau 2 Spuren x 9
 * Sektoren. MF-1027 hat diese zwei Spuren benannt: (0,0) und (26,1),
 * weil `2*40 - c - 1 == c*2 + 1` bei c = 26 aufgeht. Eine unabhaengige
 * Bestaetigung der damaligen Erklaerung, aus einer fremden Datei.
 *
 * ── Und eine der drei Groessen ist AUSGESCHLOSSEN, nicht vergessen ──────
 *
 * Dieselbe Kette wurde fuer alle drei TI-Geometrien gefahren:
 *
 *     SSSD  92 160 B   360 Mustertreffer, aber Gegenprobe linear ebenfalls
 *                      360 — einseitig fallen beide Formeln zusammen
 *                      (MF-1027 erklaert warum), also unterscheidet die
 *                      Datei nichts.
 *     DSSD 184 320 B   720/720, Gegenprobe 18  -> DIESES Fixture.
 *     DSDD 368 640 B   **hxcfe ist gescheitert**: 50,2 % 0xF6 und nur
 *                      720 von 1440 Mustertreffern — es schrieb 9 statt
 *                      18 Sektoren je Spur und fuellte den Rest.
 *
 * Der DSDD-Fall ist die MF-1021-Falle, ein zweites Mal und an derselben
 * Stelle — nur diesmal gefangen, bevor er ein Fixture wurde. Er steht
 * hier, damit niemand ihn spaeter „nachholt".
 *
 * ── Warum der Test ZWEI Plugins prueft ──────────────────────────────────
 *
 * Der Baum hat fuer TI-99 zwei Leser, und MF-1057 hat gerade gemessen,
 * dass `xdm86` woertlich die zwei Fehler trug, die MF-1027 an `v9t9`
 * behoben hatte. Beide lesen 184 320 Byte als 40 x 2 x 9 x 256. Ein
 * Fixture, das nur einen von beiden prueft, liesse den Nachbarn wieder
 * ungedeckt — genau die Lage aus MF-519/MF-529 und MF-1026.
 *
 * ── Was NICHT geprueft ist ──────────────────────────────────────────────
 *
 * * **Die Zweideutigkeit bei 184 320 Byte** (SSDD 1x40x18 gegen DSSD
 *   2x40x9, MAMEs eigener Kommentar). Dieses Fixture ist DSSD; `v9t9`
 *   liest die VIB und folgt ihr, `xdm86` nimmt fest 40x2x9. Die Frage,
 *   ob es zwei Plugins fuer ein Format braucht, bleibt P3-340.
 * * **Die Fehlsektorkarte** (+768 Byte), die MAME toleriert.
 * * **Der Schreibpfad.** Hier wird nur gelesen.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"   /* uft_track_release — ohne diesen Header eine
                              * implizite Deklaration, und die ist auf
                              * macOS-Clang ein FEHLER, nicht nur eine
                              * Warnung (gcc laesst sie durch). */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR ""
#endif

extern const uft_format_plugin_t uft_format_plugin_v9t9;
extern const uft_format_plugin_t uft_format_plugin_xdm86;

#define ZYL      40u
#define KOEPFE    2u
#define SPT       9u
#define SGR     256u
#define GESAMT  (ZYL * KOEPFE * SPT * SGR)   /* 184 320 */

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung, const char *detail)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s - %s\n", was, detail ? detail : ""); }
}

/* Dieselbe Selbstbenennung, die der IMD-Traeger trug (MF-1020).
 *
 * Die Kennung ist **17** Byte lang — „UFT-K " 6 + „C00 " 4 + „H0 " 3 +
 * „S00 " 4. Der erste Entwurf kopierte 16 und begann die Fuellung dort;
 * gemessen war dann JEDER Sektor „falsch", obwohl die ersten 16 Byte
 * stimmten. Das war ein Fehler in der Erwartung, nicht im Leser — und
 * genau die Klasse, vor der MF-1014/1026/1028 warnen („rot aus dem
 * falschen Grund" ist so gefaehrlich wie gruen aus dem falschen). */
#define NAMENSLAENGE 17u

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

/* Liest das ganze Abbild durch EIN Plugin und prueft jeden Sektor gegen
 * die Stelle, die MAMEs Regel ihm zuweist. */
static void durchlauf(const uft_format_plugin_t *p, const char *pfad,
                      const char *wer)
{
    uft_disk_t disk;
    unsigned c, s, gesehen = 0, gleich = 0, falsch = 0;
    int h;
    char det[300], erster[200];
    uint8_t soll[SGR];

    erster[0] = 0;
    memset(&disk, 0, sizeof disk);
    if (p->open(&disk, pfad, true) != UFT_OK) {
        snprintf(det, sizeof det, "%s: open scheitert", wer);
        pruefe("open liest das hxcfe-Abbild", 0, det);
        return;
    }

    snprintf(det, sizeof det, "%s: %d x %d x %d x %d", wer,
             disk.geometry.cylinders, disk.geometry.heads,
             disk.geometry.sectors, disk.geometry.sector_size);
    pruefe("Geometrie 40 x 2 x 9 x 256",
           disk.geometry.cylinders == (int)ZYL
           && disk.geometry.heads == (int)KOEPFE
           && disk.geometry.sectors == (int)SPT
           && disk.geometry.sector_size == (int)SGR, det);

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
                                 "Spur %u/%d Sektor %u traegt '%.16s'",
                                 c, h, s,
                                 sec->data ? (const char *)sec->data
                                           : "(NULL)");
                }
            }
            uft_track_release(&t);
        }
    }
    p->close(&disk);

    snprintf(det, sizeof det, "%s: %u gesehen, %u gleich, %u falsch%s%s",
             wer, gesehen, gleich, falsch, erster[0] ? " - " : "", erster);
    pruefe("720 von 720 Sektoren byteidentisch an der Stelle, die "
           "MAMEs TI-Regel ihnen zuweist",
           gesehen == ZYL * KOEPFE * SPT && gleich == gesehen
           && falsch == 0, det);
}

int main(void)
{
    char pfad[600], det[200];
    FILE *f;
    long gr;
    int konf, ok;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("TI-99: beide Leser gegen ein hxcfe-Erzeugnis - MF-1060\n");
    printf("======================================================\n");

    if (UFT_CORPUS_DIR[0] == '\0') {
        printf("SKIP: UFT_CORPUS_DIR nicht gesetzt - ohne Korpus prueft "
               "dieser Test nichts.\n");
        return 77;      /* SKIP_RETURN_CODE, seit MF-598 fuer alle Tests */
    }
    snprintf(pfad, sizeof pfad, "%s/hxcfe_uftk_ti99_dssd.v9t9",
             UFT_CORPUS_DIR);
    f = fopen(pfad, "rb");
    if (!f) {
        printf("SKIP: %s fehlt.\n", pfad);
        return 77;
    }
    fseek(f, 0, SEEK_END);
    gr = ftell(f);
    fclose(f);

    snprintf(det, sizeof det, "%ld Byte", gr);
    pruefe("das Fremdabbild ist 184 320 Byte gross", gr == (long)GESAMT,
           det);

    /* Beide Sonden nehmen es an. `v9t9` liest dabei die VIB und darf
     * hoeher liegen als `xdm86`, das nur die Groesse kennt — die
     * Bandordnung nach MF-729 entscheidet das Erkennungsrennen von
     * selbst, ohne dass eine Zahl gegen die andere gesetzt wird. */
    konf = -1;
    ok = uft_format_plugin_v9t9.probe(NULL, 0, (size_t)gr, &konf) ? 1 : 0;
    snprintf(det, sizeof det, "probe=%d konf=%d", ok, konf);
    pruefe("die v9t9-Sonde nimmt 184 320 Byte an", ok, det);

    konf = -1;
    ok = uft_format_plugin_xdm86.probe(NULL, 0, (size_t)gr, &konf) ? 1 : 0;
    snprintf(det, sizeof det, "probe=%d konf=%d", ok, konf);
    pruefe("die xdm86-Sonde nimmt 184 320 Byte an", ok, det);

    printf("\n  -- durch uft_format_plugin_v9t9 --\n");
    durchlauf(&uft_format_plugin_v9t9, pfad, "v9t9");

    printf("\n  -- durch uft_format_plugin_xdm86 --\n");
    durchlauf(&uft_format_plugin_xdm86, pfad, "xdm86");

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
