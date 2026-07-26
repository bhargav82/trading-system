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

namespace {

constexpr int kItemsPerIteration = 500;
constexpr size_t kQueueCapacity = 5000;

// Shared across the two benchmark threads. Only thread 0 constructs/destroys it.
std::unique_ptr<SPSCQueue<int>> g_queue;

// Separate queue for the round-trip latency benchmark, which carries
// timestamps (int64_t) rather than plain ints.
std::unique_ptr<SPSCQueue<int64_t>> g_latency_queue;

// Best-effort attempt to separate the two threads onto different cores.
// - Linux: hard affinity mask, actually enforced by the scheduler.
// - macOS Intel: thread_policy_set with THREAD_AFFINITY_POLICY is only a
//   *hint* (an "affinity tag"); the kernel can ignore it.
// - macOS Apple Silicon: this call is effectively a no-op.
void PinThreadToCore(int core_id) {
    #if defined(__linux__)
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core_id, &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    #elif defined(__APPLE__)
        thread_affinity_policy_data_t policy = { core_id + 1 }; // tag must be > 0
        thread_policy_set(pthread_mach_thread_np(pthread_self()),
                        THREAD_AFFINITY_POLICY,
                        (thread_policy_t)&policy,
                        THREAD_AFFINITY_POLICY_COUNT);
    #else
        (void)core_id;
    #endif
}

} // namespace

// ---------------------------------------------------------------------------
// Cross-core throughput benchmark: thread 0 = producer, thread 1 = consumer.
// Google Benchmark's ->Threads(2) launches both threads and synchronizes
// iteration counts between them, so this is a real two-thread run, not two
// independent single-threaded runs.
// ---------------------------------------------------------------------------
static void SPSC_Benchmark_CrossCore(benchmark::State& state) {
    const int thread_index = state.thread_index();

    // Pin producer to core 0, consumer to core 1. Adjust core IDs to match
    // your machine's topology (e.g. avoid hyperthread siblings of each other).
    PinThreadToCore(thread_index == 0 ? 0 : 1);

    if (thread_index == 0) {
        g_queue = std::make_unique<SPSCQueue<int>>(kQueueCapacity);
        ProfilerStart("SPSC_profiling_crosscore");
    }

    // but this guards against the unlikely case thread 1 runs first.
    while (thread_index != 0 && g_queue == nullptr) {
        std::this_thread::yield();
    }

    for (auto _ : state) {
        if (thread_index == 0) {
            // Producer
            for (int i = 0; i < kItemsPerIteration; ++i) {
                g_queue->emplace_back(i);
            }
        } else {
            // Consumer — top() returns T* (nullptr if empty, valid pointer to
            // the front element otherwise). pop() itself is void, so read
            // through the pointer *before* calling pop(), since pop() destroys
            // and zeroes that slot.
            int popped = 0;
            while (popped < kItemsPerIteration) {
                if (int* front = g_queue->top()) {
                    benchmark::DoNotOptimize(*front);
                    g_queue->pop();
                    ++popped;
                }
            }
        }
    }

    if (thread_index == 0) {
        ProfilerStop();
        g_queue.reset();
    }
}
BENCHMARK(SPSC_Benchmark_CrossCore)->Threads(2)->UseRealTime();



static void SPSC_Benchmark_RoundTripLatency(benchmark::State& state) {
    const int thread_index = state.thread_index();
    PinThreadToCore(thread_index == 0 ? 0 : 1);

    if (thread_index == 0) {
        g_latency_queue = std::make_unique<SPSCQueue<int64_t>>(kQueueCapacity);
    }
    while (thread_index != 0 && g_latency_queue == nullptr) {
        std::this_thread::yield();
    }

    ProfilerStart("SPSC_profiling_roundtrip");

    for (auto _ : state) {
        
        if (thread_index == 0) {
            auto start = std::chrono::high_resolution_clock::now();
            auto ts = start.time_since_epoch().count();
            g_latency_queue->emplace_back(ts);

            // Wait for consumer to send it back so we can measure round trip.
            int64_t* echoed_ptr;
            while ((echoed_ptr = g_latency_queue->top()) == nullptr) {
                std::this_thread::yield();
            }
            int64_t echoed = *echoed_ptr; // read before pop() destroys the slot
            g_latency_queue->pop();

            auto end = std::chrono::high_resolution_clock::now();
            auto latency_ns =
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            state.SetIterationTime(latency_ns / 1e9);
            benchmark::DoNotOptimize(echoed);
        } else {
            int64_t* ts_ptr;
            while ((ts_ptr = g_latency_queue->top()) == nullptr) {
                std::this_thread::yield();
            }
            int64_t ts = *ts_ptr; // read before pop() destroys the slot
            g_latency_queue->pop();
            g_latency_queue->emplace_back(ts); // echo it straight back to the producer
        }
    }

    if (thread_index == 0) {
        g_latency_queue.reset();
    }
}
BENCHMARK(SPSC_Benchmark_RoundTripLatency)->Threads(2)->UseManualTime();


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