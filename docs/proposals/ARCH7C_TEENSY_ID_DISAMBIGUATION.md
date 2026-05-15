# Proposal: ADF-Copy ↔ Applesauce disambiguation (audit ARCH-7 sub-finding C)

Status: **APPLIED (MF-213).** Tier 1 + `probe_teensy_serial()` +
`tests/test_teensy_probe.cpp` + the onConnect wiring landed.

Closes **task #121 / ARCH-7 sub-finding C** (`audit/ARCH-7_VID_PID.md`).
Sub-findings A (XUM1541 PID label, MF-190) and B (SCP header-vs-GUI,
MF-212) are done.

UPDATE after the `lsusb`/Device-Manager readout (§7 gap resolved):
ADF-Copy **and** Applesauce both ship the *stock* Teensy string
descriptors, so the Tier-1 string heuristic of §5 cannot distinguish
them — the implemented Tier 1 emits only the explicit-ambiguity hint
and the Tier-2 probe is mandatory, exactly the §7 "both stock" branch.
The §5 sketch's `manufacturer().contains("Evolution")` rule was
therefore NOT implemented — the real Applesauce does not report that
string. Tier-2 wiring landed in `onConnect()` (P1.23 is complete); it
warns on a probe/selection mismatch, never silently overrides.

---

## 1. The problem (firsthand-confirmed)

`adfcopy_provider_v2.{h,cpp}` and `applesauce_provider_v2.cpp` **both**
document their device's USB identity as `0x16C0:0x0483` — the *stock,
unmodified PJRC Teensy USB-Serial* identity. Both controllers are
Teensy-based and (apparently) ship the default serial descriptor. At the
USB VID/PID layer they are **genuinely indistinguishable** — this is a
hardware reality, not a typo, so it cannot be fixed by changing a
constant.

