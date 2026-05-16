add_executable(tests
    spsc_queue.cpp
    thread.cpp
    mempool.cpp
)
target_compile_definitions(tests PRIVATE LOG_LEVEL=0)
target_include_directories(tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/header/libraries
)
target_link_libraries(tests
    PRIVATE
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(tests)

# Benchmarks
add_executable(benchmarks
    spsc_queue_bench.cpp
)
target_compile_definitions(benchmarks PRIVATE LOG_LEVEL=0)
target_include_directories(benchmarks
    PRIVATE
        ${CMAKE_SOURCE_DIR}/header/libraries
)
target_link_libraries(benchmarks
    PRIVATE
        benchmark::benchmark_main
)