# XCopy Algorithm Tests

add_executable(test_xcopy_algo test_xcopy_algo.c)

target_include_directories(test_xcopy_algo PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/analysis
)

target_link_libraries(test_xcopy_algo PRIVATE
    uft_track_analysis
)

if(NOT MSVC)
    target_link_libraries(test_xcopy_algo PRIVATE m)
endif()

add_test(NAME xcopy_algo COMMAND test_xcopy_algo)
set_tests_properties(xcopy_algo PROPERTIES LABELS "analysis;xcopy")
