# CMakeLists.txt for UFT SCP Format Module
# Version: 3.3.2
# Date: 2025-01-03

# Collect source files
set(UFT_SCP_SOURCES
    uft_scp_multirev.c
)

# Create static library
add_library(uft_format_scp STATIC ${UFT_SCP_SOURCES})

# Include directories
target_include_directories(uft_format_scp PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/uft
    ${PROJECT_SOURCE_DIR}/include/uft/formats
)

# Link dependencies
target_link_libraries(uft_format_scp PRIVATE
    m  # math library for sqrt
)

# Compiler options - use C_COMPILER_ID for C code
target_compile_options(uft_format_scp PRIVATE
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wextra>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wpedantic>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wno-unused-parameter>
)

# C11 standard
set_target_properties(uft_format_scp PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)

# Optional: Build test executable
option(UFT_BUILD_SCP_TESTS "Build SCP parser tests" OFF)
if(UFT_BUILD_SCP_TESTS)
    add_executable(test_scp_multirev uft_scp_multirev.c)
    target_compile_definitions(test_scp_multirev PRIVATE SCP_MULTIREV_TEST)
    target_include_directories(test_scp_multirev PRIVATE
        ${PROJECT_SOURCE_DIR}/include
    )
    target_link_libraries(test_scp_multirev PRIVATE m)
endif()
