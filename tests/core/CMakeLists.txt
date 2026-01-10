# P1-4: Unified Track API Tests

add_executable(test_track_unified test_track_unified.c)

target_include_directories(test_track_unified PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(test_track_unified PRIVATE uft_core)

add_test(NAME track_unified COMMAND test_track_unified)

message(STATUS "Test: track_unified (P1-4)")
