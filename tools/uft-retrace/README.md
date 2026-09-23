# UFT ReTrace – reproduzierbarer Entpack- und Tracing-Workflow

UFT ReTrace macht aus historischen Amiga-Kopierprogrammen **prüfbare
Beobachtungen**. Es übersetzt kein lineares Pseudo-Disassembly blind nach C.

Der Workflow erzeugt:

- SHA-256-Inventar aller Eingaben;
- sichere ZIP-Extraktion ohne Pfadtraversal;
- Amiga-Hunk-Segmentextraktion einschließlich Relocations und Symbolen;
- statistische Hinweise auf gepackte oder komprimierte Segmente;
- Trefferlisten für Amiga-Diskettenregister, ausdrücklich nur als Hinweise;
- Differenzbereiche aus Speicherabbildern vor/nach dem Entpacken;
- normalisierte JSONL-Traces;
- Zusammenfassung von MMIO-Zugriffen und PC-Bereichen;
- reproduzierbares Provenienzmanifest;
- Markdown-Bericht und Clean-Room-Übergabe an UFT.

Historische Programme werden **nicht mitgeliefert**. Die Lizenz dieses
Workflows gilt nicht automatisch für analysierte Binärprogramme.

## Voraussetzungen

- Python 3.10 oder neuer
- keine externen Python-Pakete erforderlich
- optional WinUAE, FS-UAE oder ein Musashi-basierter 68000-Harness

## Schnellstart

Linux/macOS:

```sh
python3 -m uft_retrace inventory /pfad/disk_algorithms.zip -o work
python3 -m uft_retrace analyze work -o work/analysis.json
python3 -m uft_retrace report work -o work/REPORT.md
```

Windows PowerShell:

```powershell
py -3 -m uft_retrace inventory C:\Analyse\disk_algorithms.zip -o work
py -3 -m uft_retrace analyze work -o work\analysis.json
py -3 -m uft_retrace report work -o work\REPORT.md
```

## Laufzeitentpackung

1. Originalprogramm im Emulator laden.
2. Snapshot unmittelbar vor dem Entpacker sichern.
3. Bis zum Sprung in den neuen Speicherbereich laufen.
4. zweiten Snapshot sichern.
5. MMIO-/PC-Trace als JSONL oder vereinfachtes CSV exportieren.

```sh
python3 -m uft_retrace snapshot-diff before.bin after.bin \
  --base 0x000000 --min-run 32 -o work/snapshot_diff.json

python3 -m uft_retrace trace emulator_trace.jsonl \
  -o work/trace_summary.json
```

Der genaue Ablauf für Emulatoren steht unter `emulator/`.

## Komplettlauf

```sh
./run_pipeline.sh /pfad/disk_algorithms.zip work
```

```powershell
.\run_pipeline.ps1 C:\Analyse\disk_algorithms.zip work
```

## Traceformat

Eine Zeile ist ein JSON-Objekt:

```json
{"seq":1,"cycle":120,"pc":4096,"type":"mmio_write","address":14676004,"size":2,"value":32768}
```

Zahlen dürfen dezimal oder als Zeichenkette wie `"0x00dff024"` vorliegen.
Pflichtfelder sind `pc` und `type`. Einzelheiten stehen in
`schemas/trace.schema.json`.

## Tests

```sh
python3 -m unittest discover -s tests -v
```

Die Tests erzeugen synthetische Hunk-Dateien und Traces. Sie enthalten keinen
historischen Programmcode.

## Was als Ergebnis zählt

Ein Registertreffer in Rohbytes ist **kein Algorithmus**. Belastbar wird ein
Befund erst, wenn:

1. der ausgeführte PC im Trace liegt;
2. Eingabe und Ausgabe dokumentiert sind;
3. das Verhalten wiederholbar ist;
4. eine Gegenprobe die Behauptung zum Fallen bringt;
5. die UFT-Neuimplementierung nur das gemessene Verhalten übernimmt.
