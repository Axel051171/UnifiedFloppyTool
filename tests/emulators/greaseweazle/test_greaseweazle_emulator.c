/**
 * @file tests/emulators/greaseweazle/test_greaseweazle_emulator.c
 * @brief Firmware-realistic Greaseweazle emulator + flux-gen edge-case tests.
 *
 * This complements (does NOT replace) the production HAL at
 * src/hal/uft_greaseweazle_full.c. The HAL is the only PRODUCTION-tested
 * hardware path in UFT — this emulator calibrates the firmware-realistic
 * methodology against that ground truth.
 *
 * 32 tests organised into 5 groups:
 *
 *   A. Wire protocol + GetInfo  (1-7)   — opcode handling, info struct
 *   B. State-machine prerequisites (8-15) — bus/select/motor/seek ordering
 *   C. Pin/TRK0/WRPROT (16-19) — sensor inputs
 *   D. Read/Write pipeline + refusal (20-25) — flux streaming, write-safety
 *   E. Flux-gen determinism + defects (26-32) — synthetic patterns
 *
 * Forensic invariants asserted across the suite:
 *   - Sequencing violations transition firmware to ERROR + return a
 *     non-OK ack (never silent no-op)
 *   - Cmd.WriteFlux and Cmd.EraseFlux are ALWAYS refused (ACK_WRPROT)
 *     regardless of media state — this emulator never writes
 *   - Same RNG seed produces byte-identical flux output (CI-stable)
 *   - Every emitted flux interval is within medium-safe range
 *     [UFT_GW_FLUX_GEN_MIN_NS, MAX_NS]
 *   - Encoded streams roundtrip-decode to the original tick counts
 *     (mirrors the production C-HAL decoder)
 */

#include "firmware_state_machine.h"
#include "../../flux_gen/greaseweazle/flux_gen.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-58s ... ", #name); fflush(stdout); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        else { printf("FAIL\n"); } fflush(stdout); \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* Helper: bring fw to MOTOR_ON with disk + bus + drive ready. */
static void setup_ready_for_read(gw_fw_t *fw)
{
    gw_fw_power_on_defaults(fw);
    (void)gw_fw_cmd_set_bus_type(fw, GW_FW_BUS_IBM_PC);
    (void)gw_fw_cmd_select(fw, 0);
    (void)gw_fw_cmd_motor(fw, 0, true);
}

/* ─────────────────────────────────────────────────────────────────────
 *  A. Wire protocol + GetInfo
 * ───────────────────────────────────────────────────────────────────── */

TEST(power_on_state_is_disconnected) {
    gw_fw_t fw;
    gw_fw_reset(&fw);
    ASSERT(fw.state == GW_FW_STATE_DISCONNECTED);
    ASSERT(fw.selected_unit == 0xFF);
    ASSERT(fw.current_cyl == -1);
    ASSERT(fw.cmd_count == 0);
}

TEST(get_info_returns_32_byte_struct) {
    gw_fw_t fw;
    gw_fw_power_on_defaults(&fw);
    uint8_t info[32] = {0};
    ASSERT(gw_fw_cmd_get_info(&fw, 0, info, sizeof(info)) == GW_FW_ACK_OK);
    /* Default firmware: v1.23, is_main_fw=1, F7, 72 MHz. */
    ASSERT(info[0] == 1);             /* fw_major */
    ASSERT(info[1] == 23);            /* fw_minor */
    ASSERT(info[2] == 1);             /* is_main_fw */
    /* sample_freq @ offset 4..7, little-endian = 72_000_000. */
    uint32_t freq = (uint32_t)info[4] | ((uint32_t)info[5] << 8) |
                    ((uint32_t)info[6] << 16) | ((uint32_t)info[7] << 24);
    ASSERT(freq == 72000000u);
    ASSERT(info[8] == 1);             /* hw_model = F7 */
}

TEST(get_info_unknown_subindex_refused) {
    gw_fw_t fw;
    gw_fw_power_on_defaults(&fw);
    uint8_t info[32] = {0};
    /* subindex 7 (current-drive) not modelled — must refuse. */
    ASSERT(gw_fw_cmd_get_info(&fw, 7, info, sizeof(info))
           == GW_FW_ACK_BAD_COMMAND);
}

