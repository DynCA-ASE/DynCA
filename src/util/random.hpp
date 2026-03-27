#pragma once
#include <cstdint>

#include <random>

/// thread-safe random generator (use thread local std::mt19937_64)
class Random {
    Random() = default;
    uint64_t operator()() { return inner_(); }
    std::mt19937_64 *operator->() { return &inner_; }

 public:
    static Random &GetInstance() {
        static thread_local Random rand;
        return rand;
    }

    static constexpr uint64_t min() { return std::mt19937_64::min(); }
    static constexpr uint64_t max() { return std::mt19937_64::max(); }
    static void seed() { GetInstance()->seed(); }
    template <typename T>
    static auto seed(T &&s) -> decltype(std::declval<std::mt19937_64>().seed(std::forward<T>(s))) {
        GetInstance()->seed(std::forward<T>(s));
    }
    static void discard(size_t z) { GetInstance()->discard(z); }
    static uint64_t gen() { return GetInstance()(); }
    static uint64_t bound(uint64_t end) {
        return std::uniform_int_distribution<uint64_t>(0, end - 1)(get());
    }
    static uint64_t bound(uint64_t begin, uint64_t end) {
        return std::uniform_int_distribution<uint64_t>(begin, end - 1)(get());
    }
    static double next_double() { return std::uniform_real_distribution<double>()(get()); }
    static std::mt19937_64 &get() { return GetInstance().inner_; }

 private:
    std::mt19937_64 inner_;
};
