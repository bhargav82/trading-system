#include "../libraries/common.h"
#include "../libraries/spsc_queue.h"
#include "../libraries/thread.h"
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


static constexpr int N = 10000000; // 10 million
BENCHMARK(SPSC_CROSS_CORE) {
std::atomic<bool> start = false;
#if defined(__linux__)
    // create two threads and call a function that will loop through push and pop
    SPSCQueue<int> q(1024);
    auto* producer = launch_thread(0, 2, [&q]() {
        while (!start.load(std::memory_order_acquire)) {}
        for (int i = 0; i < N; ++i) {
            while (!q.try_insert().first) {}; // spin until it can emplace one bac
            q.emplace(i);
        }
    });
    auto* consumer = launch_thread(1, 2, [&q]() {
        // have a spin lock here until we can start reading -> then start timing
        while (!start.load(std::memory_order_acquire)) {} // spin until we can start timing
        for (int i = 0; i < N; ++i) {
            while (!q.top()) {}; 
            benchmark::DoNotOptimize(*q.top()); // make sure compiler doesn't just throw the loop away
            q.pop();
        }
    });

    assert(consumer != nullptr);
    assert(producer != nullptr);
    // now we know producer and consumer have successfully launched their respective thread and can start timing
    uint32_t cpuid;

    __mm_lfence(); // ensure that benchmarking is within the fence -> no CPU OOO
    uint64_t start = __rdtsc(); // start the timer
    start.store(true, std::memory_order_release);
    producer->join();
    consumer->join();
    uint64_t end = __rdtscp(&cpuid); // end the timer
    _mm_lfence(); // make sure no other instructions are re-ordered before the timer ends

    delete producer;
    delete consumer;
#endif

}

BENCHMARK_MAIN();