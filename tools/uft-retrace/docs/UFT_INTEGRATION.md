# Übergabe an UnifiedFloppyTool

## Keine neue Parallel-Engine

Erkannte historische Regeln werden in bestehende UFT-Schichten eingespeist:

| Messbefund | UFT-Ziel |
|---|---|
| Retry nach CRC-/Sync-Status | `src/recovery/uft_multiread_pipeline.c` |
| Auswahl der häufigsten vollständigen Lesung | bestehende Consensuslogik |
| alternative Amiga-Syncs | zentrale Amiga-Synctabelle |
| lange/kurze Spur | Trackanalyse und Source Facts |
| Raw-/Nibble-Umschaltung | CopyPlan-Empfehlung |
| Read-after-write | Verify-Stufe |
| beschädigte Sektoren überspringen | Salvage-Strategie |

## Vorgeschlagene Schnittstelle

```c
typedef struct {
    uint16_t cylinder;
    uint8_t head;
    uint8_t reads;
    uint8_t distinct_reads;
    uint8_t sectors_expected;
    uint8_t sectors_good;
    uint8_t header_checksum_errors;
    uint8_t data_checksum_errors;
    uint16_t sync_words[16];
    uint8_t sync_count;
    uint32_t track_bits;
    uint32_t revolution_ns;
    bool index_present;
    bool unstable;
    bool nonstandard_length;
} uft_amiga_track_observation_t;
```

Der historische Befund wird als Regel mit Provenienz registriert:

```c
typedef struct {
    const char *id;
    const char *source_sha256;
    const char *program_version;
    const char *measurement_id;
    bool (*matches)(const uft_amiga_track_observation_t *track);
    void (*recommend)(const uft_amiga_track_observation_t *track,
                      uft_copy_plan_t *plan,
                      uft_copy_finding_t *finding);
} uft_measured_copy_rule_t;
```

## Wichtige Begrenzung

Eine Empfehlung darf eine Benutzerauswahl nicht heimlich überschreiben. Sie
liefert:

- empfohlene Ebene;
- benötigte Fähigkeiten;
- Verlustmeldung;
- Messquelle;
- Konfidenz.

## Abnahme

1. Rotbeweis aus der beobachteten historischen Regel.
2. Neuimplementierung in C.
3. Test gegen gespeicherte Ein-/Ausgabe, nicht gegen kopierte Maschinenbytes.
4. Mutation der Regel lässt den Test fallen.
5. CopyPlan meldet fehlende Zielmöglichkeiten.
6. GUI nennt die gemessene Ursache.
