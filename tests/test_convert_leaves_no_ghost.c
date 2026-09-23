/* SPDX-License-Identifier: GPL-2.0-or-later */
/* A rejected conversion must preserve existing bytes and must not create
 * a new output. Test through the public API, including converter refusal. */
#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_format_convert.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(x) do { if (!(x)) { \
    printf("FAIL line %d: %s\n", __LINE__, #x); failures++; } } while (0)

static int put(const char *path, const void *data, size_t n)
{
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    int ok = fwrite(data, 1, n, f) == n;
    return fclose(f) == 0 && ok;
}

static int equal_file(const char *path, const void *data, size_t n)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    unsigned char *buf = malloc(n + 1);
    if (!buf) { fclose(f); return 0; }
    size_t got = fread(buf, 1, n + 1, f);
    int ok = got == n && !ferror(f) && memcmp(buf, data, n) == 0;
    free(buf);
    fclose(f);
    return ok;
}

static void rejected(const char *source, uft_format_t format)
{
    const char *target = "uft_ghost_out.hfe";
    static unsigned char original[65536];
    memset(original, 0xAB, sizeof(original));
    CHECK(put(target, original, sizeof(original)));
    uft_convert_options_t opts = uft_convert_default_options();
    opts.accept_data_loss = true;
    uft_convert_result_t result = {0};
    CHECK(uft_convert_file(source, target, format, &opts, &result) != UFT_OK);
    CHECK(!result.success);
    CHECK(equal_file(target, original, sizeof(original)));
    remove(target);

    memset(&result, 0, sizeof(result));
    CHECK(uft_convert_file(source, target, format, &opts, &result) != UFT_OK);
    CHECK(!result.success);
    FILE *f = fopen(target, "rb");
    CHECK(f == NULL);
    if (f) fclose(f);
    remove(target);
}

int main(void)
{
    CHECK(uft_register_all_formats() == UFT_OK);
    static uint8_t adf[901120];
    for (size_t i = 0; i < sizeof(adf); i++)
        adf[i] = (uint8_t)(i * 5 + (i >> 9));
    const char *src_adf = "uft_ghost_in.adf";
    CHECK(put(src_adf, adf, sizeof(adf)));
    rejected(src_adf, UFT_FORMAT_G64); /* no conversion path */
    rejected("uft_ghost_missing_directory/source.adf", UFT_FORMAT_G64);
    rejected(NULL, UFT_FORMAT_G64);

    /* Same path: an unsupported conversion must not delete its source. */
    uft_convert_result_t result = {0};
    CHECK(uft_convert_file(src_adf, src_adf, UFT_FORMAT_G64, NULL,
                           &result) != UFT_OK);
    CHECK(equal_file(src_adf, adf, sizeof(adf)));
    CHECK(uft_convert_file(src_adf, src_adf, UFT_FORMAT_G64, NULL,
                           NULL) != UFT_OK);
    CHECK(equal_file(src_adf, adf, sizeof(adf)));

    /* A truncated real HFE reaches the converter and yields no tracks. */
    const char *src_hfe = "uft_ghost_in.hfe";
    FILE *in = fopen(UFT_CORPUS_DIR "/gw_amigados.hfe", "rb");
    CHECK(in != NULL);
    if (in) {
        unsigned char header[1024];
        size_t n = fread(header, 1, sizeof(header), in);
        fclose(in);
        CHECK(n == sizeof(header));
        CHECK(put(src_hfe, header, n));
        rejected(src_hfe, UFT_FORMAT_IMG);
        remove(src_hfe);
    }
    remove(src_adf);
    printf("%s (%d failures)\n", failures ? "FAILED" : "OK", failures);
    return failures ? 1 : 0;
}
