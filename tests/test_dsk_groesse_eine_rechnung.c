/*
 * @file test_dsk_groesse_eine_rechnung.c
 * @brief Rotbeweis: Sonde liest Feld A, `open` rechnet mit Feld B —
 *        und die beiden sind sich ueber dieselbe Datei uneinig.
 *
 * DER BEFUND, GEMESSEN
 * --------------------
 * `src/formats/dsk_generic/uft_dsk_generic.c` fuehrt 49 Geometrien.
 * Zwei davon widersprechen sich SELBST:
 *
 *     {77, 2, 16, 256, 634880}   // DSK_RC
 *     {77, 2, 16, 256, 634880}   // DSK_HP
 *
 * 77 x 2 x 16 x 256 = 630 784. Angegeben sind 634 880. Differenz:
 * 4 096 Byte — genau eine Spur (16 x 256).
 *
 * Und die zwei Pfade lesen VERSCHIEDENE Felder:
 *
 *     dsk_gen_probe_idx()  nimmt `expected_size`   -> 634 880
 *     dsk_gen_open_idx()   prueft die Groesse GAR NICHT und setzt
 *                          die Geometrie           -> 630 784
 *
 * Die Folge geht in beide Richtungen:
 *
 *   * eine echte 630 784-Byte-Datei wird von der Sonde ABGEWIESEN —
 *     das Format ist ueber die Erkennung unerreichbar (Klasse
 *     MF-1039, wo `cpm`s Sonde nie zustimmen konnte);
 *   * eine 634 880-Byte-Datei wird angenommen, und die letzten
 *     4 096 Byte liegen AUSSERHALB der Geometrie — unerreichbar, ohne
 *     dass es jemand erfaehrt (Klasse MF-1224, wo `fdi_pc98` 16 384
 *     Byte still verlor).
 *
 * WELCHE DER BEIDEN ZAHLEN STIMMT, IST NICHT ZU ENTSCHEIDEN
 * --------------------------------------------------------
 * Es gibt im Baum keine Quelle fuer RC702/Piccoline oder HP LIF, die
 * eine der beiden Zahlen belegt. Das ist Stoppbedingung S5: „unsicher,
 * welche Seite falsch ist -> notieren, nicht entscheiden."
 *
 * Also darf KEINE der beiden gewinnen. Der Eigentuemer, woertlich:
 * „Das ist weniger, als der Code heute tut, und mehr, als er heute
 * weiss. Raten war der Fehler; ihn zu beheben heisst nicht, richtig zu
 * raten."
 *
 * WAS DIESER TEST VERLANGT
 * ------------------------
 * Eine Groessenrechnung, die BEIDE Pfade rufen. Wo sie „unentschieden"
 * meldet, beansprucht die Sonde nichts — die zwei Formate sind dann
 * nur noch mit genanntem Format erreichbar (`uft_disk_open_as()`), und
 * `open` nimmt die KLEINERE Geometrie, damit nichts ERFUNDEN wird.
 *
 * Gegenprobe: `DSK_RLD` traegt `{77, 2, 8, 1024, 1261568}` — gerechnet
 * und angegeben stimmen ueberein. Es muss unveraendert aufgehen, sonst
 * waere die Behebung eine pauschale Verweigerung.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_core.h"   /* uft_disk_open(), uft_disk_open_as() */
#include "uft/uft_disk.h"   /* uft_disk_close() */

static int gruen = 0;
static int rot = 0;

#define ZUSAGE(bed, text)                                                   \
    do {                                                                    \
        if (bed) { gruen++; printf("   [ok ] %s\n", (text)); }              \
        else     { rot++;   printf("   [ROT] %s\n", (text)); }              \
    } while (0)

/* Die beiden widersprechenden Zeilen und die stimmige Gegenprobe. */
#define RC_ANGEGEBEN   634880u   /* expected_size der Tafel          */
#define RC_GERECHNET   630784u   /* 77 x 2 x 16 x 256                */
#define RLD_GROESSE   1261568u   /* 77 x 2 x 8 x 1024, beide gleich  */

static const uft_format_plugin_t *finde(const char *name)
{
    const size_t n = uft_registered_format_plugin_count();
    for (size_t i = 0; i < n; i++) {
        const uft_format_plugin_t *p = uft_registered_format_plugin_at(i);
        if (p && p->name && strcmp(p->name, name) == 0) return p;
    }
    return NULL;
}

/* Legt eine Datei der gewuenschten Groesse an, gefuellt mit 0xE5 —
 * dem Fuellbyte, das diese Plugins selbst schreiben. */
static int baue(const char *pfad, size_t groesse)
{
    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;
    static uint8_t block[4096];
    memset(block, 0xE5, sizeof(block));
    size_t rest = groesse;
    while (rest) {
        size_t n = rest < sizeof(block) ? rest : sizeof(block);
        if (fwrite(block, 1, n, f) != n) { fclose(f); return 0; }
        rest -= n;
    }
    fclose(f);
    return 1;
}

static int probiert(const uft_format_plugin_t *p, size_t dateigroesse)
{
    if (!p || !p->probe) return 0;
    static uint8_t puffer[4096];
    memset(puffer, 0xE5, sizeof(puffer));
    int conf = 0;
    return p->probe(puffer, sizeof(puffer), dateigroesse, &conf) ? 1 : 0;
}

static unsigned long geometrie_bytes(const uft_disk_t *d)
{
    return (unsigned long)d->geometry.cylinders * d->geometry.heads
         * d->geometry.sectors * d->geometry.sector_size;
}

