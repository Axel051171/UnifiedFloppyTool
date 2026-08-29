/**
 * @file test_dim_atari_magic.c
 * @brief `.dim` ohne `BB` ist kein `.dim` (MF-687)
 *
 * ── Die Vorgeschichte, weil sie die Regel zeigt ──────────────────────────
 *
 * Der Fund kam aus einem Scout-Gutachten: Jacknife prüft an Offset 0 ein
 * Magic `0x4242`, unsere Probe prüft **keines**. Beim ersten Anlauf
 * (MF-684) wurde der Parser trotzdem **nicht** geändert, und das war die
 * richtige Entscheidung: **unser eigener Header** beschrieb Byte 0 als
 * „Flags (unused, often 0)" und Byte 1 als „Reserved". Eine verifizierte
 * Fremdquelle gegen die eigene Dokumentation — genau die Lage, aus der
 * fünf fabrizierte Parser kamen (FMT-2/3/10/11/12). Die zweite Quelle
 * war damals nicht beschaffbar.
 *
 * Ein Varianten-Zyklus hat sie beschafft. Jetzt sind es **drei
 * unabhängige Implementierungen**, alle drei von mir im Klon gelesen:
 *
 *   Hatari   `src/floppies/dim.c:75-76`
 *            `if (pDimFile[0x00] != 0x42 || pDimFile[0x01] != 0x42 ||
 *                 pDimFile[0x03] != 0    || pDimFile[0x0A] != 0)`
 *            → "This is not a valid DIM image!", Datei abgelehnt.
 *            Und `:36` als Spec: `0x0000 Word ID Header (0x4242('BB'))`.
 *
 *   HxC      `dim_loader/dim_loader.c:74` und `:110`
 *            `if (header->id_header == 0x4242)` / `!= 0x4242` → ablehnen.
 *
 *   Jacknife `dllmain.c:590`
 *            `*(unsigned short *)buffer == 0x4242  // "BB"`
 *
 * Damit ist die Frage entschieden, die MF-684 offenlassen musste: `BB`
 * gehört zum **Format**, nicht zu einer Fastcopy-Variante. Unser
 * Header-Kommentar war von keiner Quelle gedeckt.
 *
 * ── Was hier geprüft wird ────────────────────────────────────────────────
 *
 * Zwei Abbilder, die sich in **genau zwei Bytes** unterscheiden. Alles
 * andere — Größe, Geometrie, Nutzdaten — ist identisch. Damit kann kein
 * anderer Grund als das Magic den Unterschied erklären.
 *
 * Beide werden synthetisch gebaut, und das ist hier zulässig: geprüft
 * wird nicht, ob wir ein echtes DIM richtig **lesen**, sondern ob unsere
 * Probe eines **ablehnt**, das keines ist. Für die Ablehnung genügt die
 * Kopf-Struktur, und die steht in drei Quellen.
 *
 * ── Warum das mehr ist als Formalismus ───────────────────────────────────
 *
 * Eine Probe ohne Magic entscheidet allein nach Dateigröße und
 * Geometrie-Plausibilität. Jede Datei passender Länge — ein rohes
 * ST-Abbild, ein abgeschnittenes MSA, ein Zufallspuffer — wird dann als
 * DIM angenommen und ihre ersten 32 Byte als Geometrie gelesen. Das ist
 * keine Fehlerkennung am Rand: es ist eine erfundene Geometrie auf
 * fremden Daten, und der Benutzer sieht ein Ergebnis statt einer
 * Absage.
 */

#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_dim_atari;

static int fehler;

#define PRUEFE(bed, ...)                                                   \
    do { if (!(bed)) { printf("  FAIL "); printf(__VA_ARGS__);             \
                       printf("\n"); fehler++; } } while (0)

/* Ein Atari-DIM-Kopf nach der Hatari-Beschreibung (`dim.c:34-45`).
 *
 * 80 Spuren, 2 Seiten, 9 Sektoren, DD — die haeufigste ST-Geometrie.
 * Groesse: 32 Byte Kopf + 80*2*9*512 Nutzdaten. */
