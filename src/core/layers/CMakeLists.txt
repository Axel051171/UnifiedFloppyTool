# Layer Separation Module - CMakeLists.txt

set(LAYER_SOURCES
    uft_device_manager.c
    uft_format_advisor.c
    uft_job_manager.c
    uft_gui_bridge.c
)

add_library(uft_layers STATIC ${LAYER_SOURCES})

target_include_directories(uft_layers PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(uft_layers
    uft_unified
    pthread
)

# Thread-safe
target_compile_options(uft_layers PRIVATE -pthread)
