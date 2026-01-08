# UFT Algorithms Module
file(GLOB ALGO_SOURCES "*.c" "crc/*.c" "encoding/*.c" "tracks/*.c" "pll/*.c")
list(FILTER ALGO_SOURCES EXCLUDE REGEX ".*fuzz.*\\.c$")
list(FILTER ALGO_SOURCES EXCLUDE REGEX ".*test.*\\.c$")
if(ALGO_SOURCES)
    add_library(uft_algorithms STATIC ${ALGO_SOURCES})
    target_include_directories(uft_algorithms PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/uft
    )
    set_target_properties(uft_algorithms PROPERTIES C_STANDARD 11)
else()
    message(STATUS "uft_algorithms: No source files found, skipping")
endif()
