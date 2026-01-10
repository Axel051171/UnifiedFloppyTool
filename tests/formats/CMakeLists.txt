# Format Tests

# SCL Format Test (compiles SCL source directly for standalone testing)
add_executable(test_scl 
    test_scl.c
    ${PROJECT_SOURCE_DIR}/src/formats/scl/uft_scl.c
)
target_include_directories(test_scl PRIVATE ${PROJECT_SOURCE_DIR}/include)
add_test(NAME scl_format COMMAND test_scl)

# ZX BASIC Tokenizer Test
if(TARGET uft_zx)
    add_executable(test_zxbasic test_zxbasic.c)
    target_include_directories(test_zxbasic PRIVATE ${PROJECT_SOURCE_DIR}/include)
    target_link_libraries(test_zxbasic PRIVATE uft_zx)
    if(NOT MSVC)
        target_link_libraries(test_zxbasic PRIVATE m)
    endif()
    add_test(NAME zxbasic COMMAND test_zxbasic)
    set_tests_properties(zxbasic PROPERTIES LABELS "formats;zx")
endif()
