# Hardened Parser Tests

add_executable(test_scp_hardened test_scp_hardened.c)
target_link_libraries(test_scp_hardened uft_hardened)
target_include_directories(test_scp_hardened PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_executable(test_d64_hardened test_d64_hardened.c)
target_link_libraries(test_d64_hardened uft_hardened)
target_include_directories(test_d64_hardened PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_test(NAME scp_hardened COMMAND test_scp_hardened)
add_test(NAME d64_hardened COMMAND test_d64_hardened)
