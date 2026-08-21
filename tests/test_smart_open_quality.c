/**
 * @file test_smart_open_quality.c
 * @brief uft_smart_open() must count, not claim (MF-443, MF-444).
 *
 * This is the first test in the tree that calls uft_smart_open() at all. That
 * is not incidental — the function had no caller, which is how it kept
 * reporting a quality assessment nobody had measured:
 *
 *     quality->level            = UFT_QUALITY_GOOD;
 *     quality->readable_sectors = 100;
 *     quality->total_sectors    = 100;
 *
 * hard-coded, before a single sector was read, and uft_smart_report() printed
 * it as "Sectors: 100 / 100 readable, Quality: Good" for any disk. A report
 * that looks like a measurement and is not one is worse than no report: it
 * goes into a preservation record, where nobody can tell the two apart.
 *
 * The body now reads every track through the format plugin and counts. These
 * assertions are what makes that claim checkable, and they are deliberately
 * exact rather than "plausible":
 *
 *   - 683 sectors on a 35-track 1541 disk. Not "more than 600".
 *   - every one of them CRC-clean on an image VICE produced.
 *   - the numbers the report prints are the numbers the struct holds.
 *
 * The corpus images are the same T1b references the converter seams use, so a
 * regression here and a regression there point at the same cause.
 */

#include "uft/uft_smart_open.h"
#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must be defined by the build (tests/CMakeLists.txt)"
#endif

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

extern const uft_format_plugin_t uft_format_plugin_d64;
extern const uft_format_plugin_t uft_format_plugin_d67;
extern const uft_format_plugin_t uft_format_plugin_d81;
extern const uft_format_plugin_t uft_format_plugin_xfd;

static const char *img(const char *name)
{
    static char p[512];
    snprintf(p, sizeof(p), "%s/%s", UFT_CORPUS_DIR, name);
    return p;
}

TEST(an_empty_registry_is_reported_as_such_not_as_unknown_format) {
    /* Runs first, before anything is registered — that is the point. With no
     * plugin registered the file is never examined, and saying "Unknown format"
     * would blame the disk for the caller's omission using the same words as a
     * real non-recognition. Two states, two messages (MF-444). */
    ASSERT(uft_registered_format_plugin_count() == 0);

    uft_smart_options_t opts;
    uft_smart_options_init(&opts);

    uft_smart_result_t res;
    memset(&res, 0, sizeof(res));
    ASSERT(uft_smart_open(img("vice_c1541_35trk.d64"), &opts, &res) != 0);
    ASSERT(strstr(res.error, "No format plugins registered") != NULL);
}

TEST(registering_twice_is_not_an_error) {
    /* uft_register_all_formats() used to return on the first plugin the
     * registry already held, which made a second call fail on plugin #1 and
     * stop. Registration is a requested end state, not a one-shot event. */
    ASSERT(uft_register_format_plugin(&uft_format_plugin_d64) == UFT_OK);
    ASSERT(uft_register_format_plugin(&uft_format_plugin_d67) == UFT_OK);
    ASSERT(uft_register_format_plugin(&uft_format_plugin_d81) == UFT_OK);
    ASSERT(uft_register_format_plugin(&uft_format_plugin_xfd) == UFT_OK);
    ASSERT(uft_registered_format_plugin_count() == 4);

    /* the same plugin again must be refused, and must not disturb the four */
    ASSERT(uft_register_format_plugin(&uft_format_plugin_d64) != UFT_OK);
    ASSERT(uft_registered_format_plugin_count() == 4);

    /* All four declare .format = UFT_FORMAT_DSK, and before MF-444 the registry
     * rejected a second plugin carrying an id it already held — which is why 81
     * of 88 plugins could never register. Four on one container id is the
     * normal case, not a collision. */
    ASSERT(uft_count_format_plugins_for(UFT_FORMAT_DSK) == 4);
    ASSERT(uft_get_format_plugin_by_name("D81") == &uft_format_plugin_d81);
    ASSERT(uft_get_format_plugin_by_name("XFD") == &uft_format_plugin_xfd);
}

TEST(the_sector_counts_are_read_from_the_disk) {
    /* 35 tracks in the 1541 zone table: 17x21 + 7x19 + 6x18 + 5x17 = 683.
     * An exact number is the point — 100/100 was "plausible" too. */
    uft_smart_options_t opts;
    uft_smart_options_init(&opts);

    uft_smart_result_t res;
    memset(&res, 0, sizeof(res));
    ASSERT(uft_smart_open(img("vice_c1541_35trk.d64"), &opts, &res) == 0);

    ASSERT(res.quality.total_sectors == 683);
    ASSERT(res.quality.readable_sectors == 683);
    ASSERT(res.quality.crc_errors == 0);
    ASSERT(res.quality.level == UFT_QUALITY_PERFECT);
    ASSERT(res.quality.bit_error_rate == 0.0);

    uft_smart_close(&res);
}

