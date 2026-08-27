# Notiz (keine Neubewertung): jfdelnero/HXCFE_file_selector

Stand: 2026-08-27 · dritter Scout-Zyklus · Inventar: UFT `bb74f540`

## Regel-6-Prüfung (AGENT.md, Negativliste)

Eintrag in `data/known_negatives.json`: `status: verworfen`, Grund
„Zielmaschinen-Firmware, keine übertragbaren Algorithmen"
(Mammut-Evaluierung 2026-08-23).

**Messung** (GitHub-API, `/repos/jfdelnero/HXCFE_file_selector/commits`,
abgefragt 2026-08-27):

| SHA | Datum | Botschaft |
|---|---|---|
| `619e67c5` | 2025-07-13 | Forum moved from torlus.com to hxc2001.com |
| `0abb3f77` | 2024-07-15 | fix potential buffer overrun issues. |
| `4b531ee7` | 2023-01-13 | reformulation. |

Der jüngste Commit liegt **über ein Jahr vor** der Verwerfung
(2025-07-13 < 2026-08-23) und ist eine Doku-URL-Änderung. Seit der
Bewertung: **0 Commits**, also keine gemessene wesentliche Änderung im
relevanten Bereich.

## Ergebnis

**Keine Neubewertung.** Regel 6 lässt sie nicht zu; eine erneute
Vollbewertung wäre ein Regelverstoß. Der Negativlisten-Eintrag bleibt
unverändert bestehen.

Randnotiz für den Zusammenhang: das im selben Zyklus regulär bewertete
`HXCFE_Amiga_copy_utility` (gonk23, 2016) ist ein Ableger dieses
file_selector (`COPYING_FULL` Z. 1–5 nennt die Ableitung ausdrücklich);
der Verwerfungsgrund „Zielmaschinen-Code" trifft dessen Code-Anteil
genauso — der dortige Fund ist eine **Datei**, kein Code. Siehe
`hxcfe_amiga_copy_utility.gutachten.md`.
