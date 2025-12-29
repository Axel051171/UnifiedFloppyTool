# =============================================================================
# UnifiedFloppyTool - Test Suite
# =============================================================================

cmake_minimum_required(VERSION 3.20)

# Find Qt Test module
if(QT_VERSION_MAJOR EQUAL 6)
    find_package(Qt6 REQUIRED COMPONENTS Test)
else()
    find_package(Qt5 REQUIRED COMPONENTS Test)
endif()

# =============================================================================
# TEST UTILITIES
# =============================================================================

# Common include directories for all tests
set(TEST_INCLUDES
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Common libraries for all tests
set(TEST_LIBS
    $<IF:$<VERSION_GREATER_EQUAL:${QT_VERSION_MAJOR},6>,Qt6::Test,Qt5::Test>
)

# =============================================================================
# UNIT TESTS (C Core)
# =============================================================================

# Test: Endianness Helpers
add_executable(test_endian test_endian.c)
target_include_directories(test_endian PRIVATE ${TEST_INCLUDES})
add_test(NAME test_endian COMMAND test_endian)

# Test: MFM Decoder
add_executable(test_mfm 
    test_mfm.c
    ${CMAKE_SOURCE_DIR}/src/core/uft_mfm_scalar.c
    ${CMAKE_SOURCE_DIR}/src/core/uft_memory.c
)
target_include_directories(test_mfm PRIVATE ${TEST_INCLUDES})
add_test(NAME test_mfm COMMAND test_mfm)

# Test: GCR Decoder
add_executable(test_gcr
    test_gcr.c
    ${CMAKE_SOURCE_DIR}/src/core/uft_gcr_scalar.c
    ${CMAKE_SOURCE_DIR}/src/core/uft_memory.c
)
target_include_directories(test_gcr PRIVATE ${TEST_INCLUDES})
add_test(NAME test_gcr COMMAND test_gcr)

# Test: Sector Status
add_executable(test_sector_status
    test_sector_status.c
    ${CMAKE_SOURCE_DIR}/src/core/uft_sector_status.c
)
target_include_directories(test_sector_status PRIVATE ${TEST_INCLUDES})
add_test(NAME test_sector_status COMMAND test_sector_status)

# =============================================================================
# INTEGRATION TESTS (Qt)
# =============================================================================

# Test: Path Utilities
add_executable(test_pathutils test_pathutils.cpp)
target_include_directories(test_pathutils PRIVATE ${TEST_INCLUDES})
target_link_libraries(test_pathutils PRIVATE ${TEST_LIBS})
add_test(NAME test_pathutils COMMAND test_pathutils)

# Test: Settings Manager
add_executable(test_settings test_settings.cpp)
target_include_directories(test_settings PRIVATE ${TEST_INCLUDES})
target_link_libraries(test_settings PRIVATE ${TEST_LIBS})
add_test(NAME test_settings COMMAND test_settings)

# Test: Input Validation
add_executable(test_validation test_validation.cpp)
target_include_directories(test_validation PRIVATE ${TEST_INCLUDES})
target_link_libraries(test_validation PRIVATE ${TEST_LIBS})
add_test(NAME test_validation COMMAND test_validation)

# =============================================================================
# SANITIZER TESTS
# =============================================================================

# Test: Memory Safety (runs with ASan)
add_executable(test_memory_safety test_memory_safety.c)
target_include_directories(test_memory_safety PRIVATE ${TEST_INCLUDES})
target_sources(test_memory_safety PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core/uft_memory.c
    ${CMAKE_SOURCE_DIR}/src/core/uft_mfm_scalar.c
)
add_test(NAME test_memory_safety COMMAND test_memory_safety)

# =============================================================================
# REGRESSION TESTS
# =============================================================================

# Regression test runner (requires test samples)
add_test(NAME regression_tests
    COMMAND ${CMAKE_SOURCE_DIR}/tests/run_regression.sh
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(regression_tests PROPERTIES
    LABELS "regression"
    TIMEOUT 300
)

# =============================================================================
# PERFORMANCE TESTS
# =============================================================================

# Benchmark: MFM Decoder
add_executable(bench_mfm bench_mfm.c)
target_include_directories(bench_mfm PRIVATE ${TEST_INCLUDES})
target_sources(bench_mfm PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core/uft_mfm_scalar.c
    ${CMAKE_SOURCE_DIR}/src/core/uft_memory.c
)
# Not added to test suite (run manually)

# =============================================================================
# FUZZING TARGETS
# =============================================================================

if(UFT_ENABLE_FUZZING)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Fuzz: SCP Header Parser
        add_executable(fuzz_scp_header fuzz_scp_header.c)
        target_compile_options(fuzz_scp_header PRIVATE -fsanitize=fuzzer,address)
        target_link_options(fuzz_scp_header PRIVATE -fsanitize=fuzzer,address)
        target_include_directories(fuzz_scp_header PRIVATE ${TEST_INCLUDES})
        
        message(STATUS "Fuzzing targets enabled")
    else()
        message(WARNING "Fuzzing requires Clang compiler")
    endif()
endif()

# =============================================================================
# TEST SUMMARY
# =============================================================================

message(STATUS "")
message(STATUS "Test Suite Configuration:")
message(STATUS "  Unit Tests:        7")
message(STATUS "  Integration Tests: 3")
message(STATUS "  Regression Tests:  1")
message(STATUS "  Fuzzing:           ${UFT_ENABLE_FUZZING}")
message(STATUS "")
