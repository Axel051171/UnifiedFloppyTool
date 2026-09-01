# Verifikations- & Sanierungsplan (MF-363)

Beschlossen 2026-08-16 (Grilling-Session). Dieses Dokument ist die SSOT für
die Einfrier-Regel, die Verifikations-Stufen und die Sanierungs-Phasen.
Bei Konflikt mit älteren Doku-Aussagen gewinnt dieses Dokument.

## Ausgangsbefund (Zensus 2026-08-16)

- 88 registrierte Plugins / 138 Format-IDs, aber **kein** Format je gegen ein
  reales Referenz-Image verifiziert: `tests/golden/` = nur Framework-Code,
  `tests/vectors/` = leer (0 Bytes), obwohl beide als Ground-Truth
  dokumentiert waren.
- 5 Parser waren gegen **erfundene Specs** gebaut und ausgeliefert
  (FMT-2/3/10/11/12) — gleiche Fehlerklasse, Ursache: Code-Produktion
  schneller als Verifikation, ohne Gegengewicht.
- 1 von 9 Controllern production-wired (Greaseweazle), **ohne** Bench-Pass in
  diesem Release; keine physische Hardware vorhanden (MF-310: Tier-3
  community-delegiert).
- Zahlen-Drift quer durch die Doku (80/84/88/138 Plugins, ctest 151 vs. real
  205); README behauptete „80 fully wired" + „production-ready".
- MASTER_PLAN.md (intern ehrlich): 133 Skeleton-Header, 2.613 unimplementierte
  Deklarationen — „Kluft zwischen Anspruch und Umsetzung ist das strukturelle
  Kern-Problem."

Meta-Befund: Die Formatliste ist Teil der forensischen **Selbstauskunft**
des Tools. Ist sie unzuverlässig, ist jede Ausgabe unzuverlässig — nicht weil
Decoder falsch rechnen, sondern weil niemand weiß, welche je gegen eine reale
Datei liefen. „Keine erfundenen Daten" gilt auch auf Produkt-Ebene.

## Verifikations-Stufen (pro Format, skript-generiert)

| Stufe | Bedeutung |
|---|---|
| **T1** | Test gegen reales Referenz-Image (Hardware-Dump / Original-Datei) |
| **T1b** | Test gegen **cross-tool-erzeugtes** Image: kanonisches Fremd-Tool (VICE, WinUAE, HxC, SAMdisk, Emulator) erzeugt die Datei, UFT liest sie. Ersatz-Ground-Truth solange kein Hardware-Dump möglich — hätte alle 5 Fabrikationen gefangen |
| **T2** | Synthetischer Round-Trip **+** Spec gegen autoritative Referenz-Implementierung verifiziert (SAMdisk/Deark/WinUAE/HxC-Quelle) |
| **T3** | Ungeprüft / nur Metadaten |

Wichtig: Die 205 grünen Tests sind fast alle synthetisch. Synthetische Tests
beweisen Selbstkonsistenz, nicht Realwelt-Korrektheit — die fabrizierten
Parser hatten grüne synthetische Logik gegen erfundene Specs.

**Provenienz-Regel (verbindlich, 2026-08-16):** Provenienz ist konstitutiv
für T1/T1b, nicht dekorativ. Ohne sie ist „Parser falsch" von „Datei
beschädigt / von exotischem Tool erzeugt" nicht unterscheidbar — exakt die
Unentscheidbarkeit, die die Fabrikationen ermöglichte. Konkret:

- **T1b (cross-tool)** verlangt im Manifest: Erzeuger-Tool **und** Version
  **und** reproduzierbaren Erzeugungsweg/Lizenz. Fehlt eines, zählt der
  Eintrag nicht (Gate erzwingt das).
