/**
 * @file test_2img_nib_stride.c
 * @brief 2IMG mit NIB-Inhalt: falsche Schrittweite, erfundene Struktur (MF-725)
 *
 * ── Was der Kopf ankuendigt ─────────────────────────────────────────────
 *
 * Ein 2IMG traegt bei Versatz `0x0C` ein Feld, das sagt, **was innen
 * liegt** (`uft_2img.c:13`, aus der 2IMG-Spezifikation):
 *
 *     0 = DOS-Reihenfolge · 1 = ProDOS-Reihenfolge · 2 = NIB
 *
 * NIB ist etwas grundsaetzlich anderes als die beiden ersten: kein
 * Sektor-Abbild, sondern der **rohe Nibble-Strom** einer Spur, so wie
 * der Controller ihn sieht — mit Sync-Feldern, Adress- und Datenfeldern
 * und GCR-Kodierung.
 *
 * ── Was der Leser damit macht ───────────────────────────────────────────
 *
 * `img2_open()` setzt fuer **alle drei** Faelle `spt = 16` und
 * `sector_size = 256`, und `img2_read_track()` rechnet daraus die
 * Schrittweite:
 *
 *     off = data_offset + cyl * spt * sector_size      // = cyl * 4096
 *
 * Eine NIB-Spur ist aber **6656 Byte** gross. Zwei Quellen:
 *
 *   * `mamedev/mame@c0d3677674` `src/lib/formats/ap2_dsk.h:150`
 *     `static constexpr size_t nibbles_per_track = 0x1a00;` (= 6656),
 *     und `:975` liest mit `track * nibbles_per_track`.
 *   * Der Baum selbst: `src/formats/nib/uft_nib.c:9`
 *     `#define NIB_TRACK_SIZE 6656`.
 *
 * **Zwei Folgen, und die zweite ist die schlimmere:**
 *
 * 1. Ab Spur 1 liest der Leser an der falschen Stelle — Spur `n`
 *    beginnt bei `n * 6656`, gesucht wird bei `n * 4096`.
 * 2. Er gibt die gelesenen Bytes als **Sektoren mit Nummern 0..15**
 *    aus. Rohe Nibbles sind keine Sektoren: sie sind GCR-kodiert und
 *    tragen ihre Nummern in Adressfeldern. Die Struktur wird also
 *    **erfunden** — genau das, was `DESIGN_PRINCIPLES` „keine erfundenen
 *    Daten" nennt.
 *
 * Punkt 2 waere auch bei richtiger Schrittweite falsch. Es ist kein
 * Rechenfehler, sondern eine Bedeutungsverwechslung.
 *
 * ── Warum hier abgewiesen und nicht dekodiert wird ──────────────────────
 *
 * Seit MF-715/721 hat der Baum einen GCR-Dekoder, der genau diesen
 * Nibble-Strom lesen koennte. Ihn hier zu verdrahten waere aber neuer
 * Code im Format-Layer **ohne Pruefmoeglichkeit**: `nib` steht nach
 * GCR-3 (MF-723) auf Unabhaengigkeit gesperrt — der einzige verfuegbare
 * Erzeuger benutzt dieselben `nibblize_*.c` wie das Oracle, gegen das
 * unser Dekoder geeicht ist. Ein Differenzlauf waere tautologisch.
 *
 * Also: **abweisen mit Begruendung**, bis GCR-3 geoeffnet ist. Ein Leser,
 * der ehrlich sagt „diesen Inhalt kann ich nicht", ist besser als einer,
 * der Nibbles als Sektoren ausgibt.
 *
 * ── Was hier NICHT behandelt wird ───────────────────────────────────────
 *
 * Die Faelle 0 und 1 (DOS- gegen ProDOS-Reihenfolge). Der Leser liest das
 * Feld und **verwirft es** — `IMG2_FMT_DOS` und `IMG2_FMT_PRODOS` sind
 * definiert und nirgends benutzt. Damit erfaehrt der Aufrufer nicht, in
 * welchem Nummernkreis die Sektor-IDs stehen, die er bekommt. Das ist
 * real, aber es fehlt der Kanal, ueber den ein Leser so etwas mitteilen
 * koennte — eine Architekturfrage, kein Tagesrand. Steht als offene
 * Haelfte in `docs/OPEN_ITEMS.md` FMT-19.
 */

#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

extern const uft_format_plugin_t uft_format_plugin_2img;

#define HDR        64
#define NIB_TRACK  6656
#define TRACKS     35

static int fehler = 0;

#define PRUEFE(bed, ...) do {                                            \
    if (!(bed)) { printf("  FAIL "); printf(__VA_ARGS__);                \
                  printf("\n"); fehler++; }                              \
} while (0)

