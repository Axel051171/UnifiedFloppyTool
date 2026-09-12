/**
 * @file test_woz_gegen_to_woz2.c
 * @brief WOZ durch das PLUGIN gegen ein to_woz2-Erzeugnis (MF-1066)
 *
 * `woz` stand auf **T2** — mit Tests, aber ohne ein Abbild von fremder
 * Hand. Und die Messung, die das nachholen sollte, hat zuerst etwas
 * anderes gefunden.
 *
 * ── Der Befund: 35 Sektoren statt 560, bei gemeldeten 16 je Spur ───────
 *
 * `woz_plugin_open()` meldet `disk->geometry.sectors = 16` (und
 * `total_sectors = 35 * 1 * 16 = 560`). `woz_plugin_read_track()` legte
 * dagegen den **rohen Bitstrom als „Sektor 0"** ab — ein Sektor je Spur.
 *
 * Gemessen am Erzeugnis unten, vor der Aenderung:
 *
 *     Geometrie : 35 x 1 x 16 x 256
 *     gelesen   : 35 Spuren, **35 Sektoren gesamt**, raw_size 0
 *
 * Das ist die Gestalt von **MF-796**, wo `edsk` „9 Sektoren" meldete und
 * fuer jede Spur keinen einzigen lieferte — still, mit `UFT_OK`. Und der
 * Bitstrom landete nicht einmal in `track->raw_data`, der Stelle, die
 * ein Bitstromformat dafuer hat.
 *
 * ── Die Behebung ist eine VERDRAHTUNG, kein neuer Dekoder ──────────────
 *
 * `uft_apple_gcr_scan_track()` liegt seit MF-948 im Baum, ist
 * oracle-geprueft und wird von `uft_nib.c:118` in Produktion gerufen. Er
 * nimmt genau das, was `woz_plugin_read_track()` in der Hand hat — Bits
 * und Bitzahl — und kann:
 *
 *   * beide Kodierungen, am Adressvorspann erkannt statt vom Aufrufer
 *     mitgegeben (`D5 AA 96` = 6-and-2 / 16 Sektoren, `D5 AA B5` =
 *     5-and-3 / 13 Sektoren, MF-719);
 *   * **genau eine Umdrehung** — MF-715 hat dort gemessen, dass eine zu
 *     grosszuegige Schranke 595 statt 560 Sektoren liefert, alle korrekt
 *     dekodiert und die Zahl trotzdem falsch;
 *   * eine unbekannte Kodierung uebergehen statt raten (MF-721, der
 *     DOS-3.2-Bootsektor).
 *
 * Ihn zu rufen ist das **Verdrahten vorhandenen, unerreichbaren Codes**
 * — von der EINFRIER-REGEL ausdruecklich erlaubt. Es kommt keine Zeile
 * neuer Dekodierlogik hinzu; das Aufrufmuster ist woertlich das aus
 * `uft_nib.c:117-137`.
 *
 * ── Das Fremderzeugnis ──────────────────────────────────────────────────
 *
 * `to_woz2` (Apple-II-Disk-Tools, GPL-3, im Klon gebaut), registriertes
 * Oracle seit MF-712. **Ausgefuehrt, nicht portiert.**
 *
 *     to_woz2 uftk_dos33_35trk.do to_woz2_uftk_dos33.woz
 *
 * Eingabe ist das UFT-eigene DOS-3.3-Abbild aus dem Korpus (143 360
 * Byte), Ausgabe ein WOZ 2.0 von 252 416 Byte mit synthetisiertem
 * GCR-Strom.
 *
 * ── Warum das ein Beleg ist und kein Selbstgespraech ────────────────────
 *
 * Die zwei Seiten kennen einander nicht:
 *
 *     links : das `.do`, Dateiversatz `(spur * 16 + logisch) * 256`
 *     rechts: dasselbe Abbild, von `to_woz2` in GCR kodiert, von UFTs
 *             PLUGIN wieder in PHYSISCHE Sektoren zerlegt
 *
 * Verbunden werden sie durch die DOS-3.3-Interleave-Tafel
 * logisch -> physisch `{0,13,11,9,7,5,3,1,14,12,10,8,6,4,2,15}`
 * (Quelle: `a8rawconv`, `diska2.cpp:3-5`).
 *
 * **Waere der Versatz falsch, die Nummerierung anders oder die
 * Verschraenkung eine andere, faende der Vergleich die Bytes nicht.**
 *
 * ── Abgrenzung zu `test_do_layout_verified.c` ───────────────────────────
 *
 * Der macht denselben Vergleich, ruft aber `uft_apple_gcr_scan_track()`
 * **direkt**. Dieser hier geht durch `uft_format_plugin_woz` — also
 * ueber den Weg, den ein Benutzer nimmt. Genau der war der defekte.
 *
 * ── Was die MUTATIONSMATRIX an diesem Test berichtigt hat ───────────────
 *
 * Die erste Fassung fing **5 von 8** Mutationen. Eine der drei
 * Entkommenen war eine echte Luecke hier, nicht im Code: eine Sonde, die
 * **jede** Datei annimmt, blieb unbemerkt, weil nur mit dem echten
 * WOZ-Kopf geprobt wurde. Die Zusage darueber sagte damit nur, dass die
 * Sonde JA sagen kann — nicht, dass sie NEIN sagen kann. Geschlossen
 * durch eine Gegenprobe (Nullpuffer und ein Puffer mit falscher
 * Kennung); heute **6 von 8**.
 *
 * Die zwei uebrigen sind **benannt statt verschwiegen**, und ihre Gruende
 * sind gemessen:
 *
 *   * **Das Suchfenster auf zwei Umdrehungen zu vergroessern ist hier
 *     kein Logikfehler, sondern ein Pufferueberlauf.** MF-715 hat an
 *     `nib` gemessen, dass ein zu grosses Fenster 595 statt 560 Sektoren
 *     liefert — dort traegt der Spurpuffer 6656 Byte MIT Ueberlappung.
 *     Ein WOZ2-Spurblock ist dagegen genau eine Umdrehung: gemessen
 *     `bit_count = 50624` und **16** Adressfelder, bei verdoppeltem
 *     Fenster **30** — die 14 zusaetzlichen liegen HINTER dem Puffer, im
 *     Haldenrauschen, tragen zufaellige Spurnummern und fallen durch den
 *     Spurfilter. Was diese Mutation liefert, haengt am Haldeninhalt und
 *     nicht am Code; sie ist damit nicht isolierbar. Die Zusage „genau 16
 *     Sektoren je Spur" unten steht trotzdem — sie ist eine eigene
 *     Aussage ueber die Zahl, und der Byte-Vergleich nimmt den ERSTEN
 *     Sektor mit passender Nummer, sieht ein Duplikat also nicht.
 *   * Die Feld-Obergrenze von `UFT_A2_SECTORS_16 * 2` auf
 *     `UFT_A2_SECTORS_16` zu senken aendert an diesem Erzeugnis nichts,
 *     weil keine Spur mehr als 16 Adressfelder traegt (gemessen: 560
 *     Felder, kein einziges doppelt). Die Reserve ist fuer Spuren mit
 *     Doppelsektoren da, und eine solche liegt nicht vor.
 *
 * Drei weitere Mutationen der ersten Runde waren **unbrauchbar**, und
 * auch das ist gemessen statt angenommen: an diesem Erzeugnis gibt es
 * **0** Adressfelder mit fremder Spurnummer, **0** ohne Daten oder in
 * fremder Kodierung, und `sek[i].sector == i` fuer alle 560 — `to_woz2`
 * legt die Sektoren physisch aufsteigend. Eine Mutation, deren Bedingung
 * nie zutrifft, ist nicht durchgerutscht (MF-1028).
 *
 * ── Was NICHT geprueft ist ──────────────────────────────────────────────
 *
 * * **3,5 Zoll.** Apple-3,5"-Disketten sind zonenaufgezeichnet (8 bis 12
 *   Sektoren je Spur) und brauchen eine andere GCR-Variante; dort bleibt
 *   es beim Pseudosektor, und die Geometrie meldet weiterhin fest 12.
 *   Verzeichnet als **P3-352**.
 * * **Kopiergeschuetzte Spuren.** Findet der Abtaster kein Adressfeld,
 *   faellt der Leser auf den Bitstrom zurueck. Dieses Erzeugnis hat
 *   keine solche Spur.
 * * **Die FLUX-Chunks.** Die Merkmalstafel fuehrt sie als PARTIAL, und
 *   das bleibt so.
 * * **Der Schreibpfad.**
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

extern const uft_format_plugin_t uft_format_plugin_woz;

#define SPUREN      35u
#define SEKTOREN    16u
#define SGR        256u
#define DO_GROESSE  (SPUREN * SEKTOREN * SGR)   /* 143 360 */
#define WOZ_GROESSE 252416L

