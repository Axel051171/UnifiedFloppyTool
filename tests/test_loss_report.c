/**
 * @file test_loss_report.c
 * @brief Prinzip 1 — .loss.json Sidecar-Writer tests.
 *
 * Validiert dass ein LOSSY-DOCUMENTED-Report korrekt im JSON-Schema
 * uft-loss-report-v1 landet und alle Pflicht-Felder enthält.
 */

#include "uft/core/uft_loss_report.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-32s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* Read tmpfile contents into a heap buffer. Caller frees. */
static char *slurp(FILE *f, size_t *out_size) {
    fflush(f);
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) return NULL;
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) return NULL;
    size_t r = fread(buf, 1, (size_t)sz, f);
    buf[r] = '\0';
    if (out_size) *out_size = r;
    return buf;
}

TEST(schema_version_stable) {
    /* Stabiles Schema-Feld — Änderung wäre Breaking-Change. */
    ASSERT(strcmp(uft_loss_report_schema_version(), "uft-loss-report-v1") == 0);
}

TEST(category_strings_stable) {
    ASSERT(strcmp(uft_loss_category_string(UFT_LOSS_WEAK_BITS),        "WEAK_BITS")        == 0);
    ASSERT(strcmp(uft_loss_category_string(UFT_LOSS_FLUX_TIMING),      "FLUX_TIMING")      == 0);
    ASSERT(strcmp(uft_loss_category_string(UFT_LOSS_INDEX_PULSES),     "INDEX_PULSES")     == 0);
    ASSERT(strcmp(uft_loss_category_string(UFT_LOSS_SYNC_PATTERNS),    "SYNC_PATTERNS")    == 0);
    ASSERT(strcmp(uft_loss_category_string(UFT_LOSS_MULTI_REVOLUTION), "MULTI_REVOLUTION") == 0);
    ASSERT(strcmp(uft_loss_category_string(UFT_LOSS_CUSTOM_METADATA),  "CUSTOM_METADATA")  == 0);
    ASSERT(strcmp(uft_loss_category_string(UFT_LOSS_COPY_PROTECTION),  "COPY_PROTECTION")  == 0);
    ASSERT(strcmp(uft_loss_category_string(UFT_LOSS_LONG_TRACKS),      "LONG_TRACKS")      == 0);
    ASSERT(strcmp(uft_loss_category_string(UFT_LOSS_HALF_TRACKS),      "HALF_TRACKS")      == 0);
    ASSERT(strcmp(uft_loss_category_string(UFT_LOSS_WRITE_SPLICE),     "WRITE_SPLICE")     == 0);
    ASSERT(strcmp(uft_loss_category_string(UFT_LOSS_OTHER),            "OTHER")            == 0);
    /* Out-of-range fällt auf OTHER zurück — nie NULL. */
    ASSERT(strcmp(uft_loss_category_string((uft_loss_category_t)12345), "OTHER") == 0);
}

TEST(enum_values_stable) {
    /* ABI: Werte bleiben für Schema-v1 fest. */
    ASSERT(UFT_LOSS_WEAK_BITS        == 0);
    ASSERT(UFT_LOSS_FLUX_TIMING      == 1);
    ASSERT(UFT_LOSS_INDEX_PULSES     == 2);
    ASSERT(UFT_LOSS_SYNC_PATTERNS    == 3);
    ASSERT(UFT_LOSS_MULTI_REVOLUTION == 4);
    ASSERT(UFT_LOSS_CUSTOM_METADATA  == 5);
    ASSERT(UFT_LOSS_COPY_PROTECTION  == 6);
    ASSERT(UFT_LOSS_LONG_TRACKS      == 7);
    ASSERT(UFT_LOSS_HALF_TRACKS      == 8);
    ASSERT(UFT_LOSS_WRITE_SPLICE     == 9);
    ASSERT(UFT_LOSS_OTHER            == 99);
}

TEST(write_stream_scp_to_img_example) {
    /* Der kanonische Beispiel-Report aus Prinzip 1:
     * "SCP → IMG verliert: Weak-Bits (23 erkannt), Flux-Timing, Index-Pulse." */
    const uft_loss_entry_t entries[] = {
        { UFT_LOSS_WEAK_BITS,    23, "Weak-Bit-Flags cannot be represented in raw IMG" },
        { UFT_LOSS_FLUX_TIMING,   1, "Track-precise timing lost when sampling to sectors" },
        { UFT_LOSS_INDEX_PULSES,  1, "Index pulse positions discarded" },
    };
    const uft_loss_report_t report = {
        .source_path    = "mydisk.scp",
        .target_path    = "mydisk.img",
        .source_format  = "SCP",
        .target_format  = "IMG",
        .timestamp_unix = 1713438000ULL,
        .uft_version    = "4.1.0",
        .entries        = entries,
        .entry_count    = sizeof(entries)/sizeof(entries[0]),
    };

    FILE *f = fopen("_uft_loss_stream_tmp.txt", "w+b");
    ASSERT(f != NULL);
    ASSERT(uft_loss_report_write_stream(&report, f) == UFT_OK);

    size_t n = 0;
    char *json = slurp(f, &n);
    fclose(f);
    remove("_uft_loss_stream_tmp.txt");
    ASSERT(json != NULL);
    ASSERT(n > 0);

    /* Pflicht-Felder laut Schema v1. */
    ASSERT(strstr(json, "\"schema\": \"uft-loss-report-v1\"") != NULL);
    ASSERT(strstr(json, "\"source\"") != NULL);
    ASSERT(strstr(json, "\"target\"") != NULL);
    ASSERT(strstr(json, "\"path\": \"mydisk.scp\"") != NULL);
    ASSERT(strstr(json, "\"format\": \"SCP\"") != NULL);
    ASSERT(strstr(json, "\"path\": \"mydisk.img\"") != NULL);
    ASSERT(strstr(json, "\"format\": \"IMG\"") != NULL);
    ASSERT(strstr(json, "\"timestamp_unix\": 1713438000") != NULL);
    ASSERT(strstr(json, "\"uft_version\": \"4.1.0\"") != NULL);
    ASSERT(strstr(json, "\"category\": \"WEAK_BITS\"") != NULL);
    ASSERT(strstr(json, "\"count\": 23") != NULL);
    ASSERT(strstr(json, "FLUX_TIMING") != NULL);
    ASSERT(strstr(json, "INDEX_PULSES") != NULL);

    free(json);
}