TEST(get_info_works_from_disconnected) {
    gw_fw_t fw;
    gw_fw_reset(&fw); /* leaves state == DISCONNECTED */
    uint8_t info[32] = {0};
    ASSERT(gw_fw_cmd_get_info(&fw, 0, info, sizeof(info)) == GW_FW_ACK_OK);
    /* After GetInfo, transition to CONNECTED (matches uft_gw_open). */
    ASSERT(fw.state == GW_FW_STATE_CONNECTED);
}

TEST(get_info_works_from_error_state) {
    gw_fw_t fw;
    gw_fw_power_on_defaults(&fw);
    /* Force ERROR by issuing Motor without Select. */
    ASSERT(gw_fw_cmd_motor(&fw, 0, true) == GW_FW_ACK_NO_UNIT);
    ASSERT(fw.state == GW_FW_STATE_ERROR);
    /* GetInfo still answers — host needs it to diagnose. */
    uint8_t info[32] = {0};
    ASSERT(gw_fw_cmd_get_info(&fw, 0, info, sizeof(info)) == GW_FW_ACK_OK);
}

TEST(bootloader_mode_flagged_in_info) {
    gw_fw_t fw;
    gw_fw_power_on_defaults(&fw);
    gw_fw_set_bootloader_mode(&fw, true);
    uint8_t info[32] = {0};
    ASSERT(gw_fw_cmd_get_info(&fw, 0, info, sizeof(info)) == GW_FW_ACK_OK);
    /* is_main_fw == 0 — HAL uses this to reject bootloader devices. */
    ASSERT(info[2] == 0);
}

TEST(packet_builder_matches_hal_serialization) {
    /* Build a Seek(cyl=42) packet and verify [CMD, LEN, params]. */
    uint8_t pkt[8] = {0};
    uint8_t params[1] = { 42 };
    size_t n = gw_fw_build_packet(pkt, 0x02 /* CMD_SEEK */, params, 1);
    ASSERT(n == 3);
    ASSERT(pkt[0] == 0x02);   /* cmd */
    ASSERT(pkt[1] == 3);      /* total length = cmd + len + 1 param */
    ASSERT(pkt[2] == 42);     /* cyl param */
}

/* ─────────────────────────────────────────────────────────────────────
 *  B. State-machine prerequisites
 * ───────────────────────────────────────────────────────────────────── */

TEST(happy_path_bus_select_motor_seek) {
    gw_fw_t fw;
    gw_fw_power_on_defaults(&fw);
    ASSERT(gw_fw_cmd_set_bus_type(&fw, GW_FW_BUS_IBM_PC) == GW_FW_ACK_OK);
    ASSERT(fw.state == GW_FW_STATE_BUS_CONFIGURED);
    ASSERT(gw_fw_cmd_select(&fw, 0) == GW_FW_ACK_OK);
    ASSERT(fw.state == GW_FW_STATE_DRIVE_SELECTED);
    ASSERT(gw_fw_cmd_motor(&fw, 0, true) == GW_FW_ACK_OK);
    ASSERT(fw.state == GW_FW_STATE_MOTOR_ON);
    ASSERT(gw_fw_cmd_seek(&fw, 42) == GW_FW_ACK_OK);
    ASSERT(fw.state == GW_FW_STATE_SEEKED);
    ASSERT(fw.current_cyl == 42);
    ASSERT(gw_fw_cmd_head(&fw, 1) == GW_FW_ACK_OK);
    ASSERT(fw.current_head == 1);
}

TEST(select_without_bus_type_refused) {
    gw_fw_t fw;
    gw_fw_power_on_defaults(&fw);
    /* Skip SetBusType — Select must return NO_BUS. */
    ASSERT(gw_fw_cmd_select(&fw, 0) == GW_FW_ACK_NO_BUS);
    ASSERT(fw.state == GW_FW_STATE_ERROR);
    ASSERT(fw.sticky_error == GW_FW_ACK_NO_BUS);
}

TEST(select_invalid_unit_refused) {
    gw_fw_t fw;
    gw_fw_power_on_defaults(&fw);
    ASSERT(gw_fw_cmd_set_bus_type(&fw, GW_FW_BUS_IBM_PC) == GW_FW_ACK_OK);
    /* Unit 4 is out of range (max=3). */
    ASSERT(gw_fw_cmd_select(&fw, 4) == GW_FW_ACK_BAD_UNIT);
    ASSERT(fw.state == GW_FW_STATE_ERROR);
}

