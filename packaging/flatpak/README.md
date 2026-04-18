# Flatpak / Flathub packaging

This directory holds the files a Flatpak / Flathub manifest needs to
reference when packaging UnifiedFloppyTool.

## App ID

`io.github.Axel051171.UnifiedFloppyTool`

The prefix `io.github.` is lowercase (Flathub rule); the GitHub owner
`Axel051171` and repository name `UnifiedFloppyTool` are case-preserved
to match the canonical GitHub URL.

## Files

| File | Purpose |
|------|---------|
| `io.github.Axel051171.UnifiedFloppyTool.metainfo.xml` | AppStream metadata (Flathub submission) |
| `io.github.Axel051171.UnifiedFloppyTool.desktop`      | Freedesktop launcher entry |
| `screenshots/`                                         | Reserved for release screenshots linked from metainfo |

Validate the metainfo locally with:

```
appstreamcli validate io.github.Axel051171.UnifiedFloppyTool.metainfo.xml
```

## Hardware controller bundling

UnifiedFloppyTool shells out to external CLIs for several hardware
controllers. The recommendation for the Flatpak manifest:

- **Greaseweazle (`gw`)** — bundle. It is the most common controller and
  pure Python, so it fits cleanly in the Flatpak via a `pip install`
  module. Without it, the Flatpak is offline-analysis only.
- **KryoFlux (`dtc`)** — do **not** bundle. The KryoFlux licence
  forbids redistribution. Document that users supply their own `dtc`
  binary via `--filesystem=host:ro` or sandbox extension.
- **FC5025 (`fcimage`)** — optional; only a small user base.
- **Applesauce**, **SuperCard Pro** — closed-source, platform-specific,
  skip from the Flatpak.

USB device access requires `--device=all` in the Flatpak manifest, plus
the Greaseweazle udev rules installed on the host (documented upstream).

## Flathub submission checklist

1. `appstreamcli validate` passes on the metainfo.
2. Screenshots committed under `screenshots/` and linked in metainfo.
3. Flatpak manifest at `flathub/io.github.Axel051171.UnifiedFloppyTool`
   builds locally with `flatpak-builder`.
4. Manifest tagged at an upstream release (e.g. `v4.1.3`) — do not
   track `main`.
5. Open PR against `flathub/flathub`.

## Related upstream work

Issue [#19](https://github.com/Axel051171/UnifiedFloppyTool/issues/19)
tracks the Flathub submission. A WIP manifest is maintained at
<https://github.com/hadess/flathub/tree/wip/hadess/unified-floppy-tool>.
