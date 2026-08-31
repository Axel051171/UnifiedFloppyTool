/**
 * @file test_d13_layout_verified.c
 * @brief Die Anordnung des d13-Lesers, extern geprueft (MF-722)
 *
 * ── Was `uft_d13.c` behauptet ───────────────────────────────────────────
 *
 *     off = (cyl * 13 + s) * 256          Dateiversatz
 *     id  = s                              logische Sektornummer 0..12
 *     35 Spuren, 1 Seite, 13 Sektoren, 256 Byte
 *
 * Bis MF-722 war das eine Zusicherung ohne jede Pruefung: `d13` hatte
 * **keinen einzigen Test** (`docs/VERIFICATION_TIERS.md`) und stand
 * darum zu Recht auf T3.
 *
 * ── Der Differenzlauf, der sie extern geprueft hat ──────────────────────
 *
 * Gemessen mit dem registrierten Oracle `to_woz2` (`docs/ORACLES.md`):
 *
 *     linke Seite : uft_format_plugin_d13 liest die .d13
 *     rechte Seite: dieselbe Diskette, to_woz2 -> WOZ 2.0 ->
 *                   uft_apple_gcr_scan_track() (5-and-3, MF-721)
 *     Bruecke     : die Zuordnung logisch<->physisch ist die IDENTITAET
 *                   — zweifach belegt (MF-720): mamedev/mame
 *                   ap2_dsk.cpp:410 `return physical;` und
 *                   ciderpress2.com „13-sector floppies use physical
 *                   sector order"
 *
 * Ergebnis:
 *
 *     rechte Seite dekodiert : 454  (+1 andere Kodierung, benannt)
 *     verglichen             : 454
 *     byteidentisch          : 454
 *
 * Der eine ohne Gegenstueck ist **Spur 0, Sektor 0** — der Bootsektor,
 * den DOS 3.2 mit einer anderen 5-and-3-Variante schreibt (MF-721). Er
 * wird benannt, nicht geraten.
 *
 * ── Und ein Fund, der beim Lesen des Lesers auffiel ─────────────────────
 *
 * `d13_read_track()` fuellt einen unvollstaendigen Sektor mit `0xE5`
 * und **gibt ihn aus**. Der Schwesterleser `uft_do.c` verbietet genau
 * das seit **MF-463**, mit Begruendung im Quelltext: „invented data as
 * if it had been read".
 *
 * Ich hielt das fuer einen scharfen Fehler. **Gemessen ist er es
 * nicht:** `d13_open()` lehnt jede Datei ab, deren Groesse nicht genau
 * `D13_SIZE` ist — der Zweig ist durch die Plugin-Tuer unerreichbar.
 *
 * Dieselbe Lehre wie bei `hardsector` (MF-706) und `prodos_po_do`
 * (MF-713): erst die Erreichbarkeit messen, dann urteilen. Abschnitt 3
 * haelt darum die Pruefung fest, die wirklich schuetzt.
 */

#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

extern const uft_format_plugin_t uft_format_plugin_d13;

#define TRACKS 35
#define SPT    13
#define SS     256
#define D13SZ  (TRACKS * SPT * SS)      /* 116480 */

static int fehler = 0;

#define PRUEFE(bed, ...) do {                                            \
    if (!(bed)) { printf("  FAIL "); printf(__VA_ARGS__);                \
                  printf("\n"); fehler++; }                              \
} while (0)

