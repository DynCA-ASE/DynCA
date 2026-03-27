#include <fmt/core.h>
#include <algorithm>
#include <fstream>
#include <numeric>
#include <ranges>
#include <unordered_set>
#include <variant>
#include "util/combinadic.hpp"
#include "util/cpp23views.h"

auto read_ca(const std::string &filename)
    -> std::variant<std::vector<std::vector<int>>, std::vector<std::vector<std::string>>> {
    std::ifstream fin(filename);
    std::vector<std::vector<std::string>> result;
    auto trim = std::views::drop_while(::isspace) | std::views::reverse |
                std::views::drop_while(::isspace) | std::views::reverse;
    for (std::string line; std::getline(fin, line);) {
        result.push_back(line | trim | std::views::split(' ') |
                         cpp23::ranges::to<std::vector<std::string>>());
    }
    if (std::ranges::all_of(result, [](auto &&row) {
            return std::ranges::all_of(
                row, [](auto &&str) { return std::ranges::all_of(str, ::isdigit); });
        })) {
        return result | std::views::transform([](auto &&row) {
                   return row | std::views::transform([](auto &&str) { return std::stoi(str); }) |
                          cpp23::ranges::to<std::vector>();
               }) |
               cpp23::ranges::to<std::vector>();
    }
    return result;
}

template <typename T>
    requires requires(const T &t) {
        { std::hash<T>{}(t) } -> std::integral;
    }
struct std::hash<std::vector<T>> {
    constexpr size_t operator()(const std::vector<T> &vec) const {
        std::hash<T> hash;
        return std::accumulate(vec.begin(), vec.end(), size_t(0), [&](auto &&acc, auto &&x) {
            return acc << 4 ^ static_cast<size_t>(hash(x));
        });
    }
};

template <typename T>
size_t bucket_count(const std::vector<std::vector<T>> &ca, uint strength, uint cared) {
    std::vector<std::unordered_set<std::vector<T>>> buckets(combinadic_nCr(cared, strength));
    size_t index = 0;
    for (auto tuple = combinadic_begin(strength); tuple.back() < cared;
         combinadic_next(tuple), ++index) {
        for (auto &row : ca) {
            buckets[index].insert(
                tuple | std::views::transform([&row](const auto &idx) { return row[idx]; }) |
                cpp23::ranges::to<std::vector>());
        }
    }
    return std::transform_reduce(buckets.begin(), buckets.end(), 0, std::plus<>(),
                                 std::ranges::size);
}

int main(int argc, char *argv[]) {
    if (argc != 3 && argc != 4) {
        fmt::println("usage: {} <ca> <strength> [cared]", argv[0]);
        exit(1);
    }
    auto ca = read_ca(argv[1]);
    uint strength = std::stoi(argv[2]);
    uint var_count = visit([](auto &&ca) { return ca.front().size(); }, ca);
    uint cared = argc == 4 ? std::stoi(argv[3]) : var_count;
    auto result =
        visit([strength, cared](auto &&ca) { return bucket_count(ca, strength, cared); }, ca);
    fmt::println("{}", result);
    return 0;
}
