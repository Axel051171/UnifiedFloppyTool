/**
 * @file test_xum1541_hal.c
 * @brief Tests for the XUM1541 HAL scaffold (M3.2).
 *
 * Verifies:
 *   (a) Pure utility lookups (drive_name, tracks_for_drive,
 *       sectors_for_track) produce CBM-spec-correct values.
 *   (b) Lifecycle: config_create / destroy work without USB.
 *   (c) Setters validate input ranges (device 8..15, side 0..1, etc).
 *   (d) USB stubs return -1 with a useful error message.
 *
 * When the libusb layer lands, the stub-tests become real-USB-fixture
 * tests. The utility lookups stay as regression guards forever.
 */

#include <stdio.h>
#include <string.h>

#include "uft/hal/uft_xum1541.h"

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-44s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* ───────────────────────── Pure-utility lookups ──────────────────── */

TEST(drive_name_known_drives) {
    ASSERT(strcmp(uft_xum_drive_name(UFT_CBM_DRIVE_1541),    "Commodore 1541")    == 0);
    ASSERT(strcmp(uft_xum_drive_name(UFT_CBM_DRIVE_1541_II), "Commodore 1541-II") == 0);
    ASSERT(strcmp(uft_xum_drive_name(UFT_CBM_DRIVE_1571),    "Commodore 1571")    == 0);
    ASSERT(strcmp(uft_xum_drive_name(UFT_CBM_DRIVE_1581),    "Commodore 1581")    == 0);
    ASSERT(strcmp(uft_xum_drive_name(UFT_CBM_DRIVE_AUTO),    "auto-detect")       == 0);
}

TEST(drive_name_unknown_returns_unknown) {
    ASSERT(strcmp(uft_xum_drive_name((uft_cbm_drive_t)999), "unknown") == 0);
}

TEST(tracks_for_drive_canonical_values) {
    ASSERT(uft_xum_tracks_for_drive(UFT_CBM_DRIVE_1541)    == 35);
    ASSERT(uft_xum_tracks_for_drive(UFT_CBM_DRIVE_1541_II) == 35);
    ASSERT(uft_xum_tracks_for_drive(UFT_CBM_DRIVE_1571)    == 35);
    ASSERT(uft_xum_tracks_for_drive(UFT_CBM_DRIVE_1581)    == 80);
    ASSERT(uft_xum_tracks_for_drive(UFT_CBM_DRIVE_8050)    == 77);
    ASSERT(uft_xum_tracks_for_drive(UFT_CBM_DRIVE_8250)    == 77);
    ASSERT(uft_xum_tracks_for_drive(UFT_CBM_DRIVE_AUTO)    == 0);
}

TEST(sectors_for_track_1541_zones) {
    /* Zone 3: tracks 1..17 → 21 sectors */
    ASSERT(uft_xum_sectors_for_track(UFT_CBM_DRIVE_1541, 1)  == 21);
    ASSERT(uft_xum_sectors_for_track(UFT_CBM_DRIVE_1541, 17) == 21);
    /* Zone 2: tracks 18..24 → 19 sectors */
    ASSERT(uft_xum_sectors_for_track(UFT_CBM_DRIVE_1541, 18) == 19);
    ASSERT(uft_xum_sectors_for_track(UFT_CBM_DRIVE_1541, 24) == 19);
    /* Zone 1: tracks 25..30 → 18 sectors */
    ASSERT(uft_xum_sectors_for_track(UFT_CBM_DRIVE_1541, 25) == 18);
    ASSERT(uft_xum_sectors_for_track(UFT_CBM_DRIVE_1541, 30) == 18);
    /* Zone 0: tracks 31..35 → 17 sectors */
    ASSERT(uft_xum_sectors_for_track(UFT_CBM_DRIVE_1541, 31) == 17);
    ASSERT(uft_xum_sectors_for_track(UFT_CBM_DRIVE_1541, 35) == 17);
    /* Extended tracks 36..42 stay zone 0 */
    ASSERT(uft_xum_sectors_for_track(UFT_CBM_DRIVE_1541, 40) == 17);
    /* Out of range */
    ASSERT(uft_xum_sectors_for_track(UFT_CBM_DRIVE_1541, 0)  == 0);
    ASSERT(uft_xum_sectors_for_track(UFT_CBM_DRIVE_1541, 43) == 0);
}

