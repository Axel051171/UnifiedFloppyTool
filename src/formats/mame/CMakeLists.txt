# CMakeLists.txt for UFT MAME MFI Format Module
# Version: 3.9.0 - Standalone implementation

set(UFT_MAME_SOURCES
    uft_mame_mfi.c
)

add_library(uft_format_mame STATIC ${UFT_MAME_SOURCES})

# Minimal include directories for standalone build
target_include_directories(uft_format_mame PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_options(uft_format_mame PRIVATE
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wextra>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wno-unused-parameter>
)

set_target_properties(uft_format_mame PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)
