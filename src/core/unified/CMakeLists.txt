# UFT Core Unified Module
file(GLOB UNIFIED_SOURCES "*.c")
if(UNIFIED_SOURCES)
    add_library(uft_core_unified STATIC ${UNIFIED_SOURCES})
    target_include_directories(uft_core_unified PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/uft
    )
    set_target_properties(uft_core_unified PROPERTIES C_STANDARD 11)
else()
    message(STATUS "uft_core_unified: No source files found, skipping")
endif()
