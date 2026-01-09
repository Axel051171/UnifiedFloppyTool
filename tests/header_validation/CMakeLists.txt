# Header Validation Test
# Kompiliert nur wenn alle kritischen Typen/Konstanten definiert sind

add_executable(test_critical_types test_critical_types.c)

target_include_directories(test_critical_types PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
)

# Dieser Test wird bei jedem Build ausgeführt
add_test(NAME HeaderValidation COMMAND test_critical_types)
