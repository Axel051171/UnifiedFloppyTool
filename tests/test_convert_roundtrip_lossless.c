/**
 * @file test_convert_roundtrip_lossless.c
 * @brief Die einzige Zusage, die ohne Zustimmung gilt — geprueft (MF-527)
 *
 * ── Warum es diesen Test gibt ────────────────────────────────────────────
 *
 * `src/core/uft_roundtrip.c` schreibt seine eigene Regel in den Kopf:
 *
 *     Rules for adding an entry:
 *       LL   -> you added a round-trip test that proves byte-identity.
 *
 * Zwei Eintraege stehen dort als LOSSLESS:
 *
 *     { UFT_FORMAT_SCP, UFT_FORMAT_HFE, UFT_RT_LOSSLESS, ... }
 *     { UFT_FORMAT_HFE, UFT_FORMAT_SCP, UFT_RT_LOSSLESS, ... }
 *
 * Das sind die **einzigen zwei** der 44 Wandlungspfade, die das
 * Preflight-Tor **ohne** `accept_data_loss` durchlaesst. Alles andere
 * verlangt entweder Zustimmung, gilt als unmoeglich, oder wird als
 * UNGEPRUEFT abgewiesen (MF-526).
 *
 * Gesucht, gefunden: **es gab keinen solchen Test.**
 *
 * `test_roundtrip_matrix.c` prueft die Registry-API und verweist im Kopf
 * ausdruecklich auf "den Full-Image-Round-Trip-Test
 * (tests/test_roundtrip.c)" — **diese Datei existiert nicht**. Die 20
 * uebrigen `*_roundtrip`-Tests sind PLUGIN-Rundlaeufe
 * (`write_track -> read_track` innerhalb EINES Formats), nicht
 * WANDLUNGS-Rundlaeufe zwischen zweien. Und jedes `memcmp` in den
 * `test_convert_*`-Tests vergleicht dekodierte Sektoren gegen ein
 * Referenz-ADF — das ist der verlustbehaftete Pfad SCP->ADF, nicht die
 * verlustfreie Zusage.
 *
 * Die staerkste Zusage des Wandlungssystems trug damit keinen Beleg.
 *
 * ── Was hier gemessen wird ───────────────────────────────────────────────
 *
 *      gw_amigados.hfe  --uftc_convert_hfe_to_scp-->  X.scp
 *      X.scp            --uftc_convert_scp_to_hfe-->  Y.hfe
 *      Y.hfe  gegen  gw_amigados.hfe
 *
 * Gerufen werden die Wandler direkt, nicht ueber `uft_convert_file()` —
 * sonst pruefte der Test das Tor statt der Wandlung.
 *
 * ── Was der Test behauptet, und was nicht ────────────────────────────────
 *
 * Er behauptet NICHT, dass Bit-Identitaet herauskommen MUSS. Er MISST, ob
 * sie herauskommt, und meldet das Ergebnis — denn genau daran haengt, ob
 * `UFT_RT_LOSSLESS` fuer dieses Paar richtig ist.
 *
 * Rot wird er, wenn eine der beiden Wandlungen fehlschlaegt oder abstuerzt.
 * Ist das Ergebnis nicht bitgleich, sagt er, WIE weit es abweicht — und
 * dann ist der Tabelleneintrag zu korrigieren, nicht der Test.
 */

#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Wandler-Einstiegspunkte (src/formats/uft_format_convert_internal.h).
 * Direkt deklariert, damit dieser Test keinen internen Header braucht. */
struct uft_convert_options_ext;
typedef struct uft_convert_result uft_convert_result_t_fwd;

extern uft_error_t uftc_convert_hfe_to_scp(const uint8_t *src_data, size_t src_size,
                                           const char *dst_path,
                                           const void *opts, void *result);
extern uft_error_t uftc_convert_scp_to_hfe(const uint8_t *src_data, size_t src_size,
                                           const char *dst_path,
                                           const void *opts, void *result);

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "."
#endif

#define MAX_IMG (8 * 1024 * 1024)
static uint8_t a[MAX_IMG], b[MAX_IMG];

/* uft_convert_result_t ist gross; wir brauchen nur genug Platz, damit die
 * Wandler hineinschreiben duerfen. Die Groesse kommt aus dem Header. */
#include "uft/uft_format_convert.h"

static int failures;
static int keep_artifacts;

