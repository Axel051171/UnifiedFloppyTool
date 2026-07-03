# FluxEngine Emulator — Divergences from Real Hardware

Forensic-honesty ledger. `SPEC_STATUS: CommunityConsensus` — FluxEngine
is David Given's open-source tool (github.com/davidgiven/fluxengine); the
CLI is documented by its README and source, not a vendor SDK. FluxEngine
has no C HAL in UFT: the C++ provider
(`src/hardware_providers/fluxengine_provider_v2.cpp`) runs the
`fluxengine` command-line tool via an injectable runner. On read it asks
fluxengine to write an SCP file that UFT decodes with its own vetted SCP
parser (`src/flux/uft_scp_parser.c`). This emulator models the fluxengine
CLI + device pair.

Severity: **HIGH** could mask a bug / break on real hardware · **MEDIUM**
plausible but unverified · **LOW** cosmetic / tolerance.

| ID | Sev | Divergence |
|----|-----|------------|
| **FE-1** | HIGH | fluxengine exit codes (`FE_EXIT_*`: NO_DEVICE=1, NO_DISK=2, BAD_ARGS=3, WRITE_DENY=4, IO=5) are MODELLED. Real fluxengine exit conventions are not formally documented; the provider parses stdout/stderr text for some conditions and treats a non-zero exit as failure. The mapping here is internally consistent and drives the test, but the real code→condition table must be confirmed at the FluxEngine bench. |
| **FE-2** | HIGH | The provider asks fluxengine to write an **SCP** file and decodes it with UFT's SCP parser. This emulator emits a minimal but valid SCP container that the production parser accepts — which is the point (defects map to real parser errors) — but the exact SCP dialect a real `fluxengine read ... -o out.scp` writes (footer presence, resolution, multi-rev layout) is not byte-captured. If real fluxengine writes an SCP variant our parser mis-handles, this emulator shares the parser's assumptions and would not catch it. |
| **FE-3** | HIGH | The `fluxengine` CLI argv the provider builds (`read -c ibm -s drive:0 --tracks=cNhM --drive.revolutions=R -o out.scp`) is taken from the provider source comments (FE-F2), which themselves note the flag semantics drifted between fluxengine versions (older `read ibm -s drive:0 -c N -h N --revs=N`). The emulator does not re-validate the argv string; it models the *device response* to a read/write/rpm intent. A wrong flag in the provider would not be caught here — it would fail at the real CLI. |
| **FE-4** | MEDIUM | RPM stdout: the three shapes emitted (`"300.0 rpm"`, `"RPM: 300.0"`, `"rotational speed: 300 rpm"`) come from the provider's own regex comment `(\d+\.?\d*)\s*rpm` (icase). These are plausible fluxengine outputs but only two or three were observed; a real fluxengine build may print a different phrasing the regex still matches (or does not). |
| **FE-5** | MEDIUM | Cell timing is a deterministic 4/6/8 µs MFM-DD mix summed to a 200 ms (300 RPM) revolution, serialized as 16-bit SCP samples at 25 ns resolution. Real disks have continuous jitter and platform-specific cell families (GCR/FM/HD); only DD-MFM is modelled. |
| **FE-6** | MEDIUM | The retry model treats every transient failure as recoverable within N retries. fluxengine's real retry/error-classification (hard vs soft) is internal and not reproduced; hard faults (no device/disk) fail immediately (correct), soft IO is retried. |
| **FE-7** | LOW | Motor/seek/recalibrate are not modelled at all — the V1 provider's setMotor/seekCylinder/recalibrate are silent stubs (fluxengine handles motion implicitly per read/write), so there is no primitive to emulate. This matches the provider's honest omission of those mixins. |
| **FE-8** | LOW | Write is always refused (`FE_EXIT_WRITE_DENY`). fluxengine *can* write with `fluxengine write`; the emulator deliberately never exercises it (forensic read-only stance, matching the other controllers). The provider does implement do_write_raw_flux, so this is an emulator policy choice, not a claim about the tool. |

## What is NOT divergent (bench-relevant confidence)

- Synthetic reads decode cleanly through the PRODUCTION SCP parser
  `uft_scp_read_track_memory()` — a live conformance test of the reader
  on the FluxEngine path, not a self-consistent mock (group E/F).
- Container defect classes map to the EXACT SCP parser error codes:
  ERR_SIGNATURE (bad SCP or TRK magic), ERR_TRACK (zero offset),
  ERR_READ (truncated) — verified in group F.
- The `fluxengine rpm` stdout shapes all satisfy the provider's
  documented regex; the value round-trips.
- Medium-safety: the generator refuses out-of-spec tracks/revolutions
  and keeps intervals within [1 µs, 200 µs]; `count_unsafe` is 0 on
  clean and weak-bit streams.

## Bench-verification gate (M3 FluxEngine)

Before the FluxEngine path is called "production", a bench session must
resolve FE-1 (real exit-code/stdout vocabulary), FE-2 (SCP byte-capture
vs parser assumptions), and FE-3 (the exact CLI flag dialect of the
installed fluxengine version). Until then: SIMULATED
(FIRMWARE-REALISTIC), Tier-3 PASS is bench-only.
