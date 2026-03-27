#pragma once
#include <atomic>
#include <functional>
#include "util/logger.hpp"

inline std::atomic_bool global_stop_flag = false;

inline void global_check_stop() {
    if (!global_stop_flag.load(std::memory_order_acquire)) return;
    logger::info(logger::Color::RED, "Program exit without any ca generated!");
    std::exit(1);
}

inline void global_check_stop(std::invocable auto &&fn) {
    if (!global_stop_flag.load(std::memory_order_acquire)) return;
    std::invoke(std::forward<decltype(fn)>(fn));
    std::exit(0);
}
