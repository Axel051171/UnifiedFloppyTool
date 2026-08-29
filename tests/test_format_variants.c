/**
 * @file test_format_variants.c
 * @brief Was gelesen wird, ist nicht was geschrieben wird (MF-665)
 *
 * Stufe 4b aus `docs/plans/VARIANTEN_UND_FAEHIGKEITEN.md`.
 *
 * ── Warum dieser Typ eine Richtung brauchte ──────────────────────────────
 *
 * `uft_format_variant_t` gibt es seit langem und hatte **null
 * Instanzen**. Er beschrieb, wie eine Variante AUSSIEHT — Größen,
 * Geometrie, ein `validate`-Rückruf — aber nicht, ob wir sie **lesen**
 * oder **schreiben** können.
 *
 * Für eine Auswahlliste beim Speichern ist genau das die Frage, und HFE
 * ist der Beleg, dass beides auseinanderfällt:
 *
 *     Fähigkeits-Manifest:  "HFE v3 (STM32 bootloader)" = SUPPORTED
 *     uft_hfe.c:797:        write_track() für v3 -> UFT_ERROR_NOT_SUPPORTED
 *
 * **Beide Aussagen stimmen.** Wir lesen v3, wir schreiben es nicht. Wer
 * die Auswahlliste aus dem Manifest gebaut hätte, böte „Speichern als
 * HFEv3" an und liefe in die Ablehnung — eine Zusage, die die Maschine
 * verweigert.
 *
 * ── Was hier bewiesen wird ───────────────────────────────────────────────
 *
 * 1. HFE führt zwei Varianten, und sie unterscheiden sich in der
 *    Richtung: v1 lesbar **und** schreibbar, v3 nur lesbar.
 * 2. Die Schreib-Liste enthält v1 und **nicht** v3.
 * 3. Die Voreinstellung ist v1 — und sie ist schreibbar.
 * 4. Eine nicht schreibbare Variante trägt eine **Begründung**. Ohne
 *    die könnte die Oberfläche nur „geht nicht" sagen, was niemandem
 *    hilft.
 * 5. Erkennung aus den Kopfbytes: `HXCPICFE` → v1, `HXCHFEV3` → v3,
 *    alles andere → **NULL**, nicht „vermutlich die erste".
 *
 * Punkt 2 ist der eigentliche Rotbeweis. Wäre die Liste aus dem
 * Manifest gebaut, stünde v3 darin.
 */

#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_format_probe.h"

#include <stdio.h>
#include <string.h>

static int fehler;

#define PRUEFE(bed, ...)                                                   \
    do { if (!(bed)) { printf("  FAIL "); printf(__VA_ARGS__);             \
                       printf("\n"); fehler++; } } while (0)

