---
name: uft-release
description: |
  Use when preparing a UFT release (version bump, tag, changelog, CI
  verification, artefact publishing). Trigger phrases: "release v4.2",
  "version bump", "tag new release", "prepare release", "changelog für
  X", "bump version to", "release UFT". Executes the 8-step release
  checklist consistently. DO NOT use for: hotfix branches (separate
  workflow), internal dev builds, deploying pre-releases to package
  managers, producing DEB/RPM packages (separate skill later).
---

# UFT Release

Use this skill when preparing a UFT release. The workflow is error-prone
because 6 files need version bumps in sync and the CI must pass before tag.

## When to use this skill

- Quarterly release (v4.2, v4.3, ...)
- Hotfix release for critical bug
- Release after a major feature set (HAL M3, DeepRead enhancements, ...)

**Do NOT use this skill for:**
- Long-lived hotfix branches — separate workflow in `docs/HOTFIX.md`
- Internal dev builds / nightlies — no tag, just `make`
- Prerelease channels (beta, RC) — use `release_prerelease.sh` instead
- Creating DEB/RPM packages — separate skill (not yet written)

## The 8-step workflow

| # | Step | Script |
|---|------|--------|
| 1 | Bump version in all 6 files | `bump_version.py` |
| 2 | Update CHANGELOG.md | manual (template in script) |
| 3 | Run full test suite locally | `pre_release_check.sh` |
| 4 | Verify docs are current | `doc_check.sh` |
| 5 | Verify no TODO/XXX in code | grep filter |
| 6 | Create signed git tag | `git tag -s vX.Y.Z -m "..."` |
| 7 | Push tag; CI build runs | `git push origin vX.Y.Z` |
| 8 | Draft GitHub release notes | manual from CHANGELOG |

## The 6 files that need version bumps

| File | What |
|------|------|
| `UnifiedFloppyTool.pro` | `VERSION = X.Y.Z` |
| `CLAUDE.md` | "version X.Y.Z" in header |
| `CHANGELOG.md` | New section header |
| `docs/USER_MANUAL.md` | Version in intro |
| `include/uft/uft_version.h` | `UFT_VERSION_*` macros |
| `README.md` | Badge + install examples |

Miss one → users see inconsistent version info.

## Workflow

### Step 1: Bump versions

```bash
python3 .claude/skills/uft-release/scripts/bump_version.py --version 4.2.0
```

The script patches all 6 files atomically. Dry-run first:

```bash
python3 .claude/skills/uft-release/scripts/bump_version.py --version 4.2.0 --dry-run
```

### Step 2: CHANGELOG.md

Open `CHANGELOG.md`. Add a new section at top:

```markdown
## [4.2.0] - 2026-04-24

### Added
- ...

### Changed
- ...

### Fixed
- ...

### Removed
- ...
```

Fill from git log:
```bash
git log --pretty=format:"- %s (%h)" vLAST..HEAD > /tmp/commits.txt
# Then categorize manually into Added/Changed/Fixed/Removed
```

### Step 3: Pre-release check

```bash
bash .claude/skills/uft-release/scripts/pre_release_check.sh
```

Runs:
- Full `qmake && make -j$(nproc)` from clean
- Full `ctest --output-on-failure`
- `python3 scripts/audit_plugin_compliance.py`
- `bash .claude/skills/uft-stm32-portability/scripts/check_firmware_portability.sh`
- grep for TODO/XXX/FIXME in touched files

Any non-zero exit halts release.

### Step 4: Git tag

```bash
git tag -s v4.2.0 -m "UFT v4.2.0 — HAL M3 complete, DeepRead enhancements"
```

Use signed tags (requires GPG key configured).

### Step 5: Push + release notes

```bash
git push origin v4.2.0
```

CI picks up the tag, runs full build + tests + artefact upload. When
green, draft GitHub release notes from `CHANGELOG.md` section.

## Verification

```bash
# 1. All 6 files agree on version
grep -h "4\.2\.0" \
    UnifiedFloppyTool.pro CLAUDE.md CHANGELOG.md \
    docs/USER_MANUAL.md include/uft/uft_version.h README.md \
    | wc -l
# expect: at least 6 matches

# 2. Tag exists and is signed
git tag -v v4.2.0

# 3. CI passed
gh run list --workflow=release --limit 1
# expect: latest status=completed, conclusion=success

# 4. GitHub release page
gh release view v4.2.0
```

## Common pitfalls

### Bumping version in only 3-4 files

A missed version bump causes "which version am I running?" confusion for
users. `bump_version.py` patches all 6 atomically — use it.

### Empty CHANGELOG section

A release with `## [4.2.0]` and no entries = user doesn't know what
changed. If truly no changes, it's not a release.

### Unsigned tag

`git tag v4.2.0` (without `-s`) creates an unsigned tag. Forensic
software should ship with signed tags for provenance.

### Pre-release check skipped

"I'll fix it in the next release" → ship broken. The pre-release script
is there to block this.

### Tag pushed before CI runs locally

Pushing a tag triggers public release machinery. If your local build
fails after tag push, you've already burned a version number.

## Related

- `CHANGELOG.md` — running log of all releases
- `.github/workflows/release.yml` — CI release pipeline
- `docs/RELEASE_PROCESS.md` — longer-form documentation
- `.claude/skills/uft-stm32-portability/` — firmware check (part of gate)
