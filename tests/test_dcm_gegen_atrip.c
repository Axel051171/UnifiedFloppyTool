/**
 * @file test_dcm_gegen_atrip.c
 * @brief DCM gegen ein fremd gepacktes Abbild (MF-1053)
 *
 * ── Die Referenz ────────────────────────────────────────────────────────
 *
 * `atrip/atrip/compressors/dcm.py` (Rob McMullen, **GPL-2**), im Baum
 * unter `tools/uft-scout/work/atrip/` — **gelesen und AUSGEFUEHRT, keine
 * Zeile uebernommen**; UFTs Entpacker ist aus dem beschriebenen Verhalten
 * eigenstaendig geschrieben (Muster MF-614, wie MF-1022 bei libsap).
 *
 * Die beiden Pruefdateien hat **atrips Kodierer** erzeugt
 * (`calc_packed_data`) — das ist die fremde Hand, die die Stufe traegt.
 * Die Eingabe ist UFT-eigen und benennt sich selbst; das Rezept steht in
 * `docs/ORACLES.md`.
 *
 *   tests/corpus_free/atrip_uftk_sd_alle6.dcm     18 794 B, Kenn 0x81
 *   tests/corpus_free/atrip_uftk_dd_720x256.dcm    7 205 B, Kenn 0xA1
 *
 * Die SD-Datei loest **alle sechs** Blocktypen aus — gemessen beim
 * Zurueckentpacken durch atrip selbst:
 *
 *     0x41 x238   0x42 x1   0x43 x4   0x44 x357   0x46 x119   0x47 x1
 *
 * Die DD-Datei loest `0x43` x360, `0x44` x359 und `0x47` x1 aus — ihre
 * erste Haelfte erzwingt einen Lauf bis zum Sektorende und damit die
 * Regel „ende == 0 heisst 256", die bei 128 Byte nie auftritt. Warum sie
 * nicht ebenfalls alle sechs traegt, steht unten unter „Grenzen".
 *
 * ── Was der Vorzustand tat (MF-1051/1052, gemessen) ─────────────────────
 *
 * `probe` = 1 bei Konfidenz 90, `open` = **-25**: DCM gewann das
 * Erkennungsrennen und meldete dann, es koenne die Datei nicht lesen —
 * die Gestalt von MF-961 (`86f`), MF-1022 (`sap`) und MF-1036 (`pri`).
 *
 * Sechs strukturelle Abweichungen, jede einzeln gemessen:
 *
 *   1. Die **Dichte** stand in den falschen Bits: gelesen wurde
 *      `file_data[1] & 0x1F` — das ist die **Durchgangsnummer**. Die
 *      Dichte ist `(flags >> 5) & 3`. Weil der erste Durchgang immer 1
 *      ist, waehlte UFT bei JEDER DCM dieselbe Geometrie.
 *   2. Die **Dichtetafel** hatte 1 und 2 vertauscht und eine erfundene 3:
 *      UFT fuehrte {0:128x720, 1:128x1040, 2:256x720, 3:256x1440},
 *      richtig ist {0:720x128, 1:720x256, 2:1040x128} — und **kein**
 *      drittes. Belegt an drei echten Dateien (MF-1052).
 *   3. `0x45` ist das **Durchgangsende**; UFT hielt `0x46` dafuer.
 *   4. Das hohe Bit des Blockbytes ist **umgekehrt** belegt: gesetzt
 *      heisst *impliziter naechster Sektor*.
 *   5. Die **Sektornummer steht NACH dem Block**, nicht davor.
 *   6. Die **Anordnung ist flach**. UFT legte die Sektoren 1-3 mit 128
 *      Byte ab („CRITICAL: Sectors 1-3 are ALWAYS 128 bytes") — das ist
 *      die ATR-Konvention, nicht die DCM-Anordnung. Entschieden am
 *      Objekt (MF-1053): in einer echten DD-Datei liegt unter der
 *      flachen Anordnung bei Sektor 360 eine gueltige Atari-VTOC
 *      (`02 c4 02 7c 00` — 708 Sektoren, 124 frei) und bei Sektor 361
 *      ein Verzeichniseintrag mit lesbarem Namen („MODPLUS"); unter der
 *      128-Byte-Anordnung steht dort Rauschen.
 *
 * Und die Befehlstafel war **durchgehend** anders belegt — nicht fuenf
 * Abweichungen, sondern sieben Kodes:
 *
 *   Kode   gemessen                         UFT vorher
 *   0x41   Sektoranfang ab Index rueckwaerts „MODIFY" (roher Sektor)
 *   0x42   124 x ein Byte + 4 einzelne       „SAME"
 *   0x43   woertlich/Lauf abwechselnd        RLE, andere Kodierung
 *   0x44   Sektorende ab Index vorwaerts     Delta-Paarliste
 *   0x45   **Durchgangsende**                „GAP" (Nullen)
 *   0x46   **wie voriger Sektor**            „END"
 *   0x47   **unkomprimiert**                 existierte nicht
 *
 * ── Grenzen, benannt ────────────────────────────────────────────────────
 *
 * * Die DD-Pruefdatei traegt kein `0x41`, `0x42` oder `0x46`. Grund ist
 *   gemessen:
 *   atrips DD-Pfad ist fuer andere Muster **defekt** — sein `encode_43`
 *   legt `rle_start = 256` in einen `uint8`-Puffer (`OverflowError`),
 *   sein `decode_41`/`decode_44` zaehlen den Index als `uint8` und
 *   erreichen 256 nie, und sein Packer waehlt fuer 256-Byte-Sektoren
 *   `0x42`, das nur die Bytes 0..127 festlegt — sein eigener Dekoder
 *   liest das dann falsch. Eine Pruefdatei, die um einen Orakel-Defekt
 *   herumgebaut ist, waere keine; deshalb deckt DD hier die Anordnung
 *   und die Dichte ab, nicht die Befehlsvielfalt.
 * * Die **Schreibseite** ist nicht geprueft: `dcm_write_track` sagt seit
 *   MF-883 ab, und weder Test noch Merkmalstafel behaupten etwas anderes.
 * * Dichte 2 (1040 x 128, „Enhanced") hat **kein** Abbild im Korpus.
 */
