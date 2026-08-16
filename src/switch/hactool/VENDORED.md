# Vendored: hactool (+ eingebettetes mbedtls)

- **Upstream:** https://github.com/SciresM/hactool (ISC-Lizenz);
  eingebettet: mbedtls (https://github.com/Mbed-TLS/mbedtls, Apache-2.0)
- **Version/Commit:** nicht dokumentiert — vor v4.1.0 vendored (AUD-7,
  MF-369: Herkunfts-Manifest nachgereicht; exakter Stand unbekannt)
- **Zweck in UFT:** Teil des optionalen, **fachfremden**
  Nintendo-Switch-Cartridge-Dumper-Features (`qmake CONFIG+=switch_support`,
  siehe `.pro` und `src/gui/uft_switch_panel.cpp`). Im Default-Build wird
  nichts hiervon kompiliert.
- **Status (AUD-1, docs/KNOWN_ISSUES.md):** Das Feature ist unverifiziert —
  kein Test, kein CI-Lauf mit dem Flag; ob es vollständig linkt, ist offen.
  Entscheidung „extrahieren vs. bewusst behalten" steht aus. Bis dahin: nicht
  bewerben, Baum pristine lassen (in MF-369 wurden lediglich 4 tote
  mbedtls-yotta-Beispiel-mains entfernt; Bibliotheks-Code unangetastet).
