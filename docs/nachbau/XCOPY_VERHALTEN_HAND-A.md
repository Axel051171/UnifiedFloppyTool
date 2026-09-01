# ⚠ HAND-A-MATERIAL — wer das liest, ist für die Umsetzung verbrannt

**Nicht lesen, wenn du etwas davon implementieren sollst.** Die saubere
Übergabe ist [`XCOPY_PRUEFAUFTRAEGE.md`](XCOPY_PRUEFAUFTRAEGE.md) — sie
enthält Anforderungen an UFTs eigenes Verhalten und sonst nichts.

Diese Datei hält Verhaltensbefunde über eine **fremde, proprietäre
Vorlage** fest. Sie enthält bewusst keine Codestruktur, keine Zerlegung
in Routinen, keine Namensgebung und kein Datenlayout der Vorlage — nur
Tatsachen über das Medium und Verfahrensideen auf Verhaltensebene, wie
sie unabhängig durch Messung an echten Disketten reproduzierbar wären.

---

## Gegenstand und Rechtsstand

Vorlage: die 68k-Assemblerquelle von X-Copy Professional 5.3 (Stand
September 1992) in der 2011 auf vasm/vlink portierten Fassung.
Untersucht wurde ausschließlich das Disk-Modul; das Systemmodul
(Oberfläche, Copperlisten, CIA-Timer, Bootblock-Virenscanner) ist
OCS/ECS-gebunden und für UFT ohne Belang.

**Rechtsstand — doppelt belegt, aus zwei unabhängigen Richtungen:**

| Quelle | Aussage |
|---|---|
| `xcopy_licence.txt` (Archiv 1) | *„free for private, non-commercial use. Commercial or governmental use is not permitted."* — Weitergabe nur vollständig und kostenlos; © 1988-2011 Anguilla Software International Ltd.; **Bearbeitung kommt nicht vor** |
| Gutachten §0.2 | die Quelle selbst enthält **keine** Lizenzdatei und keinen Freigabevermerk; im Oberflächenmodul ein ASI-Copyright von 1992 mit „alle Rechte vorbehalten" |

Bestätigt durch Messung: Archiv 2 (`…_v2.zip`) enthält **nur** das ADF,
keine Lizenzdatei. Beide Lesarten decken sich.

**Folge:** Übernahme von Code, Codestruktur, Zerlegung, Namensgebung
oder Datenlayout ist ausgeschlossen. Siehe `docs/OPEN_ITEMS.md` MF-744.

## Kontaminationsvermerk

Der Verfasser des Gutachtens hat die Vorlage im Volltext gelesen und ist
für die Umsetzungsseite verbrannt.

**Ich ebenfalls** — ich habe dieses Dokument gelesen, nicht nur §4. Für
eine Umsetzung dieser Verhaltensweisen bin ich damit Hand A und
ausgeschlossen. Das *Auditieren* von UFTs eigenem Verhalten (die
Prüfaufträge) bleibt erlaubt und ist Hand-A-Arbeit.

Sichtprotokoll: `work/sichtprotokoll.json`. Beide ADFs sind
**ungeöffnet** und stehen nicht darin.

## Kennzeichnung

* **MEASURED** — im Quelltext unmittelbar ablesbar (Konstante, Vergleich,
  Autorenkommentar, Änderungsprotokoll)
* **INFERRED** — Deutung. Nicht belegt, sondern erschlossen. Vor
  Übernahme eigenständig zu prüfen.

---

## Grundannahme des Verfahrens

**INFERRED.** Das Verfahren verweigert bewusst die Interpretation des
Spurinhalts: eine Spur ist ein Bitstrom unbekannter Struktur, der
längen- und lagetreu übertragen werden soll. Die Sektorschicht wird nur
dort betreten, wo sie sich zweifelsfrei nachweisen lässt; überall sonst
gilt der Rohstrom als die Wahrheit.

Dieselbe Haltung wie unsere EINFRIER-REGEL — kein Format annehmen, das
nicht belegt ist. Unterschied: die Vorlage arbeitet auf
MFM-Bitstrom-Ebene, UFT eine Stufe tiefer auf Flusswechseln.

