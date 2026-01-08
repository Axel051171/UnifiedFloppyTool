# UFT Core Module
# Contains: Core API, Error handling, Format detection, PLL

# Collect all existing .c files
file(GLOB CORE_SOURCES "*.c")

# Exclude problematic files
list(FILTER CORE_SOURCES EXCLUDE REGEX ".*test.*\\.c$")
list(FILTER CORE_SOURCES EXCLUDE REGEX ".*_v9\\.c$")

# Headers
set(CORE_HEADERS
    uft_safe_string.h
)

add_library(uft_core STATIC ${CORE_SOURCES})

target_include_directories(uft_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/include/uft/core
    ${CMAKE_SOURCE_DIR}/include/uft/compat
)

# Link dependencies
target_link_libraries(uft_core PUBLIC
    uft_crc
)

set_target_properties(uft_core PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

if(MSVC)
    target_compile_options(uft_core PRIVATE /W3)
else()
    target_compile_options(uft_core PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
