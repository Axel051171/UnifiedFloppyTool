# UFT Archive Module CMake
# HAFTUNGSMODUS: Build Configuration

cmake_minimum_required(VERSION 3.16)

set(ARCHIVE_SOURCES
    uff_container.c
)

set(ARCHIVE_HEADERS
    ${PROJECT_SOURCE_DIR}/include/uft/archive/uff_container.h
    ${PROJECT_SOURCE_DIR}/include/uft/archive/uff_acquisition.h
    ${PROJECT_SOURCE_DIR}/include/uft/archive/uff_export.h
)

add_library(uft_archive STATIC ${ARCHIVE_SOURCES})

target_include_directories(uft_archive
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(uft_archive PUBLIC c_std_11)

target_compile_options(uft_archive PRIVATE
    $<$<C_COMPILER_ID:GNU>:$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wpedantic>>
    $<$<C_COMPILER_ID:Clang>:$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wpedantic>>
)

# Install
install(TARGETS uft_archive
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${ARCHIVE_HEADERS}
    DESTINATION include/uft/archive
)

# Tests
if(BUILD_TESTS)
    add_executable(test_uff_pipeline
        ${PROJECT_SOURCE_DIR}/tests/archive/test_uff_pipeline.c
    )
    target_link_libraries(test_uff_pipeline PRIVATE uft_archive)
    add_test(NAME uff_pipeline COMMAND test_uff_pipeline)
endif()
