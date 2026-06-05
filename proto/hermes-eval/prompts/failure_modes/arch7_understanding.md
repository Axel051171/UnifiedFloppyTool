# Failure-Mode 1 — ARCH-7 Teensy VID/PID Disambiguation

KO-Test: kann hermes-agent das Problem domänenkonform erklären?

## Prompt (verbatim)

```
You are reviewing a floppy-disk-imaging tool's hardware-provider
architecture. The tool supports two USB devices that both happen to
use Teensy-based microcontrollers, and both ship with the standard
Teensy USB Vendor ID 0x16C0 and Product ID 0x0483:

  - ADFCopy   (Amiga-floppy-to-host copier)
  - Applesauce (Apple-II-disk archiver)

Explain in 3-5 sentences:

1. What is the concrete forensic risk of connecting an ADFCopy device
   when the host software thinks it is talking to Applesauce (or vice
   versa)?

2. Why is USB descriptor enumeration alone (libusb_get_device_descriptor)
   not sufficient to disambiguate these two devices?

3. What kind of disambiguation MUST be done before sending the first
   command to the device, in a forensic context?

Be concrete. Use technical floppy-imaging language. Do not search the
codebase — answer from domain knowledge.
```

## Erwartete Qualitäts-Indikatoren

Eine gute Antwort enthält mind. 4 von diesen 6 Stichpunkten:

- **Disk-Korruption**: ein Tool das falsche Schreibsequenzen an das
  falsche Drive sendet kann reale Disketten unrettbar überschreiben
- **Magnetische Track-Geometrie unterschiedlich**: Amiga DD (80 cyl,
  2 sides) ≠ Apple-II GCR (35 cyl, 1 side); falsche Stepper-Sequenz
  kann den Drive-Kopf außerhalb des Disk-Surface positionieren
- **Encoding-Mismatch**: Amiga MFM ≠ Apple GCR — ein Bit-Stream der
  als MFM gesendet wird in einem Apple-Drive ist nicht nur falsch
  decoded, er ist physisch ein anderes Magnetfeld-Muster
- **VID/PID nicht eindeutig**: Teensy ist ein generic-USB-MCU; die
  Anwender-Firmware definiert das Geräteverhalten, NICHT die USB-
  Descriptoren
- **Disambiguation via Probe-Handshake**: ein nicht-destruktiver
  Identify-Command (z.B. `?vers` über serielle Verbindung) MUSS vor
  dem ersten echten Hardware-Command laufen
- **Forensische Konsequenz**: wenn die Disambiguation fehlt und
  silent-fail-fallback eingebaut ist (falscher Treiber-Pfad gewählt
  ohne User-Warnung), ist das Tool als Forensik-Tool disqualifiziert

## Scoring

- 5/5: Alle 6 Punkte oder mind. 5 plus eigene zusätzliche Erkenntnis
- 4/5: 4-5 Punkte
- 3/5: 3 Punkte
- 2/5: 2 Punkte ODER vage allgemeine "USB IDs collisions are bad"
  ohne floppy-spezifischen Bezug
- 1/5: 1 Punkt oder vom Thema ablenkende Antwort
- 0/5: völlig generische LLM-Antwort ("ensure proper device
  identification") ohne floppy/Magnet/Geometrie/Encoding-Inhalt →
  **DISQUALIFIZIERT die gesamte hermes-Evaluation**

## hermes Invocation

```bash
time hermes -z "$(extract_prompt_block prompts/failure_modes/arch7_understanding.md)" \
  --provider openrouter \
  --model anthropic/claude-sonnet-4.6 \
  --toolsets web \
  --max-turns 5 \
  > results/raw_logs/fm_arch7_hermes.txt
```

`--toolsets web` weil das Modell ggf. nach Teensy-Specs sucht — aber
soll NICHT das UFT-Repo durchsuchen (deshalb kein `terminal`).

## Bewertung

In `results/RESULTS.md` als Tabellen-Zeile:

| Tool | Score | Wall-Clock | Token-Cost | Notes |
|---|---|---|---|---|
| Claude Code (no skill) | x/5 | xs | $x | ... |
| hermes-agent | x/5 | xs | $x | ... |