> **Offene Lesart (MF-778):** der Schrägstrich in „Erzeugungsweg/**Lizenz**" ist zweideutig — *Weg **oder** Lizenz* oder *Weg **und** Lizenz*? Gemessen an den 14 cross-tool-Einträgen des Manifests: **alle 14** nennen Werkzeug und Erzeugungsweg, **keiner** nennt eine Lizenz. Auf der strengen Lesart wäre also **kein einziges** T1b belegt; auf der milden sind es 13 von 14. Das ist kein Detail — es entscheidet über die Hälfte der Stufentabelle. **Eigentümer-Entscheidung.**
>
> Zwei Nebenbefunde derselben Messung: `adf` nannte statt einer Version nur ein Datum („pip amitools, 2026-08") — seit MF-778 steht dort **0.8.1**, per `importlib.metadata` gemessen. Und `dim_atari` trägt einen **Klon-Hash** (`05b53aa`) statt einer Versionsnummer; das ist für ein aus Quellen gebautes Werkzeug **stärker**, nicht schwächer — eine Prüfung, die nur nach `\d+\.\d+` sucht, meldet es fälschlich als Mangel. Wer hier ein Tor baut, muss beide Formen zulassen.

- **T1 (real)** verlangt: dokumentierte archivalische Herkunft (Quelle/URL +
  SHA-256) **plus** unabhängigen Spec-Pin — die Assert-Bytes werden VOR dem
  UFT-Lauf per unabhängigem Parse (python, gegen die publizierte Spec)
  gepinnt, weil der historische Erzeuger unbekannt ist.
- Ein Netz-Fund ohne diese Angaben ist **kein** T1/T1b — bestenfalls
  T2-Material. „Irgendwo lag ein Image" ist keine Ground Truth.

## Einfrier-Regel

**Kein neuer ungeprüfter Code im Format- oder Decoder-Layer, egal unter
welchem Namen** (neues Plugin, neue Variante, neuer Decoder, neue
Registrierung). Enforcement: `.claude/CLAUDE.md` Daueraufgabe 5 (Agenten) +
geplantes `check_consistency.py`-Gate (Phase 1).

### Was „geprüft" heißt — operational (MF-498, 2026-08-23)

Der Zweck von MF-363 war nie „kein neuer Code", sondern **keine Parser
gegen erfundene Specs**. Die Formulierung oben ließ offen, was „geprüft"
bedeutet, und war damit in beide Richtungen unbrauchbar: sie blockierte
messbar belegte Arbeit, und sie hätte eine erfundene Spec mit grünen
synthetischen Tests durchgelassen — genau das, was die fünf Fabrikationen
hatten.

Neuer Code im Format- oder Decoder-Layer ist zulässig, wenn **alle drei**
Bedingungen erfüllt sind:

**(a) Gemessene Referenz.** Das Verhalten stammt aus einer *benannten*
äußeren Quelle — ein Oracle-Werkzeug, eine autoritative Spec, eine reale
Aufnahme — **oder** aus einer Messung am eigenen bereits belegten Pfad,
die *vor* dem Code steht (Rotbeweis zuerst).

**(b) Jede Behauptung im Commit ist gemessen.** Zahlen in Commit-Text,
Header und `KNOWN_ISSUES.md` sind Messwerte, keine Schätzungen. Was nicht
gemessen wurde, steht ausdrücklich als *nicht belegt* da.

**(c) Die Referenz steht im Header.** Wer die Datei in zwei Jahren öffnet,
findet dort, woher das Verhalten stammt: Werkzeug samt Version, Spec samt
Fundstelle, oder die Messung samt Aufbau.

**Was das ausdrücklich NICHT erfüllt:**

- „Ich habe es getestet" ohne benannte Referenz. Synthetische
  Selbstkonsistenz beweist Selbstkonsistenz — die fabrizierten Parser
  hatten grüne Tests.
- Ein Rotbeweis, der **nicht feuert**. Er belegt nichts; er verdeckt eine
  Lücke, die wie Evidenz aussieht (MF-497).
- Ein Messaufbau, der den Produktionspfad **nachbaut** statt ihn zu
  benutzen. Er misst eine andere Verdrahtung — in dieser Codebasis zweimal
  passiert (MF-494, MF-497), beide Male mit falschem Ergebnis in einem
  bereits veröffentlichten Dokument.

**Unverändert bleibt die Rückstands-Regel.** Ein neues Format-*Plugin*
kostet weiterhin zwei Hebungen auf T1/T1b (1:2, siehe unten). Diese
Präzisierung sagt, welcher Code überhaupt zulässig ist — nicht, dass der
Rückstand egal wäre.

**Vorgeschichte:** Die Regel wurde in dieser Form nötig, weil drei
aufeinanderfolgende Decoder-Bausteine (MF-492/495/496) unter der strengen
Lesart blockiert gewesen wären, obwohl jede ihrer Behauptungen gemessen
und durch feuernde Rotbeweise belegt war — einschließlich zweier
Selbstkorrekturen an eigenen, zu großzügig formulierten Zahlen. Eine Regel,
die diese Arbeit verbietet und eine erfundene Spec durchlässt, misst das
Falsche.

- **Moratorium** bis: (a) Label-Skript läuft, (b) die ersten 5 Formate
  (ATR, D64, ADF, FDI, NFD-r0) auf T1/T1b gehoben sind.
- Danach **1:2**: ein neues Format kostet zwei Hebungen (baut den Rückstand
  ab; 1:1 hielte ihn nur konstant). Verhältnis bei Moratorium-Ende
  bestätigen.
- **Erlaubt bleiben:** Bugfixes, Verifikations-/Test-/Korpus-Arbeit,
  Spec-Korrekturen gegen autoritative Quellen.

Auswahl-Begründung der ersten 5: ATR (eigene Atari-Sammlung), D64/ADF
(meistgenutzte Familien, VICE/WinUAE als kanonische Erzeuger), FDI + NFD-r0
(frisch gefixte Fabrikationen — deren Korrekturen sind bisher nur synthetisch
bestätigt; ein echtes Fremd-Tool-Image ist dort der härteste Test).

## Korpus-Politik

- `tests/corpus/` — lokal, **gitignored** (Urheberrecht: reale Images enthalten
  fast immer geschützte Inhalte)
- `tests/corpus_manifest/` — im Repo: SHA-256 + Herkunftsangabe pro Image;
  Dritte beschaffen das Korpus selbst und prüfen gegen die Hashes
- Rechtefreie, selbst erzeugte Images (cross-tool, eigene Inhalte) dürfen
  direkt ins Repo (Mindest-Korpus)

## Phasen

- **Phase 0 — Falschauskunft beenden (ERLEDIGT mit MF-363):** README-Schnitt
  Stufe 1 („80 fully wired" / „production-ready" raus, Zahlen korrigiert,
  „kein Bench-Pass" explizit), `tests/vectors`-Fiktion gestrichen,
  Einfrier-Regel in CLAUDE.md verankert.
- **Phase 1 — Werkzeug (ERLEDIGT mit MF-364):**
  (1) `scripts/gen_verification_tiers.py` → generiert
  [`VERIFICATION_TIERS.md`](VERIFICATION_TIERS.md) aus Registry +
  Test-Verdrahtung (tests/CMakeLists + Symbol-Referenzen + Directory-Credit) +
  [`spec_verification.json`](spec_verification.json) +
  `tests/corpus_manifest/manifest.json`. Erst-Stand: **T1=0, T1b=0, T2=15,
  T3=73** — die ehrliche Formatliste.
  (2) `scripts/update_inventory.py`: Plugin-Zahlen-Claims in
  README/CLAUDE.md gegen Code geprüft, Stale-Pattern-Blacklist.
  (3) Freeze-Gate: `scripts/format_freeze_baseline.json` (88 Symbole) —
  neue Registrierung ohne Whitelist-Eintrag ⇒ FAIL. Alle drei als
  Kategorien 6-8 in `check_consistency.py` (läuft in pre-commit + CI);
  Tier-Tabelle hat Lockfile-Semantik (stale ⇒ FAIL ⇒ `--write`).
  Befund nebenbei: die MF-332-EDSK-Verifikation gilt dem `dsk_cpc`-Plugin;
  das separate `edsk`-Plugin (amstrad/) ist ungetestet (T3). Der
  d67-Test testet `commodore/d67.c`, nicht das registrierte Plugin
  `d67/uft_d67.c` (T3) — zwei ehrliche Doppel-Implementierungs-Funde.
- **Phase 2 — Korpus + Hebungen (TEIL-ERLEDIGT mit MF-365, 3/5):**
  `tests/corpus_free/` (tracked, rechtefrei) + Manifest-Einträge mit SHA-256 +
  Provenienz; Korpus-Integritäts-Gate (Kategorie 9 in check_consistency:
  Hash-Mismatch ⇒ FAIL, negativ getestet). Gehoben auf **T1b**:
  - **D64** — VICE 3.10 `c1541` erzeugt (format + Datei geschrieben);
    `test_corpus_d64` verifiziert BAM-Signatur, PETSCII-Directory-Eintrag,
    21/19-Sektor-Zonen.
  - **ADF** — amitools `xdftool` erzeugt (OFS, Volume + Datei);
    `test_corpus_adf` verifiziert `DOS\0`-Bootblock, Rootblock-Typ,
    BCPL-Volume-Name, 80×2×11×512.
  - **ATR** — atrcopy-10.1-Template `dos2sd.atr` (pristine, byte-identisch
    zum pip-Artefakt = beweisbare Provenienz); `test_corpus_atr` verifiziert
    ATR-Header, DOS-2-VTOC (02 C3 02 C3), 720×128-Geometrie.

  **FDI auf T1 gehoben (MF-367, 4/5):** reales FDI beschafft — **Spectrofon
  #01** (1994er TR-DOS-Diskmagazin, zur freien Verteilung gemacht; via
  zxart.ee-Archiv, Release 428381). Lokal in `tests/corpus/` (gitignored),
  SHA-256 + Bezugs-URL im Manifest; `test_corpus_fdi` **skippt** bei
  fehlendem Image (exit 77, ctest SKIP_RETURN_CODE) und verifiziert lokal:
  83-Zylinder-Geometrie (nicht-standard!), TR-DOS-Struktur (16×256,
  Volume-Sektor R=9 Typ 0x16/Sig 0x10, per-Sektor-Offsets), CRC-Flags, und
  die **teil-formatierte letzte Spur (13/15 Sektoren)** — eine
  Realwelt-Irregularität, die kein synthetischer Test erzeugt. Alle
  Assert-Bytes vorab unabhängig (python, Spec-Parse) gepinnt.

  **Ehrlich offen (1/5): NFD.** Recherche abgeschlossen — es existiert
  KEIN zugängliches Werkzeug, das NFD schreibt: greaseweazle (read-only),
  NP2kai + np21/w (newdisk erzeugt nur D88), DOSBox-X/MAME/fluxengine/
  ds6_util (alle read-only), HxC (kein NFD-Loader). Einziger Erzeuger ist
  der GUI-Emulator **T98-Next** selbst. Beschaffungswege: (a) Nutzer-Session
  mit T98-Next (Blank-NFD erzeugen), (b) rechtefreies PD-NFD, falls eines
  auftaucht. Das **Moratorium bleibt in Kraft**, bis NFD-r0 auf T1/T1b ist.
- **Phase 3 (ERLEDIGT mit MF-366, bis auf Verhältnis-Bestätigung):**
  - **README Stufe 2:** Tier-Zusammenfassung (T1/T1b/T2/T3) eingebettet und
    per neuem Drift-Gate gegen die berechneten Werte geprüft (negativ
    getestet).
  - **Reifegrad-Tabelle aller Subsysteme:**
    [`SUBSYSTEM_MATURITY.md`](SUBSYSTEM_MATURITY.md) — Kernbefunde: DeepRead
    0 aktive Tests, OTDR-Test excluded, ML-Classifier nie gegen echte Disk,
    kein Protection-Scheme je gegen eine echte geschützte Original-Disk,
    Audit-Trail-API war Phantom.
  - **Bench-Protokolle:** [`BENCH_PROTOCOL.md`](BENCH_PROTOCOL.md) — pro
    bench-fähigem Controller (GW/SCP/KryoFlux/FC5025) konkrete Schritte +
    GO-Kriterium + Meldeformat; Scaffold-Controller explizit „noch nicht
    bench-fähig".
  - **Tote-Deklarationen-Welle:** die letzten 9 Skeleton-Header (162
    Phantom-Deklarationen) + ihre 5 toten (excluded) Test-Dateien gelöscht —
    darunter die nie implementierte Audit-Trail-/Forensic-Report-C-API
    (`uft_audit_trail.h`/`uft_forensic_report.h`; real existieren GUI +
    Provenance, CLAUDE.md §6 trägt jetzt einen Ehrlichkeits-Hinweis) und die
    UFT_SKELETON_PLANNED-ML-Header. Zensus danach: **0 Skeleton-Header, 0
    Phantom-Deklarationen**. Regel bleibt: keine neue Deklaration ohne
    Körper.
  - **Offen:** Verhältnis-Bestätigung bei Moratorium-Ende (Q2, Default 1:2).

## Korpus-Ausweitung über den Format-Layer hinaus (MF-377/378)

Die Tier-Systematik gilt formal für Format-Plugins; das Prinzip „gegen reale
Daten oder gar nicht" gilt für **jede** Schicht, die Aussagen über eine
Diskette macht. Erste Anwendung außerhalb des Format-Layers: die
Kopierschutz-Erkennung.

- **Korpus:** 34 reale Atari-ST-Loader aus dem dec0de-Projekt
  (Commit-gepinnt, Ground Truth Datei→Schutzsystem aus `samples/README.txt`).
  Spielecode ist urheberrechtlich geschützt → lokal-only in `tests/corpus/`,
  sha256 + Bezugsweg im Manifest (Provenienzregel MF-368 gilt unverändert).
- **Befund CopyLock ST:** Series 2 auf 14/14 realen Loadern erkannt, `magic32`
  und Offset exakt reproduziert — und zwar von **zwei** unabhängigen
  Implementierungen. Series 1 auf 0/16 erkannt, strukturell blind
  (`KNOWN_ISSUES.md` PROT-1).
- **Befund `uft_dec0de_detect()`:** 0/34 korrekt, 3 Fehl-Labels, Signaturen
  nachweislich erfunden (PROT-3). Damit ist belegt, dass die Fabrikation nicht
  auf den Format-Layer beschränkt war. Für den Umgang gilt dieselbe Regel wie
  bei FMT-2/3: erst belegen, dann entfernen — nicht stillschweigend
  weiterbetreiben.
- **Konsequenz für die Einfrier-Regel:** unverändert in Kraft. Diese Arbeit
  ist Verifikations-/Korpus-Arbeit plus ein Bugfix an Bestehendem (PROT-4),
  also ausdrücklich erlaubt; es wurde kein neuer Detektor und kein neues
  Format registriert.

## Hardware-Politik (unverändert + präzisiert)

Scaffolds bleiben (honest-stubs behaupten nichts); keine In-House-Benches
(MF-310). Neu: pro Controller ein dokumentiertes Community-Bench-Protokoll
(Phase 3), damit die Delegation real wird statt nominell.
