# UFT Parameter System - CMakeLists.txt
# Part of UnifiedFloppyTool

file(GLOB PARAMS_SOURCES "*.c")

add_library(uft_params STATIC ${PARAMS_SOURCES})

target_include_directories(uft_params PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/include/uft/params
)

set_target_properties(uft_params PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)

# Math library via zentrale Funktion
uft_link_math(uft_params)

if(MSVC)
    target_compile_options(uft_params PRIVATE /W3)
else()
    target_compile_options(uft_params PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-unused-parameter>)
endif()
