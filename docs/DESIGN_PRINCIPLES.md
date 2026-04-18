# UFT Design Principles

**Warum dieses Dokument existiert.** Dies ist kein Feature-Katalog. Es ist die Liste
der Zusagen die UFT an seine Nutzer macht — und die Regeln die UFT intern einhält
um diese Zusagen zu halten.

Jeder Plugin-Autor, jeder Contributor, jede KI die an UFT arbeitet soll diese
Prinzipien kennen. Bei Konflikt zwischen einem Prinzip und einer Code-Änderung
gewinnt das Prinzip.

Die Prinzipien entstehen aus 15+ Jahren Erfahrung mit Preservation-Tools
(KryoFlux, SuperCard Pro, Greaseweazle, PCE Tools). Jedes Prinzip adressiert
einen dokumentierten Schmerz-Punkt dieser Tools.

---

## Die Grundmission

> **Kein Bit verloren. Keine stille Veränderung. Keine erfundenen Daten.**

UFT ist ein forensisches Preservation-Tool. Die Zielgruppe sind Archivare,
Museen, digitale Forensiker, und ernsthafte Retrocomputing-Enthusiasten.

Diese Nutzer brauchen **Wahrheit über die Magnetschicht** — nicht schöne
Images, nicht "funktionierende" Kopien, nicht aufgeräumte Daten.

Wenn ein Sektor schlecht ist, wird er als schlecht markiert.
Wenn Daten fehlen, werden sie als fehlend markiert.
Wenn etwas mehrdeutig ist, wird die Mehrdeutigkeit dokumentiert.

UFT erfindet nichts. UFT verbirgt nichts. UFT repariert nichts was nicht kaputt ist.

---

## Die sieben Design-Prinzipien

### Prinzip 1 — Niemals stille Datenverluste

**Die Zusage:** Jede Operation die Informationen reduziert oder transformiert
meldet das explizit bevor sie ausgeführt wird.

**Warum das existiert:** Etablierte Tools konvertieren still. Ein SCP→IMG-Export
wirft Weak-Bit-Flags, Timing-Informationen und Sync-Pattern weg — ohne Warnung.
Der Nutzer glaubt er habe sein Archiv gesichert und hat tatsächlich ein
degradiertes Duplikat.

**Was das konkret heißt:**

Jede Konvertierung hat einen der drei Round-Trip-Status:

- **LOSSLESS** — Keine Information geht verloren. Beispiel: SCP → HFE → SCP
  ergibt byte-identisches Resultat.
- **LOSSY-DOCUMENTED** — Information geht verloren, aber das Tool sagt **vor**
  der Operation genau was wegfällt. Beispiel: "SCP → IMG verliert: Weak-Bits
  (23 erkannt), Flux-Timing (track-präzise), Index-Pulse-Positionen."
- **IMPOSSIBLE** — Die Zielformat kann die Quelldaten nicht repräsentieren.
  Tool weigert sich die Operation durchzuführen und erklärt warum.

Ohne expliziten `--accept-data-loss`-Flag bricht UFT bei LOSSY-DOCUMENTED ab.
Mit dem Flag wird die Operation durchgeführt und die Verluste in einem
Sidecar-File (`.loss.json`) dokumentiert.

**Was verboten ist:**
- Stille Normalisierung von Weak-Bits zu 0 oder 1
- Stilles Auffüllen von unlesbaren Sektoren mit Nullen oder Mustern
- Stilles Verwerfen von Metadaten die im Zielformat keinen Platz haben
- "Beste Näherung" ohne Kennzeichnung

**Was Pflicht ist:**
- Pre-Conversion-Report mit vollständiger Verlust-Liste
- Sidecar-Dateien für nicht-repräsentierbare Informationen
- Klare Warnung im Export-Dialog
- Protokollierung aller Transformationen im Audit-Log

---

### Prinzip 2 — Marginale Daten werden erfasst, nicht verworfen

**Die Zusage:** UFT erfasst auch Daten die "nicht gut genug" aussehen.
Qualitäts-Filterung ist Aufgabe des Nutzers, nicht des Tools.

