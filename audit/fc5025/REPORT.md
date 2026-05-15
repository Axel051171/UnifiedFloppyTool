# FC5025

**Verdict:** PARTIAL
**LOC:** 765 (`fc5025_provider_v2.cpp` 374 + `.h` 391) | **Integration:** injected-runner (dual-path documented: libusb USB CBW/CSW + `fcimage` CLI — neither has a production construction site in the tree)

FC5025 is a sector-only, read-only USB controller for 5.25" and 8"
drives. The V2 provider is a mixin composition of `ReadsSectors` +
`DetectsDrive`. It is an **honest scaffold**: every `do_*` path returns a
typed `ProviderError` / `SectorUnreadable` when its injected runner is
null — it never fakes a successful read.

Audited files:
- `src/hardware_providers/fc5025_provider_v2.{h,cpp}` — V2 provider
- `include/uft/hal/uft_fc5025.h` — C HAL header (`UFT_SKELETON_PLANNED`, 20/20 functions unimplemented; **no `uft_fc5025.c` exists**)
- `src/hardwaretab.cpp` — GUI routing (not wired for FC5025)
- `forms/tab_hardware.actions.yaml` — codegen wiring config

---

## D1 — Wire-Protocol / CLI contract

Constants diff: `python diff.py` -> `evidence.json`. **5 PASS / 0 FAIL / 15 MISSING.** The diff covers only the USB-identity + format-enum layer; the CBW-opcode / CLI-argv protocol layer is not in the diff because it does not exist in the provider.

| Aspect | UFT value | Reference value | Status |
|--------|-----------|-----------------|--------|
| USB VID | `0x16C0` (`uft_fc5025.h:43`) | `0x16C0` (V-USB/objdev shared VID) | PASS |
| USB PID | `0x06D6` (`uft_fc5025.h:44`) | `0x06D6` (FC5025 allocation) | PASS |
| `disk_format` ctor default | `0x02` (`fc5025_provider_v2.h:125`) | `2` = ordinal of `UFT_FC_FMT_APPLE_DOS33` | PASS (internal consistency) |
| `UFT_FC_FMT_AUTO` ordinal | `0` (`uft_fc5025.h:48`) | `0` (zero-init default) | PASS |
| `UFT_FC_FMT_APPLE_DOS33` ordinal | `2` (`uft_fc5025.h:51`) | `2` (provider default points here) | PASS |
| 15 other `uft_fc_format_t` members | present (`uft_fc5025.h:49-64`) | not pinned by reference | MISSING (expected) |
| CBW command opcodes | absent — no opcode table in the provider | DSD "FC5025 Command Set Specification v1309" | UNVERIFIED -> FC-D1-1 |
| `fcimage` CLI argv grammar (`-f -t -T -s`) | absent — argv built inside an un-constructed injected runner; only an example in `fc5025_provider_v2.h:225-234` | `fcimage(1)` man page | UNVERIFIED -> FC-D1-2 |

**Reference provenance: `mixed`** — USB VID/PID is `recalled` (well-known from the open-source `fc5025.h` in the Device Side Data Linux source release; not vendored). CBW opcodes + `fcimage` argv grammar are `needs-source`: not vendored, AND the UFT V2 provider contains no CBW opcode table and no CLI-argv table to diff against — the dialogue is entirely inside an injected `Fc5025Runner` / `Fc5025DetectRunner` lambda. `git grep` finds no production construction site for that runner anywhere in `src/` (only `tests/test_fc5025_provider_v2.cpp`). Format enum is `uft-internal` — `uft_fc_format_t` is a UFT host-side abstraction, not an FC5025 wire value.

