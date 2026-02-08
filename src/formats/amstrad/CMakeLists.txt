# CMakeLists.txt for UFT Amstrad Format Module
# Version: 3.3.2
# Date: 2025-01-03

# Collect source files
set(UFT_AMSTRAD_SOURCES
    uft_edsk_parser.c
)

# Create static library
add_library(uft_format_amstrad STATIC ${UFT_AMSTRAD_SOURCES})

# Include directories
target_include_directories(uft_format_amstrad PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/uft
    ${PROJECT_SOURCE_DIR}/include/uft/formats
)

# Compiler options - use C_COMPILER_ID for C code
target_compile_options(uft_format_amstrad PRIVATE
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wextra>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wpedantic>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wno-unused-parameter>
)

# C11 standard
set_target_properties(uft_format_amstrad PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)

# Optional: Build test executable
option(UFT_BUILD_EDSK_TESTS "Build EDSK parser tests" OFF)
if(UFT_BUILD_EDSK_TESTS)
    add_executable(test_edsk_parser uft_edsk_parser.c)
    target_compile_definitions(test_edsk_parser PRIVATE EDSK_PARSER_TEST)
    target_include_directories(test_edsk_parser PRIVATE
        ${PROJECT_SOURCE_DIR}/include
    )
endif()
