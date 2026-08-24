/**
 * @file test_geometry_from_file_is_bounded.c
 * @brief Eine Geometrie aus der Datei ist eine Behauptung (MF-543)
 *
 * ── Der Fund ─────────────────────────────────────────────────────────────
 *
 * Gefunden vom Oeffnungs-Fuzzer, nachdem er um 25 Sonden-Kennungen
 * erweitert wurde und dadurch Plugins erreichte, die nie eine Eingabe
 * gesehen hatten. Er terminierte nicht mehr: 55 GB Arbeitsspeicher bei
 * 61 GB RAM, dann `0xC0000409` (STATUS_STACK_BUFFER_OVERRUN).
 *
 * Die Ursache ist eine Kuerzung, die niemand sieht:
 *
 *     uft_disk_alloc(uint16_t ntracks, uint8_t nheads)   <- heads ist uint8
 *         d->track_count = ntracks * nheads;             <- mit 44
 *
 *     for (uint16_t h = 0; h < heads; h++)               <- heads ist uint16
 *         size_t idx = c * heads + h;                    <- mit 300
 *         disk->track_data[idx] = track;                 <- schreibt bis 599
 *
 * `uft_qrst.c` liest `heads` als uint16 aus einem 22-Byte-Kopf und prueft
 * nur `!= 0`. Der Aufruf kuerzt auf `heads & 0xFF`, die Schleife nicht.
 * Bei `heads = 300` hat das Feld 2 x 44 = 88 Plaetze und wird bis Index
 * 599 beschrieben.
 *
 * **22 Byte** genuegen. Keine Nutzdaten, kein gueltiges Abbild — nur ein
 * Kopf, dessen Zahlen niemand nachrechnet.
 *
 * ── Was dieser Test prueft ───────────────────────────────────────────────
 *
 * Nicht "stuerzt es ab" — ein Test, der auf einen Absturz wartet, nimmt
 * den Absturz in Kauf. Geprueft wird die Aussage davor:
 *
 *     Was `uft_disk_open()` als Geometrie MELDET, muss zu dem passen,
 *     was dahinter WIRKLICH angelegt ist.
 *
 * Vor der Reparatur meldete der Pfad "2 Zylinder x 44 Koepfe" fuer eine
 * Datei, die 300 Koepfe behauptet — die Kuerzung war also schon in der
 * Anzeige sichtbar, nur hat sie niemand mit der Schleife verglichen.
 *
 * Der Test faehrt ueber `uft_disk_open()`, nicht ueber das Plugin. Ein
 * Fehler, der nur ueber den Produktionspfad erreichbar ist, muss auch
 * ueber ihn geprueft werden — sonst prueft man einen Weg, den kein
 * Benutzer nimmt.
 *
 * ── Warum eine ganze Familie geprueft wird ───────────────────────────────
 *
 * `uft_disk_alloc()` hat 14 Aufrufer. Sechs Plugins benutzen das Muster
 * `idx = c * heads + h`: cfi, logical, myz80, nanowasp, qrst, rcpmfs.
 * Von denen lesen zwei `heads` als uint16 aus der Datei — QRST ungeprueft,
 * Logical von seiner eigenen Sonde auf 4 gedeckelt.
 *
 * Ein Test nur fuer QRST haette den naechsten Fall dieser Familie nicht
 * gefangen. Geprueft wird deshalb die EIGENSCHAFT, nicht der Einzelfall.
 */

#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int failures;

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

/** Legt eine Datei an und gibt ihren Pfad zurueck. */
static int write_file(const char *path, const uint8_t *data, size_t n)
{
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    if (n) fwrite(data, 1, n, f);
    fclose(f);
    return 1;
}

/**
 * Oeffnet die Datei ueber den Produktionspfad und prueft die eine
 * Eigenschaft, um die es geht: die gemeldete Kopfzahl muss die sein, mit
 * der auch gerechnet wurde.
 */