TEST(sectors_for_track_1581_fixed) {
    /* 1581 uses MFM fixed 40 sectors/track for all 80 tracks. */
    ASSERT(uft_xum_sectors_for_track(UFT_CBM_DRIVE_1581, 1)  == 40);
    ASSERT(uft_xum_sectors_for_track(UFT_CBM_DRIVE_1581, 80) == 40);
    ASSERT(uft_xum_sectors_for_track(UFT_CBM_DRIVE_1581, 81) == 0);
    ASSERT(uft_xum_sectors_for_track(UFT_CBM_DRIVE_1581, 0)  == 0);
}

TEST(sectors_for_track_8050_zones) {
    /* IEEE-488 drive zones across 77 tracks. */
    ASSERT(uft_xum_sectors_for_track(UFT_CBM_DRIVE_8050, 1)  == 29);
    ASSERT(uft_xum_sectors_for_track(UFT_CBM_DRIVE_8050, 39) == 29);
    ASSERT(uft_xum_sectors_for_track(UFT_CBM_DRIVE_8050, 40) == 27);
    ASSERT(uft_xum_sectors_for_track(UFT_CBM_DRIVE_8050, 53) == 27);
    ASSERT(uft_xum_sectors_for_track(UFT_CBM_DRIVE_8050, 54) == 25);
    ASSERT(uft_xum_sectors_for_track(UFT_CBM_DRIVE_8050, 64) == 25);
    ASSERT(uft_xum_sectors_for_track(UFT_CBM_DRIVE_8050, 65) == 23);
    ASSERT(uft_xum_sectors_for_track(UFT_CBM_DRIVE_8050, 77) == 23);
    ASSERT(uft_xum_sectors_for_track(UFT_CBM_DRIVE_8050, 78) == 0);
}

TEST(sectors_for_track_auto_returns_zero) {
    ASSERT(uft_xum_sectors_for_track(UFT_CBM_DRIVE_AUTO, 1) == 0);
}

/* ───────────────────────── Lifecycle / setters ──────────────────── */

TEST(config_create_and_destroy) {
    uft_xum_config_t *cfg = uft_xum_config_create();
    ASSERT(cfg != NULL);
    ASSERT(uft_xum_is_connected(cfg) == false);
    uft_xum_config_destroy(cfg);
    /* No way to check post-destroy directly — must not crash, and
     * NULL-input must be safe. */
    uft_xum_config_destroy(NULL);
}

TEST(set_device_validates_iec_range) {
    uft_xum_config_t *cfg = uft_xum_config_create();
    ASSERT(cfg != NULL);
    ASSERT(uft_xum_set_device(cfg, 8)  == 0);   /* min IEC */
    ASSERT(uft_xum_set_device(cfg, 15) == 0);   /* max IEC */
    ASSERT(uft_xum_set_device(cfg, 7)  == -1);  /* below */
    ASSERT(uft_xum_set_device(cfg, 16) == -1);  /* above */
    ASSERT(uft_xum_set_device(NULL, 8) == -1);  /* NULL ctx */
    uft_xum_config_destroy(cfg);
}

TEST(set_track_range_validates) {
    uft_xum_config_t *cfg = uft_xum_config_create();
    ASSERT(uft_xum_set_track_range(cfg, 1, 35)  == 0);
    ASSERT(uft_xum_set_track_range(cfg, 1, 80)  == 0);
    ASSERT(uft_xum_set_track_range(cfg, 0, 35)  == -1);  /* start < 1 */
    ASSERT(uft_xum_set_track_range(cfg, 35, 1)  == -1);  /* end < start */
    ASSERT(uft_xum_set_track_range(cfg, 1, 81)  == -1);  /* end > 80 */
    uft_xum_config_destroy(cfg);
}

