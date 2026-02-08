# UFT Forensic Module
file(GLOB FORENSIC_SOURCES "*.c")
if(FORENSIC_SOURCES)
    add_library(uft_forensic STATIC ${FORENSIC_SOURCES})
    target_include_directories(uft_forensic PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/uft
    )
    set_target_properties(uft_forensic PROPERTIES C_STANDARD 11)
else()
    message(STATUS "uft_forensic: No source files found, skipping")
endif()
