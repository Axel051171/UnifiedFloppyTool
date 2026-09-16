# A-016 · Gutachten `ChrisBertrandDotNet/ST-Recover` — **das ist `P3-63`**

**Stand:** 2026-09-16 · **Posten:** `A-016` · **Lizenz:** **Ms-RL** (Microsoft Reciprocal)
**Prüfstand:** `origin/main` = `70a940af` · Klon: `tools/uft-scout/work/ST-Recover`

---

## Ergebnis in drei Sätzen

**Die Vorbedingung dieses Postens ist gefallen — und zwar durch eigene
Arbeit, seit der Aufnahme.** `P3-63` war blockiert, weil seine zwei Größen
„gefüllte Feldgrenzen voraussetzen" und es dafür „heute keinen Erzeuger
gibt"; **seit MF-1190 gibt es ihn**, und niemand hat den Zusammenhang
bemerkt. Aus dem Repo selbst ist dagegen nichts Neues zu holen: `P3-63`
führt bereits vier belegte Bestätigungen daraus, und Ms-RL sperrt jede
Übernahme härter als Apache-2.0.

**Empfehlung: die zwei Größen sind jetzt baubar — der Bau ist ein eigener
Posten mit ABI-Frage, und dieses Gutachten benennt seine Gestalt.**

---

## 1. Die Vorbedingung ist gefallen (c) — die eigentliche Nachricht

### 1.1 Was `P3-59` entschieden hat, und warum

`P3-59` ist erledigt (MF-832), aber seine Auflösung war eine **Umkehrung**:

> *„Vier weitere Felder auf vier tote zu setzen macht die Struktur
> **irreführender**, nicht reicher."*

Der Grund war gemessen: von den fünf Positionsfeldern auf `uft_sector_t`
wird **genau eines** je gefüllt — `angular_position`, von `uft_atx.c:366`.
`id_offset` hat im ganzen Baum **eine** Fundstelle, nämlich seine eigene
Deklaration. **Es gab keinen Erzeuger.**

### 1.2 Was sich seither geändert hat

**MF-1190 hat den Erzeuger gebaut.** `include/uft/flux/uft_mfm_sector_parser.h`
trägt seither je Sektor:

| Feld | Bedeutung |
|---|---|
| `id_sync_bit` | Beginn der ersten A1-Marke des IDAM |
| `data_start_bit` | erstes Datenbit hinter der DAM-Marke |
| `gap2.start_bit` / `.end_bit` | Anfang und Ende der Lücke zwischen ID und Daten |
| `lead_gap.start_bit` / `.end_bit` | dasselbe für die Vorlauflücke |

**Das sind genau die Feldgrenzen, deren Fehlen `P3-59` zur Absage und
`P3-63` zur Blockade geführt hat** — und sie sitzen im **Produktivpfad**
`uft_mfm_decode_track()` mit zwei Aufrufern (MF-1190 hat das gemessen,
bevor es gebaut wurde).

### 1.3 Und die Aufnahme dieses Postens hat es fast gesehen

Sie schrieb: *„`duree_espace_libre_en_1er` ist die Lücke als Messgröße in
Mikrosekunden statt als Restmenge — und genau die entsteht bei
`P3-453`/Phase 2 … Wer Phase 2 baut, erzeugt die Hälfte von `P3-63` mit."*

**Phase 2 ist gebaut** (MF-1190 = `e22e1b78`). Die Hälfte ist also da, und
die andere Hälfte — die Feldgrenzen — ebenfalls, weil derselbe Commit
`id_sync_bit` und `data_start_bit` mitgebracht hat.

### 1.4 Die Entscheidung

**Die zwei Größen sind nicht mehr zurückzustellen — ihre Begründung ist
entfallen.** Gebaut wird hier trotzdem nicht, und der Grund ist die
Bauform, nicht die Sache:

* Sie hängen an **`uft_sector_t`**, der kanonischen Struktur
  (`include/uft/uft_types.h`: „This is the ONE definition … used across the
  entire project"). Jede Änderung dort ist eine **ABI-Frage**.
* Der Baum hat die Lösung bereits skizziert, im Header selbst
  (`uft_types.h:397`): ein Flag **`has_bit_positions` nach dem Muster von
  `has_angular_position`**. Damit bliebe die Struktur ehrlich — ein Feld
  ohne Flag ist eine Zahl, die wie eine Messung aussieht.
* Es braucht **Rotbeweis zuerst** und einen Aufrufer im selben Commit (D2),
  und der Weg vom MFM-Parser in `uft_sector_t` ist Decoder-Arbeit am
  Spurmodell — nicht ein Feld anhängen.

**Die Gestalt des Postens, damit er nicht neu erdacht werden muss:**

1. `has_bit_positions` an `uft_sector_t` **anhängen** (ABI-sicher, wie
   MF-1189/MF-1190).
2. Im Übergang vom `uft_mfm_sector_t` zum `uft_sector_t` die drei Felder
   füllen: `id_offset` <- `id_sync_bit`, `data_offset` <- `data_start_bit`,
   `gap_before` <- `lead_gap.end_bit - lead_gap.start_bit`.
3. **Rotbeweis:** ein Test, der ohne die Füllung rot wird, weil
   `has_bit_positions` false bleibt, obwohl der Flusspfad die Zahlen hat.
4. Die zweite `P3-63`-Größe (`duree_espace_libre_en_1er`, die Lücke in
   **Mikrosekunden**) folgt aus `gap2` plus der Zellzeit — die
   Umrechnungsstelle ist zu benennen, damit sie nicht zum zweiten Mal
   gerechnet wird (`MF-1177`).

---

## 2. Die 13 Quelldateien (a)

Gemessen: **13 Dateien, 3 222 Zeilen**, C++ (Borland-Stil, französische
Bezeichner).

| Datei | Zeilen | Urteil |
|---|---|---|
| `Classe_Piste.cpp` | **1366** | **schon in `P3-63` genannt** — die Spurklasse; hier liegen die vier belegten Werte |
| `Unit1.cpp` | 426 | **trifft nicht zu** — Oberfläche (VCL) |
| `fdrawcmd.h` | 298 | **trifft nicht zu** — Windows-FDC-Treiberkopf, fremde Schnittstelle |
| `Classe_Disquette.cpp` | 261 | **schon genannt** — Diskettenklasse, Geometrie |
| `Classe_Piste.h` | 201 | **schon genannt** — Felder der Spurklasse |
| `Acces_disque.cpp` | 184 | **trifft nicht zu** — Zugriff über `fdrawcmd`, Hardware |
| `Classe_Disquette.h` | 160 | **schon genannt** |
| `Analyse_disque.cpp` | 97 | **neu, aber klein** — Ablaufsteuerung der Analyse |
| `Unit1.h` | 86 | **trifft nicht zu** — Oberfläche |
| `Acces_disque.h` | 44 | **trifft nicht zu** |
| `ST_Recover.cpp` | 42 | **trifft nicht zu** — Einsprung |
| `Analyse_disque.h` | 41 | **neu, aber klein** |
| `Constantes.h` | 16 | **schon genannt** — hier stehen die Konstanten, die `P3-63` zitiert |

**Ergebnis: ein zweites Lesen liefert nichts Neues von Gewicht.** `P3-63`
hat die Quelle bereits ausgewertet und führt vier Bestätigungen daraus —
32 µs je Rohbyte aus `200000/6250`, „11 Sektoren/Spur in 2 Umdrehungen",
das 50-Byte-Fenster zwischen ID- und Datenmarke, und `128 << (n & 3)`.
**Das ist die ehrliche Antwort auf den Auftrag, ein zweites Mal
hinzusehen.**

Die beiden Dateien, die `P3-63` nicht nennt (`Analyse_disque.*`, 138
Zeilen), sind Ablaufsteuerung: welche Spur wann wie oft gelesen wird. Das
ist eine **Strategie**, kein Formatwissen — und die Strategiefrage hat
dieser Baum mit dem Ganzdurchlauf aus `A-010` gerade auf dem Tisch.

---

## 3. Die Ms-RL-Grenze (b)

Gemessen: `License.htm` ist die **Microsoft Reciprocal License**; `gh api`
liefert `NOASSERTION`, weil GitHub sie nicht einordnet.

**Ms-RL ist mit GPL in jeder Fassung unverträglich** — anders als
Apache-2.0, das nur gegen GPL-2 scheitert. Die GPL-3-Bindung aus `MF-698`
öffnet hier also **nichts**; `P3-452` hilft ausdrücklich nicht.

**Was erlaubt bleibt und was hier geschehen ist:** Werte und Verfahren
lesen und beschreiben, keine Zeile übernehmen. Dieses Gutachten zitiert
**keine** Zeile Quelltext aus dem Repo; die vier Werte, die `P3-63` führt,
sind physikalische Größen bzw. arithmetische Ausdrücke, und die sind nicht
schutzfähig.

---

## 4. Die fünf Fragen (e)

### 4.1 „wo können die formate verbessert werden"

Nicht die Formate — die **Sektoraussage**. Heute sagt `uft_sector_t` nicht,
wo auf der Spur ein Sektor liegt, obwohl der Flusspfad es seit MF-1190
weiß. §1.

### 4.2 „ist es auf andere formate übertragbar"

**Die Feldgrenzen ja, innerhalb von MFM** — `uft_mfm_sector_parser` bedient
jedes IBM-MFM-Format. **Für GCR und FM nicht**: dort gibt es keinen
entsprechenden Erzeuger, und ein Flag `has_bit_positions` sagt genau das
ehrlich, statt null zu liefern.

### 4.3 „welche einstellungen fehlen noch"

Keine aus diesem Repo. Seine Einstellungen (Leseversuche je Spur) sind die
Strategieachse aus `A-010`.

### 4.4 „was habe wir noch nicht"

Die **Position eines Sektors auf seiner Spur** als belegte Aussage — und
das ist seit MF-1190 keine Lücke im Wissen mehr, sondern eine im Transport.

### 4.5 „brauch es eine HAL-Erweiterungen"

**Nein.** `fdrawcmd.h` ist ein Windows-Treiberkopf; UFT geht über seine
eigenen Controller.

---

## 5. Disposition

| Teil | Urteil | Kanal |
|---|---|---|
| `Classe_Piste.cpp`, `Constantes.h` | **REFERENCE, bereits ausgewertet** | *Spec*; `P3-63` führt vier Werte daraus |
| `Analyse_disque.*` (138 Z.) | **FUNDUS** | Lesestrategie; berührt `A-010` |
| Oberfläche, `fdrawcmd.h`, Zugriff | **uninteressant** | fremde Plattform |
| jede Übernahme | **BLOCKED** | **Ms-RL**, mit GPL in jeder Fassung unverträglich |

**Kein Byte dieses Repos ist nach `src/`, `include/` oder `tests/`
geschrieben worden (f).**

---

## 6. Vorschlag für `docs/OPEN_ITEMS.md`

**Einer**, und er schreibt `P3-63` fort statt einen neuen Punkt anzulegen:

1. **`P3-63` ist nicht mehr blockiert — der fehlende Erzeuger ist seit
   MF-1190 da.** Der Punkt sagt „die zwei Groessen setzen gefuellte
   Feldgrenzen voraus; heute gibt es keinen Erzeuger dafuer".
   `uft_mfm_sector_t` trägt seit MF-1190 `id_sync_bit`, `data_start_bit`,
   `gap2` und `lead_gap` — im **Produktivpfad** `uft_mfm_decode_track()`.
   Was fehlt, ist der **Transport** in `uft_sector_t`, und der braucht ein
   Flag `has_bit_positions` nach dem Muster von `has_angular_position` —
   die Lösung steht bereits als Kommentar in `uft_types.h:397`.
   **Gestalt des Bauauftrags:** §1.4 dieses Gutachtens. *Kennzahl:* keine
   der vier; es ist die fünfte, offene Zahl (MF-640) und `P3-387`.

---

## 7. Quellen

* `https://github.com/ChrisBertrandDotNet/ST-Recover`, flach geklont
  2026-09-16 nach `tools/uft-scout/work/ST-Recover`
* **Ms-RL** (`License.htm`, `Source/Ms-RL License.htm`); 13 Quelldateien,
  **3 222 Zeilen**
* Baum: `origin/main` = `70a940af`;
  `include/uft/flux/uft_mfm_sector_parser.h` (MF-1190),
  `include/uft/uft_types.h` (`uft_sector_t`, Kommentarblock 366-413)
* Gegengehalten: `P3-63`, `P3-59` (MF-832, die Umkehrung), `P3-387`,
  `P3-453` / **MF-1190**, `MF-1177` (eine Größe, eine Rechnung),
  `MF-698`/`P3-452` (GPL-3-Bindung — hilft hier nicht)
