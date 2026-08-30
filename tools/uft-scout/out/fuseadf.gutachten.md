# Gutachten: t-w/fuseadf

> Gemessen 2026-08-29 gegen HEAD `6956da5` (2025-09-30).
> Messdatei: `tools/uft-scout/work/fuseadf.messung.json`.
> Inventar: `tools/uft-scout/work/inv.json` (SSOT ok, 88 Plugins, UFT-HEAD `bd2d5616`).
> Auftrag: Block 4 (`data/auftraege.json`), Andockstelle **FS-2**.

## Kategorie

**Daten (Fixture-Quelle) + Verhaltens-Referenz.** Der eigentliche Fund
liegt nicht in fuseadfs Code, sondern in seiner Testdaten-Beschaffung.

## 1. Was es ist

FUSE-Treiber, der ein ADF als Dateisystem einhängt. 3 418 Zeilen C
(`wc -l src/*.c`), GitLab-CI, 4 Testdateien. Die gesamte
Dateisystem-Logik delegiert an **ADFlib** (`configure.ac:32`:
`PKG_CHECK_MODULES(ADF, adflib >= 0.10.5)`).

## 2. Der Fund: FS-2 braucht fremd erzeugte Link-Abbilder — hier steht, wo sie liegen

`docs/OPEN_ITEMS.md` FS-2 nennt als benannte Grenze: FFS,
Unterverzeichnisse, Hard-/Softlinks, LFS/intl — „Jeder dieser Fälle
braucht ein fremd erzeugtes Abbild". `xdftool` kann Links nicht anlegen
(Auftragstext). fuseadf kann es auch nicht — **gemessen**:
`src/adffs.c:971` `.symlink = NULL`, `:973` `.link = NULL`. Aber es
**liest** sie (über ADFlib: `src/adfimage.c:368-381` mappt
`ADF_ST_LFILE/LDIR/LSOFT`; `adfimage_readlink()` ab
`src/adfimage.c:678` löst `realName`/`realEntry` auf).

Und sein Test-Setup zeigt die Quelle: `tests/prepare_test_data.sh:23-37`
lädt aus der **ADFlib-Testsuite**
(`https://github.com/adflib/ADFlib/raw/master/tests/data/Dumps`):

| Datei | deckt (FS-2-Grenze) |
|---|---|
| `links.adf` | Hard-/Softlinks |
| `test_link_chains.adf` | Link-Ketten (im ADFlib-Verzeichnis, nicht im fuseadf-Skript) |
| `testffs.adf` | FFS-Datenblöcke |
| `ffdisk0049.adf` | Unterverzeichnisse >1 Ebene (Fred-Fish #49, „(PD)") |
| `win32-names.adf`, `cache_crash.adf` | Namens-/Cache-Randfälle (Eignung ungeklärt) |

**[MESSBAR], zwei unabhängige Quellen (eigener Baum zählt nicht mit):**
(1) `fuseadf/tests/prepare_test_data.sh:23-37` (Dateinamen + URL);
(2) GitHub-Verzeichnislisting `adflib/ADFlib/tests/data/Dumps`
(abgerufen 2026-08-29): `links.adf`, `test_link_chains.adf`,
`testffs.adf`, `blank.adf`, `win32-names.adf` u. a. vorhanden.

Drei-Hände-Konstellation für den Kreuzvergleich (ORACLES.md, fünfte
Frage): Fixture von **ADFlib**, Oracle **xdftool** (amitools,
unabhängige Implementierung), Prüfling **UFT-Leser**. Keine zwei davon
teilen Code.

## 3. Lizenz

`COPYING` = **GPL-2.0** (aus der Datei; `vermessen.py` bestätigt).
Zone **GRÜN**. Konsequenz: sogar Port wäre zulässig — wird aber nicht
gebraucht; es wandert kein Code, nur Fixtures. Die Abbilder selbst:
ADFlib-Testdaten aus dem ADFlib-Repo (ADFlib ist GPL-2.0+);
Fred-Fish #49 ist im Dateinamen als „(PD)" geführt. **Herkunftsvermerk
je Abbild gehört ins Manifest; Zone der Abbilder als Daten: PRÜFEN-frei
für Testzwecke, bei Redistribution im Repo → Eigentümer entscheidet**
(Korpus ist ohnehin gitignored, nur SHA-256-Manifeste im Baum).

**Attribution:** fuseadf (GPL-2.0, t-w) als Wegweiser; ADFlib-Testsuite
(GPL-2.0+) als Fixture-Quelle. Kein Code übernommen.

## 4. Bewegte Kennzahl

Keine der vier direkt (ADF steht auf **T1b**; Inventar-Abfrage:
`"adf": vorhanden: true, tier: "T1b", plugin_liste_vollstaendig: true`).
**Zulieferung an den bestehenden Punkt FS-2** — kein neuer
OPEN_ITEMS-Eintrag nötig, die Beschaffungsliste schließt dessen
benannte Grenzen. Verifikationsarbeit, von der Einfrier-Regel
ausdrücklich erlaubt.

## 5. Einhängepunkt

`docs/OPEN_ITEMS.md` § FS-2 (MF-685) — dort steht wörtlich „Jeder
dieser Fälle braucht ein fremd erzeugtes Abbild"; `git grep FS-2
docs/OPEN_ITEMS.md` findet ihn.

## 6. Oracle-Kandidat

Kein neues Oracle. `xdftool` ist registriert und kalibriert (roh,
MF-685). **UNGEKLÄRT, vor dem Differenzlauf zu messen:** ob xdftool
Links listet/auflöst — sonst als Zweithand `unadf` (teilt ADFlib mit
dem Fixture-Erzeuger → nur mit dieser Einschränkung benannt).

## 7. Beschaffungsliste

Gegen `inv["korpus"]` geprüft: für ADF liegt **nur**
`tests/corpus_free/xdftool_dd_ofs.adf` (cross-tool, xdftool). Keine der
sechs ADFlib-Dateien liegt. Beschaffung = sechs `wget` gegen das
ADFlib-Repo + SHA-256 ins Manifest + Herkunftszeile je Datei.

## 8. Aufwandsklasse

**S** (Beschaffung + Manifest) für die Fixtures; die eigentliche
FS-2-Prüfarbeit (Rotbeweise gegen `is_hardlink`/`real_entry`/
`link_target`) ist Stufe 4 nach deren Regeln.

## UNGEKLÄRT

* Ob `win32-names.adf` den intl-Schalter (`uft_amiga_hash_name(…,
  bool intl)`) abdeckt — Inhalt nicht inspiziert.
* Für LFS (lange Dateinamen) ist in der ADFlib-Suite kein Kandidat
  erkennbar — diese FS-2-Grenze bleibt offen.
* Ob xdftool Links überhaupt anzeigt (siehe §6).
