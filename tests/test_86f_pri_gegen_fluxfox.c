/**
 * @file test_86f_pri_gegen_fluxfox.c
 * @brief 86F und PRI gegen fluxfox' eigene Testabbilder (MF-1071)
 *
 * `86f` und `pri` standen beide auf **T2**, und beide aus demselben
 * Grund — es fehlte ein Abbild von fremder Hand:
 *
 *   * MF-961 zu `86f`: *„T2 und nicht T1b, weil die Pruefdatei nach
 *     Messung **sehr wahrscheinlich** von fluxfox stammt und nicht vom
 *     kanonischen 86Box — eine fremde Hand, aber nicht die richtige."*
 *   * MF-1036 zu `pri`: *„T2 und nicht T1b, weil hxcfe PRI nur **lesen**
 *     kann."* Ein Schreiber fehlte.
 *
 * ── Die Quelle ──────────────────────────────────────────────────────────
 *
 * **fluxfox** (Daniel Balsom, 2024, **MIT**) liegt seit laengerem als
 * Klon im Baum und traegt in `tests/images/sector_test/` **dieselbe
 * 360K-Diskette in vierzehn Formaten** — darunter `.86f`, `.pri` und
 * `.img`. Fuer `86f` heisst das: der Vorbehalt aus MF-961 faellt zur
 * Haelfte. Die Herkunft ist jetzt **belegt statt vermutet** — es ist
 * fluxfox' eigenes Testabbild aus fluxfox' eigenem Repositorium. Dass
 * es nicht vom kanonischen 86Box stammt, bleibt bestehen und steht
 * unten unter „Was NICHT geprueft ist".
 *
 * (Die zweite Sammlung desselben Klons, `transylvania/`, ist
 * **ausdruecklich nicht** benutzt: ihre `LICENSE.txt` erlaubt das
 * Kopieren nur, solange „you do not charge any money" — das ist mit
 * GPL-2-or-later unvereinbar.)
 *
 * ── Die Bruecke ist die dritte Datei ────────────────────────────────────
 *
 * `fluxfox_sector_test_360k.img` ist dieselbe Diskette flach: 368 640
 * Byte = 40 x 2 x 9 x 512, und **linear durchnummeriert** — Block `n`
 * traegt das Byte `n & 0xFF`. Der Test prueft das, BEVOR er eine
 * Erwartung darauf baut. Damit sagt der Vergleich nicht nur, DASS Bytes
 * kamen, sondern ob es die richtigen sind, und er braucht dafuer keine
 * Tafel, der er glauben muesste.
 *
 * ── Der Befund, der beim Messen fast ein falscher geworden waere ────────
 *
 * Die erste Messung meldete fuer `86f` **702 von 720 Sektoren
 * abweichend** und eine Geometrie von `86 x 2`. Das sah nach einem
 * schweren Fehler aus — es war **meiner**.
 *
 * 86F speichert **physische Positionen**, nicht logische Spuren: die
 * Tabelle hat 86 Zylinder x 2 Seiten = 172 Eintraege, und in dieser
 * Datei sind **alle 172 belegt**. Eine 40-Spur-Diskette liegt darin
 * **doppelt geschrittet** — Position `2*c` traegt die logische Spur
 * `c`. Gemessen und eindeutig:
 *
 *     Position  2 / 0  ->  ID C1  H0, erstes Byte 0x12 = (1*2+0)*9
 *     Position  4 / 0  ->  ID C2  H0, erstes Byte 0x24 = (2*2+0)*9
 *     Position 40 / 0  ->  ID C20 H0, erstes Byte 0x68 = (20*2+0)*9 mod 256
 *     Position 78 / 1  ->  ID C39 H1, erstes Byte 0xC7 = (39*2+1)*9 mod 256
 *
 * Und die Verdopplung geht noch weiter, als es zuerst aussah: gemessen
 * tragen **160 der 172 Positionen** eine Spur, nur 12 sind leer (die
 * Positionen 80..85 auf beiden Seiten). Jede logische Spur steht also an
 * **zwei** Positionen — `2c` UND `2c+1` —, wie ein 40-Spur-Medium in
 * einem 80-Spur-Laufwerk gelesen wird. Deshalb kommen **1440** Sektoren
 * statt 720, und **alle 1440** sind byteidentisch.
 *
 * **Der Leser ist vollstaendig richtig.** Mein Vergleich hatte logische
 * gegen physische Spuren gehalten — dieselbe Klasse wie MF-1060 (17
 * statt 16 Byte) und MF-1061 (zu kleiner Sondenpuffer): die Messung war
 * falsch, nicht der Gegenstand. Deshalb laeuft dieser Test ueber die
 * **Positionen** und nimmt die logische Spur aus der gelieferten ID.
 *
 * ── Was PRI liefert, und warum das hier die richtige Frage ist ──────────
 *
 * PRI ist ein **Bitstromformat** (PCE Raw Image, Hampa Hug). Aus der
 * Datei selbst gelesen: **80 TRAK-Chunks**, je **100 000 Bit** =
 * 12 500 Byte, Bittakt **250 000**. Genau das liefert UFT.
 *
 * Sektoren sind hier also nicht zu erwarten — P3-326 haelt diesen
 * Fehlschluss fest, wo eine erste Fassung MFI mit einer
 * SEKTOR-Zusicherung geprueft hat.
 *
 * ── Mutationsmatrix: 3 von 4, und beide Ausnahmen sind gemessen ────────
 *
 * Grundlauf gruen (seit MF-1067 laeuft er zuerst). Gefangen: Kopf und
 * Zylinder im 86F-Index vertauscht; Seitenzahl fest auf 1; PRIs
 * Chunk-Felder mit vertauschten Bytes.
 *
 * **Nicht isolierbar, erste Ausnahme:** „leere Spuren trotzdem
 * verarbeiten" (`toff == 0` in `f86_read_track`) aendert an dieser Datei
 * nichts, weil **kein einziger** der 172 Tabelleneintraege den Wert 0
 * hat — nachgezaehlt. Die 12 Positionen ohne Inhalt scheitern spaeter am
 * Spurkopf, nicht an dieser Schranke.
 *
 * **Nicht isolierbar, zweite Ausnahme — und die ist ein eigener
 * Befund:** „Bittakt und Spurlaenge vertauscht" (genau der Fehler, den
 * MF-1036 in `pri` behoben hat) bleibt unbemerkt, weil **`bit_clock`
 * den Aufrufer nie erreicht**. `uft_pri.c:254` liest ihn aus dem
 * TRAK-Chunk in seine eigene Struktur, und von dort geht er nirgendwo
 * hin — kein Feld von `uft_track_t` nimmt ihn auf. Fuer ein
 * Bitstromformat ist das wesentlich: ohne Takt kann niemand den Strom
 * dekodieren, und die Datei sagt ihn ausdruecklich (250 000). Gemessen
 * und benannt, nicht nebenbei geaendert — das ist die P3-204-Klasse
 * (gelesener Wert ohne Verbraucher) und gehoert in einen eigenen
 * Eingriff mit eigener Abnahme.
 *
 * ── Was NICHT geprueft ist ──────────────────────────────────────────────
 *
 * * **Der kanonische 86Box-Erzeuger.** Die Datei ist von fluxfox. Der
 *   Vorbehalt aus MF-961 ist damit praeziser, nicht aufgehoben: aus
 *   „sehr wahrscheinlich fluxfox" ist „belegt fluxfox" geworden.
 * * **Wo PRI seinen Bitstrom ablegt.** Er kommt als Sektor 0 mit 12 500
 *   Byte, nicht in `track->raw_data` — dieselbe Gestalt, die MF-1066
 *   bei `woz` und MF-1023 bei `mfi` behoben hat. Hier nur gemessen und
 *   benannt, nicht geaendert: das ist ein eigener Eingriff mit eigener
 *   Abnahme.
 * * **Die Bedeutung von `disk_flags` Bit 3** (`0x1088` in dieser Datei).
 *   Dass die Datei doppelt geschrittet ist, ist am INHALT gemessen,
 *   nicht aus dem Flag gelesen.
 * * **Der Schreibpfad** beider Formate.
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

extern const uft_format_plugin_t uft_format_plugin_86f;
extern const uft_format_plugin_t uft_format_plugin_pri;

#define ZYL        40u
#define KOEPFE      2u
#define SEKT        9u
#define SGR       512u
#define FLACH     (ZYL * KOEPFE * SEKT * SGR)   /* 368 640 */
#define PRI_SPUREN (ZYL * KOEPFE)               /* 80 TRAK-Chunks */
#define PRI_BYTE  12500u                        /* 100 000 Bit / 8 */
#define F86_POS    86u                          /* Positionen je Seite */

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung, const char *detail)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s - %s\n", was, detail ? detail : ""); }
}