/* DOS 3.3: logische Sektornummer -> physische auf der Diskette.
 * Quelle: `a8rawconv`, `diska2.cpp:3-5`. */
static const uint8_t INTERLEAVE[SEKTOREN] = {
    0, 13, 11, 9, 7, 5, 3, 1, 14, 12, 10, 8, 6, 4, 2, 15
};

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung, const char *detail)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s - %s\n", was, detail ? detail : ""); }
}

int main(void)
{
    const uft_format_plugin_t *p = &uft_format_plugin_woz;
    uft_disk_t disk;
    char wozpfad[600], dopfad[600], det[300], erster[220];
    uint8_t *doroh;
    uint8_t kopf[64];
    FILE *f;
    long wgr, dgr;
    unsigned c, gesamt = 0, gleich = 0, falsch = 0, fehlend = 0;
    unsigned spuren_falsche_zahl = 0;
    char erste_zahl[160];
    int konf = -1, ok;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("WOZ durch das Plugin gegen to_woz2 - MF-1066\n");
    printf("============================================\n");

    if (UFT_CORPUS_DIR[0] == '\0') {
        printf("SKIP: UFT_CORPUS_DIR nicht gesetzt.\n");
        return 77;      /* SKIP_RETURN_CODE, seit MF-598 */
    }
    snprintf(wozpfad, sizeof wozpfad, "%s/to_woz2_uftk_dos33.woz",
             UFT_CORPUS_DIR);
    snprintf(dopfad, sizeof dopfad, "%s/uftk_dos33_35trk.do",
             UFT_CORPUS_DIR);

    f = fopen(dopfad, "rb");
    if (!f) { printf("SKIP: %s fehlt.\n", dopfad); return 77; }
    fseek(f, 0, SEEK_END); dgr = ftell(f); fseek(f, 0, SEEK_SET);
    doroh = (uint8_t *)malloc((size_t)(dgr > 0 ? dgr : 1));
    if (!doroh || dgr != (long)DO_GROESSE
        || fread(doroh, 1, (size_t)dgr, f) != (size_t)dgr) {
        fclose(f); free(doroh);
        printf("SKIP: %s hat %ld Byte, erwartet %u.\n",
               dopfad, dgr, (unsigned)DO_GROESSE);
        return 77;
    }
    fclose(f);

    f = fopen(wozpfad, "rb");
    if (!f) { free(doroh); printf("SKIP: %s fehlt.\n", wozpfad); return 77; }
    fseek(f, 0, SEEK_END); wgr = ftell(f); fseek(f, 0, SEEK_SET);
    if (fread(kopf, 1, sizeof kopf, f) != sizeof kopf) {
        fclose(f); free(doroh);
        printf("SKIP: %s zu kurz.\n", wozpfad);
        return 77;
    }
    fclose(f);

    snprintf(det, sizeof det, "%ld Byte, Magic %.4s", wgr, (char *)kopf);
    pruefe("das to_woz2-Erzeugnis ist 252 416 Byte gross und traegt "
           "\"WOZ2\"",
           wgr == WOZ_GROESSE && memcmp(kopf, "WOZ2", 4) == 0, det);

    ok = p->probe(kopf, sizeof kopf, (size_t)wgr, &konf) ? 1 : 0;
    snprintf(det, sizeof det, "probe=%d konf=%d", ok, konf);
    pruefe("die Sonde erkennt es an der Kennung, Band \"Merkmal "
           "getroffen\" (>= 80)", ok && konf >= 80, det);

    /* Gegenprobe. Ohne sie sagt die Zusage darueber nur, dass die Sonde
     * JA sagen kann — nicht, dass sie NEIN sagen kann. Eine Mutation, die
     * `return true` an den Anfang setzt, blieb genau deshalb unbemerkt. */
    {
        uint8_t fremd[64];
        int k0 = -1, k1 = -1, o0, o1;
        memset(fremd, 0, sizeof fremd);
        o0 = p->probe(fremd, sizeof fremd, sizeof fremd, &k0) ? 1 : 0;
        memcpy(fremd, "WOZ3", 4);            /* eine Ziffer daneben */
        memcpy(fremd + 4, "\xFF\x0A\x0D\x0A", 4);
        o1 = p->probe(fremd, sizeof fremd, sizeof fremd, &k1) ? 1 : 0;
        snprintf(det, sizeof det, "Nullpuffer=%d(%d) \"WOZ3\"=%d(%d)",
                 o0, k0, o1, k1);
        pruefe("die Sonde weist einen Nullpuffer UND eine Datei mit "
               "falscher Kennung ab", !o0 && !o1, det);
    }

    memset(&disk, 0, sizeof disk);
    if (p->open(&disk, wozpfad, true) != UFT_OK) {
        pruefe("open liest das Fremderzeugnis", 0, wozpfad);
        free(doroh);
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
    erste_zahl[0] = 0;
    for (c = 0; c < SPUREN; c++) {
        uft_track_t t;
        unsigned log;
        memset(&t, 0, sizeof t);
        if (p->read_track(&disk, (int)c, 0, &t) != UFT_OK) {
            fehlend += SEKTOREN;
            spuren_falsche_zahl++;
            if (!erster[0])
                snprintf(erster, sizeof erster, "Spur %u nicht lesbar", c);
            continue;
        }
        /* Die Zahl selbst ist eine eigene Aussage. Der Vergleich unten
         * nimmt den ERSTEN Sektor mit passender Nummer — ein zweiter
         * Umlauf im Suchfenster liefert Doppelsektoren mit denselben
         * Nummern und faellt ihm nicht auf (MF-715: 595 statt 560). */
        if (t.sector_count != SEKTOREN) {
            spuren_falsche_zahl++;
            if (!erste_zahl[0])
                snprintf(erste_zahl, sizeof erste_zahl,
                         "Spur %u hat %u Sektoren statt %u", c,
                         (unsigned)t.sector_count, SEKTOREN);
        }
        for (log = 0; log < SEKTOREN; log++) {
            const uint8_t phys = INTERLEAVE[log];
            const uft_sector_t *gefunden = NULL;
            size_t k;
            gesamt++;
            for (k = 0; k < t.sector_count; k++)
                if (t.sectors[k].id.sector == phys) {
                    gefunden = &t.sectors[k];
                    break;
                }
            if (!gefunden || !gefunden->data) {
                fehlend++;
                if (!erster[0])
                    snprintf(erster, sizeof erster,
                             "Spur %u physischer Sektor %u fehlt", c, phys);
                continue;
            }
            {
                size_t len = gefunden->data_len ? gefunden->data_len
                                                : gefunden->data_size;
                const uint8_t *soll = doroh + ((size_t)c * SEKTOREN + log)
                                              * SGR;
                if (len == SGR && memcmp(gefunden->data, soll, SGR) == 0)
                    gleich++;
                else {
                    falsch++;
                    if (!erster[0])
                        snprintf(erster, sizeof erster,
                                 "Spur %u logisch %u (physisch %u) weicht "
                                 "ab, len=%u", c, log, phys, (unsigned)len);
                }
            }
        }
        uft_track_release(&t);
    }
    p->close(&disk);
    free(doroh);

    snprintf(det, sizeof det, "%u erwartet, %u gleich, %u falsch, "
             "%u fehlend%s%s", gesamt, gleich, falsch, fehlend,
             erster[0] ? " - " : "", erster);
    pruefe("560 von 560 Sektoren byteidentisch mit dem .do-Abbild, "
           "verbunden ueber die DOS-3.3-Verschraenkung",
           gesamt == SPUREN * SEKTOREN && gleich == gesamt
           && falsch == 0 && fehlend == 0, det);

    snprintf(det, sizeof det, "%u Spuren mit abweichender Zahl%s%s",
             spuren_falsche_zahl, erste_zahl[0] ? " - " : "", erste_zahl);
    pruefe("jede der 35 Spuren traegt GENAU 16 Sektoren - kein zweiter "
           "Umlauf, kein Doppelsektor (MF-715)",
           spuren_falsche_zahl == 0, det);

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
