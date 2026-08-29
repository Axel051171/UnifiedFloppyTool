<!-- uebernommen: MF-678 -->
<!-- Befunde nach docs/OPEN_ITEMS.md, SCOUT-26 uebertragen: getrennt
     nach Fehlern gegen benannte Referenzen (kein Vorlagenbedarf) und
     echten Eigentuemer-Vorlagen (Lizenz/Verteilung betroffen). -->
# Gutachten: die ADF-Zweitmeinung (drei Repos, ein Zyklus)

* **Zyklus:** 9 (2026-08-29), Auftrag des Eigentümers — „die wichtigste
  offene Lücke im ganzen Oracle-Bestand" (`docs/ORACLES.md`, Schlusssatz)
* **Repos:**
  * https://github.com/alpine9000/adf-tools — Commit `9e1a6c0` (2024-10-09), 9 Dateien
  * https://github.com/dschwen/adfrescue — Commit `0cbc5ff` (2015-12-21), 8 Dateien
  * https://github.com/wbommel/ADFCommander — Commit `8348d94` (2019-02-14), 28 Dateien
* **Messungen:** `work/adf-tools.messung.json`, `work/adfrescue.messung.json`,
  `work/ADFCommander.messung.json` (vermessen.py, 2026-08-29);
  Inventar `work/inv.json` (HEAD `18fe2bcc`, SSOT ok, 88 Plugins)
* **Hinweis zur Erstellung:** von Hand nach Playbook 03 (gutachten.py-
  Ratenbremse: 2 Gutachten in `out/` ohne Übernahme-Marke). Dieses
  Gutachten trägt **2** OPEN_ITEMS-Vorschläge (vorläufige Marken
  SCOUT-70/71) — Regel 5 (max. 5 je Zyklus) eingehalten.

## TL;DR

**Die Lücke ist geschlossen — doppelt, und anders als erwartet.**

1. **adfrescue** (dschwen) ist genau die gesuchte Hand: ADFlib-unabhängig
   (belegt), skriptbar (rc-Semantik gemessen), extrahiert **Inhalte**
   (stärkste Oracle-Form nach MF-629: Bytes herausholen und selbst
   hashen). Heute gebaut, gegen `tests/corpus_free/xdftool_dd_ofs.adf`
   gelaufen: `marker.txt`, 127 Byte, **byteidentisch** zur
   xdftool-Extraktion (sha256 `be045cea…d7f4`, `cmp` ohne Differenz).
   Einziger Blocker: **keine Lizenzdatei** → Zone ROT → Regel 8,
   Entscheidungsvorlage an den Eigentümer (SCOUT-71).
2. **Beifang, der die Lücke selbst betrifft (Messung vor Plan):** die
   dokumentierte Prämisse, ADFlib sei „dieselbe Bibliothek, die über
   xdftool auch unser Korpus-Abbild erzeugt hat"
   (`docs/ORACLES.md:48-49` und `:156`), ist **gemessen falsch**. Das
   installierte amitools 0.8.1 (das Manifest-Werkzeug) enthält **0**
   ADFlib-Referenzen, **0** native Module, **0**
   Herkunfts-Attributionen — reine, eigenständige Python-Implementierung.
   Damit ist `unadf` (ADFlib, GPL-2.0) doch eine **unabhängige** zweite
   Hand gegen den amitools-erzeugten Korpus, exakt wie
   `PLAN_v4.1.7.md:220` es ursprünglich vorsah (SCOUT-70).

Die beiden anderen Repos scheiden aus: **adf-tools** ist ein
Amiga-natives Hardware-Werkzeug (läuft auf dem Amiga, nicht auf dem
Host), **ADFCommander** hat trotz MIT-Lizenz eine AmigaDOS-Fähigkeit
von praktisch null.

---

## Die Kernfrage, je Repo einzeln

### 1. alpine9000/adf-tools — irrelevant (falsche Maschine)

**ADFlib-unabhängig?** Ja, aber gegenstandslos.
`grep -rni "adflib|adf_dev|adfMount|adfOpenDev|adfVolume"` über den
Klon: **0 Treffer** außerhalb `.git`. Kein `.gitmodules`, kein
`vendor/`/`third_party/`. `Makefile:5` linkt nur `-lamiga`.

