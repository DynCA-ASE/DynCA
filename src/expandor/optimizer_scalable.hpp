#pragma once

#include "expandor/optimizer.hpp"

namespace expandor {
using std::pair;

class _Optimizer_Scalable : Optimizer {
    friend Optimizer;
    using Optimizer::Optimizer;

    int new_uncovered_tuples_after_remove_testcase(int tcid);
    int get_which_remove();
    void remove_testcase_greedily();
    pair<bool, pair<int, int>> get_gain_for_forcetuple(int tcid, t_tuple chosen_tp);
    bool random_greedy_step();
    void random_step();
    void forcetuple(int tid, t_tuple tp);
    bool greedy_step_forced(t_tuple tp);
    void change_bit(int v, int ad, const vector<int> &tc, vector<int> &cur_clauses_cov);
    pair<int, int> get_gain_for_forcetestcase(int tcid, const vector<int> &tc2);
    void flip_bit(int tid, int vid);
    bool check_for_flip(int tcid, int vid);
};

}  // namespace expandor::scalable
