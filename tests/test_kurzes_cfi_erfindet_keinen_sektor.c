/**
 * @file test_kurzes_cfi_erfindet_keinen_sektor.c
 * @brief Was nicht in der CFI-Datei stand, darf nicht als gelesener
 *        Sektor durchgehen (MF-1001).
 *
 * ── Woher die Frage kommt ────────────────────────────────────────────
 *
 * Tor 62 (MF-980) haelt seit langem die Regel „wer einen Sektor FUELLT,
 * muss ihn KENNZEICHNEN". Es misst dafuer die Helferkette
 * `uft_format_add_sector*()` / `uft_format_add_empty_sector()`.
 *
 * **Neun Plugins gehen an dieser Kette vorbei.** Sie schreiben direkt in
 * `track->sectors[s]`, setzen `sect->status = UFT_SECTOR_OK` von Hand —
 * und zwar BEVOR entschieden ist, ob ueberhaupt Daten da sind — und
 * fuellen im Fehlzweig `memset(sect->data, 0xE5, ...)`:
 *
 *     src/formats/cfi/uft_cfi.c            src/formats/posix/uft_posix.c
 *     src/formats/cpm/uft_cpm_diskdefs.c   src/formats/qrst/uft_qrst.c
 *     src/formats/hardsector/uft_hardsector.c
 *     src/formats/logical/uft_logical.c    src/formats/rcpmfs/uft_rcpmfs.c
 *     src/formats/myz80/uft_myz80.c        src/formats/nanowasp/uft_nanowasp.c
 *
 * Das Ergebnis ist genau die Lage aus MF-980, nur eine Ebene tiefer:
 * erfundene 0xE5 sind von echten 0xE5-Daten nicht zu unterscheiden.
 * `uft_hardsector.c` ist der schaerfste Fall — es zaehlt im selben
 * Zweig `result->bad_sectors++`, WEISS also, dass der Sektor erfunden
 * ist, und laesst ihn trotzdem als gueltig gekennzeichnet stehen.
 *
 * ── Warum der Beweis an CFI gefahren wird und nicht an hardsector ────
 *
 * Weil ein Rotbeweis, der nicht feuern kann, nichts beweist. Gemessen:
 *
 *   - `uft_hardsector_read_mem()` weist untergrosse Abbilder VORHER ab
 *     (`size < expected_size` -> `UFT_ERR_FORMAT`). Sein 0xE5-Zweig ist
 *     ueber diesen Weg gar nicht erreichbar — dort ist die fehlende
 *     Kennzeichnung Vorsorge, kein Fehler.
 *   - `uft_posix.c` leitet ohne `.geom`-Datei die Zylinderzahl aus der
 *     DATEIGROESSE ab; auch dort kann die Fuellung nicht anschlagen.
 *   - **CFI kann es.** Seine Geometrie kommt aus der BPB INNERHALB der
 *     entpackten Daten und ist von deren Laenge unabhaengig. Eine Datei,
 *     deren BPB 80 x 2 x 18 behauptet, waehrend der Inhalt eine einzige
 *     Spur ist, trifft den Zweig zwangslaeufig.
 *
 * ── Die Pruefdatei ──────────────────────────────────────────────────
 *
 * Von Hand gebaut, jede Zahl steht hier:
 *
 *   Spurblock      : 2 Byte LE Laenge = 514
 *   darin Teilblock: 2 Byte LE = 512, Bit 15 = 0 -> unkomprimiert
 *   darin          : 512 Byte Bootsektor
 *
 *   Bootsektor: [0]=0xEB  [11..12]=512  [19..20]=2880
 *               [24..25]=18  [26..27]=2
 *               -> parse_bpb liefert 2880/(18*2) = 80 Zylinder
 *
 *   Datei gesamt: 516 Byte. Entpackt: 512 Byte.
 *   Verlangt   : 80 * 2 * 18 * 512 = 1 474 560 Byte.
 *
 * Sektor 1 der Spur (0,0) steht also in der Datei, Sektor 2 nicht.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/formats/uft_cfi.h"
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

/* Kennzeichen im echten Bootsektor, damit „echt" und „erfunden"
 * unterscheidbar bleiben, ohne sich auf 0xE5 zu verlassen. */
