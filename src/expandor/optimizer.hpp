#pragma once

#include <minisat_ext/BlackBoxSolver.h>
#include <minisat_ext/Ext.h>
#include <chrono>
#include "expandor/expandor.hpp"
#include "util/intrusive_list.h"

namespace expandor {

/**
 * Implementation Note:
 * Optimizer is a base class, every step in `search()` can call either HSCA or ScalableCA.
 * To do this, we *extend* this class (_Optimizer_HSCA and _Optimizer_ScalableCA).
 * To use Optimizer, user only need to create a instance of this class.
 * At each step of `search()`, we cast `this` to corresponding pointer of extension class in order
 * to call method in extensions.
 * This is okay if and only if there is no extract fields in extensions.
 */
class Optimizer {
 public:
    Optimizer(Expandor &expandor, const OptArgument &arg);
    ~Optimizer() { IntrusiveListNode::recreate_pool(); }
    Optimizer(const Optimizer &) = delete;
    Optimizer(Optimizer &&) = default;
    Optimizer &operator=(const Optimizer &) = delete;
    Optimizer &operator=(Optimizer &&) = default;

    void search(std::chrono::milliseconds time);
    void swap_testcase_out(Expandor &expandor) {
        expandor.old_testcase.swap(last_strength_testcases);
        expandor.new_testcase.swap(testcases);
    }

 protected:
    bool is_covered(const vector<int> &tc, const t_tuple &t) const {
        for (int i = 0; i < strength; i++) {
            int pi = abs(t.v[i]) - 1, vi = t.v[i] > 0;
            if (tc[pi] != vi) return false;
        }
        return true;
    }

    void rebind_pos_to_idx();
    void restart();

    void UpdateInfo_remove_testcase(int tcid_idx);
    void uptate_unique_covered(int tcid);
    void update_covered_testcases(int tpid);
    void forcetestcase(int tcid, const vector<int> &tc2);

 protected:
    OptArgument arguments;
    const OptArgument::Config *config;

    int strength;
    int greedy_limit = 0;

    const CNF *cnf;
    const Expandor *saved_expandor;

    vector<vector<int>> last_strength_testcases;
    vector<vector<int>> testcases;
    vector<vector<int>> best_testcases;

    TupleVector tuples_U;
    vector<int> covered_times;

    vector<int> covered_tuples;
    vector<int> uncovered_tuples;

    vector<vector<int>> clauses_cov;

    vector<int> last_greedy_time;
    vector<vector<int>> last_greedy_time_cell;

    bool use_cdcl_solver = true;
    std::unique_ptr<CDCLSolver::Solver> cdcl_solver;
    std::unique_ptr<ExtMinisat::SamplingSolver> cdcl_sampler;

    vector<IntrusiveList> unique_covered_tuples;
    vector<IntrusiveList> tc_covered_tuples;
    vector<IntrusiveList> covered_testcases;
    vector<IntrusiveListNode *> unique_node;

    vector<int> testcase_pos_to_idx;
    vector<int> testcase_idx_to_pos;
    vector<int> tuples_idx_to_pos;

    vector<uint32_t> weight;
    vector<uint64_t> sum_weight;

    int testcase_idx;
    int cur_step;
};
}  // namespace expandor
