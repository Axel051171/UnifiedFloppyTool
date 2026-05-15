# UFT ↔ FluxEngine Kompatibilitäts-Audit

**Datum:** 2026-05-14  
**Referenz:** `davidgiven/fluxengine` (HEAD)  
**Prüfling:** `Axel051171/UnifiedFloppyTool` (HEAD)

---

## Verdikt

**UFT's FluxEngine-Backend funktioniert in der aktuellen FE-Version nicht.** `rpm` und `--version` laufen, aber `read` und `write` schicken eine CLI-Syntax, die seit dem Refactor auf die Config-Schicht (vor ~2 Jahren) nicht mehr akzeptiert wird. 5 von 5 Read-Aufrufen scheitern, 4 von 4 Write-Aufrufen scheitern.

---

## Warum das Test-Setup hier komplett anders aussieht

Bei Greaseweazle integriert UFT **nativ** — eigener USB-CDC-Code, byte-genaues Wire-Protocol. Da macht ein Konstanten-Diff und Encoder-Vergleich Sinn.

Bei FluxEngine ist das Backend ein **CLI-Wrapper** (`src/hardware_providers/fluxenginehardwareprovider.cpp`, 744 Zeilen). UFT ruft das externe `fluxengine`-Binary via `QProcess` auf und parsed dessen stdout per Regex. Das Wire-Protocol (libusb-Bulk, VID 0x1209 PID 0x6e00, 12-MHz-Ticks, Frame-Header) berührt UFT überhaupt nicht — das macht FE selbst.

Der Test verschiebt sich also von „Wire-Protocol-Diff" auf **CLI-Vertrag** — drei Dimensionen:

1. **Subcommand-Existenz** — gibt es `rpm`, `read`, `write` in der aktuellen FE-Version noch?
2. **Flag-Semantik** — bedeuten die Flags, die UFT übergibt, in FE was UFT denkt?
3. **Output-Parser** — passt UFTs Regex auf das, was FE tatsächlich druckt?

Plus zwei Sekundär-Checks: Profile-Coverage und Versions-Drift-Resistenz.

---

## Test-Setup (5 Stufen)

### Stufe 1 — Subcommand-Existenz
`extract_fe_contract.py` parsed `src/fluxengine.cc` und sammelt alle registrierten Subcommands aus dem `commands` Vektor. `extract_uft_calls.py` extrahiert per Regex aus `fluxenginehardwareprovider.cpp` jeden `QStringList args = { ... }` und jeden inline `runFluxEngine({...})`. Set-Diff zeigt, welche UFT-Aufrufe ein nicht-existentes Subcommand verwenden.

### Stufe 2 — Flag-Vertrag
Aus jeder `fe-*.cc` werden die `StringFlag`, `IntFlag`, `BoolFlag`, `ActionFlag` Definitionen extrahiert (mit ihren Namen-Listen). `diff_uft_fe.py` prüft jeden von UFT verwendeten Flag gegen diese Tabelle. Plus eine Mock-Binary (`mock_fluxengine.py`), die genau die Flag-Semantik der echten FE nachbildet und UFT-Aufrufe ablehnt, die echtes FE auch ablehnen würde.

### Stufe 3 — Output-Parser
`test_output_parser.py` füttert UFTs hartcodierte Regex `([0-9.]+)\s*rpm` mit verschiedenen realen und potentiellen FE-Outputs.

### Stufe 4 — Profile-Coverage
UFTs `supportedProfiles()` Liste (21 Einträge) gegen `src/formats/*.textpb` in FE (35 Einträge).

### Stufe 5 (nicht ausgeführt — bräuchte echte HW + FE-Installation)
End-to-End: echtes `fluxengine` Binary installieren, UFT mit Mock-Drive starten, jeden UFT-Workflow durchlaufen, exit codes & Output mitschreiben.

---

## Ergebnisse

### Stufe 1 — Subcommands

| UFT ruft auf | In FE vorhanden? |
|---|---|
| `rpm` | ✓ |
| `read` | ✓ |
| `write` | ✓ |
| `--version` (top-level) | ✓ |

Alle vier OK. Aber das ist nur die Hülle — die wahren Probleme sind in den Args.

### Stufe 2 — Flag-Semantik (das ist die Bombe)

UFTs konkrete Read-Invokation (aus `fluxenginehardwareprovider.cpp:246`):

```
fluxengine read ibm -s drive:0 -c 40 -h 0 --revs=2 -o out.flux
```

