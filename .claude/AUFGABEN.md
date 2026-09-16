# Arbeitsliste

**Ein Posten ist in Arbeit. Alle anderen warten.**

Gepflegt vom Skill `.claude/skills/aufgabe/SKILL.md` (`/aufgabe`).
Aufnahme startet keine Arbeit. Abschluss verlangt einen Beleg.

**Diese Liste hält Aufträge, keine Befunde.** Ein Befund gehört nach
[`docs/OPEN_ITEMS.md`](../docs/OPEN_ITEMS.md) — seit MF-588 die einzige
Liste dafür, „neue Befunde kommen dorthin, nirgendwo sonst". Hier steht
nur, **was wann bearbeitet wird**, und der Posten verweist auf seine
`P?-NNN`, statt sie zu wiederholen.

Nichts wird gelöscht: Erledigtes wandert nach unten, Verworfenes bekommt
eine Zeile `Zurückgenommen:` mit Grund. Nummern (`A-001`, `A-002`, …)
laufen fort und werden nie wiederverwendet.

> **Warum die Zählung bei `A-004` beginnt.** `A-001` bis `A-003` wurden am
> 2026-09-16 bei der Abnahme des Skills selbst verbraucht (Paarlauf mit
> zwei Unteragenten, Sitzungsprotokoll). Sie stehen in keiner Fassung
> dieser Datei, und wiederverwendet werden sie nach der Regel oben nicht.

---

## In Arbeit

*(höchstens einer — leer heißt: nichts läuft)*

### A-017 · Die Befunde der Zulieferungen EINBAUEN (Plan, Phasen 1–5)
- **Status:** **erledigt** 2026-09-16 · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „bau alles ein mach einen plan und zetzte es um" · später
  „mach A-005 ungestopt" · „beginne phase 1"
- **Kennzahl:** **keine der vier** für Phase 1 (Trägerprovenienz ist eine
  forensische Aussage, keine Tier-Stufe). Phase 4/5 berühren
  `T3 runter` mittelbar über den Abgleicher.
- **Kanal:** **Port.** Gemessen (MF-1188): alle fünf gelieferten
  Quelldateien sind `GPL-2.0-or-later`, also eigener Code unter der
  Projektlizenz — für die Phasen 1–4 gibt es keine Lizenzfrage. Die
  **Datenbestände** dahinter sind eine andere Sache und einzeln geprüft.
- **Einfrier-Regel:** **ja** für jede Phase, die den Format-/Decoder-Layer
  berührt → Rotbeweis zuerst, benannte Referenz im Header.
- **OPEN_ITEMS:** `P3-454` (Phase 1) · `P3-453` (Phase 2) · `P3-455`
  (Phase 3) · `P3-449`/`P3-451` (Phase 4) · `P3-445` (Phase 5)
- **Fertig heißt:** jede Phase mit eigenem Commit, Rotbeweis belegt,
  Produktivaufrufer im **selben** Commit (D2), Vollsuite grün.
- **Stand:** **Phase 1 und Phase 2 fertig.**
  · **Phase 1** (`P3-454`): `uft_bootstrap.{h,c}` + Aufruf in
    `fat_analyze_boot_sector()` + 9 Zusagen; D2-Probe gemessen (Aufruf weg →
    genau die zwei Verdrahtungs-Zusagen rot, 7/9, rc 1). Suite **494/494**.
    Nebenbefund gemessen und berichtigt: die Bootstrap-Datenbank **liegt**
    im gitignorierten Fremdklon (379 Schlüssel, 490 Namen, 223 verifiziert)
    — der leere Bestand ist **S3** (GPL-3.0 + Datenbankherstellerrecht),
    nicht Abwesenheit.
  · **Phase 2** (`P3-453`): die Lückenmessung sitzt **im** Produktivpfad
    `uft_mfm_decode_track()` (2 Produktivaufrufer), kein viertes Modul und
    kein dritter Wortleser. Rotbeweis zuerst, Mutationsmatrix **13 von 13
    im ersten Lauf**, Suite **495/495**. Zwei Befunde erst im Lauf: das
    erste Lückenwort weicht in 5 von 9 Fällen ab — **führende Taktzelle**,
    `0x9254 ^ 0x1254 = 0x8000`, und genau dort, wo das letzte Bit davor
    eine 1 war; und die Encoder-Lage ist von der Leseseite byteweise
    bestätigt (146 = 80+12+4+50). **S1 gehalten:** keine der fünf
    unbelegten Zahlen der Zulieferung ist übernommen, es gibt kein Feld
    `splice` und kein Herkunfts-Urteil.
  · **Phase 3** (`P3-455`, PC-Schutz aus Fehlsektormustern): **ANGEHALTEN
    MF-1192, und der Halt ist das Ergebnis.** Kein Code. Drei Gründe, jeder
    einzeln hinreichend, alle gemessen: (1) im Korpus liegen **103**
    Dateien und **0** geschützte PC-Abbilder — ein Rotbeweis für einen
    Herstellernamen könnte nur gegen eine selbst nach der Tafel gebaute
    Diskette prüfen, der geschlossene Kreis von MF-1009/MF-1028; (2) `P0-2`
    ist mit der ausdrücklichen Begründung geschlossen, dass Verdrahten die
    falsche Antwort ist; (3) `uft_pc_disk_if_t.is_bad(lba)` hat im Baum
    keine Antwort — der Header dafür ist ein Phantom (9 von 9 Funktionen
    ohne Rumpf, `P3-228`). **Die Lizenz sperrt hier NICHT** (Nachbau,
    9 Sektorlisten sind keine Datenbank, MF-698 deckt die Bindung).
    Vierter Befund: zwei der neun Muster sind über die Fehlsektoren nicht
    unterscheidbar und müssten „mehrdeutig" melden. Nächster Handgriff
    steht in `P3-455` und ist **nicht** die Tafel.
  · **Phase 4** (`P3-449`/`P3-451`, Bitkonfidenz aus A-006): **ANGEHALTEN
    MF-1193.** Kein Code, dafür drei Messungen und zwei neue Befunde.
    Gemessen: das Ziel des Ports — `src/algorithms/advanced/
    uft_multi_rev_fusion.c` (469 Z., 4 öffentliche Funktionen) — hat **0**
    Produktivaufrufer, **jede** Aufrufstelle liegt in einem Test; die
    Vorbedingung der Weak/Fuzzy-Trennung (Phasenlage je Bit) liegt in
    `src/algorithms/uft_kalman_pll.c` (399 Z., 6 öffentliche Funktionen)
    mit **0** Aufrufstellen überhaupt; und der erreichbare
    Abstimmungspfad (`multiread_*`, 1 Produktivaufrufer) stimmt
    **byteweise** ab, kann eine bitweise Karte also nicht tragen. Ein Port
    hätte eine **vierte** „weak bit"-Definition hinter eine Tür gelegt, die
    niemand öffnet. Neu eingetragen: **`P3-457`** (die Berichtigung aus
    MF-950 ist da und unerreichbar) und **`P3-458`** („weak" wird dreimal
    verschieden gerechnet, fünf Zahlen ohne Quelle). `P3-448`, `P3-449`
    und `P3-451` sind um je eine Messung erweitert.
  · **Und der Fehler liegt hier auch bei MIR, nicht nur an den
    Zulieferungen.** Ich habe fünf Phasen aus den Gutachten geordnet, ohne
    vorher die **Erreichbarkeit** ihrer Ziele zu messen. Phasen 1 und 2
    trugen, weil ihre Ziele Produktivaufrufer hatten
    (`fat_analyze_boot_sector`, `uft_mfm_decode_track`); Phasen 3 und 4
    fallen an genau dieser Frage. Ein Plan, der D2 erst beim Bauen prüft,
    plant die Hälfte seiner Posten gegen eine Wand. **Für Phase 5 wird die
    Erreichbarkeit zuerst gemessen.**
  · **Phase 5** (`P3-445`, A-005): **FERTIG MF-1195.** Und die
    vorgezogene Erreichbarkeitsmessung hat getragen — sie fiel erstmals
    seit Phase 2 positiv aus: `uft_probe_ranking_t` liegt in **3**
    Produktivdateien, `analyze_quality()` in `uft_smart_open.c` hält
    Zylinder/Köpfe/Sektoren/Sektorgröße zusammen. Deshalb wurde gebaut
    statt angehalten.
    Behoben und eingebaut: `uft_floppy_ranking_t` mit `tied`/`ambiguous`/
    `kandidaten` und den drei Feldzählern; ein Feld wird nur verglichen,
    wenn **beide** Seiten etwas sagen. 113 Referenzsätze als Tafel,
    Namensnennung (CC BY-SA 4.0) im Header, `docs/ORACLES.md` um den
    Nachtrag ergänzt, damit die dortige „nicht übernehmen"-Regel nicht
    im Widerspruch steht. **12 Zusagen, Mutationsmatrix 14/14 im zweiten
    Lauf** (der erste 8/14 — alle sechs Entkommenen waren echte Lücken
    in meinen Gegenproben), D2-Probe am Aufrufer gemessen mit geprüfter
    Rücknahme, Suite **497/497**.
    **Drei Erwartungen von mir sind dabei gefallen** (1,44 M hat 5 statt
    3 Gleichrangige und der PC ist jetzt dabei; Amiga DD 2 statt 1; die
    beiden Victor/Commodore-Sätze sind GCR, nicht MFM) — und die beiden
    Zahlen des Rotbeweises in `P3-445` waren beide falsch, weil sie an
    der defekten Fassung gemessen waren.
  · **Der Plan ist damit abgearbeitet:** Phasen 1, 2 und 5 gebaut,
    Phasen 3 und 4 mit Ergebnis angehalten. Offen bleiben die Posten
    `A-008`…`A-016` und `A-018`…`A-024` in der Warteschlange.
- **Beleg:** Phase 1 = **MF-1189** = `a27fb2d0` (9 Dateien, 894 Einfügungen)
  · Phase 2 = **MF-1190** = `e22e1b78` (6 Dateien, 856 Einfügungen)
  · Phase 3 = **MF-1192** = `32685609` (Halt, reine Doku)
  · Phase 4 = **MF-1193** = `503522c0` (Halt, reine Doku)
  · Phase 5 = **MF-1195** = `38d20378` (12 Dateien, 1607 Einfügungen).
  Alle mit Pre-Commit-Toren grün und `mcp=0`.
  **Gepusht** 2026-09-16 als Welle von 12 Commits (`76c56535..38d20378`),
  alle drei pre-push-Tore bestanden, danach `origin/main..HEAD` = 0.
- **Abschluss mit einer Einschränkung, die nicht verschwiegen wird:** die
  Zeile `Fertig heißt` verlangt für **jede** Phase Commit, Rotbeweis,
  Produktivaufrufer im selben Commit und grüne Vollsuite. Erfüllt ist das
  von den Phasen **1, 2 und 5**. Die Phasen **3 und 4** sind als
  **gemessener Halt** abgeschlossen — ein Halt ist nach dem AUFTRAG ein
  Ergebnis, aber er ist nicht das, was diese Zeile beschreibt. Der Posten
  gilt als erledigt, weil der PLAN abgearbeitet ist, nicht weil fünf
  Phasen gebaut wurden.
- **Nachtrag 2026-09-16 (MF-1199 = `70a940af`):** der Push von MF-1195 hat
  die CI rot gemacht — `Consistency check`, **1** Befund,
  `[STAND.md stale]`. Ursache gemessen: `gen_stand.py::offen()` zählt
  `docs/OPEN_ITEMS.md` aus dem **Arbeitsbaum**, in dem die Parallelsitzung
  unversionierte Zeilen liegen hatte. Versioniert 7796, Arbeitsbaum 7798,
  eingecheckt stand **7797** — ein Zwischenstand einer fremden Datei, den
  es in keinem Commit gab. Behoben mit einer Zeile (Befund `P3-463`),
  gepusht, `origin/main` = `70a940af`. Die Lehre steht als `P3-463`; der
  Umweg über einen `git worktree` ist dort **verworfen**, weil er im
  Hauptdepot zweimal `core.bare = true` hinterlassen hat.

---

`A-007` ist **erledigt** (Gutachten, MF-1186); sein Eintrag steht
unverändert an seinem Platz in der Warteschlange unten und trägt dort Status,
Stand und Beleg. Er wird hier **nicht** wiederholt — zwei Fassungen desselben
Postens wären zwei Wahrheiten (D3).

Erledigt: `A-005` (MF-1184 = `2aa7bda6`), `A-006` (MF-1185 = `6dbb4ebf`),
`A-007` (MF-1186 = `87abb0b3`), **`A-017`** (Phasen 1–5, MF-1189…MF-1195,
gepusht; Nachtrag MF-1199), **`A-008`** und **`A-009`** (MF-1201 =
`618ff62b`), **`A-010`** und **`A-011`** (MF-1202). Alle vier
Begutachtungen liegen als Dokument in `tools/uft-scout/out/`; der EINBAU
ist bei jeder ein eigener, noch nicht aufgenommener Posten.

**Der Begutachtungsblock `A-008`…`A-016` ist abgearbeitet** (neun Posten,
neun Gutachten unter `tools/uft-scout/out/`). `/aufgabe weiter` zieht als
Nächstes **`A-018`** hoch (Apple DOS 3.3).

> **Was die neun Begutachtungen zusammen ergeben haben, gezählt:**
> · **Vier Zulieferungen** (A-008…A-011): drei von ihnen liefern `.pro` und
>   `tests/CMakeLists.txt` als „aktuelle Verdrahtung" mit, jedes Mal
>   **kleiner als die des Baums**. Bei `A-009` hätte eine Dateiübernahme
>   **vier verdrahtete GUI-Reiter still wieder abgehängt** (MF-1194).
>   Nur `A-010` überschreibt nichts Gemessenes.
> · **Fünf Fremdrepos** (A-012…A-016): **vier fehlende Werkzeugketten in
>   Folge** — Swift, Rust, Forth, und schon bei A-005 `mkfs.fat`/`mtools`.
>   *Orakel* ist damit viermal hausintern verschlossen und **delegierbar**,
>   wie die Tier-3-Bank (MF-310).
> · **Lizenzlage:** einmal Port zulässig (**A-012**, MIT), zweimal
>   Apache-2.0 (A-013/A-014, kein Port), einmal **Ms-RL** (A-016, härter —
>   unverträglich mit GPL in jeder Fassung), einmal GPL-3 (A-015, Port
>   rechtlich offen, aber Forth nach C ist eine Neuschreibung).
> · **Drei Annahmen der Aufnahmen sind gefallen**, alle drei gemessen:
>   A-014 (E01 ist im Repo gar nicht umgesetzt), A-013 (drei Zahlen von
>   `P3-385` falsch), A-010 (kein CLI-Konflikt, Lizenz nicht offen).
> · **Zwei neue Befunde**, beide aus A-015: `P3-465` (MGT-Verzeichnis um
>   ein Byte verschoben, **an einem echten Korpus-Abbild gemessen**, in
>   einem Format auf **T1**) und `P3-466`.
> · **Eine Blockade ist gefallen:** `P3-63` war auf einen Erzeuger
>   gewartet, den **MF-1190 längst gebaut hatte** — bemerkt hat es erst
>   A-016.
>
> **Die Regel, die sich durch alle vier Zulieferungen zieht: nie die Datei,
> immer die Zeilen.**

> **Was die vier Begutachtungen zusammen ergeben haben — ein Muster, kein
> Einzelfall.** Drei der vier Zulieferungen liefern `UnifiedFloppyTool.pro`
> und `tests/CMakeLists.txt` als „aktuelle Verdrahtung" mit, und sie sind
> jedes Mal **kleiner als die des Baums**. Bei `A-009` hätte eine
> Dateiübernahme **vier verdrahtete GUI-Reiter still wieder abgehängt**
> (MF-1194), bei `A-008` einen gemessenen Kommentar über einen behobenen
> Defekt gelöscht. Nur `A-010` überschreibt nichts Gemessenes — dort ist die
> Überlappung eine echte Umschreibung.
>
> **Die Regel daraus, für jede künftige Zulieferung: nie die Datei, immer
> die Zeilen.** Sie kostet beim Einbau mehr und ist der einzige Weg, der
> keine stille Rücknahme erzeugt.

> **Warteschlange, gezählt statt geschätzt (Stand 2026-09-16):** `A-008`
> … `A-016` (neun Begutachtungen), dazu die sieben heute aufgenommenen
> `A-018` Apple DOS 3.3, `A-019` TR-DOS, `A-020` Amiga-Medienklassifikation,
> `A-021` FAT12-Robustheit, `A-022` diskstack, `A-023` Apple-Sektorordnung,
> `A-024` Audit-Umfang, und am Ende `A-004` (Punkt 7, angehalten am
> vereinbarten Schnitt) — **17 wartende Posten** bei einem laufenden.
>
> Nach Art sortiert, damit die Reihenfolge entscheidbar ist:
> · **Defekt im erreichbaren Pfad behauptet:** `A-019` (TR-DOS-Sonde liest
>   angeblich drei falsche Felder), `A-020` (Amiga-Klassierung),
>   `A-021` (FAT12 liefert bei Schaden erfundene Daten)
> · **Lücke:** `A-018` (Apple DOS 3.3 fehlt auf der FS-Achse ganz),
>   `A-023` (Apple-Sektorordnung, offener Punkt aus MF-714)
> · **Begutachtung fremden Codes:** `A-008`…`A-016`, `A-022`
> · **Methode statt Code:** `A-024`
>
> Vorgezogen wird nichts von selbst — das ist `/aufgabe vor`.

> **Ein Befund aus `A-007` betrifft zwei wartende Posten.** MF-698 bindet
> die verteilbare Fassung an GPL-3, und Apache-2.0 ist GPL-3-verträglich —
> die Kanal-Zeilen von `A-013` und `A-014` messen dagegen gegen GPL-2 und
> führen den Port als geschlossen. Sie bleiben unverändert stehen, bis der
> Eigentümer entscheidet; das Gutachten `a007_…` §8 nennt die Messung.

---

## Warteschlange

*(Reihenfolge = Bearbeitungsreihenfolge; oben ist als Nächstes dran)*

### A-005 · Zulieferung `UFT_Floppy_Reference_AARD_Paket.zip` begutachten
- **Status:** **erledigt** 2026-09-16 (Begutachtung; der EINBAU ist offen und
  wäre ein eigener Posten) · **Aufgenommen:** 2026-09-16 · hochgezogen mit
  „weiter zieht A-005 hoch". Der Posten bleibt an dieser Stelle stehen, bis
  der nächste `weiter`-Griff ihn nach `Erledigt` verschiebt — verschoben wird
  er dann als Ganzes, nicht kopiert.
- **Wortlaut:** „arbeite alles sehr genau aus / finde alles und alles
  raussuchen was ich übersehen habe / - wo können die formate verbessert
  werden / - ist es auf andere formate übertragbar / - welche einstellungen
  fehlen noch / - was habe wir noch nicht / - brauch es eine
  HAL-Erweiterungen / erstelle mir code Beispiele was besser gemacht werden
  kann / plan verstanden, was kannst du besser machen ??"
- **Kennzahl:** **keine der vier → und das ist hier keine Absage.** Der
  Auftrag ist eine **Beurteilung**, kein Einbau; MF-640 regelt, was in die
  Arbeitsliste geplant wird, nicht was der Eigentümer bestellt. Welche der
  vier ein Einbau bewegen würde, ist Teil der Antwort und wird gemessen,
  nicht behauptet. Vorwegnahme ausdrücklich nicht: die Wikipedia-Tafel ist
  eine **Sekundärquelle**, und eine Sekundärquelle hebt nach dem Maßstab
  von `docs/ORACLES.md` keine Tier-Stufe.
- **Kanal:** zweiteilig, und das muss es bleiben. **Daten** —
  `floppy_reference_wikipedia.tsv` (113 logische + 24 physische Sätze),
  geerntet aus „List of floppy disk formats", **CC BY-SA 4.0**, also
  *copyleft* und mit derselben Frage wie die OmniFlop-239-Ernte
  (EU-Datenbankherstellerrecht, §§ 87a ff. UrhG). **Code** — `LICENSE` des
  Pakets ist GPL-2.0-or-later, © 2024–2026 Axel Kramer, also **eigener**
  Code und keine Fremdübernahme. Für die Begutachtung selbst: *gelesen,
  nicht eingebaut*.