int main(void)
{
    char pfad[600], det[300], erster[220];
    uft_disk_t disk;
    uint8_t *flach = NULL;
    FILE *f;
    long gr;
    unsigned c, h;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("86F und PRI gegen fluxfox - MF-1071\n");
    printf("===================================\n");

    if (UFT_CORPUS_DIR[0] == '\0') {
        printf("SKIP: UFT_CORPUS_DIR nicht gesetzt.\n");
        return 77;      /* SKIP_RETURN_CODE, seit MF-598 */
    }

    /* ---- die Bruecke: dieselbe Diskette flach --------------------- */
    snprintf(pfad, sizeof pfad, "%s/fluxfox_sector_test_360k.img",
             UFT_CORPUS_DIR);
    f = fopen(pfad, "rb");
    if (!f) { printf("SKIP: %s fehlt.\n", pfad); return 77; }
    fseek(f, 0, SEEK_END); gr = ftell(f); fseek(f, 0, SEEK_SET);
    flach = (uint8_t *)malloc(FLACH);
    if (gr != (long)FLACH || !flach
        || fread(flach, 1, FLACH, f) != (size_t)FLACH) {
        fclose(f); free(flach);
        printf("SKIP: %s hat %ld Byte, erwartet %u.\n", pfad, gr,
               (unsigned)FLACH);
        return 77;
    }
    fclose(f);

    /* Die Datei ist linear durchnummeriert: Block n traegt n & 0xFF.
     * Das ist eine Eigenschaft DIESES Erzeugnisses und wird geprueft,
     * bevor daraus eine Erwartung gebaut wird. */
    {
        unsigned treffer = 0, i;
        for (i = 0; i < ZYL * KOEPFE * SEKT; i++)
            if (flach[(size_t)i * SGR] == (uint8_t)(i & 0xFF)) treffer++;
        snprintf(det, sizeof det, "%u von %u Bloecken", treffer,
                 ZYL * KOEPFE * SEKT);
        pruefe("die .img ist linear durchnummeriert - Block n traegt n",
               treffer == ZYL * KOEPFE * SEKT, det);
    }

    /* ---- 86F: ueber die POSITIONEN, nicht ueber logische Spuren ---- */
    snprintf(pfad, sizeof pfad, "%s/fluxfox_sector_test_360k.86f",
             UFT_CORPUS_DIR);
    memset(&disk, 0, sizeof disk);
    if (uft_format_plugin_86f.open(&disk, pfad, true) != UFT_OK) {
        pruefe("86f: open liest fluxfox' Erzeugnis", 0, pfad);
    } else {
        unsigned gesamt = 0, gleich = 0, falsch = 0, spuren = 0, leer = 0;
        erster[0] = 0;
        /* 86 Zylinder x 2 Seiten = 172 Positionen. Belegt ist jede
         * ZWEITE (doppelt geschrittet), also 80 Spuren mit Inhalt. */
        for (c = 0; c < F86_POS; c++)
            for (h = 0; h < KOEPFE; h++) {
                uft_track_t t;
                size_t k;
                memset(&t, 0, sizeof t);
                if (uft_format_plugin_86f.read_track(&disk, (int)c, (int)h,
                                                     &t) != UFT_OK)
                    continue;
                if (t.sector_count == 0) {
                    leer++; uft_track_release(&t); continue;
                }
                spuren++;
                for (k = 0; k < t.sector_count; k++) {
                    const uft_sector_t *s = &t.sectors[k];
                    size_t len = s->data_len ? s->data_len : s->data_size;
                    unsigned lz = s->id.cylinder;   /* LOGISCHE Spur, aus
                                                     * dem Adressfeld */
                    unsigned nr = s->id.sector;
                    gesamt++;
                    if (len != SGR || !s->data || nr < 1 || nr > SEKT
                        || lz >= ZYL || s->id.head != h) {
                        falsch++;
                        if (!erster[0])
                            snprintf(erster, sizeof erster,
                                     "Pos %u/%u: C%u H%u S%u len=%u", c, h,
                                     lz, (unsigned)s->id.head, nr,
                                     (unsigned)len);
                        continue;
                    }
                    {
                        const uint8_t *soll = flach +
                            (((size_t)lz * KOEPFE + h) * SEKT + (nr - 1))
                            * SGR;
                        if (memcmp(s->data, soll, SGR) == 0) gleich++;
                        else {
                            falsch++;
                            if (!erster[0])
                                snprintf(erster, sizeof erster,
                                         "Pos %u/%u C%u S%u weicht ab",
                                         c, h, lz, nr);
                        }
                    }
                }
                uft_track_release(&t);
            }
        uft_format_plugin_86f.close(&disk);

        snprintf(det, sizeof det, "%u Positionen mit Inhalt, %u leer",
                 spuren, leer);
        pruefe("86f: 160 der 172 Positionen tragen eine Spur, 12 sind leer "
               "- ein 40-Spur-Medium in einem 80-Spur-Laufwerk, jede Spur "
               "an ZWEI Positionen (2c und 2c+1)",
               spuren == 2u * PRI_SPUREN && leer == 12u, det);

        snprintf(det, sizeof det, "%u Sektoren, %u gleich, %u falsch%s%s",
                 gesamt, gleich, falsch, erster[0] ? " - " : "", erster);
        pruefe("86f: 1440 von 1440 Sektoren byteidentisch mit derselben "
               "Diskette als .img - das sind die 720 Sektoren JE ZWEIMAL, "
               "verbunden ueber die logische Spur aus dem ADRESSFELD und "
               "nicht ueber die Position",
               gesamt == 2u * ZYL * KOEPFE * SEKT && gleich == gesamt
               && falsch == 0, det);
    }

    /* ---- PRI: Bitstrom, keine Sektoren ---------------------------- */
    snprintf(pfad, sizeof pfad, "%s/fluxfox_sector_test_360k.pri",
             UFT_CORPUS_DIR);
    memset(&disk, 0, sizeof disk);
    if (uft_format_plugin_pri.open(&disk, pfad, true) != UFT_OK) {
        pruefe("pri: open liest fluxfox' Erzeugnis", 0, pfad);
    } else {
        unsigned spuren = 0, richtige_laenge = 0;
        size_t kleinste = (size_t)-1, groesste = 0;
        erster[0] = 0;
        snprintf(det, sizeof det, "%d x %d",
                 disk.geometry.cylinders, disk.geometry.heads);
        pruefe("pri: Geometrie 40 x 2 - aus dem Kopf der Datei",
               disk.geometry.cylinders == (int)ZYL
               && disk.geometry.heads == (int)KOEPFE, det);

        for (c = 0; c < ZYL; c++)
            for (h = 0; h < KOEPFE; h++) {
                uft_track_t t;
                size_t len = 0;
                memset(&t, 0, sizeof t);
                if (uft_format_plugin_pri.read_track(&disk, (int)c, (int)h,
                                                     &t) != UFT_OK)
                    continue;
                spuren++;
                if (t.raw_data && t.raw_size) len = t.raw_size;
                else if (t.sector_count > 0)
                    len = t.sectors[0].data_len ? t.sectors[0].data_len
                                                : t.sectors[0].data_size;
                if (len) {
                    if (len < kleinste) kleinste = len;
                    if (len > groesste) groesste = len;
                    if (len == PRI_BYTE) richtige_laenge++;
                    else if (!erster[0])
                        snprintf(erster, sizeof erster,
                                 "Spur %u/%u: %u Byte", c, h,
                                 (unsigned)len);
                }
                uft_track_release(&t);
            }
        uft_format_plugin_pri.close(&disk);

        snprintf(det, sizeof det, "%u Spuren, %u mit 12 500 Byte, "
                 "Spanne %u..%u%s%s", spuren, richtige_laenge,
                 (unsigned)(richtige_laenge ? kleinste : 0),
                 (unsigned)groesste, erster[0] ? " - " : "", erster);
        /* Die Zahl kommt aus der DATEI: 80 TRAK-Chunks, je 100 000 Bit
         * bei Bittakt 250 000. PRI ist ein Bitstromformat — Sektoren
         * waeren hier die falsche Frage (P3-326). */
        pruefe("pri: alle 80 Spuren liefern 12 500 Byte Bitstrom - die "
               "Zahl steht als \"100 000 Bit\" in den TRAK-Chunks der "
               "Datei, nicht in UFT",
               spuren == PRI_SPUREN && richtige_laenge == PRI_SPUREN, det);
    }

    free(flach);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
