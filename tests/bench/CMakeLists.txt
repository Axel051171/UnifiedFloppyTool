# UFT Benchmark Suite

option(UFT_BUILD_BENCHMARKS "Build performance benchmarks" OFF)

if(UFT_BUILD_BENCHMARKS)
    add_executable(uft_benchmark uft_benchmark.c)
    target_include_directories(uft_benchmark PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    
    # Optimize for performance
    target_compile_options(uft_benchmark PRIVATE
        $<$<C_COMPILER_ID:GNU>:-O3 -march=native>
        $<$<C_COMPILER_ID:Clang>:-O3 -march=native>
        $<$<C_COMPILER_ID:MSVC>:/O2>
    )
    
    # Add to test suite
    add_test(NAME benchmark_quick COMMAND uft_benchmark 100)
endif()
