# UFT IBM Format Module
# Part of UnifiedFloppyTool
# Supports: ISO/IBM FM/MFM tracks

file(GLOB IBM_SOURCES "*.c")
file(GLOB IBM_HEADERS "*.h")

add_library(uft_format_ibm STATIC
    ${IBM_SOURCES}
    ${IBM_HEADERS}
)

target_include_directories(uft_format_ibm PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/include/uft/compat
)

target_link_libraries(uft_format_ibm PRIVATE
    uft_encoding
)

set_target_properties(uft_format_ibm PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)

if(MSVC)
    target_compile_options(uft_format_ibm PRIVATE /W3)
else()
    target_compile_options(uft_format_ibm PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-unused-parameter>)
endif()
