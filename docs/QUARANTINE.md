# Quarantäne-Liste

Verfahren: [`QUARANTINE_PROCESS.md`](QUARANTINE_PROCESS.md).
Schema der Felder dort in §3.

> **Kennzahl.** Die Zahl **offener** Zeilen (Weg noch nicht beschritten)
> ist die Datenquelle für „Dateien mit ungeklärter Herkunft" — die
> begründete fünfte Release-Kennzahl (CLAUDE.md, MF-640).
>
> **Stand 2026-08-28: 1 vollzogen, 4 vorgemerkt, 0 aufgelöst.**

---

## Vollzogen

### `src/protection/c64/uft_track_align.c` (+ Header + Test)

| Feld | Inhalt |
|---|---|
| **Datei** | `src/protection/c64/uft_track_align.c` (1175 Z.), `include/uft/protection/uft_track_align.h` (622 Z.), `tests/test_track_align.c` (602 Z.) |
| **Verdacht** | **nibtools** (rittwage), Zone **GELB** (GPL-3.0). Beleg: Kopf `:5` „Based on nibtools by Pete Rittwage and Markus Brenner", `:479` nennt `prot.c align_rl_special()` |
| **Datum der Quelle** | LICENSE erst **2025-01-30** (`a549c18`) — davor **lizenzlos**. Aufnahme in unseren Baum **2026-02-08** (`4d622192`), also nach der GPL-3-Lizenzierung |
| **Betroffene Fähigkeit** | **keine.** 34 exportierte Funktionen, **0 Produktions-Aufrufer** (der einzige Kandidat `find_sync` war ein Namensgleichklang: `uft_mfm_sector_parser.c:104` und `uft_g64.c:223` führen je ein eigenes `static`) |
| **Audit-Stand** | **portiert** — belegt. `shift_buffer_left` zeichengleich samt des erfundenen Bezeichners `carryshift`, des Sentinels `tempbuf[length]=0x00` und des Schleifenausdrucks; **sieben** wörtliche Kommentar-Echos („back up a little", „set first byte to shift", „shift buffer left to edge of sync marks", „36 - 42 (non-standard)"); alle 14 `prot.c`-Funktionen haben eine Entsprechung, **acht namensgleich** |
| **Vorgesehener Weg** | **2** — Clean-Room-Neubau, wenn ein Baustein ihn verlangt |
| **Oracle** | `nibscan` (nibtools) — hardwarefrei mit MinGW baubar, vom Scout gebaut und gelaufen. **Noch nicht registriert**, siehe `ORACLES.md` §vorgemerkt |
| **MF** | Quarantäne **MF-635**; Bau-Nachzug **MF-637** (drittes CMakeLists übersehen) |

---

## Vorgemerkt — Audit fertig, Vollzug wartet auf `LIZ-2`

Diese vier liegen **noch im Baum und werden gebaut**. Das Audit ist
abgeschlossen; der Vollzug ist eine Eigentümer-Entscheidung, weil einer
der Fälle eine Fähigkeit kostet.

| Datei | Zeilen | Verdacht (Beleg im **eigenen** Kopf) | Fähigkeit | Weg | Oracle |
|---|---|---|---|---|---|
| `src/formats/ipf/uft_ipf_air.c` | 975 | „**Full port** of AIR `IPFReader.cs`/`IPFStruct.cs`/`IPFWriter.cs` to C. Original: © 2014 Jean Louis-Guerin (**GPL-3.0**)" | **IPF-Lesen** — das Plugin ruft `ipf_air_alloc` (`:70`), `_free` (`:78`), `_get_geometry` (`:85`), `_get_track_meta` (`:135`); keine statischen Gleichnamen | **3** (capsimg als Helper) oder 2 | capsimg — Lizenz ungeprüft |
| `src/formats/kfx/uft_kfstream_air.c` | 908 | „Full port of AIR `KFReader.cs` to C. © 2013-2015 SPS & Jean Louis-Guerin (**GPL-3.0**)" | **keine** — 0 Aufrufer, steht in `orphan_baseline.txt` | 2 | KryoFlux DTC |
| `src/formats/stx/uft_stx_air.c` | 914 | „Full port of AIR `PastiRead.cs`/`PastiStruct.cs`/`PastiWrite.cs` to C. © 2014 Jean Louis-Guerin (**GPL-3.0**)" | **keine** — 0 Aufrufer, steht in `orphan_baseline.txt` | 2 | Pasti-Spec liegt (T2-Quelle) |
| `src/formats/amiga/uft_amiga_protection.c` | 766 | „C99 **port** of XCopy Pro (1989-2011) 68000 Assembly algorithms", `:47` „Port of `ROL.L #1,D0`" — **keine Lizenz genannt** | **keine** — 0 Produktions-Aufrufer, 1 Test | 2 (§4: unklar ⇒ wie portiert) | fehlt, Beschaffung: XCopy-Verhaltensdoku |

