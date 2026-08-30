# Gutachten: hpingel/pyAccess1581

> Gemessen 2026-08-30 gegen HEAD `291554f14f` (letzter Commit 2019-10-27).
> Messdatei: `tools/uft-scout/work/pyAccess1581.messung.json`.
> Inventar: `tools/uft-scout/work/inv.json` (SSOT ok, 88 Plugins, 24
> Korpus-Abbilder, UFT-HEAD `227b6121`).
> Auftrag: Eigentümer, 2026-08-30 — ausdrückliche Frage: „kann uns hier
> irgendetwas helfen — auch wenn es Python ist?" mit drei Prüfaufträgen
> (Fähigkeitslücke / Oracle neben `c1541` / Fixture-Erzeuger).

## Kategorie

**Fundus in drei Posten (GPL-3.0)** — ein 856-Zeilen-Python-Werkzeug
(gezählt: `wc -l access1581/*.py` = 856), das ein echtes 3,5"-PC-Laufwerk
über die Arduino-Hardware von Rob Smiths „ArduinoFloppyDiskReader"
(DrawBridge-Linie) ansteuert und daraus D81- bzw. DOS-IMG-Abbilder baut.
**Kein Posten bewegt eine der vier Kennzahlen** (Begründung je Posten
unten) — darum null OPEN_ITEMS-Vorschläge. Das ist das nach Regel 9
zulässige Urteil, nicht ein „nichts gefunden": alle drei Posten stehen
unten mit dem, was ihren Kanal öffnen würde.

## Lizenz

* `LICENSE` (Wurzel) = **GPL-3.0** — „GNU GENERAL PUBLIC LICENSE /
  Version 3, 29 June 2007", Zeilen 1-2 der Datei. Messung:
  `lizenz_zone: GELB`, `lizenz_je_datei_vollstaendig: true`, 8 Dateien
  geprüft, einzige gefundene Kennung GPL (5 Quellkopf-Nennungen).
* Jeder Quellkopf wiederholt GPL-3-or-later (z. B.
  `access1581/imager.py:7-10`). **Kein vendorter Fremdschnipsel mit
  abweichender Lizenz gefunden**: die einzigen Fremd-Nennungen sind
  Credit-Blöcke für Rob Smith (`arduinointerface.py:20-27`,
  `diskformats.py:20-27`), dessen Projekt dort selbst als „GPL V3"
  benannt wird — es wird aber nur dessen **serielles Kommandoprotokoll**
  nachimplementiert, kein Code übernommen.
* **Konsequenz nach `playbook/lizenzmatrix.md`:** Zone GELB — kein Port
  in ein GPL-2.0-Projekt. Zulässig: Verhaltens-Spec, Oracle-Ausführung,
  Helfer-Prozess (separater Python-Prozess, nichts wird eingelinkt).
* **Attribution:** pyAccess1581, Henning Pingel, GPL-3.0-or-later;
  dessen Protokoll-Vorlage: ArduinoFloppyDiskReader, Robert Smith,
  GPL-3.0 (laut Credit-Block; im Rahmen dieses Gutachtens nicht am
  Fremd-Repo nachgemessen).
