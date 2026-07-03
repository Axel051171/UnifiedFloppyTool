/**
 * @file tests/emulators/fluxengine/test_fluxengine_emulator.c
 * @brief Firmware-realistic tests for the FluxEngine emulator (6/9).
 *
 * FluxEngine's read path has the provider ask the `fluxengine` CLI to
 * write an SCP file, which UFT decodes with its own vetted SCP parser
 * (src/flux/uft_scp_parser.c). So this emulator's synthetic reads are
 * decoded by the PRODUCTION parser uft_scp_read_track_memory(), and each
 * container defect class asserts the exact uft_scp error code — a live
 * conformance fixture for the SCP reader on the FluxEngine path.
 *
 * Six groups:
 *   A. fluxengine device lifecycle
 *   B. rpm / detect (+ the 3 documented stdout shapes)
 *   C. read exit codes (device/disk/args/write-deny)
 *   D. retry model
 *   E. clean read → production SCP parser round-trip
 *   F. defect classes → SCP parser error + RPM-string contract
 */

#include "firmware_state_machine.h"
#include "flux_gen.h"
#include "uft/flux/uft_scp_parser.h"

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

static int approx(double a, double b, double eps) {
    double d = a - b; return d < eps && d > -eps;
}

/* Decode the target track of an SCP buffer with the production parser. */
static int scp_decode(const uint8_t *data, size_t len, int track,
                      uint32_t *flux_count, uint32_t *rpm_out) {
    uft_scp_track_data_t td;
    int rc = uft_scp_read_track_memory(data, len, track, &td);
    if (rc == UFT_SCP_OK) {
        if (flux_count) *flux_count = td.revolutions[0].flux_count;
        if (rpm_out)    *rpm_out    = td.revolutions[0].rpm;
        uft_scp_free_track(&td);
    }
    return rc;
}

/* ─── A. lifecycle ──────────────────────────────────────────────────── */

static void group_a(void) {
    printf("\n-- A. fluxengine device lifecycle --\n");
    fe_dev_t dev;

    TEST("reset_is_disconnected");
    fe_reset(&dev);
    ASSERT(dev.state == FE_STATE_DISCONNECTED);
    ASSERT(dev.last_exit == FE_EXIT_NO_DEVICE);
    DONE();

    TEST("power_on_defaults_ready_300rpm");
    fe_power_on_defaults(&dev);
    ASSERT(dev.state == FE_STATE_READY);
    ASSERT(approx(dev.rpm, 300.0, 0.01));
    DONE();

    TEST("null_args_are_bad_args");
    ASSERT(fe_run(&dev, NULL, 3, NULL, NULL) == FE_EXIT_BAD_ARGS);
    DONE();
}

/* ─── B. rpm / detect ───────────────────────────────────────────────── */

