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
    "test_libdsk_formats"   # Type conflicts with unified_types
    "test_ti99_formats"     # Type conflicts
    "test_new_formats"      # Type conflicts
    "test_fat_extensions"   # Missing uft_fs module (not built in CI)
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
    
    # Base libraries for all tests
    set(TEST_LIBS uft_core uft_crc uft_ride)
    
    # Add test-specific dependencies
    if(${test_name} STREQUAL "test_ti99_formats")
        list(APPEND TEST_LIBS uft_ti99)
    elseif(${test_name} STREQUAL "test_fat_extensions")
        if(TARGET uft_fs)
            list(APPEND TEST_LIBS uft_fs)
        endif()
    endif()
    
    target_link_libraries(${test_name} PRIVATE ${TEST_LIBS})
    
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
