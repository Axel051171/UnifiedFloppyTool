---
name: uft-disk-analyst
description: |
  Use when analysing an actual disk image or flux capture with UFT, or when
  investigating whether a UFT format parser reads a real file correctly.
  Trigger phrases: "analysiere dieses image", "was ist das für eine diskette",
  "liest UFT diese datei richtig", "prüfe den X-parser gegen echte daten",
  "analyse this dump", "identify this disk", "verify the X reader".
  Runs a tiered analysis that labels every claim by evidence level and never
  presents a guess as a fact. DO NOT use for: adding a new format plugin
  (→ uft-format-plugin), generating synthetic fixtures (→ uft-flux-fixtures),
  build errors (→ uft-cross-platform-build), release work (→ uft-release).
---

# UFT Disk Analyst

Analyse a real disk image or flux capture, or check whether a UFT parser reads
a real file correctly. The output of this skill is an **evidence-labelled
report**, not a verdict presented as fact.

This skill exists because UFT has shipped parsers built against invented specs
(FMT-2/3/10/11/12: NFD, SaveDskF, FDI and others). The failure was never a wrong
decoder — it was a claim about a format that no real file had ever confirmed.
Every statement here carries the evidence that backs it, and where there is no
evidence, it says so.

## The one rule that governs everything

**A claim's confidence never exceeds its evidence.**

UFT tracks evidence on **two orthogonal axes**. Confusing them is itself a
reporting error, so the report always names which one it means.

### Axis 1 — verification tier: has *our parser* been checked against reality?

This is the axis this skill is about. SSOT: `docs/VERIFICATION_PLAN.md`,
generated table in `docs/VERIFICATION_TIERS.md`, computed by
`scripts/gen_verification_tiers.py`.

| Tier | What backs it |
|---|---|
| `T1` | test against a REAL reference image (hardware dump / original file) |
| `T1b` | test against a CROSS-TOOL image (VICE / WinUAE / HxC / SAMdisk / greaseweazle produced it, UFT reads it back) |
| `T2` | synthetic round-trip test AND byte layout verified against an authoritative external reference (`docs/spec_verification.json`) |
| `T3` | unverified: no test, or a synthetic test **without** spec verification |

A green T3 test proves self-consistency, not correctness. That is exactly the
trap that produced the five fabrications.

**Current state — run `python scripts/gen_verification_tiers.py` for today's
numbers.** At the time this skill was written: T1 = 2, T1b = 7, T2 = 11,
**T3 = 68** of 88 plugins. Assume by default that the format in front of you is
unverified, and check.

### Axis 2 — `spec_status`: how well is *the format itself* specified?

`include/uft/uft_format_plugin.h:54`. This says nothing about whether UFT reads
it correctly.

| Value | Meaning **in this project** |
|---|---|
| `UFT_SPEC_OFFICIAL_FULL` | public and complete specification |
| `UFT_SPEC_OFFICIAL_PARTIAL` | public but incomplete |
| `UFT_SPEC_REVERSE_ENGINEERED` | no official spec, RE-based |
| `UFT_SPEC_DERIVED` | **de-facto standard without a formal spec** |
| `UFT_SPEC_UNKNOWN` | not declared — itself a principle violation |

> **`DERIVED` does not mean "confirmed by a real file".** It is a statement
> about the *spec*, not about our testing. "Confirmed by a real file" is `T1`.
> Never raise `spec_status` because a reference image parsed — raise the tier by
> adding the image to the corpus manifest.

A finding backed by neither axis is written as *"this looks like X,
unconfirmed"* — never as *"this is X"*.

## Autonomy boundaries (binding)

| Action | Autonomy |
|---|---|
| Read the image, run UFT's read-only analysis, hexdump, inspect | free |
| Search the web for format documentation | free — **but see below** |
| Run existing tests, `gen_format_list.py`, `gen_verification_tiers.py`, `check_consistency.py` | free |
| Write a **findings report** naming a bug in a parser | free |
| Edit parser code, fix a bug, change `spec_status` | **only after human approval** |
| Add an image to `tests/corpus_manifest/manifest.json` | free with documented provenance + SHA-256 |
| Commit anything | **never autonomously** |

**Why fixes are not autonomous:** UFT is a forensic tool; its value is that its
output can be trusted. An automatic commit adopts a mistaken change as truth.
Fixing stays a human decision with the machine laying out the evidence.

### The freeze rule applies here too

`.claude/CLAUDE.md` Daueraufgabe 5 (**EINFRIER-REGEL, MF-363**): no new
unverified code in the format or decoder layer. This skill is squarely on the
allowed side — verification, corpus and test work — and **raising a format from
T3 to T1/T1b is exactly the currency the moratorium is denominated in.** What is
*not* allowed is reaching for a new parser or a new format variant because the
analysis turned up an unsupported file. Bugfixes to existing parsers are allowed.

## Web research: hypothesis, never truth

Searching for a format spec is allowed and useful. What is **not** allowed is
letting a web result become the basis of a parser or a verdict:

- A spec found online is a hypothesis until it is either the vendor's own
  document (cite the URL, record it in `docs/spec_verification.json`) **or**
  confirmed by a real file that parses under it (then add the file to the corpus
  manifest — that is what moves the tier).
