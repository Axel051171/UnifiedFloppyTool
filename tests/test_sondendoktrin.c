/**
 * @file test_sondendoktrin.c
 * @brief Die Leiter der Sonden-Doktrin, Stufe fuer Stufe (MF-1153)
 *
 * ── Worum es geht ────────────────────────────────────────────────────
 *
 * Eigentuemer-Entscheidung vom 2026-09-15; verbindliche Fassung
 * `docs/SONDEN_DOKTRIN.md`. Von 23 offenen Punkten in
 * `docs/OPEN_ITEMS.md` waren ACHT dieselbe Frage — P3-392, P3-393,
 * P3-394, P3-401, P3-402, P3-403, P3-405, P3-406 —, und sie wurde je
 * Fall neu beantwortet.
 *
 * **Der Beleg dafuer stammt aus einem einzigen Tag:** MF-1151 hat `dmk`
 * von 100 auf 75 gesenkt, weil das Format keine Kennung hat, und
 * MF-1152 hat `ssd` bei 85 gelassen, obwohl Acorn DFS ebenso keine hat.
 *
 * Seit MF-1153 wird die Konfidenz ABGELEITET:
 *
 *     Kennung            +50      Struktur         +15
 *     Selbstkonsistenz   +25      Geometrie        +10
 *     Groesse allein       0      ohne Kennung: Klemme bei 45
 *
 * ── Was dieser Test ist ──────────────────────────────────────────────
 *
 * Die Arithmetik selbst, festgenagelt. Er prueft KEIN Format — er
 * prueft die Leiter, damit niemand sie stillschweigend verschiebt. Ein
 * geaenderter Gewichtswert faellt hier auf, nicht erst an einem Abbild.
 *
 * Die Belegkombinationen sind VOLLSTAENDIG aufgezaehlt (2^4 = 16), weil
 * eine Summe mit Klemme genau an den Raendern falsch wird — und der
 * Rand ist hier die Grenze zwischen „nur die Groesse" und „Struktur
 * gelesen".
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { printf("  [OK]   %s\n", was); gruen++; }
    else {
        printf("  [ROT]  %s%s%s\n", was, hinweis ? "  -- " : "",
               hinweis ? hinweis : "");
        rot++;
    }
}

#define K   UFT_BELEG_KENNUNG
#define S   UFT_BELEG_SELBSTKONSISTENZ
#define T   UFT_BELEG_STRUKTUR
#define G   UFT_BELEG_GEOMETRIE

int main(void)
{
    char d1[200];

    printf("\nDie Leiter der Sonden-Doktrin (MF-1153)\n\n");

    /* ── 1. Alle sechzehn Kombinationen, einzeln ──────────────────────
     *
     * Vollstaendig statt stichprobenweise: eine Summe mit Klemme wird
     * an den Raendern falsch, und es gibt nur sechzehn Raender. */
    {
        struct { unsigned b; int soll; const char *was; } f[] = {
            { 0,           0, "nichts — die Groesse allein ist NIE hinreichend" },
            { G,          10, "Geometrie allein" },
            { T,          15, "Struktur allein" },
            { S,          25, "Selbstkonsistenz allein" },
            { T|G,        25, "Struktur + Geometrie" },
            { S|G,        35, "Selbstkonsistenz + Geometrie" },
            { S|T,        40, "Selbstkonsistenz + Struktur" },
            { S|T|G,      45, "alle drei OHNE Kennung — die Klemme greift (50 -> 45)" },
            { K,          50, "Kennung allein" },
            { K|G,        60, "Kennung + Geometrie" },
            { K|T,        65, "Kennung + Struktur" },
            { K|S,        75, "Kennung + Selbstkonsistenz" },
            { K|T|G,      75, "Kennung + Struktur + Geometrie" },
            { K|S|G,      85, "Kennung + Selbstkonsistenz + Geometrie" },
            { K|S|T,      90, "Kennung + Selbstkonsistenz + Struktur" },
            { K|S|T|G,   100, "alle vier" },
        };
        const int n = (int)(sizeof f / sizeof f[0]);
        int falsch = 0;
        for (int i = 0; i < n; i++) {
            const int ist = uft_probe_konfidenz(f[i].b);
            if (ist != f[i].soll) {
                falsch++;
                snprintf(d1, sizeof d1, "%s: erwartet %d, gerechnet %d",
                         f[i].was, f[i].soll, ist);
                pruefe(f[i].was, 0, d1);
            }
        }
        snprintf(d1, sizeof d1, "%d von %d Kombinationen richtig",
                 n - falsch, n);
        pruefe("alle sechzehn Belegkombinationen ergeben ihren Wert",
               falsch == 0, d1);
    }

    /* ── 2. Die Regel, um die es geht: ohne Kennung nie ins Strukturband
     *
     * Das ist P3-405 in einer Zeile. Ohne diese Zusage waere die Klemme
     * eine Zahl unter vielen. */
    {
        int verstoesse = 0, hoechste = 0;
        for (unsigned b = 0; b < 16u; b++) {
            if (b & K) continue;                 /* mit Kennung ist erlaubt */
            {
                const int k = uft_probe_konfidenz(b);
                if (k > hoechste) hoechste = k;
                if (k >= UFT_PROBE_CONF_STRUCT_MIN) verstoesse++;
            }
        }
        snprintf(d1, sizeof d1, "hoechster Wert ohne Kennung: %d "
                 "(Strukturband beginnt bei %d), %d Verstoesse",
                 hoechste, UFT_PROBE_CONF_STRUCT_MIN, verstoesse);
        pruefe("KEINE Belegkombination ohne Kennung erreicht das Band "
               "\"Struktur gelesen\" — das ist P3-405, und es gilt fuer "
               "alle acht kennungsfreien Kombinationen, nicht nur fuer "
               "die volle", verstoesse == 0 && hoechste == 45, d1);
    }

    /* ── 3. Und das Merkmalsband verlangt Kennung PLUS zwei ─────────── */
    {
        int falsch = 0;
        for (unsigned b = 0; b < 16u; b++) {
            if (uft_probe_konfidenz(b) < UFT_PROBE_CONF_MAGIC_MIN) continue;
            {
                int zusatz = 0;
                if (b & S) zusatz++;
                if (b & T) zusatz++;
                if (b & G) zusatz++;
                if (!(b & K) || zusatz < 2) falsch++;
            }
        }
        pruefe("jede Kombination im Merkmalsband (80..100) traegt eine "
               "Kennung UND mindestens zwei weitere Belege — eine "
               "Kennung allein ergibt 50, weil sie sagt \"diese Bytes "
               "oeffnen ein Tor\", nicht \"diese Datei ist es\"",
               falsch == 0, NULL);
    }

    /* ── 4. Gegenprobe: die Leiter ist MONOTON ───────────────────────
     *
     * Ein zusaetzlicher Beleg darf die Konfidenz nie SENKEN. Ohne diese
     * Zusage koennte die Klemme an der falschen Stelle sitzen und ein
     * Beleg zum Nachteil werden. */
    {
        int verstoesse = 0;
        for (unsigned b = 0; b < 16u; b++) {
            for (unsigned bit = 0; bit < 4u; bit++) {
                const unsigned mehr = b | (1u << bit);
                if (mehr == b) continue;
                if (uft_probe_konfidenz(mehr) < uft_probe_konfidenz(b))
                    verstoesse++;
            }
        }
        snprintf(d1, sizeof d1, "%d Verstoesse ueber 16 x 4 Uebergaenge",
                 verstoesse);
        pruefe("ein zusaetzlicher Beleg senkt die Konfidenz NIE — die "
               "Klemme sitzt hinter der Summe, nicht in ihr",
               verstoesse == 0, d1);
    }

    /* ── 5. Und die Baender selbst stimmen mit MF-729 zusammen ─────── */
    {
        const int ok = (uft_probe_band(0)   == UFT_PROBE_BAND_NONE)
                    && (uft_probe_band(29)  == UFT_PROBE_BAND_NONE)
                    && (uft_probe_band(30)  == UFT_PROBE_BAND_SIZE)
                    && (uft_probe_band(45)  == UFT_PROBE_BAND_SIZE)
                    && (uft_probe_band(49)  == UFT_PROBE_BAND_SIZE)
                    && (uft_probe_band(50)  == UFT_PROBE_BAND_STRUCT)
                    && (uft_probe_band(79)  == UFT_PROBE_BAND_STRUCT)
                    && (uft_probe_band(80)  == UFT_PROBE_BAND_MAGIC)
                    && (uft_probe_band(100) == UFT_PROBE_BAND_MAGIC);
        pruefe("die vier Baender aus MF-729 sind unveraendert, und die "
               "Klemme 45 landet im Band \"nur die Groesse\" — die "
               "Doktrin verschiebt keine Grenze, sie macht die Baender "
               "erreichbar statt behauptet", ok, NULL);
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