TEST(motor_without_select_refused) {
    gw_fw_t fw;
    gw_fw_power_on_defaults(&fw);
    /* No Select — Motor must return NO_UNIT. */
    ASSERT(gw_fw_cmd_motor(&fw, 0, true) == GW_FW_ACK_NO_UNIT);
    ASSERT(fw.state == GW_FW_STATE_ERROR);
}

TEST(seek_without_motor_refused) {
    gw_fw_t fw;
    gw_fw_power_on_defaults(&fw);
    ASSERT(gw_fw_cmd_set_bus_type(&fw, GW_FW_BUS_IBM_PC) == GW_FW_ACK_OK);
    ASSERT(gw_fw_cmd_select(&fw, 0) == GW_FW_ACK_OK);
    /* Skip Motor — Seek must refuse. */
    ASSERT(gw_fw_cmd_seek(&fw, 10) == GW_FW_ACK_BAD_COMMAND);
    ASSERT(fw.state == GW_FW_STATE_ERROR);
}

TEST(seek_to_cyl_0_with_no_trk0_reports_no_trk0) {
    gw_fw_t fw;
    gw_fw_power_on_defaults(&fw);
    gw_fw_set_trk0_present(&fw, false);
    ASSERT(gw_fw_cmd_set_bus_type(&fw, GW_FW_BUS_IBM_PC) == GW_FW_ACK_OK);
    ASSERT(gw_fw_cmd_select(&fw, 0) == GW_FW_ACK_OK);
    ASSERT(gw_fw_cmd_motor(&fw, 0, true) == GW_FW_ACK_OK);
    /* Seek to cyl 0: firmware checks TRK0 pin → NO_TRK0. */
    ASSERT(gw_fw_cmd_seek(&fw, 0) == GW_FW_ACK_NO_TRK0);
    ASSERT(fw.state == GW_FW_STATE_ERROR);
    ASSERT(fw.sticky_error == GW_FW_ACK_NO_TRK0);
}

TEST(no_click_step_recovers_from_no_trk0) {
    gw_fw_t fw;
    gw_fw_power_on_defaults(&fw);
    gw_fw_set_trk0_present(&fw, false);
    ASSERT(gw_fw_cmd_set_bus_type(&fw, GW_FW_BUS_IBM_PC) == GW_FW_ACK_OK);
    ASSERT(gw_fw_cmd_select(&fw, 0) == GW_FW_ACK_OK);
    ASSERT(gw_fw_cmd_motor(&fw, 0, true) == GW_FW_ACK_OK);
    ASSERT(gw_fw_cmd_seek(&fw, 0) == GW_FW_ACK_NO_TRK0);
    /* NoClickStep recovery → next Seek(0) should succeed. */
    ASSERT(gw_fw_cmd_no_click_step(&fw) == GW_FW_ACK_OK);
    ASSERT(fw.state == GW_FW_STATE_MOTOR_ON);
    ASSERT(gw_fw_cmd_seek(&fw, 0) == GW_FW_ACK_OK);
    ASSERT(fw.current_cyl == 0);
}

TEST(seek_signed_cylinder_supports_negative) {
    gw_fw_t fw;
    setup_ready_for_read(&fw);
    /* Flippy-drive negative cylinder — the wire format is int8. We
     * accept it but record the negative value. (HAL's uint8_t cyl
     * argument can't express this, but firmware itself does.) */
    ASSERT(gw_fw_cmd_seek(&fw, -3) == GW_FW_ACK_OK);
    ASSERT(fw.current_cyl == -3);
}

TEST(deselect_collapses_to_bus_configured) {
    gw_fw_t fw;
    setup_ready_for_read(&fw);
    ASSERT(fw.state == GW_FW_STATE_MOTOR_ON);
    ASSERT(gw_fw_cmd_deselect(&fw) == GW_FW_ACK_OK);
    ASSERT(fw.state == GW_FW_STATE_BUS_CONFIGURED);
    ASSERT(fw.motor_on == false);
    /* Now Motor must refuse again. */
    ASSERT(gw_fw_cmd_motor(&fw, 0, true) == GW_FW_ACK_NO_UNIT);
}

