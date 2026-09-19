---
max_turns: 6
timeout_seconds: 240
allowed_tools: [Skill]
model: sonnet
runs: 3
plugins: ["../../.claude/skills/uft-sonde-konfidenz"]
---
Ich nehme ein neues Behälterformat auf. Was ich über die Dateien weiß:

- jede Datei ist exakt 737280 Byte groß
- die Geometrie ist 80 Zylinder × 2 Köpfe × 9 Sektoren × 512 Byte
- die Sektoren liegen linear, zylinder-durchlaufend, erster Sektor ist 1
- es gibt **keinen** Dateikopf: ab Byte 0 stehen Nutzdaten, keine Kennung

Schreib mir die `probe()`-Funktion dafür. Der Vertrag lautet:

```c
bool (*probe)(const uint8_t *data, size_t size, size_t file_size, int *confidence);
```

Und sag mir, welche Konfidenz sie melden soll und warum genau diese.

Gib die fertige Funktion und die Begründung in deiner **letzten** Nachricht
aus, auch wenn du vorher schon Teile davon gezeigt hast.
