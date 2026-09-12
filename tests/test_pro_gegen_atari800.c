/**
 * @file test_pro_gegen_atari800.c
 * @brief PRO gegen zwei benannte Haende (MF-1054)
 *
 * ── Die Referenzen ──────────────────────────────────────────────────────
 *
 * 1. **Beschreibung:** `whizzosoftware.com/sio2arduino/prosys.html`.
 *    Sie sagt ueber sich selbst: *„Since the PRO format has never been
 *    officially documented, the information presented here is based on
 *    reverse engineering … therefore, this information has a very good
 *    chance of being completely wrong and is at best an educated
 *    guess."* **Allein traegt sie nach der EINFRIER-REGEL nicht** — das
 *    stand seit MF-1048 als `P3-343` im Register, mit genau diesem
 *    Satz: „Was fehlt, ist eine zweite Hand."
 *
 * 2. **Die zweite Hand, gefunden MF-1054:** `atari800/src/sio.c`
 *    (GPL-2), Klon unter `tools/uft-scout/work/atari800/`, Quellstand
 *    b6bdf05c (2026-09-08). **Nur gelesen und ausgefuehrt**, keine Zeile
 *    uebernommen. Sie bestaetigt jedes Feld der Beschreibung:
 *
 *      Erkennung   `(len-16) % 140 == 0`
 *                  und `data[0]*256 + data[1] == (len-16)/140`
 *                  und `data[2] == 'P'`                 (sio.c:500-508)
 *      Versatz     `16 + 140*(sektor-1)`, Sektorgroesse immer 128
 *                                        (SIO_SizeOfSector, PRO-Zweig)
 *      Sektorkopf  12 Byte; Byte 1 = Status, `0xFF` heisst GUT (sio.c:711)
 *                  Byte 5 = Phantomzahl, Byte 6..10 = Indizes (sio.c:691)
 *                  Phantomsektor = nominale Sektorzahl + Index
 *      Nennweite   1040, wenn die Datei mindestens 1040 Saetze fasst,
 *                  sonst 720                            (sio.c:515-523)
 *
 * **Bemerkenswert ist, WAS die zweite Hand bestaetigt:** nicht eine
 * Tabelle, sondern eine Quelle, die ihre eigene Richtigkeit bestreitet.
 * Genau das war der Grund, warum `pro` nicht gehoben werden konnte.
 *
 * ── Was der Vorzustand tat (gemessen) ───────────────────────────────────
 *
 * `uft_pro_plugin.c` suchte eine **erfundene Kennung** `"APRO"`/`"KPRO"`
 * bei Versatz 0. Dort steht in Wirklichkeit die Sektorzahl. Gemessen an
 * einer spezifikationsgerechten Datei (Kopf `02 d2 50 32`):
 *
 *     probe = 0        open = -25
 *
 * Klasse MF-961 (`86f`), MF-1022 (`sap`), MF-1029 (`myz80`), MF-1030
 * (`nanowasp`), MF-1032 (`logical`) — **zum sechsten Mal**.
 *
 * Dazu war der ganze Aufbau erfunden: „16-byte header + sector entries
 * (4 bytes each) + sector data". Es gibt keine Sektortabelle; Kopf und
 * Daten liegen **verschraenkt**. Die Geometrie kam aus `raw[7]`/`raw[6]`
 * (Polsterbytes), die Statusbits aus einer Tabelle, die es nicht gibt —
 * und **Phantomsektoren, der Zweck des Formats, kamen gar nicht vor**.
 *
 * ── Die Pruefdatei ──────────────────────────────────────────────────────
 *
 * Sie wird hier gebaut, nicht mitgeliefert: `pro` erreicht **T2**, nicht
 * T1b, weil es **keinen fremden ERZEUGER** gibt — atari800 sagt selbst
 * „.pro is read only for now" (sio.c:505), und APE ist proprietaer. Ein
 * Korpus-Abbild von fremder Hand gaebe es also nur mit einer
 * Beschaffung, die noch aussteht.
 *
 *     722 Saetze = 720 nominale Sektoren + 2 Phantomsaetze
 *     Sektor 17 und 100: Status 0x10 statt 0xFF (Befund des Controllers)
 *     Sektor 33: Phantomzahl 2, Indizes 1 und 2 -> Saetze 721 und 722
 *     jeder Sektor benennt sich selbst ("UFT-K Snnn ", Methode MF-1020)
 */
