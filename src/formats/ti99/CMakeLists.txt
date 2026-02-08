# TI-99/4A Format Support
# TIFILES and FIAD formats

add_library(uft_ti99 STATIC
    uft_tifiles.c
    uft_fiad.c
)

target_include_directories(uft_ti99 PUBLIC 
    ${PROJECT_SOURCE_DIR}/include
)

# Link to core library if available
if(TARGET uft_core)
    target_link_libraries(uft_ti99 uft_core)
endif()
