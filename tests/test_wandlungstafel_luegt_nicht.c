/**
 * @file test_wandlungstafel_luegt_nicht.c
 * @brief „Verlustfrei" braucht einen Beleg, nicht eine Absicht (MF-1288).
 *
 * ── Der Anlass ───────────────────────────────────────────────────────
 *
 * `src/formats/uft_format_convert_tables.c` fuehrte
 *
 *     { .source = UFT_FORMAT_TD0, .target = UFT_FORMAT_IMD,
 *       .quality = UFT_CONV_LOSSLESS,          // „No data loss"
 *       .description = "TD0 to IMD (preserves metadata)" },
 *
 * ohne jede Messung dahinter — und MF-1287 hat inzwischen einen Verlust
 * GEMESSEN (P3-524: die Sektorgroessen einer Spur werden auf die des
 * ersten geebnet). Die Zeile war also nicht nur unbelegt, sondern falsch.
 *
 * ── Der groessere Befund ─────────────────────────────────────────────
 *
 * Es ist kein Einzelfall. Gemessen ueber die ganze Tafel: **13 Pfade
 * behaupten LOSSLESS, und acht davon haben ueberhaupt keinen Eintrag in
 * der Rundlauf-Matrix** — also keine Stelle, an der jemand haette
 * nachsehen koennen. Darunter `SCP -> G64`, Fluss nach Bitstrom, wo
 * `CLAUDE.md` selbst sagt: „Flux->Sektor verliert Timing+WeakBits".
 *
 * ── Und die Spalte hatte keinen Leser ────────────────────────────────
 *
 * `uft_conversion_path_t.quality` wird von genau zwei Stellen gelesen,
 * und beide sind unerreichbar: `uft_convert_can()` (Deklaration und
 * Definition, **null Aufrufer**) und `uft_convert_print_matrix()` (nicht
 * einmal in einem Header deklariert, **null Aufrufer**). Die
 * Guete-Angaben waren Dokumentation in C-Syntax — Bestand, nicht
 * Faehigkeit, wie der Kopierschutz-Katalog (P0-2) und die
 * Merkmalstafel (MF-1176).
 *
 * **Dieser Test ist deshalb der erste Leser der Spalte.** Eine Zusage,
 * die niemand prueft, ist keine; eine, die ein Test prueft, wird rot,
 * wenn jemand sie bricht.
 *
 * ── Was hier NICHT behauptet wird ────────────────────────────────────
 *
 * Die Gegenrichtung ist nicht geprueft: dass ein Pfad MIT Matrixeintrag
 * auch wirklich verlustfrei IST, sagt dieser Test nicht — das sagt die
 * Matrix, und sie sagt es je Eintrag mit eigenem Beleg. Hier geht es nur
 * darum, dass niemand „verlustfrei" behaupten darf, ohne dass es
 * irgendwo nachgesehen werden kann.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "uft/uft_format_convert.h"
#include "uft/core/uft_roundtrip.h"
#include "uft/uft_types.h"

/* Die Tafel selbst. Sie hat keinen Header-Zugang — `uft_convert_get_path()`
 * findet nur EIN Paar, und dieser Test muss ALLE sehen. */
extern const uft_conversion_path_t g_conversion_paths[];
extern const size_t g_num_conversion_paths;

/* Eigene Suche statt `uft_convert_get_path()`: die steht in
 * `uft_format_convert_dispatch.c` und zoege die halbe Formatschicht in
 * dieses Testziel. Eine lineare Suche ueber dieselbe Tafel, die Gruppe 2
 * ohnehin durchlaeuft, ist keine zweite REGEL — nur ein zweiter Weg zum
 * selben Feld. */
static const uft_conversion_path_t *finde(uft_format_t a, uft_format_t b)
{
    for (size_t i = 0; i < g_num_conversion_paths; i++)
        if (g_conversion_paths[i].source == a && g_conversion_paths[i].target == b)
            return &g_conversion_paths[i];
    return NULL;
}

static int fehler = 0;
#define PRUEFE(bed, text) do { \
    if (!(bed)) { printf("    [ROT] %s (Zeile %d)\n", (text), __LINE__); fehler++; } \
} while (0)

/* GRUNDLINIE — sie darf nur FALLEN.
 *
 * Bauform von Tor 57 (MF-883) und der Sondendoktrin (MF-1153): eine Zahl,
 * die den Rueckstand benennt, statt ihn zu verschweigen, und die den Rand
 * dichtmacht — ein NEUER Pfad kann gar nicht mehr unbelegt „verlustfrei"
 * behaupten.
 *
 * Gemessen MF-1288, nachdem `TD0 -> IMD` auf UNVERIFIED gesetzt wurde.
 * Wer sie senkt, belegt den Pfad mit einem Matrixeintrag ODER nimmt die
 * Behauptung zurueck. Wer sie erhoeht, hat etwas kaputtgemacht. */
#define GRUNDLINIE 7u

