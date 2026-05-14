#!/usr/bin/env python3
"""
mock_adfcopy.py — a behavioural mock of the ADF-Copy binary serial protocol.

ADF-Copy is NOT a CLI-wrapper backend in the strict sense (it has no
subprocess CLI like KryoFlux's DTC or FluxEngine). It is a Qt-side
runner-injection backend: ADFCopyProviderV2 delegates every operation to
an injected `std::function` runner that, in production, wraps an
IADFCopyTransport over QSerialPort.

Because there is NO C-HAL header to byte-assert against (the V2 provider
has no compile-time protocol constants beyond RESP_*/clock/geometry —
see test_<backend>_vectors rationale in audit/README.md), this file
takes the mock_*.py form: it models the protocol framing the production
runner MUST implement, so the audit has an executable description of the
wire contract the runner is responsible for.

This mock is the reference implementation of the runner's protocol half:
  - command framing: 1 command byte + variable binary payload
  - response framing: 1 status byte + optional binary payload
  - PING (0x00): response is a text version string terminated by '\\n'
  - READ_FLUX (0x06): 3-byte header status+len16(BE) + binary flux payload

Run standalone: python mock_adfcopy.py   → self-test, prints OK / non-zero on fail.
"""

import struct
import sys

# ── Protocol constants — mirror adfcopy_provider_v2.cpp:74-83 ──────────────
RESP_OK     = ord('O')   # 0x4F  ADFC_RSP_OK
RESP_ERROR  = ord('E')   # 0x45  ADFC_RSP_ERROR
RESP_NODISK = ord('D')   # 0x44  ADFC_RSP_NODISK

# Opcodes — needs-source (see extract_ref.py): values from UFT comments only.
CMD_PING       = 0x00
CMD_INIT       = 0x01
CMD_SEEK       = 0x02
CMD_READ_FLUX  = 0x06
CMD_GET_STATUS = 0x0B

SAMPLE_CLOCK_HZ = 40_000_000
SAMPLE_NS       = 1e9 / SAMPLE_CLOCK_HZ   # 25.0

STD_CYLINDERS = 80
HEADS         = 2
MAX_CYLINDERS = 83   # ADFC_MAX_CYLINDERS — over-scan margin

# Status bitmask bits (ADFC_STATUS_*)
ST_DISK_PRESENT = 0x01
ST_WRITE_PROT   = 0x02
ST_MOTOR_ON     = 0x04
ST_FLUX_CAPABLE = 0x08


class MockADFCopy:
    """A scriptable ADF-Copy device. Models the binary serial protocol."""

    def __init__(self, firmware="1.111", disk_present=True,
                 write_protected=False, flux_capable=False):
        self.firmware = firmware
        self.disk_present = disk_present
        self.write_protected = write_protected
        self.flux_capable = flux_capable
        self.motor_on = False
        self.current_track = 0

    # ── command handlers ──────────────────────────────────────────────────
    def ping(self):
        """CMD_PING → text version string + '\\n'."""
        return (self.firmware + "\n").encode("ascii")

    def init(self):
        """CMD_INIT → spins motor + homes head, replies status byte."""
        self.motor_on = True
        self.current_track = 0
        return bytes([RESP_OK])

    def seek(self, track):
        """CMD_SEEK + track(1) → status byte."""
        if not (0 <= track <= MAX_CYLINDERS * 2 + 1):
            return bytes([RESP_ERROR])
        self.current_track = track
        return bytes([RESP_OK])

    def read_flux(self, track, revolutions):
        """CMD_READ_FLUX + track + revs → status(1) + len16(BE) + payload."""
        if not self.disk_present:
            return bytes([RESP_NODISK, 0x00, 0x00])
        # Synthesize `revolutions` worth of 16-bit BE 25ns-tick transitions.
        ticks = [250, 400, 250, 500] * revolutions
        payload = b"".join(struct.pack(">H", t) for t in ticks)
        length = len(payload)
        return bytes([RESP_OK, (length >> 8) & 0xFF, length & 0xFF]) + payload

    def get_status(self):
        """CMD_GET_STATUS → 1-byte bitmask."""
        mask = 0
        if self.disk_present:    mask |= ST_DISK_PRESENT
        if self.write_protected: mask |= ST_WRITE_PROT
        if self.motor_on:        mask |= ST_MOTOR_ON
        if self.flux_capable:    mask |= ST_FLUX_CAPABLE
        return bytes([mask])


# ── self-test ──────────────────────────────────────────────────────────────
def _selftest():
    errs = 0

    def check(cond, msg):
        nonlocal errs
        if not cond:
            print(f"  FAIL: {msg}")
            errs += 1

    dev = MockADFCopy(firmware="1.111", disk_present=True, flux_capable=True)

    # PING
    check(dev.ping() == b"1.111\n", "PING returns version string + newline")

    # INIT
    check(dev.init() == bytes([RESP_OK]), "INIT returns 'O'")
    check(dev.motor_on is True, "INIT sets motor_on")
    check(dev.current_track == 0, "INIT homes head to track 0")

    # SEEK
    check(dev.seek(20) == bytes([RESP_OK]), "SEEK valid track returns 'O'")
    check(dev.current_track == 20, "SEEK updates current_track")
    check(dev.seek(9999) == bytes([RESP_ERROR]), "SEEK invalid track returns 'E'")

    # READ_FLUX — header + payload framing
    resp = dev.read_flux(track=40, revolutions=2)
    check(resp[0] == RESP_OK, "READ_FLUX status byte == 'O'")
    plen = (resp[1] << 8) | resp[2]
    check(plen == len(resp) - 3, "READ_FLUX 16-bit BE length matches payload")
    # 8 transitions/rev * 2 revs = 16 transitions * 2 bytes = 32 bytes
    check(plen == 16, "READ_FLUX 2 revs => 8 transitions => 16 bytes")
    # first transition: 250 ticks => 250 * 25 ns = 6250 ns
    first = struct.unpack(">H", resp[3:5])[0]
    check(first == 250, "READ_FLUX first BE16 transition == 250 ticks")
    check(abs(first * SAMPLE_NS - 6250.0) < 1e-6, "250 ticks * 25 ns == 6250 ns")

    # READ_FLUX no disk
    nd = MockADFCopy(disk_present=False)
    check(nd.read_flux(0, 1)[0] == RESP_NODISK, "READ_FLUX no disk returns 'D'")

    # GET_STATUS bitmask
    st = dev.get_status()[0]
    check(st & ST_DISK_PRESENT, "GET_STATUS disk-present bit set")
    check(st & ST_MOTOR_ON, "GET_STATUS motor-on bit set (after INIT)")
    check(st & ST_FLUX_CAPABLE, "GET_STATUS flux-capable bit set")

    # Constants sanity
    check(SAMPLE_NS == 25.0, "SAMPLE_NS derived as 25.0 from 40 MHz")
    check(STD_CYLINDERS == 80 and HEADS == 2, "Amiga DD geometry 80x2")

    if errs:
        print(f"mock_adfcopy: {errs} error(s)")
        return 1
    print("mock_adfcopy: OK — protocol framing model self-test passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(_selftest())
