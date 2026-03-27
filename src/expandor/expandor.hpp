#pragma once

#include <cstring>
#include <iostream>
#include <utility>
#include <vector>
#include "covering_array.hpp"
#include "io/cnf.hpp"
#include "util/bitset.hpp"
#include "util/combinadic.hpp"

namespace expandor {
using std::vector;
inline constexpr int mx_strength = 6;

struct t_tuple {
    int v[mx_strength];
    t_tuple() {}
    t_tuple(int strength) {}
    t_tuple(const std::vector<int> &vec) {
        for (int i = 0; i < (int)vec.size(); i++) v[i] = vec[i];
    }
    friend bool operator==(const t_tuple &, const t_tuple &) = default;
    void print(int strength) {
        std::cout << "(";
        for (int i = 0; i < strength; i++) std::cout << v[i] << ", ";
        std::cout << ")\n";
    }
};

// Save some memory...
// Note: the reference type of TupleVector is `const t_tuple &`, but it's a mock object, only first
// `strength` values are availiable.
class TupleVector {
 public:
    class const_iterator {
        using inner_iter_t = std::vector<int>::const_iterator;
        inner_iter_t it_{};
        size_t strength_;

     public:
        using iterator_category = std::contiguous_iterator_tag;
        using iterator_concept = std::contiguous_iterator_tag;
        using value_type = t_tuple;
        using pointer_type = const t_tuple *;
        using reference_type = const t_tuple &;
        using difference_type = inner_iter_t::difference_type;

        const_iterator() = default;
        const_iterator(inner_iter_t it, size_t strength) : it_(it), strength_(strength) {}
        reference_type operator*() const { return reinterpret_cast<reference_type>(*it_); }
        pointer_type operator->() const { return reinterpret_cast<pointer_type>(&*it_); }
        reference_type operator[](difference_type n) const { return *(*this + n); }
        const_iterator &operator++() { return it_ += strength_, *this; }
        const_iterator operator++(int) {
            auto self = *this;
            ++*this;
            return self;
        }
        const_iterator &operator--() { return it_ -= strength_, *this; }
        const_iterator operator--(int) {
            auto self = *this;
            --*this;
            return self;
        }
        const_iterator &operator+=(difference_type n) { return it_ += n * strength_, *this; }
        const_iterator &operator-=(difference_type n) { return it_ -= n * strength_, *this; }
        friend const_iterator operator+(const const_iterator &i, difference_type n) {
            return {i.it_ + n * i.strength_, i.strength_};
        }
        friend const_iterator operator+(difference_type n, const const_iterator &i) {
            return i + n;
        }
        friend const_iterator operator-(const const_iterator &i, difference_type n) {
            return {i.it_ - n * i.strength_, i.strength_};
        }
        friend difference_type operator-(const const_iterator &i, const const_iterator &j) {
            assert(i.strength_ == j.strength_);
            return (i.it_ - j.it_) / i.strength_;
        }
        friend bool operator==(const const_iterator &, const const_iterator &) = default;
        friend auto operator<=>(const const_iterator &, const const_iterator &) = default;
    };
    using reference_type = const_iterator::reference_type;
    using value_type = const_iterator::value_type;
    using diffrence_type = const_iterator::difference_type;

    TupleVector() = default;
    ~TupleVector() = default;
    explicit TupleVector(size_t strength) : strength_(strength) {}

    void set_strength(size_t size) { strength_ = size; }
    size_t get_strength() const { return strength_; }

    void reserve(size_t new_size) { data_.reserve(new_size * strength_); }
    size_t size() const noexcept { return data_.size() / strength_; }
    size_t capacity() const noexcept { return data_.capacity() / strength_; }
    void shrink_to_fit() { data_.shrink_to_fit(); }

    void push_back(const t_tuple &tuple) {
        assert(strength_ > 0 && strength_ <= mx_strength);
        data_.insert(data_.cend(), tuple.v, tuple.v + strength_);
    }
    void pop_back() { data_.resize(data_.size() - strength_); }
    void clear() { data_.clear(); }
    void resize(size_t new_size, const t_tuple &tuple) {
        if (size() >= new_size) {
            data_.resize(new_size * strength_);
            return;
        }
        data_.reserve(new_size * strength_);
        while (size() < new_size) push_back(tuple);
    }

    void extends(std::span<const int> other) {
        size_t old_size = data_.size();
        data_.resize(old_size + other.size());
        std::memcpy(data_.data() + old_size, other.data(), other.size() * sizeof(int));
    }
    void extends(const TupleVector &other) { extends(other.data_); }

    reference_type operator[](size_t index) const { return begin()[index]; }
    reference_type front() const { return *begin(); }
    reference_type back() const { return end()[-1]; }

    const_iterator begin() const { return const_iterator(data_.begin(), strength_); }
    const_iterator end() const { return const_iterator(data_.end(), strength_); }

