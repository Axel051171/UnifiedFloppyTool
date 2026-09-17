# cmake/uft_format_layer.cmake — die Formatschicht-Abhaengigkeiten,
# EINMAL definiert (MF-1218).
#
# Bis hierher standen `UFT_FORMAT_LAYER_DEPS` und
# `UFT_FORMAT_LAYER_INCLUDES` in `tests/CMakeLists.txt:132-191`. CMake-
# Variablen gelten nur im eigenen Verzeichnis und darunter — jedes Ziel
# ausserhalb von `tests/` konnte sie also nicht sehen und musste seine
# eigene Liste bauen. Genau das hat `cli/uft-decode` getan (MF-1216):
# eine 14. Zusammenstellung neben den dreizehn, die
# `tests/CMakeLists.txt:205` bereits als „genau der Fehler, den dieser
# Baum viermal bezahlt hat" benennt.
#
# Der Rumpf unten ist BYTEEXAKT uebernommen, samt Kommentaren. Die
# tragen gemessene Geschichte (MF-473, MF-489, MF-660, MF-1175,
# MF-1189) — sie abzutippen haette sie beschaedigt.
#
# WAS DIESE LISTE IST, UND WAS NICHT — gemessen MF-1218
#     Sie ist die **Ergaenzung** zu den Bibliotheken `uft_core` und
#     `uft_crc`, nicht eine vollstaendige Schliessung. Wer sie fuer ein
#     eigenstaendiges Programm allein benutzt, bekommt gemessen **422**
#     undefinierte Bezuege — `uft_track_add_sector`, `uft_track_init`,
#     `uft_register_format_plugin`, `uft_registered_format_plugin_count`
#     und Verwandte liegen in `src/core/`, das die Testziele als
#     BIBLIOTHEK linken.
#
#     Und sie nennt den SCP-Flussparser NICHT: ohne `src/flux/` fehlen
#     gemessen fuenf Symbole (`uft_scp_open`, `uft_scp_close`,
#     `uft_scp_read_track`, `uft_scp_read_track_memory`,
#     `uft_scp_free_track`). Die Testzweige fuegen ihn je selbst hinzu —
#     das ist `P3-426` in einem Satz.
#
#     Deshalb steht hier keine Zusage „das genuegt". Wer ein Ziel baut,
#     nimmt diese Liste PLUS das, was sein Ziel sonst braucht, und
#     dedupliziert mit `list(REMOVE_DUPLICATES)`.
#
# WAS BEWUSST NICHT GESCHEHEN IST
#     Die ZWOELF lokalen `*_FORMAT_SOURCES`-GLOBs in
#     `tests/CMakeLists.txt` sind NICHT umgestellt. Das ist der Kern von
#     `P3-426`, es sind zwoelf Stellen, und jede aendert das
#     Linkverhalten eines Ziels — das braucht eine eigene Messung je
#     Ziel und gehoert nicht in denselben Commit wie ein Ortswechsel.
#     Diese Datei macht die Liste SICHTBAR; sie macht sie nicht
#     alleinherrschend.

include_guard(GLOBAL)

set(UFT_FORMAT_LAYER_DEPS
            # what the format layer calls out to
            # MF-473: SCP->ADF stimmt jetzt ueber die Umdrehungen ab
            ${CMAKE_SOURCE_DIR}/src/recovery/uft_multiread_pipeline.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_decoder.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_track_verdikt.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_histogram.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_sync_search.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_dewarp.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_decode_timeline.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_media_profile.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_mfm_sector_parser.c
            ${CMAKE_SOURCE_DIR}/src/formats/amiga/uft_amiga_syncs.c
            ${CMAKE_SOURCE_DIR}/src/compat/uft_fnmatch.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_preflight.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_decoder_plugin_stub.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_metadata.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_log.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_loss_report.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_mfm_encoder.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_amiga_mfm_encoder.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_probe_format_impl.c
            # MF-1175: die Anordnungsachse. `src/formats/xfd/uft_xfd.c`
            # rechnet damit jede Dateiposition, statt „einseitig flach"
            # anzunehmen — und weil der GLOB darueber nur `src/formats/*.c`
            # einsammelt, fehlte das Modul beim Linken von 20 Zielen mit
            # 54 undefinierten Bezuegen. Dieselbe Lage wie MF-1168 (13
            # Bezuege auf drei Symbole), und deshalb steht es HIER und
            # nicht in 20 Sonderzweigen.
            ${CMAKE_SOURCE_DIR}/src/core/uft_sector_order.c
            # MF-1189: `src/forensic/uft_bootstrap.c` gehoert hier NICHT
            # her, und das ist gemessen statt gemeint. Der Versuch stand
            # genau an dieser Stelle und deckte **33** der **58** Ziele,
            # die `uft_fat_bootsector.c` uebersetzen; 25 blieben uebrig,
            # weil sie an den zwoelf lokalen `*_FORMAT_SOURCES`-GLOBs aus
            # P3-426 haengen. Die Abhaengigkeit wird deshalb aus der
            # Quellenliste des Ziels ABGELEITET —
            # `uft_wire_traegerherkunft()` vor dieser Schleife.
            # MF-660: Zugang zum Faehigkeits-Manifest. Gehoert in die
            # gemeinsame Liste, nicht in einen Sonderzweig.
            ${CMAKE_SOURCE_DIR}/src/core/uft_plugin_capability.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_disk_metadata.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_roundtrip.c
            ${CMAKE_SOURCE_DIR}/src/analysis/uft_protection_probe.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_kryoflux_stream.c
            # MF-489: der Wandler sieht sein eigenes Ergebnis nach
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/uft_mfm_detect_bridge.c
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/mfm_detect.c
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/cpm_fs.c
    )

# Die Plugins inkludieren ihre Header unqualifiziert ("uft_dms.h").
set(UFT_FORMAT_LAYER_INCLUDES
    ${CMAKE_SOURCE_DIR}/include/uft/detect
    ${CMAKE_SOURCE_DIR}/include/uft/formats
    ${CMAKE_SOURCE_DIR}/include/uft/formats/cbm
    ${CMAKE_SOURCE_DIR}/include/uft/formats/c64
    ${CMAKE_SOURCE_DIR}/include/uft/flux
    ${CMAKE_SOURCE_DIR}/include/uft/fs
    )