- **Einfrier-Regel:** für die **Begutachtung** nein (kein Code). Für einen
  **Einbau** ja, mittelbar: `src/formats/reference/` liegt im Format-Layer
  und der Abgleicher rangiert Format-Kandidaten — also Rotbeweis zuerst,
  benannte Referenz im Header, jede Zahl gemessen.
- **OPEN_ITEMS:** keiner. Gemessen 2026-09-16:
  `grep -n 'OmniFlop\|Wikipedia\|CC BY-SA' docs/OPEN_ITEMS.md` findet keine
  Zeile zu dieser Ernte. Fachlich berührt, aber **nicht** erledigt:
  `LIZ-1` (Attributions-Zensus) und `P3-435` (Spurbilanz ohne Quelle →
  kein FDC-Profil, S1).
- **Bei der Aufnahme gemessen — eine Zusage des Pakets trägt nicht:**
  `INTEGRATION.md` sagt „The current UFT workspace is already wired" und
  „The repository copy already contains this wiring in
  `tests/CMakeLists.txt`". `git ls-files | grep -iE 'aard|floppy_reference'`
  ist **leer** — kein Byte des Pakets liegt im Baum, weder Quelle noch Test
  noch Daten. Das ist Teil des Prüfauftrags, nicht seine Antwort.
- **Fertig heißt:** ein Gutachten als Dokument (`tools/uft-scout/out/` oder
  `docs/`), das **(a)** jede der fünf Fragen mit einer Messung gegen den
  Baum beantwortet (`git grep` je Bezeichner, keine Aufzählung — MF-930),
  **(b)** jeden Code-Vorschlag an `Datei:Zeile` der Stelle festmacht, die er
  ersetzt, mit der Kennzahl nach MF-640, **(c)** die Lizenzfrage der Ernte
  als benannte **Eigentümer-Entscheidung** stehen lässt statt sie zu
  empfehlen, und **(d)** kein Byte des Pakets nach `src/`, `include/`,
  `tests/` oder `data/` schreibt. Der Einbau ist ein **eigener** Posten mit
  eigenem Rotbeweis.
- **Aufwand:** Begutachtung eine Sitzung. Einbau **nicht schätzbar**, weil
  er an der Lizenzentscheidung hängt.
- **Stand:** **Begutachtung fertig.** Gutachten liegt als
  `tools/uft-scout/out/a005_floppy_reference_aard.gutachten.md` (571 Zeilen).
  Alle fünf Bedingungen der Zeile `Fertig heißt` sind erfüllt: fünf Fragen je
  mit Messung, vier Code-Vorschläge je an `Datei:Zeile` mit Kennzahl, die
  Lizenzfrage steht als Frage, und `git status --short` zeigt **kein** Byte
  des Pakets unter `src/`, `include/`, `tests/`, `data/`.
  **Kernbefunde:** der Abgleicher sagt bei Gleichstand nicht ab — für eine
  PC-1,44-M-Diskette drei Sätze mit score 100, 7 verglichen, 0 abweichend,
  und keiner ist der PC-Satz; „unbekannt" und „widerspricht" sind dasselbe
  (3 von 3 gemessen); von acht angeblich fehlenden Einstellungen hat der Baum
  **fünf**; die physische Achse (`coercivity`/`tracks_per_inch`/
  `bits_per_inch`) ist mit **je 0 Treffern** die einzige echte Lücke; die
  AARD-Flagge feuert auf das Wort **AARDVARK** (confidence 45), während ihre
  Zufallsrate gemessen in Ordnung ist (6 gegen 5,60 erwartet, 0 von 64
  Flaggen). **Der Einbau ist NICHT gemacht** und braucht die
  Lizenzentscheidung — das ist ein eigener Posten, wenn der Eigentümer ihn
  will.
- **UNGESTOPPT 2026-09-16** — wörtlich „mach A-005 ungestopt". Damit ist die
  Lizenzsperre **S3** für die CC-BY-SA-4.0-Ernte aufgehoben, und der Einbau
  ist in Arbeit genommen. **Drei Dinge hebt die Entscheidung nicht auf, und
  sie bleiben Bedingungen des Einbaus:** (a) eine Sekundärquelle hebt nach
  `docs/ORACLES.md` **keine Tier-Stufe** — der Katalog kommt als Referenz
  herein, nicht als Beleg; (b) die Namensnennungs- und Weitergabepflichten
  von CC BY-SA 4.0 bleiben (die Quell-URL steht in
  `UFT_FLOPPY_REFERENCE_SOURCE_URL` und in beiden TSV); (c) **der gemessene
  Defekt des Abgleichers ist zu BEHEBEN, nicht mitzuliefern** — er meldet
  für eine PC-1,44-M-Diskette drei Sieger mit score 100 und zählt
  „unbekannt" als „widerspricht" (`P3-445`). Ihn einzubauen wie er ist,
  wäre eine Falschaussage im Erkennungspfad und verstößt gegen die
  EINFRIER-REGEL. Der Weg steht als Code im Gutachten §8.1/§8.2.
- **Beleg:** Gutachten `tools/uft-scout/out/a005_floppy_reference_aard.gutachten.md`,
  **MF-1184 = `2aa7bda6`** (2 Dateien, 1012 Zeilen, alle Vorhaken grün).
  Nachgetragen wie angekündigt: die Zeile lag im MF-1184-Commit selbst und
  konnte ihren eigenen Hash nicht kennen. Befunde als `P3-442`, `P3-444`,
  `P3-445`, `P3-446` eingetragen (MF-1188).

### A-006 · Zulieferung `hacking floppy disk.zip` (Kopierschutz/Umdrehungen) begutachten
- **Status:** **erledigt** 2026-09-16 (Begutachtung; der EINBAU ist offen) ·
  **Aufgenommen:** 2026-09-16 · hochgezogen mit „weiter zieht A-006 hoch"
- **Wortlaut:** „finde alles und alles raussuchen was ich übersehen habe /
  - wo können die formate verbessert werden / - ist es auf andere formate
  übertragbar / - welche einstellungen fehlen noch / - was habe wir noch
  nicht / - brauch es eine HAL-Erweiterungen / erstelle mir code Beispiele
  was besser gemacht werden kann / plan verstanden, was kannst du besser
  machen ??"
- **Kennzahl:** **keine der vier für die Begutachtung.** Ein Einbau würde
  keine der vier bewegen — Schutzerkennung ist keine Tier-Stufe, kein
  Wandlungspfad, kein Leck, kein Bench-Alter. **Das ist genau der Fall, für
  den MF-640 die fünfte Zahl offengelassen hat**, und der Punkt gehört bei
  der Begutachtung benannt, nicht umgangen. Die Zulieferung nennt sich
  selbst „T2-Niveau, nicht T1b" — sie weiß das.
- **Kanal:** **Spec.** Die Belegkette nennt DrCoolZic Rev 1.2 („Copyleft",
  laut Zulieferung zitieren und Werte nehmen, nicht Zeilen), Chris Evans
  2020 und Atari-Forum-Faden 21952. Der **Code** des Pakets (`uft_revolution
  .c/.h`, `uft_protection_scan.c/.h`, `test_protect.c`) trägt in der
  Zulieferung **keine Lizenzzeile** — gemessen: `grep` nach `SPDX`, `GPL`,
  `MIT`, `Lizenz`, `License` findet im Analysedokument nur die
  DrCoolZic-Zeile, und im Paket liegt **keine `LICENSE`-Datei** (anders als
  bei A-005). Herkunft und Lizenz des Codes sind damit bei der Aufnahme
  **ungeklärt** und vor jedem Einbau zu klären (`LIZ-1`-Klasse).
- **Einfrier-Regel:** **ja.** `src/protection/` und der Flusspfad sind
  Decoder-Layer. Rotbeweis zuerst, benannte Referenz im Header, jede Zahl
  gemessen — und die Zulieferung sagt selbst, dass **kein geschütztes
  Abbild im Korpus** liegt, also ist alles darin synthetisch geprüft.
- **OPEN_ITEMS:** **Treffer, und sie sind der Kern der Prüfung.** `P3-236`
  (Umdrehungsvergleich zur Weak-Bit-Erkennung, ✅ behoben), `P3-237`
  (Weak-Bit-Erkennung für mehrfach umdrehende Formate, ✅ geklärt),
  `P3-238` (HAL reicht Umdrehungsgrenzen durch, ✅ so weit es ohne Gerät
  geht), `P0-2` (Schutzkatalog ohne Aufrufer, ✅ MF-983). Dazu offen:
  `P3-41` (`uft_fuzzy_bits.c`, vier Mängel), `P3-46`/`P3-47` (Index- und
  Ovl16-Randfälle), `P3-222` (Phantom-Modul `uft_flux_analysis.h`).
- **Bei der Aufnahme gemessen — zwei Zahlen der Zulieferung, zwei
  Ergebnisse:** ihre Kopftafel sagt `uft_flux_revolution_t` habe 2 Header
  und **0** `.c`-Dateien; das **trifft zu** (gemessen über `git ls-files`).
  Ihre Aussage, die HAL reiche Umdrehungen nicht durch, steht dagegen neben
  drei ✅-Punkten: `uft_hal_read_flux_ex` hat **2** `.c`-Aufrufer, und
  `uft_multi_rev_fusion.c/.h` liegt im Baum. Die Zulieferung will
  `uft_revolution.c/.h` **neu** anlegen — Dateien dieses Namens gibt es
  nicht, ein Mehrfach-Umdrehungs-Modul aber schon. Ob das eine Lücke oder
  eine **zweite Kopie** derselben Rechnung ist (Klasse MF-1015/MF-1026/
  MF-1177), ist die erste zu klärende Frage, nicht ihre Antwort.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** jede der fünf
  Fragen mit einer Messung gegen den Baum beantwortet, **(b)** je Aussage
  der Zulieferung „trifft zu / überzeichnet / veraltet seit MF-NNN"
  vergibt — mit `git ls-files`-Messung je Bezeichner, nicht per
  Aufzählung (MF-930), **(c)** die Lizenz- und Herkunftsfrage des
  mitgelieferten Codes als offene Eigentümer-Entscheidung stehen lässt,
  **(d)** für jeden Code-Vorschlag die Stelle nennt, die er ersetzt, samt
  Rotbeweis-Skizze, und **(e)** kein Byte des Pakets nach `src/`,
  `include/` oder `tests/` schreibt. Einbau ist ein eigener Posten.
- **Aufwand:** Begutachtung eine Sitzung (521 Zeilen Analysedokument +
  4 Quelldateien + 1 Test). Einbau **nicht schätzbar** — er hängt an der
  Lizenzfrage und daran, ob ein geschütztes Abbild beschafft werden kann.
