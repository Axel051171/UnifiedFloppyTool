# v4.1.5 Release Checklist — V415-PLAN TAG.v415

**Status (2026-05-26):** WAITING ON PREREQUISITES — see "Gates" below.
**Goal-Doc:** `C:\Users\Axel\Downloads\V415_DEMO_READY_GOAL.md` (Phase B starts 2026-05-29).

## Gates (all must be ✓ before tagging v4.1.5)

| Gate | Owner | How to verify | Current |
|---|---|---|---|
| P2.4 (v4.1.4 final tag) | Axel + script | `git tag --list v4.1.4` returns one line | ⬜ blocked by RC1-Window 2026-05-29 |
| M3.1 SCP-Direct HW-Bench | Axel | `audit/scp/REPORT.md D2: PASS` (real-hw verified) | ⬜ needs SCP hardware bench session |
| HIL.GW 10/10 PASS | Axel | `releases/v4.1.5/hil_report.md` shows 10/10 | ⬜ needs Greaseweazle bench |
| LOSS.preflight Phase 1 | code | `uft_convert_file()` calls `uft_preflight_check()` | ✅ MF-263 |
| LOSS.preflight Phase 2 | code | sidecar `.loss.json` emitted by chokepoint | ✅ MF-268 (category-level; per-track v4.1.6) |
| ARCH7.C.wire | code | `probe_teensy_serial()` exists + GUI invoke | ✅ MF-213 + MF-263 |
| PLUGIN.spec_status 100% | audit | `audit_plugin_compliance.py` shows 84/84 | ✅ MF-262 |
| PLUGIN.features 100% | audit | `audit_plugin_compliance.py` shows 84/84 | ✅ MF-263 |
| BUILD.rebaseline | code | `verify_build_sources.py` 216/0 baseline | ✅ MF-272 (refresh 2026-05-26) |
| CI 5/5 green | gh | `gh run list -b <branch>` | ✅ live on 8ea2fa0 (Phase A bundle) |
| `check_consistency.py` 0/0/0/0 | code | local + CI | ✅ |
| Demo-Ready docs bundle | code | `docs/SHOWCASE.md`, `docs/CAPABILITIES.md`, `docs/demo/QUICK_DEMO.md` exist | ✅ Phase A (MF-273) + C.1 pull-forward (MF-274) |

**Currently 10/12 ✓**. Remaining 2 (P2.4, HIL.GW + M3.1-HW which counts as one bench-session) are
hardware/calendar-blocked. Code-side is fully ready.

## Tag-day workflow (when all gates ✓)

1. **Pre-tag verification** (10 min):
   ```bash
   bash scripts/release/p2_4_squash_to_main.sh   # only after RC1-Window closes
   python scripts/check_consistency.py
   python scripts/verify_build_sources.py
   python scripts/audit_plugin_compliance.py
   python tests/hil/run_hil.py --out releases/v4.1.5/hil_report.md
   ```
   All must return 0 / OK / 10/10.

2. **VERSION bump + CHANGELOG** (15 min):
   ```bash
   echo "4.1.5" > VERSION.txt
   # Edit CHANGELOG.md: add v4.1.5 section listing
   #   - MF-260 (UFT-004 + UFT-005 + UFT-T04 reductions)
   #   - MF-261 (SCP.D1.verify against samdisk)
   #   - MF-262 (spec_status 100% + BUILD.rebaseline)
   #   - MF-263 (PLUGIN.features 100% + LOSS.preflight Phase 1 + ARCH7.C + EMUCI scaffold)
   #   - Real-hw verification of M3.1 SCP-Direct (when present)
   # Edit RELEASE_NOTES.md: same content, user-facing language
   ```

3. **Tag** (1 min):
   ```bash
   git add VERSION.txt CHANGELOG.md RELEASE_NOTES.md
   git commit -m "release: v4.1.5 (MF-264)"
   git tag -a v4.1.5 -m "v4.1.5 — see CHANGELOG.md"
   git push origin main v4.1.5
   ```

4. **Artefact publish** (~10 min CI):
   - `release.yml` triggers on tag-push, builds `.deb`, `.dmg`, `.tar.gz` (Linux), `.tar.gz` (Windows MinGW)
   - SHA256SUMS auto-generated
   - GitHub Release auto-created with all artefacts

5. **Post-tag verification** (5 min):
   - Download Linux `.deb`, `dpkg -c` shows expected files
   - Download Windows `.tar.gz`, extract, run `UnifiedFloppyTool.exe`
   - Update KNOWN_ISSUES.md §M.3: mark V415-PLAN as fully closed
   - Re-pin docs/MASTER_PLAN.md current milestone

## Rollback (if a gate fails post-tag)

```bash
git tag -d v4.1.5                # local
git push origin --delete v4.1.5  # remote
# Fix the issue, re-tag from a new commit. Never reuse v4.1.5 with
# changed content (semver hygiene + signed-artefact-mismatch risk).
```

## Deferred to v4.1.6

These were explicitly out-of-scope per V415-PLAN §3:

- M3.2 XUM1541 libusb wiring (2 weeks, opencbm dep)
- M3.3 Applesauce serial-protocol full handshake
- UFI Windows + macOS backends
- ARCH-9 XUM1541 macOS .dylib loader
- 5-and-3 Apple GCR (DOS 3.2 13-sector)
- FM-decoder completion
- Per-converter LOSS.preflight Phase 2 sidecar emit
- SCOPE.switch_decision execution (option A/B/C per user choice)