/* ═══ 1. Die Zeile, um die es ging ═════════════════════════════════ */
static void gruppe_1_td0_imd(void)
{
    printf("  [1] TD0 -> IMD behauptet nicht mehr verlustfrei\n");

    const uft_conversion_path_t *p =
        finde(UFT_FORMAT_TD0, UFT_FORMAT_IMD);
    PRUEFE(p != NULL, "der Pfad TD0 -> IMD fehlt in der Tafel");
    if (!p) return;

    PRUEFE(p->quality != UFT_CONV_LOSSLESS,
           "TD0 -> IMD behauptet weiterhin LOSSLESS — ohne Matrixeintrag "
           "und gegen den gemessenen Verlust aus P3-524");
    /* ── NACHGEZOGEN MF-1307 ─────────────────────────────────────────
     *
     * Hier stand `p->quality == UFT_CONV_UNVERIFIED` mit der Begruendung:
     *
     *     „Und die Matrix sagt weiterhin nichts ueber das Paar — genau
     *      deshalb ist UNVERIFIED die richtige Angabe und nicht LOSSY:
     *      gemessen ist EIN Verlust, nicht die ganze Bilanz."
     *
     * Die Bedingung dieser Begruendung ist WEGGEFALLEN, und der Test hat
     * genau darauf gewartet — seine zweite Zusage hiess woertlich „die
     * Matrix hat inzwischen einen Eintrag — dann gehoert die Guete-Angabe
     * nachgezogen". Seit MF-1307 fuehrt `uft_roundtrip.c` das Paar als
     * LOSSY_DOCUMENTED; die Bilanz kommt aus der Vorwaertspruefung gegen
     * libdsk 1.5 und hxcfe 2.x an drei gepackten TD0 (MF-1297).
     *
     * Die Zusage dreht sich damit um: sie haelt jetzt fest, dass Tafel und
     * Matrix ZUSAMMEN wandern. Eine Guete-Angabe, die hinter der Matrix
     * herhinkt, ist dieselbe Drift, gegen die dieser Test gebaut ist. */
    const uft_roundtrip_status_t st =
        uft_roundtrip_status(UFT_FORMAT_TD0, UFT_FORMAT_IMD);
    PRUEFE(st != UFT_RT_UNTESTED,
           "TD0 -> IMD hat seit MF-1307 einen Matrixeintrag");
    PRUEFE(p->quality == UFT_CONV_LOSSY,
           "und die Tafel muss mitgezogen sein: LOSSY, nicht UNVERIFIED");

    printf("      quality=%d, Matrixstatus=%d (beides erwartet)\n",
           (int)p->quality,
           (int)uft_roundtrip_status(UFT_FORMAT_TD0, UFT_FORMAT_IMD));
}

/* ═══ 2. Die Ratsche ═══════════════════════════════════════════════
 *
 * Kein Pfad darf LOSSLESS behaupten, ohne dass die Rundlauf-Matrix
 * etwas ueber ihn sagt. Was sie sagt, prueft dieser Test NICHT — nur,
 * dass es eine Stelle gibt, an der man nachsehen kann.               */
static void gruppe_2_ratsche(void)
{
    printf("  [2] verlustfrei ohne Matrixeintrag (Grundlinie %u)\n",
           GRUNDLINIE);

    size_t lossless = 0, unbelegt = 0;
    for (size_t i = 0; i < g_num_conversion_paths; i++) {
        const uft_conversion_path_t *p = &g_conversion_paths[i];
        if (p->quality != UFT_CONV_LOSSLESS) continue;
        lossless++;
        if (uft_roundtrip_status(p->source, p->target) == UFT_RT_UNTESTED) {
            unbelegt++;
            /* Ohne `uft_format_get_name()` — die steht in der
             * Formatschicht und wuerde dieses Testziel aufblaehen. Die
             * Beschreibung der Tafel benennt das Paar ohnehin. */
            printf("      unbelegt: Format %2d -> Format %2d   %s\n",
                   (int)p->source, (int)p->target,
                   p->description ? p->description : "");
        }
    }

    printf("      %zu von %zu Pfaden behaupten LOSSLESS, %zu davon unbelegt\n",
           lossless, g_num_conversion_paths, unbelegt);

    PRUEFE(lossless > 0,
           "kein einziger Pfad behauptet LOSSLESS — dann zaehlt dieser "
           "Test nichts und ist gruen aus dem falschen Grund");
    PRUEFE(unbelegt <= GRUNDLINIE,
           "mehr unbelegte LOSSLESS-Behauptungen als die Grundlinie erlaubt");
}

/* ═══ 3. Anti-Tautologie ═══════════════════════════════════════════
 *
 * Gruppe 2 zaehlt. Eine Zaehlung ist gruen, sobald sie null liefert —
 * auch wenn sie am falschen Feld zaehlt. Deshalb hier die Gegenprobe an
 * einem Paar, das BELEGT ist: `IMD -> IMG` hat seit MF-1277 einen
 * Matrixeintrag und darf folglich NICHT in der Liste auftauchen.     */
static void gruppe_3_gegenprobe(void)
{
    printf("  [3] Gegenprobe: ein belegtes Paar zaehlt nicht mit\n");

    uft_roundtrip_status_t s =
        uft_roundtrip_status(UFT_FORMAT_IMD, UFT_FORMAT_IMG);
    PRUEFE(s != UFT_RT_UNTESTED,
           "IMD -> IMG hat keinen Matrixeintrag mehr — dann prueft "
           "Gruppe 2 die falsche Frage");

    const uft_conversion_path_t *p =
        finde(UFT_FORMAT_IMD, UFT_FORMAT_IMG);
    PRUEFE(p != NULL, "der Pfad IMD -> IMG fehlt in der Tafel");

    printf("      IMD -> IMG: Matrixstatus=%d (nicht UNTESTED)\n", (int)s);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Wandlungstafel: verlustfrei braucht einen Beleg (MF-1288) ===\n");

    gruppe_1_td0_imd();
    gruppe_2_ratsche();
    gruppe_3_gegenprobe();

    if (fehler) { printf("\n%d Zusage(n) gefallen\n", fehler); return 1; }
    printf("\nalle Zusagen gehalten\n");
    return 0;
}