**Findings:**
- **FC-D1-1** (high): UFT has no FC5025 CBW opcode table anywhere. The provider header *describes* "CMD_READ_FLEXIBLE CBW on bulk EP 0x01" (`fc5025_provider_v2.cpp:16-18`) but no code implements it — a comment is not an implementation (Hard rule #4).
- **FC-D1-2** (high): the `fcimage` CLI argv grammar exists only as a doc-comment example (`fc5025_provider_v2.h:225-234`). No production runner builds that argv. `mock_fc5025.py` is the audit's stand-in CLI, explicitly not a conformance reference.
- **FC-D1-3** (medium): CLAUDE.md states FC5025 is "read via fcimage CLI"; `uft_fc5025.h` is actually a USB CBW protocol header. The provider documents both paths; which one production would use cannot be determined because neither is constructed.

**Status: UNVERIFIED** — the USB-identity sub-layer is PASS (`recalled`-grade), but the protocol/CLI layer that D1 is actually about does not exist in the provider and has no vendored reference. Not FAIL (nothing provably wrong), not PASS (nothing provably right).

---

## D2 — Datapath

**Trace (read path):**
`FC5025ProviderV2::do_read_sector(ReadSectorParams)` (`fc5025_provider_v2.cpp:245`)
-> null-runner guard -> `null_runner_error("sector read")` if `!m_read_runner` (`:247-249`)
-> geometry validation `cylinder in [0, m_max_cylinders]`, `head in [0,1]` (`:253-258`)
-> build `Fc5025ReadRequest{cylinder, head, retries, disk_format}` (`:261-265`)
-> `m_read_runner(req)` — the injected lambda (USB CBW *or* `fcimage` subprocess) — returns `Fc5025RunResult` (`:267`)
-> branch: `exit_code != 0 || no_disk || not_ready` -> `translate_run_failure()` (`:269-270`); `sector_bytes.empty()` -> `SectorUnreadable` (`:273-287`); else -> `translate_run_success()` (`:289`).

**Conversions named:**
- `Fc5025RunResult::sector_bytes` -> `SectorRead::data` via `.assign(begin,end)` (`:214`) — verbatim byte copy, no transform.
- `crc_error_count > 0` -> `SectorMarginal` with `divergent_reads` = `[partial_bytes, empty_sentinel]` (`:184-208`) — Rule F-3 path.
- `no_disk` / `not_ready` flags -> `SectorUnreadable::physical_reason` text (`:129-149`).

**Sink — where does `SectorRead` go?** **Nowhere in production.** The V2 provider compiles into `UnifiedFloppyTool.pro` (`:369`) but `HardwareTab` does not route to it (D3) and no production code constructs an `FC5025ProviderV2` with a real runner — only `tests/test_fc5025_provider_v2.cpp`. The datapath is correct in isolation (faithful, lossless translation) but has no consumer.

**Finding FC-D2-1** (medium): the F-3 divergent-reads mechanism is structurally satisfied but semantically thin — the second entry is always an empty-vector sentinel (`m.divergent_reads.emplace_back()`, `:199`), not a second real read. `size() >= 2` is met by padding, not by preserving two genuinely divergent captures. Honest (comment `:195-198`), but a voting consumer gets one real buffer + one empty.

**Status: PARTIAL** — `do_read_sector` translation is correct and lossless; but the path has no production consumer and the F-3 entry is a padding sentinel.

---

## D3 — GUI-Integration (V2-adapted)

**Routing: FAIL.** `HardwareTab::currentProviderV2()` (`hardwaretab.cpp:249`) returns `::uft::hal::GreaseweazleProviderV2*` — a Greaseweazle-only type. There is no `FC5025ProviderV2` member on `HardwareTab`. Selecting "FC5025 (5.25\" read-only)" in the controller combo (`hardwaretab.cpp:291`) and pressing Connect reaches the P1.18 stub: `onConnect()` shows `QMessageBox::information(... "Controller routing pending")` + `updateStatus("FC5025 routing pending (P1.18)")` (`hardwaretab.cpp:624-642`).

**Capability gating: N/A.** `forms/tab_hardware.actions.yaml` has `provider_source: self->currentProviderV2()` and `extra_includes:` lists only `greaseweazle_provider_v2.h`. Every `wire_action<cap::X>` is bound to `GreaseweazleProviderV2`, never `FC5025ProviderV2`.

| Button | Capability tag | `FC5025ProviderV2` satisfies? | Wired |
|--------|----------------|-------------------------------|-------|
| (any sector-read button) | `ReadsSectors` | yes (`static_assert :339`) | no — not routed |
| (any detect button) | `DetectsDrive` | yes (`static_assert :342`) | no — not routed |
| `btnReadTest` (flux) | `ReadsRawFlux` | no — negative `static_assert :347` | correctly absent |

**Status: FAIL** — FC5025 is one of the 8 unrouted providers. The capability declarations are sound; the integration is absent.

---

## D4 — OS-Detection

The FC5025 enumerates as a USB device (VID `0x16C0` / PID `0x06D6`). There is no FC5025-specific OS enumeration code in the tree:

| OS | Mechanism | Evidence | Status |
|----|-----------|----------|--------|
| **Windows** | none — `uft_fc_detect(int*)` is unimplemented (`UFT_SKELETON_PLANNED`, no `uft_fc5025.c`). The V2 detect path delegates to an injected `Fc5025DetectRunner` with no production construction. `detectSerialPorts()` enumerates serial ports only — the FC5025 is a USB bulk device, not CDC serial, so it does not appear there. | no enumeration code reachable | UNVERIFIED |
| **Linux** | same — `uft_fc_detect` unimplemented; a udev rule for `0x16C0:0x06D6` is mentioned only in a `do_detect_drive` error string (`fc5025_provider_v2.cpp:334`), not in any rules file in the repo. | no enumeration code reachable | UNVERIFIED |
| **macOS** | same. | no enumeration code reachable | UNVERIFIED |

**Cross-check:** the provider cites `VID:PID 0x16C0:0x06D6` in `DriveAbsent::scanned_for` (`fc5025_provider_v2.cpp:340-342`) and the detect-failure `fix` string (`:331-334`); those literals match `uft_fc5025.h:43-44` and the recalled reference — the identity UFT would scan for is correct, but no code performs the scan.

**Finding FC-D4-1** (high): FC5025 device detection is entirely unimplemented. On every OS, an FC5025 plugged in will not be found by UFT.

**Status: UNVERIFIED** — no detection code to grade. Needed: implement `uft_fc_detect()` (libusb `libusb_get_device_list` + VID/PID filter) or a production `Fc5025DetectRunner`.

---

## D5 — Integritaets-Befunde

| ID | Issue | Severity | Code citation |
|----|-------|----------|---------------|
| FC-D5-1 | **Honest scaffold — no fake success.** Null runner -> `null_runner_error()` returns a 3-part `ProviderError` (`fc5025_provider_v2.cpp:93-107`, `247-249`, `317-319`). Never returns a `SectorRead` it did not receive. This is why the verdict is PARTIAL not FAIL. | positive | `:93-107, 247, 317` |
| FC-D5-2 | F-3 divergent-reads second entry is an empty-vector padding sentinel, not a real second read (`:199`). Structurally satisfies `size() >= 2` but a voting consumer gets one real buffer + one empty. Honestly commented (`:195-198`). | medium | `:184-208` |
| FC-D5-3 | ✅ **LANDED (MF-186)** — `do_detect_drive` no longer hardcodes 40/2/300 geometry. `Fc5025DetectResult` carries no geometry fields, so detect now reports `tracks=0`/`heads=0`/`rpm_nominal=0.0` = "not auto-detected" (the same sentinel `GreaseweazleProviderV2` uses); the user-selected `disk_format` determines geometry downstream. | medium | `:356-368` |
| FC-D5-4 | `do_detect_drive` firmware fallback string "FC5025 (firmware version unknown — CLI mode or not queried)" (`:367`) goes into `DriveDetected::firmware`. Clearly labelled as unknown (not a fake version) so honest, but a report consumer displaying `firmware` verbatim shows prose where a version is expected. | low | `:364-368` |
| FC-D5-5 | Header doc-comments describe protocol behaviour ("sends CMD_READ_FLEXIBLE CBW on bulk EP 0x01") that no code implements (`fc5025_provider_v2.cpp:16-18`, `.h:62-70`). Per Hard rule #4 a comment is not proof. | low (documentation) | `.h:59-70`, `.cpp:14-25` |

**Positives (forensic-integrity holds):**
- No swallowed errors — every `Fc5025RunResult` failure flag is checked and mapped to a typed outcome (`:269-289`).
- Rule F-4: `null_runner_error`, `range_error`, `translate_run_failure` all build non-empty what/why/fix; the `ProviderError` ctor throws on empty strings.
- Rule F-3 (verbatim copy): `translate_run_success` copies all `sector_bytes` with no averaging/pruning (`:214`).
- Null-runner construction is honest — never a fake success (FC-D5-1).
- No stub facade returning `UFT_OK`/`SectorRead` from nothing.

**Status: PASS with findings** — no critical (P0) integrity violation. FC-D5-3 (hardcoded geometry on detect) is ✅ fixed (MF-186); the remaining findings are low-severity.

---

## Fixes (prioritised)

**P0:** none.

**P1:**
- **FC-D3** — route `FC5025ProviderV2` through `HardwareTab` (member + `currentProviderV2()` variant accessor + add the type to `forms/tab_hardware.actions.yaml`). Until then no GUI path reaches `do_read_sector` / `do_detect_drive`.
- **FC-D1-1 / FC-D4-1** — implement a production `Fc5025Runner` / `Fc5025DetectRunner` (libusb CBW path or `fcimage` subprocess). Single biggest gap; until one exists D1's protocol layer is unauditable and D4 has no detection code.

**P2:**
- ~~**FC-D5-3** — `do_detect_drive` must report runner-provided geometry or "unknown", never a hardcoded 40/2/300.~~ ✅ landed (MF-186) — reports `0` = "not auto-detected".
- **FC-D1-1 / FC-D1-2** — vendor the DSD "Command Set Specification v1309" and the `fcimage(1)` man page; add a CBW opcode table + a CLI-argv table so D1 becomes a real diff.

**P3:**
- **FC-D5-2** — make the F-3 second `divergent_reads` entry meaningful, or document that FC5025 cannot produce true divergent reads.
- **FC-D5-5** — annotate the protocol doc-comments as "DESCRIBES INTENDED RUNNER — not implemented in this file".

---

## Reproduce

```bash
cd audit/fc5025
python extract_uft.py        # UFT-side constants (JSON)
python extract_ref.py        # official reference + provenance (JSON)
python diff.py               # writes evidence.json, prints verdict, exit!=0 on FAIL
python mock_fc5025.py -f apple33 -t 0 -T 0 | wc -c   # CLI-wrapper mock smoke (4096)
python mock_fc5025.py -f apple33 -t 99 -T 99 ; echo $?  # out-of-range -> exit 3
```

Source reads behind this report:
```bash
git grep -nE "UFT_FC5025_|uft_fc_format_t" include/uft/hal/uft_fc5025.h
sed -n '245,290p' src/hardware_providers/fc5025_provider_v2.cpp   # do_read_sector
sed -n '315,371p' src/hardware_providers/fc5025_provider_v2.cpp   # do_detect_drive
sed -n '249p;291p;624,645p' src/hardwaretab.cpp                   # routing (absent)
git grep -nE "FC5025ProviderV2" src/                              # construction sites (none)
```

## Not reviewed
- The real `fcimage` CLI argv grammar — `mock_fc5025.py` is a stand-in, not a conformance reference (FC-D1-2, needs-source).
- The FC5025 CBW opcode table — does not exist in the tree (FC-D1-1).
- Runtime behaviour against a real FC5025 — that is HIL (`tests/hil/`, P3.4).
- `tests/test_fc5025_provider_v2.cpp` internals — reviewed only enough to confirm it is the sole construction site of the provider.
