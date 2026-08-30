/**
 * @file test_do_layout_verified.c
 * @brief Die Layout-Behauptung des do-Plugins, extern geprueft (MF-716)
 *
 * ── Was `uft_do.c` behauptet ────────────────────────────────────────────
 *
 * `do_read_track()` legt fest, wo ein Sektor liegt:
 *
 *     off = (cyl * 16 + s) * 256          Dateiversatz
 *     id  = s                              logische Sektornummer 0..15
 *     35 Spuren, 1 Seite, 16 Sektoren, 256 Byte
 *
 * Bis MF-716 war das eine **Zusicherung**: fuenf Tests fassten das
 * Plugin an, keiner hielt die Anordnung gegen etwas, das nicht aus
 * diesem Baum stammt. `do` stand deshalb zu Recht auf **T3** — ein
 * gruener Test gegen die eigene Annahme beweist Selbstkonsistenz, nicht
 * Richtigkeit (das ist die FMT-2/3/10/11/12-Falle).
 *
 * ── Der Differenzlauf, der sie extern geprueft hat ──────────────────────
 *
 * Gemessen mit dem registrierten Oracle `to_woz2` (`docs/ORACLES.md`).
 * Zwei Seiten, die einander nicht kennen:
 *
 *     linke Seite : uft_format_plugin_do liest die DSK (die Behauptung
 *                   oben)
 *     rechte Seite: dieselbe Diskette, von `to_woz2` nach WOZ 2.0
 *                   kodiert, von `uft_apple_gcr_scan_track()` wieder in
 *                   PHYSISCHE Sektoren dekodiert (MF-715)
 *     Verbindung  : die DOS-3.3-Interleave-Tabelle logisch->physisch
 *                   { 0,13,11,9,7,5,3,1,14,12,10,8,6,4,2,15 }
 *                   Quelle: `a8rawconv`, `diska2.cpp:3-5`
 *
 * Ergebnis:
 *
 *     rechte Seite dekodiert : 560 Sektoren
 *     verglichen             : 560
 *     byteidentisch          : 560
 *     fehlend                :   0
 *
 * Waere der Versatz falsch, waere die Sektornummerierung anders oder
 * liefe die Interleave-Tabelle in die andere Richtung, koennte das nicht
 * aufgehen: `to_woz2` hat die Datei nach SEINEM Verstaendnis von
 * DOS-Reihenfolge gelesen, und die Rueckrechnung landet Byte fuer Byte
 * an derselben Stelle.
 *
 * Damit traegt `docs/spec_verification.json` seit MF-716 einen Eintrag
 * fuer `do`, und die Stufe steigt T3 -> T2.
 *
 * ── Was DIESER Test tut ─────────────────────────────────────────────────
 *
 * Er braucht kein Fremdwerkzeug und laeuft in CI. Er haelt fest, dass
 * das Plugin **weiterhin** die Anordnung umsetzt, die extern geprueft
 * wurde. Fiele der Versatz oder die Nummerierung um, waere der obige
 * Differenzlauf hinfaellig, ohne dass es jemand merkt — genau dafuer
 * steht er hier.
 *
 * ── Was er NICHT behauptet ──────────────────────────────────────────────
 *
 * Nichts ueber `po`. Der Differenzlauf lief ueber die **DOS**-Ordnung;
 * die ProDOS-Ordnung war nicht beteiligt und `po` bleibt T3. Und nichts
 * ueber echte historische Disketten: gemessen wurde eine fehlerfreie,
 * maschinell erzeugte Aufzeichnung.
 */

#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

extern const uft_format_plugin_t uft_format_plugin_do;

#define DSK_SIZE   143360u
#define TRACKS     35
#define SPT        16
#define SS         256

static int fehler = 0;

