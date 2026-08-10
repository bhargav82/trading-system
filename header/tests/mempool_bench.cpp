#include "../libraries/mempool.h"
#include <benchmark/benchmark.h>

// mimic an order
struct BenchObject {
    int val;
    double payload[4];

    BenchObject() = delete;
    explicit BenchObject(int v) : val(v) {}
};

// Baseline: one heap allocation + construction, then destruction + free, per iteration 
// what the matching engine would pay per order if it new/delete instead of pool
static void Mempool_NewDelete(benchmark::State& state) {
    for (auto _ : state) {
        auto* obj = new BenchObject(42);
        benchmark::DoNotOptimize(obj); // make sure compiler doesn't get rid of these lines
        delete obj;
    }
}
BENCHMARK(Mempool_NewDelete);

// construct/destruct using the pool, should be faster since no heap allocatino on hot path
static void Mempool_Preallocated(benchmark::State& state) {
    MemoryPoolHeap<BenchObject> pool(4);
    for (auto _ : state) {
        auto* obj = pool.construct(42);
        benchmark::DoNotOptimize(obj);
        pool.destruct(obj);
    }
}
BENCHMARK(Mempool_Preallocated);

// Same thing but with more objects 
constexpr int kBatchSize = 32;

static void Mempool_NewDelete_Batch(benchmark::State& state) {
    for (auto _ : state) {
        BenchObject* objs[kBatchSize];
        for (int i = 0; i < kBatchSize; ++i) {
            objs[i] = new BenchObject(i);
        }
        benchmark::DoNotOptimize(objs);
        for (int i = 0; i < kBatchSize; ++i) {
            delete objs[i];
        }
    }
}
BENCHMARK(Mempool_NewDelete_Batch);

static void Mempool_Preallocated_Batch(benchmark::State& state) {
    // need extra slot for full identifier
    MemoryPoolHeap<BenchObject> pool(kBatchSize + 1);
    for (auto _ : state) {
        BenchObject* objs[kBatchSize];
        for (int i = 0; i < kBatchSize; ++i) {
            objs[i] = pool.construct(i);
        }
        benchmark::DoNotOptimize(objs);
        for (int i = 0; i < kBatchSize; ++i) {
            pool.destruct(objs[i]);
        }
    }
}
BENCHMARK(Mempool_Preallocated_Batch);
