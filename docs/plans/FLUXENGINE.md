# Umsetzungsplan FluxEngine — Baum-Fassung

> **Verdaut, nicht kopiert.** Diese Fassung trägt nur die **offenen**
> Bausteine. Erledigtes und Übernommenes steht als Verweis dabei, damit
> niemand es zweimal baut. Nach MF-641 Nachtrag 3; Vorlage war ein
> Chat-Artefakt vom 2026-08-22, gegen HEAD `e97888fc` neu vermessen.

**Quelle der Anregung:** [davidgiven/fluxengine](https://github.com/davidgiven/fluxengine)
(vermessen: HEAD `909fac7`, 2026-06). Lizenz laut `COPYING.md`:
**GPL-2.0 (Aggregat)** — Zone GRÜN: **Code portierbar** mit
Attribution nach samdisk-Muster (Quelle + Commit im Header).

> ### Nachtrag zur Lizenz (MF-643) — GRÜN, aber mit Preis
>
> `COPYING.md` bei der Prüfung am Original nachgelesen. Der Text sagt
> ausdrücklich:
>
> > „The FluxEngine code as a whole is GPL 2.0-licensed …
> > **FluxEngine is GPL 2.0, not GPL 2.0-or-later.**"
>
> Unser Baum steht seit MF-621 auf `GPL-2.0-**or-later**`. Ein Port ist
> damit zulässig — er **verengt** aber die Lizenz des kombinierten Werks
> auf **GPL-2.0-only** und schließt jeden späteren Wechsel aus.
>
> Das ist keine Absage, sondern eine Bedingung, die vor dem ersten Port
> auf dem Tisch liegen muss:
>
> * **FE-2 und FE-4** (VCD-/AU-Export, fl2-Reader) sind echte
>   Code-Übernahmen — sie lösen die Verengung aus.
> * **FE-1, FE-3, FE-5, FE-6** brauchen keinen Port: Profildaten,
>   Synthetik-Quellen, Tür-Suche und Oracle-Betrieb kommen ohne fremden
>   Code aus.
>
> **Neuer Schnitt (MF-644) — die Verengung lässt sich ganz vermeiden.**
> FE-2 und FE-4 brauchen gar keinen Port, weil ihre Gegenstände
> **öffentlich spezifiziert** sind:
>
> * **VCD** ist im Verilog-Standard beschrieben (IEEE 1364, Abschnitt
>   Value Change Dump) — ein Textformat mit Zeitstempeln und
>   Signalwechseln.
> * **AU** ist der Sun-Audio-Header: sechs Big-Endian-Langworte, danach
>   Rohdaten. Trivial und dokumentiert.
> * **fl2** ist Protobuf; sein `.proto`-Schema **ist** die
>   Schnittstellenbeschreibung. Ein handgeschriebener Varint-Leser
>   gegen das Schema ist eine Implementierung nach Spec, kein Port.
>
> Aus Spec implementiert statt portiert, tritt die Verengung **nie**
> ein. Damit bleibt vom ganzen Plan nur **ein** Port-Zweig übrig: der
> `UFT_INCOMPLETE`-Fall in FE-6 — und der ist pro Format entscheidbar
> statt pauschal.
>
> **Gemessen (MF-644): die Tür ist offen.** Im Baum liegt heute kein
> GPL-2.0-only-Code — keine SPDX-only-Kennung, und der einverleibte
> Fremdcode ist `src/samdisk` (**MIT**, 147 Dateien) sowie
> `src/a8rawconv` (**GPL-2.0-or-later**, 46 Dateien).
>
> **Vorbehalt, der die Reihenfolge bestimmt:** die 48 Attributionen ohne
> genannte Lizenz aus `LIZ-1` sind ungeklärt, und HxCFloppyEmulator
> taucht dreimal auf. Ist eine davon ein Port von GPL-2.0-only-Code, war
> die Tür längst zu. **`LIZ-1` ist damit Vorbedingung jeder
> Lizenzentscheidung**, nicht ihr Parallelvorgang.
>
> `dep/` trägt abweichende Lizenzen, laut `COPYING.md` alle
> GPL-2.0-verträglich; das Aggregat bleibt GPL-2.0.

**Abgrenzung:** „FluxEngine" als *Hardware-Controller* ist ein anderes
Vorhaben — Provider und Runner stehen in `M3_HAL_PLAN.md`. Diese Datei
behandelt nur Software-Übernahmen aus dem FluxEngine-*Quellbaum*.

## Bereits geliefert oder anderswo verankert — nicht erneut einplanen

| Baustein (alte Zählung) | Stand |
|---|---|
| Medienprofil-Kern (§1 teilweise) | ✅ MF-471/475/483 — `src/flux/uft_media_profile.c`: Bitzellendauer aus Profil × gemessener Umdrehung. Die *Tabelle* ist schmal; Ausbau siehe FE-1 |
| Unified VFS (§3) | **übernommen von `PLAN_v4.1.7.md` Phase 1** — dort verankert, hier nur Verweis. Ein Baustein, ein Ort |
| Sektor-Cache, OS-Mount | verschoben nach `DISKFLASHBACK.md` (dort DF-1/DF-3) — FluxEngine war nie die beste Quelle dafür |
| Drive-Response (§4) | **zurückgestellt bis Tier-3-Schreibpfad** (MF-310, kein Gerät); destruktiv, nur Scratch-Disk. Kein aktiver Baustein |

## Offene Bausteine

### FE-1 — textpb-Profilernte *(S)* — Kennzahl: Profildaten MEASURED statt erfunden
`fe/src/formats/*.textpb` (37 Systeme) sind deklarative Formatphysik:
RPM, Zellendauern, Layouts, Varianten. Ernte-Skript
(`scripts/harvest_fluxengine_profiles.py`) erzeugt
`src/flux/uft_media_profiles_generated.c`, jede Zeile
`MEASURED_FROM_REFERENCE(fluxengine@<commit>)`. Eingecheckt + Hash-Tor;
Skriptlauf manuell, nie im Build. **Nebenbefund einplanen:**
Mixed-Format-Disks (CP/M, N88: erste Spuren FM, Rest MFM) verlangen
Profilwechsel je Spurbereich — Feld im Profil vorsehen, bevor die
Tabelle wächst.

### FE-2 — VCD- und AU-Fluxexport *(S)* — Kennzahl: keine direkt; Rettungskette/Diagnose
Port von `fe/lib/fluxsink/vcdfluxsink.cc` + `aufluxsink.cc` (klein,
Zone GRÜN) nach `src/flux/export/`. Kanäle: RDATA, Index, optional
dekodiertes Fenster → PulseView/sigrok, hörbarer Flux. Einhängung
ToolsTab + CLI. Verbindet UFT mit Logikanalysator-Workflow und dem
RP2350-Analyzer des Eigentümers; Gegenstück zum späteren Analog-Import
(`FLOPPYCONTROL.md`).

### FE-3 — Synthetik-Fluxquellen *(S–M)* — Kennzahl 3 halten; CI-Netz für Phase 1
`testpatternfluxsource.cc`/`erasefluxsource.cc` als Vorbild:
`src/flux/uft_flux_synth.c` mit Injektoren (definierter CRC-Fehler,
Jitter σ, Weak-Fenster, fehlender Index, Longtrack). Verwendung:
PLL-/Decoder-Regression, Positiv-/Negativkontrollen der
Multiread-Klassifikation, 288↔300-RPM-Paare. Teile existieren evtl.
als Testvektoren (MF-486 Jitter) — **Tür-Suche vor Neubau**
(MF-629-Muster), dann nur die Lücken.

### FE-4 — fl2-Reader *(S)* — Kennzahl 2: Wandlungspfade (Interop-Eingang)
FluxEngines Protobuf-Container lesen, ohne libprotobuf-Zwang:
handgeschriebener Varint-Parser (~200 Zeilen) gegen
`fe/lib/fluxsource/*.proto`. Read-only genügt. Rotbeweis: FE schreibt
fl2 → UFT liest → Sektoren identisch zu FEs eigener Dekodierung.

### FE-5 — A2R-Schreibpfad: Tür suchen *(S)* — Kennzahl 2
Gemessen: `src/samdisk/a2r.cpp` liegt vendored im Baum. **Nicht
gemessen:** ob ein Schreibweg erreichbar ist oder nur Lesen verdrahtet
wurde. Erst Erreichbarkeits-Messung (Aufruferkette), dann entweder
Verdrahtung mit Rotbeweis oder ehrliche Registry-Zeile. Kein Neubau,
bevor die Tür-Frage beantwortet ist.

### FE-6 — Exoten-Verifikation mit FE als Oracle *(S je Format)* — Kennzahl 1 direkt
FE-Binary in `ORACLES.md` registrieren (bauen, laufen lassen — kein
Oracle auf Zusicherung), dann je Exoten-Format (Agat, Smaky6, fb100,
Tartu, MX …) dreiwertig: `DECODES_EQUAL` / `UFT_INCOMPLETE` (→ Port,
Zone GRÜN erlaubt es) / `REGISTRY_ONLY` (→ ehrlich zurückstufen).
Reihenfolge nach T3-Liste; jede Entscheidung senkt Kennzahl 1 oder
macht die Formatliste wahrer — beides zählt.

## Bewusst nicht übernommen
wxWidgets-GUI/gui2 (UFT ist Qt) · FE-Buildsystem · FluxSource/-Sink
als Architektur (Provider V2 + Plugins leisten das; nur Inhalte
einzelner Module portieren) · FE-eigenes PSoC-Protokoll (Controller-
Thema, `M3_HAL_PLAN.md`).

## Reihenfolge-Empfehlung
FE-1 → FE-3 (füttern Phase 1 und CI) → FE-6 (senkt Kennzahl 1 laufend,
parallelisierbar) → FE-2/FE-4/FE-5 nach Gelegenheit. Kein Baustein
blockiert Phase 1; FE-1 und FE-3 beschleunigen sie.
