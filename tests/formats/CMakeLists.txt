# Format Tests

# SCL Format Test (compiles SCL source directly for standalone testing)
add_executable(test_scl 
    test_scl.c
    ${PROJECT_SOURCE_DIR}/src/formats/scl/uft_scl.c
)
target_include_directories(test_scl PRIVATE ${PROJECT_SOURCE_DIR}/include)
add_test(NAME scl_format COMMAND test_scl)