TEST(write_stream_empty_entries) {
    /* Ein Report darf ohne Entries geschrieben werden (z.B. als Dry-Run). */
    const uft_loss_report_t report = {
        .source_path = "a.scp",  .target_path = "a.hfe",
        .source_format = "SCP",  .target_format = "HFE",
        .timestamp_unix = 1,
        .uft_version = "4.1.0",
        .entries = NULL, .entry_count = 0,
    };
    FILE *f = fopen("_uft_loss_stream_tmp.txt", "w+b");
    ASSERT(f != NULL);
    ASSERT(uft_loss_report_write_stream(&report, f) == UFT_OK);

    char *json = slurp(f, NULL);
    fclose(f);
    remove("_uft_loss_stream_tmp.txt");
    ASSERT(json != NULL);
    ASSERT(strstr(json, "\"entries\": []") != NULL);
    free(json);
}

TEST(json_escape_special_chars) {
    /* Pfade mit Anführungszeichen / Backslashes dürfen nichts brechen. */
    const uft_loss_entry_t e = { UFT_LOSS_OTHER, 1,
        "description with \"quotes\" and \\ backslash and\nnewline" };
    const uft_loss_report_t report = {
        .source_path = "c:\\path with \"quotes\".scp",
        .target_path = "out.img",
        .source_format = "SCP", .target_format = "IMG",
        .timestamp_unix = 0,
        .uft_version = "4.1.0",
        .entries = &e, .entry_count = 1,
    };
    FILE *f = fopen("_uft_loss_stream_tmp.txt", "w+b");
    ASSERT(f != NULL);
    ASSERT(uft_loss_report_write_stream(&report, f) == UFT_OK);
    char *json = slurp(f, NULL);
    fclose(f);
    remove("_uft_loss_stream_tmp.txt");
    ASSERT(json != NULL);
    /* Escaped sequences must appear literally. */
    ASSERT(strstr(json, "\\\"quotes\\\"") != NULL);
    ASSERT(strstr(json, "\\\\") != NULL);   /* escaped backslash */
    ASSERT(strstr(json, "\\n") != NULL);    /* escaped newline */
    free(json);
}

TEST(null_input_rejected) {
    FILE *f = fopen("_uft_loss_stream_tmp.txt", "w+b");
    ASSERT(f != NULL);
    /* NULL report → NULL_POINTER. */
    ASSERT(uft_loss_report_write_stream(NULL, f) != UFT_OK);
    /* NULL stream → NULL_POINTER. */
    const uft_loss_report_t r = {0};
    ASSERT(uft_loss_report_write_stream(&r, NULL) != UFT_OK);
    fclose(f);
    remove("_uft_loss_stream_tmp.txt");
}

TEST(file_write_sidecar_default_suffix) {
    /* report->target_path + ".loss.json" wird erzeugt wenn sidecar_path NULL. */
    const uft_loss_entry_t e = { UFT_LOSS_WEAK_BITS, 1, "test" };
    const uft_loss_report_t report = {
        .source_path = "in.scp",
        .target_path = "uft_loss_test_tmp.img",
        .source_format = "SCP", .target_format = "IMG",
        .timestamp_unix = 0,
        .uft_version = "4.1.0",
        .entries = &e, .entry_count = 1,
    };
    /* passing sidecar path explicitly so we can delete it later */
    const char *sidecar = "uft_loss_test_tmp.img.loss.json";
    remove(sidecar);
    ASSERT(uft_loss_report_write(&report, NULL) == UFT_OK);
    FILE *check = fopen(sidecar, "r");
    ASSERT(check != NULL);
    char buf[4096]; size_t n = fread(buf, 1, sizeof(buf)-1, check);
    buf[n] = '\0';
    fclose(check);
    ASSERT(strstr(buf, "uft-loss-report-v1") != NULL);
    ASSERT(strstr(buf, "WEAK_BITS") != NULL);
    remove(sidecar);
}

int main(void) {
    printf("=== Prinzip 1 — .loss.json Sidecar Writer ===\n");
    RUN(schema_version_stable);
    RUN(category_strings_stable);
    RUN(enum_values_stable);
    RUN(write_stream_scp_to_img_example);
    RUN(write_stream_empty_entries);
    RUN(json_escape_special_chars);
    RUN(null_input_rejected);
    RUN(file_write_sidecar_default_suffix);
    printf("Passed %d, Failed %d\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