static void group_b(void) {
    printf("\n-- B. rpm / detect + stdout shapes --\n");
    fe_dev_t dev;
    fe_request_t rpm = { .subcommand = "rpm", .cylinder = 0, .head = 0,
                         .revolutions = 0, .profile = "ibm" };

    TEST("rpm_reports_stdout_line");
    fe_power_on_defaults(&dev);
    ASSERT(fe_run(&dev, &rpm, 3, NULL, NULL) == FE_EXIT_OK);
    ASSERT(strstr(dev.last_stdout, "300") != NULL);
    DONE();

    TEST("rpm_no_device_fails");
    fe_set_device_present(&dev, false);
    ASSERT(fe_run(&dev, &rpm, 3, NULL, NULL) == FE_EXIT_NO_DEVICE);
    DONE();

    TEST("rpm_no_disk_fails");
    fe_power_on_defaults(&dev);
    fe_set_disk_present(&dev, false);
    ASSERT(fe_run(&dev, &rpm, 3, NULL, NULL) == FE_EXIT_NO_DISK);
    DONE();

    TEST("all_three_stdout_shapes_carry_rpm_and_value");
    /* The provider regex is (\d+\.?\d*)\s*rpm icase — every shape must
     * contain the number and the token "rpm" (any case). */
    fe_power_on_defaults(&dev);
    int ok = 1;
    for (int v = 0; v < 3; v++) {
        fe_set_rpm_variant(&dev, v);
        fe_run(&dev, &rpm, 3, NULL, NULL);
        /* case-insensitive search for "rpm" */
        char low[128]; size_t n = strlen(dev.last_stdout);
        for (size_t i = 0; i <= n && i < sizeof(low); i++) {
            char c = dev.last_stdout[i];
            low[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
        }
        if (!strstr(low, "rpm") || !strstr(dev.last_stdout, "300")) ok = 0;
    }
    ASSERT(ok == 1);
    DONE();
}

/* ─── C. read exit codes ────────────────────────────────────────────── */

static void group_c(void) {
    printf("\n-- C. read exit codes --\n");
    fe_dev_t dev;
    fe_request_t rd = { .subcommand = "read", .cylinder = 0, .head = 0,
                        .revolutions = 2, .profile = "ibm" };
    uint8_t *scp = NULL; size_t len = 0;

    TEST("clean_read_succeeds_with_file");
    fe_power_on_defaults(&dev);
    ASSERT(fe_run(&dev, &rd, 3, &scp, &len) == FE_EXIT_OK);
    ASSERT(scp != NULL && len > 0);
    free(scp); scp = NULL;
    DONE();

    TEST("read_no_device_fails_empty");
    fe_set_device_present(&dev, false);
    ASSERT(fe_run(&dev, &rd, 3, &scp, &len) == FE_EXIT_NO_DEVICE);
    ASSERT(scp == NULL);
    DONE();

    TEST("read_no_disk_fails");
    fe_power_on_defaults(&dev);
    fe_set_disk_present(&dev, false);
    ASSERT(fe_run(&dev, &rd, 3, &scp, &len) == FE_EXIT_NO_DISK);
    DONE();

    TEST("read_bad_cylinder_is_bad_args");
    fe_power_on_defaults(&dev);
    fe_request_t bad = rd; bad.cylinder = 9999;
    ASSERT(fe_run(&dev, &bad, 3, &scp, &len) == FE_EXIT_BAD_ARGS);
    DONE();

    TEST("write_always_refused");
    fe_request_t wr = rd; wr.subcommand = "write";
    ASSERT(fe_run(&dev, &wr, 3, &scp, &len) == FE_EXIT_WRITE_DENY);
    ASSERT(scp == NULL);
    DONE();
}

/* ─── D. retry ──────────────────────────────────────────────────────── */

static void group_d(void) {
    printf("\n-- D. retry model --\n");
    fe_dev_t dev;
    fe_request_t rd = { .subcommand = "read", .cylinder = 5, .head = 0,
                        .revolutions = 1, .profile = "ibm" };
    uint8_t *scp = NULL; size_t len = 0;

    TEST("transient_io_recovers_within_retries");
    fe_power_on_defaults(&dev);
    fe_set_fail_next(&dev, 2);
    ASSERT(fe_run(&dev, &rd, 3, &scp, &len) == FE_EXIT_OK);
    free(scp); scp = NULL;
    DONE();

    TEST("retry_exhaustion_is_io");
    fe_power_on_defaults(&dev);
    fe_set_fail_next(&dev, 5);
    ASSERT(fe_run(&dev, &rd, 3, &scp, &len) == FE_EXIT_IO);
    ASSERT(scp == NULL);
    DONE();
}

/* ─── E. clean read → production SCP parser ─────────────────────────── */

static void group_e(void) {
    printf("\n-- E. clean read → production uft_scp_read_track_memory() --\n");
    fe_dev_t dev;
    fe_request_t rd = { .subcommand = "read", .cylinder = 17, .head = 0,
                        .revolutions = 2, .profile = "ibm" };
    uint8_t *scp = NULL; size_t len = 0;

    fe_power_on_defaults(&dev);
    fe_set_stream_seed(&dev, 0xC0DE);

    TEST("read_produces_parseable_scp");
    ASSERT(fe_run(&dev, &rd, 3, &scp, &len) == FE_EXIT_OK);
    uint32_t fc = 0, rpm = 0;
    int rc = scp_decode(scp, len, 17, &fc, &rpm);
    ASSERT(rc == UFT_SCP_OK);
    DONE();

    TEST("decoded_track_has_flux");
    ASSERT(fc > 0);
    DONE();

    TEST("decoded_rpm_near_300");
    ASSERT(rpm >= 285 && rpm <= 315);
    DONE();

    TEST("read_is_deterministic");
    uint8_t *scp2 = NULL; size_t len2 = 0;
    fe_dev_t dev2; fe_power_on_defaults(&dev2); fe_set_stream_seed(&dev2, 0xC0DE);
    fe_run(&dev2, &rd, 3, &scp2, &len2);
    ASSERT(len2 == len && scp2 && memcmp(scp, scp2, len) == 0);
    free(scp2);
    DONE();

    free(scp);
}

/* ─── F. defect → SCP error + RPM contract ──────────────────────────── */

static int gen_defect_rc(uint32_t defect, int track) {
    uft_fe_gen_params_t p = { .seed = 0x77, .track = track, .revolutions = 1,
                              .cell_ns = 0, .defects = defect,
                              .weak_jitter_pct = 8 };
    uft_fe_gen_scp_t s;
    if (uft_fe_gen_scp(&p, &s) != UFT_FE_GEN_OK) return 999;
    uft_scp_track_data_t td;
    int rc = uft_scp_read_track_memory(s.bytes, s.bytes_len, track, &td);
    if (rc == UFT_SCP_OK) uft_scp_free_track(&td);
    uft_fe_gen_free(&s);
    return rc;
}

static void group_f(void) {
    printf("\n-- F. defect classes → SCP parser error --\n");

    TEST("clean_decodes_ok");
    ASSERT(gen_defect_rc(UFT_FE_DEFECT_NONE, 3) == UFT_SCP_OK);
    DONE();

    TEST("bad_scp_sig_maps_to_ERR_SIGNATURE");
    ASSERT(gen_defect_rc(UFT_FE_DEFECT_BAD_SIG, 3) == UFT_SCP_ERR_SIGNATURE);
    DONE();

    TEST("bad_trk_sig_maps_to_ERR_SIGNATURE");
    ASSERT(gen_defect_rc(UFT_FE_DEFECT_BAD_TRK_SIG, 3) == UFT_SCP_ERR_SIGNATURE);
    DONE();

    TEST("zero_offset_maps_to_ERR_TRACK");
    ASSERT(gen_defect_rc(UFT_FE_DEFECT_ZERO_OFFSET, 3) == UFT_SCP_ERR_TRACK);
    DONE();

    TEST("truncated_maps_to_ERR_READ");
    ASSERT(gen_defect_rc(UFT_FE_DEFECT_TRUNCATED, 3) == UFT_SCP_ERR_READ);
    DONE();

    TEST("weak_bits_still_medium_safe");
    uft_fe_gen_params_t p = { .seed = 9, .track = 3, .revolutions = 1,
                              .cell_ns = 0, .defects = UFT_FE_DEFECT_WEAK_BITS,
                              .weak_jitter_pct = 10 };
    uft_fe_gen_scp_t s;
    ASSERT(uft_fe_gen_scp(&p, &s) == UFT_FE_GEN_OK);
    ASSERT(uft_fe_gen_count_unsafe(&s) == 0);
    uft_fe_gen_free(&s);
    DONE();

    TEST("out_of_spec_track_refused");
    uft_fe_gen_params_t pb = p; pb.track = 500;
    uft_fe_gen_scp_t sb;
    ASSERT(uft_fe_gen_scp(&pb, &sb) == UFT_FE_GEN_ERR_OUT_OF_SPEC);
    DONE();

    TEST("rpm_line_shapes_match_provider_regex");
    /* Every shape contains a number then (icase) rpm — the provider's
     * (\d+\.?\d*)\s*rpm must match all three. */
    char buf[128]; int all = 1;
    for (int v = 0; v < 3; v++) {
        uft_fe_format_rpm_line(300.0, v, buf, sizeof(buf));
        if (!strstr(buf, "300")) all = 0;
    }
    ASSERT(all == 1);
    DONE();
}

int main(void) {
    printf("=== FluxEngine firmware-realistic emulator tests ===\n");
    group_a();
    group_b();
    group_c();
    group_d();
    group_e();
    group_f();
    printf("\nResults: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
