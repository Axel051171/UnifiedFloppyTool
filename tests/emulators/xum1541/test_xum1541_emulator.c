/**
 * @file tests/emulators/xum1541/test_xum1541_emulator.c
 * @brief Firmware-realistic XUM1541 emulator + GCR-generator edge-case tests.
 *
 * Complements (does NOT replace) the Tier-2.5 byte-mock at
 * tests/test_xum1541_usb_mock.c and the HAL honesty tests at
 * tests/test_xum1541_hal.c. Those verify the HAL's libusb call
 * sequence; THIS suite verifies firmware sequencing semantics and
 * the synthetic GCR payloads a 1541-class drive would deliver.
 *
 * Wire model (MF-301 OpenCBM-verified): the only bulk data opcodes are
 * READ(8) and WRITE(9) with header [opcode, proto|flags, size_lo,
 * size_hi]; IEC addressing = WRITE with FLAG_WRITE_ATN + raw ATN bytes
 * as payload; WRITE status = 3 bytes with the IEC byte count in the
 * extended value; READ has no status phase (short read = EOI); IOCTLs
 * 23..31 as [cmd, arg1, arg2, 0].
 *
 * 56 tests in 5 groups:
 *   A. Control transfers + identity          (1-10)
 *   B. Bulk wire format + IEC ATN addressing (11-32)
 *   C. IOCTLs, EOI + status wire format      (33-39)
 *   D. GCR generator determinism/defects     (40-54)
 *   E. End-to-end TALK/READ pipeline         (55-56)
 *
 * The suite links src/hal/uft_xum1541.c (compiled WITHOUT USB I/O at
 * runtime) so the generator's Commodore zone tables are cross-checked
 * against the HAL SSOT uft_xum_sectors_for_track() — no duplicated
 * truth.
 *
 * Forensic invariants asserted across the suite:
 *   - CTRL_ENTER_BOOTLOADER is ALWAYS refused (bricking guard)
 *   - sequencing violations produce STATUS_ERROR + sticky state,
 *     never a silent no-op; unknown opcodes and unknown protocol
 *     nibbles are rejected, never faked
 *   - same RNG seed produces byte-identical GCR output (CI-stable)
 *   - every generated track fits its density-zone byte budget
 *     (medium-safety: an overlong track written back would destroy
 *     its own write splice)
 */

#include "firmware_state_machine.h"
#include "../../flux_gen/xum1541/flux_gen.h"
#include "uft/hal/uft_xum1541.h"

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

/* ─── Wire helpers ──────────────────────────────────────────────────── */

/** Send a raw 4-byte bulk command header. */
static uint8_t bulk4(xum_fw_t *fw, uint8_t op, uint8_t b1,
                     uint8_t b2, uint8_t b3)
{
    uint8_t cmd[4] = { op, b1, b2, b3 };
    return xum_fw_bulk_command(fw, cmd);
}

/** Send [WRITE, proto|flags, len_lo, len_hi]. */
static uint8_t cmd_write(xum_fw_t *fw, uint8_t proto_flags, uint16_t len)
{
    return bulk4(fw, UFT_XUM1541_BULK_WRITE, proto_flags,
                 (uint8_t)(len & 0xFF), (uint8_t)(len >> 8));
}

/** Send [READ, proto, len_lo, len_hi]. */
static uint8_t cmd_read(xum_fw_t *fw, uint8_t proto, uint16_t len)
{
    return bulk4(fw, UFT_XUM1541_BULK_READ, proto,
                 (uint8_t)(len & 0xFF), (uint8_t)(len >> 8));
}

/** Full ATN write: WRITE cmd with CBM|ATN|extra_flags + ATN payload.
 *  Returns the final status (command-phase error or payload status). */
static uint8_t atn_write(xum_fw_t *fw, uint8_t extra_flags,
                         const uint8_t *atn, uint16_t n)
{
    uint8_t st = cmd_write(fw, (uint8_t)(UFT_XUM1541_PROTO_CBM
                                         | UFT_XUM1541_FLAG_WRITE_ATN
                                         | extra_flags), n);
    if (st != XUM_FW_NO_STATUS) return st;   /* command-phase error */
    return xum_fw_bulk_write_payload(fw, atn, n);
}

/** IEC listen(dev,sec) exactly as OpenCBM archlib.c encodes it. */
static uint8_t iec_listen(xum_fw_t *fw, uint8_t dev, uint8_t sec)
{
    uint8_t atn[2] = { (uint8_t)(UFT_IEC_LISTEN | dev),
                       (uint8_t)(UFT_IEC_DATA | sec) };
    return atn_write(fw, 0, atn, 2);
}

/** IEC talk(dev,sec): ATN bytes + FLAG_WRITE_TALK for the turnaround. */
static uint8_t iec_talk(xum_fw_t *fw, uint8_t dev, uint8_t sec)
{
    uint8_t atn[2] = { (uint8_t)(UFT_IEC_TALK | dev),
                       (uint8_t)(UFT_IEC_DATA | sec) };
    return atn_write(fw, UFT_XUM1541_FLAG_WRITE_TALK, atn, 2);
}

static uint8_t iec_unlisten(xum_fw_t *fw)
{
    uint8_t atn = UFT_IEC_UNLISTEN;
    return atn_write(fw, 0, &atn, 1);
}

static uint8_t iec_untalk(xum_fw_t *fw)
{
    uint8_t atn = UFT_IEC_UNTALK;
    return atn_write(fw, 0, &atn, 1);
}

/** Plain CBM data write (cmd + payload), as uft_xum_iec_write does. */
static uint8_t data_write(xum_fw_t *fw, const uint8_t *data, uint16_t len)
{
    uint8_t st = cmd_write(fw, UFT_XUM1541_PROTO_CBM, len);
    if (st != XUM_FW_NO_STATUS) return st;
    return xum_fw_bulk_write_payload(fw, data, len);
}

static void setup_ready(xum_fw_t *fw)
{
    uint8_t info[8];
    xum_fw_power_on_defaults(fw);
    (void)xum_fw_ctrl(fw, UFT_XUM1541_CTRL_INIT, 0, info, sizeof(info));
}

static uft_xum_gcr_params_t default_gcr(uint64_t seed, int track)
{
    uft_xum_gcr_params_t p;
    memset(&p, 0, sizeof(p));
    p.seed  = seed;
    p.track = track;
    p.id1   = 0x41;   /* 'A' */
    p.id2   = 0x42;   /* 'B' */
    return p;
}

/* ─────────────────────────────────────────────────────────────────────
 *  A. Control transfers + identity
 * ───────────────────────────────────────────────────────────────────── */

TEST(power_on_state_is_disconnected) {
    xum_fw_t fw;
    xum_fw_reset(&fw);
    ASSERT(fw.state == XUM_FW_STATE_DISCONNECTED);
    ASSERT(fw.fw_version == XUM_FW_VERSION_DEFAULT);
    ASSERT(fw.cmd_count == 0);
    /* Disconnected device answers nothing. */
    uint8_t info[8];
    ASSERT(xum_fw_ctrl(&fw, UFT_XUM1541_CTRL_INIT, 0, info, 8)
           == XUM_FW_CTRL_REFUSED);
}