TEST(set_side_validates) {
    uft_xum_config_t *cfg = uft_xum_config_create();
    ASSERT(uft_xum_set_side(cfg, 0) == 0);
    ASSERT(uft_xum_set_side(cfg, 1) == 0);
    ASSERT(uft_xum_set_side(cfg, 2) == -1);
    ASSERT(uft_xum_set_side(cfg, -1) == -1);
    uft_xum_config_destroy(cfg);
}

TEST(set_retries_validates) {
    uft_xum_config_t *cfg = uft_xum_config_create();
    ASSERT(uft_xum_set_retries(cfg, 0)   == 0);   /* zero retries OK */
    ASSERT(uft_xum_set_retries(cfg, 100) == 0);
    ASSERT(uft_xum_set_retries(cfg, -1)  == -1);
    ASSERT(uft_xum_set_retries(cfg, 101) == -1);
    uft_xum_config_destroy(cfg);
}

/* ───────────────────────── USB stubs return error ────────────────── */

TEST(open_returns_minus_one_with_error_msg) {
    uft_xum_config_t *cfg = uft_xum_config_create();
    ASSERT(uft_xum_open(cfg, 8) == -1);
    /* error string set */
    const char *err = uft_xum_get_error(cfg);
    ASSERT(err != NULL);
    ASSERT(strstr(err, "libusb") != NULL);
    uft_xum_config_destroy(cfg);
}

TEST(read_track_gcr_stub_rejects_invalid_args) {
    uft_xum_config_t *cfg = uft_xum_config_create();
    uint8_t *gcr = NULL;
    size_t size = 0;
    /* track < 1 */
    ASSERT(uft_xum_read_track_gcr(cfg, 0, 0, &gcr, &size) == -1);
    /* side > 1 */
    ASSERT(uft_xum_read_track_gcr(cfg, 1, 2, &gcr, &size) == -1);
    /* NULL ctx / out params */
    ASSERT(uft_xum_read_track_gcr(NULL, 1, 0, &gcr, &size) == -1);
    ASSERT(uft_xum_read_track_gcr(cfg, 1, 0, NULL, &size)  == -1);
    ASSERT(uft_xum_read_track_gcr(cfg, 1, 0, &gcr, NULL)   == -1);
    uft_xum_config_destroy(cfg);
}

TEST(iec_commands_validate_address_range) {
    uft_xum_config_t *cfg = uft_xum_config_create();
    ASSERT(uft_xum_iec_listen(cfg, -1, 0) == -1);
    ASSERT(uft_xum_iec_listen(cfg, 32, 0) == -1);
    ASSERT(uft_xum_iec_talk(cfg, 8, 32)   == -1);
    /* Even valid args still return -1 because USB layer is stubbed —
     * but the error message must be "not implemented" not "out of range" */
    ASSERT(uft_xum_iec_listen(cfg, 8, 0) == -1);
    ASSERT(strstr(uft_xum_get_error(cfg), "not implemented") != NULL);
    uft_xum_config_destroy(cfg);
}

TEST(get_error_handles_null) {
    const char *err = uft_xum_get_error(NULL);
    ASSERT(err != NULL);
    ASSERT(strstr(err, "NULL") != NULL);
}

int main(void) {
    printf("=== XUM1541 HAL scaffold tests ===\n");
    RUN(drive_name_known_drives);
    RUN(drive_name_unknown_returns_unknown);
    RUN(tracks_for_drive_canonical_values);
    RUN(sectors_for_track_1541_zones);
    RUN(sectors_for_track_1581_fixed);
    RUN(sectors_for_track_8050_zones);
    RUN(sectors_for_track_auto_returns_zero);
    RUN(config_create_and_destroy);
    RUN(set_device_validates_iec_range);
    RUN(set_track_range_validates);
    RUN(set_side_validates);
    RUN(set_retries_validates);
    RUN(open_returns_minus_one_with_error_msg);
    RUN(read_track_gcr_stub_rejects_invalid_args);
    RUN(iec_commands_validate_address_range);
    RUN(get_error_handles_null);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
