# Musashi-Harness – empfohlene Automatisierung

Für wiederholbare Unit-Experimente ist ein kleiner 68000-Harness langfristig
besser als manuelle Debuggersitzungen.

## Benötigte Hooks

```c
uint8_t  read8(uint32_t address);
uint16_t read16(uint32_t address);
uint32_t read32(uint32_t address);
void write8(uint32_t address, uint8_t value);
void write16(uint32_t address, uint16_t value);
void write32(uint32_t address, uint32_t value);
void instruction_hook(uint32_t pc);
```

Die Hooks schreiben JSONL-Ereignisse. Für MMIO müssen sie keine vollständige
Amiga-Hardware simulieren, solange ein Versuch nur die aufgerufene
Entscheidungsroutine untersucht. Ein Raw-DMA-Test braucht dagegen ein
definiertes DSKLEN/DSKBYTR-Zustandsmodell.

## Determinismus

- Speicher vor jedem Lauf auf denselben Wert setzen;
- Registersatz fest vorgeben;
- maximale Instruktionszahl begrenzen;
- alle nicht modellierten MMIO-Zugriffe als Fehler behandeln;
- Eingabe- und Ausgabe-Speicherbereiche hashen;
- Hänger als Ergebnis dokumentieren, nicht durch geratenen Rückgabewert lösen.

## Nicht tun

- den ganzen Amiga improvisiert nachbauen;
- unbekannte OS-Aufrufe mit Erfolg beantworten;
- nicht initialisierten Speicher zufällig füllen;
- den Harness als unabhängiges Oracle bezeichnen, wenn sein Verhalten aus der
  untersuchten Routine abgeleitet wurde.
