# SCOPE.switch_decision — Non-Floppy Code Decision Required

**Stand:** 2026-05-25 (V415-PLAN MF-262)
**Trigger:** V415_GOAL_PLAN.md §SCOPE.switch_decision

## Fakten

UFT ist deklariert als "forensische Sicherung **historischer Floppy-Disketten**"
(CLAUDE.md §Mission). Drei Subsysteme im Source-Tree fallen darunter NICHT:

| Subsystem | Größe | Files | .pro-Zeilen | GUI-Tab | Tests | Mission-Conflict |
|---|---:|---:|---:|---|---|---|
| `src/switch/` (Nintendo Switch Cartridges + hactool + embedded mbedtls Crypto-Lib) | **10 MB** | ~140 (.c/.cpp/.h) | 35 | `src/gui/uft_switch_panel.cpp` | `tests/test_switch.c`, `tests/test_provider_switch.cpp` | **HOCH** — Switch-Cartridges sind Solid-State, keine magnetischen Floppy-Medien. mbedtls ist Crypto-Library, kein Floppy-Tool. |
| `src/cart7/` (Atari 7800 Cartridges) | 104 KB | ~7 | ~3 | — | — | MITTEL — Cartridge-Forensik liegt im erweiterten Retrocomputing-Scope, aber technisch ROM-Dump, nicht Flux. |
| `src/whdload/` (Amiga WHDLoad resload-API Catalog) | 9 KB | ~4 | ~3 | — | `tests/test_whdload_resload.c` (re-enabled MF-260) | NIEDRIG — WHDLoad löst Amiga-Floppy-Spiele auf HDD lauffähig zu machen → indirekt Floppy-relevant. |

**Gesamt:** ~10.1 MB / ~151 Files / ~41 .pro-Zeilen Off-Scope-Code.

## Die drei Optionen (per V415-PLAN)

### Option A — Extract nach eigenem Repo

- Move `src/switch/` + `src/cart7/` → neues Repo `UnifiedCartridgeTool`
- `src/whdload/` behalten (Amiga-Floppy-Relevanz)
- UFT-Mission unverändert
- ~10.1 MB Codebase-Reduktion, ~38 .pro-Zeilen entfernt
- Tests: `tests/test_switch.c`, `tests/test_provider_switch.cpp` → neues Repo

**Pro:** sauberer Scope, kleinere Build-Zeit, klare Mission-Domain.
**Con:** zweites Repo zu pflegen, eventuelle Switch-User müssen wechseln.
**Aufwand:** 2 Tage (extract + neues Repo + Doku).

### Option B — Mission in CLAUDE.md erweitern

- CLAUDE.md §Mission ergänzen: **"forensische Sicherung historischer
  magnetischer Speichermedien (Floppy) UND Cartridge-Forensik
  (Nintendo Switch, Atari 7800)"**
- Bleibt alles in einem Repo, GUI-Tabs sichtbar
- Honest in Doku, Honest in Code

**Pro:** kein Migration-Aufwand, User-Tools bleiben single-binary.
**Con:** Mission-Scope explodiert, neue Format-Plugins (Switch-Game-Updates
etc.) müssen unterstützt werden — Spec-Status `UNKNOWN` für die Switch-
Subsysteme bleibt offen.
**Aufwand:** 1 Stunde (docs update).

### Option C — Komplett löschen (Tabula Rasa)

- `src/switch/` + `src/cart7/` + `src/gui/uft_switch_panel.*` weg
- `src/whdload/` behalten (Amiga-relevant)
- ~10.1 MB Codebase-Reduktion
- Tests: `test_switch.c`, `test_provider_switch.cpp` → löschen

**Pro:** maximal sauberer Scope, kleinste Build, kein Mission-Konflikt.
**Con:** unwiderruflich — falls die Switch-Cartridge-Pipeline doch
Wert hatte (z.B. für einen User), ist sie weg.
**Aufwand:** 2 Stunden (delete + qmake/CMake reaufräumen).

## Empfehlung (per Plan)

V415_GOAL_PLAN.md sagt explizit: "Switch-Cartridges sind keine Floppy-
Disks — Konflikt mit Tool-Mission". Die ehrlichste Variante ist
**Option A** (Extract → eigenes Repo) — bewahrt die Investition, hält
UFT-Mission rein. Falls die Switch-Funktionalität in UFT nie aktiv
genutzt wurde, ist **Option C** (Delete) der pragmatische Weg.

**Option B** ist der Kompromiss, der eine Lüge in den Code schreibt: "ein
Tool für magnetic + solid-state". In der Praxis bedeutet das, dass jeder
zukünftige Refactor sich die Frage stellen muss: "ist das auch
Switch-relevant?", was die Mission verwässert.

## Hand-off

**Entscheidung erforderlich von Axel.** Sobald die Variante gewählt ist:
- Option A → eigenes 2-Tages-Issue + neuer Repo erstellt
- Option B → 1-Stunden-PR mit CLAUDE.md-Update
- Option C → 2-Stunden-PR mit Delete + qmake/CMake-Cleanup

`src/whdload/` bleibt in allen drei Optionen, da Amiga-Floppy-relevant.
