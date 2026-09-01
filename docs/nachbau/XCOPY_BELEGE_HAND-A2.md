# ⚠⚠ HAND-A-AKTE — wer das liest, ist für die Umsetzung verbrannt ⚠⚠

**NICHT LESEN, wenn du irgendetwas aus der zugehörigen Spec
implementieren sollst.** Diese Akte enthält Fundstellen (Datei,
Zeilenregion) in der fremden, proprietären X-Copy-Quelle. Wer sie
gelesen hat, ist Hand A und für jede Umsetzung ausgeschlossen
(`tools/uft-nachbau/AGENT.md`, Brandmauer;
`docs/QUARANTINE_PROCESS.md` §5 Weg 2).

Die für Hand B lesbare Hälfte ist
[`XCOPY_VERHALTEN_HAND-A2.md`](XCOPY_VERHALTEN_HAND-A2.md) — sie
verweist per Beleg-ID hierher, nie umgekehrt.

**Zitier-Entscheidung:** Diese Akte enthält bewusst **keine
wörtlichen Quelltext- oder Kommentarzitate**. Die Quelle steht unter
einer Lizenz, die keine Aufnahme in einen GPL-2-Baum erlaubt
(MF-744/746); auch eine Hand-A-Akte wird mit dem Repo verteilt.
Fundstellen werden darum als Datei + Zeilenregion + sachliche
Beschreibung des Ablesbaren geführt. Wörtlich zitiert wird
ausschließlich Lizenz-/Freigabetext (§B-18). Wer den Wortlaut einer
Codestelle braucht, öffnet die eingefrorene Vorlage selbst — und ist
danach ebenfalls Hand A.

Sichtung: 2026-09-01, im Auftrag des Eigentümers. Alle Zeilenangaben
beziehen sich auf den 1992er Stand
(`_handA/v2/Source/original_1992/Original_Source/`, SHA-256 in §B-16);
„xcop.s" = 4 943 Zeilen, „xio.s" = 4 475 Zeilen.

---

## Belege

### B-01 — Leseauslösung, Zeitschranke, Lesemenge (Sektorpfad)

* `xcop.s` 661–668: die Sektorpfad-Leseroutine setzt das
  Hardware-Sync-Register fest auf `$4489` (nicht auf den vom Benutzer
  einstellbaren Sync-Wert) und aktiviert die Wortsync-Betriebsart;
  Leselänge im DMA-Längenregister entspricht 6 496 Wörtern =
  12 992 Bytes.
* `xcop.s` 290–306: die Wartefunktion auf Leseende hat eine
  Zeitschranke von 917 000; `xio.s` 824–840 (Timer-Startroutine)
  weist die Einheit als **Mikrosekunden** aus (Umrechnung in
  Timerticks mit Faktor 10/14, kommentiert). 917 000 µs ≈ 0,92 s.
* Folge: ohne `$4489` auf der Spur startet die DMA nie, der Versuch
  endet erst über die Zeitschranke und wird als „kein Sync" geführt
  (Aufrufstellen B-02).

### B-02 — Direktkopie-Ablauf, Wiederholungen, Rückfall

* `xcop.s` 308–345 (Einseiten-Pfad) und 346–455 (Beidseiten-Pfad):
  je Anlauf wird die Fehlerziffer sofort angezeigt; bei
  Zeitüberschreitung wird nur das **erste Langwort** des Lesepuffers
  invalidiert (Rest bleibt Altbestand — Basis von B-15).
* `xcop.s` 456–505 (Fehlerbehandlung): zwei weitere Leseversuche;
  bei erneuter Zeitüberschreitung wird direkt „kein Sync" (2)
  gesetzt; nach den Versuchen Anzeige der letzten Ziffer; im
  DOSCOPY-Modus danach Rückfall auf die Rohkopie, wobei die Funktion
  die **ursprüngliche** Fehlerziffer zurückgibt (die Roh-Bewertung
  überschreibt die Anzeige nicht) — das ist die im Änderungsprotokoll
  (B-13) als 5.21-Korrektur beschriebene Semantik im Ist-Zustand der
  5.3-Quelle. In den Modi DOSCOPY+/BAMCOPY stattdessen
  Reparatur-Dekodierung + Neukodierung.

