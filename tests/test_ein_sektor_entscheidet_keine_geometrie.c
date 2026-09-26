/* Ein Sektor entscheidet keine Geometrie, und ein Fehler bleibt auf
 * seiner Seite (H-18, MF-1339).
 *
 * NEGATIVE REFERENZ: CERTIFY 1.0 (Dan Moore/Dave Small, Antic 1988),
 * `certify.lzh!CERTIFY.C` Z. 173-194, gesichtet in
 * docs/research/ATARI_COPY_TOOLS_2026-09-23.md. Dort liest der
 * Fehlerzweig von Seite 1 erneut Seite 0, und EIN Leseversuch auf Sektor
 * 10 entscheidet, ob die Diskette 9 oder 10 Sektoren je Spur hat. Beides
 * ist hier der Fall, den der Baum NICHT wiederholen darf.
 *
 * Zwei Stellen im Baum stehen in dieser Klasse (gemessen MF-1339):
 *   1. uft_diag_surface_scan() — nimmt die Geometrie aus der Konfiguration
 *      und wiederholt auf derselben Seite. Richtig, aber jeder vorhandene
 *      Test lief mit `sides = 1`; die Seitenlokalisierung war ungeprueft.
 *   2. uft_st_order_messen() — setzte `spt = sector_count`. Fehlt auf der
 *      Spur ein Sektor MITTEN in der Folge, wurde aus 10 eine 9, und
 *      Interleave und Spiralfaktor wurden ueber Feldpositionen gerechnet,
 *      die keine physischen Plaetze mehr sind.
 *
 * Die Haelfte 1 (Oberflaechen-Scan) steht in
 * tests/test_diag_seitenfehler_und_sektor10.c: uft_disc_diagnostics.h
 * definierte ein eigenes uft_sector_status_t, das mit uft_types.h
 * kollidierte -- beide Header gingen nicht in eine Uebersetzungseinheit
 * (behoben MF-1341, tests/test_diag_kopf_neben_uft_types.c).
 *
 * Produktive Aufrufer beider Funktionen: 0 (nur Tests).
 */
#include "uft/formats/st/uft_st_order.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) {
        printf("  [ok ] %s\n", was);
        gruen++;
    } else {
        printf("  [ROT] %s%s%s\n", was, hinweis ? " -- " : "", hinweis ? hinweis : "");
        rot++;
    }
}

/* --- 3. ST-Reihenfolge: eine Luecke ist keine kleinere Geometrie -------- */

static void spur_bauen(uft_track_t *t, const uint8_t *folge, size_t n)
{
    memset(t, 0, sizeof *t);
    t->sectors = (uft_sector_t *)calloc(n, sizeof(uft_sector_t));
    t->sector_count = n;
    for (size_t i = 0; i < n; i++) t->sectors[i].id.sector = folge[i];
}

static void spur_frei(uft_track_t *t) { free(t->sectors); t->sectors = NULL; }

static void t_luecke_in_der_folge(void)
{
    puts("3. ST-Spur mit Luecke: Sektor 9 fehlt, 10 ist da");
    const uint8_t folge[9] = { 1,2,3,4,5,6,7,8,10 };
    uft_track_t t;
    spur_bauen(&t, folge, 9);
    uft_st_order_t o;
    uft_st_order_messen(NULL, &t, &o);
    char h[80];
    snprintf(h, sizeof h, "gemessen=%d spt=%u", o.gemessen, o.spt);
    /* Die Spur beweist mindestens 10 Plaetze, traegt aber nur 9 Eintraege:
     * ihre Feldpositionen sind keine physischen Plaetze mehr. */
    pruefe("keine Messung aus einer lueckenhaften Folge", !o.gemessen, h);
    pruefe("keine Sektorzahl 9 behauptet", o.spt != 9, h);
    spur_frei(&t);
}

static void t_doppelte_nummer(void)
{
    puts("4. ST-Spur mit doppelter Sektornummer");
    const uint8_t folge[10] = { 1,2,3,4,5,6,7,8,9,9 };
    uft_track_t t;
    spur_bauen(&t, folge, 10);
    uft_st_order_t o;
    uft_st_order_messen(NULL, &t, &o);
    char h[80];
    snprintf(h, sizeof h, "gemessen=%d spt=%u", o.gemessen, o.spt);
    pruefe("keine Messung, wenn eine Nummer zweimal vorkommt", !o.gemessen, h);
    spur_frei(&t);
}

static void t_vollstaendig_und_kaputt(void)
{
    puts("5. ST-Spur 1..10, Sektor 10 mit gefallener Pruefsumme");
    const uint8_t folge[10] = { 1,6,2,7,3,8,4,9,5,10 };
    uft_track_t t;
    spur_bauen(&t, folge, 10);
    t.sectors[9].id.crc_ok = false;   /* Kopf gelesen, Daten schlecht */
    uft_st_order_t o;
    uft_st_order_messen(NULL, &t, &o);
    char h[80];
    snprintf(h, sizeof h, "gemessen=%d spt=%u il=%u", o.gemessen, o.spt, o.interleave);
    pruefe("ein schlechter Sektor 10 zaehlt mit: spt 10", o.gemessen && o.spt == 10, h);
    spur_frei(&t);
}

static void t_grenze_benannt(void)
{
    puts("6. Grenze: fehlt der LETZTE Sektor ganz, sieht die Spur es nicht");
    const uint8_t folge[9] = { 1,2,3,4,5,6,7,8,9 };
    uft_track_t t;
    spur_bauen(&t, folge, 9);
    uft_st_order_t o;
    uft_st_order_messen(NULL, &t, &o);
    /* Nichts auf dieser Spur zeigt einen zehnten Platz. spt ist, was DIESE
     * Spur traegt -- nicht die Geometrie der Diskette. Festgenagelt, damit
     * niemand den Wert als Geometrie liest, ohne es zu merken. */
    pruefe("spt 9 heisst: diese Spur traegt 9, nicht: die Diskette hat 9",
           o.gemessen && o.spt == 9, NULL);
    spur_frei(&t);
}

int main(void)
{
    puts("=== Ein Sektor entscheidet keine Geometrie (H-18, negative Referenz CERTIFY 1.0) ===");
    t_luecke_in_der_folge();
    t_doppelte_nummer();
    t_vollstaendig_und_kaputt();
    t_grenze_benannt();
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
