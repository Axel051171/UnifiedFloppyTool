# A-010 · Gutachten `FloImg-extrakt.zip` — Ganzdurchläufe und Mediendiagnosen

**Stand:** 2026-09-16 · **Posten:** `A-010` · **Kanal:** *Spec* · **Lizenz:** GPL-2.0-or-later
**Prüfstand:** `origin/main` = `70a940af`

---

## Ergebnis in drei Sätzen

**Dies ist die erste der drei heute begutachteten Zulieferungen, deren
Überlappung mit lebendem Code eine echte Weiterentwicklung ist und kein
älterer Stand.** Sie erweitert `uft_os_volume` ABI-sicher um den Begriff
**Ganzdurchlauf** — und trifft damit genau `P3-284`, wo ein Schalter namens
`adaptive_passes` seit jeher gesetzt und **von niemandem gelesen** wird.
Zwei Annahmen der Aufnahme dieses Postens fallen bei der Messung: es gibt
**keinen CLI-Konflikt**, und die Lizenz ist **nicht offen**.

**Empfehlung: REGISTER + PARTIAL.** Der Begriff und die Diagnose sind
übernehmenswert; der Weg dorthin ist ein Zeilendiff, kein Dateitausch.

---

## 1. Jede öffentliche Funktion gegen den Baum (a)

Gemessen je Bezeichner mit `git grep`, **`rc` geprüft** (ein scheiterndes
`git grep` liefert leere Ausgabe, die wie „0 Treffer" aussieht — dieser
Fehler ist mir heute dreimal passiert und wird hier nicht wiederholt).

| Bezeichner | rc | Treffer | Urteil |
|---|---|---|---|
| `uft_osvol_open` | 0 | 9 | **gibt es** — `include/uft/hal/uft_os_volume.h`, `src/hal/uft_os_volume.c`, `tests/test_roland_osvol.c` |
| `uft_osvol_close` | 0 | 2 | **gibt es** |
| `uft_osvol_read` | 0 | 3 | **gibt es** — dazu `src/hal/uft_hal_profiles.c` |
| `uft_osvol_write` | 0 | 2 | **gibt es** |
| `uft_osvol_query_geometry` | 0 | 2 | **gibt es** |
| `uft_osvol_lba_to_chs` | 0 | 7 | **gibt es** |
| `uft_osvol_total_sectors` | 0 | 8 | **gibt es** |
| `uft_osvol_image_size` | 0 | 8 | **gibt es anders** — steht im Header und im Test, **nicht** in `src/hal/uft_os_volume.c` |
| `uft_osvol_status_summary` | 0 | 3 | **gibt es** |
| `uft_osvol_t` | 0 | 14 | **gibt es** |
| `uft_mdiag_diagnose` | **1** | 0 | **gibt es nicht** |
| `uft_mdiag_report` | **1** | 0 | **gibt es nicht** |
| `uft_mdiag_writable` | **1** | 0 | **gibt es nicht** |
| `uft_mdiag_result_t` | **1** | 0 | **gibt es nicht** |

**Zehn von vierzehn gibt es bereits** — die Zulieferung baut auf MF-1176 auf,
statt daneben zu bauen. **Vier sind neu**, und alle vier gehören zur
Mediendiagnose, die es im Baum nirgends gibt (`uft_mdiag`: rc 1, 0 Treffer).

---

## 2. `uft_os_volume` — Zeilendiff statt Dateiübernahme (b)

| Datei | Diff gegen den Baum |
|---|---|
| `include/uft/hal/uft_os_volume.h` | **+29 / −3**, 2 Blöcke |
| `src/hal/uft_os_volume.c` | **+120 / −44**, 5 Blöcke |

### 2.1 Die 44 entfernten Zeilen sind eine UMSCHREIBUNG, keine Rücknahme

Sie gehören sämtlich zu `xfer_range()` — der Funktion, die den Bereich
überträgt. Die neue Fassung kann dasselbe und zusätzlich mehrere
**Ganzdurchläufe**. Entfernt werden ihr alter Rumpf, die alte
Zustandszuweisung (`OK` gegen `OK_RETRY`) und die alte Abbruchbehandlung;
**nicht** entfernt werden Zusagen, Kommentare mit Messungen oder
Verdrahtungen.

**Das ist der Unterschied zu A-008 und A-009.** Dort hätte eine Übernahme
einen gemessenen Kommentar bzw. vier verdrahtete GUI-Reiter gelöscht. Hier
liegt eine saubere Weiterentwicklung derselben Funktion vor.

**Trotzdem gilt: Zeilen, nicht Datei.** Der Baum hat sich seit dem Packen
weiterbewegt, und die fünf Blöcke sind einzeln nachvollziehbar.

