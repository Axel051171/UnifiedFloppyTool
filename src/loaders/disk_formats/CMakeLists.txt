# UFT Disk Format Loaders (IMD, TD0, DMK, FDI, FDC, Altair)
file(GLOB DISK_FORMATS_SOURCES "*.c")
list(FILTER DISK_FORMATS_SOURCES EXCLUDE REGEX ".*test.*\\.c$")
if(DISK_FORMATS_SOURCES)
    add_library(uft_disk_formats STATIC ${DISK_FORMATS_SOURCES})
    target_include_directories(uft_disk_formats PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/uft
    )
    set_target_properties(uft_disk_formats PROPERTIES C_STANDARD 11)
else()
    message(STATUS "uft_disk_formats: No source files found, skipping")
endif()
