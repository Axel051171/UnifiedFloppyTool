# Plan-Register und Anker-Regel

> **Ein Anker, den `git grep` nicht findet, ist keiner.**

Dieses Verzeichnis existiert wegen einer Lücke, die den Kreislauf reißen
ließ: Verwaisten-Regel, Gutachten und Priorisierung verweisen alle auf
„Plan-Anker" — aber mehrere Umsetzungspläne lebten außerhalb des Repos.
Damit war „gehört dieser Waisencode zu einem Plan?" eine
Erinnerungsfrage statt einer Abfrage.

Es ist dieselbe Bewegung wie MF-633 (Dateimengen aus `git ls-files`
statt aus gepflegten Listen), nur eine Ebene höher: auf Dokumenten
statt auf Dateien.

## Die Regel

1. **Kein Baustein ohne auffindbaren Anker.** Wer Code behält, der heute
   keinen Aufrufer hat, nennt den Baustein, der ihn verdrahten wird —
   in der Form
   `# ANKER: <Plandokument> <Abschnitt> (<ein Satz, was er tut>)`.
2. **Kein Anker außerhalb des Baums.** Ein Plan, der nur in einem Chat
   oder Kopf existiert, kann nicht ankern. Er gehört eingecheckt,
   bereinigt um Erledigtes und mit MF-Verweisen — dann zählt er.
3. **Die Verwaisten-Regel bleibt unverändert:** ohne Anker wird
   gelöscht. Git vergisst nichts, und ein Neubau mit Oracle ist billiger
   als ein Bestand ohne Zugang. Belegt: MF-626 (fdc_bitstream, 6483
   Zeilen), MF-635 (nibtools-Port).

## Register — Stand 2026-08-28

Gemessen mit `ls docs/*.md` und `grep -rlI <name> docs/`. Die Spalte
„ankerfähig" beantwortet genau eine Frage: findet `git grep` ein
Dokument, auf das sich ein Baustein berufen kann?

| Vorhaben | Dokument im Baum | ankerfähig |
|---|---|---|
| Gesamt-Reihenfolge | [`MASTER_PLAN.md`](../MASTER_PLAN.md) | **ja** |
| Fassung v4.1.7 | [`PLAN_v4.1.7.md`](../PLAN_v4.1.7.md) | **ja** |
| Mammut / Wellen | [`MAMMUT_PLAN.md`](../MAMMUT_PLAN.md) | **ja** |
| HAL-Verdrahtung M3 | [`M3_HAL_PLAN.md`](../M3_HAL_PLAN.md) | **ja** |
| Verifikation / Tier-Hebung | [`VERIFICATION_PLAN.md`](../VERIFICATION_PLAN.md) | **ja** |
| Stub-Beseitigung | [`STUB_ELIMINATION_PLAN.md`](../STUB_ELIMINATION_PLAN.md) | **ja** |
| Geplante APIs | [`PLANNED_APIS.md`](../PLANNED_APIS.md) | **ja** |
| a8rawconv-Übernahme | [`A8RAWCONV_INTEGRATION_TODO.md`](../A8RAWCONV_INTEGRATION_TODO.md) | **ja** |
| XCopy-Übernahme | [`XCOPY_INTEGRATION_TODO.md`](../XCOPY_INTEGRATION_TODO.md) | **ja** |
| **FluxEngine** | — nur erwähnt (`CAPABILITIES.md`, `BENCH_PROTOCOL.md`) | **nein** |
| **FloppyControl** | — nur erwähnt (`OPEN_ITEMS.md`, `KNOWN_ISSUES.md`) | **nein** |
| **DiskFlashback** | — nur erwähnt (`PLAN_v4.1.7.md`, `KNOWN_ISSUES.md`) | **nein** |
| **FDOS** | — **im ganzen `docs/` nicht genannt** | **nein** |

### Was die vier fehlenden bedeuten

Sie sind **keine** Ausrede, Code ohne Anker zu behalten. Solange ein
Plan nicht im Baum liegt, kann sich kein Baustein auf ihn berufen — die
Verwaisten-Regel greift dann so, als gäbe es ihn nicht. Das ist hart und
beabsichtigt: sonst wäre „der Plan steht in einem Chat" die
Universalausrede.

**Zu beschaffen (Eigentümer):** die vier Dokumente, bereinigt um
Erledigtes. Danach gehören sie hierher oder nach `docs/`, und diese
Tabelle wird nachgezogen.

## Wo Anker heute schon stehen

`docs/orphan_baseline.txt` führt die Form vor. Wer einen setzt, schreibt
ihn dorthin neben die Datei, nicht in den Quellcode — damit eine einzige
Abfrage alle Anker zeigt:

```
grep -n "ANKER:" docs/orphan_baseline.txt
```

## Warum kein Tor darauf

Ein Tor, das jeden Waisen ohne Anker rot meldet, würde heute 226-mal
feuern. Die Grundlinie **ist** dieses Tor, nur eingefroren: neue Waisen
melden, alte geduldet nennen. Ein zweites Tor auf derselben Frage wäre
Lärm. Was fehlt, ist nicht Strenge, sondern die vier Dokumente.
