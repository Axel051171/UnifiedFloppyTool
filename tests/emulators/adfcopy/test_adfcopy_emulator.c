/**
 * @file tests/emulators/adfcopy/test_adfcopy_emulator.c
 * @brief Firmware-realistic tests for the ADFCopy emulator (8/9).
 *
 * ADFCopy is a Teensy-based binary-serial controller for Amiga 3.5"
 * drives. The distinctive surface (vs Applesauce's ASCII) is the BINARY
 * command protocol: 1-byte command + payload, 1-byte 'O'/'E'/'D'
 * response, a status bitmask, and the CMD_READ_FLUX 3-byte-header + flux
 * payload.
 *
 * Six groups:
 *   A. lifecycle & CMD_INIT
 *   B. CMD_SEEK range + ordering
 *   C. CMD_GET_STATUS bitmask
 *   D. CMD_READ_FLUX header + payload
 *   E. read-only / no-disk / error visibility (forensic)
 *   F. flux structure, defects, medium-safety, determinism
 */

#include "firmware_state_machine.h"
#include "flux_gen.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_pass = 0, g_fail = 0;
#define ASSERT(cond) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  [FAIL] %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
} while (0)
#define TEST(name) printf("  [TEST] %-58s ... ", name)
#define DONE()     printf("OK\n")

/* ─── A. lifecycle & INIT ───────────────────────────────────────────── */

static void group_a(void) {
    printf("\n-- A. lifecycle & CMD_INIT --\n");
    adfc_dev_t dev;

    TEST("reset_is_idle");
    adfc_reset(&dev);
    ASSERT(dev.state == ADFC_STATE_IDLE);
    ASSERT(dev.current_track == -1);
    ASSERT(!dev.motor_on);
    DONE();

    TEST("init_spins_motor_homes_head");
    adfc_power_on_defaults(&dev);
    ASSERT(adfc_cmd_init(&dev) == ADFC_RSP_OK);
    ASSERT(dev.state == ADFC_STATE_READY);
    ASSERT(dev.motor_on && dev.current_track == 0);
    DONE();

    TEST("init_no_disk_answers_D");
    adfc_reset(&dev);
    adfc_set_disk_present(&dev, false);
    ASSERT(adfc_cmd_init(&dev) == ADFC_RSP_NODISK);
    ASSERT(dev.state != ADFC_STATE_READY);
    DONE();
}

/* ─── B. CMD_SEEK ───────────────────────────────────────────────────── */

static void group_b(void) {
    printf("\n-- B. CMD_SEEK range + ordering --\n");
    adfc_dev_t dev;

    TEST("seek_before_init_is_error");
    adfc_power_on_defaults(&dev);
    ASSERT(adfc_cmd_seek(&dev, 10) == ADFC_RSP_ERROR);
    DONE();

    TEST("seek_in_range_ok");
    adfc_cmd_init(&dev);
    ASSERT(adfc_cmd_seek(&dev, 42) == ADFC_RSP_OK);
    ASSERT(dev.current_track == 42);
    DONE();

    TEST("seek_max_track_ok");
    ASSERT(adfc_cmd_seek(&dev, UFT_ADFC_MAX_TRACK) == ADFC_RSP_OK);
    DONE();

    TEST("seek_out_of_range_is_error");
    ASSERT(adfc_cmd_seek(&dev, 200) == ADFC_RSP_ERROR);
    ASSERT(adfc_cmd_seek(&dev, -1) == ADFC_RSP_ERROR);
    ASSERT(dev.current_track == UFT_ADFC_MAX_TRACK);   /* unchanged */
    DONE();
}

/* ─── C. CMD_GET_STATUS ─────────────────────────────────────────────── */

static void group_c(void) {
    printf("\n-- C. CMD_GET_STATUS bitmask --\n");
    adfc_dev_t dev;

    TEST("status_disk_and_flux_capable");
    adfc_power_on_defaults(&dev);
    uint8_t s = adfc_cmd_get_status(&dev);
    ASSERT((s & ADFC_STATUS_DISK) != 0);
    ASSERT((s & ADFC_STATUS_FLUX) != 0);
    ASSERT((s & ADFC_STATUS_MOTOR) == 0);   /* motor off before INIT */
    DONE();

    TEST("status_motor_bit_after_init");
    adfc_cmd_init(&dev);
    s = adfc_cmd_get_status(&dev);
    ASSERT((s & ADFC_STATUS_MOTOR) != 0);
    DONE();

    TEST("status_write_protect_bit");
    adfc_power_on_defaults(&dev);
    adfc_set_write_protected(&dev, true);
    s = adfc_cmd_get_status(&dev);
    ASSERT((s & ADFC_STATUS_WPROT) != 0);
    DONE();

    TEST("status_no_disk_clears_disk_bit");
    adfc_power_on_defaults(&dev);
    adfc_set_disk_present(&dev, false);
    s = adfc_cmd_get_status(&dev);
    ASSERT((s & ADFC_STATUS_DISK) == 0);
    DONE();
}

/* ─── D. CMD_READ_FLUX ──────────────────────────────────────────────── */