**Skriptbar?** Nein — nicht auf unseren Plattformen. Es sind
**Amiga-Programme**: `Makefile:3` setzt
`CC=/usr/local/amiga/bebbo/bin/m68k-amigaos-gcc`, der Code nutzt
`struct IOExtTD` (trackdisk.device), `AllocMem(…, MEMF_CHIP)`
(`adfread.c:25`, README: „These are Amiga programs designed to run on
Amiga computers"). Sie lesen/schreiben **physische Disketten in
df0–df3**, keine ADF-Dateisysteme. 661 Zeilen gesamt (`wc -l`).

**Inhalte/Hashes?** Nein — Spur-Kopie mit Verify, keine Dateiebene.

**Bauen?** Auf dem Host nicht möglich (m68k-Crosscompiler + Amiga-NDK
nötig); nicht versucht, weil das Urteil davon nicht abhängt: selbst
gebaut liefe es nur auf einem Amiga. **Keine Hardware** ist ohnehin
Auftragsregel (MF-310).

**Lizenz:** keine LICENSE-Datei → **ROT** (alle Rechte vorbehalten).
Zusätzlich tragen alle Quellen die Attribution „Code heavily inspired
by Track_Copy.c from Amiga Devices Manual" (`adfread.c:1`, `track.c:1`,
`adf.h:1`, `adfwrite.c:1`) — eine erklärte Ableitung aus
Commodore-Handbuch-Beispielcode, dessen Lizenz ungeklärt ist. Doppelt
unportierbar.

**Urteil:** Kategorie **irrelevant**. Negativliste `bewertet`.

### 2. dschwen/adfrescue — der Fund (Oracle-Kandidat, Lizenz offen)

**ADFlib-unabhängig? — unabhängig, belegt durch:**

* `grep -rni "adflib|adf_dev|adfMount|adfOpenDev|adfVolume"`: Treffer
  **nur** in `adffaq/adf_info.html` — das ist die mitgelieferte Kopie
  der ADF-FAQ (Doku von Laurent Clévy, laut README frei verteilbar),
  kein Code.
* Der gesamte Code ist **eine Datei**, `adfrescue.cc`, 225 Zeilen
  (`wc -l`); Includes ausschließlich libc
  (`adfrescue.cc:1-5`: stdio/stdlib/string/sys/stat/types). Kein
  `.gitmodules`, kein vendor-Verzeichnis; `Makefile` = eine Zeile
  `g++ -o adfrescue adfrescue.cc`, keine Bibliotheken.
* Eigenständige OFS-Logik: BigEndian-Zugriffe selbst gebaut
  (`adfrescue.cc:22-28`), Block-Checksumme selbst
  (`adfrescue.cc:30-37`), Header-/Datenblock-Erkennung über
  `type==2/sectype==-3/self-ref` bzw. `type==8`
  (`adfrescue.cc:105-119`).
* Und — nach dem Beifang unten entscheidend — auch
  **amitools-unabhängig**: C++ vs. Python, kein gemeinsamer Code.
  Dritte Hand gegenüber Korpus-Erzeuger UND unadf.

**Skriptbar? — ja, gemessen:** `adfrescue <datei.adf>`, keine
Interaktion, extrahiert in das aktuelle Verzeichnis.
rc-Semantik (heute gemessen, MinGW-Build):

| Eingabe | Verhalten | rc |
|---|---|---|
| `xdftool_dd_ofs.adf` (Korpus, OFS) | „OFS disk filesystem detected", PASS1–3, extrahiert `0_marker.txt`, „ALL OK!" | **0** |
| `vice_c1541_35trk.d64` | „not supported yet. Exiting." | **1** |
| dasselbe ADF mit Byte 3 → `\x01` (FFS) | „Fast file system not supported yet." | **1** |
| fehlende Datei | Fehlermeldung | **1** |

Laut statt still — genau die floptool-Falle (`flopdir adf cbmdos` →
rc=0 + leere Liste) hat dieses Werkzeug **nicht**.

**Inhalte/Hashes? — stärkste Form:** extrahiert die Datei-Bytes
(Datenblock-Tabelle + Extension-Ketten, `adfrescue.cc:128-186`), wir
hashen selbst. Kein eigener Hash-Ausgang nötig.

**Gebaut und gelaufen? — ja:**

