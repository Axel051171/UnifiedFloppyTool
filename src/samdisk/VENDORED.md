# Vendored: SAMdisk (Auszüge)

- **Upstream:** https://github.com/simonowen/samdisk (Simon Owen)
- **Lizenz:** siehe Upstream (SAMdisk ist Quell-offen; Datei-Header beachten)
- **Version/Commit:** nicht dokumentiert — vor v4.1.0 vendored (AUD-7,
  MF-369: Herkunfts-Manifest nachgereicht; exakter Stand unbekannt)
- **Zweck in UFT:** **Referenz-Implementierung**, kein Produktions-Code.
  Diese Dateien werden vom Primär-Build (qmake) NICHT kompiliert. Sie dienen
  als autoritative Byte-Spec-Quelle bei Format-Verifikationen — z. B. wurde
  `fdi.cpp::ReadFDI` als Ground-Truth für die ZX-Spectrum-FDI-Neuimplemen-
  tierung genutzt (MF-359, docs/KNOWN_ISSUES.md FMT-12).
- **Regel:** pristine lassen. Keine UFT-Anpassungen in diesem Baum; Fakten
  (Strukturen, Offsets) extrahieren statt Code zu kopieren (Lizenz-Klarheit).
