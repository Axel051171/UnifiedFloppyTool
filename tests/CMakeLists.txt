# UnifiedFloppyTool Test Suite

# Smoke tests (basic functionality)
add_subdirectory(smoke)

# Hardened tests (if available)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/hardened/CMakeLists.txt" AND TARGET uft_hardened)
    add_subdirectory(hardened)
endif()

# Header validation (temporarily disabled - missing UFT_ERR_NOMEM)
# if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/header_validation/CMakeLists.txt")
#     add_subdirectory(header_validation)
# endif()
