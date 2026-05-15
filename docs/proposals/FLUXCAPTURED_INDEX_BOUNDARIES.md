# Proposal: per-revolution index boundaries in `FluxCaptured`

Status: DESIGN PROPOSAL — not applied. `include/uft/hal/outcomes.h` is a
protected P0 foundation header; this document is for human review and
go-ahead before any edit.

Blocks: REFACTOR_TASKS.md **P1.20** (migrate `FluxCaptureJob` off the
direct `uft_gw_*` C-API onto the V2 outcome surface).

Related: **P1.22** (remove `raw_handle()` escape hatch) — cannot land
while P1.20 still needs `uft_gw_flux_data_t::index_times[]`.

---

## 1. The structural gap

`FluxCaptured` (`outcomes.h:179`) carries a flat `transitions_ns` vector
that spans *all* revolutions, plus `int revolutions` — a count only.
There is no way to recover where revolution N ends and N+1 begins.

`FluxCaptureJob` (`src/fluxcapturejob.cpp:140-216`) is the production
consumer that needs those boundaries: it writes a multi-revolution SCP
file by calling `scp_writer_add_track(...)` **once per revolution**,
slicing the sample stream on per-revolution index boundaries. In the V1
path it gets those from `uft_gw_flux_data_t::index_times[]` — an array
of `index_count` cumulative-tick timestamps, one per index pulse.

Migrating the job onto V2 `FluxCaptured` as-is would either:

