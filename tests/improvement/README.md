# tests/improvement/ — "Better than Greaseweazle" suite

Tests that prove UFT does something `gw` **expectedly cannot**. Where
`tests/conformance/` proves *compatibility* (UFT behaves like `gw`),
this suite proves *superiority* (UFT does more, and the difference is
the point).

Strategy: [`docs/TESTER_STRATEGY.md`](../../docs/TESTER_STRATEGY.md) §3.

> **Status: SCAFFOLD (P3.3, Welle 3).** Directory layout, shared support
> helpers and the skip-discipline are in place. The actual tests are
> written one-property-at-a-time by the `improvement-test-author`
> agent, *once the behaviour is observable on a built `uft`*. Most need
> a built CLI (the cmake test-subset does NOT build it — use qmake);
> the GUI category additionally needs `pytest-qt`.

## The rule

A test belongs here **only if `gw` would fail it**. If `gw` could also
pass, it is a compatibility test and belongs in `tests/conformance/`.
Every test docstring states (a) the `docs/DESIGN_PRINCIPLES.md`
principle it makes executable, and (b) one sentence on why `gw`
expectedly fails it.

## Categories (per TESTER_STRATEGY §3)

| Dir | Proves | gw can't because |
|-----|--------|------------------|
| `forensic/` | loss reports, audit chain, marginal-data preservation, consent gating, provenance | gw has no forensic layer at all |
| `multi_device/` | KryoFlux / SCP / FluxEngine providers, provider-switch consistency | gw only drives Greaseweazle hardware |
| `format_extension/` | IPF / STX / KFX / proprietary JP decode | gw's format set is narrower |
| `copy_protection/` | Dungeon Master, Lenslok, long-track detection | gw does not analyse protection |
| `gui/` | main-window smoke, capability-gating, format-tab workflow | gw is CLI-only |
| `concurrency/` | parallel drives, long-running session stability | gw has no multi-device session model |

## Skip discipline

An improvement test that cannot observe its behaviour **skips** — it
never silently passes (that would fake superiority) and never hard-fails
the suite over an environment gap:

- no built `uft` binary → `uft_cli` fixture skips the test
- `pytest-qt` not installed → `@pytest.mark.requires_qt` tests skip
- behaviour not yet implemented in `src/` → the `improvement-test-author`
  agent STOPs and does not write the test at all

## Shared support

| Symbol (`_support.py`) | Purpose |
|------------------------|---------|
| `resolve_uft()` / `uft_available()` | locate the built CLI ($UFT_BIN → build paths → PATH) |
| `run_uft(*args)` / `UftRun` | run the CLI; skips if not built |
| `qt_test_available()` | is `pytest-qt` importable |
| `uft_cli` fixture (`conftest.py`) | per-test binary resolution + skip |
| `@pytest.mark.requires_qt` | GUI test marker — auto-skipped without pytest-qt |

`conftest.py` also puts `tools/` on `sys.path`, so improvement tests can
reuse `uft_diff_test.corpus()` for shared inputs from `tests/gw_corpus/`.

## Running

```bash
pip install pytest pyyaml          # + pytest-qt for the gui/ category
pytest tests/improvement/ -v
```

In the scaffold state: `test_scaffold.py` passes (it verifies the
layout itself); every category dir is otherwise empty until the
`improvement-test-author` agent populates it.
