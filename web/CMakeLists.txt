# UFT Web Viewer - WASM Build Configuration
# 
# Build with Emscripten:
#   mkdir build-wasm && cd build-wasm
#   emcmake cmake -DCMAKE_BUILD_TYPE=Release ..
#   emmake make
#
# Output: uft_web.js, uft_web.wasm

cmake_minimum_required(VERSION 3.16)
project(uft_web VERSION 1.0.0)

# Check for Emscripten
if(NOT EMSCRIPTEN)
    message(WARNING "This CMakeLists.txt is for Emscripten WASM builds")
    message(WARNING "Use: emcmake cmake ..")
    return()
endif()

# C Standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Source files for WASM build (minimal core)
set(UFT_WEB_SOURCES
    src/web/uft_web_viewer.c
    src/web/uft_web_render.c
    src/web/uft_web_formats.c
    
    # Core format support (minimal set)
    src/formats/core/uft_adf.c
    src/formats/core/uft_d64.c
    src/formats/core/uft_img.c
    src/formats/core/uft_hfe.c
    src/formats/core/uft_woz.c
    
    # Decoder
    src/decoder/uft_mfm.c
    src/decoder/uft_gcr.c
    src/decoder/uft_crc.c
    
    # Protection detection
    src/protection/uft_protection.c
)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
)

# Create WASM module
add_executable(uft_web ${UFT_WEB_SOURCES})

# Emscripten-specific flags
set_target_properties(uft_web PROPERTIES
    SUFFIX ".js"
    LINK_FLAGS "\
        -s WASM=1 \
        -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','getValue','setValue','UTF8ToString'] \
        -s ALLOW_MEMORY_GROWTH=1 \
        -s MAXIMUM_MEMORY=256MB \
        -s MODULARIZE=1 \
        -s EXPORT_NAME='UFTWeb' \
        -s FILESYSTEM=0 \
        -s NO_EXIT_RUNTIME=1 \
        -s ASSERTIONS=0 \
        -O3 \
        --closure 1"
)

# Debug build flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set_target_properties(uft_web PROPERTIES
        LINK_FLAGS "\
            -s WASM=1 \
            -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','getValue','setValue','UTF8ToString'] \
            -s ALLOW_MEMORY_GROWTH=1 \
            -s MODULARIZE=1 \
            -s EXPORT_NAME='UFTWeb' \
            -s FILESYSTEM=0 \
            -s NO_EXIT_RUNTIME=1 \
            -s ASSERTIONS=2 \
            -s SAFE_HEAP=1 \
            -g"
    )
endif()

# Output
message(STATUS "UFT Web Viewer WASM Build")
message(STATUS "  Output: uft_web.js, uft_web.wasm")
