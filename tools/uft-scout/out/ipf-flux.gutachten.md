# Gutachten: mlund/ipf-flux

> Gemessen 2026-08-30 gegen HEAD `e39e00a4c9` (2026-07-23).
> Messdatei: `tools/uft-scout/work/ipf-flux.messung.json`.
> Inventar: `tools/uft-scout/work/inv.json` (SSOT ok, 88 Plugins, UFT-HEAD `bd2d5616`).
> Auftrag: Block 4 (MF-692). Auftragserwartung: **ROT** — MF-638,
> Lizenz vor Fähigkeit. Die Lizenzlage wurde trotzdem selbst geprüft;
> das Ergebnis ist nicht ROT, sondern **PRÜFEN**, und die Konsequenz
> ist dieselbe: **kein Übernahmeweg** ohne Eigentümer-Entscheid.

## Kategorie

**Zulieferung an LIZ-2 (Entscheidungsvorlage) + Methodik-Referenz.**
Kein Übernahme-Vorschlag.

## 1. Was es ist

Reiner Rust-IPF-Decoder (2 069 Zeilen `src/*.rs`, `wc -l`;
`forbid(unsafe_code)`): parst den CAPS/SPS-Container, prüft
Record-CRCs, dekodiert Spuren in Magnetzellen **ohne libcapsimage**
(README:5-9). Timing-Typen 2–9, Gap-Typen 0–3, Weak-Spans,
Write-Splice (README §Features). Verifikation laut README:
Ganz-Disk-Fingerprints über **5 569 IPF-Abbilder**, alle identisch mit
`libcapsimage` (README §Verification) — eine Selbstauskunft, hier
nicht nachgemessen (die 5 569 Abbilder liegen nicht bei, absichtlich).

## 2. Lizenzlage — die Kette, Glied für Glied

* `LICENSE` = BSD (Messung: `kennung: BSD-2/3, zone: GRUEN`); 7
  Quelldateien tragen `SPDX-License-Identifier: BSD-3-Clause`
  (Messung `lizenz_je_datei`; Beispiel `src/block.rs:1`).
* `NOTICE` erklärt die Herkunft: „informed by MAME's floppy-format
  code (chiefly `formats/ipf_dsk.cpp` …) which the MAME project
  licenses under … BSD-3-Clause"; `src/block.rs:4-5` sagt wörtlich
  „**Ported from** MAME's `generate_block*` (BSD)".
* Für Verhalten jenseits von MAME beansprucht `NOTICE` § „Clean-room
  provenance" Reverse-Engineering aus Daten mit `libcapsimage` als
  **Blackbox** über die öffentliche API, „not produced by reading,
  copying, translating, or decompiling proprietary CAPS/libcapsimage
  implementation source code".

Warum das trotz durchgehend BSD-3-Kennung **PRÜFEN** ist und nicht
GRÜN:

1. **Die Kette hat drei Glieder:** ipf-flux ← MAME `ipf_dsk.cpp` ←
   CAPS/SPS. Ob MAMEs IPF-Modell seinerseits sauber unabhängig vom
   proprietären CAPS-Code entstand, liegt außerhalb beider Repos und
   ist mit den Mitteln dieses Gutachtens **nicht messbar**. Eine
   Ableitungskette mit Wirkung stromabwärts ist genau der Fall aus
   Regel 8: Vorlage, keine Auslegung. (Die Messung selbst hat den
   `NOTICE` bereits als `UNGEKLAERT / PRUEFEN` markiert.)
2. **Clean-room-Erklärungen sind Selbstauskünfte.** Die Führung hier
   ist deutlich über Branchenschnitt (Baseline-Commit, per-Datei-
   Verweise, Oracle nie vendort) — aber eine Behauptung im Repo ist
   ein Hinweis, nie ein Beleg (dieselbe Regel wie bei READMEs).
3. **MF-638 als Präzedenz:** ein erreichbarer, funktionierender
   IPF-Parser (`uft_ipf_air.c`, GPL-3.0-Port, LIZ-2 Entscheidung 2
   offen) wiegt eine ungeklärte Ableitung nicht auf. Ein Rust→C-Port
   von ipf-flux würde LIZ-2 nicht lösen, sondern eine **zweite**
   nicht selbst prüfbare Kette daneben stellen.

Nach `playbook/lizenzmatrix.md` heißt PRÜFEN: auch die Oracle-Nutzung
geht nur über die Eigentümer-Vorlage.

