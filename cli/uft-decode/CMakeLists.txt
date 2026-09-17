# cli/uft-decode — headless Wandler-Binaerprogramm (MF-1216)
#
# EIGENTUEMERENTSCHEIDUNG. Dieses Ziel wurde auf ausdrueckliche Anweisung
# verdrahtet („verdrahten", 2026-09-17), nachdem `A-024`/`MF-1215` die
# Datei als einzigen Posten der Menge C mit Disposition `BLOCKED`
# vorgelegt hatte. Sie steht gegen die GUI-only-Regel in `CLAUDE.md`
# („Keine CLI-Flags hinzufuegen — alles geht ueber GUI/Presets"); die
# Entscheidung dagegen ist die des Eigentuemers und hier festgehalten,
# nicht stillschweigend getroffen.
#
# WAS DER BAU BEIM VERDRAHTEN GEFUNDEN HAT — der eigentliche Ertrag
#     Das Binaerprogramm BAND und LIEF, bevor eine Zeile geaendert wurde:
#     467 Quellen, 0 Fehler, `--help` mit Ausgangskode 0. Und es konnte
#     NICHTS: `uft_convert_file()` erkennt ueber die Format-Registry, und
#     die ist zur Laufzeit LEER, bis jemand `uft_register_all_formats()`
#     ruft — was nur `src/main.cpp:43` tat. Gemessen an einer gueltigen
#     ATR aus dem Korpus: „Could not detect source format", err=-25,
#     Ausgangskode **5**.
#
#     Wer hier beim erfolgreichen Link aufhoert — und genau das
#     beschreibt die Integrations-Pruefliste in `README.md` als
#     „wiring" — verdrahtet ein Programm, das in CI auf jede Datei mit
#     Exit 5 antwortet. Deshalb ist die Abnahme unten eine WANDLUNG mit
#     Byte-Vergleich, kein Bau-Erfolg.
#
# DIE PRUEFLISTE IN `README.md` IST AN EINER STELLE FALSCH
#     Sie nennt `target_link_libraries(uft-decode PRIVATE uft_core
#     uft_crc)`. Das kann nicht linken: `uft_convert_file` steht in
#     `src/formats/uft_format_convert_dispatch.c`, und `src/formats/` ist
#     keine Bibliothek. Die wirkliche Schliessung ist unten gemessen.
#
# QUELLEN UND INCLUDE-PFADE WERDEN ABGELEITET, NICHT AUFGEZAEHLT
#     `tests/CMakeLists.txt:205` haelt fest, dass es dort bereits ZWOELF
#     lokale `*_FORMAT_SOURCES`-GLOBs gibt und nur EINER
#     `${UFT_FORMAT_LAYER_DEPS}` dazunimmt — „Eine DREIZEHNTE
#     Aufzaehlung waere genau der Fehler, den dieser Baum viermal bezahlt
#     hat". Eine VIERZEHNTE waere es hier. Und `UFT_FORMAT_LAYER_DEPS`
#     selbst ist in diesem Zielverzeichnis nicht sichtbar (Variablen aus
#     `tests/` gelten nur dort und darunter).
#
#     Darum wird beides GEGLOBT. Gemessen (2026-09-17, MinGW gcc 13.1.0):
#       * acht Verzeichnisse, **467** Uebersetzungseinheiten, 0 doppelte
#         Symbole, 0 undefinierte Bezuege ausser `uncompress` -> `-lz`
#       * `src/` VOLLSTAENDIG zu globen scheitert an genau EINER Datei:
#         `src/algorithms/uft_kalman_pll.c` bindet
#         `uft/algorithms/uft_kalman_pll.h` ein, und dieser Header
#         existiert im ganzen Dateisystem **0** Mal — die Datei ist nicht
#         unerreichbar, sie ist NICHT UEBERSETZBAR. Deshalb steht
#         `src/algorithms` nicht in der Liste.
#       * `src/forensic` MUSS dabei sein: `uft_fat_bootsector.c` ruft
#         `uft_bs_identifiziere()` (MF-1189/P3-454).
#
#     Der Include-Pfad wird aus der LAGE der Header abgeleitet, weil die
#     Plugins unqualifiziert einbinden ("uft_dms.h"). Das Restrisiko ist
#     benannt: dieser Baum hat Header-Namenskollisionen, und eine breite
#     Include-Menge kann theoretisch die falsche Datei greifen. Der
#     Waechter dagegen ist die Abnahme unten — sie faellt, wenn die
#     Wandlung nicht mehr byteidentisch ist, statt still weiterzulaufen.