* Build MinGW g++ 13.1.0: erster Versuch **rot**
  (`adfrescue.cc:32: 'u_int32_t' was not declared` — BSD-Typname);
  zweiter Versuch grün mit
  `g++ -include cstdint -Du_int32_t=uint32_t -o adfrescue.exe adfrescue.cc`
  (Compile-Flag, keine Quelländerung).
* Lauf gegen `tests/corpus_free/xdftool_dd_ofs.adf` (901 120 B, sha256
  `9af68fcc…75e0` = Manifest-Eintrag, nachgemessen):

  ```
  OFS disk filesystem detected. Continuing.
  PASS1: Scan all blocks on disk.
  PASS2: Dump files using the header blocks.
  866: '0_marker.txt' (127 bytes)
          ALL OK!
  PASS3: Collect unclaimed block chains.
  done.
  ```

* **Differenzlauf sofort mitgemessen** (beide Hände, gemeinsames
  Korpus-Abbild, Metrik = SHA-256 des extrahierten Inhalts):

  | Hand | Befehl | sha256 |
  |---|---|---|
  | adfrescue (Eigenbau) | `adfrescue.exe xdftool_dd_ofs.adf` → `0_marker.txt` | `be045ceaf11926dce92674251cb58ccc7d4cee808b79b4655bdfe2d849dcd7f4` |
  | xdftool (amitools 0.8.1, pip) | `python -m amitools.tools.xdftool … read marker.txt` | `be045ceaf11926dce92674251cb58ccc7d4cee808b79b4655bdfe2d849dcd7f4` |

  `cmp` → byteidentisch, 127 Byte. Toleranzliste: leer (Vollinhalt).

**Betriebsgrenzen (aus dem Quelltext, für den Registry-Eintrag):**

* **Nur OFS** (`DOS\0`, `adfrescue.cc:79-90`); FFS/Intl/Dircache enden
  rc=1. Unser einziges ADF-Korpusabbild ist OFS — passt heute, deckt
  aber einen künftigen FFS-Korpus nicht.
* Keine Verzeichnispfade: PASS2 scannt **alle** Blöcke nach
  checksummen-gültigen Datei-Headern (verzeichnisunabhängig — als
  Rettungswerkzeug Absicht), gibt nur Dateinamen (30-Byte-Feld,
  `adfrescue.cc:134`) mit `<index>_`-Präfix aus.
* Schreibt in das **aktuelle Verzeichnis**, Dateinamen stammen aus
  Disk-Inhalt → nur in Wegwerf-Arbeitsverzeichnis laufen lassen
  (Pfad-Hygiene ist Sache des Aufrufers).
* Keine Indexprüfung auf `dblock` (`adfrescue.cc:152` indiziert `ck[]`
  mit einem Wert von der Diskette) → gegen präparierte Abbilder nicht
  härtefest; als Oracle auf Korpusdaten irrelevant, als Warnung notiert.
* Kein `--version` → Registrierung nach MF-623:
  `version_is_unaskable` + SHA-256 des Eigenbaus + Quell-Commit
  `0cbc5ff`.

**Lizenz: keine LICENSE-Datei, keine Copyright-Header im Code**
(`grep -rni "copyright|license|spdx" adfrescue.cc Makefile`: 0 Treffer;
das Copyright im README betrifft nur die beigelegte ADF-FAQ-Doku).
**Zone ROT — alle Rechte vorbehalten.** Konsequenz nach
`playbook/lizenzmatrix.md`: Code portieren ❌; Konzept nachbauen ✅ nur
aus Doku/Blackbox; Oracle „✅ falls Binary legal beziehbar" — hier gibt
es aber **kein vertriebenes Binary**, nur Eigenbau aus lizenzloser
Quelle. Ob GitHub-ToS (Ansehen/Forken) das lokale Bauen und Ausführen
als Prüfwerkzeug deckt, ist ein Grenzfall → **Regel 8: Vorlage an den
Eigentümer, keine eigene Auslegung** (SCOUT-71, Optionen unten).

**Attribution der Quelle (MF-636):** dschwen/adfrescue, Autor Daniel
Schwen, **ohne Lizenz**. Das Gutachten übernimmt keinen Code; die
OFS-Blocksemantik ist unabhängig davon in der frei verteilbaren ADF-FAQ
(Laurent Clévy, 1997-1999) dokumentiert, die auch UFTs bestehender
AmigaDOS-Leser abdeckt.

