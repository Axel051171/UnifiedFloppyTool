---
name: aufgabe
description: Use when the user types /aufgabe, hands over a new task while other work is already open, asks what is next or what is still on the list, says "mach weiter", "nimm das auf", "was steht an", "erledigt", "das ist wichtiger" — or whenever an incoming request would otherwise be started on top of work already in progress.
---

# /aufgabe — eine Liste, ein Posten

Die Liste ist `.claude/AUFGABEN.md`. Sie ist versioniert und überlebt
`/clear`, Verdichtung und den Rechnerwechsel — der Sitzungskontext tut
das nicht, deshalb liegt sie als Datei da und nicht in `TodoWrite`.

**Zwei Sätze, aus denen alles Übrige folgt:**

1. **Aufnahme startet keine Arbeit.** `/aufgabe <Text>` analysiert,
   trägt ein, antwortet mit dem Platz — und hört auf.
2. **Höchstens ein Posten steht auf `in Arbeit`.** Das ist der Schutz
   gegen Vermischung, nicht die Liste selbst.

## Die sechs Aufrufe

| Aufruf | Tat |
|---|---|
| `/aufgabe <Text>` | aufnehmen, einordnen, Platz nennen. **Nicht beginnen.** |
| `/aufgabe` | Liste zeigen: laufender Posten, die nächsten drei, Zahl der Wartenden |
| `/aufgabe weiter` | am laufenden Posten arbeiten; ist keiner in Arbeit, den obersten der Warteschlange hochziehen und auf `in Arbeit` setzen |
| `/aufgabe fertig` | abschließen — **nur mit Beleg** (Commit-Hash + MF-Nummer) |
| `/aufgabe vor <Text>` | vordrängen — schreibt **zuerst** `Stand:` in den laufenden Posten |
| `/aufgabe stand <Text>` | Zwischenstand in den laufenden Posten schreiben, ohne zu wechseln |

## Aufnahme: das Raster

Die Analyse ist kurz und hat feste Zeilen. Freitext macht die Liste
unvergleichbar; das Raster macht sie sortierbar. **Der Wortlaut des
Auftrags wird zitiert, nicht zusammengefasst** — eine Zusammenfassung
ist schon eine Auslegung, und die Auslegung gehört in die Zeilen
darunter, wo man sie widerlegen kann.

```markdown
### A-007 · <Titel in einer Zeile>
- **Status:** aufgenommen · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „<was der Eigentümer gesagt hat, wörtlich>"
- **Kennzahl:** T3 runter | Wandlungspfade rauf | leckende Tests null |
  Bench-Alter runter | **keine der vier → Fundus**
- **Kanal:** Port | Nachbau | Helfer-Prozess | Oracle | Spec | Daten |
  Fundus | entfällt
- **Einfrier-Regel:** berührt Format-/Decoder-Layer? **ja → Rotbeweis
  zuerst** | nein
- **OPEN_ITEMS:** P3-NNN | keiner
- **Fertig heißt:** <eine prüfbare Bedingung>
- **Aufwand:** <Spanne> | nicht schätzbar
- **Stand:** —
- **Beleg:** —
```

**Vier Zeilen, die eine Regel dieses Baums vollstrecken:**

- **Kennzahl** — MF-640: was keine der vier bewegt, ist *Fundus, nicht
  Auftrag*. Es wird notiert, nicht eingeplant. Ohne diese Zeile füllt
  sich die Warteschlange mit allem, was jemandem auffällt.
- **Kanal** — MF-695: „Lizenz vor Fähigkeit" heißt nicht „Fund
  verwerfen", sondern *auf welchem Weg*. Der Weg wird bei der Aufnahme
  benannt, nicht wenn der Code schon dasteht.
- **Einfrier-Regel** — MF-363/MF-498: Format- und Decoder-Layer
  brauchen die benannte Referenz **vor** dem Code. Das steht im
  Eintrag, weil es sonst später als Einschränkung erscheint statt als
  Plan.
- **Fertig heißt** — `.claude/CLAUDE.md` §Definition of Done. Bei der
  Aufnahme geschrieben ist es eine Bedingung; am Ende geschrieben ist
  es eine Beschreibung dessen, was man ohnehin getan hat.

**Aufwand als Spanne oder „nicht schätzbar"** (Hard-Rule 3). Keine
Zahl im Eintrag, die nicht gemessen ist — auch keine geschätzte, die
wie gemessen aussieht.

