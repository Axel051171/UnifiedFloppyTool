# Smoke tests

# Test 1: Version check
add_executable(test_version test_version.c)
target_include_directories(test_version PRIVATE ${PROJECT_SOURCE_DIR}/include)
add_test(NAME smoke_version COMMAND test_version)

# Test 2: Version Consistency (detailed)
add_executable(test_version_consistency test_version_consistency.c)
target_include_directories(test_version_consistency PRIVATE ${PROJECT_SOURCE_DIR}/include)
add_test(NAME smoke_version_consistency COMMAND test_version_consistency)

# Test 3: Workflow smoke test (P1-1)
add_executable(test_smoke_workflow test_smoke_workflow.c)
target_include_directories(test_smoke_workflow PRIVATE 
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src/analysis
    ${PROJECT_SOURCE_DIR}/src/analysis/profiles
)
target_link_libraries(test_smoke_workflow PRIVATE
    uft_core
    uft_track_analysis
    uft_profiles
)
add_test(NAME smoke_workflow COMMAND test_smoke_workflow)