## Aufnahme

**B1 — zwei volle Umdrehungen, ausgelöst am Index.** *MEASURED:*
Aufnahme wird über den Index-Interrupt scharfgeschaltet und beginnt mit
dem nächsten Indexpuls; Puffer 26 624 Byte (etwas mehr als zwei
Umdrehungen), hinterer Teil vorher genullt. *INFERRED:* zwei Umdrehungen
sind das Minimum, um Spurlänge und Nahtstelle ohne Kenntnis der
Umdrehungsdauer zu bestimmen.

**B2 — Spurlänge wird gemessen, nicht angenommen.** *MEASURED:*
Rückwärtssuche nach dem letzten beschriebenen Wort; Füllstand halbiert =
Länge einer Umdrehung. Vorgabe bei Lesefehler 12 480 Byte, obere
Schranke 13 312 Byte je Umdrehung. *INFERRED:* der beschriebene
Pufferbereich ist ein direktes Maß für die Umdrehungsdauer des konkreten
Laufwerks mit der konkreten Diskette. Die Vornullung ist konstitutiv.

**B3 — Schreiblänge je Ziellaufwerk kalibriert.** *MEASURED:* vor dem
ersten Schreiben wird die Spur mit gleichförmigem Muster überschrieben
und nach B1/B2 gemessen; Ergebnis je Laufwerk zwischengespeichert. 32
Byte Abzug, ausdrücklich als Reserve für Drehzahlschwankungen.
Geschrieben wird das Minimum aus Ziel- und Quelllänge. *INFERRED:* der
Befund mit dem höchsten Übertragungswert — zwei Laufwerke desselben Typs
drehen unterschiedlich schnell, eine global angenommene Schreiblänge
schneidet ab oder überschreibt die Nahtstelle. → **P1**

**B4 — Nachsuche bitgenau.** *MEASURED:* jedes Fenster wird in allen
sechzehn Bitlagen geprüft. *INFERRED:* die technische Trennlinie
zwischen Sektorleser und Nibbler. Kopierschutz legt Syncs bewusst
bitversetzt; wer das nicht kann, klassifiziert geschützte Spuren
fälschlich als leer. → **P2**

**B5 — erweiterter Sync-Satz.** *MEASURED:* indexbezogen nur `$4489`;
im Nibbler-Modus zusätzlich `$448A`, `$9521`, `$A245`, `$A89A`. Ein
**auskommentierter Rest** ordnet drei davon Titeln zu. *INFERRED:*
Tatsache über existierende Medien, an denen unabhängig nachzumessen ist
— Korpusmaterial, kein Codematerial. Der Satz ist gewachsen, nicht
systematisch. → **P8**

**B6 — Nahtstelle über die zweite Umdrehung.** *MEASURED:* gleiche
Position auf Umdrehung zwei aufsuchen; aus dem Abstand Spurlänge und
Schreibstartpunkt. Liegt der Startpunkt zu nah am Pufferanfang, wird auf
Umdrehung zwei ausgewichen. Höchstens 24 Syncs je Spur, Mindestabstand
256 MFM-Byte nach einem Treffer. → **P6**

## Bewertung und Fehlerbehandlung

**B7 — vier Verdikte plus zwei Sonderfälle.** *MEASURED:*

| Einstufung | Bedingung |
|---|---|
| Standardspur | `$4489`, genau 11 Sektoren, durchgehend 1088 MFM-Byte Abstand |
| Rohspur | Syncs gefunden, aber nicht dem Standard entsprechend |
| Nahtstellen-Schutz | keine Syncs, aber Nahtstellenmuster nachgewiesen |
| Kein Sync | nichts gefunden |

Sonderfälle: ab 13 056 Byte gemessener Länge → überlange Spur; ein
nachgewiesener Nahtstellen-Schutz setzt die Länge auf null.
*INFERRED:* die Standardspur ist **nicht** über das Sync-Muster allein
definiert, sondern über die Konjunktion aus Sync, Sektorzahl und
gleichmäßigem Abstand. Was nur *aussieht* wie eine Standardspur, fällt
in die Rohspur-Klasse — eine Sicherheitsentscheidung zugunsten der
Rohkopie. → **P4**, **P5**

