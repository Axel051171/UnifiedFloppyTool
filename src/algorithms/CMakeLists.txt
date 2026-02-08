# UFT Algorithms Module
file(GLOB ALGO_SOURCES "*.c" "crc/*.c" "pll/*.c")

# Temporarily exclude encoding and tracks subdirectories (P0-2 TODO)
# These have header conflicts with legacy libflux types
# list(FILTER ALGO_SOURCES EXCLUDE REGEX ".*/encoding/.*")
# list(FILTER ALGO_SOURCES EXCLUDE REGEX ".*/tracks/.*")

list(FILTER ALGO_SOURCES EXCLUDE REGEX ".*fuzz.*\\.c$")
list(FILTER ALGO_SOURCES EXCLUDE REGEX ".*test.*\\.c$")

if(ALGO_SOURCES)
    add_library(uft_algorithms STATIC ${ALGO_SOURCES})
    target_include_directories(uft_algorithms PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/uft
        ${CMAKE_SOURCE_DIR}/src/encoding
    )
    set_target_properties(uft_algorithms PROPERTIES C_STANDARD 11)
    if(NOT MSVC)
        target_compile_options(uft_algorithms PRIVATE -Wno-unused-parameter)
    endif()
else()
    # Create empty placeholder
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/uft_algo_stub.c "/* stub */\n")
    add_library(uft_algorithms STATIC ${CMAKE_CURRENT_BINARY_DIR}/uft_algo_stub.c)
    message(STATUS "uft_algorithms: No source files found")
endif()