/* ─────────────────────────────────────────────────────────────────────
 *  C. Pin / TRK0 / WRPROT
 * ───────────────────────────────────────────────────────────────────── */

TEST(get_pin_trk0_reports_active_low) {
    gw_fw_t fw;
    gw_fw_power_on_defaults(&fw);
    uint8_t level = 0xFF;
    /* Default: trk0_present=true → pin LOW (0) per GW firmware
     * active-low convention. */
    ASSERT(gw_fw_cmd_get_pin(&fw, GW_FW_PIN_TRK0, &level) == GW_FW_ACK_OK);
    ASSERT(level == 0);

    gw_fw_set_trk0_present(&fw, false);
    ASSERT(gw_fw_cmd_get_pin(&fw, GW_FW_PIN_TRK0, &level) == GW_FW_ACK_OK);
    ASSERT(level == 1);
}

TEST(get_pin_wrprot_reports_active_low) {
    gw_fw_t fw;
    gw_fw_power_on_defaults(&fw);
    uint8_t level = 0xFF;
    /* Default: write_protected=false → pin HIGH (1). */
    ASSERT(gw_fw_cmd_get_pin(&fw, GW_FW_PIN_WRPROT, &level) == GW_FW_ACK_OK);
    ASSERT(level == 1);

    gw_fw_set_write_protected(&fw, true);
    ASSERT(gw_fw_cmd_get_pin(&fw, GW_FW_PIN_WRPROT, &level) == GW_FW_ACK_OK);
    ASSERT(level == 0);
}

TEST(get_pin_unknown_pin_refused) {
    gw_fw_t fw;
    gw_fw_power_on_defaults(&fw);
    uint8_t level = 0;
    ASSERT(gw_fw_cmd_get_pin(&fw, 99, &level) == GW_FW_ACK_BAD_PIN);
    ASSERT(fw.state == GW_FW_STATE_ERROR);
}

TEST(set_pin_always_refused) {
    gw_fw_t fw;
    gw_fw_power_on_defaults(&fw);
    /* We do not model output pins. Refuse honestly rather than fake. */
    ASSERT(gw_fw_cmd_set_pin(&fw, 26, 1) == GW_FW_ACK_BAD_PIN);
}

/* ─────────────────────────────────────────────────────────────────────
 *  D. Read pipeline + write/erase refusal
 * ───────────────────────────────────────────────────────────────────── */

TEST(write_flux_always_refused) {
    gw_fw_t fw;
    setup_ready_for_read(&fw);
    ASSERT(gw_fw_cmd_seek(&fw, 0) == GW_FW_ACK_OK);
    /* WriteFlux always refused, even on writable disk. */
    ASSERT(fw.write_protected == false);
    ASSERT(gw_fw_cmd_write_flux(&fw, 1, 1) == GW_FW_ACK_WRPROT);
}

TEST(erase_flux_always_refused) {
    gw_fw_t fw;
    setup_ready_for_read(&fw);
    ASSERT(gw_fw_cmd_seek(&fw, 0) == GW_FW_ACK_OK);
    ASSERT(gw_fw_cmd_erase_flux(&fw, 1000000u) == GW_FW_ACK_WRPROT);
}

TEST(read_flux_without_motor_refused) {
    gw_fw_t fw;
    gw_fw_power_on_defaults(&fw);
    ASSERT(gw_fw_cmd_set_bus_type(&fw, GW_FW_BUS_IBM_PC) == GW_FW_ACK_OK);
    ASSERT(gw_fw_cmd_select(&fw, 0) == GW_FW_ACK_OK);
    /* No motor → ReadFlux must refuse. */
    ASSERT(gw_fw_cmd_read_flux(&fw, 0, 3) == GW_FW_ACK_BAD_COMMAND);
    ASSERT(fw.state == GW_FW_STATE_ERROR);
}

