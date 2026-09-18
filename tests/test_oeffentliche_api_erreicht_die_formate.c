/**
 * @file test_oeffentliche_api_erreicht_die_formate.c
 * @brief zwoelf Formate hatten keinen Test ueber `uft_disk_open()` (MF-1144)
 *
 * ── Warum diese Achse zaehlt ──────────────────────────────────────────
 *
 * Ein Test, der `plugin->open()` am ZEIGER ruft, prueft den Leser — nicht
 * den Weg, den ein Benutzer nimmt. Der Unterschied ist in diesem Baum
 * teuer bezahlt:
 *
 *   MF-1039 (`cpm`)      die Sonde verglich mit der PUFFERgroesse und
 *                        konnte NIE zustimmen — `open()` las richtig,
 *                        das Plugin war ueber die Erkennung unerreichbar
 *   MF-1074 (`mgt`/`opus`) dieselbe Falle, siebter Fall
 *   MF-1059 (`hardsector`) alle fuenf Groessen der Tafel liegen ueber
 *                        `UFT_PROBE_BUFFER_SIZE`, die Bedingung traf nie
 *   MF-447               vor der Registrierung war die Registry LEER und
 *                        `uft_disk_open()` gab fuer JEDE Datei NULL
 *
 * In allen vier Faellen war der Leser richtig und der Weg von aussen
 * versperrt. Ein Zeiger-Test sieht das nicht.
 *
 * Der T1b-Audit (`scripts/audit_t1b.py`, Achse A4) hat **55** Formate
 * ohne einen solchen Test gezaehlt. Weil A4 den schlechtesten Rang
 * traegt, VERDECKT es dabei jeden anderen Befund dieser 55 — diese
 * Achse ist also auch die Voraussetzung dafuer, den Rest zu sehen.
 *
 * ── Was hier ZUGESICHERT wird, und was bewusst nicht ──────────────────
 *
 * Zugesichert wird zweierlei je Format:
 *
 *   1. `uft_disk_open()` liefert einen Griff — das Format ist von
 *      aussen ueberhaupt erreichbar;
 *   2. die gemeldete Geometrie erklaert die Dateigroesse RESTLOS.
 *
 * **Nicht** zugesichert wird, WELCHES Plugin gewinnt. Das ist keine
 * Bequemlichkeit, sondern gemessen: mehrere Groessen werden von mehr als
 * einem Plugin beansprucht, und drei Paare stehen genau in dieser
 * Tabelle —
 *
 *     256 256 Byte : hardsector und pdp   (je 77 x 1 x 26 x 128)
 *     819 200 Byte : mgt und sam          (je 80 x 2 x 10 x 512)
 *     204 800 Byte : ssd und tan          (je 80 x 1 x 10 x 256)
 *
 * — und in allen drei Faellen ist die Geometrie IDENTISCH, die Zusage
 * haelt also unabhaengig vom Sieger. Der Kopf von
 * `scripts/audit_geometrie_kollision.py` (Tor 58) sagt dazu, dass vier
 * Plugins 256 256 fuehren und dass es der IBM-3740-Standard fuer
 * 8-Zoll-SSSD ist, den vier Maschinen wirklich benutzt haben: die
 * Mehrdeutigkeit ist WIRKLICHKEIT, kein Defekt. Zwischen Plugins
 * entscheidet die Konfidenz-Rangfolge (MF-729).
 *
 * Welches Plugin gewonnen hat, wird deshalb **gemeldet und nicht
 * geprueft** — das macht die Kollisionslage sichtbar, ohne eine
 * Reihenfolge festzunageln, die niemand belegt hat (P3-393).
 *
 * ── Die Eingaben sind selbst gebaut, und das ist hier zulaessig ────────
 *
 * Alle zwoelf Formate sind KOPFLOS und gewinnen ihre Geometrie aus der
 * Dateigroesse; ein flaches Abbild der richtigen Groesse ist also ein
 * gueltiges Abbild. Jeder Sektor nennt seine eigene Lage, damit ein
 * Treffer nicht nur sagt, DASS etwas kam.
 *
 * **Was das nicht ist:** ein Fremdbeleg. Die Stufen T1/T1b messen die
 * HERKUNFT eines Abbilds; hier geht es allein um die Erreichbarkeit von
 * aussen. Keine Stufe bewegt sich dadurch (MF-1077).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "uft/uft_core.h"            /* uft_disk_open / uft_disk_close */
#include "uft/uft_format_plugin.h"   /* uft_register_all_formats */