#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR ""
#endif

extern const uft_format_plugin_t uft_format_plugin_dcm;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung, const char *detail)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s — %s\n", was, detail ? detail : ""); }
}

/* ── Die Eingabe, aus der atrips Packer die Pruefdateien gemacht hat ──
 *
 * Dieselbe Regel wie im Rezept in `docs/ORACLES.md`. Sie steht hier als
 * Regel und nicht als Blob, damit sichtbar ist, WAS erwartet wird. */

static void kopf_von(unsigned sek, char *aus)   /* 11 Zeichen */
{
    snprintf(aus, 12, "UFT-K S%03u ", sek);
}

/* Einzeldichte: sechs Zonen zu je 120 Sektoren, eine je Blocktyp. */
static void erwartet_sd(unsigned sek, uint8_t *b)
{
    unsigned zone = (sek - 1u) / 120u;
    char k[12];
    unsigned i;
    kopf_von(sek, k);
    memset(b, 0, 128);
    switch (zone) {
    case 0:                                     /* Kopf + langer Lauf   */
        memcpy(b, k, 11);
        for (i = 11; i < 128; i++) b[i] = 0xAA;
        break;
    case 1:                                     /* alle gleich          */
        for (i = 0; i < 128; i++) b[i] = 0x5A;
        memcpy(b, "UFT-K ZONE1 ", 12);
        break;
    case 2:                                     /* 124 gleich + 4       */
        for (i = 0; i < 128; i++) b[i] = 0x33;
        b[124] = (uint8_t)(sek & 0xFF);
        b[125] = (uint8_t)((sek >> 8) & 0xFF);
        b[126] = 0x55;
        b[127] = 0x99;
        break;
    case 3:                                     /* nur der Anfang       */
        for (i = 0; i < 128; i++) b[i] = 0xC3;
        memcpy(b, k, 11);
        break;
    case 4:                                     /* nur das Ende         */
        for (i = 0; i < 128; i++) b[i] = 0x7E;
        memcpy(b + 128 - 11, k, 11);
        break;
    default:                                    /* alles verschieden    */
        for (i = 0; i < 128; i++)
            b[i] = (uint8_t)((sek * 7u + i * 3u) & 0xFFu);
        memcpy(b, k, 11);
        break;
    }
}

/* Doppeldichte, zwei Haelften:
 *   Sektor 1..360   ein Fuellbyte bis zum Sektorende, Kopf davor —
 *                   erzwingt 0x43 mit einem Lauf, der bis 256 reicht,
 *                   und damit die Regel „ende == 0 heisst 256";
 *   Sektor 361..720 feste Grundlage, nur das ENDE wechselt -> 0x44. */
