link_directories($ENV{HOME}/local/lib)

add_executable(test_spsc_q
    spsc_queue.cpp
)

add_executable(test_matching_engine
    matching_engine.cpp
)

add_executable(test_thread
    thread.cpp
)

add_executable(test_mempool
    mempool.cpp
)

foreach(target
    test_matching_engine
    test_spsc_q
    test_thread
    test_mempool
)
    target_compile_definitions(${target}
        PRIVATE LOG_LEVEL=1
    )

    target_include_directories(${target}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/header/libraries
    )

    target_link_libraries(${target}
        PRIVATE
            GTest::gtest_main
    )
endforeach()

include(GoogleTest)

gtest_discover_tests(test_matching_engine)
gtest_discover_tests(test_spsc_q)
gtest_discover_tests(test_thread)

# Benchmarks
add_executable(benchmarks
    spsc_queue_bench.cpp
    mempool_bench.cpp
)



target_compile_definitions(benchmarks
    PRIVATE LOG_LEVEL=0
)

target_include_directories(benchmarks
    PRIVATE
        ${CMAKE_SOURCE_DIR}/header/libraries
        $ENV{HOME}/local/include
)

target_link_libraries(benchmarks
    PRIVATE
        benchmark::benchmark_main
)