**B8 — Nahtstellen-Erkennung bei syncfreien Spuren.** *MEASURED:* Suche
nach Byte-Diskontinuitäten in einem sonst gleichförmigen Muster; bis zu
fünf gelten als Schutzmerkmal, ab der sechsten wird die Spur verworfen.
Wird das Merkmal erkannt, wird ab der letzten Bruchstelle aufgefüllt und
der Pufferanfang um 768 Byte erweitert. *INFERRED:* **das Auffüllen ist
eine Rekonstruktion — die Vorlage erzeugt Daten, die sie nicht gelesen
hat.** Für ein Preservation-Werkzeug ein Warnbefund: so etwas darf
allenfalls als gekennzeichnete Ableitung geführt werden, niemals als
gelesene Daten. → **P4-Nebenauflage**

**B9 — sechs benannte Fehlerklassen.** *MEASURED:* zu wenige Sektoren;
kein Sync; kein zweiter Sync; Kopf-Prüfsummenfehler; Formatfehler im
Kopf; Daten-Prüfsummenfehler. Verify führt eine siebte, getrennte
Kennung. *INFERRED:* die Trennung „kein Sync" / „kein zweiter Sync" ist
diagnostisch wertvoll — sie unterscheidet eine unlesbare Spur von einer
zu kurz geratenen Aufnahme. Zwei völlig verschiedene Ursachen: Medium
gegen Aufnahmeparameter.

**B10 — abgestufte Wiederholung mit Rückfall auf Rohkopie.**
*MEASURED:* drei Leseversuche, danach Bedienerabfrage und Rohkopie; im
Verify drei Versuche je Ziellaufwerk mit Neuschreiben dazwischen.
*INFERRED:* ein Degradationspfad, kein Reparaturpfad — die Spur wird
danach übertragen, aber nicht mehr verstanden.

**B11 — Ehrlichkeitsregel bei degradiertem Erfolg.** *MEASURED:* das
Änderungsprotokoll zu **Version 5.21** dokumentiert eine ausdrückliche
Korrektur: zuvor wurde eine nach Lesefehler im Rohverfahren gerettete
Spur als fehlerfrei angezeigt. Die Autoren stufen das als falsch ein und
lassen die Fehlermarkierung seitdem stehen — der Lesefehler ist real und
besteht auf der Zieldiskette fort.

> *INFERRED:* **derselbe Grundsatz wie unser „kein stiller
> Datenverlust", dreiunddreißig Jahre früher und aus derselben Ursache.**
> Ein Werkzeug, das Rettung und Erfolg gleich anzeigt, erzeugt falsche
> Sicherheit beim Archivar. Gehört nicht in den Code, sondern in die
> Begründung. → **P3**

**B12 — Verify vergleicht, aber nicht überall.** *MEASURED:* der
Verify-Durchgang liest zurück, dekodiert und vergleicht wortweise gegen
den geschriebenen Puffer, je Ziellaufwerk getrennt. **Rohkopierte Spuren
werden übersprungen.** *INFERRED:* konsequent, weil der Vergleich am
dekodierten Ergebnis hängt — und zugleich die größte Lücke des
Verfahrens: gerade die Spuren, bei denen etwas schiefging, werden nicht
nachgeprüft. Ein Rückvergleich auf Rohstromebene wäre möglich gewesen,
vermutlich am Speicher einer 512-KB-Maschine gescheitert. → **P7**

**B13 — Beruhigungszeiten sind Teil des Verfahrens.** *MEASURED:* nach
jedem DMA-Vorgang 500 µs feste Wartezeit, im Quelltext mit dem
Ausschwingen des Controllers begründet. Beim Anfahren der Startspur
werden die Laufwerke einzeln gesteppt, danach gemeinsame Pause.
Schreibschutz aller Ziellaufwerke wird vorab geprüft.

---

## Was daraus NICHT folgt

**Kein Nachbau des Amiga-Schutzmoduls.** MF-740 hat das entschieden und
MF-744 bestätigt: die vier Sync-Werte sind zu 1 von 4 belegt, zwei
werden widersprochen, und die abgeleiteten Dateien haben null Aufrufer.

