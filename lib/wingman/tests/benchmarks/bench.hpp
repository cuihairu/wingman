// 轻量基准助手：无第三方依赖（vcpkg 统一供包约束），整批计时取中位数
// 抗单次抖动。绝对值受宿主负载影响大，对比请同机同负载成对测。
#pragma once

#include <algorithm>
#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace wingman::bench {

class Bench {
public:
    explicit Bench(std::string suite) : suite_(std::move(suite)) {}

    // iterations 次调用为一轮，跑 reps 轮取中位 ns/op；warmup 取 1/10 轮。
    template <typename F>
    double run(const std::string& name, F&& fn, int iterations = 1000, int reps = 7) {
        const int warmup = std::max(1, iterations / 10);
        for (int i = 0; i < warmup; ++i) fn();

        std::vector<double> nsPerOp;
        nsPerOp.reserve(static_cast<size_t>(reps));
        for (int r = 0; r < reps; ++r) {
            const auto start = clock::now();
            for (int i = 0; i < iterations; ++i) fn();
            const auto end = clock::now();
            const double ns =
                std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(
                    end - start).count() / iterations;
            nsPerOp.push_back(ns);
        }
        std::sort(nsPerOp.begin(), nsPerOp.end());
        const double median = nsPerOp[nsPerOp.size() / 2];
        const double opsPerSec = median > 0 ? 1e9 / median : 0;

        std::cout << "  " << std::left << std::setw(46) << name
                  << std::right << std::setw(12) << std::fixed << std::setprecision(1)
                  << median << " ns/op"
                  << std::setw(14) << std::setprecision(0)
                  << opsPerSec << " ops/s"
                  << "  (n=" << iterations << ")\n";
        return median;
    }

    void header(const std::string& title) const {
        std::cout << "\n== " << suite_ << ": " << title << " ==\n";
    }

private:
    using clock = std::chrono::steady_clock;
    std::string suite_;
};

} // namespace wingman::bench
