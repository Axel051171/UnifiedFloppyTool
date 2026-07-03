/**
 * @file tests/emulators/kryoflux/test_kryoflux_emulator.c
 * @brief Firmware-realistic tests for the KryoFlux emulator (5/9).
 *
 * The distinguishing feature of this emulator: the synthetic RAW streams
 * it produces are decoded by the PRODUCTION parser uft_kf_decode()
 * (src/flux/uft_kryoflux_stream.c), and every container-level defect
 * class is asserted to yield the exact uft_kf_status_t the real parser
 * returns. That makes the generator a live conformance fixture for the
 * stream decoder, not just a self-consistent mock.
 *
 * Six groups:
 *   A. DTC device lifecycle & detect
 *   B. Capture exit codes (device/disk/index/args)
 *   C. Write refusal + retry model
 *   D. Flux opcode encoding boundaries (white-box)
 *   E. Clean stream → production parser round-trip
 *   F. Defect classes → parser status + HAL SSOT cross-check
 */

#include "firmware_state_machine.h"
#include "flux_gen.h"
#include "uft/flux/uft_kryoflux.h"
#include "uft/hal/uft_kryoflux.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Two-sided float compare — avoids a libm (fabs) dependency so the test
 * links cleanly under CI's LINK_LIBRARIES_ONLY_TARGETS (MF-305). */
static int approx(double a, double b, double eps) {
    double d = a - b;
    return d < eps && d > -eps;
}

static int g_pass = 0, g_fail = 0;
#define ASSERT(cond) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  [FAIL] %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
} while (0)
#define TEST(name) printf("  [TEST] %-58s ... ", name)
#define DONE()     printf("OK\n")

/* Decode a generated stream with the production parser; return status
 * and (out) the decoded flux/index counts + avg RPM. */
static uft_kf_status_t decode(const uint8_t *bytes, size_t len,
                              uint32_t *flux_count, uint32_t *index_count,
                              double *avg_rpm) {
    uft_kf_stream_t s;
    if (uft_kf_init(&s) != UFT_UFT_KF_STATUS_OK) return UFT_UFT_KF_STATUS_READ_ERROR;
    uft_kf_status_t st = uft_kf_decode(&s, bytes, len);
    if (flux_count)  *flux_count  = s.flux_count;
    if (index_count) *index_count = s.index_count;
    if (avg_rpm)     *avg_rpm     = s.stats.avg_rpm;
    uft_kf_free(&s);
    return st;
}

/* ─── A. DTC lifecycle ──────────────────────────────────────────────── */

static void group_a(void) {
    printf("\n-- A. DTC device lifecycle & detect --\n");
    kf_dtc_t dev;

    TEST("reset_is_disconnected");
    kf_dtc_reset(&dev);
    ASSERT(dev.state == KF_DTC_STATE_DISCONNECTED);
    ASSERT(dev.max_track == UFT_KF_GEN_MAX_TRACK);
    DONE();

    TEST("detect_no_device_is_zero");
    ASSERT(kf_dtc_detect(&dev) == 0);
    ASSERT(dev.last_exit == KF_DTC_EXIT_NO_DEVICE);
    DONE();

    TEST("power_on_defaults_detects_one");
    kf_dtc_power_on_defaults(&dev);
    ASSERT(kf_dtc_detect(&dev) == 1);
    ASSERT(dev.state == KF_DTC_STATE_READY);
    DONE();
}

/* ─── B. Capture exit codes ─────────────────────────────────────────── */

static void group_b(void) {
    printf("\n-- B. Capture exit codes --\n");
    kf_dtc_t dev;
    kf_dtc_request_t req = { .track = 0, .side = 0, .revolutions = 2,
                             .double_step = false, .is_write = false };
    uint8_t *stream = NULL; size_t len = 0;

    TEST("clean_capture_succeeds");
    kf_dtc_power_on_defaults(&dev);
    ASSERT(kf_dtc_capture(&dev, &req, 3, &stream, &len) == KF_DTC_EXIT_OK);
    ASSERT(stream != NULL && len > 0);
    ASSERT(dev.captures_ok == 1);
    free(stream); stream = NULL;
    DONE();

    TEST("no_device_fails_empty");
    kf_dtc_set_device_present(&dev, false);
    ASSERT(kf_dtc_capture(&dev, &req, 3, &stream, &len) == KF_DTC_EXIT_NO_DEVICE);
    ASSERT(stream == NULL && len == 0);   /* never a short read */
    DONE();

    TEST("no_disk_fails");
    kf_dtc_power_on_defaults(&dev);
    kf_dtc_set_disk_present(&dev, false);
    ASSERT(kf_dtc_capture(&dev, &req, 3, &stream, &len) == KF_DTC_EXIT_NO_DISK);
    DONE();

    TEST("no_index_fails");
    kf_dtc_power_on_defaults(&dev);
    kf_dtc_set_index_present(&dev, false);
    ASSERT(kf_dtc_capture(&dev, &req, 3, &stream, &len) == KF_DTC_EXIT_NO_INDEX);
    DONE();

    TEST("out_of_range_track_is_bad_args");
    kf_dtc_power_on_defaults(&dev);
    kf_dtc_request_t bad = req; bad.track = 999;
    ASSERT(kf_dtc_capture(&dev, &bad, 3, &stream, &len) == KF_DTC_EXIT_BAD_ARGS);
    bad = req; bad.revolutions = 99;
    ASSERT(kf_dtc_capture(&dev, &bad, 3, &stream, &len) == KF_DTC_EXIT_BAD_ARGS);
    DONE();
}

