#include "io/casa.hpp"
#include <cassert>
#include <cstring>
#include <sstream>
#include "io/cnf.hpp"
#include "util/cpp23views.h"

auto CASA::parse(std::istream &spec, std::istream &constr) -> std::unique_ptr<CASA> {
    auto casa = parse(spec);

    unsigned num_clause;
    constr >> num_clause;
    casa->clauses_.reserve(num_clause);
    for (unsigned i = 0; i < num_clause; ++i) {
        Clause clause;
        unsigned num_lit;
        constr >> num_lit;
        clause.reserve(num_lit);
        while (num_lit--) {
            char sign;
            unsigned symbol;
            constr >> sign >> symbol;
            clause.emplace_back(symbol, sign == '-');
        }
        casa->clauses_.push_back(std::move(clause));
    }

    casa->reduce_casa();
    return casa;
}

auto CASA::parse(std::istream &spec) -> std::unique_ptr<CASA> {
    std::unique_ptr<CASA> casa(new CASA());
    unsigned num_options;
    spec >> casa->strength_ >> num_options;
    std::string line1, line2;
    std::getline(spec, line1);
    if (line1.empty()) std::getline(spec, line1);
    std::getline(spec, line2);
    std::stringstream ss(line2.empty() ? line1 : line2);
    casa->num_cared_ = line2.empty() ? num_options : std::stoul(line1);
    for (unsigned i = 0; i < num_options; ++i) {
        unsigned option;
        ss >> option;
        assert(option >= 2);
        casa->options_.push_back(option);
    }
    casa->calc_opt_infos();
    return casa;
}

void CASA::calc_opt_infos() {
    cumulative_value_counts_.reserve(options_.size());
    unsigned num_symbol = 0;
    for (auto opt : options_) {
        num_symbol += opt;
        cumulative_value_counts_.push_back(num_symbol);
    }
    owing_options_.reserve(num_symbol);
    for (unsigned i = 0; i < options_.size(); ++i) {
        for (unsigned k = 0; k < options_[i]; ++k) {
            owing_options_.push_back(i);
        }
    }

    // casa2cnf
    casa2cnf_.reserve(num_symbol);
    unsigned cnf_var = 0;
    for (unsigned i = 0; i < options_.size(); ++i) {
        if (options_[i] == 2) {
            ++cnf_var;
            casa2cnf_.push_back(-cnf_var);
            casa2cnf_.push_back(cnf_var);
            continue;
        }
        for (unsigned k = 0; k < options_[i]; ++k) {
            casa2cnf_.push_back(++cnf_var);
        }
    }
}

void CASA::print(std::ostream &spec, std::ostream &constr, bool print_cared) const {
    spec << strength_ << '\n' << options_.size() << '\n';
    if (print_cared) spec << num_cared_ << '\n';
    for (auto option : options_) {
        spec << option << ' ';
    }
    spec << '\n';

    constr << clauses_.size() << '\n';
    for (const auto &cl : clauses_) {
        constr << cl.size() << '\n';
        for (const auto &lit : cl) {
            constr << (lit.is_neg() ? '-' : '+') << ' ' << lit.get_var_id() << ' ';
        }
        constr << '\n';
    }
}

void CASA::reduce_casa() {
    size_t after_size = clauses_.size();
    std::vector<uint8_t> set(all_symbol_count());
    for (uint i = 0; i < after_size; ++i) {
        memset(set.data(), 0, sizeof(uint8_t) * set.size());
        auto &cl = clauses_[i];
        for (auto &x : cl) {
            if (x.is_neg()) {
                auto opt = option(x.get_var_id());
                for (auto v = first_symbol(opt); v <= last_symbol(opt); ++v) {
                    if (v != x.get_var_id()) {
                        set[v] = 1;
                    }
                }
            } else {
                set[x.get_var_id()] = 1;
            }
        }
        cl.clear();
        bool need_delete = false;
        for (uint opt = 0; opt < options_.size(); ++opt) {
            uint set_count = 0, first_unset_symbol = 0;
            for (auto v = first_symbol(opt); v <= last_symbol(opt); ++v) {
                if (set[v])
                    set_count++;
                else
                    first_unset_symbol = v;
            }
            if (set_count == symbol_count(opt)) {
                need_delete = true;
                break;
            } else if (set_count != 1 && set_count == symbol_count(opt) - 1) {
                cl.emplace_back(first_unset_symbol, true);
            } else {
                for (auto v = first_symbol(opt); v <= last_symbol(opt); ++v) {
                    if (set[v]) cl.emplace_back(v, false);
                }
            }
        }
        if (need_delete) {
            std::swap(cl, clauses_[--after_size]);
            continue;
        }
    }
    clauses_.resize(after_size);
}

template <>
std::unique_ptr<CASA> CASA::convert_to<CASA>() const {
    return std::make_unique<CASA>(*this);
}

template <>
std::unique_ptr<CNF> CASA::convert_to<CNF>() const {
    std::unique_ptr<CNF> cnf(new CNF());
    cnf->num_variables_ = std::abs(casa2cnf_.back());
    cnf->num_cared_ = std::abs(casa2cnf_[last_symbol(num_cared_ - 1)]);
    for (unsigned opt = 0; opt < num_cared_; opt++) {
        if (symbol_count(opt) == 2) continue;
        int first = casa2cnf_[first_symbol(opt)];
        int last = casa2cnf_[last_symbol(opt)];
        cnf->clauses_.push_back(std::views::iota(first, last + 1) |
                                cpp23::ranges::to<std::vector>());
        for (int i = first; i < last; i++) {
            for (int j = i + 1; j <= last; j++) {
                cnf->clauses_.push_back({-i, -j});
            }
        }
    }
    cnf->clauses_.reserve(cnf->clauses_.size() + clauses_.size() + 1);
    for (auto &cl : clauses_) {
        cnf->clauses_.push_back(cl | std::views::transform([this](auto &&lit) {
                                    int id = casa2cnf_[lit.get_var_id()];
                                    return lit.is_neg() ? -id : id;
                                }) |
                                cpp23::ranges::to<std::vector>());
    }
    int top = cnf->num_variables_;
    // top must be used in cl (otherwise, it maybe reduced by coprocessor)
    cnf->clauses_.push_back({top, -top});
    cnf->calc_cnf_info();
    cnf->reduce_cnf();
    cnf->group_info_.reserve(num_cared_);
    for (unsigned opt = 0; opt < num_cared_; opt++) {
        if (symbol_count(opt) == 2) continue;
        int first = casa2cnf_[first_symbol(opt)];
        int last = casa2cnf_[last_symbol(opt)];
        cnf->group_info_.push_back(std::views::iota(first, last + 1) |
                                   cpp23::ranges::to<std::vector>());
    }
    return cnf;
}
