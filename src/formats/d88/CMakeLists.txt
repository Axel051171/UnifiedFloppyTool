# ═══════════════════════════════════════════════════════════════════════════════
# UFT D88 Module - PC-88/98 Disk Format
# ═══════════════════════════════════════════════════════════════════════════════
#
# D88 format support (NEC PC-88/PC-98):
#   - Variable track/sector layouts
#   - FM/MFM encoding
#   - Multiple disk types (2D, 2DD, 2HD)
#
# ═══════════════════════════════════════════════════════════════════════════════

set(D88_SOURCES
    uft_d88_parser_v3.c
)

# Check which files exist
set(D88_SOURCES_EXIST)
foreach(src ${D88_SOURCES})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        list(APPEND D88_SOURCES_EXIST ${src})
    endif()
endforeach()

if(D88_SOURCES_EXIST)
    add_library(uft_d88 STATIC ${D88_SOURCES_EXIST})
    
    target_include_directories(uft_d88 PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/uft
    )
    
    set_target_properties(uft_d88 PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
    )
    
    if(MSVC)
        target_compile_options(uft_d88 PRIVATE /W3)
    else()
        target_compile_options(uft_d88 PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-unused-function)
    endif()
    
    message(STATUS "UFT D88: PC-88/PC-98 (.d88) support enabled")
else()
    message(WARNING "UFT D88: No compilable sources found")
endif()