**Warum das existiert:** KryoFlux interpretiert marginale Tracks oft als
komplett leer und startet nicht einmal einen Retry, weil die Designphilosophie
lautet "nur gute Images werden erhalten". Das ist forensisch falsch. Ein
Preservation-Tool soll alles erfassen — die Frage "ist das gut genug?" ist
eine nachgelagerte Entscheidung.

**Was das konkret heißt:**

Für jeden Sektor produziert UFT:
- Die gelesenen Daten (auch wenn CRC-Fehler)
- Quality-Flags: `CRC_OK`, `CRC_FAIL`, `WEAK_BITS`, `UNREADABLE`, `MARGINAL`
- Anzahl der Leseversuche
- Bei mehreren Versuchen: Liste der gelesenen Varianten
- Timing-Informationen (wann der Sektor im Track stand)

Die Retry-Strategie ist vom Nutzer konfigurierbar:
- `--retries=N` (Default 5)
- `--retry-strategy=aggressive|conservative|adaptive`
- `--accept-marginal=true|false` (Default true — marginal bedeutet nicht unbrauchbar)

Auch bei `UNREADABLE` wird der Sektor im Image repräsentiert — mit dem Flag,
aber an der korrekten Stelle. Niemals durch Nullen oder Füllbytes ersetzt.

**Was verboten ist:**
- Stille Unterdrückung marginaler Sektoren
- Retry-Verweigerung wegen "es ist eh Kopierschutz"
- Verwerfen von Read-Varianten weil "nicht alle gleich sind"

**Was Pflicht ist:**
- Jeder erfasste Sektor landet im Image — mit oder ohne Quality-Flag
- Mehrfach-Reads werden alle gespeichert wenn sie divergieren
- Bei `UNREADABLE`: Position präzise markiert, nicht übersprungen

---

### Prinzip 3 — Explizite API statt impliziter Konventionen

**Die Zusage:** Jede Voraussetzung, jedes erwartete Namens-Pattern, jede
Option-Wechselwirkung wird in der API und Fehlermeldungen explizit gemacht.

**Warum das existiert:** Ein Greaseweazle-Nutzer versucht ein IPF-Image zu
schreiben und bekommt "FATAL ERROR: Bad Kryoflux image name pattern
'C:\games\ps.raw'. Name pattern must be path/to/nameNN.N.raw (N is a digit)".
Das Pattern stand nirgends in der Hilfe. Der Nutzer musste es aus der
Fehlermeldung ableiten. Das ist Design-Versagen.

**Was das konkret heißt:**

Für jede Operation dokumentiert UFT:
- Welche Input-Formate akzeptiert werden (Liste, kein "alles relevante")
- Welche Namens-Patterns erforderlich sind (mit Beispielen)
- Welche Optionen zusammen benötigt werden (und warum)
- Welche Vorbedingungen die Hardware/Disk erfüllen muss (und wie man prüft)

Fehlermeldungen haben drei Teile:
1. **Was ist falsch** — präzise, nicht "irgendetwas ging schief"
2. **Warum** — die zugrundeliegende Ursache oder Design-Entscheidung
3. **Fix-Vorschlag** — konkreter Befehl oder Schritt der das Problem löst

**Beispiel richtig:**
```
FEHLER: Kann SCP-Stream nicht importieren.
Grund:  UFT erwartet für Multi-Track-SCP einzelne Dateien mit Pattern
        name<track>.<side>.raw (z.B. mydisk00.0.raw, mydisk00.1.raw).
        Gefunden: einzelne Datei ohne Track-Suffix.
Fix:    Entweder umbenennen nach Pattern, oder
        --single-file-scp verwenden wenn es ein kombinierter SCP ist.
```

**Beispiel falsch:**
```
ERROR: Bad file format
```

**Was verboten ist:**
- Undokumentierte Namens-Konventionen
- Option-Defaults die gefährlich sind (siehe Prinzip 4)
- Fehlermeldungen ohne Lösungs-Vorschlag
- "Magic" die erwartetes Verhalten ohne Erklärung ändert

