# Lizenzmatrix (bindend, Ziel: GPL-2.0-Projekt)

Erkennung IMMER aus LICENSE/COPYING-Datei(en), je Unterverzeichnis
geprüft (Vendoring!). README-Behauptungen sind Hinweis, nie Beleg.

| Zone | Lizenzen | Code portieren | Konzept nachbauen | Oracle-Binary | Anmerkung |
|---|---|---|---|---|---|
| GRÜN | MIT, BSD-2, BSD-3, ISC, Zlib, PD/CC0, GPL-2.0(-only/or-later), LGPL-2.1, MPL-2.0 | ✅ mit Attribution-Header | ✅ | ✅ | samdisk-Muster: Quelle+Commit im Header |
| GELB | GPL-3.0, AGPL, Apache-2.0 | ❌ (GPL-2.0-inkompatibel) | ✅ Verhaltens-Spec | ✅ | Apache-2.0 wird oft fälschlich für kompatibel gehalten — ist es mit GPLv2 nicht |
| ORANGE | BSD-4-Clause | ❌ einlinken | ✅ | ✅ | Helper-Prozess-Weg möglich (DiskFlashback-Muster) — Eigentümer-Vorlage |
| ROT | keine Lizenzdatei, proprietär, NC/ND-Klauseln | ❌ | ✅ (nur aus Doku/Blackbox) | ✅ falls Binary legal beziehbar | „keine Datei" = alle Rechte vorbehalten |
| GELB-GRÜN | **EUPL-1.2** | ⚠️ nur über die Kompatibilitätsklausel, je Fall Eigentümer-Vorlage | ✅ Verhaltens-Spec | ✅ | siehe Fußnote (1) |
| PRÜFEN | Dual-Lizenz, Datei-Header ≠ Repo-Lizenz, Datenbanken (sui generis) | — | — | — | IMMER Eigentümer-Vorlage; Daten ggf. per Laufzeit-Parser statt Kopie (diskdefs-Muster) |
| PRÜFEN | **jede Lizenz, die in dieser Tabelle nicht steht** | — | — | — | Meta-Regel (2): der Agent ordnet nie selbst ein |

### (1) EUPL-1.2 — einseitig, und deshalb keine GRÜN-Zeile

Aufgenommen nach dem gwnbd-Zyklus (MF-677), wo sie zum ersten Mal
auftauchte und den Scout richtigerweise zur Vorlage zwang.

Die EUPL-1.2 trägt eine **Kompatibilitätsklausel** (Art. 5) mit einem
Anhang, der GPL-2.0 als kompatible Lizenz nennt. Eine Kombination aus
EUPL-Werk und einem Werk unter kompatibler Lizenz darf danach unter
jener Lizenz weitergegeben werden.

Warum das trotzdem **nicht GRÜN** ist, sondern eine eigene Zeile:

* Die Wirkung ist **einseitig**. Sie erlaubt, eine Kombination unter
  GPL-2.0 weiterzugeben — sie macht EUPL-Code nicht allgemein
  GPLv2-kompatibel, und sie sagt nichts über den umgekehrten Weg.
* Die Auslösebedingung ist eine **Rechtsfrage am Einzelfall** (was
  genau als Kombination gilt, was als bloßes Nebeneinander), und sie
  wirkt **stromabwärts** auf jeden, der das Ergebnis weitergibt.

Deshalb: Verhaltens-Spec und Oracle ohne Rückfrage. Ein Port **nur**
mit Eigentümer-Vorlage je Fall, mit benannter Quelle und Commit im
Header wie beim samdisk-Muster.

### (2) Unbekannte Lizenz = automatisch PRÜFEN

Der Agent ordnet **nie selbst** ein. Steht eine Kennung nicht in der
Tabelle oben, ist die Zone PRÜFEN — auch dann, wenn sie „offensichtlich
permissiv" aussieht.

Der Grund ist die Erfahrung mit EUPL-1.2: sie sieht wie eine gewöhnliche
freie Lizenz aus, ihre GPLv2-Verträglichkeit hängt aber an einer Klausel
mit Anhang und Einzelfallwirkung. Ein Agent, der so etwas nach Augenmaß
als GRÜN durchwinkt, produziert genau die Sorte Fehler, die erst beim
Verteilen auffällt — und dann bei jemand anderem.

Der Scout hat das im gwnbd-Zyklus von sich aus richtig gemacht. Diese
Zeile macht daraus eine Regel, damit es nicht Ermessen bleibt.

Sonderfälle aus der Praxis dieses Projekts:
- **Fakten/Parameter** (Geometrien, Timing-Werte): als Werte frei, als
  kuratierte Sammlung ggf. geschützt → Laufzeit-Parser oder eigene
  Erhebung mit Provenienz.
- **GPLv2+ in GPL-2.0-Projekt**: zulässig; SPDX der portierten Datei
  korrekt führen (Lehre aus P0-5).
- **Zwei GELB/ORANGE-Quellen mischen** (z. B. GPL-3-Port neben
  BSD-4-Link im selben Binary): eigene Prüfung je Kombination,
  Eigentümer-Vorlage.