- If two sources disagree, that is a finding, not a thing to resolve by picking
  one. Report the disagreement.
- Never write a field offset, magic number or size into code because a blog said
  so. That is how the fabricated parsers were built. It goes into the report as
  *"source X claims offset N, unconfirmed against a real file"*.

Every web-sourced claim in the report names its URL. A claim without a source is
struck, not softened.

## Procedure

### 1. Establish what the file actually is — from the file

Before any research, read the bytes:

```bash
bash .claude/skills/uft-disk-analyst/scripts/analyze_image.sh <file>
```

It reports size, SHA-256, magic bytes, entropy and geometry candidates, and
labels each as `MEASURED` (read from the file) or `INFERRED` (a guess from
size/shape). It does **not** name a format from size alone: a size match is a
candidate, not an identification.

The magic table is taken from UFT's own plugin constants, not from memory — see
the header of the script. The `55 AA` boot signature is **not** a format
identifier; it means x86-bootable and appears on hard disks too. FAT12 is
identified from the BPB, not from `55 AA`.

### 2. Run UFT's own read-only analysis

Let UFT identify and parse the image. Capture what it claims: format, geometry,
filesystem, bad sectors. Then look up **both axes** for that format:

```bash
python scripts/gen_verification_tiers.py --md | grep -i <format>
```

A `T3` parser's output is itself a hypothesis, and the report says so.

### 3. Cross-check against the reference corpus

This is the step that catches fabrications.

- Manifest (in-repo truth): `tests/corpus_manifest/manifest.json`
- Image files: `tests/corpus_free/` (tracked, rights-free, self-created) or
  `tests/corpus/` (gitignored — copyright-restricted, re-acquired from the
  provenance recorded in the manifest)

If an entry for this format exists, parse it too and compare. If none exists,
that is the headline finding: **this format has no ground truth**, every verdict
about it is T3, and `references/ground_truth_sources.md` describes how to obtain
one without hardware.

### 4. Report, with evidence per line

Every line carries its label. A parser bug found here is written up — not fixed
— with a reproduction the human can run.

### 5. Only after approval: fix

Re-run steps 2–3 to show the parser now reads the reference image. The fix is
proven by the reference, not by the absence of an error. If the fix was enabled
by adding a corpus image, regenerate the tier table:

```bash
python scripts/gen_verification_tiers.py --write
```

## Report format

```
## Disk analysis — <file>, <date>

Identity
  Size:        <bytes>            [MEASURED]
  SHA-256:     <hex>              [MEASURED]
  Geometry:    <c/h/s>            [MEASURED | INFERRED]
  Magic:       <bytes @ offset>   [MEASURED]  or  none found
  Candidate:   <format>           [INFERRED from size/shape — not confirmed]

UFT parse
  Format:      <what UFT says>
  Tier:        <T1 | T1b | T2 | T3>        ← was the PARSER checked against reality
  spec_status: <OFFICIAL_FULL | ... >      ← how well is the FORMAT specified
  Filesystem:  <...>
  Bad sectors: <n> at <locations> [MEASURED]

Ground truth
  Corpus:      <manifest entry + sha256>  or  NONE — no reference for this format
  Comparison:  <matches | diverges at ...>  or  not possible without reference

Findings
  F-1  [severity]  <what is wrong, with the command that shows it>
       Evidence:   <command → output>
       Fix:        <exact step>   — NOT APPLIED, awaiting approval
       Confidence: <why this label and not a higher one>

Limits
  <what this analysis could not determine, and why>
```

## What this skill does not do

- It does not identify a format from size alone.
- It does not write parser code from a web spec.
- It does not fix or commit without approval.
- It does not call anything "verified" that a real file has not confirmed.
- It does not raise `spec_status` to stand in for a missing test.
- It does not aim for "perfect" — it aims for *labelled*, which is achievable.

## Anti-patterns (each observed in this codebase)

- **Size-to-format leap.** "880 KB, so it's an Amiga ADF." Size is a candidate;
  the encoding confirms it.
- **Blog-spec to code.** Writing an offset into a parser because a source stated
  it. → It goes into the report as unconfirmed.
- **Green because empty.** A parser that raises no error on a file it cannot
  decode; or a self-test that silently skips on this platform and exits 0. →
  Confirm it produced *correct* output, not merely that it did not fail. The
  self-test shipped with this skill runs everywhere for exactly that reason.
- **Fix presented as verified.** A repaired parser proven only by "no error now."
  → It is proven by reading the reference image correctly.
- **Wrong axis.** Raising `spec_status` because a file parsed, or claiming T1
  because a vendor document exists. The two axes do not substitute for each
  other.
- **Unverified constants in the analysis tooling itself.** The first version of
  this skill's own magic table had `HYCHE` where HFE's signature is `HXCPICFE`,
  so it could never have detected an HFE file. Tooling gets checked against the
  plugin constants like everything else.

## Self-test

```bash
bash .claude/skills/uft-disk-analyst/scripts/selftest.sh
```

Proves the core property in both directions — a real FAT12 image and random
bytes **of the same size** must not receive the same verdict, and the difference
must show up in a measured property — plus that every magic constant in the
script matches the corresponding UFT plugin. It runs on Windows/Git-Bash and
Linux alike; it never skips silently.
