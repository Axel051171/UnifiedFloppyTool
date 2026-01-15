# UFT Hardware Abstraction Layer - CMakeLists.txt

set(HAL_SOURCES
    # Core HAL
    uft_drive.c
    uft_hal_unified.c
    
    # Controller Implementations (working)
    uft_kryoflux_dtc.c
    uft_supercard.c
    uft_xum1541.c
    uft_fc5025.c
    uft_applesauce.c
    uft_nibtools.c
    
    # Greaseweazle - Full implementation
    uft_greaseweazle_full.c
    
    # Pauline - Professional floppy controller
    uft_pauline.c
    
    # Support
    uft_latency_tracking.c
    ufi.c
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
