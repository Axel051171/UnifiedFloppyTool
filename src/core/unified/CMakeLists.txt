# Unified Image Model - CMakeLists.txt

set(UNIFIED_SOURCES
    uft_unified_image.c
    uft_flux_buffer.c
    uft_decoder_registry.c
    uft_tool_adapter.c
    uft_operations.c
)

add_library(uft_unified STATIC ${UNIFIED_SOURCES})

target_include_directories(uft_unified PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(uft_unified
    uft_core
    m  # math library for sqrt()
)

# Install headers
install(FILES
    ${CMAKE_SOURCE_DIR}/include/uft/uft_unified_image.h
    ${CMAKE_SOURCE_DIR}/include/uft/uft_decoder_registry.h
    ${CMAKE_SOURCE_DIR}/include/uft/uft_tool_adapter.h
    ${CMAKE_SOURCE_DIR}/include/uft/uft_operations.h
    DESTINATION include/uft
)