static void erwartet_dd(unsigned sek, uint8_t *b)
{
    char k[12];
    unsigned i;
    kopf_von(sek, k);
    if (sek <= 360u) {
        uint8_t f = (uint8_t)((sek * 11u + 37u) & 0xFFu);
        for (i = 0; i < 256; i++) b[i] = f;
        memcpy(b, k, 11);
    } else {
        for (i = 0; i < 256; i++) b[i] = (uint8_t)((i * 37u + 91u) & 0xFFu);
        memcpy(b + 256 - 11, k, 11);
    }
}

static uint8_t *lies(const char *pfad, size_t *n)
{
    FILE *f = fopen(pfad, "rb");
    uint8_t *b;
    long len;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) { fclose(f); return NULL; }
    b = malloc((size_t)len);
    if (!b || fread(b, 1, (size_t)len, f) != (size_t)len) {
        free(b); fclose(f); return NULL;
    }
    fclose(f);
    *n = (size_t)len;
    return b;
}

/* Liest ein ganzes Abbild ueber read_track ein und vergleicht jeden
 * Sektor gegen die Regel. Gibt die Zahl der abweichenden Sektoren
 * zurueck; -1, wenn schon das Oeffnen scheitert. */
static int durchlauf(const char *pfad, unsigned erw_ss, unsigned erw_total,
                     void (*erwartet)(unsigned, uint8_t *),
                     char *erster, size_t erster_n)
{
    const uft_format_plugin_t *p = &uft_format_plugin_dcm;
    uft_disk_t disk;
    unsigned c, s, falsch = 0, gesehen = 0;
    uint8_t soll[256];

    memset(&disk, 0, sizeof disk);
    if (p->open(&disk, pfad, true) != UFT_OK) return -1;

    if ((unsigned)disk.geometry.sector_size != erw_ss) {
        snprintf(erster, erster_n, "Sektorgroesse %u statt %u",
                 (unsigned)disk.geometry.sector_size, erw_ss);
        p->close(&disk);
        return -2;
    }

    for (c = 0; c < (unsigned)disk.geometry.cylinders; c++) {
        uft_track_t t;
        memset(&t, 0, sizeof t);
        if (p->read_track(&disk, (int)c, 0, &t) != UFT_OK) continue;
        for (s = 0; s < t.sector_count; s++) {
            const uft_sector_t *sec = &t.sectors[s];
            unsigned nr = c * 18u + s + 1u;
            if (nr > erw_total) break;
            gesehen++;
            erwartet(nr, soll);
            if (sec->data && sec->data_len == erw_ss
                && memcmp(sec->data, soll, erw_ss) == 0)
                continue;
            falsch++;
            if (!erster[0]) {
                char kam[16], sll[16];
                memset(kam, 0, sizeof kam);
                memset(sll, 0, sizeof sll);
                if (sec->data && sec->data_len >= 11)
                    memcpy(kam, sec->data, 11);
                memcpy(sll, soll, 11);
                snprintf(erster, erster_n,
                         "Sektor %u: kam \"%s\" (%zu Byte), soll \"%s\"",
                         nr, kam, sec->data_len, sll);
            }
        }
    }
    if (gesehen != erw_total && !erster[0])
        snprintf(erster, erster_n, "%u Sektoren geliefert, %u erwartet",
                 gesehen, erw_total);
    p->close(&disk);
    return (int)falsch + (gesehen == erw_total ? 0 : 1000000);
}

