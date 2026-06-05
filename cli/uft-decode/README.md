# cli/uft-decode — V415-PLAN EMUCI scaffold (MF-263)

**Status (2026-05-25):** Scaffold only. `main.c` exists and is
build-ready, but not wired into qmake/CMake yet — see "Integration
checklist" below.

## Purpose

Headless, Qt-free decode/convert binary so the Emulator-CI workflow
(`.github/workflows/emulator.yml`) can do:

```bash
uft-decode --in capture.scp --out disk.d64 --format D64
```

…inside a containerized job (no Qt6 GUI stack pulled in). The current
UFT app is Qt-only and unsuitable for headless CI rounds.

## Exit codes

| Code | Meaning |
|---:|---|
| 0 | Success — file produced |
| 1 | Invalid argv |
| 2 | Source open / read error |
| 3 | Format unknown or no conversion path |
| 4 | Preflight ABORT (LOSSY without `--accept-data-loss`) |
| 5 | Converter internal failure |

## Integration checklist (for v4.1.6)

The binary is deliberately not yet wired into the build because
emulator.yml is itself still a scaffold — adding the binary to qmake/
CMake before its consumer exists would violate Master-Plan Regel 2.

When emulator.yml is ready to consume real `uft-decode` output:

1. **CMake** — new `cli/uft-decode/CMakeLists.txt`:
   ```cmake
   add_executable(uft-decode main.c)
   target_link_libraries(uft-decode PRIVATE uft_core uft_crc)
   target_include_directories(uft-decode PRIVATE
       ${CMAKE_SOURCE_DIR}/include)
   ```
   Add `add_subdirectory(cli/uft-decode)` to root `CMakeLists.txt` —
   ideally under a `if(UFT_BUILD_CLI)` option (default OFF) so the
   primary UFT app build is unaffected.

2. **qmake** — add a new `cli/uft-decode/uft-decode.pro` console-
   subproject. Since the main `UnifiedFloppyTool.pro` is a single-
   binary app, this becomes its own `.pro` invoked separately by CI
   (the emulator.yml job already runs in a Linux container — qmake
   isn't strictly required there, CMake suffices).

3. **emulator.yml** — replace the placeholder VICE round-trip with:
   ```yaml
   - run: ./build/cli/uft-decode --in tests/golden/d64/test1.scp \
                                  --out /tmp/test1.d64 --format D64
   - run: cd /tmp && c1541 -attach test1.d64 -dir | diff - expected.dir
   ```

4. **Integration test** — `tests/test_uft_decode_cli.py`:
   - subprocess.run([uft-decode, --in, fixture, --out, tmp, --format, D64])
   - assert exit code, output exists, no stderr beyond expected warnings.

## Why deferred

EMUCI.real (V415-PLAN §EMUCI.real) is estimated 2-3 weeks of work:
the CLI binary is the smaller half, the larger half is making the
Emulator-CI container ecosystem (VICE + c1541 + WinUAE + Docker
manifest) actually exercise round-trips. This scaffold establishes
the contract; the container work happens when LOSS.preflight Phase 2
(per-converter sidecar emit) is also ready, so the round-trip can
check loss-report consistency too.