TEST(init_returns_8_byte_info_and_enters_ready) {
    xum_fw_t fw;
    xum_fw_power_on_defaults(&fw);
    uint8_t info[8] = {0};
    int n = xum_fw_ctrl(&fw, UFT_XUM1541_CTRL_INIT, 0, info, sizeof(info));
    ASSERT(n == 8);
    ASSERT(info[0] == XUM_FW_VERSION_DEFAULT);          /* fw version */
    ASSERT(info[1] == (XUM_FW_CAP_CBM | XUM_FW_CAP_NIB)); /* caps */
    ASSERT(info[2] & XUM_FW_INIT_DOING_RESET);          /* power-on flag */
    ASSERT(!(info[2] & XUM_FW_INIT_NO_DEVICE));         /* drive present */
    ASSERT(fw.state == XUM_FW_STATE_READY);
}

TEST(init_doing_reset_flag_reported_exactly_once) {
    xum_fw_t fw;
    xum_fw_power_on_defaults(&fw);
    uint8_t info[8] = {0};
    ASSERT(xum_fw_ctrl(&fw, UFT_XUM1541_CTRL_INIT, 0, info, 8) == 8);
    ASSERT(info[2] & XUM_FW_INIT_DOING_RESET);
    ASSERT(xum_fw_ctrl(&fw, UFT_XUM1541_CTRL_INIT, 0, info, 8) == 8);
    ASSERT(!(info[2] & XUM_FW_INIT_DOING_RESET));   /* cleared after 1st */
}

TEST(init_reports_no_device_flag_when_drive_absent) {
    xum_fw_t fw;
    xum_fw_power_on_defaults(&fw);
    xum_fw_set_drive_present(&fw, false);
    uint8_t info[8] = {0};
    ASSERT(xum_fw_ctrl(&fw, UFT_XUM1541_CTRL_INIT, 0, info, 8) == 8);
    ASSERT(info[2] & XUM_FW_INIT_NO_DEVICE);
}

TEST(echo_legal_before_init) {
    xum_fw_t fw;
    xum_fw_power_on_defaults(&fw);   /* CONNECTED, no INIT yet */
    uint8_t echo[2] = {0};
    ASSERT(xum_fw_ctrl(&fw, UFT_XUM1541_CTRL_ECHO, 0xA55A, echo, 2) == 2);
    ASSERT(echo[0] == 0x5A && echo[1] == 0xA5);   /* LE16 */
}

TEST(bulk_command_before_init_enters_sticky_error) {
    xum_fw_t fw;
    xum_fw_power_on_defaults(&fw);   /* CONNECTED — INIT missing */
    ASSERT(cmd_write(&fw, UFT_XUM1541_PROTO_CBM | UFT_XUM1541_FLAG_WRITE_ATN, 2)
           == UFT_XUM1541_STATUS_ERROR);
    ASSERT(fw.state == XUM_FW_STATE_ERROR);
    ASSERT(fw.sticky_error == XUM_FW_ERR_NOT_INITIALIZED);
    /* Sticky: further bulk commands keep failing... */
    ASSERT(bulk4(&fw, UFT_XUM1541_IOCTL_IEC_POLL, 0, 0, 0)
           == UFT_XUM1541_STATUS_ERROR);
    /* ...until INIT recovers. */
    uint8_t info[8];
    ASSERT(xum_fw_ctrl(&fw, UFT_XUM1541_CTRL_INIT, 0, info, 8) == 8);
    ASSERT(fw.state == XUM_FW_STATE_READY);
    ASSERT(fw.sticky_error == XUM_FW_ERR_NONE);
}

TEST(enter_bootloader_always_refused) {
    xum_fw_t fw;
    setup_ready(&fw);
    uint8_t buf[8];
    ASSERT(xum_fw_ctrl(&fw, UFT_XUM1541_CTRL_ENTER_BOOTLOADER, 0, buf, 8)
           == XUM_FW_CTRL_REFUSED);
    ASSERT(fw.bootloader_refusals == 1);
    /* Firmware must remain fully usable afterwards. */
    ASSERT(fw.state == XUM_FW_STATE_READY);
    ASSERT(iec_listen(&fw, 8, 15) == UFT_XUM1541_STATUS_READY);
}

TEST(shutdown_blocks_further_commands) {
    xum_fw_t fw;
    setup_ready(&fw);
    uint8_t dummy = 0xAA;
    ASSERT(xum_fw_ctrl(&fw, UFT_XUM1541_CTRL_SHUTDOWN, 0, &dummy, 1) == 1);
    ASSERT(fw.state == XUM_FW_STATE_SHUTDOWN);
    /* Everything afterwards: no response. */
    uint8_t info[8];
    ASSERT(xum_fw_ctrl(&fw, UFT_XUM1541_CTRL_INIT, 0, info, 8)
           == XUM_FW_CTRL_REFUSED);
    ASSERT(cmd_write(&fw, UFT_XUM1541_PROTO_CBM, 4) == XUM_FW_NO_STATUS);
}

TEST(ctrl_reset_clears_sticky_error_and_bus_role) {
    xum_fw_t fw;
    setup_ready(&fw);
    ASSERT(iec_talk(&fw, 8, 15) == UFT_XUM1541_STATUS_READY);
    /* Force a sticky violation: listen-ATN while TALKING. */
    ASSERT(iec_listen(&fw, 8, 15) == UFT_XUM1541_STATUS_ERROR);
    ASSERT(fw.state == XUM_FW_STATE_ERROR);
    /* CTRL_RESET recovers to READY with idle bus. */
    ASSERT(xum_fw_ctrl(&fw, UFT_XUM1541_CTRL_RESET, 0, NULL, 0) == 0);
    ASSERT(fw.state == XUM_FW_STATE_READY);
    ASSERT(fw.sticky_error == XUM_FW_ERR_NONE);
    ASSERT(fw.iec_lines == 0);
}

TEST(version_strings_answered) {
    xum_fw_t fw;
    xum_fw_power_on_defaults(&fw);
    xum_fw_set_version_strings(&fw, "deadbee", "13.1.0", "2.1.0");
    uint8_t buf[XUM_FW_VERSTR_MAX] = {0};
    int n = xum_fw_ctrl(&fw, UFT_XUM1541_CTRL_GITREV, 0, buf, sizeof(buf));
    ASSERT(n == 7 && memcmp(buf, "deadbee", 7) == 0);
    n = xum_fw_ctrl(&fw, UFT_XUM1541_CTRL_GCCVER, 0, buf, sizeof(buf));
    ASSERT(n == 6 && memcmp(buf, "13.1.0", 6) == 0);
    n = xum_fw_ctrl(&fw, UFT_XUM1541_CTRL_LIBCVER, 0, buf, sizeof(buf));
    ASSERT(n == 5 && memcmp(buf, "2.1.0", 5) == 0);
}

/* ─────────────────────────────────────────────────────────────────────
 *  B. Bulk wire format + IEC ATN addressing
 * ───────────────────────────────────────────────────────────────────── */

TEST(atn_listen_sets_listening_and_secondary) {
    xum_fw_t fw;
    setup_ready(&fw);
    ASSERT(iec_listen(&fw, 8, 15) == UFT_XUM1541_STATUS_READY);
    ASSERT(fw.state == XUM_FW_STATE_LISTENING);
    ASSERT(fw.current_secondary == 15);
    /* WRITE status extended value = ATN bytes clocked (2). */
    ASSERT(fw.last_status_val == 2);
}

