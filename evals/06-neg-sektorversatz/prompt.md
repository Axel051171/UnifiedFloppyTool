---
max_turns: 6
timeout_seconds: 180
allowed_tools: [Skill]
model: sonnet
runs: 3
plugins: ["../../.claude/skills/uft-sonde-konfidenz"]
---
In `read_track()` kommt bei Zylinder 40 Müll: statt Spur 40 liefert es
Bytes, die zu Spur 20 auf Kopf 1 gehören. Die Datei ist 204800 Byte groß,
das Plugin rechnet mit 40 Zylindern × 2 Köpfen × 10 Sektoren × 256 Byte.

Prüf die Versatzformel.