#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

extern const uft_format_plugin_t uft_format_plugin_pro;

#define NOMINAL   720u
#define PHANTOME    2u
#define SAETZE    (NOMINAL + PHANTOME)
#define SEC_HDR    12u
#define SEC_DATA  128u
#define RECORD    (SEC_HDR + SEC_DATA)
#define FILE_HDR   16u
#define SPT        18u
#define PRO_LEN   (FILE_HDR + SAETZE * RECORD)

#define SCHLECHT_A  17u
#define SCHLECHT_B 100u
#define MIT_PHANTOM 33u

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung, const char *detail)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s — %s\n", was, detail ? detail : ""); }
}

/* Die Nutzlast eines Satzes — dieselbe Regel wie im Erzeuger. */
static void nutzlast(unsigned nr, uint8_t *b)
{
    char k[12];
    unsigned i;
    snprintf(k, sizeof k, "UFT-K S%03u ", nr);
    memcpy(b, k, 11);
    for (i = 11; i < SEC_DATA; i++)
        b[i] = (uint8_t)((nr * 7u + (i - 11u) * 3u) & 0xFFu);
}

static void baue_pro(uint8_t *d)
{
    unsigned nr;
    memset(d, 0, PRO_LEN);
    d[0] = (uint8_t)((SAETZE >> 8) & 0xFF);
    d[1] = (uint8_t)(SAETZE & 0xFF);
    d[2] = 'P';
    d[3] = '2';
    for (nr = 1; nr <= SAETZE; nr++) {
        uint8_t *h = d + FILE_HDR + (size_t)(nr - 1u) * RECORD;
        h[1] = (nr == SCHLECHT_A || nr == SCHLECHT_B) ? 0x10 : 0xFF;
        if (nr == MIT_PHANTOM) { h[5] = 2; h[6] = 1; h[7] = 2; }
        nutzlast(nr, h + SEC_HDR);
    }
}

static int schreibe(const char *pfad, const uint8_t *b, size_t n)
{
    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;
    if (fwrite(b, 1, n, f) != n) { fclose(f); return 0; }
    fclose(f);
    return 1;
}