TEST(atn_talk_sets_talking_and_secondary) {
    xum_fw_t fw;
    setup_ready(&fw);
    ASSERT(iec_talk(&fw, 8, 2) == UFT_XUM1541_STATUS_READY);
    ASSERT(fw.state == XUM_FW_STATE_TALKING);
    ASSERT(fw.current_secondary == 2);
    ASSERT(fw.last_status_val == 2);
}

TEST(atn_unlisten_returns_to_ready) {
    xum_fw_t fw;
    setup_ready(&fw);
    ASSERT(iec_listen(&fw, 8, 15) == UFT_XUM1541_STATUS_READY);
    ASSERT(iec_unlisten(&fw) == UFT_XUM1541_STATUS_READY);
    ASSERT(fw.state == XUM_FW_STATE_READY);
    ASSERT(fw.last_status_val == 1);   /* one ATN byte clocked */
}

TEST(atn_untalk_returns_to_ready) {
    xum_fw_t fw;
    setup_ready(&fw);
    ASSERT(iec_talk(&fw, 8, 15) == UFT_XUM1541_STATUS_READY);
    ASSERT(iec_untalk(&fw) == UFT_XUM1541_STATUS_READY);
    ASSERT(fw.state == XUM_FW_STATE_READY);
}

TEST(atn_unlisten_untalk_idle_is_legal_noop) {
    xum_fw_t fw;
    setup_ready(&fw);
    ASSERT(iec_unlisten(&fw) == UFT_XUM1541_STATUS_READY);
    ASSERT(iec_untalk(&fw)   == UFT_XUM1541_STATUS_READY);
    ASSERT(fw.state == XUM_FW_STATE_READY);
}

TEST(atn_byte_0x3F_is_unlisten_not_device_31) {
    /* 0x20 | 31 == 0x3F == UNLISTEN: device 31 is unencodable on the
     * IEC wire by design. The pre-MF-301 fictional opcode table let a
     * "device 31" through as an argument byte — the real wire cannot. */
    xum_fw_t fw;
    setup_ready(&fw);
    ASSERT((UFT_IEC_LISTEN | 31) == UFT_IEC_UNLISTEN);
    ASSERT((UFT_IEC_TALK   | 31) == UFT_IEC_UNTALK);
    uint8_t atn = (uint8_t)(UFT_IEC_LISTEN | 31);
    ASSERT(atn_write(&fw, 0, &atn, 1) == UFT_XUM1541_STATUS_READY);
    /* Parsed as UNLISTEN: bus stays idle, no device addressed. */
    ASSERT(fw.state == XUM_FW_STATE_READY);
    ASSERT(fw.last_error == XUM_FW_ERR_NONE);
}

TEST(listen_then_data_write_reports_byte_count) {
    xum_fw_t fw;
    setup_ready(&fw);
    ASSERT(iec_listen(&fw, 8, 15) == UFT_XUM1541_STATUS_READY);
    /* Plain CBM data write: cmd -> payload -> status; the status
     * extended value = bytes actually transferred on the IEC bus. */
    const uint8_t payload[4] = { 'I', '0', 0x0D, 0x00 };
    ASSERT(data_write(&fw, payload, 4) == UFT_XUM1541_STATUS_READY);
    ASSERT(fw.last_status_val == 4);
    ASSERT(fw.listen_bytes_accepted == 4);
}

TEST(data_write_without_listener_refused) {
    xum_fw_t fw;
    setup_ready(&fw);
    ASSERT(cmd_write(&fw, UFT_XUM1541_PROTO_CBM, 4)
           == UFT_XUM1541_STATUS_ERROR);
    ASSERT(fw.state == XUM_FW_STATE_ERROR);
    ASSERT(fw.sticky_error == XUM_FW_ERR_NO_LISTENER);
}

TEST(talk_then_read_drains_stream_no_status_phase) {
    xum_fw_t fw;
    setup_ready(&fw);
    const uint8_t stream[6] = { 1, 2, 3, 4, 5, 6 };
    xum_fw_load_talk_stream(&fw, stream, sizeof(stream));
    ASSERT(iec_talk(&fw, 8, 15) == UFT_XUM1541_STATUS_READY);
    ASSERT(fw.state == XUM_FW_STATE_TALKING);
    /* READ: data phase follows the command directly. */
    ASSERT(cmd_read(&fw, UFT_XUM1541_PROTO_CBM, 6) == XUM_FW_NO_STATUS);
    uint8_t out[8] = {0};
    uint16_t got = 0;
    ASSERT(xum_fw_bulk_read_payload(&fw, out, sizeof(out), &got)
           == UFT_XUM1541_STATUS_READY);
    ASSERT(got == 6);
    ASSERT(memcmp(out, stream, 6) == 0);
    ASSERT(fw.eoi_flag == true);   /* stream exhausted => EOI */
}

TEST(read_without_talker_refused) {
    xum_fw_t fw;
    setup_ready(&fw);
    ASSERT(cmd_read(&fw, UFT_XUM1541_PROTO_CBM, 16)
           == UFT_XUM1541_STATUS_ERROR);
    ASSERT(fw.state == XUM_FW_STATE_ERROR);
    ASSERT(fw.sticky_error == XUM_FW_ERR_NO_TALKER);
}

TEST(listen_while_talking_is_sticky_role_conflict) {
    xum_fw_t fw;
    setup_ready(&fw);
    ASSERT(iec_talk(&fw, 8, 15) == UFT_XUM1541_STATUS_READY);
    ASSERT(iec_listen(&fw, 8, 15) == UFT_XUM1541_STATUS_ERROR);
    ASSERT(fw.state == XUM_FW_STATE_ERROR);
    ASSERT(fw.sticky_error == XUM_FW_ERR_BUS_ROLE_CONFLICT);
    /* Extended value = ATN bytes clocked before the failing byte (0:
     * the very first byte was the conflicting listen address). */
    ASSERT(fw.last_status_val == 0);
}

TEST(talk_while_listening_is_sticky_role_conflict) {
    xum_fw_t fw;
    setup_ready(&fw);
    ASSERT(iec_listen(&fw, 8, 15) == UFT_XUM1541_STATUS_READY);
    ASSERT(iec_talk(&fw, 8, 15) == UFT_XUM1541_STATUS_ERROR);
    ASSERT(fw.state == XUM_FW_STATE_ERROR);
    ASSERT(fw.sticky_error == XUM_FW_ERR_BUS_ROLE_CONFLICT);
}

TEST(listen_to_absent_device_is_transient_error) {
    xum_fw_t fw;
    setup_ready(&fw);
    xum_fw_set_drive_present(&fw, false);
    ASSERT(iec_listen(&fw, 8, 15) == UFT_XUM1541_STATUS_ERROR);
    ASSERT(fw.last_error == XUM_FW_ERR_DEVICE_NOT_PRESENT);
    /* Extended value = bytes clocked before the timeout (0 here). */
    ASSERT(fw.last_status_val == 0);
    /* Transient — the adapter itself stays usable (real IEC timeout
     * does not wedge the bridge). */
    ASSERT(fw.state == XUM_FW_STATE_READY);
    xum_fw_set_drive_present(&fw, true);
    ASSERT(iec_listen(&fw, 8, 15) == UFT_XUM1541_STATUS_READY);
}

