# ═══════════════════════════════════════════════════════════════════════════════
# UFTMath.cmake - Zentrale Math-Library Verwaltung
# ═══════════════════════════════════════════════════════════════════════════════
# 
# REGEL: Die Math-Library 'm' darf NUR über diese Funktion gelinkt werden.
#        Direkte Nutzung von 'm' in target_link_libraries ist VERBOTEN.
#
# HINTERGRUND:
#   - Linux/macOS: Math-Funktionen (sin, cos, sqrt, etc.) sind in libm.so/dylib
#   - Windows:     Math-Funktionen sind Teil der C-Runtime (MSVCRT)
#                  Es gibt KEIN m.lib - Linken führt zu LNK1181!
#
# VERWENDUNG:
#   include(cmake/UFTMath.cmake)
#   uft_link_math(mein_target)
#
# ═══════════════════════════════════════════════════════════════════════════════

function(uft_link_math target)
    # Nur auf echten Unix-Systemen (Linux, macOS, BSD, etc.) linken
    # UNIX ist FALSE auf Windows, auch mit MinGW/Cygwin wenn richtig konfiguriert
    if(UNIX)
        target_link_libraries(${target} PRIVATE m)
    endif()
    # Auf Windows: nichts tun - Math ist Teil der CRT
endfunction()

# ═══════════════════════════════════════════════════════════════════════════════
# Zusätzliche Hilfsfunktion für Threads (ebenfalls plattformabhängig)
# ═══════════════════════════════════════════════════════════════════════════════

function(uft_link_threads target)
    find_package(Threads QUIET)
    if(Threads_FOUND)
        target_link_libraries(${target} PRIVATE Threads::Threads)
    endif()
endfunction()

# ═══════════════════════════════════════════════════════════════════════════════
# Hilfsfunktion für OpenMP (MSVC vs. GCC/Clang)
# ═══════════════════════════════════════════════════════════════════════════════

function(uft_link_openmp target)
    find_package(OpenMP QUIET)
    if(OpenMP_FOUND OR OpenMP_CXX_FOUND OR OpenMP_C_FOUND)
        if(MSVC)
            # MSVC: Nur Compiler-Flag, KEINE Library
            target_compile_options(${target} PRIVATE /openmp)
        else()
            # GCC/Clang: Library normal linken
            if(TARGET OpenMP::OpenMP_CXX)
                target_link_libraries(${target} PRIVATE OpenMP::OpenMP_CXX)
            elseif(TARGET OpenMP::OpenMP_C)
                target_link_libraries(${target} PRIVATE OpenMP::OpenMP_C)
            endif()
        endif()
    endif()
endfunction()
