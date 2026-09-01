# X-Copy-Verhaltens-Spec — zweites Hand-A-Blatt (für Hand B lesbar)

**Diese Datei ist die Spec-Hälfte eines Hand-A-Ergebnisses und darf
von Hand B gelesen werden.** Sie enthält ausschließlich
Verhaltensaussagen („bei Zustand X erscheint Ziffer Y") mit
Beleg-IDs. Die Belege selbst — Fundstellen, Regionen, alles
Quellennahe — liegen in der **Hand-A-Akte**
`XCOPY_BELEGE_HAND-A2.md`; **wer die öffnet, ist für die Umsetzung
verbrannt.** Von hier wird nur per Beleg-ID `[B-nn]` dorthin
verwiesen, nie umgekehrt.

Ergänzt [`XCOPY_VERHALTEN_HAND-A.md`](XCOPY_VERHALTEN_HAND-A.md)
(Blatt 1, selbst Hand-A-Material) und beantwortet die Fragen E1–E7
aus [`XCOPY_EMULATIONSSITZUNG.md`](XCOPY_EMULATIONSSITZUNG.md).

**Geltungsbereich:** alle MEASURED-Aussagen gelten für die Fassung
5.3, Stand September 1992 `[B-14]`. Sie sind als **Vorhersagen**
formuliert, die die Emulationssitzung bestätigen oder widerlegen
kann — Sichtung + Emulation ergeben zusammen die Zwei-Quellen-Lage,
die Werkstattregel 3 für Tier-Urteile verlangt.

**Evidenzklassen:** MEASURED (am Bestand abgelesen, Beleg-ID nennt
wo) · DOKUMENTIERT (aus einem Begleitdokument des Bestands) ·
INFERRED (Folgerung; vor Übernahme prüfen) · **[ZU VERIFIZIEREN]**
(einquellig; blockiert jedes Tier-Urteil).

Begriffe: „Sektormodus" = die sektorbasierten Kopierarten
(DOSCOPY-Familie), „Rohmodus" = NIBBLE, „Indexmodus" = INDEXCOPY,
„RAM-Kopie" = die Ein-Laufwerk-Kopie über den Speicher — alles
benutzersichtbare Betriebsarten.

---

## E1 — Leere Spur: Ziffer 2, und es gibt eine feste Reihenfolge

* **V-01** MEASURED `[B-01][B-02]` — Eine Spur, die das Wort `$4489`
  nirgends trägt, liefert im Sektormodus kein Lesegut; jeder
  Leseversuch endet erst über eine Zeitschranke von 917 000 µs
  (≈ 0,92 s) und wird als „kein Sync" gewertet.
* **V-02** MEASURED `[B-01][B-03]` — **Eine leere Spur zeigt am Ende
  Ziffer 2, nicht 1.** Ziffer 1 kann für eine Spur gänzlich ohne
  `$4489` nie erscheinen, denn sie setzt eingelesenes Gut voraus.
* **V-03** MEASURED `[B-02]` — Insgesamt drei Leseversuche; die
  Ziffer wird je Anlauf angezeigt. Nach dem dritten Fehlversuch
  fällt DOSCOPY auf die Rohkopie zurück, und **die rote Ziffer
  bleibt stehen** (siehe E5).
* **V-04** MEASURED `[B-03][B-04][B-05]` — **Vorrangkette der
  Direktkopie (Disk→Disk):**
  1. Kommt kein Lesegut zustande → **2**.
  2. Am eingelesenen Gut, vor jeder Sektorprüfung: sagt der zuerst
     gelesene Sektorkopf mehr als 11 Sektoren bis zur Lücke an →
     **1**; findet sich im Fenster von rund 2 050 MFM-Bytes hinter
     der aus dieser Ansage errechneten Lückenposition kein weiterer
     Sync → **3**.
  3. Danach je Sektor, beginnend bei Sektor 0: Sync fehlt an der
     erwarteten Stelle → **2**; Kopfprüfsumme falsch → **4**;
     Kopfinhalt (Formatkennung/Spurnummer) falsch → **5**, und zwar
     auch dann, wenn zusätzlich die Kopfprüfsumme falsch ist (5
     verdrängt 4); die Datenprüfsumme wird **nur bei fehlerfreiem
     Kopf überhaupt bewertet** → **6**.
  4. Der **erste** fehlerhafte Sektor entscheidet die Ziffer.
* **V-05** MEASURED `[B-06]` — **Abweichung in der RAM-Kopie:**
  dieselben Ziffern, aber dort entscheidet der **letzte** gefundene
  Fehler; eine unzulässige Sektornummer verdrängt dort Kopffehler
  mit **1**, ein Datenprüfsummenfehler verdrängt alles Vorherige
  mit **6**.
* **V-06** INFERRED `[B-15]` **[ZU VERIFIZIEREN]** — Beim ersten
  Anlauf einer leeren Spur kann für gut anderthalb Sekunden (zwei
  Wiederholungen à ~0,9 s) eine **andere rote Ziffer** stehen
  (typisch 5), ehe die Anzeige auf 2 einschwenkt. Die Emulation
  kann genau dieses Flackern sichtbar machen.

## E2 — „mehr als 11": nur über die Kopf-Ansage, keine Zählung

* **V-07** MEASURED `[B-04]` — Die einzige wirksame Schranke gegen
  zu viele Sektoren ist die **Ansage im zuerst gelesenen
  Sektorkopf**: meldet er mehr als 11 Sektoren bis zur Lücke,
  erscheint 1.
* **V-08** MEASURED `[B-04][B-05]` — Eine vom Standard abweichende
  **tatsächliche** Spanne zwischen erstem Sync und dem Sync hinter
  der Lücke löst für sich genommen **keine** Fehlermeldung aus; eine
  unabhängige Zählung der real vorhandenen Sektoren findet nicht
  statt, und auch die **Sektornummern** werden in der Direktkopie
  nicht geprüft.
* **V-09** INFERRED **[ZU VERIFIZIEREN]** — Eine standardförmige
  12-Sektor-Spur zeigt darum je nach Drehlage beim Lesestart mal
  **1** (wenn der zuerst erwischte Kopf „12 bis Lücke" ansagt), mal
  läuft sie **als fehlerfrei durch** — dann wird nur ein
  11-Sektoren-Bild geschrieben und ein Sektor geht stillschweigend
  verloren. Erwartung für die Emulation: über mehrere Versuche
  wechselndes Ergebnis. Die Handbuch-Formel „less or more than 11
  sectors" beschreibt die Ansage-Schranke, nicht eine Zählung.
  **UFTs beidseitige Zählprüfung ist feiner als die Vorlage** — bei
  der Hebung so protokollieren, nicht angleichen.

## E3 — Ziffer 3 hat mit einer zweiten Umdrehung nichts zu tun

* **V-10** MEASURED `[B-01]` — Der Sektormodus liest je Versuch
  12 992 MFM-Bytes, hart beginnend am ersten `$4489` — gut **eine**
  Umdrehung. Eine zweite Umdrehung kommt in diesem Modus nicht vor.
* **V-11** MEASURED `[B-03][B-04]` — Ziffer 3 erscheint, wenn ein
  erster Sync existiert, aber im Fenster von rund 2 050 MFM-Bytes
  hinter der errechneten Lückenposition kein weiterer Sync liegt.
* **V-12** MEASURED `[B-10]` — Im Rohmodus (zwei Umdrehungen) gibt
  es **keine** Ziffer 3; dort nur rote 2 („kein Sync") und rote 7
  („überlange Spur").
* **V-13** Folgerung — X-Copy trennt weder „zweite Umdrehung ohne
  Sync" noch „nur eine Umdrehung gelesen"; beide Begriffe haben
  keine Entsprechung. UFTs Unterscheidung FEHLT/UNBEKANNT ist eine
  **UFT-eigene Verfeinerung** ohne X-Copy-Herkunft.
* **V-14** INFERRED — Der Emulator spielt SCP-Umdrehungen als
  Endlos-Strom ab; welcher Teil ins Ein-Umdrehungs-Fenster fällt,
  hängt an der Drehlage → Ergebnis kann über Versuche wechseln,
  mehrfach messen.

## E4 — `$448A`: im Rohmodus eigenes Muster, im Sektormodus unsichtbar

* **V-15** MEASURED `[B-01]` — Der Sektormodus reagiert
  ausschließlich auf `$4489`. Eine Spur, deren Marken sämtlich
  `$448A` sind, zeigt dort **2** und fällt (bei DOSCOPY) auf die
  Rohkopie zurück.
* **V-16** MEASURED `[B-08]` — Im Rohmodus ist `$448A` einer von
  **fünf** Suchkandidaten für die erste Marke — `$4489`
  (benutzereinstellbar, Standardwert), `$448A`, `$9521`, `$A245`,
  `$A89A` — bitgenau in allen 16 Bitlagen geprüft. Nach dem ersten
  Treffer wird für den Rest der Spur **genau der getroffene Wert**
  weiterverfolgt. Im Indexmodus wird ausschließlich `$4489` gesucht.
* **V-17** MEASURED `[B-09][B-10]` — Eine `$448A`-Spur gilt als
  sync-tragende **Rohspur** (blaue 0), niemals als Standardspur:
  die Standard-Einstufung verlangt den Wert `$4489` **und** genau
  11 Sektoren **und** durchgehend 1088 MFM-Bytes Abstand.
* **V-18** Folgerung — `$448A` in `UFT_AMIGA_SYNCS` zu führen ist
  gedeckt, aber als **Roh-Marke** („hier ist etwas"), nicht als
  Standard-Sync mit Sektor-Semantik. Ob reale Medien `$448A`
  tragen, bleibt Korpusfrage (Blatt 1, P8).

## E5 — 5.3 zeigt „gerettet" rot; für 3.4 fehlt das Material

* **V-19** DOKUMENTIERT `[B-13]` — Das Änderungsprotokoll des
  Bestands dokumentiert für Fassung 5.21: zuvor (5.2) erschien nach
  der Rohkopie-Rettung eines Lesefehlers eine **grüne 0**, seither
  bleibt die **rote Ziffer** stehen, weil der Lesefehler auf der
  Zielscheibe fortbesteht.
* **V-20** MEASURED `[B-02][B-06]` — Der Stand 09/1992 (5.3)
  verhält sich korrigiert: nach dem Rückfall bleibt die
  ursprüngliche rote Ziffer; in der RAM-Kopie ist die Roh-Bewertung
  kurz sichtbar, danach erscheint wieder die ursprüngliche Ziffer.
* **V-21** unbelegt `[B-13]` — Das Änderungsprotokoll reicht nur bis
  5.1 zurück. Ob **3.4** den Rückfall überhaupt besitzt und wie es
  anzeigt: **nur über Beobachtung am Binary belegbar →
  Emulationssitzung** (der einzige Punkt, der zwei Binaries
  nebeneinander braucht).

## E6 — Kopf- und Datenprüfsummenfehler zugleich: 4 — aber nur in der Direktkopie

* **V-22** MEASURED `[B-05]` — Direktkopie: die Datenprüfsumme
  eines Sektors wird **gar nicht bewertet**, wenn schon sein Kopf
  fehlerhaft ist — die Doppellage zeigt **4**. Ist zusätzlich der
  Kopfinhalt falsch: **5**.
* **V-23** MEASURED `[B-06]` — RAM-Kopie: dieselbe Doppellage zeigt
  **6** (letzter Fehler gewinnt).
* **V-24** Folgerung — E6 in **beiden** Betriebsarten messen; eine
  Einzelmessung beantwortet die Frage nur für einen Pfad. Für
  `uft_track_verdikt.c` heißt das: die Vorlage hat **keine eine**
  Vorrangkette, sondern zwei einander widersprechende. Welcher man
  folgt (oder keiner), ist eine UFT-Entscheidung, keine Messfrage.

## E7 — Schreibstartpunkt: Ableitungsregel steht, inkl. Ausweichfall

* **V-25** MEASURED `[B-11]` — Rohmodus: Schreibstart ist die
  Stelle **10 MFM-Bytes vor dem Sync, der auf die Spurlücke
  folgt**. Die Lücke ist die am **seltensten vorkommende**
  Abstandsklasse zwischen aufeinanderfolgenden Syncs
  (Klassen-Toleranz ±32 Bytes, höchstens 24 Syncs je Spur,
  Mindestabstand 256 MFM-Bytes nach einem Treffer).
* **V-26** MEASURED `[B-11]` — **Ausweichfall: ja** — liegt dieser
  Punkt zu dicht am Aufnahmebeginn (erster Sync unmittelbar hinter
  dem Index), wird die gleichwertige Stelle auf der **zweiten**
  aufgenommenen Umdrehung benutzt. Das Roh-Schreiben selbst wird am
  **Indeximpuls** ausgelöst.
* **V-27** MEASURED `[B-11]` — Indexmodus: Schreibstart 2 Bytes vor
  der Position des zweiten Index. Syncfreie Rohspur ohne
  Bruchstellenbefund: Bild ab der Position des zweiten Index.
* **V-28** MEASURED `[B-12]` — Sektormodus: **keine** Indexbindung —
  geschrieben wird ab der zufälligen momentanen Drehlage; vor dem
  Spurbild gehen rund 1 000 Bytes neutrales Füllmuster voraus, dann
  folgt Sektor 0. Die Kopie normalisiert die Drehlage **im Bild**,
  nicht auf der Scheibe.
* **V-29** Folgerung — P3-7 kann von „unbeantwortbar" auf „Regel
  bekannt, Nachmessung offen" gehoben werden; die Nachmessung am
  realen Medium bleibt hardwaregebunden (MF-310).

## Anzeigetafel (Querschnitt, für die Tripel-Tabelle)

* **V-30** MEASURED `[B-07][B-10][B-03]` — Ziffer 0 erscheint grün;
  die Ziffern 1–8 rot; Fehlerwerte ab 9 als blaue 9. Verify-Fehler
  ist die 8. Rohmodus-Verdikte erscheinen als **0** in der
  Verdiktfarbe (grün = standardgleich, blau = Rohspur mit Syncs,
  hellgrau = Bruchstellen-Schutz, gelb = Schreib-/Zwischenzustand)
  sowie als rote **2** (kein Sync) und rote **7** (überlange Spur,
  ab 13 056 Bytes gemessener Umdrehungslänge).

---

## Nur über Beobachtung am Binary belegbar / offen

* **E5 für Fassung 3.4** — Material fehlt im Bestand →
  Emulationssitzung mit dem 3.4-Binary.
* **Übergangsbilder E1/E2** (Flacker-Ziffern, Drehlagen-
  Abhängigkeit) — hier nur INFERRED-Vorhersagen (V-06, V-09);
  Bestätigung nur am laufenden Binary.
* **E7-Nachmessung am realen Medium** — hardwaregebunden (MF-310);
  die Ableitungsregel selbst ist belegt.
* **Rohmodus auf echtem Rauschen** (entmagnetisierte Spur): ob
  Zufallsbitfolgen die Fünf-Kandidaten-Suche treffen und eine leere
  Spur als „Rohspur" durchgeht, hängt am Analogverhalten des
  Laufwerks — nur real messbar.

Kontaminations-Grundlinie, Inventar- und Lizenzbefund liegen
vollständig in der Hand-A-Akte (`[B-16]`–`[B-18]`) — sie sind
Eigentümer- und Audit-Material, kein Spec-Inhalt.