**Was Pflicht ist:**
- Jede erwartete Konvention in Hilfe-Text und Doku
- Vor-Prüfung von Voraussetzungen mit klarer Meldung
- `--help` zeigt echte Beispiele, nicht nur Syntax
- Fehlermeldungen geben Fix-Hinweis

---

### Prinzip 4 — Gefährliche Defaults sind verboten

**Die Zusage:** Operationen die Daten verändern oder verlieren können haben
niemals stille Defaults. Sie verlangen explizite Zustimmung.

**Warum das existiert:** Bei Greaseweazle führt `--format` ohne `--raw` zu
still regeneriertem Flux — bad sectors werden mit "[BAD SECTOR]"-Pattern
gefüllt. Der Nutzer glaubt Raw-Flux zu archivieren, bekommt aber
synthetisierte Daten. Das ist der Worst-Case einer Default-Falle.

**Was das konkret heißt:**

Operations werden in drei Klassen eingeteilt:

- **SICHER** — Liest Daten, verändert nichts. Default erlaubt.
  Beispiel: `uft read`, `uft verify`, `uft analyze`
- **VERLUSTBEHAFTET** — Kann Informationen reduzieren. Verlangt explizites
  `--accept-data-loss`.
  Beispiel: `uft convert --to=img` (verliert Flux-Timing)
- **DESTRUKTIV** — Verändert oder überschreibt. Verlangt explizites
  `--allow-destructive` plus Ziel-Bestätigung.
  Beispiel: `uft write`, `uft reformat`

Standardverhalten bei unklarem Kommando: **Abbruch mit Erklärung**, nie stille
Annahme.

**Konkrete Regeln für Flags die wir aus Fremd-Tools kennen:**

- **Flux-Regeneration** (Greaseweazle-Falle): Braucht explizites
  `--regenerate-flux`. Default: Raw-Flux wird byte-exakt erfasst.
- **"Perfekter" Flux statt beobachteter** (Greaseweazle-Falle): Niemals
  Default. Explizit `--idealize-flux`.
- **Platzhalter für bad sectors** (Greaseweazle-Falle): Niemals Default.
  Sektoren werden als `UNREADABLE` markiert.
- **Format-Erkennung überschreibt erkannten Typ** (SCP-Tools): Niemals still.
  Bei Konflikt zwischen `--format` und erkanntem Format: Abbruch mit Frage.

**Was verboten ist:**
- "Hilfreiche" Defaults die Daten transformieren
- Flag-Kombinationen wo ein Flag das Verhalten eines anderen unerwartet ändert
- Operations die "normalerweise" gutgehen aber Edge-Cases still verkacken

**Was Pflicht ist:**
- Explizite Klassifizierung jeder Operation
- Destruktive Flags haben ausgeschriebene Namen (`--allow-destructive`, nicht `-d`)
- Bei Ambiguität lieber nachfragen als annehmen

---

### Prinzip 5 — Round-Trip ist First-Class Funktion

**Die Zusage:** Für jede Konvertierung dokumentiert UFT ob sie verlustfrei ist,
und bestätigt das durch automatisierte Tests.

**Warum das existiert:** PCE-Tools können Sektor-Images nicht zurück in
KryoFlux- oder SCP-Streams konvertieren. Das Format-Universum ist voll von
Einbahnstraßen ohne dass Nutzer das wissen. Man exportiert, denkt man habe
archiviert, und merkt drei Jahre später dass der Rückweg nicht existiert.

**Was das konkret heißt:**

UFT pflegt eine Round-Trip-Matrix — für jedes Paar (From, To) ein Status:

| Von ↓ / Nach → | SCP | HFE | IMG | ADF | D64 | ... |
|----------------|-----|-----|-----|-----|-----|-----|
| SCP | — | LL | LD | LD | LD | ... |
| HFE | LL | — | LD | LD | LD | ... |
| IMG | IM | IM | — | ? | ? | ... |
| ADF | IM | LD | LL | — | IM | ... |

- **LL** = Lossless (byte-identischer Round-Trip)
- **LD** = Lossy-Documented (Verlust bekannt und dokumentiert)
- **IM** = Impossible (Zielformat kann Quelle nicht repräsentieren)
- **?** = Ungeprüft (wird nicht angeboten bis getestet)

