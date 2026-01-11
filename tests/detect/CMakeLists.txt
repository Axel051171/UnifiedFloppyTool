# Format Detection Tests

add_executable(test_format_detect test_format_detect.c)

target_include_directories(test_format_detect PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_sources(test_format_detect PRIVATE
    ${PROJECT_SOURCE_DIR}/src/detect/uft_format_detect.c
)

add_test(NAME format_detect COMMAND test_format_detect)
set_tests_properties(format_detect PROPERTIES LABELS "detect;core")

message(STATUS "Test: format_detect (Auto-Detection Engine)")