static void group_d(void) {
    printf("\n-- D. CMD_READ_FLUX header + payload --\n");
    adfc_dev_t dev;
    uint8_t *reply = NULL; size_t len = 0;

    TEST("read_flux_returns_O_and_payload");
    adfc_power_on_defaults(&dev);
    adfc_cmd_init(&dev);
    ASSERT(adfc_cmd_read_flux(&dev, 20, 2, &reply, &len) == ADFC_RSP_OK);
    ASSERT(reply != NULL && len > 3);
    DONE();

    TEST("header_status_ok_and_length_matches");
    ASSERT(reply[0] == UFT_ADFC_FLUX_OK);
    uint16_t hdr_len = (uint16_t)(reply[1] | (reply[2] << 8));
    ASSERT((size_t)hdr_len == len - 3);   /* LE16 length == flux byte count */
    free(reply); reply = NULL;
    DONE();

    TEST("read_flux_before_init_is_error");
    adfc_power_on_defaults(&dev);   /* not initialised */
    ASSERT(adfc_cmd_read_flux(&dev, 20, 1, &reply, &len) == ADFC_RSP_ERROR);
    ASSERT(reply == NULL);
    DONE();

    TEST("read_flux_bad_track_is_error");
    adfc_power_on_defaults(&dev); adfc_cmd_init(&dev);
    ASSERT(adfc_cmd_read_flux(&dev, 999, 1, &reply, &len) == ADFC_RSP_ERROR);
    ASSERT(adfc_cmd_read_flux(&dev, 20, 99, &reply, &len) == ADFC_RSP_ERROR);
    DONE();
}

/* ─── E. forensic: no-disk, read-only, visibility ───────────────────── */

static void group_e(void) {
    printf("\n-- E. no-disk / read-only / error visibility --\n");
    adfc_dev_t dev;
    uint8_t *reply = NULL; size_t len = 0;

    TEST("read_flux_no_disk_returns_D_header_no_flux");
    adfc_power_on_defaults(&dev);
    adfc_cmd_init(&dev);
    adfc_set_disk_present(&dev, false);
    ASSERT(adfc_cmd_read_flux(&dev, 20, 1, &reply, &len) == ADFC_RSP_NODISK);
    /* 3-byte header, no-disk status, zero flux. */
    ASSERT(reply != NULL && len == 3);
    ASSERT(reply[0] == UFT_ADFC_FLUX_NODISK);
    ASSERT(reply[1] == 0 && reply[2] == 0);
    free(reply); reply = NULL;
    DONE();

    TEST("errors_are_visible_never_silent");
    /* Every failure path returns a distinct non-'O' byte. */
    adfc_reset(&dev);
    adfc_set_disk_present(&dev, false);
    ASSERT(adfc_cmd_init(&dev) == ADFC_RSP_NODISK);
    adfc_power_on_defaults(&dev);
    ASSERT(adfc_cmd_seek(&dev, 5) == ADFC_RSP_ERROR);  /* before init */
    DONE();

    TEST("write_prot_disk_still_reads");
    /* Read-only tool: write-protect does not block reading (forensic
     * capture never writes). */
    adfc_power_on_defaults(&dev);
    adfc_set_write_protected(&dev, true);
    adfc_cmd_init(&dev);
    ASSERT(adfc_cmd_read_flux(&dev, 10, 1, &reply, &len) == ADFC_RSP_OK);
    free(reply); reply = NULL;
    DONE();
}

/* ─── F. flux structure, defects, determinism ───────────────────────── */

static void group_f(void) {
    printf("\n-- F. flux structure + defects --\n");

    TEST("clean_flux_generates_and_is_medium_safe");
    uft_adfc_gen_params_t p = { .seed = 0xBEEF, .track = 20, .revolutions = 2,
                                .defects = UFT_ADFC_DEFECT_NONE,
                                .weak_jitter_pct = 0 };
    uft_adfc_gen_flux_t f;
    ASSERT(uft_adfc_gen_flux(&p, &f) == UFT_ADFC_GEN_OK);
    ASSERT(f.flux_len > 0 && f.status == UFT_ADFC_FLUX_OK);
    ASSERT(uft_adfc_gen_count_unsafe(&f) == 0);
    DONE();

    TEST("clean_flux_is_deterministic");
    uft_adfc_gen_flux_t f2;
    uft_adfc_gen_flux(&p, &f2);
    ASSERT(f2.bytes_len == f.bytes_len);
    ASSERT(memcmp(f.bytes, f2.bytes, f.bytes_len) == 0);
    uft_adfc_gen_free(&f2);
    DONE();

    TEST("weak_bits_stay_medium_safe");
    uft_adfc_gen_params_t pw = p;
    pw.defects = UFT_ADFC_DEFECT_WEAK_BITS; pw.weak_jitter_pct = 10;
    uft_adfc_gen_flux_t w;
    ASSERT(uft_adfc_gen_flux(&pw, &w) == UFT_ADFC_GEN_OK);
    ASSERT(uft_adfc_gen_count_unsafe(&w) == 0);
    uft_adfc_gen_free(&w);
    DONE();

    TEST("short_defect_yields_fewer_flux_bytes");
    uft_adfc_gen_params_t ps = p;
    ps.defects = UFT_ADFC_DEFECT_SHORT;
    uft_adfc_gen_flux_t sh;
    ASSERT(uft_adfc_gen_flux(&ps, &sh) == UFT_ADFC_GEN_OK);
    ASSERT(sh.flux_len < f.flux_len);
    uft_adfc_gen_free(&sh);
    DONE();

    TEST("sample_clock_is_40mhz_25ns");
    ASSERT(UFT_ADFC_SAMPLE_HZ == 40000000u);
    ASSERT(UFT_ADFC_TICK_NS == 25u);
    DONE();

    TEST("out_of_spec_track_refused");
    uft_adfc_gen_params_t pb = p; pb.track = 999;
    uft_adfc_gen_flux_t fb;
    ASSERT(uft_adfc_gen_flux(&pb, &fb) == UFT_ADFC_GEN_ERR_OUT_OF_SPEC);
    DONE();

    uft_adfc_gen_free(&f);
}

int main(void) {
    printf("=== ADFCopy firmware-realistic emulator tests ===\n");
    group_a();
    group_b();
    group_c();
    group_d();
    group_e();
    group_f();
    printf("\nResults: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