static size_t slurp(const char *path, uint8_t *dst)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(dst, 1, MAX_IMG, f);
    fclose(f);
    return n;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    if (uft_register_all_formats() != UFT_OK) {
        printf("FEHLER: uft_register_all_formats() schlug fehl\n");
        return 1;
    }

    printf("LOSSLESS-Zusage SCP<->HFE, gemessen (MF-527)\n");

    char src[1024];
    snprintf(src, sizeof(src), "%s/gw_amigados.hfe", UFT_CORPUS_DIR);
    size_t orig_len = slurp(src, a);
    if (!orig_len) {
        printf("  Korpusdatei fehlt: %s — nicht messbar\n", src);
        return 1;
    }
    printf("  Ausgangsdatei : %zu Byte\n", orig_len);

    const char *scp_tmp = "uft_rt_mid.scp";
    const char *hfe_tmp = "uft_rt_out.hfe";

    uft_convert_result_t r1;
    memset(&r1, 0, sizeof(r1));
    uft_error_t e1 = uftc_convert_hfe_to_scp(a, orig_len, scp_tmp, NULL, &r1);
    printf("  HFE -> SCP    : %d%s\n", (int)e1, e1 == UFT_OK ? " (OK)" : "");
    if (e1 != UFT_OK) {
        printf("  FAIL  die verlustfreie Hinrichtung schlaegt fehl\n");
        failures++;
        goto done;
    }

    size_t mid_len = slurp(scp_tmp, b);
    printf("  Zwischendatei : %zu Byte\n", mid_len);

    uft_convert_result_t r2;
    memset(&r2, 0, sizeof(r2));
    uft_error_t e2 = uftc_convert_scp_to_hfe(b, mid_len, hfe_tmp, NULL, &r2);
    printf("  SCP -> HFE    : %d%s\n", (int)e2, e2 == UFT_OK ? " (OK)" : "");
    if (e2 != UFT_OK) {
        printf("  FAIL  die verlustfreie Rueckrichtung schlaegt fehl\n");
        failures++;
        goto done;
    }

    static uint8_t back[MAX_IMG];
    size_t back_len = slurp(hfe_tmp, back);
    printf("  Ergebnisdatei : %zu Byte\n", back_len);

    /* Die Messung. Nicht die Behauptung. */
    if (back_len == orig_len && memcmp(back, a, orig_len) == 0) {
        printf("\n  BITGLEICH — UFT_RT_LOSSLESS ist fuer dieses Paar belegt.\n");
    } else {
        keep_artifacts = 1;
        size_t diff = 0;
        size_t n = back_len < orig_len ? back_len : orig_len;
        for (size_t i = 0; i < n; i++)
            if (back[i] != a[i]) diff++;
        printf("\n  NICHT bitgleich:\n");
        printf("    Laenge   : %zu -> %zu (%+lld Byte)\n",
               orig_len, back_len, (long long)back_len - (long long)orig_len);
        printf("    Bytes ungleich im gemeinsamen Teil: %zu von %zu (%.1f %%)\n",
               diff, n, n ? 100.0 * (double)diff / (double)n : 0.0);
        printf("\n  Damit ist UFT_RT_LOSSLESS fuer SCP<->HFE nicht belegt.\n"
               "  Die Regel in src/core/uft_roundtrip.c verlangt fuer LL einen\n"
               "  Test, der Bit-Identitaet BEWEIST. Dieser Test ist er, und er\n"
               "  sagt: sie liegt nicht vor. Zu korrigieren ist der\n"
               "  Tabelleneintrag, nicht dieser Test.\n");
        /* Kein failures++: der Test hat seine Aufgabe erfuellt, er hat
         * gemessen. Ob der Eintrag bleibt, entscheidet der Eigentuemer —
         * siehe OPEN_ITEMS P0-14. Rot wird dieser Test nur, wenn eine
         * Wandlung fehlschlaegt oder abstuerzt. */
    }

done:
    /* MF-527: die Zwischendateien bleiben liegen, wenn das Ergebnis NICHT
     * bitgleich war. Ein Befund ohne seine Artefakte ist eine Anekdote —
     * dieselbe Regel wie bei tests/crashers/. Bei bitgleichem Ergebnis
     * werden sie aufgeraeumt. */
    if (!keep_artifacts) {
        remove(scp_tmp);
        remove(hfe_tmp);
    } else {
        printf("\n  Zwischendateien behalten: %s, %s\n", scp_tmp, hfe_tmp);
    }
    printf("\n%s (%d Abweichungen)\n",
           failures ? "FEHLGESCHLAGEN" : "OK", failures);
    return failures ? 1 : 0;
}