Die Matrix wird durch automatisierte Tests gepflegt. Jeder LL-Eintrag
bedeutet: es existiert ein Test der die byte-identische Round-Trip prüft.
Wenn dieser Test fehlt, ist der Status nicht LL sondern ?.

Das `compatibility-import-export`-Subsystem von UFT hält diese Matrix aktuell.

**Was verboten ist:**
- Konvertierung anbieten ohne Round-Trip-Status
- Behaupten "lossless" ohne Test der das beweist
- LD-Status ohne Dokumentation was genau verloren geht

**Was Pflicht ist:**
- Jede Konvertierung zeigt ihren Status vor Ausführung
- LL-Konvertierungen haben automatisierte Round-Trip-Tests
- LD-Konvertierungen haben Liste der verlorenen Datenkategorien
- IM-Konvertierungen erklären warum sie unmöglich sind

---

### Prinzip 6 — Emulator-Kompatibilität wird explizit getestet

**Die Zusage:** UFT testet seine Exports gegen echte Referenz-Konsumer
(Emulatoren, andere Tools). Behaupten dass ein Format "funktioniert" reicht
nicht.

**Warum das existiert:** Zwei Tools können beide behaupten "ADF-Support" und
produzieren Images die sich unterschiedlich verhalten. Eine Disk läuft in
WinUAE, nicht in FS-UAE. Eine andere läuft in VICE, aber nicht im echten C64.
"Funktioniert" ist ein Lügenwort ohne spezifizierten Ziel-Konsumer.

**Was das konkret heißt:**

Für jedes Export-Format pflegt UFT eine Kompatibilitäts-Matrix:

```
Format: ADF (Amiga Disk File)
Geprüfte Konsumer:
  WinUAE 5.3:        kompatibel (CI-getestet)
  WinUAE 4.x:        kompatibel (CI-getestet)
  FS-UAE 3.1:        kompatibel (CI-getestet)
  FS-UAE <3.0:       inkompatibel (Timing-Track wird nicht erkannt)
  Amiga Explorer:    kompatibel (manuell getestet 2026-03)
  Echte Hardware:    Rückschreibung getestet für 85% der Testfälle
```

Der `compatibility-import-export`-Agent hält diese Matrizen für alle
Export-Formate aktuell.

Wo manuelle Tests erforderlich sind, werden sie in der Community koordiniert
und die Ergebnisse transparent dokumentiert.

**Was verboten ist:**
- "Wir unterstützen Format X" ohne spezifizierten Referenz-Konsumer
- Behauptungen auf Spec-Basis statt auf Test-Basis
- Ignorieren von Inkompatibilitäten die Nutzer melden

**Was Pflicht ist:**
- Pro Export-Format mindestens zwei Referenz-Konsumer getestet
- Bei bekannter Inkompatibilität: Warning + Workaround-Hinweis
- CI-Pipeline die Exports durch mindestens einen Emulator schickt wo möglich
- Transparente Liste geprüfter und nicht-geprüfter Konsumer

---

### Prinzip 7 — Ehrlichkeit bei proprietären und unterspecifizierten Formaten

**Die Zusage:** Wo die Format-Spezifikation unvollständig, proprietär oder
reverse-engineered ist, sagt UFT das explizit und dokumentiert welche
Features unterstützt sind.

**Warum das existiert:** Das IPF-Format ist der "Goldstandard" für
Amiga-Preservation, aber SPS hat die Spec nie vollständig öffentlich gemacht.
Tools implementieren IPF unterschiedlich weil jeder es anders interpretiert.
Ein "IPF-kompatibles" Tool kann vieles heißen. Nutzer haben keine Möglichkeit
zu wissen was sie bekommen.

**Was das konkret heißt:**

Jedes Format-Plugin hat einen Spec-Status:

- **OFFIZIELL-SPEC** — Spezifikation ist öffentlich und vollständig.
  Beispiel: SCP (cbmstuff hat die Spec veröffentlicht).
- **OFFIZIELL-TEILWEISE** — Spec ist öffentlich aber unvollständig.
  Beispiel: HFE (HxC hat die Grundlagen dokumentiert, manche Details nicht).
