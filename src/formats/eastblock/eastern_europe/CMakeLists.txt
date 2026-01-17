# Eastern European Formats (KC 85, Pravetz, Meritum)
add_library(uft_eastern_europe STATIC
    uft_kc85.c
    uft_pravetz.c
    uft_meritum.c
)
target_include_directories(uft_eastern_europe PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(uft_eastern_europe uft_core)
