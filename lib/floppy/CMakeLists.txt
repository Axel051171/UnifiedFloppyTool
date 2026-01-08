# UFT Floppy Library
file(GLOB FLOPPY_SOURCES "src/*.c" "src/*/*.c")
if(FLOPPY_SOURCES)
    add_library(uft_floppy STATIC ${FLOPPY_SOURCES})
    target_include_directories(uft_floppy PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/uft
    )
    set_target_properties(uft_floppy PROPERTIES C_STANDARD 11)
else()
    message(STATUS "uft_floppy: No source files found, skipping")
endif()
