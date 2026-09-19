---
max_turns: 6
timeout_seconds: 180
allowed_tools: [Skill]
model: sonnet
runs: 3
plugins: ["../../.claude/skills/uft-sonde-konfidenz"]
---
Diese Sonde steht so bei uns im Baum. Der Kopfkommentar der Datei sagt
selbst, dass das Format keine Kennung trägt.

```c
/* FDI (PC-98) — Behälterformat mit 4096-Byte-Kopf.
 * Der Kopf trägt acht 32-Bit-Felder und KEINE Kennung;
 * die restlichen 4064 Byte sind Null.                     */
static bool fdi_probe_plugin(const uint8_t *data, size_t size,
                             size_t file_size, int *confidence)
{
    if (!data || size < 512 || !confidence) return false;

    uint32_t hdr_size  = rd32le(data + 0x08);
    uint32_t sect_size = rd32le(data + 0x10);
    uint32_t spt       = rd32le(data + 0x14);
    uint32_t heads     = rd32le(data + 0x18);
    uint32_t cyls      = rd32le(data + 0x1C);
    uint32_t fddsize   = rd32le(data + 0x20);

    if (hdr_size != 4096) return false;
    if (cyls == 0 || heads == 0 || spt == 0 || sect_size == 0) return false;
    if (fddsize != (uint64_t)cyls * heads * spt * sect_size) return false;
    if (file_size != (uint64_t)hdr_size + fddsize) return false;

    *confidence = 90;
    return true;
}
```

Ist die 90 gedeckt? Wenn nicht: welche Zahl, und warum genau die?

Gib dein Urteil und die Begründung in deiner **letzten** Nachricht aus, auch
wenn du vorher schon Teile davon gezeigt hast.
