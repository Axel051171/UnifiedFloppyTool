# Verzeichnis der Referenz-Werkzeuge (Oracles)

> Was `CAPABILITIES.md` für Controller ist, ist diese Datei für die
> Werkzeuge, die **urteilen**.

Ein Oracle entscheidet eine Behauptung, die UFT über sich selbst nicht
entscheiden kann. Die Einfrier-Regel (MF-498) verlangt für neuen Code im
Format- oder Decoder-Layer eine **benannte** Referenz — hier stehen die
Namen.

**Die Registry ist die SSOT, nicht diese Datei.**
`tests/differential/oracles.py` hält Auflösung, Version und Lizenz
maschinenlesbar; hier steht, was ein Mensch darüber wissen muss. Weichen
beide ab, gilt die Registry, und diese Datei ist gedriftet.

## Was ein Eintrag braucht

Vier Fragen, alle vier beantwortet oder kein Eintrag:

1. **Wofür ist es Referenz?** Nicht „was kann es", sondern welche
   Behauptung es entscheidet.
2. **Wie wird es gefunden?** Umgebungsvariable, dann `PATH`. Kein Raten
   in Installationsverzeichnissen.
3. **Wie wird die Herkunft gepinnt?** Versionsabfrage — oder, wenn das
   Werkzeug keine hat, `version_is_unaskable` plus SHA-256 der Datei
   (MF-623). Der Hash ist der **stärkere** Anker: eine Version gibt es in
   vielen Übersetzungen, den Hash einmal.
4. **Was ist mit der Lizenz?** Verglichen wird ausschließlich die
   **Ausgabe** eines fremden Programms. Es wandert kein Code ein — das
   ist der Unterschied, der die Lizenzfrage bei Oracles entschärft, und
   er steht bei jedem Eintrag ausdrücklich da.

**Kein Oracle auf Zusicherung.** Ein Werkzeug, das nicht gebaut und
ausgeführt wurde, ist kein Eintrag. Der Selbsttest
(`python tests/differential/oracles.py`) sagt, welche vorhanden sind —
und meldet **nicht** Erfolg, wenn es keines ist.

### Fünfte Frage: dieselbe Hand? (MF-644)

> **Wer den Korpus erzeugt hat, darf ihn nicht allein prüfen.**

Ein Oracle, das dasselbe Werkzeug ist wie der Korpus-Erzeuger, prüft
seine eigene Selbstkonsistenz — nicht UFT. Der T1b-Eintrag wird damit
**zirkulär**, und das fällt niemandem auf, weil alles grün ist.

Der Fall ist zweimal gemessen worden:

* **ADF:** `unadf` und AdfOpus teilen sich **ADFlib** — dieselbe
  Bibliothek, die über `xdftool` auch unser Korpus-Abbild erzeugt hat.
  AdfOpus schied deshalb aus, **nicht** weil es schlecht wäre
  (`PLAN_v4.1.7.md:127-133`).
* **ATR:** `atrcopy 10.1` hat `atrcopy_dos2sd.atr` **erzeugt**
  (Manifest wörtlich). Als alleiniges Oracle wäre der Eintrag zirkulär.

**Aufgelöst wird es durch eine zweite, unabhängige Hand** — nicht durch
eine zweite Fassung derselben Bibliothek. Für ATR sind es zwei:

| zweite Hand | Lizenz | Unabhängigkeit belegt durch |
|---|---|---|
| `lsatr` (dmsc/mkatr) | GPL-2.0 | eigene C-Codebasis, eigener DOS-2- und SpartaDOS-Leser, keine atrcopy-Verwandtschaft |
| `a8rawconv` | GPL-2.0-or-later | anderer Autor, anderer Ansatz (Flux-Ebene); `ATR→XFD` byteidentisch zum Korpus-XFD |

**Registrierungsregel:** jeder neue Eintrag nennt, **wer den Korpus
erzeugt hat, gegen den er prüfen soll**. Ist es dasselbe Werkzeug, wird
er nur zusammen mit einer zweiten Hand eingetragen — oder gar nicht.

## Der Differenzlauf-Standard (seit MF-629)

Verglichen wird **Inhalt, byteweise nachgerechnet**, nicht die
Verzeichnisdarstellung:

