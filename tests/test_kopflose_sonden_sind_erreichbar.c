/**
 * @file test_kopflose_sonden_sind_erreichbar.c
 * @brief mgt und opus waren ueber die Erkennung unerreichbar (MF-1074)
 *
 * ── Der erste Befund: `(void)file_size;`, zweimal ───────────────────────
 *
 * MGT und Opus Discovery sind **kopflose** Formate: keine Kennung, keine
 * Magie am Dateianfang. Beide Sonden erkennen sie deshalb ueber die
 * **Dateigroesse** — `uft_mgt_probe()` verlangt 819 200 oder 409 600,
 * und `uft_opus_probe()` verlangt seit MF-905, dass die im Bootsektor
 * angesagte Geometrie die Dateigroesse **restlos erklaert**
 * (`cyls * heads * sectors * ssize == size`).
 *
 * Bekommen haben beide die **Puffergroesse**. `mgt_probe_plugin()` und
 * `opus_probe_plugin()` begannen mit `(void)file_size;` und reichten
 * `size` weiter — und `size` ist der Sondenpuffer, 4096 Byte. Die
 * Bedingung konnte damit **nie** zutreffen:
 *
 *     mgt  : 4096 != 819200 -> false, immer
 *     opus : keine Geometrie ergibt 4096 -> false, immer
 *
 * Beide Plugins waren also **ueber die Erkennung unerreichbar**, waehrend
 * `open()` dieselben Dateien einwandfrei las. Das ist die MF-1029-Falle
 * (dort war der Groessenrueckfall toter Code, weil die Sonde 4096 statt
 * der Dateigroesse verglich), und sie steht damit zum **siebten** Mal in
 * diesem Baum — nach `myz80`, `nanowasp`, `2img`, `cpm`, `pri` und
 * `logical`.
 *
 * **Gemessen wurde es an echten Abbildern, die hier nicht liegen duerfen.**
 * Drei +D-Abbilder (je 819 200 Byte) und zwei Opus-Discovery-Abbilder
 * (737 280 und 184 320) lagen in einem Archiv im Baum; ihre Lizenz ist
 * ungeklaert und die Entscheidung darueber steht als **P3-359**. Gemessen
 * haben sie `probe = 0` bei `open = 0` mit 160 Spuren / 1600 Sektoren
 * bzw. 160 / 2880 und 40 / 720. Dieser Test kommt deshalb **ohne sie
 * aus**: er erzeugt die Puffer selbst, und das genuegt, weil der Defekt
 * genau an der Weitergabe der Dateigroesse haengt.
 *
 * ── Der zweite Befund, den der erste verdeckt hat ───────────────────────
 *
 * In `uft_mgt_probe()` stand `valid_entries >= 4 -> Konfidenz 75`, und
 * `valid_entries` zaehlte den **freien** Eintrag (`type == 0`) mit. Acht
 * freie Plaetze sind aber kein Strukturbeleg, sondern die Abwesenheit von
 * Struktur: **eine Diskette aus lauter Nullen haette 75 bekommen** — das
 * Band „Struktur gelesen" nach MF-729, fuer nichts.
 *
 * Aufgefallen ist es nicht, weil die Sonde unerreichbar war: die Eichung
 * `test_probe_confidence_on_zeros` faehrt 819 200 Byte in ihrer
 * Groessenliste, konnte MGT aber nie zum Sprechen bringen. **Der erste
 * Defekt hat den zweiten vor seinem eigenen Tor versteckt**, und das
 * Reparieren des einen haette ohne den anderen die Eichung gerissen.
 *
 * Seit MF-1074 ist ein **benutzter** Eintrag mit gueltigem Namen der
 * Beleg; ohne ihn bleibt es bei der Groesse, und die meldet **40** statt
 * der alten 50 (die genau auf der Bandgrenze lag).
 *
 * **Und die Unterscheidung ist an den echten Abbildern belegt:** das
 * frisch formatierte `plusd_clean_dsdd.mgt` bekommt **40**, das mit
 * Dateien belegte `plusd_gdos_tools_dsdd.mgt` bekommt **75**. Genau so
 * herum.
 *
 * ── Was NICHT geprueft ist ──────────────────────────────────────────────
 *
 * * **Die Stufe.** `mgt` und `opus` bleiben auf T2: der Beleg von fremder
 *   Hand liegt vor, darf aber nicht in den Korpus (P3-359).
 * * **Der Inhalt** der gelesenen Sektoren — dieser Test fragt die
 *   Erkennung, nicht die Ablage. Dafuer gibt es `test_mgt_gegen_mame`
 *   und `test_opd_geometrie`.
 * * **Die Schreibseite** beider Formate.
 */