static void expect_bounded(const char *label, const char *path,
                           const uint8_t *data, size_t n)
{
    if (!write_file(path, data, n)) {
        printf("  FAIL %-18s Datei nicht schreibbar\n", label);
        failures++;
        return;
    }

    uft_disk_t *disk = uft_disk_open(path, "rb");
    if (!disk) {
        /* Ablehnen ist die beste Antwort: der Kopf ist unsinnig. */
        printf("  ok   %-18s abgelehnt (uft_disk_open lieferte NULL)\n", label);
        remove(path);
        return;
    }

    const uft_geometry_t *g = &disk->geometry;
    printf("  ---  %-18s geoeffnet: %d Zylinder x %d Koepfe x %d Sektoren\n",
           label, g->cylinders, g->heads, g->sectors);

    /* Die pruefbare Eigenschaft ist NICHT die gemeldete Kopfzahl — die ist
     * bereits gekuerzt (44 statt 300) und sieht harmlos aus. Genau darum
     * ist der Fehler so lange unbemerkt geblieben: die Anzeige log nicht,
     * sie zeigte die eine Zahl, waehrend die Schleife mit der anderen lief.
     *
     * Pruefbar von aussen ist stattdessen: **passt die behauptete
     * Geometrie ueberhaupt in die Datei?** Ein 22-Byte-Kopf, der
     * 2 x 300 x 1 x 512 = 307200 Byte Nutzdaten behauptet, ist in sich
     * widersprueglich. Eine solche Datei zu OEFFNEN ist der Fehler; was
     * danach passiert, ist nur die Folge. */
    {
        unsigned long long need = (unsigned long long)g->cylinders * g->heads
                                * g->sectors * g->sector_size;
        printf("       Kopf behauptet %llu Byte Nutzdaten, Datei hat %zu\n",
               need, n);
        if (need > (unsigned long long)n) {
            printf("  FAIL %-18s geoeffnet, obwohl der Kopf mehr behauptet\n"
                   "       als die Datei hergibt — die Geometrie ist "
                   "ungeprueft uebernommen\n", label);
            failures++;
        } else {
            printf("  ok   %-18s Geometrie passt in die Datei\n", label);
        }
    }

    uft_disk_close(disk);
    remove(path);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    if (uft_register_all_formats() != UFT_OK) {
        printf("FEHLER: uft_register_all_formats() schlug fehl\n");
        return 1;
    }

    printf("=== Eine Geometrie aus der Datei ist eine Behauptung (MF-543) ===\n");

    /* QRST: 22-Byte-Kopf, 300 Koepfe. Genau die Eingabe, an der der
     * Fuzzer haengenblieb. */
    {
        uint8_t h[22];
        memset(h, 0, sizeof(h));
        memcpy(h, "QRST", 4);
        put16(h + 4,  1);      /* version     */
        put16(h + 6,  2);      /* cylinders   */
        put16(h + 8,  300);    /* heads  -> gekuerzt auf 44 */
        put16(h + 10, 1);      /* sectors     */
        put16(h + 12, 512);    /* sector_size */
        expect_bounded("QRST 300 Koepfe", "uft_geo_qrst.bin", h, sizeof(h));
    }

    /* Und die Gegenprobe: derselbe Kopf mit einer plausiblen Kopfzahl darf
     * NICHT abgelehnt werden. Ein Tor, das alles ablehnt, ist kein Tor. */
    {
        uint8_t h[22];
        memset(h, 0, sizeof(h));
        memcpy(h, "QRST", 4);
        put16(h + 4,  1);
        put16(h + 6,  2);
        put16(h + 8,  2);      /* heads = 2, ganz normal */
        put16(h + 10, 9);
        put16(h + 12, 512);
        /* Nutzdaten dazu, damit die Datei zu ihrem eigenen Kopf passt. */
        size_t payload = (size_t)2 * 2 * 9 * 512;
        uint8_t *buf = calloc(1, sizeof(h) + payload);
        if (buf) {
            memcpy(buf, h, sizeof(h));
            for (size_t i = 0; i < payload; i++)
                buf[sizeof(h) + i] = (uint8_t)(i * 7 + 3);
            expect_bounded("QRST 2 Koepfe", "uft_geo_qrst2.bin",
                           buf, sizeof(h) + payload);
            free(buf);
        }
    }

    /* NanoWasp: dieselbe Familie, andere Spielart. Die Schleifen sind hier
     * uint8-begrenzt, aber `sector_size` kommt ungeprueft aus dem Kopf und
     * geht in ein malloc je Sektor — bis 255 x 255 x 255 Sektoren zu je
     * 65535 Byte. */
    {
        static const char SIG[] = "nanowasp floppy image\r\n\x1A";
        uint8_t h[80];
        memset(h, 0, sizeof(h));
        memcpy(h, SIG, sizeof(SIG) - 1);
        expect_bounded("NanoWasp Kopf", "uft_geo_nw.bin", h, sizeof(h));
    }

    printf("\n%s (%d Abweichungen)\n",
           failures ? "FEHLGESCHLAGEN" : "OK", failures);
    return failures ? 1 : 0;
}
