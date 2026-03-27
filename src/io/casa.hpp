#pragma once

#include "io/base.hpp"

class Lit {
 public:
    Lit(unsigned var_id, bool negated) : var_id_(var_id), negated_(negated) {}
    explicit Lit(int var) : var_id_(std::abs(var)), negated_(var < 0) {}
    unsigned get_var_id() const { return var_id_; }
    bool is_neg() const { return negated_; }

 private:
    unsigned var_id_;
    bool negated_;
};

using Clause = std::vector<Lit>;

class CASA : public InputBase {
    friend class CNF;
    friend class CTWModel;
    CASA() = default;

 public:
    static auto parse(std::istream &spec, std::istream &constr) -> std::unique_ptr<CASA>;
    static auto parse(const std::string &spec_file, const std::string &constr_file)
        -> std::unique_ptr<CASA> {
        std::ifstream spec(spec_file);
        std::ifstream constr(constr_file);
        if (!spec.is_open() || !constr.is_open()) {
            throw std::runtime_error("Failed to open CASA files.");
        }
        return parse(spec, constr);
    }
    static auto parse(std::istream &spec) -> std::unique_ptr<CASA>;
    static auto parse(const std::string &spec_file) -> std::unique_ptr<CASA> {
        std::ifstream spec(spec_file);
        if (!spec.is_open()) {
            throw std::runtime_error("Failed to open CASA files.");
        }
        return parse(spec);
    }
    void print(std::ostream &spec, std::ostream &constr, bool print_cared = true) const;
    void print(const std::string &spec_file, const std::string &constr_file,
               bool print_cared = true) const {
        std::ofstream spec(spec_file);
        std::ofstream constr(constr_file);
        if (!spec.is_open() || !constr.is_open()) {
            throw std::runtime_error("Failed to open CASA files for writing.");
        }
        print(spec, constr, print_cared);
    }
    void reduce_casa();

    bool validate(const std::vector<uint> &row) const {
        return std::ranges::all_of(clauses_, [&](const auto &cl) {
            return std::ranges::any_of(cl, [&](const auto &lit) {
                auto var = lit.get_var_id();
                auto opt = option(var);
                return lit.is_neg() ? row[opt] != var : row[opt] == var;
            });
        });
    }

    /// MARK: Basic Information

    unsigned get_num_cared() const { return num_cared_; }
    unsigned get_strength() const { return strength_; }
    void set_strength(unsigned strength) { strength_ = strength; }
    const std::vector<unsigned> &get_options() const { return options_; }
    unsigned get_option_size() const { return options_.size(); }
    const std::vector<Clause> &get_clauses() const { return clauses_; }
    unsigned get_clause_size() const { return clauses_.size(); }

    /// MARK: Option <--> Symbol

    unsigned option(const unsigned symbol) const { return owing_options_[symbol]; };
    unsigned first_symbol(const unsigned option) const {
        return option ? cumulative_value_counts_[option - 1] : 0;
    }
    unsigned last_symbol(const unsigned option) const {
        return cumulative_value_counts_[option] - 1;
    }
    unsigned symbol_count(const unsigned option) const {
        return option ? cumulative_value_counts_[option] - cumulative_value_counts_[option - 1]
                      : cumulative_value_counts_[option];
    }
    unsigned cared_symbol_count() const { return cumulative_value_counts_[num_cared_ - 1]; }
    unsigned all_symbol_count() const { return cumulative_value_counts_.back(); }

    /// MARK: Convert formats

    template <std::derived_from<InputBase> T>
    std::unique_ptr<T> convert_to() const;
    int to_cnf_lit(unsigned symbol) const { return casa2cnf_[symbol]; }

 private:
    unsigned num_cared_ = 0;
    unsigned strength_ = 0;
    std::vector<unsigned> options_;
    std::vector<Clause> clauses_;

    void calc_opt_infos();

    /// Option <--> Symbol
    std::vector<unsigned> owing_options_, cumulative_value_counts_;

    /// CASA <--> CNF
    std::vector<int> casa2cnf_;
};
