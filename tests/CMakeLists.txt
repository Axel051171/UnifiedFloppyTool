# UFT Test Suite
# Part of INDUSTRIAL_UPGRADE_PLAN

enable_testing()

# Find all test source files
file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test_*.c")

# Exclude tests with missing dependencies (TODO: implement these modules)
set(EXCLUDED_TESTS
    "test_mega65"           # Missing uft_mega65.h
    "test_mega65_fat32"     # Missing uft_d81_probe, uft_mbr_* functions
    "test_platform"         # Missing platform test framework
    "test_pll"              # Missing PLL test dependencies  
    "test_recovery"         # Missing recovery test framework
    "test_roundtrip"        # Missing roundtrip dependencies
    "test_smoke"            # Missing smoke test framework
    "test_woz_writer"       # Missing WOZ writer test deps
    "test_writer_backend"   # Missing writer backend
)

# Create test executables
foreach(test_source ${TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    
    # Skip excluded tests
    list(FIND EXCLUDED_TESTS ${test_name} _idx)
    if(${_idx} GREATER -1)
        message(STATUS "UFT Test SKIPPED (TODO): ${test_name}")
        continue()
    endif()
    
    add_executable(${test_name} ${test_source})
    
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/uft
    )
    
    target_link_libraries(${test_name} PRIVATE
        uft_core
        uft_crc
        uft_ride
    )
    
    # Register as CTest
    add_test(NAME ${test_name} COMMAND ${test_name})
    
    set_target_properties(${test_name} PROPERTIES
        C_STANDARD 11
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    )
    
    message(STATUS "UFT Test ENABLED: ${test_name}")
endforeach()

message(STATUS "════════════════════════════════════════════════════")
message(STATUS "UFT Tests: Run 'ctest' after build to execute")
message(STATUS "════════════════════════════════════════════════════")
