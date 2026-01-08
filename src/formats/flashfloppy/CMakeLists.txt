# UFT FlashFloppy Formats
file(GLOB FF_SOURCES "*.c")
if(FF_SOURCES)
    add_library(uft_flashfloppy STATIC ${FF_SOURCES})
    target_include_directories(uft_flashfloppy PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/uft
    )
    set_target_properties(uft_flashfloppy PROPERTIES C_STANDARD 11)
else()
    message(STATUS "uft_flashfloppy: No source files found, skipping")
endif()
