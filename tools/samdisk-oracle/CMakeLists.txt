# SPDX-License-Identifier: GPL-2.0-or-later
#
# SAMdisk als hauseigenes Oracle (MF-911)
# =======================================
#
# WOZU
# ----
# `src/samdisk/` ist seit v4.1.0 ein vollstaendiger, MIT-lizenzierter
# SAMdisk-4.0-ALPHA-Quellbaum im Repo — bisher nur zum LESEN, als
# Byte-Spec-Quelle (`src/samdisk/README.md`, MF-458/899). Er wird von
# keinem Build kompiliert.
#
# `src/samdisk/README.md` nennt selbst, was daraus zu holen waere: von
# den Formaten, die `docs/VERIFICATION_TIERS.md` als **T3** (ungeprueft)
# fuehrt, hat SAMdisk fuer acht einen eigenen Handler —
#
#     cfi  cpm  fdi_pc98  ipf  mgt  sap_thomson  scl  udi
#
# und fuer die liesse sich ein Differenzlauf fahren, **ohne eine reale
# Referenzdiskette zu besitzen**. Referenzmaterial ist laut Projektdoku
# der Engpass beim T3-Abbau, nicht Code.
#
# WAS DAS HIER IST UND WAS NICHT
# ------------------------------
# Dies ist ein EIGENSTAENDIGES Bauverzeichnis. Es wird vom
# Wurzel-`CMakeLists.txt` NICHT eingebunden und veraendert den
# Produktionsbau nicht. `src/samdisk/` bleibt unangetastet — die Regel
# „pristine lassen" gilt weiter; hier wird nur darauf verwiesen.
#
# Das Ergebnis ist ein ORACLE, kein Produktionscode. Es dient dem
# Vergleich gegen UFTs eigene Parser. Uebernahme von Code aus
# `src/samdisk/` in `src/` bleibt eine eigene Entscheidung mit eigener
# Attribution (MIT verlangt Vermerk UND Lizenztext — `License.txt`).
#
# EICHUNG FEHLT NOCH
# ------------------
# `tools/uft-innendienst/scripts/kalibrierer.py` fuehrt `samdisk` unter
# den UNGEEICHTEN Oracles. Ein gebautes Binary ist noch kein geeichtes:
# bevor ein Differenzlauf zaehlt, muss belegt sein, dass die Ausgabe das
# ist, wofuer man sie haelt. Der erste Schritt dahin ist ein Lauf gegen
# ein Abbild, dessen Inhalt UNABHAENGIG bekannt ist — im Korpus liegen
# mehrere von fremder Hand (VICE c1541, greaseweazle, hxcfe, mtools).
#
# BAUEN
# -----
#     sudo apt install build-essential cmake zlib1g-dev libbz2-dev liblzma-dev
#     cmake -S tools/samdisk-oracle -B build-samdisk-oracle
#     cmake --build build-samdisk-oracle -j
#
# Auf Windows nicht vorgesehen: vier Quellen sprechen dort direkt die
# Diskettenhardware an (`Driver.cpp`, `KF_WinUsb.cpp`,
# `fdrawsys_dev.cpp`, `fdrawsys_ab_dev.cpp`) und brauchen das
# fdrawcmd-/WinUSB-SDK. Fuer ein Oracle, das nur Dateien liest, ist das
# unnoetig — deshalb ist der Bau auf Linux/WSL zugeschnitten und
# schliesst die vier aus.

cmake_minimum_required(VERSION 3.16)
project(samdisk_oracle CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SAMDISK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../src/samdisk")

if(NOT EXISTS "${SAMDISK_DIR}/SAMdisk.cpp")
    message(FATAL_ERROR
        "src/samdisk/ nicht gefunden. Dieses Verzeichnis baut den "
        "vendorten Bestand; es ist kein eigenstaendiges Projekt.")
endif()

file(GLOB SAMDISK_SOURCES "${SAMDISK_DIR}/*.cpp")

# Die vier Quellen, die die Diskettenhardware unter Windows ansprechen.
# Ein Oracle liest Dateien; es braucht kein Laufwerk.
foreach(_win
        "${SAMDISK_DIR}/Driver.cpp"
        "${SAMDISK_DIR}/KF_WinUsb.cpp"
        "${SAMDISK_DIR}/fdrawsys_dev.cpp"
        "${SAMDISK_DIR}/fdrawsys_ab_dev.cpp")
    list(REMOVE_ITEM SAMDISK_SOURCES "${_win}")
endforeach()

list(LENGTH SAMDISK_SOURCES _n)
message(STATUS "SAMdisk-Oracle: ${_n} Quellen (4 hardwarenahe ausgeschlossen)")

add_executable(samdisk_oracle ${SAMDISK_SOURCES})
target_include_directories(samdisk_oracle PRIVATE "${SAMDISK_DIR}")

# zlib/bzip2/lzma: von den Kompressionsformaten gebraucht (gemessen:
# 4 bzw. 2 bzw. 1 Quelldatei binden die Header ein). `capsimage` wird
# NICHT gebraucht — gemessen 0 Treffer —, womit die IPF-Lizenzfrage
# (MF-638) hier gar nicht erst entsteht.
find_package(ZLIB REQUIRED)
target_link_libraries(samdisk_oracle PRIVATE ZLIB::ZLIB bz2 lzma)
