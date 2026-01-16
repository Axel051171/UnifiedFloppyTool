# CMakeLists.txt for UFT 86Box Format Module
# Version: 3.9.0 - Standalone implementation

set(UFT_86BOX_SOURCES
    uft_86box.c
)

add_library(uft_format_86box STATIC ${UFT_86BOX_SOURCES})

# Minimal include directories for standalone build
target_include_directories(uft_format_86box PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_options(uft_format_86box PRIVATE
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wextra>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wno-unused-parameter>
)

set_target_properties(uft_format_86box PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)
