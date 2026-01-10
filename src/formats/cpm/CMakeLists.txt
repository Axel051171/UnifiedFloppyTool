# ═══════════════════════════════════════════════════════════════════════════════
# UFT CP/M Module - CP/M Filesystem Support
# ═══════════════════════════════════════════════════════════════════════════════
#
# CP/M filesystem support:
#   - Various disk parameter blocks (DPB)
#   - Directory parsing
#   - File extraction
#
# ═══════════════════════════════════════════════════════════════════════════════

set(CPM_SOURCES
    uft_cpm_parser_v3.c
)

# Check which files exist
set(CPM_SOURCES_EXIST)
foreach(src ${CPM_SOURCES})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        list(APPEND CPM_SOURCES_EXIST ${src})
    endif()
endforeach()

if(CPM_SOURCES_EXIST)
    add_library(uft_cpm STATIC ${CPM_SOURCES_EXIST})
    
    target_include_directories(uft_cpm PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/uft
    )
    
    set_target_properties(uft_cpm PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
    )
    
    if(MSVC)
        target_compile_options(uft_cpm PRIVATE /W3)
    else()
        target_compile_options(uft_cpm PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-unused-function)
    endif()
    
    message(STATUS "UFT CPM: CP/M filesystem support enabled")
else()
    message(WARNING "UFT CPM: No compilable sources found")
endif()