### 2.2 Die ABI-Frage ist richtig gelöst — gemessen

| | Reihenfolge der Zustände |
|---|---|
| Baum | `OK`, `OK_RETRY`, `BAD`, `SKIPPED` |
| Paket | `OK`, `OK_RETRY`, `BAD`, `SKIPPED`, **`OK_LATE_PASS`** |

**Der neue Wert ist ANGEHÄNGT.** Alle vier vorhandenen behalten ihre Zahl —
dieselbe Regel, nach der MF-1189 und MF-1190 Felder angehängt haben. Ebenso
die neuen Felder: `attempts_total`, `pass` (im Sektorstatus) und `passes`
(in der Konfiguration) stehen hinter den bestehenden.

---

## 3. Der inhaltliche Kern: `passes` ist nicht `retries`

Die Zulieferung begründet die Unterscheidung selbst, und die Begründung ist
physikalisch:

> *„`retries` wiederholt SOFORT. Kopf und Medium sind in derselben Lage, die
> Wärme ist dieselbe, der Schmutz sitzt an derselben Stelle."*

Ein **Ganzdurchlauf** geht dagegen über den ganzen Bereich und kommt später
zurück — der Kopf hat sich bewegt, das Medium hatte Ruhe. Der neue Zustand
`UFT_OSVOL_SEC_OK_LATE_PASS` sagt genau das: *erst in einem späteren
Durchlauf gelesen.*

**Und das trifft einen offenen Befund des Baums.** `P3-284` führt
`adaptive_passes` als „Schalter ohne Schaltung". Nachgemessen, mit
Kommentarfilter:

| Stelle | Art |
|---|---|
| `include/uft/recovery/uft_multiread_pipeline.h:264` | Kommentar |
| `include/uft/recovery/uft_multiread_pipeline.h:280` | **Deklaration** `bool adaptive_passes;` |
| `src/recovery/uft_multiread_pipeline.c:93` | **Zuweisung** `.adaptive_passes = true` |
| `src/recovery/uft_multiread_pipeline.c:903` | Kommentar |

**Gesetzt: einmal. Gelesen: nie.** Die einzigen beiden weiteren Nennungen
sind Kommentare — genau die Klasse `aufrufer_gegen_kommentar`, gegen die
dieser Baum seit MF-767 filtert.

Der Begriff aus dieser Zulieferung wäre damit **der erste Ort im Baum, an
dem ein Ganzdurchlauf etwas bedeutet**.

---

## 4. Zwei Annahmen der Aufnahme fallen (d) und (e)

### 4.1 Es gibt keinen CLI-Konflikt (d)

Die Aufnahme dieses Postens erwartete einen — bei A-008 gab es ihn
(`tools/uft-c64pp-catalog.c`). **Hier nicht:** gemessen trägt von den vier
Quelldateien nur `test_passes.c` ein `int main`, und das ist ein Test.
`uft_media_diag.c` und `uft_os_volume.c` haben keines.

**Entscheidung: der Konflikt entfällt.** Die Mediendiagnose ist eine
Bibliotheksfunktion; wo ihre Ausgabe erscheint, entscheidet die Oberfläche.

### 4.2 Die Lizenz ist nicht offen (e)

Gemessen trägt **jede** der vier Dateien in Zeile 1:

```
/* SPDX-License-Identifier: GPL-2.0-or-later */
```

Das ist die Projektlizenz. **Damit ist die Code-Frage geklärt** — anders als
bei A-009, wo keine der drei neuen Dateien eine Lizenzzeile hat.

