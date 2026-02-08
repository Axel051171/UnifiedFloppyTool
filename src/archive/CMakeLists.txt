# UFT Archive Module (UFF Container)
file(GLOB ARCHIVE_SOURCES "*.c")
list(FILTER ARCHIVE_SOURCES EXCLUDE REGEX ".*test.*\\.c$")
if(ARCHIVE_SOURCES)
    add_library(uft_archive STATIC ${ARCHIVE_SOURCES})
    target_include_directories(uft_archive PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/uft
    )
    set_target_properties(uft_archive PROPERTIES C_STANDARD 11)
else()
    message(STATUS "uft_archive: No source files found, skipping")
endif()
