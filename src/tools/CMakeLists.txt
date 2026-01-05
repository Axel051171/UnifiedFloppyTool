# Tool Adapters - CMakeLists.txt

set(TOOL_SOURCES
    uft_tool_greaseweazle.c
    uft_tool_fluxengine.c
    uft_tool_nibtools.c
    uft_tool_hxcfe.c
    uft_tool_adftools.c
    uft_tool_disk_analyse.c
    uft_tool_registry.c
)

add_library(uft_tools STATIC ${TOOL_SOURCES})

target_include_directories(uft_tools PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(uft_tools
    uft_unified
)

# Platform-specific (für fork/exec)
if(UNIX)
    target_compile_definitions(uft_tools PRIVATE _GNU_SOURCE)
endif()
