# Policy Module Tests

add_executable(test_write_gate test_write_gate.c)
target_link_libraries(test_write_gate PRIVATE uft_policy uft_core)
target_include_directories(test_write_gate PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_test(NAME write_gate COMMAND test_write_gate)
set_tests_properties(write_gate PROPERTIES LABELS "policy;safety")
