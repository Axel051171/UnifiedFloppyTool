# Format Adapter Tests

add_executable(test_format_adapters test_format_adapters.c)

target_include_directories(test_format_adapters PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(test_format_adapters PRIVATE
    uft_adapters
    uft_xdf
)

add_test(NAME format_adapters COMMAND test_format_adapters)
set_tests_properties(format_adapters PROPERTIES LABELS "adapters;xdf")