cmake_minimum_required(VERSION 3.16)

# --- Quellen: abgeleitet ----------------------------------------------
set(UFT_CLI_SRC_DIRS
    formats core flux detect compat recovery analysis forensic)
set(UFT_CLI_SOURCES "")
foreach(d ${UFT_CLI_SRC_DIRS})
    file(GLOB_RECURSE _found "${CMAKE_SOURCE_DIR}/src/${d}/*.c")
    list(APPEND UFT_CLI_SOURCES ${_found})
endforeach()
list(LENGTH UFT_CLI_SOURCES _n_src)
message(STATUS "uft-decode: ${_n_src} Uebersetzungseinheiten (abgeleitet)")

# --- Include-Pfade: aus der Lage der Header abgeleitet ----------------
file(GLOB_RECURSE _uft_cli_headers
     "${CMAKE_SOURCE_DIR}/include/*.h"
     "${CMAKE_SOURCE_DIR}/src/*.h")
set(UFT_CLI_INCDIRS "${CMAKE_SOURCE_DIR}/include" "${CMAKE_SOURCE_DIR}/src")
foreach(h ${_uft_cli_headers})
    get_filename_component(_d "${h}" DIRECTORY)
    list(APPEND UFT_CLI_INCDIRS "${_d}")
endforeach()
list(REMOVE_DUPLICATES UFT_CLI_INCDIRS)

add_executable(uft-decode main.c ${UFT_CLI_SOURCES})
target_include_directories(uft-decode PRIVATE ${UFT_CLI_INCDIRS})

# zlib: `src/formats/mfi/…` entpackt MAMEs Spurdaten (MF-1020). Gemessen
# ist `uncompress` der EINZIGE Bibliotheksbezug der Schliessung.
find_package(ZLIB QUIET)
if(ZLIB_FOUND)
    target_link_libraries(uft-decode PRIVATE ZLIB::ZLIB)
else()
    # `src/core/CMakeLists.txt:20` haelt fest, dass `find_package(ZLIB)`
    # auf dieser Werkzeugkette NIE angeschlagen hat, obwohl `libz.a` und
    # `zlib.h` daneben liegen. Deshalb der direkte Name als Rueckfall —
    # und er wird GENANNT, nicht still gesetzt.
    message(STATUS "uft-decode: find_package(ZLIB) ohne Treffer — "
                   "Rueckfall auf -lz (siehe src/core/CMakeLists.txt:20)")
    target_link_libraries(uft-decode PRIVATE z)
endif()

# --- Abnahme: eine WANDLUNG, kein Bau-Erfolg --------------------------
# Rotbeweis: ohne `uft_register_all_formats()` in `main.c` antwortet
# genau dieser Aufruf mit Ausgangskode 5 und „Could not detect source
# format" — gemessen, nicht vermutet. Das Ziel ist byteidentisch mit
# `atrcopy_dos2sd.xfd`, einem Erzeugnis FREMDER Hand aus dem Korpus
# (atrcopy; MF-655 hat `atr[16:] == xfd` nachgerechnet).
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/corpus_free/atrcopy_dos2sd.atr"
   AND EXISTS "${CMAKE_SOURCE_DIR}/tests/corpus_free/atrcopy_dos2sd.xfd")
    add_test(NAME uft_decode_cli_wandelt_atr_nach_xfd
             COMMAND ${CMAKE_COMMAND}
                 -DEXE=$<TARGET_FILE:uft-decode>
                 -DQUELLE=${CMAKE_SOURCE_DIR}/tests/corpus_free/atrcopy_dos2sd.atr
                 -DERWARTET=${CMAKE_SOURCE_DIR}/tests/corpus_free/atrcopy_dos2sd.xfd
                 -DZIEL=${CMAKE_CURRENT_BINARY_DIR}/cli_atr2xfd.xfd
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/pruefe_wandlung.cmake)
else()
    message(STATUS "uft-decode: Korpuspaar atrcopy_dos2sd.{atr,xfd} fehlt — "
                   "Abnahme uebersprungen (benannt, nicht still)")
endif()