#define MARKE_VERSATZ 100
#define MARKE_WERT    0x5A

static size_t baue_cfi(uint8_t *aus, size_t kapazitaet)
{
    uint8_t boot[512];
    memset(boot, 0x00, sizeof(boot));
    boot[0] = 0xEB;                       /* gueltiger Sprung        */
    boot[11] = 0x00; boot[12] = 0x02;     /* 512 Byte je Sektor      */
    boot[19] = 0x40; boot[20] = 0x0B;     /* 2880 Sektoren gesamt    */
    boot[24] = 0x12; boot[25] = 0x00;     /* 18 Sektoren je Spur     */
    boot[26] = 0x02; boot[27] = 0x00;     /* 2 Koepfe                */
    boot[MARKE_VERSATZ] = MARKE_WERT;

    const size_t noetig = 2 + 2 + sizeof(boot);
    if (kapazitaet < noetig) return 0;

    size_t p = 0;
    aus[p++] = (uint8_t)(514 & 0xFF);     /* Spurblocklaenge 514     */
    aus[p++] = (uint8_t)(514 >> 8);
    aus[p++] = (uint8_t)(512 & 0xFF);     /* Teilblock 512, Bit15=0  */
    aus[p++] = (uint8_t)(512 >> 8);
    memcpy(aus + p, boot, sizeof(boot));
    p += sizeof(boot);
    return p;
}

int main(void)
{
    printf("=== Ein kurzes CFI erfindet keinen Sektor (MF-1001) ===\n");

    uint8_t datei[1024];
    size_t laenge = baue_cfi(datei, sizeof(datei));
    if (laenge == 0) {
        printf("  [ROT]  Pruefdatei liess sich nicht bauen\n");
        return 1;
    }

    uft_disk_image_t *disk = NULL;
    cfi_read_result_t erg;
    uft_error_t rc = uft_cfi_read_mem(datei, laenge, &disk, &erg);

    pruefe("die Datei laesst sich oeffnen", rc == UFT_OK && disk != NULL,
           "ohne geoeffnete Datei sagt der Rest nichts");
    if (rc != UFT_OK || !disk) {
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }

    pruefe("die BPB bestimmt die Geometrie (80 x 2 x 18)",
           erg.cylinders == 80 && erg.heads == 2 && erg.sectors == 18,
           "sonst trifft der Beweis den falschen Zweig");

    uft_track_t *spur = disk->track_data[0];
    pruefe("Spur (0,0) existiert", spur != NULL && spur->sector_count >= 2,
           "zu wenige Sektoren angelegt");
    if (!spur || spur->sector_count < 2) {
        uft_disk_free(disk);
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }

    /* Sektor 1 STAND in der Datei — er muss echt sein. Das ist die
     * Gegenprobe: ein Test, der nur „alles ist verdaechtig" prueft,
     * wuerde auch bei einem Leser gruen, der gar nichts liest. */
    uft_sector_t *echt = &spur->sectors[0];
    pruefe("Sektor 1 traegt die Daten aus der Datei",
           echt->data != NULL && echt->data[MARKE_VERSATZ] == MARKE_WERT,
           "die Marke aus dem Bootsektor fehlt");
    pruefe("Sektor 1 gilt als gelesen",
           echt->status == UFT_SECTOR_OK,
           "ein echter Sektor darf nicht bemaengelt werden");

    /* Sektor 2 stand NICHT in der Datei. */
    uft_sector_t *erfunden = &spur->sectors[1];
    pruefe("Sektor 2 wurde gefuellt, nicht gelesen",
           erfunden->data != NULL && erfunden->data[0] == 0xE5,
           "der Fuellzweig wurde gar nicht betreten -- Beweis untauglich");

    char hinweis[160];
    snprintf(hinweis, sizeof(hinweis),
             "Sektor 2 stand nicht in der Datei, gilt aber als gelesen "
             "(status=0x%02X, data[0]=0x%02X)",
             (unsigned)erfunden->status,
             erfunden->data ? (unsigned)erfunden->data[0] : 0u);
    pruefe("Sektor 2 ist NICHT als gelesen gekennzeichnet",
           (erfunden->status & UFT_SECTOR_MISSING) != 0,
           hinweis);

    uft_disk_free(disk);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot == 0 ? 0 : 1;
}