extern const uft_format_plugin_t uft_format_plugin_hardsector;
extern const uft_format_plugin_t uft_format_plugin_mgt;
extern const uft_format_plugin_t uft_format_plugin_micropolis;
extern const uft_format_plugin_t uft_format_plugin_msx_disk;
extern const uft_format_plugin_t uft_format_plugin_nanowasp;
extern const uft_format_plugin_t uft_format_plugin_northstar;
extern const uft_format_plugin_t uft_format_plugin_pdp;
extern const uft_format_plugin_t uft_format_plugin_sam;
extern const uft_format_plugin_t uft_format_plugin_ssd;
extern const uft_format_plugin_t uft_format_plugin_t1k;
extern const uft_format_plugin_t uft_format_plugin_tan;
extern const uft_format_plugin_t uft_format_plugin_trd;

typedef struct {
    const char *name;
    const uft_format_plugin_t *plugin;
    int zyl, koepfe, spt, ss;
} api_fall_t;

/* Die Masse sind je aus dem PLUGIN gelesen, nicht aus der Dateigroesse
 * zurueckgerechnet — dieselbe Regel wie beim Durchschreibfall
 * (MF-1138). */
static const api_fall_t FAELLE[] = {
    { "hardsector", &uft_format_plugin_hardsector, 77, 1, 26, 128 },
    { "mgt",        &uft_format_plugin_mgt,        80, 2, 10, 512 },
    { "micropolis", &uft_format_plugin_micropolis, 77, 1, 16, 256 },
    { "msx_disk",   &uft_format_plugin_msx_disk,   80, 2,  9, 512 },
    { "nanowasp",   &uft_format_plugin_nanowasp,   40, 2, 10, 512 },
    { "northstar",  &uft_format_plugin_northstar,  35, 1, 10, 256 },
    { "pdp",        &uft_format_plugin_pdp,        77, 1, 26, 128 },
    { "sam",        &uft_format_plugin_sam,        80, 2, 10, 512 },
    { "ssd",        &uft_format_plugin_ssd,        80, 1, 10, 256 },
    { "t1k",        &uft_format_plugin_t1k,        40, 2,  9, 512 },
    { "tan",        &uft_format_plugin_tan,        80, 1, 10, 256 },
    { "trd",        &uft_format_plugin_trd,        80, 2, 16, 256 },
};
#define FAELLE_N ((int)(sizeof FAELLE / sizeof FAELLE[0]))

static int gruen = 0, rot = 0;
/* MF-1252: wie viele Formate NUR mit genanntem Format hineinkommen.
 * Die Zahl ist der eigentliche Befund dieses Tests — sie sagt, wie
 * viele Formate ueber Groesse UND Endung nicht bestimmbar sind. */
static int nur_mit_zwang = 0;
static void zusage(const char *was, int ok)
{
    if (ok) { printf("  [OK]   %s\n", was); gruen++; }
    else    { printf("  [ROT]  %s\n", was); rot++; }
}

/** Erste Endung aus der `;`-Liste des Plugins — nicht getippt, damit sie
 *  nicht driftet. */
static void endung_von(const uft_format_plugin_t *p, char *aus, size_t n)
{
    const char *e = p->extensions ? p->extensions : "img";
    size_t i = 0;
    while (e[i] && e[i] != ';' && i + 1 < n) { aus[i] = e[i]; i++; }
    aus[i] = 0;
    if (!aus[0]) snprintf(aus, n, "img");
}

static int baue(const char *pfad, const api_fall_t *f)
{
    FILE *g = fopen(pfad, "wb");
    if (!g) return 0;
    uint8_t *sek = malloc((size_t)f->ss);
    if (!sek) { fclose(g); return 0; }
    for (int c = 0; c < f->zyl; c++)
        for (int h = 0; h < f->koepfe; h++)
            for (int s = 0; s < f->spt; s++) {
                char marke[48];
                int n = snprintf(marke, sizeof marke,
                                 "UFT-API C%02d H%d S%02d ", c, h, s);
                memset(sek, 0, (size_t)f->ss);
                memcpy(sek, marke, (size_t)(n < f->ss ? n : f->ss));
                if (fwrite(sek, 1, (size_t)f->ss, g) != (size_t)f->ss) {
                    free(sek); fclose(g); return 0;
                }
            }
    free(sek);
    fclose(g);
    return 1;
}

