/**
 * @file test_timeline_angle_shapes.c
 * @brief Haelt die Winkelschranke auch bei anderen Verzugsformen? (MF-562)
 *
 * ── Die offene Frage ─────────────────────────────────────────────────────
 *
 * MF-502 hat die Winkelgenauigkeit gemessen und daraus eine Schranke
 * gemacht: `uft_timeline_angle_error()` liefert (Spanne − 1) × **250**
 * Grad. Der Eintrag FLUX-15 sagt aber selbst, was daran ungeprueft blieb:
 *
 *   > "Immer noch nicht gemessen: ob die Schranke auch fuer Verzugsformen
 *   >  gilt, die nicht 'ein gedehnter Abschnitt' sind — Sinusdrift,
 *   >  mehrere Zonen, Drift ueber mehrere Umdrehungen. Der Faktor 250
 *   >  stammt aus einer Familie von Testspuren, nicht aus einer Theorie."
 *
 * Das ist keine Kleinigkeit. Der Wandler ENTSCHEIDET anhand dieser
 * Schranke: liegt sie unter einem halben Achtel (22,5 Grad), ordnet er
 * den Schaden winkelweise ein; darueber verweigert er die Verteilung.
 * Eine Schranke, die eine Verzugsform unterschaetzt, laesst ihn eine
 * Verteilung aus falschen Faechern melden.
 *
 * ── Warum das ohne Hardware geht ─────────────────────────────────────────
 *
 * Eine Synthetik-Spur hat konstruktionsbedingt bekannte Winkellagen: die
 * Intervalle werden hier erzeugt, also ist die kumulierte Zeit bis zu
 * jedem Bit eine SUMME, keine Schaetzung. Der wahre Winkel ist damit
 * exakt.
 *
 * Der gemeldete Winkel dagegen nimmt eine feste Zellendauer an:
 *
 *      Winkel = Bitindex · Zellendauer / Umdrehungsdauer · 360
 *
 * Die Differenz beider ist der Fehler — und den kann man ausrechnen,
 * ohne je ein Laufwerk anzufassen.
 *
 * ── Was hier gemessen wird ───────────────────────────────────────────────
 *
 * Vier Verzugsformen, jede mit demselben Verfahren:
 *
 *      gleichmaessig   Gegenprobe — der Fehler MUSS null sein
 *      ein Abschnitt   die Form, aus der der Faktor 250 stammt
 *      drei Zonen      schnell / langsam / schnell
 *      Sinusdrift      stetig, ohne Sprung
 *      lineare Drift   das Laufwerk wird ueber die Spur langsamer
 *
 * Fuer jede: Spanne aus `uft_dewarp_intervals()` (der echte Schaetzer),
 * Schranke aus `uft_timeline_angle_error()` (die echte Schranke), und der
 * wahre groesste Fehler aus der Summe. Rot wird der Test, wenn der wahre
 * Fehler die Schranke UEBERSTEIGT — dann deckt der Faktor 250 diese Form
 * nicht.
 *
 * ── Was der Test NICHT behauptet ─────────────────────────────────────────
 *
 * Er sagt nichts ueber echte Disketten. Er sagt: fuer diese vier
 * konstruierten Formen haelt die Schranke, oder sie haelt nicht — und im
 * zweiten Fall nennt er den Faktor, der gehalten haette. Mehr ist ohne
 * Aufnahmen mit bekannter Winkelmarkierung nicht zu haben, und weniger
 * waere eine Zahl ohne Bezugsgroesse.
 */

#include "uft/flux/uft_dewarp.h"
#include "uft/flux/uft_decode_timeline.h"

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define N_IV        4000        /* Intervalle je Spur */
#define BASE_NS     2000.0      /* nominelle Zellendauer (Amiga DD) */

static uint32_t g_iv[N_IV];
static uint32_t g_out[N_IV];

/* ── Die vier Formen ─────────────────────────────────────────────────────
 *
 * Jede liefert den Streckfaktor an der Stelle x ∈ [0,1). 1.0 = nominell.
 */

static double shape_steady(double x)   { (void)x; return 1.0; }

/** Ein gedehnter Abschnitt — die Form, aus der der Faktor 250 stammt. */
static double shape_one_zone(double x)
{
    return (x >= 0.35 && x < 0.65) ? 1.12 : 1.0;
}

/** Drei Zonen: schnell, langsam, schnell. */
static double shape_three_zones(double x)
{
    if (x < 0.33) return 0.94;
    if (x < 0.66) return 1.10;
    return 0.96;
}

/** Sinusdrift — stetig, ohne Sprung. Ein Riemen, der atmet. */
static double shape_sine(double x)
{
    return 1.0 + 0.06 * sin(2.0 * 3.14159265358979 * x);
}

/** Lineare Drift: das Laufwerk wird ueber die Spur langsamer. */
static double shape_linear(double x)
{
    return 1.0 + 0.10 * x;
}

typedef struct {
    const char *name;
    double (*f)(double);
} shape_t;

