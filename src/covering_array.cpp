#include "covering_array.hpp"
#include <fmt/ranges.h>
#include "minisat_ext/BlackBoxSolver.h"

void CoveringArray::assert_array_valid(const CASA *casa, const CNF *cnf) const {
    CDCLSolver::Solver solver;
    solver.read_clauses(cnf->get_num_variables(), cnf->get_clauses());
    assert(num_vars_ == casa->get_num_cared());
    for (auto &row : ca_) {
        assert(row.size() == casa->get_num_cared());
        for (uint sym = 0, passing = 0; sym < casa->cared_symbol_count(); sym++) {
            if (passing < row.size() && sym == (uint) row[passing]) {
                solver.add_assumption(casa->to_cnf_lit(sym));
                passing++;
            } else {
                solver.add_assumption(-casa->to_cnf_lit(sym));
            }
        }
        if (auto ret = solver.solve(); solver.clear_assumptions(), !ret) {
            throw std::runtime_error(fmt::format("line {} in ca is invalid!", row));
        }
    }
}

void CoveringArray::assert_array_valid(const CNF *cnf) const {
    CDCLSolver::Solver solver;
    solver.read_clauses(cnf->get_num_variables(), cnf->get_clauses());
    assert(num_vars_ == cnf->get_num_cared());
    for (auto &row : ca_) {
        assert(row.size() == cnf->get_num_cared());
        for (uint var = 0; var < cnf->get_num_cared(); var++) {
            solver.add_assumption(var, row[var]);
        }
        if (auto ret = solver.solve(); solver.clear_assumptions(), !ret) {
            throw std::runtime_error(fmt::format("line {} in ca is invalid!", row));
        }
    }
}
