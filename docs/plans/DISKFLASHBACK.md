# Umsetzungsplan DiskFlashback — Baum-Fassung

> **Verdaut, nicht kopiert.** Nur die offenen Bausteine; Erledigtes und
> **Widerlegtes** stehen mit MF-Verweis dabei. Nach MF-641 Nachtrag 3.

Der Entwurf zielt auf eine **Mount-Kette**: ein Abbild wird erkannt,
sein Dateisystem bestimmt, und der Inhalt erscheint als durchsuchbarer
Baum — plus Abgleich gegen bekannte Disketten.

## Was die Messung am Entwurf korrigiert hat

### Baustein B ruht auf einer Annahme, die nicht zutrifft

Der Entwurf sah vor: **„bei mehreren Treffern über Schwelle alle
mounten."** Gemessen (`KNOWN_ISSUES.md`, Abschnitt Erkenner):

> **Der Erkenner liefert nie mehr als EINEN Kandidaten.** Auch nicht bei
> einem absichtlich hybriden Abbild mit AmigaDOS-Kennung **und** gültigem
> FAT-BPB im selben Sektor — er kurzschließt beim ersten starken Treffer.

Die Hybrid-Schleife im neuen Code ist richtig geschrieben, aber
**unerreichbar**. Der erste Test dafür bestand trivial (ein Kandidat,
Schleife läuft nie) und wurde **gelöscht** statt behalten — ein Test, der
nicht scheitern kann, ist keiner.

**Folge für die Planung:** Baustein B ist nicht „bauen", sondern zuerst
**„den Erkenner mehrkandidatenfähig machen"**. Das ist ein eigener
Baustein und gehört vor B.

### Ein Nullabbild wird nicht als unbekannt gemeldet

Ein 720-KB-Abbild aus lauter Nullen meldet **„CP/M (vorläufig, Stufe 3
nötig)" mit Konfidenz 20** — nicht „unbekannt". Eine Mount-Kette, die auf
„Konfidenz > 0" mountet, würde also eine leere Diskette als CP/M
präsentieren.

**Folge:** die Schwelle der Mount-Kette ist ein eigener, zu messender
Wert, keine Selbstverständlichkeit.

## Offene Bausteine

### DF-1 — Erkenner mehrkandidatenfähig *(Vorbedingung für B)*

**Kennzahl:** ungeprüfte Formate runter — ein Erkenner, der beim ersten
Treffer abbricht, verdeckt Fehlklassifikationen.

Rotbeweis liegt bereits vor: das hybride Abbild (AmigaDOS-Kennung +
gültiger FAT-BPB im selben Sektor) muss **zwei** Kandidaten liefern.
Heute liefert es einen.

### DF-2 — Mount-Kette mit gemessener Schwelle

**Kennzahl:** ungeprüfte Formate runter (Inhaltsebene, wie Phase 1).

Hängt an DF-1 **und** an Phase 1: eine Mount-Kette ist nur so ehrlich wie
der Dateisystem-Leser darunter. Vor dem Bauen steht die
Bereitschafts-Messung aus Nachtrag 1 — **erst Türen suchen, dann bauen**.

Die Schwelle muss gegen den Nullabbild-Fall geprüft sein: leer bleibt
leer, „vorläufig" mountet nicht.

### DF-3 — Abgleich gegen bekannte Disketten

**Kennzahl:** keine der vier — Fundus, bis DF-1 und DF-2 stehen.

Setzt einen Referenzbestand voraus, den es noch nicht gibt. Der
Amiga-Dumpvorrat (300 eigene Disketten + Greaseweazle) wäre die
natürliche Quelle, und er entsteht ohnehin für Phase 3.

## Warum dieser Plan bewusst kurz ist

Er trägt genau das, was im Baum belegt ist: zwei Messungen, die den
ursprünglichen Entwurf korrigieren, und die Bausteine, die daraus folgen.
Der Rest des Entwurfs existiert außerhalb des Repos und ist damit **nicht
ankerfähig** — nach der Regel in [`README.md`](README.md) kann sich kein
Code auf ihn berufen, solange er nicht hier steht.
