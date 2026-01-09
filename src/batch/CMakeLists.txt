# UFT Batch Processing Module
# Provides batch operations for multiple disk images

set(BATCH_SOURCES
    uft_batch.c
)

add_library(uft_batch OBJECT ${BATCH_SOURCES})

target_include_directories(uft_batch PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/include/uft/compat
)

# Optional: Link to core if needed
# target_link_libraries(uft_batch PRIVATE uft_core)
