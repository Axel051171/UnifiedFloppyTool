# UFT Policy Module - Write Safety Gate

set(POLICY_SOURCES
    uft_write_gate.c
)

add_library(uft_policy STATIC ${POLICY_SOURCES})

target_include_directories(uft_policy PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(uft_policy PUBLIC
    uft_core
)

set_target_properties(uft_policy PROPERTIES C_STANDARD 11)

message(STATUS "Policy module: Write Safety Gate enabled")
