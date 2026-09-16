# A-022 · Gutachten `Booyaka101/diskstack` — mehrere Abzüge EINER Diskette zusammenstimmen

**Stand:** 2026-09-16 · **Posten:** `A-022` · **Lizenz:** **MIT** · Sprache: Python
**Prüfstand:** `origin/main` = `70a940af` · Klon: `tools/uft-scout/work/diskstack`

---

## Ergebnis in drei Sätzen

**Der Fund, den die Aufnahme aus der Projektbeschreibung vorhergesagt hat,
ist gemessen bestätigt — und er trifft nicht die Formatschicht, sondern die
Berichtsschicht.** `diskstack` schreibt einen Verlustbericht, der den
**nächsten Befehl nennt** (`gw read --tracks=…` für genau die Spuren, die
noch fehlen); UFTs `src/core/uft_loss_report.c` sind **123 Zeilen**, die
JSON schreiben und keinen nächsten Schritt kennen. **Und der Orakel-Kanal
ist zum ersten Mal in dieser Reihe fast offen** — zwei von drei
Abhängigkeiten liegen vor.

**Empfehlung: REGISTER als Oracle-Kandidat, ein Befund für die
Berichtsschicht, kein Port.**

---

## 1. Lizenzurteil und Kanal

Gemessen: **MIT** (`LICENSE`), Python, **85 Dateien, 13 036 Zeilen**,
letzter Push 2026-09-12, nicht archiviert.

| Kanal | Urteil |
|---|---|
| **Port** | rechtlich offen (MIT in GPL-2-or-later), **praktisch nicht** — 13 036 Zeilen Python nach C/Qt sind eine **Neuschreibung**, kein Port. Dieselbe Lage wie `mkmgt` (Forth, A-015). |
| **Oracle** | **der stärkere Kanal, und er ist fast offen** — §2 |
| **Spec** | offen; die Module sind lesbar und dokumentiert |

---

## 2. Die Orakel-Frage, beantwortet statt offen gelassen

`docs/ORACLES.md` verlangt: *„Kein Oracle auf Zusicherung — ein Werkzeug,
das nicht gebaut und ausgeführt wurde, ist kein Eintrag."*

**Gemessen, je Abhängigkeit aus `pyproject.toml`:**

| Paket | Zustand |
|---|---|
| `click>=8.1` | **vorhanden** |
| `crcmod>=1.7` | **vorhanden** |
| `bitarray>=2.9` | **fehlt** |

**Das ist nach fünf verschlossenen Werkzeugketten in Folge der erste
Kandidat, bei dem der Kanal in Reichweite liegt** — Swift (A-013), Rust
(A-014), Forth (A-015), `mkfs.fat`/`mtools` (A-005) und die Borland-Kette
(A-016) waren alle vollständig abwesend; hier fehlt **ein** Paket.

**Nicht installiert**, und das ist Absicht: eine Paketinstallation verändert
die Python-Umgebung des Eigentümers. Das ist eine Entscheidung, kein
Nebeneffekt — und sie ist klein und umkehrbar (`pip install bitarray`).

**Was danach ein Eintrag wäre:** ein Lauf mit Versionsangabe, wie ihn
`docs/ORACLES.md` für jedes Orakel verlangt, plus der Differenzlauf aus §5.

---

## 3. Inventar-Abfrage: was UFT hat und was nicht

| `diskstack`-Modul | Entsprechung im Baum |
|---|---|
| `stack.py` (sektorweise Abstimmung mehrerer Abzüge) | **`src/recovery/uft_multiread_pipeline.c`** (MF-473) — **gibt es** |
| `candidates.py` (Kandidaten je Sektor) | `uft_multiread_pipeline` führt Kandidaten — **gibt es** |
| `formats.py` (`.scp`, KryoFlux `.raw`, `.hfe`, Sektorabbilder) | alle vier **gibt es** als Plugins |
| `cache.py`, `parallel.py` | Ablaufsteuerung — für UFT belanglos |
| `filler.py` | Füllwerte — berührt `P3-422` |
| **`report.py` (Verlustbericht mit nächstem Befehl)** | **GIBT ES NICHT** — §4 |

**Die Abstimmung selbst ist also kein Zugewinn.** UFT tut das seit MF-473,
und `P3-88` hat dort gemessen, dass Multi-Read-Voting einen Sektor
*erfinden* konnte — ein Befund, den dieser Baum bereits behoben hat.

---

## 4. Der Fund: ein Bericht, der den nächsten Befehl nennt

**Was `diskstack` tut** (gemessen im Quelltext, nicht aus der Beschreibung
zitiert):

| Stelle | Inhalt |
|---|---|
| `diskstack/report.py:99` | *„Greaseweazle `--tracks=` specs covering every unresolved sector."* |
| `diskstack/report.py:119` | *„Full `gw read` command lines for the tracks still missing data."* |
| `diskstack/report.py:76` | `f'{progress.still_bad} sectors are still bad. Another pass at …'` |
| `diskstack/cli.py:235-236` | *„diskstack prints a gw read command naming only the tracks that are still bad, so the next pass over the disk is seconds of drive time"* |

**Was UFT tut, gemessen:**

* `src/core/uft_loss_report.c` — **123 Zeilen**, fünf Funktionen:
  `uft_loss_report_schema_version`, `uft_loss_category_string`,
  `write_json_string`, `uft_loss_report_write_stream`,
  `uft_loss_report_write`. Es **schreibt JSON**. Kein nächster Schritt,
  keine Spurliste, kein Befehl.
