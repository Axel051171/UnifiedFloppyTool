# Community-Bench-Protokolle (Tier-3 Hardware-Verifikation)

Stand 2026-08-16 (MF-366). Das Projekt besitzt **keine physische Hardware**
(MF-310); Tier-3-Verifikation ist community-delegiert. Dieses Dokument macht
die Delegation real statt nominell: pro Controller ein konkretes, von jedem
Besitzer der Hardware ausführbares Protokoll. Ergebnisse bitte als GitHub-Issue
mit Label `hw-bench` melden (Log-Ausgaben + erzeugte Dateien anhängen).

**Sicherheits-Grundsatz:** Alle Lese-Benches sind zerstörungsfrei. Für
Schreib-Benches NUR wertlose Disketten verwenden — niemals Originale.

## Allgemeiner Ablauf (jeder Bench)

1. UFT-Release oder Eigen-Build starten, Hardware-Tab öffnen, Controller
   auswählen; UFT-Version + Firmware-Version + OS notieren.
2. Eine **bekannte** Diskette lesen (ideal: eine, deren Inhalt über ein
   Referenz-Tool bereits gedumpt wurde — Vergleichbarkeit!).
3. UFT-Ausgabe gegen den Referenz-Dump vergleichen (SHA-256 der Sektor-Ebene;
   bei Flux: Format-Konvertierung → Sektor-Ebene → Vergleich).
4. Log + beide Hashes + Abweichungen ins Issue.

**GO-Kriterium** pro Controller: byte-identische Sektor-Daten gegenüber dem
Referenz-Tool auf mindestens einer DD- und einer HD-Diskette (wo unterstützt),
ohne Fehler im UFT-Log.

## Pro Controller

### Greaseweazle (production-wired — formaler Bench-Pass ausstehend, HIL.GW)
- Referenz-Tool: `gw read --format=<fmt> ref.scp` (offizielle gw-Tools)
- UFT: gleiche Disk lesen → SCP speichern → `SCP → IMG/ADF/D64` konvertieren;
  gleiche Konvertierung mit dem gw-Ökosystem; Sektor-Hashes vergleichen.
- Zusätzlich: 3+ Revolutionen einer kopiergeschützten Disk lesen — UFT darf
  Multi-Rev/Weak-Fenster nicht kollabieren (Loss-Report prüfen).

### SuperCard Pro (SCP-Direct, M3.1 — UFT-008)
- Referenz-Tool: samdisk (`samdisk copy scp:` …) oder SCP-eigene Software.
- UFT: Read-Flux derselben Disk; die 22 USB-Opcodes sind gegen samdisk
  byte-verifiziert — der Bench prüft das Timing-/FIFO-Verhalten echter HW:
  (a) vollständige Revs ohne Overflow, (b) Index-Semantik (Rev-Grenzen),
  (c) 0x0000-Overflow-Akkumulator auf langen Zellen (HD-Disk innen).
- Write bleibt gesperrt (`NOT_IMPLEMENTED`) bis der Read-Pfad GO hat.

### KryoFlux (Subprocess-Wrapper um DTC)
- Voraussetzung: funktionierendes DTC-Setup des Besitzers.
- UFT: Read via UFT-GUI → Stream/SCP; Referenz: dieselbe Disk direkt mit DTC.
- Prüfen: identische Track-Anzahl, Index-Zählung, dekodierte Sektor-Hashes.

### FC5025 (Subprocess-Wrapper, read-only 5.25")
- Referenz: fcimage direkt. UFT: gleiche Disk via GUI. Sektor-Hash-Vergleich.

### XUM1541/ZoomFloppy, Applesauce, FluxEngine, ADF-Copy, USB-Floppy (UFI)
- **Noch nicht bench-fähig** — I/O-Wiring offen (M3.x, honest-stubs). Ein
  Bench-Protokoll folgt mit dem jeweiligen Wiring; Besitzer dieser Hardware,
  die beim Wiring-Test helfen wollen: bitte Issue mit Label `hw-wiring`.

## Meldeformat (Issue)

```
Controller/Firmware:   …
UFT-Version/OS:        …
Disk (Typ, Format):    …
Referenz-Tool+Version: …
Sektor-SHA256 UFT:     …
Sektor-SHA256 Referenz:…
Ergebnis:              identisch / Abweichung (Details)
Logs:                  (Anhang)
```