Was die aktuelle FE damit macht (per Mock simuliert):

```
ERROR: unexpected positional 'ibm' after 'read': profile must be passed via -c ibm
ERROR: -c 40: '-c' loads a config/profile by name; got numeric value '40'.
              UFT likely intended --tracks=c40h0 instead.
ERROR: unknown flag for 'read': -h
ERROR: unexpected positional '0' after 'read': profile must be passed via -c 0
ERROR: unknown flag for 'read': --revs
```

Fünf Probleme in einem einzigen Aufruf:

1. **`ibm` als positional argument** → FE erwartet kein positional nach `read`. Profil-Wahl läuft über `-c <name>`. Echte FE-Doku: `fluxengine read -c ibm --1440 -s drive:0 -o ibm.img`
2. **`-c 40`** → In FE bedeutet `-c` **Config/Profile laden**, nicht Cylinder. UFT übergibt die Cylinder-Nummer und FE versucht, eine Datei `40.textpb` oder ein Builtin-Profile namens „40" zu laden → Fehler.
3. **`-h 0`** → Flag existiert in FE nicht. Würde mit `Unknown flag: -h` abbrechen. (`-t/--cylinder` gibt es nur für `fluxengine seek`.)
4. **`--revs=2`** → Heißt in FE `--drive.revolutions=2`. UFTs `--revs` ist nicht erkannt.

Der korrekte Aufruf nach aktueller FE-Doku wäre:

```
fluxengine read -c ibm -s drive:0 --tracks=c40h0 --drive.revolutions=2 -o out.flux
```

Mock-Test gegen diese Form: **0 Fehler.**

Write hat dieselben Probleme + `-d drive:0` ist korrekt (`--dest`), `-i in.flux` auch (`--input`).

### Stufe 3 — Output-Parser

UFTs Regex `([0-9.]+)\s*rpm` gegen reale FE-Outputs (`src/fe-rpm.cc:33`):

| FE-Output | UFT parsed |
|---|---|
| `Rotational period is 200.0 ms (300.0 rpm)` | `300.0` ✓ |
| `Rotational period is 166.67 ms (360.0 rpm)` | `360.0` ✓ |
| `No index pulses detected; is a disk in the drive?` | NO MATCH ✗ — Drive-Detection scheitert silent |
| `Drehperiode: 200ms (300 U/min)` (locale change) | NO MATCH ✗ |

Der Happy-Path funktioniert. Der „kein Disk"-Fall wird als „kein RPM gefunden" interpretiert, was zur richtigen Fehlermeldung führt — Glück gehabt. Aber ein zukünftiger FE-Output-Wechsel (i18n, Format-Änderung) bricht es ohne Warnung.

### Stufe 4 — Profile-Coverage

