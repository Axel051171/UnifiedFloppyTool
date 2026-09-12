/**
 * @file test_groessenerkenner_stimmig.c
 * @brief Drei kopflose Groessenerkenner: die angesagte Geometrie muss die
 *        angenommene Dateigroesse RESTLOS erklaeren (MF-1041).
 *
 * ── Warum es diesen Test gibt ────────────────────────────────────────
 *
 * `syn`, `edk` und `xdm86` erkennen ihr Format **allein an der
 * Dateigroesse** — sie haben keinen Dateikopf und keine Kennung. Damit
 * gibt es genau eine Aussage, die sich ohne fremde Referenz pruefen
 * laesst, und sie ist streng: **was die Sonde annimmt, muss das `open`
 * vollstaendig auslegen koennen.** Zylinder x Koepfe x Sektoren x
 * Sektorgroesse muss die Dateigroesse ergeben, nicht weniger.
 *
 * ── Was gemessen wurde ───────────────────────────────────────────────
 *
 * Am unveraenderten Produktionspfad:
 *
 *     syn   634 880 Byte angenommen, Geometrie 77x2x16x256 = 630 784
 *           -> **4096 Byte unerreichbar**, ohne ein Wort
 *     syn   100 Byte angenommen, volle Diskette angesagt
 *     edk   100 Byte angenommen, volle Diskette angesagt
 *     xdm86 100 Byte abgewiesen (-25) — es prueft bereits
 *
 * Bei `syn` stand der Widerspruch **im eigenen Kopfkommentar**:
 * „77 cyl x 2 heads x 16 spt x 256 = 634880 bytes" — die linke Seite
 * ergibt 630 784. Die Sonde nahm die rechte Seite, `open` sagte die
 * linke an. Dieselbe Zahl fuehrt P3-260 bereits fuer zwei der 49
 * DSK-Varianten („634880 statt 630784", genau eine Spur).
 *
 * ── Was dieser Test NICHT behauptet ──────────────────────────────────
 *
 * **Er hebt keines der drei Formate.** Keines hat eine nachpruefbare
 * Referenz; die Geometrien sind gegen keine fremde Hand abgenommen. Alle
 * drei bleiben auf **T3**, gefuehrt als **P3-340**. Geprueft ist hier
 * allein die innere Stimmigkeit — und die ist pruefbar, ohne irgendetwas
 * zu glauben.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

extern const uft_format_plugin_t uft_format_plugin_syn;
extern const uft_format_plugin_t uft_format_plugin_edk;
extern const uft_format_plugin_t uft_format_plugin_xdm86;

static int gruen = 0;
static int rot = 0;

static void pruefe(const char *name, int bedingung, const char *hinweis)
{
    if (bedingung) {
        printf("  [OK]   %s\n", name);
        gruen++;
    } else {
        printf("  [ROT]  %s%s%s\n", name, hinweis ? "  -- " : "",
               hinweis ? hinweis : "");
        rot++;
    }
}

static const char *tmpdir(void)
{
    const char *t = getenv("TEMP");
    if (!t) t = getenv("TMPDIR");
    if (!t) t = ".";
    return t;
}

static int baue(const char *pfad, size_t n)
{
    FILE *f = fopen(pfad, "wb");
    uint8_t *b;
    size_t i;
    int ok = 0;
    if (!f) return 0;
    b = (uint8_t *)malloc(n ? n : 1);
    if (b) {
        for (i = 0; i < n; i++) b[i] = (uint8_t)(i & 0xFF);
        ok = (fwrite(b, 1, n, f) == n);
        free(b);
    }
    fclose(f);
    return ok;
}

/* Oeffnet eine Datei der Groesse `n` und liefert das Produkt der
 * angesagten Geometrie zurueck; 0 wenn `open` absagt. */
static unsigned long geoeffnet(const uft_format_plugin_t *p, size_t n,
                               const char *marke, uft_error_t *ee)
{
    char pfad[600];
    uft_disk_t d;
    uft_error_t e;
    unsigned long ges = 0;

    snprintf(pfad, sizeof(pfad), "%s/uft_mf1041_%s.bin", tmpdir(), marke);
    if (!baue(pfad, n)) return 0;
    memset(&d, 0, sizeof(d));
    e = p->open(&d, pfad, true);
    if (ee) *ee = e;
    if (e == UFT_OK) {
        ges = (unsigned long)d.geometry.cylinders * d.geometry.heads
            * d.geometry.sectors * d.geometry.sector_size;
        p->close(&d);
    }
    remove(pfad);
    return ges;
}