int main(void)
{
    printf("== Eine Zahl, eine Stelle (DSK_RC / DSK_HP) ==\n");

    ZUSAGE(uft_register_all_formats() == UFT_OK,
           "SPERRE: uft_register_all_formats() gelingt");

    const uft_format_plugin_t *rc  = finde("DSK_RC");
    const uft_format_plugin_t *hp  = finde("DSK_HP");
    const uft_format_plugin_t *rld = finde("DSK_RLD");
    ZUSAGE(rc && hp && rld,
           "SPERRE: DSK_RC, DSK_HP und DSK_RLD sind registriert");
    if (!rc || !hp || !rld) { printf("\n%d/%d\n", gruen, gruen + rot);
                              return 1; }

    /* 1. Der Widerspruch, durch die oeffentliche API gemessen. */
    const char *pfad_gross = "dsk_rc_634880.dsk";
    const char *pfad_klein = "dsk_rc_630784.dsk";
    ZUSAGE(baue(pfad_gross, RC_ANGEGEBEN) && baue(pfad_klein, RC_GERECHNET),
           "SPERRE: beide Pruefdateien angelegt");

    uft_disk_t *d = uft_disk_open_as(pfad_gross, true, rc);
    unsigned long erklaert = d ? geometrie_bytes(d) : 0;
    if (d) {
        printf("   DSK_RC oeffnet %u Byte und erklaert davon %lu "
               "(Differenz %ld)\n",
               RC_ANGEGEBEN, erklaert, (long)RC_ANGEGEBEN - (long)erklaert);
        uft_disk_close(d);
    }
    ZUSAGE(erklaert == RC_GERECHNET,
           "MESSUNG: die Geometrie erklaert 630 784 der 634 880 Byte — "
           "4 096 liegen ausserhalb");

    /* 2. Was verlangt wird (faellt heute). */
    ZUSAGE(!probiert(rc, RC_ANGEGEBEN),
           "S5: DSK_RC beansprucht 634 880 NICHT mehr — welche der "
           "beiden Zahlen stimmt, ist nicht entschieden");
    ZUSAGE(!probiert(rc, RC_GERECHNET),
           "S5: und 630 784 ebenso wenig — es gewinnt KEINE der beiden");
    ZUSAGE(!probiert(hp, RC_ANGEGEBEN) && !probiert(hp, RC_GERECHNET),
           "S5: dasselbe fuer DSK_HP, dieselbe Tafelzeile");

    /* BERICHTIGT waehrend der Messung: hier stand `blind == NULL` —
     * „uft_disk_open() sagt ab". Das war vor der Aenderung gruen und
     * ist danach ROT geworden, und der Grund ist eine Einsicht, die
     * festgehalten gehoert:
     *
     *   Wenn Bewerber WEGFALLEN, kann aus einem gemeldeten
     *   Gleichstand ein stiller Alleingewinner werden.
     *
     * 634 880 Byte hatten DREI Beansprucher (`IMG`, `DSK_HP`,
     * `DSK_RC`) und wurden deshalb seit MF-1251 abgewiesen. Nachdem
     * die zwei unentschiedenen Zeilen ihren Anspruch aufgeben,
     * bleibt `IMG` allein — und gewinnt ohne Gleichstand.
     *
     * Das ist kein Fehler dieser Aenderung, aber eine Eigenschaft der
     * Gleichstandsregel, die man kennen muss. Gefragt ist hier, was
     * diese Aenderung verspricht: die BEIDEN sind nicht mehr blind
     * erreichbar. Ob `IMG`s groessenbasierter Anspruch auf 634 880
     * berechtigt ist, ist eine andere Frage (`P3-503`). */
    uft_disk_t *blind = uft_disk_open(pfad_gross, true);
    const char *wer = (blind && blind->plugin && blind->plugin->name)
                    ? blind->plugin->name : "(niemand)";
    printf("   uft_disk_open(634880) erreicht jetzt: %s\n", wer);
    ZUSAGE(strcmp(wer, "DSK_RC") != 0 && strcmp(wer, "DSK_HP") != 0,
           "S5: uft_disk_open() erreicht DSK_RC/DSK_HP nicht mehr blind");
    if (blind) uft_disk_close(blind);

    /* 3. Der Ausweg bleibt offen, und er ERFINDET nichts. */
    uft_disk_t *dz = uft_disk_open_as(pfad_gross, true, rc);
    ZUSAGE(dz != NULL,
           "AUSWEG: mit genanntem Format geht es weiterhin auf");
    if (dz) {
        ZUSAGE(geometrie_bytes(dz) == RC_GERECHNET,
               "AUSWEG: und zwar mit der KLEINEREN Geometrie — lieber "
               "4 096 Byte unerreichbar als 4 096 erfunden");
        uft_disk_close(dz);
    }

    remove(pfad_gross);
    remove(pfad_klein);

    /* 4. GEGENPROBE: die stimmige Zeile bleibt unberuehrt. */
    const char *pfad_rld = "dsk_rld_1261568.dsk";
    ZUSAGE(baue(pfad_rld, RLD_GROESSE), "SPERRE: RLD-Pruefdatei angelegt");
    ZUSAGE(probiert(rld, RLD_GROESSE),
           "GEGENPROBE: DSK_RLD beansprucht seine Groesse weiterhin — "
           "gerechnet und angegeben stimmen dort ueberein");
    ZUSAGE(!probiert(rld, RLD_GROESSE + 4096u),
           "GEGENPROBE: und eine um eine Spur groessere Datei nicht");
    uft_disk_t *dr = uft_disk_open_as(pfad_rld, true, rld);
    ZUSAGE(dr && geometrie_bytes(dr) == RLD_GROESSE,
           "GEGENPROBE: seine Geometrie erklaert die Datei RESTLOS");
    if (dr) uft_disk_close(dr);
    remove(pfad_rld);

    printf("\n%d/%d\n", gruen, gruen + rot);
    return rot ? 1 : 0;
}
