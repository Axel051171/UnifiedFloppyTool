/**
 * @file test_hfe_interface_modes.c
 * @brief HFE meldet die falsche Maschine (MF-659)
 *
 * ── Der Befund ───────────────────────────────────────────────────────────
 *
 * Der Baum führte die HFE-Interface-Modi **zweimal**, und die beiden
 * Fassungen widersprachen sich ab 0x08:
 *
 *   `include/uft/uft_hfe_format.h:53-68`   (richtig)
 *       0x07 GENERIC_SHUGART · 0x08 IBMPC_ED · 0x09 MSX2_DD
 *       0x0A C64_DD · 0x0B EMU_SHUGART · 0x0C S950_DD · 0x0D S950_HD
 *
 *   `src/formats/hfe/uft_hfe.c` (lokales Enum)  (falsch)
 *       0x07 GENERIC_8BIT · 0x08 MSX2_DD · 0x09 C64_DD
 *       0x0A EMU_SHUGART · 0x0B S950_DD · 0x0C S950_HD
 *
 * `IBMPC_ED` fehlte, und dadurch war **alles ab 0x08 um eins verschoben**.
 *
 * Der Fehler saß im **registrierten** Plugin — also in dem, was der
 * Benutzer sieht. `hfe_read_metadata(disk, "interface", …)` weist eine
 * echte MSX2-Diskette (`interface_mode = 0x09`) als **„C64"** aus, und
 * eine PC-ED-Diskette (`0x08`) als „MSX2 DD". Kein Datenverlust, aber
 * eine stille Falschaussage über die Herkunft eines Mediums — für ein
 * Werkzeug, das Archive bedient, die schlimmere Sorte Fehler.
 *
 * Gefunden vom `uft-variants`-Agenten in seinem ersten Zyklus, genau in
 * der Lage, für die seine Regel 2 geschrieben ist: *widersprechen sich
 * zwei Leser im eigenen Baum, ist das ein Befund über uns.*
 *
 * ── Die Belege, alle drei selbst nachgelesen ─────────────────────────────
 *
 *   1. HxC, der Urheber des Formats:
 *      `libhxcfe.h:414-416` — IBMPC_ED 0x08, MSX2_DD 0x09, C64_DD 0x0A
 *   2. SAMdisk, unabhängig:
 *      `src/samdisk/hfe.cpp:35-53` — dieselbe Reihenfolge, IBMPC_ED
 *      an Position 8
 *   3. Unser eigener kanonischer Header (siehe oben)
 *
 * Drei Quellen, eine Antwort. Die Einfrier-Regel erlaubt das
 * ausdrücklich: Spec-Korrektur gegen eine autoritative Quelle.
 *
 * ── Warum der Test durch den ECHTEN Pfad geht ────────────────────────────
 *
 * Ein Test, der nur die Konstanten im kanonischen Header nachrechnet,
 * wäre sofort grün gewesen — der Header war ja richtig. Er hätte den
 * Fehler nicht berührt. Dieser Test baut deshalb eine HFE-Datei mit
 * gesetztem Interface-Byte, öffnet sie über das **registrierte Plugin**
 * und fragt genau die Zeichenkette ab, die die Oberfläche anzeigen
 * würde. Ein Beweis, der die fragliche Stelle nicht durchläuft, beweist
 * nichts (MF-649).
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_hfe;

static int fehler;

/* Ein minimaler, gueltiger HFEv1-Kopf. Die Werte stammen aus dem
 * Leser selbst (uft_hfe.c:78ff), nicht aus dem Gedaechtnis. */
#define HFE_HDR   512
#define TRACKLIST 512

static bool schreibe_hfe(const char *pfad, uint8_t interface_mode,
                         uint8_t track_encoding)
{
    uint8_t buf[HFE_HDR + TRACKLIST];
    memset(buf, 0, sizeof(buf));

    memcpy(buf + 0, "HXCPICFE", 8);
    buf[8]  = 0;                 /* format_revision            */
    buf[9]  = 1;                 /* number_of_tracks           */
    buf[10] = 1;                 /* number_of_sides            */
    buf[11] = track_encoding;    /* track_encoding             */
    buf[12] = 250; buf[13] = 0;  /* bitRate (LE16), 250 kbit/s */
    buf[14] = 300; buf[15] = 1;  /* floppyRPM (LE16)           */
    buf[16] = interface_mode;    /* floppyinterfacemode        */
    buf[17] = 0;                 /* dnu / reserved             */
    buf[18] = 1; buf[19] = 0;    /* track_list_offset (LE16)   */
    buf[20] = 0xFF;              /* write_allowed              */

    FILE *f = fopen(pfad, "wb");
    if (!f) return false;
    bool ok = fwrite(buf, 1, sizeof(buf), f) == sizeof(buf);
    fclose(f);
    return ok;
}

static void pruefe_interface(uint8_t mode, const char *erwartet)
{
    char pfad[512];
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(pfad, sizeof(pfad), "%s/uft_hfe_if_%02X_%d.hfe",
             dir, mode, rand() % 10000);

    if (!schreibe_hfe(pfad, mode, 0x00 /* ISOIBM_MFM */)) {
        printf("  FAIL 0x%02X: Testdatei nicht schreibbar\n", mode);
        fehler++;
        return;
    }

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    uft_error_t e = uft_format_plugin_hfe.open(&disk, pfad, true);
    if (e != UFT_OK) {
        printf("  FAIL 0x%02X: Plugin oeffnet die Datei nicht (%d)\n",
               mode, (int)e);
        fehler++;
        remove(pfad);
        return;
    }

    char wert[64] = {0};
    e = uft_format_plugin_hfe.read_metadata(&disk, "interface",
                                            wert, sizeof(wert));
    uft_format_plugin_hfe.close(&disk);
    remove(pfad);

    if (e != UFT_OK) {
        printf("  FAIL 0x%02X: read_metadata(\"interface\") liefert %d\n",
               mode, (int)e);
        fehler++;
        return;
    }
    if (strcmp(wert, erwartet) != 0) {
        printf("  FAIL 0x%02X meldet \"%s\", erwartet \"%s\"\n",
               mode, wert, erwartet);
        fehler++;
    } else {
        printf("  ok   0x%02X -> \"%s\"\n", mode, wert);
    }
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("HFE-Interface-Modi durch den echten Anzeigepfad (MF-659)\n");

    /* Unstrittig — sie standen in beiden Fassungen gleich. Sie sind
     * trotzdem hier: ein Test, der nur die strittige Haelfte prueft,
     * liesse eine Verschiebung am Tabellenanfang durch. */
    pruefe_interface(0x00, "IBM PC DD");
    pruefe_interface(0x01, "IBM PC HD");
    pruefe_interface(0x02, "Atari ST DD");
    pruefe_interface(0x04, "Amiga DD");
    pruefe_interface(0x06, "Amstrad CPC");

    /* Ab hier lag der Fehler. */
    pruefe_interface(0x08, "IBM PC ED");   /* fehlte im Plugin ganz   */
    pruefe_interface(0x09, "MSX2 DD");     /* meldete bisher "C64"    */
    pruefe_interface(0x0A, "C64 DD");      /* meldete bisher "Generic"*/

    printf("\n%s (%d Abweichungen)\n",
           fehler ? "FEHLGESCHLAGEN" : "OK", fehler);
    return fehler ? 1 : 0;
}