| Stufe | Form | Beispiel |
|---|---|---|
| schwach | Verzeichnis listen | `floptool flopdir d64 cbmdos <datei>` |
| **stark** | **Hash je Datei** | `floptool flophashes d64 cbmdos <datei>` |
| stärker | Bytes herausholen und selbst hashen | `floptool flopread … <pfad> <ziel>` |

Gemessen am Korpus-D64: `UFT MARKER`, 254 Byte,
sha1 `56fea729e9e37c473b12c3b76fc0d3e387b39b5a`.

Wo ein Werkzeug Spur- statt Dateiebene beurteilt (`nibscan`s BAM-/DIR-
und Full-CRC), ergänzt das die Datei-Ebene, es ersetzt sie nicht.

## Registrierte Oracles (6)

Stand `tests/differential/oracles.py`, 2026-08-28.

| Kurzname | Variable | Lizenz | Herkunfts-Anker | entscheidet |
|---|---|---|---|---|
| `gw` | `GW` | Unlicense | `--version` | Flux-Aufnahme und -Wandlung am Greaseweazle; Bezug für die gw-vs-UFT-Differenztests (P3.2) |
| `cpmls` | `CPMLS` | GPL-3.0 | `-h` | CP/M-Verzeichnislesung gegen eine `diskdefs`-Definition. Liest `cpmls` ein Abbild und UFT nicht gleich, liegt es an UFT |
| `hxcfe` | `HXCFE` | GPL-2.0 | `-help` | Format-Wandlung über viele Container (HFE, IMG, DSK, …); Bezug für T1b-Eingaben |
| `samdisk` | `SAMDISK` | MIT | `--version` | Container-Formate und ihre Randfälle. Die **Quelle** liegt zusätzlich im Baum (`src/samdisk/`) und dient als Spec-Referenz |
| `dtc` | `DTC` | proprietär, nur Ausführung | `-h` | KryoFlux-Rohstrom-Aufnahme; Bezug für den KryoFlux-Lesepfad |
| `floptool` | `FLOPTOOL` | GPL-2.0-or-later (MAME) | **SHA-256** (keine Versionsabfrage) | Verzeichnis **und Hashes** bei ausdrücklich genanntem Container + Dateisystem |

### floptool — der einzige, der auf dieser Maschine liegt

Beschafft aus `mame0289b`; die SHA-256 der Distribution wurde gegen die
offizielle `SHA256SUMS` geprüft. Werkzeug-Hash `6973d1b5…20ac21`.

**Gemessen am freien Korpus (MF-623/629):**

* echte Auflistung: `.d64`, `.d71`, `.g64` — dazu `pc_fat` vollständig
  (gegen ein selbstgebautes 720K-FAT12 geprüft)
* **Fallstrick:** floptool prüft den **Container**, nicht das
  **Dateisystem**. `flopdir adf cbmdos` auf einem AmigaDOS-Abbild endet
  mit `rc=0`, leerem Volumenamen und leerer Liste. Eine leere Auflistung
  ist deshalb **„kein Ergebnis"**, nie „leere Diskette" — sonst
  bestätigt das Oracle einen Lesefehler von UFT, statt ihn aufzudecken.
* Zufallsbytes und ein falscher Container fliegen dagegen laut heraus.
* `.d80` und `.d82` **hängen** (>9 min, abgebrochen). Jeder Aufruf
  braucht ein Zeitlimit.
* Gegen die fünf Phase-1-Ziele: **1 von 5** (nur D64). Für ADF und ATR
  bringt floptool kein Dateisystem mit.

## Vorgemerkt, noch nicht registriert

Diese Werkzeuge sind gemessen oder gebaut, haben aber **keinen**
Registry-Eintrag. Sie zählen deshalb für kein T1b-Manifest.

