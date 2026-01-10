# UFT Roadmap - Format-Erweiterung & Zukunftsplanung

**Stand:** 2026-01-09  
**Version:** v3.7.0  
**Basis:** Systematische Format-Analyse (docs/FORMAT_ANALYSIS.md)

---

## Aktuelle Abdeckung

| Metrik | Wert |
|--------|------|
| Formate vollständig | 36 (26%) |
| Formate partial | 62 (45%) |
| Formate fehlend | 41 (29%) |
| **Nutzbar (voll+partial)** | **98 (71%)** |
| **Gesamt bekannt** | **139** |

---

## Kritische Lücken

| Lücke | Impact | Lösung |
|-------|--------|--------|
| **ADFS fehlt** | UK-Markt, Archimedes | v3.8.0 |
| **DC42 fehlt** | Mac-Community | v3.8.0 |
| **TRD/SCL nur Read** | Spectrum Write | v3.8.0 |
| **Japan unvollständig** | DIM, NFD r1 | v3.9.0 |
| **Mac CLV fehlt** | Historisch wichtig | v4.0.0 |
| **Synthesizer = 0** | Musik-Nische | v4.x |

---

## Timeline

```
2025
═════════════════════════════════════════════════════════════

Q1 ─────────────────────────────────────────────────────────
    v3.8.0 "BBC & Mac Basics"
    ├── P0-10: ADFS Loader           [M, 2-3 Tage]
    ├── P0-11: DC42 Loader           [S, 1 Tag]
    ├── P1-10: TRD/SCL Write         [S, 4h]
    └── P1-14: D77 Loader            [S, 1 Tag]
    
    Formate: 98 → 105 (+7)
    
Q2 ─────────────────────────────────────────────────────────
    v3.9.0 "Japan & Apple"
    ├── P1-11: DIM (X68000)          [M, 2 Tage]
    ├── P1-12: NFD r1                [S, 1 Tag]
    ├── P1-13: A2R v3                [M, 2 Tage]
    └── P2-23: BBC DFS Write         [S, 4h]
    
    Formate: 105 → 115 (+10)

Q3 ─────────────────────────────────────────────────────────
    v4.0.0 "MAJOR RELEASE"
    ├── P2-20: Mac CLV (400K/800K)   [L, 5+ Tage]
    ├── GUI 2.0 Redesign
    ├── Plugin-System
    └── Cloud-Sync Beta
    
    Formate: 115 → 125 (+10)

Q4 ─────────────────────────────────────────────────────────
    v4.1.0 - v4.2.0 "Nischen"
    ├── P2-21: Synthesizer Familie   [L, 1 Woche]
    ├── P2-22: DEC RX50              [L, 3-4 Tage]
    └── P2-24: Brother WP            [M, 2 Tage]
    
    Formate: 125 → 135 (+10)

2026
═════════════════════════════════════════════════════════════

H1 ─────────────────────────────────────────────────────────
    v5.0.0 "Complete Preservation Suite"
    ├── Alle verbleibenden Formate
    ├── 100% Varianten-Support
    ├── Cloud-Integration GA
    └── Enterprise Features
    
    Formate: 135 → 139 (100%)
```

---

## Meilensteine

### M1: 105 Formate (v3.8.0)
- **ETA:** Q1 2025
- **Fokus:** BBC/Acorn, Mac Basics
- **Kritisch:** ADFS, DC42
- **Aufwand:** ~1 Woche

### M2: 115 Formate (v3.9.0)
- **ETA:** Q2 2025
- **Fokus:** Japan, Apple Updates
- **Kritisch:** DIM, NFD r1, A2R v3
- **Aufwand:** ~2 Wochen

### M3: 125 Formate (v4.0.0)
- **ETA:** Q3 2025
- **Fokus:** Mac CLV, Major Features
- **Kritisch:** Variable Speed Support
- **Aufwand:** ~3-4 Wochen

### M4: 135 Formate (v4.2.0)
- **ETA:** Q4 2025
- **Fokus:** Nischen-Formate
- **Optional:** Synthesizer, DEC
- **Aufwand:** ~2-3 Wochen

### FINAL: 139 Formate (v5.0.0)
- **ETA:** H1 2026
- **Status:** "Complete Preservation Suite"
- **Abdeckung:** 100%

---

## Ressourcen-Anforderungen

### Entwicklung
| Phase | Aufwand | Entwickler |
|-------|---------|------------|
| v3.8.0 | ~40h | 1 |
| v3.9.0 | ~80h | 1 |
| v4.0.0 | ~160h | 1-2 |
| v4.x | ~100h | 1 |

### Test-Images
- ADFS: Stairway to Hell
- DC42: Macintosh Garden
- Japan: X68000/PC-98 Archive
- Synthesizer: Community

### Hardware (optional)
- Mac mit CLV-Drive
- BBC Micro
- Sharp X68000

---

## Risiken

| Risiko | Wahrscheinlichkeit | Impact | Mitigation |
|--------|-------------------|--------|------------|
| Mac CLV zu komplex | Mittel | Hoch | Hardware-Partner |
| Japan Doku fehlt | Niedrig | Mittel | Community |
| Synth-Format undokumentiert | Mittel | Niedrig | Reverse Engineering |

---

## Erfolgsmetriken

| Metrik | Ziel v3.8.0 | Ziel v4.0.0 | Ziel v5.0.0 |
|--------|-------------|-------------|-------------|
| Formate nutzbar | 105 | 125 | 139 |
| Abdeckung % | 76% | 90% | 100% |
| Write-Support | 45 | 60 | 80 |
| Protection-Types | 20 | 25 | 30 |

---

**Motto:** "Bei uns geht kein Bit verloren" 🚀
