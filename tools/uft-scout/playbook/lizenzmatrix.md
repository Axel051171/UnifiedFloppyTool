# Lizenzmatrix (bindend, Ziel: GPL-2.0-Projekt)

Erkennung IMMER aus LICENSE/COPYING-Datei(en), je Unterverzeichnis
geprüft (Vendoring!). README-Behauptungen sind Hinweis, nie Beleg.

| Zone | Lizenzen | Code portieren | Konzept nachbauen | Oracle-Binary | Anmerkung |
|---|---|---|---|---|---|
| GRÜN | MIT, BSD-2, BSD-3, ISC, Zlib, PD/CC0, GPL-2.0(-only/or-later), LGPL-2.1, MPL-2.0 | ✅ mit Attribution-Header | ✅ | ✅ | samdisk-Muster: Quelle+Commit im Header |
| GELB | GPL-3.0, AGPL, Apache-2.0 | ❌ (GPL-2.0-inkompatibel) | ✅ Verhaltens-Spec | ✅ | Apache-2.0 wird oft fälschlich für kompatibel gehalten — ist es mit GPLv2 nicht |
| ORANGE | BSD-4-Clause | ❌ einlinken | ✅ | ✅ | Helper-Prozess-Weg möglich (DiskFlashback-Muster) — Eigentümer-Vorlage |
| ROT | keine Lizenzdatei, proprietär, NC/ND-Klauseln | ❌ | ✅ (nur aus Doku/Blackbox) | ✅ falls Binary legal beziehbar | „keine Datei" = alle Rechte vorbehalten |
| PRÜFEN | Dual-Lizenz, Datei-Header ≠ Repo-Lizenz, Datenbanken (sui generis) | — | — | — | IMMER Eigentümer-Vorlage; Daten ggf. per Laufzeit-Parser statt Kopie (diskdefs-Muster) |

Sonderfälle aus der Praxis dieses Projekts:
- **Fakten/Parameter** (Geometrien, Timing-Werte): als Werte frei, als
  kuratierte Sammlung ggf. geschützt → Laufzeit-Parser oder eigene
  Erhebung mit Provenienz.
- **GPLv2+ in GPL-2.0-Projekt**: zulässig; SPDX der portierten Datei
  korrekt führen (Lehre aus P0-5).
- **Zwei GELB/ORANGE-Quellen mischen** (z. B. GPL-3-Port neben
  BSD-4-Link im selben Binary): eigene Prüfung je Kombination,
  Eigentümer-Vorlage.
