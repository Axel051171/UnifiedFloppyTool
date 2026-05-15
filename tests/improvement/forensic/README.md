# improvement/forensic/

Tests that prove UFT's forensic layer — the thing `gw` does not have at
all. Every test here makes a `docs/DESIGN_PRINCIPLES.md` principle
executable.

**Re-scope (2026-05-14):** UFT is GUI-only — there is no `uft` CLI (see
`memory feedback_no_cli`). The original plan had these as Python
CLI-driven tests under this directory; instead the forensic improvement
tests are **core-library C tests** in `tests/test_*.c` (the
`test_loss_report.c` pattern) — they link the engine and assert the
observed behaviour of the public C API directly. This directory keeps
the README/category marker; the actual tests live in `tests/`.

| Test | DESIGN_PRINCIPLES property | Status |
|------|----------------------------|--------|
| `tests/test_loss_report.c` | "Keine stille Veränderung" — every lossy step emits a `.loss.json` sidecar | ✅ exists |
| audit-chain integrity | chain-of-custody — a hash chain over processing events verifies + detects tampering | ✅ covered by `tests/test_provenance_chain.c` (MF-214). `uft_provenance.c` *is* the implemented event hash-chain. NOTE: the separate `uft_audit_trail.*` API is a 0%-implemented `UFT_SKELETON_PLANNED` header — no test possible until it is built. |
| `tests/test_provenance_chain.c` | provenance — output records the tool/source + the chain detects post-hoc tampering | ✅ MF-214 (and it found + fixed a real bug: `uft_prov_verify()` never verified a non-empty chain) |
| `tests/test_marginal_data_preserved.c` | "Kein Bit verloren" — divergent multi-reads are preserved (weak_mask / has_weak_bits), never collapsed; a failed-CRC read cannot outvote a verified one | ✅ MF-215 (required extracting `include/uft/recovery/uft_multiread_pipeline.h` — the implemented module had no public header) |
| `tests/test_destructive_op_consent.c` | destructive/lossy conversions refuse without explicit consent; the dangerous default (no consent) is the safe one; IMPOSSIBLE/UNTESTED pairs refused even WITH consent | ✅ MF-216 — drives `uft_preflight_check()` against real round-trip-matrix pairs |

The `forensic/` improvement category is **complete** — every
DESIGN_PRINCIPLES forensic property that has an implemented, testable
surface now has a core-library C test. (`uft_audit_trail.*` remains the
one un-implemented gap; its property is covered by the provenance
chain.)

Each test asserts *observed* behaviour of the public C API — never
reaches into internals beyond the documented struct fields a tamper
would touch.
