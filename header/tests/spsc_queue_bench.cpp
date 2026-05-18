#include "common.h"
#include "spsc_queue.h"
#include <benchmark/benchmark.h>
#include <gperftools/profiler.h>



static void SPSC_Benchmark(benchmark::State& state) {
    ProfilerStart("SPSC_profiling");
    // use two threads to check actual speed
    for (auto _ : state) {
        state.PauseTiming();
        SPSCQueue<int> q(5000);
        state.ResumeTiming();
        for (int i = 0; i < 500; ++i) {
            q.emplace_back(i);
        }
    }
    ProfilerStop();
}

BENCHMARK(SPSC_Benchmark);