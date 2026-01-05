# UFT Miscellaneous Format Module (HxC-based)
# Part of UnifiedFloppyTool
# Supports: DMK, IMD, AFI, CPC DSK, MSA, Arburg, H17, etc.

file(GLOB MISC_SOURCES "*.c")
file(GLOB MISC_HEADERS "*.h")

add_library(uft_format_misc STATIC
    ${MISC_SOURCES}
    ${MISC_HEADERS}
)

target_include_directories(uft_format_misc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/include/uft/compat
)

target_link_libraries(uft_format_misc PRIVATE
    uft_encoding
)

set_target_properties(uft_format_misc PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)

if(MSVC)
    target_compile_options(uft_format_misc PRIVATE /W3)
else()
    target_compile_options(uft_format_misc PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-unused-parameter>)
endif()
