/**
 * @file test_do_gegen_hxcfe.c
 * @brief DO durch das Plugin gegen ein hxcfe-Erzeugnis (MF-1067)
 *
 * `do` stand auf **T2**. Der Grund stand ausdruecklich im Manifest und war
 * ehrlich: die einzige DO-Datei im Korpus ist `uftk_dos33_35trk.do`, und
 * ihr Eintrag sagt `origin: "derived"` — *„UFT-eigen (MF-1050) — kein
 * Fremdwerkzeug, daher KEIN Stufenkredit"*. MF-716 hatte den Leser zwar
 * gegen `to_woz2` gehalten (560 von 560 byteidentisch), aber das Abbild
 * selbst kam aus UFTs eigener Hand.
 *
 * ── Der Weg, der das aufloest ────────────────────────────────────────────
 *
 * Seit MF-1066 liegt ein **fremd erzeugtes** WOZ2 im Korpus
 * (`to_woz2_uftk_dos33.woz`). Und hxcfe kann WOZ **lesen**
 * (`APPLEII_WOZ;R `) und DO **schreiben** (`APPLE2_DO;RW`). Damit ist die
 * Kette:
 *
 *     uftk_dos33_35trk.do   (UFT)
 *         -> to_woz2        -> GCR-Bitstrom in einer WOZ2   (fremd)
 *         -> hxcfe          -> WOZ-Lader + DO-Schreiber     (fremd)
 *         -> hxcfe_woz2do_dos33.do
 *
 * **Drei fremde Umsetzungen zwischen Eingang und Ausgang**, und keine
 * davon kennt UFTs Versatzformel.
 *
 * ── Warum die GLEICHHEIT der Beleg ist, nicht ihr Fehlen ────────────────
 *
 * Das Ergebnis ist mit der UFT-eigenen Datei **byteidentisch** — 143 360
 * Byte, 0 abweichende Byte, SHA-256 `e7145bf9263eacf9…`. Das ist kein
 * Selbstgespraech, sondern genau die Lage aus MF-1033 (`nanowasp`): zwei
 * unabhaengige Schreiber, dieselben Bytes, jeder Sektor an derselben
 * Stelle — **die Uebereinstimmung IST die Aussage**. Waere hxcfes
 * Vorstellung von der DOS-3.3-Verschraenkung oder von der Versatzformel
 * eine andere, laegen die 560 Sektoren woanders.
 *
 * ── Die Gegenprobe, die aus derselben Messung faellt ────────────────────
 *
 * Dasselbe hxcfe schreibt aus **derselben** WOZ mit `-conv:APPLE2_PO` eine
 * Datei, die sich in **119 420 von 143 360 Byte** unterscheidet. Das
 * Werkzeug reicht also nicht durch — es ordnet an, und die DO-Anordnung
 * ist eine Entscheidung, keine Kopie. (Die PO-Datei wird nicht ins Korpus
 * gelegt: `po` steht seit MF-783 auf T1b und braucht sie nicht.)
 *
 * ── Wie dieser Test diskriminiert, OHNE eine Tafel zu glauben ───────────
 *
 * MF-463 steht im Kopf von `src/formats/do/uft_do.c`: DO und PO sind
 * beide 143 360 Byte gross und unterscheiden sich **nur** in der
 * Reihenfolge der Sektoren 1..14; die Sonde kann das nicht sehen, weil
 * der erste Unterschied bei 0x11000 liegt — hinter dem 65 536 Byte
 * grossen Sondenpuffer. Eine Zusage, die nur Groesse und Sektorzahl
 * prueft, wuerde eine PO-Datei genauso gruen melden.
 *
 * Dieser Test prueft deshalb **je Sektor den Inhalt gegen den Namen, den
 * der Sektor selbst traegt**: die Pruefdatei ist selbstbenennend nach
 * MF-1020, jeder Sektor beginnt mit `"UFT-K Tnn Snn "`. Damit sagt ein
 * Treffer nicht nur, DASS Bytes kamen, sondern dass die RICHTIGE Stelle
 * getroffen wurde — und es braucht dafuer keine Interleave-Tafel, der
 * die Pruefung glauben muesste.
 *
 * ── Zwei Befunde am Werkzeug, nicht am Format ───────────────────────────
 *
 * **Erstens: `id.track` ist kein Alias, sondern ein totes Feld.** Die
 * erste Fassung der Spur-Zusage unten las `gefunden->id.track`, weil der
 * Header dazu einlaedt — `uft_sector_id_t` fuehrt das Feld mit dem
 * Kommentar „Alias for cylinder (legacy code)". Es ist keiner: beide sind
 * eigene `uint8_t`, `uft_format_add_sector_with_id()` setzt allein
 * `cylinder`, und `track` bleibt beim `memset` auf 0. Gemessen ueber den
 * ganzen Baum hat `id.track` **einen** Schreiber (`uft_stx_air.c:334`)
 * und **null** Leser. Der Test meldete damit 544 von 560 falsch — an
 * einem Leser, der richtig lag. Das ist die Gestalt von MF-1065, wo
 * dieselbe Struktur zwei CRC-Felder fuehrt (`id_crc_ok` gegen
 * `id.crc_ok`) und die Zusage das tote las; **zum zweiten Mal in
 * derselben Struktur**.
 *
 * **Zweitens, und das wiegt schwerer: die Mutationsmatrix konnte es nicht
 * sehen.** Sie meldete „7 von 7 gefangen", waehrend der Test schon ohne
 * jede Mutation rot war — jede Mutation galt als gefangen, weil der Test
 * IMMER fiel. Eine Matrix ohne **Grundlauf** unterscheidet nicht zwischen
 * „die Mutation wurde gefangen" und „der Test kann gar nicht gruen
 * werden". Das ist dieselbe Klasse wie MF-1040, wo die Matrix nur
 * `[ROT]`-Zeilen auswertete und einen Absturz als durchgerutscht fuehrte.
 * Seither laeuft die Matrix **zuerst unveraendert** und bricht ab, wenn
 * dieser Lauf nicht gruen ist.
 *
 * ── Was NICHT geprueft ist ──────────────────────────────────────────────
 *
 * * **Der Schreibpfad.** `do_write_track()` gibt es; dieser Test fasst
 *   ihn nicht an.
 * * **Die Sondenkonfidenz gegen PO.** Dass die Sonde DO und PO nicht
 *   trennen kann, ist seit MF-463 benannt und nach MF-729 die ehrliche
 *   Antwort — nicht ein Fehler, den dieser Test schliessen koennte.
 * * **Andere Geometrien.** DO ist auf 35 x 16 x 256 festgelegt.
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

extern const uft_format_plugin_t uft_format_plugin_do;

#define SPUREN      35u
#define SEKTOREN    16u
#define SGR        256u
#define GROESSE     (SPUREN * SEKTOREN * SGR)   /* 143 360 */

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung, const char *detail)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s - %s\n", was, detail ? detail : ""); }
}

