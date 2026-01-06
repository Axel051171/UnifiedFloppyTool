# ==============================================================================
# UFT Toolchain Helpers
# ==============================================================================

# Detect CPU features at runtime
function(uft_detect_cpu_features)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i686")
        try_run(RUN_RESULT COMPILE_RESULT
            ${CMAKE_BINARY_DIR}/cpu_detect
            ${CMAKE_SOURCE_DIR}/cmake/detect_cpu.c
            RUN_OUTPUT_VARIABLE CPU_FLAGS
        )
        if(CPU_FLAGS)
            message(STATUS "Detected CPU features: ${CPU_FLAGS}")
        endif()
    endif()
endfunction()

# Add SIMD-optimized source file
function(uft_add_simd_source TARGET SOURCE)
    set(SIMD_VARIANT ${ARGN})
    
    if("AVX2" IN_LIST SIMD_VARIANT AND UFT_HAVE_AVX2)
        target_sources(${TARGET} PRIVATE ${SOURCE})
        set_source_files_properties(${SOURCE} PROPERTIES
            COMPILE_FLAGS "-mavx2 -DUFT_SIMD_AVX2"
        )
    elseif("SSE2" IN_LIST SIMD_VARIANT AND UFT_HAVE_SSE2)
        target_sources(${TARGET} PRIVATE ${SOURCE})
        set_source_files_properties(${SOURCE} PROPERTIES
            COMPILE_FLAGS "-msse2 -DUFT_SIMD_SSE2"
        )
    else()
        target_sources(${TARGET} PRIVATE ${SOURCE})
    endif()
endfunction()

# Generate format parser list
function(uft_generate_format_list OUTPUT_FILE)
    file(GLOB FORMAT_SOURCES "${CMAKE_SOURCE_DIR}/src/formats/uft_*_v3.c")
    
    set(FORMAT_LIST "")
    foreach(SOURCE ${FORMAT_SOURCES})
        get_filename_component(NAME ${SOURCE} NAME_WE)
        string(REGEX REPLACE "^uft_" "" FORMAT_NAME ${NAME})
        string(REGEX REPLACE "_v3$" "" FORMAT_NAME ${FORMAT_NAME})
        list(APPEND FORMAT_LIST "${FORMAT_NAME}")
    endforeach()
    
    list(LENGTH FORMAT_LIST FORMAT_COUNT)
    message(STATUS "Found ${FORMAT_COUNT} format parsers")
    
    file(WRITE ${OUTPUT_FILE}
        "/* Auto-generated format list */\n"
        "#ifndef UFT_FORMAT_LIST_H\n"
        "#define UFT_FORMAT_LIST_H\n\n"
        "#define UFT_FORMAT_COUNT ${FORMAT_COUNT}\n\n"
        "static const char* uft_format_names[] = {\n"
    )
    
    foreach(FORMAT ${FORMAT_LIST})
        file(APPEND ${OUTPUT_FILE} "    \"${FORMAT}\",\n")
    endforeach()
    
    file(APPEND ${OUTPUT_FILE}
        "    NULL\n"
        "};\n\n"
        "#endif\n"
    )
endfunction()
