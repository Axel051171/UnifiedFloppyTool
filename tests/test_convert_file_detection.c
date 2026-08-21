/**
 * @file test_convert_file_detection.c
 * @brief uft_convert_file() picks its path from a format, not a container (MF-450).
 *
 * `uft_convert_file()` is the entry point behind every file conversion, and it
 * had no test. What it did with the source format is why this file exists.
 *
 * It calls uft_probe_format(), which returns `plugin->format`. 131 of the 137
 * plugins declared `.format = UFT_FORMAT_DSK` — and the enum was never the
 * reason: uft_format_t already had UFT_FORMAT_D64, UFT_FORMAT_ADF,
 * UFT_FORMAT_ATR and 27 more values that the plugins simply did not use.
 *
 * Two consequences followed, and both are asserted below:
 *
 *   - The conversion path table is keyed on real formats (D64->G64, SCP->HFE,
 *     ADF->SCP). A source detected as DSK matched none of them, so the answer
 *     for nearly every conversion was "No conversion path from ... to ...".
 *   - `if (src_format == dst_format)` copies the file verbatim and reports a
 *     successful conversion. With everything collapsed onto one id, that branch
 *     was one badly-chosen target away from writing a D81 into a .d64 and
 *     calling it done.
 *
 * The third assertion covers the other half of ARCH-14: uft_probe_result_t has
 * carried `alternative_count` and `alternatives[4]` since it was written, and
 * the only implementation left them zero. Permanently 0 does not read as "not
 * filled in", it reads as "checked, nothing else matched".
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_format_probe.h"
#include "uft/uft_format_convert.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must be defined by the build (tests/CMakeLists.txt)"
#endif

const uft_conversion_path_t* uft_convert_get_path(uft_format_t src,
                                                  uft_format_t dst);

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-52s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

static const char *img(const char *name)
{
    static char p[512];
    snprintf(p, sizeof(p), "%s/%s", UFT_CORPUS_DIR, name);
    return p;
}

static uint8_t *slurp(const char *path, size_t *out)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n <= 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    uint8_t *b = malloc((size_t)n);
    if (!b) { fclose(f); return NULL; }
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    fclose(f);
    *out = (size_t)n;
    return b;
}

TEST(the_source_format_is_a_format_not_a_container) {
    ASSERT(uft_register_all_formats() == UFT_OK);

    size_t n = 0;
    uint8_t *d = slurp(img("vice_c1541_35trk.d64"), &n);
    ASSERT(d != NULL);

    uft_probe_result_t r;
    uft_format_t f = uft_probe_format(d, n, img("vice_c1541_35trk.d64"), &r);
    free(d);

    /* Before MF-450 this was UFT_FORMAT_DSK, like almost every other image. */
    ASSERT(f == UFT_FORMAT_D64);
    ASSERT(r.format == UFT_FORMAT_D64);
    ASSERT(r.confidence > 0);
}

TEST(the_conversion_path_table_is_reachable_again) {
    /* The table has entries for D64->G64, D64->SCP and ADF->SCP. With every
     * source collapsed onto UFT_FORMAT_DSK, uft_convert_get_path() could not
     * find any of them: the lookup compares source AND target. */
    ASSERT(uft_convert_get_path(UFT_FORMAT_D64, UFT_FORMAT_G64) != NULL);
    ASSERT(uft_convert_get_path(UFT_FORMAT_D64, UFT_FORMAT_SCP) != NULL);
    ASSERT(uft_convert_get_path(UFT_FORMAT_ADF, UFT_FORMAT_SCP) != NULL);

    /* and the id the sources used to carry matches nothing, correctly */
    ASSERT(uft_convert_get_path(UFT_FORMAT_DSK, UFT_FORMAT_G64) == NULL);
}

TEST(two_different_disks_no_longer_share_one_format) {
    /* The dangerous branch: `if (src_format == dst_format)` writes the source
     * bytes to the destination and reports success. It is correct only when the
     * two really are the same format. */
    size_t n64 = 0, n81 = 0;
    uint8_t *d64 = slurp(img("vice_c1541_35trk.d64"), &n64);
    uint8_t *d81 = slurp(img("vice_c1541_80trk.d81"), &n81);
    ASSERT(d64 && d81);

    uft_probe_result_t a, b;
    uft_format_t fa = uft_probe_format(d64, n64, NULL, &a);
    uft_format_t fb = uft_probe_format(d81, n81, NULL, &b);
    free(d64); free(d81);

    ASSERT(fa == UFT_FORMAT_D64);
    ASSERT(fb == UFT_FORMAT_D81);
    ASSERT(fa != fb);           /* both were UFT_FORMAT_DSK before MF-450 */
}

TEST(the_alternatives_field_is_filled_or_honestly_empty) {
    /* An unambiguous image reports no alternatives because there are none —
     * not because nobody looked. */
    size_t n = 0;
    uint8_t *g = slurp(img("vice_c1541_35trk.g64"), &n);
    ASSERT(g != NULL);
    uft_probe_result_t r;
    ASSERT(uft_probe_format(g, n, NULL, &r) == UFT_FORMAT_G64);
    free(g);
    ASSERT(r.alternative_count == 0);
    ASSERT(r.warnings[0] == '\0');

    /* The Atari raw image is claimed by five plugins at the same confidence.
     * Before MF-450 this also reported alternative_count == 0 — the same answer
     * as the certain case above. */
    size_t nx = 0;
    uint8_t *x = slurp(img("atrcopy_dos2sd.xfd"), &nx);
    ASSERT(x != NULL);
    uft_probe_result_t rx;
    uft_probe_format(x, nx, NULL, &rx);
    free(x);

    ASSERT(rx.alternative_count > 0);
    ASSERT(rx.alternative_count <= 4);          /* the array bound, documented */
    ASSERT(rx.warnings[0] != '\0');
    ASSERT(strstr(rx.warnings, "registration order") != NULL);
    for (int i = 0; i < rx.alternative_count; i++) {
        ASSERT(rx.alt_confidence[i] == rx.confidence);
        ASSERT(rx.alternatives[i] != rx.format);
    }
}

TEST(an_ambiguous_source_is_refused_not_converted) {
    /* Converting means running the winner's decoder over the bytes. If another
     * plugin claimed them just as strongly, the choice was registration order,
     * and the output would be a file derived from a guess and reported as a
     * conversion. */
    uft_convert_result_t res;
    memset(&res, 0, sizeof(res));

    char out[512];
    snprintf(out, sizeof(out), "%s", "uft_test_convert_out.tmp");
    remove(out);

    uft_error_t err = uft_convert_file(img("atrcopy_dos2sd.xfd"), out,
                                       UFT_FORMAT_SCP, NULL, &res);
    ASSERT(err != UFT_OK);
    ASSERT(res.success == false);
    ASSERT(res.warning_count >= 1);
    ASSERT(strstr(res.warnings[0], "ambiguous") != NULL);

    /* and nothing was written */
    FILE *f = fopen(out, "rb");
    if (f) { fclose(f); remove(out); ASSERT(0); }
}

int main(void)
{
    printf("=== uft_convert_file(): format, not container (MF-450) ===\n");
    RUN(the_source_format_is_a_format_not_a_container);
    RUN(the_conversion_path_table_is_reachable_again);
    RUN(two_different_disks_no_longer_share_one_format);
    RUN(the_alternatives_field_is_filled_or_honestly_empty);
    RUN(an_ambiguous_source_is_refused_not_converted);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
