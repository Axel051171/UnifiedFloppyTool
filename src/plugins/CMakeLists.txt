# Plugin System Module
add_library(uft_plugins STATIC
    uft_plugin_manager.c
)

target_include_directories(uft_plugins PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

if(UNIX AND NOT APPLE)
    target_link_libraries(uft_plugins PRIVATE dl)
endif()
