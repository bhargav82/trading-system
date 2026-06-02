#include "common.h"
#include "spsc_queue.h"
#include <benchmark/benchmark.h>
#include <gperftools/profiler.h>



static void SPSC_Benchmark_EMPLACE(benchmark::State& state) {
    ProfilerStart("SPSC_profiling_emplace");

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

BENCHMARK(SPSC_Benchmark_EMPLACE);




static void SPSC_Benchmark_POP(benchmark::State& state) {
    ProfilerStart("SPSC_profiling_pop");
    
    for (auto _ : state) {
        state.PauseTiming();
        SPSCQueue<int> q(5000);
        for (int i = 0; i < 500; ++i) {
            q.emplace_back(i);
        }
        state.ResumeTiming();
        for (int i = 0; i < 500; ++i) {
            q.pop();
        }
    }
    ProfilerStop();
}

BENCHMARK(SPSC_Benchmark_POP);