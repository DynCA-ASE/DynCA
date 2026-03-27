#pragma once
#include <fmt/ranges.h>
#include <minisat_ext/BlackBoxSolver.h>
#include <minisat_ext/Ext.h>
#include <cadical.hpp>
#include "expandor/expandor.hpp"
#include "io/ddnnf.hpp"
#include "util/random.hpp"

// Validator
namespace expandor {
struct DDNNF_Validator_3wise;
struct DDNNF_Validator : Validator {
    std::unique_ptr<DDNNF> ddnnf;
    uint strength;
    DDNNF_Validator(DDNNF_Validator_3wise &&other, uint strength);
    DDNNF_Validator(const std::string &nnf_path, uint strength)
        : ddnnf(DDNNF::parse(nnf_path)), strength(strength) {}
    DDNNF_Validator(const CNF *cnf, uint strength)
        : ddnnf(cnf->convert_to<DDNNF>()), strength(strength) {}
    bool check_tuple(const t_tuple &tuple) override {
        return ddnnf->check_valid(std::span(tuple.v, strength));
    }
};
struct DDNNF_Validator_3wise : Validator {
    std::unique_ptr<DDNNF> ddnnf;
    explicit DDNNF_Validator_3wise(DDNNF_Validator &&other) : ddnnf(std::move(other.ddnnf)) {}
    explicit DDNNF_Validator_3wise(const CNF *cnf) : ddnnf(cnf->convert_to<DDNNF>()) {}
    bool check_tuple(const t_tuple &tuple) override {
        return ddnnf->check_valid(reinterpret_cast<std::array<int, 3> const &>(tuple));
    }
};
inline DDNNF_Validator::DDNNF_Validator(DDNNF_Validator_3wise &&other, uint strength)
    : ddnnf(std::move(other.ddnnf)), strength(strength) {}
struct Minisat_Validator : Validator {
    CDCLSolver::Solver solver;
    uint strength;
    Minisat_Validator(const CNF *cnf, uint strength) : solver(), strength(strength) {
        solver.read_clauses(cnf->get_num_variables(), cnf->get_clauses());
    }
    bool check_tuple(const t_tuple &tuple) override {
        for (uint i = 0; i < strength; ++i) {
            int j = abs(tuple.v[i]) - 1, vj = tuple.v[i] > 0;
            solver.add_assumption(j, vj);
        }

        bool res = solver.solve();
        solver.clear_assumptions();
        return res;
    }
};
struct Cadical_Validator : Validator {
    CaDiCaL::Solver solver;
    uint strength;
    Cadical_Validator(const CNF *cnf, uint strength) : solver(), strength(strength) {
        int vars;
        solver.read_dimacs(const_cast<CNF *>(cnf)->get_cnf_file_path().c_str(), vars);
        assert(vars == (int)cnf->get_num_variables());
    }
    bool check_tuple(const t_tuple &tuple) override {
        for (uint i = 0; i < strength; ++i) solver.assume(tuple.v[i]);
        return solver.solve() == 10;
    }
};
}  // namespace expandor

// Sampling Solver
namespace expandor {
struct Minisat_SamplingSolver : SamplingSolver {
    ExtMinisat::SamplingSolver solver;
    Minisat_SamplingSolver(const CNF *cnf, int seed)
        : solver(cnf->get_num_variables(), cnf->get_clauses(), seed, true, 0) {}
    void set_prob(const std::vector<std::pair<int, int>> &prob) override { solver.set_prob(prob); }
    void assume(int lit) override { solver.add_assumption(lit); }
    void get_solution(std::vector<int> &tc) override {
        solver.get_solution(tc);
        solver.clear_assumptions();
    }
};
struct Cadical_SamplingSolver : SamplingSolver {
    CaDiCaL::Solver solver;
    int vars;
    std::vector<int> phases;
    Cadical_SamplingSolver(const CNF *cnf, int seed) : solver() {
        solver.set("seed", seed);
        solver.set("shrink", 0);
        solver.set("quiet", 1);
        solver.set("phase", 1);
        solver.set("forcephase", 1);
        solver.set("lucky", 0);
        solver.read_dimacs(const_cast<CNF *>(cnf)->get_cnf_file_path().c_str(), vars);
        assert(vars == (int)cnf->get_num_variables());
    }
    void set_prob(const std::vector<std::pair<int, int>> &prob) override {
        for (int var = 1; auto &[p0, p1] : prob) {
            bool bit = (int)Random::bound(p0 + p1) < p0;
            solver.phase(bit ? var : -var);
            ++var;
        }
    }

    void assume(int lit) override { solver.assume(lit); }
    void get_solution(std::vector<int> &tc) override {
        if (solver.solve() != 10) throw std::runtime_error("Error: cadical cannot solve...");
        tc.resize(vars);
        for (int i = 0; i < vars; ++i) {
            tc[i] = solver.val(i + 1) > 0;
        }
        for (int lit : phases) solver.unphase(lit);
        phases.clear();
    }
};
}  // namespace expandor
