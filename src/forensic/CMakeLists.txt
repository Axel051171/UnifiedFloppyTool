# UFT Forensic Module CMake
# HAFTUNGSMODUS: Build Configuration

cmake_minimum_required(VERSION 3.16)

# ============================================================================
# FORENSIC LIBRARY
# ============================================================================

set(FORENSIC_SOURCES
    uft_protection.c
    uft_recovery_bam.c
    uft_recovery_core.c
    uft_xcopy.c
    uft_forensic_params.c
)

set(FORENSIC_HEADERS
    ${PROJECT_SOURCE_DIR}/include/uft/forensic/uft_forensic.h
    ${PROJECT_SOURCE_DIR}/include/uft/forensic/uft_protection.h
    ${PROJECT_SOURCE_DIR}/include/uft/forensic/uft_recovery.h
    ${PROJECT_SOURCE_DIR}/include/uft/forensic/uft_xcopy.h
    ${PROJECT_SOURCE_DIR}/include/uft/forensic/uft_forensic_params.h
)

set(COMPAT_HEADERS
    ${PROJECT_SOURCE_DIR}/include/uft/compat/uft_protection_compat.h
    ${PROJECT_SOURCE_DIR}/include/uft/compat/uft_recovery_compat.h
    ${PROJECT_SOURCE_DIR}/include/uft/compat/uft_xcopy_compat.h
)

add_library(uft_forensic STATIC ${FORENSIC_SOURCES})

target_include_directories(uft_forensic
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(uft_forensic PUBLIC c_std_11)

# Compiler warnings
target_compile_options(uft_forensic PRIVATE
    $<$<C_COMPILER_ID:GNU>:$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wpedantic> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-unused-parameter>>
    $<$<C_COMPILER_ID:Clang>:$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wpedantic> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-unused-parameter>>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)

# Link math library on Unix
if(UNIX)
    target_link_libraries(uft_forensic PRIVATE m)
endif()

# ============================================================================
# INSTALL
# ============================================================================

install(TARGETS uft_forensic
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${FORENSIC_HEADERS}
    DESTINATION include/uft/forensic
)

install(FILES ${COMPAT_HEADERS}
    DESTINATION include/uft/compat
)

# ============================================================================
# TESTS
# ============================================================================

if(BUILD_TESTS)
    # Golden Tests
    add_executable(test_forensic_golden
        ${PROJECT_SOURCE_DIR}/tests/forensic/test_forensic_golden.c
    )
    target_link_libraries(test_forensic_golden PRIVATE uft_forensic)
    add_test(NAME forensic_golden COMMAND test_forensic_golden)
    
    # Negative Tests
    add_executable(test_forensic_negative
        ${PROJECT_SOURCE_DIR}/tests/forensic/test_forensic_negative.c
    )
    target_link_libraries(test_forensic_negative PRIVATE uft_forensic)
    add_test(NAME forensic_negative COMMAND test_forensic_negative)
endif()

# ============================================================================
# FUZZ TARGETS (optional)
# ============================================================================

if(BUILD_FUZZ AND CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(FUZZ_TARGETS
        FUZZ_PROTECTION_DETECT
        FUZZ_WEAK_BITS
        FUZZ_BAM_ANALYZE
        FUZZ_DIR_ANALYZE
        FUZZ_XCOPY_PARSE
        FUZZ_FINGERPRINT
        FUZZ_SYNC_ANALYZE
        FUZZ_FULL_PIPELINE
    )
    
    foreach(target ${FUZZ_TARGETS})
        string(TOLOWER ${target} target_lower)
        add_executable(${target_lower}
            ${PROJECT_SOURCE_DIR}/tests/forensic/fuzz_forensic.c
        )
        target_compile_definitions(${target_lower} PRIVATE ${target})
        target_compile_options(${target_lower} PRIVATE
            -fsanitize=fuzzer,address
        )
        target_link_options(${target_lower} PRIVATE
            -fsanitize=fuzzer,address
        )
        target_link_libraries(${target_lower} PRIVATE uft_forensic)
    endforeach()
endif()
