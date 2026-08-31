/**
 * @file test_kfx_probe_overclaims.c
 * @brief `kfx_probe()` beansprucht jede Datei mit einem `0x0D` (MF-727)
 *
 * ── Der Befund ──────────────────────────────────────────────────────────
 *
 * `src/formats/kfx/uft_kfx.c:57` zaehlt Vorkommen des Bytes `0x0D` in den
 * ersten 512 Byte und leitet daraus die Konfidenz ab:
 *
 *     if (oob_count >= 2) { *confidence = 80; return true; }
 *     if (oob_count >= 1) { *confidence = 40; return true; }
 *
 * `0x0D` ist der Wagenruecklauf. Ein einziges Vorkommen — irgendwo in
 * einem halben Kilobyte — genuegt, damit KFX die Datei mit Konfidenz 40
 * beansprucht.
 *
 * Gefunden wurde es nebenbei (MF-726): ein MOOF- und ein A2R-Kopf
 * tragen `FF 0A 0D 0A`, und weil beide Formate kein eigenes Plugin
 * haben, **gewann KFX**. `uft_disk_open()` uebergab eine MOOF-Datei dem
 * KryoFlux-Strom-Leser.
 *
 * ── Wie weit es reicht ──────────────────────────────────────────────────
 *
 * Statischer Zensus ueber alle Plugin-Quellen aus `git ls-files`
 * (MF-727): von **82** Quellen mit erkennbarer Konfidenz-Zuweisung
 * melden **13** hoechstens 40 — sie koennen KFX also nie ueberbieten:
 *
 *     25  t1k          35  jv1          40  dsk_generic
 *     30  edk          35  sam          40  jvc
 *     30  tan          35  syn          40  korg
 *     35  adf_arc      35  xdm86        40  pdp
 *                                       40  v9t9
 *
 * Fuenf davon — `t1k`, `edk`, `tan`, `syn`, `xdm86` — sind genau jene
 * Formate, fuer die FMT-18 draussen **kein Gegenstueck** gefunden hat.
 * Die beiden Befunde treffen dieselben Plugins.
 *
 * ── Was dieser Test dynamisch belegt ────────────────────────────────────
 *
 * `v9t9` ist der klarste Fall: seine Sonde prueft **nur die Groesse**
 * und meldet **genau 40** — denselben Wert wie KFX.
 *
 *     static bool v9t9_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
 *         (void)d; (void)s;
 *         if (fs == 92160 || fs == 184320 || fs == 368640) { *c = 40; ... }
 *     }
 *
 * **Gemessen ist es groesser, und das ist eine Berichtigung an mir
 * selbst.** Ich hielt es fuer einen Zweikampf. Eine Datei dieser Groesse
 * hat schon **ohne** `0x0D` sieben Bewerber, **fuenf davon gleichauf bei
 * 40**; der Sieger (XFD) steht bereits durch die
 * Registrierungsreihenfolge fest. Ein einzelnes `0x0D` macht acht
 * Bewerber und sechs Gleichplatzierte.
 *
 * KFX ist damit **nicht die Ursache, sondern der sechste im Gedraenge**.
 * Das 40er-Band ist ueberfuellt: mehrere kopflose Formate beanspruchen
 * dieselbe Groesse mit identischer Konfidenz — die FMT-15-Klasse, und
 * sie reicht weiter als FMT-20.
 *
 * ── Warum hier NICHT geschaerft wird ────────────────────────────────────
 *
 * Die richtige Bedingung waere der Aufbau eines KryoFlux-OOB-Blocks
 * (`0x0D`, Typbyte, 16-Bit-Groesse) — und dafuer braucht es die
 * Stream-Spezifikation als **benannte Referenz**. `dtc` ist als Oracle
 * registriert, aber auf dieser Maschine nicht vorhanden (MF-720).
 *
 * Eine Sonde blind zu verengen waere derselbe Fehler in der anderen
 * Richtung: statt zu viel zu beanspruchen, wuerde sie zu wenig — und
 * echte KryoFlux-Stroeme fielen durch, ohne dass es jemand merkt.
 * Stand: `docs/OPEN_ITEMS.md` FMT-20.
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

#define V9T9_SIZE  92160u      /* eine der drei Groessen aus v9t9_probe */

