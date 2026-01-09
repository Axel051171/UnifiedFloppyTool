# Smoke tests
add_executable(test_version test_version.c)
target_include_directories(test_version PRIVATE ${PROJECT_SOURCE_DIR}/include)
add_test(NAME smoke_version COMMAND test_version)
