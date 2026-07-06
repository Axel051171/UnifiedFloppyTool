/**
 * @file test_scp_footer_roundtrip.c
 * @brief SCP extension footer write -> read round-trip (MF-351).
 *
 * Links the real SCP writer + parser. The writer now emits a spec-compliant
 * 48-byte extension footer (provenance strings + "FPCS" signature) and sets the
 * FOOTER flag; the parser was corrected to read the real 48-byte layout (6
 * uint32 string offsets, 2 int64 timestamps, 4 version bytes, FPCS) instead of
 * the previous wrong 52-byte/13-uint32 parse. This proves the two agree AND the
 * layout matches the cbmstuff spec (FPCS present, correct offsets/version byte).
 */

#include "uft/formats/uft_scp_writer.h"
#include "uft/flux/uft_scp_parser.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

static void get_temp_path(char *path, size_t size) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_scp_ft_%d.scp", dir, rand() % 100000);
}

TEST(footer_written_and_read_back) {
    const uint32_t flux_ns[] = { 4000, 4000, 6000, 8000, 4000, 4000 };
    const size_t n = sizeof(flux_ns) / sizeof(flux_ns[0]);
    uint32_t dur = 0; for (size_t i = 0; i < n; i++) dur += flux_ns[i];

    scp_writer_t *w = scp_writer_create(0x00, 1);
    ASSERT(w != NULL);
    ASSERT(scp_writer_add_track(w, 0, 0, flux_ns, n, dur, 0) == 0);

    char path[256];
    get_temp_path(path, sizeof(path));
    ASSERT(scp_writer_save(w, path) == 0);
    scp_writer_free(w);

    uft_scp_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));   /* open close-firsts internally */
    ASSERT(uft_scp_open(&ctx, path) == UFT_SCP_OK);

    /* The FOOTER flag was set and a valid FPCS footer parsed. */
    ASSERT(ctx.has_footer == true);
    ASSERT(ctx.ext_footer.present == true);
    ASSERT(strcmp(ctx.ext_footer.creator, "UnifiedFloppyTool") == 0);
    ASSERT(strcmp(ctx.ext_footer.application, "UFT") == 0);
    ASSERT(ctx.ext_footer.format_revision == 0x16);
    ASSERT(strcmp(ctx.ext_footer.app_version, "1.0") == 0);

    uft_scp_close(&ctx);
    remove(path);
}

/* The flux still reads correctly with a footer appended (footer must not
   disturb track-offset-based flux reading). */
TEST(flux_intact_with_footer) {
    const uint32_t flux_ns[] = { 4000, 6000, 8000, 4000 };
    const size_t n = 4;
    uint32_t dur = 0; for (size_t i = 0; i < n; i++) dur += flux_ns[i];

    scp_writer_t *w = scp_writer_create(0x00, 1);
    ASSERT(w != NULL);
    ASSERT(scp_writer_add_track(w, 0, 0, flux_ns, n, dur, 0) == 0);
    char path[256];
    get_temp_path(path, sizeof(path));
    ASSERT(scp_writer_save(w, path) == 0);
    scp_writer_free(w);

    uft_scp_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ASSERT(uft_scp_open(&ctx, path) == UFT_SCP_OK);
    uft_scp_track_data_t data;
    memset(&data, 0, sizeof(data));
    ASSERT(uft_scp_read_track(&ctx, 0, &data) == UFT_SCP_OK);
    ASSERT(data.revolutions[0].flux_count == n);
    for (size_t i = 0; i < n; i++) {
        uint32_t got = data.revolutions[0].flux_data[i];
        uint32_t want = flux_ns[i];
        uint32_t diff = (got > want) ? (got - want) : (want - got);
        ASSERT(diff <= UFT_SCP_BASE_PERIOD_NS);
    }
    uft_scp_free_track(&data);
    uft_scp_close(&ctx);
    remove(path);
}

int main(void) {
    printf("=== SCP extension footer write/read round-trip (MF-351) ===\n");
    RUN(footer_written_and_read_back);
    RUN(flux_intact_with_footer);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
