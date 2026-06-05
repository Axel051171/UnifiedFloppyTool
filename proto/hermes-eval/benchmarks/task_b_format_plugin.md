# Benchmark Task B — Format-Plugin Scaffolding

**Proxy für:** `.claude/skills/uft-format-plugin/SKILL.md` —
"Scaffolds the 6 touchpoints (plugin.c + enum + registry + .pro +
test + CMakeLists)."

## Setup (manuell, Session #2)

Identischer Prompt an beide Tools:

> Scaffold a new UFT disk-image format plugin for `TAP` (Commodore
> 64 cassette tape image, .tap files). The plugin should:
> - Implement `probe`, `open`, `read_track` minimal stubs
> - Register in the plugin registry
> - Have `spec_status = REVERSE_ENGINEERED`
> - Have a `features` matrix with READ supported, WRITE unsupported
> - Compile cleanly against the current UFT build
> - Include a minimal test under `tests/test_tap_plugin.c`
>
> Use the existing `.pro` and CMake structure. Follow UFT coding
> conventions (see CLAUDE.md).

**Wichtig:** der Branch ist disposable, der Code wird NICHT in main
merged. Das Test-Format "TAP" wurde gewählt weil es im UFT-Tree bereits
einen `src/formats/tap/` Scaffold gibt, der bewusst NICHT als komplette
Implementation gemeint ist — Tools sollen es als "neuer" Plugin
behandeln und identische Outputs erzeugen.

## Bewertung

| Bewertungs-Punkt | Max | Was zählt |
|---|---|---|
| 6 Touchpoints angefasst | 5 | alle 6 (plugin.c + enum + registry + .pro + test + CMakeLists)? 5; 4-5: 3; <4: 0 |
| Build-tauglich | 5 | `cmake --build` grün ohne Edit? 5; mit ≤1 trivialer Edit 3; >1 Edit 0 |
| Coding-Convention | 5 | Englische Kommentare, deutsche Log-Strings, type-hints, etc.? |
| spec_status + features | 5 | Beide gesetzt + plausibel? 5; einer 3; keiner 0 |
| Wall-Clock | (Mess-Wert) | Sekunden |
| Token-Cost | (Mess-Wert) | USD |

## Erwartetes Claude-Code-Ergebnis

Claude Code mit dem `uft-format-plugin` Skill bekommt von dem Skill den
exakten 6-Touchpoints-Workflow plus Conventions plus die existierenden
Plugin-Beispiele als Vorlage. Output sollte auf Anhieb build-tauglich
sein.

## Was diese Task NICHT testet

- Echte TAP-Format-Korrektheit (es geht um Scaffolding, nicht Decoder)
- Performance der erzeugten Plugins
- Integration-Tests gegen reale `.tap`-Dateien
