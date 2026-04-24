/**
 * @file test_display_track.c
 * @brief Smoke tests for uft_display_track (restored from v3.7.0).
 */

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "uft/display/uft_display_track.h"

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-38s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

static uft_display_track_info_t make_sample_track(void) {
    uft_display_track_info_t t = {0};
    t.cylinder = 5;
    t.head = 0;
    t.track_length = 6250;
    t.sector_count = 11;
    for (int i = 0; i < 11; i++) {
        t.sectors[i].sector_id = i + 1;
        t.sectors[i].cylinder = 5;
        t.sectors[i].head = 0;
        t.sectors[i].data_size = 512;
        t.sectors[i].valid = true;
        t.sectors[i].id_crc_ok = true;
        t.sectors[i].data_crc_ok = (i != 3);  /* sector 4 has bad CRC */
        t.sectors[i].deleted = false;
        t.sectors[i].weak = (i == 7);          /* sector 8 weak */
    }
    return t;
}

TEST(track_map_produces_output) {
    uft_display_track_info_t t = make_sample_track();
    char buf[4096];
    int rc = uft_display_track_map(&t, buf, sizeof(buf));
    ASSERT(rc >= 0);            /* 0 or byte count; negative = error */
    ASSERT(strlen(buf) > 0);
}

TEST(sector_table_produces_output) {
    uft_display_track_info_t t = make_sample_track();
    char buf[4096];
    int rc = uft_display_sector_table(&t, buf, sizeof(buf));
    ASSERT(rc >= 0);            /* 0 or byte count; negative = error */
    ASSERT(strlen(buf) > 0);
}

TEST(svg_track_contains_svg_tag) {
    uft_display_track_info_t t = make_sample_track();
    char buf[8192];
    int rc = uft_display_svg_track(&t, buf, sizeof(buf));
    ASSERT(rc >= 0);
    ASSERT(strstr(buf, "svg") != NULL || strstr(buf, "SVG") != NULL);
}

TEST(color_track_no_color_mode) {
    uft_display_track_info_t t = make_sample_track();
    char buf[4096];
    int rc = uft_display_color_track(&t, buf, sizeof(buf), false);
    ASSERT(rc >= 0);            /* 0 or byte count; negative = error */
    ASSERT(strlen(buf) > 0);
}

TEST(flux_histogram_with_sample_data) {
    uint32_t flux_times[100];
    for (int i = 0; i < 100; i++) flux_times[i] = 2000 + (i % 10) * 100;
    char buf[4096];
    int rc = uft_display_flux_histogram(flux_times, 100, 1e9, buf, sizeof(buf));
    ASSERT(rc >= 0);            /* 0 or byte count; negative = error */
    ASSERT(strlen(buf) > 0);
}

TEST(disk_view_renders_grid) {
    int status[4 * 2] = { 1, 1, 1, 0, -1, 1, 1, 1 };  /* mixed */
    uft_display_disk_info_t d = {
        .name = "TestDisk",
        .tracks = 4, .sides = 2, .sectors_per_track = 11,
        .track_status = status,
    };
    char buf[4096];
    int rc = uft_display_disk_view(&d, buf, sizeof(buf));
    ASSERT(rc >= 0);            /* 0 or byte count; negative = error */
    ASSERT(strlen(buf) > 0);
}

TEST(html_report_contains_html) {
    int status[2 * 2] = { 1, 1, 1, 1 };
    uft_display_disk_info_t d = {
        .name = "HTMLTest",
        .tracks = 2, .sides = 2, .sectors_per_track = 11,
        .track_status = status,
    };
    char buf[16384];
    int rc = uft_display_html_report(&d, buf, sizeof(buf));
    ASSERT(rc >= 0);
    ASSERT(strstr(buf, "html") != NULL || strstr(buf, "HTML") != NULL);
}

TEST(buffer_too_small_rejected) {
    uft_display_track_info_t t = make_sample_track();
    char tiny[4];
    int rc = uft_display_track_map(&t, tiny, sizeof(tiny));
    ASSERT(rc != 0);
}

int main(void) {
    printf("=== uft_display_track smoke tests ===\n");
    RUN(track_map_produces_output);
    RUN(sector_table_produces_output);
    RUN(svg_track_contains_svg_tag);
    RUN(color_track_no_color_mode);
    RUN(flux_histogram_with_sample_data);
    RUN(disk_view_renders_grid);
    RUN(html_report_contains_html);
    RUN(buffer_too_small_rejected);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