- **Stand:** **Begutachtung fertig.** Gutachten liegt als
  `tools/uft-scout/out/a006_kopierschutz_umdrehungen.gutachten.md`
  (399 Zeilen). Alle sechs Bedingungen der Zeile `Fertig heißt` erfüllt.
  **Kernbefunde:** der Code ist gut — Bau mit `-Werror -pedantic` 0
  Warnungen, 7 Tests grün **mit eigenen Rot-Proben**, und der Klassierer
  sagt `UNDECIDED` statt zu raten (an 20 000 Bits gemessen: weak 0, fuzzy 0
  bei 50,17 % Flackern). Die **Lagebeschreibung** ist zur Hälfte veraltet:
  `P3-238` ✅ MF-951/954 (`uft_hal_read_flux_ex()` trägt die
  Umdrehungsgrenzen, **SCP verdrahtet**), `P3-236` ✅ MF-949, `P3-237` ✅
  MF-950. Drei Behauptungen berichtigt: Zeile **386** statt 355;
  **Greaseweazle behält** die Indexzeiten (`uft_gw_decode_flux_index_times`);
  vier Zählungen um 1-2 daneben. Echte Lücken: `uft_flux_revolution_t` 0
  `.c`-Verwender **plus Phantom-API** (`uft_flux_revolution_alloc`, 1
  Deklaration / 0 Definitionen), und Sektor-im-Sektor sowie verschobene
  Spuren (je 0 Treffer). **`uft_rev_align()` ist im Vertrag genannt und
  nicht geliefert** — MF-950 hat gemessen, dass ihr Fehlen 99,71 %
  falsches Flackern kostet. **Und drei meiner eigenen vier Erwartungen
  fielen**, weil die Feldbeschreibung von `flux_count` („gemittelt") ihrer
  Umsetzung (`(hi-lo)*16`) widerspricht — das ist der Befund, nicht mein
  Testergebnis. **Der Einbau ist NICHT gemacht** (S1 für zwei unbelegte
  Schwellen, Lizenzfrage zu DrCoolZics „Copyleft" offen).
- **Beleg:** Gutachten
  `tools/uft-scout/out/a006_kopierschutz_umdrehungen.gutachten.md`,
  **MF-1185 = `6dbb4ebf`** — nachgetragen wie angekündigt.

### A-007 · Zulieferung `DiskImageTool-extrakt.zip` (Herkunft · Schreibnähte · PC-Schutzmuster) begutachten
- **Status:** **erledigt** 2026-09-16 (Begutachtung; der EINBAU ist offen) ·
  **Aufgenommen:** 2026-09-16 · hochgezogen mit „weiter mit A-007"
- **Wortlaut:** „arbeite alles sehr genau aus / finde alles und alles
  raussuchen was ich übersehen habe / - wo können die formate verbessert
  werden / - ist es auf andere formate übertragbar / - welche einstellungen
  fehlen noch / - was habe wir noch nicht / - brauch es eine
  HAL-Erweiterungen / erstelle mir code Beispiele was besser gemacht werden
  kann / plan verstanden, was kannst du besser machen ??"
- **Kennzahl:** **keine der vier für die Begutachtung.** Herkunftsnachweis,
  Schreibnahterkennung und Schutzmuster sind keine Tier-Stufe, kein
  Wandlungspfad, kein Leck, kein Bench-Alter. Wie bei A-006 ist das der
  Fall, für den MF-640 die fünfte Zahl offengelassen hat („Dateien mit
  ungeklärter Herkunft" — und **hier geht es um die Herkunft eines
  Datenträgers**, nicht einer Quelldatei; ob das dieselbe Zahl ist, gehört
  zur Antwort).
- **Kanal:** **Port zulässig, und das ist gemessen, nicht angenommen.**
  Gegenstand ist `github.com/Digitoxin1/DiskImageTool` (VB.NET,
  **GPL-3.0**). Die Zulieferung begründet die Zulässigkeit damit, UFTs
  verteilbare Kombination sei über `uft_kfstream_air.c`, `uft_ipf_air.c`
  und `uft_stx_air.c` ohnehin GPL-3 — gemessen über `git ls-files` liegen
  **alle drei** im Baum (1 / 2 / 2 Treffer). Die Prämisse hat also einen
  Boden; ob sie trägt, ist eine Lizenz- und keine Messfrage und bleibt
  Eigentümer-Entscheidung (Konfliktordnung 2: Lizenz vor Fähigkeit).
- **Einfrier-Regel:** **ja.** `src/protection/` und der Flusspfad sind
  Decoder-Layer; `src/analysis/` grenzt daran. Rotbeweis zuerst, benannte
  Referenz im Header (`Modules/BootstrapDB.vb` ist benannt), jede Zahl
  gemessen.
- **OPEN_ITEMS:** berührt `P3-271` (die Spleissstelle ist bei UFT **ein**
  Zeitpunkt je Spur, in IPF eine Gap-Größe auf **beiden** Seiten je
  Sektor) und `P3-7` (der Schreibstartpunkt hat keinen Verbraucher, P6,
  MF-769) — beide direkt auf der Schreibnaht-Hälfte. Status dort ist bei
  der Begutachtung zu lesen, nicht hier zu behaupten.
- **Bei der Aufnahme gemessen — der zweite Fund liegt schon im Baum, und
  zwar ungerufen:** `include/uft/analysis/uft_deepread_splice.h` +
  `src/analysis/deepread/uft_deepread_splice.c` + `tests/
  test_flux_splice_pos.c` existieren. Gemessen über `git ls-files` nennen
  **genau zwei** Dateien `deepread_splice`/`uft_splice_`: das Modul selbst
  und der Test — **kein Produktivaufrufer.** Das ist die Write-Splice
  Detection aus `CLAUDE.md` §DeepRead, eines der **7 von 8** Module ohne
  Aufrufer (MF-627/MF-767). Ein neues `uft_splice.c` daneben wäre die
  zweite Kopie derselben Rechnung (MF-1015/MF-1026/MF-1177) statt der
  fehlenden Tür (P3-204). Welche der beiden Lagen es ist, ist die erste zu
  klärende Frage. **Gleiches gilt für den ersten Fund:**
  `src/forensic/uft_provenance.c` hat **6** `.c`-Nenner und
  `uft_fundus_provenance.c` liegt daneben — die Behauptung „für den
  Datenträger selbst führt UFT keine Provenienz" ist damit eine Aussage
  über eine **Unterscheidung**, die gemessen werden muss.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** jede der fünf
  Fragen mit einer Messung gegen den Baum beantwortet, **(b)** für jeden
  der drei Funde entscheidet „fehlt / liegt ungerufen da (P3-204) / ist
  eine zweite Kopie" — je mit `git ls-files`-Messung, nicht per
  Aufzählung (MF-930), **(c)** die GPL-3-Prämisse als benannte
  Eigentümer-Entscheidung stehen lässt und dabei die IPF-Quarantäne
  ausdrücklich prüft, **(d)** die fehlende Bootstrap-Datenbank benennt —
  die Zulieferung sagt selbst „die Datenbank ist nicht im Baum ... wer sie
  nicht hat, hat 379 leere Einträge", also dieselbe Datenbank-/Ernte-Frage
  wie A-005 —, und **(e)** kein Byte des Pakets nach `src/`, `include/`
  oder `tests/` schreibt. Einbau ist ein eigener Posten.
- **Aufwand:** Begutachtung eine Sitzung (542 Zeilen Analysedokument +
  6 Quelldateien + 1 Test + 1 Wandlerskript). Einbau **nicht schätzbar** —
  er hängt an der Lizenzentscheidung und an der Beschaffung der Datenbank.
- **Stand:** **Begutachtung fertig.** Gutachten liegt als
  `tools/uft-scout/out/a007_diskimagetool_herkunft.gutachten.md`.
  Alle fünf Bedingungen der Zeile `Fertig heißt` erfüllt.
  **Die drei Funde, je mit Urteil:** Bootstrap-Herkunft **fehlt für den
  Datenträger** (`oem_name` 9 `.c`, `bootstrap` 0 `.c`; `uft_provenance`
  6 `.c` führt Dateien, nicht Träger) · Schreibnaht **fehlt für
  IBM-MFM, aber das Muster hat drei Vorgänger** — `uff_detect_splices()`
  0 Aufrufer, `uft_deepread_detect_splice()` 0 Aufrufer, und
  `G64_DIAG_SPLICE_DETECTED` ist **ein Etikett, das niemand setzt**; die
  Zulieferung nennt zwei Stellen und übersieht die dritte · PC-Schutz aus
  Fehlsektormustern **fehlt** (`bad_sector` 25 `.c` baumweit, 1 in
  `src/protection/`, 0 Musterschluss). **Selbstzusagen tragen alle vier**,
  Bau `-Werror -pedantic` 0 Warnungen, 6 Tests grün mit drei Rot-Proben.
  **Der Nebenfund reicht über den Posten hinaus:** die Lizenzprämisse ist
  belegt — alle drei AIR-Dateien tragen `GPL-3.0-only`, und MF-698 bindet
  „die VERTEILBARE Fassung des Gesamtwerks an GPL-3". Damit messen
  `P3-385` und mein eigener `A-014`-Eintrag gegen **GPL-2**, während
  Apache-2.0 mit **GPL-3** verträglich ist — der Port-Kanal für `A-013`
  und `A-014` ist möglicherweise offen. **Eigentümer-Entscheidung, nicht
  meine.** Fünf Vorschläge stehen in §13 des Gutachtens.
- **Nachtrag MF-1187 („mach weiter mit A-007"):** zwei der vier
  Nicht-geprüft-Punkte aus §12 sind **geprüft**, und beide Ergebnisse
  betreffen mich selbst. **(1) `Modules/BootstrapDB.vb` gelesen** — sie liegt
  geklont im Baum (213 Zeilen). Die Belegkette **trägt wörtlich**: CRC32 über
  den Bootstrap-Code ist der Schlüssel (`Dictionary(Of UInteger, …)`), der
  OEM-Name der Prüfwert, und der Win9x-Rückfall steht im Original. **Neu
  gefunden:** `FindXDFMatch()` nullt bei Bootstrap-Länge `&H1AD` = 429 Byte
  die Bytes `0xE7`–`0xEE` (8 Byte) und rechnet die CRC neu — die Zulieferung
  hat das **nicht** (0 Treffer), eine echte XDF-Diskette verfehlt damit ihren
  Eintrag. Trifft MF-1087/ARCH-18. **Und ein Vorwurf von mir fällt:** der
  `Verified`-Zustand **ist** übernommen (490 OEM-Namen, 223 verifiziert).
  **(2) Die `0x4E`-Kandidatenzahl ist geprüft und ZURÜCKGENOMMEN.** Die
  Fundstellen sind überwiegend Kennungen („RAIN", „BOUN"), 68000-Opcodes
  (`0x4EFx`, `0x4E75`), ein Versatz `h[0x4E]`, eine Tafelzeile und
  Greaseweazle-Befehlsbytes — kein Füllbyte. **Und meine Nachzählung war
  selbst falsch:** 43 von 36, weil die zweite Messung still auf `grep -li`
  schaltete (36 + 7 = 43). Es steht **keine dritte Zahl** an ihrer Stelle;
  Vorschlag V3 ist zurückgenommen, V6 (XDF) ist neu.
- **Beleg:** Gutachten
  `tools/uft-scout/out/a007_diskimagetool_herkunft.gutachten.md`,
  **MF-1186 = `87abb0b3`**; Korrektur und Nachtrag **MF-1187** (Hash folgt).

### A-008 · Zulieferung `UFT_C64PP_Protection_Catalog.zip` begutachten
- **Status:** **erledigt** 2026-09-16 (Begutachtung; der EINBAU ist ein
  eigener Posten) · MF-1201 = `618ff62b` · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „arbeite alles sehr genau aus / finde alles und alles
  raussuchen was ich übersehen habe / - wo können die formate verbessert
  werden / - ist es auf andere formate übertragbar / - welche einstellungen
  fehlen noch / - was habe wir noch nicht / - brauch es eine
  HAL-Erweiterungen / erstelle mir code Beispiele was besser gemacht werden
  kann / plan verstanden, was kannst du besser machen ??"
- **Kennzahl:** **keine der vier für die Begutachtung.** Schutzerkennung ist
  keine Tier-Stufe (wie A-006). **Ein Teil bewegt aber etwas Messbares, und
  das ist ungewöhnlich:** die Zulieferung stuft die RapidLok-Heuristik zu
  einer neutralen Strukturmeldung herab. Damit trägt sie auf der Seite der
  **Ehrlichkeit** ab, was P3-38/P3-39 als Befund führen — das ist Rücknahme
  einer Falschaussage, nicht Zuwachs, und genau die Richtung von MF-1077.
- **Kanal:** **Spec** für den Inhalt (die Seiten des C64 Preservation
  Project, Abschnitt 9 „Quellen" des Auditberichts), **MIT** für den
  mitgelieferten Kern: `LICENSE_C64PP_MODULE.txt` ist MIT © 2026 „UFT
  Project contributors", `LICENSE_UFT_PROJECT.txt` liegt daneben — MIT in
  ein GPL-2-or-later-Projekt ist unproblematisch (permissiv in Copyleft).
  **Vorsicht ist trotzdem geboten:** C64-Schutzarbeit hat in diesem Baum
  die nibtools-Vorgeschichte („vier Dateien und Tage gekostet"), also
  gehört die Herkunft je Katalogeintrag geprüft, nicht je Paket.
- **Einfrier-Regel:** **ja.** `src/protection/` ist Decoder-Layer; dazu
  kommt eine GUI-Änderung. Rotbeweis zuerst — und die Zulieferung sagt
  selbst, ihre Tests seien **synthetisch** („synthetische Positiv-/Negativ-
  verträge").
- **OPEN_ITEMS:** **viele Treffer, und sie sind der Maßstab.** `P3-38` (die
  C64-Schutzerkennung rät Markennamen, das Unterscheidungsmerkmal fehlt
  ganz), `P3-39` (alle drei Markennamen an der Quelle widerlegt), `P3-144`
  (der einzige erreichbare Schutzerkenner bestand aus drei Heuristiken),
  `P3-79` (Artefaktsignaturen identifizieren Codefamilien, nicht Produkte —
  Korpus von 17 C64-Kopierprogrammen), `P3-187` (C64-Signaturkonstanten
  ohne benannte Quelle). Dazu auf der Behälterseite `P3-36`, `P3-147`,
  `P3-194`, `P3-195`.
- **Bei der Aufnahme gemessen — die schwerste Feststellung dieser vier
  Zulieferungen:** das Paket liefert `UnifiedFloppyTool.pro` und
  `tests/CMakeLists.txt` mit und nennt sie in `README_EINBAU.md`
  „**aktuelle Verdrahtung**". Gemessen sind sie **kleiner als die des
  Baums**: `tests/CMakeLists.txt` **341 230** gegen **361 386** Byte
  (−20 156), `UnifiedFloppyTool.pro` **81 628** gegen **83 293** (−1 665).
  Sie stammen also aus einem **älteren** Stand; sie zu kopieren wäre eine
  **stille Rücknahme** fremder Verdrahtung — die teuerste Fehlerklasse
  dieses Baums. Der Einbau darf diese zwei Dateien **nicht** übernehmen,
  sondern nur die eigenen Zeilen daraus. Zweitens:
  `src/gui/ProtectionAnalysisWidget.cpp/.h` **liegen schon im Baum** — die
  mitgelieferten 35 KB sind eine geänderte Kopie, kein neues Modul.
  Drittens sagt das Paket selbst, der **Qt-Gesamtbau sei nicht gelaufen**.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** jede der fünf
  Fragen mit einer Messung gegen den Baum beantwortet, **(b)** je
  Katalogeintrag „belegt / Kandidat / widerlegt" vergibt und dabei gegen
  P3-38/P3-39/P3-79 hält statt gegen die Zulieferung, **(c)** für die
  GUI-Hälfte einen **Zeilendiff** gegen die vorhandene
  `ProtectionAnalysisWidget.cpp` nennt statt eine Dateiübernahme,
  **(d)** ausdrücklich festhält, dass `.pro` und `tests/CMakeLists.txt` des
  Pakets **verworfen** werden, mit den gemessenen Bytezahlen als Grund,
  und **(e)** kein Byte des Pakets nach `src/`, `include/` oder `tests/`
  schreibt. Einbau ist ein eigener Posten.
- **Aufwand:** Begutachtung eine Sitzung (224 Zeilen Auditbericht + 20
  Katalogeinträge + 1 Test + 1 Werkzeug + GUI-Diff). Einbau **nicht
  schätzbar** — die GUI-Hälfte ist ein Diff gegen lebenden Code und der
  Qt-Bau ist unerprobt.
- **Stand:** **Messungen liegen, das Gutachten ist noch nicht geschrieben.**
  Was gemessen ist (je Aussage mit Fundstelle):
  · **Die Kernleistung der Zulieferung liegt im Baum bereits vor.** Die
    Aufnahme-Zeile oben nennt als ihren einen kennzahlwirksamen Teil, die
    RapidLok-Heuristik zu einer neutralen Strukturmeldung herabzustufen.
    Gemessen hat **MF-402** (`22c35a37`) genau das getan, und gründlicher:
    `ufm_cbm_check_vmax()` als tautologisch entfernt,
    `ufm_cbm_check_rapidlok()` in `ufm_cbm_has_half_track_beyond_35()`
    umbenannt, die Klassifikation auf Strukturnamen umgestellt, jeweils mit
    der Begründung **zitiert an Ort und Stelle**
    (`src/protection/ufm_c64_scheme_detect.c:62-78`, `:186-200`;
    `include/uft/protection/ufm_cbm_protection_methods.h:15-27`).
  · **Der produktive Pfad ist ein anderer als der, den die Aufnahme
    vermutet hat — und er ist erreichbar.** Kette gemessen:
    `src/mainwindow.cpp:107` (`new StatusTab()`) → `src/statustab.cpp:458`
    (`new ProtectionAnalysisWidget(dlg)`) →
    `src/gui/ProtectionAnalysisWidget.cpp:215` (`ufm_c64_prot_analyze`) →
    `src/protection/ufm_c64_scheme_detect.c:124`. Ein Dialog, den ein
    Bediener anklickt.
  · **`g64_detect_protection()` ist NICHT erreichbar**, und beide Türen
    sind gemessen tot: `uft_advanced_open()` hat als einzigen Aufrufer
    `tests/test_advanced_guete_ohne_messung.c:274`, und
    `uft_advanced_detect_protection()` hat **nur Prototyp und Definition,
    null Aufrufer**. Deckt sich mit `P3-147`.
  · **BERICHTIGT — hier stand ein Fehler von mir, und er bleibt zitiert
    stehen.** Der Satz lautete: „Zwei Aussagen des Auditberichts tragen
    gegen DIESEN Baum nicht: er führt `ufm_c64_metrics_from_gcr()` als
    ‚produktiv aus ProtectionAnalysisWidget erreichbar / gute Grundlage' —
    der Bezeichner kommt im ganzen Baum **nicht vor** (`git grep`, rc 0,
    0 Treffer)." **Das ist falsch.** Richtig gemessen ohne Pfadliste:
    **43 Treffer in 15 Dateien**, darunter `src/protection/ufm_c64_metrics.c`,
    `include/uft/protection/ufm_c64_metrics.h` und **vier** Stellen in
    `src/gui/ProtectionAnalysisWidget.cpp`. Der Auditbericht hat an dieser
    Stelle **recht**: die Kette G64 → rohe GCR-Spur →
    `ufm_c64_metrics_from_gcr()` → `ufm_c64_prot_analyze()` ist der
    Produktivpfad, so wie er sie beschreibt.
    **Ursache meines Fehlers, benannt statt verschwiegen:** die Messung lief
    als `git grep … -- $(git ls-files) 2>/dev/null`. Die Pfadliste sprengt
    die Argumentlänge, git bricht mit rc≠0 und leerer Ausgabe ab, `2>/dev/null`
    verschluckt das `fatal:`, und ich habe die leere Ausgabe als „0 Treffer"
    gelesen. Das ist die Klasse `grep_exitkode_bricht_die_kette` — **dritter
    Fall an einem Tag**, und der teuerste, weil die Falschaussage in eine
    verfolgte Datei geschrieben wurde. Regel ab sofort: **`rc` je Lauf
    prüfen, nie `2>/dev/null` auf eine Messung, nie `-- $(git ls-files)`.**
    · Die zweite Hälfte des alten Satzes steht weiter: der Bericht nennt
    „mehrere parallele Implementierungen" als unverdrahtet, **ohne zu
    messen, welche** — das ist unverändert richtig und bleibt ein Mangel
    des Berichts.
  · **Die Klasse ist bereits mit einem Tor versehen.**
    `scripts/audit_protection_claims.py` (MF-557, verdrahtet in
    `scripts/check_consistency.py:752`) misst genau diese Lage: 33 Dateien,
    369 `uft_`-Funktionen, **3** von außen gerufen, **353** von keinem Test
    berührt. Sein Kopf warnt wörtlich davor, den Katalog anzuschließen,
    ohne ihn geprüft zu haben. Die 20 Katalogeinträge der Zulieferung
    würden genau dorthin gelegt.
  · **Der eine gemessene Rest ist neu und gehört nicht der Zulieferung:**
    weder `ufm_c64_scheme_detect.c` noch `ufm_c64_metrics.c` kennen
    **Nachbarschaft** oder Kopfbreite (gemessen, 0 Treffer). Damit liefern
    17.0/17.5/18.0 (mit einer 1541 nicht rückschreibbar, weil der
    Schreibkopf eine ganze Spur breit ist) und 1.5/17.5/35.5 (harmlos)
    dieselbe Ausgabe — zwei physikalisch entgegengesetzte Disketten, eine
    Meldung. Das Feld dafür liegt bereit und wird gefüllt:
    `ufm_c64_track_metrics_t.track_x2` (`ufm_c64_metrics.c:142`), also
    Nachbarschaft = `|Δ track_x2| == 1`, **ohne eine neue Konstante**.
    Steht als Kern von `P3-38`; der Einbau ist nach der Zeile
    `Fertig heißt` (e) **ausdrücklich ein eigener Posten** und wurde hier
    bewusst NICHT begonnen.
  · **Verworfen bleibt, was die Aufnahme schon gemessen hat:** die
    mitgelieferten `UnifiedFloppyTool.pro` (81 628 gegen 83 293 Byte) und
    `tests/CMakeLists.txt` (341 230 gegen 361 386) sind **älter** als die
    des Baums; sie zu übernehmen wäre eine stille Rücknahme fremder
    Verdrahtung. Dazu `tools/uft-c64pp-catalog.c` — ein CLI, das gegen die
    GUI-only-Regel des Projekts steht.
  **Das Gutachten ist geschrieben:**
  `tools/uft-scout/out/a008_c64pp_protection_catalog.gutachten.md`, mit
  (a)…(e) der Zeile `Fertig heißt`. Die drei Kernzahlen daraus:
  · **(b) 0 von 20** Katalogeinträgen sind mit dem belegbar, was
    `ufm_c64_track_metrics_t` trägt. **15 von 20** verlangen mindestens
    eines von zwölf Merkmalen, die die Struktur gar nicht hat
    (`FAT_TRACK`, `TRACK_ALIGNMENT`, `CUSTOM_HEADER`, `SLIDING_BITS`,
    `BYTE_COUNT`, `RPM_SENSITIVE`, `SECTOR_PARITY`, `LOADER_TIMING`,
    `SYNC_POSITION`, `MIXED_FORMAT`, `GAP_SIGNATURE`, `EXACT_SIGNATURE`);
    die übrigen fünf verlangen Bytefolgen, Positionen innerhalb der Spur
    oder Mehrfachlesungs-Vergleiche. Widerlegt ist keiner — wer nichts
    messen kann, kann auch nichts widerlegen. **Der Katalog ist damit eine
    Beschaffungsliste, keine Erkennung.**
  · **(c) Zeilendiff GUI:** 712 gegen 885 Zeilen, **17 Blöcke, +185/−22**.
    Unter den 22 entfernten steht ein **gemessener** Kommentar über eine
    behobene Index-Konvention (`g64_get_track()` Index 2 = Spur 1.0 gegen
    `ufm_c64_metrics_from_gcr()` Index 0 = Spur 1.0; ungerechnet trifft es
    genau die drei Zonengrenzen 17→18, 24→25, 30→31 und erzeugte drei
    falsche „long track"-Treffer auf **beiden** sauberen Referenz-
    disketten). Eine Dateiübernahme löschte die Begründung mit.
  · **(d)** `.pro` und `tests/CMakeLists.txt` verworfen, Bytezahlen im
    Gutachten §4; dazu `tools/uft-c64pp-catalog.c` als CLI gegen die
    GUI-only-Regel.
  · **Das Neue, das der Baum wirklich nicht hat:** der Beweisgrad hängt bei
    der Zulieferung von der **Aufnahmequelle** ab
    (`preserves_flux_timing`). Gemessen `rc 1, 0 Treffer` über
    `src/protection/` und `include/uft/protection/` — das ist die
    Tier-Idee, angewandt auf Schutzbefunde.
- **Beleg:** Gutachten
  `tools/uft-scout/out/a008_c64pp_protection_catalog.gutachten.md`;
  Commit folgt (der Baum ist durch den unversionierten Block einer zweiten
  Sitzung vorübergehend nicht committierbar, siehe A-017 Nachtrag).

### A-009 · Zulieferung `UFT_Atari_ST_Cartridge_Detection.zip` begutachten
- **Status:** **erledigt** 2026-09-16 (Begutachtung; der EINBAU ist ein
  eigener Posten) · MF-1201 = `618ff62b` · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „arbeite alles sehr genau aus / finde alles und alles
  raussuchen was ich übersehen habe / - wo können die formate verbessert
  werden / - ist es auf andere formate übertragbar / - welche einstellungen
  fehlen noch / - was habe wir noch nicht / - brauch es eine
  HAL-Erweiterungen / erstelle mir code Beispiele was besser gemacht werden
  kann / plan verstanden, was kannst du besser machen ??"
- **Kennzahl:** **keine der vier.** Ein Cartridge-ROM ist keine Diskette,
  also keine Tier-Stufe, kein Wandlungspfad. **Und das ist hier die Pointe,
  nicht der Einwand:** die Zulieferung baut eine **Absage** — ein erkanntes
  Cartridge bleibt absichtlich `isValid == false`, gekennzeichnet über
  `DiskImageInfo::isNonDiskImage`, damit es „nicht als Diskettenabbild
  fehlklassifiziert oder an einen Disketten-Decoder weitergereicht wird".
  Das ist genau die Bauform von `UFT_CAPS_OS_VOLUME` aus MF-1176 („der
  Eintrag ist eine Absage, keine Zusage"), und Absagen bewegen in diesem
  Baum keine Kennzahl — sie verhindern Falschaussagen.
- **Kanal:** **Nachbau, selbsterklärt — und die Quelle ist nicht
  benannt.** `docs/ATARI_ST_CARTRIDGE_DETECTION.md` sagt: „Die
  Datenstruktur wurde anhand der oeffentlich beschriebenen Atari-ST-
  Cartridge-Header neu implementiert. Es wurde kein Assembler-, Loader-
  oder Entpacker-Code der Referenzseite uebernommen." Welche Seite, welche
  Fassung, welche Stelle — steht nicht da. Nach MF-636 ist eine
  Attribution eine **rechtliche Aussage**, und „nach fremder Doku
  eigenständig implementiert" muss sagen, nach welcher. Dazu gemessen:
  **keine `SPDX`-Zeile und keine Lizenzangabe** in `.c` oder `.h`
  (0 Treffer für `SPDX`, `Licen`) — dieselbe Lücke wie bei A-006.
- **Einfrier-Regel:** **ja, aber nicht das Moratorium.** Gemessen: die
  Datei registriert **kein** Format-Plugin — 0 Treffer für
  `uft_format_plugin_t`, `uft_register_format_plugin`, `DSK_PLUGIN`,
  `.probe`, `uft_probe_konfidenz`. Das Moratorium für neue Plugins greift
  also nicht; die Regel „kein neuer **ungeprüfter** Code im
  Format-/Decoder-Layer" greift sehr wohl, weil die Datei in
  `src/formats/atari/` liegt. Rotbeweis zuerst, Referenz im Header.
- **OPEN_ITEMS:** kein Treffer zu „Cartridge" in der Befundliste. Fachlich
  benachbart und **gegenläufig**: der SCOUT-17b-Block (Zeile 1456 ff.)
  führt `CRT`, `EDD` und `FDX` als „Code liegt im Baum, gebaut, aber nie
  registriert und über `uft_disk_open()` unerreichbar" — die Lage
  MF-446/447 und P3-204. Diese Zulieferung wählt ausdrücklich den anderen
  Weg (kein Plugin, sondern eine Absage im Prüfpfad), und ob das der
  richtige ist, gehört in das Gutachten. Dazu `P3-349` (Formate, die ein
  Werkzeug im Baum schreibt und UFT nicht liest).
- **Bei der Aufnahme gemessen — zweites Mal dasselbe Muster, damit ist es
  eines:** wie A-008 liefert dieses Paket lebende Baudateien mit, und sie
  sind **kleiner als die des Baums**: `tests/CMakeLists.txt` 341 513 gegen
  361 386 (**−19 873**), `UnifiedFloppyTool.pro` 81 746 gegen 83 293
  (−1 547), und — schwerer — `src/mainwindow.cpp` 25 318 gegen 25 457
  (**−139**, lebender GUI-Code). Drei weitere Dateien sind geänderte
  Kopien (`explorertab.cpp` +364, `disk_image_validator.cpp` +2 963,
  `.h` +59). **Wirklich neu sind nur drei Dateien:**
  `src/formats/atari/uft_atari_st_cartridge.c` (341 Zeilen),
  `include/uft/formats/atari/uft_atari_st_cartridge.h`,
  `tests/test_atari_st_cartridge.c`. Ein Kopieren wäre eine stille
  Rücknahme. **Und eine Gegenprobe zu meinen eigenen Gunsten:** die
  Kennung `0xABCDEF42` kommt in der `.c` **nicht** vor — sie steht als
  `UFT_STCART_MAGIC` im Header, neben `0x00FA0000`. Das ist richtig so
  (D3, Wissen nicht doppelt halten); ich notiere es, weil der erste
  Blick auf die `.c` wie eine fehlende Kennung aussah.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** jede der fünf
  Fragen mit einer Messung gegen den Baum beantwortet, **(b)** die
  Grundfrage ausdrücklich entscheidet — gehört eine Nicht-Disketten-Absage
  in `src/formats/` oder in den Prüfpfad —, **(c)** die unbenannte Quelle
  als **Nachforderung** stellt (welches Dokument, welche Fassung, welche
  Stelle) statt sie zu ergänzen, **(d)** für die sechs überlappenden
  Dateien einen **Zeilendiff** nennt statt eine Dateiübernahme, mit den
  gemessenen Bytezahlen als Grund, und **(e)** kein Byte des Pakets nach
  `src/`, `include/` oder `tests/` schreibt. Einbau ist ein eigener Posten.
- **Aufwand:** Begutachtung eine Sitzung (341 Zeilen Modul + Header + Test
  + 1413 Byte Doku + sechs Dateidiffs). Einbau **nicht schätzbar** — er
  hängt an der Quellenangabe und an sechs Diffs gegen lebenden Code.
- **Stand:** **Gutachten geschrieben**,
  `tools/uft-scout/out/a009_atari_st_cartridge.gutachten.md`, mit (a)…(e).
  · **(b) Die Grundfrage ist entschieden, und zwar gemessen:** eine
    Nicht-Disketten-Absage gehört in den **Prüfpfad**. `DiskImageValidator`
    wird von **8 produktiven GUI-Dateien** benutzt (`decodejob`,
    `explorertab`, `forensictab`, `mainwindow`, `nibbletab`, `toolstab`,
    `xcopytab`, plus `.pro` und ein Test) — der erreichbarste Baustein
    dieser ganzen Begutachtungsreihe. `src/formats/atari/uft_atari.c`
    dagegen registriert **kein Plugin** (0 Treffer für `DSK_PLUGIN`,
    `uft_format_plugin_t`, `.probe`), und die dort liegenden
    **A78-Cartridge-Strukturen sind unerreichbar** (24+35+7 Nennungen,
    Test vorhanden, keine Tür — Klasse `P3-204`). Die Zulieferung wählt
    den richtigen Ort: ihr `disk_image_validator.cpp` ruft den Erkenner
    wirklich (`info.isNonDiskImage = true`), D2 ist auf ihrer Seite
    erfüllt. Bauform wie `UFT_CAPS_OS_VOLUME` (MF-1176): eine Absage,
    keine Zusage.
  · **(d) DER SCHWERSTE BEFUND — ihre `src/mainwindow.cpp` macht MF-1194
    rückgängig.** Gemessen **+17/−50**; unter den 50 entfernten Zeilen
    steht die Verdrahtung von `ProtectionTab`, `ForensicTab`, `NibbleTab`
    und `XCopyTab` samt dem Messprotokoll, das MF-1194 **vor** dem
    Verdrahten angelegt hat. Eine Dateiübernahme hängte **vier GUI-Reiter
    still wieder ab**. Grund ist banal: das Paket ist älter als der Baum.
    Die übrigen Zahlen: `disk_image_validator.cpp` **+58/−0** (rein
    additiv), `.h` +2/−0, `explorertab.cpp` +11/−2,
    `tests/CMakeLists.txt` 341 513 gegen 361 386 B, `.pro` 81 746 gegen
    83 293 B.
  · **(c) Der Kanal trägt nicht.** Die Doku sagt „anhand der oeffentlich
    beschriebenen Atari-ST-Cartridge-Header neu implementiert" und nennt
    **keine** Seite, Fassung oder Stelle; nach MF-636 ist das zu wenig.
    Dazu gemessen: **keine** der drei neuen Dateien trägt eine SPDX-,
    Lizenz- oder Copyright-Zeile (0 Treffer). Bis zur Nachlieferung ist
    der Kanal **Fundus**, nicht Nachbau.
  · **Das Neue:** `DiskImageInfo` kennt heute `isValid` und
    `isFluxFormat`; ein Objekt, das **erkannt und trotzdem abgelehnt**
    wird, hat kein Feld und fällt mit „unbekannt" in einen Topf — Klasse
    `MF-980`/D6. `isNonDiskImage`, `nonDisk`, `STCART` und `0x00FA0000`
    kommen im ganzen Baum nur in dieser Datei hier vor.
  · **(e)** kein Byte nach `src/`, `include/`, `tests/`.
- **Beleg:** Gutachten
  `tools/uft-scout/out/a009_atari_st_cartridge.gutachten.md`; Commit folgt.

### A-010 · Zulieferung `FloImg-extrakt.zip` (Ganzdurchläufe · Mediendiagnosen) auseinandernehmen
- **Status:** **erledigt** 2026-09-16 (Begutachtung; der EINBAU ist ein
  eigener Posten) · MF-1202 · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „nimm den code komplett auseinander , sehr genau / finde
  alles und alles raussuchen was ich übersehen habe , stimme es mit mein
  aktuellen tool ab / - wo können die formate verbessert werden / - ist es
  auf andere formate übertragbar / - welche einstellungen fehlen noch / -
  was habe wir noch nicht / - brauch es eine HAL-Erweiterungen plan
  verstanden, was kannst du besser machen ??"
- **Kennzahl:** **keine der vier für die Begutachtung.** Ganzdurchläufe und
  Mediendiagnosen sind keine Tier-Stufe und kein Wandlungspfad. **Ein
  Einbau könnte allerdings P3-284 abtragen** („`adaptive_passes` war ein
  Schalter ohne Schaltung — und der forensische Bericht meldete ihn als
  ‚yes'"), und das ist Rücknahme einer Falschaussage, also die Richtung von
  MF-1077.
- **Kanal:** **Spec.** Die Belegkette der Zulieferung nennt zwei Quellen:
  **FloImg 1.02** (Petari, 8bitchip.info, Hilfe zu v1.02 vom 2011-08-08) für
  „Ganzdurchläufe bringen mehr als sofortige Wiederholungen", und
  `NFORMAT.DOC` (1992) plus cw2dmk `jv3.h` (GPL-2, nur gelesen) für die
  Spurkapazität — **letzteres ist dieselbe Quelle, an der MF-1166 und
  MF-1183 hängen**. Gemessen: **keine `LICENSE`-Datei** im Paket, aber
  `SPDX` steht in `uft_os_volume.c`. Die Paketlizenz ist damit ungeklärt,
  die Dateilizenz nicht.
- **Einfrier-Regel:** **ja.** `src/hal/` und `src/analysis/` grenzen an den
  Decoder-Layer, und `uft_os_volume.c` ist lebender Code aus MF-1176.
  Rotbeweis zuerst.
- **OPEN_ITEMS:** **viele Treffer, und sie sind der Maßstab.** `P3-284`
  (`adaptive_passes` ohne Schaltung), `P3-113` (`distinct_contents` zählte
  nicht, was sein Name sagt — behoben MF-860, `classify_passes()`),
  `P3-88` (Multi-Read-Voting konnte einen Sektor erfinden — behoben
  MF-845), `P3-291` (leere Sektoren werden gezählt, nicht gewählt),
  `P3-429` (HAL-Merkmalstafel ohne Leser).
- **Bei der Aufnahme gemessen — vier Dinge, zwei davon entscheiden schon:**
  **(1)** Das Paket **erweitert**, statt zu ersetzen:
  `include/uft/hal/uft_os_volume.h` Baum 8 145 / Paket 9 625 (**+1 480**),
  `src/hal/uft_os_volume.c` Baum 10 557 / Paket 13 984 (**+3 427**). Das
  ist das **Gegenteil** des A-008/A-009-Musters und gehört gelobt.
  **(2)** Die Wiederholungsinfrastruktur ist **da**: `retries` in **46**
  `.c` und **23** `.h`, `multiread_execute` 7 `.c`, `distinct_contents`
  3 `.c`, `classify_passes` 2 `.c`, `adaptive_passes` 2 `.c`. Ein
  „`--passes` neben `--retries`" trifft also auf einen bestellten Tisch.
  **(3) Und hier liegt ein harter Konflikt: `--passes` ist ein
  CLI-Schalter, und dieses Projekt hat kein CLI.** Gemessen: 0 Dateien
  passen auf `cli\.c|main_cli|uft_cli`, und `CLAUDE.md` sagt „Dies ist ein
  **GUI-only-Projekt**. Es gibt keinen CLI-Modus mehr." Der Vorschlag
  gehört also an die Oberfläche oder an einen Optionssatz, nicht an eine
  Kommandozeile — das ist bei der Begutachtung zu entscheiden, nicht zu
  übernehmen. **(4)** Die Zulieferung stellt selbst eine Frage, die ich
  beantworten kann: „hat das `FloppyDevice`-Subsystem überhaupt einen
  Verteiler?" Sie zählt 79 Dateien, ich zähle **84** über
  `git ls-files | xargs grep -l 'uft_floppy_device.h'` — und der
  Gedächtnisstand `arch_formate_8bit` sagt, **alle** externen Aufrufer
  seien Tests. Die Antwort lautet damit wahrscheinlich „nein", und sie
  gehört gemessen statt zitiert.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** jede
  öffentliche Funktion der vier Paketdateien einzeln gegen den Baum hält —
  „gibt es / gibt es anders / gibt es nicht", je mit `git ls-files`-Messung,
  weil der Auftrag ausdrücklich „komplett auseinander, sehr genau" und
  „stimme es mit mein aktuellen tool ab" sagt; **(b)** für `uft_os_volume`
  einen **Zeilendiff** gegen den lebenden Stand aus MF-1176 nennt, nicht
  eine Dateiübernahme; **(c)** die fünf Fragen je mit Messung beantwortet;
  **(d)** den CLI-Konflikt ausdrücklich entscheidet; **(e)** die
  Paketlizenz als offene Frage stehen lässt; und **(f)** kein Byte des
  Pakets nach `src/`, `include/` oder `tests/` schreibt.
- **Aufwand:** Begutachtung eine Sitzung (279 Zeilen Bericht + 4
  Quelldateien + 1 Test + 2 Dateidiffs gegen lebenden Code). Einbau
  **nicht schätzbar** — er hängt am CLI-Konflikt und an P3-284.
- **Stand:** **Gutachten geschrieben**,
  `tools/uft-scout/out/a010_floimg_passes_diagnosen.gutachten.md`, (a)…(f).
  **Die beste der vier heute begutachteten Zulieferungen**, und das ist
  gemessen, nicht gefühlt:
  · **(a) Jede der 14 öffentlichen Funktionen einzeln, `rc` je Lauf:**
    **10 gibt es** (`uft_osvol_open/close/read/write/query_geometry/
    lba_to_chs/total_sectors/image_size/status_summary`, `uft_osvol_t` —
    alle aus MF-1176), **4 gibt es nicht** (`uft_mdiag_diagnose/report/
    writable`, `uft_mdiag_result_t`, je rc 1 / 0 Treffer). Eine Abweichung:
    `uft_osvol_image_size` steht im Header und im Test, **nicht** in
    `src/hal/uft_os_volume.c`.
  · **(b) Zeilendiff:** `.h` **+29/−3** (2 Blöcke), `.c` **+120/−44**
    (5 Blöcke). **Die 44 entfernten Zeilen sind eine UMSCHREIBUNG von
    `xfer_range()`, keine Rücknahme** — anders als bei A-008 und A-009
    geht dabei nichts Gemessenes verloren. **ABI richtig gelöst, gemessen:**
    `UFT_OSVOL_SEC_OK_LATE_PASS` ist **angehängt**, die vier vorhandenen
    Zustände behalten ihre Zahl.
  · **Der inhaltliche Kern trifft `P3-284`.** Nachgemessen mit
    Kommentarfilter: `adaptive_passes` hat **eine** Deklaration
    (`uft_multiread_pipeline.h:280`), **eine** Zuweisung
    (`uft_multiread_pipeline.c:93`) und **zwei Kommentare** — **null
    lesende Stellen**. Der Begriff „Ganzdurchlauf" aus dieser Zulieferung
    wäre der erste Ort im Baum, an dem er etwas bedeutet.
  · **(d) Der CLI-Konflikt entfällt** — gemessen trägt von den vier
    Quelldateien nur `test_passes.c` ein `int main`, und das ist ein Test.
  · **(e) Die Lizenz ist NICHT offen** — alle vier Dateien tragen
    `SPDX-License-Identifier: GPL-2.0-or-later`, die Projektlizenz. Offen
    bleibt allein, ob die FloImg-Aussage („Ganzdurchläufe bringen mehr als
    sofortige Wiederholungen") belegt ist; sie ist die Behauptung eines
    Werkzeugautors von 2011 und im Baum **nirgends nachgemessen**.
  · **(f)** kein Byte nach `src/`, `include/`, `tests/`.
- **Beleg:** Gutachten
  `tools/uft-scout/out/a010_floimg_passes_diagnosen.gutachten.md`.

### A-011 · Zulieferung `Metadaten.zip` (Herkunft · Datierung über die VSN) begutachten
- **Status:** **erledigt** 2026-09-16 (Begutachtung; der EINBAU ist ein
  eigener Posten) · MF-1202 · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „arbeite alles sehr genau aus / finde alles und alles
  raussuchen was ich übersehen habe / - wo können die formate verbessert
  werden / - ist es auf andere formate übertragbar / - welche einstellungen
  fehlen noch / - was habe wir noch nicht / - brauch es eine
  HAL-Erweiterungen"
- **Kennzahl:** **keine der vier.** Metadaten und Datierung sind keine
  Tier-Stufe und kein Wandlungspfad. **Aber es berührt die fünfte Zahl, die
  MF-640 offengelassen hat** („Dateien mit ungeklärter Herkunft") — und
  zwar von der anderen Seite: hier geht es um die Herkunft des
  **Datenträgers**, nicht der Quelldatei. Dieselbe Verschränkung wie
  `A-007`; welche Zahl das ist, gehört zur Antwort.
- **Kanal:** **Spec, und die Belegkette ist die beste dieser Reihe.** Für
  die VSN-Formel `(Sek<<8|Hundertstel)+(Monat<<8|Tag)`,
  `(Std<<8|Min)+Jahr`: Craig Wilson, *Volume Serial Numbers and Format
  Date/Time Verification*, digital-detective.net, plus Ralf Brown. Dazu ein
  **veröffentlichter Prüfvektor** (19.10.2003 22:33:27.01 → `2514-1DF4`),
  den die Zulieferung nachgerechnet hat — „beide Wörter treffen". Weitere
  Aussagen gegen MAME `imd_dsk.cpp`, a8rawconv `rawdiskscp.cpp:380-424`
  und den **eigenen Baum** (`uft_2img.c:6-21`, `uft_scp_writer.c:292`).
  Gemessen: `SPDX` steht in beiden Quelldateien, **keine `LICENSE`-Datei**
  im Paket — wie bei `A-006`.
- **Einfrier-Regel:** **ja, mittelbar.** `src/analysis/` grenzt an den
  Decoder-Layer, und die VSN-Deutung ist eine Aussage über ein
  Dateisystemfeld. Rotbeweis zuerst — und der Prüfvektor **ist** einer,
  weil er von außen kommt.
- **OPEN_ITEMS:** **`P3-387`** ist der nächste Nachbar: „Zwei forensische
  Fähigkeiten, die der Baum nicht hat, und die seine eigene Mission
  verlangt" — darunter „`uft_format_mark_last_missing()` kennzeichnet im
  SPEICHER, und keine Datei trägt es hinaus — der Befund existiert im Lauf
  und überlebt ihn nicht". Das ist dieselbe Klasse wie „gelesen und
  weggeworfen". Dazu `P3-354` (WOZ-CRC stellt eine Beschädigung fest und
  hat keinen Weg, sie zu melden).
- **Bei der Aufnahme gemessen — vier Dinge:** **(1) Die saubersten
  Zulieferungsform bisher:** alle fünf Dateien sind **neu**, keine
  Überlappung mit dem Baum, **keine** mitgelieferten Baudateien. Kein
  Diff gegen lebenden Code nötig. **(2) Die tragende Behauptung trifft
  präzise zu:** „`metadata_count` in keiner `.c` außer MOOFs eigener
  Struktur" — gemessen ist `metadata_count` in **genau einer** `.c`-Datei
  genannt, `src/formats/apple/uft_moof_parser.c`. **(3) VSN als Begriff
  fehlt im Baum vollständig:** der einzige Treffer auf `VSN` ist
  `resload_VSNPrintF` in `src/whdload/`, also unverwandt. `volume_serial`
  wird dagegen gelesen (3 `.c` / 8 `.h`) und über `fat_format_serial()`
  nur **angezeigt** (`%04X-%04X`) — gelesen, nicht gedeutet. **(4) Die
  Zahl „drei unvereinbare Metadatenmodelle" ist womöglich zu niedrig:**
  gemessen nennen mindestens **acht** Header „metadata", darunter
  `uft_disk.h`, `uft_format_registry.h`, `uft_snapshot.h` und
  `uft_flux_meta.h`. Das gehört nachgezählt, nicht übernommen.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** jede der fünf
  Fragen mit einer Messung gegen den Baum beantwortet; **(b)** den
  veröffentlichten Prüfvektor **selbst nachrechnet** statt die Nachrechnung
  zu zitieren — das ist der einzige Rotbeweis, der hier von außen kommt;
  **(c)** die offene Bytefolge-Frage („die VSN-Bytefolge im Sektor ist aus
  den Quellen nicht zweifelsfrei zu entnehmen") als offen stehen lässt und
  messbar macht, statt sie zu entscheiden; **(d)** die Zahl der
  Metadatenmodelle nachzählt; **(e)** die Paketlizenz als offene Frage
  nennt; und **(f)** kein Byte des Pakets nach `src/`, `include/` oder
  `tests/` schreibt.
- **Aufwand:** Begutachtung eine Sitzung (487 Zeilen Bericht + 4
  Quelldateien + 1 Test, alle neu). Einbau **nicht schätzbar** — er hängt
  an der Bytefolge-Frage und daran, ob ein Träger mit bekanntem
  Formatierungsdatum beschafft werden kann (die Zulieferung nennt das
  ausdrücklich als „nicht verifiziert").
- **Stand:** **Gutachten geschrieben**,
  `tools/uft-scout/out/a011_metadaten_vsn_datierung.gutachten.md`, (a)…(f).
  · **(b) Der Prüfvektor ist SELBST nachgerechnet und trifft.**
    `19.10.2003 22:33:27.01 -> 2514-1DF4`, gerechnet statt im Kopf:
    `sekunde*256+hundertstel` = 6913 = `0x1B01`, `monat*256+tag` = 2579 =
    `0x0A13`, **lo = 9492 = `0x2514`**; `stunde*256+minute` = 5665 =
    `0x1621`, `jahr` = 2003 = `0x07D3`, **hi = 7668 = `0x1DF4`**. Beide
    Wörter und alle vier Zwischenwerte treffen. Das ist der einzige
    Rotbeweis dieser Reihe, der von außen kommt.
  · **(c) Die Bytefolge-Frage bleibt offen — ist aber jetzt ENTSCHEIDBAR.**
    Gemessen liest `src/fs/uft_fat12.c:221` `le32(d + 0x27)` (die
    Zulieferung nennt Zeile 191; die Zeile stimmt, die Nummer nicht). Aus
    `25 14 1D F4` wird als `le32` **`0xF41D1425`**, als zwei BE-Wörter mit
    lo zuerst **`0x1DF42514`**. **Der Prüfstein liegt schon im Baum:**
    `tests/test_win98_fdb.c:279` hält `0x27156C21` aus einem ECHTEN
    Win98-Abbild fest — wer die vier Rohbytes daneben legt, hat die
    Bytefolge gemessen statt zitiert. Zweite Probe: ein Zeitstempel muss
    sich in ein plausibles Datum zurückrechnen lassen.
  · **(d) Zahl der Metadatenmodelle: ZWÖLF.** `volume_serial` ist als
    Strukturfeld in zwölf Headern deklariert (`mfm_detect.h`,
    `uft_fat_bootsector.h` dreimal, `formats/uft_fat12.h`, `uft_fdi.h`,
    `fs/uft_fat12.h`, `fs/uft_fat32.h` zweimal, `uft/uft_fat12.h`,
    `uft_xdf_pxdf.h` zweimal) — **plus eine dreizehnte Stelle mit einem
    ANDEREN Namen**, `src/fs/uft_fat12.c:221` (`v->serial`). Das ist
    `MF-1177` in der Metadatenschicht.
  · **(e) Die Lizenz ist NICHT offen** — alle vier Quelldateien tragen
    `SPDX-License-Identifier: GPL-2.0-or-later`.
  · **Neu im Baum:** `uft_vsn_compute`, `uft_vsn_from_bytes` je rc 1 /
    0 Treffer. **Der Baum speichert die VSN und deutet sie nicht.**
  · **(f)** kein Byte nach `src/`, `include/`, `tests/`.
- **Beleg:** Gutachten
  `tools/uft-scout/out/a011_metadaten_vsn_datierung.gutachten.md`.
- **Stand:** —
- **Beleg:** —

### A-012 · `yas-sim/xm7-related-tools` auseinandernehmen (FM-7: D77 · T77 · FM-Dateisystem · Boot-ROM)
- **Status:** **erledigt** 2026-09-16 (Begutachtung; der EINBAU ist ein
  eigener Posten) · MF-1203 · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „https://github.com/yas-sim/xm7-related-tools.git nimm den
  code komplett auseinander , sehr genau / finde alles und alles raussuchen
  was ich übersehen habe , stimme es mit mein aktuellen tool ab / - wo
  können die formate verbessert werden / - ist es auf andere formate
  übertragbar / - welche einstellungen fehlen noch / - was habe wir noch
  nicht / - brauch es eine HAL-Erweiterungen"
- **Kennzahl:** **keine der vier für die Begutachtung** — aber dieser Fund
  zielt auf die Achse, die `P3-389` als **die dünnste des Baums** benennt:
  „Die DEKODIERKETTE ist die duennste Achse des Baums … und der FM-Weg hat
  bis heute keinen ERZEUGER." Ein FM-Erzeuger wäre kein Kennzahlwert,
  sondern das **Werkzeug**, mit dem FM-Formate überhaupt eine Prüfspur
  bekommen (MF-864 musste dafür `fluxtoimd` von fremder Hand nehmen).
- **Kanal:** **Port zulässig — und das ist die Ausnahme in dieser Reihe.**
  Gemessen über `gh api`: `license.spdx_id` = **MIT**, 925 KB, C++,
  7 Sterne, letzter Push **2022-09-21**, Standardzweig `main`. Damit ist
  der stärkste Kanal aus MF-695 offen. **Zwei Vorbehalte gehören dazu:**
  die 16 `.zip`-Dateien enthalten vorgebaute Windows-Binärdateien (Kanal
  *Oracle*, nicht *Port*), und für **Prüfdaten** gilt weiter, was
  `SCOUT-5` festgehalten hat — „das Repo ist MIT, der Disketten-INHALT
  hat eigene Urheber".
- **Einfrier-Regel:** **ja.** Jeder Einbau berührt den Format- oder
  Decoder-Layer. Rotbeweis zuerst, benannte Referenz im Header.
- **OPEN_ITEMS:** **drei Treffer, und der erste ist derselbe Autor.**
  **`P3-389`** entstand auf genau diese Eigentümer-Frage („finde was ich
  uebersehen habe") zu `yas-sim/fdc_bitstream` — Behälter und Dateisystem
  wurden bewertet, die **Dekodierkette fehlte in der Bewertung**, und die
  Messung MF-1121 ergab: kein FM-Erzeuger. **`SCOUT-5`** nennt ein
  D77-Wahrheitspaar beim Upstream (`2019FM77AVDemo-4MHz.raw`, `-8MHz.raw`,
  `2019FM77AVDemo.d77`), **blockiert bis Lizenzklärung**. Dazu `P3-218`
  („kein FM-Encoder, deshalb die Fremdabnahme durch `fluxtoimd`").
- **Bei der Aufnahme gemessen — fünf Dinge:** **(1) Es ist ein echtes
  Quellrepo:** 183 Blobs, davon **70 Quelldateien** (42 `.cpp`, 25 `.h`,
  3 `.c`), dazu `CMakeLists.txt`, 9 ROM-Listings (`.lst`), 7
  Assemblerquellen (`.s`/`.src`) — und 16 `.zip` mit Binärdateien.
  **22 Werkzeuge** auf oberster Ebene, darunter `d77enc_dec`, `d77uty`,
  `fmtools` (mit `fmfslib/cfilesys.cpp`), `t77dec`, `t772wav`, `wav2t77`,
  `fdump`, `BootROM`. **(2) `T77` fehlt im Baum vollständig:** 0 Dateien
  für `t77`/`T77`. Gemessen über zitierte Endungen in `.c`-Dateien führt
  der Baum an Bandformaten nur `"cas"` (2×) und `"tzx"` (1×) — die Methode
  ist eng, aber der T77-Nullwert ist eindeutig. **(3) Kein
  FM-7-Dateisystemeintrag** in `docs/VERIFICATION_TIERS_FS.md`.
  **(4) `d77` steht heute auf T1b** mit `test_d77_gegen_hxcfe`,
  `test_d88_header_variants` und `test_oeffentliche_api_am_korpus`, Quelle
  pc98.org — **`SCOUT-5`s Messung „T3 ohne alles" ist damit veraltet**, die
  Lizenzsperre bleibt. **(5) Und der eigentliche Grund, hinzusehen:** das
  Repo enthält **Erzeuger** — `wav2t77` schreibt Bandaudio, `d77enc`
  schreibt D77 —, und P3-389s Befund lautet, dass dem FM-Weg genau das
  fehlt. Ob einer davon **der** fehlende FM-Erzeuger ist, ist die erste zu
  klärende Frage, nicht ihre Antwort.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** die 70
  Quelldateien nach Werkzeug gruppiert durchgeht und je Werkzeug „für UFT
  brauchbar / Oracle-Kandidat / uninteressant" mit Grund vergibt — der
  Auftrag sagt „komplett auseinander, sehr genau"; **(b)** die Frage von
  P3-389 ausdrücklich beantwortet: liefert dieses Repo einen FM-**Erzeuger**,
  und wenn ja, für welche Kodierung; **(c)** je Fund den Kanal nennt (Port
  bei MIT-Quelle, Oracle bei Binärdatei, Spec bei ROM-Listing) und die
  Lizenzfrage der **Prüfdaten** getrennt davon offen lässt (SCOUT-5);
  **(d)** die fünf Fragen je mit Messung gegen den Baum beantwortet;
  **(e)** `T77` und das FM-Dateisystem als gemessene Lücken belegt statt
  behauptet; und **(f)** kein Byte nach `src/`, `include/` oder `tests/`
  schreibt — geklont wird nach `tools/uft-scout/work/`, dem dafür
  vorgesehenen Ort.
- **Aufwand:** Begutachtung **mehr als eine Sitzung** — 70 Quelldateien in
  22 Werkzeugen sind mehr als jede bisherige Zulieferung dieser Reihe
  (A-010 hatte 4). Genauer nicht schätzbar. Einbau **nicht schätzbar** und
  in jedem Fall ein eigener Posten.
- **Stand:** **Gutachten geschrieben**,
  `tools/uft-scout/out/a012_xm7_related_tools.gutachten.md`, (a)…(e).
  Repo flach geklont nach `tools/uft-scout/work/xm7-related-tools`
  (gitignoriert); **70 Quelldateien gemessen — exakt die Zahl der
  Aufnahme**, 20 Werkzeugverzeichnisse (die Aufnahme sagte 22; gemessen
  sind es 20, drei davon ohne Quelltext), 8 020 Zeilen.
  · **(b) Die erste Frage ist mit NEIN beantwortet, und zwar gemessen:
    das Repo liefert KEINEN FM-Erzeuger.** `P3-389` meint die
    FM-**Kodierung**; über alle 70 Quelldateien gibt es dafür **zwei**
    Treffer, und beide sind **Kommentare** zu einem Dichte-Byte im
    D77-Behälter (`fmfslib/cfloppy.h:17`, `d77img.h:21`). `wav2t77` ist
    eine **Audio**-Kette (AGC, Komparator, Filter, Tiefpass) — Band-FSK,
    nicht Diskettenfluss. P3-389 bleibt offen.
  · **(a) Je Werkzeug ein Urteil:** **6 brauchbar oder Oracle-Kandidat**
    (`fmtools` 3460 Z., `wav2t77` 920, `t772wav` 519, `d77uty` 341,
    `t77dec` 332, `d77enc_dec` 322), **11 uninteressant** (2 419 Zeilen
    S-Record-/ROM-/Grafikwerkzeuge ohne Diskettenbezug), **3 ohne
    Quelltext** (`BootROM`, `nosys_ipl`, `subtfr` — Kanal *Spec*).
  · **(c) Kanal: PORT zulässig** — `LICENSE.md` ist **MIT, © 2022
    Yasunori Shimura**; MIT in GPL-2-or-later ist permissiv in Copyleft.
    **Einzige Quelle dieser Reihe, bei der ein Port offensteht.** Gemessen
    trägt allerdings **0** der 70 Quelldateien eine eigene SPDX-Zeile.
  · **(d/e) Zwei gemessene Lücken:** das **FM-7-Dateisystem**
    (`fmtools/fmfslib`) — der Baum kennt FM-7 als Plattform (8 Treffer)
    und D77 als Behälter, „DISK BASIC" trifft aber nur **MSX**; und
    **T77**, im Baum **rc 1 / 0 Treffer**, vollständig abwesend. Ob T77
    fehlen SOLL, ist eine Eigentümer-Frage — `SCOPE_DECISION_NON_FLOPPY.md`
    hat Nicht-Disketten-Inhalte 2026-05-25 gelöscht, während CLAUDE.md drei
    Bandformate als unterstützt führt.
  · **Bemerkenswert:** für das FM-7-Dateisystem wäre die Verifikationskette
    **vollständig führbar** — MIT-Quelle als Referenz, `d77enc_dec` als
    Erzeuger, `d77` bereits auf T1b. Das ist selten.
- **Beleg:** Gutachten
  `tools/uft-scout/out/a012_xm7_related_tools.gutachten.md`.
- **Beleg:** —

### A-013 · `thomas-luebker/AmigaDiskKit` auseinandernehmen — **das ist `P3-385(b)`**
- **Status:** **erledigt** 2026-09-16 (Begutachtung; Port bleibt gesperrt,
  Orakel ist delegierbar) · MF-1203 · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „https://github.com/thomas-luebker/AmigaDiskKit.git nimm den
  code komplett auseinander , sehr genau / finde alles und alles raussuchen
  was ich übersehen habe , stimme es mit mein aktuellen tool ab / - wo
  können die formate verbessert werden / - ist es auf andere formate
  übertragbar / - welche einstellungen fehlen noch / - was habe wir noch
  nicht / - brauch es eine HAL-Erweiterungen"
- **Kennzahl:** **keine der vier für die Begutachtung.** Der Punkt, den es
  abtragen könnte, ist `P3-385` — die „ADFlib-unabhängige Zweitmeinung, die
  `docs/ORACLES.md:650` selbst als ‚Offene Lücke' benennt". Ein
  unabhängiges Orakel bewegt keine der vier direkt, ist aber die
  Voraussetzung für Stufenhebungen im Amiga-Zweig.
- **Kanal:** **`P3-385` hat das Urteil bereits gefällt, und ich habe es
  nachgemessen — es gilt, aber es ist nicht ausführbar.** Der Punkt sagt
  wörtlich: „**Lizenz berichtigt:** Apache-2.0, NICHT MIT — mit GPL-2
  unverträglich, also **kein Port**, nur Orakel." Gemessen bestätigt:
  `license.spdx_id` = **Apache-2.0**. **Der offene Widerspruch:** `swift`,
  `swiftc` und `xcodebuild` sind auf diesem Rechner **alle drei nicht
  vorhanden**, und `docs/ORACLES.md` verlangt „Kein Oracle auf Zusicherung
  — ein Werkzeug, das nicht gebaut und ausgeführt wurde, ist kein
  Eintrag". Der einzige Kanal, den P3-385 offenlässt, ist damit hier
  **verschlossen** — nicht grundsätzlich, aber auf dieser Maschine. Das ist
  dieselbe Lage wie die Tier-3-Hardwarebank (MF-310): delegierbar, nicht
  hausintern. Was bleibt: **Spec** (lesen) und **Daten** (die 26 `.bin`).
  Für Apache-2.0 gibt es dabei **zwei entschiedene Präzedenzfälle** —
  `P3-316` ✅ MF-1008 und `P3-353` ✅, beide vom Eigentümer festgestellt.
- **Einfrier-Regel:** **ja** für jeden Einbau; für das Lesen nicht.
- **OPEN_ITEMS:** **`P3-385`** — dieser Posten IST der Auftrag, ihn
  abzutragen; der Befund bleibt dort und wird hier nicht abgeschrieben.
  Berührt `P3-316`/`P3-353` (Apache-Präzedenz) und die Amiga-FS-Stufen.
- **Bei der Aufnahme gemessen — fünf Dinge:** **(1) Die Zahl von P3-385
  stimmt:** **139 Blobs**, genau wie dort notiert; davon **97 `.swift`**,
  26 `.bin`, 11 `.txt`. Oberste Ebene `Sources`, `Tests`, `Package.swift`
  — eine Swift-Paketstruktur, kein Werkzeugkasten. **(2) Der Umfang ist
  breiter als „Diskette":** die Beschreibung nennt „RDB/MBR layouts,
  FFS/OFS/FFS2, PFS3, FAT32, ADF & flux floppies, LHA" und ausdrücklich
  „No external tools or Python". **(3) Vier dieser Achsen fehlen dem Baum
  gemessen:** `rdb` 1 Datei / `RDB` 0 / `rigid_disk` 0, `pfs3`/`PFS3`
  **0**, `ffs2`/`FFS2` **0**, `lha` 0 / `LHA` 1. **(4) Die Amiga-Seite des
  Baums ist ungleich belegt:** `uft_amigados` steht auf **FS-T2**,
  `uft_amigados_extended`, `uft_bootblock_scanner` und
  `uft_amiga_virus_db` auf FS-T1 — und `uft_fs_amigados_driver` auf
  **FS-T0** mit der Bemerkung „kein Test nennt ein Symbol dieses Lesers".
  **(5) Es ist aktiv:** letzter Push **2026-07-29**, nicht archiviert —
  anders als der Nachbar `amigadx` (letzter Commit 2014-07-18, GPL,
  vendort ADFlib 0.7.10), der laut P3-385 gerade **nicht** unabhängig ist.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** die 97
  Swift-Dateien nach Achse gruppiert durchgeht (RDB/MBR · FFS/OFS/FFS2 ·
  PFS3 · FAT32 · ADF · Fluss · LHA) und je Achse „fehlt im Baum / ist da
  und schwächer / ist da und gleichwertig" mit `git ls-files`-Messung
  vergibt — der Auftrag sagt „komplett auseinander, sehr genau" und
  „stimme es mit mein aktuellen tool ab"; **(b)** die Orakel-Frage
  ausdrücklich entscheidet: ohne Swift-Werkzeugkette ist kein Eintrag nach
  `docs/ORACLES.md` möglich, also ist zu sagen, **was genau delegiert
  werden müsste** (Bau auf welcher Plattform, welche Ausgabe wäre der
  Beleg); **(c)** die 26 `.bin` als möglichen **Daten**-Kanal prüft und
  ihre Herkunft/Lizenz getrennt von der Apache-Frage nennt (die Lage aus
  `SCOUT-5`: Repolizenz ≠ Disketteninhalt); **(d)** die fünf Fragen je mit
  Messung beantwortet; **(e)** `P3-385` mit dem Ergebnis fortschreibt statt
  einen neuen Punkt anzulegen; und **(f)** kein Byte nach `src/`,
  `include/` oder `tests/` schreibt — geklont wird nach
  `tools/uft-scout/work/`.
- **Aufwand:** Begutachtung **mehr als eine Sitzung** — 97 Swift-Dateien
  in einer Sprache, für die hier keine Werkzeugkette existiert, also
  reines Lesen ohne Ausführen. Genauer nicht schätzbar. Einbau
  **entfällt** unter dem heutigen Lizenzurteil; was bliebe, wäre ein
  **Nachbau** nach `docs/QUARANTINE_PROCESS.md` §5 — und das ist ein
  eigener Posten mit eigener Eigentümer-Entscheidung.
- **Stand:** **Gutachten geschrieben**,
  `tools/uft-scout/out/a013_amigadiskkit.gutachten.md`, (a)…(f). Repo flach
  geklont nach `tools/uft-scout/work/AmigaDiskKit`; **97 `.swift`, 26
  `.bin`, 11 `.txt` — genau die Zahlen aus `P3-385`**, Apache-2.0
  bestätigt.
  · **(e) DREI ZAHLEN VON `P3-385` SIND FALSCH**, case-sensitive
    nachgemessen mit `rc` je Lauf: `RDB` **11 Treffer in 2 Dateien** statt
    0 — und **umgesetzt**, nicht erwähnt (`uft_hdf_parse_rdb()`,
    `calc_rdb_checksum()`, BE-Felder bei Versatz 128/132, Schranke gegen
    „malformed RDB"); `FFS2` **1** statt 0, aber nur als Kommentarzeile;
    `LHA` **8** statt 1. Richtig bleiben `rigid_disk` 0 und `PFS3` 0.
  · **(a) Vierzehn Achsen je mit Urteil.** Die eine, die dem Baum wirklich
    fehlt, ist **`PFS3`** — 6 Dateien, **3207 Zeilen**, die größte
    Einzelachse des Repos, im Baum **rc 1 / 0 Treffer**. Gleichwertig sind
    Floppy/ADF, FAT, MBR, ImageIO; schwächer sind RDB, Preview/IFF — und
    **LHA**.
  · **LHA ist der schärfste Einzelbefund:** der Baum trägt eine Format-ID
    (`UFT_FMT_LHA = 203`), drei Funktionsdeklarationen — und
    `src/fs/uft_amigados_extended.c:683` „LHA Archive - Stubs" mit **drei
    Rümpfen `return -1; /* Not implemented */`**. Klasse `P3-204`. Das
    fremde Repo hat 1797 Zeilen davon, und Apache-2.0 verbietet den Port;
    LHA ist aber dokumentiert, also wäre es ein **Nachbau**, kein Port.
  · **(b) Die Orakel-Frage ist entschieden: auf dieser Maschine
    verschlossen.** Keine Swift-Werkzeugkette, und `docs/ORACLES.md`
    verlangt Bau UND Ausführung. Delegierbar wie die Tier-3-Bank (MF-310).
    Was delegiert werden müsste, steht im Gutachten §4 — inklusive der
    Gegenprobe auf **ADFlib-Unabhängigkeit**, ohne die auch ein
    erfolgreicher Bau nichts belegt (MF-1033).
  · **(c) Daten-Kanal: 26 Dateien, 5 097 472 Byte**, gezielte Ausschnitte
    ECHTER Aufbauten (`classic-8g`, `mister-8g`, `pistorm-29g`, `hst-`,
    `pfs3-2g/8g`) — RDB-Bereiche, Bootblöcke, FAT32-Systembereiche.
    Ausgezeichnetes Fixture-Material; **Herkunft je Datei fehlt**, und die
    Repolizenz deckt den Disketteninhalt nicht (`SCOUT-5`).
  · **(f)** kein Byte nach `src/`, `include/`, `tests/`.
- **Beleg:** Gutachten
  `tools/uft-scout/out/a013_amigadiskkit.gutachten.md`.

### A-014 · `SecurityRonin/disk-forensic` auseinandernehmen — und die Vorfrage lautet: ist das Diskettenarbeit?
- **Status:** **erledigt** 2026-09-16 (Begutachtung; Urteil **Fundus** —
  die Vorfrage ist mit Nein beantwortet) · MF-1204 · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „https://github.com/SecurityRonin/disk-forensic.git nimm den
  code komplett auseinander , sehr genau / finde alles und alles raussuchen
  was ich übersehen habe , stimme es mit mein aktuellen tool ab / - wo
  können die formate verbessert werden / - ist es auf andere formate
  übertragbar / - welche einstellungen fehlen noch / - was habe wir noch
  nicht / - brauch es eine HAL-Erweiterungen"
- **Kennzahl:** **keine der vier.** Und hier ist die Zeile ausnahmsweise
  ein echter Einwand, nicht nur Buchhaltung: **der Gegenstand ist keine
  Diskette.** E01, VMDK, VHDX, VHD, QCOW2 und DMG sind Festplatten- und
  VM-Behälter, MBR/GPT/APM sind Partitionsschemata für Medien, die größer
  sind als eine Diskette, und ISO 9660 ist optisch. Eine 3,5-Zoll-Diskette
  hat keine Partitionstabelle. **Genau ein Teil trägt** — siehe die
  Aufnahme-Messung.
- **Kanal:** **Spec, und mehr ist hier nicht zu holen.** Gemessen:
  `license.spdx_id` = **Apache-2.0**, Sprache **Rust**, 574 KB, angelegt
  2026-06-05, letzter Push 2026-08-26, 4 Sterne, kein Fork. Apache-2.0
  gegen GPL-2 ist unverträglich — dieselbe Lage wie `A-013`/`P3-385`, und
  auch hier gibt es die zwei Präzedenzfälle `P3-316` ✅ und `P3-353` ✅.
  **Dazu verschlossen: `cargo` und `rustc` sind auf diesem Rechner nicht
  vorhanden**, also ist auch *Orakel* nach `docs/ORACLES.md` hausintern
  unmöglich („Kein Oracle auf Zusicherung"). Es bleibt **Lesen**.
- **Einfrier-Regel:** **nein für das Lesen.** Ein Einbau wäre eine neue
  Behälterschicht und fiele voll unter das Moratorium — als *neues Format*,
  nicht als Bugfix.
- **OPEN_ITEMS:** **`P3-387`** ist der tragende Anker — „Zwei forensische
  Fähigkeiten, die der Baum nicht hat, und die seine eigene Mission
  verlangt: ein stückweiser Hash und ein Fehlerprotokoll, das den Lauf
  überlebt." Dazu `P3-380` (ATX kann exportieren, niemand bietet es an),
  `P3-381` (darf ein Schreiber die Identitätsdatei überschreiben) und
  `KI-6.1` (keine CI-Prüfung durch echte Emulatoren).
- **Bei der Aufnahme gemessen — und ein Fund ist nicht offensichtlich:**
  **(1) Nichts davon ist im Baum.** Fünfzehn Begriffe gesucht, alle **0
  Dateien**: `E01`, `ewf`/`EWF`, `vmdk`, `vhdx`, `qcow`, `dmg`,
  `iso9660`, `GPT`, `APM`. **Mit einer Ausnahme:** `mbr` hat **2 Dateien**
  — `include/uft/formats/uft_fat32_mbr.h` und
  `src/formats/fat32/uft_fat32_mbr.c` —, und `partition` hat 0. MBR ist
  also punktuell da, als FAT32-Anhang, nicht als Achse.
  **(2) Der eine Teil, der trägt, ist `E01` — und zwar als SCHREIBZIEL,
  nicht als Leser.** Das EnCase Expert Witness Format führt eine **CRC je
  32-KiB-Block** und einen Bereichsvermerk für fehlerhafte Sektoren. Das
  ist wörtlich, was `P3-387` als fehlend benennt: „ein SHA-256 je
  (Zylinder, Kopf) statt einer Summe über die ganze Datei" und „`uft_format
  _mark_last_missing()` kennzeichnet im SPEICHER, und keine Datei trägt es
  hinaus". **E01 ist der bestehende Industriestandard für genau diese zwei
  Fähigkeiten** — und der Wert dieses Repos liegt darin, seinen Aufbau zu
  zeigen, nicht darin, VM-Behälter zu lesen. **(3) Die übrigen sieben
  Behälter sind für dieses Werkzeug Fundus**, nicht Auftrag: sie bewegen
  keine Kennzahl und gehören nicht zur Mission. Das ist eine Einordnung,
  keine Abwertung des Repos.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** die Vorfrage
  ausdrücklich beantwortet — welche der acht Behälter und drei
  Partitionsschemata für ein **Disketten**werkzeug überhaupt in Betracht
  kommen, je mit Begründung, und welche als Fundus notiert werden;
  **(b)** für `E01` den Aufbau aus dem Rust-Code **liest** und daraus
  benennt, was ein UFT-Schreiber bräuchte (Blockgröße, CRC-Stelle,
  Fehlerbereichs-Satz, Fallmetadaten) — als **Spec**, ohne eine Zeile zu
  übernehmen, und mit dem Hinweis, dass die kanonische Quelle
  libewf/ASR-Dokumentation ist und nicht dieses Repo; **(c)** die Brücke
  zu `P3-387` herstellt oder begründet verwirft; **(d)** die fünf Fragen je
  mit Messung beantwortet; **(e)** festhält, dass `cargo`/`rustc` fehlen
  und ein Orakel damit delegiert werden müsste — samt der Angabe, was
  genau zu bauen und welche Ausgabe der Beleg wäre; und **(f)** kein Byte
  nach `src/`, `include/` oder `tests/` schreibt.
- **Aufwand:** Begutachtung **eine Sitzung, wenn sie sich auf `E01`
  beschränkt** — genau das ist zu entscheiden, nicht zu unterstellen. Eine
  Durcharbeitung aller acht Behälter ist **nicht schätzbar** und wäre nach
  MF-640 Fundus. Einbau **entfällt** unter dem heutigen Lizenzurteil; ein
  E01-Schreiber wäre ein **Nachbau** nach benannter Spezifikation und ein
  eigener Posten mit eigener Eigentümer-Entscheidung.
- **Stand:** **Gutachten geschrieben**,
  `tools/uft-scout/out/a014_disk_forensic.gutachten.md`, (a)…(e).
  · **(b) DIE ANNAHME DIESER AUFNAHME FÄLLT — und das ist der Befund.**
    Sie setzte darauf, dass das Repo den **E01-Aufbau zeigt**. Gemessen
    über alle **2 146** Zeilen: **es implementiert E01 überhaupt nicht.**
    Es vergleicht die Kennung (`ewf::EVF1_SIGNATURE`, `EVF2`, `LEF2`) und
    ruft dann `::ewf::EwfReader::open(path)` — eine **externe Kiste**. Der
    Quelltext sagt es selbst an `container.rs:123-124`: „the decoder is the
    external `ewf` crate". **Keine Blockgröße, keine CRC-Stelle, kein
    Fehlerbereichs-Satz, keine Fallmetadaten.** Als *Spec*-Quelle für E01
    taugt es nicht; die kanonische Beschreibung liegt bei **libewf** bzw.
    der ASR-Dokumentation — was die Aufnahme zur Hälfte selbst
    vorweggenommen hatte.
  · **(a) Die Vorfrage ist mit NEIN beantwortet.** Von zehn Gegenständen
    ist keiner Diskettenarbeit: VMDK/VHDX/VHD sind VM-Behälter, DMG ist
    macOS, ISO 9660 optisch, GPT/APM Festplatten-Partitionsschemata. **Eine
    3,5-Zoll-Diskette hat keine Partitionstabelle** — und GPT (48), MBR
    (40) und APM (25) sind mit 113 Nennungen der größte Einzelblock des
    Repos. Einzige Berührung ist MBR, und dort ist der Baum punktuell
    versorgt (`uft_fat32_mbr.h/.c`).
  · **Eine weitere Zahl der Aufnahme fällt:** sie zählte acht Behälter
    auf, darunter **QCOW2** — gemessen **0 Treffer** im Quelltext. Das Repo
    behandelt die Behälter nicht, es **erkennt und delegiert**; einzige
    eigene Umsetzung ist `vhd.rs` (244 Z.).
  · **(c) Die Brücke zu `P3-387` ist begründet VERWORFEN**, nicht
    hergestellt: sie führt richtig über E01 (CRC je Block,
    Bereichsvermerk für fehlerhafte Sektoren) — nur nicht über dieses Repo.
  · **(e) Orakel verschlossen** (kein `cargo`/`rustc`) und **es lohnt
    nicht**: ein Orakel ohne gemeinsame Gegenstände vergleicht nichts.
  · **Urteil: FUNDUS.** Kein Port (Apache-2.0), kein Orakel, kein
    Spec-Gewinn.
- **Beleg:** Gutachten
  `tools/uft-scout/out/a014_disk_forensic.gutachten.md`.
- **Stand:** —
- **Beleg:** —

### A-015 · `programandala-net/mkmgt` auseinandernehmen — MGT steht auf T1, die Frage ist Beta DOS
- **Status:** **erledigt** 2026-09-16 (Begutachtung; **zwei neue Befunde**,
  `P3-465` und `P3-466`) · MF-1205 · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „https://github.com/programandala-net/mkmgt.git nimm den
  code komplett auseinander , sehr genau / finde alles und alles raussuchen
  was ich übersehen habe , stimme es mit mein aktuellen tool ab / - wo
  können die formate verbessert werden / - ist es auf andere formate
  übertragbar / - welche einstellungen fehlen noch / - was habe wir noch
  nicht / - brauch es eine HAL-Erweiterungen"
- **Kennzahl:** **keine der vier, und hier ist der Grund ungewöhnlich
  günstig: der Behälter ist schon fertig.** Gemessen steht `mgt` auf
  **T1** — die höchste Stufe — mit **sechs** Tests
  (`test_durchschreibprobe`, `test_kopflose_sonden_sind_erreichbar`,
  `test_mgt_gegen_mame`, `test_mgt_schreibt_in_die_datei`,
  `test_mgt_verzeichnis_vollstaendig`, `test_oeffentliche_api_am_korpus`).
  **Damit korrigiere ich meine eigene Erwartung bei der Aufnahme:** ich
  hatte MF-1006 im Kopf („`mgt` von T3 auf T2, Feldabgleich gegen MAMEs
  `coupedsk.cpp`, und er fand KEINEN Fehler") und daraus geschlossen, ein
  fremder **Schreiber** würde die Stufe heben. Er würde sie nicht heben —
  sie ist schon oben.
- **Kanal:** **Spec.** Gemessen: `license.spdx_id` = **GPL-3.0**, Sprache
  **Forth**, 40 KB, letzter Push **2020-05-14**, 2 Sterne. GPL-3 ist mit
  diesem Baum verträglich (er trägt GPL-3-Bestandteile), ein *Port* wäre
  also rechtlich offen — aber **Forth nach C ist eine Neuschreibung, kein
  Port**. Und *Orakel* ist hausintern verschlossen: `gforth`, `pforth`,
  `sf` und `swiftforth` sind **alle vier nicht vorhanden**. Es bleibt
  Lesen. **Das ist die vierte fehlende Werkzeugkette in Folge** —
  `mkfs.fat`/`mtools` (A-005), Swift (A-013), Rust (A-014), Forth (hier).
- **Einfrier-Regel:** **nein für das Lesen.** Ein Beta-DOS-Leser wäre eine
  neue Dateisystemvariante und fiele unter das Moratorium.
- **OPEN_ITEMS:** kein Treffer zu `mkmgt`, `GDOS` oder `Beta DOS`. Der
  Nachbar ist die Namensrolle/Variantenfrage: `mgt` ist ein **Behälter**,
  und was darin liegt, ist GDOS, G+DOS oder Beta DOS — die Achse, für die
  `uft-variants` zuständig ist („wo sagen wir etwas Falsches, ohne dass es
  auffällt?").
- **Bei der Aufnahme gemessen — und ein Wert bleibt:** **(1) Der Behälter
  ist belegt, das Dateisystem nicht vollständig.** Das Repo nennt
  ausdrücklich **drei** DOSe: „ZX Spectrum GDOS, G+DOS and Beta DOS".
  Gemessen im Baum: `DISCiPLE` steht in **5** Dateien, darunter
  `include/uft/formats/uft_mgt.h`, `src/formats/mgt/uft_mgt.c`,
  `include/uft/uft_format_plugin.h`, `include/uft/xdf/uft_xdf_zxdf.h` und
  `src/analysis/profiles/uft_profile_uk.c` — der Baum kennt also die
  DISCiPLE/+D-Herkunft. **`gdos` und `plusd` stehen dagegen nur in zwei
  TESTS** (`test_kopflose_sonden_sind_erreichbar.c`,
  `test_zx_und_pc98_echte_abbilder.c`), sehr wahrscheinlich als
  Korpus-Dateinamen und nicht als unterschiedene Dateisysteme.
  **`Beta DOS`/`BetaDOS`/`betados`: 0 Treffer, überall.** Das ist die
  Lücke, und sie ist klein und benennbar. **(2) Zwei MGT-Leser liegen
  schon im Baum:** `src/formats/mgt/uft_mgt.c` und `src/samdisk/mgt.cpp`.
  Ein dritter wäre die Lage aus P3-147 (G64 hat drei Leser), nicht ein
  Zugewinn. **(3) Eine Messung ist unbrauchbar und wird nicht verwendet:**
  `grep -F "+D"` meldete 12 Dateien, trifft aber jedes `x+D` in
  C-Ausdrücken. Für „+D" gibt es damit **keine** belastbare Zahl in dieser
  Aufnahme.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** ausdrücklich
  feststellt, was ein Schreiber für `mgt` **nicht** mehr leisten kann,
  weil das Format auf T1 steht — und damit die Erwartung korrigiert, statt
  sie zu bedienen; **(b)** die drei DOSe **einzeln** gegen den Baum hält:
  liest UFT GDOS, G+DOS und Beta DOS, oder nimmt es eines an und schweigt?
  Je mit `git ls-files`-Messung und, wo möglich, an einem Korpus-Abbild;
  **(c)** die Forth-Quelle als **Spec** liest — Verzeichnisaufbau,
  Sektorbelegung, Namensfelder — und daraus einen Prüfauftrag formt statt
  eines Ports; **(d)** die fünf Fragen je mit Messung beantwortet;
  **(e)** die vierte fehlende Werkzeugkette in Folge als eigenen
  Vorschlag für `docs/OPEN_ITEMS.md` benennt, weil ein Orakel-Kanal, der
  regelmäßig an der Werkzeugkette scheitert, eine Entscheidung braucht
  (delegieren wie die Tier-3-Bank, oder die Kanalregel präzisieren); und
  **(f)** kein Byte nach `src/`, `include/` oder `tests/` schreibt.
- **Aufwand:** Begutachtung **eine Sitzung** — 40 KB Forth sind der
  kleinste Gegenstand dieser Reihe, und die Frage ist eng (drei DOSe, ein
  Behälter, der schon belegt ist). Einbau: für Beta DOS **nicht
  schätzbar** und ein eigener Posten unter dem Moratorium.
- **Stand:** **Gutachten geschrieben**,
  `tools/uft-scout/out/a015_mkmgt.gutachten.md` — **und der Posten hat
  etwas gefunden, wonach er nicht gesucht hat.**
  · **(b) Die gestellte Frage, beantwortet:** UFT benennt **keines** der
    drei DOSe im Quelltext. `GDOS`, `gdos`, `G+DOS`, `BetaDOS`,
    `Beta DOS`, `betados`, `plusd` — **alle sieben rc 1 / 0 Treffer** in
    `src/` und `include/`; nur `DISCiPLE` hat 9 Treffer in 5 Dateien. Der
    Baum kennt die **Hardware**, nicht das Dateisystem. Steht als
    `P3-466`.
  · **DER EIGENTLICHE FUND — `mgt_dir_entry_t` ist ab Versatz 11 um ein
    Byte verschoben, und `mgt` steht auf T1.** Der Schreiber `mkmgt.fs`
    kommentiert jede Position: Sektorzahl **16 Bit Big-Endian auf 11-12**,
    Spur auf **13**, Sektor auf **14**, Karte auf **15-209**, GDOS-Kopf auf
    **210-219**. UFT führt `uint8_t sectors_used` (11), `track` (12),
    `sector` (13), `sector_map[195]` (14-208). **An einem ECHTEN
    Korpus-Abbild gemessen** (`zxfd_plusd_gdos_tools.mgt`): alle vier
    geprüften Einträge melden nach UFTs Lesart `sectors_used = **0**` —
    eine vorhandene Datei mit null Sektoren gibt es nicht. Dazu geht die
    Feldsumme nicht auf: **255 gegen `MGT_DIR_ENTRY_SIZE` 256**. Steht als
    `P3-465`.
  · **Warum keiner der sechs Tests anschlägt, ist mitgemessen:**
    `test_mgt_verzeichnis_vollstaendig` baut seine Daten selbst und prüft
    nur Versatz 0 und 1-10 — genau die zwei Felder, die stimmen; die vier
    verschobenen fasst er nie an. `test_mgt_gegen_mame` (MF-1006) verglich
    den **Behälter**. **Die T1-Stufe ist korrekt vergeben und sagt über das
    Verzeichnis nichts.**
  · **(a) Ein Schreiber hebt bei `mgt` nichts** — das Format steht auf T1.
    Und `mkmgt` wäre ohnehin verschlossen: Forth, und `gforth`, `pforth`,
    `sf`, `swiftforth` fehlen alle vier. **Vierte fehlende Werkzeugkette in
    Folge** nach A-005 (mkfs.fat/mtools), A-013 (Swift), A-014 (Rust).
  · **Urteil: REFERENCE.** Aus dem Repo ist nichts zu übernehmen — sein
    Wert waren die Feldlagen, und die haben einen Defekt aufgedeckt.
- **Beleg:** Gutachten `tools/uft-scout/out/a015_mkmgt.gutachten.md`;
  Befunde `P3-465` und `P3-466`.

### A-016 · `ChrisBertrandDotNet/ST-Recover` — **das ist `P3-63`**, und `P3-59` hat seine Vorbedingung umgekehrt
- **Status:** **Gutachten erledigt** 2026-09-16, **Erweiterung benannt und
  begründet zurückgestellt** · MF-1206 · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „https://github.com/ChrisBertrandDotNet/ST-Recover.git nimm
  den code komplett auseinander , sehr genau / finde alles und alles
  raussuchen was ich übersehen habe , stimme es mit mein aktuellen tool ab
  / - wo können die formate verbessert werden / - ist es auf andere formate
  übertragbar / - welche einstellungen fehlen noch / - was habe wir noch
  nicht / - brauch es eine HAL-Erweiterungen , **erweite das tool**, plan
  verstanden, was kannst du besser machen ??"
- **Kennzahl:** **keine der vier.** Beide Größen aus `P3-63` sind
  forensische Aussagen je Sektor, keine Tier-Stufe und kein
  Wandlungspfad — sie berühren die fünfte, offene Zahl (MF-640) und
  `P3-387` (ein Fehlerprotokoll, das den Lauf überlebt).
- **Kanal:** **Spec — und hier hilft `P3-452` ausdrücklich NICHT.** Gemessen:
  das Repo führt `Source/Ms-RL License.htm` und `License.htm`, also die
  **Microsoft Reciprocal License**; `gh api` liefert `NOASSERTION`, weil
  GitHub sie nicht einordnet. **Ms-RL ist mit GPL in JEDER Fassung
  unverträglich** — anders als Apache-2.0, das nur gegen GPL-2 scheitert.
  Die GPL-3-Bindung aus MF-698 öffnet den Port-Kanal hier also nicht.
  Was bleibt: Werte lesen, keine Zeilen nehmen — und **das ist schon
  geschehen**: `P3-63` führt vier belegte Bestätigungen aus dieser Quelle
  (32 µs je Rohbyte aus `200000/6250`, „11 Sektoren/Spur in 2 Umdrehungen",
  das 50-Byte-Fenster zwischen ID- und Datenmarke, und `128 << (n & 3)`).
- **Einfrier-Regel:** **ja.** Beide Größen hängen an `uft_sector_t`, der
  kanonischen Struktur aus `include/uft/uft_types.h` („This is the ONE
  definition … used across the entire project"). Rotbeweis zuerst, und eine
  ABI-Frage steht daneben.
- **OPEN_ITEMS:** **`P3-63`** — dieser Posten IST der Auftrag, ihn
  abzutragen; der Befund bleibt dort. Vorbedingung war `P3-59`, dazu
  `P3-387` (Fehlerprotokoll mit Position), `P3-51(1)` (die 11 Sektoren) und
  `P3-453` (Schreibnaht — siehe unten).
- **Bei der Aufnahme gemessen — und die Vorbedingung ist der eigentliche
  Befund:** `P3-63` sagt „die Reihenfolge steht: **nach P3-59**".
  **`P3-59` ist ✅ erledigt (MF-832) — aber seine Auflösung war eine
  Umkehrung:** „Vier weitere Felder auf vier tote zu setzen macht die
  Struktur **irreführender**, nicht reicher." Gemessen hatte MF-831, dass
  von den fünf Positionsfeldern auf `uft_sector_t` **genau eines** je
  gefüllt wird — `angular_position`, von `uft_atx.c:366`, dem einzigen
  Format, das es kann. `id_offset` hat **eine** Fundstelle im ganzen Baum,
  nämlich die Deklaration; `gap_before` einen Treffer auf einer **fremden**
  Struktur; `data_offset` und `bit_offset` haben 44 bzw. 14 Schreibstellen,
  die **alle** anderen Strukturen gehören. Dazu heute nachgemessen:
  `trouve_par_le_controleur`, `gap_us` und `duree_espace` haben je **0**
  Treffer. **`P3-63` ist damit in der Reihenfolge frei und in der Sache
  blockiert:** seine zwei Größen wären das fünfte und sechste tote Feld.
  Der Punkt sagt das selbst — „die zwei Groessen setzen gefuellte
  Feldgrenzen voraus; heute gibt es keinen Erzeuger dafuer".
- **Und eine Verbindung, die den Weg abkürzt:** `duree_espace_libre_en_1er`
  ist die **Lücke als Messgröße in Mikrosekunden statt als Restmenge** —
  und genau die entsteht bei `P3-453`/Phase 2 des laufenden Plans
  (Schreibnaht auf IBM-MFM **aus den Lückenwerten**). Beide Punkte wollen
  dieselbe Messung von zwei Seiten. Wer Phase 2 baut, erzeugt die Hälfte
  von `P3-63` mit.
- **Fertig heißt:** ein Gutachten **plus** die Erweiterung, in dieser
  Reihenfolge, weil der Auftrag „erweite das tool" sagt:
  **(a)** die 13 Quelldateien durchgehen und je Fund „schon in `P3-63`
  genannt / neu / trifft nicht zu" vergeben — `P3-63` ist vom Eigentümer
  gelesen, ein zweites Lesen muss also **etwas Neues** liefern oder das
  sagen; **(b)** die Ms-RL-Grenze einhalten: Werte und Verfahren
  beschreiben, **keine Zeile übernehmen**, und im Header benennen;
  **(c)** die Vorbedingung ausdrücklich entscheiden — einen **Erzeuger** für
  die Feldgrenzen bauen (dann sind die zwei Größen sinnvoll) oder die zwei
  Größen zurückstellen, mit Begründung; **(d)** wenn gebaut wird: Rotbeweis
  zuerst, ein Aufrufer im selben Commit (D2), und die ABI-Frage zu
  `uft_sector_t` benannt; **(e)** die fünf Fragen je mit Messung;
  **(f)** kein Byte des Repos nach `src/`, `include/` oder `tests/`.
- **Aufwand:** Begutachtung **eine Sitzung** (13 Quelldateien, 127 KB, und
  `P3-63` liefert die Vorarbeit). Die **Erweiterung** ist nicht schätzbar —
  sie hängt daran, ob ein Erzeuger für die Feldgrenzen entsteht, und das
  ist Decoder-Arbeit am Spurmodell, nicht ein Feld anhängen.
- **Stand:** **Gutachten geschrieben**,
  `tools/uft-scout/out/a016_st_recover.gutachten.md`, (a)…(f).
  · **(c) DIE VORBEDINGUNG IST GEFALLEN — durch eigene Arbeit, seit der
    Aufnahme.** `P3-63` war blockiert, weil seine zwei Größen „gefüllte
    Feldgrenzen voraussetzen" und es „heute keinen Erzeuger dafür" gebe.
    **Seit MF-1190 gibt es ihn:** `uft_mfm_sector_t` trägt `id_sync_bit`,
    `data_start_bit`, `gap2` und `lead_gap` — im **Produktivpfad**
    `uft_mfm_decode_track()`. Die Aufnahme hatte es fast gesehen („wer
    Phase 2 baut, erzeugt die Hälfte von P3-63 mit"); gebaut ist Phase 2
    als MF-1190 = `e22e1b78`, und sie hat **beide** Hälften gebracht.
  · **Entscheidung: nicht zurückgestellt, sondern benannt.** Gebaut wird
    hier nicht, und der Grund ist die Bauform: die Größen hängen an
    `uft_sector_t`, der kanonischen Struktur — das ist eine **ABI-Frage**,
    braucht Rotbeweis-zuerst und einen Aufrufer im selben Commit. Die
    Gestalt des Auftrags steht im Gutachten §1.4 und als Fortschreibung an
    `P3-63` selbst, samt der Lösung, die der Baum sich bereits notiert hat
    (`uft_types.h:397`: Flag `has_bit_positions` nach dem Muster von
    `has_angular_position`).
  · **(a) Ein zweites Lesen liefert nichts Neues von Gewicht** — und das
    ist die ehrliche Antwort. 13 Dateien, 3 222 Zeilen; `P3-63` hat die
    Quelle bereits ausgewertet (vier belegte Werte). Neu sind allein
    `Analyse_disque.*` (138 Z.), und das ist **Lesestrategie**, kein
    Formatwissen — es berührt A-010.
  · **(b) Ms-RL ist härter als Apache-2.0:** unverträglich mit GPL in
    **jeder** Fassung, `P3-452` hilft hier ausdrücklich nicht. Das
    Gutachten zitiert keine Zeile Quelltext.
  · **(f)** kein Byte nach `src/`, `include/`, `tests/`.
- **Beleg:** Gutachten `tools/uft-scout/out/a016_st_recover.gutachten.md`;
  `P3-63` fortgeschrieben (MF-1206).

### A-018 · Zulieferung `Apple DOS.zip` — DOS-3.3-Dateisystem + BASIC-Detokenisierer
- **Status:** **angehalten am gemessenen Blocker** (das Orakel ist da, ein
  echtes Abbild fehlt) · MF-1207 · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „\"C:\Users\Axel\Github\UnifiedFloppyTool-4.1.0\neue-ideen\Apple
  DOS.zip\" finde was ich vergessen habe und verbessere damit das tool"
- **Kennzahl:** **keine der vier.** Die FS-Achse ist keine der vier
  Release-Zahlen (MF-640). Der Wert des Postens ist, dass er `P3-384`
  abträgt — nicht eine Zahl.
- **Kanal:** **Port.** Gemessen: alle fünf Quellen tragen
  `SPDX-License-Identifier: GPL-2.0-or-later`, also eigener Code unter der
  Projektlizenz — keine Lizenzfrage (dieselbe Messung wie MF-1188).
  **Aber `P3-384` stellt eine eigene Bedingung, und sie gilt vor:** sein
  Status sagt „erst Orakel, dann Port" und nennt `catseye/a2tools` (GPL-2,
  837 Zeilen) als fremde Hand, samt Vormessung — kann `a2tools` ein
  LEERES DOS-3.3-Abbild anlegen oder nur ein vorhandenes befüllen? Eine
  zweite Hand desselben Hauses ersetzt das nicht; das ist die Gestalt von
  `apridisk` (MF-1009) und `qrst` (MF-1028), wo Packer und Entpacker
  Spiegelbilder derselben Erfindung waren und der Rundlauf grün.
- **Einfrier-Regel:** **ja** → Rotbeweis zuerst. Es ist die FS-Ebene, kein
  neues Format-Plugin, also greift das Moratorium nicht; die drei
  Bedingungen aus `docs/VERIFICATION_PLAN.md` greifen sehr wohl.
- **OPEN_ITEMS:** **`P3-384`** (wörtlich: „Apple DOS 3.3 fehlt in der
  Dateisystem-Stufenleiter VOLLSTÄNDIG") · berührt `P3-317` (ProDOS-
  Verzeichnisleser fehlt), `P3-228`, `P3-235`
- **Fertig heißt:** ein DOS-3.3-Leser mit Stufe in
  `docs/VERIFICATION_TIERS_FS.md`, die Stufe gegen eine **fremde Hand**
  belegt (nicht gegen den eigenen Erzeuger), Produktivaufrufer im selben
  Commit (D2), Vollsuite grün.
- **Aufwand:** **nicht schätzbar** — er hängt an der Orakel-Vormessung
  oben, nicht am Umfang der Zulieferung.
- **Gemessene Lage im Baum (damit die Lücke belegt ist, nicht behauptet):**
  Apple liegt ausschließlich auf der Behälterachse —
  `src/formats/apple/prodos_po_do.c`, 130 Zeilen, null Verzeichnisbezug
  (MF-710 hat es deshalb aus der FS-Tafel genommen). `applesoft` hat **0**
  Treffer, `track_sector_list` **0**, `TSLIST` **0**; `detokenize` gibt es
  genau **zweimal** und beide Male für **ZX**-BASIC
  (`src/formats/zx/uft_zxbasic.c`). Die 21 `VTOC`-Dateien sind
  **Atari**-VTOC. Zulieferung: 5 Quellen, **1450** Zeilen (268/340 FS,
  112/278 BASIC, 452 Test), **71** `CHECK`-Zusagen; der Bericht hat §0
  „was ich nicht gebaut habe, und warum" und §6 „Belegkette", seine zwei
  Funde sind §2.3 gelöschte Einträge und §2.4 T/S-Listen mit Löchern.
- **Stand:** **ANGEHALTEN, und der Halt ist das Ergebnis** — die eine
  Vorbedingung ist **erfüllt**, die andere ist **gemessen unerfüllbar.**
  · **Das Orakel ist da, und das ist neu.** `P3-384` verlangt „erst Orakel,
    dann Port" und nennt `catseye/a2tools` (GPL-2, 837 Zeilen). Gemessen:
    es **baut und läuft auf dieser Maschine** —
    `gcc -O2 -DDOS -o a2tools_dos.exe a2tools.c` unter MinGW 13.1.0, und
    `a2tools_dos.exe dir <abbild>` antwortet mit seiner Hilfe. Die
    Verteilung geht über `argv[1]` (DOS-Fassung) statt über `argv[0]`
    (UNIX-Fassung, wo `a2ls.exe` ≠ `a2ls` scheitert). **Damit ist die
    erste Hälfte von `P3-384` erfüllt** — und sie war offen.
  · **ABER: es gibt nichts, was es lesen könnte.** Beide Korpusdateien mit
    „dos33" im Namen werden abgewiesen („Not an Apple DOS 3.3 .dsk
    image"), und der Grund ist gemessen: an der VTOC-Stelle (Spur 17,
    Sektor 0, Versatz `0x11000`) stehen in **beiden** die Bytes
    `55 46 54 2D 4B 20 54 31` = ASCII **„UFT-K T1"**. Es sind UFTs eigene
    Selbstbenennungs-Marken; die Dateien sind **Sektorordnungs-Prüfmuster
    ohne Dateisystem**. Das Manifest sagt es selbst: `"origin": "derived"`,
    `"tool": "UFT-eigen (MF-1050) — kein Fremdwerkzeug, daher KEIN
    Stufenkredit"`.
  · **Baumweit nachgemessen:** über **alle** `.do`, `.dsk` und `.po` unter
    `tests/` gibt es **0** Dateien mit einer gültigen DOS-3.3-VTOC
    (Prüfung: DOS-Fassung 3, 35 Spuren, 16 Sektoren, 256 Byte/Sektor).
  · **Folge für die Abschlussbedingung:** sie verlangt „die Stufe gegen
    eine **fremde Hand** belegt (nicht gegen den eigenen Erzeuger)". Ein
    DOS-3.3-Leser wäre baubar — **belegbar wäre er nicht**, und ein Leser
    mit selbstgebautem Prüfstück ist genau der geschlossene Kreis von
    `apridisk` (MF-1009) und `qrst` (MF-1028). Dazu die Lehre aus
    **MF-1021**: ein erzeugtes Fixture ist erst dann ein Beleg, wenn sein
    INHALT nachgewiesen ist.
  · **Der nächste Griff ist damit benannt und klein:** ein **echtes**
    DOS-3.3-Abbild beschaffen. `a2tools` kann **nicht formatieren** (es
    hat `dir`/`out`/`in`/`del`, kein `format`), also muss das Abbild von
    außen kommen — ein zeitgenössisches Objekt oder ein Emulator-Erzeugnis.
    Erst danach ist dieser Posten arbeitsfähig.
- **Beleg:** Halt gemessen MF-1207; Orakelbau
  `tools/uft-scout/work/a2tools/a2tools_dos.exe` (gitignoriert).

### A-019 · Zulieferung `UFT-NN — TR-DOS.zip` — drei behauptete Feldadressen, SCL, Hobeta
- **Status:** **angehalten mit Ergebnis** — alle drei Adressen stimmen
  überein, der Defekt ist seit MF-970 behoben · MF-1207 ·
  **Aufgenommen:** 2026-09-16
- **Wortlaut:** „\"C:\Users\Axel\Github\UnifiedFloppyTool-4.1.0\neue-ideen\UFT-NN
  — TR-DOS.zip\" finde was ich vergessen habe und verbessere damit das"
- **Kennzahl:** **keine der vier.** `trd` steht auf **T1b**, `scl` auf
  **T1** — eine Stufe bewegt sich nicht. Der Posten ist trotzdem Auftrag
  und nicht Fundus, weil ein falsch lesender Erkenner im **erreichbaren**
  Pfad die Missionszeile selbst trifft („keine stille Veränderung"); die
  EINFRIER-REGEL lässt Bugfixes an Bestehendem ausdrücklich zu.
- **Kanal:** **Port** — alle drei Quellen `GPL-2.0-or-later`.
- **Einfrier-Regel:** **ja** → Rotbeweis zuerst, und die drei Adressen
  werden **von Hand gegen die Quelle abgezählt**, bevor eine Zeile fällt.
  Der Grund steht in `P3-281`: der damalige Bericht derselben Familie
  meldete einen echten Fehler und **hätte einen neuen eingebaut** — er
  schlug `0xF4-0xF5` vor, also zwei Byte, und `0xF5` ist bereits das erste
  Zeichen von `disc_name[8]`.
- **OPEN_ITEMS:** Nachbarn **`P3-281`** (dieselbe Klasse, ✅ behoben
  MF-970, aber **andere Datei**: der verwaiste `uft_trd_parser_v2.c`) und
  **`P3-322`** (SCL-Rest). Ein **eigener** Eintrag wird geschrieben, wenn
  die Behauptung gemessen ist — nicht vorher. Diese Liste ist keine
  zweite Befundliste, und eine ungemessene Behauptung ist kein Befund.
- **Fertig heißt:** jede der drei Adressen einzeln gegen zwei
  unabhängige Quellen abgezählt, Rotbeweis pro Adresse, und der
  Widerspruch unten aufgelöst oder als S5 festgenagelt.
- **Aufwand:** **nicht schätzbar.**
- **Ein Widerspruch steht schon jetzt im Raum (S5-Gebiet):** MF-729 hat
  **alle** Sonden geeicht — auf einem Nullpuffer darf nichts ≥ 50 melden,
  wer 50–79 beansprucht, muss ≥ 95 % zufälliger Puffer abweisen — und
  `trd` ist dort namentlich als Gewinner gegen ein PC-160K-Abbild (82)
  gemessen. Liest dieselbe Sonde die falschen Bytes und besteht die
  Eichung dennoch, sagt eine der beiden Messungen nicht, was sie zu sagen
  scheint. Welche, entscheidet der Lauf, nicht der Bericht.
- **Gemessene Lage:** 13 TR-DOS/SCL-Dateien im Baum (zwei Parser, ein
  Plugin, `src/samdisk/trd.cpp`, ein Korpusabbild `gw_trd.img`, ein Test);
  `0x8E4` — die Dateizahl aus MF-1014 — steht in `uft_trd.c`,
  `uft_scl_plugin.c` und drei Tests. Zulieferung: 3 Quellen, **1148**
  Zeilen (306/387/455), **67** `CHECK`-Zusagen; der Bericht nimmt in §6
  eine eigene Behauptung **zurück**.
- **Stand:** **ANGEHALTEN — der gemeldete Defekt existiert nicht mehr, und
  das ist gemessen.** Alle drei behaupteten Feldadressen einzeln gegen den
  Baum abgezählt:
  | Feld | Zulieferung | `src/formats/trd/uft_trd_parser_v2.c` | |
  |---|---|---|---|
  | Dateizahl | `0xE4` | `info->file_count = sys[0xE4];` (:310) | **stimmt** |
  | freie Sektoren | `0xE5` | `sys[0xE5] \| (sys[0xE6] << 8)` (:311) | **stimmt** |
  | gelöschte Dateien | `0xF4`, EIN Byte | `info->deleted_files = sys[0xF4];` (:343) | **stimmt** |
  · **Und der Baum sagt es selbst:** Zeile 314 trägt den Vermerk
    „**MF-970: hier stand `sys[0xE9] | (sys[0xEA] << 8)`.**" — genau der
    Defekt, den die Zulieferung meldet, samt zitierter Rücknahme an Ort und
    Stelle. Das ist der **zweite** Fall dieser Art nach A-008 (dort
    MF-402): eine Zulieferung meldet richtig, was der Baum bereits behoben
    hat.
  · **Das registrierte Plugin liest die Felder gar nicht.**
    `src/formats/trd/uft_trd.c` — die einzige Datei mit
    Plugin-Registrierung — liest **nur** die Dateizahl bei `0x8E4`, und
    zwar für die Sonde. Weder `0xF4` noch `0xE9`/`0xEA` kommen dort vor.
    Die falsche Lesart konnte also seit MF-970 **nirgends** wirksam werden.
  · **Ein zweiter Befund fiel nebenbei an, und er ist derselbe wie bei
    A-018:** `tests/corpus_free/gw_trd.img` (655 360 B) hat **keinen
    echten TR-DOS-Infosektor**. Die Bytes `0x8E0..0x8FF` sind eine lineare
    Rampe mit Schrittweite `0x1F` (`58 77 96 B5 D4 F3 …`) — ein
    erzeugtes Muster auf Behälterebene, kein Dateisystem. Gemessen ergäbe
    es „212 Dateien", „4851 freie Sektoren" und „196 gelöschte" —
    allesamt unmöglich. **Zweites Abbild in Folge, das seinen Namen trägt
    und sein Dateisystem nicht** (MF-1021).
  · **Der Widerspruch aus der Aufnahme (S5-Gebiet) löst sich damit auf:**
    MF-729s Sondeneichung und die Feldlage können beide stimmen, weil die
    Sonde nur `0x8E4` liest und die übrigen Felder nie anfasst.
- **Beleg:** Halt gemessen MF-1207; keine Änderung am Baum nötig.

### A-020 · Zulieferung `UFT-NN — Amiga.zip` — Dateisystem-Diskette oder Trackloader mit DOS-Kopf?
- **Status:** **angehalten am Umfang** (14 Entscheidungsstellen; der Umbau
  überschreitet die Scope-Regel) · MF-1208 · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „\"C:\Users\Axel\Github\UnifiedFloppyTool-4.1.0\neue-ideen\UFT-NN
  — Amiga.zip\" finde was ich vergessen habe und verbessere damit das"
- **Kennzahl:** **keine der vier** (MF-640). Der Wert liegt in der
  Missionszeile: eine Diskette, die nur einen DOS-Kopf trägt, darf nicht
  als Dateisystem gemeldet werden — „unbekannt" ist nicht „widerspricht"
  (MF-980/D6).
- **Kanal:** **Port** — alle drei Quellen `GPL-2.0-or-later`; der Bericht
  hat dazu ein eigenes §6 „Lizenzlage".
- **Einfrier-Regel:** **ja** → Rotbeweis zuerst.
- **OPEN_ITEMS:** Nachbarn **`P3-209`** (AmigaDOS-Prüfsumme mehrfach),
  **`P3-210`** (FS-Treiber meldete für jedes ADF ein leeres Verzeichnis),
  **`P0-16`** (HFE→ADF liefert leere ADF), **`P3-385`** (ADFlib-
  unabhängige Zweitmeinung)
- **Fertig heißt:** die vier Zustände unterscheidbar **und** an einem
  Abbild belegt, das kein Dateisystem trägt; kein Zustand, der nur
  behauptet statt gemessen ist; Produktivaufrufer im selben Commit (D2).
- **Aufwand:** **nicht schätzbar.**
- **Gemessene Lage:** die Entscheidung „ist das eine AmigaDOS-Diskette"
  fällt heute an **12** Stellen — `git ls-files` findet 12 Dateien, die
  die `DOS`-Kennung prüfen, darunter `src/analysis/uft_triage.c`,
  `src/algorithms/advanced/uft_bayesian_detect.c`, drei ADF-Parser und
  `src/formats/misc/polyglot_boot.c`. **4** `.c` unter `src/` nennen eine
  AmigaDOS-Prüfsummenfunktion (`mfm_detect.c`, `uft_adf_parser_v3.c`,
  `uft_adf.c`, `src/fs/uft_amigados.c`), während `P3-209` „DREIFACH"
  führt — ob das eine Unterzählung ist oder drei verschiedene Funktionen
  mit ähnlichem Namen, entscheidet der Lauf; hier steht es als **Frage an
  `P3-209`**, nicht als Korrektur an ihm. FS-Stufen:
  `uft_amigados` FS-T2, `uft_amigados_extended` und
  `uft_bootblock_scanner` FS-T1 („alle Tests bauen ihre Eingabe selbst"),
  `uft_fs_amigados_driver` **FS-T0 — kein Test nennt ein Symbol dieses
  Lesers**. Zulieferung: 3 Quellen, **870** Zeilen (268/269/333), **37**
  `CHECK`-Zusagen; §1 heißt „Der Anlass: ein gemessener Gegenbeweis", und
  der wird nachgefahren, bevor er zitiert wird — bei A-006 fielen drei von
  vier meiner eigenen Erwartungen.
- **Stand:** **ANGEHALTEN am Umfang, nicht an der Sache — und die Lage ist
  besser als bei A-018/A-019.** Beide Belegstücke, die die
  Abschlussbedingung verlangt, liegen im Baum, gemessen am Wurzelblock
  (Block 880, Versatz `0x6E000`):
  | Abbild | Kennung | Wurzelblock | Urteil |
  |---|---|---|---|
  | `tests/corpus_free/xdftool_dd_ofs.adf` (901 120 B) | `DOS\0` | `type=2`, `sectype=1` | **echte AmigaDOS-OFS-Diskette, von fremder Hand** (`xdftool`) |
  | `tests/differential/corpus/sources/amiga_dd.adf` (901 120 B) | `00 21 A4 B6` | kein gültiger | **ADF OHNE Dateisystem**, 99,6 % der Bytes ungleich null — der Trackloader-Fall |
  | `dim_adfs_{d,e,f}.adf` | — | — | **Acorn ADFS**, nicht Amiga; gehören nicht hierher |
  · **Was fehlt, ist der dritte Fall:** ein ADF **mit** DOS-Kopf und
    **ohne** gültigen Wurzelblock. Der wäre aus `xdftool_dd_ofs.adf`
    herstellbar, aber dann selbstgebaut — und genau darüber wacht MF-1021.
  · **DER GRUND DES HALTS, gemessen:** die Entscheidung „ist das eine
    AmigaDOS-Diskette" fällt an **14 Stellen** (ohne Fremdcode und ohne
    `uft_hdf_parser`, der RDB prüft, nicht ADF): `uft_bayesian_detect.c`,
    `uft_triage.c`, `mfm_detect.c`, `disk_image_validator.cpp`,
    `forensictab.cpp`, drei ADF-Parser, `uft_adf_plugin.c`,
    `polyglot_boot.c`, `uft_adf.c`, `uft_format_validators.c`,
    `uft_format_versions.c`, `uft_adf_bam.c`, `uft_bootblock_scanner.c`.
    Die Aufnahme sagte 12; der Unterschied ist die Musterweite, nicht die
    Sache. **Das ist die größte Verdopplung dieser Sitzung** — größer als
    die zwölf `volume_serial`-Header aus A-011.
  · **Vier Zustände als 15. Stelle einzuführen wäre `MF-1177`**: eine
    Größe, fünfzehn Rechnungen. Richtig ist EINE Stelle, die die
    vierzehn rufen — und das ist ein Umbau über vierzehn Dateien, weit
    jenseits der Umfangsregel (`.claude/CLAUDE.md` §Scope: >150 Zeilen,
    mehrere Subsysteme → anhalten, größtes fertiges Teilstück liefern).
  · **Das größte fertige Teilstück ist damit die Messung selbst**, und sie
    steht als `P3-467`.
- **Beleg:** Halt gemessen MF-1208; Befund `P3-467`.

### A-021 · Zulieferung `UFT-NN — FAT12 lesen.zip` — FAT12 lesen, wenn die Diskette nicht mehr heil ist
- **Status:** aufgenommen · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „\"C:\Users\Axel\Github\UnifiedFloppyTool-4.1.0\neue-ideen\UFT-NN
  — FAT12 lesen.zip\"  finde was ich vergessen habe und verbessere damit das"
- **Kennzahl:** **keine der vier.** FAT12 ist Dateisystem-Ebene (FS-T1 laut
  `docs/VERIFICATION_TIERS_FS.md`), keine Formatstufe. Der Wert liegt in
  der Missionszeile: ein beschädigtes Abbild darf nicht erfundene Daten
  liefern.
- **Kanal:** **Port** — alle drei Quellen `SPDX: GPL-2.0-or-later`.
- **Einfrier-Regel:** **ja** → Rotbeweis zuerst.
- **OPEN_ITEMS:** `P3-15` (FAT12 hat mit `mformat` einen fremden Erzeuger)
  · `P3-62` (erster fremder Prüfvektor für die Nibbelentpackung)
  · `P3-142` (die FAT12/16-Grenze hat ZWEI hergeleitete Werte; dazu gibt
  es ein Tor, gemessen 0)
- **Fertig heißt:** jede neue Prüfung mit benannter Quelle im Header, die
  Kettenfälle (abgerissen, im Kreis) mit Rotbeweis, und **kein** Sektor,
  der erfundene Bytes als `UFT_SECTOR_OK` meldet.
- **Aufwand:** **nicht schätzbar.**
- **Die wichtigste Lage dazu, gemessen:** das ist die **direkte
  Fortsetzung von MF-1183**. Dessen §„Geometrie: BPB, Größe, geraten" ist
  genau der Dreizustand, den ich dort gebaut habe — BPB stimmt mit der
  Dateigröße → nehmen; BPB widerspricht → absagen (MF-1039/MF-1027); kein
  BPB → exakter Treffer in der benannten Achtzeilentafel, sonst absagen.
  Die „fünfte BPB-Prüfung" der Zulieferung gehört deshalb gegen die vier
  gehalten, die seit MF-1183 dort stehen, nicht gegen einen leeren Baum.
  Umfang: 5 Einträge, **966** Zeilen (232 h / 360 c / 374 Test), **51**
  `CHECK`-Zusagen; der Bericht hat §6 „was mir dabei passiert ist" und §9
  „was ich nicht gebaut habe".
- **Stand:** —
- **Beleg:** —

### A-022 · `Booyaka101/diskstack` auseinandernehmen — mehrere Abzüge EINER Diskette zusammenstimmen
- **Status:** aufgenommen · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „https://github.com/Booyaka101/diskstack.git nimm den code
  komplett auseinander , sehr genau / finde alles und alles raussuchen was
  ich übersehen habe  , stimme es mit mein aktuellen tool ab / - wo können
  die formate verbessert werden / - ist es auf andere formate übertragbar /
  - welche einstellungen fehlen noch / - was habe wir noch nicht / - brauch
  es eine HAL-Erweiterungen verbessere damit mein tool"
- **Kennzahl:** **keine der vier** unmittelbar. Mittelbar `leckende Tests
  null` unberührt; der eigentliche Bezug ist die **fünfte, offene Zahl**
  (MF-640) und `P3-387`.
- **Kanal:** **Oracle — und das ist stärker als der Port.** Gemessen über
  `gh api`: **MIT**, Python, 225 KB, 0 Sterne, letzter Push 2026-09-12,
  nicht archiviert. MIT erlaubt den Port, aber es ist **Python** und UFT
  ist C/Qt — ein Port wäre eine Neuschreibung. Ausgeführt ist es dagegen
  eine **fremde Hand**, die `.scp`, KryoFlux-`.raw`, `.hfe` und
  Sektorabbilder liest und sektorweise **abstimmt**: genau das, was
  `src/recovery/uft_multiread_pipeline.c` tut (MF-473).
- **Einfrier-Regel:** **ja**, sobald etwas im Decoder-/Recovery-Pfad
  entsteht → Rotbeweis zuerst. Die Begutachtung selbst ist
  Verifikationsarbeit.
- **OPEN_ITEMS:** `P3-387` (ein Verlustprotokoll, das den Lauf überlebt)
- **Fertig heißt:** ein Gutachten mit Lizenzurteil, Inventar-Abfrage und
  Differenzlauf-Plan; jeder Fund mit Kennzahl und Kanal; die Frage nach
  der HAL-Erweiterung **beantwortet**, nicht offen gelassen.
- **Aufwand:** **nicht schätzbar.**
- **Der Fund, der schon in der Beschreibung steht:** *„…and say what is
  still missing … prints the `gw read --tracks=` command for what is still
  bad."* Das ist ein Verlustbericht, der den **nächsten Befehl** nennt —
  und genau diese Gestalt hat `src/core/uft_loss_report.c` heute nicht.
  Der zweite Teil des Auftrags („wo können die Formate verbessert werden")
  trifft damit nicht die Formatschicht, sondern die Berichtsschicht.
- **Stand:** —
- **Beleg:** —

### A-023 · Zulieferung `Apple-Sektorordnung.zip` — der offene Punkt aus MF-714
- **Status:** aufgenommen · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „\"C:\Users\Axel\Github\UnifiedFloppyTool-4.1.0\neue-ideen\Apple-Sektorordnung.zip\"
  finde was ich vergessen habe und verbessere damit das"
- **Kennzahl:** **keine der vier** direkt; die Anordnungsachse ist
  Vorarbeit für Formatstufen (`do`/`po`/`d13` stehen auf T1b/T2).
- **Kanal:** **Port** — alle drei Quellen `GPL-2.0-or-later`.
- **Einfrier-Regel:** **ja** → Rotbeweis zuerst, und die Skew-Tafel gehört
  gegen die vorhandene Datenzeile gehalten, nicht daneben.
- **OPEN_ITEMS:** `P3-422` (`uft_track_layout` hat gemessen **keinen**
  Aufrufer) · berührt `P3-234` (die Apple-6-and-2-Tafel liegt
  **siebenfach**, und nur eine Fassung ist oracle-geprüft)
- **Fertig heißt:** die Ordnung steht als **eine** Datenzeile in
  `include/uft/core/uft_sector_order.h` (MF-1175) — **kein** siebter
  Apple-Tafel-Ort —, mit Produktivaufrufer im selben Commit (D2).
- **Aufwand:** **nicht schätzbar.**
- **Gemessene Lage:** 814 Zeilen (219 / 252 / 343), **48** `CHECK`. Der
  Titel sagt „gelöst (**teilweise**)", §5 heißt „und wo der Weg nicht
  trägt", §7 „was mir dabei passiert ist" — die Zulieferung benennt ihre
  eigenen Grenzen, und §3 („die Skew-Tabelle ist ihr eigenes Inverses")
  ist eine prüfbare Behauptung, die vor jeder Übernahme nachgerechnet
  wird.
- **Stand:** —
- **Beleg:** —

### A-024 · AUFTRAG Audit-Umfang: komplettes Repository statt CMake-Liste (Mengen A/B/C)
- **Status:** aufgenommen · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „Nur die Dateien aus den CMake-Listen reichen nicht. …
  Gerade die nicht in CMake stehenden Dateien sind beim UFT wichtig. Dort
  können fertige, aber unerreichbare Parser, alte Versionen, Stubs oder
  nicht verdrahtete Algorithmen liegen. … Danach wird in drei Mengen
  getrennt: A: Von CMake gebauter Produktcode / B: Nur in Tests, Tools
  oder Sonderkonfigurationen gebauter Code / C: Im Repository vorhandener,
  aber nicht erreichbarer Code … Menge C wird nicht automatisch
  repariert, sondern zuerst klassifiziert: REGISTER / MERGE / REFERENCE /
  PARTIAL / BLOCKED / DELETE … **Keine Datei allein deshalb löschen, weil
  sie nicht in CMake steht.** … Am Ende sollten drei getrennte Änderungen
  entstehen"
  <br>Ein Satz im Auftrag steht ohne Bezug und wird **zitiert, nicht
  ausgelegt**: „Damit würdest du das Repository ziemlich sicher
  beschädigen."
- **Kennzahl:** **keine der vier** — es ist die Verlässlichkeit des
  Prüfstands, wie `P3-421` und `P3-426`.
- **Kanal:** **entfällt** (eigener Baum, eigene Methode).
- **Einfrier-Regel:** **nein** für das Klassifizieren. **Ja**, sobald aus
  einem REGISTER ein registriertes Plugin würde — dann greift das
  Moratorium (1 neues Format = 2 Hebungen).
- **OPEN_ITEMS:** `P3-379` (Waisenrolle: Register statt Sweep) ·
  `P3-421` (`repo_scope` meldete einmal „git ls-files nicht verfügbar"
  und prüfte daraufhin den ganzen Verzeichnisbaum) · `P3-426` (zwölf
  GLOBs im Prüfstand)
- **Fertig heißt:** die drei Mengen **abgeleitet** erzeugt (nicht
  gepflegt), jede Datei der Menge C mit **einer** der sechs Dispositionen
  belegt, und drei getrennte Commits — wobei jede Löschung die
  `Ruecknahme:`-Zeile trägt, die der `commit-msg`-Haken erzwingt.
- **Aufwand:** **nicht schätzbar.**
- **Fünf Messungen, weil vier Teile der Vorgabe hier anders liegen:**
  · „nicht nur CMake, sondern `git ls-files`" ist **schon Grundsatz** —
    MF-636 mit `scripts/repo_scope.py` und **vier** belegten Vorfällen
    veralteter Aufzählungen (MF-567, MF-578, MF-598, MF-633).
  · `git submodule update --init --recursive` ist hier ein Leerlauf:
    **`.gitmodules` existiert nicht**.
  · `git lfs pull` ebenso: **0** `filter=lfs`-Zeilen in `.gitattributes`.
  · „~155 Einträge unter `src/formats/`" — gemessen sind es **354** `.c`
    und **11** `.h` (von **2889** versionierten Dateien im Baum). Woher
    die 155 stammt, ist **offen**; keine der beiden Zahlen wird benutzt,
    bevor das geklärt ist.
  · Menge C ist **teils schon da**: `docs/orphan_baseline.txt` (328
    Zeilen), `scripts/audit_orphan_modules.py`, die Waisenrolle. **Neu**
    sind die sechs Dispositionen und die drei getrennten Commits.
- **Und ein Punkt, der die empfohlene Vorgehensweise selbst betrifft:** ein
  frischer `git clone` von GitHub würde die laufende Arbeit **nicht**
  enthalten — gemessen liegen **9** Commits nur lokal (MF-1182 … MF-1190,
  C3). Ein Audit auf dem Klon liefe gegen einen Stand ohne Phase 1 und 2.
  Quelle ist deshalb der Arbeitsbaum, oder es wird vorher gepusht.
- **Stand:** —
- **Beleg:** —

### A-004 · AUFTRAG „UFT offene Punkte, autonome Abarbeitung" — Punkt 7
- **Status:** **angehalten am vereinbarten Schnitt** · **Aufgenommen:**
  2026-09-16 (nachgetragen) · verschoben ans Ende 2026-09-16
- **Wortlaut:** „danach die verbleibenden Punkte aus OPEN_ITEMS.md, nach
  demselben Muster." · zuletzt bestätigt mit „weiter mit punkt 7"; der
  Schnitt ist genehmigt mit „ja, schnitt bei P3-423 ist ok"
- **Kennzahl:** je Teilposten verschieden; die Punkte haben T3 und die
  Wandlungspfade nicht bewegt, sondern Falschaussagen behoben — die
  Kennzahl steht im jeweiligen Commit, nicht hier
- **Kanal:** entfällt (eigener Baum)
- **Einfrier-Regel:** ja für jeden Format-Teilposten → **Rotbeweis zuerst**
- **OPEN_ITEMS:** die Restliste; nach dem Schnitt offen: **P3-425**
  (dieselbe Diskette, zwei Geometrien — `xfd` liest die Quad Density als
  40×2, `atr` weiter als 80×1, und `atr`s Sonde gewinnt mit 95 gegen 40),
  **P3-426** (`tests/CMakeLists.txt` sammelt die Format-Schicht an zwölf
  Stellen mit je einem eigenen GLOB ein, elf davon halten eine
  Aufzählung), **P3-421 Teil 2** (die Rückfallebene von `repo_scope`
  betrifft 16 Skripte), die MF-1162-Punkte und die Eigentümerränge 4–10.
  Neu dazugekommen durch die Arbeit selbst: **P3-439**, **P3-440**,
  **P3-441**.
- **Fertig heißt:** kein Punkt aus der Restliste mehr ohne Commit **oder**
  ohne benannte Stoppbedingung (S1–S5) in `docs/OPEN_ITEMS.md`
- **Aufwand:** nicht schätzbar (Restliste offen)
- **Stand:** **Schnitt erreicht, zwei Commits lokal, nichts gepusht.**
  MF-1182 (P3-406, Sonden-Doktrin Regel 2) und MF-1183 (P3-423, Geometrie
  aus dem BPB) liegen; `git rev-list --count origin/main..HEAD` = **2**.
  Beide Male Bau 0 Warnungen, Suite **493/493 grün** mit einem benannten
  Skip (`test_freezer`), `check_consistency` 0, alle Vorhaken grün.
  **Eine Lehre aus dem Lauf, die beim Wiederaufnehmen gilt:** die
  Generator-Reihenfolge ist vollständig zu fahren —
  `gen_verification_tiers.py --write` → `gen_fs_tiers.py` →
  `gen_erzeuger_zensus.py` → `update_inventory.py` → `gen_stand.py`
  **zuletzt**. `kette1182.sh` ließ die ersten zwei aus, und
  `check_consistency` brach mit `[verification tiers stale]` und
  `[FS tiers stale]` ab — der Fehler lag im Kettenskript, nicht im Baum.
  Die fertige Kette liegt als `kette1183.sh` im Job-Kratzverzeichnis.
  **Nächster Griff beim Wiederaufnehmen:** P3-425, weil dort zwei Leser
  dieselbe Nutzlast verschieden zerlegen und das Erkennungsrennen die
  falsche Geometrie gewinnt — ein messbarer Befund mit Rotbeweis-Ort.
- **Beleg:** MF-1175…MF-1181 = `90c4f88b..76c56535`; **MF-1182 =
  `1d43b345`**; **MF-1183 = `e5daccf1`**. Der Posten bleibt offen, weil die
  Restliste offen ist — nicht weil ein Beleg fehlt.

---

## Fundus

*(aufgenommen, bewegt aber keine der vier Kennzahlen aus `CLAUDE.md`
§„jeder Baustein benennt seine Kennzahl" — nach MF-640 ist das Fundus,
nicht Auftrag. Steht hier, bis ein Anlass es hochholt.)*

---

## Erledigt

*(mit Beleg: Commit-Hash und MF-Nummer)*

---

## Zurückgenommen

*(mit Grund. Eine Liste, die durch Schrumpfen produktiv aussieht, ist
der Spiegelfehler aus MF-1077.)*