## 3. Was das Repo dem Eigentümer trotzdem liefert

**a) Für LIZ-2, Entscheidung 2** (`docs/OPEN_ITEMS.md:2111` ff.:
„Neubau Clean-Room gegen die öffentliche CAPS/SPS-Spezifikation
(aufwendig)"): Es existieren inzwischen **zwei** blob-freie
IPF-Implementierungen außerhalb des CAPS-Ökosystems — ipf-flux
(BSD-3-Kette über MAME, 2 069 Zeilen) und fluxfox' eigene
MIT-Implementierung (eigenes Gutachten, `src/file_parsers/`). Die
Aufwandsschätzung „aufwendig" für einen Neubau ist damit neu zu
bepreisen: das Format ist zweifach unabhängig nachvollzogen und
dokumentiert. Das ändert nicht die Lizenzfrage, aber die
Optionenlage der Vorlage.

**b) Methodik-Vorbild für ORACLES.md:** `tools/caps-oracle.sh:1-13` —
das proprietäre `libcapsimage` wird **lokal** als Blackbox-Oracle
gelinkt, Golden-Vektoren und geschützte Abbilder werden **nie**
eingecheckt. Exakt das Muster, das unser Oracle-Verfahren verlangt,
in fremder, unabhängiger Ausführung.

**c) Nicht kopieren:** `tools/ipf-database.csv` (5 569 Zeilen
Fingerprints) ist eine kuratierte Datenbank → sui generis, Matrix-Zeile
PRÜFEN.

## 4. Attribution

ipf-flux (BSD-3-Clause, Mikael Lund), seinerseits Teil-Port von MAME
`ipf_dsk.cpp` (BSD-3-Clause, MAME-Team) mit
Clean-room-Selbstauskunft für den Rest. Kein Code übernommen, keiner
ohne Vorlage übernehmbar.

## 5. Bewegte Kennzahl

Direkt **keine** — `"ipf": vorhanden: true, tier: "T3"`
(Inventar-Abfrage zitiert), aber der Hebungsweg ist durch LIZ-2 +
PRÜFEN versperrt, bis der Eigentümer entscheidet. Als Vorlage bewegt
es die Kandidatin **fünfte Kennzahl** (Dateien mit ungeklärter
Herkunft, Befund-Stufe): LIZ-2 Entscheidung 2 bekommt eine dritte
Option mit anderem Preis.

## 6. Einhängepunkt

`docs/OPEN_ITEMS.md` § LIZ-2 (drei Entscheidungen; `git grep LIZ-2
docs/OPEN_ITEMS.md` findet ihn) — dieses Gutachten ist Anlage zur
Entscheidung 2.

## 7. Oracle-Kandidat

Formal ja (Sektor-/Zellebene, keine Datei-Ebene — Längensemantik
entfällt, stattdessen Zell-/Fingerprint-Abgleich), **aber**: PRÜFEN
heißt Vorlage auch für Oracle-Nutzung, cargo liegt auf dieser Maschine
nicht (`which cargo` leer, wie beim fluxfox-Gutachten), und der Baum
hat kein einziges IPF im Korpus (gegen `inv["korpus"]` geprüft: 0 von
24). Keine Registrierung vorgeschlagen.

## 8. Beschaffungsliste

Für die Vorlage: nichts. Für alles Weitere (erst nach
Eigentümer-Entscheid): Rust-Toolchain + mindestens ein frei
verteilbares IPF — Letzteres ist beim SPS-Ökosystem selbst ein
Beschaffungs- und Rechtsproblem, ehrlich benannt.

## 9. Aufwandsklasse

**S** für die Vorlage (dieses Gutachten als Anlage zu LIZ-2). Alles
andere hängt am Entscheid.

## UNGEKLÄRT

* Die Herkunft von MAMEs `ipf_dsk.cpp` gegenüber CAPS/SPS (Kettenglied
  3) — außerhalb der Messreichweite dieses Agenten.
* Ob die 5 569-Fingerprint-Behauptung reproduzierbar ist (bräuchte
  libcapsimage + IPF-Bestand, beides nicht beziehbar ohne Vorlage).
* Rechtscharakter des IPF-Formats selbst (SPS-Spezifikation vs.
  Implementierung) — Teil der LIZ-2-Vorlage, nicht dieses Gutachtens.