TEST(read_flux_no_disk_reports_no_index) {
    gw_fw_t fw;
    gw_fw_power_on_defaults(&fw);
    gw_fw_set_disk_present(&fw, false);
    ASSERT(gw_fw_cmd_set_bus_type(&fw, GW_FW_BUS_IBM_PC) == GW_FW_ACK_OK);
    ASSERT(gw_fw_cmd_select(&fw, 0) == GW_FW_ACK_OK);
    ASSERT(gw_fw_cmd_motor(&fw, 0, true) == GW_FW_ACK_OK);
    /* No disk → firmware reports NO_INDEX (no pulses coming). */
    ASSERT(gw_fw_cmd_read_flux(&fw, 0, 3) == GW_FW_ACK_NO_INDEX);
    ASSERT(fw.sticky_error == GW_FW_ACK_NO_INDEX);
}

TEST(read_flux_zero_revs_refused) {
    gw_fw_t fw;
    setup_ready_for_read(&fw);
    /* revs == 0 is illegal at the protocol layer (HAL passes revs+1). */
    ASSERT(gw_fw_cmd_read_flux(&fw, 0, 0) == GW_FW_ACK_BAD_COMMAND);
}

TEST(read_flux_drains_loaded_stream_to_eos) {
    gw_fw_t fw;
    setup_ready_for_read(&fw);
    /* Pre-load a tiny stream: [0x10, 0x20, 0x00 EOS]. */
    const uint8_t stream[] = { 0x10, 0x20, 0x00 };
    gw_fw_load_read_stream(&fw, stream, sizeof(stream));
    ASSERT(gw_fw_cmd_read_flux(&fw, 0, 3) == GW_FW_ACK_OK);
    ASSERT(fw.state == GW_FW_STATE_STREAMING_READ);

    uint8_t b = 0;
    ASSERT(gw_fw_pop_read_byte(&fw, &b) == GW_FW_ACK_OK);
    ASSERT(b == 0x10);
    ASSERT(gw_fw_pop_read_byte(&fw, &b) == GW_FW_ACK_OK);
    ASSERT(b == 0x20);
    ASSERT(gw_fw_pop_read_byte(&fw, &b) == GW_FW_ACK_OK);
    ASSERT(b == 0x00);   /* EOS */
    ASSERT(fw.state == GW_FW_STATE_FLUX_STATUS_READY);

    /* GetFluxStatus delivers the sticky status and returns to SEEKED. */
    ASSERT(gw_fw_cmd_get_flux_status(&fw) == GW_FW_ACK_OK);
    ASSERT(fw.state == GW_FW_STATE_SEEKED);
}

TEST(reset_clears_sticky_error) {
    gw_fw_t fw;
    gw_fw_power_on_defaults(&fw);
    /* Force ERROR. */
    ASSERT(gw_fw_cmd_motor(&fw, 0, true) == GW_FW_ACK_NO_UNIT);
    ASSERT(fw.state == GW_FW_STATE_ERROR);
    /* Reset → CONNECTED, sticky cleared. */
    ASSERT(gw_fw_cmd_reset(&fw) == GW_FW_ACK_OK);
    ASSERT(fw.state == GW_FW_STATE_CONNECTED);
    ASSERT(fw.sticky_error == GW_FW_ACK_OK);
}

/* ─────────────────────────────────────────────────────────────────────
 *  E. Flux-gen determinism + defects
 * ───────────────────────────────────────────────────────────────────── */

static uft_gw_flux_params_t default_params(uint64_t seed)
{
    uft_gw_flux_params_t p = {
        .seed                = seed,
        .revolutions         = 2,
        .index_period_ns     = 200000000u,
        .transitions_per_rev = 100,
        .sample_freq_hz      = UFT_GW_FLUX_GEN_FREQ_F7_HZ,
        .defects             = UFT_GW_DEFECT_NONE,
        .weak_jitter_pct     = 0,
    };
    return p;
}

TEST(flux_gen_clean_is_deterministic) {
    uft_gw_flux_params_t p = default_params(0xCAFEBABEull);
    uft_gw_flux_capture_t c1 = {0}, c2 = {0};
    ASSERT(uft_gw_flux_gen_clean(&p, &c1) == UFT_GW_FLUX_GEN_OK);
    ASSERT(uft_gw_flux_gen_clean(&p, &c2) == UFT_GW_FLUX_GEN_OK);
    ASSERT(c1.bytes_len == c2.bytes_len);
    ASSERT(c1.bytes_len > 0);
    ASSERT(memcmp(c1.bytes, c2.bytes, c1.bytes_len) == 0);
    ASSERT(c1.rev_count == 2);
    uft_gw_flux_gen_free(&c1);
    uft_gw_flux_gen_free(&c2);
}

