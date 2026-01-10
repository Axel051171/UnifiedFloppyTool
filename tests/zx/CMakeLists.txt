# ZX Screen Converter Tests

add_executable(test_zxscreen test_zxscreen.c)

target_include_directories(test_zxscreen PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(test_zxscreen PRIVATE
    uft_zx
)

add_test(NAME zxscreen COMMAND test_zxscreen)
set_tests_properties(zxscreen PROPERTIES LABELS "zx;screen")
