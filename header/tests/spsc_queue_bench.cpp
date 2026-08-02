#include "../libraries/common.h"
#include "../libraries/spsc_queue.h"
#include <benchmark/benchmark.h>
#include <gperftools/profiler.h>

#include <atomic>
#include <memory>
#include <thread>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/thread_policy.h>
#include <pthread.h>
#endif


constexpr int kItemsPerIteration = 500;
constexpr size_t kQueueCapacity = 5000;



static void SPSC_Benchmark_EMPLACE(benchmark::State& state) {
    ProfilerStart("SPSC_profiling_emplace");
    for (auto _ : state) {
        state.PauseTiming();
        SPSCQueue<int> q(kQueueCapacity);
        state.ResumeTiming();
        for (int i = 0; i < kItemsPerIteration; ++i) {
            q.emplace_back(i);
        }
    }
}
BENCHMARK(SPSC_Benchmark_EMPLACE);

static void SPSC_Benchmark_POP(benchmark::State& state) {
    ProfilerStart("SPSC_profiling_pop");
    for (auto _ : state) {
        state.PauseTiming();
        SPSCQueue<int> q(kQueueCapacity);
        for (int i = 0; i < kItemsPerIteration; ++i) {
            q.emplace_back(i);
        }
        state.ResumeTiming();
        for (int i = 0; i < kItemsPerIteration; ++i) {
            q.pop();
        }
    }
}
BENCHMARK(SPSC_Benchmark_POP);

BENCHMARK_MAIN();