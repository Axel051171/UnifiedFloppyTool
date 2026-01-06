# ===========================================================================
# UFT Forensic Module - File Signature Detection
# ===========================================================================
#
# Features:
# - 60+ file type signatures
# - Header + Footer detection
# - File carving support
# - Category-based filtering
#

set(UFT_FORENSIC_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/uft_file_signatures.c
)

set(UFT_FORENSIC_HEADERS
    ${CMAKE_SOURCE_DIR}/include/uft/forensic/uft_file_signatures.h
)

add_library(uft_forensic STATIC
    ${UFT_FORENSIC_SOURCES}
    ${UFT_FORENSIC_HEADERS}
)

target_include_directories(uft_forensic PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(uft_forensic PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )
endif()

set_target_properties(uft_forensic PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

message(STATUS "  UFT Forensic: File signature detection (60+ formats)")