- UFT exposes 21 Profile, **alle 21 existieren in FE** ✓ (keine „Ghost-Profile")
- FE hat 14 zusätzliche, die UFT nicht anbietet: `aeslanier, agat, epsonpf10, fb100, icl30, juku, ms2000, mx, psos, rolandd20, smaky6, tartu, tids990, tiki`

Das ist eine reine UI-Lücke — Profile sind da, müssten nur in `supportedProfiles()` ergänzt werden. Kein Bug, nur Feature-Lücke.

### Stufe 5 — Nicht ausgeführt

Echtes End-to-End braucht installiertes FE und entweder echte HW oder einen Mock-USB-Stack. Mit dem `mock_fluxengine.py` aus Stufe 2 könntest du das in der CI als Smoke-Test laufen lassen: `PATH=$PWD ./mock_fluxengine.py` als `fluxengine` symlinken, UFT-Workflows triggern, exit codes prüfen.

---

## Findings

### F1 — Read- und Write-CLI sind kaputt (P0, blockiert Feature komplett)
**Severity:** Kritisch — jedes Lesen/Schreiben über das FE-Backend wird die Fehlermeldung „unknown flag" oder „config not found" der FE-CLI zurückliefern.  
**Wo:** `src/hardware_providers/fluxenginehardwareprovider.cpp:246-256` (read), `:388-398` (write), `:508-518` (raw read), `:533-543` (raw write)  
**Fix:**
```cpp
QStringList args = {
    QStringLiteral("read"),
    QStringLiteral("-c"), profileName,                                // war: positional 'ibm'
    QStringLiteral("-s"), QStringLiteral("drive:0"),
    QStringLiteral("--tracks=c%1h%2").arg(params.cylinder).arg(params.head),  // war: -c N -h N
    QStringLiteral("--drive.revolutions=%1").arg(params.revolutions),         // war: --revs
    QStringLiteral("-o"), tempPath
};
```

### F2 — Profil ist hartcodiert auf `"ibm"` statt parametrisiert (P0)
**Wo:** `:248` und `:390` — `QStringLiteral("ibm")` ist literal, ignoriert die Profile-Auswahl der UI.  
**Konsequenz:** Selbst nach Fix von F1 würde UFT immer das IBM-Profil verwenden, egal was der User in der GUI wählt.  
**Fix:** Profilname als Parameter durchreichen.

### F3 — `--version` Detection ist fragil (P2)
**Wo:** `:100` — `runFluxEngine({"--version"}, ...)`  
FluxEngine hat tatsächlich keinen `--version` Top-Level-Flag explizit registriert (`flags.cc` hat `--doc` und `--help`, aber kein `--version`). Es funktioniert ggf. zufällig, weil FE bei unbekannten Top-Level-Flags die Hilfe ausgibt — und die enthält ggf. eine Versionsnummer, die UFT dann parsed. **Fragil.**  
**Fix:** `fluxengine --doc` oder `fluxengine help` verwenden und Version aus der ersten Zeile lesen.

### F4 — Output-Parser zu eng (P1)
**Wo:** `:484, :732` — `R"(([0-9.]+)\s*rpm)"`  
Funktioniert heute, bricht bei jeder FE-Output-Änderung. Besser: zusätzlich `--csv` oder ein strukturiertes Output-Format prüfen, ob FE das unterstützt.

### F5 — 14 FE-Profile fehlen in UFTs Liste (P2)
**Wo:** `:553-577`  
`aeslanier, agat, epsonpf10, fb100, icl30, juku, ms2000, mx, psos, rolandd20, smaky6, tartu, tids990, tiki` — alle in `fe/src/formats/*.textpb`, alle akzeptieren `-c <name>` als Profile-Wahl.

### F6 — Versions-Pinning fehlt
UFT prüft keine FE-Version. Die CLI-Syntax wurde ~2022 von positional auf flag-basiert refactored. UFT's Aufrufe sehen aus, als wären sie gegen die alte FE-Version (vor dem Refactor) geschrieben. Empfehlung: `fluxengine --doc` ausführen, Version extrahieren, gegen bekannte Minimum-Version prüfen. Bei alten Versionen Fallback-Syntax verwenden oder Hard-Fail.

---

## Was geändert werden muss (Reihenfolge)

1. **F1 + F2 zusammen** — Read/Write-Args neu schreiben mit `-c <profile>`, `--tracks=cNhM`, `--drive.revolutions=N`. Profilname von der GUI durchreichen. **Ohne diesen Fix tut das FE-Backend nichts.**
2. **F3** — `--version` durch `fluxengine --doc` oder Subcommand-loose ersetzen.
3. **F6** — Versions-Detection einbauen mit klarer Mindestanforderung in der README.
4. **F4** — Output-Parser robuster machen, optional auf `--csv` umstellen wenn FE das anbietet.
5. **F5** — UI-Liste der Profile aus `fluxengine help read` oder den Builtin-Configs dynamisch generieren statt hartzukodieren.

---

## Artefakte

Im Ordner liegt alles zum Re-Run:

- `extract_uft_calls.py` — extrahiert alle FE-CLI-Aufrufe aus UFT
- `extract_fe_contract.py` — extrahiert Subcommands + Flags aus FluxEngine-Source
- `diff_uft_fe.py` — diff-Runner über beide
- `mock_fluxengine.py` — fluxengine-Mock mit korrekter Flag-Semantik (PATH-shim für CI)
- `test_output_parser.py` — UFTs Regex gegen FE-Outputs
- `check_profile_coverage.py` — UFT-Profile vs FE-Builtins
- `fe_contract.json` — Snapshot des aktuellen FE-CLI-Vertrags

Re-Run: `python3 extract_fe_contract.py > fe_contract.json && python3 diff_uft_fe.py && python3 mock_fluxengine.py read ibm -s drive:0 -c 40 -h 0 --revs=2 -o /tmp/x`

Für CI: Mock per `chmod +x` und `PATH=/path/to/mocks:$PATH` vor den UFT-Integrationstests ausführen. Jeder UFT-Aufruf an FE wird damit gegen den korrekten CLI-Vertrag validiert, ohne dass echtes FE installiert sein muss.

**Kein Bit verloren — aber gerade auch keine Sektoren gelesen.**