int main(void)
{
    const uft_format_plugin_t *p = &uft_format_plugin_do;
    uft_disk_t disk;
    char pfad[600], eigen[600], det[300], erster[220];
    uint8_t *fremd = NULL, *selbst = NULL;
    FILE *f;
    long gr = 0, gr2 = 0;
    unsigned c, s, gesamt = 0, gleich = 0, falsch = 0, fehlend = 0;
    unsigned namen_ok = 0, namen_falsch = 0;
    unsigned spur_id_ok = 0, spur_id_falsch = 0;
    int konf = -1, ok;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("DO durch das Plugin gegen hxcfe - MF-1067\n");
    printf("=========================================\n");

    if (UFT_CORPUS_DIR[0] == '\0') {
        printf("SKIP: UFT_CORPUS_DIR nicht gesetzt.\n");
        return 77;      /* SKIP_RETURN_CODE, seit MF-598 */
    }
    snprintf(pfad, sizeof pfad, "%s/hxcfe_woz2do_dos33.do", UFT_CORPUS_DIR);
    snprintf(eigen, sizeof eigen, "%s/uftk_dos33_35trk.do", UFT_CORPUS_DIR);

    f = fopen(pfad, "rb");
    if (!f) { printf("SKIP: %s fehlt.\n", pfad); return 77; }
    fseek(f, 0, SEEK_END); gr = ftell(f); fseek(f, 0, SEEK_SET);
    fremd = (uint8_t *)malloc((size_t)(gr > 0 ? gr : 1));
    if (!fremd || fread(fremd, 1, (size_t)gr, f) != (size_t)gr) {
        fclose(f); free(fremd); printf("SKIP: %s unlesbar.\n", pfad);
        return 77;
    }
    fclose(f);

    snprintf(det, sizeof det, "%ld Byte", gr);
    pruefe("das hxcfe-Erzeugnis ist 143 360 Byte gross",
           gr == (long)GROESSE, det);

    /* Die Gleichheit mit der UFT-eigenen Datei IST die Aussage (MF-1033).
     * Sie steht deshalb als eigene Zusage da und nicht als Randbemerkung. */
    f = fopen(eigen, "rb");
    if (f) {
        fseek(f, 0, SEEK_END); gr2 = ftell(f); fseek(f, 0, SEEK_SET);
        selbst = (uint8_t *)malloc((size_t)(gr2 > 0 ? gr2 : 1));
        if (selbst && fread(selbst, 1, (size_t)gr2, f) != (size_t)gr2) {
            free(selbst); selbst = NULL;
        }
        fclose(f);
    }
    if (selbst && gr2 == gr) {
        unsigned abweichend = 0;
        long i;
        for (i = 0; i < gr; i++) if (fremd[i] != selbst[i]) abweichend++;
        snprintf(det, sizeof det, "%u von %ld Byte abweichend",
                 abweichend, gr);
        pruefe("hxcfes Ausgabe ist mit der UFT-eigenen Datei BYTEIDENTISCH "
               "- drei fremde Umsetzungen, dieselbe Anordnung",
               abweichend == 0, det);
    } else {
        pruefe("hxcfes Ausgabe ist mit der UFT-eigenen Datei BYTEIDENTISCH",
               0, "Vergleichsdatei fehlt oder hat andere Groesse");
    }

    {
        uint8_t kopf[64];
        memcpy(kopf, fremd, sizeof kopf);
        ok = p->probe(kopf, sizeof kopf, (size_t)gr, &konf) ? 1 : 0;
        snprintf(det, sizeof det, "probe=%d konf=%d", ok, konf);
        /* Nach MF-729 ist hier das Band "nur die Groesse" (30-49) die
         * ehrliche Antwort, weil DO und PO dieselbe Groesse haben und
         * sich erst hinter dem Sondenpuffer unterscheiden (MF-463). */
        pruefe("die Sonde nimmt an, beansprucht aber nicht mehr als die "
               "Groesse (Band 30-49)", ok && konf >= 30 && konf < 50, det);
    }

    memset(&disk, 0, sizeof disk);
    if (p->open(&disk, pfad, true) != UFT_OK) {
        pruefe("open liest das Fremderzeugnis", 0, pfad);
        free(fremd); free(selbst);
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }

    snprintf(det, sizeof det, "%d x %d x %d x %d",
             disk.geometry.cylinders, disk.geometry.heads,
             disk.geometry.sectors, disk.geometry.sector_size);
    pruefe("Geometrie 35 x 1 x 16 x 256",
           disk.geometry.cylinders == (int)SPUREN
           && disk.geometry.heads == 1
           && disk.geometry.sectors == (int)SEKTOREN
           && disk.geometry.sector_size == (int)SGR, det);

    erster[0] = 0;
    for (c = 0; c < SPUREN; c++) {
        uft_track_t t;
        memset(&t, 0, sizeof t);
        if (p->read_track(&disk, (int)c, 0, &t) != UFT_OK) {
            fehlend += SEKTOREN;
            if (!erster[0])
                snprintf(erster, sizeof erster, "Spur %u nicht lesbar", c);
            continue;
        }
        for (s = 0; s < SEKTOREN; s++) {
            const uft_sector_t *gefunden = NULL;
            size_t k;
            gesamt++;
            for (k = 0; k < t.sector_count; k++)
                if (t.sectors[k].id.sector == (uint8_t)s) {
                    gefunden = &t.sectors[k];
                    break;
                }
            if (!gefunden || !gefunden->data) {
                fehlend++;
                if (!erster[0])
                    snprintf(erster, sizeof erster,
                             "Spur %u Sektor %u fehlt", c, s);
                continue;
            }
            /* Die Spur-ID im Sektorkopf ist eine EIGENE Aussage. Sie
             * kommt bei einem kopflosen Format nicht aus der Datei,
             * sondern aus dem Leser — und ein Leser, der sie fest auf 0
             * setzt, faellt ohne diese Zusage nicht auf (die Klasse aus
             * MF-1027, wo `v9t9` die Spur falsch fuehrte).
             *
             * **Gelesen wird `id.cylinder`, NICHT `id.track`** — und das
             * ist gemessen, nicht Geschmack: `uft_sector_id_t` fuehrt
             * beide als eigene `uint8_t`, und der Kommentar an `track`
             * sagt „Alias for cylinder (legacy code)". Ein Alias ist es
             * nicht. `uft_format_add_sector_with_id()` setzt allein
             * `cylinder`; `track` bleibt beim `memset` auf 0. Im ganzen
             * Baum hat `id.track` **einen** Schreiber
             * (`uft_stx_air.c:334`) und **null** Leser. Die erste
             * Fassung dieser Zusage las es trotzdem — weil der Header
             * dazu einlaedt — und meldete 544 von 560 falsch. Das ist
             * die Gestalt von MF-1065, wo dieselbe Struktur zwei
             * CRC-Felder fuehrt und der Test das tote las. */
            if (gefunden->id.cylinder == (uint8_t)c) spur_id_ok++;
            else {
                spur_id_falsch++;
                if (!erster[0])
                    snprintf(erster, sizeof erster,
                             "Spur %u Sektor %u fuehrt Zylinder %u", c, s,
                             (unsigned)gefunden->id.cylinder);
            }
            {
                size_t len = gefunden->data_len ? gefunden->data_len
                                                : gefunden->data_size;
                const uint8_t *soll = fremd + ((size_t)c * SEKTOREN + s) * SGR;
                char name[16];
                if (len == SGR && memcmp(gefunden->data, soll, SGR) == 0)
                    gleich++;
                else {
                    falsch++;
                    if (!erster[0])
                        snprintf(erster, sizeof erster,
                                 "Spur %u Sektor %u weicht ab, len=%u",
                                 c, s, (unsigned)len);
                }
                /* Die eigentliche Diskriminierung: der Sektor nennt sich
                 * selbst. Eine PO-Anordnung lieferte hier einen anderen
                 * Namen, und zwar ohne dass Groesse oder Sektorzahl sich
                 * aendern (MF-463 / MF-1020). */
                snprintf(name, sizeof name, "UFT-K T%02u S%02u ", c, s);
                if (len >= 14 && memcmp(gefunden->data, name, 14) == 0)
                    namen_ok++;
                else {
                    namen_falsch++;
                    if (!erster[0] && len >= 14)
                        snprintf(erster, sizeof erster,
                                 "Spur %u Sektor %u nennt sich \"%.13s\"",
                                 c, s, (const char *)gefunden->data);
                }
            }
        }
        uft_track_release(&t);
    }
    p->close(&disk);

    snprintf(det, sizeof det, "%u erwartet, %u gleich, %u falsch, %u fehlend%s%s",
             gesamt, gleich, falsch, fehlend, erster[0] ? " - " : "", erster);
    pruefe("560 von 560 Sektoren byteidentisch mit dem Fremderzeugnis",
           gesamt == SPUREN * SEKTOREN && gleich == gesamt
           && falsch == 0 && fehlend == 0, det);

    snprintf(det, sizeof det, "%u richtig benannt, %u falsch%s%s",
             namen_ok, namen_falsch, erster[0] ? " - " : "", erster);
    pruefe("jeder der 560 Sektoren nennt SEINE EIGENE Spur und Nummer - "
           "eine PO-Anordnung faellt hier durch (MF-463)",
           namen_ok == SPUREN * SEKTOREN && namen_falsch == 0, det);

    snprintf(det, sizeof det, "%u richtig, %u falsch%s%s",
             spur_id_ok, spur_id_falsch, erster[0] ? " - " : "", erster);
    pruefe("jeder Sektor fuehrt SEINE Spurnummer im Sektorkopf - bei einem "
           "kopflosen Format kommt sie vom Leser (MF-1027)",
           spur_id_ok == SPUREN * SEKTOREN && spur_id_falsch == 0, det);

    free(fremd);
    free(selbst);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
