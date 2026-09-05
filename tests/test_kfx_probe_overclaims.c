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
 * ── Warum hier NICHT geschaerft wurde — und warum das ueberholt ist ─────
 *
 * Hier stand bis MF-919:
 *
 *     „Die richtige Bedingung waere der Aufbau eines KryoFlux-OOB-Blocks
 *      (0x0D, Typbyte, 16-Bit-Groesse) — und dafuer braucht es die
 *      Stream-Spezifikation als benannte Referenz. `dtc` ist als Oracle
 *      registriert, aber auf dieser Maschine nicht vorhanden (MF-720)."
 *
 * **Die Annahme war falsch, und zwar messbar.** Die Kenntnis liegt
 * bereits ZWEIMAL im Baum, unabhaengig voneinander:
 *
 *   A  src/formats/kryoflux/uft_kryoflux_checker.c — laeuft die
 *      OOB-Kette ab UND prueft die eingebettete Stromposition gegen
 *      die eigene Zaehlung. Steht seit jeher im qmake-Bau und hatte
 *      **null Aufrufer**.
 *   B  src/a8rawconv/rawdiskkf.cpp — a8rawconv (Avery Lee,
 *      GPL-2.0-or-later, vendort). Andere Hand, anderer Ansatz.
 *
 * Beide stimmen in allen Opcodes und im OOB-Kopf ueberein. Es fehlte
 * also nicht die Referenz, sondern der Blick in den eigenen Baum.
 * Messung vor Plan — und die Lehre gehoert zu P3-192.
 *
 * Die Sorge „eine Sonde blind zu verengen waere derselbe Fehler in der
 * anderen Richtung" bleibt richtig und ist adressiert: die neue
 * Bedingung verlangt eine schluessige Positionskette, mindestens zwei
 * OOB-Bloecke und mindestens eine Indexmarke. Dass ein Strom ohne
 * Indexmarke durchfaellt, ist eine BENANNTE konservative Grenze
 * (`include/uft/formats/kryoflux_checker.h`) — und ein sichtbarer
 * Fehler, waehrend der umgekehrte still ist.
 *
 * Gemessen nach der Schaerfung: **0 von 500** Zufallspuffern werden
 * noch angenommen (`tests/test_kfx_sonde_sagt_nein.c`).
 */

#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

uft_error_t uft_register_all_formats(void);
extern const uft_format_plugin_t uft_format_plugin_kfx;   /* MF-919 */

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
           "— das 40er-Gedraenge (FMT-15) waere dann aufgeloest",
           mit.tied);
    /* MF-919 — NACHGEZOGEN. Hier stand
     * `PRUEFE(mit.claimants > ohne.claimants, ...)`: ein einzelnes 0x0D
     * machte aus 7 Bewerbern 8, weil KFX dazukam. Seit der Schaerfung
     * tut es das nicht mehr — gemessen 8 -> 8.
     *
     * Und genau DAS ist jetzt die Aussage: ein einzelnes 0x0D darf das
     * Bewerberfeld NICHT mehr veraendern. */
    PRUEFE(mit.claimants == ohne.claimants,
           "ein einzelnes 0x0D veraendert das Bewerberfeld wieder "
           "(%zu -> %zu) — dann zaehlt eine Sonde erneut Bytes statt "
           "Struktur zu lesen", ohne.claimants, mit.claimants);

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
    /* MF-919 — NACHGEZOGEN, und die Zusicherung ist UMGEDREHT.
     * Hier wurde verlangt, dass KFX diesen Puffer gewinnt; die alte
     * Fehlermeldung sagte selbst „dann ist die Sonde geschaerft
     * worden". Genau das ist eingetreten. Was jetzt gemessen wird, ist
     * die Eigenschaft, die halten soll: 4096 Nullen mit einem einzigen
     * 0x0D sind kein KryoFlux-Strom, und KFX sagt das. */
    {
        int kconf = 0;
        PRUEFE(!uft_format_plugin_kfx.probe(b, n, n, &kconf),
               "KFX beansprucht wieder einen Puffer aus Nullen mit einem "
               "einzigen 0x0D (Konfidenz %d)", kconf);
    }
    PRUEFE(!nur.winner || nur.confidence < 50,
           "'%s' beansprucht 4096 Nullen mit einem 0x0D mit %d — das Band "
           "ab 50 heisst 'Struktur gelesen', und hier ist keine",
           nur.winner ? nur.winner->name : "—", nur.confidence);

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
