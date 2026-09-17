# Abnahme fuer `uft-decode` (MF-1216) — eine WANDLUNG, kein Bau-Erfolg.
#
# Warum ein eigener Laeufer und nicht `add_test(COMMAND uft-decode …)`:
# ctest prueft dann nur den Ausgangskode. Das reicht hier NICHT. Das
# Binaerprogramm hat vor MF-1216 gebunden, gestartet und `--help` mit
# Kode 0 gedruckt — und fuer JEDE Eingabedatei „Could not detect source
# format" gemeldet, weil niemand `uft_register_all_formats()` rief. Ein
# Test, der nur „laeuft es" fragt, haette das nicht gesehen.
#
# Geprueft wird deshalb das ERGEBNIS, byteweise gegen ein Erzeugnis
# FREMDER Hand: `tests/corpus_free/atrcopy_dos2sd.xfd` stammt von
# atrcopy, und MF-655 hat `atr[16:] == xfd` nachgerechnet.
#
# Erwartet: -D EXE, QUELLE, ERWARTET, ZIEL

foreach(v EXE QUELLE ERWARTET ZIEL)
    if(NOT DEFINED ${v})
        message(FATAL_ERROR "pruefe_wandlung: -D${v} fehlt")
    endif()
endforeach()

if(EXISTS "${ZIEL}")
    file(REMOVE "${ZIEL}")
endif()

execute_process(
    COMMAND "${EXE}" --in "${QUELLE}" --out "${ZIEL}" --format XFD
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE aus
    ERROR_VARIABLE  fehler)

if(NOT rc EQUAL 0)
    message(FATAL_ERROR
        "ROT: uft-decode endete mit ${rc}.\n"
        "Der Wert 5 mit „Could not detect source format\" heisst: die "
        "Format-Registry ist leer — `uft_register_all_formats()` fehlt in "
        "main.c (MF-1216).\n"
        "stdout: ${aus}\nstderr: ${fehler}")
endif()

if(NOT EXISTS "${ZIEL}")
    message(FATAL_ERROR
        "ROT: Ausgangskode 0, aber keine Ausgabedatei — Erfolg ohne Tat.\n"
        "stderr: ${fehler}")
endif()

file(SIZE "${ZIEL}" groesse_ist)
file(SIZE "${ERWARTET}" groesse_soll)
if(NOT groesse_ist EQUAL groesse_soll)
    message(FATAL_ERROR
        "ROT: ${groesse_ist} Byte erzeugt, ${groesse_soll} erwartet.")
endif()

file(MD5 "${ZIEL}" md5_ist)
file(MD5 "${ERWARTET}" md5_soll)
if(NOT md5_ist STREQUAL md5_soll)
    message(FATAL_ERROR
        "ROT: gleiche Groesse (${groesse_ist} Byte), aber andere Bytes.\n"
        "Das ist der interessante Fall — eine Groessenpruefung allein "
        "haette ihn durchgelassen (Lehre MF-1026).\n"
        "erzeugt:  ${md5_ist}\nerwartet: ${md5_soll}")
endif()

message(STATUS
    "GRUEN: ATR->XFD durch uft-decode, ${groesse_ist} Byte byteidentisch "
    "mit dem atrcopy-Erzeugnis (md5 ${md5_ist}).")
