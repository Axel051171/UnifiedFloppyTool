# SAMdisk Integration Module
# Author: Simon Owen (original SAMdisk project)
# UFT Integration: Core bitstream modules only (no hardware drivers)
#
# This builds the core SAMdisk bitstream processing library
# without hardware dependencies.

option(UFT_ENABLE_SAMDISK "Enable SAMdisk C++ bitstream module" ON)

if(UFT_ENABLE_SAMDISK)

# Core bitstream sources (no hardware dependencies)
set(SAMDISK_CORE_SOURCES
    # Configuration
    config.h
    SAMdisk.h
    
    # Core Types
    types.h
    Disk.cpp
    Disk.h
    
    # Bitstream Processing
    BitBuffer.cpp
    BitBuffer.h
    BitstreamDecoder.cpp
    BitstreamDecoder.h
    BitstreamEncoder.cpp
    BitstreamEncoder.h
    BitstreamTrackBuilder.cpp
    BitstreamTrackBuilder.h
    
    # Flux Processing
    FluxDecoder.cpp
    FluxDecoder.h
    FluxTrackBuilder.cpp
    FluxTrackBuilder.h
    
    # CRC
    CRC16.cpp
    CRC16.h
    
    # Utilities
    DiskUtil.cpp
    DiskUtil.h
    # MemFile.cpp - excluded, requires lzma.h
    # MemFile.h
    
    # UFT Adapter (C API)
    uft_samdisk_adapter.cpp
)

# Check if sources exist
set(SAMDISK_FOUND_SOURCES "")
foreach(src ${SAMDISK_CORE_SOURCES})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        list(APPEND SAMDISK_FOUND_SOURCES ${src})
    endif()
endforeach()

# Only build if we have the core files
list(LENGTH SAMDISK_FOUND_SOURCES SAMDISK_SOURCE_COUNT)
if(SAMDISK_SOURCE_COUNT GREATER 5)
    
    add_library(uft_samdisk_core STATIC ${SAMDISK_FOUND_SOURCES})
    
    target_include_directories(uft_samdisk_core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/uft
    )
    
    # C++17 required
    set_target_properties(uft_samdisk_core PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
    
    # Define that we're building as part of UFT
    target_compile_definitions(uft_samdisk_core PRIVATE
        UFT_BUILDING_SAMDISK=1
    )
    
    # Platform-specific settings
    if(WIN32)
        target_compile_definitions(uft_samdisk_core PRIVATE 
            _WIN32 
            NOMINMAX
            _CRT_SECURE_NO_WARNINGS
        )
    endif()
    
    if(MSVC)
        target_compile_options(uft_samdisk_core PRIVATE /W3 /EHsc)
    else()
        target_compile_options(uft_samdisk_core PRIVATE 
            -Wall 
            -Wextra 
            -Wno-unused-parameter
            -Wno-sign-compare
        )
    endif()
    
    message(STATUS "SAMdisk Core: ENABLED (${SAMDISK_SOURCE_COUNT} sources)")
    
else()
    message(STATUS "SAMdisk Core: DISABLED (missing sources)")
endif()

endif()  # UFT_ENABLE_SAMDISK

# Documentation:
#
# SAMdisk Core provides:
# - Advanced PLL/bitstream decoding (BitstreamDecoder)
# - Flux-to-bitstream conversion (FluxDecoder)
# - Bitstream encoding for write-back (BitstreamEncoder)
# - CRC calculation (CRC16)
#
# Hardware drivers are NOT included in this core build.
# For hardware support, enable the full SAMdisk project separately.
#
# See: https://github.com/simonowen/samdisk