int main(void)
{
    const uft_format_plugin_t *p = &uft_format_plugin_pro;
    const char *tmp = getenv("TEMP");
    char pfad[600], d[300];
    uint8_t *bild;
    uft_disk_t disk;
    int konf, ok;
    unsigned c;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("PRO gegen atari800 (GPL-2, nur gelesen) + sio2arduino "
           "— MF-1054\n");
    printf("==========================================================="
           "========\n");

    bild = malloc(PRO_LEN);
    if (!bild) { printf("kein Speicher\n"); return 2; }
    baue_pro(bild);

    /* 1 — die Sonde nimmt eine spezifikationsgerechte Datei an, im Band
     *     „Struktur gelesen" (50..79 nach MF-729). Keine Kennung, also
     *     nicht im Band „Merkmal getroffen". */
    konf = -1;
    ok = p->probe(bild, PRO_LEN, PRO_LEN, &konf) ? 1 : 0;
    snprintf(d, sizeof d, "probe=%d konf=%d", ok, konf);
    pruefe("die Sonde nimmt eine PRO-Datei an, Konfidenz 50..79",
           ok && konf >= 50 && konf <= 79, d);

    /* 2 — `'P'` bei Versatz 2 ist die einzige Kennung, die es gibt. */
    {
        uint8_t sav = bild[2];
        bild[2] = 'X';
        konf = -1;
        ok = p->probe(bild, PRO_LEN, PRO_LEN, &konf) ? 1 : 0;
        snprintf(d, sizeof d, "probe=%d", ok);
        pruefe("ohne 'P' bei Versatz 2 wird abgewiesen", !ok, d);
        bild[2] = sav;
    }

    /* 3 — die Sektorzahl im Kopf muss die Dateigroesse RESTLOS erklaeren.
     *     Das ist die Selbstpruefung, die eine fehlende Kennung ersetzt. */
    {
        uint8_t sav0 = bild[0], sav1 = bild[1];
        bild[0] = 0x02; bild[1] = 0xD0;        /* 720 statt 722 */
        konf = -1;
        ok = p->probe(bild, PRO_LEN, PRO_LEN, &konf) ? 1 : 0;
        snprintf(d, sizeof d, "probe=%d", ok);
        pruefe("eine Sektorzahl, die nicht zur Dateigroesse passt, "
               "wird abgewiesen", !ok, d);
        bild[0] = sav0; bild[1] = sav1;
    }

    /* 4 — eine Groesse, die kein Vielfaches von 140 ueber dem Kopf ist,
     *     kann keine PRO sein.
     *
     *     Die Groesse ist EIN BYTE ZU GROSS, nicht zu klein, und das ist
     *     nachgemessen statt gewaehlt: bei `PRO_LEN - 1` bricht schon die
     *     Zaehlpruefung von oben ein (die Ganzzahldivision ergibt 721,
     *     der Kopf sagt 722), und die Rasterpruefung waere REDUNDANT —
     *     ihre Mutation rutschte durch. Bei `PRO_LEN + 1` ergibt die
     *     Division wieder 722, die Zaehlpruefung ist also zufrieden, und
     *     nur das Raster schlaegt an. Dieselbe Falle wie MF-1014 und
     *     MF-1026: eine Zusage, die aus dem falschen Grund gruen ist. */
    {
        konf = -1;
        ok = p->probe(bild, PRO_LEN + 1u, PRO_LEN + 1u, &konf) ? 1 : 0;
        snprintf(d, sizeof d, "probe=%d", ok);
        pruefe("eine Groesse ausserhalb des 140-Byte-Rasters wird "
               "abgewiesen (ein Byte zu VIEL — sonst faengt sie schon "
               "die Zaehlpruefung)", !ok, d);
    }

    /* 5 — die erfundene Kennung des Vorzustands traegt nicht mehr. */
    {
        uint8_t fake[FILE_HDR + RECORD];
        memset(fake, 0, sizeof fake);
        memcpy(fake, "APRO", 4);
        konf = -1;
        ok = p->probe(fake, sizeof fake, sizeof fake, &konf) ? 1 : 0;
        snprintf(d, sizeof d, "probe=%d", ok);
        pruefe("die erfundene Kennung \"APRO\" oeffnet nichts mehr",
               !ok, d);
    }

    snprintf(pfad, sizeof pfad, "%s/uft_pro_mf1054.pro", tmp ? tmp : ".");
    if (!schreibe(pfad, bild, PRO_LEN)) {
        printf("SKIP: Pruefdatei liess sich nicht schreiben (%s)\n", pfad);
        free(bild);
        return 77;
    }

    memset(&disk, 0, sizeof disk);
    if (p->open(&disk, pfad, true) != UFT_OK) {
        pruefe("open liest die Pruefdatei", 0, "open scheitert");
        printf("\n%d gruen, %d rot\n", gruen, rot);
        remove(pfad);
        free(bild);
        return 1;
    }

    /* 6 — die Geometrie ist die nominale Diskette, nicht die Satzzahl. */
    snprintf(d, sizeof d, "%d x %d x %d x %d, total %u",
             disk.geometry.cylinders, disk.geometry.heads,
             disk.geometry.sectors, disk.geometry.sector_size,
             (unsigned)disk.geometry.total_sectors);
    pruefe("Geometrie 40 x 1 x 18 x 128, 720 Sektoren "
           "(nicht 722 — die zwei Phantomsaetze sind keine Diskette)",
           disk.geometry.cylinders == 40 && disk.geometry.heads == 1
           && disk.geometry.sectors == 18
           && disk.geometry.sector_size == 128
           && (unsigned)disk.geometry.total_sectors == NOMINAL, d);

    /* 7 — 720 von 720 nominalen Sektoren byteidentisch, und die
     *     Befunde sitzen genau auf 17 und 100. */
    {
        unsigned nominal_gesehen = 0, gleich = 0, falsch = 0;
        unsigned schlecht_gefunden = 0, schlecht_falsch = 0;
        uint8_t soll[SEC_DATA];
        char erster[200];
        erster[0] = 0;

        for (c = 0; c < (unsigned)disk.geometry.cylinders; c++) {
            uft_track_t t;
            unsigned s, idx = 0;
            memset(&t, 0, sizeof t);
            if (p->read_track(&disk, (int)c, 0, &t) != UFT_OK) continue;
            for (s = 0; s < t.sector_count; s++) {
                const uft_sector_t *sec = &t.sectors[s];
                unsigned nr;
                if (sec->status == UFT_SECTOR_DUPLICATE) continue;
                nr = c * SPT + idx + 1u;
                idx++;
                if (nr > NOMINAL) break;
                nominal_gesehen++;
                nutzlast(nr, soll);
                if (sec->data && sec->data_len == SEC_DATA
                    && memcmp(sec->data, soll, SEC_DATA) == 0) gleich++;
                else {
                    falsch++;
                    if (!erster[0])
                        snprintf(erster, sizeof erster,
                                 "Sektor %u weicht ab (%zu Byte)",
                                 nr, sec->data_len);
                }
                if (nr == SCHLECHT_A || nr == SCHLECHT_B) {
                    if (sec->status != UFT_SECTOR_OK && !sec->crc_ok)
                        schlecht_gefunden++;
                    else
                        schlecht_falsch++;
                } else if (sec->status != UFT_SECTOR_OK) {
                    schlecht_falsch++;
                    if (!erster[0])
                        snprintf(erster, sizeof erster,
                                 "Sektor %u ist faelschlich beanstandet",
                                 nr);
                }
            }
        }
        snprintf(d, sizeof d, "%u gesehen, %u gleich, %u falsch%s%s",
                 nominal_gesehen, gleich, falsch,
                 erster[0] ? " — " : "", erster);
        pruefe("720 von 720 nominalen Sektoren byteidentisch",
               nominal_gesehen == NOMINAL && gleich == NOMINAL
               && falsch == 0, d);

        snprintf(d, sizeof d, "%u beanstandet, %u falsch eingeordnet",
                 schlecht_gefunden, schlecht_falsch);
        pruefe("genau die zwei Sektoren mit Status != 0xFF sind "
               "beanstandet, kein dritter",
               schlecht_gefunden == 2 && schlecht_falsch == 0, d);
    }

    /* 8 — die Phantomsektoren liegen auf derselben Spur, mit derselben
     *     Nummer, und tragen die Nutzlast der Saetze 721 und 722. */
    {
        uft_track_t t;
        unsigned s, dupl = 0, dupl_richtig = 0;
        unsigned zyl = (MIT_PHANTOM - 1u) / SPT;
        uint8_t soll[SEC_DATA];
        memset(&t, 0, sizeof t);
        if (p->read_track(&disk, (int)zyl, 0, &t) != UFT_OK) {
            pruefe("Spur mit Phantomsektoren lesbar", 0, "read_track");
        } else {
            for (s = 0; s < t.sector_count; s++) {
                const uft_sector_t *sec = &t.sectors[s];
                if (sec->status != UFT_SECTOR_DUPLICATE) continue;
                dupl++;
                nutzlast(NOMINAL + dupl, soll);
                if (sec->data && sec->data_len == SEC_DATA
                    && memcmp(sec->data, soll, SEC_DATA) == 0)
                    dupl_richtig++;
            }
            snprintf(d, sizeof d, "%u Sektoren auf der Spur, %u Duplikate, "
                     "%u davon mit der erwarteten Nutzlast",
                     (unsigned)t.sector_count, dupl, dupl_richtig);
            pruefe("Sektor 33 traegt zwei Phantom-Ausfertigungen aus den "
                   "Saetzen 721 und 722, auf derselben Spur",
                   t.sector_count == SPT + 2u && dupl == 2
                   && dupl_richtig == 2, d);
        }
    }

    /* 9 — eine Spur ohne Phantome traegt genau 18 Sektoren. */
    {
        uft_track_t t;
        memset(&t, 0, sizeof t);
        ok = (p->read_track(&disk, 5, 0, &t) == UFT_OK);
        snprintf(d, sizeof d, "%u Sektoren", ok ? (unsigned)t.sector_count : 0u);
        pruefe("eine Spur ohne Phantome traegt genau 18 Sektoren",
               ok && t.sector_count == SPT, d);
    }

    p->close(&disk);
    remove(pfad);
    free(bild);

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
