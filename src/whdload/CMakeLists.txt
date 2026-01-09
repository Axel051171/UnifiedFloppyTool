# UFT WHDLoad Support Module
# Provides WHDLoad constants, API metadata, and CRC16 for Amiga disk preservation

set(WHDLOAD_SOURCES
    whd_crc16.c
    whdload_resload_api.c
)

add_library(uft_whdload OBJECT ${WHDLOAD_SOURCES})

target_include_directories(uft_whdload PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/include/uft/whdload
)

# Optional: Test executable
option(UFT_BUILD_WHDLOAD_TESTS "Build WHDLoad CRC16 test" OFF)
if(UFT_BUILD_WHDLOAD_TESTS)
    add_executable(test_whdload_crc16 test_crc16.c)
    target_link_libraries(test_whdload_crc16 PRIVATE uft_whdload)
    add_test(NAME whdload_crc16 COMMAND test_whdload_crc16)
endif()

message(STATUS "WHDLoad support module enabled")
