# cli/uft-decode — V415-PLAN EMUCI scaffold (MF-263)

**Status (2026-09-17, MF-1216): VERDRAHTET.** Ziel `uft-decode` in
`cli/uft-decode/CMakeLists.txt`, eingehängt über
`option(UFT_BUILD_CLI … ON)` in der Wurzel-`CMakeLists.txt`, mit einer
Abnahme, die eine echte Wandlung byteweise prüft
(`uft_decode_cli_wandelt_atr_nach_xfd`). Auf ausdrückliche
**Eigentümerentscheidung** („verdrahten"), nachdem `A-024`/`MF-1215`
diese Datei als einzigen Posten der Menge C mit Disposition `BLOCKED`
vorgelegt hatte — sie steht gegen die GUI-only-Regel in `CLAUDE.md`.

> **Zitiert stehen gelassen** (Hausregel „nicht entfernen, weiter
> erweitern"): *„**Status (2026-05-25):** Scaffold only. `main.c` exists
> and is build-ready, but not wired into qmake/CMake yet — see
> 'Integration checklist' below."*

**Was das Verdrahten gefunden hat, und es ist mehr als eine Bauzeile:**
das Binärprogramm **band und lief**, bevor eine Zeile geändert wurde —
467 Quellen, 0 Fehler, `--help` mit Ausgangskode 0 — und **konnte
nichts**. `uft_convert_file()` erkennt über die Format-Registry, und die
ist zur Laufzeit leer, bis jemand `uft_register_all_formats()` ruft; das
tat nur `src/main.cpp:43`. Gemessen an einer gültigen ATR aus dem Korpus:
`Could not detect source format`, `err=-25`, **Ausgangskode 5**. Seit
MF-1216 ruft `main()` die Registrierung, und derselbe Aufruf liefert
92 160 Byte, **byteidentisch** mit `tests/corpus_free/atrcopy_dos2sd.xfd`
(Erzeugnis fremder Hand, atrcopy).

**Zwei Angaben der Prüfliste unten sind gemessen falsch bzw. überholt:**

1. `target_link_libraries(uft-decode PRIVATE uft_core uft_crc)` **kann
   nicht linken** — `uft_convert_file` steht in
   `src/formats/uft_format_convert_dispatch.c`, und `src/formats/` ist
   keine Bibliothek. Die wirkliche Schließung sind **acht**
   Quellverzeichnisse (`formats core flux detect compat recovery analysis
   forensic`), **467** Übersetzungseinheiten und **`-lz`** (MFI-Spurdaten
   sind zlib-gepackt). Sie ist abgeleitet, nicht aufgezählt.
2. `if(UFT_BUILD_CLI)` **default OFF** wurde **nicht** übernommen. Ein
   Ziel, das niemand baut, ist genau der Zustand, aus dem dieses Programm
   kommt; OFF hätte ihn mit einer CMake-Zeile dekoriert. Vorgabe ist
   **ON**.

**Was weiterhin offen ist:** `.github/workflows/emulator.yml` ist immer
noch ein Gerüst (Zeile 4: `Status: SCAFFOLD ONLY`) — Schritt 3 der
Prüfliste bleibt also unerledigt, und der eigentliche Verbraucher fehlt.
Das ändert die Entscheidung nicht, nur ihre Begründung: verdrahtet wurde
auf Anweisung, nicht weil der Verbraucher fertig ist.

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
