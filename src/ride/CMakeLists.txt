# ═══════════════════════════════════════════════════════════════════════════════
# RIDE Integration Module
# ═══════════════════════════════════════════════════════════════════════════════
# Flux decoding and DOS recognition algorithms ported from RIDE
# Original: https://github.com/tomas-nestorovic/RIDE
# ═══════════════════════════════════════════════════════════════════════════════

set(RIDE_SOURCES
    uft_flux_decoder.c
    uft_flux_loaders.c
    uft_dos_handlers.c
    uft_gcr_decoder.c
    uft_hfe_loader.c
    uft_format_verify_ext.c
    uft_format_verify_common.c
    uft_udi_format.c
)

set(RIDE_HEADERS
    ${CMAKE_SOURCE_DIR}/include/uft/ride/uft_flux_decoder.h
    ${CMAKE_SOURCE_DIR}/include/uft/ride/uft_dos_recognition.h
)

add_library(uft_ride STATIC ${RIDE_SOURCES} ${RIDE_HEADERS})

target_include_directories(uft_ride
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/uft
        ${CMAKE_SOURCE_DIR}/include/uft/ride
)

target_compile_features(uft_ride PRIVATE c_std_11)

# Link math library on Unix
if(UNIX)
    target_link_libraries(uft_ride PRIVATE m)
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(uft_ride PRIVATE /W4)
else()
    target_compile_options(uft_ride PRIVATE 
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -Wno-unused-function
    )
endif()

message(STATUS "RIDE Module: Flux decoder + SCP/KryoFlux + DOS + GCR + HFE + UDI + Verify (IMG/D71/D81/ST/MSA)")
