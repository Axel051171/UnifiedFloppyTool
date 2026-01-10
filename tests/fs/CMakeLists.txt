# P1-5: FAT Detection Tests

add_executable(test_fat_detect test_fat_detect.c)

target_include_directories(test_fat_detect PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# Link with uft_fs or create minimal lib
if(TARGET uft_fs)
    target_link_libraries(test_fat_detect PRIVATE uft_fs)
else()
    # Compile fat_detect.c directly
    target_sources(test_fat_detect PRIVATE
        ${PROJECT_SOURCE_DIR}/src/fs/uft_fat_detect.c
    )
    target_link_libraries(test_fat_detect PRIVATE uft_core)
endif()

add_test(NAME fat_detect COMMAND test_fat_detect)

message(STATUS "Test: fat_detect (P1-5)")