- **REVERSE-ENGINEERED** — Keine offizielle Spec, Implementierung basiert auf
  Analyse und Community-Wissen. Beispiel: IPF, STX.
- **ABGELEITET** — Format existiert durch De-facto-Standard ohne formale Spec.
  Beispiel: verschiedene emulator-spezifische Varianten.

Für jedes Plugin gibt es eine Feature-Matrix:

```
Format: IPF (SPS Preservation Format)
Spec-Status: REVERSE-ENGINEERED (basiert auf libcaps, MAME-Implementierung,
             Community-Dokumentation. Kein offizielles SPS-Dokument öffentlich.)

Feature-Support:
  Standard MFM Tracks:       vollständig unterstützt
  Timing-Tracks:             unterstützt
  Weak-Bits:                 teilweise (Detection ja, exakte Position ±3 Bits)
  Data-Cells:                unterstützt
  Multi-Revolution-Samples:  NICHT unterstützt
  SPS-proprietäre Protection-Marker: NICHT unterstützt

Referenz-Implementation: libcaps (zur Verifikation), MAME (als Konsumer)
Test-Disks: 47 originale Amiga-Spiele aus TOSEC getestet (36 funktionieren
            vollständig, 11 teilweise durch nicht-unterstützte Features)
```

**Was verboten ist:**
- "Volle Unterstützung" claim ohne Feature-Matrix
- Proprietäre Formate implementieren ohne Hinweis auf Reverse-Engineering
- Partial-Support verstecken

**Was Pflicht ist:**
- Pro Plugin: Spec-Status-Marker
- Pro Plugin: Feature-Matrix mit supported/partial/unsupported
- Bei REVERSE-ENGINEERED: Referenz-Implementierung benannt
- Bei bekannten Grenzfällen: in Doku und in Fehlermeldung

---

## Die Meta-Prinzipien (wie wir diese Zusagen einhalten)

### Meta-Prinzip A — Tests sind Zusagen

Ein Prinzip ohne automatisierten Test ist ein Marketing-Claim. Jede Zusage in
diesem Dokument entspricht mindestens einem CI-Test:

- Round-Trip-LL: byte-exakter Vergleich
- Lossy-Documented: Sidecar-Datei hat erwartete Felder
- Marginale Daten: Sektoren mit `CRC_FAIL`-Flag werden im Image erhalten
- Emulator-Kompatibilität: Export wird durch Emulator geschickt und verifiziert
- Fehlermeldungen: Test dass jede Fehlermeldung Fix-Vorschlag enthält

Bei jedem Prinzip-Verstoß bricht CI. Prinzipien sind keine Ziele sondern Regeln.

### Meta-Prinzip B — Transparenz schlägt Bequemlichkeit

Wenn ein Prinzip im Einzelfall lästig ist (z.B. Nutzer muss explizit
`--accept-data-loss` schreiben), gewinnt das Prinzip. Bequemlichkeit wird in
der Dokumentation und Fehlermeldungen adressiert, nicht durch Prinzip-Erosion.

Die Zielgruppe (Archivare, Forensiker) schätzt Transparenz über Bequemlichkeit.
Das ist bewusst.

### Meta-Prinzip C — Veröffentlichte Fehler-Liste

UFT pflegt `docs/KNOWN_ISSUES.md` mit allen bekannten Fällen wo ein Prinzip
aktuell nicht vollständig eingehalten wird. Diese Liste ist öffentlich und
wird aktiv abgearbeitet.

Ein bekanntes Problem zu dokumentieren ist kein Eingeständnis von Schwäche —
es ist Voraussetzung für Vertrauenswürdigkeit.

### Meta-Prinzip D — Die Prinzipien sind nicht verhandelbar

Feature-Anfragen die ein Prinzip brechen werden abgelehnt — auch wenn sie
bequem wären, auch wenn "andere Tools das auch so machen", auch wenn "es
ist eh selten relevant".

Änderungen an den Prinzipien selbst brauchen:
1. Dokumentierten Anwendungsfall warum das aktuelle Prinzip nicht funktioniert
2. Review durch Maintainer und Community-Feedback
3. Update aller abhängigen Tests und Dokumentation
4. Major-Version-Bump wenn die Änderung öffentlich sichtbar ist

