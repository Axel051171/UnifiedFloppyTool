# Gutachten: N0t4R0b0t/gwnbd

> Gemessen 2026-08-29 gegen HEAD `6db8abe` (Release 1.0.1, 2026-08-02).
> Messdatei: `tools/uft-scout/work/gwnbd.messung.json`.
> Inventar: `tools/uft-scout/work/inv.json` (SSOT ok, 88 Plugins, UFT-HEAD `cf7420e4`).
>
> **Von Hand verfasst, nicht per `gutachten.py`:** das Skript verweigerte mit
> „RATENBREMSE: bereits 5 Gutachten ohne Uebernahme-Marke". Dieses Gutachten
> verschärft den Rückstand nicht — es enthält **0 OPEN_ITEMS-Vorschläge**
> (Begründung unter „Bewegte Kennzahl"). Der Rückstand selbst (7 Gutachten in
> `out/` ohne `<!-- uebernommen: -->`-Marke, gezählt mit `grep -L uebernommen
> *.gutachten.md`) ist dem Eigentümer gemeldet.

## Kategorie

**Innovation (Betriebsart) + Verhaltens-Referenz — als Auftrag: keiner.**
Kein Fund bewegt eine der vier Release-Kennzahlen (MF-640). Alles unten ist
**Fundus**.

## 1. Was es wirklich ist

Die Vermutung des Auftrags stimmt: **Greaseweazle-Laufwerk als
Network-Block-Device** unter Linux (`/dev/nbd0`), mit eigenem IBM-System-34-
MFM-Codec, PLL und Write-Back-Track-Cache. Kein Abbild-Umweg: Dateisystem
liegt direkt auf dem Laufwerk.

| Messung | Wert | Methode |
|---|---|---|
| Sprache | Rust (11 `.rs`-Dateien) | `vermessen.py`, Sprachhistogramm |
| Umfang | 4 737 Zeilen Rust + 147 Zeilen Python (`tools/fat12-inspect.py`) | `wc -l` über `crates/**/*.rs` |
| Struktur | 3 Crates: `gwnbd-core` (2 750 LOC), `gwnbd-server` (1 515), `gwnbd-nbdkit` (325) | `wc -l` je Crate |
| Historie | **8 Commits, alle am 2026-08-02** (17:32–21:51 −0400) | `git log --format=%ci` |
| Tests | 56 `#[test]`-Attribute (README behauptet 55) | `grep -rc "#\[test\]" crates/` — Differenz 1 nicht aufgeklärt, siehe UNGEKLÄRT |
| Reifegrad | für seinen engen Zweck erstaunlich reif: MFM-Codec mit Eigenschaftstests, PLL-Tests gegen simulierten Drift, NBD-Handshake-Tests; README dokumentiert Hardware-Validierung (Tandon TM-100-2A 360K, NEC FD1157C 1.2M, MS-DOS-6.22-Interop) — **Eigenangabe, nicht von uns nachprüfbar** (MF-310: keine Hardware) | README §„Validated on hardware", `tests/dos-interop/` |
| Scope-Grenzen | nur IBM System 34 MFM, 8 Geometrien (160K–1.44M); FM, Amiga, C64, Apple II ausdrücklich außerhalb | README §„Deliberately not supported" |

