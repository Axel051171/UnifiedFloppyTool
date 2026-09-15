/**
 * @file test_probe_confidence_on_text.c
 * @brief Eichung 3 — wer ein MERKMAL behauptet, darf es nicht auf Text
 *        finden (MF-1146)
 *
 * ── Die Luecke, die das schliesst ───────────────────────────────────────
 *
 * MF-729 hat den Konfidenzen Bedeutung gegeben und zwei Eichungen dazu
 * gebaut:
 *
 *   Eichung 1  `test_probe_confidence_on_zeros.c`   Nullpuffer
 *   Eichung 2  `test_probe_confidence_on_random.c`  Zufallspuffer
 *
 * Eichung 1 faengt Sonden, die auf NULLEN mehr melden als die Groesse
 * hergibt. Eichung 2 faengt weit gefasste Strukturpruefungen — und ihre
 * Regel gilt ihrem eigenen Kopf nach fuer das Band **50..79**.
 *
 * **Damit ist das Band 80..100 von keiner Eichung gedeckt**, und in
 * keinem der beiden Puffer steht TEXT. Echte Abbilder fangen aber sehr
 * oft mit Text an: OEM-Felder, Etiketten, Kommentarkoepfe,
 * selbstbenennende Pruefdateien.
 *
 * ── Der Anlass ist gemessen, nicht gedacht (MF-1144) ───────────────────
 *
 * `img_probe()` vergab seine Stufen durch ZUWEISUNG statt Sammlung:
 *
 *     *confidence = 40;                                      // Groesse
 *     if (data[0]==0xEB||data[0]==0xE9) *confidence = 60;    // fehlte
 *     if (data[510]==0x55&&data[511]==0xAA) *confidence = 80; // fehlte
 *     if (has_oem) *confidence = 85;                          // GRIFF
 *
 * `has_oem` prueft allein, ob die Bytes 3..10 druckbares ASCII sind.
 * Eine Datei ohne Sprungbefehl und ohne Bootsignatur bekam damit **85**
 * — Band „Merkmal getroffen" — fuer acht Byte Text.
 *
 * **Die Folge, ueber den echten `uft_disk_open()` gemessen:** von zwoelf
 * kopflosen Formaten gewann genau EINES sein eigenes Abbild; die
 * uebrigen gingen an IMG bzw. DSK_X820, und IMG meldete eine andere
 * Teilung mit derselben Summe (204 800 als 50x1x8x512 statt
 * 80x1x10x256). Ein Benutzer bekam fuer eine echte Acorn-DFS-Diskette
 * die falsche Spurzahl, die falschen Sektoren je Spur und die falsche
 * Sektorgroesse — bei richtiger Gesamtsumme.
 *
 * Auf Nullen faellt `has_oem` durch (Byte 3..10 sind 0x00), auf Zufall
 * ist „alle acht druckbar" rund 0,02 % wahrscheinlich, und 85 liegt
 * ausserhalb des Bandes, das Eichung 2 regelt. **Beide vorhandenen
 * Eichungen waren schmaler als ihr Gegenstand** — Klasse MF-1000.
 *
 * ── Die Regel ───────────────────────────────────────────────────────────
 *
 * **Wer auf reinem Text das Band `UFT_PROBE_CONF_MAGIC_MIN` (80)
 * beansprucht, hat kein Merkmal getroffen, sondern Text gelesen.**
 *
 * Zwei Puffer, beide bestimmt (kein Zufall, derselbe Lauf ergibt
 * dasselbe):
 *
 *   A  zyklisch druckbares ASCII (`0x20 + i % 95`)
 *   B  eine selbstbenennende Pruefdatei, wie dieser Baum sie baut
 *      (`UFT-API Cnn Hn Snn `, Rest genullt) — genau die Form, an der
 *      MF-1144 den Befund gemessen hat
 *
 * ── Warum eine Grundlinie und keine harte Null ──────────────────────────
 *
 * Eine Kennung KANN druckbar sein: `"SCP"`, `"IMD "`, `"CAPS"`,
 * `"MAMEFLOPPYIMAGE"`. Ein Treffer ist deshalb ein **Kandidat**, kein
 * Befund — dieselbe Lage wie bei A6 im T1b-Audit, das nach 13 von 14
 * widerlegten Handproben zur Liste herabgestuft wurde (MF-1133).
 *
 * Gefuehrt wird darum wie bei Tor 57: `docs/sonden_textband_baseline.txt`
 * traegt die geprueften Faelle, und **neue** Ueberziehungen faerben rot.
 * Wer eine Zeile hinzufuegt, hat die Sonde gelesen und den Grund
 * notiert; wer eine wegnimmt, hat sie behoben.
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

#define PUFFER 65536u

/* Dieselben Groessen wie Eichung 2, plus die, an denen MF-1144
 * Verdraengung gemessen hat (256 256 und 315 392). */
static const size_t GROESSEN[] = {
     89600u,   92160u,  143360u,  163840u,  174848u,  184320u,  204800u,
    232960u,  256256u,  315392u,  327680u,  368640u,  409600u,  512512u,
    655360u,  737280u,  819200u,  901120u, 1228800u, 1474560u,
};
#define N_GROESSEN (sizeof(GROESSEN) / sizeof(GROESSEN[0]))

/** Puffer A: zyklisch druckbares ASCII. */
static void fuelle_ascii(uint8_t *b)
{
    for (size_t i = 0; i < PUFFER; i++)
        b[i] = (uint8_t)(0x20 + (i % 95));
}

