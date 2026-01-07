# UFT Test Suite
cmake_minimum_required(VERSION 3.16)

# Test Framework Header
set(TEST_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# CRC Tests
add_executable(test_crc test_crc.c)
target_include_directories(test_crc PRIVATE ${TEST_INCLUDE_DIR})

# D64 View Tests
add_executable(test_d64_view test_d64_view.c
    ${CMAKE_SOURCE_DIR}/src/formats/commodore/uft_d64_view.c)
target_include_directories(test_d64_view PRIVATE 
    ${TEST_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/src/formats/commodore)

# TZX Tests (placeholder)
# add_executable(test_tzx test_tzx.c ...)

# CTest integration
enable_testing()
add_test(NAME crc COMMAND test_crc)
add_test(NAME d64_view COMMAND test_d64_view)

# Custom target to run all tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_crc test_d64_view
)