**Die Befunde sind Prüfaufträge, keine Bauaufträge.** Jeder B-Befund
mündet in eine Frage an *unser* Verhalten, nicht in eine Vorlage für
*unseren* Code. Wo eine Zahl der Vorlage genannt ist (26 624, 12 480,
1088, 768), steht sie als **Beleg für die Beobachtung**, nicht als zu
übernehmender Wert — jede davon ist an echten Medien nachzumessen.


---

## Nachtrag: die überarbeitete Gutachten-Fassung (2026-09-01)

Der Eigentümer hat eine zweite, vollständige Fassung geliefert. Sie
ersetzt die erste. Drei Punkte sind neu und belastbar:

**§0.1 — der ausgewertete Stand ist nachweislich der von 1992.**
Prüfsummengleich mit dem als „original" bezeichneten Archiv. Beide
Stände wurden vollständig verglichen: im Disk-Modul unterscheiden sie
sich in **keiner einzigen Zeile**; alle fünfzehn Abweichungen liegen im
Bootblock-Installationsteil. **Folge: B1–B13 sind Verhalten von 1992**,
keine Zutat der Bearbeiter von 2011.

**§0.2 — die Rechtseinstufung wurde korrigiert**, unabhängig und
zeitgleich zu meiner eigenen Berichtigung in MF-746. Beide kommen zum
selben Ergebnis: es gibt eine autorisierte Freigabe mit
Weiterentwicklungserlaubnis, gebunden an „unentgeltlich" und „eigenen
Quellcode mitveröffentlichen", und die Route ändert sich nicht — nur
ihre Begründung. Nicht mangels Erlaubnis, sondern wegen
**Lizenzunverträglichkeit**.

**§5.4 — eine Eichmöglichkeit, die vorher fehlte.** Zum Quellmaterial
gehören zeitgenössische lauffähige Binaries. Das Originalprogramm kann
unter Emulation gegen dieselben Fixtures laufen wie die Prüfaufträge
und als **Oracle für die erwarteten Verdikte** dienen. Nicht anwendbar
auf B3 und B13 — Kalibrierung und Beruhigungszeiten hängen an echter
Mechanik. → als **P9** in die Prüfaufträge übernommen.

### Zwei Antworten zurück an den Verfasser

**1 · Die in §0.2 vermisste Lizenz existiert — ich habe sie.**

§0.2 sagt: *„Diese Lizenz ist im vorliegenden Material nicht
auffindbar. […] Der Verweis läuft damit ins Leere."* Sie lag in einem
**anderen Archiv**: `xcopypro_source_2011.zip` enthält neben dem
Abbild eine separate Datei `xcopy_licence.txt` (1485 Byte). Wortlaut
oben unter „Rechtsstand".

Das schließt §5.3, dritter Punkt („der Umfang der Freigabe ist nur so
weit bestimmt, wie die Freigabeerklärung reicht"). Der Umfang ist
jetzt bestimmt — und er **verschärft** die Lage: neben „don't sell it"
tritt „private, non-commercial use" und „governmental use is not
permitted". Letzteres trifft unsere Zielgruppe direkt.

**2 · §0.2 fragt, unter welcher Lizenz UFT steht. Das ist eine
Tatsache, keine Entscheidung.**

| Quelle | Aussage |
|---|---|
| `LICENSE` | GNU General Public License **Version 2**, June 1991 |
| `CONTRIBUTING.md` §Licensing | *„Code written for UFT carries `SPDX-License-Identifier: GPL-2.0-or-later`."* |

Damit ist §0.2s Frage („ob überhaupt eine Konstellation denkbar ist, in
der direkte Übernahme in Frage käme") beantwortet: **nein.** Die GPL
erlaubt den Verkauf ausdrücklich und verbietet in §6 zusätzliche
Beschränkungen. Ein „don't sell it" ist genau eine solche. Es gibt
keine Fassung von GPL-2.0-or-later, unter der die Übernahme ginge.

Die Spec-Route ist damit nicht nur die sicherere, sondern die einzige.
