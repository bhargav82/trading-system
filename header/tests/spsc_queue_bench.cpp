#include "common.h"
#include "spsc_queue.h"
#include <benchmark/benchmark.h>
#include <gperftools/profiler.h>

void benchmark_helper() {
    SPSCQueue<int> q(5000);
    for (int round = 0; round < 500; ++round) {
        q.emplace_back(round);
    }
}

static void SPSC_Benchmark(benchmark::State& state) {
    ProfilerStart("SPSC_profiling");
    // use two threads to check actual speed
    for (auto _ : state) {
        for (int i = 0; i < 100; ++i) {
            benchmark_helper();
        }
    }
    ProfilerStop();
}

BENCHMARK(SPSC_Benchmark);