TEST(talk_to_wrong_device_number_is_transient_error) {
    xum_fw_t fw;
    setup_ready(&fw);   /* drive is at 8 */
    ASSERT(iec_talk(&fw, 9, 15) == UFT_XUM1541_STATUS_ERROR);
    ASSERT(fw.last_error == XUM_FW_ERR_DEVICE_NOT_PRESENT);
    ASSERT(fw.state == XUM_FW_STATE_READY);
    /* Adapter recovers without INIT/RESET. */
    ASSERT(iec_talk(&fw, 8, 15) == UFT_XUM1541_STATUS_READY);
}

TEST(talk_atn_without_talk_flag_is_sticky_bad_protocol) {
    /* A talk-address ATN byte without FLAG_WRITE_TALK means the
     * firmware skips the bus turnaround — on real HW the bus wedges.
     * The sim fails loudly instead (DIVERGENCES.md X-D12), catching a
     * HAL that forgets the flag. */
    xum_fw_t fw;
    setup_ready(&fw);
    uint8_t atn[2] = { (uint8_t)(UFT_IEC_TALK | 8),
                       (uint8_t)(UFT_IEC_DATA | 15) };
    ASSERT(atn_write(&fw, /*extra_flags=*/0, atn, 2)
           == UFT_XUM1541_STATUS_ERROR);
    ASSERT(fw.state == XUM_FW_STATE_ERROR);
    ASSERT(fw.sticky_error == XUM_FW_ERR_BAD_PROTOCOL);
}

TEST(open_close_atn_bytes_clocked_through) {
    /* OPEN (0xF0|sec) / CLOSE (0xE0|sec) are drive-side commands the
     * firmware clocks verbatim; there are NO open/close bulk opcodes
     * (the pre-MF-301 table invented them). Drive-side file semantics
     * are not modelled (X-D2) — the bytes still count as transferred. */
    xum_fw_t fw;
    setup_ready(&fw);
    /* OPEN sequence: ATN(listen 8, open ch2), then the filename as a
     * plain data write AFTER ATN release, then ATN(close ch2, unlisten). */
    uint8_t atn_open[2] = { (uint8_t)(UFT_IEC_LISTEN | 8),
                            (uint8_t)(UFT_IEC_OPEN | 2) };
    ASSERT(atn_write(&fw, 0, atn_open, 2) == UFT_XUM1541_STATUS_READY);
    ASSERT(fw.last_status_val == 2);
    ASSERT(fw.state == XUM_FW_STATE_LISTENING);
    const uint8_t fname[1] = { 0x24 };   /* '$' — directory */
    ASSERT(data_write(&fw, fname, 1) == UFT_XUM1541_STATUS_READY);
    uint8_t atn_close[2] = { (uint8_t)(UFT_IEC_CLOSE | 2),
                             UFT_IEC_UNLISTEN };
    ASSERT(atn_write(&fw, 0, atn_close, 2) == UFT_XUM1541_STATUS_READY);
    ASSERT(fw.state == XUM_FW_STATE_READY);
}

TEST(unknown_opcode_rejected_sticky) {
    /* Every pre-MF-301 fictional opcode (WRITE_DATA=0, TALK=1,
     * LISTEN=2, UNLISTEN=3, UNTALK=4, READ_DATA=7) must be rejected —
     * the real firmware knows only READ(8)/WRITE(9)/ioctls(16+). */
    const uint8_t fictional[] = { 0, 1, 2, 3, 4, 7 };
    for (size_t i = 0; i < sizeof(fictional); i++) {
        xum_fw_t fw;
        setup_ready(&fw);
        ASSERT(bulk4(&fw, fictional[i], 8, 15, 0)
               == UFT_XUM1541_STATUS_ERROR);
        ASSERT(fw.state == XUM_FW_STATE_ERROR);
        ASSERT(fw.sticky_error == XUM_FW_ERR_BAD_OPCODE);
    }
}

TEST(unknown_proto_nibble_rejected_sticky) {
    /* Proto nibble 0 (no protocol) and 7+ (beyond NIB) do not exist. */
    xum_fw_t fw;
    setup_ready(&fw);
    ASSERT(cmd_write(&fw, 0x00, 2) == UFT_XUM1541_STATUS_ERROR);
    ASSERT(fw.sticky_error == XUM_FW_ERR_BAD_PROTOCOL);
    setup_ready(&fw);
    ASSERT(cmd_read(&fw, (uint8_t)(7 << 4), 2) == UFT_XUM1541_STATUS_ERROR);
    ASSERT(fw.state == XUM_FW_STATE_ERROR);
    ASSERT(fw.sticky_error == XUM_FW_ERR_BAD_PROTOCOL);
}

TEST(unmodelled_real_protos_refused_transiently) {
    /* S1/S2/PP/P2/NIB are REAL firmware protocols this sim does not
     * model (X-D13): honest transient refusal, never fake success,
     * never a wedge. */
    const uint8_t protos[] = { UFT_XUM1541_PROTO_S1, UFT_XUM1541_PROTO_S2,
                               UFT_XUM1541_PROTO_PP, UFT_XUM1541_PROTO_P2,
                               UFT_XUM1541_PROTO_NIB };
    xum_fw_t fw;
    setup_ready(&fw);
    for (size_t i = 0; i < sizeof(protos); i++) {
        ASSERT(cmd_write(&fw, protos[i], 2) == UFT_XUM1541_STATUS_ERROR);
        ASSERT(fw.last_error == XUM_FW_ERR_NOT_MODELLED);
        ASSERT(fw.state == XUM_FW_STATE_READY);   /* not sticky */
    }
}

TEST(write_length_overrun_is_sticky_error) {
    xum_fw_t fw;
    setup_ready(&fw);
    ASSERT(iec_listen(&fw, 8, 15) == UFT_XUM1541_STATUS_READY);
    /* Announce 0x8004 (32772) > MAX_XFER_SIZE: firmware buffer overrun. */
    ASSERT(cmd_write(&fw, UFT_XUM1541_PROTO_CBM,
                     (uint16_t)(UFT_XUM1541_MAX_XFER_SIZE + 4))
           == UFT_XUM1541_STATUS_ERROR);
    ASSERT(fw.state == XUM_FW_STATE_ERROR);
    ASSERT(fw.sticky_error == XUM_FW_ERR_OVERRUN);
}

TEST(write_payload_length_mismatch_is_sticky_error) {
    xum_fw_t fw;
    setup_ready(&fw);
    ASSERT(iec_listen(&fw, 8, 15) == UFT_XUM1541_STATUS_READY);
    ASSERT(cmd_write(&fw, UFT_XUM1541_PROTO_CBM, 8) == XUM_FW_NO_STATUS);
    const uint8_t payload[4] = {0};
    /* Announced 8, delivered 4 — FIFO accounting broken. */
    ASSERT(xum_fw_bulk_write_payload(&fw, payload, 4)
           == UFT_XUM1541_STATUS_ERROR);
    ASSERT(fw.sticky_error == XUM_FW_ERR_OVERRUN);
}