**Urteil:** Kategorie **Oracle** (Kandidat, lizenz-blockiert).
Negativliste `bewertet`.

### 3. wbommel/ADFCommander — MIT, aber Fähigkeit ≈ null

**ADFlib-unabhängig?** Ja — `grep` trifft nur
`AdfCommanderLib/adf_info.txt` (wieder die ADF-FAQ als Doku, 2137
Zeilen). Kein Paket-/DLL-Bezug auf ADFlib (`packages.config`,
`.csproj`: 0 Treffer).

**Skriptbar?** Nein, gemessen am einzigen Einstieg:
`ConsoleApplication1/Program.cs:15-27` — **hartkodierte** Abbildpfade
(`HLS_NHL_2018_2019 - Kopie.adf`, `HDExampleDisk.adf`), kein
argv-Parsing, endet mit `Console.ReadKey()` (interaktive Blockade).

**Inhalte/Hashes?** Nein. Die AmigaDOS-Fähigkeit der Bibliothek (531
Zeilen gesamt) besteht aus **Bootblock-Parsen, dessen Ergebnis
verworfen wird**: `AmigaDOS.cs:50-60` baut `BootBlock bb` und benutzt
ihn nie; die Klasse `RootBlock` ist **leer**
(`AmigaDosBlockDefinitions.cs:48-52`: eine nie gesetzte Property);
Verzeichnislesen/Extraktion existieren nur für „HlsDos" — das
Custom-Diskformat eines Eishockeyspiels (`HlsDos.cs`), nicht AmigaDOS.

**Bauen?** Nicht versucht — das Urteil hängt nicht davon ab: es gibt
keinen Code-Pfad, der eine AmigaDOS-Datei listen oder extrahieren
könnte, und der Konsolen-Einstieg ist interaktiv. Ein Build würde ein
Programm erzeugen, das auf zwei mitgelieferte Spiel-Abbilder zeigt.

**Lizenz:** `LICENSE` = MIT (Volker Brömmel, 2019) → GRÜN. Nützt
nichts ohne Substanz. **Warnung Beschaffung:** die Test-Fixtures
`wb31-workbench.adf` (Workbench 3.1) und die Spiel-ADFs sind
**urheberrechtlich geschützte Fremdabbilder** — nicht übernehmen, nicht
weiterverwenden.

**Urteil:** Kategorie **irrelevant** (für die Auftragskern-Frage).
Negativliste `bewertet`.

---

## Beifang mit Tragweite: die ADFlib-Prämisse ist gemessen falsch

`docs/ORACLES.md` begründet die offene Lücke zweimal so:

> „**ADF:** `unadf` und AdfOpus teilen sich **ADFlib** — dieselbe
> Bibliothek, die über `xdftool` auch unser Korpus-Abbild erzeugt hat."
> (`docs/ORACLES.md:48-49`; gleichlautend `:156`)

