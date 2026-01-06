# Transcopy Format Module
add_library(uft_transcopy STATIC uft_transcopy.c)
target_include_directories(uft_transcopy PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(uft_transcopy PRIVATE uft_core)
