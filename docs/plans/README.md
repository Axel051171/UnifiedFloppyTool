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
| FloppyControl | [`FLOPPYCONTROL.md`](FLOPPYCONTROL.md) | **ja** (seit MF-641) |
| DiskFlashback | [`DISKFLASHBACK.md`](DISKFLASHBACK.md) | **ja** (seit MF-641) |
| FluxEngine (Software-Übernahmen) | [`FLUXENGINE.md`](FLUXENGINE.md) | **ja** (seit MF-643) |
| FreeDOS-FORMAT | [`FDOS_FORMAT.md`](FDOS_FORMAT.md) | **ja** (seit MF-643) |
| Tore-Bündel (Eigentümer-Entscheidungen) | [`TORE_BUENDEL.md`](TORE_BUENDEL.md) | — kein Anker, sondern eine Tagesordnung |
| Umsetzungsliste (nach Reife geordnet) | [`UMSETZUNGSLISTE.md`](UMSETZUNGSLISTE.md) | — kein Anker, sondern eine Reihenfolge |

**Das Register ist damit vollständig.** Alle zuvor fehlenden Pläne
liegen im Baum; jeder Baustein kann sich auf ein auffindbares Dokument
berufen.

### Wie die letzten zwei entstanden sind

Nicht durch Erfinden. Der Eigentümer hat die Chat-Artefakte geliefert,
gegen HEAD `e97888fc` neu vermessen — und die Messung hat drei Dinge
korrigiert, die aus dem Gedächtnis falsch gewesen wären: FluxEngine ist
Zone **GRÜN** statt GELB, der VFS-Baustein gehört zu
`PLAN_v4.1.7.md` Phase 1 (ein Baustein, ein Ort), der Sektor-Cache zu
`DISKFLASHBACK.md`.

Eine vierte Korrektur kam beim Einchecken dazu (MF-643): FluxEngine ist
**GPL-2.0-only**, nicht „or later". Portierbar bleibt es — aber ein Port
verengt unseren Baum von `or-later` auf `only`. Steht als Nachtrag in
`FLUXENGINE.md`.

### Warum das vorher zu Recht verweigert wurde

Weil es nichts zu schreiben gäbe. Gemessen (`grep -rn <name> docs/`):

* **FluxEngine** erscheint ausschließlich als *Hardware-Controller* —
  Zeile in `CAPABILITIES.md`, Bench-Zeile in `BENCH_PROTOCOL.md`. Kein
  einziger Baustein, keine Bedingung, kein Ziel.
* **FDOS** kommt in `docs/` **überhaupt nicht** vor.

Eine leere Plandatei anzulegen wäre kein Fortschritt, sondern ein
**erfundener Anker**: sie sähe auffindbar aus und trüge nichts. Genau
diese Bauart — ein Dokument, das etwas verspricht, was nicht dahinter
liegt — ist die Fehlerklasse, gegen die die Regel geschrieben wurde.

Die beiden anderen konnten geschrieben werden, weil der Baum ihre
Bausteine **benennt**: `KNOWN_ISSUES.md:6606` („Baustein A des
FloppyControl-Umsetzungsplans"), `:6554` („Baustein B des
DiskFlashback-Entwurfs") und `PLAN_v4.1.7.md:261` (Mount-Kette). Aus
Belegtem lässt sich verdauen; aus Nichts nicht.

### Was die zwei fehlenden bedeuten

Sie sind **keine** Ausrede, Code ohne Anker zu behalten. Solange ein
Plan nicht im Baum liegt, kann sich kein Baustein auf ihn berufen — die
Verwaisten-Regel greift dann so, als gäbe es ihn nicht. Das ist hart und
beabsichtigt: sonst wäre „der Plan steht in einem Chat" die
Universalausrede.

**Zu liefern (Eigentümer):** der Inhalt für FluxEngine und FDOS — welche
Bausteine offen sind, je mit Kennzahl-Bezug. Aus einem Satz Substanz
wird hier eine Baum-Fassung; ohne Substanz bleibt die Zeile leer, und
das ist die ehrlichere Lage.

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
Lärm. Was fehlt, ist nicht Strenge, sondern die zwei restlichen Dokumente.
