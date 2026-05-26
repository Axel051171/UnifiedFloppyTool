# CONSULT-Protokoll

Wie UFT-Agenten zusammenarbeiten wenn ein Befund den Scope des findenden
Agenten übersteigt.

**Kernidee:** Ein Agent der ein Problem findet das er nicht selbst lösen kann
gibt einen strukturierten `CONSULT`-Block in seinem Output aus. Die aufrufende
Session (Hauptkonversation oder `orchestrator`) parst den Block und routet an
den passenden Spezialisten.

---

## Block-Format

Jeder Konsultationswunsch ist ein eigener Fenced-Block mit `consult` als
Language-Tag:

````
```consult
TO: <agent-name>
QUESTION: <ein Satz, Imperativ>
CONTEXT: <datei:zeile> | <commit-hash> | <kurzer Ausschnitt>
REASON: <warum ich das selbst nicht beantworten kann>
SEVERITY: P0 | P1 | P2 | P3
```
````

Pflichtfelder: `TO`, `QUESTION`, `CONTEXT`, `REASON`. Optional: `SEVERITY`
(Default `P2`), `ATTACHMENTS` (zusätzliche Pfade).

Beispiel:

````
```consult
TO: abi-bomb-detector
QUESTION: Ist das Einfügen von spec_status am Ende von uft_format_plugin_t ABI-safe?
CONTEXT: include/uft/uft_format_plugin.h:247-256
REASON: must-fix-hunter findet 3 Plugins mit is_stub=true, will sicherstellen dass neue
        Feld-Position keine ältere Plugin-.so bricht
SEVERITY: P1
```
````

---

## Parsing-Regel für Empfänger (Haupt-Session oder `orchestrator`)

1. Scanne den Agent-Output nach ` ```consult ... ``` ` Fenced-Blocks.
2. Validiere Pflichtfelder. Fehlt eines: CONSULT ignorieren, Warnung loggen.
3. Prüfe Richtungsregel (siehe unten). Bei Verstoß: CONSULT ablehnen,
   User informieren.
4. Spawne den Ziel-Agenten mit einem Prompt der `QUESTION`, `CONTEXT`, `REASON`
   enthält. Die `ATTACHMENTS` (falls vorhanden) als `Read`-Vorlagen mitgeben.
5. Nach Rückkehr: Antwort des Konsultierten **unter** dem CONSULT-Block in die
   Konversation einhängen, ursprünglichen Agent NICHT erneut aufrufen
   (Einweg-Konsultation).

---

## Richtungsregel (Kosten-Policy)

Aus `.claude/CLAUDE.md`: Opus für Analyse/Strategie, Sonnet für Routine.
Das CONSULT-Protokoll erzwingt diese Regel:

| Von → Zu | Erlaubt? | Begründung |
|---|---|---|
| Sonnet → Sonnet | **ja** | billig, lateral |
| Sonnet → Opus | **ja** | „hochfragen" bei Architektur-Frage ist legitim |
| Opus → Sonnet | **ja** | Opus delegiert Routinearbeit — Standard-Fall |
| Opus → Opus | **NEIN** | Rekursions-/Kosten-Gefahr; Opus-Agent soll selbst entscheiden oder an den Menschen/User zurückgeben |