static void le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("2IMG mit NIB-Inhalt: Schrittweite und Bedeutung (MF-725)\n\n");

    /* Ein 2IMG, das NIB ankuendigt: 35 Spuren zu 6656 Byte. Jede Spur
     * traegt ihre Nummer in den ersten Bytes, damit eine falsche
     * Schrittweite auffaellt. */
    size_t n = HDR + (size_t)TRACKS * NIB_TRACK;
    uint8_t *img = calloc(1, n);
    if (!img) { printf("kein Speicher\n"); return 2; }

    memcpy(img, "2IMG", 4);
    memcpy(img + 4, "UFT!", 4);
    img[0x08] = HDR;                       /* Kopfgroesse   */
    img[0x0A] = 1;                         /* Version       */
    le32(img + 0x0C, 2);                   /* NIB           */
    le32(img + 0x18, HDR);                 /* Datenversatz  */
    le32(img + 0x1C, (uint32_t)(n - HDR)); /* Datenlaenge   */

    for (int t = 0; t < TRACKS; t++) {
        uint8_t *sp = img + HDR + (size_t)t * NIB_TRACK;
        memset(sp, 0xFF, NIB_TRACK);       /* Sync, wie eine echte Spur */
        sp[0] = (uint8_t)t;                /* die Spurnummer            */
        sp[1] = 0xD5; sp[2] = 0xAA; sp[3] = 0x96;
    }

    char pfad[L_tmpnam + 8];
    if (!tmpnam(pfad)) { printf("kein Wegwerf-Name\n"); free(img); return 2; }
    FILE *f = fopen(pfad, "wb");
    if (!f) { printf("Wegwerf-Datei nicht anlegbar\n"); free(img); return 2; }
    size_t w = fwrite(img, 1, n, f);
    fclose(f);
    if (w != n) { printf("Wegwerf-Datei unvollstaendig\n"); free(img); return 2; }

    printf("  Vorlage: 2IMG, Feld 0x0C = 2 (NIB), %d Spuren zu %d Byte\n",
           TRACKS, NIB_TRACK);

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    uft_error_t rc = uft_format_plugin_2img.open(&disk, pfad, true);
    printf("  open -> %s (rc=%d)\n",
           rc == UFT_OK ? "ANGENOMMEN" : "abgewiesen", rc);

    PRUEFE(rc != UFT_OK,
           "ein 2IMG mit NIB-Inhalt wird geoeffnet. Der Leser rechnet dann "
           "mit 16*256 = 4096 Byte je Spur statt 6656 und gibt rohe "
           "Nibbles als Sektoren 0..15 aus — erfundene Struktur "
           "(DESIGN_PRINCIPLES)");

    if (rc == UFT_OK) {
        /* Wenn es doch geoeffnet wurde: zeigen, wie falsch es wird. */
        uft_track_t trk;
        memset(&trk, 0, sizeof(trk));
        if (uft_format_plugin_2img.read_track(&disk, 1, 0, &trk) == UFT_OK
            && trk.sector_count > 0) {
            printf("     Spur 1, erstes Byte: 0x%02X — erwartet 0x01, "
                   "gelesen wurde bei Versatz %d statt %d\n",
                   trk.sectors[0].data[0], HDR + 4096, HDR + NIB_TRACK);
        }
        uft_format_plugin_2img.close(&disk);
    }

    /* Gegenprobe: ein Sektor-2IMG (Feld 0 = DOS) MUSS weiterhin gehen —
     * die Abweisung darf nicht zu breit sein. */
    size_t n2 = HDR + 35u * 16u * 256u;
    uint8_t *img2 = calloc(1, n2);
    if (img2) {
        memcpy(img2, "2IMG", 4);
        memcpy(img2 + 4, "UFT!", 4);
        img2[0x08] = HDR; img2[0x0A] = 1;
        le32(img2 + 0x0C, 0);                    /* DOS-Reihenfolge */
        le32(img2 + 0x18, HDR);
        le32(img2 + 0x1C, (uint32_t)(n2 - HDR));
        for (size_t i = HDR; i < n2; i++) img2[i] = (uint8_t)(i & 0xFF);

        char pfad2[L_tmpnam + 8];
        if (tmpnam(pfad2)) {
            FILE *g = fopen(pfad2, "wb");
            if (g) {
                size_t w2 = fwrite(img2, 1, n2, g);
                fclose(g);
                if (w2 == n2) {
                    memset(&disk, 0, sizeof(disk));
                    uft_error_t rc2 =
                        uft_format_plugin_2img.open(&disk, pfad2, true);
                    printf("  Sektor-2IMG (Feld 0x0C = 0) -> %s\n",
                           rc2 == UFT_OK ? "angenommen" : "ABGEWIESEN");
                    PRUEFE(rc2 == UFT_OK,
                           "ein gewoehnliches Sektor-2IMG wird abgewiesen — "
                           "die NIB-Abweisung greift zu breit");
                    if (rc2 == UFT_OK) uft_format_plugin_2img.close(&disk);
                }
                remove(pfad2);
            }
        }
        free(img2);
    }

    remove(pfad);
    free(img);

    printf("\n  Was die gruene Ampel heisst: ein 2IMG, dessen Kopf NIB "
           "ankuendigt, wird\n"
           "  abgewiesen statt als Sektor-Abbild missdeutet — und "
           "gewoehnliche\n"
           "  Sektor-2IMGs gehen weiterhin.\n"
           "  Was sie NICHT heisst: dass NIB gelesen werden kann. Der "
           "GCR-Dekoder\n"
           "  koennte es, aber die Pruefung dafuer ist nach GCR-3 "
           "(MF-723) gesperrt.\n");

    printf("\n%s (%d Abweichungen)\n", fehler ? "ROT" : "GRUEN", fehler);
    return fehler ? 1 : 0;
}
