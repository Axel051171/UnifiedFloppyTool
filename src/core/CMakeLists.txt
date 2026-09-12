# ============================================================================
# UFT Core Module
# ============================================================================
# Contains the 10 real core .c files. All must exist — if one is missing,
# the build fails fast (no more "/* stub */" fallback library).
# ============================================================================

# MF-1070: REQUIRED statt QUIET, Eigentuemer-Entscheidung zu P3-329.
#
# Vorher fand CMake zlib still, linkte es und setzte `UFT_HAS_ZLIB`
# PRIVATE — womit das Makro `uft_core` nicht verliess und KEINE `.c`-Datei
# es las. Wer zlib braucht, sitzt in `src/formats/` (mfi, imz), nicht im
# Kern. Gemessen war die Folge, dass `mfi` aus seinem eigenen
# Korpus-Abbild NULL Spuren lieferte.
#
# `REQUIRED` und `PUBLIC` sind zusammen die Entscheidung: kein stiller
# Ersatzpfad mehr, und das Makro erreicht jedes Ziel, das gegen uft_core
# linkt.
# Und ein zweiter Befund kam beim Umstellen heraus, der in P3-329 noch
# nicht stand: **CMake hat zlib auf dieser Werkzeugkette NIE gefunden.**
# `find_package(ZLIB QUIET)` meldete nichts, der else()-Zweig lief immer,
# und die Meldung „uft_core: zlib found — APD extras enabled" ist hier
# kein einziges Mal erschienen. P3-329 sagte „zlib wird gefunden,
# gelinkt" — das traf fuer den qmake-Bau ohnehin nicht zu (er kannte zlib
# gar nicht) und fuer CMake auf MinGW ebenso wenig.
#
# Der Grund ist kein Fehlen: `libz.a` und `zlib.h` liegen in der
# Qt-MinGW-Toolchain (gemessen: 127 434 bzw. 97 323 Byte unter
# `<toolchain>/x86_64-w64-mingw32/`), und der Compiler findet sie mit
# `-lz` sofort. CMakes `find_library` durchsucht diesen Pfad nur nicht
# von sich aus.
#
# Deshalb derselbe Kandidaten-Ansatz, den die `.pro` fuer libusb benutzt:
# vom Compiler auf die Toolchain-Wurzel schliessen und nachsehen. Ein
# von aussen gesetztes `ZLIB_ROOT` hat Vorrang und wird nicht ueberschrieben.
if(MINGW AND NOT ZLIB_ROOT)
    get_filename_component(_uft_cc_bin "${CMAKE_C_COMPILER}" DIRECTORY)
    get_filename_component(_uft_tc_root "${_uft_cc_bin}/.." ABSOLUTE)
    if(EXISTS "${_uft_tc_root}/x86_64-w64-mingw32/include/zlib.h")
        set(ZLIB_ROOT "${_uft_tc_root}/x86_64-w64-mingw32")
        message(STATUS "uft_core: zlib in der MinGW-Toolchain gefunden — ${ZLIB_ROOT}")
    endif()
endif()

find_package(ZLIB REQUIRED)

set(CORE_SOURCES
    uft_advanced_mode.c
    uft_core_stubs.c           # glue: real plugin-delegation for disk API
    uft_disk_create_image.c    # MF-991: die Tuer zu plugin->create
    ../flux/uft_scp_parser.c   # MF-418: uft_core_stubs' SCP adapters call into it
    uft_decode_pipeline.c
    uft_format_plugin.c        # plugin registry + probe helpers
    uft_format_verify.c        # generic/weak/flux verify
    uft_interleave.c           # TA2 M2: Atari sector interleave calculator
    uft_ir_format.c
    uft_smart_open.c
    uft_unified_types.c
    ../formats/cbm/uft_cbm_geometry.c  # MF-434: the CBM drive geometries,
                                       # used by every CBM plugin and the
                                       # GCR encoder
    uft_write_precomp.c        # TA1 M2: Mac-800K peak-shift compensation
)

# Enforce: every source listed must actually exist (no dead whitelist)
foreach(src ${CORE_SOURCES})
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        message(FATAL_ERROR "uft_core: required source ${src} missing")
    endif()
endforeach()

add_library(uft_core STATIC ${CORE_SOURCES})

target_include_directories(uft_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/include/uft/core
    ${CMAKE_SOURCE_DIR}/include/uft/compat
)

# MF-1070: PUBLIC statt PRIVATE, und kein else()-Zweig mehr.
#
# PRIVATE hiess: das Makro endet an der Grenze von `uft_core`. Die
# Verbraucher sitzen aber in `src/formats/` — `uft_mfi.c` und
# `uft_imz.c` —, also kam es nie an. PUBLIC gibt es an jedes Ziel
# weiter, das gegen uft_core linkt.
#
# Der frühere else()-Zweig meldete „APD extras disabled" und baute
# weiter. Das ist genau der stille Ersatzpfad aus P3-329: der Bau
# gelingt, eine Faehigkeit fehlt, und niemand erfaehrt es. `REQUIRED`
# oben macht daraus einen sichtbaren Abbruch.
target_link_libraries(uft_core PUBLIC uft_crc ZLIB::ZLIB)
target_compile_definitions(uft_core PUBLIC UFT_HAS_ZLIB=1)
message(STATUS "uft_core: zlib VERBINDLICH (UFT_HAS_ZLIB) — MFI/IMZ entpacken")

# uft_interleave uses floorf from libm
if(UNIX)
    target_link_libraries(uft_core PUBLIC m)
endif()

set_target_properties(uft_core PROPERTIES C_STANDARD 11)

if(NOT MSVC)
    target_compile_options(uft_core PRIVATE -Wno-unused-parameter)
endif()

message(STATUS "uft_core: ${CORE_SOURCES}")
