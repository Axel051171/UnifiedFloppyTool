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

## Einfrier-Regel

**Kein neuer ungeprüfter Code im Format- oder Decoder-Layer, egal unter
welchem Namen** (neues Plugin, neue Variante, neuer Decoder, neue
Registrierung). Enforcement: `.claude/CLAUDE.md` Daueraufgabe 5 (Agenten) +
geplantes `check_consistency.py`-Gate (Phase 1).

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

  **Ehrlich offen (2/5) — kein kanonisches Erzeuger-Tool existiert:**
  - **FDI (ZX Spectrum):** SAMdisk liest nur (`ReadFDI`, kein Writer); der
    HxC-FDI-Loader hat `(WRITEDISKFILE) 0` = read-only. Beschaffungsweg:
    rechtefreies Homebrew-/PD-FDI aus der TR-DOS-Szene (origin=real, lokal
    in `tests/corpus/`, Hash ins Manifest) ODER ein FDI-schreibfähiges
    Emulator-Tool (UKV/Real Spectrum) in einer Session mit Nutzer-Hilfe.
  - **NFD (PC-98):** HxC hat gar keinen NFD-Loader; kanonischer Erzeuger ist
    T98-Next selbst (GUI-Emulator). Beschaffungsweg: T98-Next eine Disk
    erzeugen lassen (Nutzer-Session) oder rechtefreies PD-NFD-Image.

  Das **Moratorium bleibt in Kraft**, bis auch FDI + NFD-r0 auf T1/T1b sind.
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

## Hardware-Politik (unverändert + präzisiert)

Scaffolds bleiben (honest-stubs behaupten nichts); keine In-House-Benches
(MF-310). Neu: pro Controller ein dokumentiertes Community-Bench-Protokoll
(Phase 3), damit die Delegation real wird statt nominell.
