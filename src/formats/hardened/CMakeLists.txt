# Hardened Parser Library
# Security-hardened implementations

set(HARDENED_SOURCES
    ../scp/uft_scp_hardened_impl.c
    ../d64/uft_d64_hardened_impl.c
    ../hardened_adapter.c
)

add_library(uft_hardened STATIC ${HARDENED_SOURCES})

target_include_directories(uft_hardened PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_options(uft_hardened PRIVATE
    $<$<C_COMPILER_ID:GNU>:-Wall -Wextra -Wconversion>
    $<$<C_COMPILER_ID:Clang>:-Wall -Wextra -Wconversion>
)
