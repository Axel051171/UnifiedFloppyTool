# UFT Command Line Interface Module
# Provides CLI tools for disk operations

option(UFT_BUILD_CLI "Build command-line tools" OFF)

if(UFT_BUILD_CLI)
    # Main CLI tool
    add_executable(uft_cli uft_main.c)
    target_include_directories(uft_cli PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/uft
    )
    target_link_libraries(uft_cli PRIVATE 
        uft_core
        uft_formats
    )
    
    # Batch CLI tool
    add_executable(uft_batch_cli uft_batch_cli.c)
    target_include_directories(uft_batch_cli PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    target_link_libraries(uft_batch_cli PRIVATE uft_core)
    
    # Install CLI tools
    install(TARGETS uft_cli uft_batch_cli
        RUNTIME DESTINATION bin
    )
endif()

# Always build CLI support library (for GUI integration)
set(CLI_LIB_SOURCES
    # Add CLI library sources here if needed
)

# Placeholder - CLI module sources can be added to main build
message(STATUS "CLI module available (UFT_BUILD_CLI=${UFT_BUILD_CLI})")
