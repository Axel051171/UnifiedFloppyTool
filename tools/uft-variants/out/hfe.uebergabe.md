<!-- uebernommen: MF-656 -->
# Übergabe-Entwurf: Format-Varianten `HFE`
Stand 2026-08-29 · Evidenz: `work/hfe.evidenz.json`
· Korpus: `work/korpus.json` · Regeln: `AGENT.md`

## 1 · Evidenzlage (mechanisch erhoben — Verzweigungen, Magics)
- **magic** (MESSBAR-fähig; unabhängig: hxcfe, samdisk_in_uft; eigener Baum: uft_selbst)
    - `samdisk_in_uft`: `hfe.cpp:8: constexpr std::string_view HFE_SIGNATURE{ "HXCPICFE" };`
    - `hxcfe`: `libhxcfe/sources/loaders/hfe_loader/exthfe_writer.c:109: memcpy(&FILEHEADER->HEADERSIGNATURE,"HX`
    - `uft_selbst`: `hfe/uft_hfe.c:39: #define HFE_SIGNATURE       "HXCPICFE"`
- **v_literal** (nur 1 unabhängige Quelle → [ZU VERIFIZIEREN]; unabhängig: hxcfe; eigener Baum: uft_selbst)
    - `hxcfe`: `libhxcfe/sources/loaders/hfe_loader/hfev3_loader.c:39: // Contains: HFE V3 floppy image loader`
    - `uft_selbst`: `hfe/uft_hfe.c:115: size_t          last_weak_regions;  // RAND opcodes in the most-recent v3 tra`
- **versionfeld** (MESSBAR-fähig; unabhängig: discimagemanager, hxcfe, samdisk_in_uft; eigener Baum: uft_selbst)
    - `samdisk_in_uft`: `hfe.cpp:6: // Note: currently only format revision 00 is supported.`
    - `hxcfe`: `libhxcfe/sources/loaders/hfe_loader/hfe_loader.c:154: header.formatrevision+1,`
    - `discimagemanager`: `README.md:27: For some reason, despite my best attempts, running the ARM version of Disc Image M`
    - `uft_selbst`: `hfe/uft_hfe.c:79: uint8_t     format_revision;        // 0`
- **verzweigung** (KEINE unabhängige Quelle → [ZU VERIFIZIEREN] — nur im eigenen Baum belegt; unabhängig: —; eigener Baum: uft_selbst)
    - `uft_selbst`: `hfe/uft_hfe.c:900: if (strcmp(key, "version") == 0) {`

## 2 · Korpus-Abdeckung je Version
- HFE Version **rev0**: 1 Fixture(s) (z. B. `gw_amigados.hfe`)
Versionen aus dem Dossier ohne Zeile hier = Beschaffungsauftrag

## 3 · UFT-Prüffragen (Tür-Messungen, MF-629-Muster — VOR jeder Behauptung)
- [ ] Erkennt die Probe ALLE Magics/Versionskennungen aus §1?
- [ ] Verzweigt der Reader auf das Versionsfeld — oder liest er eine
      Variante still als eine andere? (Fundstellen aus §1 als Einstieg)
- [ ] Welche Version schreibt der Writer, und ist das begründet
      (Abnehmer benannt)?
- [ ] Widersprechen sich zwei Reader im eigenen Baum? (Evidenz zeigt
      Fundstellen in mehreren UFT-Modulen ⇒ Kandidat)

## 4 · Rotbeweis-Skizzen (je nachgewiesener Variante eine)
Für jede Variante mit Fixture: Fixture rein → erwartetes Verhalten
benennen (dekodiert korrekt / lehnt mit klarer Meldung ab — NIE stiller
Müll). Für Varianten ohne Fixture: Beschaffung zuerst; ersatzweise
Synthetik NUR wenn die Variante per Spec konstruierbar ist, als solche
markiert.

## 5 · UNGEKLÄRT — Tiefenprüfung (Mensch/LLM mit playbook/, nie raten)
- [ ] Welche Indizien aus §1 sind ECHTE Feldvarianten, welche interne
      Versionszähler? (Spec/Websuche, Zwei-Quellen-Regel)
- [ ] Stille-Falschaussage-Stufe je Variante (AGENT.md Maßstab 1–4)
- [ ] WRITE-Zielversion + Abnehmer-Begründung
- [ ] Beschaffungsliste mit Lizenz je Fixture (Fixture-Lizenz wie
      Code-Lizenz)
- [ ] PROBE/READ/WRITE/SKIP-Vorschlag je Variante
- [ ] Abgleich mit docs/FORMAT_VARIANTS*.md im Zielbaum (falls Format
      dort schon behandelt: Dossier ergänzen, nicht duplizieren)