**Audit-Stand aller vier: portiert** — bei den drei AIR-Dateien ohne
jedes Ähnlichkeitsaudit, weil sie sich **selbst** als „Full port"
erklären und die Lizenz im eigenen Kopf nennen. Bei XCopy Pro nach §4
„unklar ⇒ wie portiert".

**MF:** Audit **MF-638**, Vollzug offen (`LIZ-2`).

> **Warum nicht längst vollzogen.** Zwei der drei AIR-Dateien und die
> XCopy-Datei sind Waisen — dort kostet die Quarantäne nichts, und §1
> Vorrangregel würde sogar die einfachere Löschung erlauben. Nur
> `uft_ipf_air.c` kostet **IPF-Lesen**, und §6 sagt dazu klar: der
> Fähigkeitsverlust ist kein Argument gegen die Quarantäne. Die
> Entscheidung ist trotzdem die des Eigentümers, weil sie eine
> beworbene Fähigkeit zurücknimmt — Schritt 3 des Verfahrens
> („IPF: erkannt, nicht gelesen").

---

## Was **nicht** auf dieser Liste steht, und warum

§1 trennt Quarantäne von normaler Arbeit. Drei Fälle aus derselben
Woche, die trotz Löschung **keine** Quarantänefälle sind:

| Gelöscht | Warum keine Quarantäne |
|---|---|
| `src/flux/fdc_bitstream/` (12 Dateien + 14 Header, 6483 Z.) | **MIT** mit `LICENSE.md` und Autorennennung — Zone GRÜN, keine Herkunftsfrage. Reiner Verwaisten-Fall (MF-626). Der Rückweg ist trotzdem benannt: **externes Oracle**, siehe `ORACLES.md` |
| `src/formats/c64/uft_nib_format.c` (+ Header + Test) | Waise ohne Herkunftsfrage — 27 exportierte Funktionen, **0** Produktions-Aufrufer. Verwaisten-Regel (MF-635) |
| `src/formats/uft_format_registry_v2.c` (587 Z.) | Waise; Inhalt als Dokument gesichert (`FORMAT_CATALOG.md`), MF-624 |

**Die Zuschreibung wurde trotzdem notiert** (§1 Anmerkung): `fdc_bitstream`
= yas-sim, MIT. Damit ist die Frage „gibt es Geschwister derselben
Quelle?" später beantwortbar, ohne die Historie zu durchsuchen.

---

## Rehabilitiert (Weg 1) — zur Vollständigkeit

Zwei Dateien standen unter demselben Verdacht und wurden **freigesprochen**.
Sie waren nie in Quarantäne, gehören aber ins Protokoll, weil das Audit
dieselbe Methode benutzte:

| Datei | Verdacht | Audit | Folge |
|---|---|---|---|
| `src/formats/c64/uft_gcr_ops.c` | „Based on nibtools gcr.c" | **eigenständig** — `strip_runs`, `kill_partial_sync`, `reduce_runs` je anderer Algorithmus, andere Parameter, andere Speicherstrategie; ein Echo, und das ist der Fachbegriff „header checksum" | Kopf berichtigt (MF-635): „Verhalten nach der nibtools-Dokumentation und -Quelle — EIGENSTÄNDIGE Implementierung, kein Port" |
| `src/formats/c64/uft_d64_g64.c` | „Based on nibtools" | **eigenständig** — keine Entsprechung zu `convert_GCR_sector`, keine der auffälligen Konstanten (`0x07`, `0x4b` „Original Format Pattern"), keine Tri-Bit-/Low-Frequency-Notizen | Kopf berichtigt (MF-635) |

Und der Entlastungsbefund, der die Methode bestätigt: `src/core/uft_interleave.c`
und `src/core/uft_write_precomp.c` erklären sich als „Port of a8rawconv
0.95 … **(GPL-2-or-later)**". Nachgeprüft am Original (MF-643): die
Angabe **stimmt**, GPL-Kopf wörtlich in `interleave.cpp:1-16` und
`compensation.cpp:1-16`. Zone GRÜN, kein Handlungsbedarf.

---

## Offene Vorbedingung

`LIZ-1` — **48** Attributionen nennen eine fremde Codebasis ohne Lizenz
(`scripts/audit_spdx_policy.py`, Attributionsstufe). Solange die nicht
geklärt sind, ist jede Aussage über die Gesamtlage des Baums vorläufig,
und die Lizenzentscheidung in `LIZ-2` steht auf unvollständigem Grund.