**Messung (2026-08-29, dieselbe Maschine, dasselbe pip-Paket wie im
Manifest `tests/corpus_manifest/manifest.json` — „amitools xdftool (pip
amitools, 2026-08)", installiert: amitools **0.8.1**):**

* Volltextsuche `adflib` über das gesamte installierte Paket
  (alle `.py/.pyd/.so/.dll/.txt/.md`): **0 Treffer**.
* Native Module (`.pyd/.so/.dll`) im Paket: **keine** — reine
  Python-Implementierung, nichts gelinkt, nichts gewrappt.
* Herkunfts-Attributionen (`clévy|based on|port of|adflib`, Regex über
  alle `.py`): **0 Treffer**.

**Konsequenz:** unadf (ADFlib, GPL-2.0 — Zone GRÜN, Lizenz je Datei
bereits im AdfOpus-Gutachten bestimmt: `ADFLib/Docs/license.txt`
GPL-2-Volltext, `adflib.h:123-124` ohne „or later") ist gegenüber dem
Korpus-Erzeuger **doch die unabhängige zweite Hand**, die
`PLAN_v4.1.7.md:220` von Anfang an als Pflicht führte. Die
„fünfte Frage" (dieselbe Hand?) bleibt als Regel richtig — nur ihre
ADF-Fallzuordnung war falsch. Richtig bleibt auch: AdfOpus und unadf
untereinander sind EINE Hand (beide ADFlib); als **zueinander**
redundante Zweitmeinungen ausgeschlossen.

Methode der Zahl (Regel 2/MF-615): verglichen wurde die Menge
„Dateien des installierten amitools-0.8.1-Pakets" (Quelle:
`site-packages/amitools`, os.walk) gegen die Suchbegriffe oben;
Treffer = Substring, case-insensitiv. Grenze der Messung: sie belegt
den Stand 0.8.1 auf dieser Maschine; das Manifest pinnt keine
amitools-Version (UNGEKLÄRT-Liste).

---

## Pflichtfelder gesammelt

* **Kategorie:** adfrescue = Oracle; adf-tools = irrelevant;
  ADFCommander = irrelevant; Beifang = Daten/Doku-Korrektur.
* **Lizenzzonen:** adfrescue ROT (keine Datei) → nur
  Entscheidungsvorlage; adf-tools ROT + Commodore-Ableitung → nichts;
  ADFCommander GRÜN/MIT → portierbar, aber nichts Portierenswertes;
  unadf/ADFlib GRÜN (GPL-2.0, nur Ausgabe-Vergleich).
* **Bewegte Kennzahl (MF-640):** Tier-Leiter — beide Vorschläge
  entzirkeln den `adf`-T1b-Eintrag (heute Oracle-Spalte „—",
  `docs/VERIFICATION_TIERS.md:23`) und öffnen den Weg T1b→T1; zugleich
  entsperren sie Phase 1 Nr. 2 (AmigaDOS-Leseseite,
  `PLAN_v4.1.7.md:220`). Präzedenz für diese Kennzahl-Lesart:
  SCOUT-60 („Tier-Leiter T2→T1b", `docs/OPEN_ITEMS.md:2732`).
* **Inventar-Abfrage (zitiert):** `inventar.py query work/inv.json adf
  amigados ofs "adf oracle"` →
  `adf: {vorhanden: true, tier: T1b, plugin_liste_vollstaendig: true}`;
  `amigados: {vorhanden: true, treffer: [amiga]}`; `ofs` und
  `adf oracle`: `abgedeckt: false` → von Hand geprüft:
  UFT-eigener AmigaDOS-Leser liegt in `src/fs/uft_amigados.c` (762 Z.),
  `uft_amigados_extended.c` (740 Z.), `uft_fs_amigados_driver.c`
  (173 Z.); ein registriertes ADF-Oracle existiert nicht
  (`docs/ORACLES.md` §Registrierte: 7 Einträge, keines für ADF).
* **Einhängepunkte (im Baum auffindbar):**
  `docs/ORACLES.md` (Registry-Verzeichnis + §„kein Oracle" +
  §„fünfte Frage"), `tests/differential/oracles.py` (Registry-SSOT),
  `docs/PLAN_v4.1.7.md:220` (Phase 1 Nr. 2),
  `docs/VERIFICATION_TIERS.md:23` (adf-Zeile, Oracle-Spalte).
* **Oracle-Kandidaten:** 1. `unadf` (GRÜN, sofort gangbar nach
  Bau+Messung), 2. `adfrescue` (technisch fertig vermessen,
  lizenz-blockiert, Eigentümer-Entscheid).
* **Beschaffungsliste:** nichts zu beschaffen — Korpus liegt
  (`tests/corpus_free/xdftool_dd_ofs.adf`, gegen `inv["korpus"]`
  geprüft: 22 Abbilder, ADF-OFS vorhanden); amitools liegt (pip 0.8.1);
  unadf ist zu **bauen** (ADFlib-Repo, kein Fixture-Bedarf).
  Ausdrücklich NICHT beschaffen: die ADFCommander-Fixtures
  (Workbench 3.1 = fremdes Copyright).
* **Aufwandsklasse:** SCOUT-70 = S (Doku-Korrektur + unadf
  bauen/messen/registrieren); SCOUT-71 = S (Vorlage entscheiden;
  Registrierung selbst ist S, alle Messwerte liegen hier).
* **Differenzlauf-Plan:** für adfrescue bereits **ausgeführt** (Tabelle
  oben: beide Binaries, gemeinsames Korpus-Abbild, Metrik SHA-256 je
  Datei, Toleranzliste leer). Für unadf identisch anzusetzen:
  `unadf -p <adf> marker.txt | sha256sum` gegen xdftool und gegen die
  künftige UFT-Ausleitung; erwartet `be045cea…d7f4`.

## OPEN_ITEMS-Vorschläge (2 von max. 5, Marken vorläufig)

> Nummern ab SCOUT-70, wie beauftragt **vorläufig** — vier Zyklen
> laufen parallel, Kollisionen beim Übernehmen auflösen.

| Marke | Vorschlag | Kennzahl |
|---|---|---|
| SCOUT-70 *(vorläufig)* | ORACLES.md-Prämisse korrigieren: amitools 0.8.1 ist gemessen ADFlib-frei (0 Treffer, 0 native Module) → `unadf` ist die unabhängige zweite Hand gegen den xdftool-Korpus; unadf bauen, gegen `xdftool_dd_ofs.adf` laufen lassen (erwartet `marker.txt` sha256 `be045cea…d7f4`), in `tests/differential/oracles.py` registrieren. Quelle: dieses Gutachten §Beifang; `docs/ORACLES.md:48-49,156`; AdfOpus-Gutachten (ADFlib GPL-2.0). | Tier-Leiter: `adf` T1b entzirkeln, Oracle-Spalte füllen (`VERIFICATION_TIERS.md:23`); entsperrt Phase 1 Nr. 2 |
| SCOUT-71 *(vorläufig)* | **Entscheidungsvorlage** adfrescue (Zone ROT, keine Lizenzdatei): (a) Upstream-Anfrage an dschwen um eine Lizenzdatei — Repo seit 2015 unverändert, Autor auf GitHub aktiv; (b) lokale Nutzung als nicht-redistributables Zweit-Oracle (Registry: `version_is_unaskable`, SHA-256 des Eigenbaus, Quell-Commit `0cbc5ff`, Baurezept `-include cstdint -Du_int32_t=uint32_t`); (c) verwerfen, nur Verhaltens-Referenz. Alle technischen Messwerte liegen (gebaut, gelaufen, byteidentisch zu xdftool, rc-Semantik vermessen). | dieselbe: dritte, von ADFlib UND amitools unabhängige Hand in stärkster Form (extrahieren + selbst hashen) |

## Fundus (bewegt keine Kennzahl — notiert, nicht eingeplant)

* **Verhaltens-Spec „OFS-Rettungsscan"** für die als unimplementiert
  geführten `uft_amiga_repair_bitmap/_salvage/_scan_directory`
  (`src/fs/uft_amigados_extended.c:7-8`): verzeichnisunabhängiger
  Header-Block-Scan (checksummen-gültig, T=2/ST=-3/self-ref),
  Extraktion über Datenblock-Tabelle + Extension-Kette, Waisenketten
  über Sequenznummer 1 + unbenutzt + Checksumme ok
  (adfrescue PASS1/2/3-Semantik, Blackbox; Spec-Quelle wäre die frei
  verteilbare ADF-FAQ, **nicht** der ROT-lizenzierte Code).
* adfrescues Fehlklassen-Ausgabe („contains N broken blocks",
  „Not enough data found (x/y)") als Vorbild für eine ehrliche
  Teilrettungs-Meldung — deckungsgleich mit dem Prinzip „keine
  erfundenen Daten" (Broken-Blöcke werden als Nullen gezählt und
  **gemeldet**, nie still aufgefüllt).

## UNGEKLÄRT

1. **Deckt GitHub-ToS das lokale Bauen/Ausführen lizenzloser Quelle als
   Prüfwerkzeug?** Rechtsfrage, nicht auslegbar → Teil der Vorlage
   SCOUT-71.
2. **Exakte amitools-Version des Korpus-Laufs:** das Manifest sagt „pip
   amitools, 2026-08" ohne Pin; gemessen wurde 0.8.1 (heute
   installiert). Die ADFlib-Freiheit gilt belegbar für 0.8.1; für die
   Erzeuger-Version ist sie hochwahrscheinlich (amitools war nie ein
   ADFlib-Wrapper), aber nicht auf dieser Maschine gemessen.
3. **adfrescue auf FFS:** rc=1, gemessen — für einen künftigen
   FFS-Korpus trägt nur unadf/xdftool, nicht adfrescue.
4. **adf-tools-Ableitungstiefe:** wie viel Track_Copy.c (Commodore)
   tatsächlich im Code steckt, wurde nicht bestimmt — für das Urteil
   (irrelevant + ROT) unnötig.