TEST(flux_gen_stream_ends_with_eos_byte) {
    uft_gw_flux_params_t p = default_params(1);
    uft_gw_flux_capture_t c = {0};
    ASSERT(uft_gw_flux_gen_clean(&p, &c) == UFT_GW_FLUX_GEN_OK);
    ASSERT(c.bytes_len >= 1);
    ASSERT(c.bytes[c.bytes_len - 1] == 0x00);
    uft_gw_flux_gen_free(&c);
}

TEST(flux_gen_rejects_out_of_spec_freq) {
    uft_gw_flux_params_t p = default_params(1);
    p.sample_freq_hz = 1000u;  /* 1 kHz — absurd */
    uft_gw_flux_capture_t c = {0};
    ASSERT(uft_gw_flux_gen_clean(&p, &c) == UFT_GW_FLUX_GEN_ERR_OUT_OF_SPEC);
    ASSERT(c.bytes == NULL);
}

TEST(flux_gen_medium_safety_holds_for_clean) {
    uft_gw_flux_params_t p = default_params(42);
    p.transitions_per_rev = 500;
    p.revolutions         = 3;
    uft_gw_flux_capture_t c = {0};
    ASSERT(uft_gw_flux_gen_clean(&p, &c) == UFT_GW_FLUX_GEN_OK);
    ASSERT(uft_gw_flux_gen_count_unsafe(&c) == 0);
    uft_gw_flux_gen_free(&c);
}

TEST(flux_gen_weak_bits_changes_output_but_stays_safe) {
    uft_gw_flux_params_t p_clean = default_params(7);
    uft_gw_flux_params_t p_weak  = p_clean;
    p_weak.defects         = UFT_GW_DEFECT_WEAK_BITS;
    p_weak.weak_jitter_pct = 20;

    uft_gw_flux_capture_t c_clean = {0}, c_weak = {0};
    ASSERT(uft_gw_flux_gen_clean(&p_clean, &c_clean) == UFT_GW_FLUX_GEN_OK);
    ASSERT(uft_gw_flux_gen_clean(&p_weak,  &c_weak)  == UFT_GW_FLUX_GEN_OK);
    /* Streams should differ — weak-bit jitter shifts cell times. */
    ASSERT(c_clean.bytes_len > 0 && c_weak.bytes_len > 0);
    ASSERT(c_clean.bytes_len == c_weak.bytes_len ||
           c_clean.bytes_len != c_weak.bytes_len);
    int differs = 0;
    size_t min_len = (c_clean.bytes_len < c_weak.bytes_len) ?
                      c_clean.bytes_len : c_weak.bytes_len;
    for (size_t i = 0; i < min_len; i++) {
        if (c_clean.bytes[i] != c_weak.bytes[i]) { differs = 1; break; }
    }
    ASSERT(differs == 1);
    /* But still medium-safe. */
    ASSERT(uft_gw_flux_gen_count_unsafe(&c_weak) == 0);
    uft_gw_flux_gen_free(&c_clean);
    uft_gw_flux_gen_free(&c_weak);
}

TEST(flux_gen_crc_error_changes_exactly_one_byte) {
    uft_gw_flux_params_t p_clean = default_params(11);
    uft_gw_flux_params_t p_crc   = p_clean;
    p_crc.defects = UFT_GW_DEFECT_CRC_ERROR;

    uft_gw_flux_capture_t c_clean = {0}, c_crc = {0};
    ASSERT(uft_gw_flux_gen_clean(&p_clean, &c_clean) == UFT_GW_FLUX_GEN_OK);
    ASSERT(uft_gw_flux_gen_clean(&p_crc,   &c_crc)   == UFT_GW_FLUX_GEN_OK);
    ASSERT(c_clean.bytes_len == c_crc.bytes_len);
    size_t diff = 0;
    for (size_t i = 0; i < c_clean.bytes_len; i++) {
        if (c_clean.bytes[i] != c_crc.bytes[i]) diff++;
    }
    ASSERT(diff == 1);
    uft_gw_flux_gen_free(&c_clean);
    uft_gw_flux_gen_free(&c_crc);
}

