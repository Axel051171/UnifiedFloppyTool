# UFT Test Suite
# Part of INDUSTRIAL_UPGRADE_PLAN

enable_testing()

# Find all test source files
file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test_*.c")

# Create test executables
foreach(test_source ${TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    
    add_executable(${test_name} ${test_source})
    
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/uft
    )
    
    target_link_libraries(${test_name} PRIVATE
        uft_core
        uft_crc
    )
    
    # Register as CTest
    add_test(NAME ${test_name} COMMAND ${test_name})
    
    set_target_properties(${test_name} PROPERTIES
        C_STANDARD 11
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    )
endforeach()

message(STATUS "UFT Tests: ${TEST_SOURCES}")