static void ranke(const uint8_t *b, size_t n, size_t fs,
                  uft_probe_ranking_t *r, const char *was)
{
    memset(r, 0, sizeof(*r));
    (void)uft_probe_buffer_ranked(b, n, fs, r);
    printf("  %-28s Sieger %-5s (%2d)  tied %zu  Bewerber %zu\n",
           was, r->winner ? r->winner->name : "—", r->confidence,
           r->tied, r->claimants);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("kfx_probe(): ein 0x0D genuegt (MF-727)\n\n");

    uft_error_t rc = uft_register_all_formats();
    printf("  Registry: rc=%d, %zu Plugins\n\n",
           rc, uft_registered_format_plugin_count());
    PRUEFE(uft_registered_format_plugin_count() > 100,
           "die Registry ist fast leer (%zu) — dann misst dieser Test "
           "nichts (MF-447)", uft_registered_format_plugin_count());

    size_t n = 4096;                      /* Sondenpuffer-Ausschnitt */
    uint8_t *b = calloc(1, n);
    if (!b) { printf("kein Speicher\n"); return 2; }

    /* ── 1 · Nur Nullen in v9t9-Groesse ──────────────────────────────── */
    uft_probe_ranking_t ohne;
    ranke(b, n, V9T9_SIZE, &ohne, "92160 Byte, keine 0x0D");
    PRUEFE(ohne.winner != NULL, "niemand beansprucht die v9t9-Groesse");

    /* Berichtigung an mir selbst (MF-727): ich hatte hier V9T9 als
     * Sieger erwartet und das Feld fuer ein Zweikampf gehalten. Gemessen
     * bewerben sich **sieben** Plugins, **fuenf davon gleichauf bei 40**.
     * Der Sieger (XFD) steht schon ohne KFX durch die
     * Registrierungsreihenfolge fest.
     *
     * Das 40er-Band ist also nicht knapp, sondern **ueberfuellt** — und
     * KFX ist nicht die Ursache, sondern der sechste im Gedraenge. Der
     * Befund waechst damit ueber FMT-20 hinaus: mehrere kopflose
     * Formate beanspruchen dieselbe Groesse mit identischer Konfidenz
     * (die FMT-15-Klasse). */
    PRUEFE(ohne.tied >= 2,
           "die v9t9-Groesse hat keinen Gleichstand mehr (tied=%zu) — "
           "dann sind die Sonden geschaerft worden und dieser Test ist "
           "nachzuziehen", ohne.tied);

    /* ── 2 · Dieselbe Datei, ein einziges 0x0D ───────────────────────── */
    b[300] = 0x0D;
    uft_probe_ranking_t mit;
    ranke(b, n, V9T9_SIZE, &mit, "dieselbe + ein 0x0D");

    PRUEFE(mit.tied >= 2,
           "ein einzelnes 0x0D erzeugt keinen Gleichstand mehr (tied=%zu) "
           "— dann ist kfx_probe() geschaerft worden. Gut; dieser Test "
           "ist nachzuziehen", mit.tied);
    PRUEFE(mit.claimants > ohne.claimants,
           "die Zahl der Bewerber steigt nicht (%zu -> %zu), obwohl ein "
           "0x0D dazukam", ohne.claimants, mit.claimants);

    printf("\n  Ein Byte macht aus %zu Bewerbern %zu und aus %zu "
           "Gleichplatzierten %zu.\n"
           "  Der Sieger stand aber schon vorher durch die "
           "Registrierungsreihenfolge\n"
           "  fest, nicht durch Evidenz — KFX ist hier nicht die "
           "Ursache, sondern\n"
           "  der %zu. im Gedraenge.\n",
           ohne.claimants, mit.claimants, ohne.tied, mit.tied, mit.tied);

    /* ── 3 · Und der Extremfall: nichts als ein 0x0D ─────────────────── */
    memset(b, 0, n);
    b[7] = 0x0D;
    uft_probe_ranking_t nur;
    ranke(b, n, n, &nur, "4096 Nullen + ein 0x0D");
    PRUEFE(nur.winner && strcmp(nur.winner->name, "KFX") == 0,
           "ein Puffer aus Nullen mit einem einzigen 0x0D wird nicht mehr "
           "von KFX beansprucht (Sieger '%s') — dann ist die Sonde "
           "geschaerft worden",
           nur.winner ? nur.winner->name : "—");

    free(b);

    printf("\n  Was die gruene Ampel heisst: der Befund steht "
           "unveraendert.\n"
           "  KFX beansprucht jede Datei mit einem Wagenruecklauf im "
           "ersten halben\n"
           "  Kilobyte, mit Konfidenz 40. 13 von 82 Plugin-Quellen "
           "melden hoechstens\n"
           "  40 und koennen das nie ueberbieten — und das 40er-Band ist "
           "schon ohne\n"
           "  KFX ueberfuellt (fuenf Gleichplatzierte auf 92160 Byte).\n"
           "\n"
           "  Was sie NICHT heisst: dass hier nichts zu tun waere. Die "
           "Schaerfung\n"
           "  braucht die KryoFlux-Stream-Spezifikation als benannte "
           "Referenz — blind\n"
           "  verengen waere derselbe Fehler in der anderen Richtung "
           "(FMT-20).\n");

    printf("\n%s (%d Abweichungen)\n", fehler ? "ROT" : "GRUEN", fehler);
    return fehler ? 1 : 0;
}