* **Fixture-Sonderfrage:** das mitgelieferte `raw_debug_image_d81.zip`
  fällt unter dieselbe GPL-3.0. Nach der Korb-Regel („Fixture-Lizenz wie
  Code-Lizenz", `tools/uft-innendienst/out/korb.md`) wäre eine
  Korpus-Aufnahme eine **Eigentümer-Vorlage** (PRÜFEN): GPL-3-Datenwerk
  neben GPL-2-Baum ist Aggregation, keine Verlinkung — aber diese
  Einordnung steht nicht in der Matrix, also lege ich sie vor, statt sie
  zu treffen.

## 1. Was es ist (gemessen, nicht aus dem README)

Vier Module (`access1581/`): `arduinointerface.py` (371 Z.) spricht das
DrawBridge-Serienprotokoll (Kommandotabelle `arduinointerface.py:56-77`:
`?` Version, `+`/`~`/`-` Motor, `.` Rewind, `[`/`]` Kopf, `#` Spur,
`<`+Flag Spur lesen, `>` schreiben, `X` löschen; 2 MBaud, CTS-Flusskontrolle
`:100,148-162`). Die Firmware liefert Spurdaten **2-Bit-lauflängenkodiert**;
die Dekodierung auf Zellenebene ist `decompressMap = {0:"", 1:"01",
2:"001", 3:"0001"}` (`arduinointerface.py:46`, Auflösung `:318-334`) —
also **drei Intervallklassen, kein Timing darüber hinaus**. `imager.py`
(329 Z.) sucht Sync/IDAM/DAM per **Regex auf dem Text-Bitstring**
(`imager.py:265,274`), MFM-dekodiert durch Verwerfen jedes zweiten
Zeichens (`imager.py:238-245`), prüft CRC-CCITT via
`binascii.crc_hqx(…, 0xffff)` (`imager.py:149`) und setzt das Abbild
sektorweise zusammen. `diskformats.py`: `cbm1581` = 80×2×10×512 DD,
`ibmdos` = 80×2×9×512 (`diskformats.py:29-74`). Schreibweg vorhanden,
aber ausdrücklich experimentell (Erase+Write `arduinointerface.py:188-288`,
große auskommentierte Blöcke; README: D81-Schreiben „will involve a lot
of work").

**Der für uns wichtigste Befund:** es gibt einen **hardwarefreien
Simulationsmodus**. `-s simulated` ersetzt die Hardware durch
`ArduinoSimulator` und speist den mitgelieferten Rohbitstrom ein
(`imager.py:47-50`, `arduinointerface.py:352-368`). Gemessen auf dieser
Maschine (venv, pyserial+bitstring):

```
python disk2image.py -d cbm1581 -s simulated -o oracle_sim.d81 -r 1
→ 160 von 160 Spurlesungen „10/10 valid sectors" (Log gezählt: 160)
→ Abbild 819200 Byte, Laufzeit < 1 s
→ SHA-256 a7b9f5f673a0e39e1c36deb7df6c555add1718a82b21b3b2c65737c234da4371
→ zweiter Lauf byteidentisch (cmp: identisch) — deterministisch
```

Der Rohbitstrom (`raw_debug_image_d81.zip` → 17 779 910 Byte
Python-Literal): 80 Spuren × 2 Köpfe, je 111 110–111 122 Bit
(≈ 1,11 Umdrehungen bei 100 000 Zellen/Umdrehung DD) — echte
Laufwerksaufnahme laut README (fdutils-formatierte Diskette, PC von
2002). **Nutzlast gemessen degeneriert:** Byte-Histogramm des dekodierten
Abbilds = `{0xF6: 819200}` — ausschließlich das fdformat-Füllbyte. Die
Diskette war frisch PC-formatiert, **nicht** CBM-formatiert (kein BAM,
kein Header, kein Verzeichnis; der README-Satz „Finally I used dd to put
a d81 image on the real disk" beschreibt einen Schritt, der in dieser
Aufnahme nachweislich noch nicht passiert war). Provenienz damit
unproblematisch — es liegt keine fremde Software auf der Diskette.

## 2. Die drei Fragen des Auftrags

### 2.1 Was kann es, das wir nicht können? — Hardware-Zugriff, und der ist Fundus

Inventar zitiert: `"arduino"` und `"drawbridge"` → `vorhanden: false,
abgedeckt: false` („INVENTAR DECKT DAS NICHT AB … von Hand prüfen").
Von Hand geprüft: `git grep -il -e arduino -e drawbridge -- src include
docs` → nur `docs/OPEN_ITEMS.md` und `docs/hardware/XUM1541-II-Guide.md`
— **kein** DrawBridge-Provider im Baum, und
`data/known_negatives.json` führt `RobSmithDev/ArduinoFloppyDiskReader`
bereits als `bewertet: „DrawBridge; Provider-Kandidat"`. pyAccess1581
fügt dem eine **zweite, unabhängige Dokumentation desselben Protokolls**
hinzu (Kommandotabelle + 2-Bit-Kompression + CTS-Verhalten, Fundstellen
oben) — als Verhaltens-Spec-Quelle verwertbar, als Code nicht (GELB).
**Kennzahl: keine.** Ein siebter Controller senkt kein Bench-Alter der
sechs geführten; das Projekt hat keine Hardware (MF-310). → **Fundus**,
angehängt an den bestehenden DrawBridge-Eintrag. Was den Kanal öffnete:
eine Community-Bench-Session mit DrawBridge-Hardware oder eine
Eigentümer-Entscheidung für einen M3-Provider-Baustein.

### 2.2 Taugt es als Oracle neben `c1541`? — Nein für CBM-DOS, ja (schmal) für die Decoder-Ebene

**Für `uft_cbmdos`/FS-T1b: nein, gemessen.** Es liest keine
D81-Abbilder und interpretiert kein CBM-DOS: `grep -in "bam\|directory\|
dirent\|filesystem" access1581/*.py` → **0 Treffer**; sämtliche
`open()`-Aufrufe (`imager.py:48,68,79`) sind Simulations-Eingabe und
Abbild-**Ausgabe**. Ein Werkzeug ohne Leseweg für Abbilder kann die
Verzeichnis-/BAM-Behauptungen von `uft_cbmdos` nicht entscheiden. Die
zweite Hand für genau diese Zone ist weiterhin der SCOUT-55-Fund
`libcbmimage` (`docs/OPEN_ITEMS.md:2772`).

**Fünfte Frage (MF-644): bestanden.** Quelltext-grep nach
`vice|cc1541|opencbm|cbmconvert` → 0 Treffer in den Quellen (nur
Scheintreffer „de**vice**" in meinen eigenen `.pyc`-Laufresten).
Eigenständige Implementierung von Henning Pingel; einzige Fremdlinie ist
Rob Smiths **serielles Protokoll**, nicht dessen Decoder. Gegenüber
unserem VICE-erzeugten Korpus (`vice_c1541_80trk.d81`, Inventar-Korpus)
wäre es eine echte andere Hand — die Kette lautet Linux-FDC/fdutils →
echtes Laufwerk → Arduino → unabhängiger Python-Decoder.

**Was es entscheiden könnte:** nur eine Decoder-Behauptung — „UFTs
MFM-Sektorextraktion liefert auf einer realen DD-Aufnahme dieselben
512-Byte-Sektoren wie eine unabhängige Implementierung". Ausführbar ohne
Hardware (Messlauf oben), deterministisch, Erwartung SHA-256-pinnbar.

**Längensemantik (gefordert im Auftrag):** nominal **roh** — 819200 Byte
= 80×2×10×512, ohne Fehler-Suffix, ohne Kopf; auf der mitgelieferten
Aufnahme gemessen. **Aber nicht konstruktiv garantiert:** eine Spur mit
1–9 gültigen Sektoren wird **stillschweigend ausgelassen** — das Abbild
wird kürzer und alles Nachfolgende verrutscht (`imager.py:130-132`,
Rückgabe `b''` nach bloßem `print`); eine Spur mit 0 Sektoren wird
nullgefüllt (`imager.py:126-129`); im letzten Versuch werden Sektoren
**mit ungültiger CRC dennoch aufgenommen** (`imager.py:184-188`,
„adding sector data anyway") — nach unserem Maßstab erfundene Daten.
Der interne Längen-Check ist zudem defekt: `imager.py:61-62` prüft
`len(trackData)` (das Dict) statt `trackDataTmp`, feuert also nie
korrekt. Als Oracle nur **sektor-adressiert** benutzbar (Schlüssel
Spur/Kopf/Sektor), mit mitgeschnittenem Konsolen-Log als Teil des
Urteils — nie über Datei-Hashes allein, außer das Log belegt 160×
„10/10" wie hier.

**Kennzahl: keine.** `d81` steht auf T1b (Inventar zitiert: `"d81":
vorhanden: true, tier: "T1b", plugin_liste_vollstaendig: true`); ein
Decoder-Differenzlauf senkt kein T3, öffnet keinen Wandlungspfad, und
die T3-Formate, die es theoretisch beträfe, haben keine Aufnahme, die
dieses Werkzeug lesen könnte. → **Fundus.** Was den Kanal öffnete: ein
beauftragter Differenzlauf-Baustein auf Decoder-Ebene (Plan-Anker müsste
`docs/VERIFICATION_PLAN.md` sein; heute existiert dort kein solcher
Baustein — das ist eine Feststellung, kein verkappter Vorschlag).

**Differenzlauf-Plan (Regel 7, für den Fall der Beauftragung):**
Binaries: pyAccess1581 `291554f` im venv (`-s simulated -r 1`) gegen ein
UFT-Test-Harness, das die 160 Text-Bitstrings bitpackt und durch UFTs
MFM-Sektorextraktion führt. Korpus: `raw_debug_image_d81.zip`
(SHA-256 des entpackten Literals pinnen). Metrik: byteweise Identität je
(Spur, Kopf, Sektor), 1600 Sektoren, Erwartung 512×`0xF6` plus
CRC-Status. Toleranzliste: Spuren, für die das pyAccess1581-Log etwas
anderes als „10/10" meldet (auf dieser Aufnahme: keine); die
Kopf-Vertauschungs-Quirk (`imager.py:221-222` tauscht den Kopf, wenn
`swapsides` **False** ist — für `cbm1581` also nicht, für `ibmdos`
schon). Regelkonformer Weg für jedes daraus folgende Codestück: benannte
Referenz (dieses Repo, Commit, GPL-3 → nur Verhalten), Rotbeweis zuerst
(z. B. gekippte CRC im Fixture muss den Harness röten), Referenz im
Header. Die EINFRIER-REGEL bleibt unberührt: es entsteht kein neues
Format-Plugin, auch nicht als Vorschlag.

### 2.3 Taugt es als Fixture-Erzeuger? — Ohne Hardware nur das eine mitgelieferte Stück, und das ist inhaltlich leer

Gegen `inv["korpus"]` (24 Einträge) geprüft: alle acht CBM-Zeilen sind
VICE-erzeugt; „real" gibt es nur für FDI, zwei G64 und die
CopyLock-Loader. Eine reale DD-3,5"-Aufnahme in 1581-Geometrie wäre
**neu** — aber diese hier trägt als Nutzlast ausschließlich `0xF6`
(Messung §1). Die ATR-Lehre aus demselben OPEN_ITEMS-Block gilt wörtlich
(„auf einem leeren Verzeichnis entscheidet dieser Differenzlauf
nichts", `docs/OPEN_ITEMS.md:2753-2757`): als **D81-Fixture wertlos**
(nicht einmal CBM-formatiert), als **Bitstrom-Fixture** immerhin 160
echte Spuren mit gültigen IDAMs/CRCs vom realen Laufwerk. Der
Beschaffungskorb (`korb.md`, 9 Posten, gegengelesen) fragt nichts
dergleichen an. **Kennzahl: keine.** → **Fundus**, gekoppelt an Posten
2.2 (der Differenzlauf wäre sein einziger Abnehmer). Neue Aufnahmen mit
echter Nutzlast erforderten die Hardware — siehe 2.1, MF-310.

## 3. Aufwandsklasse

**S** für jeden der drei Posten, **falls** je beauftragt (venv +
Harness-Skript bzw. Spec-Abschrift); heute Aufwand null, weil nichts
beauftragt wird.

## 4. UNGEKLÄRT

* Ob die 2-Bit-Quantisierung der DrawBridge-Firmware (drei
  Intervallklassen, kein Sub-Zell-Timing) für einen Decoder-Differenzlauf
  gegenüber echten Flux-Timings relevant Information verliert — sicher
  ist nur: Weak-Bits/Jitter-Detail überlebt sie nicht.
* Ob die `ibmdos`-Kopf-Vertauschung (`imager.py:221-222`) Verkabelung
  oder Fehler ist — ohne Hardware nicht messbar.
* Die Lizenz von ArduinoFloppyDiskReader selbst (im Credit-Block als
  GPL-3 benannt, am Fremd-Repo nicht nachgemessen) — relevant erst, wenn
  der DrawBridge-Fundus-Posten je gezogen wird.
* Die Zonen-Einordnung eines GPL-3-**Datenwerks** als lokales
  Korpus-Fixture (Aggregation vs. „Fixture-Lizenz wie Code-Lizenz") —
  liegt als PRÜFEN beim Eigentümer, siehe §Lizenz.

## 5. OPEN_ITEMS-Vorschläge

**Keine (0 von max. 5).** Kein Posten bewegt eine der vier Kennzahlen
(je Posten oben begründet); nach Regel 9 sind alle drei Fundus. Die
Erwartung des Auftrags („Gerätezugriff wäre Fundus … sag das dann
klar") hat sich bestätigt; die Oracle-Hoffnung für `uft_cbmdos` hat die
Messung widerlegt (§2.2, kein Abbild-Leseweg).