### B-03 — Fehlerziffern-Tafel

* `xcop.s` 1161–1175: sechs Rückgabewerte 1–6 mit Autorenkommentaren
  sinngemäß: 1 = zu wenige Sektoren, 2 = kein Sync, 3 = kein zweiter
  Sync, 4 = Kopfprüfsummenfehler, 5 = Fehler im Kopf-/Format-Langwort,
  6 = Datenblock-Prüfsummenfehler.
* `xcop.s` 615, 1531: Verify-Fehler wird als eigene 8 geführt.

### B-04 — Anordnungsprüfung vor der Sektorprüfung

* `xcop.s` 812–875 (Puffer-Sortierstufe):
  * Das „Sektoren bis Lücke"-Feld des zuerst gelesenen Sektorkopfs
    wird dekodiert; Vergleich mit 11: **größer** → Ziffer 1;
    **gleich** → Kurzweg ohne jede weitere Anordnungsprüfung;
    **kleiner** → Lückensuche.
  * Lückenfolgesync-Suche: ab der errechneten Position (Feldwert ×
    $440 MFM-Bytes) wortweise Suche nach `$4489` über ein Fenster von
    $400+1 Wörtern (~2 050 Bytes); kein Treffer → Ziffer 3.
  * Spannenvergleich gegen $2EC0 (= 11 × 1088): arithmetisch
    nachvollzogen ist der verglichene Wert aus denselben Zeigern
    aufgebaut, gegen die er verglichen wird — die Differenz ist
    konstruktionsbedingt **immer** $2EC0, der Fehlerzweig ist
    unerreichbar. Es gibt also **keine** wirksame Prüfung der
    tatsächlichen Spanne und keine Zählung der real vorhandenen
    Sektoren. (Grundlage der E2-Aussagen; die Unerreichbarkeit ist
    meine Ableitung aus der Zeigerarithmetik, kein Autorenkommentar —
    bei Zweifel am Binary gegenprüfen.)

### B-05 — Sektorprüfung der Direktkopie (erster Fehler gewinnt)

* `xcop.s` 880–915 (Prüfschleife) und 924–971 (Kopfprüfung):
  * Je Sektor: Sync an erwarteter Stelle fehlt → 2 (sofortige
    Rückkehr). Kopfprüfsumme falsch → Merker 4; Kopf-Langwort
    (Formatkennung $FF + Spurnummer, oberes Wort) falsch → Merker 5,
    **der 4 überschreibt** (beide Prüfungen laufen nacheinander in
    derselben Kopfprüfung, der Merker ist ein einzelnes Feld).
  * Nach der Kopfprüfung: Merker ≠ 0 → sofortige Rückkehr — die
    Datenprüfsumme dieses Sektors wird **nie bewertet**. Nur bei
    sauberem Kopf: Datenprüfsumme, Fehler → 6, sofortige Rückkehr.
  * Elf Sektoren werden sequentiell geprüft (Zähler $0A+1), beginnend
    bei Sektor 0 des sortierten Bilds; Suchüberlauf jenseits $32C0
    Bytes → 1.
  * Die Sektornummer wird in diesem Pfad **nicht** geprüft (nur
    Formatkennung + Spurnummer).

### B-06 — RAM-Kopierpfad (letzter Fehler gewinnt)

* `xcop.s` 974–1038 (Dekodierprüfung): der Fehlermerker wird je
  Befund **überschrieben** und erst am Ende zurückgegeben: Kopfmerker
  4/5 wie B-05, danach Sektornummern-Plausibilität (Feld > 10 → 1,
  überschreibt), danach Datenprüfsumme (6, überschreibt). Kein
  vorzeitiger Abbruch außer bei „kein Sync im Fenster von $220
  Wörtern" → 2.
