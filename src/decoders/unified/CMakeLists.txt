# UFT Unified Decoders
file(GLOB UNIFIED_DEC_SOURCES "*.c")
if(UNIFIED_DEC_SOURCES)
    add_library(uft_decoders_unified STATIC ${UNIFIED_DEC_SOURCES})
    target_include_directories(uft_decoders_unified PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/uft
    )
    set_target_properties(uft_decoders_unified PROPERTIES C_STANDARD 11)
else()
    message(STATUS "uft_decoders_unified: No source files found, skipping")
endif()
