/**
 * @file test_flux_truncation_is_reported.c
 * @brief Eine gekappte Spur muss es sagen (MF-550)
 *
 * ── Der Fund ─────────────────────────────────────────────────────────────
 *
 * Drei Wandler bauen aus einem Bitstrom Flusswechsel und legen sie in einem
 * Puffer fuer 131072 Eintraege ab. Was nicht hineinpasste, verschwand:
 *
 *     if (flux_count < 131072) {
 *         flux_buf[flux_count++] = accum_ns;
 *     }
 *     accum_ns = 0;
 *
 * Keine Warnung, kein Zaehler, und die Spur galt weiterhin als gewandelt.
 *
 * ── Warum das nicht theoretisch ist ──────────────────────────────────────
 *
 * `lut[].length` ist ein uint16 aus der HFE-Datei, also bis 65535; je Seite
 * die Haelfte, 32767 Byte. Bei acht Bit je Byte sind das **262136** moegliche
 * Flusswechsel gegen einen Deckel von 131072 — die Haelfte faellt weg.
 *
 * Eine normale Amiga-DD-Spur hat 12500 Byte und damit hoechstens 100000
 * Uebergaenge; sie bleibt darunter. Der Deckel greift also nicht im Alltag,
 * sondern genau dann, wenn eine Datei ungewoehnlich ist — und das ist der
 * Fall, in dem ein forensisches Werkzeug am wenigsten schweigen darf.
 *
 * ── Was schlimmer ist als das Fehlen ─────────────────────────────────────
 *
 * `accum_ns = 0` lief weiter mit. Nach der Kappung wird also weiter
 * zurueckgesetzt, ohne dass etwas abgelegt wird. Selbst wenn spaeter wieder
 * Platz waere, stimmte der Zeitbezug nicht mehr. Der abgeschnittene Teil ist
 * nicht "etwas weniger Daten", sondern gar keine verwertbare Aussage.
 *
 * ── Was dieser Test prueft ───────────────────────────────────────────────
 *
 * Er baut eine HFE, deren Spur laenger ist, als der Puffer fassen kann, und
 * deren Bits dicht genug stehen, dass wirklich mehr als 131072 Uebergaenge
 * entstehen. Dann muss `HFE -> SCP`:
 *
 *   1. die Kappung MELDEN (eine Warnung, die die Zahl nennt), und
 *   2. die Spur NICHT als gewandelt zaehlen.
 *
 * Ohne (1) merkt es niemand. Ohne (2) meldet der Wandler am Ende Erfolg
 * fuer ein Abbild, dem die halbe Spur fehlt.
 */

#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_format_convert.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern uft_error_t uftc_convert_hfe_to_scp(const uint8_t *src_data,
                                           size_t src_size,
                                           const char *dst_path,
                                           const uft_convert_options_ext_t *opts,
                                           uft_convert_result_t *result);

/* Eine Spur mit 32512 Byte je Seite: 65024 gesamt (die LUT-Laenge zaehlt
 * BEIDE Seiten). Alle Bits gesetzt -> ein Uebergang je Zelle -> 260096
 * Uebergaenge je Seite, fast doppelt so viel wie der Puffer fasst.
 *
 * Der erste Anlauf nahm 32768 je Seite und damit 65536 gesamt. Das ist
 * genau eins zu viel: das LUT-Feld ist ein uint16, 65536 wird darin zu 0,
 * und der Wandler ueberspringt eine Spur der Laenge null. Der Test meldete
 * daraufhin "0 Spuren, 0 Warnungen" — nicht weil die Meldung fehlte,
 * sondern weil er nie eine Spur gebaut hatte. Ein Messfehler, der wie ein
 * Befund aussieht. */
#define HEAD_LEN     32512
#define TRACK_TOTAL  (HEAD_LEN * 2)
#define BLOCKS       (TRACK_TOTAL / 512)
#define LUT_BLOCK    1
#define DATA_BLOCK   2
#define FILE_SIZE    ((DATA_BLOCK + BLOCKS) * 512)

static uint8_t img[FILE_SIZE];
static int failures;

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Eine gekappte Spur muss es sagen (MF-550) ===\n");

    memset(img, 0, sizeof(img));

    /* HFE-v1-Kopf. Die Felder stehen so in include/uft/uft_hfe_format.h;
     * ein Zylinder, ein Kopf, damit genau eine Spur entsteht. */
    memcpy(img, "HXCPICFE", 8);
    img[8]  = 0;              /* revision            */
    img[9]  = 1;              /* number_of_track     */
    img[10] = 1;              /* number_of_side      */
    img[11] = 0;              /* track_encoding      */
    put16(img + 12, 250);     /* bitRate kbit/s      */
    put16(img + 14, 300);     /* floppyRPM           */
    img[16] = 7;              /* interface mode      */
    img[17] = 0;
    put16(img + 18, LUT_BLOCK);

    uint8_t *lut = img + LUT_BLOCK * 512;
    put16(lut + 0, DATA_BLOCK);
    put16(lut + 2, TRACK_TOTAL);

    /* Alle Bits gesetzt: jede Zelle ein Uebergang. */
    memset(img + DATA_BLOCK * 512, 0xFF, (size_t)BLOCKS * 512);

    uft_convert_options_ext_t o;
    uft_convert_result_t r;
    memset(&o, 0, sizeof(o));
    memset(&r, 0, sizeof(r));
    o.accept_data_loss = true;

    const char *dst = "uft_trunc_out.scp";
    remove(dst);

    uft_error_t e = uftc_convert_hfe_to_scp(img, sizeof(img), dst, &o, &r);

    printf("  Ergebnis %d: %d Spuren gewandelt, %d gescheitert, "
           "%d Warnungen\n",
           (int)e, r.tracks_converted, r.tracks_failed, r.warning_count);

    int said_it = 0;
    for (int i = 0; i < r.warning_count && i < 8; i++) {
        printf("    %s\n", r.warnings[i]);
        if (strstr(r.warnings[i], "verworfen")) said_it = 1;
    }

    if (!said_it) {
        printf("  FAIL: die Kappung wurde nicht gemeldet — genau der\n"
               "        Zustand, den MF-550 beseitigt hat\n");
        failures++;
    } else {
        printf("  ok   die Kappung wird gemeldet und beziffert\n");
    }

    if (r.tracks_converted > 0) {
        printf("  FAIL: eine gekappte Spur zaehlt als gewandelt\n");
        failures++;
    } else {
        printf("  ok   die gekappte Spur zaehlt nicht als gewandelt\n");
    }

    remove(dst);
    printf("\n%s (%d Abweichungen)\n",
           failures ? "FEHLGESCHLAGEN" : "OK", failures);
    return failures ? 1 : 0;
}
