# Parameter System - CMakeLists.txt

set(PARAMS_SOURCES
    uft_params.c
    uft_params_mapping.c
    uft_params_validation.c
)

add_library(uft_params STATIC ${PARAMS_SOURCES})

target_include_directories(uft_params PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(uft_params
    m
)
