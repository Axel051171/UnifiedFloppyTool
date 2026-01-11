# ADR-0004: Score-basierte Format Auto-Detection

**Status:** Accepted  
**Datum:** 2025-01-11  
**Autor:** UFT Team

---

## Kontext

UFT unterstützt 50+ Disk-Image-Formate. Benutzer erwarten, dass das Tool
automatisch erkennt, welches Format eine Datei hat — auch ohne Extension.

Bisheriger Ansatz war eine lange `if/else`-Kette:
```c
if (has_d64_signature(data)) return FORMAT_D64;
else if (has_adf_signature(data)) return FORMAT_ADF;
else if (looks_like_fat(data)) return FORMAT_FAT;
// ... 50 weitere
```

Probleme:
- Reihenfolge-abhängig
- Keine Konfidenz-Information
- Schwer erweiterbar
- FAT-False-Positives bei anderen Formaten

## Entscheidung

**Wir implementieren eine Registry-basierte Detection mit Confidence-Scores.**

Jedes Format registriert eine `probe()`-Funktion, die einen Score (0-100) zurückgibt.
Die Detection wählt das Format mit dem höchsten Score.

```c
typedef struct {
    const char *name;
    const char *extensions;     // ".d64;.d71;.d81"
    int (*probe)(const uint8_t *data, size_t len);
    int priority;               // Tiebreaker bei gleichem Score
} uft_format_detector_t;
```

## Begründung

| Ansatz | Erweiterbar | Testbar | Konfidenz | Performant |
|--------|-------------|---------|-----------|------------|
| if/else Kette | ❌ | ⚠️ | ❌ | ✅ |
| **Score-Registry** | ✅ | ✅ | ✅ | ✅ |
| ML-Classifier | ⚠️ | ⚠️ | ✅ | ❌ |

### Score-Berechnung

```
Score = Basis-Score × (Priority / 100)

Beispiel:
- D64 probe() returns 95, priority 100 → Score = 95
- FAT probe() returns 60, priority 50  → Score = 30
→ D64 gewinnt
```

### Confidence-Level

| Score | Bedeutung |
|-------|-----------|
| 90-100 | Definitiv dieses Format |
| 70-89 | Sehr wahrscheinlich |
| 50-69 | Möglicherweise |
| 30-49 | Unwahrscheinlich |
| 0-29 | Nicht dieses Format |

## Konsequenzen

### Positiv ✅
- Neue Formate durch einfache Registrierung
- Testbare Probe-Funktionen
- Benutzer sieht Konfidenz
- Korrekte Priorisierung (spezifisch vor generisch)

### Negativ ⚠️
- Alle Probes werden aufgerufen (aber: early-exit bei Score 100)
- Etwas mehr Speicher für Registry

### API-Design
```c
// Einfache Nutzung
uft_format_t fmt = uft_detect_format(data, len);

// Mit Details
uft_detect_result_t result;
uft_detect_format_ex(data, len, &result);
printf("Format: %s (Confidence: %d%%)\n", 
       result.name, result.confidence);
```

---

## Änderungshistorie

| Datum | Autor | Änderung |
|-------|-------|----------|
| 2025-01-11 | UFT Team | Initial erstellt |