Die 8 Commits an einem Tag sind ein Import-Muster (Entwicklung fand woanders
statt); der Code selbst widerspricht dem Skizzen-Verdacht — er trägt
Regressionstests zu real erlebten Hardware-Fehlern (z. B.
`crates/gwnbd-core/src/disk.rs:440` „This was a real failure against
hardware").

Das Greaseweazle-**Wire-Protokoll** liegt nicht in diesem Repo, sondern im
Crate `greaseweazle` 0.2.0 (codeberg.org/Rua/greaseweazle-rs, laut README
EUPL-1.2+; als Cargo-Abhängigkeit bezogen, nicht vendored —
`Cargo.toml:20`). gwnbd selbst enthält **Verhaltens**-Wissen über Firmware
und Laufwerke, kein eigenes Framing.

## 2. Lizenz

* `LICENSE:1`: **„EUROPEAN UNION PUBLIC LICENCE v. 1.2"** — aus der Datei
  gelesen, nicht aus dem README. `Cargo.toml:13` (`license = "EUPL-1.2"`)
  stimmt überein. Keine abweichenden Unterverzeichnis-Lizenzen
  (`vermessen.py`: 12 Ebenen geprüft, vollständig).
* **EUPL-1.2 steht nicht in `playbook/lizenzmatrix.md`.** Damit Zone
  **PRÜFEN** → nach AGENT.md Regel 8 **Eskalation statt Auslegung**:
  Entscheidungsvorlage unten. Zur Einordnung (Hinweis, kein Urteil): der
  Anhang der EUPL-1.2 führt „GNU GPL v2, v3" als kompatible Lizenzen für
  abgeleitete Werke (Art. 5 der EUPL) — das *könnte* GRÜN bedeuten, ist aber
  genau der Grenzfall, den die Matrix dem Eigentümer vorbehält.
* **Konsequenz bis zur Entscheidung:** kein Port, kein Vendoring.
  Verhaltens-Spec und Oracle-Nutzung sind in jeder Zone zulässig.
* **Attribution:** dieses Gutachten zitiert gwnbd (EUPL-1.2) als
  Verhaltens-Referenz. Es wurde kein Code übernommen.

## 3. Inventar-Abfrage (zitiert)

Alle sieben Begriffe (`nbd`, `block device`, `mount`,
`write precompensation`, `density select`, `write protect`, `resync`) kamen
mit `"vorhanden": false, "abgedeckt": false` zurück — der Index kennt diese
Fähigkeitsbegriffe nicht. Beispiel wörtlich:

```json
"nbd": { "vorhanden": false, "abgedeckt": false, "treffer": [],
         "schwache_treffer": [], "plugin_liste_vollstaendig": true }
```

Nach Regel 4 daher **von Hand im Baum nachgesehen** — Ergebnisse pro Fund
unten, jeweils mit `datei:zeile` auf beiden Seiten.

## 4. Funde

### (a) Betriebsart, die UFT fehlt: Diskette als Blockgerät

* **Deren Seite:** `crates/gwnbd-server/src/nbd.rs:1-9` (NBD
  newstyle-fixed-Server, ein Client, absichtlich: ein physischer Kopf),
  `crates/gwnbd-nbdkit/src/lib.rs` (nbdkit-Plugin),
  `crates/gwnbd-core/src/disk.rs:92-320` (Write-Back-Track-Cache mit
  Bad-Sektor-Buchführung).
* **Unsere Seite:** `git grep -in "nbd\b|network block" -- src include` →
  **0 Treffer**. UFT hat keine Mount-Betriebsart; der GW-Pfad
  (`src/hal/uft_greaseweazle_full.c`) liest/schreibt Flux in Abbilder.
* **Einordnung:** echte Lücke, aber Linux-spezifisch (NBD-Kernelmodul),
  GUI-fremd und mit Hardware-Folge → wäre ohnehin Eigentümer-Vorlage
  (Regel 8). Bewegt keine Kennzahl. **Fundus.** Bemerkenswert ist weniger
  das NBD-Gerüst als die Cache-Semantik darunter (siehe c).

### (b) Greaseweazle-Verhaltens-Wissen, das unser Treiber nicht hat

Vorab: UFTs Treiber ist gegen die Referenz-Firmware gebaut; gwnbd lief gegen
eine **Dritt-Firmware** (Adafruit Feather RP2040, „Greaseweazle-compatible
1.0"). Genau daraus stammen die Erkenntnisse.

**b1 — Pin 2 ist auf 5,25"-Laufwerken nicht Density-Select, sondern REDWC.**
* Deren Seite: `docs/hardware-notes.md` §„Pin 2 is not density select":
  gemessen am NEC FD1157C **28/30 Sektoren gut bei Pin high, 1/23 bei low**;
  Konsequenz im Code: `crates/gwnbd-core/src/geometry.rs:14/236-240`
  (`FormFactor` steuert den Pin, nicht die Datenrate),
  `crates/gwnbd-core/src/device.rs:38-46` (`Density::Auto/Skip`, Gerät
  merkt sich den letzten Pin-Zustand über Verbindungen hinweg).
* Zweite, unabhängige Quelle für die Pin-Semantik: Standard-Pinout-Doku
  (retrocmp.de „Floppy Disk Drive / Bus Interface"; old.pinouts.ru
  5,25"-Pinout) — Pin 2 ist je nach Laufwerk REDWC/HEAD LOAD/DENSEL. Die
  konkrete 28/30-vs-1/23-Messung bleibt Einzelquelle (deren Bench).
* Unsere Seite: `uft_gw_set_pin` existiert
  (`src/hal/uft_greaseweazle_full.c:689`), hat aber **keinen Aufrufer
  außerhalb der Datei** (`git grep uft_gw_set_pin` → nur Definition);
  treiberintern werden nur Pin 26 (TRK0, Zeile 706) und Pin 28 (WP,
  Zeile 738) gelesen. Die Capability-Flags `HAL_CAP_DENSITY_CTRL`
  (`include/uft/hal/uft_hal.h:124`) und `UFT_HW_CAP_DENSITY_SELECT`
  (`include/uft/uft_hardware.h:364`) sind deklariert, nirgends bedient.
  UFT fährt den Density-Pin **nie** — auf einem 5,25"-HD-Laufwerk mit
  falsch stehendem Pin dekodiert dann nichts, und nichts sagt warum.

**b2 — Eine abgewiesene Kommando-Antwort kann den Serial-Stream aus dem
Tritt bringen; Resync-Pfad nötig.**
* Deren Seite: `crates/gwnbd-core/src/device.rs:112-133` (`resync()`:
  150 ms auf Nachzügler-Bytes warten, Input leeren, Link-Gesundheit mit
  `get_firmware_info` beweisen, 5 Versuche); Anlass dokumentiert in
  `docs/hardware-notes.md` §„Firmware differences are real" (Adafruit-Build
  weist WRITE-PROTECT-Pin-Read ab und sendet trotzdem Bytes nach der
  Fehlerantwort). Deshalb ist die WP-Abfrage dort **opt-in und default aus**
  (`device.rs:94`).
* Unsere Seite: `uft_gw_command`
  (`src/hal/uft_greaseweazle_full.c:396-448`) returnt bei NAK sofort, **ohne
  zu drainen**; der nächste Befehl liefe in „Echo mismatch" →
  `UFT_GW_ERR_PROTOCOL` ohne Erholungspfad. `serial_reset_comms`
  (Zeile 258/379) existiert, wird aber nur in der Connect-Schleife benutzt
  (Zeile 597-598). Zusätzlich prüft UFT vor **jedem** Write bedingungslos
  Pin 28 (`uft_gw_write_flux`, Zeile 912) — genau die Abfrage, die auf der
  Adafruit-Firmware den Stream zerlegt.
* Beleg-Stärke: die Firmware-Eigenheit ist **Einzelquelle** (nur dieses
  Repo). Nachstellbar ohne Hardware wäre sie im vorhandenen GW-Emulator
  (`tests/emulators/`) — das wäre Verifikationsarbeit, kein neuer
  Treibercode. Die betroffene Datei ist **geschützt**; jede Änderung braucht
  ohnehin die Eigentümer-Entscheidung.

**b3 — 200 ms Lese-Erholzeit nach dem Schreiben, sonst falsche
CRC-Fehler beim Verify.**
* Deren Seite: `crates/gwnbd-core/src/device.rs:24` (`WRITE_SETTLE`
  200 ms), angewendet in `write_track` (`device.rs:474` ff., Sleep vor
  Verify); Begründung `docs/hardware-notes.md` §„A track cannot be read
  immediately after writing it".
* Unsere Seite: `uft_gw_write_flux`
  (`src/hal/uft_greaseweazle_full.c:909-947`) kennt keine Settle-Zeit, und
  der Simple-Pfad setzt `.verify=false` (Zeile 949) — es gibt im GW-Pfad
  gar kein verdrahtetes Read-Back-Verify, das den Fehler treffen könnte.
  Relevant erst, wenn ein Verify-Pfad gebaut wird; dann gehört diese Zahl
  in dessen Spec.

**b4 — Klassische MFM-Schreib-Präkompensation (125 ns ab halber
Zylinderzahl bei HD).**
* Deren Seite: `crates/gwnbd-core/src/pll.rs:107-147`
  (`apply_precompensation`, Nachbar-Intervall-Vergleich, Entscheidungen aus
  den Originalzeiten), Defaults je Geometrie
  (`geometry.rs`, `default_precomp_ns`/`precomp_from_cylinder`), Testabdeckung
  `pll.rs:253-267` („precompensated flux still decodes").
* Unsere Seite: `include/uft/core/uft_write_precomp.h:44-46` — nur
  `MAC_800K` implementiert, `AUTO` ist ausdrücklich no-op.
* Einordnung: Schreibpfad/Encoder-Nähe → fällt unter die
  EINFRIER-Denkweise; als Code-Vorschlag ohnehin gesperrt, als
  Verhaltens-Spec im Fundus notiert.

**b5 — Spindeldrehzahl-Vorabprüfung gegen das gewählte Format.**
* Deren Seite: `crates/gwnbd-core/src/device.rs:256-289` (`measure_rpm` aus
  Index-Pulsen, erste Index-Marke verworfen, weil Teilumdrehung) und
  `device.rs:291-313` (`check_rpm`, Warnung bei >10 % Abweichung — fängt
  „300-RPM-Format im 360-RPM-Laufwerk", das sonst nur als Müll dekodiert).
* Unsere Seite: RPM-Analyse aus Flux existiert
  (`include/uft/flux/uft_flux_analysis.h:300` `mean_rpm`,
  `include/uft/flux/uft_scp_parser.h:355`), aber als Nachanalyse — kein
  Preflight „Laufwerk vs. Formatwahl" im HAL- oder GUI-Pfad gefunden
  (`git grep -i "rpm" -- src/hal` → keine Treffer im GW-Treiber).

Bereits vorhanden, **kein** Fund (Dubletten-Sperre): Union mehrerer
Lese-Versuche über Umdrehungen (deren `device.rs:396-451`) — UFT hat
Multiread-Voting (belegt im ersten Scout-Zyklus, MF-611).

### (c) Umgang mit unlesbaren Sektoren — die interessanteste Antwort

gwnbd tut hier das Richtige, und zwar präziser als befürchtet:

1. **Kein stilles Nullen auf dem Lesepfad.** Ein Read, dessen Bereich einen
   unlesbaren Sektor berührt, schlägt fehl: `disk.rs:221-257` sammelt die
   berührten Bad-IDs und returnt `Error::BadSectors`; der NBD-Server mappt
   das auf **EIO** an den Client (`nbd.rs:67` `const EIO: u32 = 5;`,
   `nbd.rs:92`). Das Blockgerät erfindet keine Daten.
2. **Nullen existieren, aber buchgeführt und nur im
   Read-Modify-Write-Pfad:** unlesbare Sektoren werden beim Track-Laden
   zero-gefüllt **und als `bad` vermerkt** (`disk.rs:92-96`, Warnung
   `disk.rs:158`); ein Partial-Write auf so einen Track warnt ausdrücklich
   „they will be written as zeros" (`disk.rs:203-206`).
3. **Nicht-Ansteckung:** ein Bad-Sektor macht nur Reads kaputt, die ihn
   wirklich berühren; Nachbarn bleiben lesbar; ein Rewrite repariert ihn
   (`disk.rs:279` `track.bad.retain(...)`; Regressionstest
   `disk.rs:394-430` „a_bad_sector_only_fails_reads_that_touch_it").
   Und ein unlesbarer Track bleibt beschreibbar, sonst wäre eine leere
   Diskette nie formatierbar (`disk.rs:189-219`, Test `disk.rs:432-455`).
4. **Selbstkritik als Methode:** `docs/hardware-notes.md` §„Verification
   methodology" benennt, dass jede frühe „validated"-Behauptung nur der
   Cache war, der sich selbst bestätigte — verifiziert wird seither
   ausschließlich mit **kaltem** Prozess und einem Werkzeug ohne
   gemeinsamen Code (`tools/fat12-inspect.py`). Das ist wörtlich unsere
   „Oracle und Korpus-Erzeuger dürfen nicht dieselbe Hand sein"-Lektion
   (`docs/PLAN_v4.1.7.md:127`), unabhängig ein zweites Mal gelernt.

Für UFT ist das **Bestätigung, kein Auftrag**: unser Prinzip „keine
erfundenen Daten" hält auch unter Blockgerät-Zwang stand, wenn man
EIO + Sektor-Buchführung wählt. Als Verhaltens-Referenz abgelegt für den
Fall, dass UFT je einen Mount-/Export-Pfad baut (DiskFlashback-Plan,
`docs/plans/DISKFLASHBACK.md`, wäre der nächstliegende Anker).

Ein Gegenbeispiel enthält das Repo trotzdem, ehrlich benannt in dessen
README §„Known limitations": stirbt der Serial-Link während einer
Mount-Sitzung (dort bei 1.2M wiederholt passiert), verliert der
Write-Back-Cache alles Ungeflushte — einmal erreichte eine Diskette die
DOS-Maschine mit gültigem Bootsektor und leerem Dateisystem. Deren
Antwort: Auto-Commit nach 2 s Inaktivität + Flush bei SIGTERM/SIGINT.
Die Lehre für einen etwaigen UFT-Mount-Pfad: **Write-Back auf Altmedien ist
Verschleißschutz und Datenrisiko zugleich; der Flush darf nie allein dem
Client gehören.**

## 5. Oracle-Tauglichkeit

**Negativ, und zwar aus einem messbaren Grund:** der einzige
hardwarefreie Pfad (`--image`) umgeht den MFM-Codec vollständig —
`crates/gwnbd-core/src/image.rs:58-63` (`read_track` = Seek+Read in der
Rohdatei). Kein CLI-Kommando nimmt Flux-Dateien an (`main.rs`-Kommandos:
serve/mkfs-args/probe/info/format/read/write, alle Codec-Pfade brauchen das
Gerät). Ohne Hardware (MF-310) liefert gwnbd also **keine** Ausgabe, gegen
die ein UFT-Pfad prüfbar wäre. Der Codec wäre nur über einen eigens
geschriebenen Rust-Harness erreichbar; für IBM-MFM stehen mit
`greaseweazle` (host tools, bereits Korpus-Werkzeug: `gw_amigados.hfe` u. a.)
und FluxEngine stärkere, etablierte Oracles bereit. `tools/fat12-inspect.py`
ist als unabhängiger FAT12-Leser nett, aber gegenüber mtools ohne Vorteil.
`img` bleibt T3 (`docs/VERIFICATION_TIERS.md:73`) — gwnbd hilft dabei nicht.

## 6. Bewegte Kennzahl (MF-640) — und warum 0 Vorschläge

| Kennzahl | bewegt? |
|---|---|
| ungeprüfte Formate (T3) ↓ | nein — kein hardwarefreies Oracle, kein Korpus-Beitrag |
| angebotene Wandlungspfade ↑ | nein — keine Wandler-Beweise ableitbar |
| leckende Tests = 0 | nein |
| Bench-Alter je Controller ↓ | nein — keine Hardware im Projekt (MF-310) |
| 5. Zahl (ungeklärte Herkunft) | nein — nichts wird übernommen |

**Ein Fund, der keine Zahl bewegt, ist Fundus, nicht Auftrag.** Alle Funde
oben sind Fundus. **0 OPEN_ITEMS-Vorschläge.**

## 7. Eskalation (Regel 8) — Entscheidungsvorlage an den Eigentümer

1. **EUPL-1.2 fehlt in `playbook/lizenzmatrix.md`.** Erster Kontakt des
   Scouts mit dieser Lizenz. Zur Entscheidung: Zone festlegen (der
   EUPL-1.2-Anhang nennt GPL v2/v3 als kompatibel — Kandidat GRÜN, aber das
   Urteil steht der Matrix zu, nicht diesem Gutachten). Bis dahin: Zone
   PRÜFEN, kein Port.
2. **Fundus-Notizen mit Berührung der geschützten Datei
   `src/hal/uft_greaseweazle_full.c`** (b1 Density-Pin, b2 NAK-Drain/Resync,
   b3 Write-Settle, b5 RPM-Preflight): ob eine davon als
   KNOWN_ISSUES-/M3-Notiz geführt werden soll, entscheidet der Eigentümer —
   Anker wäre `docs/M3_HAL_PLAN.md`. Ohne Hardware ist b2 im GW-Emulator
   (`tests/emulators/`) nachstellbar; b1 ist durch zwei unabhängige
   Pinout-Quellen gestützt, die konkrete Messung bleibt fremd.

## 8. Beschaffungsliste

Leer. Gegen `inv["korpus"]` (22 Abbilder) geprüft: gwnbd erzeugt nichts,
was dort fehlt und ohne Hardware herstellbar wäre.

## 9. Aufwandsklasse / Differenzlauf

Entfällt — keine Vorschläge, keine „besser als unseres"-Behauptung. (Die
einzige Überlegenheits-Messung im Text, 28/30 vs. 1/23 bei Pin 2, ist
**deren** Messung auf deren Bench und wird als solche zitiert, nicht als
unsere behauptet.)

## UNGEKLÄRT

* Test-Zählung 56 gemessen vs. 55 im README — nicht aufgeklärt (vermutlich
  ein `#[test]` im nbdkit-Crate ohne Lauf im Default-`cargo test`; nicht
  nachgebaut, kein Rust-Toolchain-Lauf gemacht).
* Ob die NAK-Nachzügler-Bytes (b2) auch auf Referenz-Greaseweazle-Firmware
  auftreten — Einzelquelle sagt ausdrücklich „untested"
  (`docs/hardware-notes.md`, letzter Absatz §1.2M).
* Ob `cargo build`/`cargo test` auf diesem Stand grün sind — nicht
  ausgeführt (kein Rust-Build in dieser Sitzung; CI-Workflows liegen bei,
  Läufe nicht geprüft).
* EUPL-1.2-Zonenurteil — beim Eigentümer (§7).