| Werkzeug | Stand | offen |
|---|---|---|
| `nibconv`, `nibscan` (nibtools) | vom Scout **gebaut**, hardwarefrei mit MinGW ohne OpenCBM, auf dem Korpus gelaufen; liefert BAM-/DIR- und Full-CRC über dekodierte Spuren | Registry-Eintrag (`version_is_unaskable` + SHA-256, `VERSION` ist nur ein Build-Datum) — `SCOUT-20` |
| `dskx` (FloppyControl) | Quelle gelesen, CLI belegt (`list`/`extract`/`--deleted`) | **nicht gebaut, nicht gelaufen** — vor jedem Eintrag bauen. Eng auf gelöschte FAT12-Einträge und Bad-Cluster zu schneiden, weil floptool den normalen Inhalt bereits liest — `SCOUT-F1` |
| `lsatr` (dmsc/mkatr) | GPL-2.0, unabhängige C-Codebasis; liest DOS 1/2.0/2.5/MyDOS/SpartaDOS/BW-DOS | bauen (`make`), dann Eintrag. **Zweite Hand für ATR** — löst die Zirkularität gegen atrcopy |
| `a8rawconv` | im Baum vendort (`src/a8rawconv/`), heute gebaut; `ATR→XFD` byteidentisch zum Korpus-XFD | Eintrag ausstehend (`SCOUT-33`). Zugleich die In-Tree-Referenz für den FM-Pfad |
| `atrcopy` | erzeugt unseren ATR-Korpus; `crc`-Unterbefehl liefert CRC32 je Datei über den Inhalt | **Auflage:** braucht `numpy<1.23` — unter NumPy 2.5.1 drei gemessene Abstürze bei jedem Abbild-Open. Nur zusammen mit `lsatr` eintragen (Zirkularität) |
| **fdc_bitstream** (yas-sim) | **extern**, nicht im Baum — die vendorte Kopie ist mit MF-626 gelöscht | als **Upstream-Oracle** bauen und eintragen, **nicht** zurückholen (siehe unten) |

### Warum fdc_bitstream extern bleibt (MF-644)

Der Wunsch dahinter ist richtig: ein **zweiter, unabhängiger
MFM-Decoder als Schiedsrichter** ist genau, was Tier-Hebung und
Rettungskette später brauchen.

Der Weg dorthin ist aber nicht `git revert` auf MF-626. Ein
zurückgeholtes Vendoring bringt 2795 Zeilen in den Baum, die

* die Verwaisten-Regel bei **jedem** Durchgang erneut anfassen muss,
* eine Baulast in beiden Build-Systemen tragen,
* und einen Anker brauchen, der nur existiert, um sie zu rechtfertigen.

Als **externes Oracle** entfällt all das: Upstream im Prüfstand bauen,
hier registrieren, fertig. Kein Code im Baum, keine Lizenzfrage, keine
Baulast — und derselbe Schiedsrichter.

Der Anker gehört deshalb **hierher**, nicht in einen Plan: ein Oracle
ist kein Baustein, den man später verdrahtet, sondern ein Werkzeug, das
urteilt.

## Was ausdrücklich **kein** Oracle ist

| Werkzeug | Grund |
|---|---|
| `unadf`, AdfOpus | teilen sich **ADFlib** — dieselbe Hand wie `xdftool`-erzeugte Korpus-Abbilder, also keine Zweitmeinung |
| `amigadx` | vendorte ADFlib 0.7.10 **und** Total-Commander-Plugin ohne Konsolen-Einstieg |
| `ADFDiskBox` | ruft nur `cmd.exe /C gw …`; **keine** eigene ADF-Ebene (0 Treffer auf `ReadAllBytes\|FileStream\|adflib\|RootBlock`) |
| `FloppyControl` selbst | WinForms-GUI, nicht skriptbar. Nur sein `dskx` ist ein Konsolenprogramm |
| `WinUAE` | ADFlib-unabhängig und damit inhaltlich interessant, aber GUI **und** ohne Lizenzdatei im Repo — Referenz ja, Oracle nein |

**Offene Lücke:** eine **ADFlib-unabhängige, skriptbare** Zweitmeinung
für ADF fehlt weiterhin. Drei Zyklen haben sie gesucht und nicht
gefunden. Phase 1 Nr. 2 (AmigaDOS) hängt daran.

## Pflege

* Der **Scout** trägt neue Kandidaten hier ein — er baut sie ohnehin —
  und nennt dabei Lizenz, Bau-Rezept und was das Werkzeug entscheidet.
* Der **MF-Workflow** trägt den Registry-Eintrag nach, sobald das
  Werkzeug gebaut und gelaufen ist.
* `tests/differential/test_oracles.py` prüft die Registry auf sich
  selbst (14 Prüfungen, läuft in ctest als `oracle_registry`). Fehlt ein
  Werkzeug auf der Maschine, überspringen sich die Tests, die es
  brauchen — sauber und sichtbar, nie stillschweigend grün.