#define DIM_HDR   32
#define TRACKS    80
#define SIDES     2
#define SPT       9
#define SEKTOR    512
#define NUTZ      ((size_t)TRACKS * SIDES * SPT * SEKTOR)
#define GESAMT    (DIM_HDR + NUTZ)

static uint8_t *baue_dim(bool mit_magic)
{
    uint8_t *p = (uint8_t *)calloc(1, GESAMT);
    if (!p) return NULL;

    if (mit_magic) {
        p[0x00] = 0x42;          /* 'B' — ID Header, Hatari dim.c:36 */
        p[0x01] = 0x42;          /* 'B' */
    }
    p[0x02] = 1;                 /* Konfiguration automatisch erkannt */
    p[0x03] = 0;                 /* alle Sektoren enthalten */
    p[0x06] = SIDES - 1;         /* Hatari: "add 1 to get sides" */
    p[0x08] = SPT;
    p[0x0A] = 0;                 /* Startspur */
    p[0x0C] = TRACKS - 1;        /* Endspur */
    p[0x0D] = 0;                 /* DD */

    /* Etwas Inhalt, damit kein Nullpuffer geprueft wird. */
    for (size_t i = DIM_HDR; i < GESAMT; i += 512) p[i] = (uint8_t)(i / 512);
    return p;
}

static bool probe(const uint8_t *daten, int *konfidenz)
{
    *konfidenz = 0;
    if (!uft_format_plugin_dim_atari.probe) return false;
    return uft_format_plugin_dim_atari.probe(daten, GESAMT, GESAMT, konfidenz);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("DIM-Probe: ohne `BB` ist es kein DIM (MF-687)\n\n");

    uint8_t *mit  = baue_dim(true);
    uint8_t *ohne = baue_dim(false);
    if (!mit || !ohne) { printf("FEHLER: kein Speicher\n"); return 1; }

    /* Gegenprobe auf den Messaufbau selbst: die beiden duerfen sich in
     * GENAU zwei Bytes unterscheiden. Waere mehr verschieden, koennte
     * ein anderer Grund das Ergebnis erklaeren, und der Test wuerde das
     * Richtige aus dem Falschen schliessen. */
    size_t abweichungen = 0;
    for (size_t i = 0; i < GESAMT; i++)
        if (mit[i] != ohne[i]) abweichungen++;
    PRUEFE(abweichungen == 2,
           "die beiden Abbilder unterscheiden sich in %zu Byte, erlaubt "
           "sind genau 2 — sonst erklaert der Test nicht, was er misst",
           abweichungen);

    int k_mit = 0, k_ohne = 0;
    bool a_mit  = probe(mit,  &k_mit);
    bool a_ohne = probe(ohne, &k_ohne);

    printf("  mit  `BB`: angenommen=%s Konfidenz=%d\n",
           a_mit ? "ja" : "nein", k_mit);
    printf("  ohne `BB`: angenommen=%s Konfidenz=%d\n",
           a_ohne ? "ja" : "nein", k_ohne);

    PRUEFE(a_mit,
           "ein gueltiges DIM muss angenommen werden — sonst waere die "
           "Magic-Pruefung zu streng und wuerde echte Dateien abweisen");

    PRUEFE(!a_ohne,
           "eine Datei OHNE das Magic wird als DIM angenommen. Drei "
           "unabhaengige Umsetzungen lehnen sie ab (Hatari dim.c:75, HxC "
           "dim_loader.c:74/110, Jacknife dllmain.c:590). Ohne diese "
           "Pruefung wird jede Datei passender Laenge als DIM gelesen und "
           "ihre ersten 32 Byte als Geometrie ausgelegt");

    if (a_mit && !a_ohne)
        printf("  ok   das Magic entscheidet, nicht die Dateigroesse\n");

    free(mit); free(ohne);

    printf("\n%s (%d Abweichungen)\n",
           fehler ? "FEHLGESCHLAGEN" : "OK", fehler);
    return fehler ? 1 : 0;
}
