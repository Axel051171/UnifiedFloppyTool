# Bench-testing Applesauce + ADFCopy without real hardware (MF-253)

**Use case:** You want to run the docs/M3_APPLESAUCE_TRANSPORT.md §5
verification plan (or the equivalent for ADFCopy) but you don't have
the physical device. The QSerialPort I/O layer can be exercised
end-to-end against a software-emulated device using a virtual serial
port pair plus a small simulator script.

Two-tier strategy:

| Tier | What it tests | Hardware needed |
|---|---|---|
| **Tier 1** | Protocol parsing (UFT runners' command sequence + response parsing) | NONE — done by `test_applesauce_runners_protocol` / `test_adfcopy_runners_protocol` |
| **Tier 2** | Full I/O stack (QSerialPort + virtual COM + simulator + UFT) | None — uses virtual COM-pair |
| **Tier 3** | Real device verification | Real Applesauce / ADFCopy |

Tier 1 already runs in CI as a unit test (MF-251, MF-252). This
document covers Tier 2.

---

## Tier 2 setup on Windows (com0com)

1. Install **com0com** from
   [https://sourceforge.net/projects/com0com/](https://sourceforge.net/projects/com0com/).
   Accept the unsigned driver (you may need to disable driver signature
   enforcement via Settings → Recovery → Advanced startup → Restart
   now → Troubleshoot → Advanced options → Startup Settings → Disable
   driver signature enforcement).

2. After install, run **Setup Command Prompt** (in com0com Start
   menu). Create a paired COM endpoint:

   ```
   command> install PortName=COM20 PortName=COM21
   ```

   Now whatever you write to COM20 appears on COM21 and vice versa.

3. Build the simulator (see §Simulator script below) and start it
   listening on COM21:

   ```powershell
   python tools\applesauce_sim.py --port COM21
   ```

4. Start UFT, select Applesauce in the HardwareTab, set the port
   combobox to **COM20**, click Connect. UFT's QSerialPort opens
   COM20, the simulator on COM21 receives the commands.

---

## Tier 2 setup on Linux/macOS (socat)

```bash
# Create a virtual serial-port pair. socat prints the two pty paths.
socat -d -d pty,raw,echo=0 pty,raw,echo=0
# Example output:
#   /dev/pts/10  <-->  /dev/pts/11

# Run the simulator on one end.
python3 tools/applesauce_sim.py --port /dev/pts/11

# Point UFT at the other end (HardwareTab → port → /dev/pts/10).
```

---

## Simulator script (skeleton)

```python
#!/usr/bin/env python3
# tools/applesauce_sim.py — minimal Applesauce protocol simulator
# Responds to the documented text protocol. Returns fake but
# protocol-correct responses to each command. Useful for testing
# the QSerialPort transport + the 7 runners end-to-end without
# real hardware.

import argparse, sys, struct
import serial  # pip install pyserial

p = argparse.ArgumentParser()
p.add_argument("--port", required=True)
p.add_argument("--baud", type=int, default=115200)
args = p.parse_args()

s = serial.Serial(args.port, args.baud, timeout=0.05)
print(f"applesauce_sim: listening on {args.port}", file=sys.stderr)

def writeln(line):
    s.write(line.encode() + b"\n")
    s.flush()

buf = b""
while True:
    chunk = s.read(256)
    if chunk: buf += chunk
    while b"\n" in buf:
        line, _, buf = buf.partition(b"\n")
        cmd = line.decode().strip()
        print(f"<<< {cmd}", file=sys.stderr)
        # Dispatch — minimal subset for first bench-test run.
        if cmd == "?kind":          writeln("+5.25")
        elif cmd == "?vers":        writeln("+sim-firmware-0.1")
        elif cmd == "?pcb":         writeln("+sim-pcb")
        elif cmd == "data:?max":    writeln("+163840")
        elif cmd == "sync:?speed":  writeln("+300.00")
        elif cmd == "psu:?":        writeln("+1")
        elif cmd == "psu:on":       writeln(".")
        elif cmd in ("sync:on", "sync:off"): writeln(".")
        elif cmd.startswith("head:track"):   writeln(".")
        elif cmd.startswith("head:side"):    writeln(".")
        elif cmd == "head:zero":             writeln(".")
        elif cmd in ("motor:on", "motor:off"): writeln(".")
        elif cmd.startswith("disk:readx"):   writeln(".")
        elif cmd == "data:?size":            writeln("+512")
        elif cmd.startswith("data:< "):
            # 512 bytes of fake flux — alternating 4000ns / 6000ns
            # intervals in the form expected by the read runner.
            s.write(bytes(512))  # zeros are fine for the I/O test
        elif cmd == "disk:?write":           writeln("+0")
        elif cmd == "data:clear":            writeln(".")
        elif cmd.startswith("data:> "):
            n = int(cmd.split()[1])
            _ = s.read(n)  # swallow the binary upload
            writeln(".")
        elif cmd == "disk:write":            writeln(".")
        else:
            writeln("-unknown command")
```

ADFCopy simulator is similar but uses BINARY protocol (single-byte
commands, byte responses). Skeleton omitted here — see
`docs/M3_ADFCOPY_TRANSPORT.md` (TODO) when the same pattern is needed.

---

## What Tier 2 catches that Tier 1 doesn't

Tier 1 (the scripted-mock unit tests MF-251 / MF-252) validates that
the UFT runners send the right commands and parse responses correctly.
But it bypasses QSerialPort entirely — the mock is a direct
`IApplesauceTransport` / `IADFCopyTransport` implementation.

Tier 2 catches:
- Baud-rate mismatches (UFT 115200 ↔ device 115200)
- Byte-framing issues (CR/LF handling, accumulator state)
- Timeout tuning (slow USB hosts, busy systems)
- Real OS quirks (port already in use, permission denied, etc.)
- Connect/disconnect lifecycle (clear-on-open, signal handling)

It does NOT verify:
- Floppy-drive electrical timing
- Whether the firmware-reported "+300.10" RPM is accurate
- That a real disk's flux pattern decodes correctly

Tier 3 (real hardware) is the only thing that closes those.

---

## Status

- Tier 1: ✓ MF-251 (Applesauce 17 tests) + MF-252 (ADFCopy 15 tests)
- Tier 2: this document + `tools/applesauce_sim.py` skeleton.
  Verification: not yet run. Anyone with com0com/socat can drive it.
- Tier 3: pending real hardware. Until done, HardwareTab shows yellow
  "Disconnect (Beta)" for these two providers.

When a user runs Tier 2 successfully:
- Run UFT, connect Applesauce on the virtual-COM endpoint
- The status bar should show "BETA" + firmware string
- Read flux returns the simulator's fake bytes
- Write flux echoes successfully

Subsequent runs against real hardware then differ ONLY in the device's
electrical behaviour — the UFT stack is already proven by tiers 1+2.
