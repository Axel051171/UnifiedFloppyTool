/**
 * @file test_probe_confidence_on_zeros.c
 * @brief Eichung 1 — was eine Sonde auf lauter Nullen melden darf (MF-729)
 *
 * ── Die Regel ───────────────────────────────────────────────────────────
 *
 * Ein Puffer aus **lauter Nullen** traegt keine Signatur. Kein Magic,
 * kein Bootblock, kein Verzeichnis, keine Pruefsumme. Das Einzige, was
 * eine Sonde darin finden kann, ist die **Dateigroesse**, die ihr
 * getrennt mitgeteilt wird.
 *
 * Also gilt: **wer den Inhalt nicht liest, bleibt unter 50.** Was auf
 * einem Nullpuffer 50 oder mehr meldet, behauptet eine Erkenntnis, die
 * es nicht haben kann.
 *
 * Die Baender stehen im Kopf von `include/uft/uft_format_plugin.h`.
 *
 * ── Warum es diese Eichung gibt ─────────────────────────────────────────
 *
 * Gemessen (MF-728) meldeten die Sonden auf Nullen **35 bis 85** fuer
 * exakt dieselbe Erkenntnis:
 *
 *     DIM 85 · TRD 82 · D81 80 · NIB 80 · MSX 75 · D64 75 · D13 75
 *     D71 70 · ADF 70 · DO 55 · PO 55 · IMG 40 · V9T9 40 · JV1 35
 *
 * Da die Zahlen ohne gemeinsame Skala vergeben wurden, war der Vergleich
 * zwischen zwei Plugins willkuerlich: ein PC-160K-Abbild verlor gegen
 * `TRD` (82), ein Macintosh-800K gegen `D81` (80), ein PC-360K gegen
 * `MSX` (75) — nicht weil die mehr erkannt haetten, sondern weil ihre
 * Zahl groesser gewaehlt war.
 *
 * ── Warum ueber ALLE Plugins, nicht ueber eine Liste ────────────────────
 *
 * Die erste Fassung dieses Tests (MF-728) fuehrte 14 Plugins in einer
 * Tabelle. Das war ein Protokoll, kein Tor: ein neues Plugin mit 85 auf
 * Nullen waere nie aufgefallen. Seit MF-729 laeuft er ueber
 * `uft_registered_format_plugin_at()` — die Menge kommt aus der Quelle,
 * nicht aus einer gepflegten Liste (CLAUDE.md §Dateimengen; in diesem
 * Baum dreizehnmal belegt, dass Aufzaehlungen still veralten).
 *
 * ── Was diese Eichung NICHT prueft ──────────────────────────────────────
 *
 * Ob eine Sonde, die 50..79 beansprucht, den Inhalt **wirklich**
 * geprueft hat. `trd_probe()` hob seine Konfidenz auf 82, wenn
 * `data[0x8E4] <= 128` — wahr fuer die Haelfte aller Bytewerte, auf
 * Nullen aber zufaellig falschbar. Dagegen steht Eichung 2:
 * `tests/test_probe_confidence_on_random.c`.
 */

#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

uft_error_t uft_register_all_formats(void);

static int fehler = 0;

#define PRUEFE(bed, ...) do {                                            \
    if (!(bed)) { printf("  FAIL "); printf(__VA_ARGS__);                \
                  printf("\n"); fehler++; }                              \
} while (0)

/* Gaengige Diskettengroessen. Eine Sonde beansprucht nur ihre eigenen;
 * ueber diese Liste ist jede erreichbar, die ein reales Format bedient. */
static const size_t GROESSEN[] = {
     92160u,  102400u,  116480u,  143360u,  163840u,  174848u,  184320u,
    204800u,  232960u,  250880u,  256256u,  327680u,  349696u,  368640u,
    409600u,  655360u,  737280u,  802816u,  819200u,  901120u, 1146880u,
   1228800u, 1474560u, 1720320u, 2949120u,
};

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Eichung 1: was Sonden auf lauter Nullen melden (MF-729)\n\n");

    uft_error_t rc = uft_register_all_formats();
    size_t anzahl = uft_registered_format_plugin_count();
    printf("  Registry: rc=%d, %zu Plugins, %zu Groessen\n\n",
           rc, anzahl, sizeof(GROESSEN) / sizeof(GROESSEN[0]));
    PRUEFE(rc == UFT_OK && anzahl > 100,
           "die Registry ist nicht gefuellt (%zu) — dann misst dieser "
           "Test nichts (MF-447)", anzahl);

    const size_t n = 65536;              /* voller Sondenpuffer */
    uint8_t *nullen = calloc(1, n);
    if (!nullen) { printf("kein Speicher\n"); return 2; }

    int    hoechste = 0;
    const char *hoechster = "—";
    size_t hoechste_gr = 0;
    size_t verstoesse = 0, ansprueche = 0;

    for (size_t g = 0; g < sizeof(GROESSEN) / sizeof(GROESSEN[0]); g++) {
        for (size_t i = 0; i < anzahl; i++) {
            const uft_format_plugin_t *p = uft_registered_format_plugin_at(i);
            if (!p || !p->probe) continue;
            int k = -1;
            if (!p->probe(nullen, n, GROESSEN[g], &k)) continue;
            ansprueche++;
            if (k > hoechste) {
                hoechste = k; hoechster = p->name ? p->name : "?";
                hoechste_gr = GROESSEN[g];
            }
            if (k >= UFT_PROBE_CONF_STRUCT_MIN) {
                if (verstoesse < 12)
                    printf("  VERSTOSS  %-10s %9zu Byte  Konfidenz %3d\n",
                           p->name ? p->name : "?", GROESSEN[g], k);
                verstoesse++;
            }
        }
    }
    free(nullen);

    printf("\n  Ansprueche auf Nullpuffern : %zu\n", ansprueche);
    printf("  hoechste Konfidenz         : %d  (%s, %zu Byte)\n",
           hoechste, hoechster, hoechste_gr);
    printf("  Verstoesse (>= %d)          : %zu\n",
           UFT_PROBE_CONF_STRUCT_MIN, verstoesse);

    PRUEFE(verstoesse == 0,
           "%zu Sonde(n) melden auf einem Puffer OHNE JEDE INFORMATION "
           "eine Konfidenz von %d oder mehr. Das Band 50..79 heisst "
           "'Struktur gelesen' — auf Nullen ist da keine. Der Wert "
           "gehoert ins Band 30..49 (siehe Kopf von "
           "uft/uft_format_plugin.h)", verstoesse,
           UFT_PROBE_CONF_STRUCT_MIN);

    printf("\n  Was die gruene Ampel heisst: keine Sonde behauptet auf "
           "Nullen mehr,\n"
           "  als die Groesse hergibt.\n"
           "  Was sie NICHT heisst: dass die Sonden im Band 50..79 ihren "
           "Inhalt\n"
           "  wirklich pruefen — das misst Eichung 2 "
           "(test_probe_confidence_on_random.c).\n");

    printf("\n%s (%d Abweichungen)\n", fehler ? "ROT" : "GRUEN", fehler);
    return fehler ? 1 : 0;
}