`0x16C0` is the shared Van-Ooijen/V-USB hobby VID; `0x0403:0x6001`
(KryoFlux's GUI hint) is the generic FTDI FT232 identity — the same
class of "not actually unique" problem.

## 2. Current state — verified

`hardwaretab.cpp::detectSerialPorts()` (the VID/PID → `controllerHint`
block, ~`:446-454`) has hints for Greaseweazle, SCP, KryoFlux and
XUM1541 — but **no `0x16C0:0x0483` entry at all**. So today an ADF-Copy
or an Applesauce shows up *un-identified* (falls through to
`port.description()` or the bare port name), not *mis-identified*.

That is the honest current state. The hazard the audit flags is
forward-looking: anyone adding a naive `0x16C0:0x0483 → "ADF-Copy"`
hint would be **wrong half the time**. This proposal is the correct
shape so that naive fix never gets written.

`git grep MF-146` is empty — the "MF-146 disambiguation" the original
audit brief assumed exists does not exist in the tree.

## 3. Why VID/PID alone cannot work — and what can

Three signals are available, in increasing cost and authority:

| Signal | Source | Cost | Authority |
|--------|--------|------|-----------|
| VID/PID | `QSerialPortInfo::vendorIdentifier()/productIdentifier()` | free | **none** — both devices are `0x16C0:0x0483` |
| String descriptors | `QSerialPortInfo::manufacturer()` / `description()` (iProduct) / `serialNumber()` | free (already read at enum time) | **heuristic** — only if the firmware authors set distinctive strings |
| Protocol probe | open the port, send each candidate's identify command | one port open + short I/O | **authoritative** — works even with identical USB descriptors |

`detectSerialPorts()` today reads only `description()`. `manufacturer()`
and `serialNumber()` are available on the same `QSerialPortInfo` object
at zero extra cost.

## 4. Proposed design — two tiers

### Tier 1 — string-descriptor heuristic (cheap, no port open)

At enumeration, for a `0x16C0:0x0483` port, inspect `manufacturer()` +
`description()`:

- **Applesauce** — `applesauce_provider_v2.cpp:229,867` already names the
  manufacturer string **"Evolution Interactive"** (John Googin's
  company, per `applesauce_provider_v2.h:59`). If `manufacturer()`
  contains "Evolution" or `description()` contains "Applesauce" →
  confident Applesauce hint.
- **ADF-Copy** — the actual iManufacturer/iProduct strings are
  **`needs-source`** (see §7). If they turn out distinctive, add the
  matching rule; if ADF-Copy ships the *stock* Teensy strings
  ("Teensyduino" / "USB Serial"), Tier 1 cannot identify it and Tier 2
  is required.

Tier 1 output is a **hint**, not a decision. If the strings are
ambiguous or stock, the hint is `"USB-CDC serial (0x16C0:0x0483 —
ADF-Copy or Applesauce, probe to confirm)"` — explicit ambiguity, never
a silent guess.

### Tier 2 — protocol probe (authoritative, opens the port)

The two devices have **distinct, non-overlapping identify handshakes** —
this is the key enabling fact:

| Device | Identify command | Expected response | Source |
|--------|------------------|-------------------|--------|
| **ADF-Copy** | `ADFC_CMD_PING` = byte `0x00` | a text version string terminated by `\n` | `adfcopy_provider_v2.h:128,262` |
| **Applesauce** | the ASCII text command `?vers` | a firmware version string | `applesauce_provider_v2.h:299` |

An ADF-Copy does not understand `?vers`; an Applesauce does not treat a
bare `0x00` as a version query. So a probe that tries **both** and sees
which one answers correctly is authoritative even when the USB
descriptors are byte-identical.

Probe algorithm (non-destructive — see §6):
1. Open the port (115200 8N1), short timeout.
2. Send `?vers`, read with a ~200 ms timeout. A plausible Applesauce
   version line → **Applesauce**, done.
3. Else send `0x00`, read a `\n`-terminated line with a short timeout. A
   plausible ADF-Copy version line → **ADF-Copy**, done.
4. Neither → **unidentified**; surface that honestly, do not pick one.
5. Close the port.

### Where each tier runs

- **Tier 1** can land **now** — it is a self-contained improvement to
  `detectSerialPorts()`'s hint logic, no provider routing needed. It
  only sets the `controllerHint` string the user sees in the port combo.
- **Tier 2** is best run **lazily, on Connect** (not on every 2-second
  port-refresh poll — opening every Teensy port repeatedly would be
  intrusive). It belongs in the per-provider connect path, which means
  it is **gated behind P1.23** (non-GW provider routing): the probe
  result decides *which V2 provider* to construct, and there is no
  non-GW routing yet. Until P1.23, Tier 2's probe logic can exist as a
  free function (`probe_teensy_serial(portName) -> {AdfCopy, Applesauce,
  Unknown}`) with its own unit test, ready to be wired when routing
  lands.

## 5. Sketch — Tier 1 in `detectSerialPorts()`

Not applied. Inserted into the VID/PID hint chain (`hardwaretab.cpp`
~`:446`):

```cpp
} else if (vid == 0x16C0 && pid == 0x0483) {
    /* ARCH-7-C: 0x16C0:0x0483 is the stock PJRC Teensy USB-Serial ID,
     * claimed by BOTH ADF-Copy and Applesauce — indistinguishable by
     * VID/PID. Tier-1 heuristic on the string descriptors; ambiguity
     * is surfaced explicitly, never guessed. Authoritative Tier-2
     * protocol probe runs on Connect (gated behind P1.23 routing). */
    const QString mfg  = port.manufacturer();
    const QString prod = port.description();
    if (mfg.contains("Evolution", Qt::CaseInsensitive) ||
        prod.contains("Applesauce", Qt::CaseInsensitive)) {
        controllerHint = "Applesauce";
    } else if (/* ADF-Copy distinctive strings — NEEDS lsusb -v */ false) {
        controllerHint = "ADF-Copy";
    } else {
        controllerHint = "USB-CDC (0x16C0:0x0483 — ADF-Copy or "
                         "Applesauce; probe on Connect)";
    }
}
```

## 6. Forensic constraints

- **The probe must be non-destructive.** `ADFC_CMD_PING` (0x00) and
  `?vers` are pure identify queries — no motor, no seek, no flux, no
  write. The probe must use *only* these. It must never send a command
  that moves the head or touches media.
- **No silent guess.** If neither tier identifies the device, the GUI
  shows "unidentified 0x16C0:0x0483 device" — it must not default to
  one provider. Picking wrong = sending ADF-Copy bytes to an Applesauce
  (or vice versa), which is exactly the kind of "stille Veränderung"
  the project forbids.
- **The probe respects port ownership.** It opens, probes, closes —
  before the real provider takes the port. It must not leave the port
  open or the device in a non-default state.

## 7. `needs-source` — what Axel must supply

Tier 1's ADF-Copy rule and the exact Applesauce strings cannot be
finalised from the repo. One readout per device:

```
Linux:   lsusb -v -d 16c0:0483 | grep -iE 'iManufacturer|iProduct|iSerial'
Windows: Device Manager → the device → Details → "Bus reported device
         description" / "Manufacturer"
```
Run it with an **ADF-Copy** attached, then with an **Applesauce**
attached. If the two produce distinct iManufacturer/iProduct strings,
Tier 1 alone suffices and Tier 2 becomes a rarely-needed fallback. If
both report the *stock* Teensy strings, Tier 2 is mandatory.

Until then: Tier 1 ships with only the Applesauce "Evolution
Interactive" rule (already in-repo) + the explicit-ambiguity fallback;
the ADF-Copy string rule is left as a documented TODO.

## 8. Scope / sequencing

| Part | Depends on | Can land |
|------|-----------|----------|
| Tier 1 (string-descriptor hint in `detectSerialPorts()`) | — | **now** (one `else if`, non-protected file) — but the ADF-Copy rule waits on §7 |
| `probe_teensy_serial()` free function + unit test | — | **now** (self-contained, testable with a mock serial) |
| Tier 2 wired into Connect → provider construction | **P1.23** (non-GW routing) | after P1.23 |

KryoFlux's generic-FTDI `0x0403:0x6001` is the same class of problem;
the same Tier-2 "probe to confirm" pattern applies (KryoFlux's DTC
subprocess can identify the device) but it is out of scope for ARCH-7-C
— noted for a follow-up.

## 9. Recommendation: **GO**

- The two-tier design is the only correct shape: VID/PID alone provably
  cannot work, string descriptors are a free heuristic, and the
  distinct identify handshakes make an authoritative probe possible.
- It is forensically safe: non-destructive probe, explicit ambiguity,
  no silent guess.
- It touches **no protected header** — Tier 1 is `hardwaretab.cpp`,
  the probe is a new free function. (Both are Qt code → applying needs
  a Qt build to verify; this proposal is the design, not the patch.)
- It is honest about the `needs-source` gap (§7) and does not fabricate
  the ADF-Copy strings.

Applying it = the Tier-1 hint + the `probe_teensy_serial()` function +
its test; Tier-2 wiring follows P1.23.
