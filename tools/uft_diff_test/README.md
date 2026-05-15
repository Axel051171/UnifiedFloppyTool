# uft_diff_test

Differential-testing harness for UFT ↔ Greaseweazle compatibility.

Strategy: [`docs/TESTER_STRATEGY.md`](../../docs/TESTER_STRATEGY.md) §2 + §5.

> **Status: HARNESS WIRED (P3.2, Welle 2).** `differential_command()`
> really runs `gw` + `uft`, applies the registry masks and classifies
> the result. The classification logic is fully unit-tested with a
> `FakeRunner` (no binary needed). What is still pending is the
> *corpus*: real `gw` v1.23 reference inputs/outputs and the ~50
> per-command tests — that needs a machine with `gw` installed
> (HIL / Axel's setup, TESTER_STRATEGY §4).

## What it provides

| Symbol | Purpose |
|--------|---------|
| `differential_command(command, args, input_file, registry, *, runner, gw_path, uft_path)` | run gw vs uft, mask-compare, classify |
| `DiffResult` / `DiffStatus` | outcome: `IDENT` / `DIVERGE_OK` / `DIVERGE_BAD` / `FAIL` / `TOOL_MISSING` / `SKELETON` |
| `DiffResult.assert_pass()` | pass on IDENT/DIVERGE_OK, raise on DIVERGE_BAD/FAIL, **skip** on TOOL_MISSING/SKELETON |
| `ToolRunner` / `SubprocessRunner` / `ToolInvocation` / `ToolResult` | tool-execution layer — `runner` is injectable for testing |
| `corpus(relpath)` / `corpus_root()` | resolve paths inside `tests/gw_corpus/` |
| `sha256_file(path)` | hex sha256 of a file |
| `load_registry()` / `DivergenceRegistry` / `DivergenceEntry` | parse + query `tests/conformance/divergence_registry.yaml` (incl. `mask` regexes) |

## Classification

1. either tool absent → `TOOL_MISSING` (skip); crashed / non-zero exit → `FAIL`
2. payloads byte-identical → `IDENT`
3. apply every registry `mask` regex applicable to the command; if the
   masked payloads are now equal → `DIVERGE_OK` (`divergence_ids` lists
   the DIV-NNN entries that were load-bearing)
4. still different → `DIVERGE_BAD` — fix the code or add a DIV-NNN entry

## Why TOOL_MISSING / SKELETON skip instead of pass

A check that could not actually run must not masquerade as a green
gw-compat proof. `assert_pass()` calls `pytest.skip()` for both, so the
skipped count makes the not-yet-real coverage visible.

## Binary resolution

`gw` / `uft` are resolved in this order: explicit `gw_path`/`uft_path`
arg → env override `UFT_DIFF_GW` / `UFT_DIFF_UFT` → bare name on PATH.

## Running the tests

```bash
pip install pytest pyyaml
pytest tests/conformance/ -v
```

Without `gw`/`uft` on PATH: the harness-classification tests
(`test_harness.py`, FakeRunner-driven) **pass**; the real `--version`
smoke differential **skips** with `TOOL_MISSING`.

## Next (P3.2-proper, HIL)

Freeze real `gw` v1.23 reference outputs into `tests/gw_corpus/` and
write the per-command tests with the `differential-test-author` agent.
`differential_command()`'s signature is frozen — those tests need no
harness changes.