#include "uft/uft_format_plugin.h"
#include "uft/formats/uft_mgt.h"
#include "uft/formats/uft_opus.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

extern const uft_format_plugin_t uft_format_plugin_mgt;
extern const uft_format_plugin_t uft_format_plugin_opus;

#define PUFFER 4096u

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung, const char *detail)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s - %s\n", was, detail ? detail : ""); }
}

/* Ein MGT-Verzeichnis, wie es auf Spur 0 liegt: vier Sektoren zu je
 * zwei 256-Byte-Eintraegen. `benutzt` legt einen Eintrag mit Typ 1 und
 * gueltigem Namen an. */
static void mgt_verzeichnis(uint8_t *p, int benutzt)
{
    memset(p, 0, PUFFER);
    if (benutzt) {
        p[0] = 1;                       /* type: BASIC-Programm */
        memcpy(p + 1, "HALLO     ", 10);
        p[11] = 4;                      /* sectors_used */
        p[12] = 4;                      /* track */
        p[13] = 1;                      /* sector */
    }
}

/* Ein Opus-Bootsektor: JR-Opcode, dann Zylinder, Sektoren und Flags.
 * heads = (flags & 0x10) ? 2 : 1, ssize = 128 << (flags >> 6). */
static void opus_bootsektor(uint8_t *p, uint8_t zyl, uint8_t sekt,
                            int zwei_koepfe)
{
    memset(p, 0, PUFFER);
    p[0] = 0x18;                        /* JR */
    p[1] = 0x00;
    p[2] = zyl;
    p[3] = sekt;
    p[4] = (uint8_t)(0x40 | (zwei_koepfe ? 0x10 : 0x00));  /* 256 Byte */
}

