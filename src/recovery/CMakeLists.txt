# UFT Recovery Module
file(GLOB RECOVERY_SOURCES "*.c")
if(RECOVERY_SOURCES)
    add_library(uft_recovery STATIC ${RECOVERY_SOURCES})
    target_include_directories(uft_recovery PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/uft
    )
    set_target_properties(uft_recovery PROPERTIES C_STANDARD 11)
else()
    message(STATUS "uft_recovery: No source files found, skipping")
endif()