TEST(write_payload_without_command_is_sticky_error) {
    /* A payload with no preceding WRITE header would be misparsed as a
     * command by the real firmware — the sim fails loudly. */
    xum_fw_t fw;
    setup_ready(&fw);
    const uint8_t payload[2] = { 0x28, 0x6F };
    ASSERT(xum_fw_bulk_write_payload(&fw, payload, 2)
           == UFT_XUM1541_STATUS_ERROR);
    ASSERT(fw.state == XUM_FW_STATE_ERROR);
    ASSERT(fw.sticky_error == XUM_FW_ERR_BAD_PROTOCOL);
}

/* ─────────────────────────────────────────────────────────────────────
 *  C. IOCTLs, EOI + status wire format
 * ───────────────────────────────────────────────────────────────────── */

TEST(get_eoi_clear_eoi_roundtrip) {
    xum_fw_t fw;
    setup_ready(&fw);
    ASSERT(bulk4(&fw, UFT_XUM1541_IOCTL_GET_EOI, 0, 0, 0)
           == UFT_XUM1541_STATUS_READY);
    ASSERT(fw.last_status_val == 0);   /* no EOI yet */
    /* Drain a stream to completion -> EOI set. */
    const uint8_t stream[2] = { 0xAA, 0xBB };
    xum_fw_load_talk_stream(&fw, stream, 2);
    ASSERT(iec_talk(&fw, 8, 15) == UFT_XUM1541_STATUS_READY);
    ASSERT(cmd_read(&fw, UFT_XUM1541_PROTO_CBM, 2) == XUM_FW_NO_STATUS);
    uint8_t out[4]; uint16_t got = 0;
    ASSERT(xum_fw_bulk_read_payload(&fw, out, 4, &got)
           == UFT_XUM1541_STATUS_READY);
    ASSERT(bulk4(&fw, UFT_XUM1541_IOCTL_GET_EOI, 0, 0, 0)
           == UFT_XUM1541_STATUS_READY);
    ASSERT(fw.last_status_val == 1);
    ASSERT(bulk4(&fw, UFT_XUM1541_IOCTL_CLEAR_EOI, 0, 0, 0)
           == UFT_XUM1541_STATUS_READY);
    ASSERT(bulk4(&fw, UFT_XUM1541_IOCTL_GET_EOI, 0, 0, 0)
           == UFT_XUM1541_STATUS_READY);
    ASSERT(fw.last_status_val == 0);
}

TEST(iec_poll_returns_line_mask_in_status_value) {
    /* IOCTL result travels in the status extended value — exactly what
     * uft_xum_iec_poll() extracts via UFT_XUM1541_GET_STATUS_VAL. */
    xum_fw_t fw;
    setup_ready(&fw);
    xum_fw_set_iec_lines(&fw, XUM_FW_IEC_CLOCK | XUM_FW_IEC_DATA);
    ASSERT(bulk4(&fw, UFT_XUM1541_IOCTL_IEC_POLL, 0, 0, 0)
           == UFT_XUM1541_STATUS_READY);
    ASSERT(fw.last_status_val == (XUM_FW_IEC_CLOCK | XUM_FW_IEC_DATA));
    /* Serialized wire form round-trips through the HAL's accessors. */
    uint8_t buf[UFT_XUM1541_STATUSBUF_SIZE];
    xum_fw_status_serialize(UFT_XUM1541_STATUS_READY, fw.last_status_val, buf);
    ASSERT(UFT_XUM1541_GET_STATUS(buf) == UFT_XUM1541_STATUS_READY);
    ASSERT(UFT_XUM1541_GET_STATUS_VAL(buf)
           == (XUM_FW_IEC_CLOCK | XUM_FW_IEC_DATA));
}

TEST(iec_wait_ready_on_match_busy_otherwise) {
    xum_fw_t fw;
    setup_ready(&fw);
    xum_fw_set_iec_lines(&fw, XUM_FW_IEC_CLOCK);
    /* Wait for CLOCK set: already true -> READY. */
    ASSERT(bulk4(&fw, UFT_XUM1541_IOCTL_IEC_WAIT, XUM_FW_IEC_CLOCK, 1, 0)
           == UFT_XUM1541_STATUS_READY);
    /* Wait for DATA set: not true -> BUSY (sim has no time model). */
    ASSERT(bulk4(&fw, UFT_XUM1541_IOCTL_IEC_WAIT, XUM_FW_IEC_DATA, 1, 0)
           == UFT_XUM1541_STATUS_BUSY);
    /* Wait for DATA released: true -> READY. */
    ASSERT(bulk4(&fw, UFT_XUM1541_IOCTL_IEC_WAIT, XUM_FW_IEC_DATA, 0, 0)
           == UFT_XUM1541_STATUS_READY);
}

TEST(iec_setrelease_updates_lines) {
    xum_fw_t fw;
    setup_ready(&fw);
    ASSERT(bulk4(&fw, UFT_XUM1541_IOCTL_IEC_SETRELEASE,
                 XUM_FW_IEC_ATN | XUM_FW_IEC_CLOCK, 0, 0)
           == UFT_XUM1541_STATUS_READY);
    ASSERT(fw.iec_lines == (XUM_FW_IEC_ATN | XUM_FW_IEC_CLOCK));
    ASSERT(bulk4(&fw, UFT_XUM1541_IOCTL_IEC_SETRELEASE,
                 0, XUM_FW_IEC_ATN, 0)
           == UFT_XUM1541_STATUS_READY);
    ASSERT(fw.iec_lines == XUM_FW_IEC_CLOCK);
}

TEST(pp_write_read_roundtrip) {
    /* Parallel-port latch: PP_WRITE sets, PP_READ returns the value in
     * the status extended value. */
    xum_fw_t fw;
    setup_ready(&fw);
    ASSERT(bulk4(&fw, UFT_XUM1541_IOCTL_PP_READ, 0, 0, 0)
           == UFT_XUM1541_STATUS_READY);
    ASSERT(fw.last_status_val == 0);
    ASSERT(bulk4(&fw, UFT_XUM1541_IOCTL_PP_WRITE, 0x5A, 0, 0)
           == UFT_XUM1541_STATUS_READY);
    ASSERT(bulk4(&fw, UFT_XUM1541_IOCTL_PP_READ, 0, 0, 0)
           == UFT_XUM1541_STATUS_READY);
    ASSERT(fw.last_status_val == 0x5A);
}

TEST(parburst_refused_honestly) {
    /* PARBURST_READ/WRITE (30/31) need a drive-side nibbler routine —
     * honest transient refusal, adapter stays usable (X-D13). */
    xum_fw_t fw;
    setup_ready(&fw);
    ASSERT(bulk4(&fw, UFT_XUM1541_IOCTL_PARBURST_READ, 0, 0, 0)
           == UFT_XUM1541_STATUS_ERROR);
    ASSERT(fw.last_error == XUM_FW_ERR_NOT_MODELLED);
    ASSERT(fw.state == XUM_FW_STATE_READY);
    ASSERT(bulk4(&fw, UFT_XUM1541_IOCTL_PARBURST_WRITE, 0, 0, 0)
           == UFT_XUM1541_STATUS_ERROR);
    ASSERT(fw.last_error == XUM_FW_ERR_NOT_MODELLED);
    ASSERT(fw.state == XUM_FW_STATE_READY);
}