/* Eine .d13 anlegen, in der jeder Sektor seine Kennung traegt. */
static char *schreibe(size_t bytes, uint8_t *img)
{
    static char pfad[L_tmpnam + 8];
    if (!tmpnam(pfad)) return NULL;
    FILE *f = fopen(pfad, "wb");
    if (!f) return NULL;
    size_t n = fwrite(img, 1, bytes, f);
    fclose(f);
    return (n == bytes) ? pfad : NULL;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("d13: die extern gepruefte Anordnung haelt (MF-722)\n\n");

    uint8_t *img = malloc(D13SZ);
    if (!img) { printf("kein Speicher\n"); return 2; }
    for (int t = 0; t < TRACKS; t++)
        for (int s = 0; s < SPT; s++) {
            uint8_t *p = img + ((size_t)t * SPT + (size_t)s) * SS;
            for (int i = 0; i < SS; i++)
                p[i] = (uint8_t)((t * 13 + s + i) & 0xFF);
            p[0] = (uint8_t)t;
            p[1] = (uint8_t)s;
        }

    /* ── 1 · Geometrie und Anordnung ─────────────────────────────────── */
    char *pfad = schreibe(D13SZ, img);
    if (!pfad) { printf("Wegwerf-Datei fehlgeschlagen\n"); free(img); return 2; }

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    uft_error_t rc = uft_format_plugin_d13.open(&disk, pfad, true);
    PRUEFE(rc == UFT_OK, "das d13-Plugin oeffnet die Datei nicht (rc=%d)", rc);
    if (rc != UFT_OK) { remove(pfad); free(img); return 1; }

    printf("  Geometrie: %d Spuren, %d Seite(n), %d Sektoren, %d Byte\n",
           disk.geometry.cylinders, disk.geometry.heads,
           disk.geometry.sectors, disk.geometry.sector_size);
    PRUEFE(disk.geometry.cylinders == TRACKS, "Spurzahl abweichend");
    PRUEFE(disk.geometry.sectors == SPT, "Sektoren je Spur abweichend");
    PRUEFE(disk.geometry.sector_size == SS, "Sektorgroesse abweichend");

    int gelesen = 0, richtig = 0;
    for (int t = 0; t < TRACKS; t++) {
        uft_track_t trk;
        memset(&trk, 0, sizeof(trk));
        if (uft_format_plugin_d13.read_track(&disk, t, 0, &trk) != UFT_OK) {
            PRUEFE(false, "read_track scheitert auf Spur %d", t);
            continue;
        }
        if (trk.sector_count != SPT)
            PRUEFE(false, "Spur %d liefert %zu Sektoren statt %d",
                   t, trk.sector_count, SPT);
        for (size_t i = 0; i < trk.sector_count; i++) {
            gelesen++;
            const uint8_t *soll = img + ((size_t)t * SPT + i) * SS;
            if (memcmp(trk.sectors[i].data, soll, SS) == 0) richtig++;
            else if (richtig + 4 > gelesen)
                PRUEFE(false, "Spur %d Platz %zu: Inhalt stimmt nicht mit "
                       "Versatz (%d*13+%zu)*256 ueberein", t, i, t, i);
        }
    }
    uft_format_plugin_d13.close(&disk);
    remove(pfad);

    printf("  Sektoren gelesen: %d   an der behaupteten Stelle: %d\n",
           gelesen, richtig);
    PRUEFE(gelesen == 455, "erwartet 455 Sektoren, gelesen %d", gelesen);
    PRUEFE(richtig == 455,
           "nur %d von 455 Sektoren liegen dort, wo d13_read_track sie "
           "behauptet — der Differenzlauf aus MF-722 ist damit hinfaellig",
           richtig);

    /* ── 2 · Die Sonde entscheidet allein an der Groesse ──────────────── */
    int konf = -1;
    uint8_t klein[512];
    memset(klein, 0, sizeof(klein));
    bool ja = uft_format_plugin_d13.probe(klein, sizeof(klein),
                                          D13SZ, &konf);
    printf("\n  Sonde auf %d Byte Nullen: %s (Konfidenz %d)\n",
           D13SZ, ja ? "JA" : "NEIN", konf);
    PRUEFE(ja, "die Sonde nimmt die richtige Groesse nicht an");

    /* ── 3 · Eine abgeschnittene Datei kommt gar nicht erst herein ────
     *
     * Berichtigung an mir selbst (MF-722): beim Lesen von
     * `d13_read_track()` fiel auf, dass es einen unvollstaendigen Sektor
     * mit `0xE5` fuellt und **ausgibt** — genau das, was `uft_do.c` seit
     * MF-463 mit Begruendung verbietet („invented data as if it had been
     * read"). Ich hielt das fuer einen scharfen Fehler.
     *
     * Gemessen ist er es **nicht**: `d13_open()` lehnt jede Datei ab,
     * deren Groesse nicht **genau** `D13_SIZE` ist. Der 0xE5-Zweig ist
     * durch die Plugin-Tuer unerreichbar.
     *
     * Das ist dieselbe Lehre wie bei `hardsector` (MF-706) und
     * `prodos_po_do` (MF-713): **erst die Erreichbarkeit messen, dann
     * urteilen.** Dieser Abschnitt haelt die Pruefung fest, die
     * tatsaechlich schuetzt — die Groessenpruefung beim Oeffnen. */
    size_t kurz = (size_t)(2 * SPT + 5) * SS + 100;   /* mitten im Sektor */
    char *pfad2 = schreibe(kurz, img);
    if (!pfad2) {
        PRUEFE(false, "zweite Wegwerf-Datei liess sich nicht anlegen");
    } else {
        memset(&disk, 0, sizeof(disk));
        uft_error_t rc2 = uft_format_plugin_d13.open(&disk, pfad2, true);
        printf("\n  abgeschnittene Datei (%zu statt %d Byte): open -> %s\n",
               kurz, D13SZ, rc2 == UFT_OK ? "ANGENOMMEN" : "abgewiesen");
        PRUEFE(rc2 != UFT_OK,
               "eine abgeschnittene Datei wird geoeffnet — dann greift der "
               "0xE5-Zweig in d13_read_track() und liefert Fuellmaterial "
               "als gelesene Daten aus (MF-463)");
        if (rc2 == UFT_OK) uft_format_plugin_d13.close(&disk);
        remove(pfad2);
    }

    /* Und eine zu GROSSE Datei ebenso — sonst waere die Schranke nur
     * halb da. */
    char *pfad3 = schreibe(D13SZ, img);
    if (pfad3) {
        FILE *anhang = fopen(pfad3, "ab");
        if (anhang) { fputc(0x42, anhang); fclose(anhang); }
        memset(&disk, 0, sizeof(disk));
        uft_error_t rc3 = uft_format_plugin_d13.open(&disk, pfad3, true);
        printf("  ein Byte zu lang            : open -> %s\n",
               rc3 == UFT_OK ? "ANGENOMMEN" : "abgewiesen");
        PRUEFE(rc3 != UFT_OK, "eine zu lange Datei wird angenommen");
        if (rc3 == UFT_OK) uft_format_plugin_d13.close(&disk);
        remove(pfad3);
    }

    free(img);
    printf("\n  Was die gruene Ampel heisst: die Anordnung, die MF-722 "
           "gegen to_woz2\n"
           "  geprueft hat (454/454 byteidentisch, 1 benannt), gilt "
           "unveraendert — und\n"
           "  dass eine Datei mit falscher Groesse gar nicht erst "
           "geoeffnet wird.\n"
           "  Was sie NICHT heisst: dass echte 13-Sektor-Disketten "
           "gelesen werden.\n");

    printf("\n%s (%d Abweichungen)\n", fehler ? "ROT" : "GRUEN", fehler);
    return fehler ? 1 : 0;
}
