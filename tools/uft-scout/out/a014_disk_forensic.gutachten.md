# A-014 · Gutachten `SecurityRonin/disk-forensic` — ist das Diskettenarbeit?

**Stand:** 2026-09-16 · **Posten:** `A-014` · **Lizenz:** **Apache-2.0** · Sprache: Rust
**Prüfstand:** `origin/main` = `70a940af` · Klon: `tools/uft-scout/work/disk-forensic`

---

## Ergebnis in drei Sätzen

**Die Vorfrage ist mit Nein zu beantworten, und der eine Teil, den die
Aufnahme dieses Postens als tragend benannt hat, trägt bei näherem Hinsehen
auch nicht.** Die Aufnahme setzte darauf, dass das Repo den **E01-Aufbau**
zeigt — gemessen **implementiert es E01 überhaupt nicht**: es erkennt das
Format an der Kennung und reicht das Dekodieren an eine **externe Kiste**
weiter. Was bleibt, sind 2 146 Zeilen über Partitionsschemata und
Behälter-Beschnüffelung, und eine Diskette hat keine Partitionstabelle.

**Empfehlung: FUNDUS.** Kein Port (Apache-2.0), kein Orakel (kein `cargo`),
kein Spec-Gewinn für E01.

---

## 1. Die Vorfrage, ausdrücklich beantwortet (a)

| Gegenstand | im Repo | für ein **Disketten**werkzeug? |
|---|---|---|
| **E01 / EWF** | **nur Kennungserkennung** (§2) | wäre relevant — aber nicht von hier (§2) |
| **VMDK** | 15 Nennungen | **nein** — VM-Behälter |
| **VHDX** | 11 | **nein** — VM-Behälter |
| **VHD** | `vhd.rs`, 244 Z. | **nein** — VM-Behälter |
| **QCOW2** | **0 Treffer** | **nein** — und es ist gar nicht da (§3) |
| **DMG** | 13 | **nein** — macOS-Abbild |
| **ISO 9660** | 5 | **nein** — optisch |
| **GPT** | **48** | **nein** — eine Diskette hat keine GPT |
| **MBR** | **40** | **am Rand** — im Baum punktuell da (§5.1) |
| **APM** | **25** | **nein** — Apple Partition Map, Festplatten |

**Eine 3,5-Zoll-Diskette hat keine Partitionstabelle.** Die drei
Partitionsschemata machen mit 113 Nennungen den größten Einzelblock des
Repos aus und sind für dieses Werkzeug gegenstandslos.

**Das ist eine Einordnung, keine Abwertung des Repos** — es ist für
Festplatten- und VM-Forensik gebaut und tut dort offenbar sauber, was es
soll.

---

## 2. Der entscheidende Befund: E01 ist hier nicht drin (b)

Die Aufnahme dieses Postens sagte: *„Der eine Teil, der trägt, ist `E01` …
der Wert dieses Repos liegt darin, seinen Aufbau zu zeigen."*

**Gemessen: das Repo zeigt den Aufbau nicht.** Alle Nennungen von
`E01`/`EWF`/`EnCase` über 2 146 Zeilen, vollständig:

| Stelle | was dort steht |
|---|---|
| `container.rs:14` | `use forensicnomicon::{aff4, dmg, ewf, qcow2, vhd, vhdx, vmdk};` |
| `container.rs:287-288` | `ContainerFormat::Ewf` — ein Aufzählungswert |
| `container.rs:324-328` | `header.starts_with(&ewf::EVF1_SIGNATURE)` … `EVF2` … `LEF2` → **Kennungsvergleich** |
| `container.rs:122-125` | **`::ewf::EwfReader::open(path)`** |
| Rest | Kommentare und Testpfadnamen (`evidence.E01`) |

Und der Quelltext sagt es selbst, an `container.rs:123-124`:

> *„`ewf` (imported) is forensicnomicon's magic module; the decoder is the
> external `ewf` crate, reached via the absolute path."*

**Es gibt hier keine Blockgröße, keine CRC-Stelle, keinen
Fehlerbereichs-Satz, keine Fallmetadaten** — nichts von dem, was die
Abschlussbedingung (b) aus dem Code lesen wollte. Der Aufbau liegt in der
`ewf`-Kiste und ursprünglich in **libewf** / der ASR-Dokumentation.

