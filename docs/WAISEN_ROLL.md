# Waisenrolle — Symbole ohne Aufrufer, mit Art und Status

**Zweck (MF-1114, auf Eigentümer-Anweisung):** ein **Register statt
eines Sweeps**. Die Waisen-Grundlinie `docs/orphan_baseline.txt` führt
Dateien ohne Aufrufer als reine Pfadliste und wird über die
**Zeilenzahl** verglichen (206 = 206) — sie kann nicht sagen, *welcher
Art* ein Fund ist und *was* mit ihm geschehen soll. Die Namensrolle
`docs/FORMAT_ROLL.md` kann das für Formate. Diese Rolle tut es für
**Symbole**.

Sie wird **gepflegt, nicht erzeugt** — aus demselben Grund, den
`FORMAT_ROLL.md` nennt: ein Generator, der sie jedes Mal neu schreibt,
würde eine Löschung stillschweigend mitschreiben und damit genau das
verdecken, wogegen sie steht.

## Warum sie überhaupt gebraucht wird — am eigenen Bau gemessen

Der erste Lauf über `git ls-files src/formats` fand **13**
Dateischreiber der Form `uft_<fmt>_write`. MF-930 hatte **elf**
gezählt, und die Zahl stand seither in `CLAUDE.md`, in P3-204 und im
Kopf von Tor 57. **Zwei standen auf keiner Liste:**

| Symbol | wo | was die Messung sagt |
|---|---|---|
| `uft_atx_write` | `src/formats/atx/uft_atx.c` | 8 `fwrite`, Aufrufer **nur in Tests**, **kein** `.write_track` im Plugin — die Zusage ist aber ehrlich zurückgenommen (`Write = UNSUPPORTED`), also keine Falschaussage |
| `uft_ibm3740_write` | `src/formats/hardsector/uft_hardsector.c` | **zweiter** Schreiber in einer Datei, die schon einen führt |

Das ist der fünfzehnte Fall von *Aufzählung statt Messung* in diesem
Baum — und er ist beim Bau des Registers aufgefallen, nicht bei einem
Sweep drei Monate später. Genau das ist der Zweck.

## Felder

| Feld | Bedeutung |
|---|---|
| **Muster** | Symbolname oder Glob (`src/protection/**`). Ein Glob deckt eine Gruppe; so bleibt die Rolle lesbar, statt 3559 Einzelzeilen zu tragen. |
| **Art** | `schreiber` · `deklaration` · `erkennung` · `schutz` · `deepread` · `skelett` · `experiment` |
| **Status** | `verdrahtet` · `offen` · `ohne_koerper` · `absicht` · `zurueckgenommen` |
| **Beleg** | `MF-`/`P3-`/`P0-`-Nummer. Pflicht bei jedem Status außer `offen`, wo ein Plan-Anker genügt. |

## Regeln (`scripts/audit_waisenrolle.py`, im Konsistenzlauf)

1. **Vollständig bei der Art `schreiber`.** Jedes `uft_<fmt>_write` im
   Baum hat eine Zeile. Diese Art ist maschinell eindeutig messbar, und
   genau hier sind die zwei Nachzügler aufgefallen.
2. **Beleg-Pflicht.** `verdrahtet`, `absicht`, `zurueckgenommen` und
   `ohne_koerper` verlangen eine `MF-`/`P3-`/`P0-`-Nummer.
3. **Keine Zeile verschwindet** ohne Statuswechsel — verglichen wird
   gegen `HEAD`, wie bei der Namensrolle.
4. **Kein Neuzugang ohne Zeile** bei der Art `schreiber`.

Was die Rolle **nicht** tut: die 3559 Symbole mit der Marke `WAISE`
oder `NUR_TESTS` einzeln führen. `tools/uft-innendienst/scripts/tuersucher.py`
misst sie und schärft die Frage selbst — 3559 → 1777 (in
`include/uft/**` deklariert) → **289** (`ANGEBOT_OHNE_ABNEHMER`: der
Header wird von einem fremden `src/`-Verzeichnis eingebunden und
niemand ruft das Symbol). Eine Rolle mit 3559 Zeilen wäre Buchhaltung;
eine Zahl, die bei besserer Messung auf ein Zwölftel fällt, ist kein
Fortschritt (MF-1077, G2).

## Die Rolle

| Muster | Art | Status | Beleg |
|---|---|---|---|
| `uft_opus_write` | schreiber | verdrahtet | MF-931 |
| `uft_cfi_write` | schreiber | verdrahtet | MF-1004 |
| `uft_mgt_write` | schreiber | verdrahtet | MF-1006 |
| `uft_apridisk_write` | schreiber | verdrahtet | MF-1009 |
| `uft_nanowasp_write` | schreiber | verdrahtet | MF-1095 |
| `uft_myz80_write` | schreiber | verdrahtet | MF-1112 |
| `uft_qrst_write` | schreiber | verdrahtet | MF-1112 |
| `uft_logical_write` | schreiber | offen | P3-204 |
| `uft_posix_write` | schreiber | offen | P3-204 |
| `uft_hardsector_write` | schreiber | offen | P3-204 |
| `uft_ibm3740_write` | schreiber | offen | MF-1114 |
| `uft_atx_write` | schreiber | offen | MF-1114 |
| `uft_rcpmfs_write` | schreiber | absicht | MF-1035 |
| `uft_crc_get_config` | deklaration | ohne_koerper | MF-1113 |
| `uft_crc_get_config_by_name` | deklaration | ohne_koerper | MF-1113 |
| `uft_crc_get_table` | deklaration | ohne_koerper | MF-1113 |
| `src/protection/**` | schutz | absicht | P0-2 |
| `src/analysis/deepread/**` | deepread | offen | MF-627 |
| `include/uft/**` | skelett | absicht | P3-378 |