TEST(status_buf_serialization_is_3_bytes_le) {
    /* OpenCBM XUM_STATUSBUF_SIZE = 3: [code, val_lo, val_hi]. The HAL
     * reads exactly this via xum_wait_status() since MF-301
     * (X-DELTA-1 RESOLVED). This test pins the wire format on the
     * firmware side; the HAL macros must agree. */
    uint8_t buf[UFT_XUM1541_STATUSBUF_SIZE] = {0};
    xum_fw_status_serialize(UFT_XUM1541_STATUS_READY, 0x1234, buf);
    ASSERT(buf[0] == UFT_XUM1541_STATUS_READY);
    ASSERT(buf[1] == 0x34);
    ASSERT(buf[2] == 0x12);
    ASSERT(UFT_XUM1541_GET_STATUS(buf) == UFT_XUM1541_STATUS_READY);
    ASSERT(UFT_XUM1541_GET_STATUS_VAL(buf) == 0x1234);
}

/* ─────────────────────────────────────────────────────────────────────
 *  D. GCR generator: determinism, structure, defects
 * ───────────────────────────────────────────────────────────────────── */

TEST(gcr_encode_decode_roundtrip_all_byte_values) {
    for (int v = 0; v < 256; v++) {
        uint8_t in[4]  = { (uint8_t)v, (uint8_t)(255 - v),
                           (uint8_t)(v ^ 0x5A), (uint8_t)(v >> 1) };
        uint8_t gcr[5], out[4];
        uft_xum_gcr_encode4(in, gcr);
        ASSERT(uft_xum_gcr_decode5(gcr, out) == 0);
        ASSERT(memcmp(in, out, 4) == 0);
    }
}

TEST(gcr_track_clean_is_deterministic) {
    uft_xum_gcr_params_t p = default_gcr(0xC0FFEE, 1);
    uft_xum_gcr_track_t a = {0}, b = {0};
    ASSERT(uft_xum_gcr_gen_track(&p, &a) == UFT_XUM_GCR_OK);
    ASSERT(uft_xum_gcr_gen_track(&p, &b) == UFT_XUM_GCR_OK);
    ASSERT(a.len == b.len);
    ASSERT(memcmp(a.bytes, b.bytes, a.len) == 0);
    /* Different seed differs. */
    uft_xum_gcr_params_t p2 = p; p2.seed = 0xC0FFEF;
    uft_xum_gcr_track_t c = {0};
    ASSERT(uft_xum_gcr_gen_track(&p2, &c) == UFT_XUM_GCR_OK);
    ASSERT(memcmp(a.bytes, c.bytes, a.len) != 0);
    uft_xum_gcr_free(&a); uft_xum_gcr_free(&b); uft_xum_gcr_free(&c);
}

TEST(gcr_track_len_equals_zone_budget) {
    const int probe_tracks[] = { 1, 17, 18, 24, 25, 30, 31, 35, 42 };
    for (size_t i = 0; i < sizeof(probe_tracks) / sizeof(int); i++) {
        uft_xum_gcr_params_t p = default_gcr(7, probe_tracks[i]);
        uft_xum_gcr_track_t t = {0};
        ASSERT(uft_xum_gcr_gen_track(&p, &t) == UFT_XUM_GCR_OK);
        ASSERT(t.len == t.budget);
        ASSERT(t.budget == uft_xum_gcr_zone_budget(t.speed_zone));
        ASSERT(uft_xum_gcr_count_unsafe(&t) == 0);
        uft_xum_gcr_free(&t);
    }
}

TEST(gcr_zone_tables_match_hal_ssot) {
    /* Cross-check the generator's zone tables against the HAL SSOT
     * uft_xum_sectors_for_track() (src/hal/uft_xum1541.c) for every
     * legal 1541 track. No duplicated truth allowed to drift. */
    for (int track = 1; track <= 42; track++) {
        int zone = uft_xum_gcr_speed_zone(track);
        ASSERT(zone >= 0 && zone <= 3);
        ASSERT(uft_xum_gcr_sectors_for_zone(zone) ==
               uft_xum_sectors_for_track(UFT_CBM_DRIVE_1541, track));
    }
    /* Out-of-bounds agreement too. */
    ASSERT(uft_xum_gcr_speed_zone(0)  == -1);
    ASSERT(uft_xum_gcr_speed_zone(43) == -1);
    ASSERT(uft_xum_sectors_for_track(UFT_CBM_DRIVE_1541, 43) == 0);
}

TEST(gcr_track_sync_count_is_2x_sectors) {
    uft_xum_gcr_params_t p = default_gcr(3, 20);   /* zone 2: 19 sectors */
    uft_xum_gcr_track_t t = {0};
    ASSERT(uft_xum_gcr_gen_track(&p, &t) == UFT_XUM_GCR_OK);
    ASSERT(t.sector_count == 19);
    ASSERT(uft_xum_gcr_count_syncs(t.bytes, t.len) == 2u * 19u);
    uft_xum_gcr_free(&t);
}

TEST(gcr_track_headers_parse_with_valid_checksums) {
    uft_xum_gcr_params_t p = default_gcr(11, 5);   /* zone 3: 21 sectors */
    uft_xum_gcr_track_t t = {0};
    ASSERT(uft_xum_gcr_gen_track(&p, &t) == UFT_XUM_GCR_OK);
    uft_xum_gcr_header_t hdr[24];
    int n = uft_xum_gcr_parse_headers(t.bytes, t.len, hdr, 24);
    ASSERT(n == 21);
    for (int i = 0; i < n; i++) {
        ASSERT(hdr[i].sector == (uint8_t)i);   /* sequential layout */
        ASSERT(hdr[i].track == 5);
        ASSERT(hdr[i].id1 == 0x41 && hdr[i].id2 == 0x42);
        ASSERT(hdr[i].checksum_ok);
    }
    uft_xum_gcr_free(&t);
}

TEST(gcr_track_data_checksums_all_valid_when_clean) {
    uft_xum_gcr_params_t p = default_gcr(13, 33);   /* zone 0: 17 sectors */
    uft_xum_gcr_track_t t = {0};
    ASSERT(uft_xum_gcr_gen_track(&p, &t) == UFT_XUM_GCR_OK);
    uint8_t ok[24] = {0};
    int n = uft_xum_gcr_parse_data_blocks(t.bytes, t.len, ok, 24);
    ASSERT(n == 17);
    for (int i = 0; i < n; i++) ASSERT(ok[i] == 1);
    uft_xum_gcr_free(&t);
}

TEST(gcr_track_refuses_out_of_bounds_track) {
    uft_xum_gcr_track_t t = {0};
    uft_xum_gcr_params_t p = default_gcr(1, 0);
    ASSERT(uft_xum_gcr_gen_track(&p, &t) == UFT_XUM_GCR_ERR_OUT_OF_SPEC);
    p.track = 43;
    ASSERT(uft_xum_gcr_gen_track(&p, &t) == UFT_XUM_GCR_ERR_OUT_OF_SPEC);
    p.track = -3;
    ASSERT(uft_xum_gcr_gen_track(&p, &t) == UFT_XUM_GCR_ERR_OUT_OF_SPEC);
    /* Half-track at the physical stop (42 + 0.5) also refused. */
    p.track = 42;
    p.defects = UFT_XUM_DEFECT_HALF_TRACK;
    ASSERT(uft_xum_gcr_gen_track(&p, &t) == UFT_XUM_GCR_ERR_OUT_OF_SPEC);
}

