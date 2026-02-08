# East Block Formats (Robotron, Pravetz, Meritum)
add_library(uft_eastblock STATIC
    uft_robotron.c
    uft_pravetz.c
    uft_meritum.c
)
target_include_directories(uft_eastblock PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(uft_eastblock uft_core)
