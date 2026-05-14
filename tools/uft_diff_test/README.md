# uft_diff_test

Differential-testing harness for UFT ↔ Greaseweazle compatibility.

Strategy: [`docs/TESTER_STRATEGY.md`](../../docs/TESTER_STRATEGY.md) §2 + §5.

> **Status: SKELETON (P3.1, Welle 1).** The API surface is complete and
> frozen; `differential_command()` does not yet execute `gw`/`uft`. Real
> subprocess execution + byte comparison land in Welle 2 (P3.2).

## What it provides

| Symbol | Purpose |
|--------|---------|
| `differential_command(command, args, input_file, registry)` | run gw vs uft, compare, classify — **stub in skeleton** |
| `DiffResult` / `DiffStatus` | the outcome: `IDENT` / `DIVERGE_OK` / `DIVERGE_BAD` / `FAIL` / `SKELETON` |
| `DiffResult.assert_pass()` | pass on IDENT/DIVERGE_OK, raise on DIVERGE_BAD/FAIL, **skip** on SKELETON |
| `corpus(relpath)` / `corpus_root()` | resolve paths inside `tests/gw_corpus/` |
| `sha256_file(path)` | hex sha256 of a file |
| `load_registry()` / `DivergenceRegistry` | parse + query `tests/conformance/divergence_registry.yaml` |

## Why SKELETON skips instead of passes

A `SKELETON` result means the gw-compat check is *not yet wired*, not
that it *succeeded*. `assert_pass()` calls `pytest.skip()` for it, so
the suite stays honest: pytest is green (skeleton stands) but the
skipped count makes the not-yet-real coverage visible.

## Running the skeleton tests

```bash
pip install pytest pyyaml
pytest tests/conformance/ -v
```

Expected in the skeleton: the corpus-integrity and registry-parsing
tests **pass**; the `differential_command()` smoke test **skips** with
a "lands in Welle 2 (P3.2)" reason.

## Filling it in (Welle 2 / P3.2)

`differential_command()` keeps its signature and return type. Only its
body changes — see the "Welle-2 body sketch" comment in `__init__.py`.
Tests written against the skeleton API today keep working unchanged.