TEST(weak_bits_change_data_but_keep_sync_structure) {
    uft_xum_gcr_params_t pc = default_gcr(21, 10);
    uft_xum_gcr_params_t pw = pc;
    pw.defects = UFT_XUM_DEFECT_WEAK_BITS;
    uft_xum_gcr_track_t c = {0}, w = {0};
    ASSERT(uft_xum_gcr_gen_track(&pc, &c) == UFT_XUM_GCR_OK);
    ASSERT(uft_xum_gcr_gen_track(&pw, &w) == UFT_XUM_GCR_OK);
    ASSERT(c.len == w.len);
    ASSERT(memcmp(c.bytes, w.bytes, c.len) != 0);
    /* Sync structure identical: 2 per sector. */
    ASSERT(uft_xum_gcr_count_syncs(w.bytes, w.len) ==
           uft_xum_gcr_count_syncs(c.bytes, c.len));
    /* Sector 0's data block no longer decodes cleanly. */
    uint8_t ok[24] = {0};
    int n = uft_xum_gcr_parse_data_blocks(w.bytes, w.len, ok, 24);
    ASSERT(n == 21);
    ASSERT(ok[0] == 0);
    for (int i = 1; i < n; i++) ASSERT(ok[i] == 1);
    uft_xum_gcr_free(&c); uft_xum_gcr_free(&w);
}

TEST(checksum_error_breaks_exactly_sector0_data) {
    uft_xum_gcr_params_t p = default_gcr(22, 10);
    p.defects = UFT_XUM_DEFECT_CHECKSUM_ERROR;
    uft_xum_gcr_track_t t = {0};
    ASSERT(uft_xum_gcr_gen_track(&p, &t) == UFT_XUM_GCR_OK);
    uint8_t ok[24] = {0};
    int n = uft_xum_gcr_parse_data_blocks(t.bytes, t.len, ok, 24);
    ASSERT(n == 21);
    ASSERT(ok[0] == 0);                       /* 1541 DOS error 23 */
    for (int i = 1; i < n; i++) ASSERT(ok[i] == 1);
    /* Headers stay pristine — only the data checksum is wrong. */
    uft_xum_gcr_header_t hdr[24];
    ASSERT(uft_xum_gcr_parse_headers(t.bytes, t.len, hdr, 24) == 21);
    for (int i = 0; i < 21; i++) ASSERT(hdr[i].checksum_ok);
    uft_xum_gcr_free(&t);
}

TEST(sync_long_creates_40_byte_run) {
    uft_xum_gcr_params_t p = default_gcr(23, 8);
    p.defects = UFT_XUM_DEFECT_SYNC_LONG;
    uft_xum_gcr_track_t t = {0};
    ASSERT(uft_xum_gcr_gen_track(&p, &t) == UFT_XUM_GCR_OK);
    ASSERT(uft_xum_gcr_max_sync_run(t.bytes, t.len)
           == UFT_XUM_GCR_LONG_SYNC_LEN);
    ASSERT(t.len == t.budget);   /* still fits the zone budget */
    ASSERT(uft_xum_gcr_count_unsafe(&t) == 0);
    uft_xum_gcr_free(&t);
}

TEST(sync_short_removes_one_detectable_sync) {
    uft_xum_gcr_params_t pc = default_gcr(24, 8);
    uft_xum_gcr_params_t ps = pc;
    ps.defects = UFT_XUM_DEFECT_SYNC_SHORT;
    uft_xum_gcr_track_t c = {0}, s = {0};
    ASSERT(uft_xum_gcr_gen_track(&pc, &c) == UFT_XUM_GCR_OK);
    ASSERT(uft_xum_gcr_gen_track(&ps, &s) == UFT_XUM_GCR_OK);
    /* Sector 1's data sync shrank to 1 byte = 8 bits < the 10-bit sync
     * threshold: one sync fewer, one data block undiscoverable. */
    ASSERT(uft_xum_gcr_count_syncs(s.bytes, s.len) ==
           uft_xum_gcr_count_syncs(c.bytes, c.len) - 1);
    uint8_t ok[24] = {0};
    ASSERT(uft_xum_gcr_parse_data_blocks(s.bytes, s.len, ok, 24) == 20);
    uft_xum_gcr_free(&c); uft_xum_gcr_free(&s);
}

TEST(killer_track_has_zero_syncs_and_full_budget) {
    uft_xum_gcr_params_t p = default_gcr(25, 36);   /* extended track */
    p.defects = UFT_XUM_DEFECT_KILLER_TRACK;
    uft_xum_gcr_track_t t = {0};
    ASSERT(uft_xum_gcr_gen_track(&p, &t) == UFT_XUM_GCR_OK);
    ASSERT(t.sector_count == 0);
    ASSERT(t.len == t.budget);
    ASSERT(uft_xum_gcr_count_syncs(t.bytes, t.len) == 0);
    for (size_t i = 0; i < t.len; i++) ASSERT(t.bytes[i] == 0x55);
    uft_xum_gcr_free(&t);
}

TEST(density_mismatch_forces_zone3_layout_on_inner_track) {
    uft_xum_gcr_params_t p = default_gcr(26, 35);   /* nominal zone 0 */
    p.defects = UFT_XUM_DEFECT_DENSITY_MISMATCH;
    uft_xum_gcr_track_t t = {0};
    ASSERT(uft_xum_gcr_gen_track(&p, &t) == UFT_XUM_GCR_OK);
    ASSERT(t.speed_zone == 3);                      /* written density */
    ASSERT(t.sector_count == 21);                   /* not the DOS 17 */
    ASSERT(t.budget == uft_xum_gcr_zone_budget(3)); /* 7692, not 6250 */
    ASSERT(uft_xum_gcr_count_unsafe(&t) == 0);
    /* Header track byte still says 35 — DOS sees an impossible track. */
    uft_xum_gcr_header_t hdr[24];
    ASSERT(uft_xum_gcr_parse_headers(t.bytes, t.len, hdr, 24) == 21);
    ASSERT(hdr[0].track == 35);
    uft_xum_gcr_free(&t);
}

TEST(half_track_is_deterministic_and_structurally_degraded) {
    uft_xum_gcr_params_t p = default_gcr(27, 17);
    p.defects = UFT_XUM_DEFECT_HALF_TRACK;
    uft_xum_gcr_track_t a = {0}, b = {0};
    ASSERT(uft_xum_gcr_gen_track(&p, &a) == UFT_XUM_GCR_OK);
    ASSERT(uft_xum_gcr_gen_track(&p, &b) == UFT_XUM_GCR_OK);
    ASSERT(a.len == b.len);
    ASSERT(memcmp(a.bytes, b.bytes, a.len) == 0);   /* reproducible */
    /* Crosstalk destroyed part of the sync coherence. */
    uft_xum_gcr_params_t pc = default_gcr(27, 17);
    uft_xum_gcr_track_t c = {0};
    ASSERT(uft_xum_gcr_gen_track(&pc, &c) == UFT_XUM_GCR_OK);
    size_t clean_syncs = uft_xum_gcr_count_syncs(c.bytes, c.len);
    size_t half_syncs  = uft_xum_gcr_count_syncs(a.bytes, a.len);
    ASSERT(half_syncs < clean_syncs);
    ASSERT(uft_xum_gcr_count_unsafe(&a) == 0);
    uft_xum_gcr_free(&a); uft_xum_gcr_free(&b); uft_xum_gcr_free(&c);
}

