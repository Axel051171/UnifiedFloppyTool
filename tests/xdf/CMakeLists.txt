# XDF Integration Tests
add_executable(test_xdf_xcopy test_xdf_xcopy.c)
target_include_directories(test_xdf_xcopy PRIVATE 
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src/analysis
    ${PROJECT_SOURCE_DIR}/src/analysis/profiles
)
target_link_libraries(test_xdf_xcopy PRIVATE uft_profiles uft_track_analysis uft_core)
add_test(NAME xdf_xcopy_integration COMMAND test_xdf_xcopy)

# XDF Adapter Test
if(TARGET uft_xdf)
    add_executable(test_xdf_adapter test_xdf_adapter.c)
    target_include_directories(test_xdf_adapter PRIVATE ${PROJECT_SOURCE_DIR}/include)
    target_link_libraries(test_xdf_adapter PRIVATE uft_xdf)
    add_test(NAME xdf_adapter COMMAND test_xdf_adapter)
    set_tests_properties(xdf_adapter PROPERTIES LABELS "xdf;adapter")
endif()
