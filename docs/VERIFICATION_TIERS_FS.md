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
| **gesamt gefuehrt** | **8** | |

Dazu **26 ungefuehrte Kandidaten** ausserhalb von `src/fs/` — siehe unten. Die Kennzahl zaehlt heute nur die gefuehrten; wer sie liest, muss beide Zahlen sehen (MF-710).

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

## Kandidaten ausserhalb von `src/fs/` — ungefuehrt (MF-710)

Die Tabelle oben fuehrt die Leser in `src/fs/`. Dieser Abschnitt nennt Dateien im uebrigen Baum, die ein **Verzeichnis lesen** und damit dieselbe Arbeit tun, ohne eine Stufe zu tragen. Sie sind **nicht** eingestuft — hier steht, worueber zu entscheiden ist, nicht ein Urteil.

Warum der Abschnitt existiert: bis MF-710 waehlte `leser()` seine Dateien mit `(WURZEL/'src'/'fs').glob('*.c')` — eine hartkodierte Verzeichnisliste in genau jenem Werkzeug, das eine der vier Release-Kennzahlen speist. Gemessen fuehrte die Tabelle **8** Leser, waehrend der Baum **34** Dateien hat, die ein Verzeichnis lesen. Die Kennzahl unterberichtete damit still. Das ist das **zwoelfte** belegte Vorkommen der Aufzaehlung statt der Messung (MF-567/578/598/633/651/652/668/671/678/703/708) — und das erste in einem Werkzeug, das ich selbst dagegen gebaut habe.

Die Dateimenge kommt jetzt aus `git ls-files` (`scripts/repo_scope.py`). Das Merkmal ist gemessen, nicht geraten: mindestens 5 Nennungen von Verzeichnis-Begriffen ausserhalb von Kommentaren.

**26 Kandidaten**, nach Nennungen sortiert:

| Datei | Verzeichnis-Nennungen | Zeilen |
|---|---|---|
| `src/formats/ssd/uft_ssd_parser_v2.c` | 48 | 556 |
| `src/detect/mfm/cpm_fs.c` | 41 | 1426 |
| `src/formats/atari/atari_dos2.c` | 23 | 1007 |
| `src/formats/trd/uft_trd_parser_v2.c` | 23 | 666 |
| `src/formats/c64/uft_bam_editor.c` | 20 | 976 |
| `src/formats/nintendo/uft_switch.c` | 13 | 411 |
| `src/formats/opus/uft_opus.c` | 10 | 425 |
| `src/formats/atari/uft_atari_dos.c` | 9 | 511 |
| `src/formats/mgt/uft_mgt.c` | 9 | 437 |
| `src/formats/atari/uft_atari8_disk.c` | 8 | 338 |
| `src/formats/c64/uft_d64_file.c` | 8 | 849 |
| `src/formats/c64/uft_t64.c` | 8 | 759 |
| `src/formats/atari/atari_sparta.c` | 7 | 411 |
| `src/formats/c64/uft_cmd.c` | 7 | 510 |
| `src/formats/d71/uft_d71_parser_v2.c` | 7 | 373 |
| `src/formats/d81/uft_d81_parser_v2.c` | 7 | 440 |
| `src/formats/msx/uft_msx.c` | 7 | 987 |
| `src/formats/scl/uft_scl_parser_v2.c` | 7 | 504 |
| `src/detect/mfm/mfm_detect.c` | 6 | 1794 |
| `src/formats/c64/uft_geos.c` | 6 | 581 |
| `src/formats/commodore/uft_m2i.c` | 6 | 433 |
| `src/formats/flex/uft_flex.c` | 6 | 276 |
| `src/fileops/uft_file_ops_extended.c` | 5 | 597 |
| `src/formats/atari/atari_check.c` | 5 | 681 |
| `src/formats/jv3/uft_jv3.c` | 5 | 293 |
| `src/formats/legacy/uft_fdi.c` | 5 | 512 |

**Was ein Eintrag hier NICHT heisst:** dass die Datei ungeprueft ist. Viele tragen einen Format-Test und stehen in `docs/VERIFICATION_TIERS.md` — die Plugin-Leiter misst sie, diese hier nicht. Der Befund ist die **Luecke zwischen beiden Leitern**, nicht ein Mangel je Datei.

**Was zu entscheiden ist,** je Datei genau eines: nach `src/fs/` verschieben (dann traegt sie eine FS-Stufe), ausdruecklich als Format-Leser fuehren (dann genuegt die Plugin-Leiter), oder als Doppelung zurueckziehen. Das ist eine Eigentuemer-Entscheidung der ORPH-5-Klasse.


## Was eine Sprosse nicht sagt

Sie ist eine **untere** Schranke: der Leser ist mindestens so weit geprueft. Nicht gemessen werden die **Tiefe** der Pruefung (ein Datentraegername genuegt fuer FS-T1b), die **Unabhaengigkeit einer zweiten Hand** (die fuenfte Frage, MF-644 — sie steht als Prosa im Registry-Eintrag), die **Schreibpfade** und Aufrufe hinter Makros oder Funktionszeigern.