/* ─── C. Write refusal + retry ──────────────────────────────────────── */

static void group_c(void) {
    printf("\n-- C. Write refusal + retry model --\n");
    kf_dtc_t dev;
    uint8_t *stream = NULL; size_t len = 0;

    TEST("write_always_refused");
    kf_dtc_power_on_defaults(&dev);
    kf_dtc_request_t w = { .track = 0, .side = 0, .revolutions = 1,
                           .double_step = false, .is_write = true };
    ASSERT(kf_dtc_capture(&dev, &w, 3, &stream, &len) == KF_DTC_EXIT_WRITE_DENY);
    ASSERT(stream == NULL);
    DONE();

    TEST("transient_io_recovers_within_retries");
    kf_dtc_power_on_defaults(&dev);
    kf_dtc_set_fail_next(&dev, 2);   /* 2 transient failures */
    kf_dtc_request_t r = { .track = 5, .side = 0, .revolutions = 1,
                           .double_step = false, .is_write = false };
    ASSERT(kf_dtc_capture(&dev, &r, 3, &stream, &len) == KF_DTC_EXIT_OK);
    ASSERT(dev.retries_used == 2);
    free(stream); stream = NULL;
    DONE();

    TEST("retry_exhaustion_is_io_error");
    kf_dtc_power_on_defaults(&dev);
    kf_dtc_set_fail_next(&dev, 5);   /* more than max_retries */
    ASSERT(kf_dtc_capture(&dev, &r, 3, &stream, &len) == KF_DTC_EXIT_IO);
    ASSERT(stream == NULL);
    DONE();
}

/* ─── D. Flux opcode encoding ───────────────────────────────────────── */

static void group_d(void) {
    printf("\n-- D. Flux opcode encoding boundaries --\n");
    uint8_t b[16];

    TEST("flux1_single_byte_range");
    size_t pos = 0;
    ASSERT(uft_kf_gen_encode_flux(b, sizeof(b), &pos, 0x60) == 1);
    ASSERT(b[0] == 0x60 && pos == 1);
    DONE();

    TEST("small_value_uses_flux2_not_opcode");
    pos = 0;
    /* value 5 < 0x0E cannot be a bare byte (0x05 is a Nop/opcode) → Flux2 */
    ASSERT(uft_kf_gen_encode_flux(b, sizeof(b), &pos, 5) == 2);
    ASSERT(b[0] == 0x00 && b[1] == 0x05);
    DONE();

    TEST("flux2_two_byte_range");
    pos = 0;
    ASSERT(uft_kf_gen_encode_flux(b, sizeof(b), &pos, 0x0300) == 2);
    ASSERT(b[0] == 0x03 && b[1] == 0x00);
    DONE();

    TEST("flux3_for_large_value");
    pos = 0;
    ASSERT(uft_kf_gen_encode_flux(b, sizeof(b), &pos, 0x8000) == 3);
    ASSERT(b[0] == 0x0C && b[1] == 0x80 && b[2] == 0x00);
    DONE();

    TEST("ovl16_prefixes_huge_value");
    pos = 0;
    /* 0x10005 = one Ovl16 + residual 5 (Flux2). */
    ASSERT(uft_kf_gen_encode_flux(b, sizeof(b), &pos, 0x10005) == 3);
    ASSERT(b[0] == 0x0B && b[1] == 0x00 && b[2] == 0x05);
    DONE();
}

/* ─── E. Clean stream → production parser ───────────────────────────── */

