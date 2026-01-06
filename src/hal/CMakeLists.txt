# UFT Hardware Abstraction Layer - CMakeLists.txt

set(HAL_SOURCES
    uft_drive.c
    # uft_hal.c  # DISABLED: Stub, superseded by uft_hal_unified.c
    # uft_hal_profiles.c     # DISABLED: many API mismatches
    uft_hal_unified.c
    # uft_hal_v2.c           # DISABLED: API mismatches
    # uft_greaseweazle_full.c # DISABLED: needs greaseweazle headers
)

add_library(uft_hal STATIC ${HAL_SOURCES})

target_include_directories(uft_hal PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(uft_hal PRIVATE
    uft_core
)

# Platform-specific serial port support
if(WIN32)
    # Windows uses native API
elseif(APPLE)
    # macOS
else()
    # Linux: pthread may be needed
    find_package(Threads)
    if(Threads_FOUND)
        target_link_libraries(uft_hal PRIVATE Threads::Threads)
    endif()
endif()