static const uft_format_variant_t *finde(const uft_format_plugin_t *p,
                                          const char *name)
{
    if (!p || !p->variants) return NULL;
    for (size_t i = 0; i < p->variant_count; i++)
        if (p->variants[i].name && strcmp(p->variants[i].name, name) == 0)
            return &p->variants[i];
    return NULL;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    if (uft_register_all_formats() != UFT_OK) {
        printf("FEHLER: uft_register_all_formats() schlug fehl\n");
        return 1;
    }
    printf("Varianten tragen eine Richtung (MF-665)\n\n");

    const uft_format_plugin_t *hfe = uft_get_format_plugin_by_name("HFE");
    if (!hfe) {
        printf("  FAIL HFE nicht in der Registry\n");
        return 1;
    }

    /* 1. Zwei Varianten, verschiedene Richtung. */
    PRUEFE(hfe->variants != NULL && hfe->variant_count == 2,
           "HFE muss zwei Varianten fuehren, hat %zu", hfe->variant_count);

    const uft_format_variant_t *v1 = finde(hfe, "HFEv1");
    const uft_format_variant_t *v3 = finde(hfe, "HFEv3");
    PRUEFE(v1 != NULL, "HFEv1 fehlt");
    PRUEFE(v3 != NULL, "HFEv3 fehlt");
    if (!v1 || !v3) { printf("\nFEHLGESCHLAGEN\n"); return 1; }

    PRUEFE(v1->can_read && v1->can_write, "HFEv1 muss lesbar UND schreibbar sein");
    PRUEFE(v3->can_read, "HFEv3 muss lesbar sein — der v3-Pfad ist verdrahtet");
    PRUEFE(!v3->can_write,
           "HFEv3 darf NICHT als schreibbar gelten: uft_hfe.c:797 lehnt es ab");

    /* 2. DER ROTBEWEIS: die Schreib-Liste kennt v3 nicht. */
    const uft_format_variant_t *liste[8];
    size_t n = uft_plugin_write_variants(hfe, liste, 8);
    PRUEFE(n == 1, "genau eine schreibbare HFE-Variante erwartet, %zu gezaehlt", n);
    for (size_t i = 0; i < n && i < 8; i++) {
        PRUEFE(strcmp(liste[i]->name, "HFEv3") != 0,
               "HFEv3 steht in der SCHREIB-Liste — genau das darf nicht "
               "passieren, die Maschine lehnt es ab");
        printf("  schreibbar: %s\n", liste[i]->name);
    }

    /* 3. Voreinstellung — und sie muss schreibbar sein. */
    const uft_format_variant_t *std = uft_plugin_default_write_variant(hfe);
    PRUEFE(std != NULL, "keine Voreinstellung");
    if (std) {
        PRUEFE(strcmp(std->name, "HFEv1") == 0,
               "Voreinstellung ist \"%s\", erwartet HFEv1", std->name);
        PRUEFE(std->can_write, "die Voreinstellung muss schreibbar sein");
    }

    /* 4. Eine nicht schreibbare Variante muss sich erklaeren. */
    PRUEFE(v3->write_note && v3->write_note[0],
           "HFEv3 ist nicht schreibbar und sagt nicht warum — die "
           "Oberflaeche haette nur \"geht nicht\" anzuzeigen");

    /* 5. Erkennung aus dem Kopf; Unbekanntes ergibt NULL. */
    {
        const uint8_t k1[8] = { 'H','X','C','P','I','C','F','E' };
        const uint8_t k3[8] = { 'H','X','C','H','F','E','V','3' };
        const uint8_t kx[8] = { 'N','I','C','H','T','H','F','E' };

        const uft_format_variant_t *e;
        e = uft_plugin_variant_of(hfe, k1, sizeof(k1));
        PRUEFE(e == v1, "HXCPICFE muss als HFEv1 erkannt werden");
        e = uft_plugin_variant_of(hfe, k3, sizeof(k3));
        PRUEFE(e == v3, "HXCHFEV3 muss als HFEv3 erkannt werden");
        e = uft_plugin_variant_of(hfe, kx, sizeof(kx));
        PRUEFE(e == NULL,
               "fremde Kopfbytes muessen NULL ergeben — nicht die erste "
               "Variante als Vermutung");
    }

    /* 6. Ein Plugin ohne Variantentabelle bietet nichts an, und das ist
     *    die ehrliche Voreinstellung: lieber keine Liste als eine
     *    geratene. */
    {
        const uft_format_plugin_t *d64 = uft_get_format_plugin_by_name("D64");
        if (d64) {
            PRUEFE(uft_plugin_write_variants(d64, liste, 8) == 0,
                   "D64 fuehrt keine Varianten — die Liste muss leer sein");
            PRUEFE(uft_plugin_default_write_variant(d64) == NULL,
                   "ohne Tabelle darf es keine Voreinstellung geben");
        }
        PRUEFE(uft_plugin_write_variants(NULL, liste, 8) == 0,
               "NULL-Plugin muss 0 liefern, nicht abstuerzen");
        PRUEFE(uft_plugin_variant_of(NULL, (const uint8_t *)"x", 1) == NULL,
               "NULL-Plugin muss NULL liefern");
    }

    printf("\n%s (%d Abweichungen)\n",
           fehler ? "FEHLGESCHLAGEN" : "OK", fehler);
    return fehler ? 1 : 0;
}
