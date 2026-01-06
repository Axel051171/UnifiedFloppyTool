# UFT Security Compiler Flags
# Aktiviert Stack-Schutz und weitere Sicherheitsmaßnahmen

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    # Stack protection
    add_compile_options(-fstack-protector-strong)
    
    # Fortify source (buffer overflow detection)
    add_compile_definitions(_FORTIFY_SOURCE=2)
    
    # Position Independent Executable
    add_compile_options(-fPIE)
    add_link_options(-pie)
    
    # Relocation Read-Only
    add_link_options(-Wl,-z,relro,-z,now)
    
    # Warnings als Fehler für sicherheitsrelevante Probleme
    add_compile_options(
        -Wall
        -Wextra
        -Wformat=2
        -Wformat-security
        -Wstrict-overflow=2
        -Warray-bounds
        -Wstringop-overflow
    )
    
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
