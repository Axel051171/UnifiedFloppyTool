# Prompt — Task B Format-Plugin-Scaffold

Identischer Text an Claude Code UND hermes-agent.

## Voraussetzung

Repository-Checkout ist auf einem disposable Test-Branch
`test/hermes-eval-scaffold` der von `main` abgeleitet ist. Die Tools
haben Read+Write+Bash auf dem Repo. Sie sollen NEUE Dateien anlegen
und bestehende kleine Konfig-Dateien (`*.pro`, `tests/CMakeLists.txt`)
um Einträge ergänzen.

## Prompt (verbatim)

```
You are extending the UnifiedFloppyTool (UFT) codebase with a new
disk-image format plugin. The format you are adding is "TAP" — the
Commodore 64 cassette tape image format (.tap files). This is a NEW
plugin; do not assume any existing TAP code in the repo is
authoritative (treat it as if it does not exist).

A complete UFT format plugin touches SIX places:

1. `src/formats/tap/uft_tap_plugin.c` — the plugin implementation
   with `probe`, `open`, `read_track` functions
2. `include/uft/formats/uft_format_ids.h` — add `UFT_FMT_TAP = <next-free-id>`
3. `src/uft_format_registry.c` — register the plugin via
   `UFT_REGISTER_PLUGIN(...)`
4. `UnifiedFloppyTool.pro` — add the new .c file under SOURCES
5. `tests/test_tap_plugin.c` — minimal test that probes a fake TAP
   header and asserts the probe returns true
6. `tests/CMakeLists.txt` — register `test_tap_plugin` as a ctest case

The plugin's metadata MUST set:
  - `spec_status = UFT_SPEC_REVERSE_ENGINEERED`
  - `features` array with one entry: `{UFT_PLUGIN_FEATURE_READ,
     UFT_FEATURE_SUPPORTED}`. Write capability = UNSUPPORTED (omit
     the write feature entry).
  - `is_stub = false` (we are scaffolding a real read path, even if
     the read_track body is a small placeholder)

The TAP file format starts with the 12-byte ASCII signature
"C64-TAPE-RAW" followed by a single version byte. Your `probe`
function should return true iff the first 12 bytes match this
signature.

For `read_track`, return `UFT_ERR_NOT_IMPLEMENTED` with a doc-comment
saying "M3.x track-extraction pending — placeholder, NOT a silent
no-op." This is the honest-stub pattern (allowed for an unwired track
extraction; only the high-level operation is honest-stubbed, the
plugin itself is real).

Follow existing UFT conventions:
  - English code comments and docstrings
  - German log messages (`log_info("TAP plugin probe: ...")`)
  - Use `pathlib`-style path handling where applicable
  - No `shell=True` in any subprocess calls
  - All functions return `uft_error_t`

After writing all 6 files, run:
  cmake --build build -j
  ctest -R test_tap_plugin --output-on-failure

If the build succeeds AND the test passes, output a final line:
"OK — 6 touchpoints implemented, build green, test_tap_plugin PASS."

If the build fails, output a final line:
"FAIL — <which touchpoint blocked the build>" and stop.

Do not start the work until you have read at least one existing
UFT format plugin (e.g. `src/formats/scl/uft_scl_plugin.c`) to
understand the local conventions for the plugin struct layout.
```

## Was die Tools machen MÜSSEN

- Ein bestehendes Plugin als Vorlage lesen (z.B. SCL oder T64)
- 6 neue Datei-Touchpoints korrekt anlegen
- Build via cmake durchlaufen lassen
- Test via ctest durchlaufen lassen
- Final-Status-Zeile ausgeben

## Was die Tools NICHT machen sollen

- TAP-Format vollständig implementieren (nur Scaffold)
- Andere Format-Plugins anfassen
- Tests anderer Plugins anfassen

## Bewertung gegen Rubric

Siehe `proto/hermes-eval/results/RUBRIC.md` (Punkte 4-7, plus
Wall-Clock + Token-Cost).

## hermes-agent Invocation

```bash
cd <repo-root>
git checkout main
git checkout -b test/hermes-eval-scaffold
time hermes -z "$(extract_prompt_block proto/hermes-eval/prompts/task_b_format_plugin.md)" \
  --provider openrouter \
  --model anthropic/claude-sonnet-4.6 \
  --toolsets terminal \
  --max-turns 40 \
  > proto/hermes-eval/results/raw_logs/task_b_hermes.txt
```

(`--max-turns 40` weil das Scaffolding mehr Tool-Calls braucht als das
Audit aus Task A.)

## Claude-Code Invocation

Identisch zu Task A: separate Session, OHNE `uft-format-plugin` Skill
(sonst Skill-Bias). Beide Tools müssen mit denselben Basis-Werkzeugen
arbeiten.

## Build-Validierung als objektiver KO-Test

Anders als Task A (subjektive Bewertung) hat Task B einen objektiven
KO-Test eingebaut: der Build muss laufen + der Test muss grün sein.
Das macht Task B zum primären Differenzierer im PoC.

Failure-Klasse "FAIL — build error" zählt automatisch 0 Punkte in
der "Build-tauglich"-Rubrik-Kategorie.