int main(void)
{
    const uft_format_plugin_t *p = &uft_format_plugin_dcm;
    char sd[600], dd[600], d[300];
    uint8_t *roh;
    size_t n;
    int konf, r;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("DCM gegen atrips Packer (GPL-2, ausgefuehrt) — MF-1053\n");
    printf("======================================================\n");

    if (UFT_CORPUS_DIR[0] == '\0') {
        printf("SKIP: UFT_CORPUS_DIR nicht gesetzt — ohne Korpus prueft "
               "dieser Test nichts\n");
        return 77;      /* SKIP_RETURN_CODE, seit MF-598 fuer alle Tests */
    }

    snprintf(sd, sizeof sd, "%s/atrip_uftk_sd_alle6.dcm", UFT_CORPUS_DIR);
    snprintf(dd, sizeof dd, "%s/atrip_uftk_dd_720x256.dcm", UFT_CORPUS_DIR);

    roh = lies(sd, &n);
    if (!roh) {
        printf("SKIP: %s fehlt im Korpus\n", sd);
        return 77;
    }

    /* 1 — die Sonde, auf dem Produktionsweg gerufen */
    konf = -1;
    d[0] = 0;
    snprintf(d, sizeof d, "probe=%d konf=%d",
             p->probe(roh, n, n, &konf) ? 1 : 0, konf);
    pruefe("die Sonde nimmt eine echte DCM an",
           p->probe(roh, n, n, &konf) && konf >= 30, d);

    /* 2 — eine Datei mit erfundener Kennung 0xF8 wird abgewiesen.
     *     Beide Referenzen kennen NUR 0xF9 und 0xFA. */
    {
        uint8_t *fake = malloc(n);
        memcpy(fake, roh, n);
        fake[0] = 0xF8;
        konf = -1;
        snprintf(d, sizeof d, "probe=%d",
                 p->probe(fake, n, n, &konf) ? 1 : 0);
        pruefe("Archivtyp 0xF8 gibt es nicht und wird abgewiesen",
               !p->probe(fake, n, n, &konf), d);
        free(fake);
    }

    /* 3 — eine Datei mit unbekannter Dichte 3 wird abgewiesen.
     *     Die Tafel hat drei Eintraege, nicht vier. */
    {
        uint8_t *fake = malloc(n);
        memcpy(fake, roh, n);
        fake[1] = (uint8_t)((fake[1] & ~0x60) | 0x60);   /* Dichte 3 */
        konf = -1;
        snprintf(d, sizeof d, "probe=%d", p->probe(fake, n, n, &konf) ? 1 : 0);
        pruefe("Dichtekode 3 gibt es nicht und wird abgewiesen",
               !p->probe(fake, n, n, &konf), d);
        free(fake);
    }
    /* 4 — der erste Durchgang muss die Nummer 1 tragen. Beide Referenzen
     *     weisen alles andere ausdruecklich ab („Expected pass one of DCM
     *     archive first"). */
    {
        uint8_t *fake = malloc(n);
        memcpy(fake, roh, n);
        fake[1] = (uint8_t)((fake[1] & ~0x1F) | 2);      /* Durchgang 2 */
        konf = -1;
        snprintf(d, sizeof d, "probe=%d", p->probe(fake, n, n, &konf) ? 1 : 0);
        pruefe("ein erstes Stueck mit Durchgangsnummer 2 wird abgewiesen",
               !p->probe(fake, n, n, &konf), d);
        free(fake);
    }
    free(roh);

    /* 5 — Einzeldichte: 720 von 720 Sektoren byteidentisch */
    d[0] = 0;
    r = durchlauf(sd, 128, 720, erwartet_sd, d, sizeof d);
    if (r == -1) snprintf(d, sizeof d, "open scheitert");
    pruefe("SD: 720 von 720 Sektoren byteidentisch, alle sechs Blocktypen",
           r == 0, d);

    /* 6 — Doppeldichte: Dichte 1 ist 720 x 256, nicht 1040 x 128 */
    d[0] = 0;
    r = durchlauf(dd, 256, 720, erwartet_dd, d, sizeof d);
    if (r == -1) snprintf(d, sizeof d, "open scheitert");
    pruefe("DD: 720 von 720 Sektoren byteidentisch bei Sektorgroesse 256",
           r == 0, d);

    /* 7 — die Anordnung ist flach: Sektor 4 liegt bei 3 x 256.
     *     Unter der ATR-Konvention laege er bei 3 x 128 = 384. */
    {
        uft_disk_t disk;
        uft_track_t t;
        uint8_t soll[256];
        memset(&disk, 0, sizeof disk);
        d[0] = 0;
        if (p->open(&disk, dd, true) != UFT_OK) {
            pruefe("DD: Sektor 4 liegt flach bei 3 x 256", 0,
                   "open scheitert");
        } else {
            memset(&t, 0, sizeof t);
            if (p->read_track(&disk, 0, 0, &t) == UFT_OK
                && t.sector_count >= 4) {
                erwartet_dd(4, soll);
                snprintf(d, sizeof d, "data_len=%zu", t.sectors[3].data_len);
                pruefe("DD: Sektor 4 liegt flach bei 3 x 256 "
                       "(nicht bei 3 x 128 wie in einer ATR)",
                       t.sectors[3].data
                       && t.sectors[3].data_len == 256
                       && memcmp(t.sectors[3].data, soll, 256) == 0, d);
            } else {
                pruefe("DD: Sektor 4 liegt flach bei 3 x 256", 0,
                       "Spur 0 lieferte keine vier Sektoren");
            }
            p->close(&disk);
        }
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
