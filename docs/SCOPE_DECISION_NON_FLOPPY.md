# SCOPE.switch_decision — AUSGEFÜHRT 2026-08-19 (MF-271)

Die Entscheidung unten stand seit 2026-05-25 und war für „nach dem
v4.1.5-Tag" terminiert. Ausgeführt am 2026-08-19.

**Entfernt:** `src/switch/` (788 Dateien, 10 MB, darin vendored hactool +
mbedtls), `src/cart7/`, `include/uft/cart7/`,
`src/gui/uft_switch_panel.{h,cpp}`, `src/gui/uft_cart7_panel.{h,cpp}`,
92 Zeilen aus der `.pro` (switch_support-Block, win32-POSIX-Shims,
cart7_support-Block).

**Backup:** Tag `archive/pre-mf271-switch-removal`, gepusht.

**Was den Ausschlag gab, über die Ursprungsbegründung hinaus:** die
Herkunft war *benannt*, aber nicht *belegt*. `src/switch/hactool/VENDORED.md`
nannte Upstream und Lizenzen (hactool ISC, mbedtls Apache-2.0), doch im Baum
lag kein ISC-Lizenztext, kein gepinnter Upstream-Commit („exakter Stand
unbekannt"), und keine CI-Job baute je `CONFIG+=switch_support`. Für ein
Projekt, das forensische Nachvollziehbarkeit beansprucht, ist eine
Lizenzangabe ohne Beleg dieselbe Kategorie wie ein Parser gegen eine
erfundene Spezifikation — nur auf der Rechteebene. Ein eigenes Repository
hätte die Frage verschoben, nicht beantwortet.

**Korrektur an der Liste unten:** die beiden dort genannten Tests gehören
**nicht** dazu und bleiben im Baum.

| Datei | was sie wirklich testet |
|---|---|
| `tests/test_switch.c` | das **Format-Plugin** Switch XCI/NSP (`src/formats/nintendo/uft_switch.c`) — ein Disk-Image-Parser aus den 88, kein Cartridge-Dumper |
| `tests/test_provider_switch.cpp` | das **Umschalten zwischen Hardware-Providern** (`ProviderV2Variant`, MF-221) — HAL-Abdeckung, nichts mit Nintendo zu tun |

Beide waren beim ersten Durchgang mitgelöscht und sind zurückgeholt; die
Suite steht wieder bei 201/201. Die Liste unten hatte auf dem Wort „switch"
zusammengefasst, was nur den Namen teilt.

---

# SCOPE.switch_decision — RESOLVED 2026-05-25 → Option C (Delete)

**Decision:** **Option C — Delete** `src/switch/` + `src/cart7/` +
`src/gui/uft_switch_panel.{h,cpp}` + Tests. `src/whdload/` bleibt
(Amiga-Floppy-relevant). User-bestätigt 2026-05-25.

**Begründung:** seit v4.1.0 Release wurden Switch + cart7 ausschließlich
von cleanup-passes berührt (`MF-011 Welle 7` + die initiale Import).
Kein einziger Feature-Commit. Keine Bug-Reports. Keine User-Iteration.
Die `mbedtls`-Maintenance läuft als toter Aufwand mit (~10 MB / 150 Files
/ ~38 .pro-Zeilen Off-Scope-Last).

**Ausführungs-Timing:** **POST v4.1.5-tag**, nicht im RC1-Window
(2026-05-29). Ein 10 MB / 150-Files Cleanup-PR in den letzten 4 Tagen
des Windows ist exakt die Art von Last-Minute-Big-Change die das
Window verhindern soll. Implementation als MF-271 nach v4.1.5-tag,
geschätzt 2h:
- qmake .pro: ~38 Zeilen entfernen
- CMake: kein Eintrag (Switch ist nicht in tests/CMakeLists)
- Delete tree: `src/switch/`, `src/cart7/`, `src/gui/uft_switch_panel.*`,
  `tests/test_switch.c`, `tests/test_provider_switch.cpp`
- Doku: CLAUDE.md erwähnt Switch nicht → keine Änderung nötig

**Backup-Strategie:** Pre-delete `git tag archive/pre-mf271-switch-removal`
auf dem letzten Commit der die Subsysteme enthält. Falls jemand jemals
wieder Switch-Cartridge-Support braucht, ist der Code via Tag-Auschecken
erreichbar.

---

# SCOPE.switch_decision — Original Analysis (Historical)

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
