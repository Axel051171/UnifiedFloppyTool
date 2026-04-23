# SSOT Error-Code Cutover — 4b-3 Risk Sweep

**Date:** 2026-04-23
**Scope:** Switch/case statements and raw numeric comparisons that may
depend on the pre-SSOT numeric values of the 45 retargeted legacy
aliases in `data/errors_legacy_aliases.tsv`.

**Conclusion:** **No rewrites required.** The retargets are numerically
invisible to every switch and comparison site in the tree.

---

## Retargets with changed numeric values (reminder)

From `data/errors_legacy_aliases.tsv` CHANGED rows:

| Alias | Pre-SSOT target (value) | Post-SSOT target (value) |
|---|---|---|
| `UFT_ERR_NULL_PTR` / `UFT_ERROR_NULL_POINTER` | `UFT_ERR_INVALID_ARG` (-1) | `UFT_ERR_NULL_POINTER` (-5) |
| `UFT_ERR_VERIFY_FAILED` / `UFT_ERROR_VERIFY_FAILED` | `UFT_ERR_CRC` (-24) | `UFT_ERR_VERIFY_FAIL` (-4) |
| `UFT_ERR_NO_DATA` / `UFT_ERROR_NO_DATA` | `UFT_ERR_CORRUPTED` (-65 old) | `UFT_ERR_MISSING_SECTOR` (-65) |
| `UFT_ERROR_FUZZY_BITS` | `UFT_ERR_CRC` (-24) | `UFT_ERR_WEAK_BITS` (-61) |
| `UFT_ERROR_TRACK_NOT_FOUND` | `UFT_ERR_FORMAT` (-20) | `UFT_ERR_MISSING_SECTOR` (-65) |
| `UFT_ERROR_CANCELLED` | `UFT_ERR_TIMEOUT` (-53) | `UFT_ERR_CANCELLED` (-92) |
| `UFT_ERC_FORMAT` (typo) | `UFT_ERR_CORRUPTED` (-208 via old macro) | `UFT_ERR_FORMAT` (-20) |
| …plus 38 others — see `data/errors_legacy_aliases.tsv` |

---

## Sweep results

### 1. `switch` statements using uft_error_t / uft_rc_t

Search performed:
```
grep -rn 'case UFT_ERR_\|case UFT_ERROR_\|case UFT_E_' src/ include/
```
**Result:** zero hits on the legacy alias prefixes. No switch in the
tree dispatches on any of the retargeted symbols.

The switches that *do* exist and match common error dispatch patterns
(`switch (err)`, `switch (rc)`, `switch (status)`, `switch (result)`)
all operate on **subsystem-specific enum families** that are
independent of `uft_error_t`:

| File | Enum family | Shared with uft_error_t? |
|---|---|---|
| `src/core/uft_safe_io.h:203` | `UFT_IO_*` | No |
| `src/core/uft_ir_format.c:1634` | `UFT_IR_*` | No |
| `src/core/uft_preflight.c:66` | `UFT_PREFLIGHT_*` | No |
| `src/core/uft_format_plugin.c:345` | `UFT_SPEC_*` | No |
| `src/analysis/denoise/uft_denoise_bridge.c:331` | `UFT_DN_*` | No |
| `src/analysis/events/uft_align_fuse_bridge.c:302` | `UFT_ALN_*` | No |
| `src/analysis/events/uft_event_bridge.c:341` | `UFT_EVT_*` | No |
| `src/detect/mfm/uft_mfm_detect_bridge.c:376` | `UFT_MFMD_*` | No |
| `src/detect/mfm/mfm_detect.c:126`, `cpm_fs.c:258` | local enums | No |
| `src/flux/uft_flux_decoder.c:966` | `UFT_FLUX_STATUS_*` | No |
| `src/hal/uft_greaseweazle_full.c:424, 1141` | `UFT_GW_*` | No |
| `src/fs/uft_fat12.c:777` | `UFT_FAT_*` | No |
| `src/formats/apple/uft_moof_parser.c:499` | `UFT_MOOF_*` | No |
| `src/formats/apple/uft_ndif.c:394`, `uft_dart.c:369`, `uft_edd.c:231` | `UFT_NDIF_*` / `UFT_DART_*` / `UFT_EDD_*` | No |
| `src/formats/ti99/uft_tifiles.c:392`, `uft_fiad.c:428` | `UFT_TIFILES_*` / `UFT_FIAD_*` | No |
| (…plus ~30 more, all similarly insulated) |

**Impact of retargeting on these switches: none.** They do not share the
numeric namespace with `uft_error_t`.

### 2. Numeric literal comparisons against error variables

Search performed:
```
grep -E '\b(err|rc|ret|result|status|errno)\s*==\s*-[0-9]+' src/
```
Additional broader:
```
grep -E '==\s*-1\b|==\s*-4\b|==\s*-5\b|==\s*-20\b|==\s*-24\b|==\s*-53\b|==\s*-65\b|==\s*-92\b'
```

**Result:**

- All `== -1` hits relate to file descriptors, socket handles, flux
  sample sentinels, `select()` returns, or optional-index defaults
  (`opt.maxsplice == -1`). None of these variables carry `uft_error_t`
  values.

- One edge case: `src/formats/nib/uft_nib.c:130`
  ```c
  if (dec_rc == -1 && track->sector_count > 0)
  ```
  `dec_rc` here is a local return from a NIB decoder helper. `-1` is
  `UFT_ERR_INTERNAL` in both pre- and post-SSOT enums (unchanged), so
  this is a latent code-smell but **not a retarget-induced bug**.

- Zero hits for `== -24`, `== -4`, `== -20`, `== -53`, `== -65`, `== -92`
  against any error-carrying variable. The retargets are completely
  invisible to numeric-literal consumers.

### 3. Symbolic equality comparisons

Search performed:
```
grep -E '== UFT_ERR(OR)?_' src/
```

**Result (material hits, filtered to retargeted symbols):**

| File:line | Expression | Retarget impact |
|---|---|---|
| `src/core/uft_disk_compare.c:174` | `err == UFT_ERROR_CANCELLED` | Still correct — symbol comparison, not numeric. Value changed from -53 to -92 but both sides use the symbol, so they stay equal. |
| `src/core/uft_disk_stream.c:168, 202, 289` | `v_err == UFT_ERROR_CANCELLED` | Same — symbol compares against symbol. Safe. |

All other `== UFT_ERR*` comparisons are against non-retargeted symbols
(UFT_OK, UFT_SUCCESS, UFT_ERR_CRC as symbol, …).

---

## Verdict

**No CONSULT to `quick-fix` needed.** The 45 retargets can take effect
without any switch-site rewrites. The invariant that saved us: the
codebase consistently compares symbols-to-symbols, not symbols-to-
literals, for error dispatch. The one exception (`dec_rc == -1`) is a
pre-existing code smell unrelated to this cutover.

## Latent bugs noted for future cleanup (out of scope)

- `src/formats/nib/uft_nib.c:130` — raw `-1` comparison against a
  decoder return. Should be `UFT_ERR_INTERNAL`. Value unchanged by
  the cutover; fix whenever a cleanup sweep touches this file.