 private:
    std::vector<int> data_;
    size_t strength_ = 0;
};

enum class ValidatorEnum { ddnnf, minisat, cadical };
enum class SolverEnum { minisat, cadical };
struct Argument {
    ValidatorEnum use_validator = ValidatorEnum::ddnnf;
    SolverEnum use_solver = SolverEnum::minisat;
    int candidate_set_size = 100;
};

struct OptArgument {
    int alt_length = 200;
    int opt_time_ms = 3600'000;
    struct Config {
        bool use_weight = true;
        int cell_taboo_percent = 80;
        int forced_greedy_percent = 70;
        int cell_level_percent = 20;
        int taboo_size = 10;
    };
    std::vector<Config> configs = std::vector<Config>(1);
};

// clang-format off

/// A flag for optimzation method
enum class OptMethod : uint32_t {
    NONE      = 0,
    STRENGTH2 = 1 << 2,
    STRENGTH3 = 1 << 3,
    STRENGTH4 = 1 << 4,
    STRENGTH5 = 1 << 5,
    STRENGTH6 = 1 << 6,
    ALL       = 0b1111100,
};
// clang-format on
#define BinOp(ty, op)                                                                        \
    inline OptMethod operator op(OptMethod x, ty y) {                                        \
        return static_cast<OptMethod>(static_cast<uint32_t>(x) op static_cast<uint32_t>(y)); \
    }
BinOp(OptMethod, &);
BinOp(OptMethod, |);
BinOp(OptMethod, ^);
BinOp(uint32_t, &);
BinOp(uint32_t, |);
BinOp(uint32_t, ^);
#undef BinOp

struct Validator {
    virtual ~Validator() = default;
    virtual bool check_tuple(const t_tuple &tuple) = 0;
};

struct SamplingSolver {
    virtual ~SamplingSolver() = default;
    virtual void set_prob(const std::vector<std::pair<int, int>> &prob) = 0;
    virtual void assume(int lit) = 0;
    virtual void get_solution(std::vector<int> &tc) = 0;
};

class Expandor {
    friend class Optimizer;
    friend class _Optimizer_HSCA;
    friend class _Optimizer_Scalable;

 public:
    Expandor(const CNF *cnf, const CoveringArray &init_ca, const Argument &arg, int seed,
             const char *ddnnf_path = nullptr);
    ~Expandor() = default;
    void Expand();
    void Optimize(OptMethod opt_method, const OptArgument &arg);
    vector<vector<int>> GetFinalCA();

 private:
    bool use_invalid_expand = true;
    bool use_cache = true;
    bool need_all_valid_tuples = false;
    int strength;
    int candidate_set_size;
    int group_num;
    int uncovered_nums;
    const CNF *cnf;
    vector<int> group_id;
    vector<vector<int>> member;
    TupleVector tuples_U;
    size_t valid_nums;
    TupleVector valid_tuples;
    TupleVector uncovered_tuples;
    TupleVector t_clauses;
    vector<vector<int>> old_testcase;
    vector<vector<int>> new_testcase;
    vector<BitSet> covered_last_strength_bitmap;
    vector<BitSet> covered_now_strength_bitmap;
    vector<int> count_each_var_uncovered[2];
    vector<vector<int>> candidate_testcase_set_;
    vector<BitSet> CA;
    vector<bool> group_flag;
    vector<std::pair<int, int>> gain;
    std::unique_ptr<Validator> validator;
    std::unique_ptr<SamplingSolver> cdcl_sampler;

    void reinit_CA();
    void set_covered_now_strength_bitmap(const BitSet &tc, int idx, int last, t_tuple tuple);
    void get_remaining_valid_tuples(int value, int idx, int last, int _ed, t_tuple tuple);
    bool check_part_invalid(t_tuple tp);
    bool check_clauses_invalid(t_tuple tp);
    void GenerateCandidateTestcaseSet();
    int get_gain(const vector<int> &testcase);
    int SelectTestcaseFromCandidateSetByTupleNum();
    int GenerateTestcase();
    void Update_t_TupleInfo(const vector<int> &testcase, bool setmap);
    void Update_t_TupleInfo(int st, const vector<int> &testcase, const vector<int> &sidx);
    void ReplenishTestCase();
    void GenerateCoveringArray();

 private:  // combination variables
    uint64_t GetBase(int t, int n, const vector<int> &vec) const {
        if (t == 2) return (2ll * n - vec[0] - 1) * vec[0] / 2 + vec[1] - vec[0] - 1;
        long long res = combinadic_nCr(n, t) - combinadic_nCr(n - vec[0], t);
        vector<int> v(t - 1);
        for (int i = 0; i < t - 1; i++) v[i] = vec[i + 1] - vec[0] - 1;
        return res + GetBase(t - 1, n - vec[0] - 1, v);
    }
    uint64_t TupleToIndex(int strength, t_tuple t) const {
        const int nvar = cnf->get_num_variables();
        if (strength == 1) {
            int idx = abs(t.v[0]) - 1;
            return t.v[0] < 0 ? idx : nvar + idx;
        }

        vector<int> vec(strength);
        for (int i = 0; i < strength; i++) vec[i] = abs(t.v[i]) - 1;
        uint64_t base1 = GetBase(strength, nvar, vec);
        uint64_t base2 = 0;
        for (int i = 0; i < strength; i++) {
            int v = t.v[i] > 0;
            base2 |= v << (strength - i - 1);
        }
        return base2 * combinadic_nCr(nvar, strength) + base1;
    }

    bool is_covered(const vector<int> &tc, const t_tuple &t) const {
        for (int i = 0; i < strength; i++) {
            int pi = abs(t.v[i]) - 1, vi = t.v[i] > 0;
            if (tc[pi] != vi) return false;
        }
        return true;
    }
};

}  // namespace expandor
