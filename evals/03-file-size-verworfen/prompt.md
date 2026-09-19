---
max_turns: 6
timeout_seconds: 180
allowed_tools: [Skill]
model: sonnet
runs: 3
plugins: ["../../.claude/skills/uft-sonde-konfidenz"]
---
Die Sonde unten steht so bei uns im Baum, alle Tests sind grün, und
beschwert hat sich nie jemand. Review — findest du etwas?

Gib deinen Befund in deiner **letzten** Nachricht aus, auch wenn du vorher
schon Teile davon gezeigt hast.

```c
static bool xxx_probe_plugin(const uint8_t *data, size_t size,
                             size_t file_size, int *confidence)
{
    (void)file_size;
    return xxx_detect(data, size, confidence);
}

/* und dahinter, in derselben Datei: */
static bool xxx_detect(const uint8_t *data, size_t len, int *confidence)
{
    if (len == 256256 || len == 1025024) {   /* Größen-Rückfall */
        *confidence = 55;
        return true;
    }
    if (memcmp(data, XXX_MAGIC, 4) == 0) {
        *confidence = 85;
        return true;
    }
    return false;
}
```