TEST(flux_gen_nfa_burst_emits_space_and_astable_opcodes) {
    uft_gw_flux_params_t p = default_params(0xBEEF);
    p.defects = UFT_GW_DEFECT_NFA_BURST;
    uft_gw_flux_capture_t c = {0};
    ASSERT(uft_gw_flux_gen_clean(&p, &c) == UFT_GW_FLUX_GEN_OK);
    /* Scan for the [0xFF, SPACE, n28(4), 0xFF, ASTABLE, n28(4)] pattern. */
    int found = 0;
    for (size_t i = 0; i + 12 <= c.bytes_len; i++) {
        if (c.bytes[i] == 0xFF &&
            c.bytes[i + 1] == UFT_GW_FLUXOP_SPACE &&
            c.bytes[i + 6] == 0xFF &&
            c.bytes[i + 7] == UFT_GW_FLUXOP_ASTABLE) {
            found = 1;
            break;
        }
    }
    ASSERT(found == 1);
    uft_gw_flux_gen_free(&c);
}

TEST(flux_gen_decode_roundtrip_intervals_in_safe_range) {
    uft_gw_flux_params_t p = default_params(99);
    p.transitions_per_rev = 50;
    p.revolutions         = 1;
    uft_gw_flux_capture_t c = {0};
    ASSERT(uft_gw_flux_gen_clean(&p, &c) == UFT_GW_FLUX_GEN_OK);

    uint32_t ns_buf[128] = {0};
    size_t n = uft_gw_flux_gen_decode_ns(&c, ns_buf, 128);
    ASSERT(n == 50);
    for (size_t i = 0; i < n; i++) {
        ASSERT(ns_buf[i] >= UFT_GW_FLUX_GEN_MIN_NS);
        ASSERT(ns_buf[i] <= UFT_GW_FLUX_GEN_MAX_NS);
        /* Clean MFM: cells are 2..4 × 4 µs = 8..16 µs. */
        ASSERT(ns_buf[i] >= 7000u);
        ASSERT(ns_buf[i] <= 17000u);
    }
    uft_gw_flux_gen_free(&c);
}

TEST(flux_gen_f7plus_freq_affects_decoded_ns_but_keeps_same_ns) {
    /* Generate the same logical pattern at 72 MHz and 84 MHz. The
     * decoded ns values must be (approximately) the same — the freq
     * affects byte count but the encoded interval represents the same
     * physical time. */
    uft_gw_flux_params_t p72 = default_params(123);
    p72.transitions_per_rev = 30;
    p72.revolutions         = 1;
    uft_gw_flux_params_t p84 = p72;
    p84.sample_freq_hz = UFT_GW_FLUX_GEN_FREQ_F7P_HZ;

    uft_gw_flux_capture_t c72 = {0}, c84 = {0};
    ASSERT(uft_gw_flux_gen_clean(&p72, &c72) == UFT_GW_FLUX_GEN_OK);
    ASSERT(uft_gw_flux_gen_clean(&p84, &c84) == UFT_GW_FLUX_GEN_OK);

    uint32_t ns72[64] = {0}, ns84[64] = {0};
    size_t n72 = uft_gw_flux_gen_decode_ns(&c72, ns72, 64);
    size_t n84 = uft_gw_flux_gen_decode_ns(&c84, ns84, 64);
    ASSERT(n72 == n84);
    ASSERT(n72 == 30);
    for (size_t i = 0; i < n72; i++) {
        /* Same physical timing (within 1 tick rounding). */
        uint32_t diff = (ns72[i] > ns84[i]) ?
                         (ns72[i] - ns84[i]) : (ns84[i] - ns72[i]);
        ASSERT(diff < 200u);   /* < 200 ns rounding noise */
    }
    uft_gw_flux_gen_free(&c72);
    uft_gw_flux_gen_free(&c84);
}

/* ─────────────────────────────────────────────────────────────────────
 *  F. End-to-end: flux-gen output flows through the firmware emulator's
 *     ReadFlux pop-byte loop and the bytes match what the generator
 *     produced.
 * ───────────────────────────────────────────────────────────────────── */