* `--tracks=` kommt im ganzen Baum **nur** in
  `src/hardware_providers/fluxengine_provider_v2.cpp` vor — und dort als
  **Argumentbau für einen Lesevorgang** (`args.push_back("--tracks=c"…)`),
  nicht als Vorschlag an einen Bediener.

**Der Unterschied ist nicht technisch, sondern gedanklich:** UFTs Bericht
sagt, **was verloren ist**. `diskstack`s Bericht sagt, **was als Nächstes zu
tun ist**, und begründet es mit der Ersparnis — ein zweiter Lauf über drei
Spuren statt über achtzig.

**Und das ist genau die Gestalt, die `P3-387` sucht:** *„ein
Fehlerprotokoll, das den Lauf überlebt."* Ein Protokoll, das den nächsten
Befehl trägt, überlebt den Lauf **notwendigerweise** — es ist für den
nächsten gemacht.

---

## 5. Differenzlauf-Plan

**Voraussetzung:** `pip install bitarray` (§2).

1. **Gegenstand:** zwei oder mehr Abzüge derselben Diskette. Der Baum hat
   dafür Material — der Mehrfachlese-Pfad ist seit MF-473 getestet.
2. **Lauf A:** `diskstack` über die Abzüge, Ausgabe des Verlustberichts.
3. **Lauf B:** UFTs `uft_multiread_pipeline` über dieselben Abzüge.
4. **Verglichen wird nicht das Ergebnis, sondern die MENGE der ungelösten
   Sektoren.** Stimmen beide überein, ist die Abstimmung bestätigt; weichen
   sie ab, ist die Abweichung der Befund — und sie ist je Sektor benennbar.
5. **Die Gegenprobe gehört dazu:** ein Abzug, in dem ein Sektor absichtlich
   in zwei Fassungen widersprüchlich vorliegt. `P3-88` hat gemessen, dass
   UFT dort einmal einen Sektor **erfunden** hat; ein zweites Werkzeug auf
   demselben Fall ist die schärfste Probe, die dieser Baum für den Pfad
   hat.

---

## 6. Die HAL-Frage, beantwortet

**Nein — und die Begründung ist die interessantere Hälfte.**

`diskstack` liest **Dateien** (`.scp`, KryoFlux `.raw`, `.hfe`,
Sektorabbilder) und fasst Hardware nie an. Es **druckt** einen
`gw read`-Befehl, statt ihn auszuführen — die Steuerung bleibt beim
Bediener.

**Genau diese Trennung ist auf UFT übertragbar, ohne die HAL zu berühren:**
der Bericht muss nicht lesen können, um zu sagen, *was* zu lesen wäre. Er
braucht nur die Liste der ungelösten Spuren — und die hat
`uft_multiread_pipeline` bereits.

---

## 7. Disposition

| Teil | Urteil | Kennzahl | Kanal |
|---|---|---|---|
| `report.py` (Verlustbericht mit nächstem Befehl) | **REGISTER als Befund** | die fünfte, offene Zahl (MF-640); `P3-387` | *Spec* — der Gedanke, nicht der Code |
| `stack.py`, `candidates.py` | **PARTIAL** | — | *Oracle* für den Differenzlauf (§5) |
| `formats.py` | **uninteressant** | — | UFT liest alle vier bereits |
| `cache.py`, `parallel.py`, `cli.py` | **uninteressant** | — | Ablaufsteuerung, Python-spezifisch |
| Port des Ganzen | **verworfen** | — | 13 036 Zeilen Python = Neuschreibung |

**Kein Byte dieses Repos ist nach `src/`, `include/` oder `tests/`
geschrieben worden.** Der Klon liegt unter `tools/uft-scout/work/`.

---

## 8. Vorschlag für `docs/OPEN_ITEMS.md`

**Einer**, und er schreibt `P3-387` fort:

1. **Der Verlustbericht sagt, was fehlt — nicht, was als Nächstes zu tun
   ist.** `src/core/uft_loss_report.c` sind 123 Zeilen, die JSON schreiben;
   `--tracks=` steht im Baum nur im FluxEngine-Provider, als Argumentbau.
   `diskstack` (MIT) zeigt die Gegenform: ein Bericht, der
   `gw read --tracks=…` für genau die ungelösten Spuren nennt, „so the next
   pass over the disk is seconds of drive time". **Das ist die Gestalt, die
   `P3-387` sucht** — ein Protokoll, das den nächsten Lauf bedient,
   überlebt ihn notwendigerweise. Die Liste der ungelösten Spuren liegt in
   `uft_multiread_pipeline` bereits vor; die HAL ist **nicht** berührt
   (§6). *Kennzahl:* keine der vier; die fünfte, offene (MF-640).

---

## 9. Quellen

* `https://github.com/Booyaka101/diskstack`, flach geklont 2026-09-16 nach
  `tools/uft-scout/work/diskstack`
* **MIT License**; Python, **85** `.py`-Dateien, **13 036** Zeilen; letzter
  Push 2026-09-12
* Abhängigkeiten gemessen: `click` vorhanden, `crcmod` vorhanden,
  `bitarray` **fehlt**
* Baum: `origin/main` = `70a940af`; `src/core/uft_loss_report.c` (123 Z.),
  `src/recovery/uft_multiread_pipeline.c`,
  `src/hardware_providers/fluxengine_provider_v2.cpp`
* Gegengehalten: `P3-387`, `P3-88` (Multi-Read-Voting konnte einen Sektor
  erfinden), `MF-473`, `MF-640` (die fünfte, offene Zahl)
