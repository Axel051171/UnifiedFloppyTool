/**
 * @file test_convert_identity_lossless.c
 * @brief Wandlung in dasselbe Format muss bitgleich sein (MF-532)
 *
 * ── Warum es diesen Test gibt ────────────────────────────────────────────
 *
 * MF-527 hat den einzigen LOSSLESS-Eintrag der Matrix widerlegt und
 * herabgestuft. Seither fuehrt `src/core/uft_roundtrip.c` **keinen**
 * LL-Eintrag mehr — es gibt keine Wandlung, die ohne ausdrueckliche
 * Zustimmung laeuft.
 *
 * Die Regel im Kopf jener Datei sagt, wie man einen verdient:
 *
 *     LL -> you added a round-trip test that proves byte-identity.
 *
 * Dieser Test verdient zwei.
 *
 * ── Was geprueft wird ────────────────────────────────────────────────────
 *
 * Die Wandlung eines Abbilds in **sein eigenes Format**. `uft_convert_file()`
 * hat dafuer einen eigenen Zweig:
 *
 *     if (src_format == dst_format) {
 *         err = uftc_write_output_file(dst_path, src_data, src_size);
 *
 * — eine woertliche Kopie. Bit-Identitaet ist damit nicht nur plausibel,
 * sondern durch die Bauart gegeben. Geprueft wird sie trotzdem, denn
 * "durch die Bauart gegeben" ist genau die Sorte Annahme, die dieser Baum
 * an mehreren Stellen teuer bezahlt hat.
 *
 * Zwei Quellen aus dem Korpus, zwei Formatfamilien:
 *
 *     vice_c1541_35trk.d64  ->  D64   (Commodore, 174848 B)
 *     xdftool_dd_ofs.adf    ->  ADF   (Amiga,     901120 B)
 *
 * ── Und die andere Haelfte: ohne Zustimmung ──────────────────────────────
 *
 * Eine verlustfreie Wandlung muss **ohne** `accept_data_loss` laufen. Das
 * ist der Sinn der LL-Einstufung, und der Test prueft es ausdruecklich —
 * sonst waere der Eintrag eine Zusage ohne Wirkung.
 */

#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_format_convert.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "."
#endif

#define MAX_IMG (4 * 1024 * 1024)
static uint8_t a[MAX_IMG], b[MAX_IMG];

static int failures;

static size_t slurp(const char *path, uint8_t *dst)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(dst, 1, MAX_IMG, f);
    fclose(f);
    return n;
}

static void check_identity(const char *name, uft_format_t fmt,
                           const char *fmt_name, const char *out_name)
{
    char src[1024];
    snprintf(src, sizeof(src), "%s/%s", UFT_CORPUS_DIR, name);
    size_t n_in = slurp(src, a);
    if (!n_in) {
        printf("  FAIL %s fehlt (%s)\n", name, src);
        failures++;
        return;
    }

    /* OHNE Zustimmung — das ist der Punkt. */
    uft_convert_options_t o;
    memset(&o, 0, sizeof(o));
    uft_convert_result_t r;
    memset(&r, 0, sizeof(r));

    remove(out_name);
    uft_error_t e = uft_convert_file(src, out_name, fmt, &o, &r);
    if (e != UFT_OK) {
        printf("  FAIL %s -> %s: abgelehnt (%d)%s%s\n", name, fmt_name, (int)e,
               r.warning_count > 0 ? " — " : "",
               r.warning_count > 0 ? r.warnings[0] : "");
        failures++;
        remove(out_name);
        return;
    }

    size_t n_out = slurp(out_name, b);
    if (n_out != n_in || memcmp(a, b, n_in) != 0) {
        size_t diff = 0, m = n_out < n_in ? n_out : n_in;
        for (size_t i = 0; i < m; i++) if (a[i] != b[i]) diff++;
        printf("  FAIL %s -> %s: NICHT bitgleich (%zu -> %zu B, %zu Bytes "
               "verschieden)\n", name, fmt_name, n_in, n_out, diff);
        failures++;
    } else {
        printf("  ok   %s -> %s: bitgleich, %zu Byte, ohne Zustimmung\n",
               name, fmt_name, n_in);
    }
    remove(out_name);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    if (uft_register_all_formats() != UFT_OK) {
        printf("FEHLER: uft_register_all_formats() schlug fehl\n");
        return 1;
    }

    printf("Identitaets-Wandlung muss bitgleich sein und ohne Zustimmung "
           "laufen (MF-532)\n");

    check_identity("vice_c1541_35trk.d64", UFT_FORMAT_D64, "D64",
                   "uft_ident_out.d64");
    check_identity("xdftool_dd_ofs.adf",   UFT_FORMAT_ADF, "ADF",
                   "uft_ident_out.adf");

    printf("\n%s (%d Abweichungen)\n",
           failures ? "FEHLGESCHLAGEN" : "OK", failures);
    return failures ? 1 : 0;
}