TEST(a_different_disk_gives_different_numbers) {
    /* The strongest evidence that the counting is real: a 2040/4040 disk has
     * 690 blocks, not 683, because DOS 1 puts 20 sectors in zone 2 where 2.6
     * puts 19. A hard-coded body cannot tell these two apart. */
    uft_smart_options_t opts;
    uft_smart_options_init(&opts);

    uft_smart_result_t res;
    memset(&res, 0, sizeof(res));
    ASSERT(uft_smart_open(img("vice_c1541_2040.d67"), &opts, &res) == 0);

    ASSERT(res.quality.total_sectors == 690);
    ASSERT(res.quality.readable_sectors == 690);
    ASSERT(res.quality.level == UFT_QUALITY_PERFECT);

    uft_smart_close(&res);
}

TEST(the_d81_is_counted_too_though_it_is_not_gcr) {
    /* 1581: MFM, 80 tracks x 2 sides x 40 sectors = 3200 blocks. Included
     * because it exercises a plugin with a different geometry model — the
     * count must come from the plugin, not from a CBM assumption baked into
     * this function. */
    uft_smart_options_t opts;
    uft_smart_options_init(&opts);

    uft_smart_result_t res;
    memset(&res, 0, sizeof(res));
    ASSERT(uft_smart_open(img("vice_c1541_80trk.d81"), &opts, &res) == 0);

    ASSERT(res.quality.total_sectors == 3200);
    ASSERT(res.quality.readable_sectors == 3200);

    uft_smart_close(&res);
}

TEST(unmeasured_fields_say_so_instead_of_claiming_zero) {
    /* Neither CRC correction nor multi-revolution fusion runs on this path.
     * Reporting 0 would assert that nothing needed correcting and no weak bit
     * was resolved — two claims nobody checked. NOT_DETERMINED says what is
     * true: this was not measured. */
    uft_smart_options_t opts;
    uft_smart_options_init(&opts);

    uft_smart_result_t res;
    memset(&res, 0, sizeof(res));
    ASSERT(uft_smart_open(img("vice_c1541_35trk.d64"), &opts, &res) == 0);

    ASSERT(res.quality.crc_corrected == UFT_QUALITY_NOT_DETERMINED);
    ASSERT(res.quality.weak_bits_resolved == UFT_QUALITY_NOT_DETERMINED);

    uft_smart_close(&res);
}

TEST(the_report_prints_the_numbers_the_struct_holds) {
    /* The report is the part that reaches a human, so it gets its own check:
     * no formatting path may reintroduce a number the struct never had. */
    uft_smart_options_t opts;
    uft_smart_options_init(&opts);

    uft_smart_result_t res;
    memset(&res, 0, sizeof(res));
    ASSERT(uft_smart_open(img("vice_c1541_35trk.d64"), &opts, &res) == 0);

    char *report = uft_smart_report(&res);
    ASSERT(report != NULL);

    ASSERT(strstr(report, "683 / 683 readable") != NULL);
    ASSERT(strstr(report, "Perfect") != NULL);
    /* the fields that were not measured must say so, in words */
    ASSERT(strstr(report, "corrected: not determined") != NULL);
    /* and the old fabricated line must be gone for good */
    ASSERT(strstr(report, "100 / 100") == NULL);

    free(report);
    uft_smart_close(&res);
}

TEST(the_registry_recognises_more_than_the_local_table) {
    /* MF-444: detection now asks uft_probe_buffer_format() first — 88 plugins
     * instead of the 16-entry table in this file. XFD is one of the 72 the
     * table never listed; before, this call returned "Unknown format". */
    uft_smart_options_t opts;
    uft_smart_options_init(&opts);

    uft_smart_result_t res;
    memset(&res, 0, sizeof(res));
    int rc = uft_smart_open(img("atrcopy_dos2sd.xfd"), &opts, &res);
    ASSERT(rc == 0);
    ASSERT(res.detection.format_name != NULL);
    ASSERT(res.error[0] == '\0');

    uft_smart_close(&res);
}

int main(void)
{
    printf("=== uft_smart_open(): counted, not claimed (MF-443/444) ===\n");
    RUN(an_empty_registry_is_reported_as_such_not_as_unknown_format);
    RUN(registering_twice_is_not_an_error);
    RUN(the_sector_counts_are_read_from_the_disk);
    RUN(a_different_disk_gives_different_numbers);
    RUN(the_d81_is_counted_too_though_it_is_not_gcr);
    RUN(unmeasured_fields_say_so_instead_of_claiming_zero);
    RUN(the_report_prints_the_numbers_the_struct_holds);
    RUN(the_registry_recognises_more_than_the_local_table);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
