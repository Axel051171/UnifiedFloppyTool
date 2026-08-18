# Flatpak / Flathub packaging

This directory holds the upstream-hosted files a Flatpak manifest
references. The canonical desktop launcher lives one level up at
`../linux/io.github.Axel051171.UnifiedFloppyTool.desktop` and is shared
between the Flatpak build and the distro .deb/.rpm packaging.

## App ID

`io.github.Axel051171.UnifiedFloppyTool`

The prefix `io.github.` is lowercase (Flathub rule); the GitHub owner
`Axel051171` and repository name `UnifiedFloppyTool` are case-preserved
to match the canonical GitHub URL.

## Files in this directory

| File | Purpose |
|------|---------|
| `io.github.Axel051171.UnifiedFloppyTool.metainfo.xml` | AppStream metadata |
| `screenshots/`                                         | Release screenshots linked from metainfo — **does not exist yet, see below** |

Validate the metainfo locally with:

```
appstreamcli validate io.github.Axel051171.UnifiedFloppyTool.metainfo.xml
```

## Blocking item before any Flathub submission: a screenshot

The metainfo carries **no `<screenshots>` block**. It previously linked
`screenshots/main-window.png`, which has never existed in this repository —
a 404 that `appstreamcli` flags and that renders as a broken entry in
software centres. The dead link was removed rather than left in place
(MF-413).

Flathub requires at least one screenshot, so this is a hard blocker for
submission. It needs an actual capture of the running application from a
machine that can start the GUI; it cannot be produced from CI or from a
headless checkout. Steps:

1. Run the GUI, capture the main window (PNG, no scaling artefacts).
2. Save it as `packaging/flatpak/screenshots/main-window.png`.
3. Restore the `<screenshots>` block — the previous wording is preserved in
   the XML comment where the block used to be.
4. Re-run `appstreamcli validate`.

## Related files outside this directory

- `../linux/io.github.Axel051171.UnifiedFloppyTool.desktop` — desktop
  launcher, installed by both the Flatpak manifest and the `.deb` build
  in `.github/workflows/release.yml`.
- `../../resources/icons/app_256.png` — icon, installed under the
  reverse-DNS name in both packaging paths.

## Hardware controller bundling (confirmed by @hadess)

Greaseweazle is bundled in the Flatpak manifest (pure Python, pulled
via pip). KryoFlux `dtc` is not bundled — its licence forbids
redistribution; users supply their own via a sandbox extension.
Applesauce, SuperCard Pro and FC5025 tooling is closed-source or
platform-specific and not bundled.

USB access is granted via `--device=all` in the manifest; Greaseweazle
udev rules install on the host, not inside the sandbox.

## Upstream work

Issue [#19](https://github.com/Axel051171/UnifiedFloppyTool/issues/19)
tracks the Flathub submission. A working manifest is maintained by
@hadess at
<https://github.com/hadess/flathub/tree/wip/hadess/unified-floppy-tool>;
upstream (this repo) does not plan to submit to Flathub directly.