#define PRUEFE(bed, ...) do {                                            \
    if (!(bed)) { printf("  FAIL "); printf(__VA_ARGS__);                \
                  printf("\n"); fehler++; }                              \
} while (0)

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("do: die extern gepruefte Anordnung haelt (MF-716)\n\n");

    /* Jeder Sektor traegt seine eigene Kennung — eine Verwechslung
     * zweier Sektoren faellt damit auf, eine Verschiebung auch. */
    uint8_t *img = malloc(DSK_SIZE);
    if (!img) { printf("kein Speicher\n"); return 2; }
    for (int t = 0; t < TRACKS; t++)
        for (int s = 0; s < SPT; s++) {
            uint8_t *p = img + ((size_t)t * SPT + (size_t)s) * SS;
            for (int i = 0; i < SS; i++)
                p[i] = (uint8_t)((t * 16 + s + i) & 0xFF);
            p[0] = (uint8_t)t;
            p[1] = (uint8_t)s;
        }

    char pfad[L_tmpnam + 8];
    if (!tmpnam(pfad)) { printf("kein Wegwerf-Name\n"); free(img); return 2; }
    FILE *f = fopen(pfad, "wb");
    if (!f) { printf("Wegwerf-Datei nicht anlegbar\n"); free(img); return 2; }
    size_t geschrieben = fwrite(img, 1, DSK_SIZE, f);
    fclose(f);
    if (geschrieben != DSK_SIZE) {
        printf("Wegwerf-Datei unvollstaendig\n"); free(img); return 2;
    }

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    uft_error_t rc = uft_format_plugin_do.open(&disk, pfad, true);
    PRUEFE(rc == UFT_OK, "das do-Plugin oeffnet die Datei nicht (rc=%d)", rc);
    if (rc != UFT_OK) { remove(pfad); free(img); return 1; }

    printf("  Geometrie: %d Spuren, %d Seite(n), %d Sektoren, %d Byte\n",
           disk.geometry.cylinders, disk.geometry.heads,
           disk.geometry.sectors, disk.geometry.sector_size);
    PRUEFE(disk.geometry.cylinders == TRACKS, "Spurzahl abweichend");
    PRUEFE(disk.geometry.heads == 1, "Seitenzahl abweichend");
    PRUEFE(disk.geometry.sectors == SPT, "Sektoren je Spur abweichend");
    PRUEFE(disk.geometry.sector_size == SS, "Sektorgroesse abweichend");

    int gelesen = 0, richtig = 0;
    for (int t = 0; t < TRACKS; t++) {
        uft_track_t trk;
        memset(&trk, 0, sizeof(trk));
        if (uft_format_plugin_do.read_track(&disk, t, 0, &trk) != UFT_OK) {
            PRUEFE(false, "read_track scheitert auf Spur %d", t);
            continue;
        }
        if (trk.sector_count != SPT)
            PRUEFE(false, "Spur %d liefert %zu Sektoren statt %d",
                   t, trk.sector_count, SPT);
        for (size_t i = 0; i < trk.sector_count; i++) {
            gelesen++;
            int s = trk.sectors[i].id.sector;
            /* Die Nummer, die das Plugin vergibt … */
            if (s != (int)i) {
                PRUEFE(false, "Spur %d, Platz %zu traegt Nummer %d", t, i, s);
                continue;
            }
            /* … und der Versatz, den es dafuer gelesen hat. */
            const uint8_t *soll = img + ((size_t)t * SPT + (size_t)s) * SS;
            if (memcmp(trk.sectors[i].data, soll, SS) == 0) richtig++;
            else if (richtig + 4 > gelesen)
                PRUEFE(false, "Spur %d Sektor %d: Inhalt stimmt nicht mit "
                       "Versatz (%d*16+%d)*256 ueberein", t, s, t, s);
        }
    }
    uft_format_plugin_do.close(&disk);
    remove(pfad);
    free(img);

    printf("  Sektoren gelesen: %d   an der behaupteten Stelle: %d\n",
           gelesen, richtig);
    PRUEFE(gelesen == 560, "erwartet 560 Sektoren, gelesen %d", gelesen);
    PRUEFE(richtig == 560,
           "nur %d von 560 Sektoren liegen dort, wo do_read_track sie "
           "behauptet — der Differenzlauf aus MF-716 ist damit hinfaellig",
           richtig);

    printf("\n  Was die gruene Ampel heisst: die Anordnung, die MF-716 "
           "gegen to_woz2\n"
           "  geprueft hat (560/560 byteidentisch), gilt unveraendert.\n"
           "  Was sie NICHT heisst: dass `po` geprueft waere (andere "
           "Ordnung, bleibt T3)\n"
           "  oder dass echte historische Disketten gelesen werden.\n");

    printf("\n%s (%d Abweichungen)\n", fehler ? "ROT" : "GRUEN", fehler);
    return fehler ? 1 : 0;
}
