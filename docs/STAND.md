# Stand des Baums (generiert)

**NICHT von Hand editieren** — erzeugt von `scripts/gen_stand.py` (MF-704). Jede Zahl hat eine Quelle im Baum und wird bei jedem Lauf neu gelesen.

Stand: 2026-08-31

---

## Die vier Release-Kennzahlen

| Kennzahl | Stand | Richtung | Quelle |
|---|---|---|---|
| ungeprüfte **Format-Plugins** (T3) | **50** von 88 | runter | `docs/VERIFICATION_TIERS.md` |
| ungeprüfte **Dateisystem-Leser** | T0 1 · T1 5 · T1b 1 · T2 1 | runter | `docs/VERIFICATION_TIERS_FS.md` (MF-694) |
| angebotene **Wandlungspfade** | **14**, davon 6 verlustfrei | rauf | `src/core/uft_roundtrip.c` |
| leckende Tests | 0 zu halten | null halten | ASan/UBSan in CI |
| **Bench-Alter je Controller** | keine Hardware (MF-310) | runter | `docs/CAPABILITIES.md` |

---

## Lizenzlage

### Im eigenen Baum

- **6** Port-Erklärungen im Quellkopf, davon **1** ohne SPDX-Kopf
  - src/formats/amiga/uft_amiga_protection.c             C99 port of XCopy Pro (1989-2011) 68000 Assembly algorithms:
- SPDX außerhalb der Politik: **0**
- Fließtext-Attributionen (Verdachts-Stufe, `LIZ-1`): **172**
- Quarantäne: 1 vollzogen, 4 vorgemerkt, 0 aufgelöst (`docs/QUARANTINE.md`)

### Gesichtete Fremd-Repos, nach Lizenzzone

- **GRUEN**: 12 — ADFCommander, ADFDiskBox, FloppyTools, atrcopy, disk-peek, fluxpy, hfs2dfxml, lib1541img, mame, mfmdisk, mfsreader, picturedsk
- **GELB**: 10 — DiscImageManager, DiskImageTool, apple-ii-fluxdoctor, apple2-disk-tools, atari-st-tools, flux-analyze, fluxtoimd, hardsector_tool, nibtools, pyAccess1581
- **PRUEFEN**: 16 — FloppyControl, FluxBridge, HxCFloppyEmulator, OpenCBM, a8rawconv, amigadx, atrip, dfsimage, fluxfox, fuseadf, gwnbd, ipf-flux, libcbmimage, libdsk, mkatr, mkd64
- **ROT**: 7 — DiskToolC64, Jacknife, adf-tools, adfopus, adfrescue, floppydiskimagetool, st2disk

> ROT heisst **keine** gefundene Lizenz, nicht „schlecht“. MF-703 prüft der Vermesser auch Lizenzdateien unter anderem Namen (`gpl-3.0.txt`), das Wurzelverzeichnis und Lizenz-Prosa im Quellkopf — zwei von neun ROT-Repos kamen dadurch zurück. Ein Fund ohne Kanal verfällt nicht, er wartet benannt im Fundus (`CLAUDE.md` §Der stärkste legale Kanal).

---

## Was offen ist

`docs/OPEN_ITEMS.md` führt **7070** Zeilen in **82** Abschnitten.

**erledigt** (1):
- GCR-1 — der 6-and-2-Dekoder steht, 560 von 560 Sektoren belegt (MF-715)

**offen** (10):
- FMT-15 — kopflose Formate erkennen allein an der Größe (MF-691)
- Scout-Block 4 — neun Gutachten, zwei Aufträge, ein Fundus (MF-694)
- LIZ-4 — „allen Code mit Lizenzproblem nachbauen": was die Messung daraus macht (MF-697)
- FS-3 — die PC-Seite: Leser echt, Schreiber elf Attrappen, Tür auf dem Nebengleis (MF-709)
- ORAK-2 — `to_woz2`: das erste Oracle, das baut, läuft und dessen Ausgabe wir erkennen (MF-711)
- GCR-2 — `d13`: 5-and-3, der Beweisweg steht bereits offen (MF-717)
- FMT-17 — die zweite Quelle liegt vor (MF-720)
- FMT-18 — sechs Formate ohne jedes fremde Gegenstück (MF-720)
- FMT-19 — `2img` verwirft, was sein Kopf sagt (MF-725)
- FMT-20 — `kfx_probe()` beansprucht jede Datei mit einem `0x0D` (MF-726)

**wartet-eigentuemer** (8):
- ORPH-5 — `uft_convert_memory()` ist öffentlich und wird nur von Tests gerufen (MF-693)
- ORAK-1 — zwei Oracles tragen einen Test, ohne registriert zu sein (MF-693)
- Drei Eigentümer-Handgriffe, die Fundus in Nutzung verwandeln (MF-695)
- FMT-16 — `86f` verfehlt die Spezifikation an vier Stellen und kündigt es als „SUPPORTED" an (MF-707)
- FMT-17 — `do` und `po` entscheiden ohne hinzusehen (MF-713)
- GCR-3 — `nib` ist auf Unabhaengigkeit gesperrt, nicht auf Code (MF-723)
- ORPH-6 — MOOF und A2R: kein Zugang, aber die falsche Tuer geht auf (MF-726)
- FMT-21 — Konfidenz ohne Skala: 35 bis 85 für dieselbe Erkenntnis (MF-728)

**ohne Status-Marke: 63** — noch nicht gesichtet, weder offen noch erledigt. Die Marke wird von Hand vergeben (`docs/OPEN_ITEMS.md`, Abschnitt Status-Marke); ein Prosa-Scan waere gemessen schlechter als die Luecke.

---

## Wo was steht

| Dokument | Rolle | gepflegt von |
|---|---|---|
| `docs/STAND.md` | **diese Seite** — der Einstieg, alle Zahlen abgeleitet | `scripts/gen_stand.py` |
| `docs/OPEN_ITEMS.md` | die EINE Liste: offene Punkte **mit Begründung** | von Hand, Status-Marke je Abschnitt |
| `docs/KNOWN_ISSUES.md` | Geschichtsbuch — was war, und warum es so entschieden wurde | wächst, wird nie gekürzt |
| `docs/QUARANTINE.md` | Dateien mit Herkunftsfrage, je eine Zeile mit Rückweg | Verfahren: `QUARANTINE_PROCESS.md` |
| `docs/VERIFICATION_TIERS*.md` | Prüfstufen je Format bzw. Dateisystem | generiert, mit Frische-Tor |
| `docs/MASTER_PLAN.md` | Bau-Meilensteine (M3.x, HAL-Wiring) | von Hand |

**Alte Listen werden nicht gelöscht, sondern umgeleitet.** Eine gelöschte Liste nimmt ihre Begründungen mit — und die sind der Grund, warum dieser Baum Entscheidungen nicht zweimal trifft. Was gelöscht gehört, sind *doppelte Zahlen*, nicht *Gedächtnis*.