int main(void)
{
    printf("\nDie oeffentliche API erreicht die Formate (MF-1144)\n\n");

    /* MF-447: ohne dies ist die Registry LEER und `uft_disk_open()`
     * gibt fuer jede Datei NULL. Die Zusage steht hier, weil der ganze
     * Test sonst aus dem falschen Grund rot waere. */
    zusage("uft_register_all_formats() meldet Erfolg",
           uft_register_all_formats() == UFT_OK);
    if (rot) { printf("\n%d gruen, %d rot\n", gruen, rot); return 1; }

    for (int i = 0; i < FAELLE_N; i++) {
        const api_fall_t *f = &FAELLE[i];
        const long groesse = (long)f->zyl * f->koepfe * f->spt * f->ss;

        char endung[16];
        endung_von(f->plugin, endung, sizeof endung);
        char pfad[160];
        snprintf(pfad, sizeof pfad, "t_api_%s.%s", f->name, endung);

        char txt[240];
        if (!baue(pfad, f)) {
            snprintf(txt, sizeof txt, "%s: Pruefabbild angelegt", f->name);
            zusage(txt, 0);
            continue;
        }

        /* MF-1251/MF-1252: der Vertrag hat sich GEAENDERT, und das ist
         * der Fund, nicht die Regression.
         *
         * Vorher hiess die Zusage „uft_disk_open() erreicht ein
         * Plugin" — und sie war gruen, weil IRGENDWER das Rennen
         * gewann, nicht weil der Richtige gewann. Gemessen wurden acht
         * dieser Formate allein durch Registrierungsreihenfolge
         * erreicht (`ssd`/`tan` bei 204 800, `hardsector`/`pdp` bei
         * 256 256, `mgt`/`sam` bei 819 200, `trd` bei 655 360,
         * `nanowasp` bei 409 600).
         *
         * Der ehrliche Vertrag lautet: erreichbar heisst „ohne Zwang,
         * ODER mit genanntem Format" — und welcher der beiden Faelle
         * eintritt, wird GEZAEHLT statt verschwiegen. */
        uft_probe_ranking_t rang;
        uft_disk_t *d = uft_disk_open_ranked(pfad, true, &rang);
        bool mit_zwang = false;
        if (!d && rang.tied > 1) {
            /* Mehrdeutig. Dann muss das GENANNTE Format hineinkommen —
             * sonst ist das Plugin wirklich unerreichbar. */
            d = uft_disk_open_as(pfad, true, f->plugin);
            mit_zwang = (d != NULL);
        }
        snprintf(txt, sizeof txt,
                 "%s: erreichbar%s (%ld Byte, .%s%s)",
                 f->name,
                 mit_zwang ? " NUR mit genanntem Format" : "",
                 groesse, endung,
                 (rang.tied > 1) ? ", mehrdeutig" : "");
        zusage(txt, d != NULL);
        if (mit_zwang) nur_mit_zwang++;

        if (d) {
            const long erklaert = (long)d->geometry.cylinders
                                * d->geometry.heads
                                * d->geometry.sectors
                                * d->geometry.sector_size;
            const char *sieger = (d->plugin && d->plugin->name)
                               ? d->plugin->name : "?";
            snprintf(txt, sizeof txt,
                     "%s: die Geometrie erklaert die Datei RESTLOS "
                     "(%d x %d x %d x %d = %ld; Sieger: %s)",
                     f->name, d->geometry.cylinders, d->geometry.heads,
                     d->geometry.sectors, d->geometry.sector_size,
                     erklaert, sieger);
            zusage(txt, erklaert == groesse);
            uft_disk_close(d);
        }
        remove(pfad);
    }

    /* Gegenprobe: eine Datei, die KEINE dieser Geometrien erklaert, darf
     * ueber die oeffentliche API nicht mit einer angesagten Groesse
     * aufgehen, die es nicht gibt (Klasse MF-1019/MF-1038). Ohne sie
     * koennte „oeffnet immer" gruen sein. */
    {
        FILE *g = fopen("t_api_winzig.img", "wb");
        if (g) {
            uint8_t null[100];
            memset(null, 0, sizeof null);
            fwrite(null, 1, sizeof null, g);
            fclose(g);
            uft_disk_t *d = uft_disk_open("t_api_winzig.img", true);
            if (d) {
                const long erklaert = (long)d->geometry.cylinders
                                    * d->geometry.heads
                                    * d->geometry.sectors
                                    * d->geometry.sector_size;
                char txt[220];
                snprintf(txt, sizeof txt,
                         "Gegenprobe: 100 Byte gehen auf als \"%s\" und "
                         "sagen %ld Byte an",
                         (d->plugin && d->plugin->name) ? d->plugin->name
                                                        : "?", erklaert);
                zusage(txt, erklaert == 100);
                uft_disk_close(d);
            } else {
                zusage("Gegenprobe: 100 Byte werden abgewiesen", 1);
            }
            remove("t_api_winzig.img");
        }
    }

    printf("\n  BEFUND: %d Formate kommen NUR mit genanntem Format "
           "hinein —\n"
           "          ueber Groesse UND Endung sind sie nicht "
           "bestimmbar.\n"
           "          Was den Gleichstand bricht, ist Inhalt.\n",
           nur_mit_zwang);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
