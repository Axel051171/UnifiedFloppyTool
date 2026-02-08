# ==============================================================================
# UFT Track Analysis - Platform Profiles
# 50+ disk format profiles for XCopy-style analysis
# ==============================================================================

cmake_minimum_required(VERSION 3.16)

set(UFT_PROFILES_SOURCES
    uft_profile_japanese.c
    uft_profile_uk.c
    uft_profile_us.c
    uft_profile_misc.c
    uft_profile_xdf.c
    uft_profiles_all.c
)

set(UFT_PROFILES_HEADERS
    uft_profile_japanese.h
    uft_profiles_all.h
)

add_library(uft_profiles STATIC ${UFT_PROFILES_SOURCES} ${UFT_PROFILES_HEADERS})

target_include_directories(uft_profiles
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link with parent track analysis module
target_link_libraries(uft_profiles
    PRIVATE uft_track_analysis
)

# C11 Standard
set_target_properties(uft_profiles PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

# Strict warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(uft_profiles PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(uft_profiles PRIVATE /W4)
endif()

message(STATUS "UFT Profiles: 50+ platform profiles configured")
message(STATUS "  - Japanese: PC-98, X68000, FM-Towns")
message(STATUS "  - UK: Archimedes, SAM, Spectrum+3, Oric, Dragon")
message(STATUS "  - US: TI-99, TRS-80, Victor 9000, Kaypro, Osborne")
message(STATUS "  - Misc: Enterprise, Einstein, Thomson, Microbee, Sord")