- collapse the capture to a single revolution → wrong SCP output →
  **Rule F-3 violation** ("Kein Bit verloren / keine stille
  Veränderung"); or
- keep `raw_handle()` alive forever → blocks P1.22.

So `FluxCaptured` must gain a way to express index boundaries.

### Doc-vs-code gap to fix as part of this

`greaseweazle_provider_v2.h:24-26` claims Rule F-3 preserves "the
per-revolution index boundaries", but `do_read_raw_flux`
(`greaseweazle_provider_v2.cpp:246-264`) only copies `flux->samples[]`
and **discards `flux->index_times[]` entirely**. The header comment at
`:186-191` likewise claims the index timestamps are "converted and
appended after the sample data" — they are not; nothing touches them.
This proposal closes that gap: the field gives the index data a real,
typed home, and both comments must be corrected to describe it
accurately (no more "appended after the sample data" fiction).

---

## 2. Proposed `outcomes.h` change

### Option analysis

| Option | Forensic faithfulness (F-3) | `scp_writer_add_track` ergonomics | Disruption |
|---|---|---|---|
| **A. `std::vector<std::size_t> revolution_starts`** — indices into `transitions_ns` | Lossy: a transition stream cannot represent an index pulse landing *between* two transitions; the index time gets quantised to the nearest transition boundary. Silent rounding = F-3 risk. | Good — directly gives `[start,end)` slice ranges. But you still need per-rev *duration* in ns, which a start-index does not carry. | Additive. |
| **B. `std::vector<std::uint32_t> index_times_ns`** — cumulative-ns timestamp of each index pulse, same units as the C-API's `index_times[]` (just ticks→ns) | **Lossless.** Stores exactly what the hardware reported, in the canonical HAL unit (ns). No quantisation to transition boundaries. Mirrors `uft_gw_flux_data_t::index_times` 1:1. | Consumer walks `transitions_ns` accumulating ns until it reaches each `index_times_ns[r]` — same loop the V1 job already runs on ticks. Per-rev duration = `index_times_ns[r] - index_times_ns[r-1]`, directly. | Additive. |
| **C. nested `std::vector<std::vector<std::uint32_t>> revolutions_ns`** — pre-sliced per-rev streams | Lossless for the slices, but **changes the shape of the type**: `transitions_ns` becomes redundant or contradictory. Every existing provider and the 3 audit-flagged byte-container providers (kryoflux/fluxengine — P1.24) would have to be rewritten. `int revolutions` becomes redundant too. | Best for the consumer. | **Breaking** — touches every `FluxCaptured` site and contradicts the existing flat-vector contract that 6 providers already implement. |

### Pick: Option B — `std::vector<std::uint32_t> index_times_ns`

Rationale:

1. **F-3 faithfulness.** It stores the index pulse positions exactly as
   the hardware/firmware reported them, converted only by the same
   ticks→ns scale already applied to `transitions_ns`. No quantisation
   to transition boundaries (Option A's flaw), no reshaping that forces
   providers to fabricate structure (Option C's flaw).
2. **Honest absence.** A provider that has no index information leaves
   the vector **empty**. Empty is unambiguous: "this provider did not
   observe index pulses" — it is *not* "one revolution". A consumer
   seeing an empty vector must not invent a single-revolution boundary;
   it must fall back explicitly and visibly (see §4). This is the
   "must not silently fabricate boundaries it doesn't have" constraint
   met directly by the type.
3. **Consumer ergonomics.** `index_times_ns[r] - index_times_ns[r-1]`
   is the per-revolution duration `scp_writer_add_track` needs, with no
   extra field. Slicing `transitions_ns` is the identical accumulate
   loop the V1 job already runs — just on ns instead of ticks.
4. **Minimal disruption.** Purely additive. `transitions_ns`,
   `revolutions`, `sample_ns`, `quality` keep their exact current
   meaning. All 6 existing provider construction sites keep compiling
   (they just don't set the new field → empty → honest).
5. **Matches the C-API.** `uft_gw_flux_data_t::index_times` is already
   "index pulse positions (ticks from start)". Option B is that array
   with one unit conversion. The mapping is mechanical and auditable.

### Exact diff (NOT applied — for human to apply to the protected header)

```diff
@@ outcomes.h  (struct FluxCaptured, ~line 179)
 struct FluxCaptured {
     CHS position;
     /** Raw transition intervals in nanoseconds. */
     std::vector<std::uint32_t> transitions_ns;
     int revolutions;
     /** Sample resolution in ns (e.g. 25 for SCP, 41.6 for KryoFlux). */
     double sample_ns;
     QualityFlag quality = QualityFlag::None;
+    /**
+     * Index-pulse positions, as cumulative nanoseconds from the start
+     * of the capture — one entry per index pulse the hardware observed.
+     * Same time-base as the running sum of `transitions_ns`.
+     *
+     * This is forensic data (rule F-3): it is the *measured* boundary
+     * between physical revolutions, not a derived count. A consumer
+     * slices `transitions_ns` into per-revolution streams by walking
+     * the cumulative interval sum up to each `index_times_ns[r]`; the
+     * duration of revolution r is `index_times_ns[r] - index_times_ns[r-1]`
+     * (with an implied 0 before the first entry).
+     *
+     * EMPTY means the provider observed no index pulses (e.g. a
+     * subprocess backend that returns an undelimited byte container).
+     * Empty is NOT "one revolution" — a consumer must not fabricate a
+     * boundary it was not given. `revolutions` still carries the
+     * provider's count; treat (revolutions >= 1, index_times_ns empty)
+     * as "N revolutions, boundaries unknown" and handle it explicitly.
+     *
+     * INVARIANT when non-empty: strictly increasing; `.size()` should
+     * equal `revolutions` (one index pulse per revolution captured).
+     */
+    std::vector<std::uint32_t> index_times_ns;
 };
```

Note on default-construction: aggregate `FluxCaptured{}` (used by
`test_hal_foundation.cpp:124,146` and `test_hal_foundation.cpp:186`)
keeps compiling — the new member is a `std::vector` and default-inits
to empty. Adding a member after `quality` (which has a default member
initialiser) is legal aggregate-wise because the new member also needs
no explicit initialiser at the call sites; all current aggregate
initialisations of `FluxCaptured` are either `{}` or use designated /
field-by-field assignment, not positional brace lists past `quality`.
(Confirmed against every construction site — see §6.)

---

## 3. `GreaseweazleProviderV2::do_read_raw_flux` population sketch

Insert after the existing `transitions_ns` fill loop
(`greaseweazle_provider_v2.cpp:264`), before `uft_gw_flux_free(flux)`:

```cpp
/* Rule F-3: preserve the per-revolution index boundaries the firmware
 * reported. uft_gw_flux_data_t::index_times[] holds index_count
 * cumulative-tick timestamps; convert each to ns with the SAME scale
 * already applied to the samples. Do NOT synthesise entries — if the
 * firmware reported none, leave the vector empty. */
if (flux->index_times && flux->index_count > 0) {
    captured.index_times_ns.reserve(flux->index_count);
    for (uint8_t r = 0; r < flux->index_count; ++r) {
        captured.index_times_ns.push_back(
            uft_gw_ticks_to_ns(flux->index_times[r], sample_freq));
    }
}
```

Then the header comments at `greaseweazle_provider_v2.h:24-26` and
`greaseweazle_provider_v2.cpp:186-191` are corrected to: "the index
timestamps are converted ticks→ns and stored in
`FluxCaptured::index_times_ns`" — deleting the false "appended after the
sample data" claim.

`captured.revolutions` already derives from `flux->index_count` when
available (`:250-252`); after this change `revolutions ==
index_times_ns.size()` holds on the Greaseweazle happy path, satisfying
the invariant in the doc comment.

---

## 4. `FluxCaptureJob` consumption sketch

Replace the V1 `index_times[]` walk (`fluxcapturejob.cpp:161-208`).
After the job is migrated to call the V2 surface
(`read_raw_flux(ReadFluxParams)` returning a `FluxOutcome`), it visits
the outcome and, for `FluxCaptured`:

```cpp
[&](const uft::hal::FluxCaptured& fc) {
    if (fc.index_times_ns.empty()) {
        // Honest fallback: provider gave no boundaries. Do NOT invent a
        // single-revolution split. Either (a) emit the whole stream as
        // one SCP track entry and log that per-rev structure was
        // unavailable, or (b) treat as an error for this job, since a
        // multi-revolution SCP capture without boundaries is degraded.
        // FluxCaptureJob's contract is multi-rev SCP, so log + skip the
        // track (same "honest path" as the seek-failure branch at
        // fluxcapturejob.cpp:128-135) rather than write a wrong file.
        ++errors;
        qWarning() << "cyl" << cyl << "side" << side
                   << "- no index boundaries in FluxCaptured; track "
                      "omitted (no fabricated single-rev split)";
        return;
    }

    // index_times_ns[r] is cumulative ns from capture start.
    std::size_t sample_start = 0;
    std::uint64_t cumulative_ns = 0;
    std::uint32_t prev_index_ns = 0;
    int rev_idx_in_writer = 0;
    const std::size_t n = fc.transitions_ns.size();

    for (std::size_t r = 0;
         r < fc.index_times_ns.size()
         && rev_idx_in_writer < m_revolutions;
         ++r) {
        const std::uint32_t target_ns = fc.index_times_ns[r];

        std::size_t sample_end = sample_start;
        while (sample_end < n && cumulative_ns < target_ns) {
            cumulative_ns += fc.transitions_ns[sample_end];
            ++sample_end;
        }

        if (sample_end > sample_start) {
            const std::uint32_t duration_ns = target_ns - prev_index_ns;
            int rc2 = scp_writer_add_track(
                writer, cyl, side,
                fc.transitions_ns.data() + sample_start,
                sample_end - sample_start,
                duration_ns,
                rev_idx_in_writer);
            if (rc2 != 0) {
                ++errors;
                qWarning() << "scp_writer_add_track rc=" << rc2;
            }
            ++rev_idx_in_writer;
        }

        prev_index_ns = target_ns;
        sample_start  = sample_end;
    }
}
```

This is structurally the **same loop** as V1 (`:178-208`), with two
mechanical changes: (1) it walks ns instead of ticks — no `ns_per_tick`
multiply needed because `transitions_ns` and `index_times_ns` are
already in ns; (2) the pre-conversion `all_ns` buffer
(`fluxcapturejob.cpp:170-173`) disappears entirely — `fc.transitions_ns`
*is* the ns buffer. The other `FluxOutcome` alternatives
(`FluxMarginal`, `FluxUnreadable`, `HardwareDisconnected`,
`ProviderError`, `CapabilityRequiresPolicy`) get visit arms that
`++errors` + `qWarning` and skip the track, matching the existing
honest-path behaviour.

---

## 5. Cross-provider check

Every flux-reading V2 provider's `do_read_raw_flux` was read. Whether
each can populate `index_times_ns`:

| Provider | Source / file | Has index info? | What it puts in `index_times_ns` |
|---|---|---|---|
| **Greaseweazle** | `greaseweazle_provider_v2.cpp:196-278` | **Yes** — `uft_gw_flux_data_t::index_times[]` / `index_count` from firmware | Converted ticks→ns, one entry per index pulse. See §3. |
| **SuperCard Pro** | `scp_provider_v2.cpp:~125-216` | **Not yet.** `uft_scp_direct_read_flux` returns a flat `flux_buf` + `captured_count` only; no index array in the M3.1 C-API surface. The SCP device *does* deliver index data on the wire, but the current C-API does not expose it. | **Empty.** Honest: M3.1 USB layer is not wired (`:128-131` returns NOT_IMPLEMENTED today anyway). When the C-API later exposes SCP index timing, populate it then — and add a P1.24-style task. Do not synthesise. |
| **KryoFlux** | `kryoflux_provider_v2.cpp:~210-348` | **Encoded but not decoded here.** The KryoFlux stream *contains* index opcodes, but this provider re-interprets the raw byte container as LE `uint32_t` words (`:316-345`) without decoding — that is exactly the ARCH-2 issue REFACTOR_TASKS.md **P1.24** flags. It has no decoded index times to offer. | **Empty.** This is the honest state until P1.24 lands a real KryoFlux stream decoder; that decoder would also extract index positions and could fill `index_times_ns`. Note in P1.24's scope. |
| **FluxEngine** | `fluxengine_provider_v2.cpp:~440-506` | **Encoded but not decoded here.** Same ARCH-2 pattern as KryoFlux: `bytes_to_words` on an undecoded `.flux` container (`:495`). No decoded index info. | **Empty.** Same as KryoFlux — P1.24 territory. |
| **Applesauce** | `applesauce_provider_v2.cpp:~389-477` | **No** in the current runner contract. `bytes_to_transitions_ns(result.flux_bytes)` produces a flat transition stream; `result.revolutions` is a count only — there is no per-rev boundary array in `ApplesauceRunResult`. | **Empty.** If the Applesauce text protocol later surfaces index timing, the runner struct would gain a field and this provider could fill it. Until then: empty, honest. |
| **ADFCopy** | `adfcopy_provider_v2.cpp:~290-364` | **No.** Same shape as Applesauce: `bytes_to_transitions_ns` flat stream + `result.revolutions` count. No boundary array. | **Empty.** Honest. |

**Conclusion:** the field makes sense for all providers. Greaseweazle
populates it today; the other five leave it **empty**, which the type
contract defines as "boundaries unknown" — never "one revolution".
This is why Option B (empty-as-honest-absence) is correct and Option A
(an index vector that would *tempt* a provider to push a single `{size}`
sentinel) is not. No provider is forced to fabricate. The
empty-vs-populated split also cleanly tracks the P1.24 work boundary:
the two ARCH-2 byte-container providers (KryoFlux, FluxEngine) will be
able to fill the field *as a side effect* of getting a real decoder.

---

## 6. `FluxMarginal` recommendation

`FluxMarginal` (`outcomes.h:189`) also carries `transitions_ns` but has
**no `revolutions` count and no `sample_ns`** — it is deliberately a
thinner, "something is wrong with the timing" outcome, not a
structured multi-revolution capture.

**Recommendation: do NOT add `index_times_ns` to `FluxMarginal` in this
change.**

- No current consumer slices a `FluxMarginal` per revolution.
  `FluxCaptureJob` would treat `FluxMarginal` as an error/skip (§4),
  not write per-rev SCP tracks from it.
- `FluxMarginal` already lacks `revolutions`/`sample_ns`; adding only
  `index_times_ns` would be an inconsistent partial structure.
- Adding it "for consistency" with no call-site violates the project's
  own anti-goal ("Do NOT add a concept/field just in case").
- If a future need arises (e.g. a recovery tool wants per-rev structure
  out of a marginal capture), that is its own proposal with its own
  call-site justification, and at that point `FluxMarginal` should
  probably gain `revolutions` + `sample_ns` too, as a coherent set.

So: scope this change to `FluxCaptured` only.

---

## 7. Additive-vs-breaking analysis

**This is an ADDITIVE change.** A new `std::vector` member that
default-constructs to empty. Every existing site keeps compiling:

- Aggregate `FluxCaptured{}` → new member empty. OK.
- Field-by-field assignment (`captured.position = ...; captured.x = ...`)
  → new member simply not assigned → empty. OK.
- No site uses a positional brace-init list that runs *past* `quality`,
  so appending a member after `quality` does not shift any positional
  initialiser. (Verified below.)

### Full list of `FluxCaptured` construction sites

| Site | Style | Affected? |
|---|---|---|
| `greaseweazle_provider_v2.cpp:248` | `FluxCaptured captured;` + field assignment | No — will *gain* the §3 populate block |
| `scp_provider_v2.cpp:202` | `FluxCaptured captured;` + field assignment | No — stays empty (honest) |
| `kryoflux_provider_v2.cpp:339` | `FluxCaptured captured;` + field assignment | No — stays empty (honest) |
| `fluxengine_provider_v2.cpp:497` | `FluxCaptured captured;` + field assignment | No — stays empty (honest) |
| `applesauce_provider_v2.cpp:470` | `FluxCaptured fc;` + field assignment | No — stays empty (honest) |
| `adfcopy_provider_v2.cpp:357` | `FluxCaptured fc;` + field assignment | No — stays empty (honest) |
| `tests/mock_provider_v2.cpp:143` | `FluxCaptured f;` + field assignment | No |
| `tests/test_hal_foundation.cpp:124` | `return FluxCaptured{};` | No |
| `tests/test_hal_foundation.cpp:146` | `return FluxCaptured{};` | No |
| `tests/test_hal_foundation.cpp:186` | `FluxCaptured fc;` | No |

### Full list of `FluxCaptured` read sites

| Site | What it reads | Affected? |
|---|---|---|
| `src/hardwaretab.cpp:850` (`onFluxOutcome`) | currently reads scalar fields only | No — could optionally show rev count from `index_times_ns.size()` later |
| `src/fluxcapturejob.cpp` (post-P1.20) | **will read `index_times_ns`** — the whole point | This is the new consumer (§4) |
| `tests/test_mock_provider_v2.cpp:151-152` | `std::holds_alternative` + scalar fields | No |
| `tests/test_*_provider_v2.cpp` (adfcopy/applesauce/fluxengine/kryoflux/scp/greaseweazle) | visit arms check `transitions_ns` non-empty, `revolutions > 0`, `sample_ns > 0` | No — existing asserts still hold; `index_times_ns` is additionally available |
| `tests/test_hal_conformance.cpp:591` | visit arm, scalar fields | No |
| `tests/test_hal_foundation.cpp:214` | visit arm | No — but **should gain** a new `static_assert`/check that `index_times_ns` exists and is a `std::vector<std::uint32_t>`, per the foundation-contract workflow |

### Required follow-up if approved

Per the Type System Architect workflow, applying this needs:

1. The `outcomes.h` diff in §2 (human applies — protected header).
2. A new check in `tests/test_hal_foundation.cpp` asserting the field's
   presence and type, so the foundation contract reflects the change.
3. Build + run `g++ -std=c++20 -I include tests/test_hal_foundation.cpp`.
4. A row in `docs/REFACTOR_BRIEF.md` recording the `FluxCaptured`
   extension.
5. The Greaseweazle provider populate block (§3) + header-comment
   correction — that is **P1.20 / provider-migrator work**, not the
   architect's, but it is the immediate unblock.

---

## 8. Recommendation: **GO**

- The change is the minimal type extension that unblocks P1.20 and, in
  turn, P1.22.
- It is strictly additive — zero existing consumers break, all 10
  construction sites and all read sites keep compiling.
- Option B is the only one of the three that is both lossless (F-3) and
  cannot tempt a provider into fabricating boundaries — empty is the
  honest, type-enforced "I don't know" state, and 5 of 6 providers will
  legitimately be empty until later tasks (P1.24) give them real index
  decoding.
- It simultaneously fixes the existing doc-vs-code lie in
  `greaseweazle_provider_v2.{h,cpp}` (claims index boundaries are
  preserved; code discards them).
- Scope is contained to one struct; `FluxMarginal` is explicitly left
  alone (no call-site → no change).

No STOP condition is triggered: only 1 variant changes, no migrated
provider is invalidated (additive), `tests/golden/` is untouched.