int main(void)
{
    uint8_t *puffer = (uint8_t *)malloc(PUFFER);
    char det[200];
    int k;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Kopflose Sonden sind erreichbar - MF-1074\n");
    printf("=========================================\n");
    if (!puffer) { printf("kein Speicher\n"); return 2; }

    /* ── 1. MGT: die Dateigroesse erreicht die Sonde ──────────────── */
    mgt_verzeichnis(puffer, 0);
    k = -1;
    {
        int ok = uft_format_plugin_mgt.probe(puffer, PUFFER,
                                             MGT_DISK_SIZE, &k) ? 1 : 0;
        snprintf(det, sizeof det, "probe=%d konf=%d", ok, k);
        pruefe("mgt: ein leeres Verzeichnis in einer 819 200-Byte-Datei "
               "wird ANGENOMMEN - vor MF-1074 reichte der Wrapper die "
               "Puffergroesse 4096 weiter und die Sonde sagte immer nein",
               ok == 1, det);
        pruefe("mgt: und zwar im Band \"nur die Groesse\" (< 50, MF-729) - "
               "vor MF-1074 haette derselbe Nullpuffer **75** bekommen, "
               "weil freie Eintraege als Strukturbeleg zaehlten",
               ok == 1 && k > 0 && k < 50, det);
    }

    /* ── 2. MGT: ein benutzter Eintrag ist der Strukturbeleg ──────── */
    mgt_verzeichnis(puffer, 1);
    k = -1;
    {
        int ok = uft_format_plugin_mgt.probe(puffer, PUFFER,
                                             MGT_DISK_SIZE, &k) ? 1 : 0;
        snprintf(det, sizeof det, "probe=%d konf=%d", ok, k);
        pruefe("mgt: EIN benutzter Eintrag mit gueltigem Namen hebt die "
               "Konfidenz ins Band \"Struktur gelesen\" (50-79) - an "
               "echten Abbildern gemessen: clean 40, mit Dateien 75",
               ok == 1 && k >= 50 && k < 80, det);
    }

    /* ── 3. MGT: eine falsche Groesse wird abgewiesen ─────────────── */
    mgt_verzeichnis(puffer, 1);
    k = -1;
    {
        int ok = uft_format_plugin_mgt.probe(puffer, PUFFER,
                                             MGT_DISK_SIZE + 1u, &k) ? 1 : 0;
        snprintf(det, sizeof det, "probe=%d konf=%d", ok, k);
        pruefe("mgt: 819 201 Byte werden abgewiesen - die Groesse ist das "
               "einzige Merkmal, also muss sie genau stimmen", ok == 0, det);
    }

    /* ── 4. MGT: ein zu kleiner Puffer urteilt nicht ──────────────── */
    mgt_verzeichnis(puffer, 1);
    k = -1;
    {
        int ok = uft_format_plugin_mgt.probe(puffer, 1024u,
                                             MGT_DISK_SIZE, &k) ? 1 : 0;
        snprintf(det, sizeof det, "probe=%d konf=%d", ok, k);
        pruefe("mgt: ein Puffer, der das Verzeichnis nicht traegt, fuehrt "
               "zu NEIN statt zu einem Urteil ueber ungelieferte Bytes",
               ok == 0, det);
    }

    /* ── 5. Opus: die Dateigroesse erreicht die Sonde ─────────────── */
    opus_bootsektor(puffer, 80, 18, 1);          /* 80*2*18*256 = 737280 */
    k = -1;
    {
        int ok = uft_format_plugin_opus.probe(puffer, PUFFER,
                                              737280u, &k) ? 1 : 0;
        snprintf(det, sizeof det, "probe=%d konf=%d", ok, k);
        pruefe("opus: ein Bootsektor, dessen Geometrie 737 280 Byte "
               "restlos erklaert, wird ANGENOMMEN - vor MF-1074 verglich "
               "die Sonde gegen die Puffergroesse 4096 und konnte nie "
               "zustimmen", ok == 1 && k > 0, det);
    }

    opus_bootsektor(puffer, 40, 18, 0);          /* 40*1*18*256 = 184320 */
    k = -1;
    {
        int ok = uft_format_plugin_opus.probe(puffer, PUFFER,
                                              184320u, &k) ? 1 : 0;
        snprintf(det, sizeof det, "probe=%d konf=%d", ok, k);
        pruefe("opus: dasselbe fuer die einseitige 184 320-Byte-Diskette",
               ok == 1 && k > 0, det);
    }

    /* ── 6. Opus: Bootsektor und Dateigroesse muessen zusammenpassen ─ */
    opus_bootsektor(puffer, 80, 18, 1);
    k = -1;
    {
        int ok = uft_format_plugin_opus.probe(puffer, PUFFER,
                                              184320u, &k) ? 1 : 0;
        snprintf(det, sizeof det, "probe=%d konf=%d", ok, k);
        pruefe("opus: ein Bootsektor, der 737 280 ansagt, wird an einer "
               "184 320-Byte-Datei ABGEWIESEN - genau das ist die "
               "Pruefung aus MF-905, und sie war unerreichbar",
               ok == 0, det);
    }

    /* ── 7. Opus: ein Nullpuffer hat keinen Sprungbefehl ──────────── */
    memset(puffer, 0, PUFFER);
    k = -1;
    {
        int ok = uft_format_plugin_opus.probe(puffer, PUFFER,
                                              737280u, &k) ? 1 : 0;
        snprintf(det, sizeof det, "probe=%d konf=%d", ok, k);
        pruefe("opus: lauter Nullen werden abgewiesen - `data[0]` ist kein "
               "JR-Opcode; die Eichung nach MF-729 bleibt gewahrt",
               ok == 0, det);
    }

    /* ── 8. Opus: Geometrie stimmt, Sprungbefehl fehlt ───────────── */
    opus_bootsektor(puffer, 80, 18, 1);
    puffer[0] = 0x00;              /* kein JR */
    k = -1;
    {
        int ok = uft_format_plugin_opus.probe(puffer, PUFFER,
                                              737280u, &k) ? 1 : 0;
        snprintf(det, sizeof det, "probe=%d konf=%d", ok, k);
        pruefe("opus: eine Datei, deren Geometriefelder passen, aber ohne "
               "Sprungbefehl beginnt, wird ABGEWIESEN - die Pruefung auf "
               "0x18 ist an einem Nullpuffer redundant (dort greift schon "
               "`cyls == 0`) und NUR hier isolierbar; gemessen statt "
               "behauptet, wie MF-1031 es vorgemacht hat",
               ok == 0, det);
    }

    free(puffer);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
