# UFT Security Compiler Flags
# Aktiviert Stack-Schutz und weitere Sicherheitsmaßnahmen

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    # Stack protection
    add_compile_options(-fstack-protector-strong)
    
    # Fortify source (buffer overflow detection)
    add_compile_definitions(_FORTIFY_SOURCE=2)
    
    # Position Independent Code (works on all platforms)
    add_compile_options(-fPIC)
    
    # Platform-specific linker flags
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # PIE and RELRO only on Linux
        add_link_options(-Wl,-z,relro,-z,now)
    endif()
    
    # Common warnings
    add_compile_options(
        -Wall
        -Wextra
        -Wformat=2
        -Wformat-security
        -Wstrict-overflow=2
        -Warray-bounds
    )
    
    # GCC-specific warnings (not available on Clang/macOS)
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-Wstringop-overflow)
    endif()
    
    # Optional: AddressSanitizer für Debug-Builds
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
        if(ENABLE_ASAN)
            add_compile_options(-fsanitize=address,undefined)
            add_link_options(-fsanitize=address,undefined)
        endif()
    endif()
endif()

if(MSVC)
    # MSVC security flags
    add_compile_options(/GS /sdl /guard:cf)
    add_link_options(/DYNAMICBASE /NXCOMPAT /GUARD:CF)
endif()
