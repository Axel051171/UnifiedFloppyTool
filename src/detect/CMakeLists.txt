# UFT Format Detection Module

set(DETECT_SOURCES
    uft_format_detect.c
)

add_library(uft_detect STATIC ${DETECT_SOURCES})

target_include_directories(uft_detect PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

message(STATUS "Module: uft_detect (Format Auto-Detection)")