Ein `CONSULT: orchestrator` aus einem Opus-Agenten ist erlaubt (orchestrator
ist Opus, aber sein Zweck ist Routing — zählt als „an Mensch zurückgeben").

---

## Direkter Agent-Call (tool-level)

Vier Agenten haben `Agent` in ihrer Tool-Liste und dürfen direkt sub-spawnen:

- **`orchestrator`** — Master-Router. Darf jeden beliebigen Spezialisten
  spawnen. Haupt-Nutzer des CONSULT-Protokolls als Empfänger.
- **`must-fix-hunter`** — darf die 9 Scan-Unterkategorien parallel an
  Spezialisten fan-outen (`dispatching-parallel-agents`-Pattern). Keine
  weiteren Kaskaden — Sub-Agenten dürfen nicht weiter spawnen.
- **`preflight-check`** — darf vor Release-Tag an `abi-bomb-detector`,
  `consistency-auditor`, `must-fix-hunter` fan-outen. Gleiche Kaskaden-Regel.
- **`deep-diagnostician`** — darf gezielt Spezialisten für Teilfragen
  spawnen (historisch so konfiguriert).

Alle übrigen Agenten **dürfen keine Sub-Agenten direkt spawnen**. Sie
verwenden CONSULT-Blocks; das Routing übernimmt der Empfänger.

---

## Kosten-Guardrails

- **Kaskaden-Tiefe max 2:** Haupt-Session → Agent → evtl. ein Sub-Agent. Nicht
  tiefer. Ein spawnender Agent (s.o.) darf nicht selbst einen weiteren
  Spawn-fähigen Agenten aufrufen.
- **Opus-budget:** Pro User-Turn max ein Opus-Agent als Haupt-Call, max ein
  Opus-Sub-Call.
- **CONSULT-Anzahl:** Ein Agent-Output mit mehr als 3 CONSULT-Blöcken signalisiert
  falschen Agenten-Scope — der aufrufende Empfänger soll das zurückmelden.
- **Rekursion ausgeschlossen:** Ein Agent darf sich nicht selbst konsultieren.
  Empfänger blockt.

---

## Was CONSULT NICHT ist

- **Keine synchronen Ping-Pong-Dialoge** — ein Agent fragt, der nächste
  antwortet, Ende. Wenn eine Kette entsteht, ist das ein Design-Problem:
  der Scope eines Agenten war falsch zugeschnitten.
- **Kein Ersatz für den Menschen** — bei echter Uneindeutigkeit
  (Architektur-Entscheidung, Produkt-Strategie) gibt der Agent `CONSULT:
  human` zurück. Der Empfänger legt den Prompt vor den User, spawnt nicht.
- **Kein Status-Log** — CONSULT bedeutet „ich brauche eine konkrete Antwort",
  nicht „zur Information".

---

## Wann NICHT konsultieren (Autonomie-Grenze)

CONSULT ist für echte Sub-Scope-Fragen, nicht für Selbstvergewisserung
oder Arbeitsabgabe. Die folgenden Patterns sind explizit verboten — sie
verraten alle, dass der Agent versucht, eine eigene Entscheidung gegen
einen anderen abzustützen statt sie zu treffen:

| Verbotenes Pattern | Was statt dessen |
|---|---|
| `CONSULT: human · QUESTION: Ist das OK?` | Entscheidung treffen, im Output begründen, weitermachen |
| `CONSULT: human · QUESTION: Welcher von zwei Ansätzen?` ohne Trade-off-Analyse | Erst Analyse machen, dann **mit konkreter Empfehlung** konsultieren |
| `CONSULT: orchestrator · QUESTION: Was würdest du tun?` | Du selbst — der orchestrator routet, er denkt nicht für dich |
| `CONSULT: <X> · QUESTION: Kannst du mal schauen ob…?` | Du schaust selbst; CONSULT nur wenn du `<X>`s Werkzeuge brauchst |
| `CONSULT: human · QUESTION: Soll ich weitermachen?` | Wenn keine Hard-Rule oder Design-Prinzip blockiert: ja, weitermachen |
| CONSULT als Übergabe „mein Teil ist fertig" | Aufgabe selbst zu Ende führen ODER ehrlich offen melden |

### Pflicht-Felder für ein legitimes CONSULT

Zusätzlich zu `TO`, `QUESTION`, `CONTEXT`, `REASON`: der `REASON` muss
**konkret** enthalten:

- Was du selbst schon versucht hast (`grep`/`Read`/`git log`/Test-Run)
- Welche Hypothesen du verworfen hast und warum
- Welche **konkrete Information** dir noch fehlt (kein „mehr Kontext")
- Warum genau **dieser** Empfänger die Information hat (Werkzeug-Zugriff,
  Architektur-Wissen, Datei-Kompetenz)

Ein gutes CONSULT klingt: „Ich habe X und Y geprüft, Z verworfen weil…,
brauche jetzt von `abi-bomb-detector`: ist Layout-Änderung A→B in
`uft_format_plugin_t` ABI-safe für Plugins kompiliert mit gcc 14?"

Ein schlechtes CONSULT klingt: „Kannst du mal schauen ob das so passt?"
— das ist Arbeit zurückgeben, nicht Wissen holen.

### Konsequenz

Der Empfänger (Haupt-Session oder `orchestrator`) lehnt CONSULTs ab, die
diese Kriterien nicht erfüllen, mit Hinweis auf diese Sektion. Der
fragende Agent macht weiter mit der eigenen besten Entscheidung.

---

## Minimal-Implementation für den Empfänger

Pseudocode in der Haupt-Session:

```
for block in parse_consult_blocks(agent_output):
    if not validate(block):
        warn_user(block); continue
    if violates_direction_rule(caller, block.TO):
        reject(block, "direction"); continue
    if block.TO == "human":
        show_to_user(block); continue
    spawn_agent(block.TO, prompt=format(block))
```

Der `orchestrator` hat dies als seine Hauptschleife.

---

## Versionierung

Aktuelle Protokoll-Version: **v1**. Änderungen am Block-Format sind Breaking
Changes und brauchen einen `PROTOCOL_VERSION`-Header im Agenten-Prompt.

---

## Beispiele aus typischen Flows

**must-fix-hunter findet Versions-Drift** → will SSOT-Design-Rat:
````
```consult
TO: single-source-enforcer
QUESTION: Wie integriere ich die neue AppStream metainfo release-version ohne neuen Drift-Punkt?
CONTEXT: packaging/flatpak/io.github.Axel051171.UnifiedFloppyTool.metainfo.xml:88
REASON: Metainfo enthält <release version="4.1.3"/>, ist aber nicht in VERSION-Ableitungskette; fügt 11. Drift-Stelle hinzu
SEVERITY: P1
```
````

**consistency-auditor blockt BLOCK-4 (neuer Stub)** → will Stub-Plan:
````
```consult
TO: stub-eliminator
QUESTION: Welche der vier Aktionen für uft_xyz_probe neu eingefügt in src/formats/xyz/?
CONTEXT: src/formats/xyz/uft_xyz.c:42-48
REASON: Funktion gibt nur UFT_OK zurück aber ist in Plugin-Struct als probe registriert — Prinzip 7.3 Verletzung
SEVERITY: P0
```
````

**forensic-integrity findet stille Normalisierung** → will Architektur-Review:
````
```consult
TO: orchestrator
QUESTION: Architektur-Entscheidung — darf der Flux-Normalizer weak-bits auf 0 setzen oder braucht er Flag?
CONTEXT: src/decoder/uft_flux_normalize.c:217
REASON: Prinzip-1-Verletzung (stille Datenänderung). Fix könnte Architektur-Impact haben da alle Decoder-Pipelines die Funktion nutzen.
SEVERITY: P0
```
````