* `xcop.s` 1425–1483 (RAM-Leseschleife): drei Versuche; Anzeige
  **einmal** nach dem letzten Versuch; bei bleibendem Fehler im
  DOSCOPY-Modus Bedienerabfrage (Lesefehler-Meldung 506–524), dann
  Rohkopie; danach wird die gemerkte ursprüngliche Ziffer **erneut**
  angezeigt (die Roh-Zwischenanzeige ist also transient sichtbar).

### B-07 — Anzeigelogik der Ziffern

* `xcop.s` 762–775: Wert 0 → grüne Anzeige; 1–8 → rote Ziffer;
  ≥ 9 → blaue 9.

### B-08 — Roh-Analyse: fünf Kandidaten, 16 Bitlagen, Weiterverfolgung

* `xcop.s` 2110–2145: Erstsuche vergleicht jedes 16-Bit-Fenster in
  allen 16 Bitlagen gegen den benutzereingestellten Sync-Wert **und**
  die vier weiteren Kandidaten `$9521`, `$A245`, `$A89A`, `$448A`
  (Standardwert des einstellbaren Syncs: `$4489`). In der
  Index-Betriebsart wird ausschließlich `$4489` gesucht (2144–2155).
* `xcop.s` 2156–2200: nach dem ersten Treffer wird **der getroffene
  Wert** für die Folgesuche übernommen (Mindestabstand $100 MFM-Bytes
  nach Treffer, höchstens 24 Einträge).
* `xcop.s` 2348–2352: auskommentierte Tabelle ordnet drei der Werte
  Spieletiteln zu (bereits in Blatt 1, B5).

### B-09 — Standardspur-Einstufung im Roh-Pfad

* `xcop.s` 2297–2313: „Standardspur" (grüne Einstufung) nur wenn der
  getroffene Sync-Wert `$4489` ist **und** genau 11 Sektoren gezählt
  wurden **und** die Abstandsklassen-Tabelle einen Eintrag
  „Länge $440 (Wörter), 11 Vorkommen"-äquivalent trägt (Klassenbildung
  mit ±$20-Byte-Toleranz, 2255–2280).

### B-10 — Roh-Verdikte und ihre Anzeige

* `xcop.s` 2313–2345: Einstufungen grün (standardgleich), blau
  (Rohspur mit Syncs; auch Index-Betriebsart ohne Befund), hellgrau
  (Bruchstellen-Schutz), rot (kein Sync); Sonderfall: gemessene
  Umdrehungslänge ≥ $3300 (13 056) → Rückgabewert „überlange Spur".
* `xcop.s` 720–740 (Roh-Anzeigefunktion): Sonderfall → rote **7**;
  rot → rote **2**; alle anderen → **0** in der Verdiktfarbe.
* `xcop.s` 2346–2360 + Folgeroutine: ohne Sync wird auf
  Bruchstellen geprüft; Befund → hellgrau, sonst rot; Schreibstart
  dann Position des zweiten Index (−2 Bytes Justage).

### B-11 — Schreibstart im Roh-Pfad, Ausweichfall, Indexbindung

* `xcop.s` 2255–2295: die Abstandsklasse mit den **wenigsten**
  Vorkommen gilt als Lücke; Schreibstart = Position des Syncs nach
  der Lücke − 10 MFM-Bytes; liegt dieser Punkt vor/nahe dem
  Pufferanfang, wird stattdessen derselbe Sync auf der **zweiten**
  aufgenommenen Umdrehung genommen. Index-Betriebsart: zweiter
  Index − 2.
* `xcop.s` 2005–2040 (Roh-Schreib-/Leseroutine): DMA-Start wird am
  **Index-Interrupt** scharfgeschaltet (für Lesen und Schreiben);
  Zeitschranke 917 500 µs; bleibt der Index aus, erfolgt eine
  „kein Index"-Meldung.
