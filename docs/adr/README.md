# Architecture Decision Records (ADRs)

Dieses Verzeichnis enthält die **Architecture Decision Records** für UFT.

## Was ist ein ADR?

Ein ADR dokumentiert eine wichtige technische Entscheidung:
- **Warum** wurde etwas so entschieden (nicht nur *was*)
- **Welche Alternativen** wurden betrachtet
- **Welche Konsequenzen** hat die Entscheidung

## Index

| ADR | Titel | Status | Datum |
|-----|-------|--------|-------|
| [0001](ADR-0001-c99-standard.md) | C99 als Sprachstandard | Accepted | 2025-01-11 |
| [0002](ADR-0002-pragma-pack.md) | Pragma Pack für Struct-Packing | Accepted | 2025-01-11 |
| [0003](ADR-0003-error-codes.md) | Einheitliche Error Codes | Accepted | 2025-01-11 |
| [0004](ADR-0004-format-auto-detection.md) | Score-basierte Format Detection | Accepted | 2025-01-11 |

## Neues ADR erstellen

1. Kopiere [ADR-TEMPLATE.md](ADR-TEMPLATE.md)
2. Benenne zu `ADR-XXXX-kurzer-titel.md` (nächste freie Nummer)
3. Fülle alle Abschnitte aus
4. Status auf `Proposed` setzen
5. Pull Request erstellen
6. Nach Review: Status auf `Accepted` ändern

## Status-Definitionen

| Status | Bedeutung |
|--------|-----------|
| **Proposed** | Entwurf, noch nicht final |
| **Accepted** | Angenommen und gültig |
| **Deprecated** | Veraltet, sollte nicht mehr befolgt werden |
| **Superseded** | Ersetzt durch neueres ADR |

## Tipps

- ADRs sind **immutable** — bei Änderungen: neues ADR + altes auf "Superseded"
- Kurz und prägnant halten
- Fakten > Meinungen
- Alternativen dokumentieren (auch verworfene)