/** Puffer B: selbstbenennende Pruefdatei, 256-Byte-Raster. */
static void fuelle_marken(uint8_t *b)
{
    memset(b, 0, PUFFER);
    for (size_t off = 0; off + 256 <= PUFFER; off += 256) {
        char m[40];
        int n = snprintf(m, sizeof m, "UFT-API C%02d H%d S%02d ",
                         (int)(off / 2560), (int)((off / 256) % 2),
                         (int)((off / 256) % 10));
        memcpy(b + off, m, (size_t)n);
    }
}

/** Grundlinie lesen: eine Sonde je Zeile, `#` ist Kommentar. */
static size_t lies_grundlinie(char namen[][32], size_t max)
{
    const char *pfade[] = {
        "docs/sonden_textband_baseline.txt",
        "../docs/sonden_textband_baseline.txt",
#ifdef UFT_REPO_DIR
        UFT_REPO_DIR "/docs/sonden_textband_baseline.txt",
#endif
    };
    FILE *f = NULL;
    for (size_t i = 0; i < sizeof pfade / sizeof pfade[0] && !f; i++)
        f = fopen(pfade[i], "r");
    if (!f) return 0;

    size_t n = 0;
    char zeile[128];
    while (n < max && fgets(zeile, sizeof zeile, f)) {
        char *p = zeile;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || !*p) continue;
        size_t k = 0;
        while (p[k] && p[k] != '\n' && p[k] != '\r' && k < 31) {
            namen[n][k] = p[k]; k++;
        }
        namen[n][k] = 0;
        if (namen[n][0]) n++;
    }
    fclose(f);
    return n;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Eichung 3: wer ein MERKMAL behauptet, darf es nicht auf Text "
           "finden (MF-1146)\n\n");

    uft_error_t rc = uft_register_all_formats();
    size_t anzahl = uft_registered_format_plugin_count();
    printf("  Registry: rc=%d, %zu Plugins · %zu Groessen · 2 Textpuffer\n",
           rc, anzahl, N_GROESSEN);
    printf("  Schwelle: Band >= %d (\"Merkmal getroffen\")\n\n",
           UFT_PROBE_CONF_MAGIC_MIN);
    PRUEFE(rc == UFT_OK && anzahl > 100,
           "die Registry ist nicht gefuellt (%zu) — dann misst dieser "
           "Test nichts (MF-447)", anzahl);
    if (anzahl == 0) return 2;

    static char grundlinie[64][32];
    const size_t n_grund = lies_grundlinie(grundlinie, 64);
    printf("  Grundlinie: %zu bekannte Faelle\n\n", n_grund);

    uint8_t *buf = malloc(PUFFER);
    int *hoechste = calloc(anzahl, sizeof(int));
    size_t *bei = calloc(anzahl, sizeof(size_t));
    if (!buf || !hoechste || !bei) { printf("kein Speicher\n"); return 2; }

    for (int variante = 0; variante < 2; variante++) {
        if (variante == 0) fuelle_ascii(buf); else fuelle_marken(buf);
        for (size_t g = 0; g < N_GROESSEN; g++) {
            for (size_t i = 0; i < anzahl; i++) {
                const uft_format_plugin_t *p =
                    uft_registered_format_plugin_at(i);
                if (!p || !p->probe) continue;
                int k = -1;
                if (!p->probe(buf, PUFFER, GROESSEN[g], &k)) continue;
                if (k >= UFT_PROBE_CONF_MAGIC_MIN && k > hoechste[i]) {
                    hoechste[i] = k;
                    bei[i] = GROESSEN[g];
                }
            }
        }
    }

    size_t ueberzogen = 0, neu = 0;
    for (size_t i = 0; i < anzahl; i++) {
        if (hoechste[i] < UFT_PROBE_CONF_MAGIC_MIN) continue;
        const uft_format_plugin_t *p = uft_registered_format_plugin_at(i);
        const char *name = (p && p->name) ? p->name : "?";
        ueberzogen++;

        bool bekannt = false;
        for (size_t j = 0; j < n_grund; j++)
            if (strcmp(grundlinie[j], name) == 0) { bekannt = true; break; }

        printf("  %-9s %-14s Konfidenz %3d bei %zu Byte\n",
               bekannt ? "bekannt" : "NEU", name, hoechste[i], bei[i]);
        if (!bekannt) neu++;
    }

    printf("\n  Sonden im Merkmalsband auf Text : %zu\n", ueberzogen);
    printf("  davon NEU (nicht in der Grundlinie) : %zu\n", neu);

    PRUEFE(neu == 0,
           "%zu Sonde(n) beanspruchen auf reinem TEXT das Band "
           ">= %d, ohne in der Grundlinie zu stehen. Entweder die Sonde "
           "lesen und die Stufe begruenden, oder die Zeile mit Grund in "
           "docs/sonden_textband_baseline.txt aufnehmen — eine Kennung "
           "KANN druckbar sein (\"SCP\", \"IMD \", \"CAPS\"), ein Treffer "
           "ist also ein Kandidat und kein Befund (MF-1133)",
           neu, UFT_PROBE_CONF_MAGIC_MIN);

    free(bei); free(hoechste); free(buf);

    printf("\n%s\n", fehler ? "FEHLER" : "OK");
    return fehler ? 1 : 0;
}
