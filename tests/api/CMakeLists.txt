# API Tests CMakeLists.txt

cmake_minimum_required(VERSION 3.14)

# Add library for new APIs
add_library(uft_api_core STATIC
    ${CMAKE_SOURCE_DIR}/src/pipeline/uft_pipeline.c
    ${CMAKE_SOURCE_DIR}/src/codec/uft_codec.c
    ${CMAKE_SOURCE_DIR}/src/detection/uft_confidence.c
    ${CMAKE_SOURCE_DIR}/src/hal/uft_hal_unified.c
    ${CMAKE_SOURCE_DIR}/src/params/uft_param_bridge.c
)

target_include_directories(uft_api_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_options(uft_api_core PRIVATE
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wpedantic>
)

# Test executables
add_executable(test_pipeline test_pipeline.c)
target_link_libraries(test_pipeline uft_api_core)

add_executable(test_codec test_codec.c)
target_link_libraries(test_codec uft_api_core)

add_executable(test_confidence test_confidence.c)
target_link_libraries(test_confidence uft_api_core)

add_executable(test_hal_unified test_hal_unified.c)
target_link_libraries(test_hal_unified uft_api_core)

add_executable(test_param_bridge test_param_bridge.c)
target_link_libraries(test_param_bridge uft_api_core)

# CTest
enable_testing()
add_test(NAME Pipeline COMMAND test_pipeline)
add_test(NAME Codec COMMAND test_codec)
add_test(NAME Confidence COMMAND test_confidence)
add_test(NAME HAL COMMAND test_hal_unified)
add_test(NAME ParamBridge COMMAND test_param_bridge)
