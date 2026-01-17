# UFT Apple Format Module
# Part of UnifiedFloppyTool
# Supports: Apple II (DO/PO/NIB/2MG/WOZ), Macintosh (DSK/GCR/DiskCopy/NDIF/SMI)

file(GLOB APPLE_SOURCES "*.c")
file(GLOB APPLE_HEADERS "*.h")

# Create static library
add_library(uft_format_apple STATIC
    ${APPLE_SOURCES}
    ${APPLE_HEADERS}
)

target_include_directories(uft_format_apple PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/include/uft/compat
)

# Link dependencies
target_link_libraries(uft_format_apple PRIVATE
    uft_encoding
)

set_target_properties(uft_format_apple PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)

if(MSVC)
    target_compile_options(uft_format_apple PRIVATE /W3)
else()
    target_compile_options(uft_format_apple PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-unused-parameter>)
endif()

# Note: WOZ disk image format (uft_woz.c) is included via GLOB