* `xcop.s` 2046–2076: Schreiblänge je Ziellaufwerk kalibriert,
  −$20 Bytes Reserve (Blatt 1, B3).

### B-12 — Schreiben im Sektorpfad: keine Indexbindung, Füllvorlauf

* `xcop.s` 669–680: die Sektorpfad-Schreibroutine startet die DMA
  **sofort** (keine Index-Scharfschaltung); vor dem Spurbild werden
  $FA Langworte (= 1 000 Bytes) Füllmuster `$AAAAAAAA` gesetzt;
  Schreib-DMA-Länge $1955 Wörter (12 970 Bytes).

### B-13 — Änderungsprotokoll (Begleitdokument)

* `WHATS_NEW.DOC` (6 185 B, SHA in B-16), Abschnitt „XCopy 5.21":
  dokumentiert sinngemäß, dass vor 5.21 nach Lesefehler +
  Roh-Rettung eine grüne 0 („Roh OK") erschien, und dass seit 5.21
  die rote Ziffer stehen bleibt, weil der Lesefehler auf der
  Zieldiskette fortbesteht.
* Das Protokoll reicht nur bis Fassung 5.1 zurück; über 3.4 sagt es
  nichts.

### B-14 — Fassungskennung

* `xcopy.i` (1992er Stand): Versionskonstanten 5 / 3 / 0 → die
  gesichtete Quelle ist Fassung 5.3.0, Stand September 1992
  (deckt Gutachten §0.1).

### B-15 — Puffer-Altbestand beim Erstanlauf (Basis der Flacker-Vorhersage)

* Aus B-02: nach Zeitüberschreitung wird nur das erste Langwort
  invalidiert; der Lesepuffer wird zwischen Spuren wiederverwendet.
  Der erste Anlauf einer leeren Spur bewertet darum Altbestand der
  zuvor gelesenen Spur; deren Kopf-Langworte tragen die **falsche
  Spurnummer** → erwartete transiente Ziffer 5 (via B-05), Dauer ≈
  zwei Wiederholungen à ~0,92 s (B-01), dann 2. INFERRED — genaues
  Übergangsbild nur am Binary belegbar.

### B-16 — Kontaminations-Grundlinie (geöffnete Dateien)

Arbeitsordner `C:\Users\Axel\Github\xcopy\_handA\` (außerhalb des
UFT-Baums). `v2/` = `xcopypro_source_2011_v2(1).zip`, `ref/` =
`XCopyPro_Source_Reference.zip`, `master/` = `xcopy-master.zip`.

| Datei | Größe (B) | SHA-256 |
|---|---:|---|
| `v2/Readme 2011` | 2 032 | `23331d9c3f63e2cb55f4aafe454cff976bc9159870fc7d20d772bf5afbdda1d0` |
| `v2/Source/How_to_compile_XCopy.txt` | 3 396 | `cb52c324cd0ac32b0e21c04baea057234667f2ea1b000e7b7a55315df4472ab5` |
| `v2/Source/original_1992/Original_Source/xcop.s` | 102 861 | `242942a53ff7e825fe17f555a65ab1ae0e3f6c6ec89a33eaf680716f2b51cf05` |
| `v2/Source/original_1992/Original_Source/xio.s` | 93 902 | `bd825facdc68e8b8c0471264aff1a1d07364b524bc0cebe4abe328413d59148e` |
| `v2/Source/original_1992/Original_Source/xcopy.i` | 1 739 | `ba42e9e957f36bdb4ebe4376cad6adf69969051cc3770745a3191868d4dee90f` |
| `v2/Source/original_1992/Original_Source/WHAT_TO_DO.DOC` | 50 | `eb50cb7fd4baf93cc723aaa3f64e422a2847a74e580013004e2a222e0c16f0de` |
| `v2/Source/fixed_2011/xcop.s` (nur Diff gegen 1992) | 102 805 | `847bc332d8efccd9f9f6ea75e49d60975b5f0c4a6ec92930c7c7b1fab5c138f3` |
| `v2/Source/fixed_2011/xio.s` (nur Diff gegen 1992) | 93 907 | `13d5ba173683134852b9af02983cdb2858991066ae21700a3c9fe2913d49e694` |
| `v2/Source/fixed_2011/WHATS_NEW.DOC` | 6 185 | `b38ebfd890b396b716d836c1761e7ce3ac5643db2d58c71088ac0a107763fde1` |
| `v2/Source/fixed_2011/WHAT_TO_DO.DOC` | 50 | identisch mit 1992 |
| `v2/Source/fixed_2011/depack.s` (Kopf) | 1 794 | `2b236342c2caf02be75fd69ba36fea762c5ae41f00dcc59f600a2a0d0520fd66` |
| `v2/Source/fixed_2011/config.asm` (Kopf) | 5 149 | `f2eca6485b0617c80c537d8b449752e8d73ab6118c7af73e3bccd2c5cffe58a3` |
| `v2/Source/fixed_2011/Makefile` | 601 | `cba69e389d806afc275548bf68b0b65f435c90e361423d8643100813b958c6cc` |
| `v2/Devs/help.guide` (Kopf) | 3 971 | `d309bf76d6a07334e884f955c33eb961ea5e7710283a21fc30826071c24620f4` |
| `xcopy_src/xio.s` (nur Diff, dritte Variante) | 93 898 | `4acffdfa10ed1ee7205c529365e294db27f80ce54a961eb8c3d48d5faf0e132a` |
| `ref/xcopy_extracted/Readme 2011` | 1 656 | `2367968e4a30118d250167bd5a3e17328c80f6e3c1d0426a94b31593238ea198` |
| `master/xcopy-master/README` | 4 344 | `034fcd6218076292d82bf1095ee8b8c9a36cffc028334c60167c229b19da0d20` |
| `master/xcopy-master/APPINFO/XCOPY.LSM` | 665 | `b2ac3592c74dce292fc80e1ff77f90500145ec04c94c26531e728d7235ca62c9` |
| `master/xcopy-master/doc/copying.txt` (Kopf) | 18 075 | `708a2f9e2ee59cb196063f5b379ac711378efe8d8549e024a2f0005963484406` |

Container (gehasht, entpackt): `xcopypro_source_2011_v2(1).zip`
`0df1adc3…15cc` (524 372 B) · `XCopyPro_Source_Reference.zip`
`efbe0473…3853` (73 344 B) · `xcopy-master.zip` `f36d22af…3255`
(46 948 B) · `xcopypro_src_original_1992.lha` `7eb5296b…9491` ·
`xcopypro_src_fixed_2011.lha` `c13b82ba…15cd`.

**Gehasht, aber NICHT geöffnet:** `master/…/doc/history.txt`
(`c1d88b17…4cc7`) und alle FreeDOS-Quellen; die drei Oracle-ZIPs
(`xcopy_01_95_master(1).zip`, `xcopy_v3.4…(1).zip`,
`xcopy_v5.21…(1).zip`); `xcopy.zip`; `xvslibrary.*`; `DiskSalv/`,
`FixDisk/`; sämtliche Binaries, Grafiken und AmigaDOS-Systemdateien
im `v2`-Baum; `ref/xcopy_extracted/Source/*` (hashidentisch mit dem
1992er Stand).

Registriert: `tools/uft-nachbau/work/sichtprotokoll.json` (Lauf A,
23 Einträge), `work/vorlage.json` (22 Dateien eingefroren,
Pfad + SHA-256). Werkzeug-Abnahme im selben Lauf:
`sichtprotokoll.py --selbsttest` **9/9**.

### B-17 — Inventarbefund (per SHA-256)

* Das `v2`-Archiv ist **kein ADF**, sondern ein entpackter
  AmigaDOS-Diskettenbaum; die Quellen darin doppelt als `.lha`
  („original 1992", „fixed 2011").
* `XCopyPro_Source_Reference.zip` enthält exakt den 1992er Stand
  (xcop.s/xio.s hashidentisch) plus Readme v1.0.
* `xcopy_src/` ist ein **Mischbaum**: xcop.s = 1992, xio.s = dritte
  Variante (12 Diff-Zeilen zur fixed-Fassung, 277 zur 1992er).
  `xcopy_src(1)/` = hashidentisch mit fixed 2011.
* Diff 1992→fixed: xio.s 13 Änderungsstellen, xcop.s 5 — alle im
  Bau-/Einbindungsbereich, keine im Disk-Verhalten gesichtet
  (deckt Gutachten §0.1).

### B-18 — Lizenzbefund (ohne Urteil — MF-679)

1. **`xcopypro_source_2011_v2(1).zip`: keine Lizenzdatei.** Enthält
   `Readme 2011` v1.1 (29.12.2011) mit dem Freigabetext, wörtlich:
   *„This software is released as is, without any guarantees or
   warranties. You are welcome to enhance it or develop further
   versions, just keep it free (don't sell it) and release your
   source as well. Regarding all other matters please respect the
   licence that came with this disk image."* — sowie: *„Copyright (c)
   1992-2011 Anguilla Software International Ltd., Bletchley Manor,
   Long Ground, Anguilla, BWI."* Der verwiesene Lizenztext liegt
   **nicht** in diesem Archiv (er liegt in
   `xcopypro_source_2011.zip`, MF-747). Nichts Abweichendes zur
   Aktenlage gefunden.
2. **`XCopyPro_Source_Reference.zip`:** Readme v1.0 (26.12.2011),
   gleicher Freigabetext ohne Nachtrag; sonst keine Lizenzangabe.
3. **Quelldateien:** kein Lizenzvermerk; im Oberflächenteil der
   Anzeige-Text „COPYRIGHT ASI 1992, ALL RIGHTS RESERVED"
   (`xio.s` ~3956). Deckt Gutachten §0.2.
4. **Beiträge Dritter 2011:** „fixed"-Fassung laut
   `How_to_compile_XCopy.txt` von „phx" und „acd2001" (English Amiga
   Board; auch im Readme v1.1 genannt) — **ohne eigene
   Lizenzerklärung**. Fürs Disk-Verhalten irrelevant (B-17), für
   Weiterverwendung der fixed-Fassung nicht.
5. **Fremdcode-Verdacht:** `depack.s` ist eine Entpack-Routine für
   PowerPacker-gepackte Daten, ohne Autoren-/Lizenzvermerk; solche
   Entpacker kursierten als Fremdbeigaben Dritter. Herkunft
   ungeklärt → Eigentümer-Vorlage (Werkstattregel 8). Für den
   UFT-Nachbau ohne Belang.
6. **`xcopy-master.zip` ist eine Namenskollision:** FreeDOS XCOPY
   1.9a (DOS-Dateikopierer, Autor Rene Ableidinger,
   „Copying-policy: GNU General Public License, Version 2",
   `doc/copying.txt` = GPL-2-Volltext). Kein Zusammenhang mit dem
   Amiga-X-Copy; Empfehlung an den Eigentümer: aus dem
   X-Copy-Bestand aussortieren.
7. **Weiterverteilungs-Hinweis `v2`-Baum:** enthält
   Commodore-Systemdateien (`C/`, `Libs/`, `Devs/`, `L/`) und
   Drittbibliotheken (`req.library`, `amigaguide.library`). Für UFT
   ohne Bedeutung, für eine Weitergabe des Archivs nicht.

Keine Doppel-Lizenz, kein abweichender Header, kein Widerspruch zur
Aktenlage MF-744/746 gefunden. Urteil beim Eigentümer.