static const shape_t SHAPES[] = {
    { "gleichmaessig", shape_steady      },
    { "ein Abschnitt", shape_one_zone    },
    { "drei Zonen",    shape_three_zones },
    { "Sinusdrift",    shape_sine        },
    { "lineare Drift", shape_linear      },
};
#define N_SHAPES ((int)(sizeof(SHAPES) / sizeof(SHAPES[0])))

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Haelt die Winkelschranke auch bei anderen Formen? "
           "(MF-562) ===\n\n");
    printf("  %-14s %8s %10s %10s %10s\n",
           "Form", "Spanne", "Schranke", "wahr", "noetig");
    printf("  %-14s %8s %10s %10s %10s\n",
           "", "", "Grad", "Grad", "Faktor");
    printf("  %s\n",
           "------------------------------------------------------------");

    double worst_needed = 0.0;
    const char *worst_shape = "-";

    for (int s = 0; s < N_SHAPES; s++) {
        /* 1. FLUSSINTERVALLE bauen, nicht einzelne Zellen.
         *
         * Der erste Anlauf erzeugte je Intervall EINE Zellendauer. Der
         * Entzerrer wies alles ab, und zwar zu Recht: `inst_clock()`
         * (uft_dewarp.c:43) rechnet `k = d / t` und verlangt
         * k in [2 - 0.75, 4 + 0.75]. Ein MFM-Strom hat 4, 6 oder 8 us
         * zwischen zwei Flusswechseln, also k in {2,3,4}; k = 1 kommt
         * darin nicht vor.
         *
         * Ein Messaufbau, der dem Produktionspfad etwas vorlegt, das er
         * nie zu sehen bekommt, misst nichts — er misst seine eigene
         * Ablehnung.
         *
         * Die Folgen fuer die Rechnung: ein Intervall ueberspringt k
         * Zellen, der Bitindex waechst also um k, nicht um 1. Die WAHRE
         * Zeit bleibt die Summe der Intervalle. */
        double t_true[N_IV + 1];
        size_t bit_at[N_IV + 1];
        t_true[0] = 0.0;
        bit_at[0] = 0;
        for (int i = 0; i < N_IV; i++) {
            double x = (double)i / (double)N_IV;
            /* Deterministische Folge aus {2,3,4} — kein rand(), damit der
             * Lauf auf jeder Plattform derselbe ist. */
            int k = 2 + ((i * 7 + i / 3) % 3);
            double ns = (double)k * BASE_NS * SHAPES[s].f(x);
            g_iv[i] = (uint32_t)(ns + 0.5);
            t_true[i + 1] = t_true[i] + (double)g_iv[i];
            bit_at[i + 1] = bit_at[i] + (size_t)k;
        }
        const double rev_ns = t_true[N_IV];

        /* 2. Die Spanne kommt aus dem ECHTEN Schaetzer. */
        uft_dewarp_result_t r;
        memset(&r, 0, sizeof(r));
        if (!uft_dewarp_intervals(g_iv, N_IV, BASE_NS, 0.0, g_out, &r)) {
            printf("  %-14s  dewarp lehnte ab\n", SHAPES[s].name);
            failures++;
            continue;
        }

        /* 3. Die Schranke kommt aus der ECHTEN Funktion. */
        uft_decode_timeline_t tl;
        memset(&tl, 0, sizeof(tl));
        tl.cell_ns       = BASE_NS;
        tl.revolution_ns = rev_ns;
        tl.warp_span     = r.warp_span;
        const double bound = uft_timeline_angle_error(&tl);

        /* 4. Der wahre Fehler: gemeldeter Winkel gegen den aus der Summe.
         *
         * Der gemeldete Winkel nimmt eine feste Zellendauer an. Der wahre
         * steht in t_true[]. Beide auf [0,360) und die kuerzere Distanz
         * nehmen — ein Fehler von 359 Grad ist einer von 1. */
        double worst = 0.0;
        for (int i = 0; i <= N_IV; i++) {
            double a_reported =
                fmod((double)bit_at[i] * BASE_NS / rev_ns, 1.0) * 360.0;
            double a_true     = fmod(t_true[i] / rev_ns, 1.0) * 360.0;
            double d = fabs(a_reported - a_true);
            if (d > 180.0) d = 360.0 - d;
            if (d > worst) worst = d;
        }

        /* 5. Welcher Faktor haette gehalten? */
        double needed = (r.warp_span > 1.0)
                        ? worst / (r.warp_span - 1.0) : 0.0;
        if (needed > worst_needed) {
            worst_needed = needed;
            worst_shape = SHAPES[s].name;
        }

        printf("  %-14s %8.4f %10.1f %10.1f %10.0f\n",
               SHAPES[s].name, r.warp_span, bound, worst, needed);

        if (worst > bound + 0.05) {
            printf("        >>> DIE SCHRANKE HAELT NICHT: %.1f Grad wahr "
                   "gegen %.1f Grad Schranke.\n"
                   "        >>> Fuer diese Form braeuchte es Faktor %.0f "
                   "statt %.0f.\n",
                   worst, bound, needed, UFT_TIMELINE_ANGLE_ERR_PER_SPAN);
            failures++;
        }
    }

    printf("\n  Groesster noetiger Faktor: %.0f (%s), eingebaut ist %.0f\n",
           worst_needed, worst_shape, UFT_TIMELINE_ANGLE_ERR_PER_SPAN);

    /* Die Gegenprobe gehoert dazu: waere die Schranke beliebig gross,
     * haette der Test nichts gezeigt. */
    if (UFT_TIMELINE_ANGLE_ERR_PER_SPAN > 5.0 * worst_needed &&
        worst_needed > 0.0) {
        printf("  Hinweis: die Schranke ist mehr als fuenfmal so gross wie "
               "noetig.\n"
               "  Das ist keine Zusicherung, sondern Spielraum — wer sie "
               "enger zieht,\n"
               "  muss diese Messung wiederholen.\n");
    }

    printf("\n%s (%d Abweichungen)\n",
           failures ? "FEHLGESCHLAGEN" : "OK", failures);
    return failures ? 1 : 0;
}
