# C64 Copy Protection Detection Module
# Based on Super-Kit 1541 V2.0 documentation

add_library(uft_c64_protection STATIC
    uft_c64_protection.c
)

target_include_directories(uft_c64_protection PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(uft_c64_protection uft_core)

# Add defines for protection detection features
target_compile_definitions(uft_c64_protection PRIVATE
    UFT_C64_PROTECTION_VERSION="1.0"
    UFT_C64_KNOWN_TITLES_DB=1
)

# C64/1541 Track Alignment Module
# Based on nibtools by Pete Rittwage (c64preservation.com)
# Provides track alignment for protected disk mastering

add_library(uft_track_align STATIC
    uft_track_align.c
)

target_include_directories(uft_track_align PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
)

# Add defines for track alignment features
target_compile_definitions(uft_track_align PRIVATE
    UFT_TRACK_ALIGN_VERSION="1.0"
)
