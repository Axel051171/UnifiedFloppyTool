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
    uft_scp_direct.c       # MF-254: libusb-based SCP direct transport

    # Greaseweazle - Full implementation
    uft_greaseweazle_full.c

    # Pauline - Professional floppy controller
    uft_pauline.c

    # Support
    uft_latency_tracking.c
    ufi.c
)

# Linux-only: Native FDC support via /dev/fd0
if(UNIX AND NOT APPLE)
    list(APPEND HAL_SOURCES uft_linux_fdc.c)
endif()

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

# MF-254 (v4.1.5-hardening): libusb-1.0 for SCP-Direct production
# transport. Optional dependency — if absent, uft_scp_direct.c
# compiles in honest-stub mode (UFT_ERR_NOT_IMPLEMENTED on every
# I/O call). When present, the real libusb wiring is enabled via
# UFT_HAS_LIBUSB and the SCP can be opened/closed/seeked against
# real hardware.
#
# Search order:
#   1. pkg-config (libusb-1.0.pc) — preferred on UNIX + msys2
#   2. Manual probe in Qt's MinGW prefix (matches the install path
#      we use when libusb is dropped into C:/Qt/Tools/mingw1310_64)
find_package(PkgConfig QUIET)
set(_uft_libusb_found FALSE)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBUSB libusb-1.0)
    if(LIBUSB_FOUND)
        target_include_directories(uft_hal PRIVATE ${LIBUSB_INCLUDE_DIRS})
        target_link_libraries(uft_hal PRIVATE ${LIBUSB_LIBRARIES})
        target_link_directories(uft_hal PRIVATE ${LIBUSB_LIBRARY_DIRS})
        target_compile_definitions(uft_hal PRIVATE UFT_HAS_LIBUSB=1)
        set(_uft_libusb_found TRUE)
    endif()
endif()
if(NOT _uft_libusb_found)
    # Fallback: probe Qt MinGW prefix for a hand-installed libusb
    # (the layout the bench-test install instructions create).
    find_path(_UFT_LIBUSB_INCLUDE_DIR libusb-1.0/libusb.h)
    find_library(_UFT_LIBUSB_LIB
        NAMES usb-1.0 libusb-1.0
        PATH_SUFFIXES lib)
    if(_UFT_LIBUSB_INCLUDE_DIR AND _UFT_LIBUSB_LIB)
        target_include_directories(uft_hal PRIVATE
            ${_UFT_LIBUSB_INCLUDE_DIR})
        target_link_libraries(uft_hal PRIVATE ${_UFT_LIBUSB_LIB})
        target_compile_definitions(uft_hal PRIVATE UFT_HAS_LIBUSB=1)
        set(_uft_libusb_found TRUE)
    endif()
endif()
if(_uft_libusb_found)
    message(STATUS "UFT uft_hal: libusb-1.0 FOUND — "
                   "SCP-Direct libusb wiring ENABLED")
else()
    message(STATUS "UFT uft_hal: libusb-1.0 NOT FOUND — "
                   "SCP-Direct stays honest-stub (NOT_IMPLEMENTED)")
endif()