**Die Aufnahme hatte das zur Hälfte vorweggenommen** („mit dem Hinweis,
dass die kanonische Quelle libewf/ASR-Dokumentation ist und nicht dieses
Repo"). Sie nahm nur an, das Repo zeige den Aufbau trotzdem. Es tut es
nicht.

---

## 3. Eine Zahl der Aufnahme fällt

Die Aufnahme zählte **acht** Behälter auf, darunter **QCOW2**. Gemessen im
Quelltext: **`QCOW` hat 0 Treffer** — der Name steht nur im `use`-Ausdruck
der Fremdkiste (`forensicnomicon::qcow2`) und in einem Kommentar der
Programmhilfe, nicht in einer eigenen Behandlung.

Das ändert am Urteil nichts (QCOW2 ist ohnehin kein Diskettenformat), aber
es gehört richtiggestellt: **das Repo behandelt die Behälter nicht, es
erkennt sie und delegiert.** Das ist auch bei VMDK, VHDX und DMG so — die
einzige eigene Umsetzung ist `vhd.rs` (244 Zeilen).

---

## 4. Die Brücke zu `P3-387` — begründet verworfen (c)

`P3-387` benennt zwei fehlende Fähigkeiten: *„ein SHA-256 je (Zylinder,
Kopf) statt einer Summe über die ganze Datei"* und
*„`uft_format_mark_last_missing()` kennzeichnet im SPEICHER, und keine
Datei trägt es hinaus."*

**Die Brücke, die die Aufnahme gesehen hat, ist richtig gedacht und führt
über E01** — das Format trägt eine CRC je Block und einen Bereichsvermerk
für fehlerhafte Sektoren, also genau die zwei Fähigkeiten.

**Sie führt nur nicht über dieses Repo** (§2). Wer sie bauen will, braucht
libewf oder die ASR-Spezifikation; `disk-forensic` trägt dazu nichts bei,
was nicht auch der Name des Formats verraten würde.

**Verworfen, nicht hergestellt** — und der Grund ist gemessen, nicht
gemeint.

---

## 5. Die fünf Fragen (d)

### 5.1 „wo können die formate verbessert werden"

Nichts aus diesem Repo. Die einzige Berührung ist **MBR**, und dort ist der
Baum punktuell versorgt: `include/uft/formats/uft_fat32_mbr.h` und
`src/formats/fat32/uft_fat32_mbr.c` — MBR als FAT32-Anhang, nicht als
eigene Achse. Für Disketten genügt das, weil es dort keine gibt.

### 5.2 „ist es auf andere formate übertragbar"

**Die Behälter-Beschnüffelung ja, dem Prinzip nach.** `container.rs`
erkennt am **Inhalt**, nicht an der Endung, und unterscheidet den seltenen
Fall eines *komprimierten Behälters* (`evidence.E01.gz`) vom komprimierten
Inhalt. Das ist eine saubere Trennung, die der Baum an anderer Stelle
gebrauchen könnte — aber sie ist gängige Praxis, kein Fund.

### 5.3 „welche einstellungen fehlen noch"

Keine, die dieses Repo beantwortet.

### 5.4 „was habe wir noch nicht"

**E01 als Schreibziel** — und das steht bereits als `P3-387`, ohne dieses
Repo.

### 5.5 „brauch es eine HAL-Erweiterungen"

**Nein.**

---

## 6. Orakel: verschlossen und delegierbar (e)

`cargo` und `rustc` sind auf dieser Maschine nicht vorhanden (bei der
Aufnahme gemessen). `docs/ORACLES.md` verlangt *„Kein Oracle auf
Zusicherung — ein Werkzeug, das nicht gebaut und ausgeführt wurde, ist kein
Eintrag."* Ein Eintrag ist damit hausintern **unmöglich**.

**Was delegiert werden müsste, falls der Eigentümer es je will:** Bau mit
einer Rust-Werkzeugkette, Ziel `disk4n6` (422 Zeilen, `src/bin/`),
Versionsabfrage für `docs/ORACLES.md`, und ein Differenzlauf gegen UFT.

**Aber es lohnt nicht**, und das ist die ehrlichere Aussage: der Gegenstand
ist nicht Diskette (§1), und der eine Teil, der es wäre, ist hier nicht
umgesetzt (§2). Ein Orakel ohne gemeinsame Gegenstände vergleicht nichts.

---

## 7. Disposition

| Teil | Urteil | Grund |
|---|---|---|
| `container.rs` (443 Z.) | **FUNDUS** | Behältererkennung; Prinzip bekannt, Lizenz sperrt den Port |
| `vhd.rs` (244 Z.) | **uninteressant** | VM-Behälter, kein Diskettenformat |
| GPT/MBR/APM (113 Nennungen) | **uninteressant** | eine Diskette hat keine Partitionstabelle |
| E01/EWF | **nicht vorhanden** | nur Kennung + Delegation (§2) |
| `bin/disk4n6.rs` (422 Z.) | **Orakel-Kandidat, verschlossen** | kein `cargo`; und es lohnt nicht (§6) |

**Kein Byte dieses Repos ist nach `src/`, `include/` oder `tests/`
geschrieben worden.** Der Klon liegt unter `tools/uft-scout/work/`.

---

## 8. Vorschläge für `docs/OPEN_ITEMS.md`

**Einer**, und er schreibt einen bestehenden Punkt fort statt einen neuen
anzulegen:

1. **`P3-387` bekommt seine Quellenangabe, und sie ist NICHT dieses Repo.**
   Wer den stückweisen Hash und das überlebende Fehlerprotokoll bauen will,
   findet das Vorbild in **E01** (CRC je Block, Bereichsvermerk für
   fehlerhafte Sektoren) — die kanonische Beschreibung liegt bei **libewf**
   bzw. der **ASR-Dokumentation**. `SecurityRonin/disk-forensic` erkennt
   E01 nur an der Kennung und delegiert an die externe `ewf`-Kiste
   (`container.rs:122-125`, gemessen über alle 2 146 Zeilen).
   *Kennzahl:* keine der vier.

---

## 9. Quellen

* `https://github.com/SecurityRonin/disk-forensic`, flach geklont
  2026-09-16 nach `tools/uft-scout/work/disk-forensic`
* **Apache License 2.0**; Rust, **30** `.rs`-Dateien, **2 146** Zeilen in
  `src/` (8 Dateien)
* Baum: `origin/main` = `70a940af`
* Gegengehalten: `P3-387` (stückweiser Hash, überlebendes
  Fehlerprotokoll), `P3-316`/`P3-353` (Apache-Präzedenz), `MF-310`
  (delegierbar statt hausintern), `docs/ORACLES.md` (kein Oracle auf
  Zusicherung)
