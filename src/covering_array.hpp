#pragma once

#include <cassert>
#include <charconv>
#include <ostream>
#include <vector>
#include "io/casa.hpp"
#include "io/cnf.hpp"
#include "util/cpp23views.h"

class CoveringArray {
 public:
    CoveringArray() = default;
    CoveringArray(unsigned strength, unsigned num_vars)
        : strength_(strength), num_vars_(num_vars) {}
    CoveringArray(unsigned strength, unsigned num_vars, std::vector<std::vector<int>> ca)
        : strength_(strength), num_vars_(num_vars), ca_(std::move(ca)) {}
    ~CoveringArray() = default;

    void reserve(unsigned num_testcases) { ca_.reserve(num_testcases); }
    void add_tuple(std::vector<int> tuple) {
        assert(!num_vars_ || tuple.size() == num_vars_);
        num_vars_ = tuple.size();
        ca_.push_back(std::move(tuple));
    }
    void set_strength(unsigned strength) { strength_ = strength; }
    void set_num_vars(unsigned num_vars) { num_vars_ = num_vars; }
    unsigned get_strength() const { return strength_; }
    unsigned get_num_vars() const { return num_vars_; }
    unsigned get_num_testcases() const { return ca_.size(); }
    std::vector<std::vector<int>> &get_ca() { return ca_; }
    const std::vector<std::vector<int>> &get_ca() const { return ca_; }
    bool is_casa() const { return ca_.front().back() > 1; }

    void cnf_val2idx() {
        for (auto &row : ca_) {
            for (auto [idx, val] : row | cpp23::views::enumerate) {
                val = idx << 1 | val;
            }
        }
    }

    void cnf_idx2val() {
        for (auto &row : ca_) {
            for (auto &val : row) {
                val = val & 1;
            }
        }
    }

    CoveringArray cnf2casa(const CASA *casa) const {
        auto vcnt = casa->get_num_cared();
        return CoveringArray(  //
            strength_, vcnt,   //
            ca_ | std::views::transform([vcnt, casa](auto const &row) {
                return std::views::iota(0u, vcnt) | std::views::transform([&](auto &&opt) -> int {
                           auto first = casa->first_symbol(opt);
                           auto last = casa->last_symbol(opt);
                           for (uint v = first; v <= last; ++v) {
                               auto lit = casa->to_cnf_lit(v);
                               if ((lit > 0 && row[lit - 1]) || (lit < 0 && !row[-lit - 1]))
                                   return v;
                           }
                           assert(false);
                           __builtin_unreachable();
                       }) |
                       cpp23::ranges::to<std::vector>();
            }) | cpp23::ranges::to<std::vector>());
    }

    CoveringArray casa2cnf(const CASA *casa) const {
        auto vcnt = std::abs(casa->to_cnf_lit(casa->last_symbol(casa->get_num_cared() - 1)));
        return CoveringArray(  //
            strength_, vcnt,   //
            ca_ | std::views::transform([vcnt, casa](auto &&row) {
                std::vector<int> vec(vcnt, 0);
                std::ranges::for_each(row, [&](auto &&x) {
                    auto lit = casa->to_cnf_lit(x);
                    if (lit > 0) vec[lit - 1] = 1;
                });
                return vec;
            }) | cpp23::ranges::to<std::vector>());
    }

    void assert_array_valid(const CASA *casa, const CNF *cnf) const;
    void assert_array_valid(const CNF *cnf) const;

    friend std::ostream &operator<<(std::ostream &os, const CoveringArray &ca) {
        for (auto &row : ca.ca_) {
            for (bool first = true; auto &val : row) {
                os << (first ? "" : ",") << val;
                first = false;
            }
            os << '\n';
        }
        return os;
    }

    friend std::istream &operator>>(std::istream &is, CoveringArray &ca) {
        ca.ca_.clear();
        for (std::string _line; std::getline(is, _line);) {
            std::string_view line = _line;
            while (!line.empty() && isspace(line.front())) line = line.substr(1);
            while (!line.empty() && isspace(line.back())) line = line.substr(0, line.size() - 1);
            if (line.empty()) continue;
            ca.ca_.push_back(line | std::views::split(' ') | std::views::transform([](auto &&sub) {
                                 std::string_view sv(sub.begin(), sub.end());
                                 int x = 0;
                                 std::from_chars(sv.begin(), sv.end(), x);
                                 return x;
                             }) |
                             cpp23::ranges::to<std::vector>());
        }
        if (!ca.ca_.empty()) {
            ca.num_vars_ = ca.ca_.front().size();
        }
        return is;
    }

 private:
    unsigned strength_;
    unsigned num_vars_;
    std::vector<std::vector<int>> ca_;
};