int main(void)
{
    char d1[260];

    printf("=== Groessenerkenner: Geometrie gegen Dateigroesse "
           "(MF-1041) ===\n");

    /* ── syn ─────────────────────────────────────────────────────── */
    {
        uft_error_t e = UFT_OK;
        unsigned long g = geoeffnet(&uft_format_plugin_syn, 630784u,
                                    "syn", &e);
        snprintf(d1, sizeof(d1), "open=%d, Geometrie ergibt %lu", (int)e, g);
        pruefe("syn: 630 784 Byte gehen auf, und die Geometrie erklaert "
               "sie RESTLOS (77 x 2 x 16 x 256)",
               e == UFT_OK && g == 630784u, d1);
    }
    {
        uft_error_t e = UFT_OK;
        int c = -1;
        bool ja = uft_format_plugin_syn.probe(NULL, 0, 634880u, &c);
        (void)geoeffnet(&uft_format_plugin_syn, 634880u, "syn2", &e);
        snprintf(d1, sizeof(d1), "probe=%d, open=%d", (int)ja, (int)e);
        pruefe("syn: 634 880 Byte werden ABGEWIESEN — die Zahl stand im "
               "eigenen Kopf, aber 77 x 2 x 16 x 256 sind 630 784 (eine "
               "Spur weniger; Klasse P3-260)", !ja && e != UFT_OK, d1);
    }
    {
        uft_error_t e = UFT_OK;
        (void)geoeffnet(&uft_format_plugin_syn, 100u, "syn3", &e);
        snprintf(d1, sizeof(d1), "open=%d", (int)e);
        pruefe("syn: eine 100-Byte-Datei wird abgewiesen — vorher wurde "
               "sie geoeffnet und als volle Diskette angesagt",
               e != UFT_OK, d1);
    }

    /* ── edk ─────────────────────────────────────────────────────── */
    {
        uft_error_t e1 = UFT_OK, e2 = UFT_OK;
        unsigned long a = geoeffnet(&uft_format_plugin_edk, 819200u,
                                    "edk1", &e1);
        unsigned long b = geoeffnet(&uft_format_plugin_edk, 1638400u,
                                    "edk2", &e2);
        snprintf(d1, sizeof(d1), "DD: open=%d, %lu; HD: open=%d, %lu",
                 (int)e1, a, (int)e2, b);
        pruefe("edk: beide Groessen gehen auf, und beide Geometrien "
               "erklaeren sie restlos (80 x 2 x 10 x 512 und "
               "80 x 2 x 20 x 512)",
               e1 == UFT_OK && a == 819200u
               && e2 == UFT_OK && b == 1638400u, d1);
    }
    {
        uft_error_t e = UFT_OK;
        (void)geoeffnet(&uft_format_plugin_edk, 100u, "edk3", &e);
        snprintf(d1, sizeof(d1), "open=%d", (int)e);
        pruefe("edk: eine 100-Byte-Datei wird abgewiesen — vorher las "
               "`open` die Groesse und benutzte sie nur fuer DD gegen HD",
               e != UFT_OK, d1);
    }

    /* ── xdm86: war schon stimmig, wird festgenagelt ─────────────── */
    {
        static const size_t gr[3] = { 92160u, 184320u, 368640u };
        int alle = 1, i;
        for (i = 0; i < 3; i++) {
            uft_error_t e = UFT_OK;
            char marke[16];
            unsigned long g;
            snprintf(marke, sizeof(marke), "xdm%d", i);
            g = geoeffnet(&uft_format_plugin_xdm86, gr[i], marke, &e);
            if (e != UFT_OK || g != (unsigned long)gr[i]) {
                printf("       %lu Byte: open=%d, Geometrie %lu\n",
                       (unsigned long)gr[i], (int)e, g);
                alle = 0;
            }
        }
        pruefe("xdm86: alle drei Groessen gehen auf, und jede Geometrie "
               "erklaert ihre Datei restlos", alle, NULL);
    }
    {
        uft_error_t e = UFT_OK;
        (void)geoeffnet(&uft_format_plugin_xdm86, 100u, "xdmk", &e);
        snprintf(d1, sizeof(d1), "open=%d", (int)e);
        pruefe("xdm86: eine 100-Byte-Datei wird abgewiesen — das tat es "
               "schon vorher, und es bleibt so", e != UFT_OK, d1);
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
