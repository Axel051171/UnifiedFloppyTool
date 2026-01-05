# ═══════════════════════════════════════════════════════════════════════════════
# UnifiedFloppyTool Tests
# ═══════════════════════════════════════════════════════════════════════════════

cmake_minimum_required(VERSION 3.16)

# Enable testing
enable_testing()

# ─────────────────────────────────────────────────────────────────────────────
# Smoke Tests
# ─────────────────────────────────────────────────────────────────────────────
add_executable(smoke_test smoke_test.c)
target_include_directories(smoke_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME smoke_test COMMAND smoke_test)
set_tests_properties(smoke_test PROPERTIES LABELS "smoke;unit")

# ─────────────────────────────────────────────────────────────────────────────
# I/O Tests
# ─────────────────────────────────────────────────────────────────────────────
add_executable(io_test io_test.c)
target_include_directories(io_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME io_test COMMAND io_test)
set_tests_properties(io_test PROPERTIES LABELS "io;unit")

# ─────────────────────────────────────────────────────────────────────────────
# Parser Tests
# ─────────────────────────────────────────────────────────────────────────────
add_executable(parser_test parser_test.c)
target_include_directories(parser_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME parser_test COMMAND parser_test)
set_tests_properties(parser_test PROPERTIES LABELS "parser;unit")

# ─────────────────────────────────────────────────────────────────────────────
# Core Tests (if sources exist)
# ─────────────────────────────────────────────────────────────────────────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_core.c")
    add_executable(test_core test_core.c test_main.c)
    target_include_directories(test_core PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(test_core PRIVATE uft_core)
    add_test(NAME test_core COMMAND test_core)
    set_tests_properties(test_core PROPERTIES LABELS "core;unit")
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Format Tests
# ─────────────────────────────────────────────────────────────────────────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_formats.c")
    add_executable(test_formats test_formats.c test_main.c)
    target_include_directories(test_formats PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(test_formats PRIVATE uft_core)
    add_test(NAME test_formats COMMAND test_formats)
    set_tests_properties(test_formats PROPERTIES LABELS "formats;unit")
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Error Path Tests
# ─────────────────────────────────────────────────────────────────────────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_error_paths.c")
    add_executable(test_error_paths test_error_paths.c test_main.c)
    target_include_directories(test_error_paths PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(test_error_paths PRIVATE uft_core)
    add_test(NAME test_error_paths COMMAND test_error_paths)
    set_tests_properties(test_error_paths PROPERTIES LABELS "error;unit")
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Golden Tests (if directory exists)
# ─────────────────────────────────────────────────────────────────────────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/golden")
    add_subdirectory(golden)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Fuzz Tests (if directory exists)
# ─────────────────────────────────────────────────────────────────────────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fuzz")
    add_subdirectory(fuzz)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Integration Tests
# ─────────────────────────────────────────────────────────────────────────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration")
    add_subdirectory(integration)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Test Summary
# ─────────────────────────────────────────────────────────────────────────────
message(STATUS "Tests configured:")
message(STATUS "  - smoke_test (label: smoke)")
message(STATUS "  - io_test (label: io)")
message(STATUS "  - parser_test (label: parser)")
