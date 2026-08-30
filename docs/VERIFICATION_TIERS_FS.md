# Verifikations-Stufen der Dateisystem-Schicht (generiert)

**NICHT von Hand editieren** — erzeugt von `scripts/gen_fs_tiers.py` (MF-694). Die Stufen und ihre Grenzen stehen im Kopf dieses Skripts.

Diese Tabelle ist die **Dateisystem-Seite** der Kennzahl „ungeprueft runter. `docs/VERIFICATION_TIERS.md` misst Format-Plugins und kann diese Schicht nicht sehen: sein Generator ordnet ueber `uft_format_plugin_<sym>` zu, und die Leser in `src/fs/` haben keine solchen Verweise. Gemessen (MF-693): die Registrierung von `xdftool` bewegte in der Plugin-Tabelle nichts — eine Hauptspur ohne Skala sieht nach Stillstand aus, waehrend sie laeuft.

## Zusammenfassung

| Stufe | Leser | heisst |
|---|---|---|
| FS-T0 | 1 | kein Test |
| FS-T1 | 5 | nur selbst gebaute Eingaben — zirkulaer |
| FS-T1b | 1 | Korpus von fremder Hand, Hand nicht registriert |
| FS-T2 | 1 | Korpus von **registrierter** fremder Hand |
| **gesamt** | **8** | |

## Pro Leser

| Leser | Stufe | Tests | woran es haengt |
|---|---|---|---|
| `uft_adf_bam` | **FS-T1** | `test_adf_bam` | alle Tests bauen ihre Eingabe selbst — geprueft gegen den eigenen Erzeuger |
| `uft_amiga_virus_db` | **FS-T1** | `test_amiga_virus_db` | alle Tests bauen ihre Eingabe selbst — geprueft gegen den eigenen Erzeuger |
| `uft_amigados` | **FS-T2** | `test_adf`, `test_adf_directory_crosstool`, `test_amiga_extract`, `test_amigados_cycle` | `xdftool_dd_ofs.adf` stammt von `amitools xdftool (pip amitools, 2026-08)` — im Oracle-Register als `xdftool`, der Beleg ist zitierfaehig |
| `uft_amigados_extended` | **FS-T1** | `test_amigados_validate` | alle Tests bauen ihre Eingabe selbst — geprueft gegen den eigenen Erzeuger |
| `uft_bootblock_scanner` | **FS-T1** | `test_amigados_validate`, `test_bootblock_scanner` | alle Tests bauen ihre Eingabe selbst — geprueft gegen den eigenen Erzeuger |
| `uft_cbmdos` | **FS-T1b** | `test_cbmdos_directory` | `vice_c1541_35trk.d64` stammt von `VICE 3.10 c1541 (VICE-Team/svn-mirror release 3.10.0, GTK3VICE-3.10-win64)` — fremde Hand, aber **nicht im Oracle-Register**: der Beleg traegt kein Urteil (ORAK-1) |
| `uft_fat12` | **FS-T1** | `test_fatfs` | alle Tests bauen ihre Eingabe selbst — geprueft gegen den eigenen Erzeuger |
| `uft_fs_amigados_driver` | **FS-T0** | — | kein Test nennt ein Symbol dieses Lesers |

## Was eine Sprosse nicht sagt

Sie ist eine **untere** Schranke: der Leser ist mindestens so weit geprueft. Nicht gemessen werden die **Tiefe** der Pruefung (ein Datentraegername genuegt fuer FS-T1b), die **Unabhaengigkeit einer zweiten Hand** (die fuenfte Frage, MF-644 — sie steht als Prosa im Registry-Eintrag), die **Schreibpfade** und Aufrufe hinter Makros oder Funktionszeigern.

