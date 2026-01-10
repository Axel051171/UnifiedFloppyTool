# ═══════════════════════════════════════════════════════════════════════════════
# UFT TRD Module - ZX Spectrum TR-DOS Format
# ═══════════════════════════════════════════════════════════════════════════════
#
# TR-DOS (Beta Disk Interface) support:
#   - 80/40 tracks × 2 sides × 16 sectors
#   - 256 bytes per sector
#   - TR-DOS filesystem parsing
#
# ═══════════════════════════════════════════════════════════════════════════════

set(TRD_SOURCES
    uft_trd_parser_v3.c
)

# Check which files exist and are compilable
set(TRD_SOURCES_EXIST)
foreach(src ${TRD_SOURCES})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        list(APPEND TRD_SOURCES_EXIST ${src})
    endif()
endforeach()

if(TRD_SOURCES_EXIST)
    add_library(uft_trd STATIC ${TRD_SOURCES_EXIST})
    
    target_include_directories(uft_trd PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/uft
    )
    
    set_target_properties(uft_trd PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
    )
    
    if(MSVC)
        target_compile_options(uft_trd PRIVATE /W3)
    else()
        target_compile_options(uft_trd PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-unused-function)
    endif()
    
    message(STATUS "UFT TRD: ZX Spectrum TR-DOS (.trd) support enabled")
else()
    message(WARNING "UFT TRD: No compilable sources found")
endif()
