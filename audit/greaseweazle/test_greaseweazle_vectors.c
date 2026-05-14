/*
 * test_greaseweazle_vectors.c — D1 wire-protocol vector check (native backend).
 *
 * Compile-time assertion that UFT's Greaseweazle opcode / ACK / USB-ID
 * constants equal the official keirf/greaseweazle protocol values
 * (reference provenance: "recalled", see audit/greaseweazle/extract_ref.py).
 *
 * This is the C twin of diff.py: diff.py checks at audit time by parsing
 * the header; this file makes the same contract a BUILD GATE — if a
 * future edit changes an opcode, the build breaks here, not silently on
 * the wire.
 *
 * Reproduce:
 *   gcc -std=c11 -I../../include -fsyntax-only test_greaseweazle_vectors.c
 *
 * No runtime, no main() body work — every check is static_assert.
 */
#include "uft/hal/uft_greaseweazle_full.h"

/* ── USB identity ──────────────────────────────────────────────────── */
_Static_assert(UFT_GW_USB_VID == 0x1209,
    "GW USB VID must be 0x1209 (pid.codes allocation)");
_Static_assert(UFT_GW_USB_PID == 0x4D69,
    "GW USB PID must be 0x4D69");
_Static_assert(UFT_GW_USB_PID_F7 == UFT_GW_USB_PID,
    "GW F7 enumerates with the same VID/PID as the base device");

/* ── Command opcodes (firmware inc/cmd.h class Cmd) ────────────────── */
_Static_assert(UFT_GW_CMD_GET_INFO        == 0x00, "Cmd.GetInfo");
_Static_assert(UFT_GW_CMD_UPDATE          == 0x01, "Cmd.Update");
_Static_assert(UFT_GW_CMD_SEEK            == 0x02, "Cmd.Seek");
_Static_assert(UFT_GW_CMD_HEAD            == 0x03, "Cmd.Head");
_Static_assert(UFT_GW_CMD_SET_PARAMS      == 0x04, "Cmd.SetParams");
_Static_assert(UFT_GW_CMD_GET_PARAMS      == 0x05, "Cmd.GetParams");
_Static_assert(UFT_GW_CMD_MOTOR           == 0x06, "Cmd.Motor");
_Static_assert(UFT_GW_CMD_READ_FLUX       == 0x07, "Cmd.ReadFlux");
_Static_assert(UFT_GW_CMD_WRITE_FLUX      == 0x08, "Cmd.WriteFlux");
_Static_assert(UFT_GW_CMD_GET_FLUX_STATUS == 0x09, "Cmd.GetFluxStatus");
_Static_assert(UFT_GW_CMD_GET_INDEX_TIMES == 0x0A, "Cmd.GetIndexTimes");
_Static_assert(UFT_GW_CMD_SWITCH_FW_MODE  == 0x0B, "Cmd.SwitchFwMode");
_Static_assert(UFT_GW_CMD_SELECT          == 0x0C, "Cmd.Select");
_Static_assert(UFT_GW_CMD_DESELECT        == 0x0D, "Cmd.Deselect");
_Static_assert(UFT_GW_CMD_SET_BUS_TYPE    == 0x0E, "Cmd.SetBusType");
_Static_assert(UFT_GW_CMD_SET_PIN         == 0x0F, "Cmd.SetPin");
_Static_assert(UFT_GW_CMD_RESET           == 0x10, "Cmd.Reset");
_Static_assert(UFT_GW_CMD_ERASE_FLUX      == 0x11, "Cmd.EraseFlux");
_Static_assert(UFT_GW_CMD_SOURCE_BYTES    == 0x12, "Cmd.SourceBytes");
_Static_assert(UFT_GW_CMD_SINK_BYTES      == 0x13, "Cmd.SinkBytes");
_Static_assert(UFT_GW_CMD_GET_PIN         == 0x14, "Cmd.GetPin");
_Static_assert(UFT_GW_CMD_TEST_MODE       == 0x15, "Cmd.TestMode");
_Static_assert(UFT_GW_CMD_NO_CLICK_STEP   == 0x16, "Cmd.NoClickStep");

/*
 * NOTE — UFT_GW_CMD_READ_MEM (0x20), WRITE_MEM (0x21), GET_INFO_EXT (0x22)
 * are present in the UFT header but are NOT in the recalled official Cmd
 * enum. They are intentionally NOT asserted here: see REPORT.md D1
 * finding GW-D1-1 (UNVERIFIED — needs a vendored cmd.h to confirm whether
 * these are a protocol extension or a UFT-side invention).
 */

/* ── ACK codes (firmware inc/cmd.h class Ack) ──────────────────────── */
_Static_assert(UFT_GW_ACK_OK             == 0x00, "Ack.Okay");
_Static_assert(UFT_GW_ACK_BAD_COMMAND    == 0x01, "Ack.BadCommand");
_Static_assert(UFT_GW_ACK_NO_INDEX       == 0x02, "Ack.NoIndex");
_Static_assert(UFT_GW_ACK_NO_TRK0        == 0x03, "Ack.NoTrk0");
_Static_assert(UFT_GW_ACK_FLUX_OVERFLOW  == 0x04, "Ack.FluxOverflow");
_Static_assert(UFT_GW_ACK_FLUX_UNDERFLOW == 0x05, "Ack.FluxUnderflow");
_Static_assert(UFT_GW_ACK_WRPROT         == 0x06, "Ack.Wrprot");
_Static_assert(UFT_GW_ACK_NO_UNIT        == 0x07, "Ack.NoUnit");
_Static_assert(UFT_GW_ACK_NO_BUS         == 0x08, "Ack.NoBus");
_Static_assert(UFT_GW_ACK_BAD_UNIT       == 0x09, "Ack.BadUnit");
_Static_assert(UFT_GW_ACK_BAD_PIN        == 0x0A, "Ack.BadPin");
_Static_assert(UFT_GW_ACK_BAD_CYLINDER   == 0x0B, "Ack.BadCylinder");
_Static_assert(UFT_GW_ACK_OUT_OF_SRAM    == 0x0C, "Ack.OutOfSRAM");
_Static_assert(UFT_GW_ACK_OUT_OF_FLASH   == 0x0D, "Ack.OutOfFlash");

/* Translation unit needs one external symbol to be non-empty under -c. */
int uft_audit_gw_vectors_ok(void) { return 0; }