static void group_e(void) {
    printf("\n-- E. Clean stream → production uft_kf_decode() --\n");
    uft_kf_gen_params_t p = { .seed = 0xBEE5, .track = 17, .side = 0,
                              .revolutions = 2, .cell_ns = 0,
                              .defects = UFT_KF_DEFECT_NONE, .weak_jitter_pct = 0 };
    uft_kf_gen_stream_t s;

    TEST("clean_stream_generates_ok");
    ASSERT(uft_kf_gen_stream(&p, &s) == UFT_KF_GEN_OK);
    ASSERT(s.bytes_len > 0 && s.flux_count > 0);
    ASSERT(s.index_count == 2);
    DONE();

    TEST("production_parser_decodes_clean_as_ok");
    uint32_t fc = 0, ic = 0; double rpm = 0;
    uft_kf_status_t st = decode(s.bytes, s.bytes_len, &fc, &ic, &rpm);
    ASSERT(st == UFT_UFT_KF_STATUS_OK);   /* StreamEnd+EOF present */
    DONE();

    TEST("decoded_flux_count_matches_generated");
    ASSERT(fc == s.flux_count);
    DONE();

    TEST("decoded_index_count_matches");
    ASSERT(ic == 2);
    DONE();

    TEST("decoded_rpm_near_300");
    ASSERT(rpm > 280.0 && rpm < 320.0);
    DONE();

    TEST("clean_stream_is_medium_safe");
    ASSERT(uft_kf_gen_count_unsafe(&s) == 0);
    DONE();

    TEST("clean_stream_is_deterministic");
    uft_kf_gen_stream_t s2;
    uft_kf_gen_stream(&p, &s2);
    ASSERT(s2.bytes_len == s.bytes_len);
    ASSERT(memcmp(s.bytes, s2.bytes, s.bytes_len) == 0);
    uft_kf_gen_free(&s2);
    DONE();

    uft_kf_gen_free(&s);
}

/* ─── F. Defect → parser status + HAL SSOT ──────────────────────────── */

static void gen_and_decode(uint32_t defect, uft_kf_status_t *st,
                           uint32_t *index_count) {
    uft_kf_gen_params_t p = { .seed = 0x1357, .track = 3, .side = 0,
                              .revolutions = 2, .cell_ns = 0,
                              .defects = defect, .weak_jitter_pct = 8 };
    uft_kf_gen_stream_t s;
    if (uft_kf_gen_stream(&p, &s) != UFT_KF_GEN_OK) { *st = UFT_UFT_KF_STATUS_READ_ERROR; return; }
    *st = decode(s.bytes, s.bytes_len, NULL, index_count, NULL);
    uft_kf_gen_free(&s);
}

static void group_f(void) {
    printf("\n-- F. Defect classes → parser status + HAL SSOT --\n");

    TEST("hal_ssot_sample_clock_agrees");
    ASSERT(approx(uft_kf_get_sample_clock(), UFT_KF_GEN_SAMPLE_HZ, 1.0));
    DONE();

    TEST("missing_end_maps_to_MISSING_END");
    uft_kf_status_t st; uint32_t ic;
    gen_and_decode(UFT_KF_DEFECT_MISSING_END, &st, &ic);
    ASSERT(st == UFT_UFT_KF_STATUS_MISSING_END);
    DONE();

    TEST("dev_no_index_maps_to_DEV_INDEX");
    gen_and_decode(UFT_KF_DEFECT_DEV_NO_INDEX, &st, &ic);
    ASSERT(st == UFT_UFT_KF_STATUS_DEV_INDEX);
    DONE();

    TEST("dev_buffer_maps_to_DEV_BUFFER");
    gen_and_decode(UFT_KF_DEFECT_DEV_BUFFER, &st, &ic);
    ASSERT(st == UFT_UFT_KF_STATUS_DEV_BUFFER);
    DONE();

    TEST("no_index_block_decodes_but_zero_indexes");
    gen_and_decode(UFT_KF_DEFECT_NO_INDEX_BLK, &st, &ic);
    /* Still a clean container (StreamEnd present) → OK, but no Index. */
    ASSERT(st == UFT_UFT_KF_STATUS_OK);
    ASSERT(ic == 0);
    DONE();

    TEST("truncated_stream_is_not_ok");
    gen_and_decode(UFT_KF_DEFECT_TRUNCATED, &st, &ic);
    /* Cut before terminators → MISSING_END (or MISSING_DATA on a
     * chopped multi-byte opcode). Never OK. */
    ASSERT(st == UFT_UFT_KF_STATUS_MISSING_END ||
           st == UFT_UFT_KF_STATUS_MISSING_DATA);
    DONE();

    TEST("weak_bits_still_medium_safe");
    uft_kf_gen_params_t p = { .seed = 9, .track = 3, .side = 0,
                              .revolutions = 1, .cell_ns = 0,
                              .defects = UFT_KF_DEFECT_WEAK_BITS,
                              .weak_jitter_pct = 10 };
    uft_kf_gen_stream_t s;
    ASSERT(uft_kf_gen_stream(&p, &s) == UFT_KF_GEN_OK);
    ASSERT(uft_kf_gen_count_unsafe(&s) == 0);
    uft_kf_gen_free(&s);
    DONE();

    TEST("out_of_spec_revs_refused");
    uft_kf_gen_params_t pb = p; pb.revolutions = 99;
    uft_kf_gen_stream_t sb;
    ASSERT(uft_kf_gen_stream(&pb, &sb) == UFT_KF_GEN_ERR_OUT_OF_SPEC);
    DONE();
}

int main(void) {
    printf("=== KryoFlux firmware-realistic emulator tests ===\n");
    group_a();
    group_b();
    group_c();
    group_d();
    group_e();
    group_f();
    printf("\nResults: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
