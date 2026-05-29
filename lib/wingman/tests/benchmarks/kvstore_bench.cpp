// Wingman Performance Benchmark
// Uses simple manual timing to avoid extra dependencies

#include <wingman/kvstore.hpp>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>

using namespace std::chrono;
using namespace wingman;

// Simple benchmark helper class
class Benchmark {
public:
    template<typename Func>
    static double run(const std::string& name, Func&& func, int iterations = 1000) {
        // Warmup
        func();

        auto start = high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            func();
        }
        auto end = high_resolution_clock::now();

        auto duration = duration_cast<microseconds>(end - start).count();
        double avg = static_cast<double>(duration) / iterations;

        std::cout << std::left << std::setw(40) << name
                  << std::right << std::setw(10) << std::fixed << std::setprecision(2)
                  << avg << " μs/op" << std::endl;

        return avg;
    }
};

// ========== KVStore Benchmarks ==========

void benchmark_kvstore() {
    std::cout << "=== KeyValueStore Performance Benchmark ===" << std::endl;
    std::cout << std::endl;

    KeyValueStore store;

    // 1. SET operation
    Benchmark::run("SET (simple)", [&]() {
        store.set("key", "value");
    });

    // 2. GET operation
    store.set("bench_key", "bench_value");
    Benchmark::run("GET (existing key)", [&]() {
        volatile auto val = store.get("bench_key");
        (void)val;
    });

    // 3. GET non-existing key
    Benchmark::run("GET (non-existing key)", [&]() {
        volatile auto val = store.get("nonexistent");
        (void)val;
    });

    // 4. EXISTS operation
    Benchmark::run("EXISTS", [&]() {
        volatile auto exists = store.exists("bench_key");
        (void)exists;
    });

    // 5. DELETE operation
    Benchmark::run("DELETE", [&]() {
        store.set("temp", "value");
        store.del("temp");
    });

    // 6. INCR operation
    store.set("counter", "100");
    Benchmark::run("INCR", [&]() {
        store.incr("counter");
    });

    // 7. HSET operation
    Benchmark::run("HSET", [&]() {
        store.hset("hash", "field", "value");
    });

    // 8. HGET operation
    store.hset("bench_hash", "bench_field", "bench_value");
    Benchmark::run("HGET", [&]() {
        volatile auto val = store.hget("bench_hash", "bench_field");
        (void)val;
    });

    // 9. LPUSH operation
    Benchmark::run("LPUSH", [&]() {
        store.lpush("list", "item");
    });

    // 10. LPOP operation
    store.lpush("bench_list", "item");
    Benchmark::run("LPOP", [&]() {
        store.lpop("bench_list");
        store.lpush("bench_list", "item");  // Restore
    });

    std::cout << std::endl;
}

// ========== Throughput Tests ==========

void benchmark_throughput() {
    std::cout << "=== Throughput Tests ===" << std::endl;
    std::cout << std::endl;

    KeyValueStore store;

    const int count = 10000;

    // Write throughput
    {
        auto start = high_resolution_clock::now();
        for (int i = 0; i < count; ++i) {
            store.set("key_" + std::to_string(i), "value_" + std::to_string(i));
        }
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - start).count();
        double ops_per_sec = (count * 1000.0) / duration;

        std::cout << std::left << std::setw(40) << "WRITE throughput"
                  << std::right << std::setw(10) << std::fixed << std::setprecision(0)
                  << ops_per_sec << " ops/sec" << std::endl;
    }

    // Read throughput
    {
        auto start = high_resolution_clock::now();
        for (int i = 0; i < count; ++i) {
            volatile auto val = store.get("key_" + std::to_string(i));
            (void)val;
        }
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - start).count();
        double ops_per_sec = (count * 1000.0) / duration;

        std::cout << std::left << std::setw(40) << "READ throughput"
                  << std::right << std::setw(10) << std::fixed << std::setprecision(0)
                  << ops_per_sec << " ops/sec" << std::endl;
    }

    // Mixed throughput (50% read, 50% write)
    {
        auto start = high_resolution_clock::now();
        for (int i = 0; i < count; ++i) {
            if (i % 2 == 0) {
                store.set("mix_key_" + std::to_string(i), "value");
            } else {
                volatile auto val = store.get("mix_key_" + std::to_string(i - 1));
                (void)val;
            }
        }
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - start).count();
        double ops_per_sec = (count * 1000.0) / duration;

        std::cout << std::left << std::setw(40) << "MIXED (50R/50W) throughput"
                  << std::right << std::setw(10) << std::fixed << std::setprecision(0)
                  << ops_per_sec << " ops/sec" << std::endl;
    }

    std::cout << std::endl;
}

// ========== Memory Efficiency Tests ==========

void benchmark_memory() {
    std::cout << "=== Memory Efficiency Tests ===" << std::endl;
    std::cout << std::endl;

    KeyValueStore store;

    const int count = 1000;
    size_t total_size = 0;

    for (int i = 0; i < count; ++i) {
        std::string key = "key_" + std::to_string(i);
        std::string value = "value_" + std::to_string(i);
        total_size += key.size() + value.size();
        store.set(key, value);
    }

    double avg_size = static_cast<double>(total_size) / count;

    std::cout << std::left << std::setw(40) << "Average key-value size"
              << std::right << std::setw(10) << std::fixed << std::setprecision(2)
              << avg_size << " bytes" << std::endl;

    std::cout << std::left << std::setw(40) << "Total data size"
              << std::right << std::setw(10) << std::fixed << std::setprecision(0)
              << static_cast<double>(total_size) / 1024 << " KB" << std::endl;

    auto stats = store.stats();
    std::cout << std::left << std::setw(40) << "Stored keys"
              << std::right << std::setw(10) << stats["strings"] << std::endl;

    std::cout << std::endl;
}

int main() {
    std::cout << "Wingman Performance Benchmark" << std::endl;
    std::cout << "======================" << std::endl;
    std::cout << std::endl;

    benchmark_kvstore();
    benchmark_throughput();
    benchmark_memory();

    std::cout << "=== Benchmark Complete ===" << std::endl;

    return 0;
}
