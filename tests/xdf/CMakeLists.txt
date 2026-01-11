# XDF Integration Tests
add_executable(test_xdf_xcopy test_xdf_xcopy.c)
target_include_directories(test_xdf_xcopy PRIVATE 
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src/analysis
    ${PROJECT_SOURCE_DIR}/src/analysis/profiles
)
target_link_libraries(test_xdf_xcopy PRIVATE uft_profiles uft_track_analysis uft_core)
add_test(NAME xdf_xcopy_integration COMMAND test_xdf_xcopy)