## OPEN_ITEMS: verweisen, nicht verdoppeln

`docs/OPEN_ITEMS.md` ist seit MF-588 die **einzige** Liste der Befunde.
Vor jeder Aufnahme wird dort gesucht (`grep -n` nach zwei bis drei
Stichworten des Auftrags):

- **Treffer** → die `P?-NNN` kommt in die Zeile `OPEN_ITEMS:`, der
  Befund selbst wird **nicht** abgeschrieben. Der Posten ist der
  Auftrag, ihn abzutragen; die Messung bleibt drüben.
- **Kein Treffer, und es ist ein Befund** (etwas im Baum hält seine
  eigene Zusage nicht) → er gehört nach `OPEN_ITEMS.md`, und der
  Posten hier verweist auf die neue Nummer. Diese Liste ist keine
  zweite Befundliste.

## Die Ein-Schloss-Regel

Kommt ein Auftrag herein, während einer läuft, wird **angehängt**. Die
Antwort ist eine Zeile: Nummer, Titel, Platz, und was gerade läuft.

Ein Wechsel geschieht nur über `/aufgabe vor` — und der schreibt
**zuerst** `Stand:` in den laufenden Posten: wo stehe ich, was ist
gemessen, was fehlt, welche Datei ist halb geändert. Ein Wechsel ohne
Standzeile ist ein stiller Verlust, und still Verlorenes ist in diesem
Baum die teuerste Fehlerklasse.

## Abschluss verlangt einen Beleg

`/aufgabe fertig` trägt Commit-Hash und MF-Nummer ein und verschiebt den
Posten nach `Erledigt`. **Ohne Beleg bleibt er offen** — dann ist die
Liste gegen `git log` prüfbar statt behauptet, und das ist derselbe
Maßstab, den dieser Baum an jede Zahl legt.

Es gibt kein „90 % fertig" (`.claude/CLAUDE.md` §Eigenverantwortung).
Was nicht vollständig ist, bleibt `in Arbeit` mit einer Standzeile.

## Kein Löschen

Erledigtes wandert nach unten. Verworfenes bekommt `Zurückgenommen:`
mit Grund. Ein Posten verschwindet nicht, weil er unbequem geworden
ist — MF-1077: die Zahl darf nicht das Motiv sein, und eine Liste, die
durch Schrumpfen produktiv aussieht, ist genau dieser Fehler.

## Rationalisierungen

| Ausrede | Wirklichkeit |
|---|---|
| „Das ist in zwei Minuten erledigt, ich mach's direkt" | Aufnahme ist stumm. Anhängen. Die zwei Minuten waren noch nie zwei Minuten. |
| „Die neue Aufgabe ist offensichtlich wichtiger" | Das entscheidet der Eigentümer mit `vor`, nicht du. Anhängen und die Lage nennen. |
| „Ich mache beide parallel, sie berühren sich nicht" | Das ist wörtlich die Vermischung, gegen die diese Liste existiert. |
| „Der laufende Posten ist quasi fertig" | Dann schließ ihn mit Beleg ab. „Quasi" ist kein Zustand. |
| „Ich schreibe den Stand nachher" | Nachher ist der Kontext weg. Der Stand steht **vor** dem Wechsel. |
| „Der Posten hat sich erledigt, ich streiche ihn" | `Zurückgenommen:` mit Grund. Streichen ist keine Behebung. |
| „Ich schreibe den Befund hier rein, das ist schneller" | Zwei Befundlisten sind zwei Wahrheiten. `OPEN_ITEMS.md`, dann verweisen. |
| „Der Dauerauftrag sagt, ich soll ohne Rückfrage durcharbeiten" | Er sagt nicht, dass Aufnahme Ausführung ist. Aufnahme bleibt stumm; gearbeitet wird auf `weiter`. |

## Rote Fahnen — anhalten und anhängen

- Du greifst zu `Read`/`Edit`, unmittelbar nachdem ein neuer Auftrag kam
- Zwei Posten stehen auf `in Arbeit`
- Ein Eintrag hat keine Zeile `Fertig heißt`
- Ein Posten steht in `Erledigt` ohne Commit-Hash
- Eine Zahl im Eintrag, die nirgends gemessen wurde
- Ein Befund steht ausformuliert hier statt in `OPEN_ITEMS.md`