---

## Wie diese Prinzipien entstanden sind

Die sieben Design-Prinzipien sind keine abstrakte Philosophie. Jedes Prinzip
adressiert einen konkreten, dokumentierten Fehler der etablierten Tools:

| Prinzip | Adressierter Tool-Fehler |
|---------|--------------------------|
| 1 — Nie stille Datenverluste | SCP→IMG-Konvertierung verliert Weak-Bits ohne Warnung |
| 2 — Marginale Daten erfassen | KryoFlux interpretiert marginale Tracks als leer, startet keinen Retry |
| 3 — Explizite API | Greaseweazle verlangt undokumentiertes Pfad-Pattern für KryoFlux-Streams |
| 4 — Keine gefährlichen Defaults | `gw read --format` ohne `--raw` produziert synthetischen statt echten Flux |
| 5 — Round-Trip als First-Class | PCE-Tools können Sektor-Images nicht zurück in SCP konvertieren |
| 6 — Explizite Emulator-Tests | "ADF-kompatibel" bei verschiedenen Tools bedeutet verschiedenes |
| 7 — Ehrlichkeit bei proprietären Formaten | IPF ist nicht offiziell dokumentiert, Implementierungen divergieren |

Die Community dieser Tools lebt seit 15+ Jahren mit diesen Problemen. UFT
existiert um sie zu lösen — nicht durch neue Features, sondern durch bessere
Disziplin.

---

## Was UFT _nicht_ ist

Damit die Identität klar bleibt:

**UFT ist nicht:**
- Ein schnellstes Floppy-Tool (Performance zählt, aber nie vor Korrektheit)
- Ein einfachstes Floppy-Tool (Zielgruppe verträgt Komplexität)
- Eine Ein-Klick-Lösung für Casual-User (andere Tools erfüllen diese Nische besser)
- Ein Kopierschutz-Cracker (wir analysieren, wir umgehen nicht)
- Ein Emulator (wir erzeugen Images, Emulatoren nutzen sie)

**UFT ist:**
- Ein forensisches Preservation-Tool mit hoher Latte für Datenintegrität
- Eine Alternative für Nutzer die von Kompromissen anderer Tools genug haben
- Ein Werkzeug für Archivare die in zehn Jahren ihre Arbeit noch rekonstruieren
  können müssen
- Ein Referenz-Implementierung für Preservation-Standards

---

## Für Contributors

Wenn du an UFT mitarbeitest — ob als menschlicher Entwickler oder als KI —
gelten diese Prinzipien für jede Code-Änderung.

**Vor jedem Commit die Prüfung:**
1. Verletzt meine Änderung eines der sieben Prinzipien?
2. Wenn ja — kann ich es anders lösen?
3. Wenn nein — brauche ich Review durch Maintainer?

**Die häufigsten Fallen:**

- "Nur schnell einen Default setzen" → wahrscheinlich Prinzip 4 Verstoß
- "Der Nutzer will doch, dass es 'einfach funktioniert'" → wahrscheinlich
  Prinzip 1 oder 4 Verstoß
- "Das ist ein seltener Edge-Case" → für forensische Nutzer gibt es keine
  seltenen Edge-Cases
- "Andere Tools machen das auch so" → deshalb existiert UFT

**Bei Unsicherheit:** Im Issue-Tracker fragen bevor committen. Ein Prinzip-
Verstoß im Master ist schwerer zu reparieren als eine Diskussion im Issue.

---

## Schluss

Diese Prinzipien sind das Herz von UFT. Features kommen und gehen — Prinzipien
bleiben.

Wenn UFT in zehn Jahren noch existiert und die Preservation-Community UFT
nutzt weil sie wissen "dieses Tool lügt mich nicht an", dann hat dieses
Dokument seine Aufgabe erfüllt.

---

**Version:** 1.0
**Stand:** 2026-04
**Maintainer:** Siehe CONTRIBUTING.md

Änderungen an diesem Dokument erfordern Maintainer-Review und werden im
Changelog unter dem Abschnitt "Design Principles" dokumentiert.
