# UFT detection Module
set(DETECTION_SOURCES
    uft_blkid.c
    uft_confidence.c
    uft_confidence_v2.c
    uft_format_detect_v2.c
)

add_library(uft_detection STATIC ${DETECTION_SOURCES})

target_include_directories(uft_detection PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(uft_detection PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)