**Offen bleibt etwas anderes, und das gehört benannt:** die *Erkenntnis*
stammt laut Belegkette aus der **Hilfe zu FloImg 1.02** (Petari,
8bitchip.info, 2011-08-08). Eine Programmhilfe ist urheberrechtlich
geschützter Text. Übernommen ist hier aber **keine Formulierung, sondern
eine Tatsachenbehauptung** („Ganzdurchläufe bringen mehr als sofortige
Wiederholungen") — und Tatsachen sind nicht schutzfähig. Nach `MF-695` ist
das der Kanal *Spec*, und er steht offen. **Eine Prüfung fehlt trotzdem:**
ob die Aussage belegt ist, hat niemand nachgemessen; sie ist die
Behauptung eines Werkzeugautors von 2011. Vorschlag 2 unten.

---

## 5. Die fünf Fragen (c)

### 5.1 „wo können die formate verbessert werden"

Nicht die Formate — die **Erfassung**. Der Sektorstatus des Baums kennt
heute vier Zustände; ihm fehlt die Unterscheidung „beim ersten Anlauf" gegen
„erst nachdem das Medium Ruhe hatte". Für einen forensischen Bericht ist das
ein Unterschied: der zweite Fall sagt etwas über den **Zustand des Mediums**,
nicht nur über das Ergebnis.

### 5.2 „ist es auf andere formate übertragbar"

**Vollständig.** Ganzdurchläufe sind eine Eigenschaft des *Leseverfahrens*,
nicht des Formats. Der Begriff passt auf jeden Controller und jedes Medium;
`uft_osvol_*` ist nur der erste Ort, an dem er auftaucht, weil dort der
Sektortransport über das Wirtssystem liegt (MF-1176).

### 5.3 „welche einstellungen fehlen noch"

Genau die eine, die die Zulieferung mitbringt: `passes` neben `retries`,
beide mindestens 1. Der Baum hat den **Namen** dafür bereits
(`adaptive_passes`), aber keine Bedeutung — §3.

### 5.4 „was habe wir noch nicht"

Die **Mediendiagnose** (`uft_mdiag_*`, 4 Symbole, rc 1 / 0 Treffer im ganzen
Baum). Sie beantwortet eine Frage, die der Baum heute nicht stellt: *ist
dieses Medium überhaupt noch beschreibbar, und was sagt das Fehlerbild über
seinen Zustand?*

### 5.5 „brauch es eine HAL-Erweiterungen"

**Ja — und sie ist genau die, die hier vorliegt.** `uft_os_volume` **ist**
die HAL-Seite (`src/hal/`), und die Erweiterung ist additiv und ABI-sicher
(§2.2). Das ist die erste der drei Zulieferungen, bei der die Antwort auf
diese Frage nicht „nein" lautet.

---

## 6. Disposition

| Teil | Urteil | Begründung |
|---|---|---|
| `uft_os_volume.h` (+29/−3) | **REGISTER** als Zeilendiff | ABI-sicher angehängt (§2.2) |
| `uft_os_volume.c` (+120/−44) | **PARTIAL** als Zeilendiff | Umschreibung von `xfer_range()`, fünf Blöcke einzeln |
| `uft_media_diag.c` | **REGISTER** | 4 Symbole, im Baum nicht vorhanden |
| `test_passes.c` | **PARTIAL** | prüft die eigene Umsetzung; als Rotbeweis-Skizze brauchbar, nicht als Abnahme |
| `UFT-NN_Passes_Diagnosen.md` | **REFERENCE** | Belegkette; die FloImg-Aussage ist ungeprüft (§4.2) |

**Kein Byte dieser Zulieferung ist nach `src/`, `include/` oder `tests/`
geschrieben worden (f).**

---

## 7. Vorschläge für `docs/OPEN_ITEMS.md`

1. **`adaptive_passes` ist gesetzt und wird nie gelesen** — gemessen mit
   Kommentarfilter: eine Deklaration, eine Zuweisung, zwei Kommentare, null
   lesende Stellen (§3). Verschärft `P3-284` um die Messung. *Kennzahl:*
   keine der vier.
2. **Die Aussage „Ganzdurchläufe bringen mehr als sofortige Wiederholungen"
   ist im Baum nirgends belegt** — sie stammt aus einer Programmhilfe von
   2011 und ist die Behauptung eines Werkzeugautors. Bevor ein
   Sektorzustand darauf gebaut wird, gehört sie gemessen (ein Korpus mit
   marginalen Sektoren genügt). *Kennzahl:* keine der vier; es ist
   `EINFRIER-REGEL` (b): jede Zahl gemessen, Ungemessenes als „nicht
   belegt".
3. **`uft_osvol_image_size` steht im Header und im Test, aber nicht in
   `src/hal/uft_os_volume.c`** (§1). Entweder ist sie anderswo umgesetzt
   oder inline — gemessen ist nur, dass die `.c` sie nicht nennt.
   *Kennzahl:* keine der vier.

---

## 8. Quellen

* Zulieferung: `neue-ideen/FloImg-extrakt.zip` — `uft_os_volume.c` (13 984 B),
  `uft_os_volume.h` (9 625 B), `uft_media_diag.c` (7 859 B),
  `test_passes.c` (9 073 B), Bericht `UFT-NN_Passes_Diagnosen.md` (12 113 B);
  alle vier Quelldateien `SPDX-License-Identifier: GPL-2.0-or-later`
* Belegkette der Zulieferung: FloImg 1.02 (Petari, 8bitchip.info,
  Hilfe vom 2011-08-08); `NFORMAT.DOC` (1992); cw2dmk `jv3.h` (GPL-2,
  nur gelesen)
* Baum: `origin/main` = `70a940af`
* Gegengehalten: `P3-284`, `P3-113`, `P3-88`, `MF-1176`, `MF-695`, `MF-767`
