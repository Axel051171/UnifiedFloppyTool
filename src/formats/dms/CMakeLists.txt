# DMS (Disk Masher System) decompression library
# Based on xDMS 1.3 (Public Domain), refactored for UFT

add_library(uft_dms STATIC
    uft_dms.c
)

target_include_directories(uft_dms PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft/formats
)

# Test executable
add_executable(test_uft_dms
    test_uft_dms.c
    uft_dms.c
)
target_include_directories(test_uft_dms PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft/formats
)
target_link_libraries(test_uft_dms m)
add_test(NAME dms_tests COMMAND test_uft_dms)