/* ─────────────────────────────────────────────────────────────────────
 *  E. End-to-end: generator output through the firmware TALK/READ path
 * ───────────────────────────────────────────────────────────────────── */

TEST(e2e_gcr_track_via_talk_read_pipeline) {
    uft_xum_gcr_params_t p = default_gcr(0xA1B2C3, 18);
    uft_xum_gcr_track_t t = {0};
    ASSERT(uft_xum_gcr_gen_track(&p, &t) == UFT_XUM_GCR_OK);

    xum_fw_t fw;
    setup_ready(&fw);
    xum_fw_load_talk_stream(&fw, t.bytes, t.len);
    ASSERT(iec_talk(&fw, 8, 15) == UFT_XUM1541_STATUS_READY);

    /* Drain in 1024-byte chunks, as a nibbler host loop would. */
    uint8_t *drained = (uint8_t *)malloc(t.len);
    ASSERT(drained != NULL);
    size_t pos = 0;
    while (pos < t.len) {
        uint16_t chunk = (uint16_t)((t.len - pos > 1024) ? 1024
                                                         : (t.len - pos));
        ASSERT(cmd_read(&fw, UFT_XUM1541_PROTO_CBM, chunk)
               == XUM_FW_NO_STATUS);
        uint16_t got = 0;
        ASSERT(xum_fw_bulk_read_payload(&fw, drained + pos, chunk, &got)
               == UFT_XUM1541_STATUS_READY);
        ASSERT(got == chunk);
        pos += got;
    }
    ASSERT(pos == t.len);
    ASSERT(memcmp(drained, t.bytes, t.len) == 0);
    ASSERT(fw.eoi_flag == true);
    /* The drained bytes still parse as a full GCR track. */
    uft_xum_gcr_header_t hdr[24];
    ASSERT(uft_xum_gcr_parse_headers(drained, t.len, hdr, 24) == 19);

    free(drained);
    uft_xum_gcr_free(&t);
    ASSERT(iec_untalk(&fw) == UFT_XUM1541_STATUS_READY);
    ASSERT(fw.state == XUM_FW_STATE_READY);
}

TEST(e2e_short_read_reports_partial_len_and_eoi) {
    const uint8_t stream[10] = { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };
    xum_fw_t fw;
    setup_ready(&fw);
    xum_fw_load_talk_stream(&fw, stream, sizeof(stream));
    ASSERT(iec_talk(&fw, 8, 15) == UFT_XUM1541_STATUS_READY);
    /* Request 64, only 10 available: short read + EOI, like a real
     * drive ending its transmission early. This is exactly the case
     * uft_xum_iec_read's bytes_read out-param (MF-301) preserves. */
    ASSERT(cmd_read(&fw, UFT_XUM1541_PROTO_CBM, 64) == XUM_FW_NO_STATUS);
    uint8_t out[64] = {0};
    uint16_t got = 0;
    ASSERT(xum_fw_bulk_read_payload(&fw, out, sizeof(out), &got)
           == UFT_XUM1541_STATUS_READY);
    ASSERT(got == 10);
    ASSERT(memcmp(out, stream, 10) == 0);
    ASSERT(fw.eoi_flag == true);
}

int main(void)
{
    printf("=== XUM1541 Firmware-Realistic Emulator + GCR-Gen Tests ===\n");
    fflush(stdout);

    printf("\n-- A. Control transfers + identity --\n");
    RUN(power_on_state_is_disconnected);
    RUN(init_returns_8_byte_info_and_enters_ready);
    RUN(init_doing_reset_flag_reported_exactly_once);
    RUN(init_reports_no_device_flag_when_drive_absent);
    RUN(echo_legal_before_init);
    RUN(bulk_command_before_init_enters_sticky_error);
    RUN(enter_bootloader_always_refused);
    RUN(shutdown_blocks_further_commands);
    RUN(ctrl_reset_clears_sticky_error_and_bus_role);
    RUN(version_strings_answered);

    printf("\n-- B. Bulk wire format + IEC ATN addressing --\n");
    RUN(atn_listen_sets_listening_and_secondary);
    RUN(atn_talk_sets_talking_and_secondary);
    RUN(atn_unlisten_returns_to_ready);
    RUN(atn_untalk_returns_to_ready);
    RUN(atn_unlisten_untalk_idle_is_legal_noop);
    RUN(atn_byte_0x3F_is_unlisten_not_device_31);
    RUN(listen_then_data_write_reports_byte_count);
    RUN(data_write_without_listener_refused);
    RUN(talk_then_read_drains_stream_no_status_phase);
    RUN(read_without_talker_refused);
    RUN(listen_while_talking_is_sticky_role_conflict);
    RUN(talk_while_listening_is_sticky_role_conflict);
    RUN(listen_to_absent_device_is_transient_error);
    RUN(talk_to_wrong_device_number_is_transient_error);
    RUN(talk_atn_without_talk_flag_is_sticky_bad_protocol);
    RUN(open_close_atn_bytes_clocked_through);
    RUN(unknown_opcode_rejected_sticky);
    RUN(unknown_proto_nibble_rejected_sticky);
    RUN(unmodelled_real_protos_refused_transiently);
    RUN(write_length_overrun_is_sticky_error);
    RUN(write_payload_length_mismatch_is_sticky_error);
    RUN(write_payload_without_command_is_sticky_error);

    printf("\n-- C. IOCTLs, EOI + status wire format --\n");
    RUN(get_eoi_clear_eoi_roundtrip);
    RUN(iec_poll_returns_line_mask_in_status_value);
    RUN(iec_wait_ready_on_match_busy_otherwise);
    RUN(iec_setrelease_updates_lines);
    RUN(pp_write_read_roundtrip);
    RUN(parburst_refused_honestly);
    RUN(status_buf_serialization_is_3_bytes_le);

    printf("\n-- D. GCR generator: determinism + defects --\n");
    RUN(gcr_encode_decode_roundtrip_all_byte_values);
    RUN(gcr_track_clean_is_deterministic);
    RUN(gcr_track_len_equals_zone_budget);
    RUN(gcr_zone_tables_match_hal_ssot);
    RUN(gcr_track_sync_count_is_2x_sectors);
    RUN(gcr_track_headers_parse_with_valid_checksums);
    RUN(gcr_track_data_checksums_all_valid_when_clean);
    RUN(gcr_track_refuses_out_of_bounds_track);
    RUN(weak_bits_change_data_but_keep_sync_structure);
    RUN(checksum_error_breaks_exactly_sector0_data);
    RUN(sync_long_creates_40_byte_run);
    RUN(sync_short_removes_one_detectable_sync);
    RUN(killer_track_has_zero_syncs_and_full_budget);
    RUN(density_mismatch_forces_zone3_layout_on_inner_track);
    RUN(half_track_is_deterministic_and_structurally_degraded);

    printf("\n-- E. End-to-end TALK/READ pipeline --\n");
    RUN(e2e_gcr_track_via_talk_read_pipeline);
    RUN(e2e_short_read_reports_partial_len_and_eoi);

    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