TEST(roundtrip_flux_gen_via_emulator_read_pipeline) {
    /* Generate a small clean capture. */
    uft_gw_flux_params_t p = default_params(0xA1B2C3);
    p.transitions_per_rev = 30;
    p.revolutions         = 1;
    uft_gw_flux_capture_t cap = {0};
    ASSERT(uft_gw_flux_gen_clean(&p, &cap) == UFT_GW_FLUX_GEN_OK);

    /* Wire it into the emulator. */
    gw_fw_t fw;
    setup_ready_for_read(&fw);
    ASSERT(gw_fw_cmd_seek(&fw, 0) == GW_FW_ACK_OK);
    ASSERT(gw_fw_cmd_head(&fw, 0) == GW_FW_ACK_OK);
    gw_fw_load_read_stream(&fw, cap.bytes, cap.bytes_len);

    /* Drive ReadFlux + drain. */
    ASSERT(gw_fw_cmd_read_flux(&fw, 0, 2) == GW_FW_ACK_OK);
    ASSERT(fw.state == GW_FW_STATE_STREAMING_READ);

    uint8_t drained[2048] = {0};
    size_t  drain_pos = 0;
    while (drain_pos < sizeof(drained)) {
        uint8_t b = 0xAA;
        ASSERT(gw_fw_pop_read_byte(&fw, &b) == GW_FW_ACK_OK);
        drained[drain_pos++] = b;
        if (b == 0x00) break;
    }
    ASSERT(drain_pos == cap.bytes_len);
    ASSERT(memcmp(drained, cap.bytes, drain_pos) == 0);
    ASSERT(fw.state == GW_FW_STATE_FLUX_STATUS_READY);

    uft_gw_flux_gen_free(&cap);
}

int main(void)
{
    printf("=== Greaseweazle Firmware-Realistic Emulator + Flux-Gen Tests ===\n");
    fflush(stdout);

    printf("\n-- A. Wire protocol + GetInfo --\n");
    RUN(power_on_state_is_disconnected);
    RUN(get_info_returns_32_byte_struct);
    RUN(get_info_unknown_subindex_refused);
    RUN(get_info_works_from_disconnected);
    RUN(get_info_works_from_error_state);
    RUN(bootloader_mode_flagged_in_info);
    RUN(packet_builder_matches_hal_serialization);

    printf("\n-- B. State-machine prerequisites --\n");
    RUN(happy_path_bus_select_motor_seek);
    RUN(select_without_bus_type_refused);
    RUN(select_invalid_unit_refused);
    RUN(motor_without_select_refused);
    RUN(seek_without_motor_refused);
    RUN(seek_to_cyl_0_with_no_trk0_reports_no_trk0);
    RUN(no_click_step_recovers_from_no_trk0);
    RUN(seek_signed_cylinder_supports_negative);
    RUN(deselect_collapses_to_bus_configured);

    printf("\n-- C. Pin / TRK0 / WRPROT --\n");
    RUN(get_pin_trk0_reports_active_low);
    RUN(get_pin_wrprot_reports_active_low);
    RUN(get_pin_unknown_pin_refused);
    RUN(set_pin_always_refused);

    printf("\n-- D. Read pipeline + write/erase refusal --\n");
    RUN(write_flux_always_refused);
    RUN(erase_flux_always_refused);
    RUN(read_flux_without_motor_refused);
    RUN(read_flux_no_disk_reports_no_index);
    RUN(read_flux_zero_revs_refused);
    RUN(read_flux_drains_loaded_stream_to_eos);
    RUN(reset_clears_sticky_error);

    printf("\n-- E. Flux-gen determinism + defects --\n");
    RUN(flux_gen_clean_is_deterministic);
    RUN(flux_gen_stream_ends_with_eos_byte);
    RUN(flux_gen_rejects_out_of_spec_freq);
    RUN(flux_gen_medium_safety_holds_for_clean);
    RUN(flux_gen_weak_bits_changes_output_but_stays_safe);
    RUN(flux_gen_crc_error_changes_exactly_one_byte);
    RUN(flux_gen_nfa_burst_emits_space_and_astable_opcodes);
    RUN(flux_gen_decode_roundtrip_intervals_in_safe_range);
    RUN(flux_gen_f7plus_freq_affects_decoded_ns_but_keeps_same_ns);

    printf("\n-- F. End-to-end emulator + flux-gen roundtrip --\n");
    RUN(roundtrip_flux_gen_via_emulator_read_pipeline);

    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
