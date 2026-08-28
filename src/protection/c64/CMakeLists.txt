# C64 Copy Protection Detection Module
# Based on Super-Kit 1541 V2.0 documentation

add_library(uft_c64_protection STATIC
    uft_c64_protection.c
    c64_protection_db.c
    c64_protection_analysis.c
)

target_include_directories(uft_c64_protection PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(uft_c64_protection uft_core)

# Add defines for protection detection features
target_compile_definitions(uft_c64_protection PRIVATE
    UFT_C64_PROTECTION_VERSION="1.0"
    UFT_C64_KNOWN_TITLES_DB=1
)

# MF-637: das Ziel `uft_track_align` ist entfernt. Die Quelle war ein Port
# aus nibtools (GPL-3.0 seit 2025-01-30, davor lizenzlos) und liegt seit
# MF-635 in Quarantaene — Beweis: `shift_buffer_left` zeichengleich samt
# des erfundenen Bezeichners `carryshift`, sieben woertliche
# Kommentar-Echos, Entsprechungen zu allen 14 prot.c-Funktionen.
#
# Die Bibliothek wurde gebaut und von NIEMANDEM verlinkt: kein anderes
# CMakeLists nennt das Ziel. Ein drittes Bau-Verzeichnis fuer dieselbe
# Datei — nach der .pro und tests/CMakeLists.txt — und genau dieses hat
# die CI rot gemacht, weil es beim Loeschen uebersehen wurde.
